#!/usr/bin/env python3
"""Reverse homing (VA -> exact COFF symbol name).  The adjudication primitive.

A handed-off {VA: mangled_name} proposal must NEVER be trusted as authored: a
measured calibration over 4 prior handoffs found 3 payable but EVERY ONE needed
its mangled name corrected first (one was handed off `@@UAAXXZ` where we emit
`@@M...`).  This tool removes the authoring step entirely: given retail VAs, find
reloc-masked byte-identical body, and report the EXACT COFF symbol name(s).

Usage: rev_home.py <worktree> <va> [<va> ...]
"""
import os, struct, sys, json, glob
from pathlib import Path
from collections import defaultdict

ROOT = "/home/free/code/milohax/rb3-xenon"
BAND = f"{ROOT}/orig/45410914/band.exe"

sys.path.insert(0, f"{ROOT}/scripts/harvest")
from homing_scan import parse_obj, extract_funcs, parse_band, band_bytes

def main():
    wt = sys.argv[1]
    vas = [int(a, 16) for a in sys.argv[2:]]
    data, secs, ents = parse_band()
    size_of = {}
    for b, fl in ents:
        size_of.setdefault(b, fl)

    targets = {}
    for va in vas:
        sz = size_of.get(va)
        targets[va] = sz
        print(f"0x{va:08x}: pdata size = {sz}")

    # For each VA, retail body
    tbodies = {}
    for va, sz in targets.items():
        if sz:
            tbodies[va] = band_bytes(data, secs, va, sz)

    # scan every obj
    hits = defaultdict(list)   # va -> [(unit, name)]
    objs = glob.glob(os.path.join(wt, 'build', '45410914', 'src', '**', '*.obj'), recursive=True)
    print(f"scanning {len(objs)} objs...", file=sys.stderr)
    root = os.path.join(wt, 'build', '45410914', 'src')
    for p in objs:
        unit = os.path.relpath(p, root)[:-4]
        try:
            funcs = extract_funcs(p)
        except Exception as e:
            continue
        for name, (body, offs) in funcs.items():
            L = len(body)
            for va, sz in targets.items():
                if sz != L:
                    continue
                bb = tbodies.get(va)
                if bb is None or len(bb) != L:
                    continue
                m = bytearray(bb)
                for off in offs:
                    for b in range(4):
                        if off+b < len(m):
                            m[off+b] = 0
                if bytes(m) == body:
                    hits[va].append((unit, name, len(offs)))
    out = {}
    for va in vas:
        h = hits.get(va, [])
        names = sorted({n for _, n, _ in h})
        print(f"\n=== 0x{va:08x} (size {targets.get(va)}) : {len(h)} obj-hits, {len(names)} distinct names")
        for unit, name, nr in sorted(h):
            print(f"    {unit:60s} rel={nr:2d} {name}")
        out[f"0x{va:08x}"] = dict(size=targets.get(va), hits=[dict(unit=u, name=n, nreloc=r) for u,n,r in sorted(h)], distinct_names=names)
    json.dump(out, open(f"/home/free/tmp/rev_home_out.json", "w"), indent=1)

if __name__ == '__main__':
    main()
