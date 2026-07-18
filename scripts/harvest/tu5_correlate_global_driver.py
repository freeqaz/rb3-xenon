#!/usr/bin/env python3
# Global driver for tu5_reloc_masked_correlate: sweeps ALL paired units,
# classifies pairings (CLEAN 1<->1 / MULTI ICF-ambiguous / amb_tgt / base_taken /
# nomatch), emits ready-to-merge map fragment of yield entries.
# Landed +1,493 strict on 2026-07-18 (366709b9). Inputs: pairs.json = per-unit
# {name, tgt: build/45410914/obj/<u>.obj, baseobj: build obj path} enumeration.
import json, sys, os
from collections import defaultdict
sys.path.insert(0, '/home/free/code/milohax/rb3-xenon/scripts/harvest')
import tu5_reloc_masked_correlate as C

ROOT='/home/free/code/milohax/rb3-xenon'
os.chdir(ROOT)
pairs=json.load(open('/home/free/tmp/correlator_sizing/pairs.json'))

# report: map (unit_name) -> {fn_name: match%}
r=json.load(open('build/45410914/report.json'))
rep={}
for u in r['units']:
    rep[u['name']]={f['name']:f.get('match_percent_normalized',0) for f in u.get('functions',[])}

proposals=[]     # clean 1<->1, base not already named in target, target content unique
per_unit=[]
errors=[]
for p in pairs:
    unit=p['name']
    try:
        tgt=C.func_bodies(p['tgt'])
        base=C.func_bodies(p['baseobj'])
    except Exception as e:
        errors.append((unit,str(e))); continue
    base_by_content=defaultdict(list)
    for n,b in base.items():
        base_by_content[b].append(n)
    # target content multiplicity among unmapped fn_
    unmapped={n:b for n,b in tgt.items() if n.startswith('fn_')}
    tgt_content=defaultdict(list)
    for n,b in unmapped.items():
        tgt_content[b].append(n)
    # names already present (named) in target obj = already mapped/matched
    tgt_named=set(n for n in tgt if not n.startswith('fn_'))
    unit_clean=0; unit_multi=0; unit_nomatch=0; unit_amb_tgt=0; unit_base_taken=0
    match_pct=rep.get(unit,{})
    for n in sorted(unmapped):
        b=unmapped[n]
        cands=base_by_content.get(b,[])
        if len(cands)==0:
            unit_nomatch+=1; continue
        if len(cands)>1:
            unit_multi+=1; continue
        bn=cands[0]
        # target-side ICF ambiguity: this content shared by >1 unmapped fn_
        if len(tgt_content[b])>1:
            unit_amb_tgt+=1; continue
        # base name already appears named in target (already mapped) => ICF twin
        if bn in tgt_named:
            unit_base_taken+=1; continue
        # this fn currently NOT strict matched?
        cur=match_pct.get(n,0)
        proposals.append(dict(unit=unit, fn_addr='0x'+n[3:].upper(), fn=n,
                              mangled_name=bn, size=len(b), cur_pct=cur))
        unit_clean+=1
    per_unit.append(dict(unit=unit, clean=unit_clean, multi=unit_multi,
                         nomatch=unit_nomatch, amb_tgt=unit_amb_tgt,
                         base_taken=unit_base_taken,
                         n_unmapped=len(unmapped)))

json.dump(proposals, open('/home/free/tmp/correlator_sizing/proposals.json','w'), indent=1)
json.dump(per_unit, open('/home/free/tmp/correlator_sizing/per_unit.json','w'), indent=1)
json.dump(errors, open('/home/free/tmp/correlator_sizing/errors.json','w'), indent=1)
print('pairs run:', len(pairs), 'errors:', len(errors))
print('CLEAN proposals total:', len(proposals))
print('  of which cur_pct<100:', sum(1 for x in proposals if x['cur_pct']<100))
print('  cur_pct==100 (already matched, no yield):', sum(1 for x in proposals if x['cur_pct']>=100))
# top units by clean
per_unit.sort(key=lambda x:-x['clean'])
print('--- top units by clean count ---')
for x in per_unit[:20]:
    if x['clean']: print(f"  {x['unit']:40s} clean={x['clean']:3d} multi={x['multi']:3d} amb_tgt={x['amb_tgt']:3d} base_taken={x['base_taken']:3d} nomatch={x['nomatch']:3d}")
