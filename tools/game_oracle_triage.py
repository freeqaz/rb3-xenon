#!/usr/bin/env python3
"""Whole-game identification triage from the RB3-Wii BinDiff oracle.

Consumes `unified_id_rb3wii.json` (RB3-360 addr -> RB3-Wii source TU, produced by
`docs/plans/bindiff-vs-rb3wii.md`) and triages **every** game TU it locates --
not just meta_band -- into a porting/pin-priority map.

For each TU it computes the dominant contiguous cluster (8 KB gap-split), its
density and purity, the candidate `.text` span snapped to `symbols.txt`, and the
count of high-confidence individual identifications.  It then cross-references
local availability:

  * WIRED    -- declared in config/45410914/objects.json (compiles today)
  * PRESENT  -- file exists under src/ (ported, maybe not wired)
  * PORTABLE -- file exists in ../rb3 (Wii decomp) and can be ported

and buckets the result:

  A) WIRED + PINNABLE        -> immediate band3 pin-wave (feed the pairing stream)
  B) PRESENT-but-UNWIRED + PINNABLE
  C) PORTABLE + PINNABLE (not present) -> porting-priority queue (ranked by mass)
  D) located but NOT pinnable (scatter) -> low density/purity; deprioritise

Read-only: touches no build inputs (no objects.json / splits.txt / ninja).

Usage:
    python3 tools/game_oracle_triage.py            # human report to stdout
    python3 tools/game_oracle_triage.py --json OUT  # also write the full table
"""
import argparse
import bisect
import json
import os
import re
from collections import Counter, defaultdict

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
ORACLE = os.path.join(ROOT, "unified_id_rb3wii.json")
SYMBOLS = os.path.join(ROOT, "config/45410914/symbols.txt")
OBJECTS = os.path.join(ROOT, "config/45410914/objects.json")
RB3_WII = os.path.abspath(os.path.join(ROOT, "..", "rb3"))

GAP = 8192          # 8 KB gap-split (matches the DC3 oracle convention)
AVG_FN = 40         # avg fn-size proxy for the density denominator
PIN_DENSITY = 15.0  # %  -- dominant-cluster density bar
PIN_PURITY = 0.70   # fraction of in-span matches that belong to the TU
PIN_DOM = 3         # min fns in the dominant cluster
HI_SIM, HI_CONF = 0.8, 0.9


def tu_key(bindiff_src):
    """band3/src/meta_band/Foo.cpp -> ('band3', 'meta_band/Foo.cpp')."""
    for top in ("band3", "network"):
        marker = top + "/src/"
        i = bindiff_src.find(marker)
        if i >= 0:
            return top, bindiff_src[i + len(marker):]
    return None, bindiff_src


def load_symbols():
    syms = []
    rx = re.compile(r"fn_([0-9A-Fa-f]+) = \.text:0x([0-9A-Fa-f]+);.*size:0x([0-9A-Fa-f]+)")
    with open(SYMBOLS) as f:
        for line in f:
            m = rx.match(line)
            if m:
                syms.append((int(m.group(2), 16), int(m.group(3), 16)))
    syms.sort()
    return syms


def load_wired():
    o = json.load(open(OBJECTS))
    wired = set()
    for group in o.values():
        if not isinstance(group, dict):
            continue
        for k in group.get("objects", {}):
            wired.add(k)          # e.g. "band3/meta_band/Foo.cpp"
    return wired


def clusters(addrs):
    addrs = sorted(addrs)
    out, cur = [], [addrs[0]]
    for a in addrs[1:]:
        if a - cur[-1] <= GAP:
            cur.append(a)
        else:
            out.append(cur)
            cur = [a]
    out.append(cur)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", help="write the full per-TU table to this path")
    ap.add_argument("--area", help="restrict to TUs whose canonical id starts with this (e.g. band3/meta_band)")
    args = ap.parse_args()

    recs = json.load(open(ORACLE))
    syms = load_symbols()
    sym_addrs = [s[0] for s in syms]

    def snap_end(addr):
        i = bisect.bisect_left(sym_addrs, addr)
        if i < len(sym_addrs) and sym_addrs[i] == addr:
            return addr + syms[i][1]
        return addr + 4

    wired = load_wired()

    # Dedup by rb3 addr, keeping the highest-similarity attribution.
    best = {}
    for r in recs:
        top, rel = tu_key(r["bindiff_src"])
        if top is None:
            continue
        a = int(r["rb3_addr"], 16)
        sim = r.get("similarity", 0.0)
        if a not in best or sim > best[a][1]:
            best[a] = (f"{top}/{rel}", sim, r.get("confidence", 0.0))

    addr_to_tu = {a: v[0] for a, v in best.items()}
    all_addrs = sorted(addr_to_tu)

    by_tu = defaultdict(list)
    hiconf = Counter()
    for a, (tu, sim, conf) in best.items():
        by_tu[tu].append(a)
        if sim >= HI_SIM and conf >= HI_CONF:
            hiconf[tu] += 1

    rows = []
    for tu, addrs in by_tu.items():
        if args.area and not tu.startswith(args.area):
            continue
        cl = clusters(addrs)
        dom = max(cl, key=len)
        s, e = dom[0], snap_end(dom[-1])
        span = e - s
        density = (len(dom) / (span / AVG_FN) * 100.0) if span > 0 else 100.0
        inside = [a for a in all_addrs if s <= a < e]
        cnt = Counter(addr_to_tu[a] for a in inside)
        purity = cnt[tu] / len(inside) if inside else 0.0
        contaminants = [(t.split("/")[-1], c) for t, c in cnt.most_common(3) if t != tu]

        top = tu.split("/")[0]
        rel = tu.split("/", 1)[1]
        present = os.path.exists(os.path.join(ROOT, "src", top, rel))
        portable = os.path.exists(os.path.join(RB3_WII, "src", top, rel))
        is_wired = tu in wired

        # Pinnable: a high-purity dominant cluster that won't swallow neighbours.
        # A *large* high-purity cluster (dom>=8) is worth pinning even below the
        # density bar -- the big meta_band TUs (AccomplishmentManager/Progress)
        # are sparse (sub-40B accessors spread out) but their dominant run is a
        # real, high-purity sub-range. Purity is the true collision gate; density
        # only screens tiny loose hulls.
        pinnable = (len(dom) >= PIN_DOM and purity >= PIN_PURITY
                    and (density >= PIN_DENSITY or len(dom) >= 8))

        if is_wired and pinnable:
            bucket = "A_wired_pinnable"
        elif present and pinnable:
            bucket = "B_present_unwired_pinnable"
        elif portable and pinnable:
            bucket = "C_portable_pinnable"
        else:
            bucket = "D_scatter"

        rows.append({
            "tu": tu,
            "matched": len(addrs),
            "dom": len(dom),
            "n_clusters": len(cl),
            "span_start": f"0x{s:08X}",
            "span_end": f"0x{e:08X}",
            "span_bytes": span,
            "density": round(density, 1),
            "purity": round(purity, 3),
            "hiconf": hiconf[tu],
            "wired": is_wired,
            "present": present,
            "portable": portable,
            "pinnable": pinnable,
            "bucket": bucket,
            "contaminants": contaminants,
        })

    rows.sort(key=lambda r: (-r["dom"], -r["density"]))

    if args.json:
        json.dump(rows, open(args.json, "w"), indent=1)

    # ---- human report ----
    buckets = defaultdict(list)
    for r in rows:
        buckets[r["bucket"]].append(r)

    tot_addr = len(addr_to_tu)
    print(f"# Game-code identification triage (RB3-Wii oracle)")
    print(f"# {tot_addr} located game fns across {len(by_tu)} TUs "
          f"(deduped from {len(recs)} oracle records)\n")

    def show(title, key, extra=""):
        b = buckets.get(key, [])
        n_fns = sum(r["dom"] for r in b)
        print(f"\n{'='*92}\n{title}  --  {len(b)} TUs, {n_fns} dominant-cluster fns{extra}\n{'='*92}")
        print(f"{'TU':52s} {'dom':>4s} {'dens%':>6s} {'pur%':>5s} {'hiC':>4s}  candidate .text span")
        for r in b:
            cont = ""
            if r["purity"] < 1.0 and r["contaminants"]:
                cont = "  contam=" + ",".join(f"{t}:{c}" for t, c in r["contaminants"])
            print(f"{r['tu']:52s} {r['dom']:4d} {r['density']:6.1f} {r['purity']*100:5.0f} "
                  f"{r['hiconf']:4d}  {r['span_start']}-{r['span_end']}{cont}")

    show("BUCKET A -- WIRED + PINNABLE (immediate pin wave; feed pairing stream)",
         "A_wired_pinnable")
    show("BUCKET B -- PRESENT but UNWIRED + PINNABLE (add objects.json entry + pin)",
         "B_present_unwired_pinnable")
    show("BUCKET C -- PORTABLE from ../rb3 + PINNABLE (porting-priority queue)",
         "C_portable_pinnable", extra="  [ranked by recoverable mass]")

    # D summary only (large)
    d = buckets.get("D_scatter", [])
    d_fns = sum(r["dom"] for r in d)
    print(f"\n{'='*92}\nBUCKET D -- located but NOT pinnable (scatter)  --  {len(d)} TUs, {d_fns} dom fns\n{'='*92}")
    print("(low density/purity; the oracle's address-sequence propagation lost the tight run.")
    print(" These are identified but need better localization before pinning. Top 12 by mass:)")
    for r in sorted(d, key=lambda r: -r["matched"])[:12]:
        print(f"  {r['tu']:52s} matched={r['matched']:3d} dom={r['dom']:3d} "
              f"dens={r['density']:5.1f}% pur={r['purity']*100:3.0f}% clusters={r['n_clusters']}")

    # headline
    print(f"\n{'#'*92}")
    A = buckets["A_wired_pinnable"]; B = buckets["B_present_unwired_pinnable"]; C = buckets["C_portable_pinnable"]
    print(f"# A (wired+pinnable):   {len(A):3d} TUs / {sum(r['dom'] for r in A):4d} fns -- pin NOW")
    print(f"# B (present+pinnable): {len(B):3d} TUs / {sum(r['dom'] for r in B):4d} fns -- wire+pin")
    print(f"# C (portable+pinnable):{len(C):3d} TUs / {sum(r['dom'] for r in C):4d} fns -- port+pin queue")
    print(f"# D (scatter):          {len(d):3d} TUs / {d_fns:4d} fns -- needs better localization")
    print(f"{'#'*92}")


if __name__ == "__main__":
    main()
