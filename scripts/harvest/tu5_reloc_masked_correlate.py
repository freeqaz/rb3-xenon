#!/usr/bin/env python3
"""Correlate unmapped target fn_<addr> against base compiled symbols by
reloc-masked byte content. A unique byte-identical match => guaranteed
strict-100 flip when a map entry {addr: base_name} is added.

Handles two obj layouts:
 - dtk target: one COMDAT .text$dup section per function, func sym val=0.
 - base compiled: functions may share a .text section, val=offset.
Section body is read via the function symbol's section; relocs in range masked.
"""
import struct, sys
from pathlib import Path
from collections import defaultdict

def parse(path):
    data = Path(path).read_bytes()
    nsec = struct.unpack_from("<H", data, 2)[0]
    symoff = struct.unpack_from("<I", data, 8)[0]
    nsym = struct.unpack_from("<I", data, 12)[0]
    optsz = struct.unpack_from("<H", data, 16)[0]
    str_start = symoff + nsym*18
    secs = {}
    base = 20 + optsz
    for i in range(nsec):
        o = base + i*40
        raw_size = struct.unpack_from("<I", data, o+16)[0]
        praw = struct.unpack_from("<I", data, o+20)[0]
        preloc = struct.unpack_from("<I", data, o+24)[0]
        nreloc = struct.unpack_from("<H", data, o+32)[0]
        chars = struct.unpack_from("<I", data, o+36)[0]
        relocs = [struct.unpack_from("<I", data, preloc+j*10)[0] for j in range(nreloc)]
        # parallel full-form relocs (va, symbol_table_index, type); 'relocs'
        # kept as-is for existing importers. NOTE: type 0x12 (IMAGE_REL_PPC_PAIR)
        # entries carry a displacement in the symidx field, NOT a symbol index.
        relocs_full = [struct.unpack_from("<IIH", data, preloc+j*10) for j in range(nreloc)]
        secs[i+1] = dict(raw_size=raw_size, praw=praw, relocs=relocs,
                        relocs_full=relocs_full, chars=chars, idx=i+1)
    syms = []
    i = 0
    while i < nsym:
        o = symoff + i*18
        nm = data[o:o+8]
        if nm[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", nm, 4)[0]
            e = data.index(b"\0", str_start+so)
            name = data[str_start+so:e].decode("latin1")
        else:
            name = nm.split(b"\0")[0].decode("latin1")
        val = struct.unpack_from("<I", data, o+8)[0]
        secn = struct.unpack_from("<h", data, o+12)[0]
        typ = struct.unpack_from("<H", data, o+14)[0]
        sc = data[o+16]
        naux = data[o+17]
        syms.append(dict(name=name, val=val, secn=secn, typ=typ, sc=sc, idx=i))
        i += 1 + naux
    return data, secs, syms

IMAGE_SCN_CNT_CODE = 0x20

def func_bodies(path):
    data, secs, syms = parse(path)
    # function symbols in code sections
    by_sec = defaultdict(list)
    for sy in syms:
        if sy["secn"] > 0 and sy["secn"] in secs and sy["sc"] in (2,3) and sy["typ"] == 0x20:
            by_sec[sy["secn"]].append(sy)
    out = {}
    for secn, members in by_sec.items():
        s = secs[secn]
        if not (s["chars"] & IMAGE_SCN_CNT_CODE):
            continue
        members = sorted(members, key=lambda x: x["val"])
        for k, sy in enumerate(members):
            start = sy["val"]
            end = members[k+1]["val"] if k+1 < len(members) else s["raw_size"]
            if s["praw"] == 0 or end <= start:
                continue
            body = bytearray(data[s["praw"]+start:s["praw"]+end])
            for rva in s["relocs"]:
                if start <= rva < end:
                    off = rva - start
                    for b in range(4):
                        if off+b < len(body):
                            body[off+b] = 0
            # keep only the first (usually only) sym per (secn,start)
            out.setdefault(sy["name"], bytes(body))
    return out

def main():
    tgt = func_bodies(sys.argv[1])
    base = func_bodies(sys.argv[2])
    base_by_content = defaultdict(list)
    for n, b in base.items():
        base_by_content[b].append(n)
    unmapped = {n: b for n, b in tgt.items() if n.startswith("fn_")}
    print(f"target funcs={len(tgt)} unmapped(fn_)={len(unmapped)} base funcs={len(base)}")
    uniq = []
    for n in sorted(unmapped):
        b = unmapped[n]
        cands = base_by_content.get(b, [])
        if len(cands) == 1:
            uniq.append((n, len(b), cands[0]))
            print(f"  {n} size={len(b):4d} UNIQUE -> {cands[0]}")
        elif len(cands) > 1:
            print(f"  {n} size={len(b):4d} MULTI({len(cands)})")
        else:
            print(f"  {n} size={len(b):4d} NOMATCH")
    print(f"UNIQUE byte-matches: {len(uniq)}")
    # emit json of unique proposals
    import json
    props = {f"0x{n[3:].upper()}": bn for n, _, bn in uniq}
    Path(sys.argv[3]).write_text(json.dumps(props, indent=1)) if len(sys.argv) > 3 else None

if __name__ == "__main__":
    main()
