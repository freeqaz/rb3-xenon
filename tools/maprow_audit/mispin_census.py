#!/usr/bin/env python3
"""Lane MISPIN-1: establishing TRUE OWNERSHIP for the 8 rows no rename can pay.

WHAT THIS IS FOR
----------------
STEPS-1 split the non-closable single-step repairs on one test -- does our obj
for the unit that OWNS address A define the proposed name?  DEFINED rows pay
(Delta functions 0, bytes arrive); NOT_DEFINED rows do not, and they are not
naming defects at all: the name is defined in a DIFFERENT unit than the splits
pin.  Those are MIS-PINS, and the remedy is a `.text` span move in splits.txt,
never a map edit.  STEPS-1 correctly declined to land them.  This file works
them.

THE INSTRUMENT, AND WHY THE OBVIOUS ONE IS VACUOUS
---------------------------------------------------
The tempting probe is two-unit: "the name is defined in X's obj and absent from
Y's, therefore X owns it".  That is decisive ONLY when the defining set is a
SINGLETON.  On a template/inline COMDAT the same symbol is emitted by every
instantiating TU -- one such set was measured at FIVE members -- so the two-unit
probe confirms whichever unit you happened to point it at.  This file therefore
runs the DEFINING-SET CENSUS OVER THE WHOLE TREE (~1,200 objs) and reports the
cardinality, so a multi-definer row is visible as multi rather than silently
adjudicated.

CORROBORATION (the pattern that worked twice for the splits lanes)
  A correctly-owned re-homing shows the DEFINING unit's pinned address range
  having exactly the HOLE that the moved block fills.  So for every row we print
  the defining unit's own `.text` pins and where A sits relative to them --
  adjacency/hole-filling is evidence, an arbitrary distant pin is not.

SIZING.  A span is sized from retail `.pdata`, never "up to the next named
symbol" -- one row measured 88 B by `.pdata` and 304 B by naming.  `.pdata` is
big-endian and its word 2 is prologLen = w & 0xFF, funcLen = (w >> 8) & 0x3FFFFF
in INSTRUCTIONS; an MSB-first read yields 25 KB "functions".  We reuse
rtti_vtable_index.pdata_sizes (which carries its own shift control) rather than
re-decoding.

⚠ `.pdata` in splits.txt is DERIVED OUTPUT -- every split run clears the whole
`.pdata` set and re-derives one range per `.text` block.  Only `.text` is ever
edited.
"""
import argparse
import collections
import glob
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
from cj4_coff import read_symbols                                # noqa: E402
from rtti_vtable_index import pdata_sizes, pdata_starts          # noqa: E402
from retail_reader import Image as RImage                        # noqa: E402

OURS = 'build/45410914/src'
BAND = 'orig/45410914/band.exe'


def defining_sets(objroot=OURS):
    """-> {symbol: {obj basename, ...}} over the WHOLE tree.

    SectionNumber > 0 is the only field separating a DEFINITION from an
    undefined external REFERENCE; a substring scan cannot tell them apart.
    """
    out = collections.defaultdict(set)
    nobj = 0
    for p in glob.glob(os.path.join(objroot, '**', '*.obj'), recursive=True):
        try:
            syms = read_symbols(open(p, 'rb').read())
        except Exception:
            continue
        nobj += 1
        b = os.path.basename(p)
        for s in syms:
            if s.section > 0:
                out[s.name].add(b)
    return out, nobj


def unit_pins(splits=CS.SPLITS):
    """-> {unit: [(start, end)]} for .text only, plus the flat owner list."""
    d = collections.defaultdict(list)
    for s, e, u in CS.text_owners(splits):
        d[u].append((s, e))
    return d


def obj_of(unit):
    return os.path.basename(unit)[:-4] + '.obj'


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--json', help='write the census here')
    a = ap.parse_args()

    S = CC.solve()
    sel, rej, st = CS.select(S)
    mis = sorted(k for k, v in rej.items() if 'MIS-PIN' in v)
    print('candidates %(candidates)d -> CASE1 %(case1)d -> STEPS-1 selected %(selected)d'
          % st)
    print('MIS-PIN rows (the worklist): %d\n' % len(mis))

    defs, nobj = defining_sets()
    print('[census] defining sets built over %d objs, %d distinct defined symbols\n'
          % (nobj, len(defs)))

    pins = unit_pins()
    owners = CS.text_owners()

    def owner(x):
        for s, e, u in owners:
            if s <= x < e:
                return u

    img = RImage(BAND)
    sizes = pdata_sizes(img)
    starts = set(pdata_starts(img))

    rows = []
    for k in mis:
        A = int(k, 16)
        cd = S['cand'][A]
        pinned = owner(A)
        dset = sorted(defs.get(cd['new'], ()))
        cset = sorted(defs.get(cd['cur'], ()))
        # which splits units correspond to the defining objs?
        obj2unit = collections.defaultdict(list)
        for u in pins:
            obj2unit[obj_of(u)].append(u)
        dunits = sorted({u for o in dset for u in obj2unit.get(o, ())})

        r = dict(addr=k, cur=cd['cur'], new=cd['new'], cls=cd['cls'], slot=cd['slot'],
                 pinned_unit=pinned, pinned_obj=obj_of(pinned) if pinned else None,
                 defining_objs=dset, defining_units=dunits,
                 incumbent_defining_objs=cset,
                 singleton=len(dset) == 1,
                 pdata_start=A in starts, pdata_size=sizes.get(A),
                 denied=A in S['deny'])
        rows.append(r)

        print('=' * 78)
        print('%s   pinned in %s' % (k, pinned))
        print('  incumbent : %s' % cd['cur'])
        print('  proposed  : %s   (%s slot %d)' % (cd['new'], cd['cls'], cd['slot']))
        print('  .pdata    : start=%s size=%s' % (r['pdata_start'], r['pdata_size']))
        print('  denylist  : %s' % r['denied'])
        print('  DEFINING SET for proposed name: %d obj(s) %s'
              % (len(dset), '<-- SINGLETON, decisive' if len(dset) == 1 else
                 ('<-- EMPTY: nobody defines it' if not dset else
                  '<-- MULTI (COMDAT): two-unit probe would be vacuous')))
        for o in dset:
            print('      %s%s' % (o, '   [== the pin]' if pinned and o == obj_of(pinned) else ''))
        if dunits:
            print('  defining unit pins:')
            for u in dunits:
                for (s2, e2) in sorted(pins[u]):
                    inside = '  <== A IS INSIDE' if s2 <= A < e2 else ''
                    print('      %-34s .text 0x%08X-0x%08X%s' % (u, s2, e2, inside))
        print('  incumbent name defined in: %s' % (cset or 'nobody'))

    if a.json:
        json.dump(rows, open(a.json, 'w'), indent=1)
        print('\nwrote %s' % a.json)

    n_single = sum(1 for r in rows if r['singleton'])
    n_empty = sum(1 for r in rows if not r['defining_objs'])
    print('\n[summary] %d rows: %d singleton-defined (decisive), %d multi-defined, '
          '%d defined by NOBODY' % (len(rows), n_single,
                                    len(rows) - n_single - n_empty, n_empty))


if __name__ == '__main__':
    main()
