#!/usr/bin/env python3
"""Oracle-quality pre-screen for identity-transfer harvest target selection.

B2 warm-up (2026-06-21) found the dominant wall is NOT the source port — it is
ORACLE VA MISATTRIBUTION: the rb3-Wii→retail BinDiff oracle (unified_id_rb3wii.json)
maps a TU's methods onto retail VAs that hold UNRELATED functions. Symptom that is
detectable PRE-PORT: the retail function at the oracle VA is 5–25× the oracle's Wii
size, and/or that VA already owns a foreign mangled name. RockCentral (+17) was a
good-oracle exception; ChordPreview/Scoring are misattributed.

This tool scores each TU's oracle rows so a harvest wave only ports GOOD-oracle TUs
(RockCentral-shaped), instead of burning multi-hour ports on misattributed ones.

A method is GOOD-oracle if: real-bodied (retail >44B) AND retail/wii size ratio in
the two-compiler band [LO,HI] AND the VA is not already owned by a foreign name.
TU score = good / real-bodied. Predicted yield ≈ good count.
"""
import json, re, sys, argparse
from pathlib import Path
from collections import defaultdict

ROOT = Path(__file__).resolve().parent.parent
ORACLE = ROOT / "unified_id_rb3wii.json"
SYMS = ROOT / "config/45410914/symbols.txt"
TMAP = ROOT / "scripts/target_symbol_map.json"

def load_retail_sizes():
    sz = {}
    rx = re.compile(r"^\S+ = \.text:0x([0-9A-Fa-f]+);.*?\btype:function\b.*?size:0x([0-9A-Fa-f]+)")
    for l in SYMS.read_text().splitlines():
        m = rx.search(l)
        if m: sz[int(m.group(1), 16)] = int(m.group(2), 16)
    return sz

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--lo", type=float, default=0.30, help="min retail/wii size ratio (two-compiler band)")
    ap.add_argument("--hi", type=float, default=3.5, help="max retail/wii size ratio")
    ap.add_argument("--tu", help="only this TU (basename, e.g. RockCentral.cpp)")
    ap.add_argument("--min-good", type=int, default=3, help="min good methods to list a TU")
    ap.add_argument("--top", type=int, default=40)
    args = ap.parse_args()

    oracle = json.load(open(ORACLE))
    sizes = load_retail_sizes()
    tmap = json.load(open(TMAP))

    by_tu = defaultdict(list)
    for r in oracle:
        by_tu[Path(r.get("bindiff_src", "?")).name].append(r)

    rows = []
    detail = {}
    for tu, recs in by_tu.items():
        cls = tu[:-4] if tu.endswith(".cpp") else tu  # crude class hint
        real = good = mis_size = foreign = stub = 0
        good_methods = []
        for r in recs:
            va = int(str(r["rb3_addr"]), 16)
            wii = r.get("size") or 0
            ret = sizes.get(va, 0)
            if ret <= 0x2C:
                stub += 1; continue
            real += 1
            ratio = ret / wii if wii else 999
            fname = tmap.get(hex(va)) or tmap.get(f"0x{va:08X}")
            is_foreign = bool(fname) and cls not in (fname or "")
            in_band = args.lo <= ratio <= args.hi
            if is_foreign:
                foreign += 1
            elif not in_band:
                mis_size += 1
            else:
                good += 1
                good_methods.append((hex(va), r.get("wii_name", "?"), wii, ret, round(ratio, 2)))
        if real:
            rows.append((tu, real, good, mis_size, foreign, stub, good / real))
            detail[tu] = good_methods

    if args.tu:
        recs = [x for x in rows if x[0] == args.tu]
        if not recs: print(f"no oracle rows for {args.tu}"); return
        tu, real, good, ms, fo, st, q = recs[0]
        print(f"{tu}: real={real} GOOD={good} mis-size={ms} foreign={fo} stub={st} quality={q:.0%}")
        for va, nm, w, rt, ra in detail[tu]:
            print(f"   GOOD {va} {nm[:54]:<54} wii={w} retail={rt} ratio={ra}")
        return

    rows.sort(key=lambda x: -x[2])  # by good count
    print(f"oracle-quality sweep (band {args.lo}-{args.hi}x); {len(rows)} TUs with real-bodied oracle rows")
    print(f"{'TU':<34}{'real':>5}{'GOOD':>5}{'misSz':>6}{'frgn':>5}{'qual':>6}")
    print("-" * 62)
    tot_good = 0
    for tu, real, good, ms, fo, st, q in rows:
        tot_good += good
        if good >= args.min_good and rows.index((tu, real, good, ms, fo, st, q)) < args.top:
            print(f"{tu:<34}{real:>5}{good:>5}{ms:>6}{fo:>5}{q:>5.0%}")
    print("-" * 62)
    print(f"TOTAL GOOD-oracle real-bodied methods across {len(rows)} TUs: {tot_good}")
    print(f"TUs with >={args.min_good} good methods: {sum(1 for r in rows if r[2] >= args.min_good)}")

if __name__ == "__main__":
    main()
