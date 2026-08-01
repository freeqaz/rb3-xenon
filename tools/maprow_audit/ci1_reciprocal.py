#!/usr/bin/env python3
"""LANE CI-1 part 3: turn one-sided steals into RECIPROCAL swaps.

A repair renames address X from ??_GOld to ??_GNew. Our home-unit obj exports
BOTH names, so the rename hands ??_GNew a partner and simultaneously ORPHANS
??_GOld -- a net-zero (or negative) trade unless we also point ??_GOld at its
real retail address.

The oracle answers that directly: ??_GOld should be the occupant of class Old's
own vtable at the SAME slot index where X sits in New's vtable (siblings share
layout). We read it and report what the map currently calls it.
"""
import sys, os, json, re, collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'maprow_dtor'))
from ci1_dtor_census import Census                                    # noqa
from ci1_adjudicate import (parse_splits, unit_index, unit_of,        # noqa
                            obj_for_unit, coff_symbols)


def main():
    out_dir = '/home/free/tmp/laneCI1'
    C = Census()
    reps = json.load(open(os.path.join(out_dir, 'ci1_repairs.json')))
    units = parse_splits()
    iv = unit_index(units)
    name2addr = collections.defaultdict(list)
    for a, n in C.mapi.items():
        name2addr[n].append(a)

    objcache = {}

    def unit_syms(u):
        if u not in objcache:
            p = obj_for_unit(u) if u else None
            objcache[u] = coff_symbols(p) if p else set()
        return objcache[u]

    out = []
    for r in reps:
        va = int(r['addr'], 16)
        old = r['cls']
        newc = r['proposed'][4:].split('@@')[0]
        # slot index where va sits in newc's vtable
        slots = [(i, off) for c, i, off in
                 [(x['cls'], x['slot'], x['coloff']) for x in r['cands']]
                 if c == newc] if False else \
                [(x['slot'], x['coloff']) for x in r['cands'] if x['cls'] == newc]
        rec = dict(addr=r['addr'], sym=r['sym'], old_cls=old,
                   proposed=r['proposed'], unit=r['unit'], slots=slots)
        # what occupies OLD's vtable at the same slot?
        occ = []
        for (i, off) in slots:
            for vt, coloff, sl in C.vt_of_class.get(old, []):
                if coloff != off or i >= len(sl):
                    continue
                a2 = C.slot_named[sl[i]]
                occ.append(dict(slot=i, coloff=off, addr=hex(a2),
                                mapname=C.mapi.get(a2),
                                same_unit=unit_of(iv, a2) == r['unit'],
                                unit=unit_of(iv, a2)))
        rec['old_slot_occupant'] = occ
        # is the old name exported by our home-unit obj (i.e. does the rename
        # actually orphan something)?
        rec['unit_exports_old'] = r['sym'] in unit_syms(r['unit'])
        rec['unit_exports_new'] = r['proposed'] in unit_syms(r['unit'])
        out.append(rec)
        print(f"\n{r['addr']} {r['sym'][:50]}")
        print(f"   -> {r['proposed'][:50]}  unit={r['unit']}")
        print(f"   our obj exports old={rec['unit_exports_old']} "
              f"new={rec['unit_exports_new']}")
        for o in occ:
            print(f"   OLD {old} vtable slot {o['slot']}/col{o['coloff']} = "
                  f"{o['addr']} map={o['mapname']} unit={o['unit']} "
                  f"same_unit={o['same_unit']}")
        if not occ:
            print(f"   (no vtable for OLD class {old} at that slot)")

    json.dump(out, open(os.path.join(out_dir, 'ci1_reciprocal.json'), 'w'),
              indent=1)
    print(f'\n-> {out_dir}/ci1_reciprocal.json')


if __name__ == '__main__':
    main()
