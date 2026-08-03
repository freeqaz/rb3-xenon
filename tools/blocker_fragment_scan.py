"""Classify unpaired-anon blocker rows as FRAGMENT vs COMPLETE.

Usage:  venv/bin/python tools/blocker_fragment_scan.py <ceiling_blocker_partition.json>
                                                       [game|engine|all]

The tier argument selects WHICH units are the treatment population.  It
defaults to `game` so DU-1's measurement reproduces byte-for-byte; `engine`
(src/system/**) was added by lane DW-3 to test whether DU-1's game-tier
framing transfers.  The DETECTOR and the CONTROL are untouched by the flag --
only the unit-selection predicate moves, so the two tiers cannot drift apart.

A retail function that symbols.txt has chopped into several fn_ rows shows a
tell that needs no map and no source: the row's LAST instruction is not a
terminator, so control falls through into the next row's address.

CONTROL (mandatory -- an untreated population): run the identical test over
rows that are MAPPED AND AT mpn==100.  Those are known-good whole functions,
so the fragment rate there is the false-positive rate of this detector.
"""
import json,re,sys
from pathlib import Path
root=Path(__file__).resolve().parent.parent
TERM=re.compile(r'\t(blr|bctr|bctrl|b\s|ba\s|rfi)')
INSN=re.compile(r'^/\* [0-9A-F]{8} [0-9A-F]{8}  (?:[0-9A-F]{2} ){4}\*/\t(.*)$')
def parse(p):
    bodies={};cur=None;buf=[]
    for L in Path(p).read_text().splitlines():
        mm=re.match(r'^\.fn (\S+?), ',L)
        if mm: cur=mm.group(1);buf=[];continue
        if L.startswith('.endfn') and cur: bodies[cur]=buf;cur=None;continue
        if cur is not None:
            m2=INSN.match(L)
            if m2:
                t=m2.group(1).strip()
                if t.startswith('.4byte'):   # alignment padding, not an instruction
                    continue
                buf.append(t)
    return bodies
def is_fragment(ins):
    if not ins: return None
    last=ins[-1]
    op=last.split()[0]
    return not (op=='blr' or op=='bctr' or op=='b' or op.startswith('b ') or op=='rfi')
part=json.load(open(sys.argv[1] if len(sys.argv)>1 else 'part.json'))
rep=json.load(open(root/'build/45410914/report.json'))
m=json.load(open(root/'scripts/target_symbol_map.json'))
TIER=sys.argv[2] if len(sys.argv)>2 else 'game'
if TIER not in ('game','engine','all'):
    print(f"REFUSING: unknown tier {TIER!r} (want game|engine|all)"); sys.exit(4)
def tier(sp):
    if not sp: return False
    g = sp.startswith('src/band3/') or sp.startswith('src/network/')
    e = sp.startswith('src/system/')
    return {'game':g,'engine':e,'all':g or e}[TIER]
unitsrc={r['unit']:r['source_path'] for r in part}
def asmp(unit):
    stem=unit.split('/',1)[1] if unit.startswith('default/') else unit
    p=root/'build/45410914/asm'/(stem+'.s')
    return p if p.exists() else None
# ---- treatment: game unpaired-anon blockers
treat=[];ctrl=[]
cache={}
for r in part:
    if not tier(r['source_path']): continue
    p=asmp(r['unit'])
    if not p: continue
    if p not in cache: cache[p]=parse(p)
    B=cache[p]
    for b in r['blockers']:
        key='fn_%08X'%int(b['va'],16)
        ins=B.get(key)
        f=is_fragment(ins) if ins else None
        treat.append((r['unit'],b['va'],b['size'] if 'size' in b else None,b['label'],f,len(ins) if ins else 0))
# ---- control: mapped rows at mpn==100 in the SAME units
gm={r['unit'] for r in part if tier(r['source_path'])}
for u in rep['units']:
    if u['name'] not in gm: continue
    p=asmp(u['name'])
    if not p: continue
    if p not in cache: cache[p]=parse(p)
    B=cache[p]
    for f in (u.get('functions') or []):
        if f['match_percent_normalized']!=100.0: continue
        if re.match(r'^fn_[0-9A-Fa-f]{8}$',f['name']): continue
        # find its VA via the map inverse
        pass
# control by VA: every target row in these units whose VA IS in the map
for u in rep['units']:
    if u['name'] not in gm: continue
    p=asmp(u['name'])
    if not p: continue
    B=cache[p]
    txt=Path(p).read_text()
    for va,sz in re.findall(r'# \.text:0x[0-9A-Fa-f]+ \| (0x[0-9A-Fa-f]+) \| size: (0x[0-9A-Fa-f]+)',txt):
        if va.lower() not in m: continue
        ins=B.get('fn_'+va[2:].upper())
        if ins is None: continue
        fr=is_fragment(ins)
        ctrl.append((u['name'],va,int(sz,16),fr,len(ins)))
tf=[t for t in treat if t[4] is not None]
cf=[c for c in ctrl if c[3] is not None]
nt=sum(1 for t in tf if t[4]); nc=sum(1 for c in cf if c[3])
print(f"TREATMENT  {TIER} unpaired-anon blockers : {nt}/{len(tf)} = {100*nt/max(len(tf),1):.1f}% fall-through")
print(f"CONTROL    {TIER} MAPPED rows same units : {nc}/{len(cf)} = {100*nc/max(len(cf),1):.1f}% fall-through")
if len(cf)==0: print("!! CONTROL EMPTY -- refusing to interpret"); sys.exit(4)
print(f"ENRICHMENT : {(nt/len(tf))/max(nc/len(cf),1e-9):.2f}x\n")
print("per-unit blocker verdicts (FRAG = falls through into next row):")
for u,va,sz,lab,f,n in sorted(tf):
    print(f"  {'FRAG' if f else 'whole'}  {u[:44]:44s} {va} {str(sz):>5}B {lab[:24]:24s} ins={n}")

