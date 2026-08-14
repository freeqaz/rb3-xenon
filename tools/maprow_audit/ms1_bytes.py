#!/usr/bin/env python3
"""MAPSUS-1 AXIS 2: can RETAIL BYTES adjudicate a proposed transposition?

WHY THIS FILE EXISTS.  ``ms1_cycles.py`` says the MAP_SUSPECT population contains
7 closed 2-cycles.  Closure proves the proposal is a PERMUTATION -- injective by
construction -- it does NOT prove the permutation is the right one.  Worse, every
row in this population carries ``screenA_on == EQUAL``, which reads as the map
CORROBORATING itself: "retail's body at addr(our_name) already equals our
compilation of our_name, including relocation target names".  If that agreement
is real, the swap is WRONG and applying it would damage correct code -- the
1,248 B mistake the brief warns about.

SO THE FIRST THING THIS FILE DOES IS ATTACK ITS OWN PROPOSAL.  For each address
it reports whether screenA's agreement is INFORMATIVE or VACUOUS:

  * ``bodies_identical``  -- are the two retail bodies (at A and at B) equal word
    for word with NOTHING masked?  If yes the pair is a true masked class: no
    byte at either address can distinguish the two names, screenA cannot fail on
    either assignment, and its EQUAL verdict carries zero information about the
    assignment.  (This is the structural reason the rotation survived every
    previous audit.)
  * ``discriminating_relocs`` -- how many relocation slots have target names that
    DIFFER between A and B *and* are not placeholders?  These are the only slots
    where the assignment is testable at all.  Zero discriminating slots => the
    map cannot be checked from bodies, full stop.
  * ``sizes`` -- retail extents.  Sizes come from the split, not from the name,
    so a size difference is a genuine map-independent discriminator; equal sizes
    are another way for the class to be masked.

⚠ A ZERO HERE IS NOT GOOD NEWS.  ``discriminating_relocs == 0`` means this file
found nothing, which is exactly what a broken reader also returns.  --selfcheck
therefore requires the reader to (a) find every requested symbol, and (b) report
a NON-zero discriminating count on a control pair that is known NOT to be a
masked class, so a silent read failure cannot masquerade as "masked class".
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

TARGET_GLOB = str(ROOT / "build" / "45410914" / "obj" / "**" / "*.obj")
OURS_GLOB = str(ROOT / "build" / "45410914" / "src" / "**" / "*.obj")


def load(label, pattern):
    paths = glob.glob(pattern, recursive=True)
    idx = collect(paths, label)
    print("  %-8s %6d objs -> %6d symbols" % (label, len(paths), len(idx)))
    return idx


def reloc_names(rec):
    return {o: n for (o, n, _t) in rec[1]}


def compare(rec_a, rec_b):
    """Return (bodies_identical, n_disc, disc) for two retail records."""
    if rec_a is None or rec_b is None:
        return None, None, []
    ba, bb = rec_a[0], rec_b[0]
    ra, rb = reloc_names(rec_a), reloc_names(rec_b)
    identical = (ba == bb) and (rec_a[2] == rec_b[2])
    disc = []
    for o in sorted(set(ra) | set(rb)):
        na, nb = ra.get(o, ""), rb.get(o, "")
        if na != nb and not placeholder(na) and not placeholder(nb):
            disc.append((o, na, nb))
    return identical, len(disc), disc


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--cycles", default="/home/free/tmp/mapsus1_cycles.json")
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--out", default="/home/free/tmp/mapsus1_bytes.json")
    args = ap.parse_args()

    print("loading object indexes ...")
    tgt = load("target", TARGET_GLOB)
    ours = load("ours", OURS_GLOB)

    cyc = json.load(open(args.cycles))["cycles"]
    pairs = []
    for c in cyc:
        if len(c) != 2:
            continue
        pairs.append((c[0]["addr"], c[0]["cur"], c[1]["addr"], c[1]["cur"]))

    if args.selfcheck:
        # (a) every requested symbol must be present on the retail side
        missing = [n for _, n, _, m in pairs for n in (n, m) if n not in tgt]
        print("\nSELFCHECK")
        print("  requested retail symbols: %d   MISSING: %d %s"
              % (2 * len(pairs), len(missing), missing[:3]))
        # (b) a control pair that is NOT a masked class must show discrimination.
        # ⚠ FIRST ATTEMPT AT THIS CONTROL WAS VACUOUS AND IS KEPT AS A LESSON: it
        # picked the two LARGEST retail symbols, which are 119 KB / 85 KB
        # unbounded blobs whose relocations are all placeholders, so it returned
        # discriminating_relocs == 0 -- indistinguishable from "masked class" and
        # for an entirely unrelated reason.  Size is the wrong selector; what the
        # control needs is symbols that CARRY NAMED RELOCATIONS at all.
        ctl = None
        named = sorted(
            (k for k in tgt
             if sum(1 for (_o, n, _t) in tgt[k][1] if not placeholder(n)) >= 4),
            key=lambda k: -sum(1 for (_o, n, _t) in tgt[k][1] if not placeholder(n)))
        if len(named) >= 2:
            ctl = compare(tgt[named[0]], tgt[named[1]])
            print("  control (two unrelated retail fns with the most NAMED relocs,"
                  " %d B / %d B):" % (tgt[named[0]][2], tgt[named[1]][2]))
            print("      bodies_identical=%s  discriminating_relocs=%d"
                  % (ctl[0], ctl[1]))
        ok = (not missing) and ctl is not None and ctl[1] > 0 and ctl[0] is False
        print("  reader can find symbols AND can report discrimination: %s" % ok)
        return 0 if ok else 1

    out = []
    print("\n=== the 7 proposed transpositions, adjudicated on retail bytes ===")
    for (a, na, b, nb) in pairs:
        ra, rb = tgt.get(na), tgt.get(nb)
        ident, ndisc, disc = compare(ra, rb)
        oa, ob = ours.get(na), ours.get(nb)
        print("\n  %s  %.66s" % (a, na))
        print("  %s  %.66s" % (b, nb))
        if ra is None or rb is None:
            print("      RETAIL SIDE ABSENT (a=%s b=%s) -- cannot adjudicate"
                  % (ra is not None, rb is not None))
            out.append(dict(a=a, na=na, b=b, nb=nb, verdict="ABSENT"))
            continue
        print("      retail sizes: %d / %d      bodies_identical(unmasked): %s"
              % (ra[2], rb[2], ident))
        print("      discriminating reloc slots between the two retail bodies: %d" % ndisc)
        for (o, x, y) in disc[:6]:
            print("        +0x%02x  A:'%.42s'  B:'%.42s'" % (o, x, y))
        # our own compilation of the two names -- is OUR pair a masked class too?
        oident, ondisc, odisc = compare(oa, ob) if (oa and ob) else (None, None, [])
        print("      OUR sizes: %s / %s   our bodies_identical: %s   our disc slots: %s"
              % (oa[2] if oa else "-", ob[2] if ob else "-", oident, ondisc))
        for (o, x, y) in (odisc or [])[:6]:
            print("        +0x%02x  ours(A-name):'%.38s'  ours(B-name):'%.38s'" % (o, x, y))
        verdict = ("MASKED_CLASS_bytes_cannot_decide" if ident and ndisc == 0
                   else "BYTES_DISCRIMINATE")
        print("      => %s" % verdict)
        out.append(dict(a=a, na=na, b=b, nb=nb, verdict=verdict,
                        retail_sizes=[ra[2], rb[2]], bodies_identical=ident,
                        n_disc=ndisc, disc=[[o, x, y] for o, x, y in disc],
                        our_sizes=[oa[2] if oa else None, ob[2] if ob else None],
                        our_identical=oident, our_disc=ondisc))

    print("\n=== summary ===")
    for k, v in collections.Counter(r["verdict"] for r in out).most_common():
        print("   %-36s %d" % (k, v))
    json.dump(out, open(args.out, "w"), indent=1)
    print("wrote", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
