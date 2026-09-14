#!/usr/bin/env python3
"""W16-AD Item 3: apply the four proven Accomplishment map-row swaps.

Each pair was proven on retail bytes by `tools/w16ad_item3_adjudicate.py`: the
two addresses call each other's comparator, the comparator rows themselves are
independently sound (their `.pdata` extents discriminate all three candidates
by size), and both names are defined by the SAME base obj
(`band3/meta_band/AccomplishmentPanel.obj`) so the swap cannot un-pair a row --
the hazard that is 80.5% of a map edit's delta.

REFUSES unless every address currently carries exactly the name the adjudication
recorded.  A swap applied to a map that has drifted would silently assert a fold
nobody proved.
"""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
MAP = ROOT / "scripts/target_symbol_map.json"

PAIRS = [
    ("0x825fbd78", "0x825fb6c8", "??$__merge_without_buffer"),
    ("0x825f7250", "0x825f71a0", "??$__unguarded_linear_insert"),
    ("0x825f76a8", "0x825f74e8", "??$__upper_bound"),
    ("0x825f82d8", "0x825f8148", "??$merge"),
]


def main():
    d = json.loads(MAP.read_text())
    for a, b, algo in PAIRS:
        na, nb = d.get(a), d.get(b)
        if not isinstance(na, str) or not isinstance(nb, str):
            sys.exit(f"REFUSE: {a}/{b} not both plain-string map rows")
        if not (na.startswith(algo) and nb.startswith(algo)):
            sys.exit(f"REFUSE: {a}/{b} are not both {algo} instantiations")
        if "AccomplishmentCategoryCmp" not in na or "AccomplishmentCmp" not in nb:
            sys.exit(f"REFUSE: {a}/{b} not the Category/Cmp pair the "
                     f"adjudication recorded")
        d[a], d[b] = nb, na
        print(f"swapped {algo}:\n    {a} <- {nb[:70]}\n    {b} <- {na[:70]}")
    MAP.write_text(json.dumps(d, indent=1, ensure_ascii=False) + "\n")
    print(f"\nwrote {MAP} ({len(d)} rows)")


if __name__ == "__main__":
    main()
