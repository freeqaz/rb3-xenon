#!/usr/bin/env python3
"""obj_col.py -- read _RTTICompleteObjectLocator offset/cdOffset fields out of
OUR compiled COFF .obj, by locating the `??_R4...` symbols in the symbol table.

This is the POSITIVE CONTROL for vbase_rtti.py: the compiler
(class_layout_report.py) already tells us the true subobject offsets on our
side, so if this parser reproduces them, the identical field interpretation can
be trusted when applied to retail band.exe.

In a COFF the COL's pointer fields are relocations (zero in raw data), but
`offset` (+4) and `cdOffset` (+8) are plain big-endian integers -- exactly the
two fields we need.
"""
from __future__ import annotations
import struct
import sys


def read_obj(path):
    b = open(path, "rb").read()
    machine, nsec, _ts, symptr, nsym = struct.unpack_from("<HHIII", b, 0)
    secs = []
    for i in range(nsec):
        o = 20 + i * 40
        name = b[o:o + 8].rstrip(b"\x00").decode("ascii", "replace")
        vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", b, o + 8)
        secs.append((name, rawptr, rawsize))
    strtab_off = symptr + nsym * 18
    syms = []
    i = 0
    while i < nsym:
        o = symptr + i * 18
        raw = b[o:o + 8]
        if raw[:4] == b"\x00\x00\x00\x00":
            off = struct.unpack_from("<I", raw, 4)[0]
            end = b.find(b"\x00", strtab_off + off)
            name = b[strtab_off + off:end].decode("ascii", "replace")
        else:
            name = raw.rstrip(b"\x00").decode("ascii", "replace")
        value, secnum, _typ, _cls, naux = struct.unpack_from("<IhHBB", b, o + 8)
        syms.append((name, value, secnum))
        i += 1 + naux
    return b, secs, syms


def main(path):
    b, secs, syms = read_obj(path)
    print(f"# {path}")
    n_r4 = 0
    for name, value, secnum in syms:
        if not name.startswith("??_R4"):
            continue
        n_r4 += 1
        if secnum <= 0 or secnum > len(secs):
            print(f"  {name}: (no section)")
            continue
        sname, rawptr, rawsize = secs[secnum - 1]
        base = rawptr + value
        sig, off, cd = struct.unpack_from(">III", b, base)
        print(f"  offset=0x{off:<5x} cdOffset=0x{cd:<3x} sig={sig}   {name}")
    print(f"# ??_R4 COL symbols found: {n_r4}  (DENOMINATOR for any 'not found')")


if __name__ == "__main__":
    for p in sys.argv[1:]:
        main(p)
