#!/usr/bin/env python3
"""EC-4: re-run the neighbourhood oracle's enrichment with an ISOLATION-MATCHED control.

THE CONFOUND THIS EXISTS TO REMOVE
----------------------------------
The neighbourhood oracle asks "does any map-named neighbour of this row share a
class with the row's unit?".  Whether it CAN answer "yes" depends almost
entirely on a structural property nobody controlled for: what fraction of the
neighbours it inspects are the row's OWN unit's code.  Measured on the live tree
(named rows, k=6, window 0x600):

    position  charged own-unit-neighbours   control own-unit-neighbours
    WHOLE          11.13%                        58.95%
    START          48.41%                        67.03%
    MID            85.39%                        90.57%
    END            55.51%                        70.98%

Two consequences, both fatal to the raw cross-tab:

  * "MID blockers are 0% foreign" is very largely STRUCTURAL.  A MID row is
    bracketed by its own block's functions by construction, so the oracle can
    hardly ever call it foreign.  That is a property of the QUESTION, not
    evidence that MID rows are correctly attributed.
  * The at-100 CONTROL is NOT the same structural population as the charged
    rows -- for WHOLE it is 5x less isolated.  So a charged-vs-control ratio
    pools two groups that differ in the very variable that drives the flag, and
    the resulting "enrichment" is partly composition.

This tool therefore bins rows by ISOLATION (own-unit-neighbour fraction) and
reports charged-vs-control WITHIN each bin, plus a single isolation-adjusted
(direct-standardised) rate: the charged foreign rate reweighted onto the
control's isolation distribution, and vice versa.  If the enrichment survives
standardisation it is attribution; if it collapses it was isolation.
"""
import argparse
import bisect
import collections
import json
import pathlib
import re
import sys

ANON_RX = re.compile(r"^fn_([0-9A-Fa-f]{8})$")
BINS = [(0.0, 0.001), (0.001, 0.25), (0.25, 0.50), (0.50, 0.75), (0.75, 1.001)]
BIN_NAMES = ["iso=0%", "0-25%", "25-50%", "50-75%", "75-100%"]


def bin_of(x):
    for i, (lo, hi) in enumerate(BINS):
        if lo <= x < hi:
            return i
    return len(BINS) - 1


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--root", default=".")
    ap.add_argument("--positions", required=True)
    ap.add_argument("--neigh", required=True)
    ap.add_argument("--k", type=int, default=6)
    ap.add_argument("--window", type=lambda x: int(x, 0), default=0x600)
    ap.add_argument("--out", required=True)
    a = ap.parse_args()

    root = pathlib.Path(a.root).resolve()
    amap = json.loads((root / "scripts/target_symbol_map.json").read_text())
    pairs = sorted((int(k, 16), v) for k, v in amap.items() if k.startswith("0x"))
    addrs = [p[0] for p in pairs]

    pos = json.loads(pathlib.Path(a.positions).read_text())
    neigh = json.loads(pathlib.Path(a.neigh).read_text())
    N = {(r["unit"], r["sym"]): r for r in neigh}

    blocks = collections.defaultdict(set)
    for r in pos:
        blocks[r["unit"]].add(tuple(int(x, 16) for x in r["block"]))

    rows = []
    for r in pos:
        key = (r["unit"], r["sym"])
        nr = N.get(key)
        if nr is None or nr["tmpl"]:
            continue
        va = int(r["va"], 16)
        i = bisect.bisect_left(addrs, va)
        same = tot = 0
        for j in range(max(0, i - a.k), min(len(pairs), i + a.k + 1)):
            nva, _ = pairs[j]
            if nva == va or abs(nva - va) > a.window:
                continue
            tot += 1
            if any(lo <= nva < hi for lo, hi in blocks[r["unit"]]):
                same += 1
        if not tot:
            continue
        rows.append(dict(unit=r["unit"], sym=r["sym"], pos=r["pos"], va=r["va"],
                         size=r["size"], fuzzy=r["fuzzy"], mpn=r["mpn"],
                         iso=same / tot, nbrs=tot,
                         foreign=nr["foreign"], stratum=nr["stratum"]))
    pathlib.Path(a.out).write_text(json.dumps(rows, indent=1))

    ch = [r for r in rows if r["stratum"] == "CHARGED"]
    co = [r for r in rows if r["stratum"] == "CONTROL"]

    def rate(rs):
        return (sum(1 for r in rs if r["foreign"]), len(rs))

    print(f"rows with an adjudicable neighbourhood: charged={len(ch)} control={len(co)}\n")
    print("=== FOREIGN RATE WITHIN ISOLATION BIN (own-unit-neighbour fraction) ===")
    print(f"{'bin':10s} {'chg_f':>6s} {'chg_n':>6s} {'chg%':>7s} | {'ctl_f':>6s} {'ctl_n':>6s} {'ctl%':>7s} | {'enr':>7s}")
    cb = [[r for r in ch if bin_of(r["iso"]) == i] for i in range(len(BINS))]
    ob = [[r for r in co if bin_of(r["iso"]) == i] for i in range(len(BINS))]
    for i, nm in enumerate(BIN_NAMES):
        cf, cn = rate(cb[i])
        of, on = rate(ob[i])
        cp = 100 * cf / cn if cn else 0
        op = 100 * of / on if on else 0
        e = (cp / op) if op else float("inf")
        print(f"{nm:10s} {cf:6d} {cn:6d} {cp:6.2f}% | {of:6d} {on:6d} {op:6.2f}% | {e:6.2f}x")

    cf, cn = rate(ch)
    of, on = rate(co)
    craw = 100 * cf / cn if cn else 0
    oraw = 100 * of / on if on else 0
    print(f"\nCRUDE      charged {craw:.2f}%  control {oraw:.2f}%  = "
          f"{craw/oraw if oraw else float('inf'):.2f}x  <-- CONFOUNDED")

    # direct standardisation: charged rates reweighted onto the CONTROL isolation mix
    num = den = 0.0
    for i in range(len(BINS)):
        w = len(ob[i])
        if not w or not cb[i]:
            continue
        f, n = rate(cb[i])
        num += w * (f / n)
        den += w
    adj = 100 * num / den if den else float("nan")
    print(f"STANDARDISED charged rate on the CONTROL isolation mix: {adj:.2f}%  "
          f"vs control {oraw:.2f}%  = {adj/oraw if oraw else float('inf'):.2f}x")

    # and the reverse direction, as a symmetry check
    num = den = 0.0
    for i in range(len(BINS)):
        w = len(cb[i])
        if not w or not ob[i]:
            continue
        f, n = rate(ob[i])
        num += w * (f / n)
        den += w
    adj2 = 100 * num / den if den else float("nan")
    print(f"STANDARDISED control rate on the CHARGED isolation mix: {adj2:.2f}%  "
          f"vs charged {craw:.2f}%  = {craw/adj2 if adj2 else float('inf'):.2f}x")
    print(f"\nwrote {a.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
