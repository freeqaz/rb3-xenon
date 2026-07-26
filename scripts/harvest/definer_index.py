#!/usr/bin/env python3
"""laneAV lead: build an index mangled-name -> [objs that DEFINE it] from our
compiled COFF objects.  A symbol is DEFINED here iff its section number > 0.

This is the `n_definers` gate: a VA whose mangled name is defined by no compiled
obj cannot be made to pay by any map edit or splits pin.
"""
import struct, sys, json, collections
from pathlib import Path

def coff_defined(path):
    b = path.read_bytes()
    if len(b) < 20: return []
    machine, nsec, ts, symptr, nsym, oh, ch = struct.unpack_from('<HHIIIHH', b, 0)
    if not symptr or not nsym or symptr + nsym*18 > len(b): return []
    strtab = symptr + nsym*18
    out = []
    i = 0
    while i < nsym:
        off = symptr + i*18
        raw = b[off:off+8]
        value, secnum, styp, sclass, naux = struct.unpack_from('<IhHBB', b, off+8)
        if raw[:4] == b'\0\0\0\0':
            so = struct.unpack_from('<I', raw, 4)[0]
            e = b.find(b'\0', strtab+so)
            name = b[strtab+so:e].decode('latin1')
        else:
            name = raw.rstrip(b'\0').decode('latin1')
        if secnum > 0 and sclass in (2, 3, 105):   # EXTERNAL / STATIC / WEAK-ish
            out.append((name, sclass))
        i += 1 + naux
    return out

def build(root):
    idx = collections.defaultdict(list)
    objs = sorted(Path(root).rglob('*.obj'))
    for p in objs:
        try: syms = coff_defined(p)
        except Exception: continue
        rel = str(p)
        for n, sc in syms:
            idx[n].append(rel)
    return idx, len(objs)

if __name__ == '__main__':
    root = sys.argv[1]
    idx, nobj = build(root)
    print('objs scanned: %d, distinct defined symbols: %d' % (nobj, len(idx)), file=sys.stderr)
    if len(sys.argv) > 2:
        json.dump({k: v for k, v in idx.items()}, open(sys.argv[2], 'w'))
