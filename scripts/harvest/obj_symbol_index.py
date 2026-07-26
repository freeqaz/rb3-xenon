#!/usr/bin/env python3
"""Index DEFINED COFF symbols across all our compiled objs -> {sym: [objpaths]}."""
import struct, sys, json
from pathlib import Path

def defined_syms(path):
    d = open(path, "rb").read()
    if len(d) < 20: return set()
    nsec = struct.unpack_from("<H", d, 2)[0]
    symptr, nsym = struct.unpack_from("<II", d, 8)
    if not symptr or not nsym: return set()
    strtab = symptr + nsym * 18
    out = set()
    def s_at(off):
        e = d.index(b"\0", strtab + off)
        return d[strtab + off:e].decode("latin1")
    i = 0
    while i < nsym:
        o = symptr + i * 18
        raw = d[o:o+8]
        if raw[:4] == b"\0\0\0\0":
            name = s_at(struct.unpack_from("<I", raw, 4)[0])
        else:
            name = raw.rstrip(b"\0").decode("latin1")
        val, sec, typ, cls, naux = struct.unpack_from("<IhHBB", d, o+8)
        # cls 2=EXTERNAL, 3=STATIC, 6=LABEL ; sec>0 => defined in this obj
        if sec > 0 and cls in (2, 3) and not name.startswith("."):
            out.add(name)
        i += 1 + naux
    return out

if __name__ == "__main__":
    root = Path(sys.argv[1])
    idx = {}
    objs = sorted(root.rglob("*.obj"))
    for p in objs:
        rel = str(p.relative_to(root))
        try:
            for s in defined_syms(p):
                idx.setdefault(s, []).append(rel)
        except Exception as e:
            print("ERR", rel, e, file=sys.stderr)
    json.dump(idx, open(sys.argv[2], "w"))
    print("objs=%d syms=%d" % (len(objs), len(idx)), file=sys.stderr)
