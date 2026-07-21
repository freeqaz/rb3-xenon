#!/usr/bin/env python3
"""Resolve a conflicted JSON-dict file during a wave rebase by a 3-WAY dict
merge against the merge-base — not a 2-way union, and not a text merge.

Used for the union files that every wave branch touches:
  - scripts/target_symbol_map.json   (anon fn_<addr> -> MSVC mangled name)
  - config/45410914/objects.json     (per-TU compile/match declarations)

Stage orientation (land.sh always `git rebase main` while on the worker branch):
  - stage 1 (:1:) = merge-BASE   (common ancestor of main and the worker branch)
  - stage 2 (:2:) = OURS         = main  (the rebase upstream we replay onto)
  - stage 3 (:3:) = THEIRS       = the worker branch commit being replayed
NOTE: this is the *reverse* of a normal `git merge` (where ours=your branch).
land.sh only ever rebases, so ours is always main here. Do not reuse this
resolver on a plain merge without re-checking orientation.

Why 3-way (the batch-5 resurrection bug, docs/plans/fpcarve-batch5.md #1):
  The old resolver was a 2-way union (`union(ours, theirs)`): it kept every key
  present on *either* side and, on a leaf conflict, silently "kept theirs". With
  ours=main and theirs=an older worker branch that predates a main-side change:
    - main DELETED a key (stale __unwind$ funclet) but the worker still has it
      -> union re-ADDS it = resurrection (blocks real-fn pairing; forced the
      manual re-deletes dc8bf767 + d92cef87).
    - main RE-POINTED a key V1->V2 but the worker still has V1 -> "keep theirs"
      silently reverts main back to the stale V1.
  A 3-way merge consults the base so a side that MATCHES the base is "unchanged"
  and the side that DIFFERS wins; only genuine both-changed-differently is a
  real CONFLICT — and we now REFUSE loudly (nonzero exit) instead of silent-pick.

Merge rules (per leaf key; recurse when both sides hold a dict):
  base=b, ours(main)=o, theirs(worker)=t; MISSING = key absent on that side.
  - o == t                     -> take it (or drop if both MISSING).
  - only o changed vs base      -> MAIN wins (take o; drop the key if o MISSING
                                   = main deleted it).
  - only t changed vs base      -> WORKER wins (take t; drop if t MISSING).
  - both changed, o != t        -> CONFLICT: record it, refuse the whole file.
  A genuine worker addition (absent in base+main, present in worker) is "only t
  changed" -> taken. A main-only addition is "only o changed" -> taken.

Usage: resolve_json_union.py <worktree> <relpath-within-repo>
Exit 0 success (file rewritten, staged by caller); 2 if a needed stage is
missing; 3 if there is a real both-changed conflict (caller must DEFER).
"""
import sys, json, subprocess, collections, os

# Sentinel: key absent on a given side (distinct from a JSON null value).
MISSING = object()


def stage(wt, relpath, n):
    """Read git conflict stage `n` of `relpath` as an OrderedDict, or None."""
    r = subprocess.run(["git", "-C", wt, "show", f":{n}:{relpath}"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return None
    return json.loads(r.stdout, object_pairs_hook=collections.OrderedDict)


def merge3(base, ours, theirs):
    """3-way recursive dict merge.

    ours = main (rebase upstream), theirs = worker branch commit, base =
    merge-base. Returns (merged OrderedDict, conflicts list). Each conflict is
    (dotted_path, base_val, ours_val, theirs_val) with MISSING for absent sides.
    A non-empty conflicts list means the caller must refuse the merge.
    """
    conflicts = []

    def rec(b, o, t, path):
        merged = collections.OrderedDict()
        # Key order: ours(main) first (preserve main's layout), then
        # worker-new, then base-only. Base-only keys never survive (they are
        # absent from both live sides = deleted), but include them so the leaf
        # logic can still classify/emit conflicts deterministically.
        keys, seen = [], set()
        for src in (o, t, b):
            for k in src:
                if k not in seen:
                    seen.add(k)
                    keys.append(k)

        for k in keys:
            bv = b[k] if k in b else MISSING
            ov = o[k] if k in o else MISSING
            tv = t[k] if k in t else MISSING

            # Both live sides hold a dict -> recurse (objects.json nesting).
            if isinstance(ov, dict) and isinstance(tv, dict):
                merged[k] = rec(bv if isinstance(bv, dict) else {},
                                ov, tv, f"{path}{k}.")
                continue

            if ov == tv:
                # Identical on both sides (incl. both MISSING == both deleted).
                if ov is not MISSING:
                    merged[k] = ov
                continue

            # ov != tv: at least one side diverged from base.
            o_changed = (ov != bv)   # MISSING != value == True; MISSING==MISSING
            t_changed = (tv != bv)

            if o_changed and t_changed:
                # Both changed the same key differently -> real conflict.
                conflicts.append((f"{path}{k}", bv, ov, tv))
                if ov is not MISSING:      # provisional; file is refused anyway
                    merged[k] = ov
                continue
            if o_changed:
                # Main changed, worker matches base -> MAIN wins.
                if ov is not MISSING:
                    merged[k] = ov
                # else: main deleted the key -> drop (the fix for resurrection).
                continue
            # t_changed: worker changed, main matches base -> WORKER wins.
            if tv is not MISSING:
                merged[k] = tv
            # else: worker deleted the key -> drop.

        return merged

    merged = rec(base or collections.OrderedDict(),
                 ours or collections.OrderedDict(),
                 theirs or collections.OrderedDict(), "")
    return merged, conflicts


def _fmt(v):
    if v is MISSING:
        return "<absent>"
    return str(v)[:40]


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        sys.exit(2)
    wt, relpath = sys.argv[1], sys.argv[2]

    base = stage(wt, relpath, 1)      # merge-base (may legitimately be None)
    ours = stage(wt, relpath, 2)      # main
    theirs = stage(wt, relpath, 3)    # worker branch
    if ours is None or theirs is None:
        print(f"  {relpath}: missing stage (not both-modified?) — skip")
        sys.exit(2)
    if base is None:
        # No common ancestor version staged (file added independently on both
        # sides). Without a base we cannot detect deletions/re-points; fall back
        # to base={} — every live key looks "added", and a both-added-different
        # key becomes a loud CONFLICT rather than a silent pick.
        print(f"  {relpath}: WARN no merge-base stage — base treated as empty "
              f"(deletion/re-point detection disabled for this file)")

    merged, conflicts = merge3(base, ours, theirs)

    if conflicts:
        print(f"  {relpath}: REFUSING — {len(conflicts)} both-changed-differently "
              f"conflict(s) (main vs worker); resolve by hand:")
        for path, bv, ov, tv in conflicts:
            print(f"    {path}: base={_fmt(bv)} main={_fmt(ov)} worker={_fmt(tv)}")
        sys.exit(3)

    with open(os.path.join(wt, relpath), "w") as f:
        json.dump(merged, f, indent=1, ensure_ascii=False)
        f.write("\n")

    added = sum(1 for k in merged if not (ours and k in ours))
    # Flat-file headline: keys a 2-way union would have RESURRECTED (main
    # deleted them, worker still holds the base value) that we correctly dropped.
    prevented = 0
    if base:
        for k, bv in base.items():
            if (k not in ours) and (theirs and theirs.get(k, MISSING) == bv) \
                    and (k not in merged):
                prevented += 1
    print(f"  {relpath}: 3-way merged -> {len(merged)} entries "
          f"(+{added} from worker; {prevented} main-side deletion(s) protected "
          f"from resurrection)")


if __name__ == "__main__":
    main()
