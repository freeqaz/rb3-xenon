import json, collections, sys, os, re
WT="/home/free/tmp/laneCS1/wt"
sys.path.insert(0,WT+"/tools"); sys.path.insert(0,WT+"/tools/maprow_audit"); os.chdir(WT)
import thunk_identity as T, thunk_oracle as TO
o,tmap,timg = TO.load()
raw=json.load(open(WT+"/scripts/target_symbol_map.json"))
bij=set(a.lower() for a in raw["_bijection_arbitrary"]); icf=set(a.lower() for a in raw["_icf_arbitrary"])
rt=json.load(open("/home/free/tmp/laneCS1/work/treat.json"))
rec={r["addr"]:r for r in rt}
addrkeys={k.lower() for k in raw if k.startswith("0x")}
names_in_map=collections.Counter(v for k,v in raw.items() if k.startswith("0x") and isinstance(v,str))
print("pre-existing duplicate NAMES in map:", sum(1 for n,c in names_in_map.items() if c>1))

_VT=re.compile(r'\$([0-4])PPPPPPPM@([0-9A-P]+)@')
ACC={'0':'A','1':'A','2':'I','3':'I','4':'U'}
def synth_thunk(thunkname, targetname):
    """Build the corrected thunk name: METHOD+SIG from the retail target body,
    CLASS + vtordisp encoding from the thunk row itself (its slot's owner)."""
    m=_VT.search(thunkname)
    if not m: return None
    cls=TO.qcls(thunkname)
    if not cls or not isinstance(targetname,str) or not targetname.startswith("?"): return None
    # split target into <prefix-with-method> @@ <access><sig>
    if "@@" not in targetname: return None
    head, sig = targetname.split("@@",1)
    if not sig or sig[0] not in "ABEFIJMNQRUV": return None
    meth = TO.method_of(targetname)
    if not meth: return None
    if targetname.startswith("??_G") or targetname.startswith("??_E"):
        pre = targetname[:4]
        return f"{pre}{cls}@@{m.group(0)}{sig[1:]}"
    if not targetname.startswith("?") or targetname.startswith("??"): return None
    return f"?{meth}@{cls}@@{m.group(0)}{sig[1:]}"

cond=[a for a in rec if rec[a]["verdict"]=="METHOD_DIFFERS"]
both=[a for a in cond if rec[a].get("target_row") and o.slot_consensus(a)
      and TO.method_of(rec[a]["target_row"])==o.slot_consensus(a)]
print(f"\ncandidates: METHOD_DIFFERS={len(cond)}  doubly-corroborated(branch==slot)={len(both)}")

stats=collections.Counter(); props={}
for a in both:
    r=rec[a]
    if r["target"] in bij or r["target"] in icf: stats["reject_ICF/bij_target"]+=1; continue
    if not o.class_ok(a, r["proposed"]):
        # ancestry already contradicts the INCUMBENT class -> class is unsafe to carry over
        stats["reject_incumbent_class_contradicted"]+=1; continue
    n=synth_thunk(r["proposed"], r["target_row"])
    if not n: stats["reject_unsynthesizable"]+=1; continue
    if n==r["proposed"]: stats["reject_noop"]+=1; continue
    if names_in_map.get(n): stats["reject_NON_INJECTIVE(name exists)"]+=1; continue
    stats["PROPOSE"]+=1; props[a]=(r["proposed"],n)
for k,v in stats.most_common(): print(f"   {k:38s} {v}")
print("\nsample proposals:")
for a,(oldn,newn) in list(props.items())[:10]:
    print(f"  {a}\n     - {oldn[:64]}\n     + {newn[:64]}")
json.dump({a:{"expect":v[0],"new":v[1]} for a,v in props.items()},
          open("/home/free/tmp/laneCS1/work/props.json","w"),indent=1)
print("\nproposals written:",len(props))
