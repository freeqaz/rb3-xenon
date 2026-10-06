#!/usr/bin/env python3
"""Row-level before/after of two report.json files, attributing each moved row to
the retail words that differ between the two target images.

Written for the W16-PT retarget (docs/decomp/W16PT_CLEAN_TU5_RETARGET_2026-10-06.md),
where the acceptance test was: every row that moves between a report built on the
RB3 Deluxe image and one built on clean retail TU5 must contain a word that differs
between those images, and no other row may move.

Usage:
  tools/report_row_diff.py <before/report.json> <after/report.json> \
      --words <pe_word_diff.json> [--root <repo or worktree>]

A row is (unit, kind, name, address). Function rows are resolved to a VA through
fn_<addr> names, scripts/target_symbol_map.json and config/45410914/symbols.txt.
Section rows are unit aggregates (.text etc.) and move whenever a function in the
unit does; they are listed but carry no VA.

Exit status: 0 when every moved FUNCTION row contains a differing word, 1 when any
moved function row contains none (or cannot be resolved to a VA).
"""
import argparse
import json
import re
import sys


def rows(r):
    out = {}
    for u in r["units"]:
        for f in u.get("functions", []):
            k = (u["name"], "fn", f["name"], f.get("address"))
            out[k] = (int(f.get("size", 0)), float(f.get("fuzzy_match_percent", 0)),
                      f.get("match_percent_normalized"), bool(f.get("masked_equal", False)))
        for s in u.get("sections", []):
            out[(u["name"], "sec", s["name"], None)] = (
                int(s.get("size", 0)), float(s.get("fuzzy_match_percent", 0)), None, None)
    return out


def name_to_va(root):
    n2va = {}
    for a, n in json.load(open(f"{root}/scripts/target_symbol_map.json")).items():
        if a.startswith("0x"):
            n2va.setdefault(n, []).append(int(a, 16))
    for line in open(f"{root}/config/45410914/symbols.txt"):
        m = re.match(r"(\S+) = \.(\w+):0x([0-9A-Fa-f]+);", line)
        if m:
            n2va.setdefault(m.group(1), []).append(int(m.group(3), 16))
    return n2va


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("before")
    ap.add_argument("after")
    ap.add_argument("--words", required=True)
    ap.add_argument("--root", default=".")
    a = ap.parse_args()
    A, B = json.load(open(a.before)), json.load(open(a.after))
    words = [w["va"] for w in json.load(open(a.words))["words"] if w["va"] is not None]
    ra, rb = rows(A), rows(B)
    n2va = name_to_va(a.root)
    # Rows whose KEY changed (objdiff's `address` can move between runs) are
    # re-paired by (unit, kind, name) when that is unique on both sides.
    only_a, only_b = set(ra) - set(rb), set(rb) - set(ra)
    rekey = {}
    for ka in only_a:
        cands = [kb for kb in only_b if kb[:3] == ka[:3]]
        if len(cands) == 1:
            rekey[ka] = cands[0]
    unpaired = [k for k in only_a if k not in rekey] + \
               [k for k in only_b if k not in rekey.values()]
    print(f"rows: before {len(ra)}  after {len(rb)}  re-keyed {len(rekey)}  "
          f"unpaired {len(unpaired)}")
    for k in unpaired:
        print("  UNPAIRED", k)
    pairs = [(k, k) for k in ra if k in rb] + list(rekey.items())
    bad = len(unpaired)
    moved = 0
    for ka, kb in sorted(pairs):
        if ra[ka][1:] == rb[kb][1:] and ra[ka][0] == rb[kb][0]:
            continue
        moved += 1
        unit, kind, name, _ = ka
        size, fa, ma, _ = ra[ka]
        _, fb, mb, _ = rb[kb]
        line = f"{unit:28s} {kind:3s} {name[:58]:58s} {size:6d} B  fuzzy {fa:8.4f} -> {fb:8.4f}"
        if kind == "fn":
            m = re.match(r"fn_([0-9A-Fa-f]{8})$", name)
            vas = [int(m.group(1), 16)] if m else n2va.get(name, [])
            hits = sorted({w for v in vas for w in words if v <= w < v + size})
            line += f"  va {','.join(hex(v) for v in vas) or '?'}  words {[hex(w) for w in hits]}"
            if not hits:
                bad += 1
                line += "  <-- NO DIFFERING WORD"
        print(line)
    print(f"moved rows: {moved}")
    for m in sorted(A["measures"]):
        if A["measures"][m] != B["measures"].get(m):
            print(f"measure {m}: {A['measures'][m]} -> {B['measures'][m]}")
    print("VERDICT:", "PASS" if not bad else f"FAIL ({bad} row(s) unexplained)")
    return 0 if not bad else 1


if __name__ == "__main__":
    sys.exit(main())
