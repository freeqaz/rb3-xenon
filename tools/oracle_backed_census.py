#!/usr/bin/env python3
"""Rank our fuzzy-0 rows by the DC3 oracle's score FOR THAT ROW.

Finds the shape that W16-FG proved pays: a big NAMED row at fuzzy 0, in a
PAIRABLE in-scope unit, whose DC3 counterpart is matched. FG banked +3,280 B in
one build on exactly this; the census then produced W16-FJ (3,228 B) and
reopened W16-FK (740 B) in a single query.

Two screens this deliberately does NOT use, because both were measured wrong:

  * the oracle's UNIT percentage. `PeakDetector` is a 5.3% DC3 unit whose
    `Detect` row sits at 96.88; `GranularSynth` is 35.7% with `Synthesize` at
    99.95. A unit-% screen discards both.

  * a same-basename unit lookup. `?ReverbConvertI3DL2ToNative@@` reads "absent
    in DC3" that way -- DC3 keeps it in .../Synth, a different unit -- and that
    false negative closed a 740 B vein for a day. Oracle lookups are keyed on
    the NAME across every DC3 unit. Ambiguity is not a real risk here: only 17
    of 48,348 DC3 names appear in more than one unit, and none were in the
    oracle-backed population when this was written.

Usage:
    python3 tools/oracle_backed_census.py [--min-size N] [--top N] [--tsv]
"""
import argparse
import glob
import json
import os
import sys

DC3_REPORTS = "/home/free/code/milohax/dc3-decomp/build/*/report.json"
PLACEHOLDER = ("fn_", "lbl_", "jumptable_", "data_", "bss_", "rdata_")


def load_dc3():
    """name -> (best fuzzy, unit, size). Keyed on name across ALL units."""
    best = {}
    for path in glob.glob(DC3_REPORTS):
        with open(path) as fh:
            rep = json.load(fh)
        for unit in rep["units"]:
            for fn in unit.get("functions", []):
                name = fn.get("name")
                if not name:
                    continue
                # report.json is protobuf-JSON: defaults are omitted, and some
                # numerics arrive as strings. Coerce, never assume presence.
                fuzzy = float(fn.get("fuzzy_match_percent", 0) or 0)
                if name not in best or fuzzy > best[name][0]:
                    best[name] = (fuzzy, unit["name"], int(fn.get("size", 0)))
    return best


def tier(fuzzy):
    if fuzzy is None:
        return "NO DC3 ROW"
    if fuzzy >= 99.9:
        return "DC3 >=99.9 (solved)"
    if fuzzy >= 90:
        return "DC3 90-99.9 (near)"
    if fuzzy >= 50:
        return "DC3 50-90"
    return "DC3 <50 (weak)"


TIERS = ["DC3 >=99.9 (solved)", "DC3 90-99.9 (near)", "DC3 50-90",
         "DC3 <50 (weak)", "NO DC3 ROW"]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--report", default="build/45410914/report.json")
    ap.add_argument("--objdiff", default="objdiff.json")
    ap.add_argument("--min-size", type=int, default=0)
    ap.add_argument("--top", type=int, default=25)
    ap.add_argument("--tsv", action="store_true", help="rows only, for piping")
    args = ap.parse_args()

    with open(args.report) as fh:
        rep = json.load(fh)
    with open(args.objdiff) as fh:
        pairable = {u["name"] for u in json.load(fh)["units"] if u.get("base_path")}
    dc3 = load_dc3()
    if not dc3:
        sys.exit(f"no DC3 reports matched {DC3_REPORTS} -- is ../dc3-decomp built?")

    rows, by_tier = [], {t: [0, 0] for t in TIERS}
    for unit in rep["units"]:
        name = unit["name"]
        # in-scope: must be able to pair at all, and not vendor/unattributed
        if name not in pairable or "xdk" in name or name.startswith("auto_"):
            continue
        for fn in unit.get("functions", []):
            sym = fn.get("name", "")
            if not sym or sym.startswith(PLACEHOLDER):
                continue
            if float(fn.get("fuzzy_match_percent", 0) or 0) != 0.0:
                continue
            size = int(fn.get("size", 0))
            if size < args.min_size:
                continue
            oracle = dc3.get(sym)
            t = tier(oracle[0] if oracle else None)
            by_tier[t][0] += 1
            by_tier[t][1] += size
            rows.append((size, name, sym, oracle))

    rows.sort(reverse=True, key=lambda r: r[0])

    if args.tsv:
        for size, unit, sym, oracle in rows:
            o = f"{oracle[0]:.4f}\t{oracle[1]}" if oracle else "\t"
            print(f"{size}\t{unit}\t{sym}\t{o}")
        return

    total = sum(v[1] for v in by_tier.values())
    print(f"NAMED ROWS AT FUZZY 0, PAIRABLE IN-SCOPE UNITS: "
          f"{sum(v[0] for v in by_tier.values())} rows / {total} B\n")
    for t in TIERS:
        n, b = by_tier[t]
        pct = (100.0 * b / total) if total else 0.0
        print(f"  {t:<22} {n:5d} rows  {b:8d} B   {pct:5.1f}%")
    backed_r = by_tier[TIERS[0]][0] + by_tier[TIERS[1]][0]
    backed_b = by_tier[TIERS[0]][1] + by_tier[TIERS[1]][1]
    # ---- PRICE BY THE ORACLE'S DISTANCE FROM 100, NOT BY THE ROW'S SIZE ----
    # W16-FJ's correction, measured. `matched_code` pays only at fuzzy == 100,
    # so a FAITHFUL port of a sub-100 oracle pays EXACTLY ZERO: it reproduces
    # the oracle's score, wall and all. Of 3,228 B briefed to FJ as
    # "oracle-backed", 1,112 B crossed and 2,116 B landed on DC3's wall,
    # reproducing DC3's scores to four decimals -- which is itself the
    # strongest available evidence that the port was faithful.
    solved_r, solved_b = by_tier[TIERS[0]]   # DC3 >= 99.9
    near_r,   near_b   = by_tier[TIERS[1]]   # DC3 90 - 99.9
    print(f"\n  ORACLE-BACKED (>=90): {backed_r} rows / {backed_b} B -- but that"
          f" is NOT the prize:")
    print(f"    COLLECTABLE  (oracle >=99.9): {solved_r:4d} rows / {solved_b:6d} B"
          f"  <- work these first")
    print(f"    ORACLE'S WALL (oracle 90-99.9): {near_r:4d} rows / {near_b:6d} B"
          f"  <- a faithful port pays 0 B here")
    print("    Quoting the sum as the prize overprices this vein by the second"
          " line.\n")

    print(f"=== top {args.top} by size ===")
    for size, unit, sym, oracle in rows[:args.top]:
        o = (f"DC3 f={oracle[0]:8.4f} {oracle[1].split('/')[-1]}"
             if oracle else "NO DC3 ROW")
        print(f"  {size:6d} B  {unit[:38]:<38} {sym[:46]:<46} {o}")
    print("\nNOTE: a unit's unsolved-byte total silently includes the arg-only")
    print("DRAINED class. Subtract it before quoting a prize. matched_code is")
    print("all-or-nothing per row: only fuzzy == 100.0 pays.")
    print("")
    print("  The DRAINED class is `mpn == 100 AND fuzzy < 100` -- a row whose")
    print("  ONLY penalties are relocation-name args. It is NOT 'fuzzy looks")
    print("  like 99.x'. W16-FI's correction, measured: of 9 rows / 384 B filed")
    print("  as drained on the 99.x eyeball, only 6 / 264 B actually were; the")
    print("  other three read mpn == fuzzy sub-100 on BOTH rulers -- ordinary")
    print("  broken rows -- and TWO of them crossed as collateral, paying 80 B,")
    print("  which was 27% of that lane's entire delta. A row misfiled into a")
    print("  closed class is invisible to the lane told to skip it.")


if __name__ == "__main__":
    main()
