#!/usr/bin/env python3
"""Compare two equivalence_audit CSVs and adjudicate the survivors.

WHAT THIS IS FOR
----------------
Lane S4 established that a DIVERGENT verdict from this harness is NOT a bug
report until it has been screened, and that the raw "matched but wrong" set was
99.2% instrument artifact. Lane U1 then changed the harness itself, so two
questions have to be answered together:

  1. What did the change do to the VERDICT POPULATION? (an A/B over the same
     units -- deltas compose, absolutes do not)
  2. Which surviving DIVERGENT rows are real, after the screens?

Both screens from S4 are applied here, because neither is optional:

  * BYTE-IDENTITY -- a proof, not a heuristic. If the two sides' function
    bodies are byte-identical with identical relocation counts, deterministic
    emulation over identical code CANNOT diverge, so the divergence is
    attributable entirely to how relocation targets were mocked. This screen
    killed S4's two most attractive candidates.
  * ARTIFACT CLASSES -- `data_layout`, `merged_*`, `scratch_return_reg`,
    `build_env`, `regalloc`, `stack_layout`, `fpr_precision`, `orig_error`.
    Note `data_layout` is NEW as of the U1 port: it is S4's "mock-region
    artifact" reclassified at source rather than screened after the fact.

⚠ `cap_exhausted*` is INDETERMINATE, not a real-bug class. Both sides hit the
emulator instruction cap, so nothing was compared. Treating it as real inflated
S4's worklist by 542 rows. It is excluded, not counted.

Usage:
    venv/bin/python scripts/unicorn/deep_schedule_report.py \
        --before ~/tmp/u1_before.csv --after ~/tmp/u1_after.csv \
        --report build/45410914/report.json \
        --out docs/decomp/unicorn_deep_schedule_2026-09-10.csv
"""

import argparse
import collections
import csv
import json
import os
import sys

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
sys.path.insert(0, PROJECT_ROOT)

# Classes that are harness artifacts, never decomp defects.
ARTIFACT_CLASSES = {
    "data_layout", "build_env", "regalloc", "merged_call", "merged_arg",
    "stack_layout", "fpr_precision", "orig_error", "scratch_return_reg",
}
# Real-bug vocabulary (decomp.db's own).
REAL_CLASSES = {
    "logic", "call_count", "call_arg", "return_value", "object_memory", "error",
}


def load(path):
    with open(path) as f:
        return {(r["unit"], r["symbol"]): r for r in csv.DictReader(f)}


def load_scores(report_path):
    """symbol -> (fuzzy, mpn, size). report.json numerics may be JSON STRINGS
    with protobuf defaults OMITTED, so coerce every one and never index."""
    if not report_path or not os.path.exists(report_path):
        return {}
    with open(report_path) as f:
        rep = json.load(f)
    out = {}
    for unit in rep.get("units", []):
        for fn in unit.get("functions", []):
            name = fn.get("name")
            if not name:
                continue
            out[name] = (
                float(fn.get("fuzzy_match_percent", 0) or 0),
                float(fn.get("match_percent_normalized", 0) or 0),
                int(fn.get("size", 0) or 0),
            )
    return out


def body_bytes(unit, symbol):
    """Both sides' bytes for a symbol, or (None, None) if unavailable."""
    try:
        from scripts.unicorn_runner.coff import COFFParser
        from scripts.unicorn_runner.run import resolve_unit
        from scripts.unicorn_runner.extractor import (
            extract_from_decomp, extract_from_original)
        d_path, o_path = resolve_unit(unit, PROJECT_ROOT)
        d, o = COFFParser(d_path), COFFParser(o_path)
        db, dr = extract_from_decomp(d, symbol)
        ob, orl = extract_from_original(o, symbol)
        return (db, dr), (ob, orl)
    except Exception:
        return (None, None), (None, None)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--before", required=True)
    ap.add_argument("--after", required=True)
    ap.add_argument("--report", default=None)
    ap.add_argument("--out", default=None, help="CSV of adjudicated survivors")
    args = ap.parse_args()

    B, A = load(args.before), load(args.after)
    keys = sorted(set(B) & set(A))
    scores = load_scores(args.report)

    print(f"matched rows on both legs: {len(keys)} "
          f"(before {len(B)}, after {len(A)})")

    print("\n=== EVIDENCE TRANSITION (before -> after) ===")
    trans = collections.Counter((B[k]["evidence"], A[k]["evidence"]) for k in keys)
    for (b, a), n in sorted(trans.items(), key=lambda kv: -kv[1]):
        print(f"  {b:<20} -> {a:<20} {n:5d}{'' if a == b else '   <== FLIP'}")

    print("\n=== DIVERGENCE CLASS (before -> after) ===")
    cb = collections.Counter(B[k]["div_class"] for k in keys
                             if B[k]["verdict"] == "DIVERGENT")
    ca = collections.Counter(A[k]["div_class"] for k in keys
                             if A[k]["verdict"] == "DIVERGENT")
    print(f"  {'class':<24} {'BEFORE':>7} {'AFTER':>7}")
    for c in sorted(set(cb) | set(ca)):
        kind = ("ARTIFACT" if c in ARTIFACT_CLASSES else
                "real" if c in REAL_CLASSES else "")
        print(f"  {c or '(none)':<24} {cb.get(c, 0):7d} {ca.get(c, 0):7d}  {kind}")

    # ---- adjudicate the AFTER-leg survivors ------------------------------
    rows = []
    for k in keys:
        a = A[k]
        if a["verdict"] != "DIVERGENT":
            continue
        cls = a["div_class"]
        if cls in ARTIFACT_CLASSES:
            continue
        if cls.startswith("cap_exhausted") or a["cap_exhausted"] == "1":
            continue          # INDETERMINATE, not a bug class
        fuzzy, mpn, size = scores.get(k[1], (None, None, None))
        (db, dr), (ob, orl) = body_bytes(*k)
        identical = (db is not None and ob is not None and db == ob
                     and len(dr or []) == len(orl or []))
        rows.append({
            "unit": k[0], "symbol": k[1], "div_class": cls,
            "confidence": a["confidence"],
            "before_evidence": B[k]["evidence"],
            "fuzzy": "" if fuzzy is None else f"{fuzzy:.2f}",
            "mpn": "" if mpn is None else f"{mpn:.2f}",
            "size": "" if size is None else size,
            "byte_identical": int(bool(identical)),
            "screen": ("BYTE_IDENTICAL_ARTIFACT" if identical
                       else "SURVIVES_BOTH_SCREENS"),
        })

    survivors = [r for r in rows if r["screen"] == "SURVIVES_BOTH_SCREENS"]
    killed = [r for r in rows if r["screen"] != "SURVIVES_BOTH_SCREENS"]
    print(f"\n=== ADJUDICATION ===")
    print(f"  DIVERGENT (after)                     "
          f"{sum(1 for k in keys if A[k]['verdict'] == 'DIVERGENT'):5d}")
    print(f"  - artifact class / cap_exhausted      "
          f"{sum(1 for k in keys if A[k]['verdict'] == 'DIVERGENT') - len(rows):5d}")
    print(f"  = real-class candidates               {len(rows):5d}")
    print(f"  - byte-identical bodies (PROOF)       {len(killed):5d}")
    print(f"  = SURVIVE BOTH SCREENS                {len(survivors):5d}")

    matched_but_wrong = [r for r in survivors
                         if r["fuzzy"] and (float(r["fuzzy"]) >= 100.0
                                            or float(r["mpn"] or 0) >= 100.0)]
    print(f"  of which score 100% (MATCHED BUT WRONG) {len(matched_but_wrong):5d}")

    if survivors:
        print("\n=== SURVIVORS ===")
        for r in sorted(survivors, key=lambda r: -(int(r["size"] or 0))):
            print(f"  [{r['div_class']:<13}] {r['fuzzy'] or '?':>6}/"
                  f"{r['mpn'] or '?':<6} {str(r['size']):>5}B  {r['symbol'][:70]}")

    if args.out:
        os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
        with open(args.out, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(rows[0].keys()) if rows else
                               ["unit", "symbol", "div_class"])
            w.writeheader()
            w.writerows(rows)
        print(f"\nwrote {args.out} ({len(rows)} adjudicated rows)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
