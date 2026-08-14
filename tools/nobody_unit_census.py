#!/usr/bin/env python3
"""Census the "WE DO NOT HOLD THE BODY" class (IDENT-1 class 3) BY UNIT.

WHY
---
Lane IDENT-1 (docs/decomp/identification-frontier-censused-2026-08-14.md)
settled that identification tooling is worth ~0.2% of total_code, and that the
largest *reachable* slice of the frontier is class 3: 5,363 rows / 1,194,012 B
/ 11.57% of total_code that sit in units which ALREADY PIN and ALREADY COMPILE
a base obj, and simply lack a byte-identical body.  That is ordinary decomp
work wearing an identification hat.

IDENT-1 reported class 3 as a single binary-wide number.  A single number does
not tell you whether to fund it: 1.19 MB spread over 500 units at 2 kB each is
a decade-long grind, while the same bytes in 20 units of 60 kB each is a
fundable wave.  THE SHAPE IS THE PLANNING RESULT.  This tool measures it.

DEFINITION (kept identical to IDENT-1's, so the totals reconcile)
-----------------------------------------------------------------
A row is class 3 when ALL of:
  * it is an anonymous (placeholder-named) row in a TARGET obj -- retail has a
    function here that no map entry names;
  * its unit HAS a base obj (objdiff.json base_path) -- so the unit already
    pins and already compiles, and a matching body would draw credit today;
  * NO symbol anywhere in our compiled supply has a relocation-normalized
    body hash equal to this row's.  We do not hold this body, in any TU.

⚠ SIZES COME FROM report.json, NEVER FROM SLICE LENGTHS.  A section slice runs
to the next defining symbol, absorbing alignment padding and trailing unwind
bytes that dtk does not bill to the function; using slice lengths overstates
the prize.  (Same reasoning as ident_body_channel.py.)

⚠ SELF-VALIDATION.  The per-unit rows are summed back to the binary-wide class
totals and the script REFUSES if they disagree, so a census that silently drops
rows cannot be mistaken for a small one.

USAGE
    python3 tools/nobody_unit_census.py --worktree <wt> [--tsv out.tsv] [--top N]
"""

import argparse
import collections
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ident_body_channel import (  # noqa: E402
    build_supply, build_demand, is_placeholder,
)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--tsv')
    ap.add_argument('--rows-tsv')
    ap.add_argument('--top', type=int, default=40)
    args = ap.parse_args()
    wt = Path(args.worktree)

    objdiff = json.loads((wt / 'objdiff.json').read_text())
    unit_cfg = {u['name']: u for u in objdiff['units']}

    print('indexing...', file=sys.stderr)
    supply, name_units, name_size = build_supply(wt, unit_cfg)
    demand = build_demand(wt, unit_cfg)

    named_target_rows = sum(
        1 for rows in demand.values() for (n, h, s) in rows if not is_placeholder(n))
    if named_target_rows < 1000:
        sys.exit('REFUSING: only %d named target rows -- this worktree looks '
                 'PRE-RENAMER. Build it first.' % named_target_rows)
    print(f'  renamer trap check PASSED ({named_target_rows} named target rows)',
          file=sys.stderr)

    report = json.loads((wt / 'build/45410914/report.json').read_text())
    total_code = int(report['measures']['total_code'])
    rep = {}
    unit_meta = {}
    for u in report['units']:
        meta = u.get('metadata') or {}
        unit_meta[u['name']] = {
            'src': meta.get('source_path'),
            'auto': bool(meta.get('auto_generated')),
            'cats': meta.get('progress_categories') or [],
            'nrows': len(u.get('functions', [])),
            'ubytes': sum(int(f.get('size', 0) or 0) for f in u.get('functions', [])),
            'umatched': sum(int(f.get('size', 0) or 0) for f in u.get('functions', [])
                            if float(f.get('fuzzy_match_percent', 0) or 0) >= 100.0),
        }
        for f in u.get('functions', []):
            rep[(u['name'], f['name'])] = (
                int(f.get('size', 0) or 0),
                float(f.get('match_percent_normalized', 0) or 0))

    # ---- classify
    per_unit = collections.defaultdict(lambda: [0, 0])   # unit -> [rows, bytes]
    rows_out = []
    cls = collections.Counter()
    cls_b = collections.Counter()
    for uname, rows in demand.items():
        has_base = bool(unit_cfg.get(uname, {}).get('base_path'))
        for name, h, _slice_size in rows:
            if not is_placeholder(name):
                continue
            rsize, rmpn = rep.get((uname, name), (0, 0.0))
            mine = supply.get(h)
            if mine:
                key = 'have_body'
            elif not has_base:
                key = 'no_body_NO_BASE_OBJ'     # IDENT-1 class 2
            elif rmpn >= 100.0:
                key = 'no_body_but_already_paired'
            else:
                key = 'CLASS3_no_body_has_base'  # IDENT-1 class 3
            cls[key] += 1
            cls_b[key] += rsize
            if key == 'CLASS3_no_body_has_base':
                per_unit[uname][0] += 1
                per_unit[uname][1] += rsize
                rows_out.append((uname, name, rsize))

    print()
    print('=== anonymous target rows, by body-holding state ===')
    for k in ('have_body', 'no_body_NO_BASE_OBJ', 'no_body_but_already_paired',
              'CLASS3_no_body_has_base'):
        print(f'  {k:30s} {cls[k]:7d} rows  {cls_b[k]:10,d} B  '
              f'{100.0*cls_b[k]/total_code:6.2f}%')

    c3_rows = cls['CLASS3_no_body_has_base']
    c3_bytes = cls_b['CLASS3_no_body_has_base']

    # ---- SELF-VALIDATION: per-unit table must reconstruct the class total
    su_rows = sum(v[0] for v in per_unit.values())
    su_bytes = sum(v[1] for v in per_unit.values())
    assert su_rows == c3_rows == len(rows_out), (su_rows, c3_rows, len(rows_out))
    assert su_bytes == c3_bytes, (su_bytes, c3_bytes)
    print(f'\nSELF-VALIDATION OK: per-unit table reconstructs {c3_rows} rows / '
          f'{c3_bytes:,} B exactly')

    # ---- THE SHAPE
    ranked = sorted(per_unit.items(), key=lambda kv: -kv[1][1])
    print(f'\n=== SHAPE: {len(ranked)} units carry the class ===')
    cum = 0
    marks = [0.25, 0.5, 0.75, 0.9]
    mi = 0
    for i, (u, (n, b)) in enumerate(ranked, 1):
        cum += b
        while mi < len(marks) and cum >= marks[mi] * c3_bytes:
            print(f'  top {i:4d} units ({100.0*i/len(ranked):5.1f}% of units) '
                  f'= {marks[mi]*100:.0f}% of the bytes')
            mi += 1
    sizes = [b for _, (n, b) in ranked]
    print(f'  median unit = {sorted(sizes)[len(sizes)//2]:,} B   '
          f'mean = {c3_bytes//len(ranked):,} B   max = {max(sizes):,} B')
    for thr in (65536, 32768, 16384, 8192, 4096, 1024):
        sel = [s for s in sizes if s >= thr]
        print(f'  units with >= {thr:6,d} B: {len(sel):4d}  '
              f'({sum(sel):9,d} B = {100.0*sum(sel)/c3_bytes:5.1f}% of class)')

    print(f'\n=== TOP {args.top} UNITS ===')
    print(f'  {"unit":44s} {"rows":>5s} {"bytes":>9s} {"%cls":>6s}  '
          f'{"unit_matched%":>13s}  src')
    for u, (n, b) in ranked[:args.top]:
        m = unit_meta.get(u, {})
        ub = m.get('ubytes', 0)
        pctm = 100.0 * m.get('umatched', 0) / ub if ub else 0.0
        print(f'  {u:44s} {n:5d} {b:9,d} {100.0*b/c3_bytes:5.1f}%  '
              f'{pctm:12.1f}%  {m.get("src") or "-"}')

    if args.tsv:
        with open(args.tsv, 'w') as fh:
            fh.write('unit\trows\tbytes\tunit_total_bytes\tunit_matched_bytes\tsrc\n')
            for u, (n, b) in ranked:
                m = unit_meta.get(u, {})
                fh.write(f'{u}\t{n}\t{b}\t{m.get("ubytes",0)}\t'
                         f'{m.get("umatched",0)}\t{m.get("src") or ""}\n')
        print(f'\nwrote {args.tsv}')
    if args.rows_tsv:
        with open(args.rows_tsv, 'w') as fh:
            fh.write('unit\ttarget\tbytes\n')
            for u, t, s in sorted(rows_out, key=lambda r: (r[0], -r[2])):
                fh.write(f'{u}\t{t}\t{s}\n')
        print(f'wrote {args.rows_tsv}')


if __name__ == '__main__':
    main()
