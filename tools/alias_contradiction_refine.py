#!/usr/bin/env python3
"""Split CONTRADICTED alias memberships into DECISIVE and NOT-DECISIVE (ALIAS-2).

    python3 tools/alias_contradiction_refine.py --wt <repo-with-built-objs> \
        --memberships ~/tmp/alias2_memberships.json --out ~/tmp/alias2_withdraw.json

WHY THIS EXISTS
---------------
`verdict()` returns CONTRADICTED as its FALLBACK -- "none of L1..L5 could prove a
fold, and some word positively differs". That is not the same as "no fold
reading exists", and treating it as such would repeat the exact error GROUNDED-1
documented: three hash_map rows that flat T1 called REFUTED are PROVEN folds by
internal inconsistency, and withdrawing them would have been the error that lane
existed to avoid.

The two classes behave differently and must be separated:

* SIZE MISMATCH between survivor and folded spelling -- DECISIVE. Two COMDATs of
  different size cannot be folded by /OPT:ICF under any reading. This is
  GROUNDED-1's own standard, and the class whose withdrawal it predicted and
  measured to the byte.

* RELOCATION TARGET NAME differs -- NOT decisive on its face. `_Copy_Construct<T>`
  has identical machine code for every T and differs ONLY in the constructor it
  calls, so retail naming one callee and us naming another is exactly what a
  template twin looks like *whether or not* the two callees are themselves
  folded. If the callees fold, the parent fold is real and the alias is earned.

  ⇒ SECOND-ORDER TEST: compare the two differing CALLEES' body sizes (retail's
  callee in the retail objs, ours in our objs). If THOSE differ in size they
  cannot fold either, and the parent contradiction becomes decisive. If they are
  the same size, the parent is left NOT-DECISIVE and is NOT withdrawn -- it goes
  to a worklist as a candidate missing alias, which is the opposite action.

This tool therefore fails toward KEEPING an alias, which is the conservative
direction for a withdrawal audit: a false withdrawal destroys a real credit and
is invisible afterwards, while a retained-but-unproven alias stays on the books
and is re-auditable.
"""
import argparse, collections, json, os, re, sys
from pathlib import Path

RE_NAMES = re.compile(r"target names differ @(0x[0-9a-fA-F]+): (\S+) vs (\S+)")
RE_SIZE = re.compile(r"size (\d+) vs (\d+)")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--wt", required=True)
    ap.add_argument("--memberships", required=True)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()
    wt = Path(a.wt).resolve()

    sys.path.insert(0, str(Path(__file__).resolve().parent))
    from alias_forgiveness_audit import Sides
    S = Sides(wt)
    if sum(1 for n in S.traw if n.startswith("?")) < 1000:
        sys.exit("REFUSING: target objs look PRE-RENAMER; every size lookup would miss.")

    def size(side, n):
        v = side.get(n)
        return len(v[0]) if v else None

    mem = json.load(open(os.path.expanduser(a.memberships)))
    con = [x for x in mem if x["cls"] == "CONTRADICTED"]
    print("CONTRADICTED memberships: %d" % len(con))

    # The stored `why` is truncated to 200 chars upstream, which can cut a
    # mangled callee name in half and silently turn a decisive case into an
    # unresolved one. Re-adjudicate to get the full reason.
    memo = {}
    for x in con:
        _v, w = S.verdict(x["survivor"], x["folded"], memo)
        x["why"] = str(w)
    print("re-adjudicated with untruncated reasons")

    # ⛔ THE COMPARISON MUST BE WITHIN ONE BUILD.  Comparing RETAIL's body size to
    # OURS conflates "these two COMDATs are different" (a refuted fold) with "our
    # build emits this family at the wrong size" (a source defect that refutes
    # nothing about retail's link).  Our STLport emits __uninitialized_copy and
    # _M_allocate_and_copy uniformly +8 B vs retail across ~95 same-name pairs, so
    # a retail-vs-ours test fires on that whole family for the wrong reason and
    # would withdraw ~95 aliases to hide one shared defect.
    #   apples-to-apples: our(S) vs our(F).  If OUR build gives the two spellings
    #   different-sized COMDATs, they cannot be one COMDAT in any build.
    #   If our(S) == our(F) but retail(S) != our(S), the pair is INTERNALLY
    #   CONSISTENT and the gap is OUR divergence -> a source lead, not a
    #   withdrawal.
    dec, notdec, unknown, builddiv = [], [], [], []
    for x in con:
        why = x["why"]
        oS, oF = size(S.oraw, x["survivor"]), size(S.oraw, x["folded"])
        rS = size(S.traw, x["survivor"])
        ms = RE_SIZE.search(why)
        if ms and "different body SIZE" in why:
            if oS is not None and oF is not None and oS != oF:
                x["decisive"] = "SURVIVOR_SIZE_MISMATCH"
                x["detail"] = ("our(S)=%d B vs our(F)=%d B [retail(S)=%s] -- our own "
                               "build gives them different-sized COMDATs" % (oS, oF, rS))
                dec.append(x)
            elif oS is not None and oF is not None and oS == oF:
                x["decisive"] = "BUILD_DIVERGENCE"
                x["detail"] = ("our(S)=our(F)=%d B but retail(S)=%s -- INTERNALLY "
                               "CONSISTENT; our build diverges from retail. Source "
                               "defect, NOT a refuted fold." % (oS, rS))
                builddiv.append(x)
            else:
                x["decisive"] = "SURVIVOR_UNRESOLVED"
                x["detail"] = "our(S)=%s our(F)=%s retail(S)=%s" % (oS, oF, rS)
                unknown.append(x)
            continue
        mn = RE_NAMES.search(why)
        if mn:
            off, tcal, bcal = mn.group(1), mn.group(2), mn.group(3)
            # Same rule one level down: compare the two CALLEES inside OUR build.
            oA, oB = size(S.oraw, tcal), size(S.oraw, bcal)
            rA = size(S.traw, tcal)
            if oA is None or oB is None:
                x["decisive"] = "CALLEE_UNRESOLVED"
                x["detail"] = ("callees @%s our(A)=%s our(B)=%s retail(A)=%s : %s vs %s"
                               % (off, oA, oB, rA, tcal[:44], bcal[:44]))
                unknown.append(x); continue
            if oA != oB:
                x["decisive"] = "CALLEE_SIZE_MISMATCH"
                x["detail"] = ("callees @%s differ in OUR build: %s(%d B) vs %s(%d B)"
                               % (off, tcal[:46], oA, bcal[:46], oB))
                dec.append(x)
            else:
                x["decisive"] = "CALLEE_SAME_SIZE"
                x["detail"] = ("callees @%s both %d B in our build -- may themselves "
                               "fold; NOT withdrawn" % (off, oA))
                notdec.append(x)
            continue
        x["decisive"] = "OTHER"; x["detail"] = why[:120]
        unknown.append(x)

    print("\nREFINED SPLIT OF THE CONTRADICTIONS")
    print("  DECISIVE   (withdraw)      %4d" % len(dec))
    for k, n in collections.Counter(d["decisive"] for d in dec).most_common():
        print("       %-24s %4d" % (k, n))
    print("  NOT DECISIVE (keep)        %4d   callees same size -- may fold" % len(notdec))
    print("  BUILD DIVERGENCE (keep)    %4d   our(S)==our(F), retail differs -- "
          "SOURCE DEFECT, not a refuted fold" % len(builddiv))
    print("  UNRESOLVED  (keep)         %4d" % len(unknown))
    if builddiv:
        print("\n  build-divergence leads (fix the source, do NOT withdraw):")
        for x in builddiv[:8]:
            print("    %s\n      %s" % (x["survivor"][:88], x["detail"][:120]))

    print("\nDECISIVE withdrawals, by group:")
    byg = collections.defaultdict(list)
    for d in dec:
        byg[d["i"]].append(d)
    for i, xs in sorted(byg.items(), key=lambda kv: -len(kv[1])):
        print("  group %-5d %3d memberships  %s" % (i, len(xs), xs[0]["survivor"][:74]))
        for x in xs[:2]:
            print("        <- %s\n           %s" % (x["folded"][:82], x["detail"][:110]))

    json.dump({"decisive": dec, "not_decisive": notdec, "unresolved": unknown,
               "build_divergence": builddiv},
              open(os.path.expanduser(a.out), "w"), indent=1)
    print("\nwrote %s" % a.out)


if __name__ == "__main__":
    main()
