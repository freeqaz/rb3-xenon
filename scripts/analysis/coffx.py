"""Minimal COFF (PPC/XBOX360 MSVC + dtk-split) reader: sections, symbols, relocations.

Mirrors the parts of objdiff's obj::read that funclet_signature depends on:
  * section.data / section.address (COFF: 0) / section.kind
  * symbol.address (= COFF Value, section-relative) and symbol.size (inferred)
  * section.relocations[].address (= COFF reloc VirtualAddress, section-relative)
"""
import struct

IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3
IMAGE_SYM_CLASS_LABEL = 6
IMAGE_SCN_CNT_CODE = 0x00000020
IMAGE_SCN_MEM_EXECUTE = 0x20000000


class Sec:
    __slots__ = ('name', 'vsize', 'vaddr', 'rawsize', 'rawptr', 'relptr', 'nrel',
                 'chars', 'data', 'relocs', 'is_code', 'index')


class Sym:
    __slots__ = ('name', 'value', 'sec', 'typ', 'cls', 'naux', 'size', 'index', 'kind')


def read_coff(data):
    if len(data) < 20:
        return None, None
    mach, nsec, tds, symoff, nsym, optsz, ch = struct.unpack_from('<HHIIIHH', data, 0)
    if symoff == 0 or nsym == 0:
        return None, None
    strtab = symoff + nsym * 18
    secs = []
    so = 20 + optsz
    for i in range(nsec):
        o = so + i * 40
        if o + 40 > len(data):
            return None, None
        s = Sec()
        nb = data[o:o + 8]
        if nb[:1] == b'/':
            try:
                a = strtab + int(nb[1:].rstrip(b'\0').decode())
                e = data.find(b'\0', a)
                s.name = data[a:e].decode('ascii', 'replace')
            except Exception:
                s.name = nb.rstrip(b'\0').decode('ascii', 'replace')
        else:
            s.name = nb.rstrip(b'\0').decode('ascii', 'replace')
        s.vsize, s.vaddr, s.rawsize, s.rawptr, s.relptr, _lp, s.nrel, _nl = \
            struct.unpack_from('<IIIIIIHH', data, o + 8)
        s.chars = struct.unpack_from('<I', data, o + 36)[0]
        s.index = i
        s.is_code = bool(s.chars & (IMAGE_SCN_CNT_CODE | IMAGE_SCN_MEM_EXECUTE))
        if s.rawptr and s.rawsize:
            s.data = data[s.rawptr:s.rawptr + s.rawsize]
        else:
            s.data = b''
        s.relocs = []
        # NRELOC_OVFL not handled (nrel==0xffff); irrelevant at these sizes.
        for r in range(s.nrel):
            ro = s.relptr + r * 10
            if ro + 10 > len(data):
                break
            va, symidx, typ = struct.unpack_from('<IIH', data, ro)
            s.relocs.append((va, symidx, typ))
        secs.append(s)

    syms = []
    i = 0
    while i < nsym:
        off = symoff + i * 18
        if off + 18 > len(data):
            break
        nb = data[off:off + 8]
        if nb[:4] == b'\x00\x00\x00\x00':
            a = strtab + struct.unpack_from('<I', nb, 4)[0]
            e = data.find(b'\0', a)
            name = data[a:e if e >= 0 else len(data)].decode('ascii', 'replace')
        else:
            name = nb.split(b'\x00')[0].decode('ascii', 'replace')
        val = struct.unpack_from('<I', data, off + 8)[0]
        sec = struct.unpack_from('<h', data, off + 12)[0]
        typ = struct.unpack_from('<H', data, off + 14)[0]
        cls = data[off + 16]
        naux = data[off + 17]
        s = Sym()
        s.name, s.value, s.sec, s.typ, s.cls, s.naux = name, val, sec, typ, cls, naux
        s.size = 0
        s.index = i
        syms.append(s)
        i += 1 + naux
    return secs, syms


K_FUNC, K_OBJ, K_SEC, K_UNK = 'F', 'O', 'S', 'U'


def sym_kind(s):
    """object-crate CoffSymbol::kind() -> objdiff SymbolKind."""
    if s.cls == IMAGE_SYM_CLASS_STATIC and s.value == 0 and s.naux > 0:
        return K_SEC
    derived = K_FUNC if ((s.typ >> 4) & 0xF) == 0x2 else K_OBJ
    if s.cls in (IMAGE_SYM_CLASS_EXTERNAL, IMAGE_SYM_CLASS_STATIC, 105):
        return derived
    if s.cls == IMAGE_SYM_CLASS_LABEL:
        return K_UNK          # object -> SymbolKind::Label -> objdiff Unknown
    return K_UNK


LABEL_PREFIXES = ('.L', 'LAB_', 'switchD_')


def infer_sizes(secs, syms):
    """Faithful port of objdiff `infer_symbol_sizes` for COFF objects (COFF
    symbols always start with size 0)."""
    for s in syms:
        s.kind = sym_kind(s)
        s.size = 0
    lst = [s for s in syms if s.sec > 0 and s.sec - 1 < len(secs)]
    lst.sort(key=lambda s: (s.sec - 1, 0 if s.kind == K_SEC else 1, s.value, s.index))
    n = len(lst)
    i = 0
    last_end = (-1, 0)
    while i < n:
        s = lst[i]
        sidx = s.sec - 1
        i += 1
        if s.size != 0:
            continue
        if last_end[0] == sidx and last_end[1] > s.value:
            continue
        j = i
        nxt = None
        while j < n:
            t = lst[j]
            if t.sec - 1 != sidx:
                break
            islabel = (t.size == 0 and t.cls == IMAGE_SYM_CLASS_STATIC
                       and any(t.name.startswith(p) for p in LABEL_PREFIXES))
            if s.kind in (K_FUNC, K_OBJ):
                ok = t.kind in (K_FUNC, K_OBJ)
            else:
                ok = True
            if ok and not islabel:
                nxt = t
                break
            j += 1
        sec = secs[sidx]
        secsize = sec.rawsize if sec.rawsize else sec.vsize
        nxt_addr = nxt.value if nxt is not None else secsize
        if s.kind == K_SEC and not sec.is_code:
            newsize = 0
        else:
            newsize = max(0, nxt_addr - s.value)
        if newsize > 0:
            s.size = newsize
            if s.kind != K_SEC:
                last_end = (sidx, s.value + newsize)
    return syms


def funclet_signature(sec, sym):
    """objdiff's funclet_signature: symbol bytes with a 4-byte window zeroed at
    every relocation address inside the symbol."""
    if sym.size == 0:
        return None
    start = sym.value
    end = start + sym.size
    if end > len(sec.data):
        return None
    b = bytearray(sec.data[start:end])
    for (va, si, typ) in sec.relocs:
        if va < start or va >= end:
            continue
        o = va - start
        for k in range(o, min(o + 4, len(b))):
            b[k] = 0
    return bytes(b)


def is_funclet_like(name):
    if name.startswith('__unwind$'):
        return name[9:].isdigit() and len(name) > 9
    if name.startswith('__catch$'):
        return name[8:].isdigit() and len(name) > 8
    if name.startswith('__unwind__merged_'):
        return True
    if name.startswith('fn_'):
        r = name[3:]
        return len(r) == 8 and all(c in '0123456789abcdefABCDEF' for c in r)
    if name.startswith('??__E') or name.startswith('??__F'):
        return True
    return False
