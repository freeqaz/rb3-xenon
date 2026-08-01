#!/usr/bin/env python3
"""LANE CI-1 items 2 & 3: re-verify CH-4's secondary-vtable dtor renames and
the StickerProvider repoint, from the oracle, before proposing any insert.

These addresses were DELETED from the map by lane CG-1 ("replacement
uncertain"), so they are ABSENT now -- the repair is an INSERT, not an edit.

The exact `$4PPPPPPPM@...` adjustor mangling is NOT hand-derived here. We ask
our own compiled obj for the home unit which ??_E symbols it exports and
require the proposal to be one of them: that is simultaneously the correctness
check and the home-unit pairing gate, and it cannot be satisfied by a name we
invented.
"""
import sys, os, json, re, collections

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
sys.path.insert(0, os.path.join(HERE, '..', 'maprow_dtor'))
from ci1_dtor_census import Census                                     # noqa
from ci1_adjudicate import (parse_splits, unit_index, unit_of,          # noqa
                            obj_for_unit, coff_symbols)

# (addr, CH-4's claimed class, CH-4's claimed col_offset)
ITEM2 = [
    ('0x827685e0', 'MsgSource',      28),
    ('0x824b8818', 'LightPreset',   216),
    ('0x826b2918', 'PracticePanel', 108),
    ('0x823c1798', 'CharIKHead',    180),
    ('0x826cb610', 'TrainerPanel',  100),
    ('0x8232ac50', None,             48),   # AMBIGUOUS -- brief says leave
]
ITEM3 = [('0x8262c280', 'StickerProvider', 0)]


def main():
    C = Census()
    units = parse_splits()
    iv = unit_index(units)
    objcache = {}

    def unit_syms(u):
        if u not in objcache:
            p = obj_for_unit(u) if u else None
            objcache[u] = coff_symbols(p) if p else set()
        return objcache[u]

    name2addr = collections.defaultdict(list)
    for a, n in C.mapi.items():
        name2addr[n].append(a)

    props = []
    for label, items in (('ITEM2', ITEM2), ('ITEM3', ITEM3)):
        print(f'\n================= {label} =================')
        for addr, claim, coloff in items:
            va = int(addr, 16)
            print(f'\n{addr}  CH-4 claim: {claim} @ col_offset {coloff}')
            print(f'   currently in map: {C.mapi.get(va)!r}  '
                  f'(absent => CG-1 deleted it)')
            att = C.attr_named.get(va, {})
            print(f'   retail vtable attribution: '
                  f'{ {c: [(i, o) for _, i, o in e] for c, e in att.items()} }')
            t = C.dtor_bl(va)
            print(f'   dtor_bl -> {hex(t) if t else None} = '
                  f'{C.mapi.get(t) if t else None}')
            print(f'   vftable refs in body: {sorted(C.vftable_refs(va))}')
            u = unit_of(iv, va)
            syms = unit_syms(u)
            print(f'   home unit: {u}  (our obj exports {len(syms)} symbols)')
            if claim is None:
                print('   -> AMBIGUOUS per brief; LEFT ALONE')
                continue
            # candidate symbols our obj actually exports for this class
            cands = sorted(s for s in syms
                           if re.match(r'\?\?_[EG]%s@@' % re.escape(claim), s))
            print(f'   our obj exports for {claim}: {cands}')
            free = [s for s in cands if s not in name2addr]
            print(f'   of those, NOT already used at another address: {free}')
            # pick: prefer a $4 adjustor form for secondary vtables (coloff>0)
            pick = None
            for s in free:
                if (coloff > 0) == ('$4' in s or '$R' in s or '$1' in s):
                    pick = s
                    break
            if pick is None and free:
                pick = free[0]
            print(f'   PROPOSED: {pick}')
            if pick:
                props.append(dict(addr=addr, expect=None, new=pick,
                                  why=f'{label} {claim} col{coloff} '
                                      f'vtable+dtor_bl+home-unit'))
    json.dump(props, open('/home/free/tmp/laneCI1/edits_item23_raw.json', 'w'),
              indent=1)
    print(f'\n{len(props)} proposals -> edits_item23_raw.json')


if __name__ == '__main__':
    main()
