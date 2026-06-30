#!/usr/bin/env python3
"""Measure strict-match net delta between two report.json snapshots.

Counters self-measurement error: prints the EXACT net (gains - regressions) over
the strict-100 set, plus the per-function regression and gain lists. Use in a
worktree A/B:

  cp build/45410914/report.json ~/tmp/BASE.json          # before edit
  ... apply edit; rm stamp; touch config.yml; tools/fresh_report.sh ...
  scripts/harvest/measure_delta.py ~/tmp/BASE.json build/45410914/report.json

Exit 0 always; the verdict is in the printed NET line. A claimed win REQUIRES
net > 0 AND zero unexplained regressions (the REGRESSED list empty or fully
explained). Run the rebuild TWICE and confirm the NET is identical (deterministic).
"""
import json
import sys

STRICT = 99.999


def matched_set(path):
    d = json.load(open(path))
    s = {}
    for u in d["units"]:
        un = u.get("name")
        for f in (u.get("functions") or []):
            if f["match_percent_normalized"] >= STRICT:
                s[(un, f["name"])] = True
    return set(s.keys())


def main():
    if len(sys.argv) != 3:
        sys.exit("usage: measure_delta.py <baseline_report.json> <new_report.json>")
    base = matched_set(sys.argv[1])
    new = matched_set(sys.argv[2])
    gained = sorted(new - base)
    regressed = sorted(base - new)
    net = len(new) - len(base)
    print(f"BASELINE strict-100: {len(base)}")
    print(f"NEW      strict-100: {len(new)}")
    print(f"NET: {net:+d}   (gained {len(gained)}, regressed {len(regressed)})")
    if regressed:
        print("\nREGRESSED (was 100, now <100) — MUST be zero/explained for a win:")
        for un, nm in regressed[:60]:
            print(f"  - {un}  {nm}")
        if len(regressed) > 60:
            print(f"  ... +{len(regressed)-60} more")
    if gained:
        print("\nGAINED (now 100):")
        for un, nm in gained[:60]:
            print(f"  + {un}  {nm}")
        if len(gained) > 60:
            print(f"  ... +{len(gained)-60} more")
    print(f"\nVERDICT: {'WIN' if net > 0 and not regressed else ('NET-ZERO/NEGATIVE' if net <= 0 else 'NET+ BUT HAS REGRESSIONS — investigate')}")


if __name__ == "__main__":
    main()
