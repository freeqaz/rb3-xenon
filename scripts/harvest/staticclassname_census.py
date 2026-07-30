#!/usr/bin/env python3
"""Census every OBJ_CLASSNAME `StaticClassName` / `Message::Type` body in retail,
recover its class-name STRING, and join that against target_symbol_map.json,
splits.txt and the per-obj COMDAT supply -- to find (a) mispaired names and
(b) bodies that currently earn ZERO and could earn 100 with a pin/carve.

WHY THIS EXISTS (lane BS-2, 2026-07-30)
---------------------------------------
`OBJ_CLASSNAME(X)` defines `X::StaticClassName()` INLINE in the class body, so
its COMDAT is emitted by EVERY TU that includes the header; the linker keeps one
and places it in that obj's contribution.  Retail compiles it to a rigid
22-instruction template whose ONLY varying fields are three relocations: the
guard word, the cached Symbol, and the class-name string.

objdiff runs functionRelocDiffs=None.  Therefore **every one of these ~450
bodies is byte-identical to every other under masking**, and a body scores
100.0% purely on whether the unit it is pinned in supplies a COMDAT with the
mapped name -- the name does not have to be CORRECT.  Two consequences:

  * a wrong name here earns silent false credit and BLOCKS the real owner;
  * byte-similarity is worthless for identifying these.  Use the STRING (this
    tool) plus the caller test: a class's ClassName()/SetType() `bl` their OWN
    StaticClassName, at +0x14 and +0x4c respectively.  Spatial adjacency in
    .text is corroborating evidence, because /O1 without LTCG preserves per-obj
    contribution grouping.

So the yield in this channel is NOT "fix the names" (metric-neutral: the false
credit just moves).  It is the two populations this tool prints last:
UNPINNED bodies, and bodies pinned in a unit that CANNOT supply their name.
Those earn 0 today and are free upside.

USAGE
    python3 scripts/harvest/staticclassname_census.py [--project-dir .]
"""
import argparse
import bisect
import json
import os
import re
import struct
import sys

BASE = 0x82000000
TVA, TRAW, TSZ = 0x82270000, 0x264E00, 0x9DCE3C

# Fully-fixed words of the template (index = instruction slot).
FIX = {0: 0x7D8802A6, 2: 0x3BE1FF90, 3: 0x9421FF90, 6: 0x7C7D1B78,
       9: 0x556907FF, 10: 0x4082001C, 11: 0x616B0001, 14: 0x7FC3F378,
       17: 0x817E0000, 18: 0x7FA3EB78, 19: 0x917D0000, 20: 0x383F0070}
# Words whose high half is fixed (low half is a relocated @ha/@l field).
SHAPE = {4: 0x3D400000, 5: 0x3D600000, 7: 0x3BCB0000, 8: 0x816A0000,
         12: 0x916A0000, 13: 0x3D600000, 15: 0x388B0000}


def census(binpath):
    """Return [(va, class_name_string)] for every template instance in .text."""
    d = open(binpath, 'rb').read()
    t = d[TRAW:TRAW + TSZ]
    u32 = lambda i: struct.unpack_from('>I', t, i)[0]
    out = []
    for i in range(0, TSZ - 22 * 4, 4):
        if u32(i) != 0x7D8802A6:
            continue
        if any(u32(i + k * 4) != v for k, v in FIX.items()):
            continue
        if any((u32(i + k * 4) & 0xFFFF0000) != v for k, v in SHAPE.items()):
            continue
        if (u32(i + 4) & 0xFC000003) != 0x48000001:          # bl __savegprlr_29
            continue
        if (u32(i + 16 * 4) & 0xFC000003) != 0x48000001:     # bl Symbol::Symbol
            continue
        if (u32(i + 21 * 4) & 0xFC000003) != 0x48000000:     # b  __restgprlr_29
            continue
        hi, lo = u32(i + 13 * 4) & 0xFFFF, u32(i + 15 * 4) & 0xFFFF
        sva = ((hi << 16) + (lo - 0x10000 if lo >= 0x8000 else lo)) & 0xFFFFFFFF
        o = sva - BASE                     # .rdata: RVA == raw offset in band.exe
        if not 0 <= o < len(d):
            continue
        try:
            s = d[o:d.index(b'\0', o, o + 80)].decode('ascii')
        except Exception:
            continue
        if s and all(32 <= ord(c) < 127 for c in s):
            out.append((TVA + i, s))
    return out


def load_splits(path):
    rows, unit = [], None
    for line in open(path):
        s = line.rstrip('\n')
        if not s.strip():
            continue
        if not s.startswith((' ', '\t')):
            unit = s.strip().rstrip(':')
            continue
        m = re.match(r'\s*\.text\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)', s)
        if m:
            rows.append((int(m.group(1), 16), int(m.group(2), 16), unit))
    rows.sort()
    return rows


def supply_index(objroot):
    """name -> [source paths whose .obj emits that COMDAT]."""
    sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
    idx = {}
    for dp, _, fns in os.walk(objroot):
        for f in fns:
            if not f.endswith('.obj'):
                continue
            p = os.path.join(dp, f)
            try:
                names = _coff_names(p)
            except Exception:
                continue
            rel = os.path.relpath(p, objroot)[:-4] + '.cpp'
            for n in names:
                idx.setdefault(n, set()).add(rel)
    return {k: sorted(v) for k, v in idx.items()}


def _coff_names(path):
    d = open(path, 'rb').read()
    _, _, _, psym, nsym, _, _ = struct.unpack_from('<HHIIIHH', d, 0)
    strt = psym + nsym * 18
    out, i = [], 0
    while i < nsym:
        o = psym + i * 18
        raw = d[o:o + 8]
        if raw[:4] == b'\0\0\0\0':
            off = struct.unpack_from('<I', raw, 4)[0]
            name = d[strt + off:d.index(b'\0', strt + off)].decode('utf8', 'replace')
        else:
            name = raw.rstrip(b'\0').decode('utf8', 'replace')
        secn, naux = struct.unpack_from('<h', d, o + 12)[0], d[o + 17]
        if secn > 0 and (name.startswith('?StaticClassName@')
                         or (name.startswith('?Type@') and name.endswith('SA?AVSymbol@@XZ'))):
            out.append(name)
        i += 1 + naux
    return out


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument('--project-dir', default='.')
    a = ap.parse_args()
    P = a.project_dir
    cen = census(os.path.join(P, 'orig/45410914/band.exe'))
    smap = json.load(open(os.path.join(P, 'scripts/target_symbol_map.json')))
    a2n = {int(k, 16): v for k, v in smap.items() if k.startswith('0x')}
    sp = load_splits(os.path.join(P, 'config/45410914/splits.txt'))
    starts = [r[0] for r in sp]
    supply = supply_index(os.path.join(P, 'build/45410914/src'))

    objs = json.load(open(os.path.join(P, 'config/45410914/objects.json')))
    paths = []
    def rec(o):
        if isinstance(o, dict):
            for k, v in o.items():
                if isinstance(k, str) and k.endswith('.cpp'):
                    paths.append(k)
                rec(v)
        elif isinstance(o, list):
            for v in o:
                rec(v)
    rec(objs)
    bybase = {}
    for p in set(paths):
        bybase.setdefault(os.path.basename(p), []).append(p)

    def resolve(u):
        if u is None:
            return None
        if u in paths:
            return u
        c = bybase.get(os.path.basename(u)) or []
        return c[0] if len(c) == 1 else next((p for p in paths if p.endswith('/' + u)), None)

    def owner(va):
        i = bisect.bisect_right(starts, va) - 1
        return sp[i][2] if i >= 0 and sp[i][0] <= va < sp[i][1] else None

    unpinned, dead = [], []
    for va, s in cen:
        u, n = owner(va), a2n.get(va)
        if u is None:
            unpinned.append((va, s, n))
        elif n is not None and resolve(u) not in supply.get(n, []):
            dead.append((va, s, n, u, supply.get(n, [])))

    print(f'{len(cen)} template bodies in .text\n')
    print(f'== UNPINNED (earn 0; free upside if pinned into a supplier) : {len(unpinned)}')
    for va, s, n in unpinned:
        print(f'  {va:08x} "{s}"  map={n}  suppliers={len(supply.get(n or "", []))}')
    print(f'\n== PINNED but unit CANNOT supply the mapped name (earn 0) : {len(dead)}')
    for va, s, n, u, sup in dead:
        print(f'  {va:08x} "{s}"\n      map={n}\n      unit={u}  suppliers={len(sup)} {sup[:3]}')


if __name__ == '__main__':
    main()
