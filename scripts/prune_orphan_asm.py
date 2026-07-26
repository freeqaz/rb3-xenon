#!/usr/bin/env python3
"""Delete orphaned `build/<title>/asm/*.s` files -- the stale-carve trap.

WHY THIS EXISTS
---------------
dtk (jeff) writes one `.s` per split unit as a *reference-only side effect*.
`build.ninja` declares only `build/45410914/config.json` as the split rule's
output (`grep -c 45410914/asm build.ninja` -> 0), so:

  * ninja never cleans a `.s`,
  * jeff never sweeps the directory (plain `File::create` per unit,
    `jeff/src/cmd/xex.rs`),
  * a unit removed from `config/45410914/splits.txt` leaves its `.s` behind
    **forever**, frozen at whatever binary geometry was current when it was
    last written.

Measured 2026-07-26 (laneAU-4): 12,812 `.s` on disk vs 4,174 live units =
**8,638 orphans** (8,548 `auto_*` + 90 named).  A lane read carve geometry off
`asm/HamPlayerData.s` (dated Jun 11, pre-dating the Jul-15 TU5 flip) and drew a
wrong conclusion: it claims `.pdata@0x821FB8D8 -> fn_8237FBD8 len 0x9C`, but no
live `.pdata` record begins at `0x8237FBD8` at all.

Nothing in the tool surface validates a `.s` against the live carve.  Several
live-ish tools glob the whole tree and ingest orphans wholesale --
`scripts/grind/classify_funclets.py` (writes tags into `decomp.db`),
`scripts/recarve/funclets.py`, `tools/fn_resolver.py`, plus ~14 one-off
`scripts/harvest/*` scanners that open `build/45410914/asm/{basename}.s`
directly.  Pruning the orphans fixes every one of those readers at once, which
is strictly cheaper than adding a freshness gate to each.

**mtime is NOT a usable freshness proxy.**  72 of the 90 named orphans carried
*that same day's* date, because splits.txt was rewritten between two split runs
minutes apart.  Worse, both `asm/Faders.s` (live, 21:17, 108 KB, first fn
`fn_822E4500`) and `asm/system/synth/Faders.s` (orphan, 21:07, 2.3 KB, first fn
`fn_82310E50`) exist simultaneously -- same unit name, different geometry.
The only sound discriminator is **membership in the live
`build/<title>/config.json` unit list**, which is what this script uses.

Deleting these is safe: `build/` is gitignored, the files are not ninja inputs,
and the next split regenerates every live one.

USAGE
-----
    scripts/prune_orphan_asm.py                      # dry run (default)
    scripts/prune_orphan_asm.py --apply              # actually delete
    scripts/prune_orphan_asm.py --check              # exit 1 if orphans exist
    scripts/prune_orphan_asm.py --project-dir ~/tmp/wt-foo --apply

★ Run `--apply` only in your own worktree, or in main when no lane has a live
  `ninja` in flight (the split rule rewrites this directory).
"""
import argparse
import json
import os
import sys

TITLE = "45410914"


def live_unit_roots(build_dir):
    """Unit roots (path minus extension) from dtk's own output manifest."""
    cfg_path = os.path.join(build_dir, "config.json")
    if not os.path.exists(cfg_path):
        raise SystemExit(
            f"error: {cfg_path} not found -- run a split first; without it there "
            f"is no ground truth for which .s files are live."
        )
    with open(cfg_path) as f:
        cfg = json.load(f)
    roots = set()
    for unit in cfg.get("units", []):
        name = unit.get("name")
        if name:
            roots.add(os.path.splitext(name)[0])
    if not roots:
        raise SystemExit(f"error: {cfg_path} lists no units -- refusing to prune.")
    return roots, cfg_path


def find_orphans(asm_dir, roots):
    orphans = []
    for dirpath, _dirnames, filenames in os.walk(asm_dir):
        for fn in filenames:
            if not fn.endswith(".s"):
                continue
            full = os.path.join(dirpath, fn)
            rel = os.path.relpath(full, asm_dir)[:-2]  # strip ".s"
            if rel not in roots:
                orphans.append(full)
    return sorted(orphans)


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project-dir", default=None,
                    help="repo root to operate on (default: this script's repo)")
    ap.add_argument("--apply", action="store_true", help="actually delete (default: dry run)")
    ap.add_argument("--check", action="store_true",
                    help="exit 1 if any orphan exists; delete nothing")
    ap.add_argument("--list", action="store_true", help="print every orphan path")
    args = ap.parse_args()

    root = args.project_dir or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    build_dir = os.path.join(root, "build", TITLE)
    asm_dir = os.path.join(build_dir, "asm")
    if not os.path.isdir(asm_dir):
        print(f"no asm dir at {asm_dir} -- nothing to do.")
        return 0

    roots, cfg_path = live_unit_roots(build_dir)
    orphans = find_orphans(asm_dir, roots)
    named = [p for p in orphans if not os.path.basename(p).startswith("auto_")]

    total = sum(1 for d, _n, fs in os.walk(asm_dir) for f in fs if f.endswith(".s"))
    print(f"live units in {cfg_path}: {len(roots)}")
    print(f".s files on disk:          {total}")
    print(f"ORPHANS:                   {len(orphans)}  ({len(named)} named, "
          f"{len(orphans) - len(named)} auto_*)")

    if args.list:
        for p in orphans:
            print("   ", os.path.relpath(p, root))
    elif named:
        print("named orphans (the ones that mislead a human reader):")
        for p in named[:40]:
            print("   ", os.path.relpath(p, root))
        if len(named) > 40:
            print(f"    ... and {len(named) - 40} more (use --list)")

    if args.check:
        return 1 if orphans else 0

    if not args.apply:
        print("\n(dry run -- pass --apply to delete)")
        return 0

    freed = 0
    for p in orphans:
        try:
            freed += os.path.getsize(p)
            os.remove(p)
        except OSError as e:
            print(f"  warn: {p}: {e}", file=sys.stderr)
    # drop directories left empty (repeat until stable: os.walk caches dirnames,
    # so a parent emptied by removing its last child needs another pass)
    while True:
        removed = False
        for dirpath, _dirnames, _filenames in os.walk(asm_dir, topdown=False):
            if dirpath == asm_dir:
                continue
            try:
                os.rmdir(dirpath)
                removed = True
            except OSError:
                pass
        if not removed:
            break
    print(f"\ndeleted {len(orphans)} orphaned .s ({freed / 1e6:.1f} MB freed)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
