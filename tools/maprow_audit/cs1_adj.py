#!/usr/bin/env python3
"""Lane CS-1: dethunk-adjudicate the _bijection_arbitrary thunk set.

Population = VAs listed in _bijection_arbitrary that (a) still exist in the map,
(b) carry a compiler-generated thunk name, (c) whose RETAIL body is a forwarder.

The load-bearing control is NOT a shuffle -- it is the UNTREATED POPULATION:
the same instrument over thunks that are NOT in _bijection_arbitrary. If the
METHOD_DIFFERS rate is the same in both, "arbitrary by construction" is not
predictive of being wrong, and the worry is retired for the set.
"""
import json, sys, random, collections
sys.path.insert(0, "/home/free/tmp/laneCS1/wt/tools")
import thunk_identity as T

WT = "/home/free/tmp/laneCS1/wt"
img = T.Image(WT + "/orig/45410914/band.exe")
raw = json.load(open(WT + "/scripts/target_symbol_map.json"))

# --- case-fold address keys: dethunk_named looks up '0x%08x' (lowercase).
# 7 uppercase keys exist -> they would read TARGET_UNNAMED spuriously.
tmap = {}
for k, v in raw.items():
    tmap[k.lower() if k.startswith("0x") else k] = v

bij = set(a.lower() for a in raw["_bijection_arbitrary"])
icf = set(a.lower() for a in raw["_icf_arbitrary"])

def is_thunkrow(a, n):
    return (isinstance(n, str) and T.thunk_kind(n) is not None
            and T.is_forwarder(img, int(a, 16)) is not None)

allrows = {a: n for a, n in tmap.items()
           if isinstance(a, str) and a.startswith("0x") and isinstance(n, str)}
pop_treat = {a: n for a, n in allrows.items() if a in bij and is_thunkrow(a, n)}
pop_ctrl  = {a: n for a, n in allrows.items() if a not in bij and is_thunkrow(a, n)}

print("=== POPULATION ===")
print("  bijection listed            :", len(raw["_bijection_arbitrary"]))
print("  ... surviving in map        :", len(bij & set(allrows)))
print("  ... thunk-named             :", sum(1 for a in bij & set(allrows) if T.thunk_kind(allrows[a])))
print("  ... AND forwarder body      :", len(pop_treat), "  <-- TREATMENT")
print("  untreated thunk rows        :", len(pop_ctrl), "  <-- CONTROL")
print("  kinds(treat):", collections.Counter(T.thunk_kind(n) for n in pop_treat.values()).most_common())

def run(pop, tm):
    return [T.adjudicate_strict(img, tm, a, n) for a, n in pop.items()]

def tally(recs, label):
    c = collections.Counter(r["verdict"] for r in recs)
    res = c["AGREE"] + c["AGREE_METHOD"] + c["METHOD_DIFFERS"]
    rate = 100.0 * c["METHOD_DIFFERS"] / res if res else float("nan")
    print(f"  {label:28s} n={len(recs):5d}  AGREE={c['AGREE']:5d} AGREE_METHOD={c['AGREE_METHOD']:4d} "
          f"METHOD_DIFFERS={c['METHOD_DIFFERS']:4d} TARGET_UNNAMED={c['TARGET_UNNAMED']:4d} "
          f"NO_BRANCH={c['NO_BRANCH']:3d} UNPARSABLE={c['UNPARSABLE']:3d} | resolved={res} MD%={rate:.2f}")
    return c, rate

print("\n=== TREATMENT vs UNTREATED CONTROL ===")
rt = run(pop_treat, tmap); ct, rate_t = tally(rt, "bijection-arbitrary")
rc = run(pop_ctrl, tmap);  cc, rate_c = tally(rc, "NOT bijection (untreated)")
print(f"  enrichment (treat/ctrl MD%) = {rate_t/rate_c:.2f}x")

print("\n=== ANTI-VACUITY: deliberate mis-naming of the TREATMENT set ===")
print("  (shuffle names WITHIN shape group -- the exact swap objdiff cannot see)")
for seed in (11, 12, 13):
    rnd = random.Random(seed)
    groups = collections.defaultdict(list)
    for a, n in pop_treat.items():
        d = T.is_forwarder(img, int(a, 16))
        groups[(len(d[0]), T.thunk_kind(n))].append(a)
    mis = dict(tmap)
    for g, addrs in groups.items():
        names = [pop_treat[a] for a in addrs]
        rnd.shuffle(names)
        for a, n in zip(addrs, names):
            mis[a] = n
    mp = {a: mis[a] for a in pop_treat}
    tally(run(mp, mis), f"within-shape shuffle s={seed}")

json.dump(rt, open("/home/free/tmp/laneCS1/work/treat.json", "w"), indent=1)
json.dump(rc, open("/home/free/tmp/laneCS1/work/ctrl.json", "w"), indent=1)
