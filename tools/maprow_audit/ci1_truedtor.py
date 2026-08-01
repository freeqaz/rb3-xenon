#!/usr/bin/env python3
"""LANE CI-1 part 4: find each class's TRUE ??_G address without assuming
slot 0, then look for CLOSED PERMUTATIONS of map rows.

"slot 0 == destructor" is a Milo Hmx::Object CONVENTION, not a rule: RndDrawable
puts UpdateSphere at 0 and UIPanel-derived classes put Load at 0. So the dtor
slot is IDENTIFIED, per class, by evidence:

    a VA in class C's vtable is C's deleting destructor iff
      (i)  its body branches to a callee (the real ??1/??_D), and
      (ii) that callee materialises C's OWN VTABLE ADDRESS (the vftable store
           every destructor performs).

Both facts are retail bytes; neither consults target_symbol_map.json.

Why a permutation matters: renaming address X from ??_GOld to ??_GNew orphans
??_GOld, because our compiled obj for that unit exports BOTH names and objdiff
pairs by name within a unit. A one-sided rename is therefore a -1 trade even
when it is CORRECT. If the displaced names form a closed cycle inside one unit,
applying the whole cycle is name-count-neutral.
"""
import sys, os, json, re, collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'maprow_dtor'))
from ci1_dtor_census import Census                                    # noqa
from ci1_adjudicate import (parse_splits, unit_index, unit_of,         # noqa
                            obj_for_unit, coff_symbols)


def main():
    out_dir = '/home/free/tmp/laneCI1'
    C = Census()
    units = parse_splits()
    iv = unit_index(units)

    # ---- true dtor address per class, by instrument C -------------------
    true_dtor = {}          # class -> [VAs]
    for cls, vts in C.vt_of_class.items():
        if not cls:
            continue
        hits = []
        for vt, coloff, sl in vts:
            if coloff != 0:
                continue
            for i, fn in enumerate(sl[:12]):     # dtor is early in every Milo vt
                va = C.slot_named[fn]
                if cls in C.vftable_refs(va):
                    hits.append((i, va))
        if hits:
            true_dtor[cls] = sorted(set(hits))
    print(f'[i] classes with a primary vtable: '
          f'{sum(1 for c,v in C.vt_of_class.items() if any(o==0 for _,o,_ in v))}')
    print(f'[i] classes where instrument C identifies a dtor slot: {len(true_dtor)}')
    slotdist = collections.Counter(i for v in true_dtor.values() for i, _ in v)
    print(f'[i] dtor slot index distribution: {dict(sorted(slotdist.items())[:8])}')

    # ---- CONTROL: on rows the census called AGREE, does instrument C put the
    # dtor at the address the map already uses? (positive control)
    rows = json.load(open(os.path.join(out_dir, 'ci1_census.json')))
    ok = bad = nod = 0
    for r in rows:
        if r['verdict_named'] != 'AGREE':
            continue
        cls, va = r['cls'], int(r['addr'], 16)
        td = true_dtor.get(cls)
        if not td:
            nod += 1
            continue
        if va in {a for _, a in td}:
            ok += 1
        else:
            bad += 1
    print(f'[control] AGREE rows: instrument C confirms {ok}, contradicts {bad}, '
          f'silent {nod}  (denominator {ok+bad+nod})')

    # ---- proposed assignment: address -> correct ??_G name ---------------
    want = {}
    for cls, td in true_dtor.items():
        for i, va in td:
            want.setdefault(va, set()).add(cls)
    amb = sum(1 for v in want.values() if len(v) > 1)
    print(f'[i] addresses instrument C calls a dtor: {len(want)} '
          f'(ambiguous/multi-class: {amb})')

    json.dump({hex(k): sorted(v) for k, v in want.items()},
              open(os.path.join(out_dir, 'ci1_truedtor.json'), 'w'), indent=1)

    # ---- for each repair candidate, is there a swap partner? -------------
    reps = json.load(open(os.path.join(out_dir, 'ci1_repairs.json')))
    objcache = {}

    def unit_syms(u):
        if u not in objcache:
            p = obj_for_unit(u) if u else None
            objcache[u] = coff_symbols(p) if p else set()
        return objcache[u]

    print('\n=== swap-partner search ===')
    plan = []
    for r in reps:
        va = int(r['addr'], 16)
        old = r['cls']
        td = true_dtor.get(old)
        rec = dict(r)
        rec['old_true_dtor'] = [[i, hex(a), C.mapi.get(a),
                                unit_of(iv, a)] for i, a in (td or [])]
        partner = None
        for i, a in (td or []):
            if a == va:
                continue
            if unit_of(iv, a) == r['unit']:
                partner = a
                break
        rec['swap_partner'] = hex(partner) if partner else None
        rec['swap_partner_mapname'] = C.mapi.get(partner) if partner else None
        plan.append(rec)
        print(f"{r['addr']} {old[:30]:30s} -> {r['proposed'][4:].split('@@')[0][:26]}")
        print(f"   instrument C says ??_G{old} lives at: {rec['old_true_dtor']}")
        print(f"   in-unit swap partner: {rec['swap_partner']} "
              f"(currently {rec['swap_partner_mapname']})")
    json.dump(plan, open(os.path.join(out_dir, 'ci1_plan.json'), 'w'), indent=1)
    print(f'\n-> {out_dir}/ci1_truedtor.json , ci1_plan.json')


if __name__ == '__main__':
    main()
