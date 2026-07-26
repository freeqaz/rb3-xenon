#!/usr/bin/env python3
"""Unit-scoped NEAR-twin root-cause census over every unmapped sub-100%
anonymous target symbol in a pinned unit.

For each such symbol, find the same-unit base symbol of identical masked length
with the fewest differing 4-byte words, and when exactly one word differs,
decode it into a root cause:

  FRAME_OFFSET       word at +0x00, addi rX, r12, imm on both sides
                     -> the funclet's r12-relative reference into the PARENT's
                        stack frame; parent frame layout differs
  MEMBER_OFFSET      addi vs addi   -> cleanup addresses a different member
  MEMBER_PTR_OFFSET  lwz  vs lwz    -> different pointer-member offset
  MEMBER_FORM        addi vs lwz    -> pointer member vs embedded member
                        (a declared-TYPE difference, not just an offset)

Requires a FULL build in the worktree first: setup_worktree.sh reflinks main's
dirty build dir, and scanning pre-build reads other lanes' uncommitted objs.
"""
import sys,os,json,glob,collections,struct
sys.path.insert(0,os.path.join(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0,'/home/free/tmp/laneAM')
from coffx import read_coff, infer_sizes, funclet_signature, K_SEC
WT=sys.argv[1]
def load(path):
    try: data=open(path,'rb').read()
    except OSError: return None
    secs,syms=read_coff(data)
    if secs is None: return None
    infer_sizes(secs,syms); out=[]
    for s in syms:
        if s.sec<=0 or s.size==0 or s.kind==K_SEC or s.cls not in (2,3): continue
        sec=secs[s.sec-1]
        if not sec.is_code: continue
        g=funclet_signature(sec,s)
        if g is None: continue
        lo,hi=s.value,s.value+s.size; rend=0
        for (va,si,typ) in sec.relocs:
            if lo<=va<hi: rend=max(rend,va-lo+4)
        end=len(g)
        while end>=4 and g[end-4:end]==b'\0\0\0\0' and rend<=end-4: end-=4
        if end: out.append((g[:end], sec.data[s.value:s.value+s.size], s.name, s.size))
    return out
def opk(w): return {14:'addi',32:'lwz',36:'stw',15:'addis'}.get(w>>26,'op%d'%(w>>26))
tmap={k.lower():v for k,v in json.load(open(f'{WT}/scripts/target_symbol_map.json')).items() if isinstance(v,str) and k.startswith('0x')}
rep=json.load(open(f'{WT}/build/45410914/report.json'))
pct={f['name']:f['match_percent_normalized'] for u in rep['units'] for f in (u.get('functions') or [])}
BASE=collections.defaultdict(list)
for p in glob.glob(f'{WT}/build/45410914/src/**/*.obj',recursive=True): BASE[os.path.basename(p)].append(p)
st=collections.Counter(); rows=[]
root=f'{WT}/build/45410914/obj'
for tp in sorted(glob.glob(f'{root}/**/*.obj',recursive=True)):
    rel=os.path.relpath(tp,root)
    bp=os.path.join(f'{WT}/build/45410914/src',rel)
    if not os.path.exists(bp):
        c=BASE.get(os.path.basename(tp))
        if not c or len(c)!=1: continue
        bp=c[0]
    ts=load(tp); bs=load(bp)
    if not ts or not bs: continue
    bysz=collections.defaultdict(list)
    for g,raw,nm,sz in bs: bysz[len(g)].append((g,raw,nm))
    for g,raw,nm,sz in ts:
        if not nm.startswith('fn_') or tmap.get('0x'+nm[3:].lower()): continue
        p=pct.get(nm)
        if p is None or p>=100.0: continue
        c=bysz.get(len(g))
        if not c: st['no_same_size']+=1; continue
        best=None
        for bg,braw,bn in c:
            d=[i for i in range(0,len(g),4) if g[i:i+4]!=bg[i:i+4]]
            if best is None or len(d)<len(best[0]): best=(d,bn,braw)
            if not d: break
        d,bn,braw=best
        if not d: st['exact_but_no_free_name']+=1; continue
        if len(d)==1:
            i=d[0]; tv=struct.unpack('>I',raw[i:i+4])[0]; bv=struct.unpack('>I',braw[i:i+4])[0]
            tk,bk=opk(tv),opk(bv)
            cause=('FRAME_OFFSET' if i==0 and tk==bk=='addi' else 'MEMBER_OFFSET' if tk==bk=='addi'
                   else 'MEMBER_PTR_OFFSET' if tk==bk=='lwz' else 'MEMBER_FORM' if {tk,bk}=={'addi','lwz'} else 'OTHER_1WORD')
            st['1W_'+cause]+=1
            rows.append(dict(sym=nm,unit=rel,size=sz,pct=p,cause=cause,base=bn,at=i,
                             tgt='0x%08x'%tv,bas='0x%08x'%bv))
        elif len(d)<=3: st['DIFF_%dW'%len(d)]+=1
        elif len(d)<=8: st['DIFF_4_8W']+=1
        else: st['DIFF_MANY']+=1
print(json.dumps(st,indent=1))
json.dump(rows,open(sys.argv[2] if len(sys.argv)>2 else '/home/free/tmp/laneAT/neartwin_global.json','w'),indent=1)
c=collections.Counter((r['unit'],r['cause']) for r in rows)
print('\ntop 1-word clusters:')
for k,v in c.most_common(25): print(f'  {v:4d}  {k[0]}  {k[1]}')
