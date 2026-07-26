#!/usr/bin/env python3
"""Textual RENAME / DELETE editor for scripts/target_symbol_map.json.

`tu5_map_apply_fragment.py` can only ADD (it asserts the key is absent). The
joint map<->splits lane also needs the two other primitives:

  RENAME  point an existing VA at a different mangled name
  DELETE  drop a VA entirely ("unmapped beats wrongly-mapped" -- deleting a
          provably wrong entry has gained matches more than once, because a
          wrong name manufactures a false partial pairing that *blocks* the
          right one)

HARD PROJECT INVARIANT (same as the fragment applier): never
json.dump-rewrite target_symbol_map.json.  It is a ~21.7k-line, 1-space-indent
file whose formatting is a load-bearing convention.  This tool rewrites only
the single line belonging to each edited key and leaves every other line
byte-identical.

Guards (fail fast):
  * the key must exist (case-insensitively) -- a typo'd VA is an error, not a
    silent no-op
  * for RENAME, the *old* value must equal `from` when `from` is supplied, so a
    stale plan cannot clobber an entry another wave already moved
  * for RENAME, the new value must not already be a mapped value anywhere in
    the map -- a duplicate mangled name is a guaranteed regression whenever the
    two VAs land in the same unit (objdiff pairs by name inside a unit)
  * duplicate VAs must stay at 0: the key set is compared case-insensitively
    before and after

Edit file format (JSON):
  {"rename": {"0xVA": "new name", "0xVA2": ["expected old name", "new name"]},
   "delete": ["0xVA3", ...]}

Usage:
  map_edit_textual.py edits.json scripts/target_symbol_map.json [--dry-run]
"""
import argparse
import json
import re
import sys

VA_RE = re.compile(r'^\s*"(0[xX][0-9a-fA-F]+)"\s*:\s*(".*")\s*,?\s*$')


def load_keys(text):
    cur = json.loads(text)
    return cur


def apply_edits(edits, map_path, dry_run=False):
    text = open(map_path).read()
    lines = text.split('\n')
    cur = json.loads(text)

    renames = edits.get('rename', {})
    deletes = list(edits.get('delete', []))

    # ---- resolve requested keys to their real (possibly upper-case) spelling
    bylow = {}
    for k in cur:
        if VA_RE.match(f' "{k}": "x",'):
            bylow.setdefault(k.lower(), []).append(k)
    for low, ks in bylow.items():
        assert len(ks) == 1, f'duplicate VA already present: {ks}'

    def resolve(k):
        ks = bylow.get(k.lower())
        assert ks, f'key not in map: {k}'
        return ks[0]

    vals = {v for v in cur.values() if isinstance(v, str)}

    plan = {}   # real_key -> new value or None for delete
    for k, spec in renames.items():
        rk = resolve(k)
        if isinstance(spec, (list, tuple)):
            want_old, new = spec
            assert cur[rk] == want_old, (
                f'stale plan for {k}: map has {cur[rk]!r}, plan expected {want_old!r}')
        else:
            new = spec
        assert new not in vals, f'name collision, would duplicate: {new}'
        assert cur[rk] != new, f'no-op rename for {k}'
        plan[rk] = new
        vals.add(new)
    for k in deletes:
        rk = resolve(k)
        assert rk not in plan, f'{k} both renamed and deleted'
        plan[rk] = None

    # ---- rewrite exactly the matching lines
    out = []
    seen = set()
    for ln in lines:
        m = VA_RE.match(ln)
        if m and m.group(1) in plan:
            k = m.group(1)
            seen.add(k)
            new = plan[k]
            if new is None:
                continue                      # DELETE: drop the line
            trail = ',' if ln.rstrip().endswith(',') else ''
            out.append(f' "{k}": {json.dumps(new)}{trail}')
            continue
        out.append(ln)
    missing = set(plan) - seen
    assert not missing, f'keys present in JSON but no matching line found: {missing}'

    new_text = '\n'.join(out)
    after = json.loads(new_text)            # must still parse
    # duplicate-VA invariant
    lows = [k.lower() for k in after]
    assert len(lows) == len(set(lows)), 'duplicate VAs introduced'
    for k, v in plan.items():
        if v is None:
            assert k not in after, k
        else:
            assert after[k] == v, k
    for k in cur:
        if k not in plan:
            assert after.get(k) == cur[k], f'collateral change at {k}'

    if not dry_run:
        open(map_path, 'w').write(new_text)
    nren = sum(1 for v in plan.values() if v is not None)
    ndel = sum(1 for v in plan.values() if v is None)
    return nren, ndel, len(cur), len(after)


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('edits')
    ap.add_argument('map')
    ap.add_argument('--dry-run', action='store_true')
    a = ap.parse_args()
    nren, ndel, before, after = apply_edits(json.load(open(a.edits)), a.map,
                                            a.dry_run)
    print(f'{"DRY-RUN " if a.dry_run else ""}renamed {nren}, deleted {ndel}; '
          f'keys {before} -> {after}')


if __name__ == '__main__':
    sys.exit(main())
