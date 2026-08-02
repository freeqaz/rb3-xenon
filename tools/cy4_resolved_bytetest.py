#!/usr/bin/env python3
"""Lane CY-4 part 2: (i) give NULL 1 real statistical power, and (ii) byte-test
the pairs that weak-external resolution just made testable.

(i) POWERING THE NULL
    cy4_weakext_adjudicate's NULL 1 drew ONE random target per fireable pair and
    scored 0/199. That is honest but nearly powerless: with ~2.1k candidate names
    the expected hit count is ~0.09, so observing 0 is uninformative. Here the
    null is (a) CONDITIONED on the ??_G destructor name family -- the hardest
    possible pool, every candidate has the exact shape the test looks for -- and
    (b) repeated R times to estimate a RATE rather than a single Bernoulli draw.
    If the weak-external default matched "any plausible destructor", this fires.

(ii) BYTE-TESTING THE RESOLVED PAIRS
    Before resolution B had no body, so CW-2's channel 1 returned
    SKIP:no_our_body and the row was UNKNOWN. After resolution B' = ??_G<C> is a
    function we compile, so we can ask CW-2's own question unchanged:
        ours[B'] vs RETAIL BYTES at addr(T), masked by our own relocation table
    and channel 2: locate B' in retail BY CONTENT.
    Verdicts reuse CW-2's lattice exactly; UNKNOWN stays explicit and is never a
    fallthrough into a charged class.

Read-only.
"""

import argparse
import collections
import json
import os
import random
import sys
from pathlib import Path

ROOT = os.environ.get("CY4_ROOT", str(Path(__file__).resolve().parent.parent))
os.environ.setdefault("CW2_ROOT", ROOT)
sys.path.insert(0, os.path.join(ROOT, "tools"))
from xbin_adjudicate import Xbin, Locator                   # noqa: E402


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=ROOT)
    ap.add_argument("--weakext", required=True)
    ap.add_argument("--adj", required=True)
    ap.add_argument("--out", default=None)
    ap.add_argument("--reps", type=int, default=400)
    ap.add_argument("--seed", type=int, default=20260802)
    a = ap.parse_args()

    W = json.load(open(a.weakext))
    pairs = json.load(open(a.adj))["pairs"]

    # ---------- (i) powered, family-conditioned null ----------------------
    alltargets = sorted({p[0] for p in pairs})
    gfam = sorted({t for t in alltargets if t.startswith("??_G")})
    benign = W["benign_weakext"]              # [T, B, nsites, cls]
    resolved = W["resolved_now_testable"]     # [T, B, B', nsites, cls]
    fireable = [(t, b) for t, b, _n, _c in benign] + \
               [(t, b) for t, b, _d, _n, _c in resolved]
    # default symbol per fireable B
    dflt = {b: t for t, b, _n, _c in benign}
    dflt.update({b: d for _t, b, d, _n, _c in resolved})

    rnd = random.Random(a.seed)
    hits = []
    for _ in range(a.reps):
        h = 0
        for _t, b in fireable:
            if dflt[b] == rnd.choice(gfam):
                h += 1
        hits.append(h)
    mean = sum(hits) / len(hits)
    treat = len(benign)
    print("=== NULL 1, POWERED and CONDITIONED on the ??_G destructor family ===")
    print("   candidate pool (distinct ??_G target names) %5d" % len(gfam))
    print("   fireable pairs                              %5d" % len(fireable))
    print("   treatment  default == true T                %5d (%5.1f%%)"
          % (treat, 100.0 * treat / len(fireable)))
    print("   null mean over %d reps                     %7.2f (%5.2f%%)  max %d"
          % (a.reps, mean, 100.0 * mean / len(fireable), max(hits)))
    print("   null fired at least once in %d/%d reps"
          % (sum(1 for h in hits if h), a.reps))
    if mean > 0:
        print("   ENRICHMENT                                  %.0fx" % (treat / mean))

    # ---------- (ii) byte-test the resolved pairs -------------------------
    x = Xbin(a.root)
    loc = Locator(x, cap=20000)
    print("\nour bodies %d   map names %d   pdata %d"
          % (len(x.ours), len(x.name2addr), len(x.pd)))

    def adjudicate(T, Bp):
        ta = x.name2addr.get(T)
        if not ta:
            return "UNKNOWN", "no_addr_for_T", []
        if len(ta) > 1:
            return "UNKNOWN", "T_multiple_addrs", []
        va = ta[0]
        rb = x.test(Bp, va)
        rt = x.test(T, va)
        if rb == "MATCH":
            return "BENIGN_direct", "ours[B']==retail@addr(T)", [va]
        hs, st = loc.hits(Bp)
        if va in hs:
            return "BENIGN_located", "addr(T) in hits(B')", hs
        if rt == "MATCH" and hs:
            return "WRONG_2ch", "T at addr(T); B' elsewhere (%d)" % len(hs), hs
        if rt == "MATCH" and rb == "DIFF":
            return "WRONG_1ch", "T at addr(T); B' unlocatable (%s)" % st, hs
        return "UNKNOWN", "B':%s T:%s loc:%s" % (rb, rt, st), hs

    verd = collections.Counter()
    vsites = collections.Counter()
    rows = []
    for T, B, Bp, ns, cls in resolved:
        v, why, hs = adjudicate(T, Bp)
        verd[v] += 1
        vsites[v] += ns
        rows.append([T, B, Bp, ns, cls, v, why, ["0x%08x" % h for h in hs[:6]]])

    print("\n=== BYTE VERDICTS on the %d pairs weak-ext resolution made testable ==="
          % len(resolved))
    for k in ("WRONG_2ch", "WRONG_1ch", "BENIGN_direct", "BENIGN_located", "UNKNOWN"):
        if verd[k]:
            print("   %-16s %4d pairs  %4d sites" % (k, verd[k], vsites[k]))
    print("\n   still-UNKNOWN reasons:")
    ur = collections.Counter(r[6] for r in rows if r[5] == "UNKNOWN")
    for k, n in ur.most_common():
        print("      %-56s %4d" % (k[:56], n))

    print("\n   newly CHARGED rows (two channels), by size of charge:")
    for r in sorted([r for r in rows if r[5] == "WRONG_2ch"], key=lambda z: -z[3])[:15]:
        print("     %2ds  ours call B=%s\n          resolves to  %s\n"
              "          retail calls T=%s\n          %s"
              % (r[3], r[1][:76], r[2][:76], r[0][:76], r[6]))

    if a.out:
        json.dump({"rows": rows, "null_mean": mean, "reps": a.reps,
                   "gfam_pool": len(gfam), "treatment": treat,
                   "fireable": len(fireable)}, open(a.out, "w"))
        print("\nwrote " + a.out)


if __name__ == "__main__":
    main()
