#!/usr/bin/env python3
"""WRONGCALL-2: classify the charged relocation-name pairs by SCREEN, not by tier.

``tools/icf_relocname_census.py`` answers "can this pair be PROVEN a fold?".  That
is not the question the campaign now needs.  A charged pair ``(rn, on)`` -- retail's
slot names ``rn``, our slot names ``on`` -- has four possible causes, and the census
collapses three of them into REFUTED/UNDECIDABLE:

  FOLD        the retail address is an /OPT:ICF survivor carrying both spellings
  MAP_DEFECT  scripts/target_symbol_map.json put ``rn`` on the wrong address
  SOURCE      our source genuinely calls a different function than retail does
  UNDECIDABLE none of the screens can fire

Two screens separate them, and only ONE direction of each is sound:

  SCREEN A -- SAME-NAME BODY EQUALITY (the sound converse, WRONGCALL-1).
      Is the retail body at the address the map calls ``rn`` byte-equal (modulo
      relocated fields) to what OUR compiler emits for a symbol ALSO spelled
      ``rn``?  If yes, the map row is RIGHT: a name cannot be wrong when the body
      under it already matches ours.
      >> The opposite direction is VACUOUS and is NOT used (a745039e): bodies
      differing does not make the name wrong, because the map address need not be
      a function head, our spelling may be a different overload, or either side
      may simply be a near-miss we have not matched yet.

  SCREEN B -- CROSS BODY EQUALITY.
      Is the retail body under ``rn`` equal to OUR body under ``on``?  Identical
      code under two names is the ICF-fold shape.

The combination is what classifies:

  A=EQUAL & B=EQUAL   -> FOLD          (our rn and our on are themselves twins)
  A=EQUAL & B=DIFFERS -> SOURCE_CAND   (map name verified; our callee is a
                                        demonstrably different body)
  A=ABSENT/DIFFERS    -> fall through to B alone:
      B=EQUAL         -> FOLD_OR_MAP   (the two are structurally indistinguishable
                                        without a third channel; see the module
                                        docstring of icf_pair_adjudicate.py)
      B=DIFFERS       -> UNDECIDABLE
  either side vacuous -> UNDECIDABLE_vacuous

>> RUN --selfcheck FIRST.  Nine instruments in three days were caught unable to
   fail.  --selfcheck proves screen A can return EQUAL, can return DIFFERS, and
   reports its COVERAGE (how many pairs it is even able to look at).  A screen
   with coverage 0 reads as a clean "nothing found" and is worthless.
"""

import argparse
import collections
import glob
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from icf_alias_build import collect, relocs_agree, vacuous, placeholder  # noqa: E402


def load_sides():
    tgt = collect(sorted(glob.glob(str(ROOT / "build/45410914/obj/**/*.obj"), recursive=True)), "t")
    ours = collect(sorted(glob.glob(str(ROOT / "build/45410914/src/**/*.obj"), recursive=True)), "o")
    return tgt, ours


def charged_pairs(tgt, ours):
    """Reproduce the census enumerator exactly, but KEEP the victim symbols."""
    sites = collections.Counter()
    victims = collections.defaultdict(set)
    for name, (mb, rel, sz) in ours.items():
        rt = tgt.get(name)
        if not rt or len(rt[1]) != len(rel):
            continue
        for (ro, rn, rty), (oo, on, oty) in zip(rt[1], rel):
            if ro != oo or rty != oty or rn == on:
                continue
            sites[(rn, on)] += 1
            victims[(rn, on)].add(name)
    al = json.load(open(ROOT / "scripts/symbol_aliases.json"))
    eq = {}
    for g in al["groups"]:
        grp = set([g["survivor"]] + list(g["folded"]))
        for n in grp:
            eq.setdefault(n, set()).update(grp)
    for k in list(sites):
        if k[1] in eq.get(k[0], ()) or k[0] in eq.get(k[1], ()):
            del sites[k]
    return sites, victims


def screen_a(tgt, ours, rn):
    """SOUND direction only: EQUAL => the map row for rn is right."""
    rt, ob = tgt.get(rn), ours.get(rn)
    if rt is None:
        return "NO_TARGET"      # rn's address is not inside any pinned target obj
    if ob is None:
        return "NO_OURS"        # we do not compile a symbol spelled rn
    if vacuous(rt) or vacuous(ob):
        return "VACUOUS"
    return "EQUAL" if rt[0] == ob[0] else "DIFFERS"


def screen_b(tgt, ours, rn, on):
    rt, ob = tgt.get(rn), ours.get(on)
    if rt is None or ob is None:
        return "ABSENT"
    if vacuous(rt) or vacuous(ob):
        return "VACUOUS"
    return "EQUAL" if rt[0] == ob[0] else "DIFFERS"


def classify(a, b):
    if a == "EQUAL" and b == "EQUAL":
        return "FOLD"
    if a == "EQUAL" and b == "DIFFERS":
        return "SOURCE_CAND"
    if a == "EQUAL" and b in ("VACUOUS", "ABSENT"):
        return "NAME_RIGHT_only"
    if b == "EQUAL":
        return "FOLD_OR_MAP"
    if b == "VACUOUS" or a == "VACUOUS":
        return "UNDECIDABLE_vacuous"
    if b == "ABSENT":
        return "UNDECIDABLE_absent"
    return "UNDECIDABLE_bodies_differ"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--top", type=int, default=0, help="restrict to top-N pairs by site count")
    ap.add_argument("--out", default="/home/free/tmp/wc2_classified.json")
    args = ap.parse_args()

    tgt, ours = load_sides()
    sites, victims = charged_pairs(tgt, ours)
    real = [(p, c) for p, c in sites.items() if not placeholder(p[0]) and not placeholder(p[1])]
    real.sort(key=lambda x: (-x[1], x[0]))
    if args.top:
        real = real[: args.top]

    if args.selfcheck:
        # Screen A must be able to return BOTH verdicts, and its coverage must be > 0.
        cov = collections.Counter(screen_a(tgt, ours, rn) for (rn, _on), _c in real)
        print("SELFCHECK -- screen A verdict distribution over %d real-name pairs:" % len(real))
        for k, v in cov.most_common():
            print("   %-10s %5d" % (k, v))
        ok_fire = cov["EQUAL"] > 0
        ok_fail = cov["DIFFERS"] > 0
        print("   CAN FIRE (EQUAL>0):   %s" % ok_fire)
        print("   CAN FAIL (DIFFERS>0): %s" % ok_fail)
        covered = cov["EQUAL"] + cov["DIFFERS"]
        print("   COVERAGE: %d/%d = %.1f%% (rest: rn absent from one side or vacuous)"
              % (covered, len(real), 100.0 * covered / max(1, len(real))))
        if not (ok_fire and ok_fail and covered):
            print("   >> SCREEN IS VACUOUS -- do not believe any classification from it.")
            return 1
        return 0

    res = collections.Counter()
    rows = []
    for (rn, on), c in real:
        a = screen_a(tgt, ours, rn)
        b = screen_b(tgt, ours, rn, on)
        # a second, symmetric reading of screen A on OUR side's name
        a2 = screen_a(tgt, ours, on)
        cls = classify(a, b)
        res[cls] += 1
        rows.append({"cls": cls, "sites": c, "retail_name": rn, "our_name": on,
                     "screenA_rn": a, "screenA_on": a2, "screenB": b,
                     "victims": sorted(victims[(rn, on)])[:6],
                     "n_victims": len(victims[(rn, on)])})
    print("\nWC2 CLASSIFICATION of %d real-name charged pairs:" % len(real))
    tot_sites = 0
    for k, v in res.most_common():
        s = sum(r["sites"] for r in rows if r["cls"] == k)
        tot_sites += s
        print("   %-26s %5d pairs  %6d sites" % (k, v, s))
    print("   %-26s %5d pairs  %6d sites" % ("TOTAL", len(real), tot_sites))
    json.dump(rows, open(args.out, "w"), indent=1)
    print("wrote", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
