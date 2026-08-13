#!/usr/bin/env python3
"""Lane MISPIN-1: the LOCAL evidence for (or against) a `.text` span move.

The census (mispin_census.py) answers WHO DEFINES the proposed name.  That is
necessary but not sufficient: a span move also has to be justified GEOMETRICALLY
-- the block being moved must look like it belongs to the destination unit, and
the hole it leaves must look like a hole.  This file supplies that half.

WHY `.pdata` CANNOT SIZE THESE SPANS (measured, and it changes the method)
--------------------------------------------------------------------------
The house rule is "size a span by `.pdata`, never by naming".  It does not apply
here and the reason is structural: an adjustor thunk is an 8-byte leaf that
touches neither the stack nor the link register, so it gets NO unwind record.
Measured on this worklist: 0 of 8 addresses are `.pdata` BeginAddresses and
`pdata_sizes` returns None for all 8 -- the same sub-`.pdata` stub stratum
AUDIT-NC bounded CD-7's ICF census away from.  So `.pdata` is silent, and the
ruler has to be the THUNK BODY itself: decode forward from A to its tail branch.

WHAT IS PRINTED PER ROW
  - the pin block that currently contains A, and A's position inside it
  - every retail `.pdata` start inside that block (real function boundaries)
  - the destination unit's pins bracketing A, and the resulting HOLE
  - the retail thunk at A: its instructions, its tail-branch target, and what
    the map calls that target (the identity channel)
  - the map rows in [block start, block end) so a move can be checked for
    collateral: which OTHER names would ride along with the moved span
"""
import argparse
import collections
import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..'))
sys.path.insert(0, os.path.join(HERE, '..', 'maprow_dtor'))

import col_cycles as CC                                          # noqa: E402
import col_steps as CS                                           # noqa: E402
from rtti_vtable_index import pdata_starts, pdata_sizes          # noqa: E402
from retail_reader import Image as RImage, insns, tail_b_target  # noqa: E402

BAND = 'orig/45410914/band.exe'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--census', default=os.path.expanduser('~/tmp/mispin_census.json'))
    a = ap.parse_args()

    rows = json.load(open(a.census))
    S = CC.solve()
    tmap = S['tmap']
    owners = CS.text_owners()
    img = RImage(BAND)
    pds = sorted(pdata_starts(img))
    psz = pdata_sizes(img)

    pins = collections.defaultdict(list)
    for s, e, u in owners:
        pins[u].append((s, e))

    # map rows by address for collateral analysis
    maddr = sorted((int(k, 16), v) for k, v in tmap.items() if isinstance(v, str))

    for r in rows:
        A = int(r['addr'], 16)
        print('=' * 78)
        print('%s  %s' % (r['addr'], 'DENIED' if r['denied'] else ''))
        print('  incumbent : %s' % r['cur'])
        print('  proposed  : %s' % r['new'])
        print('  pinned in : %s' % r['pinned_unit'])
        print('  defined in: %s  (%s)'
              % (', '.join(r['defining_objs']),
                 'SINGLETON' if r['singleton'] else 'MULTI -- census not decisive alone'))

        blk = next(((s, e) for (s, e, u) in owners if s <= A < e), None)
        if blk:
            s, e = blk
            print('  pin block containing A: 0x%08X-0x%08X (%d B); A at +%d, %d B to end'
                  % (s, e, e - s, A - s, e - A))
            inside = [p for p in pds if s <= p < e]
            print('  retail .pdata starts inside that block: %d %s'
                  % (len(inside), ' '.join('0x%08X' % p for p in inside[:12])))
            print('  A is a .pdata start: %s' % (A in set(pds)))
            # collateral: map rows inside the block
            coll = [(x, n) for (x, n) in maddr if s <= x < e]
            print('  map rows inside the block: %d' % len(coll))
            for x, n in coll:
                print('      0x%08X %s%s' % (x, n[:88], '   <== A' if x == A else ''))

        # destination-unit geometry
        for du in r['defining_units']:
            ps = sorted(pins[du])
            before = [p for p in ps if p[1] <= A]
            after = [p for p in ps if p[0] > A]
            b = before[-1] if before else None
            f = after[0] if after else None
            print('  dest %s:' % du)
            if b:
                print('      prev pin ends   0x%08X   (%d B before A)' % (b[1], A - b[1]))
            if f:
                print('      next pin starts 0x%08X   (%d B after A)' % (f[0], f[0] - A))
            if b and f:
                print('      => HOLE 0x%08X-0x%08X (%d B); A sits %s'
                      % (b[1], f[0], f[0] - b[1],
                         'INSIDE it' if b[1] <= A < f[0] else 'OUTSIDE it'))

        # retail thunk body
        print('  retail thunk at A:')
        for sz in (32, 24, 16, 8):
            t = None
            try:
                t = tail_b_target(img, A, sz)
            except Exception:
                pass
            if t:
                print('      tail branch (window %d B) -> 0x%08X  = %s'
                      % (sz, t, tmap.get('0x%08x' % t, 'UNMAPPED')))
                break
        try:
            for ins in insns(img, A, 24):
                print('      %s' % (ins,))
        except Exception as ex:
            print('      <decode failed: %s>' % ex)


if __name__ == '__main__':
    main()
