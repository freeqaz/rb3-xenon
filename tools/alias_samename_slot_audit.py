#!/usr/bin/env python3
"""Audit every installed alias membership for proofs that rest on a SAME-NAME slot.

Lane W16-TT (2026-10-07), docs/decomp/W16TT_FOLD_LEADS_ON_RETAIL_2026-10-07.md.

THE GAP.  Both T1 comparators accept a relocation slot whose two target names are
EQUAL without looking at the callee: `icf_alias_build.relocs_agree` does
`if rn == on: continue`, and `icf_pair_adjudicate.chase` returns True for
`survivor == our_name and depth > 0` ("name equality IS the evidence").  The retail
name comes from scripts/target_symbol_map.json, so the slot is only as good as the
map's identification of the callee.  When the map names a callee with OUR spelling
on a body that is not ours, the one slot that discriminates the fold reads as
agreement.  Group 771 was restored this way on 2026-09-15: the map named
0x82773E70 `__destroy_range_aux<reverse_iterator<vector<short>*>>`, but that body
destroys 12-byte elements and ours destroys 2-byte ones (two levels down).

★ W16-TV: both gaps are now closed IN THE TOOLS; this audit compares the
pre-fix policies (base) with the shipped ones (strict).  See run() below.

WHAT THIS RAN (W16-TT's version), per membership (survivor S, folded F):
  base   -- the shipped chase, unchanged (flat T1 first, as the tool does).  A
            REFUTED here is a STALE proof: the evidence changed under it.
  strict -- the same chase, except a same-name slot at depth > 0 whose callee is
            present and non-vacuous on BOTH sides is compared on bytes (the
            depth-0 comparison, recursing through its own slots).  Slots that
            cannot be checked (callee absent or vacuous on either side) keep the
            shipped acceptance, so strict only adds failures that byte evidence
            supports.

    python3 tools/alias_samename_slot_audit.py --out ~/tmp/samename_audit.json
    python3 tools/alias_samename_slot_audit.py --selftest   # 771 must fail strict

Needs a BUILT tree (renamed target objs).  Read-only.
"""
import argparse, json, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import icf_pair_adjudicate as m  # noqa: E402
from test_alias_proof_gaps import (OLD, NEW, under, plant_771, G771_SURVIVOR,  # noqa: E402
                                   G771_FOLDED, SOUND_FOLDED)

# ★ W16-TV (2026-10-07): the strict pass this tool used to bolt on with two
# monkeypatched wrappers is now the SHIPPED behaviour of icf_pair_adjudicate
# (same-name slots read, flat T1 chase-confirmed) and icf_alias_build
# (unnamed retail callee refused by flat T1).  So:
#   base   -- the pre-W16-TV policies (test_alias_proof_gaps.OLD): what the
#             comparators said before the fix.
#   strict -- the shipped policies (NEW).
# Both legs go through icf_pair_adjudicate.membership_verdict, the single
# verdict function the callee-name drift check and the snapshot also use.


def run(groups, tgt, ours, mapped, only=None):
    res = []
    # one fresh memo per membership: see membership_verdict's warning
    for gi, g in enumerate(groups):
        if only is not None and gi not in only:
            continue
        s = g["survivor"]
        for f in g.get("folded", []):
            b, _w, _t = under(OLD, lambda: m.membership_verdict(tgt, ours, s, f, mapped))
            st, why, tr = under(NEW, lambda: m.membership_verdict(tgt, ours, s, f, mapped))
            res.append({"group": gi, "address": g.get("address"), "survivor": s,
                        "folded": f, "base": b, "strict": st, "why": why[:300],
                        "samename_contradicted": [x[2] for x in tr
                                                  if x[1] == "SAMENAME-CONTRADICTED"],
                        "samename_unverified": [x[2] for x in tr
                                                if x[1] == "SAMENAME-UNVERIFIED"]})
    return res


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out")
    ap.add_argument("--selftest", action="store_true")
    a = ap.parse_args()
    groups = json.loads((ROOT / "scripts/symbol_aliases.json").read_text())["groups"]
    tgt, ours = m.load_sides()
    mapped = m.load_mapped()
    mangled = sum(1 for n in tgt if n.startswith("?"))
    if mangled < 1000:
        sys.exit("REFUSING: target objs look PRE-RENAMER (%d mangled names)" % mangled)
    if a.selftest:
        # Positive control: group 771's 2026-09-15 proof (the map then gave the
        # depth-1 callee 0x82773E70 OUR spelling) must pass base and fail strict.
        T, M = plant_771(tgt, mapped)
        flat, _ = under(OLD, lambda: m.adjudicate(T, ours, G771_SURVIVOR, G771_FOLDED, M,
                                                  verbose=False))
        st, _w, _t = under(NEW, lambda: m.membership_verdict(T, ours, G771_SURVIVOR,
                                                             G771_FOLDED, M, {}))
        print("selftest (09-15 map state): pre-W16-TV flat T1 = %s ; shipped = %s" % (flat, st))
        good = flat == "PROVEN" and st != "PROVEN"
        # Negative control: W16-JE's SetObjConcrete<UILabel> must stay PROVEN.
        keep, _w, _t = under(NEW, lambda: m.membership_verdict(
            tgt, ours, groups[0]["survivor"], SOUND_FOLDED, mapped, {}))
        print("negative control (SetObjConcrete<UILabel>): shipped = %s" % keep)
        good = good and keep == "PROVEN"
        print("SELFTEST", "PASS" if good else "FAIL")
        sys.exit(0 if good else 1)
    res = run(groups, tgt, ours, mapped)
    Path(a.out).write_text(json.dumps(res, indent=1))
    import collections
    c = collections.Counter((r["base"], r["strict"]) for r in res)
    print("memberships", len(res), "same-name slot reads", dict(m.SAMENAME_TALLY))
    for k, v in sorted(c.items()):
        print("  base=%-11s strict=%-11s %d" % (k[0], k[1], v))


if __name__ == "__main__":
    main()
