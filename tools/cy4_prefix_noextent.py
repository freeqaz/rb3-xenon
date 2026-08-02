#!/usr/bin/env python3
"""Lane CY-4 part 5: decide the RETAIL_no_extent block with a PREFIX test.

After the weak-external channel, the largest remaining blocker in the at-100
residue is not our source and not retail's bytes -- it is a missing .pdata
extent. 379 pairs / 1,235 sites read

    B:SKIP:no_pdata_extent  T:SKIP:no_pdata_extent  loc:...

CW-2's channel 1 gates on `pd[addr(T)] == len(ours[B])`, so with no extent at
addr(T) the gate can never open, however good either body is. (Memory:
.pdata-absence is NOT a "not a function" test -- the address is usually a real
function, we simply have no authoritative length for it.)

THE TEST, AND WHY IT IS SAFE
----------------------------
Compare ours[B], masked by our own relocation table exactly as CW-2 does,
against retail bytes at addr(T) for len(ours[B]) bytes -- a PREFIX comparison,
with no length agreement required.

A prefix match is sound evidence that the code at addr(T) IS our callee, so it
can support BENIGN. A prefix MISMATCH is NOT evidence of a wrong callee: our
decomp of B may simply be imperfect, and the 79.5%/96.7% recall figures on the
existing channels show that is common. So this test EMITS ONLY `BENIGN_prefix`
OR LEAVES THE ROW UNKNOWN. It is structurally incapable of manufacturing a
charge -- the defect-manufacturing direction is closed by construction, not by
care.

It inherits CW-2's anti-vacuity guard unchanged: >= 4 words AND relocated words
< 50% of the body, so a body that is mostly relocation cannot match by default.

CALIBRATION (both able to fail)
-------------------------------
  positive control : ours[X] prefix-vs-retail@addr(X) over mapped symbols, with
                     the .pdata gate DELIBERATELY DISABLED, so it measures the
                     prefix test itself rather than the length gate.
  RANDOM-OFFSET NULL: the identical masked body compared at random .text
                     offsets. This is the null the ICF work established as the
                     only honest one for body comparison, because PC-relative
                     displacements make naive byte identity vacuous.

Read-only.
"""

import argparse
import collections
import json
import os
import random
import re
import sys
from pathlib import Path

ROOT = os.environ.get("CY4_ROOT", str(Path(__file__).resolve().parent.parent))
os.environ.setdefault("CW2_ROOT", ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools"))
import xbin_adjudicate as XB                                    # noqa: E402
from xbin_adjudicate import Xbin, mask_words, retail_bytes      # noqa: E402
from cy4_weakext_adjudicate import build_index                  # noqa: E402


def prefix_test(x, name, va):
    """-> 'MATCH' | 'DIFF' | 'SKIP:<reason>'. Never a bool."""
    rec = x.ours.get(name)
    if rec is None:
        return "SKIP:no_our_body"
    raw, relocs = rec
    ln = len(raw)
    nwords = ln // 4
    if nwords < 4:
        return "SKIP:body_too_small"
    mo, no = mask_words(raw, relocs)
    if mo is None:
        return "SKIP:" + no
    if no * 2 >= nwords:
        return "SKIP:mask_ge_half"
    rb = retail_bytes(x.img, va, ln)
    if rb is None:
        return "SKIP:unreadable"
    mr, _ = mask_words(rb, relocs)
    return "MATCH" if mo == mr else "DIFF"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=ROOT)
    ap.add_argument("--adj", required=True)
    ap.add_argument("--resolved", required=True)
    ap.add_argument("--sample", type=int, default=4000)
    ap.add_argument("--seed", type=int, default=20260802)
    a = ap.parse_args()

    x = Xbin(a.root)
    weak, defined, _p, _n, _c = build_index(a.root)
    rnd = random.Random(a.seed)
    ts = [s for s in x.img.secs if s[0] == '.text'][0]
    lo, hi = 0x82000000 + ts[1], 0x82000000 + ts[1] + ts[2]

    # ---- calibration -----------------------------------------------------
    pop = [n for n in x.ours if len(x.name2addr.get(n, ())) == 1]
    if len(pop) > a.sample:
        pop = rnd.sample(pop, a.sample)
    pos = collections.Counter()
    null = collections.Counter()
    for n in pop:
        va = x.name2addr[n][0]
        pos[prefix_test(x, n, va)] += 1
        ra = rnd.randrange(lo, hi - 4096) & ~3
        null[prefix_test(x, n, ra)] += 1

    def rate(c):
        d = c["MATCH"] + c["DIFF"]
        return (100.0 * c["MATCH"] / d if d else float("nan")), d

    pr, pd_ = rate(pos)
    nr, nd = rate(null)
    print("=== PREFIX TEST CALIBRATION (pdata length gate DISABLED) ===")
    print("   positive control ours[X] @ addr(X)   %6.2f%% MATCH of %d decided"
          % (pr, pd_))
    print("   RANDOM-OFFSET NULL                   %6.2f%% MATCH of %d decided"
          % (nr, nd))
    if nr > 0:
        print("   enrichment                           %.1fx" % (pr / nr))
    else:
        print("   null fired 0/%d -- stated as a BOUND (<%.3f%%), not an enrichment"
              % (nd, 100.0 / max(1, nd)))
    for c, lab in ((pos, "positive"), (null, "null")):
        sk = {k: v for k, v in c.items() if k.startswith("SKIP")}
        print("   %-9s skips: %s" % (lab, sk))

    # ---- apply to the RETAIL_no_extent block ------------------------------
    decided_cy4 = {(r[0], r[1]) for r in json.load(open(a.resolved))["rows"]
                   if r[5] != "UNKNOWN"}
    pairs = json.load(open(a.adj))["pairs"]
    targets = []
    for T, B, cls, v, reason, _h, ns in pairs:
        if v != "UNKNOWN":
            continue
        if weak.get(B) == T and B not in defined:
            continue
        if (T, B) in decided_cy4:
            continue
        if "no_pdata_extent" not in reason:
            continue
        targets.append((T, B, cls, ns))

    print("\n=== APPLYING PREFIX TEST TO THE RETAIL_no_extent BLOCK ===")
    print("   candidate pairs %d / sites %d"
          % (len(targets), sum(t[3] for t in targets)))
    out = collections.Counter()
    outs = collections.Counter()
    benign = []
    for T, B, cls, ns in targets:
        ta = x.name2addr.get(T)
        if not ta or len(ta) > 1:
            out["UNKNOWN:no_unique_addr"] += 1
            outs["UNKNOWN:no_unique_addr"] += ns
            continue
        va = ta[0]
        r = prefix_test(x, B, va)
        if r == "MATCH":
            out["BENIGN_prefix"] += 1
            outs["BENIGN_prefix"] += ns
            benign.append((T, B, ns))
        else:
            k = "UNKNOWN:" + ("B_differs" if r == "DIFF" else r)
            out[k] += 1
            outs[k] += ns
    for k, n in out.most_common():
        print("   %-34s %5d pairs  %5d sites" % (k, n, outs[k]))
    print("\n   NOTE: this test cannot emit a WRONG verdict by construction;")
    print("   every non-MATCH stays UNKNOWN.")
    for T, B, ns in benign[:10]:
        print("     BENIGN %2ds  B=%s\n                 T=%s" % (ns, B[:74], T[:74]))


if __name__ == "__main__":
    main()
