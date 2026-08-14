#!/usr/bin/env python3
"""foldprove2_cheapkill.py -- run the CHEAP structural kill over WRONGCALL-3's 100
fold-shaped pairs BEFORE any body work.

Lane FOLDPROVE-2 (2026-08-14).  Lane WRONGCALL-3 re-tested NOGROUP-1's 313
"wrong-callee source defects" on the MASKED BODY ALONE and found 100 pairs whose
masked body is BYTE-IDENTICAL -- they differ only in relocation TARGET NAMES, which
is TRUE BY CONSTRUCTION for two template instantiations over layout-compatible types.
It routed them to a fold lane rather than to source work.

Lane FOLDPROVE-1's lesson, which this script exists to apply FIRST:

    The census gate SHORT-CIRCUITS on vacuity/tolerance BEFORE testing residency and
    uniqueness, so its bucket labels report THE FIRST GATE THAT FIRED, not the full
    obstruction set.  Non-short-circuited, 41 of its 60 were ALSO residency-blocked,
    and 41 of 41 had addr(F) != addr(S).  RETAIL KEEPING TWO ADDRESSES IS THE
    DEFINITION OF "NOT FOLDED".  The check was cheap and the map already had the answer.

So: evaluate ALL gates non-short-circuited, and report every obstruction per pair.

  G-RESIDENCY   the map places F at its own live address.  If addr(F) exists and is
                DISJOINT from addr(S), retail keeps two bodies => NOT a fold, and an
                alias there would forgive a genuinely wrong callee.  Complete refutation.
  G-UNIQUENESS  retail must keep exactly ONE address for the body signature.  N>1 means
                our call site's true target is one of several.
  G-VACUITY     body below icf_alias_build's floor (<4 words unmasked, or >50%
                relocated) -- a 4-byte `blr` compares equal to everything.
  G-LITERAL     every relocation slot's target name agrees LITERALLY (no placeholder
                tolerance).  For THIS population this is expected to FAIL by
                construction -- the per-instantiation callee names ARE the difference --
                so it is reported, never used alone to refute.

Read-only.  Writes a JSON verdict file only.
"""
import argparse
import collections
import glob
import json
import os
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(ROOT, "tools"))

from icf_alias_build import collect, relocs_agree, vacuous  # noqa: E402
from nogroup_census import body_sig, load_map, literal_relocs  # noqa: E402

QUEUE = os.path.join(ROOT, "docs/decomp/nogroup-wrong-callee-queue-NOGROUP1.tsv")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--out", default="/home/free/tmp/fp2_cheapkill.json")
    ap.add_argument("--shape", default="MASKED_IDENTICAL")
    a = ap.parse_args()

    tgt = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/obj/**/*.obj"), recursive=True)), "t")
    ours = collect(sorted(glob.glob(os.path.join(ROOT, "build/45410914/src/**/*.obj"), recursive=True)), "o")
    _m, name2addr = load_map()
    mapped = set(name2addr)

    # retail body-signature index: keys are target-obj symbols = distinct retail addrs
    sig_addrs = collections.defaultdict(list)
    for n, rec in tgt.items():
        sig_addrs[body_sig(rec)].append(n)

    lines = open(QUEUE).read().splitlines()
    head = lines[0].split("\t")
    ci = {k: i for i, k in enumerate(head)}
    out = []
    for ln in lines[1:]:
        p = ln.split("\t")
        if len(p) < len(head):
            continue
        if p[ci["wc3_shape"]] != a.shape:
            continue
        S, F = p[ci["retail_S"]], p[ci["ours_F"]]
        rt, ob = tgt.get(S), ours.get(F)
        aS = sorted(name2addr.get(S, ()))
        aF = sorted(name2addr.get(F, ()))
        sig = body_sig(rt) if rt else None
        dupes = sig_addrs.get(sig, []) if sig else []

        obstructions = []
        # G-RESIDENCY: the decisive cheap one
        if aF:
            if set(aF) & set(aS):
                resid = "SHARED_ADDR"          # map already puts them on one address
            else:
                resid = "TWO_ADDRESSES"        # complete refutation
                obstructions.append("residency:addr(F)!=addr(S)")
        else:
            resid = "F_NOT_MAP_RESIDENT"
        if len(dupes) != 1:
            obstructions.append("uniqueness:%d" % len(dupes))
        if rt is None or ob is None:
            obstructions.append("absent_body")
        else:
            if vacuous(rt) or vacuous(ob):
                obstructions.append("vacuity")
            if not literal_relocs(rt, ob):
                obstructions.append("literal")
            if not relocs_agree(rt, ob, mapped, strict=True):
                obstructions.append("relocs_agree")

        out.append({
            "retail": S, "ours": F,
            "solo_B": int(p[ci["solo_closable_B"]]),
            "solo_rows": int(p[ci["solo_rows"]]),
            "sites": int(p[ci["sites"]]),
            "strength": p[ci["strength"]],
            "units": p[ci["units"]],
            "addr_retail": aS, "addr_ours": aF,
            "residency": resid,
            "retail_addrs_for_body": len(dupes),
            "dupe_sample": sorted(dupes)[:6],
            "size": rt[2] if rt else None,
            "nrel": len(rt[1]) if rt else None,
            "obstructions": obstructions,
            "survives_cheap_kill": resid != "TWO_ADDRESSES" and len(dupes) == 1,
        })

    json.dump(out, open(a.out, "w"), indent=1)

    tot_B = sum(r["solo_B"] for r in out)
    print("population: %d pairs / %d solo_B\n" % (len(out), tot_B))

    print("=== G-RESIDENCY (the cheap decisive gate) ===")
    c = collections.Counter(r["residency"] for r in out)
    b = collections.Counter()
    for r in out:
        b[r["residency"]] += r["solo_B"]
    for k, v in c.most_common():
        print("  %-20s %4d pairs  %7d B" % (k, v, b[k]))

    print("\n=== G-UNIQUENESS (retail addresses holding this body) ===")
    c = collections.Counter(min(r["retail_addrs_for_body"], 9) for r in out)
    for k in sorted(c):
        print("  %s addr%-8s %4d pairs" % (k if k < 9 else "9+", "", c[k]))

    print("\n=== JOINT: survives residency AND uniqueness ===")
    surv = [r for r in out if r["survives_cheap_kill"]]
    print("  survivors: %d pairs / %d B" % (len(surv), sum(r["solo_B"] for r in surv)))
    print("  killed:    %d pairs / %d B" % (len(out) - len(surv), tot_B - sum(r["solo_B"] for r in surv)))

    print("\n=== full obstruction sets (non-short-circuited) ===")
    c = collections.Counter(",".join(r["obstructions"]) or "(none)" for r in out)
    for k, v in c.most_common(20):
        print("  %-52s %4d" % (k[:52], v))

    print("\n=== survivors, by solo_B ===")
    for r in sorted(surv, key=lambda r: -r["solo_B"]):
        print("  %7d B %2d rows  %-4s nrel=%-3s %s" % (
            r["solo_B"], r["solo_rows"], r["size"], r["nrel"], r["retail"][:64]))
        print("           ours: %s" % r["ours"][:78])
    return 0


if __name__ == "__main__":
    sys.exit(main())
