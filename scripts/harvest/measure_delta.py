#!/usr/bin/env python3
"""Measure match net delta between two report.json snapshots.

Counters self-measurement error: prints the EXACT strict net (gains - regressions)
over the strict-100 set, PLUS a per-function FUZZY regression scan (any function
whose match% dropped, even if it never touched 100). The fuzzy scan is the safety
half of the partials-landable land gate — a cascade patch can drop a function
99->80 without leaving the strict set, which the strict diff alone cannot see.

Use in a worktree A/B:

  cp build/45410914/report.json ~/tmp/BASE.json          # before edit
  ... apply edit; rm stamp; touch config.yml; tools/fresh_report.sh ...
  scripts/harvest/measure_delta.py ~/tmp/BASE.json build/45410914/report.json

Exit 0 always; the verdict is in the printed NET line. A claimed win REQUIRES
net > 0 AND zero unexplained strict regressions AND zero real fuzzy regressions
(drop > --fuzzy-eps). Run the rebuild TWICE and confirm NET is identical.
"""
import argparse
import json

STRICT = 99.999


def pct_map(path):
    """(unit, fn) -> normalized match percent, for every function."""
    d = json.load(open(path))
    m = {}
    for u in d["units"]:
        un = u.get("name")
        for f in (u.get("functions") or []):
            m[(un, f["name"])] = f["match_percent_normalized"]
    return m


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("baseline", help="baseline report.json (before edit)")
    ap.add_argument("new", help="new report.json (after edit)")
    ap.add_argument("--fuzzy-eps", type=float, default=1.0,
                    help="min %% drop to count as a REAL fuzzy regression "
                         "(smaller drops are treated as reloc-name noise; default 1.0)")
    args = ap.parse_args()

    bmap = pct_map(args.baseline)
    nmap = pct_map(args.new)

    base = {k for k, v in bmap.items() if v >= STRICT}
    new = {k for k, v in nmap.items() if v >= STRICT}
    gained = sorted(new - base)
    regressed = sorted(base - new)
    net = len(new) - len(base)

    # Fuzzy per-function regressions: common functions whose % dropped by > eps.
    # This catches sub-100 regressions the strict set-diff misses.
    fuzzy_reg = []
    for k in bmap.keys() & nmap.keys():
        drop = bmap[k] - nmap[k]
        if drop > args.fuzzy_eps and nmap[k] < STRICT:
            fuzzy_reg.append((drop, k[0], k[1], bmap[k], nmap[k]))
    fuzzy_reg.sort(reverse=True)  # worst drop first

    print(f"BASELINE strict-100: {len(base)}")
    print(f"NEW      strict-100: {len(new)}")
    print(f"NET: {net:+d}   (gained {len(gained)}, regressed {len(regressed)})")

    if regressed:
        print("\nSTRICT REGRESSED (was 100, now <100) — MUST be zero/explained for a win:")
        for un, nm in regressed[:60]:
            print(f"  - {un}  {nm}  ({bmap.get((un,nm),0):.3f} -> {nmap.get((un,nm),0):.3f})")
        if len(regressed) > 60:
            print(f"  ... +{len(regressed)-60} more")

    if fuzzy_reg:
        print(f"\nFUZZY REGRESSED (dropped > {args.fuzzy_eps}%, still <100) — "
              "a real regression under partials-landable, MUST be zero for a win:")
        for drop, un, nm, b, n in fuzzy_reg[:60]:
            print(f"  ~ {un}  {nm}  ({b:.3f} -> {n:.3f}, -{drop:.3f})")
        if len(fuzzy_reg) > 60:
            print(f"  ... +{len(fuzzy_reg)-60} more")

    if gained:
        print("\nGAINED (now 100):")
        for un, nm in gained[:60]:
            print(f"  + {un}  {nm}")
        if len(gained) > 60:
            print(f"  ... +{len(gained)-60} more")

    clean = not regressed and not fuzzy_reg
    if net > 0 and clean:
        verdict = "WIN"
    elif net <= 0 and clean:
        verdict = "NET-ZERO/NEGATIVE"
    else:
        verdict = "HAS REGRESSIONS — investigate (strict and/or fuzzy)"
    print(f"\nVERDICT: {verdict}")


if __name__ == "__main__":
    main()
