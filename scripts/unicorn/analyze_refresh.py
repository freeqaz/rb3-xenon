#!/usr/bin/env python3
"""Compare a fresh unicorn audit against the stored 2026-07-16 verdicts.

Answers three questions the DB alone cannot:

1. STALENESS -- how many of the stored verdicts still hold when re-measured
   on today's objects? The DB carries `unicorn_signal_version` and
   `unicorn_probe_schedule_hash` as staleness keys, but both are unchanged
   since 2026-07-16 while the harness itself was patched (extractor.py
   `777f5641`, engine/comparator `ae56d085`). So the staleness keys report
   "current" for verdicts that are not. Re-measuring is the only honest test.

2. COVERAGE -- stored vs refreshed, against live DB rows.

3. MATCHED-BUT-WRONG -- rows the metric scores 100% that behave differently
   from retail. This is the deliverable: a divergence there is a bug the
   match metric certifies as correct.

Reads the 100%-set from report.json. That is a TOOL-DEPENDENT claim -- the
report records which objdiff-cli produced it, and the hash is printed so the
worklist can be read against the right binary.
"""

import argparse
import csv
import json
import os
import sqlite3
import sys
from collections import Counter, defaultdict

# Divergence classes that indicate a REAL behavioural bug vs. classes that
# are emulation/build artifacts. Vocabulary is the project's existing one --
# do not invent new labels.
REAL_CLASSES = {
    "logic", "call_count", "call_arg", "return_value", "object_memory",
    "error", "wild_jump_match", "cap_exhausted", "cap_exhausted_decomp",
}
ARTIFACT_CLASSES = {
    "build_env", "regalloc", "merged_call", "merged_arg", "stack_layout",
    "fpr_precision", "orig_error", "cap_exhausted_orig",
    "unmapped_access_mismatch",
}


def load_report_100(report_path):
    """Return dicts: symbol -> (fuzzy, mpn, size, unit). Coerce every numeric:
    report.json is protobuf-JSON, so defaults are OMITTED and several
    numerics are JSON STRINGS."""
    with open(report_path) as f:
        d = json.load(f)
    prov = d.get("provenance", {})
    info = {
        "tool_commit": prov.get("tool_commit"),
        "tool_binary_hash": prov.get("tool_binary_hash"),
        "tool_version": prov.get("tool_version"),
        "diff_config": prov.get("diff_config"),
        "total_functions": int(d["measures"].get("total_functions", 0) or 0),
        "matched_code_percent": float(
            d["measures"].get("matched_code_percent", 0) or 0),
    }
    by_sym = {}
    for u in d.get("units", []):
        uname = u.get("name", "")
        for fn in u.get("functions", []):
            name = fn.get("name")
            if not name:
                continue
            fz = float(fn.get("fuzzy_match_percent", 0) or 0)
            mpn = float(fn.get("match_percent_normalized", 0) or 0)
            size = int(fn.get("size", 0) or 0)
            prev = by_sym.get(name)
            # Keep the highest-scoring instance if a symbol appears twice.
            if prev is None or fz > prev[0]:
                by_sym[name] = (fz, mpn, size, uname)
    return by_sym, info


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", required=True, help="fresh audit CSV")
    ap.add_argument("--baseline-db", required=True,
                    help="snapshot of decomp.db holding the 2026-07-16 verdicts")
    ap.add_argument("--report", required=True)
    ap.add_argument("--out", default=None, help="write worklist markdown here")
    args = ap.parse_args()

    # --- fresh ----------------------------------------------------------
    fresh = {}
    with open(args.csv) as f:
        for r in csv.DictReader(f):
            fresh[r["symbol"]] = r
    print(f"fresh audit rows      : {len(fresh)}")

    # --- stored ---------------------------------------------------------
    con = sqlite3.connect(args.baseline_db)
    stored = {}
    live = {}
    for sym, v, cls, conf, lv, pct, unit, size in con.execute(
            "select symbol, unicorn_verdict, unicorn_class, unicorn_confidence,"
            " live, current_percent, unit, size from functions"):
        if v is not None:
            stored[sym] = (v, cls, conf)
        live[sym] = (lv, pct, unit, size)
    print(f"stored verdicts       : {len(stored)}")
    n_live = sum(1 for s in live.values() if s[0] == 1)
    print(f"live DB rows          : {n_live}")

    stored_on_live = sum(1 for s in stored if live.get(s, (0,))[0] == 1)
    print(f"stored verdicts on live rows: {stored_on_live} "
          f"({100.0*stored_on_live/max(n_live,1):.2f}% coverage of live)")

    # --- staleness: re-measure agreement --------------------------------
    both = [s for s in stored if s in fresh
            and fresh[s]["verdict"] in ("EQUIVALENT", "DIVERGENT")]
    agree = flip = 0
    flips = Counter()
    for s in both:
        old = stored[s][0]
        new = fresh[s]["verdict"]
        if old == new:
            agree += 1
        else:
            flip += 1
            flips[f"{old}->{new}"] += 1
    print(f"\n--- STALENESS (re-measured on today's objects) ---")
    print(f"comparable symbols    : {len(both)}")
    if both:
        print(f"verdict AGREES        : {agree} ({100.0*agree/len(both):.1f}%)")
        print(f"verdict FLIPPED       : {flip} ({100.0*flip/len(both):.1f}%)")
        for k, v in flips.most_common():
            print(f"    {k}: {v}")
    gone = sum(1 for s in stored if s not in fresh)
    print(f"stored verdicts with NO fresh measurement: {gone}")

    # --- evidence quality ------------------------------------------------
    ev = Counter(r["evidence"] for r in fresh.values())
    print(f"\n--- EVIDENCE QUALITY (fresh) ---")
    for k, v in ev.most_common():
        print(f"  {k:20s} {v:6d}  {100.0*v/len(fresh):5.1f}%")
    er, em = ev.get("equiv_real", 0), ev.get("equiv_matching_err", 0)
    if er + em:
        print(f"  => of {er+em} EQUIVALENT, {em} "
              f"({100.0*em/(er+em):.1f}%) are a SHARED EMULATION ERROR "
              f"(no equivalence evidence)")

    # --- matched-but-wrong ----------------------------------------------
    by_sym, info = load_report_100(args.report)
    print(f"\n--- REPORT PROVENANCE (the 100%-set is tool-dependent) ---")
    for k in ("tool_version", "tool_commit", "tool_binary_hash"):
        print(f"  {k}: {info[k]}")

    worklist = []
    for sym, r in fresh.items():
        if r["verdict"] != "DIVERGENT":
            continue
        cls = r["div_class"] or ""
        if cls in ARTIFACT_CLASSES:
            continue
        ent = by_sym.get(sym)
        if not ent:
            continue
        fz, mpn, size, unit = ent
        if fz < 100.0 and mpn < 100.0:
            continue
        worklist.append({
            "symbol": sym, "unit": unit, "size": size, "fuzzy": fz, "mpn": mpn,
            "div_class": cls, "confidence": r["confidence"],
            "evidence": r["evidence"],
            "cap": r.get("cap_exhausted", ""),
        })

    # Rank: stable_divergent first, then real-bug classes, then size.
    def rank(w):
        return (
            0 if w["confidence"] == "stable_divergent" else 1,
            0 if w["fuzzy"] >= 100.0 else 1,      # bytes actually credited
            0 if w["div_class"] in ("call_arg", "call_count", "return_value",
                                    "object_memory", "logic") else 1,
            -w["size"],
        )
    worklist.sort(key=rank)

    print(f"\n--- MATCHED-BUT-WRONG WORKLIST ---")
    print(f"DIVERGENT rows scoring 100% (fuzzy or mpn), artifact classes "
          f"excluded: {len(worklist)}")
    cc = Counter(w["div_class"] for w in worklist)
    for k, v in cc.most_common():
        print(f"  {k:22s} {v}")
    both100 = [w for w in worklist if w["fuzzy"] >= 100.0 and w["mpn"] >= 100.0]
    print(f"  of which BOTH rulers 100%: {len(both100)}")

    if args.out:
        with open(args.out, "w") as f:
            f.write("# Unicorn matched-but-wrong worklist\n\n")
            f.write("Functions whose emulated behaviour DIFFERS from retail "
                    "while the match metric scores them 100%.\n\n")
            f.write(f"- report.json objdiff-cli: {info['tool_version']} "
                    f"commit `{info['tool_commit']}` "
                    f"binary `{info['tool_binary_hash']}`\n")
            f.write(f"- ruler: {info['diff_config'][0] if info['diff_config'] else '?'}\n")
            f.write(f"- artifact classes excluded: "
                    f"{', '.join(sorted(ARTIFACT_CLASSES))}\n\n")
            f.write(f"Total: **{len(worklist)}** rows "
                    f"({len(both100)} at 100% on BOTH rulers)\n\n")
            f.write("| # | symbol | unit | size | fuzzy | mpn | class | confidence |\n")
            f.write("|---|--------|------|-----:|------:|----:|-------|------------|\n")
            for i, w in enumerate(worklist, 1):
                f.write(f"| {i} | `{w['symbol']}` | {w['unit']} | {w['size']} "
                        f"| {w['fuzzy']:.2f} | {w['mpn']:.2f} | {w['div_class']} "
                        f"| {w['confidence']} |\n")
        print(f"\nworklist -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
