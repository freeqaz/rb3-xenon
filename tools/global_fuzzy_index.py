#!/usr/bin/env python3
"""PROTOTYPE: global cross-binary fuzzy index (DC3 named -> RB3 anon) via
banded MinHash LSH over reloc-masked opcode shingles. Finds high-similarity
pairs WITHOUT per-unit scoping, so it can identify partials in UNPINNED RB3
regions the scoped fuzzy_content_match.py never looks at.

Pipeline:
  1. Parse all RB3 fns (addr) + all DC3 named fns (name) -> opcode-shingle sets
     (reloc word zeroed -> opcode token; 4-shingles).
  2. MinHash each (K perms), band into B bands of R rows.
  3. Candidate pairs = share >=1 LSH bucket. Verify with exact Jaccard on
     shingle sets; emit pairs >= threshold whose RB3 addr is UNPINNED.
"""
import sys, os, glob, struct, re, hashlib, random, bisect, json
from collections import defaultdict
sys.path.insert(0,'/home/free/code/milohax/rb3-xenon/tools')
from fuzzy_content_match import read_coff_functions, opcodes, parse_splits

ROOT='/home/free/code/milohax/rb3-xenon'
RB3_GLOB=os.path.join(ROOT,'build/45410914/obj/auto_03_*_text.obj')
DC3_DIR='/home/free/code/milohax/.dc3_text_scratch/named/obj'
K=64; B=16; R=4   # 64 minhashes, 16 bands x 4 rows
random.seed(1)
PERMS=[(random.randint(1,2**61-1),random.randint(0,2**61-1)) for _ in range(K)]
MOD=(1<<61)-1
NSH=4

def shingles(code):
    ops=opcodes(code)
    if len(ops)<NSH: return frozenset([tuple(ops)]) if ops else frozenset()
    return frozenset(tuple(ops[i:i+NSH]) for i in range(len(ops)-NSH+1))

def minhash(sh):
    if not sh: return None
    hs=[hash(s)&0xffffffffffffffff for s in sh]
    sig=[]
    for a,b in PERMS:
        sig.append(min((a*h+b)%MOD for h in hs))
    return tuple(sig)

def main():
    minsize=int(sys.argv[1]) if len(sys.argv)>1 else 64
    thr=float(sys.argv[2]) if len(sys.argv)>2 else 0.85
    splits=parse_splits(os.path.join(ROOT,'config/45410914/splits.txt'))
    pins=[]
    for cpp,(lo,hi) in splits.items(): pins.append((lo,hi))
    pins.sort(); pstarts=[p[0] for p in pins]
    def pinned(a):
        i=bisect.bisect_right(pstarts,a)-1
        return 0<=i<len(pins) and pins[i][0]<=a<pins[i][1]
    # DC3 functions
    dc3=[]
    for f in glob.glob(os.path.join(DC3_DIR,'**','*.obj'),recursive=True):
        for fn in read_coff_functions(f):
            if fn['size']<minsize or fn['name'].startswith(('fn_','sub_','$')): continue
            sh=shingles(fn['code'])
            if sh: dc3.append((fn['name'],os.path.basename(f),sh,len(fn['code'])))
    print(f"DC3 fns indexed: {len(dc3)}",file=sys.stderr)
    # RB3 functions (unpinned, unnamed)
    rb3=[]
    for f in sorted(glob.glob(RB3_GLOB)):
        for fn in read_coff_functions(f):
            m=re.match(r'fn_([0-9A-Fa-f]+)$',fn['name'])
            if not m or fn['size']<minsize: continue
            a=int(m.group(1),16)
            if pinned(a): continue   # only UNPINNED
            sh=shingles(fn['code'])
            if sh: rb3.append((a,sh,len(fn['code'])))
    print(f"RB3 UNPINNED fns: {len(rb3)}",file=sys.stderr)
    # build LSH buckets over DC3
    dc3_sig=[minhash(sh) for _,_,sh,_ in dc3]
    buckets=defaultdict(list)
    for idx,sig in enumerate(dc3_sig):
        if not sig: continue
        for b in range(B):
            band=sig[b*R:(b+1)*R]
            buckets[(b,band)].append(idx)
    print(f"LSH buckets: {len(buckets)}",file=sys.stderr)
    # query RB3
    pairs=[]
    for a,sh,sz in rb3:
        sig=minhash(sh)
        if not sig: continue
        cand=set()
        for b in range(B):
            cand.update(buckets.get((b,sig[b*R:(b+1)*R]),()))
        best=None
        for ci in cand:
            dn,do,dsh,dsz=dc3[ci]
            j=len(sh&dsh)/len(sh|dsh)
            if best is None or j>best[0]:
                best=(j,dn,do,dsz)
        if best and best[0]>=thr:
            pairs.append({'rb3_addr':'0x%08X'%a,'dc3_name':best[1],'dc3_obj':best[2],
                          'jaccard':round(best[0],3),'rb3_size':sz,'dc3_size':best[3]})
    pairs.sort(key=lambda p:-p['jaccard'])
    json.dump(pairs,open('global_fuzzy_pairs.json','w'),indent=1)
    print(f"\nGLOBAL LSH pairs (unpinned RB3, jaccard>={thr}): {len(pairs)}")
    near=[p for p in pairs if p['jaccard']>=0.97 and p['rb3_size']==p['dc3_size']]
    print(f"  of those, jaccard>=0.97 AND same size: {len(near)} (strong byte-match candidates)")
    for p in pairs[:15]:
        print(f"  {p['rb3_addr']} j={p['jaccard']} sz={p['rb3_size']}/{p['dc3_size']} {p['dc3_name'][:50]}")
