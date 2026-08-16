#!/usr/bin/env python3
"""MAPID-1: exact byte forecast for naming 0x827bcd38.

Only a row currently at fuzzy==100 can LOSE bytes by gaining a charge
(matched_code is all-or-nothing per row). So intersect the 7 disagreeing
symbols with report.json's fuzzy==100 set and sum their sizes.
"""
import json, sys
from pathlib import Path

wt = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(wt / "tools"))
from alias_forgiveness_audit import Sides                                   # noqa

S = Sides(wt)
TARGET = "?MemAlloc@@YAPAXHH@Z"
dis = []
for n, (raw, rel) in sorted(S.traw.items()):
    for o, nm, _t in rel:
        if nm != "fn_827BCD38":
            continue
        ob = S.oraw.get(n)
        if ob is None:
            continue
        ours = {oo: onm for oo, onm, _ in ob[1]}.get(o)
        if ours and not (ours == TARGET or S.equiv(ours, TARGET)):
            dis.append((n, o, ours))

d = json.loads((wt / "build/45410914/report.json").read_text())
at100, sizes = {}, {}
for u in d["units"]:
    for f in u.get("functions", []):
        nm = f.get("name")
        if not nm:
            continue
        fz = float(f.get("fuzzy_match_percent", 0) or 0)
        sz = int(f.get("size", 0) or 0)
        sizes.setdefault(nm, sz)
        if fz == 100.0:
            at100[nm] = (u["name"], sz)

print("the %d disagreeing rows, and whether naming can cost them:" % len(dis))
loss = 0
for n, o, ours in dis:
    hit = at100.get(n)
    if hit:
        loss += hit[1]
        print("  AT 100  %7d B  %-56s @0x%-4x we call %s" % (hit[1], n[:56], o, ours[:34]))
    else:
        print("  below   %7s    %-56s @0x%-4x we call %s"
              % (sizes.get(n, "?"), n[:56], o, ours[:34]))
print("\nFORECAST worst case Δmatched_code = -%d B  (only fuzzy==100 rows can fall)" % loss)
print("plus: ?MemAlloc@@YAPAXHH@Z becomes pairable (ours 20 B stub vs retail 644 B => ~0%%, no byte gain)")
