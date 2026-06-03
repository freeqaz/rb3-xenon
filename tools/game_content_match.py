#!/usr/bin/env python3
"""Cross-binary COFF content-matcher for GAME code: identify RB3 functions by
the name of OUR COMPILED GAME BASE obj's defining section symbol.

Why
---
Sibling of tools/dc3_content_match.py. Same masked-hash trick, but the NAMED
("oracle") side is swapped from the DC3 byte-twin to OUR OWN compiled game base
objects (the recompiled rb3-Wii -> MSVC source under
build/45410914/src/{band3,network}/**/*.obj). Where our ported game source is
byte-faithful to the retail RB3 source, the instruction stream is identical
modulo relocation operands; mask the reloc operands (the COFF reloc table tells
us exactly where) and hash the rest. A base-obj function and an RB3 target
function with the same masked hash, where the hash is unique on BOTH sides, are
the SAME function -> we know that RB3 fn_<addr> IS that mangled game symbol.

This finds game functions that already compile byte-identically but sit OUTSIDE
the unit's current .text pin (mis-anchored game splits, same failure mode the
engine stubs had). Feed the output to a relocate-splits pass to actually land
them.

Inputs
------
RB3 .text:  build/45410914/obj/auto_03_*_text.obj  (whole .text, per-fn COMDAT
            sections, already renamed where known). RB3 addr recovered from the
            fn_<addr> symbol, or reverse target_symbol_map for already-renamed.
            IDENTICAL to dc3_content_match.py.
Game base:  build/45410914/src/{band3,network}/**/*.obj  (our compiled source,
            MSVC-mangled defining section symbols). unit = relpath against
            build/45410914/src, with .obj -> .cpp.

Output
------
game_content_match.json: list of {rb3_addr (UPPERCASE "0x%08X"), mangled_name,
unit (relpath like band3/game/SongDB.cpp), size, masked_sha}. Only 1:1
unambiguous masked-hash matches are emitted (a hash unique on BOTH sides), which
is the high-confidence set.
"""
import argparse
import glob
import hashlib
import json
import os
import re
import sys
from collections import defaultdict

# reuse the COFF parser / masking VERBATIM
from dc3_content_match import read_coff_functions, rb3_addr_of  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
RB3_OBJ_GLOB = os.path.join(ROOT, "build", "45410914", "obj", "auto_03_*_text.obj")
GAME_SRC_DIR = os.path.join(ROOT, "build", "45410914", "src")
TSM = os.path.join(ROOT, "scripts", "target_symbol_map.json")


def game_unit_of(path):
    """relpath of a game base .obj against build/45410914/src, .obj -> .cpp."""
    rel = os.path.relpath(path, GAME_SRC_DIR)
    if rel.endswith(".obj"):
        rel = rel[:-4] + ".cpp"
    return rel.replace(os.sep, "/")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--rb3-glob", default=RB3_OBJ_GLOB)
    ap.add_argument("--game-src-dir", default=GAME_SRC_DIR)
    ap.add_argument("--tsm", default=TSM)
    ap.add_argument("--out", default=os.path.join(ROOT, "game_content_match.json"))
    ap.add_argument("--min-size", type=int, default=16,
                    help="ignore tiny functions (<N bytes); collide too easily. "
                         "16 is the sweet spot (matches dc3_content_match.py).")
    args = ap.parse_args()

    tsm = json.load(open(args.tsm))
    rev_tsm = {}
    for k, v in tsm.items():
        if k.lower().startswith("0x"):
            rev_tsm[v] = int(k, 16)

    # RB3 (target) side: masked_hash -> {addr: size}  (dedup by address)
    # IDENTICAL to dc3_content_match.py
    rb3_by_hash = defaultdict(dict)
    rb3_files = sorted(glob.glob(args.rb3_glob))
    for f in rb3_files:
        for name, code, h, sz in read_coff_functions(f):
            if sz < args.min_size:
                continue
            a = rb3_addr_of(name, rev_tsm)
            if a is None:
                continue
            rb3_by_hash[h][a] = sz
    print(f"RB3: {len(rb3_files)} objs, {sum(len(v) for v in rb3_by_hash.values())} "
          f"fns >= {args.min_size}B, {len(rb3_by_hash)} distinct masked hashes",
          file=sys.stderr)

    # GAME base side: masked_hash -> {mangled_name: (unit, size)}  (dedup by name)
    game_by_hash = defaultdict(dict)
    game_files = sorted(glob.glob(os.path.join(args.game_src_dir, "band3", "**", "*.obj"),
                                  recursive=True))
    game_files += sorted(glob.glob(os.path.join(args.game_src_dir, "network", "**", "*.obj"),
                                   recursive=True))
    per_unit_fns = defaultdict(int)
    for f in game_files:
        unit = game_unit_of(f)
        for name, code, h, sz in read_coff_functions(f):
            if sz < args.min_size:
                continue
            if name.startswith("fn_") or name.startswith("sub_") or name.startswith("FUN_"):
                continue
            game_by_hash[h][name] = (unit, sz)
            per_unit_fns[unit] += 1
    print(f"GAME: {len(game_files)} base objs, "
          f"{sum(len(v) for v in game_by_hash.values())} named fns >= {args.min_size}B, "
          f"{len(game_by_hash)} distinct masked hashes", file=sys.stderr)

    # 1:1 unambiguous matches (hash maps to exactly one addr AND one name)
    matches = []
    ambiguous = 0
    for h, rb3map in rb3_by_hash.items():
        gamemap = game_by_hash.get(h)
        if not gamemap:
            continue
        if len(rb3map) == 1 and len(gamemap) == 1:
            addr, sz = next(iter(rb3map.items()))
            name, (unit, gsz) = next(iter(gamemap.items()))
            matches.append({
                "rb3_addr": "0x%08X" % addr,
                "mangled_name": name,
                "unit": unit,
                "size": sz,
                "masked_sha": h,
            })
        else:
            ambiguous += 1
    matches.sort(key=lambda m: m["rb3_addr"])
    json.dump(matches, open(args.out, "w"), indent=1)
    print(f"\n1:1 content matches: {len(matches)}   (ambiguous hashes: {ambiguous})")
    print(f"wrote {args.out}")

    # per-unit coverage of MATCHED fns
    byunit = defaultdict(int)
    for m in matches:
        byunit[m["unit"]] += 1
    print("\nper-unit matched fns (sorted):")
    for u, n in sorted(byunit.items(), key=lambda x: -x[1]):
        print(f"  {u:50s} {n:4d}  (of {per_unit_fns[u]} named >= {args.min_size}B)")


if __name__ == "__main__":
    main()
