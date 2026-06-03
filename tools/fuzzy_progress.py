#!/usr/bin/env python3
"""Fuzzy-weighted progress reporter for the partial-match porting strategy.

matched_functions (objdiff's count at match_percent_normalized==100) UNDER-
represents true progress while the link-graph is incomplete: a byte-correct
function whose bl/data relocations point at not-yet-named callees is suppressed
below 100. This reporter adds the leading indicators that PREDICT where the
count will climb:

  matched (==100)        the official count.
  near  [99,100)         ~1 instruction off. MOSTLY base-class layout / unnamed-
                         callee bound (see classify_nearmiss.py) -- not free, but
                         the highest-density pool of almost-done work.
  fuzzy fn-equivalents   sum(match_percent_normalized)/100. A continuous metric
                         that moves when ANY function climbs, even 40->70. This
                         is the leading indicator: it rises BEFORE matched_functions
                         does, so it shows porting/naming work is landing even
                         when no function has crossed 100 yet.
  >=50 / >=90 / >=99     count of functions in flight at each band.

Usage:
  tools/fuzzy_progress.py                       # report current build
  tools/fuzzy_progress.py --report other/report.json
  tools/fuzzy_progress.py --baseline saved.json # also print delta vs a saved report
  tools/fuzzy_progress.py --by-unit 25          # top units by fuzzy headroom
"""
import argparse, json, os, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT = os.path.join(ROOT, "build", "45410914", "report.json")


def summarize(path):
    d = json.load(open(path))
    total = matched = near = ge50 = ge90 = ge99 = 0
    fuzzy_equiv = 0.0
    code_total = 0
    code_fuzzy = 0.0
    per_unit = {}  # unit -> [fuzzy_headroom_bytes, near_count]
    for unit in d["units"]:
        un = unit["name"]
        hr = 0.0
        nc = 0
        for f in unit.get("functions", []):
            mp = f.get("match_percent_normalized", 0.0)
            sz = int(f.get("size", 0))
            total += 1
            fuzzy_equiv += mp / 100.0
            code_total += sz
            code_fuzzy += sz * mp / 100.0
            if mp >= 50:
                ge50 += 1
            if mp >= 90:
                ge90 += 1
            if mp >= 99:
                ge99 += 1
            if mp == 100:
                matched += 1
            elif mp >= 99:
                near += 1
            # headroom = bytes NOT yet matched, weighted by how close (climbability)
            if 0 < mp < 100:
                hr += sz * (mp / 100.0)  # invested-but-unfinished mass
                nc += 1
        if hr:
            per_unit[un] = (hr, nc)
    return {
        "total": total, "matched": matched, "near": near,
        "ge50": ge50, "ge90": ge90, "ge99": ge99,
        "fuzzy_equiv": fuzzy_equiv,
        "code_total": code_total, "code_fuzzy": code_fuzzy,
        "per_unit": per_unit,
    }


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--report", default=DEFAULT)
    ap.add_argument("--baseline", default="")
    ap.add_argument("--by-unit", type=int, default=0)
    a = ap.parse_args()
    s = summarize(a.report)
    print(f"=== fuzzy progress ({a.report}) ===")
    print(f"  total functions       {s['total']}")
    print(f"  matched (==100)       {s['matched']:6d}   ({100*s['matched']/s['total']:.2f}%)")
    print(f"  near [99,100)         {s['near']:6d}   (1-insn-off pool)")
    print(f"  >=99                  {s['ge99']:6d}")
    print(f"  >=90                  {s['ge90']:6d}")
    print(f"  >=50                  {s['ge50']:6d}")
    print(f"  fuzzy fn-equivalents  {s['fuzzy_equiv']:8.1f}   (+{s['fuzzy_equiv']-s['matched']:.1f} over matched)")
    print(f"  code-byte fuzzy%      {100*s['code_fuzzy']/s['code_total']:.4f}%")
    if a.baseline:
        b = summarize(a.baseline)
        print(f"\n=== delta vs {a.baseline} ===")
        print(f"  matched   {s['matched']-b['matched']:+d}")
        print(f"  near      {s['near']-b['near']:+d}")
        print(f"  fuzzy-eq  {s['fuzzy_equiv']-b['fuzzy_equiv']:+.1f}")
    if a.by_unit:
        print(f"\n=== top {a.by_unit} units by fuzzy headroom (invested-but-unfinished) ===")
        ranked = sorted(s["per_unit"].items(), key=lambda kv: -kv[1][0])
        for un, (hr, nc) in ranked[:a.by_unit]:
            print(f"   {un[:48]:48s}  {hr:8.0f}B  {nc} partial fns")


if __name__ == "__main__":
    main()
