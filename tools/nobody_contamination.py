#!/usr/bin/env python3
"""How much of class 3 ("we do not hold the body") is actually a MIS-PIN?

MOTIVATION (measured, not assumed)
----------------------------------
The single largest class-3 unit in the binary is `default/system/rnddx9/Rnd`
(29,320 B).  Reading its retail bytes shows 24,260 B of that is a DIFFERENT TU
-- an RBN/UGC song-metadata validator whose assert strings name `.\Validator.cpp`
-- swallowed by an over-wide `.text` pin.  Only 5,060 B is real DxRnd code.

If that is typical, class 3 is materially smaller than its headline and the
work it implies is "fix pins", not "write bodies".  So SIZE IT.

THE DETECTOR, AND ITS CONTROL
-----------------------------
A unit's NAMED rows are the rows the map has already tied to this TU, so their
address hull is evidence about where the TU actually lives.  A class-3 row far
OUTSIDE that hull is a mis-pin suspect.

⚠ "Outside the hull" is NOT sufficient on its own: the hull is only as wide as
the named subset, and a TU legitimately extends past its named functions.  So
the detector reports, per unit, the size of the outside-hull class-3 mass AND
the GAP between it and the hull, and only flags a unit when a contiguous
outside block is both large and separated.  Units with too few named rows to
form a hull are reported as UNDECIDABLE rather than silently counted either way
-- an instrument that cannot say "I don't know" is the one that cannot fail.

⚠ CONTROL: the same hull test is applied to the unit's NAMED rows, which are
inside by construction, so it cannot fail there.  The honest control is the
UNDECIDABLE bucket plus the requirement that a flag be corroborated by retail
bytes (string evidence), which this tool reports but does not itself perform.
"""

import argparse
import collections
import json
import sys
from pathlib import Path


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--rows-tsv', required=True)
    ap.add_argument('--min-block', type=int, default=2048)
    ap.add_argument('--min-gap', type=int, default=256)
    args = ap.parse_args()
    wt = Path(args.worktree)

    report = json.loads((wt / 'build/45410914/report.json').read_text())
    total_code = int(report['measures']['total_code'])

    # class-3 rows, by unit, with addresses parsed from the placeholder name
    c3 = collections.defaultdict(list)
    with open(args.rows_tsv) as fh:
        next(fh)
        for line in fh:
            u, t, s = line.rstrip('\n').split('\t')
            c3[u].append((int(t.split('_')[1], 16), int(s)))
    c3_bytes = sum(s for v in c3.values() for _, s in v)

    # named-row address hull per unit, from the target symbol map
    smap = json.loads((wt / 'scripts/target_symbol_map.json').read_text())
    # map is addr(str) -> mangled name; we need addr -> unit, so use the report
    # unit membership: a named report row's address is the map key that carries
    # that name.
    name_addr = {}
    for k, v in smap.items():
        if isinstance(v, str) and k.startswith("0x"):
            name_addr.setdefault(v, int(k, 16))

    # ⛔ THE HULL MUST BE BUILT FROM TU-OWNED SYMBOLS ONLY.
    #
    # The first version of this tool used every named row and was VACUOUS: it
    # did not fire on `default/system/rnddx9/Rnd`, the case it was built from.
    # Cause: `?_M_incr@_List_iterator_base@stlpmtx_std@@` is mapped to
    # 0x8272BAC8 -- the pin's very first address -- and that one shared STL
    # COMDAT stretched the hull across the whole foreign Validator block, so
    # every mis-pinned row read "inside hull".  A shared COMDAT carries NO
    # evidence about which TU a span belongs to (and that particular row is
    # itself an ICF-arbitrary assignment).
    #
    # So: keep only names defined in EXACTLY ONE of our base objs, and drop
    # template/STL instantiations outright.
    objdiff = json.loads((wt / 'objdiff.json').read_text())
    unit_cfg = {u['name']: u for u in objdiff['units']}
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from ident_body_channel import build_supply
    _supply, name_units, _sz = build_supply(wt, unit_cfg, verbose=False)

    def tu_owned(n):
        if name_units.get(n) is None or len(name_units[n]) != 1:
            return False
        return '@stlpmtx_std@@' not in n and '?$' not in n

    hull = {}
    for u in report['units']:
        addrs = [name_addr[f['name']] for f in u.get('functions', [])
                 if f['name'] in name_addr and tu_owned(f['name'])]
        if len(addrs) >= 3:
            hull[u['name']] = (min(addrs), max(addrs), len(addrs))

    verdict = collections.Counter()
    vbytes = collections.Counter()
    flagged = []
    for u, rows in c3.items():
        b = sum(s for _, s in rows)
        h = hull.get(u)
        if not h:
            verdict['UNDECIDABLE (<3 named rows)'] += 1
            vbytes['UNDECIDABLE (<3 named rows)'] += b
            continue
        lo, hi, n = h
        outside = [(a, s) for a, s in rows if a < lo or a > hi]
        ob = sum(s for _, s in outside)
        if not outside:
            verdict['INSIDE hull (genuine gap)'] += 1
            vbytes['INSIDE hull (genuine gap)'] += b
            continue
        # largest contiguous outside block + its gap to the hull
        outside.sort()
        blocks, curs, cure = [], outside[0][0], outside[0][0] + outside[0][1]
        for a, s in outside[1:]:
            if a - cure <= 64:
                cure = a + s
            else:
                blocks.append((curs, cure))
                curs, cure = a, a + s
        blocks.append((curs, cure))
        best = max(blocks, key=lambda blk: blk[1] - blk[0])
        bsize = best[1] - best[0]
        gap = lo - best[1] if best[1] <= lo else best[0] - hi
        if bsize >= args.min_block and gap >= args.min_gap:
            verdict['MIS-PIN SUSPECT'] += 1
            vbytes['MIS-PIN SUSPECT'] += ob
            vbytes['(of which: rest of unit inside)'] += b - ob
            flagged.append((u, b, ob, bsize, gap, best))
        else:
            verdict['outside but small/adjacent'] += 1
            vbytes['outside but small/adjacent'] += b

    print(f'class-3 total: {c3_bytes:,} B over {len(c3)} units\n')
    print(f'{"verdict":34s} {"units":>6s} {"class-3 B":>11s}  {"% class":>7s}')
    for k in ('MIS-PIN SUSPECT', 'outside but small/adjacent',
              'INSIDE hull (genuine gap)', 'UNDECIDABLE (<3 named rows)'):
        print(f'{k:34s} {verdict[k]:6d} {vbytes[k]:11,d}  '
              f'{100.0*vbytes[k]/c3_bytes:6.2f}%')
    print(f'\nMIS-PIN SUSPECT bytes are {100.0*vbytes["MIS-PIN SUSPECT"]/total_code:.2f}% '
          f'of total_code')

    print(f'\n=== flagged units (top 30 by suspect bytes) ===')
    print(f'  {"unit":44s} {"c3B":>7s} {"outB":>7s} {"block":>7s} {"gap":>8s}  range')
    for u, b, ob, bsize, gap, best in sorted(flagged, key=lambda r: -r[2])[:30]:
        print(f'  {u[:44]:44s} {b:7,d} {ob:7,d} {bsize:7,d} {gap:8,d}  '
              f'0x{best[0]:08X}-0x{best[1]:08X}')


if __name__ == '__main__':
    main()
