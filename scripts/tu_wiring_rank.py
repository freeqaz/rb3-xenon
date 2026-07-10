#!/usr/bin/env python3
"""Per-owning-module census: map every game/engine map entry (addr<0x82800000)
to an owning class/module, count total vs orphan, check wired + source oracle,
rank unwired-with-source by function count."""
import glob
import json
import os
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

ROOT = Path("/home/free/tmp/wt-tucensus")
BOUND = 0x82800000

# ---- reuse census: pinned ranges + defined syms ----
def parse_defined_syms(data):
    if len(data) < 20: return []
    sym_off = struct.unpack_from("<I", data, 8)[0]
    n = struct.unpack_from("<I", data, 12)[0]
    if not sym_off or not n: return []
    st = sym_off + n * 18; out = []; i = 0
    while i < n:
        eo = sym_off + i * 18
        if eo + 18 > len(data): break
        nb = data[eo:eo+8]
        if nb[:4] == b"\x00\x00\x00\x00":
            so = struct.unpack_from("<I", nb, 4)[0]; ao = st + so
            try:
                end = data.index(b"\x00", ao); name = data[ao:end].decode("ascii","replace")
            except ValueError: name = ""
        else:
            name = nb.split(b"\x00")[0].decode("ascii","replace")
        secn = struct.unpack_from("<h", data, eo+12)[0]; aux = data[eo+17]
        if secn > 0: out.append(name)
        i += 1 + aux
    return out

defined = set()
for f in glob.glob(str(ROOT/"build/45410914/src/**/*.obj"), recursive=True):
    try: defined.update(parse_defined_syms(open(f,"rb").read()))
    except Exception: pass

ranges = []
for line in (ROOT/"config/45410914/splits.txt").read_text().splitlines():
    m = re.search(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)", line)
    if m: ranges.append((int(m.group(1),16), int(m.group(2),16)))
ranges.sort()
def in_pinned(a):
    lo,hi=0,len(ranges)
    while lo<hi:
        mid=(lo+hi)//2; s,e=ranges[mid]
        if a<s: hi=mid
        elif a>=e: lo=mid+1
        else: return True
    return False

# ---- owning module extraction ----
def owning_module(mangled):
    """Best-effort: return the primary class name that owns this symbol."""
    n = mangled
    if n.startswith("__unwind") or n.startswith("$"): return None  # funclet residue
    if not n.startswith("?"):
        # C-style / global function
        return "<Cfunc>"
    body = n[1:]
    # special names ??x
    if body.startswith("?"):
        body = body[1:]
        # strip leading op token: digits or _<L>
        body = re.sub(r"^(_?[A-Za-z0-9]+?)(?=[A-Z_])", "", body, count=0)
    idx = body.find("@@")
    if idx < 0:
        idx = body.find("@")
        if idx<0: return "<global>"
    scope = body[:idx]
    parts = [p for p in scope.split("@") if p]
    if not parts: return "<global>"
    # drop the leaf member/op name (first part) -> class chain is the rest,
    # outermost last. Owning class = the OUTERMOST real named scope.
    chain = parts[1:] if len(parts)>1 else parts
    # filter template/STL noise scopes; pick first non-template, non-anon
    for p in chain:
        if p.startswith("?$") or p.startswith("_") or "$" in p: continue
        if p.startswith("?A0x"): continue
        return p
    # fall back to last
    for p in reversed(chain):
        if p.startswith("?A0x"): continue
        return p
    return chain[-1] if chain else "<global>"

# ---- classify ----
raw = json.loads((ROOT/"scripts/target_symbol_map.json").read_text())
mods = defaultdict(lambda: {"total":0,"orphan":0,"pinned":0,"compiled":0,"addrs":[]})
for k,v in raw.items():
    if not k.lower().startswith("0x"): continue
    try: a=int(k.lower().removeprefix("0x"),16)
    except ValueError: continue
    if a>=BOUND: continue
    mod = owning_module(v)
    if mod is None:
        mod="__funclet"
    d = mods[mod]; d["total"]+=1; d["addrs"].append(a)
    if in_pinned(a): d["pinned"]+=1
    elif v in defined: d["compiled"]+=1
    else: d["orphan"]+=1

# ---- source availability ----
oj = (ROOT/"config/45410914/objects.json").read_text()
wired = {os.path.basename(x) for x in re.findall(r'"([^"]+\.cpp)"', oj)}
def src_exists(mod, root):
    # try exact and case-insensitive
    for pat in (f"{mod}.cpp",):
        r = glob.glob(f"{root}/**/{pat}", recursive=True)
        if r: return r[0]
    # case-insensitive
    low=mod.lower()
    for f in glob.glob(f"{root}/**/*.cpp", recursive=True):
        if os.path.basename(f).lower()==low+".cpp": return f
    return ""

rows=[]
for mod,d in mods.items():
    if mod.startswith("<") or mod=="__funclet": continue
    if d["orphan"]==0 and d["pinned"]==0 and d["compiled"]==0: continue
    w = (mod+".cpp") in wired
    ours = src_exists(mod,str(ROOT/"src"))
    wii = src_exists(mod,"/home/free/code/milohax/rb3/src")
    dc3 = src_exists(mod,"/home/free/code/milohax/dc3-decomp/src")
    rows.append((mod,d["total"],d["orphan"],d["pinned"],d["compiled"],w,bool(ours),bool(wii),bool(dc3),min(d["addrs"]),max(d["addrs"])))

# unwired only, ranked by total fn count
unwired = [r for r in rows if not r[5]]
unwired.sort(key=lambda r:-r[1])
print("=== UNWIRED modules (game/engine, addr<0x82800000), ranked by total map entries ===")
print(f"{'module':28} {'tot':>4} {'orph':>4} {'pin':>4} {'src?':>13}  span")
for r in unwired[:50]:
    mod,tot,orph,pin,comp,w,ours,wii,dc3,lo,hi = r
    srcs = ('ours ' if ours else '') + ('wii ' if wii else '') + ('dc3' if dc3 else '')
    srcs = srcs or 'NONE'
    print(f"{mod:28} {tot:>4} {orph:>4} {pin:>4} {srcs:>13}  0x{lo:08X}-0x{hi:08X}")

json.dump([{"mod":r[0],"total":r[1],"orphan":r[2],"pinned":r[3],"compiled":r[4],
            "wired":r[5],"ours":r[6],"wii":r[7],"dc3":r[8],
            "lo":f"0x{r[9]:08X}","hi":f"0x{r[10]:08X}"} for r in rows],
          open(ROOT/"scripts/_census_modules.json","w"), indent=1)
print(f"\ntotal modules: {len(rows)}, unwired: {len(unwired)}", file=sys.stderr)
