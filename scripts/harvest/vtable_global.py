#!/usr/bin/env python3
"""Global vtable alignment (gated).

The entire retail .rdata (all vtable pointer arrays) lives in one dtk target
obj: build/45410914/obj/auto_00_82000400_rdata.obj. Its relocations, sorted by
offset, form runs = the retail vtables (each pointer -> fn_<VA> anon, or a
mangled name if that VA is already mapped).

Base side: every ??_7Class@@6B@ COMDAT across base objs gives an ordered list
of method mangled names.

Align each base vtable to a target run by ALREADY-MAPPED anchor names at a
constant slot offset (>=2 anchors, 0 disagreements). Then anon target slots
inherit the base method name -> new identification.

4-part output gate (2026-07-18, from the reattrib audit of the 110-entry
naming wave — see /home/free/tmp/reattrib_patches/NOTES.md):

1. OWNING-UNIT ROUTER. Each candidate VA is resolved against the pinned
   `.text` spans in config/45410914/splits.txt. A candidate goes to the LIVE
   fragment only if it is *pairable*: the VA's owning pinned unit's base obj
   actually defines the proposed symbol (this covers both the same-unit case
   and the cross-unit case where the true owner compiles the method — the
   OWNED_OTHER class the audit found can pair correctly once named).
   Everything else (unpinned VA, or owner lacking the symbol) routes to
   global_frag_unpinned.json — a REVIEW bucket that must NOT be auto-seeded
   into the live map / correlator.

2. PURECALL GUARD. Reject any VA whose body is the MSVC _purecall handler
   (R6025 shape: `li r3,0x19; bl _NMSG_WRITE; ...; bl abort`). Pure slots of
   abstract classes all point at this shared thunk; positional naming would
   otherwise mislabel it (the SecBetweenUploads@TourProgress class).

3. RETURN-SHAPE SANITY. Parse the mangled name's return token and compare
   against a capstone disasm of the VA. ONLY empirically-validated
   contradiction rules are hard rejects (validated against the current
   17,302-entry map, 2026-07-18):
     - float/double return + body exactly `li r3,imm; blr`  (0/84 mapped
       float no-arg getters have this shape -> clear contradiction)
     - void return + body exactly `li r3,imm; blr`          (0/4051 mapped
       void fns have this shape -> dead-code contradiction)
   Non-void + bare `blr` is only a REVIEW FLAG (3/4516 mapped legit retail
   breadcrumb stubs have it). NOTE: the audit's proposed "byte return =>
   `li r3,N; blr` leaf" rule is FALSE for this codebase — all 19 mapped
   StaticByteCode@...@@SAEXZ bodies are non-leaf String-building
   registration routines (String ctor + helper call), byte-identical in
   shape to our compiled ?ByteCode@X@@UBAEXZ COMDATs. Do not add a
   byte-leaf rule; it would reject correct entries.

4. The gate runs against the CURRENT scripts/target_symbol_map.json: VAs
   already mapped, and names already placed at another VA, are skipped.

Usage: vtable_global.py [PROJ] [OUTDIR]
  PROJ   = repo/worktree root (default: cwd)
  OUTDIR = output dir (default: ~/tmp/vtgate_out)
Outputs: OUTDIR/global_frag.json          (live, pairable — safe to insert)
         OUTDIR/global_frag_unpinned.json (review bucket, do NOT auto-seed)
         OUTDIR/global_rejects.json       (purecall / return-shape rejects)
         OUTDIR/global_evidence.txt
"""
import json, os, re, struct, sys, glob

PROJ = os.path.abspath(sys.argv[1]) if len(sys.argv) > 1 else os.getcwd()
OUTDIR = os.path.abspath(sys.argv[2]) if len(sys.argv) > 2 else os.path.expanduser('~/tmp/vtgate_out')
RDATA_OBJ = os.path.join(PROJ,'build/45410914/obj/auto_00_82000400_rdata.obj')
FN=re.compile(r'^fn_([0-9a-fA-F]{8})')

# ---------------------------------------------------------------- COFF utils
def read_coff(path):
    data = open(path,'rb').read()
    machine,nsec,ts,symoff,nsym,opt,flags = struct.unpack_from('<HHIIIHH',data,0)
    stroff = symoff + nsym*18
    strsz = struct.unpack_from('<I',data,stroff)[0]
    strtab = data[stroff:stroff+strsz]
    def name_at(off):
        if data[off:off+4]==b'\x00\x00\x00\x00':
            so=struct.unpack_from('<I',data,off+4)[0]
            e=strtab.index(b'\x00',so); return strtab[so:e].decode('ascii','replace')
        return data[off:off+8].rstrip(b'\x00').decode('ascii','replace')
    syms=[]; i=0
    while i<nsym:
        so=symoff+i*18
        nm=name_at(so)
        val,sec,typ,stor,aux=struct.unpack_from('<IhHBB',data,so+8)
        syms.append(dict(index=i,name=nm,value=val,section=sec,storage=stor))
        i+=1+aux
    secoff=20+opt
    secs=[]
    for s in range(nsec):
        h=secoff+s*40
        raw=data[h:h+8].rstrip(b'\x00')
        if raw.startswith(b'/'):
            o=int(raw[1:]); e=strtab.index(b'\x00',o); snm=strtab[o:e].decode('ascii','replace')
        else: snm=raw.decode('ascii','replace')
        vs,va,rsz,roff,reloff,lnoff,nrel,nln,ch=struct.unpack_from('<IIIIIIHHI',data,h+8)
        secs.append(dict(name=snm,reloff=reloff,nrel=nrel))
    symbyidx={s['index']:s for s in syms}
    return data,syms,secs,symbyidx

def vtable_slots(data,syms,secs,symbyidx):
    """Return {vtname: [(offset,slot_symbol_name),...]} for each ??_7...@@6B vtable."""
    out={}
    for sym in syms:
        if sym['name'].startswith('??_7') and '6B' in sym['name'] and sym['section']>0:
            sec=secs[sym['section']-1]
            base=sym['value']
            slots=[]
            for r in range(sec['nrel']):
                ro=sec['reloff']+r*10
                rva,sidx,rtype=struct.unpack_from('<IIH',data,ro)
                ts=symbyidx.get(sidx,{}).get('name','')
                slots.append((rva,ts))
            slots=[(o-base,t) for (o,t) in slots if o>=base]
            slots.sort()
            out[sym['name']]=slots
    return out

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

# ---------------------------------------------------------------- gate
class Gate:
    """Owning-unit router + purecall guard + return-shape sanity (see module doc)."""
    def __init__(self, proj):
        self.proj=proj
        # 1) pinned .text spans from splits.txt -> (start,end,unit)
        self.spans=[]
        unit=None
        for ln in open(os.path.join(proj,'config/45410914/splits.txt')):
            m=re.match(r'^(\S.*?):\s*$',ln)
            if m: unit=m.group(1); continue
            m=re.match(r'^\s+\.text\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)',ln)
            if m and unit and unit!='Sections':
                self.spans.append((int(m.group(1),16),int(m.group(2),16),unit))
        self.spans.sort()
        # 2) PE image (decompressed basefile) for byte reads at VA
        d=open(os.path.join(proj,'orig/45410914/band.exe'),'rb').read()
        pe=struct.unpack_from('<I',d,0x3c)[0]
        nsec=struct.unpack_from('<H',d,pe+6)[0]
        optsz=struct.unpack_from('<H',d,pe+20)[0]
        secoff=pe+24+optsz
        imgbase=struct.unpack_from('<I',d,pe+24+28)[0]
        self.img=d; self.imgsecs=[]
        for s in range(nsec):
            h=secoff+s*40
            vs,va,rsz,ro=struct.unpack_from('<IIII',d,h+8)
            self.imgsecs.append((imgbase+va,vs,ro))
        import capstone
        self.md=capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_32|capstone.CS_MODE_BIG_ENDIAN)
        self._unit_defined={}   # normalized unit -> set(defined syms) (lazy)
        self._unit_basepath={}  # normalized unit -> base obj path

    @staticmethod
    def norm_unit(u):
        """'default/band3/game/TrackerDisplay' or 'band3/game/TrackerDisplay.cpp' -> canonical."""
        if u.startswith('default/'): u=u[len('default/'):]
        u=re.sub(r'\.(cpp|cc|c)$','',u)
        return u

    def index_units(self, objdiff_units):
        for u in objdiff_units:
            bp=u.get('base_path')
            if not bp: continue
            if not os.path.isabs(bp): bp=os.path.join(self.proj,bp)
            self._unit_basepath[self.norm_unit(u['name'])]=bp

    def owning_unit(self, va):
        import bisect
        i=bisect.bisect_right(self.spans,(va,0xffffffff,''))-1
        if i>=0:
            s,e,u=self.spans[i]
            if s<=va<e: return self.norm_unit(u)
        return None

    def unit_defines(self, nunit, sym):
        if nunit not in self._unit_defined:
            defined=set()
            bp=self._unit_basepath.get(nunit)
            if bp and os.path.exists(bp):
                try:
                    d,syms,secs,sbi=read_coff(bp)
                    defined={s['name'] for s in syms if s['section']>0 and s['name'].startswith('?')}
                except Exception: pass
            self._unit_defined[nunit]=defined
        return sym in self._unit_defined[nunit]

    def _read(self, va, n):
        for base,vs,ro in self.imgsecs:
            if base<=va<base+vs:
                return self.img[ro+(va-base):ro+(va-base)+n]
        return None

    def body(self, va, max_bytes=0x80):
        """Instructions up to and including the first blr (capped)."""
        b=self._read(va,max_bytes)
        if not b: return None
        out=[]
        for i in self.md.disasm(b,va):
            out.append(i)
            if i.mnemonic=='blr': break
        return out

    def is_purecall(self, va):
        """R6025 shape: `li r3, 0x19` immediately followed by bl, in a small multi-call body."""
        body=self.body(va,0x80)
        if not body or len(body)>26: return False
        ops=[(i.mnemonic,i.op_str.replace(' ','')) for i in body]
        if sum(1 for m,_ in ops if m=='bl')<2: return False
        for k in range(len(ops)-1):
            if ops[k][0]=='li' and ops[k][1] in ('r3,0x19','r3,25') and ops[k+1][0]=='bl':
                return True
        return False

    # mangled-name return-token parser (simple non-template instance/static methods only)
    _RXI=re.compile(r'^\?[^?@]+@(?:[^?@]+@)*@([UEMQAIB])([ABCD])A(_.|\?|.)')
    _RXS=re.compile(r'^\?[^?@]+@(?:[^?@]+@)*@([STCDKL])A(_.|\?|.)')
    @classmethod
    def ret_token(cls, nm):
        if '?$' in nm: return None
        m=cls._RXS.match(nm)
        if m: return m.group(2)
        m=cls._RXI.match(nm)
        if m: return m.group(3)
        return None

    def ret_shape(self, va, nm):
        """Return (verdict, reason): verdict in ('ok','reject','flag')."""
        tok=self.ret_token(nm)
        if tok is None: return ('ok','unparsed')
        body=self.body(va,0x10)
        if not body: return ('ok','no-bytes')
        bare_blr = body[0].mnemonic=='blr'
        li_blr = (len(body)>=2 and body[0].mnemonic=='li'
                  and body[0].op_str.split(',')[0].strip()=='r3' and body[1].mnemonic=='blr')
        if tok in ('M','N') and li_blr:
            return ('reject','float-return but body is li r3,imm; blr')
        if tok=='X' and li_blr:
            return ('reject','void return but body is li r3,imm; blr')
        if tok!='X' and bare_blr:
            return ('flag','non-void return but body is bare blr')
        return ('ok','')

def gate_candidates(gate, candidates, evidence):
    """Apply the 4-part gate. Returns (live, review, rejects, stats).
    evidence[va] is a dict; gets owner/route/flags folded in for live entries."""
    live={}; review={}; rejects={}
    stats=dict(same_unit=0,cross_unit_pairable=0,cross_unit_review=0,unpinned=0,
               purecall=0,ret_reject=0,ret_flag=0)
    for va,nm in sorted(candidates.items()):
        if nm in ('__CONFLICT__','__C__'): continue
        iva=int(va,16); ev=evidence[va]
        if gate.is_purecall(iva):
            rejects[va]=dict(name=nm,reason='purecall_thunk',**ev); stats['purecall']+=1; continue
        verdict,reason=gate.ret_shape(iva,nm)
        if verdict=='reject':
            rejects[va]=dict(name=nm,reason='ret-shape: '+reason,**ev); stats['ret_reject']+=1; continue
        flags=[reason] if verdict=='flag' else []
        if flags: stats['ret_flag']+=1
        owner=gate.owning_unit(iva)
        evunit=gate.norm_unit(ev['unit'])
        if owner is not None and owner==evunit:
            live[va]=nm; evidence[va]=dict(ev,owner=owner,route='same_unit',flags=flags)
            stats['same_unit']+=1
        elif owner is not None and gate.unit_defines(owner,nm):
            # cross-unit but the true owning unit compiles this method -> pairable
            live[va]=nm; evidence[va]=dict(ev,owner=owner,route='cross_unit_pairable',flags=flags)
            stats['cross_unit_pairable']+=1
        else:
            route='cross_unit_review' if owner is not None else 'unpinned'
            stats[route]+=1
            review[va]=dict(name=nm,owner=owner,route=route,flags=flags,**ev)
    return live,review,rejects,stats

def write_outputs(outdir, prefix, live, review, rejects, evidence, stats, header):
    os.makedirs(outdir,exist_ok=True)
    json.dump(live,open(os.path.join(outdir,f'{prefix}_frag.json'),'w'),indent=1)
    json.dump(review,open(os.path.join(outdir,f'{prefix}_frag_unpinned.json'),'w'),indent=1)
    json.dump(rejects,open(os.path.join(outdir,f'{prefix}_rejects.json'),'w'),indent=1)
    with open(os.path.join(outdir,f'{prefix}_evidence.txt'),'w') as f:
        for va in sorted(live):
            e=evidence[va]
            f.write(f"{va}\t{live[va]}\tvtable={e['vtable']}\tslot={e['slot']}\tunit={e['unit']}\t"
                    f"owner={e['owner']}\troute={e['route']}\tanchors={e.get('anchors','?')}\tflags={e['flags']}\n")
    print(f'{header} live={len(live)} review={len(review)} rejects={len(rejects)} stats={stats}',file=sys.stderr)
    print(f'wrote {outdir}/{prefix}_frag.json (+_frag_unpinned, +_rejects, +_evidence)',file=sys.stderr)

# ---------------------------------------------------------------- main
def main():
    runs=extract_runs(RDATA_OBJ)
    print(f'[runs] {len(runs)} pointer runs in rdata obj (>=3 slots)',file=sys.stderr)
    sym_pos={}
    for ri,run in enumerate(runs):
        for p,s in enumerate(run):
            sym_pos.setdefault(s,[]).append((ri,p))

    o=json.load(open(os.path.join(PROJ,'objdiff.json')))
    mp=json.load(open(os.path.join(PROJ,'scripts/target_symbol_map.json')))
    mapvals=set(v for k,v in mp.items() if isinstance(v,str))
    mapkeys=set(k.lower() for k in mp)
    defined=set()
    for bp in glob.glob(os.path.join(PROJ,'build/45410914/src/**/*.obj'),recursive=True):
        try: d,syms,secs,sbi=read_coff(bp)
        except: continue
        for s in syms:
            if s['section']>0 and s['name'].startswith('?'): defined.add(s['name'])
    print(f'[defined] {len(defined)}',file=sys.stderr)

    gate=Gate(PROJ)
    gate.index_units(o['units'])

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

    candidates={}; evidence={}
    matched_vt=0; ambig_vt=0
    for vtn,bnames,un in base_vts:
        anchors={i:nm for i,nm in enumerate(bnames) if nm in mapvals}
        if len(anchors)<2: continue
        first_i=min(anchors)
        cand_aligns=set()
        for ri,p in sym_pos.get(anchors[first_i],[]):
            cand_aligns.add((ri,p-first_i))
        good=[]
        for ri,ao in cand_aligns:
            run=runs[ri]; ok=0; bad=0
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
            if nm in mapvals: continue           # name already located at another VA
            va='0x'+m.group(1).lower()
            if va in mapkeys: continue           # VA already mapped
            if va in candidates and candidates[va]!=nm:
                candidates[va]='__CONFLICT__'
            elif candidates.get(va)!='__CONFLICT__':
                candidates[va]=nm
                evidence[va]=dict(vtable=vtn,slot=i,unit=un,anchors=ok)
    print(f'matched_vt={matched_vt} ambig_vt={ambig_vt} candidates={len(candidates)}',file=sys.stderr)

    live,review,rejects,stats=gate_candidates(gate,candidates,evidence)
    write_outputs(OUTDIR,'global',live,review,rejects,evidence,stats,'[gate]')

if __name__=='__main__':
    main()
