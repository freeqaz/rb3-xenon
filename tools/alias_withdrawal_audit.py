#!/usr/bin/env python3
"""Audit the withdrawal ledger: how much of it would a regeneration undo?

`scripts/symbol_aliases.json` carries withdrawal records that no tool consulted
until lane W8-A.  This answers, with provenance, the question that sizes the
exposure: **of the withdrawals on record, how many would the generator put back
as LIVE memberships if it ran today?**

Run it against a generated candidate file (produced with
`--no-withdrawal-guard`, or by any pre-W8-A build of the generator):

    python3 tools/alias_withdrawal_audit.py --candidate <candidate.json>

⚠ THE ANSWER IS A PROPERTY OF THE INPUTS, NOT A CONSTANT.  The generator's
candidate supply is a CENSUS SNAPSHOT of this tree's objects, so the number
moves with the tree, with the map, and with how fresh the census is.  Measured
on one tree (2fc2552a, 1,205 objs) the same audit reads **81** from the archived
2026-07-31 census and **110** from a census taken the same hour -- the stale
inputs understate by 26%.  Quote the number WITH its inputs or do not quote it.

MAP-COUPLED WITHDRAWALS
-----------------------
Lane W7-D observed that for its `0x823c8908` group, regeneration is blocked by
the MAP FIX and not by the withdrawal: the differing words sit at unrelocated
COMDAT offsets, so the generator's "modulo relocated fields" masking cannot hide
them once the map row is right.  The converse is the hazard: a map-coupled
withdrawal whose map row is NOT yet repaired is one map edit away from
regenerating, and the withdrawal record was the only thing holding it.
`--map-coupled` lists them so the two symptoms of one error stay linked.
"""

import argparse
import collections
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from alias_withdrawals import load_ledger  # noqa: E402

# Withdrawal classes whose stated cause is a MAP identification, not a property
# of the two bodies.  For these the alias and the map row are TWO SYMPTOMS OF
# ONE ERROR (lane W7-C proved exactly that at 0x823c8908), so a map repair and a
# withdrawal are not interchangeable remedies -- you generally need both.
MAP_COUPLED = {
    "MAP_DEFECT_INVERTED_CONCLUSION",
    "SURVIVOR_NAME_INHERITED_FROM_MAP",
    "SURVIVOR_SPELLING_CORRECTED_BY_MAP_REPAIR",
    "SURVIVOR_MISNAMED",
    "WRONG_SURVIVOR_IDENTITY",
    "DISTINCT_ADDRESSES_CANNOT_FOLD",
    "PIGEONHOLE_DISTINCT_RETAIL_ADDRESS",
    "FIXPOINT_ROOT_DIFFERS",
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--ledger", default=str(ROOT / "scripts" / "symbol_aliases.json"))
    ap.add_argument("--candidate", default="",
                    help="a generated alias file to score the ledger against")
    ap.add_argument("--map-coupled", action="store_true",
                    help="list the map-coupled withdrawals and their map status")
    a = ap.parse_args()

    ledger = load_ledger(a.ledger)
    doc = json.loads(Path(a.ledger).read_text())
    groups = doc["groups"]
    live = {(g["survivor"], f) for g in groups for f in g["folded"]}
    live_ad = {(g.get("address"), f) for g in groups for f in g["folded"]}

    print("ledger        : %s" % a.ledger)
    print("groups        : %d (%d carry >=1 withdrawal)"
          % (len(groups), sum(1 for g in groups if g.get("withdrawn"))))
    print("withdrawal records: %d over %d distinct (survivor,spelling)"
          % (len(ledger), len(ledger.by_survivor)))

    # (1) internal consistency: withdrawn AND live in the SAME shipped file
    surv_names = {g["survivor"] for g in groups}
    viol = [w for w in ledger.records
            if (w.survivor, w.spelling) in live or (w.address, w.spelling) in live_ad]
    print("\n-- ledger self-consistency --")
    print("  withdrawn AND still live at the same group: %d" % len(viol))
    print("    ...whose spelling is also another group's SURVIVOR: %d"
          % sum(1 for w in viol if w.spelling in surv_names))
    cc = collections.Counter(w.cls for w in viol)
    for k, v in cc.most_common():
        print("      %-44s %5d" % (k, v))

    # (2) exposure against a candidate regeneration
    if a.candidate:
        cand = json.loads(Path(a.candidate).read_text())["groups"]
        hits = []
        for g in cand:
            for f in g["folded"]:
                w = ledger.lookup(g["survivor"], g.get("address"), f)
                if w is not None:
                    hits.append((w, (g["survivor"], f) in live))
        print("\n-- exposure vs %s --" % a.candidate)
        print("  candidate groups: %d, live memberships: %d"
              % (len(cand), sum(len(g["folded"]) for g in cand)))
        print("  *** withdrawn memberships RE-EMITTED AS LIVE: %d" % len(hits))
        print("      carried from the shipped file's own live set: %d"
              % sum(1 for _, c in hits if c))
        print("      NEWLY re-fabricated by generation           : %d"
              % sum(1 for _, c in hits if not c))
        cc = collections.Counter(w.cls for w, _ in hits)
        for k, v in cc.most_common():
            print("      %-44s %5d" % (k, v))

    # (3) the map-coupled class
    if a.map_coupled:
        tm = json.loads((ROOT / "scripts" / "target_symbol_map.json").read_text())
        by_addr = {k.lower(): v for k, v in tm.items()
                   if isinstance(k, str) and k.lower().startswith("0x")}
        mc = [w for w in ledger.records if w.cls in MAP_COUPLED]
        print("\n-- MAP-COUPLED withdrawals: %d record(s) --" % len(mc))
        print("   (the alias and the map row are two symptoms of one error; a\n"
              "    withdrawal alone does not hold if the map row is still wrong)")
        for w in sorted(mc, key=lambda x: (x.cls, str(x.address))):
            cur = by_addr.get(str(w.address).lower(), "<address not in map>")
            agree = "map==survivor" if cur == w.survivor else "MAP DIFFERS"
            print("\n  %s" % w.cls)
            print("    address  %s" % w.address)
            print("    survivor %s" % w.survivor)
            print("    map says %s   [%s]" % (cur, agree))
            print("    folded   %s" % w.spelling)
            print("    lane     %s" % w.lane)
    return 0


if __name__ == "__main__":
    sys.exit(main())
