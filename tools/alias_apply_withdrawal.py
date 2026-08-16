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

★ SUPERSEDED FOR 6 OF THE 7 -- lane GROUNDED-2, 2026-08-16 (rb3-xenon
`79d74650`).  The paragraph above is preserved as written; two of its claims are
now measured false, and `--fix-grounded1` MUST NOT be re-run against those six
records, which no longer exist (they are `restored[].superseded_record` now).

  * "a UNIFORM +8 B across the whole __uninitialized_copy /
    _M_allocate_and_copy family ... ONE shared source defect in the STLport copy
    helpers" -- there is no such defect.  Lane STLPORT-1 (`ff832b50`) found the
    +8 was `tools/coff_bodies_ext.py` billing the SUCCESSOR's 8-byte EH prefix to
    the preceding function.  The counts (95 pairs, 52 at retail-96, 43 at
    retail-100) reproduce; only the cause was wrong.
  * "the membership stays WITHDRAWN ... do not re-add without positive fold
    evidence" -- the positive evidence was then produced.  objdiff at the
    instruction level, a raw compare against band.exe over the full COMDAT span,
    and probe_icf_foldtest all read FOLD on 7 of 7 (the six plus the
    never-withdrawn control), and retail's own symbols.txt places an 8-byte
    `except_data` object at A+extent with the funclet at A+extent+8, so the
    "96 vs 104" was the FUNCLET PREFIX, never a body size.  Restoring the six
    measured +1,728 B / 12 rows UP / 0 DOWN.

  The METHODOLOGICAL ruling -- keep the size test inside one build -- is
  UNAFFECTED and was right: our(S) == our(F) raw-identically at all six.  What
  the ruling could not do is prove a fold, and that is what needed the retail
  side measured with the RIGHT ruler.  Lane report:
  `decomp-synth docs/plans/il-witness/GROUNDED2_RESTORATION_2026-08-16.md`.
"""
import argparse, collections, json, os, sys
from pathlib import Path

LANE = "ALIAS-2 2026-08-16"

# ⚠ SUPERSEDED TEMPLATE -- kept verbatim so the records it already stamped can be
# traced back to their generator, and so a reader who greps the note text lands
# here.  Do NOT stamp it again: its "UNIFORM +8 B / shared STLport source defect"
# clause was refuted by STLPORT-1 (`ff832b50`) and its "stays WITHDRAWN" clause by
# GROUNDED-2 (`79d74650`).  `--fix-grounded1` now refuses; see the docstring.
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
        # Refuses by design since GROUNDED-2 (2026-08-16).  Re-stamping G1_NOTE
        # would re-assert a refuted cause on the records it can still reach, and
        # on a RESTORED group it would write a `withdrawn` entry for a spelling
        # that is live in `folded`.  Left as an explicit refusal rather than
        # deleted, so an operator who reaches for the flag reads why.
        sys.exit(
            "--fix-grounded1 is RETIRED. Six of the seven records it targeted were\n"
            "restored on measurement (rb3-xenon 79d74650, +1,728 B) and the note it\n"
            "stamps carries a cause STLPORT-1 refuted (ff832b50). See this module's\n"
            "docstring and docs/plans/il-witness/GROUNDED2_RESTORATION_2026-08-16.md."
        )

    if a.dry_run:
        print("DRY RUN -- no write"); return
    ali.write_text(json.dumps(doc, indent=1) + "\n")
    print("wrote %s" % ali)


if __name__ == "__main__":
    main()
