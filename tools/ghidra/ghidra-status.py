#!/usr/bin/env python3
"""
Check Ghidra MCP server status and list available binaries.

Usage:
    ghidra-status.py
    ghidra-status.py --reinit
"""

import argparse
import json
import sys
from pathlib import Path

# Add parent directory to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from mcp_client import MCPClient, MCPError, SESSION_CACHE_FILE

REPO = Path(__file__).resolve().parent.parent.parent


def _live_project_home() -> Path:
    """ghidra_projects/ is gitignored and lives only in the main checkout; resolve it from a worktree too."""
    import subprocess
    cand = REPO / "ghidra_projects/RB3Xenon/RB3Xenon"
    if cand.exists():
        return cand
    common = subprocess.run(["git", "rev-parse", "--path-format=absolute", "--git-common-dir"], cwd=REPO,
                            capture_output=True, text=True).stdout.strip()
    return Path(common).parent / "ghidra_projects/RB3Xenon/RB3Xenon" if common else cand


def staleness_line() -> str:
    """One line: what the live program was synced from, against today's symbol map.
    Written by tools/ghidra/ghidra_sync.sh (ghidra_sync_census.py --stamp) into the project home."""
    stamp_path = _live_project_home() / "sync_state.json"
    try:
        now_rows = sum(1 for k, v in json.load(open(REPO / "scripts/target_symbol_map.json")).items()
                       if k.lower().startswith("0x") and isinstance(v, str) and v)
    except Exception:
        now_rows = None
    if not stamp_path.exists():
        return f"Sync: no stamp at {stamp_path} (never synced by ghidra_sync.sh); map now {now_rows} rows"
    st = json.load(open(stamp_path))
    delta = "" if now_rows is None else f" (now {now_rows}, {now_rows - st.get('map_rows', 0):+d})"
    stale = now_rows is not None and now_rows != st.get("map_rows")
    return (f"Sync: {st.get('program')} {st.get('named')} named fns, {st.get('map_name_agrees')} agree with the "
            f"map / map {st.get('map_rows')} rows at sync{delta} / last sync {st.get('synced_at')} "
            f"at {st.get('repo_head')}" + ("  [STALE: re-run tools/ghidra/ghidra_sync.sh]" if stale else ""))


def main():
    parser = argparse.ArgumentParser(
        description="Check Ghidra MCP server status"
    )
    parser.add_argument(
        "--reinit",
        action="store_true",
        help="Force session re-initialization"
    )
    parser.add_argument(
        "--json", "-j",
        action="store_true",
        help="Output raw JSON"
    )

    args = parser.parse_args()

    try:
        client = MCPClient()

        # Check for cached session
        cached_session = None
        if SESSION_CACHE_FILE.exists() and not args.reinit:
            cached_session = SESSION_CACHE_FILE.read_text().strip()

        client.initialize(force=args.reinit)

        if args.json:
            result = {
                "status": "connected",
                "session_id": client.session_id,
                "was_cached": cached_session == client.session_id,
                "binaries": client.list_binaries(),
                "default_program": client.binary,
                "sync": staleness_line(),
            }
            print(json.dumps(result, indent=2))
        else:
            print("Ghidra MCP Status")
            print("=" * 40)
            print(f"Status: Connected")
            print(f"Session ID: {client.session_id}")
            if cached_session == client.session_id:
                print(f"Session: Using cached session")
            else:
                print(f"Session: New session created")

            print("")
            print("Available Binaries:")

            binaries = client.list_binaries()
            if isinstance(binaries, dict):
                bin_list = binaries.get("binaries", binaries.get("programs", []))
            elif isinstance(binaries, list):
                bin_list = binaries
            else:
                bin_list = [binaries]

            if bin_list:
                for b in bin_list:
                    if isinstance(b, dict):
                        name = b.get("name", b.get("path", str(b)))
                        print(f"  - {name}")
                    else:
                        print(f"  - {b}")
            else:
                print("  (none found)")
            print(f"Default program: {client.binary}")
            print(staleness_line())

    except MCPError as e:
        if args.json:
            print(json.dumps({"status": "error", "message": str(e)}, indent=2))
        else:
            print(f"Status: Not connected")
            print(f"Error: {e}")
        sys.exit(1)
    except KeyboardInterrupt:
        sys.exit(130)


if __name__ == "__main__":
    main()
