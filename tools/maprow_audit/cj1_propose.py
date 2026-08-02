#!/usr/bin/env python3
"""CJ-1: turn the map-independent truncation sweep into PIN EXTENSIONS + a
CHECKABLE YIELD PREDICTION.

TWO INDEPENDENT INSTRUMENTS, AND WHY THE CONJUNCTION IS THE GATE
----------------------------------------------------------------
truncated_pins.py LOCATES a function of unit U in unpinned retail bytes next to
U's block edge, using COFF bytes only (map-independent).  Measured on the whole
tree that instrument alone has a *high* false-positive rate -- an untreated null
puts the same masked patterns in a RANDOM unpinned window 12.44% of the time,
because the median located body is 32 bytes with ~20 unmasked, and MSVC's ICF
folds only reloc-IDENTICAL bodies, so SHAPE-identical twins survive in bulk
(CLAUDE.md: 3,967 shape-identical survivors in 1,061 groups).  A masked byte
match therefore CANNOT distinguish a function from its shape twin.

So the byte oracle is used as a LOCATOR, and `target_symbol_map.json` is used as
an INDEPENDENT CONFIRMER: a row is CONFIRMED when the map, which was built from
entirely different evidence (strings/vtables/RTTI/fingerprints), independently
names that very address with the very symbol the bytes matched.  The sweep stays
map-independent -- the map never proposes a location, it only adjudicates one.

THE YIELD PREDICTOR (this is the instrument CI-4 lacked)
--------------------------------------------------------
objdiff pairs target<->base BY NAME WITHIN A UNIT, so a function pays iff
   (a) its address is inside one of U's pinned .text blocks, AND
   (b) the map names that address, AND
   (c) U's compiled obj DEFINES that symbol.
For a proposed extension span [a,b) we can evaluate (b) and (c) exactly and
count them.  That counts the WHOLE TRUNCATED TAIL, not one function per row --
which is exactly why CI-4 predicted +8/+12 and measured +19/+25.

SAFETY (structural, then verified mechanically)
-----------------------------------------------
Extensions grow ONLY into unpinned territory, whose functions were in no unit
and so could not have been matched anywhere; the whole-binary denominator is
fixed.  Hence Delta >= 0 structurally.  The ONE way an extension can regress is
a NAME COLLISION -- swallowing an address whose map symbol already names a
different address inside U -- which would let objdiff pair the wrong pair.  That
is checked explicitly (`collisions`), not assumed.

⛔ Refusals: the Quazal /Od block 0x82A6D168-0x82B54190 (4b3c098d unpinned 8 bad
pins there), and any span that would overlap another unit's pins.
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
    """address(int) -> symbol.

    ⚠ TWO documented traps handled here.  (1) The file carries NON-ADDRESS
    metadata keys (e.g. '_splits_fill_unresolved_comment') -- int(k,16) throws
    on them.  (2) DUPLICATE KEYS: map appliers insert at the TOP of the file and
    `json.load` silently keeps the LAST occurrence, which has produced phantom
    edits with a clean-looking diff and a zero delta.  We hook the raw pairs and
    REPORT duplicates instead of silently resolving them.
    """
    dup = collections.Counter()

    def hook(pairs):
        for k, _v in pairs:
            dup[k] += 1
        return dict(pairs)

    p = os.path.join(root, 'scripts', 'target_symbol_map.json')
    m = json.load(open(p), object_pairs_hook=hook)
    dups = {k: c for k, c in dup.items() if c > 1}
    if dups:
        print(f'⚠ target_symbol_map.json has {len(dups)} DUPLICATE keys '
              f'(json.load keeps the LAST): {list(dups)[:5]}')
    out = {}
    skipped = 0
    for k, v in m.items():
        try:
            out[int(k, 16)] = v
        except ValueError:
            skipped += 1
    print(f'[map] {len(out):,} address rows ({skipped} non-address keys '
          f'skipped, {len(dups)} duplicates)')
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--sweep', required=True)
    ap.add_argument('--out-splits', default=None)
    ap.add_argument('--out-json', default=None)
    ap.add_argument('--max-ext', type=int, default=4096,
                    help='refuse a single extension larger than this')
    ap.add_argument('--wave', type=int, default=1)
    ap.add_argument('--limit-units', type=int, default=0)
    ap.add_argument('--byte-only', action='store_true',
                    help='wave 2: also act on byte-located rows the map does '
                         'not confirm')
    a = ap.parse_args()

    root = T.ROOT
    units = T.parse_splits()
    pins = T.Pins(units)
    amap = load_map(root)
    sweep = json.load(open(a.sweep))
    rows = sweep['rows']

    uobjs = T.unit_objs(units)

    # ---------------- confirmation
    conf, byteonly = [], []
    for r in rows:
        s = amap.get(r['addr'])
        (conf if s == r['sym'] else byteonly).append(r)
    print(f'--- located rows: {len(rows):,}')
    print(f'    CONFIRMED by map (independent instrument agrees on address AND '
          f'symbol): {len(conf):,}')
    print(f'    byte-only (map silent or disagrees):        {len(byteonly):,}')

    # ENRICHMENT vs an untreated control: how often does the map name a RANDOM
    # unpinned address with the symbol we happened to probe?  Base rate.
    random.seed(7)
    unp = []
    prev = T.TEXT_VA
    for s0, e0 in pins.merged:
        if s0 > prev:
            unp.append((prev, s0))
        prev = max(prev, e0)
    tot_unp = sum(b - x for x, b in unp)
    named_unp = sum(1 for k in amap if not pins.is_pinned(k))
    print(f'    [null] unpinned .text bytes: {tot_unp:,}; map rows in unpinned '
          f'space: {named_unp:,}')
    hits = 0
    N = 20000
    for _ in range(N):
        x, b = random.choice(unp)
        va = random.randrange(x, max(x + 1, b))
        va &= ~3
        if va in amap:
            hits += 1
    print(f'    [null] a RANDOM 4-aligned unpinned address carries ANY map row '
          f'{hits}/{N} = {100*hits/N:.3f}%  (and must ALSO name the exact '
          f'symbol our bytes matched)')

    use = conf + (byteonly if a.byte_only else [])

    # ---------------- build extension proposals
    per_unit = collections.defaultdict(list)
    for r in use:
        per_unit[r['unit']].append(r)

    proposals = []
    refused = collections.Counter()
    for u, rs in sorted(per_unit.items()):
        blocks = sorted(units[u])
        newblocks = [list(b) for b in blocks]
        for r in rs:
            A, sz = r['addr'], r['size']
            if QUAZAL[0] <= A < QUAZAL[1]:
                refused['quazal'] += 1
                continue
            bs, be = r['block']
            # find the block object
            idx = None
            for i, (x, y) in enumerate(blocks):
                if (x, y) == (bs, be):
                    idx = i
                    break
            if idx is None:
                refused['block_gone'] += 1
                continue
            if r['kind'] == 'fwd':
                want = A + sz
                if want - be > a.max_ext:
                    refused['too_big'] += 1
                    continue
                if want > pins.next_pinned_start(be):
                    refused['overlap'] += 1
                    continue
                newblocks[idx][1] = max(newblocks[idx][1], want)
            else:
                want = A
                if bs - want > a.max_ext:
                    refused['too_big'] += 1
                    continue
                if want < pins.prev_pinned_end(bs - 1):
                    refused['overlap'] += 1
                    continue
                newblocks[idx][0] = min(newblocks[idx][0], want)
        delta = [(tuple(n), tuple(o)) for n, o in zip(newblocks, blocks)
                 if tuple(n) != tuple(o)]
        if delta:
            proposals.append((u, blocks, [tuple(x) for x in newblocks], delta))

    print(f'--- proposals: {len(proposals)} units, refusals {dict(refused)}')

    # ---------------- YIELD PREDICTION + collision check
    total_pred = 0
    total_bytes = 0
    detail = []
    for u, old, new, delta in proposals:
        tgtp, srcps = uobjs[u]
        bdef = T.Obj(srcps[0]).defined if srcps else set()
        tdef = T.Obj(tgtp).defined if os.path.exists(tgtp) else set()
        pred = 0
        spans = []
        for (ns, ne), (os_, oe) in zip(
                [d[0] for d in delta], [d[1] for d in delta]):
            for lo, hi in ((ns, os_), (oe, ne)):
                if hi <= lo:
                    continue
                spans.append((lo, hi))
                total_bytes += hi - lo
                for A, S in amap.items():
                    if lo <= A < hi and S in bdef:
                        pred += 1
        # COLLISION CHECK: does a swallowed address carry a symbol that ALREADY
        # names a different address inside U's existing pins?
        coll = 0
        for lo, hi in spans:
            for A, S in amap.items():
                if lo <= A < hi and S in tdef:
                    coll += 1
        total_pred += pred
        detail.append(dict(unit=u, spans=[(hex(x), hex(y)) for x, y in spans],
                           bytes=sum(y - x for x, y in spans), predicted=pred,
                           collisions=coll))
        if coll:
            print(f'  !! COLLISION in {u}: {coll} swallowed map symbols already '
                  f'defined in the target obj')

    detail.sort(key=lambda d: -d['predicted'])
    print(f'--- PREDICTION: +{total_pred} matched functions from '
          f'{total_bytes:,} newly pinned bytes across {len(proposals)} units')
    print(f'    (metric condition evaluated exactly: map names the address AND '
          f'our obj defines that symbol AND it is newly inside the pin)')
    for d in detail[:25]:
        print(f'    {d["unit"]:52s} +{d["predicted"]:3d}  {d["bytes"]:6,}B  '
              f'coll={d["collisions"]}')

    # ---------------- emit splits
    if a.out_splits:
        src = open(T.SPLITS).read().split('\n')
        repl = {}
        for u, old, new, delta in proposals:
            repl[u] = (old, new)
        out = []
        cur = None
        bi = 0
        for ln in src:
            if ln.strip() and not ln.startswith((' ', '\t')) and \
                    ln.rstrip().endswith(':'):
                cur = ln.strip()[:-1]
                bi = 0
                out.append(ln)
                continue
            m = re.match(r'^(\s*\.text\s+start:)(0x[0-9a-fA-F]+)(\s+end:)'
                         r'(0x[0-9a-fA-F]+)(.*)$', ln)
            if m and cur in repl:
                old, new = repl[cur]
                if bi < len(new):
                    s2, e2 = new[bi]
                    ln = f'{m.group(1)}0x{s2:08X}{m.group(3)}0x{e2:08X}{m.group(5)}'
                bi += 1
            out.append(ln)
        with open(a.out_splits, 'w') as f:
            f.write('\n'.join(out))
        print('wrote', a.out_splits)

    if a.out_json:
        json.dump(dict(detail=detail, predicted=total_pred,
                       proposals=[(u, [list(x) for x in n])
                                  for u, o, n, d in proposals]),
                  open(a.out_json, 'w'), indent=1)
        print('wrote', a.out_json)
    return 0


if __name__ == '__main__':
    sys.exit(main())
