#!/usr/bin/env python3
"""SRCCAND-1 AXIS 4: TYPE CONSISTENCY -- which SIDE of a charged pair is wrong?

THE CONSTRAINT.  A template wrapper's callee is a function OF ITS OWN TYPE
ARGUMENT.  ``_Destroy<T>(T*)`` calls ``T::~T``; ``__ucopy_ptrs<T*,T*>`` copies
``T``; ``vector<T>::_M_erase`` destroys ``T``.  So for a charged slot inside a
victim ``V``, the callee's type tokens should INTERSECT the victim's.  That test
needs no map address and no body comparison -- it is pure type logic over the two
spellings -- which is why it can adjudicate rows where both screens A and B (both
of which read bodies at MAPPED addresses) are exhausted.

THE SCREEN IS SYMMETRIC, AND THAT SYMMETRY IS THE POINT.  For each charged slot
we ask the SAME question of both sides:

  OURS_CONSISTENT / RETAIL_INCONSISTENT  -> our callee belongs to the victim's
      type family and retail's does not.  The retail-side NAME is the suspect,
      i.e. a MAP defect at the callee's address (the ``vector<DistEntry>::~vector``
      sitting inside a CamShotCrowd helper cluster).  NOT a source defect.
  RETAIL_CONSISTENT / OURS_INCONSISTENT  -> retail's callee belongs and ours does
      not.  OUR SOURCE calls the wrong function.  This is the class the queue was
      opened to find.
  BOTH_CONSISTENT   both belong (e.g. overloads within one class) -- the screen
      cannot separate them; falls through to another channel.
  BOTH_INCONSISTENT neither belongs; typically a non-template victim where the
      constraint has no force.

A screen that could only ever indict the map would be worthless -- it would
confirm whatever it was pointed at.  ``--selfcheck`` therefore requires BOTH
one-sided verdicts to occur, and reports a NULL: the same screen run against
randomly re-paired callees, which must not produce the same one-sided rate.
"""

import argparse
import collections
import json
import re
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent.parent
sys.path.insert(0, str(ROOT / "tools" / "maprow_audit"))
from sc1_characterize import undname  # noqa: E402

# Tokens that carry no discriminating type information: STL machinery, the
# allocator that decorates every instantiation, and the primitive spellings.
_NOISE = {
    "class", "struct", "enum", "union", "const", "void", "bool", "char", "int",
    "unsigned", "short", "long", "float", "double", "public", "private",
    "protected", "virtual", "static", "__cdecl", "operator", "stlpmtx_std",
    "std", "StlNodeAlloc", "vector", "list", "pair", "less", "allocator",
    "_Nonconst_traits", "_Const_traits", "iterator", "_List_iterator",
    "__false_type", "__true_type", "size_t", "Hmx", "scalar", "deleting", "dtor",
    "ctor", "thunk", "vtordisp", "adjustor", "new", "delete", "__int64",
}

_TOK = re.compile(r"[A-Za-z_][A-Za-z_0-9]*")


def tokens(demangled):
    return {t for t in _TOK.findall(demangled) if t not in _NOISE and len(t) > 2}


def verdict(vt, rt, ot):
    """vt = victim tokens, rt = retail callee tokens, ot = our callee tokens."""
    if not vt:
        return "NO_VICTIM_TYPE"
    r_ok, o_ok = bool(vt & rt), bool(vt & ot)
    if o_ok and not r_ok:
        return "MAP_SUSPECT"        # ours belongs to the family, retail's does not
    if r_ok and not o_ok:
        return "SOURCE_SUSPECT"     # retail's belongs, ours does not
    if r_ok and o_ok:
        return "BOTH_CONSISTENT"
    return "BOTH_INCONSISTENT"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--axis3", default="/home/free/tmp/srccand1_axis3.json")
    ap.add_argument("--selfcheck", action="store_true")
    ap.add_argument("--out", default="/home/free/tmp/srccand1_typecons.json")
    args = ap.parse_args()

    rows = [r for r in json.load(open(args.axis3)) if r["body_equal"] and r["victims_eq"]]
    names = set()
    for r in rows:
        names.add(r["retail_name"])
        names.add(r["our_name"])
        names.update(r["victims_eq"])
    d = undname(sorted(names))

    out = []
    for r in rows:
        vt = set()
        for v in r["victims_eq"]:
            vt |= tokens(d[v])
        rt, ot = tokens(d[r["retail_name"]]), tokens(d[r["our_name"]])
        out.append(dict(r, verdict=verdict(vt, rt, ot),
                        victim_tokens=sorted(vt)[:8],
                        retail_tokens=sorted(rt)[:6], our_tokens=sorted(ot)[:6]))

    c = collections.Counter(r["verdict"] for r in out)

    if args.selfcheck:
        import random
        random.seed(11)
        shuf = [r["our_name"] for r in rows]
        random.shuffle(shuf)
        nullc = collections.Counter()
        for r, on in zip(rows, shuf):
            vt = set()
            for v in r["victims_eq"]:
                vt |= tokens(d[v])
            nullc[verdict(vt, tokens(d[r["retail_name"]]), tokens(d[on]))] += 1
        n = max(1, len(rows))
        print("SELFCHECK over %d body-equal rows" % len(rows))
        print("  real: %s" % dict(c))
        print("  null: %s" % dict(nullc))
        print("  MAP_SUSPECT    real %.1f%%  null %.1f%%"
              % (100.0 * c["MAP_SUSPECT"] / n, 100.0 * nullc["MAP_SUSPECT"] / n))
        print("  SOURCE_SUSPECT real %.1f%%  null %.1f%%"
              % (100.0 * c["SOURCE_SUSPECT"] / n, 100.0 * nullc["SOURCE_SUSPECT"] / n))
        ok = c["MAP_SUSPECT"] > 0 and c["SOURCE_SUSPECT"] > 0
        print("  BOTH one-sided verdicts occur (screen is not one-way): %s" % ok)
        return 0 if ok else 1

    print("\n=== AXIS 4: type consistency over %d body-equal charged rows ===" % len(out))
    for k, v in c.most_common():
        print("   %-18s %4d pairs  %4d sites" % (k, v, sum(r["sites"] for r in out if r["verdict"] == k)))

    print("\n=== shape x verdict ===")
    x = collections.Counter((r["shape"], r["verdict"]) for r in out)
    shp = sorted({r["shape"] for r in out})
    ver = [k for k, _ in c.most_common()]
    print("   %-20s %s" % ("shape", " ".join("%14s" % v[:14] for v in ver)))
    for s in shp:
        print("   %-20s %s" % (s, " ".join("%14d" % x[(s, v)] for v in ver)))

    print("\n=== SOURCE_SUSPECT rows (our source calls the wrong function) ===")
    for r in sorted([r for r in out if r["verdict"] == "SOURCE_SUSPECT"], key=lambda r: -r["sites"]):
        print("  [%d sites] V=%s" % (r["sites"], d[r["victims_eq"][0]][:100]))
        print("      R %s" % d[r["retail_name"]][:110])
        print("      O %s" % d[r["our_name"]][:110])

    json.dump(out, open(args.out, "w"), indent=1)
    print("\nwrote", args.out)
    return 0


if __name__ == "__main__":
    sys.exit(main())
