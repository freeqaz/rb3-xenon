#!/usr/bin/env python3
"""Lane CZ-3: how much of SRC_imperfect_T is actually a MAP MISPAIR?

I classified 97.2% of SRC_imperfect_T as "CONSISTENT body defect" on the grounds
that objdiff independently scores T below 100. That inference is WRONG as stated:
mpn < 100 is equally consistent with a MAP MISPAIR (we are diffing our body for
class X against retail's body for class Y). Two rows caught it --
??_GFlowQueueable's target calls ??1RndEnvAnim, ??_GHamListRibbon's target calls
an unnamed fn with a different vbase displacement.

THE INVARIANT (a language rule, not a similarity score)
-------------------------------------------------------
MSVC's scalar deleting destructor ??_G<C> is generated as
    { this->~C(); if (flags & 1) operator delete(this); return this; }
so its body ALWAYS calls the destructor OF ITS OWN CLASS -- ??1<C> directly, or
??_D<C> (the vbase-dtor helper) which in turn calls it. A target body named
??_G<C> whose only dtor callee is ??1<D> for some D != C is therefore NOT
??_G<C>. This is ownership, not resemblance.

⚠ SCOPE LIMIT, stated up front: the invariant can only speak where dtk NAMED the
callee. An unnamed fn_8XXXXXXX callee is UNDECIDABLE and gets its own bucket -- it
is never counted as agreeing and never counted as violating (gate 3: no unknown
bucket may default into the charged class).

CONDITIONED NULL
----------------
Population: ??_G rows AT mpn 100.0 -- same generator, same objs, same reader; the
only difference is whether the row is in the charged residue. If the violation
rate is the same in both, the invariant does not discriminate and I must say so.
A failure looks like: treated ~= null, or treated == 0.

Read-only.
"""
import collections
import json
import os
import re
import sys
from pathlib import Path

ROOT = os.environ.get("CZ3_ROOT", ".")
sys.path.insert(0, os.path.join(ROOT, "tools"))
from coff_bodies_ext import function_bodies_ext                    # noqa: E402
from icf_site_census import load_unit_objs                         # noqa: E402

CLS = re.compile(r'^\?\?_[GE](.+?)@@')
DTOR = re.compile(r'^\?\?(1|_D)(.+?)@@')


def cls_of(n):
    m = CLS.match(n)
    return m.group(1) if m else None


def main():
    root = Path(ROOT)
    rep = json.load(open(os.path.join(ROOT, "build/45410914/report.json")))
    pct = {}
    for u in rep["units"]:
        for f in (u.get("functions") or []):
            pct[(u["name"], f["name"])] = f["match_percent_normalized"]

    objp = load_unit_objs(root)
    # index every TARGET obj once: (unit, fn) -> called names
    calls = {}
    for unit, (tp, _bp) in objp.items():
        try:
            bl = list(function_bodies_ext(Path(tp)))
        except Exception:
            continue
        for nm, _raw, relocs, _e in bl:
            if not nm.startswith("??_G") and not nm.startswith("??_E"):
                continue
            calls.setdefault((unit, nm), [n for _o, n, t in relocs if t == 0x06])

    def verdict(unit, nm):
        c = cls_of(nm)
        if c is None:
            return "unparsed"
        cs = calls.get((unit, nm))
        if cs is None:
            return "no_target_body"
        named_dtors = []
        anon = 0
        for n in cs:
            m = DTOR.match(n)
            if m:
                named_dtors.append(m.group(2))
            elif re.match(r'^_?fn_[0-9a-fA-F]+$', n):
                anon += 1
        if not named_dtors:
            return "UNDECIDABLE (no NAMED dtor callee)" if anon or not cs \
                   else "UNDECIDABLE (no dtor callee)"
        return "AGREES" if c in named_dtors else "VIOLATES (map mispair)"

    groups = collections.defaultdict(collections.Counter)
    examples = collections.defaultdict(list)
    for (unit, nm), p in pct.items():
        if not (nm.startswith("??_G") or nm.startswith("??_E")):
            continue
        if (unit, nm) not in calls:
            continue
        g = "TREATED (mpn < 100)" if p < 100.0 else "NULL (mpn == 100)"
        v = verdict(unit, nm)
        groups[g][v] += 1
        if v.startswith("VIOLATES"):
            c = cls_of(nm)
            cs = calls[(unit, nm)]
            examples[g].append((unit, nm, c, [x for x in cs if DTOR.match(x)]))

    print("=== ??_G / ??_E dtor-class invariant ===")
    for g in ("TREATED (mpn < 100)", "NULL (mpn == 100)"):
        cnt = groups[g]
        tot = sum(cnt.values())
        dec = cnt["AGREES"] + cnt["VIOLATES (map mispair)"]
        print("\n%s   n=%d" % (g, tot))
        for k, n in cnt.most_common():
            print("    %-38s %5d  (%5.1f%% of all)" % (k, n, 100.0 * n / max(1, tot)))
        if dec:
            print("    -> violation rate among DECIDABLE: %d/%d = %.2f%%"
                  % (cnt["VIOLATES (map mispair)"], dec,
                     100.0 * cnt["VIOLATES (map mispair)"] / dec))

    t, n = groups["TREATED (mpn < 100)"], groups["NULL (mpn == 100)"]
    td = t["AGREES"] + t["VIOLATES (map mispair)"]
    nd = n["AGREES"] + n["VIOLATES (map mispair)"]
    if td and nd:
        tr = t["VIOLATES (map mispair)"] / td
        nr = n["VIOLATES (map mispair)"] / nd
        print("\n=== CONDITIONED NULL ===")
        print("   treated violation rate %.2f%%   null violation rate %.2f%%" %
              (100 * tr, 100 * nr))
        if nr > 0:
            print("   ENRICHMENT %.1fx" % (tr / nr))
        else:
            print("   null fired 0 -- report as a BOUND, not an enrichment")
        if tr == 0:
            print("   ** TREATED FIRED 0 -- the invariant is VACUOUS here **")

    print("\n=== sample VIOLATIONS in the treated group ===")
    for unit, nm, c, ds in examples["TREATED (mpn < 100)"][:18]:
        print("   %-30s %-46s our_class=%-22s target dtor callee=%s"
              % (unit[:30], nm[:46], c[:22], ds[:2]))
    print("\n=== sample VIOLATIONS in the NULL group (mpn==100) ===")
    for unit, nm, c, ds in examples["NULL (mpn == 100)"][:8]:
        print("   %-30s %-46s our_class=%-22s target dtor callee=%s"
              % (unit[:30], nm[:46], c[:22], ds[:2]))


if __name__ == "__main__":
    main()
