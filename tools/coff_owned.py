#!/usr/bin/env python3
"""Split a PE/COFF obj's defined function symbols into OWNED vs SHARED.

With /Gy every function lands in its own COMDAT section, but the COMDAT
SELECTION field distinguishes them:

  IMAGE_COMDAT_SELECT_NO_DUPLICATES (1) -- a non-inline function whose single
      definition lives in THIS translation unit's .cpp body.
  IMAGE_COMDAT_SELECT_ANY (2)           -- inline / template / implicitly-generated,
      emitted identically by every TU that instantiates it.

"Defined symbols" alone is a useless proxy for "what this .cpp contributes":
22 scatter objs define 11,524 symbols, overwhelmingly (2)-class header noise.
"""
import struct
import sys
from pathlib import Path

SEL_NO_DUPLICATES = 1
SEL_ANY = 2


def analyze(path):
    """Return (owned, shared) sets of defined function symbol names."""
    try:
        data = Path(path).read_bytes()
    except OSError:
        return set(), set()
    if len(data) < 20:
        return set(), set()
    _m, nsec, _t, ptr_sym, n_sym, opt, _c = struct.unpack_from("<HHIIIHH", data, 0)
    if ptr_sym == 0 or n_sym == 0 or ptr_sym + n_sym * 18 > len(data):
        return set(), set()
    strtab = data[ptr_sym + n_sym * 18:]

    # section table: need characteristics to know which are executable
    sec_off = 20 + opt
    exec_secs = set()
    for i in range(nsec):
        off = sec_off + i * 40
        if off + 40 > len(data):
            break
        chars = struct.unpack_from("<I", data, off + 36)[0]
        if chars & 0x00000020:  # IMAGE_SCN_CNT_CODE
            exec_secs.add(i + 1)

    def symname(off):
        raw = data[off:off + 8]
        if raw[:4] == b"\x00\x00\x00\x00":
            soff = struct.unpack_from("<I", raw, 4)[0]
            end = strtab.find(b"\x00", soff)
            return strtab[soff:end].decode("latin-1") if end >= 0 else ""
        return raw.rstrip(b"\x00").decode("latin-1")

    # pass 1: section-definition symbols carry the COMDAT selection in their aux
    sec_selection = {}
    entries = []
    i = 0
    while i < n_sym:
        off = ptr_sym + i * 18
        name = symname(off)
        value = struct.unpack_from("<I", data, off + 8)[0]
        secnum = struct.unpack_from("<h", data, off + 12)[0]
        sclass = data[off + 16]
        naux = data[off + 17]
        aux_off = off + 18
        if sclass == 3 and naux >= 1 and secnum > 0:  # IMAGE_SYM_CLASS_STATIC
            # section definition aux record: Selection at byte 14
            sel = data[aux_off + 14] if aux_off + 15 <= len(data) else 0
            if sel:
                sec_selection[secnum] = sel
        entries.append((name, secnum, sclass, value))
        i += 1 + naux

    owned, shared = set(), set()
    for name, secnum, sclass, _value in entries:
        if secnum <= 0 or secnum not in exec_secs:
            continue
        if sclass not in (2, 3):  # EXTERNAL or STATIC
            continue
        if name.startswith((".text", ".", "$")):
            continue
        sel = sec_selection.get(secnum, 0)
        if sel == SEL_NO_DUPLICATES:
            owned.add(name)
        elif sel:
            shared.add(name)
        else:
            owned.add(name)  # non-COMDAT code section: unambiguously ours
    return owned, shared


if __name__ == "__main__":
    for p in sys.argv[1:]:
        o, s = analyze(p)
        print(f"{p}: owned={len(o)} shared={len(s)}")
        for n in sorted(o)[:40]:
            print("   OWNED", n)
