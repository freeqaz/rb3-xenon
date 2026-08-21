#!/usr/bin/env python3
"""vtable_side_by_side.py -- print retail vs our vtable slot-for-slot.

WHY THIS EXISTS SEPARATELY FROM tools/vtable_order_sweep.py
-----------------------------------------------------------
The sweep reports only the slots it CHARGES, which is the right output for a
worklist and the wrong output for adjudication.  When retail and our table have
DIFFERENT LENGTHS the interesting question is *where the two sequences diverge*
-- an insertion at slot k shifts every later slot, so the sweep shows a run of
"mismatches" that are really one defect, and it shows nothing at all for the
slots its fold/thunk/owner filters excluded.  Those excluded slots are exactly
the ones that tell you whether ours is a PREFIX of retail, a SUFFIX, or a
different table entirely.

⚠ THIS TOOL DELIBERATELY RENDERS NO VERDICT.  It joins OFFSET to OFFSET
(reusing the sweep's own `our_vtable_by_offset` -- never a mangled-name rule;
see §12a of docs/decomp/VTABLE_SLOT_COUNT_FIXES_2026-08-20.md for the
compiler-verified refutation of the name rule) and prints both columns with the
fold annotation attached, so a human adjudicates on retail bytes.  Every prior
wave that shipped a wrong vtable edit did so by trusting a name.

⚠ A retail slot with NO map entry prints `--` and is NOT evidence of agreement;
it is absence of evidence.  Fold occupancy is printed as `x{N}` so an ICF
fold-survivor name is visible as such rather than read as a defect (wave 6
misread exactly that and wave 7 had to correct it).

Usage:
  python3 tools/vtable_side_by_side.py --class BandCamShot
  python3 tools/vtable_side_by_side.py --class BandCamShot --table 1
"""
import argparse
import collections
import json
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(ROOT, 'scripts'))

import vtable_order_sweep as V


def load(project_dir):
    """Rebuild EXACTLY the sweep's inputs, so this cannot disagree with it."""
    R = V.retail()
    exts = R.extents
    with open(os.path.join(project_dir, 'scripts', 'target_symbol_map.json')) as fh:
        raw = json.load(fh)
    addr2name = {}
    for k, v in raw.items():
        if isinstance(v, str):
            addr2name[k.lower()] = v
        elif isinstance(v, list) and v and isinstance(v[0], str):
            addr2name[k.lower()] = v[0]
    vts = sorted(V.enumerate_retail_vtables(R))
    starts = [va for va, _n in vts]
    text = [(s.va, s.va + s.rawsize) for s in R.sections if s.name == '.text'][0]
    tables = {va: V.read_retail_slots(R, va, exts, starts, text) for va, _n in vts}
    occ = V.fold_counts(tables)
    return R, vts, tables, occ, addr2name


def main():
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--class', dest='cls', required=True)
    ap.add_argument('--project-dir', default=ROOT)
    ap.add_argument('--table', type=int, default=None,
                    help="0-based index among this class's retail vtables")
    a = ap.parse_args()

    R, vts, tables, occ, addr2name = load(a.project_dir)
    mine = [(va, n) for va, n in vts if V.bare_class(n) == a.cls]
    if not mine:
        print(f'no retail vtable whose RTTI names {a.cls!r}')
        return 1

    for ti, (va, rtti) in enumerate(mine):
        if a.table is not None and ti != a.table:
            continue
        slots = tables[va]
        sub_off, sub_base = V.retail_subobject_base(R, va)
        ours, how = V.our_vtable_by_offset(a.cls, a.project_dir, sub_off)
        print(f'\n=== {a.cls} table {ti}  vt=0x{va:08x}  '
              f'retail_subobject_offset={sub_off}  (base per retail: {sub_base}) ===')
        print(f'    our table: {how}   retail_slots={len(slots)} '
              f'our_slots={len(ours) if ours else 0}')
        if not ours:
            print('    (no our-side table joined -- nothing to compare)')
            continue
        n = max(len(slots), len(ours))
        for i in range(n):
            if i < len(slots):
                sva, w, in_pdata = slots[i]
                rname = addr2name.get(f'0x{w:08x}', None)
                fold = occ.get(w, 1)
                foldtag = f' x{fold}' if fold != 1 else ''
                pd = '' if in_pdata else ' !nopdata'
                rcol = f'0x{w:08x}{foldtag}{pd} {rname or "--"}'
            else:
                rcol = '(end of retail table)'
            ocol = ours[i]['symbol'] if i < len(ours) else '(end of our table)'
            flag = ''
            if i < len(slots) and i < len(ours):
                rn = addr2name.get(f"0x{slots[i][1]:08x}")
                if rn and rn != ocol:
                    flag = '   <<< DIFFERS'
            print(f'  [{i:2d}] retail {rcol[:96]}')
            print(f'       ours   {ocol[:96]}{flag}')

        # ⛔ ANTI-VACUITY GUARD.  The first version of this tool looked up the
        # map with f'{w:08x}' while the map's keys carry a '0x' prefix, so
        # EVERY retail name resolved to None and every slot printed '--'.
        # Nothing failed; the output just silently became "our column plus
        # positions", which still looks like a usable side-by-side and had
        # already been read as one.  A whole table resolving zero names is not
        # a plausible state of the map -- say so rather than print it.
        named = sum(1 for s in slots
                    if addr2name.get(f'0x{s[1]:08x}') is not None)
        if slots and named == 0:
            print(f'  ⛔ WARNING: 0 of {len(slots)} retail slots resolved to a '
                  f'map name. Every row above reads "--", which is absence of '
                  f'EVIDENCE, not agreement. Suspect the lookup, not the map.')
        else:
            print(f'  [name coverage] {named}/{len(slots)} retail slots named '
                  f'by the map; the rest are unknown, NOT agreeing.')
    return 0


if __name__ == '__main__':
    sys.exit(main())
