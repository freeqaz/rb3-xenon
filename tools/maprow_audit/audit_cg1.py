#!/usr/bin/env python3
"""PART 1 (lane CH-4): audit the callee rows lane CG-1's 24 landed map changes
rest on, using an oracle that never consults the map.

CG-1 adjudicated by asking THE MAP what a retail callee is, and said plainly it
had not checked that those callee rows are themselves correct. This re-derives
the same facts from retail bytes:

  ORACLE A (primary): slot0(C) = address at slot 0 of the primary
      (col_offset==0) vtable of the class RTTI labels C, dethunked.
      For a polymorphic class this IS ??_G C. Measured agreement with the map
      over all virtual-method rows: 93.1% present, vs 0.30% under a
      class-label-shuffled null.
      ⚠ ASYMMETRIC: presence is strong evidence, ABSENCE is weak -- 6.9% of
      correct rows are absent, so absence alone can never condemn a row.

  ORACLE B (independent second instrument): dtor_bl(??_G) = the first bl in a
      ??_G body that is not an MSVC save/restore helper. That is ??1 C (or
      ??_D C for virtual-base classes). Agreement 69.2% -- noisier, so it is
      used only as corroboration, never alone.

A and B are genuinely independent of each other: A reads .rdata vtable layout,
B reads .text instruction bytes. Neither reads target_symbol_map.json.
"""
import sys, os, json, re, collections, pickle

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'maprow_dtor'))
from retail_reader import Image, insns, branch_target          # noqa
from rtti_vtable_index import Rtti, demangle_class, pdata_sizes  # noqa
from vtable_attr import Attr, dethunk, thunk_target             # noqa

ALLOW = {36, 38, 44, 46, 54, 55, 50, 51, 32, 34, 40, 42, 14, 31, 37, 53,
         62, 58, 60, 63}


class Oracle:
    def __init__(self):
        self.img = Image()
        self.sizes = pdata_sizes(self.img)
        self.A = Attr(self.img)
        self.prim = {}    # class -> set of slot0 addresses (dethunked)
        self.byslot = collections.defaultdict(dict)
        for va, cs in self.A.deth.items():
            for c, ents in cs.items():
                for vt, i, off in ents:
                    if off == 0 and i == 0:
                        self.prim.setdefault(c, set()).add(va)
                    self.byslot[c][i] = self.byslot[c].get(i) or va
        self._h = {}

    def has_vtable(self, c):
        return c in self.prim

    def slot0(self, c):
        return self.prim.get(c, set())

    def is_helper(self, va):
        if va in self._h:
            return self._h[va]
        r = False
        if not self.sizes.get(va):
            for a, i in insns(self.img, va, 96):
                if i == 0x4E800020:
                    r = True
                    break
                if (i >> 26) not in ALLOW:
                    break
        self._h[va] = r
        return r

    def dtor_bl(self, va):
        sz = self.sizes.get(va)
        if not sz:
            return None
        for a, i in insns(self.img, va, sz):
            t = branch_target(a, i)
            if t and t[1]:
                if self.is_helper(t[0]):
                    continue
                return t[0]
        return None

    def classes_of(self, va):
        return set(self.A.deth.get(va, {}))


def main():
    o = Oracle()
    raw = json.load(open(os.path.join(HERE, '..', '..',
                                      'scripts/target_symbol_map.json')))
    M = {k: v for k, v in raw.items() if k.startswith('0x') and isinstance(v, str)}
    Mi = {int(k, 16): v for k, v in M.items()}
    acts = json.load(open('/home/free/tmp/laneCG1/final_actions.json'))
    coh = {r['sym']: r for r in
           json.load(open('/home/free/tmp/laneCG1/cohort_adjudicated.json'))}

    def cls_of_sym(s):
        m = re.match(r'\?\?_[GE]([A-Za-z_0-9@?$]+?)@@', s)
        return m.group(1) if m else None

    out = []
    for a in acts:
        if a['action'] == 'KEEP':
            continue
        va = int(a['addr'], 16)
        rec = dict(action=a['action'], addr=a['addr'], sym=a['sym'],
                   cls=a['cls'], strat=a['strat'], new=a.get('new'),
                   why=a['why'])
        rec['retail_classes'] = sorted(o.classes_of(va))
        rec['is_slot0_of'] = sorted(c for c in o.prim if va in o.prim[c])
        rec['row_cls_has_vtable'] = o.has_vtable(a['cls'])
        rec['row_cls_slot0'] = [hex(x) for x in o.slot0(a['cls'])]
        newc = cls_of_sym(a['new']) if a.get('new') else None
        rec['new_cls'] = newc
        if newc:
            rec['new_cls_has_vtable'] = o.has_vtable(newc)
            rec['new_cls_slot0'] = [hex(x) for x in o.slot0(newc)]
            rec['ORACLE_A'] = ('CONFIRMS_NEW' if va in o.slot0(newc)
                               else ('NO_VTABLE_FOR_NEW' if not o.has_vtable(newc)
                                     else 'SILENT'))
        # oracle B: what does this body destroy, read from retail?
        t = o.dtor_bl(va)
        rec['dtor_bl'] = hex(t) if t else None
        rec['dtor_bl_mapname'] = Mi.get(t) if t else None
        # thunk rows: resolve ourselves
        d, depth = dethunk(o.img, va)
        rec['dethunks_to'] = hex(d) if d != va else None
        rec['dethunk_depth'] = depth
        rec['dethunk_target_mapname'] = Mi.get(d) if d != va else None
        rec['dethunk_target_classes'] = sorted(o.classes_of(d)) if d != va else []
        out.append(rec)

    json.dump(out, open('/home/free/tmp/laneCH4/part1_audit.json', 'w'), indent=1)
    print(f'audited {len(out)} landed changes -> part1_audit.json')
    for r in out:
        print(f"\n--- {r['action']} {r['addr']} {r['sym'][:58]}")
        print(f"    row class {r['cls']!r} has_vtable={r['row_cls_has_vtable']} "
              f"slot0={r['row_cls_slot0']}")
        print(f"    retail says this VA is in vtables of: {r['retail_classes']}")
        print(f"    is slot0 of: {r['is_slot0_of']}")
        if r.get('new_cls'):
            print(f"    NEW {r['new_cls']!r} has_vtable={r.get('new_cls_has_vtable')} "
                  f"slot0={r.get('new_cls_slot0')}  => ORACLE_A={r.get('ORACLE_A')}")
        print(f"    dtor_bl -> {r['dtor_bl']} = {r['dtor_bl_mapname']}")
        if r['dethunks_to']:
            print(f"    dethunks(depth {r['dethunk_depth']}) -> {r['dethunks_to']} "
                  f"= {r['dethunk_target_mapname']} classes={r['dethunk_target_classes']}")


if __name__ == '__main__':
    main()
