"""Worktree pool for agent isolation.

Each agent works in its own git worktree to prevent file conflicts.
Worktrees share build artifacts via symlinks to the main repo.
"""

import logging
import shutil
import subprocess
import sqlite3
from pathlib import Path
from typing import Optional

from .database import get_connection, DEFAULT_DB_PATH

logger = logging.getLogger("decomp_orchestrator")


class WorktreePool:
    """
    Manages a pool of git worktrees for parallel agent execution.

    Each worktree is a detached HEAD checkout that shares:
    - bin/ (objdiff-cli, analyze-function)
    - decomp.db (SQLite database)
    - .claude/ (agent prompts, MCP config)
    - compile_commands.json (for clangd)
    - .clangd (clangd config)
    """

    # Paths to symlink (lightweight, shared resources)
    DEFAULT_SYMLINKS = [
        "decomp.db",                # SQLite database
        "compile_commands.json",    # For clangd
        ".clangd",                  # Clangd config
        "orig",                     # Original binary for comparison
    ]

    # Paths to copy (need to be writable or avoid symlink issues)
    DEFAULT_COPIES = [
        "bin",                      # objdiff-cli, analyze-function (tools)
        "config",                   # Build config (config.yml, etc.)
        "objdiff.json",             # objdiff-cli project config
        "build.ninja",              # Ninja build file
        "build",                    # Entire build directory (each worktree gets own copy)
        "tools",                    # Analysis tools (analyze_function.py, struct_db.py, etc.)
    ]

    # Subset of copies to re-sync from main repo on each acquire.
    # Keeps tools up-to-date without touching per-worktree build artifacts.
    REFRESH_ON_ACQUIRE = [
        "bin",                      # objdiff-cli, analyze-function
        "tools",                    # analyze_function.py, struct_db.py, etc.
        "objdiff.json",             # objdiff-cli project config
        "build.ninja",              # Ninja build file
        "config",                   # Build config
    ]

    def __init__(
        self,
        main_repo: Path,
        pool_dir: Path,
        pool_size: int = 3,
        db_path: str | Path = DEFAULT_DB_PATH,
        symlinks: list[str] | None = None,
        copies: list[str] | None = None,
    ):
        """
        Initialize worktree pool.

        Args:
            main_repo: Path to main git repository
            pool_dir: Directory to create worktrees in (e.g., /tmp/decomp-agents)
            pool_size: Number of worktrees to create
            db_path: Path to SQLite database
            symlinks: List of paths to symlink from main repo
            copies: List of paths to copy from main repo (for writable dirs)
        """
        self.main_repo = Path(main_repo).resolve()
        self.pool_dir = Path(pool_dir).resolve()
        self.pool_size = pool_size
        self.db_path = str(db_path)
        self.symlinks = symlinks if symlinks is not None else self.DEFAULT_SYMLINKS
        self.copies = copies if copies is not None else self.DEFAULT_COPIES

    def initialize(self, force: bool = False) -> list[Path]:
        """
        Create pool_size worktrees and mark them as available.

        Args:
            force: If True, remove existing worktrees first

        Returns:
            List of created worktree paths
        """
        self.pool_dir.mkdir(parents=True, exist_ok=True)

        # Prune stale worktree registrations (paths deleted but still tracked by git)
        subprocess.run(
            ["git", "worktree", "prune"],
            cwd=self.main_repo,
            capture_output=True,
        )

        conn = get_connection(self.db_path)
        created = []

        for i in range(self.pool_size):
            worktree_path = self.pool_dir / f"agent-{i}"

            if worktree_path.exists():
                if force:
                    self._remove_worktree(worktree_path, conn)
                else:
                    # Already exists - just ensure it's in database
                    self._ensure_in_database(conn, worktree_path)
                    created.append(worktree_path)
                    continue

            # Create detached worktree from HEAD
            try:
                subprocess.run(
                    ["git", "worktree", "add", str(worktree_path), "HEAD", "--detach"],
                    cwd=self.main_repo,
                    check=True,
                    capture_output=True,
                    text=True,
                )
            except subprocess.CalledProcessError as e:
                print(f"Error creating worktree {worktree_path}: {e.stderr}")
                continue

            # Setup symlinks (replace existing dirs/files with symlinks to main repo)
            for link_name in self.symlinks:
                src = self.main_repo / link_name
                dst = worktree_path / link_name
                if src.exists():
                    # Ensure parent directory exists (for nested paths)
                    dst.parent.mkdir(parents=True, exist_ok=True)
                    # Remove any existing file/directory at dst
                    if dst.is_symlink():
                        dst.unlink()
                    elif dst.is_file():
                        dst.unlink()
                    elif dst.is_dir():
                        shutil.rmtree(dst)
                    # Create symlink to main repo
                    if not dst.exists():
                        dst.symlink_to(src)

            # Setup copies (copy directories/files that need to be writable)
            for copy_name in self.copies:
                src = self.main_repo / copy_name
                dst = worktree_path / copy_name
                if src.exists():
                    # Ensure parent directory exists
                    dst.parent.mkdir(parents=True, exist_ok=True)
                    # Remove any existing file/directory at dst
                    if dst.is_symlink():
                        dst.unlink()
                    elif dst.is_file():
                        dst.unlink()
                    elif dst.is_dir():
                        shutil.rmtree(dst)
                    # Copy from main repo
                    if not dst.exists():
                        if src.is_dir():
                            shutil.copytree(
                                src, dst,
                                ignore=shutil.ignore_patterns(
                                    "gen",                # game assets (6.2G symlink)
                                    "report_raw.cache",   # dtk report cache (564M)
                                    "report.cache",       # dtk report cache (70M)
                                    "baselines",          # regression baselines (40M)
                                    "asm",                # disassembly text (241M)
                                    "data",               # dtk data section (36M)
                                    "default.xex",        # linked XEX (28M)
                                    "bsf_*",              # BSF test dirs
                                ),
                            )
                            print(f"  Copied directory: {copy_name}")
                        else:
                            shutil.copy2(src, dst)
                            print(f"  Copied file: {copy_name}")

            # Regenerate build.ninja with absolute tool paths
            self._regenerate_build_ninja(worktree_path)

            # Write empty .mcp.json to prevent name collisions with SDK mcp_servers
            self._write_agent_mcp_json(worktree_path)

            # Mark available in database
            self._ensure_in_database(conn, worktree_path)
            created.append(worktree_path)
            print(f"Created worktree: {worktree_path}")

        conn.commit()
        return created

    def _ensure_in_database(self, conn: sqlite3.Connection, path: Path) -> None:
        """Ensure worktree is tracked in database."""
        existing = conn.execute(
            "SELECT id FROM worktrees WHERE path = ?", (str(path),)
        ).fetchone()

        if existing is None:
            conn.execute(
                """
                INSERT INTO worktrees (path, status, session_id)
                VALUES (?, 'available', NULL)
                """,
                (str(path),),
            )

    def _remove_worktree(self, path: Path, conn: sqlite3.Connection | None = None) -> None:
        """Remove a worktree."""
        subprocess.run(
            ["git", "worktree", "remove", str(path), "--force"],
            cwd=self.main_repo,
            check=False,  # Don't fail if already removed
            capture_output=True,
        )

        # Use provided connection or create new one
        should_close = conn is None
        if conn is None:
            conn = get_connection(self.db_path)
        conn.execute("DELETE FROM worktrees WHERE path = ?", (str(path),))
        conn.commit()

    def _restore_symlinks(self, worktree_path: Path) -> None:
        """Restore symlinks after git checkout/clean recreates directories from git."""
        for link_name in self.symlinks:
            src = self.main_repo / link_name
            dst = worktree_path / link_name
            if not src.exists():
                continue
            # Ensure parent directory exists (for nested paths like build/tools)
            dst.parent.mkdir(parents=True, exist_ok=True)
            # Remove existing file/dir if it's not already the correct symlink
            if dst.is_symlink():
                if dst.resolve() == src.resolve():
                    continue  # Already correct
                dst.unlink()
            elif dst.is_file():
                dst.unlink()
            elif dst.is_dir():
                shutil.rmtree(dst)
            # Create symlink
            if not dst.exists():
                dst.symlink_to(src)

    def _write_agent_mcp_json(self, worktree_path: Path) -> None:
        """Write an empty .mcp.json to prevent name collisions with SDK mcp_servers.

        The orchestrator MCP server is provided programmatically via the SDK
        mcp_servers option. The git-tracked .mcp.json also defines "orchestrator",
        causing a name collision that drops tools (notably run_diff_inspect).
        Other servers (decomp.me, pyghidra) aren't needed by agents.
        """
        import json
        mcp_json_path = worktree_path / ".mcp.json"
        with open(mcp_json_path, "w") as f:
            json.dump({"mcpServers": {}}, f)

    def _regenerate_build_ninja(self, worktree_path: Path) -> None:
        """Regenerate build.ninja with absolute tool paths.

        The main repo's build.ninja may use relative paths (../jeff/...) that
        break when copied to worktrees in /tmp/. Mirrors setup_worktree.sh by
        running configure.py with absolute paths to dtk, objdiff-cli, and wibo.
        """
        tool_dir = self.main_repo.parent  # e.g., /home/user/code/milohax
        dtk = tool_dir / "jeff" / "target" / "release" / "dtk"
        objdiff = tool_dir / "objdiff" / "target" / "release" / "objdiff-cli"
        wibo = tool_dir / "wibo" / "build" / "release" / "wibo"

        # Only regenerate if configure.py exists and tools are available
        configure_py = worktree_path / "configure.py"
        if not configure_py.exists():
            logger.warning(f"configure.py not found in {worktree_path}")
            return

        missing = [str(t) for t in [dtk, objdiff, wibo] if not t.exists()]
        if missing:
            logger.warning(f"Skipping build.ninja regen — missing tools: {missing}")
            return

        try:
            result = subprocess.run(
                [
                    "python3", str(configure_py),
                    "--dtk", str(dtk),
                    "--objdiff", str(objdiff),
                    "--wibo", str(wibo),
                ],
                cwd=worktree_path,
                capture_output=True,
                text=True,
                timeout=30,
            )
            if result.returncode != 0:
                logger.warning(f"configure.py failed: {result.stderr[:200]}")
        except Exception as e:
            logger.warning(f"Failed to regenerate build.ninja: {e}")

    def _refresh_tools(self, worktree_path: Path) -> None:
        """Re-copy tool directories from main repo to keep worktree in sync."""
        for name in self.REFRESH_ON_ACQUIRE:
            src = self.main_repo / name
            dst = worktree_path / name
            if not src.exists():
                continue
            try:
                if dst.is_symlink():
                    dst.unlink()
                elif dst.is_file():
                    dst.unlink()
                elif dst.is_dir():
                    shutil.rmtree(dst)

                if src.is_dir():
                    shutil.copytree(src, dst)
                else:
                    shutil.copy2(src, dst)
            except Exception as e:
                logger.warning(f"Failed to refresh {name} in worktree: {e}")

    def _get_main_repo_commit(self) -> Optional[str]:
        """Get the current commit SHA of the main repo."""
        try:
            result = subprocess.run(
                ["git", "rev-parse", "HEAD"],
                cwd=self.main_repo,
                capture_output=True,
                text=True,
                check=True,
            )
            return result.stdout.strip()
        except subprocess.CalledProcessError:
            return None

    def _sync_worktree_to_commit(self, worktree_path: Path, commit: str) -> bool:
        """Sync worktree to a specific commit.

        Args:
            worktree_path: Path to the worktree
            commit: Commit SHA to sync to

        Returns:
            True if sync succeeded, False otherwise
        """
        try:
            # Fetch the commit from the main repo (in case it's not in worktree yet)
            subprocess.run(
                ["git", "fetch", str(self.main_repo), commit],
                cwd=worktree_path,
                capture_output=True,
                check=True,
            )
            # Checkout to the specific commit
            subprocess.run(
                ["git", "checkout", commit, "--force"],
                cwd=worktree_path,
                capture_output=True,
                check=True,
            )
            return True
        except subprocess.CalledProcessError as e:
            print(f"Error syncing worktree to {commit[:8]}: {e}")
            return False

    def acquire(self, session_id: str, _depth: int = 0) -> Optional[Path]:
        """
        Get an available worktree, clean it, and mark as in_use.

        Uses atomic UPDATE with subquery to prevent race conditions where
        multiple agents could claim the same worktree.

        Args:
            session_id: Unique identifier for the session claiming this worktree
            _depth: Internal recursion depth counter (prevents stack overflow)

        Returns:
            Path to worktree, or None if none available
        """
        # Prevent infinite recursion if all worktrees become dirty
        if _depth >= self.pool_size:
            print(f"All {self.pool_size} worktrees are dirty, none available")
            return None

        conn = get_connection(self.db_path)

        # Atomically find and claim an available worktree
        # This prevents race conditions where multiple agents SELECT the same worktree
        cursor = conn.execute(
            """
            UPDATE worktrees
            SET status = 'in_use', session_id = ?, updated_at = CURRENT_TIMESTAMP
            WHERE id = (
                SELECT id FROM worktrees
                WHERE status = 'available'
                LIMIT 1
            )
            RETURNING id, path
            """,
            (session_id,),
        )
        row = cursor.fetchone()
        conn.commit()

        if row is None:
            return None  # No worktrees available

        worktree_id = row["id"]
        worktree_path = Path(row["path"])

        # Get main repo's current commit and sync worktree to it
        main_commit = self._get_main_repo_commit()
        if main_commit:
            if not self._sync_worktree_to_commit(worktree_path, main_commit):
                # Sync failed - mark as dirty and try next worktree
                conn.execute(
                    "UPDATE worktrees SET status = 'dirty', session_id = NULL WHERE id = ?",
                    (worktree_id,),
                )
                conn.commit()
                return self.acquire(session_id, _depth + 1)
        else:
            # Couldn't get main repo commit, fall back to local HEAD reset
            try:
                subprocess.run(
                    ["git", "checkout", "HEAD", "--force"],
                    cwd=worktree_path,
                    check=True,
                    capture_output=True,
                )
            except subprocess.CalledProcessError as e:
                print(f"Error resetting worktree {worktree_path}: {e}")
                conn.execute(
                    "UPDATE worktrees SET status = 'dirty', session_id = NULL WHERE id = ?",
                    (worktree_id,),
                )
                conn.commit()
                return self.acquire(session_id, _depth + 1)

        # Clean untracked files (exclude copied directories and symlinked paths)
        try:
            clean_cmd = ["git", "clean", "-fd"]
            for copy_name in self.copies:
                clean_cmd.extend(["-e", copy_name])
            for link_name in self.symlinks:
                clean_cmd.extend(["-e", link_name])
            subprocess.run(
                clean_cmd,
                cwd=worktree_path,
                check=True,
                capture_output=True,
            )
            # Restore symlinks after clean (git clean may remove them)
            self._restore_symlinks(worktree_path)
        except subprocess.CalledProcessError as e:
            print(f"Error cleaning worktree {worktree_path}: {e}")
            # Mark as dirty and release our claim
            conn.execute(
                "UPDATE worktrees SET status = 'dirty', session_id = NULL WHERE id = ?",
                (worktree_id,),
            )
            conn.commit()
            return self.acquire(session_id, _depth + 1)  # Recursively try next

        # Refresh tool directories from main repo so worktrees stay in sync
        self._refresh_tools(worktree_path)

        # Regenerate build.ninja with absolute tool paths.
        # The main repo's build.ninja uses relative paths (../jeff/...) that
        # don't resolve from worktrees in /tmp/. Re-running configure.py with
        # absolute paths makes ninja work from any location.
        self._regenerate_build_ninja(worktree_path)

        # Write empty .mcp.json to prevent name collisions with SDK mcp_servers
        self._write_agent_mcp_json(worktree_path)

        return worktree_path

    def release(self, session_id: str) -> bool:
        """
        Return worktree to pool and mark as available.

        Args:
            session_id: Session that held the worktree

        Returns:
            True if a worktree was released, False if none found
        """
        conn = get_connection(self.db_path)

        cursor = conn.execute(
            """
            UPDATE worktrees
            SET status = 'available', session_id = NULL, updated_at = CURRENT_TIMESTAMP
            WHERE session_id = ?
            """,
            (session_id,),
        )
        conn.commit()

        return cursor.rowcount > 0

    def extract_patch(self, session_id: str) -> Optional[str]:
        """
        Get git diff from worktree associated with session.

        Includes both modifications to tracked files AND new untracked files
        in src/ directories. New files are captured by temporarily staging
        them with `git add -N` (intent-to-add).

        Args:
            session_id: Session ID to get patch for

        Returns:
            Patch text or None if no changes
        """
        conn = get_connection(self.db_path)

        row = conn.execute(
            "SELECT path FROM worktrees WHERE session_id = ?",
            (session_id,),
        ).fetchone()

        if row is None:
            return None

        worktree_path = Path(row["path"])

        try:
            # Find new untracked files in directories we care about:
            # - src/: new header/source files created during decomp work
            # - config/: new symbol or config entries
            # - docs/sessions/: agent session logs
            untracked_result = subprocess.run(
                ["git", "ls-files", "--others", "--exclude-standard",
                 "src/", "config/", "docs/sessions/"],
                cwd=worktree_path,
                capture_output=True,
                text=True,
                check=True,
            )
            untracked_files = [f for f in untracked_result.stdout.strip().split('\n') if f]

            # Stage new files with intent-to-add so they appear in diff
            if untracked_files:
                subprocess.run(
                    ["git", "add", "-N"] + untracked_files,
                    cwd=worktree_path,
                    capture_output=True,
                    check=True,
                )

            # Get diff for source, headers, config (symbols.txt, objects.json, etc.),
            # and agent session logs
            result = subprocess.run(
                ["git", "diff", "HEAD", "--",
                 "src/", "include/", "config/", "docs/sessions/"],
                cwd=worktree_path,
                capture_output=True,
                text=True,
                check=True,
            )

            # Reset the staging area to clean up intent-to-add markers
            if untracked_files:
                reset_result = subprocess.run(
                    ["git", "reset", "HEAD", "--"] + untracked_files,
                    cwd=worktree_path,
                    capture_output=True,
                    text=True,
                )
                if reset_result.returncode != 0:
                    logger.warning(
                        f"git reset after extract_patch failed (rc={reset_result.returncode}): "
                        f"{reset_result.stderr.strip()}"
                    )
                    # Force-clean the index as fallback
                    subprocess.run(
                        ["git", "reset", "HEAD"],
                        cwd=worktree_path,
                        capture_output=True,
                    )

            return result.stdout if result.stdout.strip() else None
        except subprocess.CalledProcessError as e:
            logger.warning(f"extract_patch failed for session {session_id}: {e}")
            return None

    def get_worktree_for_session(self, session_id: str) -> Optional[Path]:
        """Get the worktree path for a session."""
        conn = get_connection(self.db_path)

        row = conn.execute(
            "SELECT path FROM worktrees WHERE session_id = ?",
            (session_id,),
        ).fetchone()

        return Path(row["path"]) if row else None

    def status(self) -> dict:
        """Get pool status."""
        conn = get_connection(self.db_path)

        total = conn.execute("SELECT COUNT(*) FROM worktrees").fetchone()[0]
        available = conn.execute(
            "SELECT COUNT(*) FROM worktrees WHERE status = 'available'"
        ).fetchone()[0]
        in_use = conn.execute(
            "SELECT COUNT(*) FROM worktrees WHERE status = 'in_use'"
        ).fetchone()[0]
        dirty = conn.execute(
            "SELECT COUNT(*) FROM worktrees WHERE status = 'dirty'"
        ).fetchone()[0]

        sessions = conn.execute(
            """
            SELECT session_id, path FROM worktrees
            WHERE status = 'in_use' AND session_id IS NOT NULL
            """
        ).fetchall()

        return {
            "total": total,
            "available": available,
            "in_use": in_use,
            "dirty": dirty,
            "active_sessions": [
                {"session_id": row["session_id"], "path": row["path"]}
                for row in sessions
            ],
        }

    def cleanup(self) -> None:
        """Remove all worktrees and database entries."""
        conn = get_connection(self.db_path)

        rows = conn.execute("SELECT path FROM worktrees").fetchall()
        for row in rows:
            self._remove_worktree(Path(row["path"]))

        # Remove pool directory if empty
        if self.pool_dir.exists() and not any(self.pool_dir.iterdir()):
            self.pool_dir.rmdir()
