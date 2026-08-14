#!/usr/bin/env python3
"""AUDIT the class-3 byte figures against dtk's own asm extents.

WHY
---
The class-3 census bills each row the `size` field from report.json.  Spot-check
of the top row in `default/Shader` found `fn_824A59D4` billed **8,852 B** while
dtk's own `Shader.s` emits **12 bytes** for it (`li r11,1; clrlwi r3,r11,24;
blr` -- it returns true).  Its three neighbours agree exactly (20/28/1436), so
this is a per-symbol inflation, not a uniform offset.

This is the known "dtk bills an UNBOUNDED symbol to the next boundary" hazard
(memory: project_total_code_denominator_inflated_2026-08-09).  An inflated row
is billed to `total_code`, contributes 0 matched, and -- crucially for planning
-- makes a trivial 12-byte accessor look like the single fattest prize in its
unit.  So EVERY headline byte number for this class has to be re-derived from
the asm extent, and the difference reported.

⚠ The asm extent is dtk's own emitted `size:` comment for that address, i.e.
the same tool's other opinion.  Where the two disagree the asm is the one that
matches the disassembly, which is what was checked by hand above.
"""

import argparse
import collections
import json
import re
from pathlib import Path

SIZE_RE = re.compile(r'# \.text:0x[0-9A-Fa-f]+ \| 0x([0-9A-F]+) \| size: (0x[0-9A-Fa-f]+)')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--rows-tsv', required=True)
    args = ap.parse_args()
    wt = Path(args.worktree)

    objdiff = json.loads((wt / 'objdiff.json').read_text())
    unit_cfg = {u['name']: u for u in objdiff['units']}

    c3 = collections.defaultdict(list)
    with open(args.rows_tsv) as fh:
        next(fh)
        for line in fh:
            u, t, s = line.rstrip('\n').split('\t')
            c3[u].append((t, int(s)))

    # asm extents, per unit, keyed by address
    def asm_sizes(uname):
        tp = unit_cfg.get(uname, {}).get('target_path')
        if not tp:
            return None
        p = wt / tp.replace('/obj/', '/asm/').replace('.obj', '.s')
        if not p.exists():
            return None
        out = {}
        for line in p.read_text(errors='ignore').splitlines():
            m = SIZE_RE.match(line)
            if m:
                out[m.group(1).lower()] = int(m.group(2), 16)
        return out

    rep_total = asm_total = 0
    checked = missing = 0
    inflated = []
    per_unit_rep = collections.Counter()
    per_unit_asm = collections.Counter()
    for uname, rows in c3.items():
        sizes = asm_sizes(uname)
        for t, rsize in rows:
            rep_total += rsize
            per_unit_rep[uname] += rsize
            addr = t.split('_')[1].lower()
            a = (sizes or {}).get(addr)
            if a is None:
                missing += 1
                asm_total += rsize          # no opinion -> keep report's
                per_unit_asm[uname] += rsize
                continue
            checked += 1
            asm_total += a
            per_unit_asm[uname] += a
            if a != rsize:
                inflated.append((uname, t, rsize, a, rsize - a))

    print(f'class-3 rows checked against asm: {checked}  (no asm opinion: {missing})')
    print(f'  report.json bytes : {rep_total:,}')
    print(f'  dtk asm bytes     : {asm_total:,}')
    print(f'  INFLATION         : {rep_total - asm_total:,} B '
          f'({100.0*(rep_total-asm_total)/rep_total:.1f}% of the class as billed)')
    over = [x for x in inflated if x[4] > 0]
    under = [x for x in inflated if x[4] < 0]
    print(f'  rows disagreeing  : {len(inflated)}  (over-billed {len(over)}, '
          f'under-billed {len(under)})')

    print('\n=== top 25 over-billed rows ===')
    print(f'  {"unit":38s} {"symbol":16s} {"report":>8s} {"asm":>7s} {"delta":>8s}')
    for u, t, r, a, d in sorted(over, key=lambda x: -x[4])[:25]:
        print(f'  {u[:38]:38s} {t:16s} {r:8,d} {a:7,d} {d:8,d}')

    print('\n=== class-3 by unit, RE-RANKED on asm extents (top 25) ===')
    print(f'  {"unit":44s} {"asmB":>8s} {"repB":>8s}')
    for u, b in per_unit_asm.most_common(25):
        print(f'  {u[:44]:44s} {b:8,d} {per_unit_rep[u]:8,d}')


if __name__ == '__main__':
    main()
