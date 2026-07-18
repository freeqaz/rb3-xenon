#!/usr/bin/env python3
"""Inspect reloc destination names for functions in target vs base obj.
Extends tu5_reloc_masked_correlate parsing to resolve reloc SymbolTableIndex->name."""
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
        relocs = []
        for j in range(nreloc):
            va = struct.unpack_from("<I", data, preloc+j*10)[0]
            sidx = struct.unpack_from("<I", data, preloc+j*10+4)[0]
            typ = struct.unpack_from("<H", data, preloc+j*10+8)[0]
            relocs.append((va, sidx, typ))
        secs[i+1] = dict(raw_size=raw_size, praw=praw, relocs=relocs, chars=chars, idx=i+1)
    # full raw-index -> name table (including aux slots resolved to owner name)
    idxname = {}
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
        idxname[i] = name
        syms.append(dict(name=name, val=val, secn=secn, typ=typ, sc=sc, idx=i, naux=naux))
        i += 1 + naux
    return data, secs, syms, idxname

IMAGE_SCN_CNT_CODE = 0x20

def func_reloc_seq(path):
    """Return {func_name: (masked_body, [(off, dest_name), ...])}"""
    data, secs, syms, idxname = parse(path)
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
            seq = []
            for (va, sidx, typ) in s["relocs"]:
                if start <= va < end:
                    off = va - start
                    for b in range(4):
                        if off+b < len(body):
                            body[off+b] = 0
                    seq.append((off, idxname.get(sidx, f"?idx{sidx}"), typ))
            seq.sort()
            if sy["name"] not in out:
                out[sy["name"]] = (bytes(body), seq)
    return out

if __name__ == "__main__":
    unit_tgt = sys.argv[1]
    unit_base = sys.argv[2]
    t = func_reloc_seq(unit_tgt)
    b = func_reloc_seq(unit_base)
    # find a MULTI group: base content shared by >1 base sym
    base_by_content = defaultdict(list)
    for n,(body,seq) in b.items():
        base_by_content[body].append(n)
    shown = 0
    for tn in sorted(t):
        if not tn.startswith("fn_"): continue
        body, tseq = t[tn]
        cands = base_by_content.get(body, [])
        if len(cands) > 1:
            print(f"\n=== MULTI target {tn} size={len(body)} nreloc={len(tseq)} cands={len(cands)} ===")
            print(f"  TGT reloc dests: {[x[1] for x in tseq]}")
            for c in cands:
                cbody, cseq = b[c]
                print(f"  BASE {c}")
                print(f"       reloc dests: {[x[1] for x in cseq]}")
            shown += 1
            if shown >= 6: break
