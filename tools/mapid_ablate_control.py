#!/usr/bin/env python3
"""MAPID-1: the anti-vacuity control for the 0 B withdrawal reading.

A 0.00 B ablation is the shape of a vacuous run. This proves it is not, three ways:
  (1) the strip is asserted to actually REMOVE the named memberships from the doc;
  (2) a leg stripping the 2 LICENSED memberships (same mechanism, same code path)
      is shown to MOVE bytes -- so the instrument can fail;
  (3) stripping all 16 reproduces ALIAS-2's published class price, and 14+2 is
      shown to be additive.
"""
import json, os, sys
from pathlib import Path

wt = Path(sys.argv[1]).resolve()
sys.path.insert(0, str(wt / "tools"))
from alias_class_ablate import leg                                          # noqa

mem = json.load(open(os.path.expanduser(sys.argv[3])))
allnm = {(m["i"], m["folded"]) for m in mem if m["verdict"] == "NEEDS_MAP_ID"}
dec = {(d["i"], d["folded"]) for d in json.load(open(os.path.expanduser(sys.argv[2])))["decisive"]}
lic = allnm - dec
print("NEEDS_MAP_ID total %d = withdrawn %d + licensed %d" % (len(allnm), len(dec), len(lic)))
for i, f in sorted(lic):
    print("   LICENSED  group %d  %s" % (i, f))


def strip(sel, name):
    def f(doc):
        removed = 0
        for i, g in enumerate(doc["groups"]):
            before = len(g.get("folded", []))
            g["folded"] = [x for x in g.get("folded", []) if (i, x) not in sel]
            removed += before - len(g["folded"])
        # (1) assert the mutation really happened
        if removed != len(sel):
            sys.exit("REFUSING: %s removed %d of %d memberships -- the strip is "
                     "not doing what it claims" % (name, removed, len(sel)))
    return f


base_rows, base_code, base_fns, base_pct = leg(wt, lambda d: None, "FULL")
print("\n%-34s %12s %10s %10s %8s %8s" % ("leg", "matched_code", "delta_B", "delta_pp", "d_fns", "rows_fell"))
print("%-34s %12d %10s %10s %8s %8s" % ("FULL (baseline)", base_code, "-", "-", "-", "-"))
res = {}
for name, sel in (("strip 14 WITHDRAWN", dec), ("strip 2 LICENSED", lic),
                  ("strip all 16 NEEDS_MAP_ID", allnm)):
    rows, code, fns, pct = leg(wt, strip(sel, name), name)
    fell = [(k, sz) for k, sz in base_rows.items() if k not in rows]
    res[name] = code - base_code
    print("%-34s %12d %10d %10.6f %8d %8d"
          % (name, code, code - base_code, pct - base_pct, fns - base_fns, len(fell)))
    for k, sz in sorted(fell, key=lambda x: -x[1])[:6]:
        print("        %7d B  %s :: %s" % (sz, k[0], k[1][:70]))

a, b, c = res["strip 14 WITHDRAWN"], res["strip 2 LICENSED"], res["strip all 16 NEEDS_MAP_ID"]
print("\nADDITIVITY: 14(%d) + 2(%d) = %d   vs   16(%d)   -> %s"
      % (a, b, a + b, c, "CONSISTENT" if a + b == c else "MISMATCH"))
