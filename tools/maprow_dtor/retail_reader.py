#!/usr/bin/env python3
"""Retail band.exe reader: VA->file offset via real PE section headers, plus
PowerPC bl-target extraction with RECURSIVE thunk following.

Carries positive + fail-on-demand controls (--selftest). The VA->offset map is
built from the section table, NOT from `va-0x82000000` (that shortcut is only
valid for .rdata on this image -- see project_bandexe_read_traps).
"""
import struct, json, re, sys

BAND = 'orig/45410914/band.exe'


class Image:
    def __init__(self, path=BAND):
        self.d = open(path, 'rb').read()
        pe = struct.unpack_from('<I', self.d, 0x3c)[0]
        assert self.d[pe:pe + 4] == b'PE\0\0', 'not a PE'
        nsec = struct.unpack_from('<H', self.d, pe + 6)[0]
        optsz = struct.unpack_from('<H', self.d, pe + 20)[0]
        self.base = struct.unpack_from('<I', self.d, pe + 24 + 28)[0]
        st = pe + 24 + optsz
        self.secs = []
        for i in range(nsec):
            o = st + 40 * i
            name = self.d[o:o + 8].rstrip(b'\0').decode('ascii', 'replace')
            vsz, va, rsz, ptr = struct.unpack_from('<IIII', self.d, o + 8)
            self.secs.append(dict(name=name, va=va, vsz=vsz, rsz=rsz, ptr=ptr))

    def off(self, va):
        rva = va - self.base
        for s in self.secs:
            if s['va'] <= rva < s['va'] + max(s['vsz'], s['rsz']):
                delta = rva - s['va']
                if delta < s['rsz']:
                    return s['ptr'] + delta
                return None  # in virtual tail, no file bytes
        return None

    def sect(self, va):
        rva = va - self.base
        for s in self.secs:
            if s['va'] <= rva < s['va'] + max(s['vsz'], s['rsz']):
                return s['name']
        return None

    def word(self, va):
        o = self.off(va)
        if o is None or o + 4 > len(self.d):
            return None
        return struct.unpack_from('>I', self.d, o)[0]

    def body(self, va, size):
        o = self.off(va)
        if o is None:
            return None
        return self.d[o:o + size]


def insns(img, va, size):
    b = img.body(va, size)
    if not b:
        return []
    return [(va + 4 * i, struct.unpack_from('>I', b, 4 * i)[0])
            for i in range(len(b) // 4)]


def branch_target(addr, ins):
    """Return (target, is_link) for I-form b/bl/ba/bla, else None."""
    if (ins >> 26) != 18:
        return None
    li = ins & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    aa = (ins >> 1) & 1
    lk = ins & 1
    return ((li if aa else addr + li) & 0xFFFFFFFF, bool(lk))


def bl_targets(img, va, size):
    out = []
    for a, i in insns(img, va, size):
        t = branch_target(a, i)
        if t and t[1]:
            out.append(t[0])
    return out


def tail_b_target(img, va, size):
    """Unconditional non-link branch (a thunk's forward)."""
    for a, i in insns(img, va, size):
        t = branch_target(a, i)
        if t and not t[1]:
            return t[0]
    return None


def dethunk(img, va, sizes, depth=0, seen=None):
    """RECURSIVELY follow thunk chains. Retail has two-level chains
    (vtordisp -> adjustor -> base); a single-level read treats the
    intermediate thunk as the body and manufactures wrong targets."""
    if seen is None:
        seen = set()
    if va in seen or depth > 8:
        return va, depth
    seen.add(va)
    sz = sizes.get(va)
    if sz is None or sz > 32:
        return va, depth
    ins = insns(img, va, sz)
    # a thunk is a short body ending in an unconditional b (possibly after
    # an addi to adjust `this`)
    tb = None
    for a, i in ins:
        t = branch_target(a, i)
        if t and not t[1]:
            tb = t[0]
    if tb is None:
        return va, depth
    return dethunk(img, tb, sizes, depth + 1, seen)


def selftest():
    img = Image()
    ok = True
    print('sections:')
    for s in img.secs:
        print(f"  {s['name']:10s} rva={s['va']:#010x} vsz={s['vsz']:#x} "
              f"raw={s['ptr']:#x} rsz={s['rsz']:#x}")
    print(f'image base {img.base:#x}')

    # CONTROL 1: the documented trap -- va-0x82000000 must be WRONG for .text
    va = 0x82270000
    naive = va - 0x82000000
    real = img.off(va)
    print(f'\n  trap check: va {va:#x} -> real off {real:#x}, naive {naive:#x} '
          f'-> {"DIFFER (good, mapping is real)" if real != naive else "EQUAL (suspicious)"}')
    ok &= real != naive

    # CONTROL 2: fail-on-demand -- an address outside every section must return None
    bad = img.off(0x70000000)
    print(f'  fail-on-demand: off(0x70000000) = {bad} '
          f'-> {"None (good)" if bad is None else "NOT None (BROKEN)"}')
    ok &= bad is None

    # CONTROL 3: branch decoder against hand-computed values
    # bl +0x20 at 0x82000000 -> 0x82000020 ; encoding 0x48000021
    t = branch_target(0x82000000, 0x48000021)
    print(f'  bl decode: {t} want (0x82000020, True) -> '
          f'{"OK" if t == (0x82000020, True) else "FAIL"}')
    ok &= t == (0x82000020, True)
    # backward: bl -0x10 -> 0x480000... li=-16 => 0x4BFFFFF1
    t = branch_target(0x82000000, 0x4BFFFFF1)
    print(f'  bl back:   {t} want (0x81fffff0, True) -> '
          f'{"OK" if t == (0x81FFFFF0, True) else "FAIL"}')
    ok &= t == (0x81FFFFF0, True)
    # non-branch must decode to None
    t = branch_target(0x82000000, 0x60000000)  # ori r0,r0,0 (nop)
    print(f'  non-branch: {t} -> {"None (good)" if t is None else "FAIL"}')
    ok &= t is None
    print('SELFTEST', 'PASS' if ok else 'FAIL')
    return 0 if ok else 1


if __name__ == '__main__':
    sys.exit(selftest())
