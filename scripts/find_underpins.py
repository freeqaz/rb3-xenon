#!/usr/bin/env python3
"""Find candidate UNDER-PINNED .text ranges: a pinned TU whose range ends
exactly at a function start, where that function (and the contiguous run after
it) is NOT owned by any other pinned TU — i.e. it sits in an unowned auto_ blob
and may belong to THIS TU but was left out by an old-jeff pin.

Deterministic pre-filter only (ownership + contiguous-unowned run length). Real
membership must be confirmed empirically (extend pin, rebuild, the captured fns
match) — this just ranks where to look.
"""
import re, sys, json, bisect
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
SYM = ROOT/"config/45410914/symbols.txt"
SPLITS = ROOT/"config/45410914/splits.txt"

# function table (addr -> size) from symbols.txt
funcs=[]
for l in SYM.read_text().splitlines():
    m=re.match(r"^fn_[0-9A-Fa-f]+ = \.text:0x([0-9A-Fa-f]+);.*?\btype:function\b.*?size:0x([0-9A-Fa-f]+)",l)
    if m: funcs.append((int(m.group(1),16),int(m.group(2),16)))
funcs.sort(); faddr=[f[0] for f in funcs]; fsize={a:s for a,s in funcs}

# all pinned .text ranges
ranges=[]; tu_ranges={}; cur=None
for l in SPLITS.read_text().splitlines():
    h=re.match(r"^(\S+\.(?:cpp|c|cc)):\s*$",l)
    if h: cur=h.group(1); continue
    t=re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)",l)
    if t and cur:
        s,e=int(t.group(1),16),int(t.group(2),16)
        ranges.append((s,e,cur)); tu_ranges.setdefault(cur,[]).append((s,e))
ranges.sort(); rstart=[r[0] for r in ranges]

def owned(addr):
    """is addr inside any pinned range?"""
    i=bisect.bisect_right(rstart,addr)-1
    while i>=0 and ranges[i][0]>addr-0x20000:  # scan back a little for overlaps
        s,e,tu=ranges[i]
        if s<=addr<e: return tu
        i-=1
    return None

def next_fn(addr):
    i=bisect.bisect_left(faddr,addr)
    return funcs[i] if i<len(faddr) else None

cands=[]
for s,e,tu in ranges:
    f=next_fn(e)
    if not f or f[0]!=e:  # range end must abut a function start
        continue
    if owned(e):          # next fn already owned by another pin -> correct boundary
        continue
    # measure the contiguous unowned run starting at e.
    # Split it into REAL functions (>0x2C) vs ICF-stub thunks (<=0x2C): a run
    # made entirely of <=44B stubs is the ICF-fold farm (extending a pin into it
    # manufactures FAKE byte-identical matches, not real ones — see memory /
    # tools/icf_alias_check.py). Only real functions make an honest under-pin.
    STUB=0x2C
    run_fns=0; run_bytes=0; real_fns=0; real_bytes=0; a=e
    while True:
        nf=next_fn(a)
        if not nf or nf[0]!=a: break
        if owned(a): break
        sz=fsize.get(a,0)
        if sz==0: break
        run_fns+=1; run_bytes+=sz
        if sz>STUB: real_fns+=1; real_bytes+=sz
        a=nf[0]+sz
    if real_fns>0:
        cands.append({"tu":tu,"end":e,"run_fns":run_fns,"run_bytes":run_bytes,
                      "real_fns":real_fns,"real_bytes":real_bytes,"run_end":a})
cands.sort(key=lambda x:-x["real_bytes"])
print(f"{len(ranges)} pinned ranges; {len(cands)} end at an unowned run with >=1 REAL (>44B) function")
print("(stub-only runs are dropped: extending into an ICF-stub farm = fake matches)\n")
print(f"{'TU':<44}{'end':>11}{'real_fns':>9}{'real_bytes':>11}{'stubs':>7}")
print("-"*82)
for c in cands[:45]:
    stubs=c["run_fns"]-c["real_fns"]
    print(f"{c['tu']:<44}0x{c['end']:08X}{c['real_fns']:>9}0x{c['real_bytes']:>8X}{stubs:>7}")
json.dump(cands,open("/tmp/underpins.json","w"),indent=1)
print(f"\n[json] /tmp/underpins.json ({len(cands)} real-function under-pin candidates)")
