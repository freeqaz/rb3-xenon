#!/usr/bin/env python3
"""Lane DL-1: CALIBRATE the existing DC3<->RB3 BinDiff structural-match artifact.

The brief assumed this channel was unexamined.  It is not: tools/bindiff_match.json
(11,057 rows, 2026-05-26) is a Ghidra+BinDiff DC3-primary / RB3-secondary result
that has been in the tree since project inception.  What was NEVER done is the
positive-control calibration -- measuring how often its proposed dc3_name agrees
with a name we now independently KNOW to be right.

Ground truth did not exist in May.  It does now:
  TIER-GOLD  a named function sitting at match_percent_normalized == 100 in
             report.json.  Our source compiles to retail's bytes at that
             address, so the (address, name) pairing is byte-verified.
  TIER-MAP   any row of scripts/target_symbol_map.json (27,959).  These are
             HYPOTHESES, some known wrong (43% of one worklist were mispairs),
             so TIER-MAP is reported separately and never merged into GOLD.

CONTROLS (each able to fail; see docs/decomp/INSTRUMENT_DESIGN.md)
  C1 known-positive map parse -- hard assert ?Poll@Character@@UAAXXZ @0x82351090.
     Without it a broken parser yields 0 agreement, shaped like a decisive negative.
  C2 shuffle null -- permute dc3_name across rows and re-measure agreement.
     A comparator that "agrees" on shuffled data is comparing nothing.
  C3 size-matched null -- replace each dc3_name with a random DC3 name of the
     SAME body size.  Harder null than C2: rules out agreement arising from
     size coincidence alone.
  C4 discriminator test -- similarity/confidence distribution for AGREE vs
     DISAGREE.  If they coincide, `similarity` cannot be used as a classifier,
     which is the failure this project has hit three times.
"""
import json
import os
import random
import sys
from collections import Counter, defaultdict

REPO = "/home/free/code/milohax/rb3-xenon"
sys.path.insert(0, os.path.join(REPO, "tools"))
import dc3_map  # noqa: E402

OUT = "/home/free/tmp/laneDL1"


# --------------------------------------------------------------------------
# inputs
# --------------------------------------------------------------------------
def load_dc3():
    m = dc3_map.parse_map(
        "/home/free/code/milohax/dc3-decomp/orig/373307D9/ham_xbox_r.map")
    # C1: known-positive.  A silently-broken parse must not read as "no matches".
    kp = m.get("?Poll@Character@@UAAXXZ")
    assert kp and kp["addr"] == 0x82351090, (
        f"C1 FAILED: DC3 map parse did not recover the known positive "
        f"?Poll@Character@@UAAXXZ @0x82351090 (got {kp}). Every downstream zero "
        f"in this run would be an artifact.")
    by_addr = defaultdict(list)
    for name, e in m.items():
        by_addr[e["addr"]].append(name)
    print(f"[C1 PASS] DC3 map: {len(m)} .text symbols, "
          f"{len(by_addr)} distinct VAs (ICF folds names onto shared VAs)")
    return m, by_addr


def load_ground_truth():
    """rb3_va -> (name, tier).  GOLD wins over MAP."""
    tsm = json.load(open(os.path.join(REPO, "scripts/target_symbol_map.json")))
    tsm = {a: n for a, n in tsm.items()
           if a.startswith("0x") and isinstance(n, str)}
    name2va = {}
    for a, n in tsm.items():
        name2va.setdefault(n, int(a, 16))
    va2name_map = {int(a, 16): n for a, n in tsm.items()}

    rep = json.load(open(os.path.join(REPO, "build/45410914/report.json")))
    gold = {}
    for unit in rep["units"]:
        for fn in unit.get("functions") or []:
            n = fn.get("name", "")
            if n.startswith("fn_") or not n:
                continue
            if fn.get("match_percent_normalized") != 100.0:
                continue
            va = name2va.get(n)
            if va is not None:
                gold[va] = n
    gt = {va: (n, "MAP") for va, n in va2name_map.items()}
    for va, n in gold.items():
        gt[va] = (n, "GOLD")
    print(f"[gt] TIER-MAP {len(va2name_map)} rows; TIER-GOLD {len(gold)} "
          f"byte-verified named-at-100 rows")
    return gt


# --------------------------------------------------------------------------
# disagreement taxonomy -- a bare rate hides which KIND of wrong it is
# --------------------------------------------------------------------------
def split_sym(mangled):
    """(method, class) from an MSVC mangled name, best effort."""
    if not mangled.startswith("?"):
        return (mangled, "")
    body = mangled[1:]
    if body.startswith("?"):          # ??0Foo@@ operators/ctors
        parts = body.split("@")
        return (parts[0], parts[1] if len(parts) > 1 else "")
    parts = body.split("@")
    return (parts[0], parts[1] if len(parts) > 1 else "")


def classify(pred, truth):
    if pred == truth:
        return "AGREE"
    pm, pc = split_sym(pred)
    tm, tc = split_sym(truth)
    if pc and pc == tc:
        return "SAME_CLASS_DIFF_METHOD"
    if pm and pm == tm:
        return "SAME_METHOD_DIFF_CLASS"
    return "UNRELATED"


def measure(rows, gt, by_addr, label, name_override=None):
    """Return stats over rows that have ground truth."""
    tax = Counter()
    tiers = Counter()
    sims = defaultdict(list)
    icf_rescued = 0
    n_gt = 0
    detail = []
    for i, r in enumerate(rows):
        va = int(r["rb3_addr"], 16)
        if va not in gt:
            continue
        truth, tier = gt[va]
        pred = name_override[i] if name_override else r["dc3_name"]
        n_gt += 1
        verdict = classify(pred, truth)
        # ICF tolerance: DC3 may fold several names onto one VA; if the truth
        # is among the names AT THAT VA, the structural match found the right
        # code and only the name label is ambiguous.
        if verdict != "AGREE":
            aliases = by_addr.get(int(r["dc3_addr"], 16), [])
            if truth in aliases:
                verdict = "AGREE_ICF_ALIAS"
                icf_rescued += 1
        tax[verdict] += 1
        tiers[(tier, verdict in ("AGREE", "AGREE_ICF_ALIAS"))] += 1
        sims[verdict].append((r.get("similarity", 0), r.get("confidence", 0)))
        if len(detail) < 40 and verdict not in ("AGREE", "AGREE_ICF_ALIAS"):
            detail.append((r["rb3_addr"], truth, pred, verdict,
                           r.get("similarity"), tier))
    ok = tax["AGREE"] + tax["AGREE_ICF_ALIAS"]
    print(f"\n=== {label} ===")
    print(f"  rows with ground truth : {n_gt} of {len(rows)}")
    if n_gt:
        print(f"  AGREE                  : {ok} ({100*ok/n_gt:.2f}%)"
              f"   [exact {tax['AGREE']}, icf-alias {tax['AGREE_ICF_ALIAS']}]")
        for k in ("SAME_CLASS_DIFF_METHOD", "SAME_METHOD_DIFF_CLASS", "UNRELATED"):
            print(f"  {k:23s}: {tax[k]} ({100*tax[k]/n_gt:.2f}%)")
    for tier in ("GOLD", "MAP"):
        good, bad = tiers[(tier, True)], tiers[(tier, False)]
        if good + bad:
            print(f"  tier {tier:4s}: {good}/{good+bad} = "
                  f"{100*good/(good+bad):.2f}% agree")
    return tax, sims, n_gt, detail


def dist(vals):
    if not vals:
        return "n=0"
    v = sorted(vals)
    n = len(v)
    return (f"n={n} min={v[0]:.3f} p25={v[n//4]:.3f} med={v[n//2]:.3f} "
            f"p75={v[3*n//4]:.3f} max={v[-1]:.3f} mean={sum(v)/n:.4f}")


def main():
    rows = json.load(open(os.path.join(REPO, "tools/bindiff_match.json")))
    print(f"[in] bindiff_match.json: {len(rows)} rows")
    dc3, by_addr = load_dc3()
    gt = load_ground_truth()

    tax, sims, n_gt, detail = measure(rows, gt, by_addr, "TREATMENT (real BinDiff proposals)")

    # ---- C2 shuffle null -------------------------------------------------
    rnd = random.Random(1234)
    shuffled = [r["dc3_name"] for r in rows]
    rnd.shuffle(shuffled)
    measure(rows, gt, by_addr, "C2 NULL: dc3_name shuffled across rows",
            name_override=shuffled)

    # ---- C3 size-matched null -------------------------------------------
    by_size = defaultdict(list)
    for r in rows:
        by_size[r.get("size", 0)].append(r["dc3_name"])
    sized = [rnd.choice(by_size[r.get("size", 0)]) for r in rows]
    measure(rows, gt, by_addr, "C3 NULL: random DC3 name of the SAME body size",
            name_override=sized)

    # ---- C4 is similarity a discriminator? -------------------------------
    print("\n=== C4 DISCRIMINATOR: similarity / confidence by verdict ===")
    good_s = [s for k in ("AGREE", "AGREE_ICF_ALIAS") for s, c in sims[k]]
    bad_s = [s for k in ("SAME_CLASS_DIFF_METHOD", "SAME_METHOD_DIFF_CLASS",
                         "UNRELATED") for s, c in sims[k]]
    good_c = [c for k in ("AGREE", "AGREE_ICF_ALIAS") for s, c in sims[k]]
    bad_c = [c for k in ("SAME_CLASS_DIFF_METHOD", "SAME_METHOD_DIFF_CLASS",
                         "UNRELATED") for s, c in sims[k]]
    print(f"  similarity  AGREE   : {dist(good_s)}")
    print(f"  similarity  WRONG   : {dist(bad_s)}")
    print(f"  confidence  AGREE   : {dist(good_c)}")
    print(f"  confidence  WRONG   : {dist(bad_c)}")
    # precision at the strictest available gate
    strict = [(r, gt[int(r['rb3_addr'],16)]) for r in rows
              if int(r["rb3_addr"], 16) in gt
              and r.get("similarity", 0) >= 1.0 and r.get("confidence", 0) >= 0.99]
    if strict:
        okn = sum(1 for r, (t, _) in strict
                  if r["dc3_name"] == t
                  or t in by_addr.get(int(r["dc3_addr"], 16), []))
        print(f"  gate sim==1.0 & conf>=0.99 : {okn}/{len(strict)} = "
              f"{100*okn/len(strict):.2f}% precision")

    print("\n=== sample DISAGREEMENTS (first 40) ===")
    for d in detail:
        print(f"  {d[0]}  truth={d[1][:60]:60s} pred={d[2][:60]:60s} "
              f"{d[3]} sim={d[4]} {d[5]}")

    json.dump({"n_rows": len(rows), "n_with_gt": n_gt,
               "taxonomy": dict(tax)},
              open(os.path.join(OUT, "calibration_bindiff.json"), "w"), indent=1)


if __name__ == "__main__":
    main()
