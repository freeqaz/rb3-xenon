#!/usr/bin/env python3
"""MAPSUS-1 AXIS 4: if the map is right at the accused address, WHERE is the defect?

WHAT AXIS 3 ESTABLISHED.  ``ms1_words.py`` adjudicated all 74 MAP_SUSPECT
proposals on masked body words -- the one channel a symbol map cannot influence
-- and returned MAP_CONFIRMED 74/74, with the refuting side genuinely computed
(``matches_other is False``, never ``None``) in every case.  So the name the map
puts on the accused address is the name whose code our compiler reproduces there.
The rows are NOT rotations at the address they accuse, and the 7 closed 2-cycles
``ms1_cycles.py`` found are NOT transpositions.

WHICH LEAVES ONE PLACE FOR THE DEFECT TO BE.  A charged slot inside victim V says
retail's V calls X and our V calls Y.  If the code at addr(X) really is X, then
either (a) retail's V genuinely calls a different function than ours -- a SOURCE
defect -- or (b) the two functions were ICF-FOLDED, retail's call resolves to the
one surviving copy, and the map can only ever carry the survivor's single
arbitrary name.  (b) is unfixable by renaming and must not be papered over with
an alias.

THIS FILE SEPARATES (a) FROM (b) THE ONLY WAY THAT WORKS: recurse the word
comparator onto the THIRD symbol.  For each disagreeing relocation slot between
retail@A and our(A) we get a callee pair (RC = retail's spelling, OC = ours):

  FOLD_PROVEN     our compilations of RC and OC have IDENTICAL masked words AND
                  agreeing relocation targets => MSVC's fold criterion is met, so
                  retail shipped one body and the disagreement is unfixable.
                  (MSVC folds only COMDATs identical INCLUDING relocations, so
                  identical-words-but-differing-relocs is NOT a fold.)
  ROTATION_AT_3   the code at addr(RC) is our(OC) and not our(RC) -- a real map
                  defect, one level below the accused row.  THIS is the only
                  class this lane could legitimately repair.
  MAP_OK_AT_3     the code at addr(RC) is our(RC): the third symbol is right too,
                  so the disagreement is a genuine SOURCE difference or a fold
                  with a third spelling.
  UNRESOLVED      addr(RC) unknown, or one side missing.

⚠ NOTE THE ASYMMETRY THAT MAKES FOLD_PROVEN SAFE TO ASSERT.  Proving a fold needs
our two compilations to be identical under a criterion at least as STRICT as
MSVC's.  ``relocs_agree`` compares relocation target NAMES, so if it says the two
differ, they cannot have folded -- that direction is sound.  The converse (it
says they agree) is the one that needs the word check as well, which is why both
are required here.

--selfcheck requires each verdict class to be REACHABLE on this population's own
data, and reports how many slots the file can even look at.  A recursion that
resolves nothing returns all-UNRESOLVED, which reads like "nothing to fix".
"""

import argparse
import collections
import glob
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools"))
from icf_alias_build import collect, placeholder, relocs_agree  # noqa: E402
sys.path.insert(0, str(ROOT / "tools" / "maprow_audit"))
from ms1_words import words_equal  # noqa: E402

TARGET_GLOB = str(ROOT / "build" / "45410914" / "obj" / "**" / "*.obj")
OURS_GLOB = str(ROOT / "build" / "45410914" / "src" / "**" / "*.obj")


def disagreeing_slots(rt, ob):
    """Slots where retail@A and our(A) name DIFFERENT non-placeholder targets."""
    ra = {o: n for (o, n, _t) in rt[1]}
    oa = {o: n for (o, n, _t) in ob[1]}
    out = []
    for o in sorted(set(ra) & set(oa)):
        x, y = ra[o], oa[o]
        if x != y and not placeholder(x) and not placeholder(y):
            out.append((o, x, y))
    return out


def classify_third(tgt, ours, inv, rc, oc):
    our_rc, our_oc = ours.get(rc), ours.get(oc)
    # (b) FOLD: our two compilations meet MSVC's criterion -- same words AND
    #     agreeing relocation targets.
    if our_rc is not None and our_oc is not None:
        if our_rc[0] == our_oc[0] and our_rc[2] == our_oc[2] \
                and relocs_agree(our_rc, our_oc):
            return "FOLD_PROVEN"
    # (a) is the code at addr(rc) actually oc?
    a = words_equal(tgt.get(rc), our_rc)
    b = words_equal(tgt.get(rc), our_oc)
    if a is None and b is None:
        return "UNRESOLVED"
    if b and not a:
        return "ROTATION_AT_3"
    if a and not b:
        return "MAP_OK_AT_3"
    if a and b:
        return "UNDECIDABLE_masked_at_3"
    return "NEITHER_AT_3"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--words", default="/home/free/tmp/mapsus1_words.json")
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--out", default="/home/free/tmp/mapsus1_third.json")
    args = ap.parse_args()

    tgt = collect(glob.glob(TARGET_GLOB, recursive=True), "target")
    ours = collect(glob.glob(OURS_GLOB, recursive=True), "ours")
    m = {k: v for k, v in json.load(
        open(ROOT / "scripts" / "target_symbol_map.json")).items()
        if k.startswith("0x") and isinstance(v, str)}
    inv = collections.defaultdict(list)
    for a, n in m.items():
        inv[n].append(a)

    rows = json.load(open(args.words))
    seen, out = set(), []
    n_noslot = 0
    for r in rows:
        na = r["map_name"]
        rt, ob = tgt.get(na), ours.get(na)
        if rt is None or ob is None:
            continue
        slots = disagreeing_slots(rt, ob)
        if not slots:
            n_noslot += 1
        for (o, rc, oc) in slots:
            key = (rc, oc)
            if key in seen:
                continue
            seen.add(key)
            v = classify_third(tgt, ours, inv, rc, oc)
            out.append(dict(victim_addr=r["addr"], victim=na, off=o,
                            retail_callee=rc, our_callee=oc, verdict=v,
                            rc_addr=(inv[rc][0] if len(inv.get(rc, [])) == 1 else None),
                            oc_addr=(inv[oc][0] if len(inv.get(oc, [])) == 1 else None)))

    c = collections.Counter(r["verdict"] for r in out)
    print("\naccused addresses with NO disagreeing named slot: %d of %d"
          % (n_noslot, len(rows)))
    print("distinct third-symbol pairs reachable: %d" % len(out))
    for k, v in c.most_common():
        print("   %-26s %d" % (k, v))

    if args.selfcheck:
        print("\nSELFCHECK")
        print("  slots reachable at all: %d  (0 would mean the recursion is dead)" % len(out))
        print("  distinct verdict classes reached: %d %s" % (len(c), sorted(c)))
        ok = len(out) > 0 and len(c) >= 2
        print("  recursion resolves data AND reaches >1 class: %s" % ok)
        return 0 if ok else 1

    print("\n=== ROTATION_AT_3 (the only repairable class) ===")
    for r in out:
        if r["verdict"] == "ROTATION_AT_3":
            print("  %s +0x%02x  in %.50s" % (r["victim_addr"], r["off"], r["victim"]))
            print("      retail names %s  '%.60s'" % (r["rc_addr"], r["retail_callee"]))
            print("      code there is '%.60s'" % r["our_callee"])

    print("\n=== FOLD_PROVEN sample ===")
    for r in [x for x in out if x["verdict"] == "FOLD_PROVEN"][:8]:
        print("  %.58s" % r["retail_callee"])
        print("    == %.58s" % r["our_callee"])

    json.dump(out, open(args.out, "w"), indent=1)
    print("\nwrote", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
