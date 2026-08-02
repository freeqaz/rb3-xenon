#!/usr/bin/env python3
"""CO-4 TASK A first cut.

For each census row where base >= 2*tgt, ask a MAP-INDEPENDENT question:
what does retail's .pdata say the function at the mapped address actually spans?

  pdata ~= base (ours)  -> the target extent is TRUNCATED by an intervening map
                           row -> MAP DEFECT (spurious split), fixable by DELETE
  pdata ~= tgt          -> retail's function really is that small -> our code is
                           genuinely 2-3x bigger -> SOURCE divergence or MISPAIR
  A not a pdata start   -> A is INSIDE a retail function -> bogus row
"""
import bisect, json, os, re, sys
ROOT = sys.argv[1] if len(sys.argv) > 1 else "/home/free/tmp/laneCO4/wt"
sys.path.insert(0, os.path.join(ROOT, "tools"))
import va_size

data, imgbase, secs = va_size.load(os.path.join(ROOT, va_size.PE))
starts = va_size.pdata_starts(data, secs)
SS = set(starts)

def pd(va):
    i = bisect.bisect_left(starts, va)
    if i < len(starts) and starts[i] == va:
        return starts[i+1] - va if i+1 < len(starts) else None
    return None

def enclosing(va):
    i = bisect.bisect_right(starts, va) - 1
    if i < 0: return None
    return starts[i]

MAP = json.load(open(os.path.join(ROOT, "scripts/target_symbol_map.json")))
byname = {}
for a, n in MAP.items():
    if not a.startswith("0x"): continue
    byname.setdefault(n, []).append(int(a, 16))

# splits.txt -> unit -> [(start,end)] for .text
pins = {}
cur = None
for line in open(os.path.join(ROOT, "config/45410914/splits.txt")):
    s = line.rstrip("\n")
    if not s.strip() or s.lstrip().startswith("#"): continue
    if not s[0].isspace():
        cur = s.split(":")[0].strip(); continue
    m = re.match(r"\s*\.text\s+start:(0x[0-9a-fA-F]+)\s+end:(0x[0-9a-fA-F]+)", s)
    if m and cur:
        pins.setdefault(cur, []).append((int(m.group(1),16), int(m.group(2),16)))

# unit name in report.json is like 'default/UISlider'; splits key is the source path
UNITSRC = {}
cfg = json.load(open(os.path.join(ROOT, "objdiff.json")))
for u in cfg["units"]:
    UNITSRC[u["name"]] = u.get("metadata", {}).get("source_path") or u.get("source_path")

rows = [r for r in json.load(open(os.path.expanduser("~/tmp/laneCO4/census.json")))
        if r["tgt"] > 0 and r["base"] >= 2*r["tgt"]]

out = []
for r in rows:
    src = UNITSRC.get(r["unit"])
    import os as _o
    prng = pins.get(_o.path.basename(src) if src else "", [])
    cands = byname.get(r["name"], [])
    inpin = [a for a in cands if any(lo <= a < hi for lo, hi in prng)]
    pick = inpin[0] if len(inpin) == 1 else (inpin[0] if inpin else None)
    rec = dict(r); rec.pop("target_path",None); rec.pop("base_path",None)
    rec.update(n_cands=len(cands), n_in_pin=len(inpin), src=src,
               addr=hex(pick) if pick else None)
    if pick is not None:
        rec["is_pdata_start"] = pick in SS
        rec["pdata_size"] = pd(pick)
        rec["enclosing"] = hex(enclosing(pick)) if enclosing(pick) else None
    out.append(rec)

json.dump(out, open(os.path.expanduser("~/tmp/laneCO4/probe.json"),"w"), indent=1)

def cls(rec):
    if rec.get("addr") is None: return "NO_ADDR"
    if not rec.get("is_pdata_start"): return "NOT_PDATA_START"
    ps = rec.get("pdata_size")
    if ps is None: return "PDATA_LAST"
    if abs(ps - rec["base"]) <= 16: return "PDATA~OURS(truncation)"
    if abs(ps - rec["tgt"]) <= 16: return "PDATA~TGT(real small)"
    return f"PDATA_OTHER"
from collections import Counter
c = Counter(cls(x) for x in out)
print("rows:", len(out)); print(c)
print()
for x in sorted(out, key=lambda z: -(z["base"]-z["tgt"])):
    print(f"{cls(x):24s} tgt={x['tgt']:6d} ours={x['base']:6d} pdata={x.get('pdata_size')} "
          f"nc={x['n_cands']}/{x['n_in_pin']} {x.get('addr')} {x['name'][:62]}")
