#!/usr/bin/env python3
"""COFF extent model mirroring objdiff's symbol-size inference.

Rules transcribed from ../objdiff/objdiff-core/src/obj/read.rs (map_symbols,
infer_symbol_sizes, is_local_label) and object-0.37.3 read/coff/symbol.rs
(size(), kind()).  combine_text_sections defaults to FALSE, so extents never
cross a COMDAT boundary.
"""
import struct
from typing import Dict, List, Optional, Tuple

# storage classes
C_EXTERNAL = 2
C_STATIC = 3
C_LABEL = 6
C_FILE = 103
C_SECTION = 104
C_WEAK_EXTERNAL = 105

# objdiff SymbolKind
K_FUNCTION = "function"
K_OBJECT = "object"
K_SECTION = "section"
K_UNKNOWN = "unknown"


class Sym:
    __slots__ = ("name", "value", "sec", "sclass", "typ", "naux", "raw",
                 "kind", "size", "local", "addr")

    def __repr__(self):
        return f"<{self.name} sec={self.sec} addr={self.addr} size={self.size} {self.kind}>"


class Sec:
    __slots__ = ("index", "name", "size", "addr", "code", "data", "relocs",
                 "_relset")

    def reloc_in(self, addr, size):
        rs = getattr(self, "_relset", None)
        if rs is None:
            rs = self._relset = sorted(r[0] for r in self.relocs)
        import bisect
        i = bisect.bisect_left(rs, addr)
        return i < len(rs) and rs[i] < addr + size


def parse(data: bytes):
    """Return (sections, symbols) with objdiff-equivalent kind/size/addr."""
    if len(data) < 20:
        return None
    nsec = struct.unpack_from("<H", data, 2)[0]
    symoff = struct.unpack_from("<I", data, 8)[0]
    nsym = struct.unpack_from("<I", data, 12)[0]
    opt = struct.unpack_from("<H", data, 16)[0]
    if symoff == 0 or nsym == 0:
        return None
    strt = symoff + nsym * 18

    def sname(off):
        end = data.index(b"\x00", strt + off)
        return data[strt + off:end].decode("ascii", errors="replace")

    # ---- sections ----
    sections: List[Sec] = []
    for s in range(nsec):
        so = 20 + opt + s * 40
        if so + 40 > len(data):
            break
        nm = data[so:so + 8].rstrip(b"\x00").decode("ascii", errors="replace")
        if nm.startswith("/"):  # long name in string table
            try:
                nm = sname(int(nm[1:]))
            except Exception:
                pass
        vsize = struct.unpack_from("<I", data, so + 8)[0]
        vaddr = struct.unpack_from("<I", data, so + 12)[0]
        rawsz = struct.unpack_from("<I", data, so + 16)[0]
        rawpt = struct.unpack_from("<I", data, so + 20)[0]
        relpt = struct.unpack_from("<I", data, so + 24)[0]
        nrel = struct.unpack_from("<H", data, so + 32)[0]
        chars = struct.unpack_from("<I", data, so + 36)[0]
        sc = Sec()
        sc.index = s + 1
        sc.name = nm
        # object-crate CoffSection::size(): raw size, or virtual size if raw==0
        sc.size = rawsz if rawsz else vsize
        sc.addr = vaddr
        # IMAGE_SCN_CNT_CODE = 0x20
        sc.code = bool(chars & 0x20)
        sc.data = data[rawpt:rawpt + rawsz] if rawpt and rawsz else b""
        rl = []
        for r in range(nrel):
            ro = relpt + r * 10
            if ro + 10 > len(data):
                break
            va, si = struct.unpack_from("<II", data, ro)
            ty = struct.unpack_from("<H", data, ro + 8)[0]
            rl.append((va, si, ty))
        sc.relocs = rl
        sc._relset = None
        sections.append(sc)
    bysec = {sc.index: sc for sc in sections}

    # ---- symbols ----
    symbols: List[Sym] = []
    i = 0
    while i < nsym:
        eo = symoff + i * 18
        if eo + 18 > len(data):
            break
        nb = data[eo:eo + 8]
        if nb[:4] == b"\x00\x00\x00\x00":
            try:
                name = sname(struct.unpack_from("<I", nb, 4)[0])
            except Exception:
                name = ""
        else:
            name = nb.split(b"\x00")[0].decode("ascii", errors="replace")
        value = struct.unpack_from("<I", data, eo + 8)[0]
        sec = struct.unpack_from("<h", data, eo + 12)[0]
        typ = struct.unpack_from("<H", data, eo + 14)[0]
        sclass = data[eo + 16]
        naux = data[eo + 17]

        derived_fn = ((typ >> 4) & 0xF) == 2  # IMAGE_SYM_DTYPE_FUNCTION
        has_aux_section = (sclass == C_STATIC and naux > 0)
        has_aux_function = (sclass == C_EXTERNAL and derived_fn and naux > 0)

        # object-crate kind()
        if sclass == C_STATIC:
            okind = K_SECTION if has_aux_section else (K_FUNCTION if derived_fn else K_OBJECT)
        elif sclass in (C_EXTERNAL, C_WEAK_EXTERNAL):
            okind = K_FUNCTION if derived_fn else K_OBJECT
        elif sclass == C_SECTION:
            okind = K_SECTION
        elif sclass == C_FILE:
            i += 1 + naux
            continue  # objdiff filters File symbols out entirely
        else:  # LABEL and everything else -> object::SymbolKind::Label/Unknown
            okind = K_UNKNOWN

        # object-crate size()
        size = 0
        if sclass == C_STATIC and has_aux_section:
            size = struct.unpack_from("<I", data, symoff + (i + 1) * 18)[0]
        elif sclass == C_EXTERNAL:
            if sec == 0:
                size = value
            elif has_aux_function:
                size = struct.unpack_from("<I", data, symoff + (i + 1) * 18 + 4)[0]
        # objdiff: section symbols get size forced to 0
        if okind == K_SECTION:
            size = 0

        sy = Sym()
        sy.name = name
        sy.value = value
        sy.sec = sec
        sy.sclass = sclass
        sy.typ = typ
        sy.naux = naux
        sy.raw = i
        sy.kind = okind
        sy.size = size
        sy.local = sclass not in (C_EXTERNAL, C_WEAK_EXTERNAL)
        s_obj = bysec.get(sec)
        sy.addr = (s_obj.addr if s_obj else 0) + value
        symbols.append(sy)
        i += 1 + naux

    return sections, symbols


def _is_local_label(sy: Sym) -> bool:
    if sy.size != 0:
        return False
    if sy.name.startswith("$L"):
        return True
    return sy.local and any(sy.name.startswith(p) for p in (".L", "LAB_", "switchD_"))


def infer_sizes(sections: List[Sec], symbols: List[Sym]) -> None:
    """objdiff infer_symbol_sizes, in place."""
    bysec = {sc.index: sc for sc in sections}
    order = sorted(
        range(len(symbols)),
        key=lambda i: (
            symbols[i].sec if symbols[i].sec > 0 else 1 << 30,
            0 if symbols[i].kind == K_SECTION else 1,
            symbols[i].addr,
            symbols[i].size,
        ),
    )
    S = [symbols[i] for i in order]

    it = 0
    last_end = (0, 0)
    n = len(S)
    while it < n:
        cur = S[it]
        if cur.sec <= 0:
            break
        sec_idx = cur.sec
        it += 1
        if cur.size != 0:
            if cur.kind != K_SECTION:
                last_end = (sec_idx, cur.addr + cur.size)
            continue
        if last_end[0] == sec_idx and last_end[1] > cur.addr:
            continue
        nxt = None
        while True:
            if it >= n:
                break
            cand = S[it]
            if cand.sec != sec_idx:
                break
            if cur.kind in (K_FUNCTION, K_OBJECT):
                ok = cand.kind in (K_FUNCTION, K_OBJECT)
            else:  # Unknown / Section -> stop at any symbol
                ok = True
            if ok and not _is_local_label(cand):
                nxt = cand
                break
            it += 1
        sc = bysec.get(sec_idx)
        if sc is None:
            continue
        next_addr = nxt.addr if nxt is not None else sc.addr + sc.size
        if sc.code:
            # ★ ArchPpc::infer_function_size (objdiff-core/src/arch/ppc/mod.rs:462):
            # trim trailing 4-byte ZERO words that carry no relocation.  This is
            # applied to BOTH sides, so pure zero-padding never shows up as an
            # extent delta -- finding this is what stopped me attributing the
            # -4/-12 classes to padding.
            while next_addr >= cur.addr + 4:
                off = next_addr - 4 - sc.addr
                if off < 0 or off + 4 > len(sc.data):
                    break
                if sc.data[off:off + 4] != b"\x00\x00\x00\x00":
                    break
                if sc.reloc_in(next_addr - 4, 4):
                    break
                next_addr -= 4
        new_size = max(0, next_addr - cur.addr)
        if new_size > 0:
            cur.size = new_size
            if cur.kind == K_UNKNOWN:
                cur.kind = K_FUNCTION if sc.code else K_OBJECT
            if cur.kind != K_SECTION:
                last_end = (sec_idx, cur.addr + cur.size)


def load(path) -> Optional[Tuple[List[Sec], List[Sym]]]:
    with open(path, "rb") as f:
        data = f.read()
    p = parse(data)
    if not p:
        return None
    secs, syms = p
    infer_sizes(secs, syms)
    return secs, syms


def is_hidden(name: str) -> bool:
    return (name.startswith("except_data_") or name.startswith("__unwind")
            or name.startswith("__catch"))
