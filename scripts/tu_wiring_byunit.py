#!/usr/bin/env python3
"""Attribute every map entry to its OWNING splits.txt unit (by address range),
then report per-unit: total, compiled, uncompiled, wired?, source. This is the
robust ranking (address-based, no mangling parse). Ranks pinned-but-uncompiled
units = target objs ready, just need TU wiring."""
import glob, json, os, re, struct, sys, bisect
from collections import defaultdict
from pathlib import Path

ROOT = Path("/home/free/tmp/wt-tucensus")

# defined syms in our compiled objs
def parse_defined(data):
    if len(data)<20: return []
    so=struct.unpack_from("<I",data,8)[0]; n=struct.unpack_from("<I",data,12)[0]
    if not so or not n: return []
    st=so+n*18; out=[]; i=0
    while i<n:
        eo=so+i*18
        if eo+18>len(data): break
        nb=data[eo:eo+8]
        if nb[:4]==b"\x00\x00\x00\x00":
            o=struct.unpack_from("<I",nb,4)[0]; ao=st+o
            try: name=data[ao:data.index(b"\x00",ao)].decode("ascii","replace")
            except ValueError: name=""
        else: name=nb.split(b"\x00")[0].decode("ascii","replace")
        secn=struct.unpack_from("<h",data,eo+12)[0]; aux=data[eo+17]
        if secn>0: out.append(name)
        i+=1+aux
    return out
defined=set()
for f in glob.glob(str(ROOT/"build/45410914/src/**/*.obj"),recursive=True):
    try: defined.update(parse_defined(open(f,"rb").read()))
    except Exception: pass

# splits.txt: unit -> list of (lo,hi); build sorted range->unit index
txt=(ROOT/"config/45410914/splits.txt").read_text()
cur=None; unit_ranges=[]
for line in txt.splitlines():
    if line and not line[0].isspace() and line.rstrip().endswith(":") and line.rstrip()[:-1].endswith((".cpp",".c")):
        cur=line.rstrip()[:-1]; continue
    mt=re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)",line)
    if mt and cur:
        unit_ranges.append((int(mt.group(1),16),int(mt.group(2),16),cur))
unit_ranges.sort()
starts=[r[0] for r in unit_ranges]
def unit_of(a):
    i=bisect.bisect_right(starts,a)-1
    if i>=0 and unit_ranges[i][0]<=a<unit_ranges[i][1]:
        return unit_ranges[i][2]
    return None

oj=(ROOT/"config/45410914/objects.json").read_text()
wired={os.path.basename(x) for x in re.findall(r'"([^"]+\.cpp)"',oj)}

raw=json.loads((ROOT/"scripts/target_symbol_map.json").read_text())
per=defaultdict(lambda:{"tot":0,"comp":0,"uncomp":0,"lo":1<<40,"hi":0})
for k,v in raw.items():
    if not k.lower().startswith("0x"): continue
    try: a=int(k.lower().removeprefix("0x"),16)
    except ValueError: continue
    u=unit_of(a)
    if u is None: continue
    d=per[u]; d["tot"]+=1; d["lo"]=min(d["lo"],a); d["hi"]=max(d["hi"],a)
    if v in defined: d["comp"]+=1
    else: d["uncomp"]+=1

# rank pinned-but-uncompiled units (uncomp dominant, not wired)
def src(mod,root):
    base=mod[:-4] if mod.endswith(".cpp") else (mod[:-2] if mod.endswith(".c") else mod)
    r=glob.glob(f"{root}/**/{os.path.basename(base)}.cpp",recursive=True)
    if not r: r=glob.glob(f"{root}/**/{os.path.basename(base)}.c",recursive=True)
    return bool(r)

rows=[]
for u,d in per.items():
    base=os.path.basename(u)
    w=base in wired
    rows.append((u,base,d["tot"],d["comp"],d["uncomp"],w,d["lo"],d["hi"]))

# unwired units with pinned functions (target ready), ranked by uncomp count
unwired=[r for r in rows if not r[5] and r[4]>0]
unwired.sort(key=lambda r:-r[4])
print(f"=== units with PINNED-but-UNCOMPILED functions, NOT wired (target obj ready) ===")
print(f"{'unit':40} {'tot':>4} {'unc':>4}  src(wii/dc3)  span")
for u,base,tot,comp,unc,w,lo,hi in unwired[:45]:
    wii='wii' if src(base,'/home/free/code/milohax/rb3/src') else ''
    dc3='dc3' if src(base,'/home/free/code/milohax/dc3-decomp/src') else ''
    ours='ours' if src(base,str(ROOT/'src')) else ''
    s=' '.join(x for x in (ours,wii,dc3) if x) or 'NONE'
    print(f"{u[:40]:40} {tot:>4} {unc:>4}  {s:14} 0x{lo:08X}")
print(f"\ntotal pinned units: {len(rows)}, unwired-with-pinned: {len(unwired)}",file=sys.stderr)
json.dump([{"unit":r[0],"tot":r[2],"comp":r[3],"uncomp":r[4],"wired":r[5],
            "lo":f"0x{r[6]:08X}","hi":f"0x{r[7]:08X}"} for r in rows],
          open(ROOT/"scripts/_census_byunit.json","w"),indent=1)
