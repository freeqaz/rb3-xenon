import glob,sys,json,collections,hashlib
sys.path.insert(0,'tools')
from icf_alias_build import collect, relocs_agree, vacuous, placeholder
sys.path.insert(0,'.')
from tools.icf_pair_adjudicate import family, chase
tgt=collect(sorted(glob.glob('build/45410914/obj/**/*.obj',recursive=True)),'t')
ours=collect(sorted(glob.glob('build/45410914/src/**/*.obj',recursive=True)),'o')
mapped=set()
m=json.load(open('scripts/target_symbol_map.json'))
for a,n in m.items():
    for x in (n if isinstance(n,list) else [n]):
        if x: mapped.add(x)
# charged pairs: same symbol present both sides, slot-wise name disagreement
sites=collections.Counter(); victims=collections.defaultdict(set)
for name,(mb,rel,sz) in ours.items():
    rt=tgt.get(name)
    if not rt or len(rt[1])!=len(rel): continue
    for (ro,rn,rty),(oo,on,oty) in zip(rt[1],rel):
        if ro!=oo or rty!=oty or rn==on: continue
        sites[(rn,on)]+=1; victims[(rn,on)].add(name)
al=json.load(open('scripts/symbol_aliases.json'))
eq={}
for g in al['groups']:
    grp=set([g['survivor']]+list(g['folded']))
    for n in grp: eq.setdefault(n,set()).update(grp)
before=len(sites)
for k in list(sites):
    if k[1] in eq.get(k[0],()) or k[0] in eq.get(k[1],()): del sites[k]
print("distinct charged pairs: %d raw, %d after subtracting ALREADY-ALIASED (%d removed)"%(before,len(sites),before-len(sites)))
real=[(p,c) for p,c in sites.items() if not placeholder(p[0]) and not placeholder(p[1])]
print("  of which BOTH sides carry real names:",len(real),"sites",sum(c for _,c in real))
print("  retail-side placeholder (unmapped callee = triage backlog):",
      len([1 for p,c in sites.items() if placeholder(p[0])]))
real.sort(key=lambda x:-x[1])
N=int(sys.argv[1]) if len(sys.argv)>1 else 100
res=collections.Counter(); rows=[]
for (rn,on),c in real[:N]:
    if rn not in tgt or on not in ours: res['UNDECIDABLE_absent']+=1; continue
    if vacuous(tgt[rn]) or vacuous(ours[on]):
        f=family(tgt,ours,on,rn)
        if len(f['retail_family'])==1 and f['our_slot0_matches_retail'] and f['excluded_by_slot0']:
            res['PROVEN_family_thunk']+=1; rows.append(('PROVEN_family_thunk',c,rn,on)); continue
        res['UNDECIDABLE_vacuous']+=1; rows.append(('UNDECIDABLE_vacuous',c,rn,on)); continue
    if tgt[rn][0]!=ours[on][0]: res['REFUTED_bytes']+=1; rows.append(('REFUTED_bytes',c,rn,on)); continue
    if relocs_agree(tgt[rn],ours[on],mapped,strict=True):
        res['PROVEN_flatT1']+=1; rows.append(('PROVEN_flatT1',c,rn,on)); continue
    if chase(tgt,ours,rn,on,mapped,out=[]):
        res['PROVEN_chase']+=1; rows.append(('PROVEN_chase',c,rn,on)); continue
    f=family(tgt,ours,on,rn)
    if len(f['retail_family'])==1 and len(f['our_family'])>1 and f['excluded_by_slot0']:
        res['PROVEN_family']+=1; rows.append(('PROVEN_family',c,rn,on)); continue
    res['REFUTED_relocs']+=1; rows.append(('REFUTED_relocs',c,rn,on))
print("\nADJUDICATION of the top %d real-name pairs by site count:"%N)
for k,v in res.most_common(): print("   %-24s %4d"%(k,v))
prov=sum(v for k,v in res.items() if k.startswith('PROVEN'))
print("   => PROVEN %d / REFUTED %d / UNDECIDABLE %d"%(prov,
   sum(v for k,v in res.items() if k.startswith('REFUTED')),
   sum(v for k,v in res.items() if k.startswith('UNDECID'))))
json.dump([(a,b,c,d) for a,b,c,d in rows],open('/home/free/tmp/census_rows.json','w'))
