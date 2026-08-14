#!/usr/bin/env python3
"""incomplete_group_adjudicate.py -- adjudicate the incomplete-group candidates on RETAIL BYTES.

Lane INCOMPLETE-1. Consumes the enumeration from incomplete_group_census.py and
decides each candidate with the ALREADY-VALIDATED comparators, so a verdict here
is the verdict the shipped generator would reach:

    icf_alias_build.relocs_agree   flat T1 (literal relocation-target names)
    icf_pair_adjudicate.chase      recursive T1 (name equality closed under folds)
    icf_pair_adjudicate.family     pigeonhole (N of ours onto 1 retail address)

⇒ The NOVELTY of this lane is the ENUMERATOR, not the comparator. ONMSG-1's
finding was that a pairwise comparator does not ENUMERATE a fold class
exhaustively; it verifies one fine. So candidates are supplied from a structure
the comparator never looked at (objdiff's charged relocation-name sites, with
both forgiveness rules applied), and each is then put through the existing gate.

GATES, in the order they can refute:

  G0 SIZE      different-size COMDATs cannot fold. Checked first and for free
               (masked bodies of different length compare unequal).
  G1 RESIDENCY the folded spelling F must NOT be map-resident at a DIFFERENT LIVE
               address. This is lane T1-AUDIT's finding (190 spellings removed
               from 40 groups): where the map places F on its own live address,
               the alias asserts retail has ONE body where the map says TWO, and
               the remedy is a MAP ROW REPAIR, which an alias would foreclose.
  G2 VACUITY   a body that is mostly relocated fields carries no information;
               ⚠ scoping note (lane FATAL-1): when a body has NO relocation at
               all, nothing is masked and byte identity IS /OPT:ICF's complete
               criterion -- the hazard cannot apply.
  G3 IDENTITY  retail bytes at addr(S) vs our compiled body for F: masked bytes
               equal, relocation (offset,type) sequence equal, and relocation
               TARGET NAMES equal -- literally (flat T1) or closed under already
               proven folds (chase). Raw body comparison is NOT used anywhere:
               it is silently vacuous, since two COMDATs differing only in
               relocation TARGETS read as "different" (lane ALIASAUDIT-2 flipped
               7 of 17 verdicts on exactly that).

MERGE candidates (both names map-resident, in different groups) are adjudicated
by a DIFFERENT and stricter question and reported separately -- see --merge.
"""

import argparse
import collections
import glob
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from icf_alias_build import collect, relocs_agree, vacuous, placeholder  # noqa: E402
from icf_pair_adjudicate import chase, family  # noqa: E402
from incomplete_group_census import charged_pairs, load_groups  # noqa: E402


def load_map():
    m = json.load(open(os.path.join(ROOT, "scripts/target_symbol_map.json")))
    name2addr = collections.defaultdict(set)
    for a, n in m.items():
        for x in (n if isinstance(n, list) else [n]):
            if x:
                name2addr[x].add(a)
    return m, name2addr


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--merge", action="store_true", help="adjudicate the MERGE class instead")
    ap.add_argument("--out", default="/home/free/tmp/incomplete_verdicts.json")
    args = ap.parse_args()

    tgt = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/obj/**/*.obj"), recursive=True)), "t")
    ours = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/src/**/*.obj"), recursive=True)), "o")
    owner, groups = load_groups(os.path.join(ROOT, "scripts/symbol_aliases.json"))
    surv = {g["survivor"]: i for i, g in enumerate(groups)}
    _m, name2addr = load_map()
    mapped = set(name2addr)

    sites, victims, _ = charged_pairs(tgt, ours)
    for p in [p for p in sites if placeholder(p[0]) or placeholder(p[1])]:
        del sites[p]
    for p in list(sites):
        gi, gj = owner.get(p[0]), owner.get(p[1])
        if gi is not None and gi == gj:
            del sites[p]

    rep = json.load(open(os.path.join(ROOT, "build/45410914/report.json")))
    mpn, fuzzy, size = {}, {}, {}
    for u in rep["units"]:
        for f in u.get("functions", []):
            mpn[f["name"]] = float(f["match_percent_normalized"])
            fuzzy[f["name"]] = float(f.get("fuzzy_match_percent", 0.0))
            size[f["name"]] = int(f["size"])

    if args.merge:
        cand = [p for p in sites if owner.get(p[0]) is not None and owner.get(p[1]) is not None]
    else:
        cand = [p for p in sites
                if p[0] in surv and owner.get(p[1]) is None]

    verdicts = []
    tally = collections.Counter()
    for (rn, on) in sorted(cand, key=lambda p: -sites[p]):
        rec = {"retail": rn, "ours": on, "sites": sites[(rn, on)],
               "group": owner.get(rn), "rows": sorted(victims[(rn, on)])}
        rt, ob = tgt.get(rn), ours.get(on)

        # G1 RESIDENCY -- for MERGE both are resident by definition; report addrs.
        addr_f = sorted(name2addr.get(on, ()))
        addr_s = sorted(name2addr.get(rn, ()))
        rec["addr_survivor"], rec["addr_folded"] = addr_s, addr_f

        if not args.merge and addr_f:
            rec["verdict"] = "REFUTED_map_resident"
            rec["why"] = ("our spelling is itself map-resident at %s; an alias would assert "
                          "one retail body where the map asserts two (lane T1-AUDIT). "
                          "Remedy is a map-row adjudication, not a membership." % ",".join(addr_f))
            tally[rec["verdict"]] += 1
            verdicts.append(rec)
            continue

        if rt is None or ob is None:
            rec["verdict"] = "UNDECIDABLE_absent"
            rec["why"] = "no %s body compiled/pinned" % ("retail" if rt is None else "our")
            tally[rec["verdict"]] += 1
            verdicts.append(rec)
            continue

        rec["size_retail"], rec["size_ours"] = rt[2], ob[2]
        rec["nrel"] = len(ob[1])

        # G0 SIZE
        if rt[2] != ob[2]:
            rec["verdict"] = "REFUTED_size"
            rec["why"] = "different-size COMDATs cannot fold (%d vs %d B)" % (rt[2], ob[2])
            tally[rec["verdict"]] += 1
            verdicts.append(rec)
            continue

        # G2 VACUITY -- with FATAL-1's scoping note: nrel==0 => nothing masked.
        vac = (vacuous(rt) or vacuous(ob)) and len(ob[1]) > 0
        if vac:
            f = family(tgt, ours, on, rn)
            if len(f["retail_family"]) == 1 and f["our_slot0_matches_retail"] and f["excluded_by_slot0"]:
                rec["verdict"] = "PROVEN_family_thunk"
                rec["why"] = ("body is vacuity-floored but the pigeonhole decides it: %d of our "
                              "spellings share this body+slot0-callee and retail has exactly ONE "
                              "address for them (%d body-twins excluded by slot0)."
                              % (len(f["our_family"]), len(f["excluded_by_slot0"])))
                rec["our_family"] = f["our_family"][:12]
            else:
                rec["verdict"] = "UNDECIDABLE_vacuous"
                rec["why"] = "masked body carries too little unmasked information to decide"
            tally[rec["verdict"]] += 1
            verdicts.append(rec)
            continue

        # G3 IDENTITY
        if rt[0] != ob[0]:
            rec["verdict"] = "REFUTED_bytes"
            rec["why"] = "retail bytes at addr(S) differ from our body for F (relocation-masked)"
            tally[rec["verdict"]] += 1
            verdicts.append(rec)
            continue

        if relocs_agree(rt, ob, mapped, strict=True):
            rec["verdict"] = "PROVEN_flatT1"
            rec["why"] = ("retail bytes at %s are identical to our body for F modulo relocated "
                          "fields, and all %d relocation TARGET NAMES agree literally."
                          % (",".join(addr_s) or "addr(S)", len(ob[1])))
            tally[rec["verdict"]] += 1
            verdicts.append(rec)
            continue

        out = []
        if chase(tgt, ours, rn, on, mapped, out=out):
            rec["verdict"] = "PROVEN_chase"
            rec["why"] = ("recursive T1: identical modulo relocated fields, with every differing "
                          "relocation target itself resolving to a proven fold "
                          "(/OPT:ICF is iterative, so a folded callee is the NORMAL case for a "
                          "template family).")
            rec["chase_notes"] = out[:8]
            tally[rec["verdict"]] += 1
            verdicts.append(rec)
            continue

        f = family(tgt, ours, on, rn)
        if len(f["retail_family"]) == 1 and len(f["our_family"]) > 1 and f["excluded_by_slot0"]:
            rec["verdict"] = "PROVEN_family"
            rec["why"] = ("pigeonhole: %d of our spellings share this body+slot0-callee and retail "
                          "has exactly ONE address for them." % len(f["our_family"]))
            rec["our_family"] = f["our_family"][:12]
            tally[rec["verdict"]] += 1
            verdicts.append(rec)
            continue

        rec["verdict"] = "REFUTED_relocs"
        rec["why"] = "relocation targets disagree and the disagreement does not close under proven folds"
        rec["chase_notes"] = out[:8]
        tally[rec["verdict"]] += 1
        verdicts.append(rec)

    # price the proven set
    def closable(recs):
        rows = set()
        for r in recs:
            rows |= set(r["rows"])
        return [x for x in rows if fuzzy.get(x, 100.0) < 100.0 and mpn.get(x, 0.0) >= 100.0]

    print("=" * 80)
    print("ADJUDICATION -- %s class  (%d candidate pairs)"
          % ("MERGE" if args.merge else "MEMBERSHIP(survivor-side)", len(cand)))
    print("=" * 80)
    for k, v in tally.most_common():
        recs = [r for r in verdicts if r["verdict"] == k]
        cl = closable(recs)
        print("  %-26s %4d pairs  %5d sites  %4d closable rows  %8d B"
              % (k, v, sum(r["sites"] for r in recs), len(cl),
                 sum(size.get(x, 0) for x in cl)))
    prov = [r for r in verdicts if r["verdict"].startswith("PROVEN")]
    cl = closable(prov)
    print("\n  => PROVEN %d pairs / %d closable rows / %d B"
          % (len(prov), len(cl), sum(size.get(x, 0) for x in cl)))
    json.dump(verdicts, open(args.out, "w"), indent=1)
    print("wrote %s" % args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
