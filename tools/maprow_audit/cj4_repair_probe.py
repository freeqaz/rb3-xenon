#!/usr/bin/env python3
"""For each D_weak_external row, test the CI-1 repair rule under a HOME-UNIT GATE.

The rule: the map slot names ??_E<C>@@UAAPAXI@Z which our obj emits only as a
WEAK EXTERNAL (section 0, storage 105) aliasing ??_G<C>@@UAAPAXI@Z.  A weak
external cannot pair, so the slot should carry the DEFINED form instead.

★★ HOME-UNIT GATE (CH-3 measured -4 from 9 cross-unit moves): the replacement
form must be DEFINED IN THE SAME UNIT the row is homed to.  A replacement that
is defined in some OTHER unit is REFUSED, not landed.
"""
import json, os, sys, collections
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cj4_coff as coff, cj4_norm as norm

WT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', '..')
detail = json.load(open(os.environ.get('CJ4_DETAIL','/home/free/tmp/laneCJ4/detail.json')))
cfg = json.load(open(os.path.join(WT, 'objdiff.json')))
unit_base = {u['name']: u.get('base_path') for u in cfg['units']}
unit_tgt = {u['name']: u.get('target_path') for u in cfg['units']}

cache = {}
def state(unit):
    if unit not in cache:
        p = unit_base.get(unit)
        cache[unit] = (coff.classify(open(os.path.join(WT, p), 'rb').read())
                       if p and os.path.exists(os.path.join(WT, p)) else {})
    return cache[unit]

tcache = {}
def tstate(unit):
    if unit not in tcache:
        p = unit_tgt.get(unit)
        tcache[unit] = (coff.classify(open(os.path.join(WT, p), 'rb').read())
                        if p and os.path.exists(os.path.join(WT, p)) else {})
    return tcache[unit]

# where is each name defined across ALL compiled base objs (for the gate)
allc = collections.defaultdict(list)
for u in cfg['units']:
    p = u.get('base_path')
    if p and os.path.exists(os.path.join(WT, p)):
        for n, st in state(u['name']).items():
            if st == 'DEFINED':
                allc[n].append(u['name'])

edits = []
print(f"{'addr':<12} {'incumbent':<44} {'candidate':<44} verdict")
for addr, name, home, _ in detail.get('D_weak_external', []):
    assert name.startswith('??_E')
    cand = '??_G' + name[4:]
    hs = state(home)
    st_inc, st_cand = hs.get(name), hs.get(cand)
    where = allc.get(cand, [])
    if st_cand == 'DEFINED' and home in where:
        v = 'REPAIR (home-unit gate PASS)'
        edits.append(dict(addr=addr, expect=name, new=cand,
                          why=f'incumbent is WEAK EXTERNAL (sc105) aliasing {cand}; '
                              f'{cand} DEFINED in home unit {home}'))
    elif st_cand == 'DEFINED':
        v = f'REFUSE cand defined but not in home ({where[:2]})'
    else:
        v = f'REFUSE cand state={st_cand} in home; defined@{where[:2]}'
    print(f'{addr:<12} {name:<44} {cand:<44} {v}')

json.dump(edits, open(os.environ.get('CJ4_EDITS','/home/free/tmp/laneCJ4/edits.json'), 'w'), indent=1)
print(f'\n{len(edits)} edits -> /home/free/tmp/laneCJ4/edits.json')
