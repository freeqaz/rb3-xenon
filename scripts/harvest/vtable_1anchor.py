#!/usr/bin/env python3
"""1-anchor recovery pass (precise): for base vtables the 2-anchor pass could
not place, accept a target run when:
  - the single anchor symbol occurs in EXACTLY ONE run position globally
    (unambiguous placement), AND
  - the run length == base vtable length (whole vtable present), AND
  - every aligned already-mangled target slot equals its base method name
    (full-run consistency, 0 disagreements).
Then anon slots inherit their base method name.
"""
import json, os, re, struct, sys, glob, types
PROJ = sys.argv[1] if len(sys.argv) > 1 else '/home/free/tmp/wt-namewave'
RDATA_OBJ = os.path.join(PROJ,'build/45410914/obj/auto_00_82000400_rdata.obj')
src=open('/home/free/tmp/namewave/vtable_global.py').read().replace('main()\n','')
mod=types.ModuleType('vg'); exec(compile(src,'vg','exec'),mod.__dict__)
read_coff=mod.read_coff; vtable_slots=mod.vtable_slots; extract_runs=mod.extract_runs
FN=re.compile(r'^fn_([0-9a-fA-F]{8})')

def main():
    runs=extract_runs(RDATA_OBJ)
    sym_pos={}
    for ri,run in enumerate(runs):
        for p,s in enumerate(run): sym_pos.setdefault(s,[]).append((ri,p))
    o=json.load(open(os.path.join(PROJ,'objdiff.json')))
    mp=json.load(open(os.path.join(PROJ,'scripts/target_symbol_map.json')))
    mapvals=set(v for v in mp.values() if isinstance(v,str))
    mapkeys=set(k.lower() for k in mp)
    defined=set()
    for bp in glob.glob(os.path.join(PROJ,'build/45410914/src/**/*.obj'),recursive=True):
        try: d,syms,secs,sbi=read_coff(bp)
        except: continue
        for s in syms:
            if s['section']>0 and s['name'].startswith('?'): defined.add(s['name'])
    seen=set(); base_vts=[]
    for u in o['units']:
        bp=u.get('base_path')
        if not bp or not os.path.exists(bp): continue
        try: d,syms,secs,sbi=read_coff(bp)
        except: continue
        for vtn,slots in vtable_slots(d,syms,secs,sbi).items():
            bnames=[nm for off,nm in sorted(slots) if nm.startswith('?') and not nm.startswith('??_7')]
            if len(bnames)<3: continue
            key=(vtn,tuple(bnames))
            if key in seen: continue
            seen.add(key); base_vts.append((vtn,bnames,u['name']))
    candidates={}; evidence=[]; matched=0
    for vtn,bnames,un in base_vts:
        anchors={i:nm for i,nm in enumerate(bnames) if nm in mapvals}
        if len(anchors)!=1: continue      # this pass = exactly 1 anchor
        i,nm=next(iter(anchors.items()))
        locs=sym_pos.get(nm,[])
        if len(locs)!=1: continue          # globally unique placement
        ri,p=locs[0]; ao=p-i; run=runs[ri]
        if len(run)!=len(bnames): continue  # whole vtable present
        # full-run consistency
        bad=0
        for j,bn in enumerate(bnames):
            q=j+ao
            if not(0<=q<len(run)): bad+=1; break
            if run[q].startswith('?') and run[q]!=bn: bad+=1; break
        if bad: continue
        matched+=1
        for j,bn in enumerate(bnames):
            q=j+ao; m=FN.match(run[q])
            if not m or bn not in defined: continue
            va='0x'+m.group(1).lower()
            if va in mapkeys: continue
            if va in candidates and candidates[va]!=bn: candidates[va]='__C__'
            elif candidates.get(va)!='__C__':
                candidates[va]=bn; evidence.append((va,bn,vtn,j,un))
    # also drop any name already a map value
    frag={va:nm for va,nm in candidates.items() if nm!='__C__' and nm not in mapvals}
    print(f'matched_vt={matched} names={len(frag)}',file=sys.stderr)
    json.dump(frag,open('/home/free/tmp/namewave/frag_1anchor.json','w'),indent=1)
    with open('/home/free/tmp/namewave/evidence_1anchor.txt','w') as f:
        for va,nm,vtn,j,un in sorted(evidence):
            if frag.get(va)==nm: f.write(f'{va}\t{nm}\tvtable={vtn}\tslot={j}\tunit={un}\tanchors=1uniq\n')
main()
