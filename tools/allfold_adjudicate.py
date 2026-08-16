#!/usr/bin/env python3
"""Adjudicate the ALL_FOLD stratum on RETAIL BYTES (lane ALIAS-2, 2026-08-16).

    python3 tools/allfold_adjudicate.py --wt <repo-with-built-objs> --sample 200

THE STANDING RULING IT TESTS
----------------------------
RULERGAP-1's 247,376 B `ALL_FOLD` stratum "stays unaliased -- CLASSIFIED, not
PROVEN" (coordinator ruling, CAMPAIGN_STATE_2026-08-14 §3c). A row is ALL_FOLD
when EVERY charged relocation-name site in it carries the classifier's `FOLD`
label; matched_code is all-or-nothing per row, so one unclosable site withholds
the whole row.

WHY THE LABEL IS NOT THE PROOF -- and this is checkable, not rhetorical.  The
classifier's own vocabulary (`FOLD` vs `GENUINE: different size` vs `GENUINE:
same size, different code`) shows it decides on SIZE plus CODE. That is exactly
the comparison the template-twin vacuity defeats: `vector<Foo>::erase` and
`vector<Bar>::erase` have identical machine bytes and differ ONLY in the
destructor they call, so "same size, same code" is consistent with two DIFFERENT
functions. `verdict()` additionally compares relocation TARGET NAMES and, failing
that, falls through to the ICF fixpoint (L2), full-word compare (L3), our-side
COMDAT identity (L4) and retail's internal inconsistency (L5).

So this asks the ruling's question directly: of the sites the classifier calls
FOLD, how many are PROVEN by an instrument that cannot be fooled by a twin?

⚠ THIS TOOL INSTALLS NOTHING. Adding an alias lifts matched_code BY
CONSTRUCTION, which is why an unproven alias is an integrity hazard rather than a
win. The output is an argument to be weighed, not a patch.

⚠ The pairs file is a DATED artifact (relocname-audit-2026-08-06, tree
a236686e); the tree has moved since. It is used only as a CANDIDATE LIST -- every
verdict here is recomputed against today's objs.
"""
import argparse, collections, json, os, random, sys
from pathlib import Path

PAIRS = ("/home/free/code/milohax/decomp-bench/archive/harvest/"
         "relocname-audit-2026-08-06/pairs_folded2.json")
PROVEN = {"L1_T1", "L2_RECURSIVE", "L3_EXACT", "L4_OURSIDE", "L5_INCONSISTENCY"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wt", required=True)
    ap.add_argument("--pairs", default=PAIRS)
    ap.add_argument("--sample", type=int, default=200)
    ap.add_argument("--seed", type=int, default=20260816)
    a = ap.parse_args()
    wt = Path(a.wt).resolve()

    rows = json.load(open(a.pairs))
    bysym = collections.defaultdict(list)
    for r in rows:
        bysym[(r["unit"], r["sym"])].append(r)

    def kind(r):
        f = r.get("fold", "")
        return "FOLD" if f.startswith("FOLD") else (
            "GENUINE" if f.startswith("GENUINE") else "OTHER")

    allfold = {k: v for k, v in bysym.items() if {kind(r) for r in v} == {"FOLD"}}
    print("ALL_FOLD rows (every charged site classified FOLD): %d of %d rows"
          % (len(allfold), len(bysym)))
    pairs = collections.Counter()
    for k, v in allfold.items():
        for r in v:
            if r["target_symbol"] != r["base_symbol"]:
                pairs[(r["target_symbol"], r["base_symbol"])] += 1
    print("distinct (target,base) pairs inside ALL_FOLD rows: %d over %d sites"
          % (len(pairs), sum(pairs.values())))

    # Import the module from THIS tree (whose main() is guarded) but read objs
    # from --wt, which may be another checkout.
    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from alias_forgiveness_audit import Sides
    S = Sides(wt)

    # byte-weight the sample by site count: the pairs that matter are the ones
    # charged most often, but include a random tail so the answer is not just
    # the head's answer.
    top = [p for p, _ in pairs.most_common(a.sample // 2)]
    rest = [p for p in pairs if p not in set(top)]
    random.Random(a.seed).shuffle(rest)
    sel = top + rest[: a.sample - len(top)]
    print("adjudicating %d pairs (%d most-charged + %d random tail)\n"
          % (len(sel), len(top), len(sel) - len(top)))

    memo = {}
    verd = collections.Counter()
    sites = collections.Counter()
    examples = collections.defaultdict(list)
    for i, (tn, bn) in enumerate(sel):
        try:
            v, why = S.verdict(tn, bn, memo)
        except Exception as e:                     # never let one pair kill the run
            v, why = "ERROR", repr(e)[:80]
        c = "PROVEN" if v in PROVEN else v
        verd[c] += 1; sites[c] += pairs[(tn, bn)]
        if len(examples[c]) < 4:
            examples[c].append((pairs[(tn, bn)], tn, bn, v, str(why)[:100]))
        if i % 25 == 0:
            print("  %d/%d" % (i, len(sel)), flush=True)

    n = sum(verd.values())
    print("\nVERDICTS ON THE ALL_FOLD SAMPLE (pairs, and the sites they carry)")
    for c, k in verd.most_common():
        print("  %-14s %4d pairs (%5.1f%%)  %5d charged sites" % (c, k, 100.0 * k / n, sites[c]))
    print("\nexamples per class:")
    for c in verd:
        print(" [%s]" % c)
        for s, tn, bn, v, why in examples[c]:
            print("   %3d sites  %s\n            <- %s\n            %s | %s"
                  % (s, tn[:88], bn[:88], v, why))


if __name__ == "__main__":
    main()
