#!/usr/bin/env python3
"""SRCCAND-1: victim-level view of the charged relocation-name queue.

WHY THIS EXISTS, AND WHAT IT FIXES IN MY OWN EARLIER INSTRUMENT.
``sc1_characterize.py``'s permutation axis grouped charges by victim using ONLY
the both-verified SOURCE_CAND rows.  A victim's other charges (FOLD, UNDECIDABLE,
NAME_RIGHT_only) were invisible, so its multiset was under-populated, compared
unequal, and read SUBSTITUTION -- 180 of 188.  That is the work-manufacturing
direction: it says "we call something retail never calls" when the truth may be
"we call the right things in the wrong order and you only showed me half of them".

So recompute from the objs: for EVERY victim function, enumerate EVERY charged
slot in offset order, with no class restriction.  Then a victim is

  PERMUTATION   the multiset of retail-side callee names at its charged slots
                EQUALS the multiset of our-side names.  Our source calls exactly
                the right set of functions in the wrong ORDER.  Read the order
                off the victim's own retail body -- no map, no third channel.
  SUBSTITUTION  disjoint: we call things retail does not.
  PARTIAL       overlapping but unequal multisets.

>> --selfcheck must show the PERMUTATION verdict CAN fire.  A permutation
   detector that never fires is indistinguishable from "there are no orderings
   to fix", which is exactly the decisive-negative shape that closes veins.
"""

import argparse
import collections
import glob
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from icf_alias_build import collect, placeholder  # noqa: E402


def load_sides():
    tgt = collect(sorted(glob.glob(str(ROOT / "build/45410914/obj/**/*.obj"), recursive=True)), "t")
    ours = collect(sorted(glob.glob(str(ROOT / "build/45410914/src/**/*.obj"), recursive=True)), "o")
    return tgt, ours


def victim_charges(tgt, ours, aliases):
    """{victim: [(slot_offset, retail_name, our_name)]} over ALL charged slots."""
    out = collections.defaultdict(list)
    for name, (mb, rel, sz) in ours.items():
        rt = tgt.get(name)
        if not rt or len(rt[1]) != len(rel):
            continue
        for (ro, rn, rty), (oo, on, oty) in zip(rt[1], rel):
            if ro != oo or rty != oty or rn == on:
                continue
            if on in aliases.get(rn, ()) or rn in aliases.get(on, ()):
                continue
            out[name].append((ro, rn, on))
    return out


def load_aliases():
    al = json.load(open(ROOT / "scripts/symbol_aliases.json"))
    eq = {}
    for g in al["groups"]:
        grp = set([g["survivor"]] + list(g["folded"]))
        for n in grp:
            eq.setdefault(n, set()).update(grp)
    return eq


def verdict(charges, realonly=True):
    ch = [c for c in charges
          if not realonly or (not placeholder(c[1]) and not placeholder(c[2]))]
    if not ch:
        return None, 0
    r = collections.Counter(c[1] for c in ch)
    o = collections.Counter(c[2] for c in ch)
    if r == o:
        return "PERMUTATION", len(ch)
    if r & o:
        return "PARTIAL", len(ch)
    return "SUBSTITUTION", len(ch)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--classified", default="/home/free/tmp/srccand1_classified.json")
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--out", default="/home/free/tmp/srccand1_victims.json")
    args = ap.parse_args()

    tgt, ours = load_sides()
    vc = victim_charges(tgt, ours, load_aliases())

    rows = json.load(open(args.classified))
    bv = {(r["retail_name"], r["our_name"]) for r in rows
          if r["cls"] == "SOURCE_CAND" and r["screenA_on"] == "EQUAL"}

    # victims carrying >=1 both-verified SOURCE_CAND charge
    tgt_victims = {v: ch for v, ch in vc.items()
                   if any((c[1], c[2]) in bv for c in ch)}

    vd = {v: verdict(ch) for v, ch in tgt_victims.items()}
    allvd = {v: verdict(ch) for v, ch in vc.items()}

    if args.selfcheck:
        c = collections.Counter(x[0] for x in allvd.values() if x[0])
        ct = collections.Counter(x[0] for x in vd.values() if x[0])
        print("SELFCHECK -- verdicts over ALL %d charged victims: %s"
              % (len(allvd), dict(c)))
        print("            verdicts over the %d TARGET victims:  %s"
              % (len(vd), dict(ct)))
        print("  PERMUTATION CAN FIRE (whole population): %s" % (c["PERMUTATION"] > 0))
        print("  all three verdicts observed:             %s" % (len(c) == 3))
        return 0 if c["PERMUTATION"] > 0 else 1

    print("\n=== victims carrying a both-verified SOURCE_CAND charge: %d ===" % len(vd))
    c = collections.Counter(x[0] for x in vd.values() if x[0])
    for k, v in c.most_common():
        print("   %-14s %4d victims" % (k, v))
    print("\n=== control: ALL %d charged victims ===" % len(vc))
    ca = collections.Counter(x[0] for x in allvd.values() if x[0])
    for k, v in ca.most_common():
        print("   %-14s %4d victims" % (k, v))

    perms = sorted([v for v, x in vd.items() if x[0] == "PERMUTATION"])
    print("\n=== PERMUTATION victims (order defect, readable off retail) ===")
    for v in perms:
        print("  %s" % v[:120])
        for off, rn, on in sorted(tgt_victims[v]):
            print("     +0x%04x  R %s\n              O %s" % (off, rn[:100], on[:100]))

    json.dump({"target_victims": {v: [list(c) for c in ch] for v, ch in tgt_victims.items()},
               "verdicts": {v: list(x) for v, x in vd.items()},
               "all_verdicts": {v: list(x) for v, x in allvd.items()}},
              open(args.out, "w"), indent=1)
    print("\nwrote", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
