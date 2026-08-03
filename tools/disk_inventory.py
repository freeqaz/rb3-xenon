#!/usr/bin/env python3
"""READ-ONLY disk / worktree inventory for the rb3-xenon box.

Written by lane DP-4 (2026-08-03) to diagnose btrfs metadata pressure and
tmpfs quota exhaustion. This script MUTATES NOTHING: it only runs read-only
git plumbing and stats files. It is NOT a build input (nothing in
configure.py / tools/project.py / build.ninja references it).

    python3 tools/disk_inventory.py               # classified worktree table
    python3 tools/disk_inventory.py --json OUT    # machine-readable
    python3 tools/disk_inventory.py --fs          # filesystem headroom only

WHAT THE NUMBERS MEAN (read this before quoting any of them)
------------------------------------------------------------
* `du` measures DATA (and for btrfs CoW reflinks it counts shared extents
  once PER FILE, so it OVERSTATES unique data for a worktree fleet).
  `du` DOES NOT MEASURE METADATA. Never present a du figure as an answer to
  a metadata question.
* The metadata proxy used here is FILE COUNT. btrfs metadata cost scales with
  inodes + extent items + reflink backrefs, all of which track file count far
  better than they track bytes.
* `btrfs filesystem df` "Metadata used/total" is the fill level of ALREADY
  ALLOCATED metadata chunks. It hitting ~95% is NOT by itself an emergency:
  btrfs allocates a new chunk from UNALLOCATED device space on demand. The
  real cliff is UNALLOCATED -> 0. This script reports unallocated explicitly
  because the fill-percentage alone is the classic btrfs misreading.
* Metadata is DUP: 1 GiB of logical metadata consumes 2 GiB of device space.
"""

from __future__ import annotations

import argparse
import json
import os
import re
import subprocess
import sys
import time

REPOS = [
    "/home/free/code/milohax/rb3-xenon",
    "/home/free/code/milohax/rb3",
    "/home/free/code/milohax/dc3-decomp",
    "/home/free/code/milohax/milo-native-engine",
    "/home/free/code/milohax/wibo",
    "/home/free/code/milohax/jeff",
    "/home/free/code/milohax/objdiff",
    "/home/free/code/milohax/objcache",
]

NOW = time.time()
DAY = 86400.0


def sh(args, cwd=None, timeout=60):
    try:
        r = subprocess.run(args, cwd=cwd, capture_output=True, text=True, timeout=timeout)
        return r.returncode, r.stdout.strip(), r.stderr.strip()
    except (subprocess.TimeoutExpired, OSError) as e:
        return -1, "", str(e)


# --------------------------------------------------------------- filesystem

def fs_headroom(path="/home"):
    """Report btrfs allocation state, distinguishing FILL from HEADROOM."""
    rc, out, _ = sh(["btrfs", "filesystem", "usage", "--raw", path])
    info = {}
    if rc == 0:
        for pat, key in (
            (r"Device size:\s+(\d+)", "device_size"),
            (r"Device allocated:\s+(\d+)", "device_allocated"),
            (r"Device unallocated:\s+(\d+)", "device_unallocated"),
            (r"Free \(estimated\):\s+(\d+)", "free_estimated"),
        ):
            m = re.search(pat, out)
            if m:
                info[key] = int(m.group(1))
        m = re.search(r"Data,single: Size:(\d+), Used:(\d+)", out)
        if m:
            info["data_size"], info["data_used"] = int(m.group(1)), int(m.group(2))
        m = re.search(r"Metadata,DUP: Size:(\d+), Used:(\d+)", out)
        if m:
            info["meta_size"], info["meta_used"] = int(m.group(1)), int(m.group(2))
    if "meta_size" in info and "device_unallocated" in info:
        # DUP => each logical metadata byte costs 2 device bytes
        info["meta_growth_headroom_logical"] = info["device_unallocated"] // 2
        info["meta_fill_pct"] = 100.0 * info["meta_used"] / info["meta_size"]
    return info


def tmpfs_quota():
    """Per-user tmpfs quota -- INVISIBLE TO df, and the true cause of EDQUOT."""
    rc, out, _ = sh(["quota", "-v"])
    rows = []
    for ln in out.splitlines():
        f = ln.split()
        if len(f) >= 4 and f[0] in ("tmpfs",) and f[1].rstrip("*").isdigit():
            blocks = int(f[1].rstrip("*"))
            limit = int(f[3]) if f[3].isdigit() else 0
            rows.append({"fs": f[0], "kib_used": blocks, "kib_limit": limit,
                         "pct": (100.0 * blocks / limit) if limit else None})
    return rows


# --------------------------------------------------------------- worktrees

def list_worktrees(repo):
    rc, out, _ = sh(["git", "worktree", "list", "--porcelain"], cwd=repo)
    if rc != 0:
        return []
    wts, cur = [], {}
    for ln in out.splitlines():
        if ln.startswith("worktree "):
            if cur:
                wts.append(cur)
            cur = {"path": ln[9:], "repo": repo}
        elif ln.startswith("HEAD "):
            cur["head"] = ln[5:]
        elif ln.startswith("branch "):
            cur["branch"] = ln[7:].replace("refs/heads/", "")
        elif ln == "detached":
            cur["branch"] = "(detached)"
        elif ln.startswith("prunable"):
            cur["prunable"] = True
        elif ln.startswith("locked"):
            cur["locked"] = True
    if cur:
        wts.append(cur)
    return wts


def merged_branches(repo, base="main"):
    rc, out, _ = sh(["git", "branch", "--merged", base], cwd=repo)
    if rc != 0:
        return set()
    return {ln.strip().lstrip("* ").strip() for ln in out.splitlines() if ln.strip()}


def active_paths():
    """Directories currently in use by a live process (cwd or open fd)."""
    act = set()
    for p in os.listdir("/proc"):
        if not p.isdigit():
            continue
        for probe in ("cwd",):
            try:
                t = os.readlink(f"/proc/{p}/{probe}")
            except OSError:
                continue
            if t.startswith("/home/free/tmp/"):
                act.add(t)
    # also anything a running command line mentions
    for p in os.listdir("/proc"):
        if not p.isdigit():
            continue
        try:
            with open(f"/proc/{p}/cmdline", "rb") as f:
                cl = f.read().decode("utf8", "replace").replace("\0", " ")
        except OSError:
            continue
        for m in re.finditer(r"/home/free/tmp/[A-Za-z0-9_.\-]+", cl):
            act.add(m.group(0))
    return act


def dir_mtime(path):
    """Most recent activity signal we can get cheaply."""
    best = 0.0
    for probe in (path, os.path.join(path, ".git")):
        try:
            best = max(best, os.stat(probe).st_mtime)
        except OSError:
            pass
    return best


def classify(wt, merged, active, dirty, ahead):
    """Reclamation class. NOTE: merged-ness is a WEAK signal on this box --
    project memory records 'UNMERGED != UNLANDED, lanes land by PATCH'.
    So a branch being unmerged does NOT mean the work is unsaved, and a
    branch being merged does NOT prove the worktree is finished."""
    path = wt["path"]
    if any(a == path or a.startswith(path + "/") or path.startswith(a + "/") for a in active):
        return "HOLD-ACTIVE", "a live process is using this path"
    if wt.get("locked"):
        return "HOLD-LOCKED", "git worktree lock is set"
    if dirty:
        return "REVIEW-DIRTY", f"{dirty} uncommitted change(s) -- unlanded work may live here"
    age = (NOW - wt.get("mtime", NOW)) / DAY
    if age < 2:
        return "HOLD-RECENT", f"touched {age:.1f}d ago -- lane may only be idle, not finished"
    if wt.get("branch") in merged and not ahead:
        return "SAFE-MERGED", f"clean, merged into main, idle {age:.0f}d"
    if ahead:
        return "REVIEW-AHEAD", f"{ahead} commit(s) not on main, idle {age:.0f}d"
    return "REVIEW-CLEAN", f"clean but unmerged, idle {age:.0f}d"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", metavar="OUT")
    ap.add_argument("--fs", action="store_true", help="filesystem headroom only")
    ap.add_argument("--repo", action="append", help="limit to these repos")
    ap.add_argument("--no-status", action="store_true",
                    help="skip per-worktree git status (faster, loses dirty flag)")
    args = ap.parse_args()

    fs = fs_headroom()
    qs = tmpfs_quota()

    print("=" * 74)
    print("FILESYSTEM HEADROOM  (/home, btrfs)")
    print("=" * 74)
    g = 1 << 30
    if fs:
        print(f"  data      {fs.get('data_used',0)/g:9.1f} GiB used / {fs.get('data_size',0)/g:.1f} GiB allocated")
        print(f"  metadata  {fs.get('meta_used',0)/g:9.1f} GiB used / {fs.get('meta_size',0)/g:.1f} GiB allocated"
              f"  ({fs.get('meta_fill_pct',0):.1f}% FILL)")
        print(f"  UNALLOCATED (the number that actually matters):"
              f" {fs.get('device_unallocated',0)/g:.1f} GiB")
        print(f"    -> room to grow metadata by ~{fs.get('meta_growth_headroom_logical',0)/g:.0f} GiB"
              f" logical (DUP: 2x device cost)")
        print("  NOTE: metadata FILL% high + UNALLOCATED large  ==  NOT an ENOSPC emergency.")
        print("        The btrfs read-only cliff needs UNALLOCATED ~ 0.")
    else:
        print("  (btrfs usage unavailable)")

    print()
    print("=" * 74)
    print("TMPFS PER-USER QUOTA  (invisible to df -- the real cause of EDQUOT)")
    print("=" * 74)
    for r in qs:
        if r["pct"] is not None:
            print(f"  {r['fs']:8s} {r['kib_used']/1048576:7.2f} GiB used /"
                  f" {r['kib_limit']/1048576:.2f} GiB quota  ({r['pct']:.1f}%)")
    print("  zsh heredocs use $TMPPREFIX (default /tmp/zsh), NOT $TMPDIR --")
    print("  so heredocs are charged against this quota, not against /home.")

    if args.fs:
        return 0

    repos = args.repo or REPOS
    active = active_paths()
    rows = []
    for repo in repos:
        if not os.path.exists(repo):
            continue
        merged = merged_branches(repo)
        for wt in list_worktrees(repo):
            if os.path.realpath(wt["path"]) == os.path.realpath(repo):
                continue  # the primary checkout, never a reclamation target
            wt["mtime"] = dir_mtime(wt["path"])
            wt["exists"] = os.path.isdir(wt["path"])
            dirty = 0
            ahead = 0
            if wt["exists"] and not args.no_status:
                rc, out, _ = sh(["git", "status", "--porcelain"], cwd=wt["path"], timeout=30)
                dirty = len([l for l in out.splitlines() if l.strip()]) if rc == 0 else 0
                rc, out, _ = sh(["git", "rev-list", "--count", "main..HEAD"],
                                cwd=wt["path"], timeout=30)
                ahead = int(out) if rc == 0 and out.isdigit() else 0
            wt["dirty"], wt["ahead"] = dirty, ahead
            wt["cls"], wt["why"] = classify(wt, merged, active, dirty, ahead)
            wt["age_days"] = (NOW - wt["mtime"]) / DAY
            rows.append(wt)

    print()
    print("=" * 74)
    print(f"WORKTREE INVENTORY  ({len(rows)} worktrees across {len(repos)} repos)")
    print("=" * 74)
    by = {}
    for r in rows:
        by.setdefault(r["cls"], []).append(r)
    order = ["HOLD-ACTIVE", "HOLD-LOCKED", "HOLD-RECENT", "REVIEW-DIRTY",
             "REVIEW-AHEAD", "REVIEW-CLEAN", "SAFE-MERGED"]
    for c in order:
        v = by.get(c, [])
        if not v:
            continue
        ages = sorted(x["age_days"] for x in v)
        print(f"  {c:14s} {len(v):5d}   age median {ages[len(ages)//2]:6.1f}d"
              f"  max {ages[-1]:6.1f}d")
    for c, v in sorted(by.items()):
        if c not in order:
            print(f"  {c:14s} {len(v):5d}")

    print()
    print("  Per-repo:")
    per = {}
    for r in rows:
        per.setdefault(r["repo"], []).append(r)
    for repo, v in sorted(per.items(), key=lambda x: -len(x[1])):
        print(f"    {os.path.basename(repo):22s} {len(v):5d}")

    print()
    print("  CAVEAT: 'SAFE-MERGED' is a starting point for review, NOT an")
    print("  authorization to delete. On this box lanes land by PATCH, so")
    print("  UNMERGED != UNLANDED and MERGED != FINISHED. A completion notice")
    print("  means 'idle', not 'done'. Confirm with the lane owner.")

    if args.json:
        with open(args.json, "w") as f:
            json.dump({"fs": fs, "tmpfs_quota": qs, "worktrees": rows}, f, indent=1)
        print(f"\n  wrote {args.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
