#!/usr/bin/env python3
"""STATIC PRE-FLIGHT: replicate dtk's split validation against the EMITTED
tu5_valayer/{splits.txt, symbols.txt} — independent of the generator's state.
Proves the 5 gates dtk enforces before we spend a build."""
import re, bisect, json, os, argparse
from collections import defaultdict
_ap = argparse.ArgumentParser()
_ap.add_argument("--base", default=os.environ.get("TU5_BASE"))
_args, _ = _ap.parse_known_args()
_BASE = _args.base or (os.path.dirname(os.path.abspath(__file__)) + "/")
if not _BASE.endswith("/"): _BASE += "/"
_WT = os.path.abspath(os.path.join(_BASE, "..", "..")) + "/"
OUT = _BASE + "tu5_valayer/"
FROZ = _BASE + "valayer_baseline_main/"
def h(s): return int(s, 16)
TL, TH = 0x82270000, 0x82270000 + 0x9dce3c

# ---- read emitted splits .text ranges (incl rename:.text$xx) ----
pin_re = re.compile(r'^(\s+)\.text\s+start:0x([0-9A-Fa-f]+) end:0x([0-9A-Fa-f]+)(\s+rename:\S+)?\s*$')
uhdr = re.compile(r'^(\S.*):\s*$')
ranges = []   # (start, end, unit, rename)
cur = None
for line in open(OUT+"splits.txt"):
    hm = uhdr.match(line)
    if hm and 'start:' not in line and 'type:' not in line and hm.group(1) != "Sections":
        cur = hm.group(1); continue
    pm = pin_re.match(line)
    if pm:
        ranges.append((h(pm.group(2)), h(pm.group(3)), cur, (pm.group(4) or "").strip()))

# ---- read emitted symbols.txt sized .text symbols ----
sym_re = re.compile(r'^(\S+) = \.text:0x([0-9A-Fa-f]+); // type:(\w+) size:0x([0-9A-Fa-f]+)')
sized = {}       # addr -> list of (name, size)   (to detect conflicting sizes)
for line in open(OUT+"symbols.txt"):
    m = sym_re.match(line)
    if m:
        sized.setdefault(h(m.group(2)), []).append((m.group(1), int(m.group(4), 16)))
sized_addrs = sorted(sized)

results = {}

# GATE 1: no overlaps of ANY class (plain .text + rename sub-splits)
ov = []
rs = sorted(ranges)
for i in range(len(rs)-1):
    s, e, u, rn = rs[i]; ns, ne, nu, nrn = rs[i+1]
    if e > ns:
        ov.append(f"{u}{rn} 0x{s:08X}-0x{e:08X}  vs  {nu}{nrn} 0x{ns:08X}-0x{ne:08X} (ovl {e-ns})")
results["gate1_overlaps"] = ov

# GATE 2: no split boundary (start or exclusive end) bisects a sized symbol.
# A boundary B bisects symbol [a, a+sz) iff a < B < a+sz.
def bisects(B):
    i = bisect.bisect_right(sized_addrs, B) - 1
    if i < 0: return None
    a = sized_addrs[i]
    sz = max(s for _, s in sized[a])
    if a < B < a + sz:
        return (a, sz, sized[a][0][0])
    return None
bis = []
for s, e, u, rn in ranges:
    for label, B in (("start", s), ("end", e)):
        hit = bisects(B)
        if hit:
            bis.append(f"{u}{rn} {label} 0x{B:08X} bisects {hit[2]} [0x{hit[0]:08X}..0x{hit[0]+hit[1]:08X})")
results["gate2_boundary_bisects_symbol"] = bis

# GATE 3: no address carries two conflicting sizes
conflict = []
for a, lst in sized.items():
    szs = set(s for _, s in lst)
    if len(szs) > 1:
        conflict.append(f"0x{a:08X} sizes={sorted(hex(x) for x in szs)} names={[n for n,_ in lst][:4]}")
results["gate3_conflicting_sizes"] = conflict

# GATE 4: all pins in TU5 .text bounds, non-degenerate
oob = [f"{u} 0x{s:08X}-0x{e:08X}" for s, e, u, rn in ranges if not (TL <= s < e <= TH)]
results["gate4_out_of_bounds"] = oob

# GATE 6: no split boundary bisects a real TU5 .pdata function (dtk's AUTHORITY).
import struct
_pd = open(_WT + "orig/45410914/band_tu5.exe","rb").read()[0x1f1600:0x1f1600+0x70e00]
PDATA = []
for i in range(len(_pd)//8):
    b, w = struct.unpack_from(">II", _pd, i*8)
    L = ((w >> 8) & 0x3FFFFF) * 4
    if TL <= b < TH and L > 0: PDATA.append((b, b+L))
PDATA.sort()
PBEG = [b for b, e in PDATA]
def pd_encl(va):
    i = bisect.bisect_right(PBEG, va) - 1
    if i >= 0 and PDATA[i][0] <= va < PDATA[i][1]: return PDATA[i]
    return None
pdbis = []
for s, e, u, rn in ranges:
    for label, B in (("start", s), ("end", e)):
        hit = pd_encl(B)
        if hit and hit[0] < B < hit[1]:
            pdbis.append(f"{u}{rn} {label} 0x{B:08X} inside .pdata fn [0x{hit[0]:08X}..0x{hit[1]:08X})")
results["gate6_boundary_inside_pdata_fn"] = pdbis

# GATE 5: unit parity vs baseline
def units(path):
    us = set();
    for line in open(path):
        m = uhdr.match(line)
        if m and 'start:' not in line and 'type:' not in line and m.group(1) != "Sections":
            us.add(m.group(1))
    return us
bu = units(FROZ+"splits.txt"); nu = units(OUT+"splits.txt")
results["gate5_units_lost"] = sorted(bu - nu)

print("=== STATIC PRE-FLIGHT (dtk split rules) ===")
labels = [
 ("GATE 1  overlaps (any class)",        "gate1_overlaps"),
 ("GATE 2  boundary bisects symbol",     "gate2_boundary_bisects_symbol"),
 ("GATE 3  conflicting-size addresses",  "gate3_conflicting_sizes"),
 ("GATE 4  out-of-bounds pins",          "gate4_out_of_bounds"),
 ("GATE 5  units lost",                  "gate5_units_lost"),
 ("GATE 6  boundary inside .pdata fn",   "gate6_boundary_inside_pdata_fn"),
]
allpass = True
for lbl, k in labels:
    n = len(results[k]); ok = (n == 0)
    allpass &= ok
    print(f"  {lbl:38s} {n:5d}  {'PASS' if ok else 'FAIL'}")
    for x in results[k][:8]:
        print(f"        - {x}")
print(f"  ranges={len(ranges)}  base_units={len(bu)} new_units={len(nu)}")
print(f"\n  ==> {'ALL PASS — GREEN' if allpass else 'FAILURES — RED'}")
json.dump({k: v for k, v in results.items()}, open(OUT+"P3_preflight.json", "w"), indent=1)
