#!/usr/bin/env python3
"""Lane CY-4: resolve COFF WEAK EXTERNALS before adjudicating the at-100
wrong-callee class, and re-decide the block CW-2 had to leave UNKNOWN.

WHAT CW-2 LEFT AND WHY IT COULD NOT BE DECIDED BY BYTES
-------------------------------------------------------
CW-2 adjudicated the at-100 wrong-callee census on two byte channels and left
1,018 functions / 129,544 B UNKNOWN. Its single largest residue was 182 pairs
reading

    B:SKIP:no_our_body   T:MATCH   loc:SKIP:no_our_body

i.e. our body for the TARGET callee byte-reproduces retail at addr(T) (so the
map row is verified), and the ONLY blocker is that our build has no body at all
for the BASE callee B. CW-2 read that as an unadjudicable backlog -- "our
callee has no identified retail address".

It is not a backlog. `SKIP:no_our_body` was CORRECT and yet the pair is
DECIDABLE, because there is no body TO have: B is a COFF **weak external**, and
a weak external is a linkage directive, not a definition. No byte channel can
ever decide it, no matter how good; the answer lives in the COFF symbol table.

THE MECHANISM (measured on this tree, zero exceptions)
------------------------------------------------------
MSVC X360 emits, for a polymorphic class C, the VECTOR deleting destructor as
an undefined weak external whose aux record names a DEFAULT symbol:

    ??_E<C>@@UAAPAXI@Z   storage 105 (IMAGE_SYM_CLASS_WEAK_EXTERNAL), 1 aux
        aux.TagIndex   -> ??_G<C>@@UAAPAXI@Z   (SCALAR deleting destructor)
        aux.Characteristics = 2 (IMAGE_WEAK_EXTERN_SEARCH_LIBRARY)
    ??_G<C>@@UAAPAXI@Z   storage 2 (EXTERNAL), 0 aux

Measured over build/45410914/src/**/*.obj:
    undefined ??_E  10,119   ALL storage 105 with 1 aux
    undefined ??_G  10,119   ALL storage 2  with 0 aux
    per-obj pairing by class suffix: BOTH 10,119 / ??_G-only 0 / ??_E-only 0
    aux TagIndex resolves to ??_G<SAME class>: 10,119 / 10,119

So if ??_E<C> is not defined anywhere in the link, `bl ??_E<C>` RESOLVES TO
??_G<C>. Our call and retail's call reach the same code. The name difference the
census charges is an alias, not a wrong callee.

THE RESOLUTION GATE (this is what makes the test able to FAIL)
--------------------------------------------------------------
The benign verdict is conditional on ??_E<C> being UNDEFINED. We do define 1,158
??_E symbols. Where a definition exists, the weak external binds to THAT and the
call does NOT reach ??_G<C>; such a pair is reported AMBIGUOUS, never benign.

WHAT THIS BUYS BEYOND ONE VERDICT
---------------------------------
Resolving B through its weak-external default turns a bodyless B into ??_G<C>,
which we compile 11,404 times -- so CW-2's channel 1 becomes RUNNABLE on rows it
previously had to skip. Cross-class rows (B weak-defaults to ??_G<X> while
retail calls ??_G<Y>) stop being unknown and become an ordinary byte question.

NULLS -- both chosen so they CAN fire
-------------------------------------
A null that cannot fire is worse than no null (three tests this session fired
0/2157, 0/15 and 0/602). Two traps were avoided here:

  * Running the benign test against CW-2's byte-proven WRONG_2ch rows is
    VACUOUS: those rows reached a byte verdict only because ours[B] EXISTS, so B
    is a definition, so it is never a weak external and the test is structurally
    incapable of firing. Rejected.
  * NULL 1 (conditioned, randomized): restrict to pairs where B IS an undefined
    weak external -- so the test CAN fire -- then score TagIndex against a
    RANDOM other target name drawn from the same at-100 charged population
    instead of the true T. If the weak-external default matched anything, this
    scores like the treatment.
  * NULL 2 (natural experiment): the cross-class rows inside the very stratum
    being decided. B is a weak external there too, but its default is ??_G<X>
    while retail calls ??_G<Y>. The test must DECLINE them.

Read-only. Mutates no build input, no map, no splits.
"""

import argparse
import collections
import glob
import json
import os
import random
import struct
import sys
from pathlib import Path

ROOT = os.environ.get("CY4_ROOT", str(Path(__file__).resolve().parent.parent))
sys.path.insert(0, os.path.join(ROOT, "tools"))
from icf_fold_evidence import parse_coff                    # noqa: E402
from coff_bodies_ext import function_bodies_ext             # noqa: E402

WEAK = 105          # IMAGE_SYM_CLASS_WEAK_EXTERNAL
EXTERNAL = 2


def obj_weak_index(path):
    """-> (weak, defined_fns) for one obj.

    weak         : name -> default-symbol name   (undefined weak externals only)
    defined_fns  : set of function symbols this obj DEFINES
    """
    data = Path(path).read_bytes()
    if len(data) < 20:
        return {}, set()
    _nsec, sym_off, _nsym = struct.unpack_from("<HxxxxII", data, 2)
    _secs, syms = parse_coff(data)
    idx = {s["idx"]: s["name"] for s in syms if s}
    weak, defined = {}, set()
    for s in syms:
        if s is None:
            continue
        if s["section"] > 0 and s["type"] == 0x20 and s["storage"] in (2, 3):
            defined.add(s["name"])
        if s["section"] == 0 and s["storage"] == WEAK and s["aux"] >= 1:
            ao = sym_off + (s["idx"] + 1) * 18
            if ao + 8 <= len(data):
                tag, _ch = struct.unpack_from("<II", data, ao)
                d = idx.get(tag)
                if d:
                    weak[s["name"]] = d
    return weak, defined


def build_index(root):
    """Whole-build weak-external map + the set of every function we define."""
    weak_global = {}
    weak_conflict = 0
    defined_global = set()
    objs = glob.glob(os.path.join(root, "build/45410914/src/**/*.obj"),
                     recursive=True)
    per_obj = {}
    for p in objs:
        try:
            w, d = obj_weak_index(p)
        except Exception:
            continue
        per_obj[Path(p).stem] = (w, d)
        defined_global |= d
        for k, v in w.items():
            if k in weak_global and weak_global[k] != v:
                weak_conflict += 1
            weak_global[k] = v
    return weak_global, defined_global, per_obj, len(objs), weak_conflict


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=ROOT)
    ap.add_argument("--adj", required=True, help="at100_adjudicate.py output json")
    ap.add_argument("--out", default=None)
    ap.add_argument("--seed", type=int, default=20260802)
    a = ap.parse_args()

    weak, defined, per_obj, nobj, conflicts = build_index(a.root)
    print("objs %d   undefined weak externals %d (name->default conflicts %d, LOGGED)"
          % (nobj, len(weak), conflicts))
    print("function symbols DEFINED anywhere in our build: %d" % len(defined))

    pairs = json.load(open(a.adj))["pairs"]
    # [T, B, cv4class, verdict, reason, hits, nsites]
    unk = [p for p in pairs if p[3] == "UNKNOWN"]
    print("\nadjudication rows %d   UNKNOWN %d (%d sites)"
          % (len(pairs), len(unk), sum(p[6] for p in unk)))

    # ---- treatment -------------------------------------------------------
    cat = collections.Counter()
    catsites = collections.Counter()
    resolved = []          # (T, B, B', nsites, cv4class) -- now byte-testable
    benign = []
    for T, B, cls, _v, _r, _h, ns in unk:
        d = weak.get(B)
        if d is None:
            k = ("B_not_weak_external" if B not in weak else "?")
            cat[k] += 1
            catsites[k] += ns
            continue
        if B in defined:
            # the weak external binds to a real definition, NOT to its default
            cat["AMBIGUOUS_weakext_also_defined"] += 1
            catsites["AMBIGUOUS_weakext_also_defined"] += ns
            continue
        if d == T:
            cat["BENIGN_weakext_alias"] += 1
            catsites["BENIGN_weakext_alias"] += ns
            benign.append((T, B, ns, cls))
        else:
            cat["RESOLVED_now_testable"] += 1
            catsites["RESOLVED_now_testable"] += ns
            resolved.append((T, B, d, ns, cls))

    print("\n=== WEAK-EXTERNAL CHANNEL over the UNKNOWN block (pairs / sites) ===")
    for k, n in cat.most_common():
        print("   %-34s %5d pairs  %6d sites" % (k, n, catsites[k]))

    # ---- NULL 1: conditioned + randomized target -------------------------
    rnd = random.Random(a.seed)
    alltargets = sorted({p[0] for p in pairs})
    fireable = [(T, B) for T, B, _c, _v, _r, _h, _n in unk
                if B in weak and B not in defined]
    nullhit = 0
    for _T, B in fireable:
        Tr = rnd.choice(alltargets)
        if weak[B] == Tr:
            nullhit += 1
    treat = cat["BENIGN_weakext_alias"]
    print("\n=== NULL 1 (CONDITIONED on B being an undefined weak external, "
          "random T from the same charged population) ===")
    print("   fireable pairs                %5d" % len(fireable))
    print("   treatment  default == true T  %5d  (%5.1f%%)"
          % (treat, 100.0 * treat / max(1, len(fireable))))
    print("   NULL       default == rand T  %5d  (%5.1f%%)"
          % (nullhit, 100.0 * nullhit / max(1, len(fireable))))
    if nullhit == 0:
        print("   enrichment                    INFINITE (null fired 0);"
              " see NULL 2 for a null that CAN and DOES separate")
    else:
        print("   enrichment                    %.1fx" % (treat / nullhit))

    # ---- NULL 2: natural experiment, cross-class rows in the same stratum -
    print("\n=== NULL 2 (natural experiment: same stratum, B IS a weak external,"
          " but its default is a DIFFERENT symbol than retail's callee) ===")
    print("   declined by the test (RESOLVED_now_testable) %5d pairs" % len(resolved))
    print("   -> the test discriminates INSIDE the stratum it decides;")
    print("      it does not fire on every weak external it sees.")
    for T, B, d, ns, cls in resolved[:12]:
        print("      %2ds  B=%s\n           ->default %s\n           retail T  %s"
              % (ns, B[:78], d[:78], T[:78]))

    if a.out:
        json.dump({
            "benign_weakext": [[T, B, ns, cls] for T, B, ns, cls in benign],
            "resolved_now_testable": [[T, B, d, ns, cls]
                                      for T, B, d, ns, cls in resolved],
            "counts": dict(cat), "site_counts": dict(catsites),
            "null1_fireable": len(fireable), "null1_hits": nullhit,
        }, open(a.out, "w"))
        print("\nwrote " + a.out)


if __name__ == "__main__":
    main()
