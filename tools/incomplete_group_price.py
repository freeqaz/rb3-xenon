#!/usr/bin/env python3
"""incomplete_group_price.py -- price the incomplete-group census HONESTLY.

Lane INCOMPLETE-1. The census's "bytes on rows below 100" is a GROSS UPPER BOUND
and must not be quoted as a prize: `matched_code` is ALL-OR-NOTHING PER ROW, so a
row pays only when EVERY charge on it clears. Two filters make the number real:

  (1) mpn == 100 is REQUIRED. A row at match_percent_normalized < 100 carries
      instruction-level (non-relocation-arg) mismatches, which no alias can
      touch. Such a row cannot cross no matter how many names are forgiven.
  (2) EVERY charged slot on the row must be in the class being priced. A row
      carrying one MEMBERSHIP charge and one FRESH charge does not cross when
      only the MEMBERSHIP class is completed.

Reports the realisable prize per class, and splits MEMBERSHIP by DIRECTION,
because the two directions are different claims:

  SURVIVOR_SIDE  retail-side name is a group SURVIVOR, our spelling is unowned.
                 Adding our spelling to that group is purely ADDITIVE -- it
                 asserts one more spelling landed on an address the group
                 already claims. This is ONMSG-1's exact shape.
  FOLDED_SIDE    retail-side name is a FOLDED member of a group (not survivor).
  OURS_SIDE      only our spelling is owned; the retail name is unowned. NOT a
                 group completion -- the retail name is the thing needing
                 identification. Reported, not harvested.
"""

import collections
import glob
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from icf_alias_build import collect, placeholder  # noqa: E402
from incomplete_group_census import charged_pairs, load_groups  # noqa: E402


def main():
    tgt = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/obj/**/*.obj"), recursive=True)), "t")
    ours = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/src/**/*.obj"), recursive=True)), "o")
    owner, groups = load_groups(os.path.join(ROOT, "scripts/symbol_aliases.json"))
    surv = {g["survivor"]: i for i, g in enumerate(groups)}

    sites, victims, _ = charged_pairs(tgt, ours)
    for p in [p for p in sites if placeholder(p[0]) or placeholder(p[1])]:
        del sites[p]
    for p in list(sites):
        gi, gj = owner.get(p[0]), owner.get(p[1])
        if gi is not None and gi == gj:
            del sites[p]

    def klass(p):
        gi, gj = owner.get(p[0]), owner.get(p[1])
        if gi is not None and gj is not None:
            return "MERGE"
        if gi is not None:
            return "MEMBERSHIP_SURVIVOR" if p[0] in surv else "MEMBERSHIP_FOLDED"
        if gj is not None:
            return "OURS_SIDE"
        return "FRESH"

    # row -> set of charged pairs on it
    row_charges = collections.defaultdict(set)
    for p in sites:
        for r in victims[p]:
            row_charges[r].add(p)

    rep = json.load(open(os.path.join(ROOT, "build/45410914/report.json")))
    mpn, fuzzy, size, unit_of = {}, {}, {}, {}
    for u in rep["units"]:
        for f in u.get("functions", []):
            mpn[f["name"]] = float(f["match_percent_normalized"])
            fuzzy[f["name"]] = float(f.get("fuzzy_match_percent", 0.0))
            size[f["name"]] = int(f["size"])
            unit_of[f["name"]] = u["name"]

    print("=" * 90)
    print("REALISABLE PRIZE per class  (mpn==100 AND every charge on the row in-class)")
    print("=" * 90)
    print("%-22s %7s %7s %9s %9s %11s %11s" %
          ("class", "pairs", "sites", "rows", "gross B", "closable", "closable B"))

    summary = {}
    for k in ("MEMBERSHIP_SURVIVOR", "MEMBERSHIP_FOLDED", "MERGE", "OURS_SIDE", "FRESH"):
        ps = [p for p in sites if klass(p) == k]
        rows = set()
        for p in ps:
            rows |= victims[p]
        gross = [r for r in rows if fuzzy.get(r, 100.0) < 100.0]
        closable = [r for r in gross
                    if mpn.get(r, 0.0) >= 100.0 and all(klass(q) == k for q in row_charges[r])]
        summary[k] = {
            "pairs": len(ps), "sites": sum(sites[p] for p in ps),
            "rows": len(rows), "gross_bytes": sum(size.get(r, 0) for r in gross),
            "closable_rows": len(closable), "closable_bytes": sum(size.get(r, 0) for r in closable),
            "rows_list": sorted(closable),
        }
        s = summary[k]
        print("%-22s %7d %7d %9d %9d %11d %11d" %
              (k, s["pairs"], s["sites"], len(gross), s["gross_bytes"],
               s["closable_rows"], s["closable_bytes"]))

    # A row can also be closable by a UNION of classes (e.g. membership+merge).
    allk = {"MEMBERSHIP_SURVIVOR", "MEMBERSHIP_FOLDED", "MERGE"}
    rows = set()
    for p in sites:
        if klass(p) in allk:
            rows |= victims[p]
    union = [r for r in rows
             if fuzzy.get(r, 100.0) < 100.0 and mpn.get(r, 0.0) >= 100.0
             and all(klass(q) in allk for q in row_charges[r])]
    print("\nUNION of the three group-touching classes: %d closable rows / %d B"
          % (len(union), sum(size.get(r, 0) for r in union)))

    # rank MEMBERSHIP_SURVIVOR pairs by realisable bytes
    print("\nTop MEMBERSHIP_SURVIVOR pairs by CLOSABLE bytes:")
    per = []
    for p in [p for p in sites if klass(p) == "MEMBERSHIP_SURVIVOR"]:
        cl = [r for r in victims[p]
              if fuzzy.get(r, 100.0) < 100.0 and mpn.get(r, 0.0) >= 100.0
              and all(klass(q) == "MEMBERSHIP_SURVIVOR" for q in row_charges[r])]
        per.append((sum(size.get(r, 0) for r in cl), len(cl), sites[p], p))
    per.sort(reverse=True)
    for b, n, st, p in per[:25]:
        if not b:
            continue
        print("  %7d B  %3d rows %3d sites  g%-5s %s\n%s<- %s"
              % (b, n, st, owner[p[0]], p[0], " " * 34, p[1]))

    json.dump({k: v for k, v in summary.items()},
              open(os.environ.get("PRICE_OUT", "/home/free/tmp/incomplete_price.json"), "w"), indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main())
