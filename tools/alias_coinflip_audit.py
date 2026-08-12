#!/usr/bin/env python3
"""Audit a rendered ICF-alias map for objdiff's NONDETERMINISTIC name->group choice.

THE DEFECT (objdiff-core/src/obj/map_file.rs, parse_msvc_map)
------------------------------------------------------------
The parser buckets every map line by its 8-hex address, then walks
``address_to_symbols.values()`` -- a ``std::collections::HashMap`` -- and does
``equivalences.insert(sym, group)`` for each member of each group.  ``insert``
overwrites, so **a name that appears at more than one address keeps whichever of
its groups the iteration reached LAST**, and Rust's ``RandomState`` is seeded per
HashMap instance from OS entropy.  The same binary on the same map file can
therefore hand a different group to that name on every run.  (Proven here: the
upstream parser called twice IN ONE PROCESS returns two different maps --
``test_parse_msvc_map_is_reproducible``.)

Consumption is symmetric (``objdiff-core/src/diff/code.rs``): a relocation name
pair (L, R) is forgiven iff ``equiv[L] contains R`` OR ``equiv[R] contains L``.

THE EXACT AT-RISK CONDITION
---------------------------
Let L sit in groups G1..Gn (n = number of map addresses naming L) and R in
H1..Hm.  Forgiveness on a given run is ``(R in Gi) or (L in Hj)`` for the
surviving i, j.  So:

    DETERMINISTIC-FORGIVEN  <=>  (R in EVERY Gi)  OR  (L in EVERY Hj)
    DETERMINISTIC-CHARGED   <=>  L and R never share a group
    COIN FLIP               <=>  otherwise

Two corollaries this tool exists to keep straight, because both circulating
statements of the rule are wrong in one direction:

* "a pair whose BOTH names are duplicated is a coin flip"
  (``scripts/icf_alias_merge.py``) -- both-duplicated is NECESSARY but NOT
  SUFFICIENT.  Measured on the 1,440-group file: 41,829 co-occurring pairs have
  both names duplicated and every one of them is deterministic, because the
  groups of a duplicated name all carry the same folded set.
* "a pair with only ONE name duplicated can still be a coin flip" -- it cannot.
  If R sits at exactly one address H and the pair co-occurs anywhere, the
  co-occurring group IS H, so ``L in H`` holds on every run.  If L is not in H
  then R is in no group of L either (that would give R a second address), so the
  pair is deterministically CHARGED.  Either way there is no flip.

Usage
-----
    python3 tools/alias_coinflip_audit.py                      # rendered map
    python3 tools/alias_coinflip_audit.py --map <path>
    python3 tools/alias_coinflip_audit.py --census scripts/namecheck_df_census.json
    python3 tools/alias_coinflip_audit.py --strict             # exit 1 on any flip

``--census`` classifies REAL charged (target, base) relocation-name pairs rather
than the map's own co-occurring pairs, which is the number that matters: a coin
flip on a pair no call site produces costs nothing.
"""

import argparse
import collections
import json
import re
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_MAP = PROJECT_ROOT / "build" / "45410914" / "icf_aliases.map"

# Must stay identical to parse_msvc_map's regex.
MAP_LINE = re.compile(r"^\s*\d{4}:[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]{8})\s+")


def load_groups(map_path):
    """address -> frozenset(names), for addresses naming more than one symbol."""
    addr_to_syms = collections.OrderedDict()
    with open(map_path) as fh:
        for line in fh:
            m = MAP_LINE.match(line)
            if m:
                addr_to_syms.setdefault(m.group(2).upper(), []).append(m.group(1))
    return {a: frozenset(s) for a, s in addr_to_syms.items() if len(s) > 1}


def index(groups):
    name_to_groups = collections.defaultdict(list)
    for addr, members in groups.items():
        for name in members:
            name_to_groups[name].append(addr)
    return name_to_groups


def classify(left, right, groups, name_to_groups):
    gl = [groups[a] for a in name_to_groups.get(left, ())]
    gr = [groups[a] for a in name_to_groups.get(right, ())]
    if not gl and not gr:
        return "DET-CHARGED"
    outcomes = {
        (right in gi) or (left in hj)
        for gi in (gl or [frozenset()])
        for hj in (gr or [frozenset()])
    }
    if outcomes == {True}:
        return "DET-FORGIVEN"
    if outcomes == {False}:
        return "DET-CHARGED"
    return "COIN-FLIP"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--map", default=str(DEFAULT_MAP))
    ap.add_argument("--census", default="",
                    help="namecheck_df_census.json-shaped file with rows of "
                         "{target, base, sites, fns}")
    ap.add_argument("--strict", action="store_true",
                    help="exit 1 if any coin-flip pair exists")
    args = ap.parse_args()

    groups = load_groups(args.map)
    name_to_groups = index(groups)
    dup = {n: v for n, v in name_to_groups.items() if len(v) > 1}
    print(f"map {args.map}")
    print(f"  groups {len(groups)}   distinct names {len(name_to_groups)}   "
          f"names at >1 address {len(dup)}")
    if dup:
        worst = max(dup.items(), key=lambda kv: len(kv[1]))
        print(f"  worst duplicated name: {len(worst[1])} addresses  {worst[0][:80]}")

    # DISTINCT unordered pairs -- a pair co-occurring in several groups is one
    # pair, not several, and counting occurrences inflates the denominator.
    pairs = set()
    for members in groups.values():
        ordered = sorted(members)
        for i in range(len(ordered)):
            for j in range(i + 1, len(ordered)):
                pairs.add((ordered[i], ordered[j]))
    counts = collections.Counter()
    flips = []
    for left, right in pairs:
        verdict = classify(left, right, groups, name_to_groups)
        counts[verdict] += 1
        if verdict == "COIN-FLIP" and len(flips) < 40:
            flips.append((left, right))
    print(f"  distinct co-occurring pairs {sum(counts.values())}: "
          f"{counts['DET-FORGIVEN']} deterministic, {counts['COIN-FLIP']} COIN FLIP")
    for left, right in flips:
        print(f"    COIN FLIP  {left[:70]}  |  {right[:70]}")

    census_flips = 0
    if args.census:
        rows = json.loads(Path(args.census).read_text())["rows"]
        pair_counts = collections.Counter()
        site_counts = collections.Counter()
        for row in rows:
            verdict = classify(row["target"], row["base"], groups, name_to_groups)
            pair_counts[verdict] += 1
            site_counts[verdict] += row.get("sites", 0)
        print(f"census {args.census}: {len(rows)} real relocation-name pairs")
        for verdict in ("DET-FORGIVEN", "DET-CHARGED", "COIN-FLIP"):
            print(f"  {verdict:14} pairs {pair_counts[verdict]:6}  "
                  f"sites {site_counts[verdict]}")
        census_flips = pair_counts["COIN-FLIP"]

    if args.strict and (counts["COIN-FLIP"] or census_flips):
        print("REFUSING (--strict): the alias map admits a nondeterministic pair.",
              file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
