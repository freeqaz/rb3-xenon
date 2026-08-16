#!/usr/bin/env python3
"""MAPID-1: price the 14-membership withdrawal with report-only legs.

Includes the anti-vacuity check a 0.00% result demands: how many of the stripped
memberships' groups had rows present AND at fuzzy==100 in the FULL leg (i.e. how
many chances the ablation had to find a hit), and whether the leg dropped rows at
all. A leg that drops nothing proves nothing.
"""
import json, os, sys
from pathlib import Path

wt = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(wt / "tools"))
from alias_class_ablate import leg                                          # noqa

dec = json.load(open(os.path.expanduser(sys.argv[2])))["decisive"]
sel = {(d["i"], d["folded"]) for d in dec}
print("withdrawal set: %d memberships over %d groups" % (len(sel), len({i for i, _ in sel})))


def strip(doc):
    for i, g in enumerate(doc["groups"]):
        g["folded"] = [x for x in g.get("folded", []) if (i, x) not in sel]


base_rows, base_code, base_fns, base_pct = leg(wt, lambda d: None, "FULL")
print("FULL      matched_code %d (%.6f%%)  matched_functions %d  rows@100 %d"
      % (base_code, base_pct, base_fns, len(base_rows)))

rows, code, fns, pct = leg(wt, strip, "WITHDRAWN")
print("WITHDRAWN matched_code %d (%.6f%%)  matched_functions %d  rows@100 %d"
      % (code, pct, fns, len(rows)))
print("\nDELTA  matched_code %+d B   %+.6f pp   matched_functions %+d   rows@100 %+d"
      % (code - base_code, pct - base_pct, fns - base_fns, len(rows) - len(base_rows)))

fell = [(k, sz) for k, sz in base_rows.items() if k not in rows]
print("rows that FELL: %d totalling %d B  (reconciles: %s)"
      % (len(fell), sum(sz for _, sz in fell),
         sum(sz for _, sz in fell) == base_code - code))
for k, sz in sorted(fell, key=lambda x: -x[1])[:15]:
    print("   %7d B  %s :: %s" % (sz, k[0], k[1][:78]))
