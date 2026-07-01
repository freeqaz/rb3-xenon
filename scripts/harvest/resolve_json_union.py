#!/usr/bin/env python3
"""Resolve a conflicted JSON-dict file during a wave rebase by merging git
stage-2 (ours) + stage-3 (theirs) as DICTS — not as text.

Used for the union files that every wave branch touches:
  - scripts/target_symbol_map.json   (anon fn_<addr> -> MSVC mangled name)
  - config/45410914/objects.json     (per-TU compile/match declarations)

Both are pure additive dict maps across independent lanes, so a textual
3-way merge spuriously conflicts; a dict union (ours-first, then new-from-theirs)
is the correct, deterministic resolution. Insertion order is preserved.

Usage: resolve_json_union.py <worktree> <relpath-within-repo>
Exit 0 on success (file rewritten, staged by caller), 2 if a stage is missing.
"""
import sys, json, subprocess, collections, os

wt, relpath = sys.argv[1], sys.argv[2]


def stage(n):
    r = subprocess.run(["git", "-C", wt, "show", f":{n}:{relpath}"],
                       capture_output=True, text=True)
    if r.returncode != 0:
        return None
    return json.loads(r.stdout, object_pairs_hook=collections.OrderedDict)


ours, theirs = stage(2), stage(3)
if ours is None or theirs is None:
    print(f"  {relpath}: missing stage (not both-modified?) — skip")
    sys.exit(2)

added = 0


def union(a, b, path=""):
    """Recursive dict union: ours-first, new-from-theirs added; when BOTH sides
    hold a dict under the same key, recurse instead of replacing. A shallow
    keep-theirs on objects.json's nested module dicts silently dropped every
    main-side TU wiring under the colliding module (2026-07-01 'zeroed wave':
    the SongData lane clobbered 4 TUs landed minutes earlier)."""
    global added
    merged = collections.OrderedDict(a)
    for k, v in b.items():
        if k not in merged:
            merged[k] = v
            added += 1
        elif isinstance(merged[k], dict) and isinstance(v, dict):
            merged[k] = union(merged[k], v, f"{path}{k}.")
        elif merged[k] != v:
            # same LEAF key, different value — keep theirs (branch's intent) but warn
            print(f"  {relpath}: CONFLICT key {path}{k}: "
                  f"ours={str(merged[k])[:30]} theirs={str(v)[:30]} -> keeping theirs")
            merged[k] = v
    return merged


merged = union(ours, theirs)

with open(os.path.join(wt, relpath), "w") as f:
    json.dump(merged, f, indent=1, ensure_ascii=False)
    f.write("\n")
print(f"  {relpath}: merged {len(ours)} ours + {added} new = {len(merged)} entries")
