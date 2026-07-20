#!/usr/bin/env python3
"""Enumerate maximal unpinned .text runs from ground-truth fingerprints layout,
annotate each with real string union + majority autoid source proposal + channel."""
import json, re, os, sys, glob
from collections import defaultdict, Counter
ROOT="/home/free/code/milohax/rb3-xenon"

# pins
pins=[]; cur=None
for ln in open(f"{ROOT}/config/45410914/splits.txt"):
    m=re.match(r'^(\S+\.(cpp|c)):',ln.strip())
    if m: cur=m.group(1)
    mt=re.search(r'\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)',ln)
    if mt: pins.append((int(mt.group(1),16),int(mt.group(2),16),cur))
pins.sort()
pin_starts=[p[0] for p in pins]
import bisect
def in_pin(va):
    i=bisect.bisect_right(pin_starts,va)-1
    if i>=0 and pins[i][0]<=va<pins[i][1]: return pins[i][2]
    return None

# wired basenames
d=json.load(open(f"{ROOT}/config/45410914/objects.json"))
wired=set()
for grp,gv in d.items():
    for o in gv.get('objects',[]):
        nm=o.get('name') if isinstance(o,dict) else o
        if nm: wired.add(os.path.basename(nm))

# in-tree cpp basenames
intree=set(os.path.basename(p) for p in glob.glob(f"{ROOT}/src/**/*.cpp",recursive=True))

# fingerprints ground truth
fp=json.load(open(f"{ROOT}/fingerprints.json"))
fns=sorted(((int(k,16),v['size'],v.get('strings',[])) for k,v in fp.items()), key=lambda x:x[0])

# autoid src per VA
auto=json.load(open(f"{ROOT}/autoid.json"))
srcof={}
for e in auto:
    srcof[int(e['fn'].split('_')[1],16)]=e['src']

# maximal unpinned runs: contiguous fns none pinned, break on gap>0x400 OR a pinned fn
runs=[]; cur=[]
for va,sz,strs in fns:
    if in_pin(va) is not None:
        if cur: runs.append(cur); cur=[]
        continue
    if cur and va-(cur[-1][0]+cur[-1][1])>0x400:
        runs.append(cur); cur=[]
    cur.append((va,sz,strs))
if cur: runs.append(cur)

def is_junk_str(s):
    return len(s)<3
cands=[]
for run in runs:
    s=run[0][0]; e=run[-1][0]+run[-1][1]
    nb=e-s; nf=len(run)
    strs=set()
    for va,sz,st in run:
        for x in st: strs.add(x)
    # majority src
    votes=Counter(srcof.get(va) for va,_,_ in run if srcof.get(va))
    src=votes.most_common(1)[0][0] if votes else None
    base=os.path.basename(src) if src else None
    distinct=[x for x in strs if not is_junk_str(x)]
    cands.append(dict(start=s,end=e,nb=nb,nf=nf,nstr=len(distinct),
        src=src, base=base, wired=(base in wired) if base else None,
        intree=(base in intree) if base else None,
        strings=sorted(distinct)[:40]))

# rank: require >=3 distinct strings, a src, not wired
def channel(c):
    if not c['src']: return '?'
    p=c['src']
    if '/band3/' in p or '/network/' in p: return 'game'
    if 'dc3-decomp' in p or '/system/' in p: return 'engine'
    return '?'
good=[c for c in cands if c['src'] and c['nstr']>=3 and not c['wired']]
good.sort(key=lambda c:(-c['nstr'],-c['nf']))
json.dump(good,open('/home/free/tmp/fp2_runs.json','w'),indent=1)
print(f"total runs {len(runs)}, candidate (>=3 str, src, unwired) {len(good)}",file=sys.stderr)
print(f"{'BASE':32s} {'CH':6s} {'SPAN':21s} {'FN':>4s} {'BYT':>6s} {'STR':>4s} intree")
for c in good[:70]:
    print(f"{(c['base'] or '?'):32s} {channel(c):6s} {c['start']:08x}..{c['end']:08x} {c['nf']:4d} {c['nb']:6d} {c['nstr']:4d} {c['intree']}")
