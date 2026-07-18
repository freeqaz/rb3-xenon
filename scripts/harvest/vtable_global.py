#!/usr/bin/env python3
"""Global vtable alignment.

The entire retail .rdata (all vtable pointer arrays) lives in one dtk target
obj: build/45410914/obj/auto_00_82000400_rdata.obj. Its relocations, sorted by
offset, form runs = the retail vtables (each pointer -> fn_<VA> anon, or a
mangled name if that VA is already mapped).

Base side: every ??_7Class@@6B@ COMDAT across base objs gives an ordered list of
method mangled names.

Align each base vtable to a target run by ALREADY-MAPPED anchor names at a
constant slot offset (>=2 anchors, 0 disagreements). Then anon target slots
inherit the base method name -> new identification.
"""
import json, os, re, struct, sys, glob, types

PROJ = sys.argv[1] if len(sys.argv) > 1 else '/home/free/tmp/wt-namewave'
RDATA_OBJ = os.path.join(PROJ,'build/45410914/obj/auto_00_82000400_rdata.obj')

src=open('/home/free/tmp/namewave/vtable_align.py').read().replace('main()\n','')
mod=types.ModuleType('va'); exec(compile(src,'va','exec'),mod.__dict__)
read_coff=mod.read_coff; vtable_slots=mod.vtable_slots
FN=re.compile(r'^fn_([0-9a-fA-F]{8})')

def extract_runs(obj):
    d,syms,secs,sbi=read_coff(obj)
    runs=[]
    for sec in secs:
        if sec['nrel']==0: continue
        rels=[]
        for r in range(sec['nrel']):
            ro=sec['reloff']+r*10
            rva,sidx,rt=struct.unpack_from('<IIH',d,ro)
            ts=sbi.get(sidx,{}).get('name','')
            rels.append((rva,ts))
        rels.sort()
        cur=[]; prev=None
        for off,ts in rels:
            code = ts.startswith('fn_') or (ts.startswith('?') and not ts.startswith('??_7') and not ts.startswith('??_R') and not ts.startswith('??_C'))
            if not code:
                if len(cur)>=3: runs.append(cur)
                cur=[]; prev=None; continue
            if prev is not None and off-prev!=4:
                if len(cur)>=3: runs.append(cur)
                cur=[]
            cur.append(ts); prev=off
        if len(cur)>=3: runs.append(cur)
    return runs

def main():
    runs=extract_runs(RDATA_OBJ)
    print(f'[runs] {len(runs)} pointer runs in rdata obj (>=3 slots)',file=sys.stderr)
    # index: symbol -> list of (run_id, pos)
    sym_pos={}
    for ri,run in enumerate(runs):
        for p,s in enumerate(run):
            sym_pos.setdefault(s,[]).append((ri,p))

    o=json.load(open(os.path.join(PROJ,'objdiff.json')))
    mp=json.load(open(os.path.join(PROJ,'scripts/target_symbol_map.json')))
    mapvals=set(v for k,v in mp.items() if isinstance(v,str))
    defined=set()
    for bp in glob.glob(os.path.join(PROJ,'build/45410914/src/**/*.obj'),recursive=True):
        try: d,syms,secs,sbi=read_coff(bp)
        except: continue
        for s in syms:
            if s['section']>0 and s['name'].startswith('?'): defined.add(s['name'])
    print(f'[defined] {len(defined)}',file=sys.stderr)

    # collect base vtables (dedup by vtable name+content)
    seen_vt=set()
    base_vts=[]
    for u in o['units']:
        bp=u.get('base_path')
        if not bp or not os.path.exists(bp): continue
        try: d,syms,secs,sbi=read_coff(bp)
        except: continue
        for vtn,slots in vtable_slots(d,syms,secs,sbi).items():
            bnames=[nm for off,nm in sorted(slots) if nm.startswith('?') and not nm.startswith('??_7')]
            if len(bnames)<3: continue
            key=(vtn,tuple(bnames))
            if key in seen_vt: continue
            seen_vt.add(key)
            base_vts.append((vtn,bnames,u['name']))
    print(f'[base vtables] {len(base_vts)} distinct',file=sys.stderr)

    candidates={}; evidence=[]
    matched_vt=0; ambig_vt=0
    for vtn,bnames,un in base_vts:
        anchors={i:nm for i,nm in enumerate(bnames) if nm in mapvals}
        if len(anchors)<2: continue
        first_i=min(anchors)
        # candidate (run,align) from first anchor's positions
        cand_aligns=set()
        for ri,p in sym_pos.get(anchors[first_i],[]):
            cand_aligns.add((ri,p-first_i))
        good=[]
        for ri,ao in cand_aligns:
            run=runs[ri]; ok=0; bad=0
            # anchor agreement
            for i,nm in anchors.items():
                p=i+ao
                if 0<=p<len(run) and run[p]==nm: ok+=1
                else: bad+=1
            if bad!=0 or ok<2: continue
            # FULL-run consistency: every aligned slot that is already mangled
            # in the target run must equal the base method name at that slot.
            full_bad=0
            for i,nm in enumerate(bnames):
                p=i+ao
                if not (0<=p<len(run)): continue
                rs=run[p]
                if rs.startswith('?') and rs!=nm:
                    full_bad+=1
            if full_bad: continue
            good.append((ri,ao,ok))
        if not good: continue
        # tiebreak: unique max-anchor run wins; else ambiguous
        maxok=max(g[2] for g in good)
        top=[g for g in good if g[2]==maxok]
        if len(top)>1:
            ambig_vt+=1; continue
        ri,ao,ok=top[0]; run=runs[ri]
        matched_vt+=1
        for i,nm in enumerate(bnames):
            p=i+ao
            if not (0<=p<len(run)): continue
            m=FN.match(run[p])
            if not m: continue
            if nm not in defined: continue
            va='0x'+m.group(1).lower()
            if va in candidates and candidates[va]!=nm:
                candidates[va]='__CONFLICT__'
            elif candidates.get(va)!='__CONFLICT__':
                candidates[va]=nm
                evidence.append((va,nm,vtn,i,un,ok))
    frag={va:nm for va,nm in candidates.items() if nm!='__CONFLICT__'}
    print(f'matched_vt={matched_vt} ambig_vt={ambig_vt} names={len(frag)}',file=sys.stderr)
    json.dump(frag,open('/home/free/tmp/namewave/global_frag.json','w'),indent=1)
    with open('/home/free/tmp/namewave/global_evidence.txt','w') as f:
        for va,nm,vtn,i,un,ok in sorted(evidence):
            if frag.get(va)==nm:
                f.write(f'{va}\t{nm}\tvtable={vtn}\tslot={i}\tunit={un}\tanchors={ok}\n')
    print('wrote global_frag.json + global_evidence.txt',file=sys.stderr)

main()
