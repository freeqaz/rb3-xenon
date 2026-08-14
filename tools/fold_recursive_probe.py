#!/usr/bin/env python3
"""fold_recursive_probe.py -- decide a fold-shaped pair by RECURSIVE RELOCATION
CONSISTENCY, the channel this population actually needs.

Lane FOLDPROVE-2 (2026-08-14).  WRONGCALL-3 left 100 pairs whose MASKED bodies are
byte-identical and which differ ONLY in relocation TARGET NAMES -- because for two
template instantiations over layout-compatible types the code is identical while the
callees are PER-INSTANTIATION symbols.  That is why `T_F` is false BY CONSTRUCTION
and why neither `relocs_agree` nor the masked-body comparator can decide them: the
entire evidentiary gap IS the differing relocation slots.

THE CHANNEL.  /OPT:ICF folds only COMDATs identical INCLUDING RELOCATIONS (CD-7).  So
a parent fold is real only if every differing relocation slot RESOLVES TO ONE ADDRESS
in the shipped image -- i.e. THE FOLD IS RECURSIVE.  map<CRC,float>::operator[] can
fold with map<int,float>::operator[] only if the _Rb_tree<CRC,...> helpers it calls
themselves folded with the _Rb_tree<int,...> helpers we call.

That makes each differing slot decidable, and -- the load-bearing part -- REFUTABLE:

  A. both slot targets map-resident, addr differs  => REFUTE.  The parent's relocations
     point at two distinct retail addresses, so the parent COMDATs are NOT identical
     including relocations.  This is FOLDPROVE-1's residency kill applied one level down,
     where it has never been run.
  B. both map-resident, same addr                  => AGREE (already one address).
  C. our slot target not map-resident, retail's is => recurse: compare retail's body at
     that name against OUR body for our slot target.  Masked-identical => CONSISTENT;
     masked-DIFFERENT or size-different => REFUTE (the callees are different code).
  D. retail slot target is a placeholder (fn_/lbl_) => UNRESOLVED on this channel.
     Literal (lbl_) slots are fold_literal_probe.py's job, not this one.

A pair is SUPPORTED only when >=1 slot resolved and ZERO slots are unresolved and zero
refute.  Any refute is decisive against the fold; unresolved slots leave it UNDECIDED,
never proven -- an unproven alias lifts name_check BY CONSTRUCTION, so erring strict is
the only safe direction.

⚠ SUPPORTED is ONE channel.  The lane rule is >=2 independent channels before install.

Read-only.
"""
import argparse
import collections
import glob
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

from icf_alias_build import collect, placeholder  # noqa: E402
from nogroup_census import load_map  # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--infile", default="/home/free/tmp/fp2_cheapkill.json")
    ap.add_argument("--out", default="/home/free/tmp/fp2_recursive.json")
    ap.add_argument("--only-survivors", action="store_true", default=True)
    a = ap.parse_args()

    tgt = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/obj/**/*.obj"), recursive=True)), "t")
    ours = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/src/**/*.obj"), recursive=True)), "o")
    _m, name2addr = load_map()

    def addrs(n):
        return name2addr.get(n, set())

    pairs = json.load(open(a.infile))
    if a.only_survivors:
        pairs = [p for p in pairs if p["survives_cheap_kill"]]

    out = []
    for p in pairs:
        S, F = p["retail"], p["ours"]
        rt, ob = tgt.get(S), ours.get(F)
        slots = []
        if rt is None or ob is None or len(rt[1]) != len(ob[1]):
            verdict, why = "UNDECIDED", "body or relocation-count mismatch"
        else:
            for (_o, rn, _t), (_o2, on, _t2) in zip(rt[1], ob[1]):
                if rn == on:
                    slots.append(("AGREE_SAME_NAME", rn, on))
                    continue
                ar, ao = addrs(rn), addrs(on)
                if ar and ao:
                    if ar & ao:
                        slots.append(("AGREE_SAME_ADDR", rn, on))
                    else:
                        slots.append(("REFUTE_two_addrs", rn, on))
                elif placeholder(rn):
                    slots.append(("UNRESOLVED_placeholder_retail", rn, on))
                else:
                    # recurse: retail's callee body vs OUR callee body
                    rc, oc = tgt.get(rn), ours.get(on)
                    if rc is None or oc is None:
                        slots.append(("UNRESOLVED_no_body", rn, on))
                    elif rc[2] != oc[2]:
                        slots.append(("REFUTE_callee_size", rn, on))
                    elif rc[0] != oc[0]:
                        slots.append(("REFUTE_callee_body", rn, on))
                    else:
                        slots.append(("CONSISTENT_callee_masked_eq", rn, on))
            kinds = [s[0] for s in slots]
            nref = sum(1 for k in kinds if k.startswith("REFUTE"))
            nunres = sum(1 for k in kinds if k.startswith("UNRESOLVED"))
            nres = sum(1 for k in kinds if k.startswith(("AGREE_SAME_ADDR", "CONSISTENT")))
            if nref:
                verdict = "REFUTED"
                why = "%d relocation slot(s) resolve to different retail bodies" % nref
            elif nunres:
                verdict = "UNDECIDED"
                why = "%d slot(s) unresolvable on this channel" % nunres
            elif nres:
                verdict = "SUPPORTED"
                why = "all %d differing slot(s) resolve to one retail body" % nres
            else:
                verdict = "UNDECIDED"
                why = "no differing slot carried information"
        q = dict(p)
        q["recursive_verdict"] = verdict
        q["recursive_why"] = why
        q["slots"] = slots
        out.append(q)

    json.dump(out, open(a.out, "w"), indent=1)

    print("recursive-relocation channel over %d cheap-kill survivors\n" % len(out))
    c = collections.Counter(r["recursive_verdict"] for r in out)
    b = collections.Counter()
    for r in out:
        b[r["recursive_verdict"]] += r["solo_B"]
    for k, v in c.most_common():
        print("  %-12s %4d pairs  %7d B" % (k, v, b[k]))

    print("\n=== slot-kind census ===")
    ck = collections.Counter(s[0] for r in out for s in r["slots"])
    for k, v in ck.most_common():
        print("  %-32s %5d" % (k, v))

    for want in ("SUPPORTED", "UNDECIDED"):
        sel = [r for r in out if r["recursive_verdict"] == want]
        print("\n=== %s (%d) ===" % (want, len(sel)))
        for r in sorted(sel, key=lambda r: -r["solo_B"])[:25]:
            print("  %6d B %2d rows  sz=%-4s %s" % (r["solo_B"], r["solo_rows"], r["size"], r["retail"][:70]))
            print("          ours: %s" % r["ours"][:76])
            print("          %s" % r["recursive_why"])
    return 0


if __name__ == "__main__":
    sys.exit(main())
