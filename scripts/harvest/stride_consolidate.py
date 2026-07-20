import json, re, os, collections
r=json.load(open('build/45410914/report.json'))
smap=json.load(open('scripts/target_symbol_map.json'))
inv={}
for k,v in smap.items():
    if k.startswith('0x'): inv[v]=int(k,16)
pats=['_M_fill_insert','uninitialized_fill_n','uninitialized_copy','_M_erase','?resize@','_M_insert_overflow']
asm_cache={}
def load_asm(unit):
    base=unit.split('/')[-1]; p=f'build/45410914/asm/{base}.s'
    if not os.path.exists(p): return None
    lines=open(p).read().splitlines(); fns=[]
    for i,l in enumerate(lines):
        m=re.match(r'\.fn (fn_[0-9A-Fa-f]+)', l)
        if m: fns.append((int(m.group(1)[3:],16),i))
    return lines,fns
def body(unit,va):
    if unit not in asm_cache: asm_cache[unit]=load_asm(unit)
    if asm_cache[unit] is None: return None
    lines,fns=asm_cache[unit]
    for j,(v,i) in enumerate(fns):
        if v==va:
            end=fns[j+1][1] if j+1<len(fns) else len(lines); return lines[i:end]
    return None
def elemsize(b):
    li={}; divw=collections.Counter(); mulli=collections.Counter()
    for l in b:
        l=l.strip()
        m=re.search(r'\bli\s+(r\d+),\s*(0x[0-9a-f]+|\d+)',l)
        if m: li[m.group(1)]=int(m.group(2),0)
        m=re.search(r'\bmulli\s+r\d+,\s*r\d+,\s*(0x[0-9a-f]+|-?\d+)',l)
        if m: mulli[int(m.group(1),0)]+=1
        m=re.search(r'\bdivw\.?\s+r\d+,\s*r\d+,\s*(r\d+)',l)
        if m and m.group(1) in li: divw[li[m.group(1)]]+=1
    # prefer divw (element size), fallback mulli
    if divw: return divw.most_common(1)[0][0]
    if mulli: return mulli.most_common(1)[0][0]
    return None
# extract element type name from demangled
def etype(dm):
    m=re.search(r'vector<(.+?), class stlpmtx_std::StlNodeAlloc', dm)
    if m: return m.group(1)
    m=re.search(r'__uninitialized_(?:fill_n|copy)<(.+?)(?:\s+const)?\s*\*',dm)
    if m: return m.group(1)
    m=re.search(r'vector<(.+?), class XboxAllocator',dm)
    if m: return m.group(1)
    return dm[:50]
agg=collections.defaultdict(lambda: {'sizes':collections.Counter(),'units':set(),'best_mp':0,'nfns':0})
for un in r['units']:
    for f in un.get('functions',[]):
        n=f.get('name') or ''
        if not any(p in n for p in pats): continue
        mp=f.get('match_percent_normalized',0.0)
        if not (0.0<mp<100.0): continue
        va=inv.get(n)
        if va is None: continue
        b=body(un['name'],va)
        if not b: continue
        sz=elemsize(b)
        dm=(f.get('metadata') or {}).get('demangled_name','')
        et=etype(dm)
        a=agg[et]
        if sz: a['sizes'][sz]+=1
        a['units'].add(un['name'].split('/')[-1]); a['best_mp']=max(a['best_mp'],mp); a['nfns']+=1
rows=[]
for et,a in agg.items():
    retail = a['sizes'].most_common(1)[0][0] if a['sizes'] else None
    rows.append((retail if retail else -1, et, dict(a['sizes']), sorted(a['units']), a['nfns'], round(a['best_mp'],1)))
for retail,et,sizes,units,nf,mp in sorted(rows, key=lambda x:(x[0] or 0)):
    print(f'retail={retail!s:>5}  n={nf:2d} bestmp={mp:5}  {et[:60]}')
    print(f'         sizes={sizes} units={units}')
