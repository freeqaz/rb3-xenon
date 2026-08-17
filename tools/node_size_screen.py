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
            if is_set and any(key.startswith(p) for p in FOUR_BYTE_KEYS) and vt != 4:
                flag = (f"  <== MISMATCH: 4-byte SET value but builder allocates "
                        f"{s:#x} (value_type {vt} B)")
            elif not is_set and vt < 8:
                # a map's value_type is pair<const K,V>, which is >= 8 B always,
                # so a 0x14 node is impossible for ANY map. This is the rule that
                # catches the 0x825948c0 class (a set survivor wearing a map name).
                flag = (f"  <== MISMATCH: MAP value_type cannot be {vt} B "
                        f"(pair<K,V> >= 8); builder allocates {s:#x} => this is a SET")
            print(f"0x{a:08x}  {'SET' if is_set else 'MAP'} <{key[:34]:34s}>  "
                  f"-> builder 0x{t:08x} li r3,{s:#x} (value_type {vt} B){flag}")


if __name__ == "__main__":
    main()
