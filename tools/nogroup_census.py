#!/usr/bin/env python3
"""nogroup_census.py -- classify the charged relocation-name pairs that touch NO alias group.

Lane NOGROUP-1 (2026-08-14). Lane INCOMPLETE-1 (5f44f30f) censused every charged
relocation-name pair tree-wide, harvested the INCOMPLETE-GROUP slice (+60,700 B),
and explicitly flagged the remainder as a different vein it did not work:

    FRESH (neither name in any group)  1,393 pairs / 2,123 sites

By construction each FRESH pair is a charge where NEITHER side is already aliased,
so each is one of: a fold nobody has proven, a WRONG MAP NAME, or a GENUINE SOURCE
DEFECT.  Those have different correct actions -- install an alias / repair a map row
/ fix our source -- so the CLASSIFICATION is the deliverable and any harvest is
secondary.

THE DISCRIMINATOR
-----------------
Every FRESH pair is (S, F) where S is the retail-side callee name and F is ours.  S
is necessarily a MAP name: dtk spells a target-side callee with a mangled name only
when obj_target_symbol_renamer rewrote it from scripts/target_symbol_map.json, so a
non-placeholder S is a map claim about a specific address A.  That makes every pair
an auditable assertion.  Two independent tests decide it:

    T_F   retail bytes at addr(S)  ==  our compiled body for F
    T_S   retail bytes at addr(S)  ==  our compiled body for S

both relocation-MASKED with relocation TARGET NAMES compared (icf_alias_build's
relocs_agree, strict), because a raw body comparison is silently vacuous -- two
COMDATs differing only in relocation targets read as "different", and lane
ALIASAUDIT-2 flipped 7 of 17 verdicts on exactly that.

    T_F & T_S    FOLD        two of OUR distinct spellings both compile to retail's
                             single body at A.  Under /OPT:ICF's criterion (identical
                             including relocations) they are one address in the
                             shipped binary.  Action: alias -- IF it survives the
                             residency + retail-uniqueness gates below.
    T_F & !T_S   MAPNAME     A looks like our F and does NOT look like our own S.
                             The map put the wrong name on A.  Action: map-row repair,
                             which is a true identification, not a forgiveness.
    !T_F & T_S   DEFECT      the map's name at A is corroborated by our OWN body for
                             S, and our call site reaches F instead.  Retail really
                             calls S there.  Action: fix our source.  No alias may
                             ever forgive this -- it is the wrong-callee case an
                             alias is most dangerous against.
    !T_F & !T_S  UNDECIDABLE subdivided by the reason each test failed.

GATES ON THE FOLD CLASS (an unproven alias lifts name_check BY CONSTRUCTION, so a
false membership is a fabricated gain, not a miss):

  G-RESIDENCY   F must not itself be map-resident at a live address.  Where the map
                places F on its own address, the alias asserts retail has ONE body
                where the map says TWO, and the remedy is a map-row repair which the
                alias would foreclose (lane T1-AUDIT: 190 spellings removed).
  G-UNIQUENESS  retail must keep exactly ONE address for the body.  If N>1 addresses
                carry it, our call site's true target is one of several and the alias
                may forgive a genuinely wrong callee.  Lane INCOMPLETE-1 enforced this
                in batch and it rejected 80.5% of a set every other gate had passed.
                ⚠ alias_uniqueness_audit.py's refinement is carried: raw duplicate
                COUNT is misleading because dtk labels unreferenced spans as
                functions, so duplicates are reported alongside whether the survivor
                is the REFERENCE-DOMINANT address for the body.

PRICING -- deliberately stricter than the enumerating census
------------------------------------------------------------
matched_code is ALL-OR-NOTHING PER ROW.  A row pays only when EVERY charge on it is
closed.  incomplete_group_census.price() reports "bytes on sub-100 rows", which
counts a row in full even when it also carries charges from other classes (or
instruction-level mismatches an alias can never touch).  Here a row is CLOSABLE by a
class only when:

    fuzzy < 100          (it is not already paying)
    mpn  >= 100          (no instruction-level mismatch; all penalties are arg-only)
    every non-forgiven charged pair on the row is IN the class being priced

⚠ RESIDUAL IMPRECISION, stated because it bounds the headline in the optimistic
direction: mpn == 100 also tolerates REGISTER and BRANCH-DEST arg penalties, which a
name fix does not close either.  So even this figure is an upper bound; it is simply
a far tighter one.  (MPNGAP-1's CustomizePanel row is the canonical instance.)

Read-only.  Mutates no build input.
"""

import argparse
import collections
import glob
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

from icf_alias_build import collect, relocs_agree, placeholder, vacuous  # noqa: E402
from incomplete_group_census import charged_pairs, load_groups  # noqa: E402


def load_map():
    m = json.load(open(os.path.join(ROOT, "scripts/target_symbol_map.json")))
    name2addr = collections.defaultdict(set)
    for a, n in m.items():
        for x in (n if isinstance(n, list) else [n]):
            if x:
                name2addr[x].add(a)
    return m, name2addr


def body_sig(rec):
    """Relocation-normalized body signature: masked bytes + (offset,type,target-name) seq.

    Two target symbols sharing this signature hold the same code AND resolve their
    relocations to the same symbols -- /OPT:ICF's complete criterion (CD-7).
    """
    mb, rel, size = rec
    return (size, mb, tuple((o, t, n) for (o, n, t) in rel))


def identical(a, b, mapped):
    """Reloc-masked identity with TARGET NAMES compared (never a raw body compare)."""
    if a is None or b is None:
        return False
    if a[2] != b[2]:          # G0 SIZE: different-size COMDATs cannot fold
        return False
    if a[0] != b[0]:          # masked bytes
        return False
    return relocs_agree(a, b, mapped, strict=True)


def literal_relocs(a, b):
    """Every relocation slot's TARGET NAME agrees LITERALLY -- no placeholder tolerance.

    ⛔ WHY THIS IS SEPARATE FROM relocs_agree, MEASURED ON THIS VERY POPULATION.
    relocs_agree TOLERATES a retail-side fn_/lbl_ placeholder when our side is not
    map-resident, which is correct for its own question but far too permissive for
    asserting a NOVEL fold. Retail spells a string literal `lbl_82XXXXXX` (the address
    is absent from the map) while our side spells it `??_C@...`, so the slot is
    tolerated rather than compared -- and TWO Type() STATICS INTERNING DIFFERENT
    STRING LITERALS COMPARE EQUAL. `?Type@AddUserResultMsg` vs `?Type@SpeechEnableMsg`
    (88 B, nrel=17) passed relocs_agree here on exactly that hole.

    Measured contamination of the raw fold class: 17 of 23 pairs rested on a tolerated
    slot. When the relocation targets are unresolvable, the comparator CANNOT decide,
    so the honest verdict is UNDECIDABLE -- not PROVEN. Erring strict is the intended
    direction: an unproven alias lifts name_check BY CONSTRUCTION, so a false fold is
    a fabricated gain, not a miss.
    """
    if a is None or b is None or len(a[1]) != len(b[1]):
        return False
    return all(rn == on for (_o, rn, _t), (_o2, on, _t2) in zip(a[1], b[1]))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/home/free/tmp/nogroup_verdicts.json")
    ap.add_argument("--top", type=int, default=25)
    args = ap.parse_args()

    tgt = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/obj/**/*.obj"), recursive=True)), "t")
    ours = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/src/**/*.obj"), recursive=True)), "o")
    owner, groups = load_groups(os.path.join(ROOT, "scripts/symbol_aliases.json"))
    _m, name2addr = load_map()
    mapped = set(name2addr)

    sites, victims, aligned = charged_pairs(tgt, ours)

    # --- the two forgiveness rules name_check applies (skipping either overcounts ~8x)
    for p in [p for p in sites if placeholder(p[0]) or placeholder(p[1])]:
        del sites[p]
    for p in list(sites):
        gi, gj = owner.get(p[0]), owner.get(p[1])
        if gi is not None and gi == gj:
            del sites[p]

    # --- per-row charge inventory over the SURVIVING (non-forgiven) pairs.
    #     This is what makes all-or-nothing pricing possible: a row is closable by a
    #     class only if EVERY one of its surviving charges is in that class.
    row_charges = collections.defaultdict(set)
    for p in sites:
        for r in victims[p]:
            row_charges[r].add(p)

    fresh = [p for p in sites if owner.get(p[0]) is None and owner.get(p[1]) is None]

    rep = json.load(open(os.path.join(ROOT, "build/45410914/report.json")))
    mpn, fuzzy, size = {}, {}, {}
    for u in rep["units"]:
        for f in u.get("functions", []):
            mpn[f["name"]] = float(f["match_percent_normalized"])
            fuzzy[f["name"]] = float(f.get("fuzzy_match_percent", 0.0))
            size[f["name"]] = int(f["size"])

    # --- retail body-signature index, for G-UNIQUENESS.
    #     Keys are target-obj symbol names; each is a distinct retail address.
    sig_addrs = collections.defaultdict(list)
    for n, rec in tgt.items():
        sig_addrs[body_sig(rec)].append(n)

    verdicts = []
    for (rn, on) in sorted(fresh, key=lambda p: -sites[p]):
        rt, ob, os_ = tgt.get(rn), ours.get(on), ours.get(rn)
        rec = {
            "retail": rn, "ours": on, "sites": sites[(rn, on)],
            "rows": sorted(victims[(rn, on)]),
            "addr_retail": sorted(name2addr.get(rn, ())),
            "addr_ours": sorted(name2addr.get(on, ())),
            "have_retail_body": rt is not None,
            "have_our_body": ob is not None,
            "have_our_S_body": os_ is not None,
            "size_retail": rt[2] if rt else None,
            "size_ours": ob[2] if ob else None,
            "nrel": len(ob[1]) if ob else None,
        }

        if rt is None or ob is None:
            rec["verdict"] = "UNDECIDABLE_absent"
            rec["why"] = ("no %s body available: retail S is outside any pinned target obj"
                          % "retail" if rt is None else
                          "our spelling F is not compiled in any of our objs")
            verdicts.append(rec)
            continue

        t_f = identical(rt, ob, mapped)
        t_s = identical(rt, os_, mapped) if os_ is not None else False
        rec["T_F"], rec["T_S"] = t_f, t_s

        vac = vacuous(rt) or vacuous(ob)
        lit = literal_relocs(rt, ob)
        rec["vacuous"], rec["literal_relocs"] = vac, lit

        if t_f and t_s:
            # FOLD SHAPE -- now the gates that decide whether it is PROVEN and USABLE.
            sig = body_sig(rt)
            dupes = sig_addrs.get(sig, [])
            rec["retail_addrs_for_body"] = len(dupes)
            rec["dupe_sample"] = sorted(dupes)[:8]
            if vac:
                rec["verdict"] = "FOLD_unproven_vacuous"
                rec["why"] = ("body carries too little unmasked information to decide (<%d words, or "
                              ">50%% relocated): a 4-byte `blr` or a bare `b <target>` compares equal "
                              "to everything." % 4)
            elif not lit:
                rec["verdict"] = "FOLD_unproven_tolerance"
                rec["why"] = ("identity rests on relocation slots that relocs_agree TOLERATED rather "
                              "than compared (retail-side placeholder, e.g. a `lbl_` string literal). "
                              "When the targets are unresolvable the comparator cannot decide.")
            elif rec["addr_ours"]:
                rec["verdict"] = "FOLD_blocked_residency"
                rec["why"] = ("our spelling is itself map-resident at %s; an alias would assert one "
                              "retail body where the map asserts two. Remedy is a map-row "
                              "adjudication, not an alias (lane T1-AUDIT)." % ",".join(rec["addr_ours"]))
            elif len(dupes) != 1:
                rec["verdict"] = "FOLD_blocked_uniqueness"
                rec["why"] = ("retail holds this body at %d distinct addresses, so our call site's true "
                              "target is one of several and the alias may forgive a genuinely wrong "
                              "callee (the gate that rejected 80.5%% for INCOMPLETE-1)." % len(dupes))
            else:
                rec["verdict"] = "FOLD_PROVEN"
                rec["why"] = ("both our spellings compile to retail's single body at %s, identical "
                              "modulo relocated fields with all %d relocation target names agreeing "
                              "LITERALLY, the body is above the vacuity floor, our spelling is not "
                              "itself map-resident, and retail keeps exactly ONE address for the body."
                              % (",".join(rec["addr_retail"]) or "addr(S)", len(ob[1])))
        elif t_f and not t_s:
            # ⚠ DELIBERATELY NOT CALLED "map name wrong". Measured on this population:
            # 46 of 51 have our S at fuzzy < 100 in report.json, so !T_S is explained by
            # "our S is simply unfinished" and carries NO information about the map. Only
            # the S-at-100 residue is a genuine map contradiction worth adjudicating.
            corrob = fuzzy.get(rn)
            rec["fuzzy_S"] = corrob
            if corrob is not None and corrob >= 100.0:
                rec["verdict"] = "MAP_CONTRADICTION"
                rec["why"] = ("retail bytes at addr(S) match our body for F, yet our own S is graded "
                              "fuzzy==100 against target S -- two readings that cannot both hold. "
                              "A genuine map/pin inconsistency; adjudicate per row on retail bytes.")
            else:
                rec["verdict"] = "MAP_REPAIR_CANDIDATE"
                rec["why"] = ("retail bytes at addr(S) match our body for F, but our own S is %s so it "
                              "cannot corroborate or contradict the map. NOT a proven map error -- a "
                              "candidate needing per-row retail-byte adjudication."
                              % ("sub-100 (fuzzy %.1f)" % corrob if corrob is not None else "not graded"))
        elif t_s and not t_f:
            corrob = fuzzy.get(rn)
            rec["fuzzy_S"] = corrob
            strong = corrob is not None and corrob >= 100.0
            rec["verdict"] = "SOURCE_DEFECT" if strong else "SOURCE_DEFECT_weak"
            rec["why"] = ("the map's name at addr(S) is corroborated by OUR OWN body for S%s, and our "
                          "call site reaches F instead ⇒ retail really calls S here and our source "
                          "calls the wrong function. An alias here would forgive a real bug."
                          % (" (independently graded fuzzy==100 by objdiff)" if strong else ""))
        else:
            if rt[2] != ob[2]:
                rec["verdict"] = "UNDECIDABLE_size"
                rec["why"] = "different-size COMDATs cannot fold (%d vs %d B); neither test corroborates" % (rt[2], ob[2])
            elif rt[0] != ob[0]:
                rec["verdict"] = "UNDECIDABLE_bytes"
                rec["why"] = "masked bodies differ and our own S body does not corroborate the map either"
            else:
                rec["verdict"] = "UNDECIDABLE_relocs"
                rec["why"] = "bodies agree modulo relocation but relocation TARGET NAMES disagree"
        verdicts.append(rec)

    # ---------------- pricing ----------------
    def price(recs, label_pairs):
        """(rows_touched, closable_rows, closable_bytes, gross_bytes) under all-or-nothing."""
        inclass = set(label_pairs)
        rows = set()
        for r in recs:
            rows |= set(r["rows"])
        gross = sum(size.get(x, 0) for x in rows if fuzzy.get(x, 100.0) < 100.0)
        cl = [x for x in rows
              if fuzzy.get(x, 100.0) < 100.0
              and mpn.get(x, 0.0) >= 100.0
              and row_charges[x] <= inclass]
        return len(rows), len(cl), sum(size.get(x, 0) for x in cl), gross

    by = collections.defaultdict(list)
    for r in verdicts:
        by[r["verdict"]].append(r)

    print("=" * 96)
    print("NOGROUP CENSUS -- charged relocation-name pairs touching NO alias group")
    print("=" * 96)
    print("aligned function pairs : %d" % aligned)
    print("FRESH pairs / sites    : %d / %d" % (len(fresh), sum(sites[p] for p in fresh)))
    print()
    print("%-28s %6s %6s %7s %9s %11s %11s" %
          ("verdict", "pairs", "sites", "rows", "closable", "closable B", "gross B"))
    order = ["FOLD_PROVEN", "FOLD_unproven_vacuous", "FOLD_unproven_tolerance",
             "FOLD_blocked_residency", "FOLD_blocked_uniqueness",
             "MAP_CONTRADICTION", "MAP_REPAIR_CANDIDATE",
             "SOURCE_DEFECT", "SOURCE_DEFECT_weak",
             "UNDECIDABLE_size", "UNDECIDABLE_bytes", "UNDECIDABLE_relocs", "UNDECIDABLE_absent"]
    tot_cl = tot_b = tot_gross = 0
    for k in order:
        if k not in by:
            continue
        recs = by[k]
        pairs = [(r["retail"], r["ours"]) for r in recs]
        nr, ncl, b, gross = price(recs, pairs)
        tot_cl += ncl
        tot_b += b
        tot_gross += gross
        print("%-28s %6d %6d %7d %9d %11d %11d"
              % (k, len(recs), sum(r["sites"] for r in recs), nr, ncl, b, gross))

    allpairs = [(r["retail"], r["ours"]) for r in verdicts]
    nr, ncl, b, gross = price(verdicts, allpairs)
    print("-" * 96)
    print("%-28s %6d %6d %7d %9d %11d %11d"
          % ("ALL FRESH (jointly closed)", len(verdicts),
             sum(r["sites"] for r in verdicts), nr, ncl, b, gross))
    print()
    print("⚠ 'gross B' is incomplete_group_census.price()'s rule (bytes on sub-100 rows), which")
    print("  counts a row in full even when other charges on it are NOT in the class. 'closable B'")
    print("  additionally requires mpn>=100 and EVERY surviving charge on the row to be in-class.")

    json.dump(verdicts, open(args.out, "w"), indent=1)
    print("\nwrote %s (%d records)" % (args.out, len(verdicts)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
