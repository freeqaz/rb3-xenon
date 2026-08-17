#!/usr/bin/env python3
"""Price a map-name repair from report.json's CHARGED SITES, not from the patch.

WHY (W9/W14/W15, and two lanes mispriced on one day): a wrong map name is not a
free +N. It is a local +N financed by a charge on EVERY caller that relocates
against it, so repairing one is frequently net POSITIVE. `matched_code` keys on
`fuzzy == 100` and is ALL-OR-NOTHING per row, so a row at 99.9 earns ZERO bytes
and costs nothing to move.

⚠ `report.json` is protobuf-JSON: defaults are OMITTED and several numerics are
JSON STRINGS. Every read here is `int(x.get(k, 0))` / `float(x.get(k, 0.0))`.

Usage:
    python3 tools/rbtree_price.py <symbol-substring> [more...]
    python3 tools/rbtree_price.py --addr 0x824730e8 ...   (via target_symbol_map)
"""
import json
import os
import sys

ROOT = os.environ.get("RB3_ROOT", ".")
REPORT = os.path.join(ROOT, "build/45410914/report.json")
MAP = os.path.join(ROOT, "scripts/target_symbol_map.json")


def load():
    d = json.load(open(REPORT))
    out = []
    for unit in d.get("units", []):
        uname = unit.get("name", "?")
        for fn in unit.get("functions", []):
            out.append((uname, fn))
    return d, out


def row(fn):
    return dict(name=fn.get("name", ""),
                size=int(fn.get("size", 0) or 0),
                fuzzy=float(fn.get("fuzzy_match_percent", 0.0) or 0.0),
                mpn=float(fn.get("match_percent_normalized", 0.0) or 0.0))


def main():
    d, rows = load()
    args = [a for a in sys.argv[1:] if a != "--addr"]
    if "--addr" in sys.argv:
        smap = {k.lower(): v for k, v in json.load(open(MAP)).items()}
        args = [smap.get(a.lower(), a) for a in args]
    m = d["measures"]
    print(f"baseline matched_functions={m['matched_functions']} "
          f"matched_code={m['matched_code']} "
          f"code%={m['matched_code_percent']}\n")
    for pat in args:
        hits = [(u, row(f)) for u, f in rows if pat in f.get("name", "")]
        if not hits:
            print(f"== {pat[:90]}\n   NO ROW IN report.json (unpaired / absent)")
            continue
        print(f"== {pat[:100]}")
        for u, r in sorted(hits, key=lambda x: -x[1]["size"]):
            earn = "EARNS" if r["fuzzy"] >= 100.0 else "earns 0"
            print(f"   {u:44s} {r['size']:6d} B  fuzzy {r['fuzzy']:10.5f}  "
                  f"mpn {r['mpn']:10.5f}  {earn}")
        print()


if __name__ == "__main__":
    main()
