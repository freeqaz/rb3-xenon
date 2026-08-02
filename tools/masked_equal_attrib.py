#!/usr/bin/env python3
"""Attribute a `masked_equal_functions` delta between two objdiff reports.

WHY THIS EXISTS
---------------
`masked_equal_functions` drifted three times across waves BV..CC (-21, -1, -2)
with nobody attributing it, and it is a *subtrahend* of the honest proxy that
every lane is priced on (honest = matched_functions - masked_equal_functions).
An unexplained term in the pricing formula is exactly the kind of thing that
turns into a phantom regression six weeks later.

WHAT `masked_equal` ACTUALLY MEANS (read from the fork, not inferred)
--------------------------------------------------------------------
objdiff-cli/src/cmd/report.rs builds `partner_groups`: base symbol index ->
target symbol it paired with. Any group with len > 1 is an OVER-SUBSCRIPTION
(several of our symbols competing for one retail symbol). One member is elected
"owner" (the one that did NOT come from funclet pairing, else lowest index);
every OTHER member is `oversubscribed`. Then:

    if match_percent_normalized == 100.0:
        measures.matched_functions += 1
        if is_oversubscribed:
            measures.masked_equal_functions += 1   # disclosure, a SUBSET

So `masked_equal` counts *surplus* members of duplicate-pairing groups that
nonetheless scored a normalized 100. It is a DISCLOSURE of soft credit, never an
addition to it.

*** SUPERSEDED IN PART, 2026-08-02 (lane DA-4): "surplus" IS NO LONGER RIGHT. ***
The fork was flipped so the disclosure covers EVERY funclet byte-signature
pairing, not just the over-subscribed surplus of a group: masked_equal_functions
1,096 -> 22,640 (52.10% of matched_functions), honest 42,358 -> 20,814, with NO
score key moving. Read the sentence above as the OLD semantics.
What this means for THIS tool:
  * Its ARITHMETIC AND CLASSES ARE UNAFFECTED. It attributes a delta between two
    reports, and both reports are produced by one binary, so the same-ruler
    assumption holds. RESOLVED / LOST_CREDIT / VANISHED / NEW_SURPLUS / NEW_ROW
    all still mean what they say.
  * ⚠ But NEVER feed it an OLD.json produced before the flip and a NEW.json
    produced after -- that is a cross-ruler comparison and it would report
    ~+21,500 NEW_SURPLUS rows from an untouched tree. tools/ab_measure.py has a
    same-ruler guard for exactly this (373d17c6); this tool does NOT.
  * "NEW_SURPLUS ... working as designed" is now the COMMON case, not the rare
    one, since most funclet pairings are supply-backed and merely arbitrary in
    per-row attribution.
See docs/decomp/RULER_CHANGE_2026-08-02.md.

=> A masked_equal DROP is therefore ambiguous on its face and must be split:
     RESOLVED     still 100, flag gone  -> duplicate pairing resolved. GOOD:
                                           honest gains a genuinely real point.
     LOST_CREDIT  fell below 100        -> the function actually stopped
                                           matching. A real regression hiding
                                           inside a "good-looking" masked drop.
     VANISHED     row absent entirely   -> unit/split churn, not a score change.
   and a masked_equal RISE likewise:
     NEW_SURPLUS  climbed to 100 but as the surplus member -> matched +1 while
                  honest correctly declines to count it. Working as designed.
     NEW_ROW      appeared from nowhere -> unit/split churn.

The headline number cannot distinguish RESOLVED from LOST_CREDIT, and those have
opposite signs in meaning. That is the whole point of this script.

USAGE
    python3 tools/masked_equal_attrib.py OLD.json NEW.json [--all]
    python3 tools/masked_equal_attrib.py --self-test
"""

import json
import sys
from collections import Counter


def index(path):
    """(unit, symbol) -> (normalized_pct, masked_equal_bool, size)."""
    with open(path) as fh:
        rep = json.load(fh)
    out = {}
    for unit in rep["units"]:
        for fn in unit.get("functions") or []:
            out[(unit["name"], fn["name"])] = (
                fn.get("match_percent_normalized"),
                bool(fn.get("masked_equal")),
                int(fn.get("size", 0)),
            )
    return out, rep["measures"]


def classify(old, new):
    was = {k for k, v in old.items() if v[1]}
    now = {k for k, v in new.items() if v[1]}
    rows = []
    for k in sorted(was - now):
        if k not in new:
            rows.append(("VANISHED", k, old[k], None))
        elif new[k][0] == 100.0:
            rows.append(("RESOLVED", k, old[k], new[k]))
        else:
            rows.append(("LOST_CREDIT", k, old[k], new[k]))
    for k in sorted(now - was):
        if k not in old:
            rows.append(("NEW_ROW", k, None, new[k]))
        else:
            rows.append(("NEW_SURPLUS", k, old[k], new[k]))
    return rows


def render(rows, measures_old, measures_new, show_all):
    def pct(v):
        return "ABSENT" if v is None else f"{v[0]:.4g}"

    mo = measures_old.get("masked_equal_functions", 0)
    mn = measures_new.get("masked_equal_functions", 0)
    ho = measures_old["matched_functions"] - mo
    hn = measures_new["matched_functions"] - mn
    print(
        f"masked_equal {mo} -> {mn}  (delta {mn - mo:+d})   "
        f"matched {measures_old['matched_functions']} -> {measures_new['matched_functions']}"
        f"  ({measures_new['matched_functions'] - measures_old['matched_functions']:+d})   "
        f"honest {ho} -> {hn}  ({hn - ho:+d})"
    )
    tally = Counter(r[0] for r in rows)
    for kind in ("RESOLVED", "LOST_CREDIT", "VANISHED", "NEW_SURPLUS", "NEW_ROW"):
        if tally[kind]:
            print(f"  {kind:12s} {tally[kind]}")
    if not rows:
        print("  (no membership change)")
    limit = len(rows) if show_all else 40
    if rows:
        print("\n  kind         unit / symbol                                   old -> new   size")
    for kind, key, o, n in rows[:limit]:
        print(f"  {kind:12s} {key[0][:34]:34s} {key[1][:26]:26s} "
              f"{pct(o):>6} -> {pct(n):>6}  {(n or o)[2]}")
    if len(rows) > limit:
        print(f"  ... {len(rows) - limit} more (--all)")

    if tally["LOST_CREDIT"]:
        print(f"\n!! {tally['LOST_CREDIT']} LOST_CREDIT row(s): these fell BELOW 100. A masked_equal")
        print("   drop that looks benign is hiding a real regression here. Investigate before")
        print("   crediting the honest-proxy movement.")
    else:
        print("\nOK: no LOST_CREDIT rows -- every masked_equal departure kept a normalized 100,")
        print("    i.e. duplicate pairings genuinely resolved rather than credit disappearing.")


def self_test():
    """The detector must distinguish RESOLVED from LOST_CREDIT, and must NOT
    invent transitions for rows that merely changed percentage without ever
    being masked (the vacuity failure mode this project keeps hitting)."""
    old = {
        ("u", "resolved"): (100.0, True, 40),
        ("u", "lost"): (100.0, True, 40),
        ("u", "vanished"): (100.0, True, 40),
        ("u", "newsurplus"): (99.9, False, 40),
        ("u", "noise"): (73.0, False, 40),   # negative control: never masked
        ("u", "steady"): (100.0, True, 40),  # negative control: stays masked
    }
    new = {
        ("u", "resolved"): (100.0, False, 40),
        ("u", "lost"): (61.0, False, 40),
        ("u", "newsurplus"): (100.0, True, 40),
        ("u", "noise"): (95.0, False, 40),   # moved a lot, still not masked
        ("u", "steady"): (100.0, True, 40),
        ("u", "newrow"): (100.0, True, 32),
    }
    got = {(k[1], kind) for kind, k, _, _ in classify(old, new)}
    expect = {
        ("resolved", "RESOLVED"),
        ("lost", "LOST_CREDIT"),
        ("vanished", "VANISHED"),
        ("newsurplus", "NEW_SURPLUS"),
        ("newrow", "NEW_ROW"),
    }
    if got != expect:
        print(f"SELF-TEST FAIL\n  missing: {expect - got}\n  spurious: {got - expect}")
        return 1
    print("SELF-TEST PASS: RESOLVED/LOST_CREDIT/VANISHED/NEW_SURPLUS/NEW_ROW all fire; "
          "an unmasked row that moved 73->95 and a steady masked row add nothing spurious")
    return 0


def main(argv):
    if "--self-test" in argv:
        return self_test()
    args = [a for a in argv[1:] if not a.startswith("--")]
    if len(args) != 2:
        print(__doc__)
        return 2
    old, mo = index(args[0])
    new, mn = index(args[1])
    render(classify(old, new), mo, mn, "--all" in argv)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
