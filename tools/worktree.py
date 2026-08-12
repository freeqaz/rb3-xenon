#!/usr/bin/env python3
"""Build-ready git worktrees for this decomp project, and their lifecycle.

A plain `git worktree add` gives you the source and nothing else, which for this
repo is not enough to build or measure anything:

  * `build/` is gitignored, so a fresh worktree has no target objects, no
    compilers and no PCH.  Copying it byte-for-byte costs 5GB and a minute.
    On a COW filesystem it costs neither: `cp --reflink` shares every extent
    until one side writes, so a worktree starts at ~0 bytes exclusive and only
    grows by what its own build actually changes.  `ls` reports that number, so
    the cost of keeping a worktree around is visible rather than assumed.

  * `orig/` holds the retail image dtk splits the target objects out of.  It is
    read-only for every consumer and large, so it is shared by symlink -- per
    FILE, because a repo may track a `.gitkeep` inside it and then the directory
    already exists in the worktree.

  * configure.py defaults its tool paths to SIBLINGS of the repo
    (`../jeff/target/release/dtk`, `../objdiff/...`, `../wibo/...`).  Those
    resolve next to the parent and not next to a worktree nested three levels
    down, so ninja tries to rebuild toolchains that are not there -- and since
    the regen edge is a `generator`, it does not fail cleanly, it loops until
    "manifest still dirty after 100 tries".  They are pinned explicitly here and
    recorded in `configure_args`, so the regen edge stays consistent.

  * build.ninja embeds absolute output paths, so the first build in a worktree
    is a cold one no matter what is in `build/`.  That is ~30s here, and it is
    why `new` builds and then VERIFIES: a worktree whose numbers do not match
    the parent's is a silently wrong measurement, which is worse than a slow one.

  * Regenerating build.ninja means the worktree's graph is what configure.py
    produces TODAY, which is not necessarily what the parent's graph contains.
    Measured 2026-08-12 on dc3: the parent carries 967 `build/<ver>/data/*.obj`
    link inputs that a fresh configure emits none of.  Per-unit measurement is
    unaffected -- objdiff compares `build/<ver>/src/**` against
    `build/<ver>/obj/**` and never consults the link, which is why the verify
    step passes -- but do not do LINK-side work in a worktree without checking
    that edge against the parent first.

Usage:
    tools/worktree.py new <name> [--branch B] [--base REF] [--no-build]
    tools/worktree.py ls [--json]
    tools/worktree.py rm <name>... [--force] [--keep-branch]
    tools/worktree.py gc [--yes]

`rm` refuses a worktree with uncommitted changes or with commits that are not
in the default branch, unless forced.  `gc` removes every worktree that is both
clean and fully merged -- the ones whose work has already landed.
"""
import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import time
from pathlib import Path

WT_DIR = ".claude/worktrees"
# Sibling checkouts configure.py would otherwise resolve relative to the repo.
TOOL_PINS = [
    ("--dtk", "jeff/target/release/dtk"),
    ("--objdiff", "objdiff/target/release/objdiff-cli"),
    ("--wrapper", "wibo/build/release/wibo"),
]
# Gitignored, read-only, and too large to copy: shared by per-file symlink.
SHARED_ASSETS = ["orig"]


def git(*args, cwd=None, check=True):
    r = subprocess.run(["git", *args], cwd=cwd, capture_output=True, text=True)
    if check and r.returncode:
        sys.exit(f"git {' '.join(args)}: {r.stderr.strip()}")
    return r.stdout.strip()


def repo_root():
    here = Path(__file__).resolve().parent
    root = Path(git("rev-parse", "--show-toplevel", cwd=here))
    common = Path(git("rev-parse", "--path-format=absolute",
                      "--git-common-dir", cwd=here))
    if common.parent != root:
        sys.exit(f"run this from the primary checkout, not a worktree\n"
                 f"  primary: {common.parent}")
    return root


def default_branch(root):
    for ref in ("refs/remotes/origin/HEAD",):
        out = git("symbolic-ref", "--quiet", "--short", ref, cwd=root, check=False)
        if out:
            return out.split("/", 1)[1]
    for name in ("main", "master"):
        if git("rev-parse", "--verify", "--quiet", name, cwd=root, check=False):
            return name
    sys.exit("cannot determine the default branch")


def worktrees(root):
    """[(path, branch)] for every worktree under WT_DIR."""
    out, cur = [], {}
    for line in git("worktree", "list", "--porcelain", cwd=root).splitlines() + [""]:
        if not line:
            if cur.get("worktree"):
                p = Path(cur["worktree"])
                if WT_DIR in str(p):
                    out.append((p, cur.get("branch", "").replace("refs/heads/", "")
                                or "(detached)"))
            cur = {}
            continue
        k, _, v = line.partition(" ")
        cur[k] = v
    return out


def cow_usage(path):
    """(total_bytes, exclusive_bytes) -- exclusive is what removal actually frees."""
    r = subprocess.run(["btrfs", "filesystem", "du", "-s", "--raw", str(path)],
                       capture_output=True, text=True)
    if r.returncode:
        return None, None
    for line in r.stdout.splitlines()[1:]:
        f = line.split()
        if len(f) >= 3 and f[0].isdigit():
            return int(f[0]), int(f[1])
    return None, None


def human(n):
    if n is None:
        return "?"
    for unit in ("B", "K", "M", "G", "T"):
        if n < 1024 or unit == "T":
            return f"{n:.0f}{unit}" if unit == "B" else f"{n:.1f}{unit}"
        n /= 1024


def configure_flags(root):
    helptext = subprocess.run([sys.executable, "configure.py", "--help"], cwd=root,
                              capture_output=True, text=True).stdout
    args, siblings = [], root.parent
    for flag, rel in TOOL_PINS:
        target = siblings / rel
        if flag in helptext and target.exists():
            args += [flag, str(target)]
    return args


def measure(path, ruler="none"):
    """matched_code at a ruler, or None if objdiff cannot report."""
    out = Path("/tmp") / f"wt_measure_{os.getpid()}.json"
    cmd = ["objdiff-cli", "report", "generate", "-p", ".",
           "-c", f"functionRelocDiffs={ruler}", "-o", str(out)]
    if subprocess.run(cmd, cwd=path, capture_output=True).returncode:
        return None
    try:
        m = json.loads(out.read_text())["measures"]
        return m["matched_code_percent"], int(m["matched_code"])
    finally:
        out.unlink(missing_ok=True)


# ------------------------------------------------------------------ new
def cmd_new(root, args):
    name = args.name
    if not re.fullmatch(r"[\w.-]+", name):
        sys.exit("name may contain only letters, digits, dot, underscore, dash")
    wt = root / WT_DIR / name
    if wt.exists():
        sys.exit(f"{wt} already exists -- `rm` it first")
    branch = args.branch or name
    base = args.base or default_branch(root)

    git("worktree", "add", str(wt), "-b", branch, base, cwd=root)
    print(f"worktree  {wt}  [{branch}] from {base}")

    src = root / "build"
    if src.is_dir():
        t0 = time.time()
        r = subprocess.run(["cp", "-a", "--reflink=always", str(src), str(wt / "build")],
                           capture_output=True, text=True)
        if r.returncode:
            print("  note: no COW support here -- falling back to a full copy, "
                  "which costs real space and real time")
            shutil.copytree(src, wt / "build", symlinks=True)
        print(f"  build/    reflinked in {time.time() - t0:.1f}s "
              f"({human(cow_usage(wt / 'build')[0])} shared, ~0 exclusive)")

    for asset in SHARED_ASSETS:
        adir = root / asset
        if not adir.is_dir():
            continue
        n = 0
        for f in adir.rglob("*"):
            if not f.is_file():
                continue
            link = wt / f.relative_to(root)
            link.parent.mkdir(parents=True, exist_ok=True)
            if not link.exists():
                link.symlink_to(f)
                n += 1
        print(f"  {asset + '/':10s}{n} file(s) shared by symlink")

    flags = configure_flags(root)
    r = subprocess.run([sys.executable, "configure.py", *flags], cwd=wt,
                       capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"configure.py failed:\n{(r.stdout + r.stderr)[-2000:]}")
    print(f"  configure {' '.join(flags) or '(no tool pins needed)'}")

    if args.no_build:
        print("  (skipped build; run `ninja` in the worktree)")
        return
    t0 = time.time()
    r = subprocess.run(["ninja"], cwd=wt, capture_output=True, text=True)
    if r.returncode:
        sys.exit(f"cold build failed:\n{(r.stdout + r.stderr)[-3000:]}")
    print(f"  build     cold build in {time.time() - t0:.0f}s")

    here, there = measure(root), measure(wt)
    if here and there and here != there:
        sys.exit(f"VERIFY FAILED: parent measures {here[0]:.6f}% ({here[1]} bytes) "
                 f"and the worktree measures {there[0]:.6f}% ({there[1]} bytes).\n"
                 f"Do not trust numbers from this worktree.")
    if there:
        print(f"  verify    none ruler {there[0]:.6f}% -- matches the parent")


# ------------------------------------------------------------------ ls
def cmd_ls(root, args):
    dflt = default_branch(root)
    rows = []
    for wt, branch in worktrees(root):
        dirty = len(git("status", "--porcelain", cwd=wt, check=False).splitlines())
        ahead = git("rev-list", "--count", f"{dflt}..HEAD", cwd=wt, check=False) or "?"
        merged = ahead == "0"
        total, excl = cow_usage(wt)
        rows.append({"name": wt.name, "path": str(wt), "branch": branch,
                     "dirty": dirty, "ahead": ahead, "merged": merged,
                     "total_bytes": total, "exclusive_bytes": excl})
    if args.json:
        print(json.dumps(rows, indent=1))
        return
    if not rows:
        print("no worktrees")
        return
    print(f"{'name':18s} {'branch':34s} {'ahead':>5s} {'dirty':>5s} "
          f"{'shared':>8s} {'own':>8s}")
    for r in rows:
        print(f"{r['name']:18s} {r['branch']:34s} {r['ahead']:>5s} "
              f"{r['dirty']:>5d} {human(r['total_bytes']):>8s} "
              f"{human(r['exclusive_bytes']):>8s}")
    freeable = sum(r["exclusive_bytes"] or 0 for r in rows if r["merged"] and not r["dirty"])
    print(f"\n`own` is what removing the worktree actually frees; the rest is "
          f"shared with the parent.")
    if freeable:
        print(f"{human(freeable)} is held by worktrees that are clean and fully "
              f"merged -- `gc` reclaims it.")


# ------------------------------------------------------------------ rm / gc
def remove_one(root, wt, branch, dflt, force, keep_branch):
    dirty = git("status", "--porcelain", cwd=wt, check=False)
    ahead = git("rev-list", "--count", f"{dflt}..HEAD", cwd=wt, check=False)
    if not force:
        if dirty:
            print(f"  REFUSE {wt.name}: {len(dirty.splitlines())} uncommitted "
                  f"change(s).  Commit them, or pass --force.")
            return 0
        if ahead not in ("0", ""):
            print(f"  REFUSE {wt.name}: {ahead} commit(s) not in {dflt}.  Land the "
                  f"branch first, or pass --force.")
            return 0
    _, excl = cow_usage(wt)
    shutil.rmtree(wt / "build", ignore_errors=True)
    git("worktree", "remove", "--force", str(wt), cwd=root)
    note = ""
    if not keep_branch and branch != "(detached)":
        if git("branch", "-d", branch, cwd=root, check=False):
            note = f", branch {branch} deleted"
        else:
            note = f", branch {branch} KEPT (not merged into {dflt})"
    print(f"  removed {wt.name}: {human(excl)} reclaimed{note}")
    return excl or 0


def cmd_rm(root, args):
    dflt = default_branch(root)
    index = {wt.name: (wt, br) for wt, br in worktrees(root)}
    freed = 0
    for name in args.name:
        if name not in index:
            print(f"  no worktree named {name}")
            continue
        wt, br = index[name]
        if Path.cwd().is_relative_to(wt):
            print(f"  REFUSE {name}: you are standing in it")
            continue
        freed += remove_one(root, wt, br, dflt, args.force, args.keep_branch)
    print(f"{human(freed)} reclaimed")


def cmd_gc(root, args):
    dflt = default_branch(root)
    cand = []
    for wt, br in worktrees(root):
        if Path.cwd().is_relative_to(wt):
            continue
        if git("status", "--porcelain", cwd=wt, check=False):
            continue
        if git("rev-list", "--count", f"{dflt}..HEAD", cwd=wt, check=False) != "0":
            continue
        cand.append((wt, br))
    if not cand:
        print(f"nothing to collect -- every worktree is dirty or ahead of {dflt}")
        return
    total = sum(cow_usage(wt)[1] or 0 for wt, _ in cand)
    print(f"clean and fully merged into {dflt}, holding {human(total)}:")
    for wt, br in cand:
        print(f"  {wt.name}  [{br}]  {human(cow_usage(wt)[1])}")
    if not args.yes:
        print("\nre-run with --yes to remove them")
        return
    freed = sum(remove_one(root, wt, br, dflt, False, False) for wt, br in cand)
    print(f"{human(freed)} reclaimed")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    p = sub.add_parser("new", help="create a build-ready worktree")
    p.add_argument("name")
    p.add_argument("--branch", help="branch name (default: the worktree name)")
    p.add_argument("--base", help="base ref (default: the default branch)")
    p.add_argument("--no-build", action="store_true")
    p.set_defaults(fn=cmd_new)

    p = sub.add_parser("ls", help="list worktrees with what each one costs")
    p.add_argument("--json", action="store_true")
    p.set_defaults(fn=cmd_ls)

    p = sub.add_parser("rm", help="remove worktrees")
    p.add_argument("name", nargs="+")
    p.add_argument("--force", action="store_true",
                   help="remove even with uncommitted or unlanded work")
    p.add_argument("--keep-branch", action="store_true")
    p.set_defaults(fn=cmd_rm)

    p = sub.add_parser("gc", help="remove every clean, fully merged worktree")
    p.add_argument("--yes", action="store_true")
    p.set_defaults(fn=cmd_gc)

    args = ap.parse_args()
    args.fn(repo_root(), args)


if __name__ == "__main__":
    main()
