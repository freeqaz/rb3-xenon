#!/usr/bin/env python3
"""Class-3 DISCRIMINATOR: is "we do not hold the body" UNWRITTEN or DIVERGENT?

The class-3 definition (no compiled body hashes equal to this retail row's) is
silent about the mechanism, and the two mechanisms need completely different
work:

  UNWRITTEN  retail's pinned span contains functions our source never defines.
             Work = write new code from an oracle.
  DIVERGENT  we DO compile a symbol for it, but our body differs.  Work =
             ordinary near-miss matching on code that already exists.

There is no name to join on (these rows are anonymous by construction), so the
discriminator is a COUNTING one, per unit:

    supply_side   = code symbols our base obj defines
    demand_side   = code rows in the retail target obj
    deficit       = demand_side - supply_side

A unit where retail has 46 functions and we compile 12 is short of code.  A
unit where retail has 46 and we compile 200 is not short of code at all -- its
class-3 rows are divergence (or misattribution), and writing new bodies there
would be wrong.

⚠ The counts are NOT directly comparable without care: our base obj carries
COMDAT template/inline instantiations that retail folded away or never emitted
in this TU, so supply_side is biased UP.  The deficit is therefore a
CONSERVATIVE unwritten-detector: a positive deficit is strong evidence of
missing code; a negative one is not proof of its absence.  Reported as such.
"""

import argparse
import collections
import json
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from ident_body_channel import (  # noqa: E402
    parse_coff, function_slices, is_placeholder,
)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--worktree', required=True)
    ap.add_argument('--units-tsv', required=True, help='output of nobody_unit_census.py')
    ap.add_argument('--top', type=int, default=40)
    args = ap.parse_args()
    wt = Path(args.worktree)

    objdiff = json.loads((wt / 'objdiff.json').read_text())
    unit_cfg = {u['name']: u for u in objdiff['units']}

    want = []
    with open(args.units_tsv) as fh:
        next(fh)
        for line in fh:
            u, rows, byts, ub, um, src = line.rstrip('\n').split('\t')
            want.append((u, int(rows), int(byts), src))
    want = want[:args.top]

    print(f'{"unit":42s} {"c3rows":>6s} {"c3B":>7s} {"tgtfn":>6s} {"ourfn":>6s} '
          f'{"deficit":>7s}  verdict')
    tot = collections.Counter()
    totb = collections.Counter()
    for uname, c3rows, c3b, src in want:
        u = unit_cfg.get(uname, {})
        bp, tp = u.get('base_path'), u.get('target_path')
        nb = nt = 0
        if bp and (wt / bp).exists():
            nb = sum(1 for _ in function_slices(wt / bp))
        if tp and (wt / tp).exists():
            nt = sum(1 for _ in function_slices(wt / tp))
        deficit = nt - nb
        verdict = 'UNWRITTEN (short of code)' if deficit > 0 else 'divergent/other'
        tot[verdict] += c3rows
        totb[verdict] += c3b
        print(f'{uname[:42]:42s} {c3rows:6d} {c3b:7,d} {nt:6d} {nb:6d} '
              f'{deficit:+7d}  {verdict}')
    print()
    for k in tot:
        print(f'  {k:26s} {tot[k]:6d} class-3 rows  {totb[k]:9,d} B')


if __name__ == '__main__':
    main()
