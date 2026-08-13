#!/usr/bin/env python3
"""Identify an anonymous retail row by RELOCATION-NORMALIZED BODY IDENTITY.

Assigning a name to a retail address is the highest-risk operation in this
repo: an audit found 20.6% of one wave MISIDENTIFIED, and the metric cannot
catch it -- under `name_check` an alias is pure *forgiveness*, so an unproven
name lifts the score BY CONSTRUCTION.  "It scored better" is therefore not
evidence, and neither is same-size-so-probably.

This tool produces evidence that never consults the metric.  Both the dtk-split
target obj and our compiled obj store each function in its own COMDAT section,
so we can hash the instruction words directly.  Words overlapped by a
relocation are masked (the opcode field is kept, the relocated displacement or
immediate is zeroed) because the same function at two addresses has different
`bl` displacements -- a raw memcmp is silently vacuous for exactly this reason.

A hit is only reported when the mapping is BIJECTIVE: exactly one symbol on our
side and exactly one row on the retail side carry that body hash.  If several
of our functions share a body -- very common for tiny accessors, which
/OPT:ICF then folds into ONE retail survivor -- the tool reports the ambiguity
and assigns nothing.  Picking one of an ICF-folded group is a coin flip.

    python3 tools/body_match.py <target.obj> <ours.obj> [symbol ...]
    python3 tools/body_match.py <target.obj> <ours.obj> --all   # every bijection
"""

import collections
import hashlib
import struct
import sys


def parse(path):
    d = open(path, "rb").read()
    _m, nsec, _t, psym, nsym, _o, _c = struct.unpack_from("<HHIIIHH", d, 0)
    secs = {}
    for i in range(nsec):
        o = 20 + i * 40
        _vs, _va, size, ptr, prel, _pl, nrel, _nl, _ch = struct.unpack_from("<IIIIIIHHI", d, o + 8)
        relocs = set()
        for k in range(nrel):
            va_, _sym, _typ = struct.unpack_from("<IIH", d, prel + k * 10)
            relocs.add(va_)
        secs[i + 1] = {"data": d[ptr:ptr + size] if ptr else b"", "relocs": relocs}
    strtab = psym + nsym * 18
    fns, i = [], 0
    while i < nsym:
        o = psym + i * 18
        raw = d[o:o + 8]
        _val, sec, typ, sclass, naux = struct.unpack_from("<IhHBB", d, o + 8)
        if raw[:4] == b"\x00\x00\x00\x00":
            soff = struct.unpack_from("<I", raw, 4)[0]
            end = d.index(b"\x00", strtab + soff)
            name = d[strtab + soff:end].decode("latin1")
        else:
            name = raw.rstrip(b"\x00").decode("latin1")
        if sclass == 2 and sec > 0 and typ == 0x20:  # EXTERNAL, defined, function
            fns.append((name, sec))
        i += 1 + naux
    return secs, fns


def body_hash(sec):
    """sha1 over instruction words, with relocated fields masked out."""
    data, relocs = sec["data"], sec["relocs"]
    out = bytearray()
    for off in range(0, len(data) - 3, 4):
        word = struct.unpack_from(">I", data, off)[0]
        if any(off <= r < off + 4 for r in relocs):
            op = word >> 26
            # branch forms keep only the opcode; the rest keep the opcode+regs half
            word = (word & 0xFC000000) if op in (16, 18) else (word & 0xFFFF0000)
        out += struct.pack(">I", word)
    return hashlib.sha1(bytes(out)).hexdigest(), len(data)


def index(secs, fns):
    by_hash = collections.defaultdict(list)
    for name, sec in fns:
        by_hash[body_hash(secs[sec])].append(name)
    return by_hash


def main():
    target_path, base_path = sys.argv[1], sys.argv[2]
    wanted = sys.argv[3:]
    tsecs, tfns = parse(target_path)
    bsecs, bfns = parse(base_path)
    tgt = index(tsecs, [(n, s) for n, s in tfns if n.startswith(("fn_", "lbl_"))])
    ours = index(bsecs, bfns)
    by_name = {n: s for n, s in bfns}

    if wanted == ["--all"]:
        wanted = sorted(by_name)
    print("%-58s %6s  %s" % ("our symbol", "size", "verdict"))
    proven = 0
    for want in wanted:
        sec = by_name.get(want)
        if sec is None:
            print("%-58s %6s  NOT IN OUR OBJ" % (want[:58], "-"))
            continue
        key = body_hash(bsecs[sec])
        mine, theirs = ours[key], tgt.get(key, [])
        if len(theirs) == 1 and len(mine) == 1:
            proven += 1
            print("%-58s %6d  PROVEN -> %s" % (want[:58], key[1], theirs[0]))
        elif theirs:
            print(
                "%-58s %6d  AMBIGUOUS: %d ours x %d retail (ICF-folded?) -> %s"
                % (want[:58], key[1], len(mine), len(theirs), ",".join(theirs[:4]))
            )
        elif wanted != sorted(by_name):
            print("%-58s %6d  no body match" % (want[:58], key[1]))
    print("\n%d bijective (PROVEN) identification(s)." % proven)


if __name__ == "__main__":
    main()
