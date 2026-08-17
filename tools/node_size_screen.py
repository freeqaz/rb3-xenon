#!/usr/bin/env python3
"""Screen every mapped _Rb_tree::_M_insert against the ALLOCATION SIZE of the
_M_create_node it actually calls.

WHY THIS IS DECISIVE AND MAP-INDEPENDENT
────────────────────────────────────────
An _Rb_tree node is `_Rb_tree_node_base` (16 B) + value_type. _M_create_node
therefore opens with `li r3, 16 + sizeof(value_type)` before its allocator
call. That immediate is a fact about the retail body, not about the map.

_M_insert can only call ITS OWN tree's _M_create_node, so:

    16 + sizeof(value_type spelled in _M_insert's mangled name)
        MUST EQUAL
    the `li r3,N` in the body _M_insert actually branches to

A mismatch is a WRONG MAP NAME and cannot be explained by ICF: different-size
COMDATs cannot fold, so this is not the "arbitrary survivor name" class.

Found by lane W9-FALSECREDIT this way:
  * 0x825948c0 named map<int,float>::_M_insert (needs 0x18) calls a 0x14
    builder => it is a 4-byte-value SET. Corrected to set<ScoreType>. +268 B.
  * 0x82456190 named set<FaderGroup*>::_M_insert (needs 0x14) calls a 0x20
    builder => 16-byte value_type. UNADJUDICATED.

Lane W12-MAPLEADS settled the other two W9 leftovers (0x822dda78 => set<G>,
0x822deed0 => map<G,G>, both re-homed to ChordShapeGenerator.cpp, +632 B) and
records two KNOWN LIMITS of this screen, so a clean run is not a clearance:

  * IT ONLY SCREENS `_M_insert`. The defect is usually a whole FAMILY
    (insert_unique overloads + _M_insert) mis-named consistently, and a
    consistent family is INVISIBLE to the metric -- only the one edge to a
    correctly-named _M_create_node is charged. Correcting the _M_insert
    ALONE therefore breaks family consistency and CHARGES its callers
    (measured: -696 B if the two insert_unique rows are left behind).
    Always enumerate the callers before pricing a fix.
  * ITS MAP RULE ONLY FIRES FOR value_type < 8. It misses same-shape
    defects with a large value_type -- e.g. 0x824730e8 is named
    map<G, RndFont3d::CharInfo*> (pair<const G,ptr> = 8 => node 0x18) yet
    branches to a 0x24 builder (value_type 20 B). Flagged, NOT adjudicated.

Usage: python3 node_size_screen.py     (from the repo root)
"""
import json
import os
import re
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from retail_body import Img  # noqa: E402  (tools/retail_body.py)

ROOT = os.environ.get("RB3_ROOT", ".")
MAP = os.path.join(ROOT, "scripts/target_symbol_map.json")

# value_type sizes we can infer from the mangled first template args
FOUR_BYTE_KEYS = ("H", "I", "J", "K", "M", "PA", "PB", "W4", "VSymbol@@")


def words(img, va, n):
    b = img.read(va, n * 4)
    return [struct.unpack_from(">I", b, i * 4)[0] for i in range(len(b) // 4)]


def first_bl(img, va, limit=40):
    """First bl in the body that is NOT the prologue-save helper."""
    out = []
    for i, w in enumerate(words(img, va, limit)):
        if (w >> 26) == 18 and (w & 1):
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            out.append(va + i * 4 + li)
    return out


def alloc_size(img, va, limit=24):
    """The `li r3, N` immediate near the top of a create_node body."""
    for w in words(img, va, limit):
        if (w >> 26) == 14 and ((w >> 21) & 31) == 3 and ((w >> 16) & 31) == 0:
            imm = w & 0xFFFF
            return imm - 0x10000 if imm & 0x8000 else imm
    return None


SCALARS = {"C": 1, "D": 1, "E": 1, "F": 2, "G": 2,
           "H": 4, "I": 4, "J": 4, "K": 4, "M": 4, "N": 8, "O": 8}


def _tok(s, i):
    """Parse ONE mangled type token at s[i:] -> (size, align, next_i) or None."""
    if i >= len(s):
        return None
    if s.startswith("_N", i):
        return (1, 1, i + 2)
    if s.startswith("W4", i) or (s[i] in "PQ" and i + 1 < len(s) and s[i + 1] in "AB"):
        j = s.find("@@", i)
        return (4, 4, j + 2) if j >= 0 else None
    if s.startswith("VSymbol@@", i):
        return (4, 4, i + 9)
    if s[i] in SCALARS:
        return (SCALARS[s[i]], SCALARS[s[i]], i + 1)
    return None


def pair_size(name):
    """sizeof(pair<const A,B>) from the mangled `U?$pair@$$CB<A><B>@`, or None
    when either half is a class we cannot size. Alignment-aware, so
    pair<const G,G> correctly comes out 4 -- the case that made the old
    `vt >= 8 for any map` rule wrong."""
    k = name.find("U?$pair@$$CB")
    if k < 0:
        return None
    a = _tok(name, k + len("U?$pair@$$CB"))
    if not a:
        return None
    b = _tok(name, a[2])
    if not b:
        return None
    off = (a[0] + b[1] - 1) // b[1] * b[1]
    al = max(a[1], b[1])
    return (off + b[0] + al - 1) // al * al


def main():
    img = Img()
    smap = {int(k, 16): v for k, v in json.load(open(MAP)).items()
            if v and k.startswith("0x")}
    rx = re.compile(r'^\?_M_insert@\?\$_Rb_tree@')
    rows = [(a, n) for a, n in smap.items() if rx.match(n)]
    print(f"mapped _M_insert rows: {len(rows)}\n")
    for a, n in sorted(rows):
        key = n.split("_Rb_tree@")[1].split("U?$less@")[0]
        is_set = "_Identity@" in n
        # find the create_node it calls: a callee whose body starts li r3,N
        sizes = {}
        for t in first_bl(img, a):
            s = alloc_size(img, t)
            if s and 16 < s <= 0x200:
                sizes[t] = s
        if not sizes:
            print(f"0x{a:08x}  {'SET' if is_set else 'MAP'} <{key[:34]:34s}>  no create_node found")
            continue
        for t, s in sizes.items():
            vt = s - 16
            flag = ""
            declared = None if is_set else pair_size(n)
            if declared is not None:
                # EXACT test: we can size the declared pair<const K,V>, so any
                # disagreement with the builder's own `li r3,N` is decisive and
                # there is no heuristic left to argue about.
                if declared != vt:
                    flag = (f"  <== MISMATCH: name declares pair = {declared} B "
                            f"(node {declared + 16:#x}) but builder allocates "
                            f"{s:#x} (value_type {vt} B)")
            elif is_set and any(key.startswith(p) for p in FOUR_BYTE_KEYS) and vt != 4:
                flag = (f"  <== MISMATCH: 4-byte SET value but builder allocates "
                        f"{s:#x} (value_type {vt} B)")
            elif not is_set and vt < 8:
                # ⚠ CORRECTED by lane W12-MAPLEADS. This branch USED to assert
                # "a map's value_type is pair<const K,V>, which is >= 8 B always,
                # so this is a SET". THAT IS FALSE: pair<const unsigned short,
                # unsigned short> is 4 B, so map<G,G> has a 0x14 node. The
                # measured case 0x822deed0 was flagged "=> this is a SET" and is
                # in fact map<G,G>::_M_insert -- the FLAG was right (the name was
                # wrong) but the DIAGNOSIS was wrong. A small node is consistent
                # with BOTH a 4-byte set and a map of two 2-byte types; resolve
                # which by disassembling the builder (a set copies the value
                # ONCE, a map copies each pair half -- e.g. 0x822dd240 does one
                # `lhz 0/sth 0`, 0x822ddb48 does `lhz 0/sth 0` AND `lhz 2/sth 2`).
                flag = (f"  <== MISMATCH: builder allocates {s:#x} => value_type "
                        f"{vt} B, too small for this MAP's pair<K,V>; consistent "
                        f"with set<4B> OR map<2B,2B> -- disassemble the builder")
            print(f"0x{a:08x}  {'SET' if is_set else 'MAP'} <{key[:34]:34s}>  "
                  f"-> builder 0x{t:08x} li r3,{s:#x} (value_type {vt} B){flag}")


if __name__ == "__main__":
    main()
