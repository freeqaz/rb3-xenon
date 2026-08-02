#!/usr/bin/env python3
"""census.py -- WHOLE-BINARY comparison of virtual/multiple-base SUBOBJECT
OFFSETS, retail band.exe RTTI vs our compiled .objs.

This is the untreated-population control for lane CO-1.  The brief named four
classes; this asks the same question of EVERY class that has RTTI on both
sides, so we can tell:

  * how many classes AGREE  (the untreated population -- if most agree, the
    layout MODEL is fine and the disagreements are per-class member defects)
  * how many disagree, and whether the disagreement is a UNIFORM SHIFT of all
    base subobjects (=> a surplus/missing member in the derived class's own
    prefix) or a STRUCTURAL difference (different count of subobjects, or
    non-uniform deltas => a genuine ordering/model defect)

Our side: `??_R4<...>` COL symbols in build/45410914/src/**/*.obj.
Retail side: vbase_rtti.py's parser over orig/45410914/band.exe.

Prints denominators next to every zero, per THE STANDARD.
"""
from __future__ import annotations
import argparse
import struct
import sys
from collections import defaultdict
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from vbase_rtti import Image, cols_for_td  # noqa: E402
from obj_col import read_obj  # noqa: E402


def demangle_col_class(sym):
    """`??_R4LocalBandUser@@6BUser@@@` -> 'LocalBandUser'.
    `??_R4Object@Hmx@@6B@`            -> 'Object@Hmx'
    Returns None if it doesn't parse."""
    if not sym.startswith("??_R4"):
        return None
    rest = sym[5:]
    i = rest.find("@@")
    if i < 0:
        return None
    return rest[:i]


def our_cols(objdir):
    """class -> set of (offset, cdOffset) from OUR objs."""
    out = defaultdict(set)
    nobj = 0
    for p in sorted(Path(objdir).rglob("*.obj")):
        try:
            b, secs, syms = read_obj(p)
        except Exception:
            continue
        nobj += 1
        for name, value, secnum in syms:
            if not name.startswith("??_R4"):
                continue
            if secnum <= 0 or secnum > len(secs):
                continue
            cls = demangle_col_class(name)
            if cls is None:
                continue
            _sname, rawptr, _rawsize = secs[secnum - 1]
            base = rawptr + value
            if base + 12 > len(b):
                continue
            sig, off, cd = struct.unpack_from(">III", b, base)
            if sig != 0:
                continue
            out[cls].add((off, cd))
    return out, nobj


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--objdir",
                    default="build/45410914/src")
    ap.add_argument("--image",
                    default="/home/free/code/milohax/rb3-xenon/orig/45410914/band.exe")
    ap.add_argument("--gate-effect", action="store_true",
                    help="measure how many COLs the signature gate REJECTS "
                         "(proves the gate is load-bearing, i.e. can fail)")
    a = ap.parse_args()

    img = Image(Path(a.image))
    tds = img.find_tds()
    # retail: name -> sorted list of (offset, cd)
    retail = {}
    retail_relaxed = {}
    for va, name in tds.items():
        cs = cols_for_td(img, va, strict=True)
        if cs:
            retail.setdefault(name, set()).update(
                (c["offset"], c["cdOffset"]) for c in cs)
        if a.gate_effect:
            cr = cols_for_td(img, va, strict=False)
            if cr:
                retail_relaxed.setdefault(name, set()).update(
                    (c["offset"], c["cdOffset"]) for c in cr)

    ours, nobj = our_cols(a.objdir)
    print(f"# our objs scanned: {nobj}")
    print(f"# classes with RTTI on our side:    {len(ours)}")
    print(f"# classes with RTTI on retail side: {len(retail)}  "
          f"(of {len(tds)} TypeDescriptors)")

    if a.gate_effect:
        strict_n = sum(len(v) for v in retail.values())
        relax_n = sum(len(v) for v in retail_relaxed.values())
        print(f"# GATE EFFECT: COLs accepted strict={strict_n} "
              f"relaxed={relax_n}  REJECTED BY GATE={relax_n - strict_n}")

    common = sorted(set(ours) & set(retail))
    print(f"# classes present on BOTH sides:    {len(common)}   "
          f"<-- THE DENOMINATOR")

    agree, uniform, structural = [], [], []
    for cls in common:
        o = sorted(ours[cls])
        r = sorted(retail[cls])
        if o == r:
            agree.append(cls)
            continue
        oo = [x[0] for x in o]
        rr = [x[0] for x in r]
        if len(oo) == len(rr):
            deltas = {b - a_ for a_, b in zip(rr, oo)}
            # a uniform shift of every NON-zero subobject, with the primary
            # vftable still at 0, is the "surplus member in own prefix" shape
            nz = {b - a_ for a_, b in zip(rr, oo) if not (a_ == 0 and b == 0)}
            if len(nz) == 1:
                uniform.append((cls, nz.pop(), oo, rr))
                continue
            structural.append((cls, "nonuniform", oo, rr, sorted(deltas)))
        else:
            structural.append((cls, "count %d vs %d" % (len(oo), len(rr)),
                               oo, rr, []))

    print(f"\n## AGREE (untreated-population control): {len(agree)} / {len(common)}")
    print(f"## UNIFORM SHIFT (own-prefix size defect): {len(uniform)} / {len(common)}")
    print(f"## STRUCTURAL (real model/order defect):   {len(structural)} / {len(common)}")

    # Only classes with >1 subobject can express a vbase-displacement defect at
    # all -- report that honest sub-denominator too.
    askable = [c for c in common if len(retail[c]) > 1 or len(ours[c]) > 1]
    ask_agree = [c for c in askable if sorted(ours[c]) == sorted(retail[c])]
    print(f"\n## classes where the question is even ASKABLE (>1 subobject): "
          f"{len(askable)}")
    print(f"##   of those, AGREE EXACTLY: {len(ask_agree)}")

    print("\n=== UNIFORM SHIFT (delta, ours, retail) ===")
    for cls, d, oo, rr in sorted(uniform, key=lambda x: -abs(x[1])):
        print(f"  {d:+#7x}  {cls:<46} ours={[hex(x) for x in oo]} "
              f"retail={[hex(x) for x in rr]}")

    print("\n=== STRUCTURAL ===")
    for cls, why, oo, rr, d in structural:
        print(f"  {cls:<46} {why:<16} ours={[hex(x) for x in oo]} "
              f"retail={[hex(x) for x in rr]}")


if __name__ == "__main__":
    main()
