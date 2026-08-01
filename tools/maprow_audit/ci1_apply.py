#!/usr/bin/env python3
"""Apply lane CI-1 map repairs TEXTUALLY (1-space indent is load-bearing; never
json.dump this file). Same gating discipline as CH-4's apply_ch4.py:

  * every edit states the EXPECTED current value and refuses on mismatch
  * the line must be UNIQUELY locatable in the raw text
  * the result is re-parsed with an object_pairs_hook duplicate-key detector
    (appliers that insert at the top produce a phantom edit otherwise: json.load
    keeps the LAST key, so the diff looks clean and the delta is 0)
  * name-injectivity is reported BEFORE and AFTER; the lane's budget is 0 new
    duplicates

Usage:  ci1_apply.py <edits.json>      # [{addr, expect|null, new, why}, ...]
        ci1_apply.py <edits.json> --dry
"""
import json, re, sys, os, collections

MAP = os.path.join(os.path.dirname(os.path.abspath(__file__)),
                   '..', '..', 'scripts', 'target_symbol_map.json')


def dupcheck(pairs):
    seen = collections.Counter(k for k, _ in pairs)
    dup = [k for k, n in seen.items() if n > 1]
    if dup:
        raise SystemExit(f'DUPLICATE KEYS in map: {dup[:10]}')
    return dict(pairs)


def addr_collisions(m):
    """Two DIFFERENT key spellings of the SAME address (e.g. 0x8262C280 vs
    0x8262c280). object_pairs_hook cannot see these -- they are distinct JSON
    keys -- but the renamer resolves both to one address, so one silently wins.
    """
    by = collections.defaultdict(list)
    for k in m:
        if k.startswith('0x'):
            by[int(k, 16)].append(k)
    return {hex(a): ks for a, ks in by.items() if len(ks) > 1}


def injectivity(m):
    n2a = collections.defaultdict(list)
    for k, v in m.items():
        if isinstance(v, str):
            n2a[v].append(k)
    return {n: a for n, a in n2a.items() if len(a) > 1}


def main():
    edits = json.load(open(sys.argv[1]))
    dry = '--dry' in sys.argv
    src = open(MAP).read()
    before = json.loads(src, object_pairs_hook=dupcheck)
    dup_before = injectivity(before)
    print(f'map keys before: {len(before)}   duplicate NAMES before: '
          f'{len(dup_before)}')

    keys = [k for k in before if k.startswith('0x')]

    def anchor_for(addr):
        """Nearest existing key BELOW addr, for diff locality only."""
        v = int(addr, 16)
        lo = [k for k in keys if int(k, 16) < v]
        return max(lo, key=lambda k: int(k, 16)) if lo else keys[0]

    for e in edits:
        addr, expect, new = e['addr'], e.get('expect'), e['new']
        cur = before.get(addr)
        if cur != expect:
            raise SystemExit(f'GATE FAIL {addr}: expected {expect!r}, found {cur!r}')
        if expect is None:
            # INSERT a brand-new key after a nearby anchor line. Inserting at
            # the TOP is the documented phantom-edit footgun (json.load keeps
            # the LAST key, so the diff looks clean and the delta is 0).
            anc = anchor_for(addr)
            pat = re.compile(r'^ "%s": ("(?:[^"\\]|\\.)*"),\n' % re.escape(anc), re.M)
            m = pat.search(src)
            if not m:
                raise SystemExit(f'anchor line for {anc} not found')
            src = (src[:m.end()] + ' "%s": %s,\n' % (addr, json.dumps(new))
                   + src[m.end():])
            print(f'  INSERT {addr} = {new}   [{e.get("why","")}]')
            continue
        old_line = ' "%s": %s,\n' % (addr, json.dumps(expect))
        if src.count(old_line) != 1:
            raise SystemExit(f'{addr}: line not uniquely locatable '
                             f'({src.count(old_line)} hits)')
        src = src.replace(old_line, ' "%s": %s,\n' % (addr, json.dumps(new)))
        print(f'  EDIT {addr}: {expect}  ->  {new}   [{e.get("why","")}]')

    after = json.loads(src, object_pairs_hook=dupcheck)
    col = addr_collisions(after)
    if col:
        raise SystemExit(f'REFUSING: case-variant duplicate ADDRESS keys: {col}')
    dup_after = injectivity(after)
    changed = {k for k in after if before.get(k) != after.get(k)}
    print(f'map keys after:  {len(after)} (delta {len(after)-len(before)})   '
          f'changed keys: {len(changed)}')
    print(f'duplicate NAMES after: {len(dup_after)}  '
          f'(new: {sorted(set(dup_after) - set(dup_before))})')
    if len(changed) != len(edits):
        raise SystemExit(f'expected {len(edits)} changed, got {len(changed)}')
    if set(dup_after) - set(dup_before):
        raise SystemExit('REFUSING: this patch introduces new duplicate names')
    if dry:
        print('DRY RUN -- not written')
        return
    open(MAP, 'w').write(src)
    print('written')


if __name__ == '__main__':
    main()
