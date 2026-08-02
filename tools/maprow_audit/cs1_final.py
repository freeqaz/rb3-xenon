import json, collections, sys, os
WT="/home/free/tmp/laneCS1/wt"
sys.path.insert(0,WT+"/tools"); sys.path.insert(0,WT+"/tools/maprow_audit"); os.chdir(WT)
import thunk_identity as T, thunk_oracle as TO
o,tmap,timg = TO.load()
raw=json.load(open(WT+"/scripts/target_symbol_map.json"))
rt=json.load(open("/home/free/tmp/laneCS1/work/treat.json")); rec={r["addr"]:r for r in rt}
props=json.load(open("/home/free/tmp/laneCS1/work/props.json"))
folds=json.load(open("/home/free/tmp/laneCS1/work/folds.json"))
print("folds.json keys:", list(folds)[:6])

# build VA -> reloc-group-size and VA -> shape-group-size
def grpmap(d):
    m={}
    for k,v in (d.items() if isinstance(d,dict) else []):
        if isinstance(v,list):
            for va in v: m[int(va,16) if isinstance(va,str) else va]=len(v)
    return m
reloc = grpmap(folds.get("reloc") or folds.get("reloc_identical") or {})
shape = grpmap(folds.get("shape") or folds.get("shape_identical") or {})
print("reloc-grouped VAs:",len(reloc)," shape-grouped VAs:",len(shape))

names=collections.Counter(v for k,v in raw.items() if k.startswith("0x") and isinstance(v,str))
keep={}; rej=collections.Counter()
for a,d in props.items():
    tva=int(rec[a]["target"],16)
    if reloc.get(tva,1)>1: rej["target is a RELOC-IDENTICAL fold rep"]+=1; continue
    if shape.get(tva,1)>1: rej["target in SHAPE group (identity ambiguous)"]+=1; continue
    keep[a]=d
print("\nfold gate on the 15 proposals:")
for k,v in rej.most_common(): print(f"   reject {k}: {v}")
print(f"   SURVIVE: {len(keep)}")

# injectivity: count duplicate names BEFORE and AFTER
after=collections.Counter(names)
for a,d in keep.items():
    after[d["expect"]]-=1
    if after[d["expect"]]==0: del after[d["expect"]]
    after[d["new"]]+=1
db=sum(1 for n,c in names.items() if c>1); da=sum(1 for n,c in after.items() if c>1)
print(f"\nINJECTIVITY  duplicate names before={db}  after={da}  introduced={da-db}")
trip=[{"addr":a,"expect":d["expect"],"new":d["new"]} for a,d in sorted(keep.items())]
json.dump(trip, open("/home/free/tmp/laneCS1/work/cs1_triples.json","w"), indent=1)
print("triples written:",len(trip))
for t in trip: print("  ",t["addr"],"\n     -",t["expect"][:70],"\n     +",t["new"][:70])
