#!/usr/bin/env python3
"""MAPID-1: forecast the cost of NAMING 0x827bcd38 = ?MemAlloc@@YAPAXHH@Z.

name_check FORGIVES placeholder target names, so every retail `bl fn_827BCD38`
site is currently UNCHARGED. Naming it converts each into a CHECKED site:
  our callee == ?MemAlloc@@YAPAXHH@Z (or equiv)  -> still 0
  our callee is anything else                    -> a NEW charge

So walk every retail body that calls fn_827BCD38, find OUR body for the same
symbol, read what WE call at the same offset, and tally agree vs disagree. This
is the (b) economics of the brief made numeric BEFORE measuring.
"""
import sys
from pathlib import Path

wt = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(wt / "tools"))
from alias_forgiveness_audit import Sides                                   # noqa

S = Sides(wt)
assert sum(1 for n in S.traw if n.startswith("?")) > 1000, "PRE-RENAMER"
TARGET = "?MemAlloc@@YAPAXHH@Z"

agree = disagree = nobody = 0
dis = []
for n, (raw, rel) in sorted(S.traw.items()):
    for o, nm, _t in rel:
        if nm != "fn_827BCD38":
            continue
        ob = S.oraw.get(n)
        if ob is None:
            nobody += 1
            continue
        om = {oo: onm for oo, onm, _ in ob[1]}
        ours = om.get(o)
        if ours is None:
            nobody += 1
        elif ours == TARGET or S.equiv(ours, TARGET):
            agree += 1
        else:
            disagree += 1
            dis.append((n, o, ours))

print("retail sites calling fn_827BCD38, matched against OUR body at the same offset:")
print("  AGREE    (we call %s or equiv) : %d   -> stay at 0 charge" % (TARGET, agree))
print("  DISAGREE (we call something else)      : %d   -> NEW charges" % disagree)
print("  NO BASE  (we do not compile that body) : %d   -> no site to charge" % nobody)
print("\nthe disagreeing sites (what we call instead):")
from collections import Counter
c = Counter(x[2] for x in dis)
for nm, k in c.most_common(20):
    print("   %4d x  %s" % (k, nm[:80]))
print("\nsample rows:")
for n, o, ours in dis[:12]:
    print("   %-62s @0x%-4x -> %s" % (n[:62], o, ours[:50]))
