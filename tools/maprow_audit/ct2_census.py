#!/usr/bin/env python3
"""Lane CT-2 step 1: reproduce CS-1's population, then settle the UPPERCASE question.

CS-1 reported 107 TARGET_UNNAMED. My brief says "some may be the uppercase-key
artifact -- CHECK FIRST, it is free". But cs1_adj.py:22-24 ALREADY case-folded
the map before calling adjudicate_strict, so I predict the artifact count under
the folded map is ZERO and the free win was already taken. Measure both legs.
"""
import json, sys, os, collections
WT = "/home/free/tmp/laneCT2/wt"
sys.path.insert(0, WT + "/tools"); sys.path.insert(0, WT + "/tools/maprow_audit")
os.chdir(WT)
import thunk_identity as T

img = T.Image("orig/45410914/band.exe")
raw = json.load(open("scripts/target_symbol_map.json"))

folded = {}
for k, v in raw.items():
    folded[k.lower() if k.startswith("0x") else k] = v
assert len(folded) == len(raw), "case-fold COLLIDED -- would be lossy"

bij = set(a.lower() for a in raw["_bijection_arbitrary"])

def is_thunkrow(a, n):
    return (isinstance(n, str) and T.thunk_kind(n) is not None
            and T.is_forwarder(img, int(a, 16)) is not None)

allrows = {a: n for a, n in folded.items()
           if isinstance(a, str) and a.startswith("0x") and isinstance(n, str)}
treat = {a: n for a, n in allrows.items() if a in bij and is_thunkrow(a, n)}

print("=== CS-1 POPULATION REPRODUCTION ===")
print("  _bijection_arbitrary listed :", len(raw["_bijection_arbitrary"]))
print("  ... surviving in map        :", len(bij & set(allrows)))
print("  ... AND thunk+forwarder     :", len(treat), "(CS-1: 602)")

# --- Leg 1: FOLDED map (what CS-1 actually ran) --------------------------
rec_f = {a: T.adjudicate_strict(img, folded, a, n) for a, n in treat.items()}
cf = collections.Counter(r["verdict"] for r in rec_f.values())
print("\n  folded-map verdicts :", dict(cf))
tu_folded = {a for a, r in rec_f.items() if r["verdict"] == "TARGET_UNNAMED"}
print("  TARGET_UNNAMED (folded) :", len(tu_folded), "(CS-1 reported 107)")

# --- Leg 2: RAW map (what an unfixed dethunk_named would have seen) ------
rec_r = {a: T.adjudicate_strict(img, raw, a, n) for a, n in treat.items()}
tu_raw = {a for a, r in rec_r.items() if r["verdict"] == "TARGET_UNNAMED"}
print("  TARGET_UNNAMED (raw)    :", len(tu_raw))
print("  => artifact rows the fold already rescued:", len(tu_raw - tu_folded))
print("  => artifact rows STILL in CS-1's 107     :", len(tu_folded - tu_raw), "(expect 0)")

# how many of the 7 uppercase keys are branch targets of ANY thunk in the map?
up = [k for k in raw if k.startswith("0x") and k != k.lower()]
hits = collections.Counter()
for a in allrows:
    d = T.is_forwarder(img, int(a, 16))
    if d and ("0x%08x" % d[1]) in {u.lower() for u in up}:
        hits[("0x%08x" % d[1])] += 1
print("\n  uppercase keys that ARE a forwarder target somewhere in the map:", dict(hits))

json.dump(sorted(tu_folded), open("/home/free/tmp/laneCT2/tu107.json", "w"), indent=1)
json.dump({a: rec_f[a] for a in rec_f}, open("/home/free/tmp/laneCT2/treat602.json", "w"), indent=1)
print("\nwrote tu107.json (%d) and treat602.json (%d)" % (len(tu_folded), len(rec_f)))
