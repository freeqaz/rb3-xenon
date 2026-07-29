#!/usr/bin/env python3
"""retail_handler_strings.py -- list, in order, the handler-name string constants
that a RETAIL function body references.

Companion to handler_drift_scan.py.  Reads the dtk-split target listing
(build/45410914/asm/<Unit>.s) for the function, collects every `lbl_82XXXXXX`
operand, and resolves it against the flat retail image (orig/45410914/band.exe).
Any label that resolves to a printable C string is printed in reference order --
for a Milo `BEGIN_HANDLERS` body those strings ARE the retail handler list.

Usage:
  python3 scripts/harvest/retail_handler_strings.py Mesh fn_82420498
  python3 scripts/harvest/retail_handler_strings.py Mesh '?Handle@RndMesh@@UAA?AVDataNode@@PAVDataArray@@_N@Z'
"""
import json
import os
import re
import struct
import sys

PROJ = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))


def load_image(path):
    d = open(path, "rb").read()
    pe = struct.unpack_from("<I", d, 0x3C)[0]
    nsec = struct.unpack_from("<H", d, pe + 6)[0]
    osz = struct.unpack_from("<H", d, pe + 20)[0]
    ib = struct.unpack_from("<I", d, pe + 24 + 28)[0]
    secs = []
    for i in range(nsec):
        o = pe + 24 + osz + i * 40
        vs, va, rs, rp = struct.unpack_from("<IIII", d, o + 8)
        secs.append((ib + va, vs, rp, rs))
    return d, secs


def read_at(d, secs, va, n=96):
    for base, vs, rp, rs in secs:
        if base <= va < base + vs:
            off = rp + (va - base)
            if rp == 0:
                return None
            return d[off : off + n]
    return None


def cstr(d, secs, va):
    b = read_at(d, secs, va)
    if not b:
        return None
    e = b.find(b"\0")
    if e <= 0:
        return None
    s = b[:e]
    if not all(32 <= c < 127 for c in s):
        return None
    return s.decode()


def main():
    unit, fn = sys.argv[1], sys.argv[2]
    if not fn.startswith("fn_"):
        m = json.load(open(os.path.join(PROJ, "scripts/target_symbol_map.json")))
        rev = {}
        for k, v in m.items():
            for x in v if isinstance(v, list) else [v]:
                rev.setdefault(x, k)
        addr = rev.get(fn)
        if not addr:
            print("no map entry for", fn)
            return 1
        fn = "fn_" + addr[2:].upper()
    sp = os.path.join(PROJ, "build/45410914/asm/%s.s" % unit)
    d, secs = load_image(os.path.join(PROJ, "orig/45410914/band.exe"))

    body, on = [], False
    for line in open(sp, errors="replace"):
        if re.match(r"\.fn %s," % re.escape(fn), line):
            on = True
            continue
        if on and line.startswith(".endfn"):
            break
        if on:
            body.append(line.rstrip())

    if not body:
        print("function %s not found in %s" % (fn, sp))
        return 1

    frame = None
    seen, order = set(), []
    for line in body:
        m = re.search(r"stwu r1, -(0x[0-9a-f]+)\(r1\)", line)
        if m and frame is None:
            frame = m.group(1)
        for lbl in re.findall(r"lbl_([0-9A-F]{8})", line):
            va = int(lbl, 16)
            if va in seen:
                continue
            seen.add(va)
            s = cstr(d, secs, va)
            if s:
                order.append((va, s))
    print("%s  %s  frame=%s  strings=%d" % (unit, fn, frame, len(order)))
    for va, s in order:
        print("  %08X  %s" % (va, s))
    return 0


if __name__ == "__main__":
    sys.exit(main())
