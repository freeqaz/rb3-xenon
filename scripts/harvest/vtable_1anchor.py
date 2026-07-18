#!/usr/bin/env python3
"""1-anchor recovery pass (precise, gated): for base vtables the 2-anchor pass
could not place, accept a target run when:
  - the single anchor symbol occurs in EXACTLY ONE run position globally
    (unambiguous placement), AND
  - the run length == base vtable length (whole vtable present), AND
  - every aligned already-mangled target slot equals its base method name
    (full-run consistency, 0 disagreements).
Then anon slots inherit their base method name.

Shares the 4-part output gate with vtable_global.py (owning-unit router,
purecall guard, return-shape sanity, current-map skip) — see its module doc.

Usage: vtable_1anchor.py [PROJ] [OUTDIR]
Outputs: OUTDIR/1anchor_frag.json          (live, pairable)
         OUTDIR/1anchor_frag_unpinned.json (review bucket, do NOT auto-seed)
         OUTDIR/1anchor_rejects.json
         OUTDIR/1anchor_evidence.txt
"""
import json, os, re, sys, glob, importlib.util

_here=os.path.dirname(os.path.abspath(__file__))
_spec=importlib.util.spec_from_file_location('vtable_global',os.path.join(_here,'vtable_global.py'))
vg=importlib.util.module_from_spec(_spec); _spec.loader.exec_module(vg)
read_coff=vg.read_coff; vtable_slots=vg.vtable_slots; extract_runs=vg.extract_runs
Gate=vg.Gate; gate_candidates=vg.gate_candidates; write_outputs=vg.write_outputs

PROJ = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.getcwd()
OUTDIR = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.expanduser('~/tmp/vtgate_out')
RDATA_OBJ = os.path.join(PROJ,'build/45410914/obj/auto_00_82000400_rdata.obj')
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

    gate=Gate(PROJ)
    gate.index_units(o['units'])

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

    candidates={}; evidence={}; matched=0
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
            if bn in mapvals: continue
            va='0x'+m.group(1).lower()
            if va in mapkeys: continue
            if va in candidates and candidates[va]!=bn: candidates[va]='__C__'
            elif candidates.get(va)!='__C__':
                candidates[va]=bn; evidence[va]=dict(vtable=vtn,slot=j,unit=un,anchors='1uniq')
    print(f'matched_vt={matched} candidates={len(candidates)}',file=sys.stderr)

    live,review,rejects,stats=gate_candidates(gate,candidates,evidence)
    write_outputs(OUTDIR,'1anchor',live,review,rejects,evidence,stats,'[gate-1anchor]')

if __name__=='__main__':
    main()
