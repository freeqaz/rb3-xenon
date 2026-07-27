#!/usr/bin/env python3
"""thunk_identity_namer -- name retail adjustor thunks BY CONSTRUCTION.

An MSVC adjustor thunk is three instructions and nothing else:

    [lwz r11,-V(rX) ; subf rX,r11,rX]   vtordisp fetch  (optional)
    [addi rX,rX,-A]                     this-adjust     (optional)
    b   CALLEE

Its mangled name is a TOTAL FUNCTION of (callee's qualified-name prefix, V, A):
MSVC writes `<prefix>W<A>` when there is no vtordisp and `<prefix>$4<V><A>` when
there is, with V/A in the standard mangled-number encoding.  So once the callee
is named, the thunk's own name is not guessed -- it is computed, and then
confirmed against the unique symbol in our obj carrying that exact encoding.

Two traps this tool exists to avoid (both produced false "class not ported"
verdicts in earlier passes):

  * MSVC names the deleting-destructor BODY `??_G<C>` but names every adjustor
    thunk of it `??_E<C>`.  Any scope comparison that does not fold _G/_E finds
    "no thunk for ??_G<C>@@" for EVERY polymorphic class and calls it missing.
  * A plain tail call (`mr r4,r3; li r3,16; b PoolFree`) is small and ends in an
    unconditional `b`, but it is not a thunk.  Shape must be checked, not size.

Read-only.  Emits a proposal list; nothing is applied here.
"""
import argparse
import sys,json,struct,re,collections
from pathlib import Path
_ap=argparse.ArgumentParser()
_ap.add_argument("--emit",default="/home/free/tmp/missvirt/thunkname.json")
ARGS=_ap.parse_args()
ROOT=Path(__file__).resolve().parents[2]
sys.path.insert(0,str(ROOT/"scripts"/"harvest"))
from size_order_automap import _ordered_funcs,_asm_target_funcs
BUILD=ROOT/"build"/"45410914"; SPLITS=ROOT/"config"/"45410914"/"splits.txt"
IMAGE=ROOT/"orig"/"45410914"/"band.exe"
raw=json.load(open(ROOT/"scripts"/"target_symbol_map.json"))
m={int(k,16):v for k,v in raw.items() if k.lower().startswith("0x") and isinstance(v,str)}
arb={int(x,16) for x in raw.get("_bijection_arbitrary",[])}|{int(x,16) for x in raw.get("_icf_arbitrary",[])}
byname=collections.defaultdict(list)
for k,v in m.items(): byname[v].append(k)
d=open(IMAGE,'rb').read()
pe=struct.unpack_from("<I",d,0x3C)[0]; nsec=struct.unpack_from("<H",d,pe+6)[0]
oh=struct.unpack_from("<H",d,pe+20)[0]; ib=struct.unpack_from("<I",d,pe+24+28)[0]
secs=[]
for i in range(nsec):
    o=pe+24+oh+i*40; vs,va,rs,ro=struct.unpack_from("<IIII",d,o+8); secs.append((ib+va,vs,ro))
def word(va):
    for sva,vs,ro in secs:
        if sva<=va<sva+vs: return struct.unpack_from(">I",d,ro+(va-sva))[0]
def shape(va,sz):
    if not sz or sz>0x20: return None
    ws=[word(va+4*i) for i in range(sz//4)]
    if any(w is None for w in ws): return None
    while ws and ws[-1]==0: ws.pop()
    if not ws: return None
    idx=0; vt=None; adj=0; reg=None
    if len(ws)>=3 and (ws[0]>>26)==32 and ((ws[0]>>21)&31)==11:
        rA=(ws[0]>>16)&31; imm=ws[0]&0xFFFF
        if imm>=0x8000: imm-=0x10000
        if (ws[1]>>26)==31 and ((ws[1]>>1)&0x3FF)==40 and ((ws[1]>>21)&31)==rA and ((ws[1]>>16)&31)==11 and ((ws[1]>>11)&31)==rA:
            vt=imm; reg=rA; idx=2
        else: return None
    if idx<len(ws) and (ws[idx]>>26)==14:
        rD=(ws[idx]>>21)&31; rA=(ws[idx]>>16)&31; imm=ws[idx]&0xFFFF
        if imm>=0x8000: imm-=0x10000
        if rD==rA and imm<0 and (reg is None or rD==reg): adj=imm; reg=rD; idx+=1
        elif vt is None: return None
    if idx!=len(ws)-1: return None
    b=ws[idx]
    if b>>26!=18 or (b&1) or ((b>>1)&1): return None
    li=b&0x03FFFFFC
    if li&0x02000000: li-=0x04000000
    if vt is None and adj==0: return None
    if reg not in (3,4): return None
    return (vt,-adj,va+4*idx+li)
def mnum(s,i):
    if i>=len(s): return None,i
    if s[i]=='?':
        v,j=mnum(s,i+1); return (None if v is None else -v),j
    if s[i].isdigit(): return int(s[i])+1,i+1
    j=i;v=0
    while j<len(s) and 'A'<=s[j]<='P': v=v*16+(ord(s[j])-65); j+=1
    if j==i: return None,i
    if j<len(s) and s[j]=='@': j+=1
    if v>=0x80000000: v-=0x100000000
    return v,j
def td(n):
    mm=re.search(r"@@W",n)
    if mm:
        v,_=mnum(n,mm.end())
        return (None,v) if v is not None else None
    mm=re.search(r"@@\$4",n)
    if mm:
        vt,j=mnum(n,mm.end()); ad,_=mnum(n,j)
        return (vt,ad) if vt is not None and ad is not None else None
    return None
def load_units():
    u=collections.defaultdict(list); cur=None
    for line in open(SPLITS):
        if not line.strip() or line.startswith("Sections:"): continue
        if not line[0].isspace(): cur=line.strip().rstrip(":"); continue
        p=line.split()
        if len(p)>=3 and p[0]==".text" and p[1].startswith("start:"):
            u[cur].append((int(p[1].split(":")[1],16),int(p[2].split(":")[1],16)))
    return u
def paths(u):
    rel=u[:-4] if u.endswith(".cpp") else u
    a=BUILD/"asm"/(rel+".s"); b=BUILD/"src"/(rel+".obj")
    if not b.exists():
        c=list((BUILD/"src").rglob(Path(rel).name+".obj")); b=c[0] if len(c)==1 else b
    return a,b
def prefix(n):
    """qualified-name prefix shared by a virtual and its thunks, template-safe."""
    mm=re.search(r"@@(W[0-9A-P?]|\$4)",n)
    if mm: return n[:mm.start()+2]
    mm=re.search(r"@@[QAEIMUBV][A-Z]",n)
    if mm: return n[:mm.start()+2]
    return None
def norm(p):
    """??_G / ??_E equivalence for deleting dtors"""
    return "??_D*"+p[4:] if p.startswith(("??_G","??_E")) else p

props=[]; st=collections.Counter()
for u in sorted(load_units()):
    asm,bobj=paths(u)
    if not (asm.exists() and bobj.exists()): continue
    try:
        tf=[(va,sz,mk) for va,sz,mk in _asm_target_funcs(asm) if va]
        bf=_ordered_funcs(bobj)
    except Exception: continue
    tmask={va:mk for va,sz,mk in tf}
    idx=collections.defaultdict(list)     # (normprefix,(vt,adj)) -> [symbol]
    for f in bf:
        x=td(f['name'])
        if not x: continue
        p=prefix(f['name'])
        if p: idx[(norm(p),x)].append(f)
    for va,sz,mk in tf:
        s=shape(va,sz)
        if not s: continue
        st['thunks']+=1
        cn=m.get(s[2])
        if not cn: st['callee_unnamed']+=1; continue
        p=prefix(cn)
        if not p: st['callee_noprefix']+=1; continue
        cands=idx.get((norm(p),(s[0],s[1])),[])
        if len(cands)!=1: st['no_unique_sym_%d'%min(len(cands),2)]+=1; continue
        sym=cands[0]; cur=m.get(va)
        if cur==sym['name']: st['already_correct']+=1; continue
        bok = tmask.get(va)==sym['masked']
        st['DISAGREE_bytesok' if bok else 'DISAGREE_bytesdiff']+=1
        props.append(dict(va="0x%08x"%va, unit=u, cur=cur, new=sym['name'],
                          bytes_ok=bok, callee=cn, vt=s[0], adj=s[1],
                          callee_arb=(s[2] in arb), free=(sym['name'] not in byname)))
print(dict(st))
ok=[p for p in props if p['bytes_ok']]
print("byte-verified disagreements:",len(ok))
print("  of those, name currently FREE:",sum(1 for p in ok if p['free']))
print("  of those, VA currently UNMAPPED:",sum(1 for p in ok if p['cur'] is None))
print("  UNMAPPED *and* name FREE (pure +1 candidates):",sum(1 for p in ok if p['cur'] is None and p['free']))
json.dump(props,open(ARGS.emit,"w"),indent=1)
for p in [q for q in ok if q['cur'] is None and q['free']][:25]:
    print(f"  {p['va']} {p['unit'][:32]:32s} -> {p['new'][:70]}")
