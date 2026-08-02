#!/usr/bin/env python3
"""CJ-1: emit a splits.txt with TRUNCATED PINS extended.  Two modes.

MODE `payable`  -- the CI-4 lever, re-derived map-independently and then
                   map-CONFIRMED.  Extends a unit's block to swallow an address
                   the map names with a symbol THAT UNIT'S COMPILED OBJ DEFINES.
                   Small, high-confidence, attribution is clean.

MODE `gapfill`  -- the MAP-INDEPENDENT play.  Fills every CODE-BEARING unpinned
                   gap <= --limit bytes that lies between two pinned blocks, by
                   extending one neighbour to meet the other.  This is the
                   experiment that decides whether the vein extends past the
                   map: CI-4's spans contain only 35 map rows whose symbol the
                   extending unit defines, yet CI-4 MEASURED +44 -- so something
                   other than named-row pairing paid, and only a measurement can
                   say whether it generalises.

★★★ WHY EXTENSIONS CANNOT REGRESS (structural, then checked)
------------------------------------------------------------
An extension grows ONLY into UNPINNED territory.  Functions there were in no
unit, so they could not have been matched anywhere, and the whole-binary
denominator (66,003 functions) is fixed.  Hence Delta >= 0 by construction.
The single exception is a NAME COLLISION -- swallowing an address whose map
symbol already names a different address inside the same unit -- which is
checked explicitly below rather than assumed.

⛔ HARD REFUSALS
  - the Quazal /Od block 0x82A6D168-0x82B54190 (4b3c098d unpinned 8 bad pins
    there for correctness; /Od is OBJECT-granular so the block is a real
    boundary)
  - any span that would overlap another unit's pins
  - any end that would fall MID-FUNCTION.  Block ends are snapped to a
    BOUNDARY drawn from retail `.pdata` BeginAddresses + map row addresses +
    existing pin edges.  dtk hard-fails a split whose range "ends within
    symbol", so this is a correctness requirement, not tidiness.

★ `.pdata` IS DERIVED OUTPUT and is NEVER written here -- every split run clears
the whole `.pdata` set and re-derives it from `.text`.  We edit `.text` only.
"""
import os
import re
import sys
import json
import struct
import bisect
import argparse
import collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import truncated_pins as T  # noqa: E402
from cj1_census import load_map  # noqa: E402

QUAZAL = (0x82A6D168, 0x82B54190)
PD_RAW = 0x1F1600
PD_SIZE = 0x70C28
PAD_WORDS = (b'\x00\x00\x00\x00', b'\x60\x00\x00\x00')


def pdata_starts(retail):
    out = []
    for i in range(PD_SIZE // 8):
        beg, _pk = struct.unpack_from('>II', retail.d, PD_RAW + 8 * i)
        if beg:
            out.append(beg)
    out.sort()
    return out


def is_padding(retail, lo, hi):
    b = retail.bytes_at(lo, hi - lo)
    if not b:
        return True
    return all(b[i:i + 4] in PAD_WORDS for i in range(0, len(b) - 3, 4))


def write_splits(src_path, out_path, newmap):
    """newmap: unit -> list of (start,end) in the SAME ORDER as the file."""
    src = open(src_path).read().split('\n')
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
        if m and cur in newmap:
            nb = newmap[cur]
            if bi < len(nb):
                s2, e2 = nb[bi]
                ln = f'{m.group(1)}0x{s2:08X}{m.group(3)}0x{e2:08X}{m.group(5)}'
            bi += 1
        out.append(ln)
    open(out_path, 'w').write('\n'.join(out))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--mode', choices=('payable', 'gapfill'), required=True)
    ap.add_argument('--payable', default=None)
    ap.add_argument('--limit', type=int, default=512)
    ap.add_argument('--out', required=True)
    ap.add_argument('--report', default=None)
    a = ap.parse_args()

    units = T.parse_splits()
    pins = T.Pins(units)
    retail = T.Retail()
    amap = load_map(T.ROOT)
    uobjs = T.unit_objs(units)
    pds = pdata_starts(retail)

    # boundary set: never end a block mid-function
    bounds = set(pds)
    bounds.update(amap)
    for s, e, _u in pins.flat:
        bounds.add(s)
        bounds.add(e)
    bl = sorted(bounds)

    def next_bound(x):
        i = bisect.bisect_right(bl, x)
        return bl[i] if i < len(bl) else None

    new = {u: [list(b) for b in rs] for u, rs in units.items()}
    changes = []
    refused = collections.Counter()

    if a.mode == 'payable':
        rows = json.load(open(a.payable))
        for astr, v in rows.items():
            A = int(astr, 16)
            u = v['unit']
            if QUAZAL[0] <= A < QUAZAL[1]:
                refused['quazal'] += 1
                continue
            bs, be = int(v['block'][0], 16), int(v['block'][1], 16)
            idx = next((i for i, (x, y) in enumerate(units[u])
                        if (x, y) == (bs, be)), None)
            if idx is None:
                refused['block_gone'] += 1
                continue
            # ★ REACHABILITY GATE.  Nearness is not enough: the whole span
            # between the block edge and A must be UNPINNED, or the extension
            # runs straight through a FOREIGN unit's block.  The overlap
            # verifier caught exactly this (TrackPanelDir would have swallowed
            # FlowNode 0x82307630-0x82307b74; CharIKHand would have swallowed
            # Rnd_NG).  Distance-to-nearest-block is the WRONG predicate.
            span = (be, A) if A >= be else (A, bs)
            if pins.next_pinned_start(span[0]) < span[1] or \
                    pins.is_pinned(span[0]):
                refused['not_reachable_unpinned'] += 1
                continue
            if A >= be:      # forward truncation
                nb = next_bound(A)
                end = min(nb if nb else A + v.get('size', 4),
                          pins.next_pinned_start(be))
                if end <= new[u][idx][1]:
                    refused['no_growth'] += 1
                    continue
                changes.append((u, idx, 'fwd', new[u][idx][1], end))
                new[u][idx][1] = end
            else:            # backward truncation
                st = max(A, pins.prev_pinned_end(bs - 1))
                if st >= new[u][idx][0]:
                    refused['no_growth'] += 1
                    continue
                changes.append((u, idx, 'bwd', st, new[u][idx][0]))
                new[u][idx][0] = st
    else:
        # gapfill: for each gap between MERGED runs, extend a neighbour across.
        # Choose the owner by evidence where available (a map row in the gap
        # whose symbol one neighbour's obj defines), else default to the LEFT
        # block -- spatial TU grouping makes the preceding unit the prior.
        objcache = {}

        def defines(u, s):
            if u not in objcache:
                srcs = uobjs.get(u, (None, []))[1]
                objcache[u] = T.Obj(srcs[0]).defined if srcs else set()
            return s in objcache[u]

        # index blocks by their end / start for neighbour lookup
        by_end = {}
        by_start = {}
        for u, rs in units.items():
            for i, (s, e) in enumerate(rs):
                by_end.setdefault(e, []).append((u, i))
                by_start.setdefault(s, []).append((u, i))

        for gi in range(len(pins.merged) - 1):
            lo = pins.merged[gi][1]
            hi = pins.merged[gi + 1][0]
            if hi <= lo or hi - lo > a.limit:
                continue
            if QUAZAL[0] <= lo < QUAZAL[1] or QUAZAL[0] <= hi < QUAZAL[1]:
                refused['quazal'] += 1
                continue
            if is_padding(retail, lo, hi):
                refused['pure_padding'] += 1
                continue
            L = by_end.get(lo, [])
            R = by_start.get(hi, [])
            if not L and not R:
                refused['no_neighbour'] += 1
                continue
            pick = None
            for A2, S2 in amap.items():
                if lo <= A2 < hi:
                    for u, i in L:
                        if defines(u, S2):
                            pick = ('fwd', u, i)
                            break
                    if pick:
                        break
                    for u, i in R:
                        if defines(u, S2):
                            pick = ('bwd', u, i)
                            break
                    if pick:
                        break
            if pick is None:
                pick = ('fwd', L[0][0], L[0][1]) if L else \
                       ('bwd', R[0][0], R[0][1])
            kind, u, i = pick
            if kind == 'fwd':
                changes.append((u, i, 'fwd', new[u][i][1], hi))
                new[u][i][1] = hi
            else:
                changes.append((u, i, 'bwd', lo, new[u][i][0]))
                new[u][i][0] = lo

    # ---------------- mechanical verification
    flat = []
    for u, rs in new.items():
        for s, e in rs:
            if e <= s:
                print(f'!! EMPTY/INVERTED block in {u}: {hex(s)}-{hex(e)}')
                return 1
            flat.append((s, e, u))
    flat.sort()
    ov = 0
    for i in range(len(flat) - 1):
        if flat[i][1] > flat[i + 1][0]:
            ov += 1
            if ov <= 5:
                print(f'!! OVERLAP {flat[i][2]} {hex(flat[i][0])}-'
                      f'{hex(flat[i][1])} vs {flat[i+1][2]} '
                      f'{hex(flat[i+1][0])}-{hex(flat[i+1][1])}')
    print(f'[verify] overlaps: {ov}')
    if ov:
        return 1
    inq = sum(1 for s, e, _ in flat if QUAZAL[0] <= s < QUAZAL[1])
    old_inq = sum(1 for s, e, _ in pins.flat if QUAZAL[0] <= s < QUAZAL[1])
    print(f'[verify] blocks starting inside Quazal /Od: {inq} (was {old_inq})')
    if inq > old_inq:
        return 1

    # collision check: a swallowed map symbol already defined in the target obj
    coll = 0
    tcache = {}
    for u, i, kind, x, y in changes:
        t = tcache.get(u)
        if t is None:
            tp = uobjs[u][0]
            t = tcache[u] = (T.Obj(tp).defined if os.path.exists(tp) else set())
        for A2, S2 in amap.items():
            if x <= A2 < y and S2 in t:
                coll += 1
    print(f'[verify] name collisions (swallowed symbol already in target obj): '
          f'{coll}')

    grew = sum(y - x for _u, _i, _k, x, y in changes)
    print(f'--- mode={a.mode}: {len(changes)} block edits across '
          f'{len(set(c[0] for c in changes))} units, +{grew:,} pinned bytes')
    print(f'    refusals: {dict(refused)}')

    # yield prediction
    pred_named = 0
    for u, i, kind, x, y in changes:
        srcs = uobjs.get(u, (None, []))[1]
        d = T.Obj(srcs[0]).defined if srcs else set()
        for A2, S2 in amap.items():
            if x <= A2 < y and S2 in d:
                pred_named += 1
    nf = 0
    for _u, _i, _k, x, y in changes:
        nf += bisect.bisect_left(pds, y) - bisect.bisect_left(pds, x)
    print(f'--- PREDICTION inputs: named-and-defined map rows swallowed = '
          f'{pred_named}; .pdata function starts swallowed = {nf}')

    write_splits(T.SPLITS, a.out, {u: [tuple(b) for b in rs]
                                   for u, rs in new.items()})
    print('wrote', a.out)
    if a.report:
        json.dump(dict(changes=[[u, i, k, hex(x), hex(y)]
                                for u, i, k, x, y in changes],
                       grew=grew, pred_named=pred_named, pdata_funcs=nf),
                  open(a.report, 'w'), indent=1)
    return 0


if __name__ == '__main__':
    sys.exit(main())
