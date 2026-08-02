import json, collections, sys, os, random
WT="/home/free/tmp/laneCS1/wt"
sys.path.insert(0,WT+"/tools"); sys.path.insert(0,WT+"/tools/maprow_audit"); os.chdir(WT)
import thunk_identity as T, thunk_oracle as TO
o,tmap,timg = TO.load()
raw=json.load(open(WT+"/scripts/target_symbol_map.json"))
bij=set(a.lower() for a in raw["_bijection_arbitrary"])
rt=json.load(open("/home/free/tmp/laneCS1/work/treat.json"))
rec={r["addr"]:r for r in rt}

def shape(va):
    """reloc-MASKED body of a forwarder: words up to and including the terminal
    `b`, with the 24-bit branch displacement ZEROED. This is the equivalence
    the bijection used ('reloc-masked byte-identical')."""
    ws=[]
    for i in range(6):
        w=timg.word(va+4*i)
        if w is None: return None
        if (w>>26)==18:
            ws.append(w & 0xFC000003); return tuple(ws)
        if (w>>26) in (16,19): return None
        ws.append(w)
    return None

# shape-group EVERY thunk row in the map (not just mine)
allth={}
for a,n in raw.items():
    if not (isinstance(a,str) and a.startswith("0x") and isinstance(n,str)): continue
    al=a.lower()
    if T.thunk_kind(n) is None: continue
    s=shape(int(al,16))
    if s: allth[al]=(n,s)
groups=collections.defaultdict(list)
for a,(n,s) in allth.items(): groups[s].append(a)
sizes=collections.Counter(len(v) for v in groups.values())
print(f"map thunk rows shape-grouped: {len(allth)} into {len(groups)} groups")
print("  group-size histogram (top):", sizes.most_common(8))
mine=[a for a in rec]
gm=collections.Counter(len(groups[allth[a][1]]) for a in mine if a in allth)
print(f"  my 602 land in groups of size: {gm.most_common(6)}")

# evidence method per VA
def evm(a):
    r=rec.get(a)
    if r and r.get("target_row"): return TO.method_of(r["target_row"])
    d=T.dethunk_named(timg,tmap,int(a,16))
    return TO.method_of(tmap.get("0x%08x"%d[0])) if d[0] else None

# Within each shape group, is the multiset of INCUMBENT methods == multiset of
# EVIDENCE methods?  If yes the group is a closed permutation and re-assignment
# is exact + injective.
closed=0; rows=0; examined=0; part=0
for s,addrs in groups.items():
    if len(addrs)<2: continue
    if not any(a in rec for a in addrs): continue
    examined+=1
    cur=collections.Counter(TO.method_of(allth[a][0]) for a in addrs)
    ev =collections.Counter(x for x in (evm(a) for a in addrs) if x)
    if cur==ev:
        closed+=1; rows+=len(addrs)
    elif sum((cur&ev).values())>0: part+=1
print(f"\nshape groups (size>=2) containing >=1 of my rows: {examined}")
print(f"   exact closed permutation (cur==evidence multiset): {closed}  covering {rows} rows")
print(f"   partial overlap: {part}")
