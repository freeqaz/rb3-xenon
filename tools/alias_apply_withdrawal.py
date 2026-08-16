#!/usr/bin/env python3
"""Apply ALIAS-2's withdrawals to scripts/symbol_aliases.json.

    python3 tools/alias_apply_withdrawal.py --wt <worktree> \
        --withdraw ~/tmp/alias2_withdraw.json [--fix-grounded1] [--dry-run]

WITHDRAWAL IS PER-MEMBERSHIP, NEVER PER-GROUP.  The largest alias block in the
tree (`??2CriticalSection@@SAPAXI@Z`, 125 spellings) contains 4 refuted
memberships. Withdrawing the group would destroy ~73 kB of separately-proven
credit to remove 4 bad spellings; withdrawing the memberships removes exactly
what is refuted.

GROUPS ARE KEPT, NEVER PRUNED, even if emptied -- per `a745039e`, a
zero-forgiveness spelling becomes live again as porting advances, and that prune
cost +94,616 B to reverse. Each removal is recorded in the group's `withdrawn`
list so a future generator cannot silently re-propose it.

--fix-grounded1 CORRECTS (it does not reverse) the stated reason on 7 of
GROUNDED-1's 8 withdrawal records.  Those records say the fold is refuted because
"retail's instantiation is 96/100 B against our const 104/108 B -- two COMDATs of
different size cannot fold, so the alias was forgiving our use of the wrong
overload".  Re-measured WITHIN OUR BUILD, our survivor and our folded spelling
are the SAME size in all 7 (104/104 and 108/108); the gap is between retail and
us, not between the two spellings, and it is a UNIFORM +8 B across the whole
__uninitialized_copy / _M_allocate_and_copy family.  So the recorded diagnosis
("we instantiate a different overload") is false and would send the next lane to
change a const-ness that is not the defect.

⚠ The correction does NOT restore the aliases and does NOT change the metric.
Refuting a refutation returns a pair to UNPROVEN, not to PROVEN, and re-adding an
alias lifts matched_code BY CONSTRUCTION -- the hazard direction. The 8th record
(`Keys<Quat>::Remove`, KeyLessEq vs KeyGreaterEq) is CONFIRMED and left untouched:
our KeyLessEq is 176 B and our KeyGreaterEq 192 B, different bodies, so those
callees cannot fold.
"""
import argparse, collections, json, os, sys
from pathlib import Path

LANE = "ALIAS-2 2026-08-16"

G1_NOTE = (
    "CORRECTED by %s. The original reason -- 'different body SIZE (%s) -- cannot "
    "be one COMDAT ... we instantiate a different overload' -- compared RETAIL's "
    "survivor against OUR folded spelling, which are two different builds. "
    "Measured WITHIN our build, our(survivor) == our(folded) == %s B, so the two "
    "spellings are NOT different-sized COMDATs and that argument does not refute "
    "the fold. The real divergence is that our STLport emits the whole "
    "__uninitialized_copy / _M_allocate_and_copy family UNIFORMLY +8 B vs retail "
    "(52 of 57 and 43 of 47 size-differing same-name pairs). ⇒ This is ONE shared "
    "source defect in the STLport copy helpers, NOT a per-pair wrong overload. "
    "The membership stays WITHDRAWN because refuting a refutation yields UNPROVEN, "
    "not PROVEN; do not re-add it without positive fold evidence."
) % (LANE, "%s", "%s")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wt", required=True)
    ap.add_argument("--withdraw", required=True)
    ap.add_argument("--fix-grounded1", action="store_true")
    ap.add_argument("--dry-run", action="store_true")
    a = ap.parse_args()
    wt = Path(a.wt).resolve()
    ali = wt / "scripts/symbol_aliases.json"
    doc = json.loads(ali.read_text())
    groups = doc["groups"]

    dec = json.load(open(os.path.expanduser(a.withdraw)))["decisive"]
    byg = collections.defaultdict(list)
    for d in dec:
        byg[d["i"]].append(d)

    removed = kept_missing = 0
    for i, xs in byg.items():
        g = groups[i]
        fold = list(g.get("folded", []))
        rec = g.setdefault("withdrawn", [])
        for x in xs:
            if x["folded"] in fold:
                fold.remove(x["folded"]); removed += 1
            else:
                kept_missing += 1
            rec.append({"spelling": x["folded"], "lane": LANE,
                        "class": x["decisive"], "why": x["detail"],
                        "note": ("Refuted WITHIN OUR BUILD: our own compiler gives "
                                 "these two spellings (or their callees) "
                                 "different-sized COMDATs, which cannot be one "
                                 "COMDAT under /OPT:ICF in any build. Do NOT "
                                 "re-add.")})
        g["folded"] = fold
    print("memberships removed: %d   (already absent: %d) across %d groups"
          % (removed, kept_missing, len(byg)))

    if a.fix_grounded1:
        n = 0
        for g in groups:
            for r in g.get("withdrawn", []):
                if r.get("lane", "").startswith("GROUNDED-1") and \
                        "different body SIZE" in r.get("why", ""):
                    r["superseded_by"] = LANE
                    r["note"] = G1_NOTE % (r.get("why", ""), "same")
                    n += 1
        print("GROUNDED-1 records corrected (reason only, alias NOT restored): %d" % n)

    if a.dry_run:
        print("DRY RUN -- no write"); return
    ali.write_text(json.dumps(doc, indent=1) + "\n")
    print("wrote %s" % ali)


if __name__ == "__main__":
    main()
