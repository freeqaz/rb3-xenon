#!/usr/bin/env python3
"""Lane CZ-3: deflate SRC_imperfect_T from PAIRS to FUNCTIONS, and test whether
the +8 size bucket is the known EH-PREFIX artifact rather than a body defect.

Two things the pair count hides:
  * a pair is (T, B). One bad T called with several different B's is ONE
    function of work counted many times. Memory is explicit that SITE COUNT IS
    NOT DEFECT COUNT; the same applies one level up, pair count is not either.
  * `length_differs` fires on pd(retail .pdata) != len(our COFF body). Lane CM-3
    (5f05def4) established that MSVC emits an 8-byte EH PREFIX BEFORE the entry
    and our extents over-run it. If that artifact is present here, it shows up as
    a SPIKE at exactly +8 (and +4/-4 for the other known extent deltas), NOT as a
    smooth distribution.

FALSIFIABLE: if +8 is the EH artifact, rows at +8 should be EH-bearing
(their obj section carries an __unwind$ / .xdata companion) at a rate far above
rows at other nonzero deltas. If the two rates are equal, the +8 spike is NOT the
EH artifact and I must say so.

Read-only.
"""
import collections
import json
import os
import sys
from pathlib import Path

ROOT = os.environ.get("CZ3_ROOT", ".")
sys.path.insert(0, os.path.join(ROOT, "tools"))

rows = json.load(open(sys.argv[1]))["rows"]

print("=== PAIRS -> FUNCTIONS deflation ===")
print("pairs                        %5d" % len(rows))
print("distinct T (functions)       %5d" % len({r["T"] for r in rows}))
print("distinct (T, unit)           %5d"
      % len({(r["T"], r["rep"][0][0] if r["rep"] else None) for r in rows}))

buck = collections.defaultdict(set)
for r in rows:
    ent = r["rep"]
    if not ent:
        k = "T_NOT_TRACKED"
    else:
        b = max(e[1] for e in ent)
        k = "MAP_PIN_contradiction(mpn=100)" if b == 100.0 else (
            "workable_50_99" if b >= 50 else "workable_lt50")
    buck[k].add(r["T"])
print("\nDISTINCT FUNCTIONS per bucket:")
for k, v in sorted(buck.items(), key=lambda z: -len(z[1])):
    print("   %-34s %4d" % (k, len(v)))

work = {r["T"] for r in rows if r["rep"] and max(e[1] for e in r["rep"]) < 100.0}
print("\nWORKABLE distinct functions: %d" % len(work))

uc = collections.Counter()
ucf = collections.defaultdict(set)
for r in rows:
    if r["rep"] and max(e[1] for e in r["rep"]) < 100.0:
        uc[r["rep"][0][0]] += 1
        ucf[r["rep"][0][0]].add(r["T"])
print("\n%-46s %6s %6s" % ("unit", "pairs", "FNS"))
for k, n in uc.most_common(12):
    print("   %-46s %5d  %5d" % (k, n, len(ucf[k])))

print("\n=== the MemMgr concentration, spelled out ===")
mm = [r for r in rows if r["rep"] and r["rep"][0][0] == "default/MemMgr"]
print("MemMgr pairs %d  distinct T %d" % (len(mm), len({r["T"] for r in mm})))
for t in sorted({r["T"] for r in mm}):
    sub = [r for r in mm if r["T"] == t]
    e = sub[0]["rep"][0]
    print("   %-46s pairs=%3d pd=%s ours=%s mpn=%.2f"
          % (t[:46], len(sub), sub[0]["pd"], sub[0]["ours"], e[1]))

# ---- +8 EH-prefix hypothesis -----------------------------------------------
print("\n=== is the +8 bucket the EH-PREFIX artifact? ===")
from coff_bodies_ext import function_bodies_ext                    # noqa: E402
import glob
ehnames = set()
allnames = set()
for p in glob.glob(ROOT + "/build/45410914/src/**/*.obj", recursive=True):
    try:
        bl = list(function_bodies_ext(Path(p)))
    except Exception:
        continue
    for nm, _raw, _rl, entry in bl:
        allnames.add(nm)
        if entry:                    # nonzero entry offset == EH prefix present
            ehnames.add(nm)
print("our objs: %d function bodies, %d with a NONZERO entry offset (EH prefix)"
      % (len(allnames), len(ehnames)))

by = collections.defaultdict(set)
for r in rows:
    if r["pd"] is None or r["ours"] is None:
        continue
    by[r["ours"] - r["pd"]].add(r["T"])
def rate(names):
    if not names:
        return None
    return 100.0 * sum(1 for n in names if n in ehnames) / len(names)
plus8 = by.get(8, set())
other = set()
for d, ns in by.items():
    if d not in (0, 8):
        other |= ns
zero = by.get(0, set())
print("   delta=+8   fns %3d   EH-prefix rate %s" % (len(plus8), rate(plus8)))
print("   delta!=0,8 fns %3d   EH-prefix rate %s" % (len(other), rate(other)))
print("   delta=0    fns %3d   EH-prefix rate %s  (control: byte-DIFF rows)"
      % (len(zero), rate(zero)))
print("   whole-tree background rate %.1f%%" % (100.0 * len(ehnames) / max(1, len(allnames))))
for t in sorted(plus8):
    print("      +8: %s  EH=%s" % (t[:80], t in ehnames))
