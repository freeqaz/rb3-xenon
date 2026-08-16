#!/usr/bin/env python3
"""Adjudicate EVERY installed alias membership on RETAIL BYTES (lane ALIAS-2).

    python3 tools/alias_membership_adjudicate.py --wt <repo-with-built-objs> \
        --out ~/tmp/alias2_memberships.json

For each group in scripts/symbol_aliases.json and each folded spelling F in it,
this asks `verdict(survivor, F)` -- the layered L1..L5 adjudicator -- and records
the class and reason. It takes a MEMBERSHIP, not a call site, so unlike the
name-keyed site census it has no blind spot for anonymous rows: the question
"is this fold real?" is answered from the two bodies alone.

Pair this with tools/alias_group_ablate.py (which says WHICH group forgives WHICH
bytes) to price every forgiven byte by the strength of the evidence under it.

⚠ Read the objs from a tree that is NOT being mutated. Sides() also reads
scripts/symbol_aliases.json to build its equivalence closure, so pointing this at
a tree where an ablation sweep is rewriting that file yields an inconsistent
closure. Point it at a stable checkout.

⚠ A worktree's reflinked target objs are PRE-RENAMER, so every retail mangled
name reads 'absent' until it has been built once. This script asserts a
known-name sanity check before reporting anything, because a vacuous run here
returns 'NEEDS_SOURCE: absent' for everything -- a decisive-looking negative that
is pure instrument failure.
"""
import argparse, collections, json, os, sys
from pathlib import Path

PROVEN = {"L1_T1", "L2_RECURSIVE", "L3_EXACT", "L4_OURSIDE", "L5_INCONSISTENCY"}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wt", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    wt = Path(a.wt).resolve()

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from alias_forgiveness_audit import Sides
    S = Sides(wt)

    # ---- anti-vacuity gate: the renamer must have run in this tree ----------
    n_t = len(S.traw); n_o = len(S.oraw)
    mangled = sum(1 for n in S.traw if n.startswith("?"))
    print("target bodies %d (mangled %d) | our bodies %d" % (n_t, mangled, n_o))
    if mangled < 1000:
        sys.exit("REFUSING: only %d mangled names among %d target bodies -- the "
                 "target objs look PRE-RENAMER. Every verdict would read 'absent'."
                 % (mangled, n_t))

    groups = json.loads((wt / "scripts/symbol_aliases.json").read_text())["groups"]
    memo, out = {}, []
    cls = collections.Counter()
    for i, g in enumerate(groups):
        surv = g["survivor"]
        for f in g.get("folded", []):
            try:
                v, why = S.verdict(surv, f, memo)
            except Exception as e:
                v, why = "ERROR", repr(e)[:120]
            c = "PROVEN" if v in PROVEN else v
            cls[c] += 1
            out.append({"i": i, "survivor": surv, "folded": f,
                        "verdict": v, "cls": c, "why": str(why)[:200]})
        if i % 100 == 0:
            print("  group %d/%d (%d memberships)" % (i, len(groups), len(out)), flush=True)

    json.dump(out, open(os.path.expanduser(a.out), "w"))
    print("\nMEMBERSHIP VERDICTS (%d memberships over %d groups)" % (len(out), len(groups)))
    for c, n in cls.most_common():
        print("  %-14s %6d (%5.2f%%)" % (c, n, 100.0 * n / max(1, len(out))))
    print("wrote %s" % a.out)


if __name__ == "__main__":
    main()
