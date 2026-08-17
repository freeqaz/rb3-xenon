import json, collections
rep=json.load(open('build/45410914/report.json'))
mpn={}
for u in rep['units']:
    for f in u.get('functions',[]): mpn[f['name']]=float(f.get('match_percent_normalized',0))

def profile(r):
    hard=0; namechg=0; regchg=0; immchg=0; brchg=0; pairs=collections.Counter()
    for i in r['instructions']:
        mt=i.get('match_type')
        if mt=='equal': continue
        if mt!='diff_arg': hard+=1; continue
        t=i.get('target') or {}; b=i.get('base') or {}
        ta=t.get('typed_args',[]); ba=b.get('typed_args',[])
        kinds=set(); sp=None
        for x,y in zip(ta,ba):
            if x.get('value')!=y.get('value'):
                kinds.add(x.get('type'))
                if x.get('type')=='Symbol': sp=(x.get('value'),y.get('value'))
        if kinds=={'Symbol'}:
            namechg+=1; pairs[sp]+=1
        else:
            if 'Register' in kinds: regchg+=1
            elif 'BranchDest' in kinds: brchg+=1
            else: immchg+=1
    return hard,namechg,regchg,immchg,brchg,pairs

for label,path in (('VocalTrack','/home/free/tmp/w19_vt.json'),('VocalPlayer','/home/free/tmp/w19_vp.json')):
    recs=[json.loads(l) for l in open(path) if l.strip()]
    rows=[]
    for r in recs:
        h,n,g,m,br,p=profile(r)
        rows.append((r['target_size'],r['fuzzy_match_percent'],h,n,g,m,br,r['symbol'],p))
    rows.sort(key=lambda x:-x[0])
    print('='*104); print(label)
    print('%7s %8s %5s %5s %5s %5s %5s  %s'%('SIZE','FUZZY','hard','NAME','reg','imm','br','SYMBOL'))
    coll=0; collrows=0; blocked=0
    for sz,fz,h,n,g,m,br,sym,p in rows:
        flag='COLLECTABLE' if n==0 else 'name-charged'
        if n==0: coll+=sz; collrows+=1
        else: blocked+=sz
        print('%7d %8.4f %5d %5d %5d %5d %5d  %-11s %s'%(sz,fz,h,n,g,m,br,flag,sym[:46]))
    print('  --> rows with NO real name charge: %d rows, %d B  (source work alone can cross these)'%(collrows,coll))
    print('  --> rows needing a name/map fix too: %d B'%blocked)
