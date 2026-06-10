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
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from fuzzy_content_match import read_coff_functions, opcodes, parse_splits
from dc3_obj_source import DC3_OBJ_DIR, iter_dc3_objs

ROOT='/home/free/code/milohax/rb3-xenon'
RB3_GLOB=os.path.join(ROOT,'build/45410914/obj/auto_03_*_text.obj')
# Canonical retail-DC3 TARGET tree (shared with dc3_content_match / fuzzy_content_match).
DC3_DIR=DC3_OBJ_DIR
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
    dc3_dir=sys.argv[3] if len(sys.argv)>3 else DC3_DIR  # CLI override of DC3 obj tree
    splits=parse_splits(os.path.join(ROOT,'config/45410914/splits.txt'))
    pins=[]
    for cpp,(lo,hi) in splits.items(): pins.append((lo,hi))
    pins.sort(); pstarts=[p[0] for p in pins]
    def pinned(a):
        i=bisect.bisect_right(pstarts,a)-1
        return 0<=i<len(pins) and pins[i][0]<=a<pins[i][1]
    # DC3 functions. Skip NON-REAL names: anonymous (fn_/sub_), guard/thunk ($),
    # and dtk/linker ICF-fold artifacts (merged_<addr>, __unwind*, __catch*,
    # __tls*, jumptable_*) — these are not usable mangled symbols, so emitting
    # one as an "identity" gives downstream (fn_resolver / target_symbol_map) a
    # dead name (e.g. fn_82534F88 -> merged_828DC218 or jumptable_82A1CDA4).
    # Survey of .dc3_text_scratch/named/obj confirms only these prefixes occur
    # as artifact names (lbl_, switchdata, jpt_, sub_ are NOT present in this
    # tree; __unwind is the dominant noise at ~21k entries; jumptable_ ~23 entries
    # but jaccard saturates to 1.0 on their short opcode patterns, poisoning T4).
    NONREAL=('fn_','sub_','$','merged_','__unwind','__catch','__tls','jumptable_',
             'lbl_','switchdata_','jpt_')
    dc3=[]
    for f in iter_dc3_objs(dc3_dir):
        for fn in read_coff_functions(f):
            if fn['size']<minsize or fn['name'].startswith(NONREAL): continue
            sh=shingles(fn['code'])
            if sh: dc3.append((fn['name'],os.path.basename(f),sh,len(fn['code'])))
    print(f"DC3 fns indexed: {len(dc3)}",file=sys.stderr)
    # RB3 functions (unpinned, unnamed).
    # IMPORTANT: read_coff_functions iterates multiple obj files (the RB3 text obj
    # contains ICF-merged duplicate COMDAT sections), so the same fn_<addr> can
    # appear multiple times in the raw stream. Deduplicate by address — keep the
    # FIRST occurrence (arbitrary; all copies are byte-identical by definition of
    # ICF merging) so each RB3 address produces exactly ONE output row.
    rb3_seen=set()
    rb3=[]
    for f in sorted(glob.glob(RB3_GLOB)):
        for fn in read_coff_functions(f):
            m=re.match(r'fn_([0-9A-Fa-f]+)$',fn['name'])
            if not m or fn['size']<minsize: continue
            a=int(m.group(1),16)
            if a in rb3_seen: continue   # deduplicate ICF copies
            rb3_seen.add(a)
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
        # Iterate candidates in a STABLE order and break Jaccard ties on a
        # deterministic key (the DC3 name). Without this the winner among
        # ICF-identical bodies (jaccard==1.0 across many functions) depends on
        # Python set-iteration order and varies run-to-run — the same
        # nondeterminism that made global_fuzzy_pairs disagree with itself and
        # with dc3_content_match. We pick the lexicographically-smallest name so
        # the output is reproducible regardless of obj enumeration.
        for ci in sorted(cand):
            dn,do,dsh,dsz=dc3[ci]
            j=len(sh&dsh)/len(sh|dsh)
            if best is None or j>best[0] or (j==best[0] and dn<best[1]):
                best=(j,dn,do,dsz)
        if best and best[0]>=thr:
            # Size-ratio sanity gate: drop pairs where one body is >3x the other.
            # Short opcode patterns (ICF-saturated shingles) can produce jaccard==1.0
            # false matches between bodies of wildly different size (e.g. a 92-byte
            # DC3 stub matching inside a 316-byte RB3 function). A [0.33, 3.0] ratio
            # window catches these without dropping any of the 4414 good real-name
            # pairs observed in the current dataset (all surveyed known-good T4 pairs
            # have ratio within [0.44, 2.78] — and the 7 boundary cases at the
            # extremes are all ICF-saturation FPs confirmed not in dc3_content_match).
            # fn_resolver.py T4 does NOT size-gate, so the gate here is the right place.
            dc3sz=best[3]
            if dc3sz>0 and sz>0:
                ratio=sz/dc3sz
                if ratio<0.33 or ratio>3.0: continue
            pairs.append({'rb3_addr':'0x%08X'%a,'dc3_name':best[1],'dc3_obj':best[2],
                          'jaccard':round(best[0],3),'rb3_size':sz,'dc3_size':best[3]})
    pairs.sort(key=lambda p:-p['jaccard'])
    json.dump(pairs,open('global_fuzzy_pairs.json','w'),indent=1)
    print(f"\nGLOBAL LSH pairs (unpinned RB3, jaccard>={thr}): {len(pairs)}")
    near=[p for p in pairs if p['jaccard']>=0.97 and p['rb3_size']==p['dc3_size']]
    print(f"  of those, jaccard>=0.97 AND same size: {len(near)} (strong byte-match candidates)")
    for p in pairs[:15]:
        print(f"  {p['rb3_addr']} j={p['jaccard']} sz={p['rb3_size']}/{p['dc3_size']} {p['dc3_name'][:50]}")


if __name__ == "__main__":
    main()
