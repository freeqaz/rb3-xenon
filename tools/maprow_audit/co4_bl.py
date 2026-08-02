#!/usr/bin/env python3
"""CH3: CALL-COUNT channel -- which SIDE inlined?

Counts `bl` (opcode 18, LK=1) in the retail body at A vs our compiled body.
  retail_bl >  our_bl  -> retail CALLS where we INLINE  (we are bigger: our
                          source/headers inline something retail did not)
  retail_bl <  our_bl  -> retail INLINED where we call
This is generated from .text immediates only -- it shares nothing with the
RTTI/vtable channel except the address A.

Also emits the final per-row table combining .pdata, slot consensus, prefix.
"""
import json, os, struct, sys
from collections import Counter
ROOT="/home/free/tmp/laneCO4/wt"
for p in ("tools","tools/maprow_dtor","tools/extent_census"): sys.path.insert(0,os.path.join(ROOT,p))
import coffx
from retail_reader import Image
img=Image(os.path.join(ROOT,"orig/45410914/band.exe"))
cfg=json.load(open(os.path.join(ROOT,"objdiff.json")))
paths={u["name"]:(u.get("target_path"),u.get("base_path")) for u in cfg["units"]}
_c={}
def load(p):
    if p not in _c: _c[p]=coffx.load(os.path.join(ROOT,p))
    return _c[p]
def our_body(bp,nm,size):
    L=load(bp)
    if not L: return None
    secs,syms=L; bysec={s.index:s for s in secs}
    c=[s for s in syms if s.name==nm and s.sec>0 and s.kind==coffx.K_FUNCTION and bysec.get(s.sec) and bysec[s.sec].code]
    if not c: return None
    sy=min(c,key=lambda s:abs(s.size-size)); sc=bysec[sy.sec]
    return sc.data[sy.addr-sc.addr:sy.addr-sc.addr+sy.size]
def nbl(b):
    if not b: return None
    n=0
    for i in range(0,len(b)-3,4):
        w=struct.unpack_from(">I",b,i)[0]
        if (w>>26)==18 and (w&1): n+=1
    return n
rows=json.load(open(os.path.expanduser("~/tmp/laneCO4/adj3.json")))
out=[]
for r in rows:
    rec=dict(r)
    if r.get("addr"):
        A=int(r["addr"],16); sz=r.get("pdata_size") or r["tgt"]
        rec["bl_retail"]=nbl(img.body(A,sz))
        rec["bl_ours"]=nbl(our_body(paths[r["unit"]][1],r["name"],r["base"]))
    out.append(rec)
json.dump(out,open(os.path.expanduser("~/tmp/laneCO4/final.json"),"w"),indent=1)

def verdict(r):
    if not r.get("addr"): return "UNADJ:no-address-in-home-pin"
    if r.get("slot")=="CONFIRM": return "SOURCE:name confirmed by vtable slot"
    if (r.get("pfx") or 0)>=8 and (r.get("pfx_unmasked") or 0)>=4: return "SOURCE:prefix>>null"
    br,bo=r.get("bl_retail"),r.get("bl_ours")
    if br is not None and bo is not None and br>bo: return "SOURCE:retail calls, we inline"
    if br is not None and bo is not None and br<bo: return "SOURCE:retail inlines, we call"
    return "UNADJ:no-channel-fired"
for r in out: r["verdict"]=verdict(r)
json.dump(out,open(os.path.expanduser("~/tmp/laneCO4/final.json"),"w"),indent=1)
print("VERDICTS:",Counter(r["verdict"] for r in out))
print()
print("bl direction (rows with both counts):",
      Counter("retail>ours" if r.get("bl_retail",0)>r.get("bl_ours",0) else
              ("retail<ours" if (r.get("bl_retail") is not None and r.get("bl_ours") is not None and r["bl_retail"]<r["bl_ours"]) else "eq/na")
              for r in out))
print()
hdr=f"{'verdict':40s} {'slot':12s} {'tgt':>6s} {'ours':>6s} {'pdat':>6s} {'blR':>4s} {'blO':>4s} {'pfx':>4s}  name"
print(hdr)
for r in sorted(out,key=lambda z:(z["verdict"],-(z["base"]-z["tgt"]))):
    print(f"{r['verdict']:40s} {str(r.get('slot')):12s} {r['tgt']:6d} {r['base']:6d} "
          f"{str(r.get('pdata_size')):>6s} {str(r.get('bl_retail')):>4s} {str(r.get('bl_ours')):>4s} "
          f"{str(r.get('pfx')):>4s}  {r['name'][:58]}")
