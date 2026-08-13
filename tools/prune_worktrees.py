#!/usr/bin/env python3
"""Reclaim disk from idle lane worktrees — conservatively, and reversibly.

WHY THIS EXISTS
---------------
Lanes here work in throwaway worktrees under ``~/tmp`` (see ``scripts/
setup_worktree.sh``). Nothing ever removed them, so they accumulated: 202
registered rb3-xenon worktrees holding ~412 GB when this tool was written.

THE TWO COUNTER-INTUITIVE FACTS THIS TOOL IS BUILT ON
-----------------------------------------------------
1. **``git worktree remove`` does NOT delete the branch.** Verified empirically
   before this tool was written: commit into a scratch worktree, remove the
   worktree with ``--force``, and the branch ref, the commit sha and the file
   contents all still resolve. So *unlanded commits are not a reason to keep a
   directory* — they live in the object store, not in the checkout. The only
   irrecoverable loss is **uncommitted** content: dirty tracked files, and
   untracked-but-not-ignored files.

   ⚠ Untracked files are frequently a lane's *entire* deliverable (a new
   ``tools/*.py``). "No tracked modifications" is NOT "clean". This tool always
   checks ``--untracked-files=all`` as well, and ``git worktree remove --force``
   will happily delete an untracked-only deliverable without a word of warning
   — that check is ours to make, not git's.

2. **``git branch --merged`` is a USELESS signal in this repo and is
   deliberately not used.** Lanes land by patch, so a fully-landed branch still
   reads as unmerged. This tool reports patch-id equivalence (``git cherry``)
   as *information only* — landedness never gates removal, because per (1)
   nothing is lost either way.

⛔ This tool NEVER deletes a branch. Branches are cheap and hold the history
   that makes the log worth reading. It removes worktree *directories* only.

POLICY
------
Default (conservative). A worktree is removable iff ALL hold:

  * not protected (main repo, ``--protect`` paths, foreign roots);
  * ``.git`` present (a worktree whose ``.git`` was moved away by a killed
    ``--agent-tools`` run is left alone — see CLAUDE.md);
  * no dirty tracked files;
  * no untracked non-ignored files;
  * not modified within ``--min-age-hours`` (default 2), by recursive newest
    mtime over the whole tree plus git's own admin dir;
  * if detached HEAD: its commit is reachable from some ref (otherwise removal
    could orphan it to gc — the one case where removal *can* lose commits).

Opt-in escalations, each OFF by default:

  * ``--allow-dirty-generated`` — permit removal when the ONLY dirty tracked
    paths are known **build side-effects**. The allowlist is deliberately tiny:

        config/45410914/symbols.txt

    Justification: every split run rewrites ``symbols.txt``, so any worktree
    that has ever built carries a diff there that no human authored. It is a
    generated artifact, regenerated from the XEX on the next split.

    ⛔ ``scripts/target_symbol_map.json`` and ``config/45410914/splits.txt`` are
    NOT on the allowlist and must never be added: uncommitted map/splits edits
    are a lane's actual *deliverable* in this project, not drift.

  * ``--older-than DAYS`` — additionally require the tree to be untouched for
    that many days.

``--dry-run`` is the DEFAULT. Removal requires an explicit ``--execute``.

ARCHIVE
-------
Every run writes a manifest (JSON + TSV) under ``--archive-dir`` recording, for
every worktree considered: path, branch, tip sha, landed/unlanded counts, dirty
tracked list, untracked non-ignored list, size, decision and reason. Before
removing a tree that carries ANY uncommitted content, its ``git diff`` and a
tar of its untracked non-ignored files are saved there first. That archive is
the only recovery a mistake can have, and it costs almost nothing.

LIVENESS RACE
-------------
Lanes spawn while this runs. Every candidate is **re-probed immediately before
its own removal** (mtime, dirty, untracked, protect set) and skipped if
anything changed. House rule: a completion notice means "idle", not "finished"
— err toward keeping.

EXAMPLES
--------
    python3 tools/prune_worktrees.py                      # dry-run inventory
    python3 tools/prune_worktrees.py --protect ~/tmp/wt-live --execute
    python3 tools/prune_worktrees.py --allow-dirty-generated --execute
"""

from __future__ import annotations

import argparse
import datetime as dt
import json
import os
import shlex
import subprocess
import sys
import tarfile
import time

# Dirty tracked paths that are pure build side-effects. See module docstring
# for why this list is tiny and why map/splits edits may never join it.
GENERATED_DIRTY_ALLOWLIST = {
    "config/45410914/symbols.txt",
}

# Sibling checkouts that own worktrees of THIS repo but are not ours to manage
# (decomp-synth stages bundles inside its own tree). Resolved relative to the
# repo at runtime so no absolute user path is baked into the source.
FOREIGN_SIBLINGS = ["decomp-synth"]


def default_foreign_roots(repo):
    parent = os.path.dirname(os.path.abspath(repo))
    return [os.path.join(parent, name) + os.sep for name in FOREIGN_SIBLINGS]


def run(args, cwd=None, check=False, timeout=300):
    """Run a command, return (rc, stdout, stderr) with text decoding."""
    p = subprocess.run(
        args, cwd=cwd, capture_output=True, text=True, timeout=timeout
    )
    if check and p.returncode != 0:
        raise RuntimeError(
            "command failed (%d): %s\n%s" % (p.returncode, shlex.join(args), p.stderr)
        )
    return p.returncode, p.stdout, p.stderr


def git(repo, *args, **kw):
    # --no-optional-locks: never let our own probing refresh/write the index,
    # which would destroy the mtime liveness signal we are about to read.
    return run(["git", "--no-optional-locks", "-C", repo, *args], **kw)


def list_worktrees(repo):
    rc, out, err = git(repo, "worktree", "list", "--porcelain", check=True)
    entries, cur = [], {}
    for line in out.splitlines():
        if not line.strip():
            if cur:
                entries.append(cur)
                cur = {}
            continue
        key, _, val = line.partition(" ")
        cur[key] = val
    if cur:
        entries.append(cur)
    return entries


def newest_mtime(path, files_only=False):
    """Newest mtime anywhere in the tree (single filesystem), as epoch float.

    Uses find(1) rather than os.walk: ~0.05 s over a 32k-file worktree, versus
    seconds for a Python-level stat of every entry across 200 trees.

    ``files_only`` exists because of a measured trap: the mtime of git's own
    admin DIRECTORY (``.git/worktrees/<name>/``) is bumped for EVERY worktree
    simultaneously by git housekeeping (a plain ``git worktree list`` did it to
    all 209 here). Reading it made all 209 trees look "modified 0.1 h ago" — a
    liveness gate that cannot fail, which is worse than no gate. The FILES
    inside that dir (``index``, ``HEAD``, ``logs/HEAD``) are honest signals.
    """
    cmd = ["find", path, "-xdev"]
    if files_only:
        # `index` is excluded on purpose: it is a CACHE that any observer can
        # refresh (a status run by a pool poller, another agent, or an earlier
        # pass of this very tool), so its mtime is evidence that somebody
        # LOOKED at the tree, not that a lane WORKED in it. Trusting it made 27
        # trees read "modified 0.1 h ago" in lockstep. HEAD and logs/HEAD move
        # only on real ref changes and are kept.
        cmd += ["-type", "f", "!", "-name", "index", "!", "-name", "index.lock"]
    rc, out, _ = run(cmd + ["-printf", "%T@\\n"], timeout=600)
    best = 0.0
    for line in out.splitlines():
        try:
            v = float(line)
        except ValueError:
            continue
        if v > best:
            best = v
    return best


def dir_size(path):
    rc, out, _ = run(["du", "-sxB1", path], timeout=1800)
    if rc == 0 and out.split():
        try:
            return int(out.split()[0])
        except ValueError:
            pass
    return 0


def probe(repo, wt, base_ref, want_size=True):
    """Collect every fact a decision needs about one worktree."""
    path = wt["worktree"]
    info = {
        "path": path,
        "branch": None,
        "detached": "detached" in wt,
        "head": wt.get("HEAD"),
        "exists": os.path.isdir(path),
        "git_ok": False,
        "dirty_tracked": [],
        "untracked": [],
        "unlanded": None,
        "landed": None,
        "newest_mtime": 0.0,
        "size_bytes": 0,
        "errors": [],
    }
    ref = wt.get("branch")
    if ref:
        info["branch"] = ref[len("refs/heads/") :] if ref.startswith("refs/heads/") else ref

    if not info["exists"]:
        info["errors"].append("path missing (prunable)")
        return info

    dotgit = os.path.join(path, ".git")
    info["git_ok"] = os.path.exists(dotgit)
    if not info["git_ok"]:
        # A killed decomp-synth --agent-tools run moves .git to a sidecar.
        # Never touch these; they need `git worktree repair`, not removal.
        info["errors"].append(".git missing (killed agent-tools run?)")

    info["newest_mtime"] = newest_mtime(path)
    # git's own admin dir also records activity (index/HEAD/logs writes).
    # Read it from the worktree's .git FILE ("gitdir: <admin>") rather than
    # guessing basename(path): nested worktrees such as ~/tmp/laneX/wt have a
    # basename ("wt") that does not name their admin dir at all.
    admin = ""
    if os.path.isfile(dotgit):
        try:
            with open(dotgit) as fh:
                head = fh.read(4096)
            for line in head.splitlines():
                if line.startswith("gitdir:"):
                    admin = line.split(":", 1)[1].strip()
                    break
        except OSError as exc:
            info["errors"].append("cannot read .git: %s" % exc)
    info["admin_dir"] = admin
    if admin and os.path.isdir(admin):
        adm = newest_mtime(admin, files_only=True)
        info["newest_mtime"] = max(info["newest_mtime"], adm)

    if info["git_ok"]:
        rc, out, err = git(path, "status", "--porcelain", "--untracked-files=all")
        if rc != 0:
            info["errors"].append("status failed: %s" % err.strip()[:200])
        else:
            for line in out.splitlines():
                if not line:
                    continue
                code, name = line[:2], line[3:]
                # Handle rename entries "R  old -> new"
                if " -> " in name:
                    name = name.split(" -> ", 1)[1]
                name = name.strip().strip('"')
                if code == "??":
                    info["untracked"].append(name)
                else:
                    info["dirty_tracked"].append(name)

        if info["branch"]:
            # Information only. `git cherry` marks '+' = not upstream (unlanded
            # by patch-id), '-' = already applied upstream (landed by patch).
            rc, out, _ = git(path, "cherry", base_ref, info["branch"])
            if rc == 0:
                info["unlanded"] = sum(1 for ln in out.splitlines() if ln.startswith("+"))
                info["landed"] = sum(1 for ln in out.splitlines() if ln.startswith("-"))

    if want_size:
        info["size_bytes"] = dir_size(path)
    return info


def commit_reachable(repo, sha):
    """Is this commit reachable from any ref? (detached-HEAD safety.)"""
    if not sha:
        return False
    rc, out, _ = git(repo, "branch", "--all", "--contains", sha)
    if rc == 0 and out.strip():
        return True
    rc, out, _ = git(repo, "tag", "--contains", sha)
    return rc == 0 and bool(out.strip())


def decide(repo, info, args, protect_set, now):
    """Return (removable, reason). Reasons are stable strings for reporting."""
    path = info["path"]
    if path in protect_set:
        return False, "protected:explicit"
    for root in args.foreign_root:
        if path.startswith(root):
            return False, "protected:foreign-root"
    if not info["exists"]:
        return False, "missing:needs-worktree-prune"
    if not info["git_ok"]:
        return False, "keep:no-git-file"
    if info["errors"] and not info["git_ok"]:
        return False, "keep:probe-error"

    age_h = (now - info["newest_mtime"]) / 3600.0 if info["newest_mtime"] else 1e9
    if age_h < args.min_age_hours:
        return False, "keep:active-%.1fh" % age_h
    if args.older_than is not None and age_h < args.older_than * 24:
        return False, "keep:younger-than-%dd" % args.older_than

    if info["untracked"]:
        return False, "keep:untracked-work(%d)" % len(info["untracked"])

    if info["dirty_tracked"]:
        if args.allow_dirty_generated and all(
            p in GENERATED_DIRTY_ALLOWLIST for p in info["dirty_tracked"]
        ):
            pass  # generated-only drift, escalation enabled
        else:
            return False, "keep:dirty-tracked(%d)" % len(info["dirty_tracked"])

    if info["detached"] and not commit_reachable(repo, info["head"]):
        # Removing this would leave the commit with no ref holding it alive.
        return False, "keep:detached-unreachable"

    return True, "remove"


def human(n):
    for unit in ("B", "KB", "MB", "GB", "TB"):
        if abs(n) < 1024 or unit == "TB":
            return "%.1f%s" % (n, unit)
        n /= 1024.0


def rescue_slug(path):
    """Collision-free archive dir name for a worktree path.

    NOT basename(): nested lane trees are overwhelmingly named ``<lane>/wt``,
    so basename collides and each rescue would silently overwrite the previous
    one — data loss in the mechanism that exists to prevent data loss.
    """
    return path.strip("/").replace("/", "_")


def archive_uncommitted(repo, info, dest):
    """Save git diff + untracked files before removing a tree that has them."""
    os.makedirs(dest, exist_ok=True)
    path = info["path"]
    rc, out, _ = git(path, "diff", "HEAD")
    with open(os.path.join(dest, "uncommitted.patch"), "w") as fh:
        fh.write(out)
    with open(os.path.join(dest, "untracked.list"), "w") as fh:
        fh.write("\n".join(info["untracked"]) + "\n")
    if info["untracked"]:
        with tarfile.open(os.path.join(dest, "untracked.tar.gz"), "w:gz") as tf:
            for rel in info["untracked"]:
                full = os.path.join(path, rel)
                if os.path.exists(full):
                    tf.add(full, arcname=rel)


def main(argv=None):
    ap = argparse.ArgumentParser(
        description="Prune idle lane worktrees. Dry-run by default; never deletes branches.",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--repo", default=os.getcwd(), help="main repo (default: cwd)")
    ap.add_argument("--base-ref", default="main", help="ref for landedness info (default: main)")
    ap.add_argument(
        "--protect", action="append", default=[],
        help="worktree path to never touch (repeatable). Live lanes go here.",
    )
    ap.add_argument(
        "--protect-file", default=None,
        help="file of worktree paths to protect, one per line ('#' comments ok)",
    )
    ap.add_argument(
        "--foreign-root", action="append", default=None,
        help="path prefix owned by another project; never touched (repeatable). "
             "Defaults to sibling checkouts: %s" % ", ".join(FOREIGN_SIBLINGS),
    )
    ap.add_argument(
        "--min-age-hours", type=float, default=2.0,
        help="skip trees modified within this many hours (default: 2)",
    )
    ap.add_argument("--older-than", type=int, default=None, help="only remove trees idle N+ days")
    ap.add_argument(
        "--allow-dirty-generated", action="store_true",
        help="permit removal when the only dirty tracked paths are generated build artifacts",
    )
    ap.add_argument("--no-size", action="store_true", help="skip du (faster inventory)")
    ap.add_argument(
        "--archive-dir",
        default=os.path.join(os.path.expanduser("~"), "tmp", "worktree-prune-archive"),
        help="where manifests and rescued uncommitted work are written",
    )
    ap.add_argument("--execute", action="store_true", help="actually remove (default: dry-run)")
    ap.add_argument("--limit", type=int, default=None, help="cap removals this run")
    args = ap.parse_args(argv)

    repo = os.path.abspath(args.repo)
    if not args.foreign_root:
        args.foreign_root = default_foreign_roots(repo)
    protect_set ={os.path.abspath(os.path.expanduser(p)).rstrip("/") for p in args.protect}
    if args.protect_file:
        with open(args.protect_file) as fh:
            for line in fh:
                line = line.split("#", 1)[0].strip()
                if line:
                    protect_set.add(os.path.abspath(os.path.expanduser(line)).rstrip("/"))
    protect_set.add(repo)

    stamp = dt.datetime.now().strftime("%Y%m%d-%H%M%S")
    run_dir = os.path.join(os.path.expanduser(args.archive_dir), stamp)
    os.makedirs(run_dir, exist_ok=True)

    entries = list_worktrees(repo)
    # The first entry is the main worktree; protect it unconditionally.
    if entries:
        protect_set.add(os.path.abspath(entries[0]["worktree"]).rstrip("/"))

    print("repo=%s  worktrees=%d  mode=%s" % (repo, len(entries), "EXECUTE" if args.execute else "DRY-RUN"))
    print("archive=%s" % run_dir)
    print("protected=%d  min_age_hours=%s  allow_dirty_generated=%s"
          % (len(protect_set), args.min_age_hours, args.allow_dirty_generated))
    print()

    now = time.time()
    records = []
    for wt in entries:
        info = probe(repo, wt, args.base_ref, want_size=not args.no_size)
        removable, reason = decide(repo, info, args, protect_set, now)
        info["decision"] = "REMOVE" if removable else "KEEP"
        info["reason"] = reason
        records.append(info)

    def write_manifest():
        with open(os.path.join(run_dir, "manifest.json"), "w") as fh:
            json.dump({"run": stamp, "repo": repo, "args": vars(args), "worktrees": records},
                      fh, indent=1)
        with open(os.path.join(run_dir, "manifest.tsv"), "w") as fh:
            fh.write("decision\treason\tpath\tbranch\thead\tunlanded\tlanded\tdirty\t"
                     "untracked\tsize_bytes\tidle_hours\n")
            for r in records:
                idle = (now - r["newest_mtime"]) / 3600.0 if r["newest_mtime"] else -1
                fh.write("\t".join([
                    r["decision"], r["reason"], r["path"], r["branch"] or "(detached)",
                    (r["head"] or "")[:12], str(r["unlanded"]), str(r["landed"]),
                    ",".join(r["dirty_tracked"]) or "-",
                    ",".join(r["untracked"]) or "-",
                    str(r["size_bytes"]), "%.1f" % idle,
                ]) + "\n")

    write_manifest()

    cands = [r for r in records if r["decision"] == "REMOVE"]
    keeps = [r for r in records if r["decision"] == "KEEP"]
    reclaim = sum(r["size_bytes"] for r in cands)

    print("CANDIDATES: %d worktrees, %s reclaimable" % (len(cands), human(reclaim)))
    by_reason = {}
    for r in keeps:
        by_reason.setdefault(r["reason"].split("(")[0].split("-")[0]
                             if r["reason"].startswith("keep:active") else r["reason"].split("(")[0],
                             []).append(r)
    print("KEEPING: %d" % len(keeps))
    for reason in sorted(by_reason, key=lambda k: -len(by_reason[k])):
        rs = by_reason[reason]
        print("  %-34s %4d  %s" % (reason, len(rs), human(sum(x["size_bytes"] for x in rs))))
    print()

    if not args.execute:
        print("DRY-RUN — nothing removed. Re-run with --execute to remove.")
        print("Manifest: %s/manifest.tsv" % run_dir)
        return 0

    removed, freed = 0, 0
    for r in cands:
        if args.limit is not None and removed >= args.limit:
            break
        path = r["path"]
        # RE-PROBE immediately before removal: lanes spawn while we work.
        fresh = probe(repo, {"worktree": path,
                             "branch": ("refs/heads/" + r["branch"]) if r["branch"] else None,
                             "HEAD": r["head"],
                             **({"detached": ""} if r["detached"] else {})},
                      args.base_ref, want_size=False)
        ok, why = decide(repo, fresh, args, protect_set, time.time())
        if not ok:
            print("SKIP  %-58s changed since scan: %s" % (path, why))
            r["decision"], r["reason"] = "KEEP", "skipped-at-removal:" + why
            continue

        branch, head = r["branch"], r["head"]
        if fresh["dirty_tracked"] or fresh["untracked"]:
            archive_uncommitted(repo, fresh, os.path.join(run_dir, "rescued", rescue_slug(path)))

        rc, out, err = run(["git", "-C", repo, "worktree", "remove", "--force", path], timeout=600)
        if rc != 0:
            print("FAIL  %-58s %s" % (path, err.strip()[:160]))
            r["decision"], r["reason"] = "KEEP", "remove-failed"
            continue

        # ★ Verify nothing was lost: the branch and the tip commit must still resolve.
        if branch:
            vrc, vout, verr = git(repo, "rev-parse", "--verify", "refs/heads/" + branch)
            if vrc != 0:
                write_manifest()
                print("\n⛔ STOP: branch %r no longer resolves after removing %s\n%s"
                      % (branch, path, verr))
                return 3
        if head:
            vrc, _, verr = git(repo, "rev-parse", "--verify", head + "^{commit}")
            if vrc != 0:
                write_manifest()
                print("\n⛔ STOP: commit %s no longer resolves after removing %s\n%s"
                      % (head, path, verr))
                return 3

        removed += 1
        freed += r["size_bytes"]
        r["decision"], r["reason"] = "REMOVED", "removed;branch-verified"
        print("OK    %-58s %8s  branch %s intact" % (path, human(r["size_bytes"]), branch or "(detached)"))

    run(["git", "-C", repo, "worktree", "prune"])
    write_manifest()
    print("\nREMOVED %d worktrees, freed %s. 0 branches deleted." % (removed, human(freed)))
    print("Manifest: %s/manifest.tsv" % run_dir)
    return 0


if __name__ == "__main__":
    sys.exit(main())
