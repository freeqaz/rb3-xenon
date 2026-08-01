#!/usr/bin/env python3
"""UNTREATED-POPULATION CONTROL for the ??_E -> ??_G home-unit gate.

A gate that passes 10/10 on the treated stratum proves nothing unless it can
REFUSE.  So run the identical rule over the ??_E rows the audit did NOT charge:

  * class C (incumbent already DEFINED)  -> no repair is needed; if the gate
    would still "REPAIR" these, it is firing on the whole population.
  * class G (absent everywhere)          -> the ??_G candidate should also be
    absent, so the gate must REFUSE.

Prints the refusal rate per stratum.  A reachable refusal branch is the point.
"""
import json, os, sys, collections, re, bisect
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import cj4_coff as coff, cj4_norm as norm

WT = sys.argv[1] if len(sys.argv) > 1 else os.path.join(
    os.path.dirname(os.path.abspath(__file__)), '..', '..')
cfg = json.load(open(os.path.join(WT, 'objdiff.json')))
unit_base = {u['name']: u.get('base_path') for u in cfg['units']}
m = json.load(open(os.path.join(WT, 'scripts/target_symbol_map.json')))
rows = {k: v for k, v in m.items() if k.lower().startswith('0x') and isinstance(v, str)}

cache = {}
def state(unit):
    if unit not in cache:
        p = unit_base.get(unit)
        cache[unit] = (coff.classify(open(os.path.join(WT, p), 'rb').read())
                       if p and os.path.exists(os.path.join(WT, p)) else {})
    return cache[unit]

# rebuild empirical home index over LIVE target objs
tgt_def = collections.defaultdict(list)
for u in cfg['units']:
    p = u.get('target_path')
    fp = os.path.join(WT, p) if p else None
    if fp and os.path.exists(fp):
        for n, st in coff.classify(open(fp, 'rb').read()).items():
            if st == 'DEFINED':
                tgt_def[norm.key(n)].append(u['name'])

strata = collections.defaultdict(lambda: collections.Counter())
examples = collections.defaultdict(list)
for addr, name in rows.items():
    if not name.startswith('??_E'):
        continue
    homes = tgt_def.get(norm.key(name), [])
    if len(homes) != 1:
        continue
    home = homes[0]
    if not unit_base.get(home) or not os.path.exists(os.path.join(WT, unit_base[home])):
        continue
    hs = state(home)
    inc = hs.get(norm.key(name))
    cand = '??_G' + name[4:]
    cs = hs.get(norm.key(cand))
    stratum = {'DEFINED': 'C_already_defined', 'WEAK': 'D_weak(treated)',
               'UNDEF': 'E_undef_ref', 'COMMON': 'E_undef_ref'}.get(inc, 'G_absent')
    verdict = 'WOULD-REPAIR' if cs == 'DEFINED' else 'REFUSE'
    strata[stratum][verdict] += 1
    if len(examples[(stratum, verdict)]) < 3:
        examples[(stratum, verdict)].append((addr, name, cs))

print(f"{'stratum':<20} {'N':>5} {'WOULD-REPAIR':>13} {'REFUSE':>8}  refusal%")
for s in sorted(strata):
    c = strata[s]; n = sum(c.values())
    print(f'{s:<20} {n:>5} {c["WOULD-REPAIR"]:>13} {c["REFUSE"]:>8}  {100*c["REFUSE"]/n:6.1f}%')
print('\nexamples:')
for k in sorted(examples):
    print(' ', k, examples[k][:2])
