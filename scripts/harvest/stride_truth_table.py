import json,re,os,collections
r=json.load(open('build/45410914/report.json'))
smap=json.load(open('scripts/target_symbol_map.json'))
inv={}
for k,v in smap.items():
    if k.startswith('0x'): inv[v]=int(k,16)
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
def selfincr(b):
    # count self-increment addi/subi (ptr stride); return counter
    c=collections.Counter()
    for l in b:
        m=re.search(r'\b(addi|subi)\s+(r\d+),\s*(r\d+),\s*(0x[0-9a-f]+|\d+)',l)
        if m and m.group(2)==m.group(3): c[int(m.group(4),0)]+=1
    return c
def filldivw(b):
    li={}; c=collections.Counter()
    for l in b:
        m=re.search(r'\bli\s+(r\d+),\s*(0x[0-9a-f]+|\d+)',l)
        if m: li[m.group(1)]=int(m.group(2),0)
        m=re.search(r'\bmulli\s+r\d+,\s*r\d+,\s*(0x[0-9a-f]+|-?\d+)',l)
        if m: c[int(m.group(1),0)]+=1
        m=re.search(r'\bdivw\.?\s+r\d+,\s*r\d+,\s*(r\d+)',l)
        if m and m.group(1) in li: c[li[m.group(1)]]+=1
    return c
def etype(dm):
    for pat in [r'vector<(.+?), class stlpmtx_std::StlNodeAlloc',r'vector<(.+?), class XboxAllocator',r'__uninitialized_(?:fill_n|copy)<(.+?)(?:\s+const)?\s*\*']:
        m=re.search(pat,dm)
        if m: return m.group(1)
    return dm[:45]
COPY=collections.defaultdict(collections.Counter)   # etype -> stride counter from copy loops (truth)
FILL=collections.defaultdict(collections.Counter)   # etype -> fill divw (suspect)
MP=collections.defaultdict(float)
UN=collections.defaultdict(set)
for un in r['units']:
    for f in un.get('functions',[]):
        n=f.get('name') or ''
        dm=(f.get('metadata') or {}).get('demangled_name','')
        is_copy = 'uninitialized_copy' in n or ('operator=' in dm and 'vector' in dm)
        is_fill = '_M_fill_insert' in n or 'uninitialized_fill_n' in n or '_M_insert_overflow' in n
        if not (is_copy or is_fill): continue
        mp=f.get('match_percent_normalized',0.0)
        if not (0.0<mp<100.0): continue
        va=inv.get(n)
        if va is None: continue
        b=body(un['name'],va)
        if not b: continue
        et=etype(dm)
        UN[et].add(un['name'].split('/')[-1]); MP[et]=max(MP[et],mp)
        if is_copy:
            for sz,cnt in selfincr(b).items():
                if cnt>=2 and sz not in (96,112,128,1): COPY[et][sz]+=cnt
        if is_fill:
            for sz,cnt in filldivw(b).items(): FILL[et][sz]+=cnt
allt=set(COPY)|set(FILL)
rows=[]
for et in allt:
    copy=COPY[et].most_common(1)[0][0] if COPY[et] else None
    fill=FILL[et].most_common(1)[0][0] if FILL[et] else None
    truth=copy if copy is not None else fill
    rows.append((truth or 0, et, copy, fill, round(MP[et],1), sorted(UN[et])))
for truth,et,copy,fill,mp,un in sorted(rows):
    flag=''
    if copy is not None and fill is not None and copy!=fill: flag=' <FOLD-DIFF fill!=copy>'
    print(f'truth={truth!s:>4} copy={copy!s:>4} fill={fill!s:>4} mp={mp:5} {et[:48]:48s} {un}{flag}')
