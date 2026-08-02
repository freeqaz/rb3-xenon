#!/usr/bin/env python3
"""CK-1: census of the UNPINNED .text gaps that remain after CJ-1's waves.

WHY
---
CJ-1's paying lever was `cj1_wave.py --mode gapfill`: fill a code-bearing
unpinned gap between two pinned blocks by extending a neighbour across it.  It
landed the <=512B and <=2048B buckets (+493 matched, 0 regressions) and
DELIBERATELY DID NOT LAND a 16KB-gap wave that was only 41% honest.

Before proposing anything, size what is LEFT.  This tool is pure geometry +
retail `.pdata`; it proposes nothing and edits nothing.

DENOMINATORS ARE PRINTED NEXT TO EVERY COUNT (project standard).

★ Half-open [start,end).  Every interval test is `s <= va < e`.
⛔ The Quazal /Od block 0x82A6D168-0x82B54190 is reported SEPARATELY and is
   never eligible.
"""
import os
import sys
import json
import struct
import bisect
import argparse
import collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
import truncated_pins as T  # noqa: E402

QUAZAL = (0x82A6D168, 0x82B54190)
PD_RAW = 0x1F1600
PD_SIZE = 0x70C28
PAD_WORDS = (b'\x00\x00\x00\x00', b'\x60\x00\x00\x00')

BUCKETS = [(0, 64), (64, 256), (256, 512), (512, 1024), (1024, 2048),
           (2048, 4096), (4096, 8192), (8192, 16384), (16384, 1 << 30)]


def pdata_starts(retail):
    """Sorted retail .pdata BeginAddresses = authoritative function starts."""
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


def gaps(units, pins):
    """Unpinned intervals strictly between two MERGED pinned runs, plus the
    head/tail slack.  Only INTERIOR gaps (both neighbours exist) are fillable
    by the gapfill lever, so they are flagged."""
    by_end = collections.defaultdict(list)
    by_start = collections.defaultdict(list)
    for u, rs in units.items():
        for i, (s, e) in enumerate(rs):
            by_end[e].append((u, i))
            by_start[s].append((u, i))
    out = []
    for gi in range(len(pins.merged) - 1):
        lo = pins.merged[gi][1]
        hi = pins.merged[gi + 1][0]
        if hi <= lo:
            continue
        out.append(dict(lo=lo, hi=hi, size=hi - lo,
                        left=by_end.get(lo, []), right=by_start.get(hi, [])))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--json', default=None)
    ap.add_argument('--splits', default=None,
                    help='alternate splits.txt (POSITIVE CONTROL: point at a '
                         'pre-CJ-1 revision and the <=2048 buckets must be '
                         'POPULATED, proving this census sees the very gaps '
                         'CJ-1 acted on)')
    ap.add_argument('--selftest', action='store_true')
    a = ap.parse_args()

    units = T.parse_splits(a.splits)
    pins = T.Pins(units)
    retail = T.Retail()
    pds = pdata_starts(retail)

    covered = sum(e - s for s, e in pins.merged)
    print(f'[geometry] {len(units):,} units, {len(pins.flat):,} .text blocks, '
          f'{len(pins.merged):,} merged runs')
    print(f'[geometry] pinned {covered:,} B of {T.TEXT_SIZE:,} B .text = '
          f'{100*covered/T.TEXT_SIZE:.2f}%  (unpinned {T.TEXT_SIZE-covered:,} B)')
    print(f'[geometry] retail .pdata function starts: {len(pds):,}')
    inside = sum(1 for p in pds if pins.is_pinned(p))
    print(f'[geometry] .pdata starts INSIDE a pin: {inside:,} / {len(pds):,} '
          f'= {100*inside/len(pds):.2f}%')

    if a.selftest:
        # C1: the VA->file mapping control from truncated_pins must hold, else
        # every padding/byte read below is garbage.
        got = retail.bytes_at(0x8261FFC8, 8)
        want = bytes.fromhex('8164fffc7c8b2050')
        print(f'[C1] VA->file on known thunk: {got.hex()} '
              f'-> {"OK" if got == want else "BROKEN"}')
        # C2 FAIL-ON-DEMAND for is_padding: a real code window must NOT be
        # called padding, and a synthetic zero window MUST be.
        code_is_pad = is_padding(retail, 0x8261FFC8, 0x8261FFD4)
        print(f'[C2] is_padding on KNOWN CODE = {code_is_pad} '
              f'(must be False) -> {"OK" if not code_is_pad else "BROKEN"}')
        # find a real padding run to prove the True branch is reachable
        found_pad = None
        for g in gaps(units, pins):
            if g['size'] >= 8 and is_padding(retail, g['lo'], g['hi']):
                found_pad = g
                break
        print(f'[C2b] a REAL padding gap exists: '
              f'{hex(found_pad["lo"]) if found_pad else None} '
              f'-> {"OK (True branch reachable)" if found_pad else "INERT"}')
        # C3: .pdata covers TWO executable sections.  band.exe has `.text`
        # (0x82270000 + 0x9DCE3C) AND `BINK` (0x82C4D000 + 0x10010, the Bink
        # video middleware, flags 0x60000020 = code).  An earlier version of
        # this control asserted every .pdata BeginAddress lies in `.text` and
        # reported BROKEN -- that assertion was WRONG, not the constants.
        # BINK functions are correctly unpinnable by a `.text` split.
        BINK = (0x82C4D000, 0x82C4D000 + 0x10010)
        n_text = sum(1 for p in pds if T.TEXT_VA <= p < T.TEXT_END)
        n_bink = sum(1 for p in pds if BINK[0] <= p < BINK[1])
        n_other = len(pds) - n_text - n_bink
        print(f'[C3] pdata starts: .text={n_text:,} BINK={n_bink:,} '
              f'other={n_other:,} / {len(pds):,} total '
              f'-> {"OK" if n_other == 0 else "UNEXPLAINED RESIDUE"}')
        return 0

    gl = gaps(units, pins)
    print(f'\n[gaps] interior unpinned gaps between merged runs: {len(gl):,}')

    rows = []
    for g in gl:
        lo, hi = g['lo'], g['hi']
        inq = QUAZAL[0] <= lo < QUAZAL[1] or QUAZAL[0] <= hi < QUAZAL[1]
        pad = is_padding(retail, lo, hi)
        nf = bisect.bisect_left(pds, hi) - bisect.bisect_left(pds, lo)
        rows.append(dict(lo=lo, hi=hi, size=hi - lo, quazal=inq, padding=pad,
                         pdata_funcs=nf,
                         left=[u for u, _ in g['left']],
                         right=[u for u, _ in g['right']]))

    tot_b = sum(r['size'] for r in rows)
    print(f'[gaps] total interior gap bytes: {tot_b:,} '
          f'({100*tot_b/max(1,T.TEXT_SIZE-covered):.1f}% of all unpinned bytes '
          f'-- the rest is head/tail slack outside any pin pair)')

    elig = [r for r in rows if not r['quazal'] and not r['padding']]
    print(f'[gaps] eligible (not Quazal, not pure padding): {len(elig):,} / '
          f'{len(rows):,}')
    print(f'       refused quazal={sum(1 for r in rows if r["quazal"]):,}  '
          f'pure-padding={sum(1 for r in rows if r["padding"] and not r["quazal"]):,}')

    print(f'\n{"bucket":>16} {"gaps":>7} {"bytes":>12} {"pdataFns":>9} '
          f'{"bothNbr":>8}')
    for b0, b1 in BUCKETS:
        sel = [r for r in elig if b0 <= r['size'] < b1]
        if not sel:
            continue
        both = sum(1 for r in sel if r['left'] and r['right'])
        lbl = f'{b0}-{b1 if b1 < (1<<30) else "inf"}'
        print(f'{lbl:>16} {len(sel):>7,} {sum(r["size"] for r in sel):>12,} '
              f'{sum(r["pdata_funcs"] for r in sel):>9,} {both:>8,}')
    print(f'{"TOTAL":>16} {len(elig):>7,} {sum(r["size"] for r in elig):>12,} '
          f'{sum(r["pdata_funcs"] for r in elig):>9,} '
          f'{sum(1 for r in elig if r["left"] and r["right"]):>8,}')

    if a.json:
        json.dump(rows, open(a.json, 'w'), indent=1)
        print('\nwrote', a.json)
    return 0


if __name__ == '__main__':
    sys.exit(main())
