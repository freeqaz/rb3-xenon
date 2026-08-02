#!/usr/bin/env python3
"""Size the at-100 property-name DEFECT class, using tools/retail_props.py.

Companion to tools/retail_props.py.  Landed under tools/ (rather than left in a
lane scratch dir) because this is the THIRD rebuild of this instrument family --
CQ-3 wrote it, CR-4 rewrote it, CS-4 rewrote it again.

WHY THIS CLASS IS INVISIBLE TO THE DEFAULT RULER: property names are relocation
ARGS, and objdiff's report.rs hard-sets reloc args to None.  So a function that
enumerates the RIGHT number of properties in the RIGHT order with the WRONG
NAMES scores exactly 100.0.  This script is the only thing that sees them.

⚠ TWO ARTIFACT CLASSES THAT LOOK LIKE DEFECTS AND ARE NOT -- both were measured
on this corpus and both must be subtracted before quoting a number:
  * MSVC truncates `??_C@` literal SYMBOL NAMES at 32 characters, so our side
    reads `leaderboards/battle_rankrange/ge` where retail reads `...get`.  46
    rows.  Compare with a "32-char prefix" rule.
  * A slot can be a string LITERAL on our side and a global `const char*` on
    retail's (e.g. gNullStr); comparing literals-only then shows a spurious
    extra entry.  5 rows.
Before the ??_C@ escape decoder was written, a further 113 rows read as defects
purely because `/` is mangled `?1`.  Raw counts from this script are MEANINGLESS
without the classification step -- 113 + 46 + 5 = 164 artifacts vs 11 real.

MEASURED 2026-08-02 (lane CS-4, at 34621476 + CR-1/CR-4 landed):
  2,463 named retail symbols carry a property list; 2,157 of them are at 100.0.
  Of those, 11 (0.51%) have a retail list != our compiled list.  Roughly 4 look
  like map mispairs (fully disjoint lists) and 7 like real source defects,
  including a pure ORDER SWAP (TourPerformerLocal: retail custom,random --
  ours random,custom) which is the cleanest possible example of the class.
  731 UNNAMED retail fns carry a property list -- an identification backlog,
  not a defect (the anon naming gate keeps them at 0.0 regardless).

⚠ (c') matches an unnamed fn's list against source PROPSYNCS blocks.  Lists of
<3 properties are NOT trustworthy: `['label']` matches FlowLabel but the
function is provably UIListLabel::SyncProperty (settled by vtable slot), and
`['content']` matches two different unnamed fns.  Use >=3, or corroborate.

Original header:
CS-4: size the at-100 property-name defect class the CORRECTED index can see.

Three strata, measured not argued:
 (a) NAMED rows at 100.0 whose retail property list differs from the list our
     own compiled obj produces.  These are true at-100 defects: the default
     ruler masks relocation args, so a wrong property NAME scores 100.
 (b) NAMED rows at 100.0 whose retail list is NOT our class's list but IS some
     other class's -- the CR-4 UIListMesh shape (a mispair hiding at 100).
 (c) UNNAMED retail functions (`fn_XXXXXXXX`) that carry a property list.  These
     can never reach 100 (anon naming gate: objdiff cannot pair anon bodies), so
     they are an identification backlog, not a defect -- but each is a row the
     index can now name.  fn_8281FDD8 = UIListLabel::SyncProperty came out here.
"""
import json, os, re, sys, collections
WT = os.environ.get("WT", os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, os.path.join(WT, "tools"))
import retail_props as RP

ex = RP.Extractor(WT)
od = json.load(open(os.path.join(WT, "objdiff.json")))
units = {u["name"]: u for u in od["units"]}
rep = json.load(open(os.path.join(WT, "build/45410914/report.json")))

# ---- report.json: fuzzy_match_percent / match_percent_normalized are
# ---- TOP-LEVEL keys on the function record, NOT under `measures`.
pct = {}
sample = None
for u in rep["units"]:
    for f in u.get("functions", []):
        if sample is None:
            sample = f
        p = f.get("match_percent_normalized")
        if p is None:
            p = f.get("fuzzy_match_percent")
        pct[(u["name"], f["name"])] = (float(p) if p is not None else None, int(f["size"]))
print("SANITY -- one raw report.json function record (proves the key names):")
print("   keys:", sorted(sample.keys()))
print("   mpn=%r fuzzy=%r size=%r" % (sample.get("match_percent_normalized"),
                                      sample.get("fuzzy_match_percent"), sample.get("size")))
missing = sum(1 for v in pct.values() if v[0] is None)
print(f"   records with NEITHER percent key: {missing}/{len(pct)}"
      f"  ({'OK' if missing == 0 else 'REFUSE -- would silently read 0.000'})")

# ---- source PROPSYNCS table: class -> list  (for stratum b) --------------
src_lists = {}
for u in od["units"]:
    sp = (u.get("metadata") or {}).get("source_path")
    if not sp:
        continue
    try:
        txt = open(os.path.join(WT, sp)).read()
    except OSError:
        continue
    for m in re.finditer(r"BEGIN_PROPSYNCS\((\w+)\)", txt):
        j = txt.find("END_PROPSYNCS", m.end())
        lst = RP.SRC_RE.findall(txt[m.end():j])
        if lst:
            src_lists.setdefault("|".join(lst), []).append((m.group(1), sp))

a_defects, b_mispairs, unnamed = [], [], []
n_named_100 = n_named = 0
for u in od["units"]:
    tp, bp = u.get("target_path"), u.get("base_path")
    if not tp:
        continue
    p = os.path.join(WT, tp)
    if not os.path.exists(p):
        continue
    c = ex.obj(p)
    if c is None:
        continue
    for name, s in list(c.symbol_map.items()):
        if s.get("section", 0) <= 0 or name.startswith((".", "except_data", "__", "$")):
            continue
        try:
            t = ex.props(p, name)
        except Exception:
            continue
        if not t:
            continue
        if re.fullmatch(r"fn_[0-9A-Fa-f]{8}", name):
            unnamed.append((u["name"], name, t))
            continue
        n_named += 1
        pr = pct.get((u["name"], name))
        if pr is None or pr[0] is None:
            continue
        if pr[0] < 100.0:
            continue
        n_named_100 += 1
        b = ex.props(os.path.join(WT, bp), name) if bp else None
        if b is None:
            continue
        if t != b:
            a_defects.append((u["name"], name, t, b))
            key = "|".join(t)
            if key in src_lists:
                b_mispairs.append((u["name"], name, t, src_lists[key]))
    ex._objs.pop(p, None)

print(f"\nnamed retail symbols carrying a property list: {n_named}   of which at 100.0: {n_named_100}")
print(f"(a) at-100 rows where RETAIL list != OUR COMPILED list : {len(a_defects)}")
for r in a_defects[:15]:
    print(f"      {r[0]}  {r[1][:60]}\n         retail={r[2]}\n         ours  ={r[3]}")
print(f"(b) ... of those, retail's list IS another class's PROPSYNCS: {len(b_mispairs)}")
for r in b_mispairs[:15]:
    print(f"      {r[0]}  {r[1][:60]}  -> {r[3]}")
print(f"\n(c) UNNAMED retail fns carrying a property list: {len(unnamed)}  (identification backlog)")
byunit = collections.Counter(x[0] for x in unnamed)
for k, v in byunit.most_common(20):
    print(f"      {v:4d}  {k}")
json.dump({"a": a_defects, "b": b_mispairs, "c": unnamed},
          open(os.environ.get("CS4_OUT", "defectclass.json"), "w"), indent=1)

# ---- can an unnamed fn's list be matched to exactly one source class? ----
solved = []
for unit, name, t in unnamed:
    key = "|".join(t)
    hits = src_lists.get(key, [])
    if len(hits) == 1:
        solved.append((unit, name, t, hits[0]))
strong = [r for r in solved if len(r[2]) >= 3]
weak = [r for r in solved if len(r[2]) < 3]
print(f"\n(c') unnamed fns whose list matches EXACTLY ONE source PROPSYNCS block: {len(solved)}")
print(f"      STRONG (>=3 properties): {len(strong)}")
for r in strong:
    print(f"        {r[0]:34s} {r[1]}  n={len(r[2])}  ->  {r[3][0]}  ({r[3][1]})")
print(f"      WEAK (<3 properties -- NOT trustworthy on their own): {len(weak)}")
for r in weak:
    print(f"        {r[0]:34s} {r[1]}  {r[2]}  ->  {r[3][0]}")
json.dump(solved, open(os.environ.get("CS4_OUT2", "unnamed_solved.json"), "w"), indent=1)
