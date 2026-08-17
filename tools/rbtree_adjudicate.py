#!/usr/bin/env python3
"""Gate a proposed _Rb_tree map-name repair BEFORE it is shipped.

Three gates, each of which has cost a lane real bytes when skipped:

 1. PRICE from report.json, not from the patch. `matched_code` keys on
    `fuzzy == 100` and is ALL-OR-NOTHING per row, so a row at 99.9 earns ZERO
    and is free to move, while a row at 100 is live credit a wrong assignment
    destroys.
 2. COLLISION: the map is asserted injective on NAME. The correct post-check is
    a DELTA against the pre-edit collision set, never purity -- the map has
    pre-existing duplicates and a purity assert fires on them (W15).
 3. OBJ-CAN-DEFINE: the pinned unit's base obj must be able to define the
    replacement name, or the row goes PERMANENTLY 0% (W9: -180 B / -3 fns). If
    it cannot, the right repair is a RE-HOME (move the pin), not a rename.

Usage:
    python3 tools/rbtree_adjudicate.py 0x824f9288=<newname> [0xADDR=<name>...]
    python3 tools/rbtree_adjudicate.py --where 0x824f9288 ...   # pin + obj only
"""
import glob
import json
import os
import re
import sys

ROOT = os.environ.get("RB3_ROOT", ".")
MAP = os.path.join(ROOT, "scripts/target_symbol_map.json")
SPLITS = os.path.join(ROOT, "config/45410914/splits.txt")
REPORT = os.path.join(ROOT, "build/45410914/report.json")
OBJDIR = os.path.join(ROOT, "build/45410914")


def splits_owner(addr):
    """Which splits.txt heading's .text block contains this address.

    ⚠ Headings are BARE for some units and NESTED for others (707/569); key on
    the FULL heading text, never basename()."""
    cur, hits = None, []
    rx = re.compile(r"\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")
    for line in open(SPLITS):
        if line and not line[0].isspace() and line.rstrip().endswith(":"):
            cur = line.strip()[:-1]
            continue
        m = rx.search(line)
        if m and cur:
            lo, hi = int(m.group(1), 16), int(m.group(2), 16)
            if lo <= addr < hi:
                hits.append((cur, lo, hi))
    return hits


_OBJ_CACHE = {}


def obj_defines(unit, name):
    """Does the compiled obj for this unit define this symbol?"""
    path = os.path.join(OBJDIR, "src", unit + ".obj")
    if not os.path.exists(path):
        alt = glob.glob(os.path.join(OBJDIR, "src", "**",
                                     os.path.basename(unit) + ".obj"),
                        recursive=True)
        if not alt:
            return None, f"no obj for {unit}"
        path = alt[0]
    if path not in _OBJ_CACHE:
        sys.path.insert(0, os.path.join(ROOT, "tools"))
        from coff_bodies_ext import function_bodies_ext
        try:
            _OBJ_CACHE[path] = {n for n, _b, _r, _e in function_bodies_ext(path)}
        except Exception as e:                     # noqa: BLE001
            _OBJ_CACHE[path] = set()
    return (name in _OBJ_CACHE[path]), path


def price(name):
    d = json.load(open(REPORT))
    for unit in d.get("units", []):
        for fn in unit.get("functions", []):
            if fn.get("name") == name:
                return (unit.get("name"), int(fn.get("size", 0) or 0),
                        float(fn.get("fuzzy_match_percent", 0.0) or 0.0),
                        float(fn.get("match_percent_normalized", 0.0) or 0.0))
    return None


def main():
    raw = json.load(open(MAP))
    smap = {k: v for k, v in raw.items() if k.startswith("0x")}
    byname = {}
    for k, v in smap.items():
        byname.setdefault(v, []).append(k)
    predup = {n for n, ks in byname.items() if len(ks) > 1}
    print(f"map rows {len(smap)}   pre-existing duplicate names {len(predup)}\n")

    if "--where" in sys.argv:
        for a in sys.argv[1:]:
            if a == "--where":
                continue
            ad = int(a, 16)
            cur = smap.get(a.lower()) or smap.get(f"0x{ad:08x}")
            print(f"{a}  current: {(cur or '<unmapped>')[:100]}")
            for h, lo, hi in splits_owner(ad):
                print(f"    pinned in {h}  [{lo:#x},{hi:#x})")
            if cur:
                p = price(cur)
                print(f"    price: {p}")
            print()
        return

    props = []
    for a in sys.argv[1:]:
        addr, _, new = a.partition("=")
        props.append((addr.lower(), new))

    for addr, new in props:
        ad = int(addr, 16)
        cur = smap.get(addr) or smap.get(f"0x{ad:08x}")
        print(f"== {addr}")
        print(f"   from {(cur or '<unmapped>')[:110]}")
        print(f"   to   {new[:110]}")
        p = price(cur) if cur else None
        if p:
            print(f"   PRICE unit={p[0]} size={p[1]} fuzzy={p[2]:.5f} mpn={p[3]:.5f}"
                  f"  -> {'LIVE CREDIT AT RISK' if p[2] >= 100 else 'earns 0, free to move'}")
        else:
            print("   PRICE: no report row")
        owners = splits_owner(ad)
        print(f"   PIN {owners}")
        if new in byname:
            print(f"   ⛔ COLLISION: name already mapped at {byname[new]}")
        else:
            print("   collision: free")
        for h, _lo, _hi in owners:
            unit = h[:-4] if h.endswith(".cpp") else h
            ok, path = obj_defines(unit, new)
            print(f"   OBJ {unit}: defines replacement = {ok}   ({path})")
        print()


if __name__ == "__main__":
    main()
