#!/usr/bin/env python3
"""Attribute an A/B byte delta to individual ROWS, from the two archived reports.

A lane that ships four components in one patch must not report one aggregate
number and call the decomposition "as predicted" -- that is unfalsifiable. This
diffs legA_report.json.gz against legB_report.json.gz row by row, so every byte
of the measured delta is assigned to a named row and the pre-registered
per-component predictions can actually be scored.

`matched_code` counts a row's FULL size when `fuzzy_match_percent == 100` and
zero otherwise (all-or-nothing), so the delta is a SET DIFFERENCE over the
fuzzy==100 population -- NOT a sum of per-row percentage changes.

⚠ report.json is protobuf-JSON: absent numerics mean 0 and several are JSON
STRINGS. Everything is coerced.

Usage: python3 tools/rbtree_attribute.py <rundir>
"""
import gzip
import json
import sys


def rows(path):
    op = gzip.open if path.endswith(".gz") else open
    d = json.load(op(path, "rt"))
    out = {}
    for unit in d.get("units", []):
        u = unit.get("name", "?")
        for fn in unit.get("functions", []):
            out[(u, fn.get("name", ""))] = (
                int(fn.get("size", 0) or 0),
                float(fn.get("fuzzy_match_percent", 0.0) or 0.0),
                float(fn.get("match_percent_normalized", 0.0) or 0.0))
    return d["measures"], out


def main():
    rd = sys.argv[1].rstrip("/")
    mA, A = rows(f"{rd}/legA_report.json.gz")
    mB, B = rows(f"{rd}/legB_report.json.gz")
    dcode = int(mB["matched_code"]) - int(mA["matched_code"])
    dfn = int(mB["matched_functions"]) - int(mA["matched_functions"])
    print(f"Δmatched_code {dcode:+d}   Δmatched_funcs {dfn:+d}")
    print(f"A {mA['matched_functions']} fns / {mA['matched_code']} B / "
          f"{mA['matched_code_percent']}")
    print(f"B {mB['matched_functions']} fns / {mB['matched_code']} B / "
          f"{mB['matched_code_percent']}\n")

    gained = [(sz, k, (A[k][1] if k in A else None))
              for k, (sz, fz, _m) in B.items()
              if fz >= 100.0 and (k not in A or A[k][1] < 100.0)]
    lost = [(sz, k, (B[k][1] if k in B else None))
            for k, (sz, fz, _m) in A.items()
            if fz >= 100.0 and (k not in B or B[k][1] < 100.0)]
    gained.sort(reverse=True)
    lost.sort(reverse=True)
    print(f"== CROSSED TO fuzzy==100: {len(gained)} rows, "
          f"+{sum(g[0] for g in gained)} B")
    for sz, (u, n), was in gained[:40]:
        print(f"  +{sz:6d}  {u:36s} was={was}  {n[:76]}")
    print(f"\n== FELL OFF fuzzy==100: {len(lost)} rows, "
          f"-{sum(x[0] for x in lost)} B")
    for sz, (u, n), now in lost[:40]:
        print(f"  -{sz:6d}  {u:36s} now={now}  {n[:76]}")
    net = sum(g[0] for g in gained) - sum(x[0] for x in lost)
    print(f"\nrow-level net {net:+d}  headline {dcode:+d}  "
          f"{'AGREE' if net == dcode else 'DISAGREE'}")


if __name__ == "__main__":
    main()
