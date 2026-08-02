#!/usr/bin/env python3
"""CJ-1: HONEST SIZING of the truncated-pin vein, with its denominator.

★★★ THE STRUCTURAL BOUND (this is the finding that sizes the whole vein)
------------------------------------------------------------------------
objdiff pairs target<->base symbols BY NAME WITHIN A UNIT.  A retail function
whose address is NOT in target_symbol_map.json is emitted by dtk into the target
obj as an anonymous `fn_8XXXXXXX`; our compiled obj carries the MSVC mangled
name.  Per the anon-naming gate, anonymous target BODIES CAN NEVER PAIR.

⇒ EXTENDING A PIN OVER UNNAMED RETAIL CODE PAYS EXACTLY ZERO.

Therefore the payable population of the ENTIRE truncated-pin vein -- map-derived
or not -- is bounded above by:

    { map rows whose address is currently UNPINNED }

and the exact metric condition for one of those to pay after a pin extension is
the conjunction

    (a) address A is unpinned now and inside the extension after,
    (b) the map names A with symbol S,                      [(b) is the bound]
    (c) some unit U's COMPILED obj DEFINES S,
    (d) A is adjacent to one of U's pinned .text blocks (so an extension of U,
        rather than a brand-new block, reaches it).

This tool measures (a)-(d) and PRINTS THE DENOMINATOR AT EVERY STAGE.

⚠ This is why a "map-independent" sweep cannot beat the map here.  Being
map-independent buys freedom in WHERE YOU LOOK, but the metric only pays for
NAMED addresses, so the map is not a sampling bias to be escaped -- it is the
payable set itself.  A byte-located truncation at an unnamed address is a real
truncation that the metric cannot reward.
"""
import os
import re
import sys
import json
import random
import argparse
import collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import truncated_pins as T  # noqa: E402

QUAZAL = (0x82A6D168, 0x82B54190)


def load_map(root):
    dup = collections.Counter()

    def hook(pairs):
        for k, _v in pairs:
            dup[k] += 1
        return dict(pairs)
    p = os.path.join(root, 'scripts', 'target_symbol_map.json')
    m = json.load(open(p), object_pairs_hook=hook)
    out = {}
    for k, v in m.items():
        try:
            out[int(k, 16)] = v
        except ValueError:
            pass
    return out


def sym_index(units):
    """symbol -> set(splits unit key) that DEFINES it in its compiled obj."""
    uobjs = T.unit_objs(units)
    sym_units = collections.defaultdict(set)
    n = 0
    for u, (tgt, srcs) in uobjs.items():
        if not srcs:
            continue
        o = T.Obj(srcs[0])
        if not o.ok:
            continue
        n += 1
        for s in o.defined:
            sym_units[s].add(u)
    return sym_units, n


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--splits', default=None)
    ap.add_argument('--adj', type=int, default=4096)
    ap.add_argument('--json', default=None)
    a = ap.parse_args()

    if a.splits:
        T.SPLITS = a.splits
    root = T.ROOT
    units = T.parse_splits(T.SPLITS)
    pins = T.Pins(units)
    amap = load_map(root)

    print(f'=== CJ-1 truncated-pin census  (splits: {T.SPLITS})')
    print(f'[0] map rows total                          {len(amap):,}')
    unpinned = {A: S for A, S in amap.items() if not pins.is_pinned(A)}
    print(f'[a] map rows at an UNPINNED address         {len(unpinned):,}'
          f'   <-- HARD CEILING of the whole vein')
    print(f'    (of {len(amap):,} map rows; pinned .text = '
          f'{sum(e-s for s,e in pins.merged):,} B, unpinned = '
          f'{T.TEXT_SIZE - sum(e-s for s,e in pins.merged):,} B)')

    sym_units, nobj = sym_index(units)
    print(f'[idx] compiled objs indexed: {nobj}, distinct defined symbols '
          f'{len(sym_units):,}')

    have_def = {A: S for A, S in unpinned.items() if S in sym_units}
    print(f'[c] ...AND some unit\'s compiled obj DEFINES that symbol   '
          f'{len(have_def):,}')

    # (d) adjacency to a DEFINING unit's block
    adj = {}
    far = {}
    quaz = 0
    for A, S in have_def.items():
        if QUAZAL[0] <= A < QUAZAL[1]:
            quaz += 1
            continue
        best = None
        for u in sym_units[S]:
            for s, e in units[u]:
                if e <= A:
                    d = A - e
                elif A < s:
                    d = s - A
                else:
                    d = 0
                if best is None or d < best[0]:
                    best = (d, u, (s, e))
        if best and best[0] <= a.adj:
            adj[A] = (S, best)
        else:
            far[A] = (S, best)
    print(f'[d] ...AND within {a.adj}B of a DEFINING unit\'s pinned block  '
          f'{len(adj):,}      (farther away: {len(far):,}, '
          f'inside Quazal /Od: {quaz})')

    # distance profile of the payable set
    prof = collections.Counter()
    for A, (S, (d, u, blk)) in adj.items():
        prof[0 if d == 0 else (16 if d <= 16 else (64 if d <= 64 else
             (512 if d <= 512 else 4096)))] += 1
    print(f'    gap profile of the payable set: '
          + '  '.join(f'<={k}B:{prof[k]}' for k in (0, 16, 64, 512, 4096)))

    byunit = collections.Counter(v[1][1] for v in adj.values())
    print(f'    spread over {len(byunit)} units; top: {byunit.most_common(12)}')

    # UNTREATED-POPULATION CONTROL: base rate that a random unpinned 4-aligned
    # address is within `adj` of the block of a unit defining a RANDOM symbol.
    random.seed(31337)
    unp_iv = []
    prev = T.TEXT_VA
    for s, e in pins.merged:
        if s > prev:
            unp_iv.append((prev, s))
        prev = max(prev, e)
    allsym = [s for s, us in sym_units.items() if len(us) == 1]
    hit = 0
    N = 4000
    for _ in range(N):
        x, b = random.choice(unp_iv)
        A = random.randrange(x, max(x + 1, b)) & ~3
        S = random.choice(allsym)
        u = next(iter(sym_units[S]))
        ok = False
        for s, e in units[u]:
            d = (A - e) if e <= A else ((s - A) if A < s else 0)
            if d <= a.adj:
                ok = True
                break
        if ok:
            hit += 1
    print(f'[null] a RANDOM unpinned address is within {a.adj}B of a RANDOM '
          f'symbol\'s defining unit {hit}/{N} = {100*hit/N:.2f}%')
    print('       (denominator printed on purpose -- the payable set\'s '
          'adjacency must be read against this base rate)')

    if a.json:
        out = {hex(A): dict(sym=S, gap=d, unit=u, block=[hex(b[0]), hex(b[1])])
               for A, (S, (d, u, b)) in adj.items()}
        json.dump(out, open(a.json, 'w'), indent=1)
        print('wrote', a.json, f'({len(out)} payable rows)')
    return 0


if __name__ == '__main__':
    sys.exit(main())
