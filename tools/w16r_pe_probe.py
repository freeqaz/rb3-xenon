#!/usr/bin/env python3
"""W16-R retail-byte probe helper (lane W16-R, 2026-09-14).

Img(path): minimal PE reader for band.exe / ham_xbox_r.exe -- off/word/bytes,
dis(va,n) via capstone PPC big-endian (returns [] outside text_range()),
branch_target(va,word) for b/bl.  pdata(im): BIG-ENDIAN .pdata records as
(begin,end,flags,prolog).  Used by the heredoc probes recorded in
docs/decomp/W16R_SETDISKERROR_CAVE_SERVER_VTABLE_2026-09-14.md.
"""
"""Minimal PE section mapper + PPC BE disassembler for band.exe images."""
import struct, sys
from capstone import Cs, CS_ARCH_PPC, CS_MODE_BIG_ENDIAN, CS_MODE_32

class Img:
    def __init__(self, path):
        self.path = path
        self.d = open(path, 'rb').read()
        d = self.d
        pe = struct.unpack_from('<I', d, 0x3c)[0]
        assert d[pe:pe+4] == b'PE\0\0', path
        nsec = struct.unpack_from('<H', d, pe+6)[0]
        opt_sz = struct.unpack_from('<H', d, pe+20)[0]
        self.imgbase = struct.unpack_from('<I', d, pe+24+28)[0]
        self.entry = struct.unpack_from('<I', d, pe+24+16)[0] + self.imgbase
        secs = []
        off = pe + 24 + opt_sz
        for i in range(nsec):
            name = d[off:off+8].rstrip(b'\0').decode()
            vsize, va, rsize, raw = struct.unpack_from('<IIII', d, off+8)
            secs.append((name, self.imgbase+va, vsize, raw, rsize))
            off += 40
        self.secs = secs
        self.cs = Cs(CS_ARCH_PPC, CS_MODE_BIG_ENDIAN | CS_MODE_32)
    def off(self, va):
        for name, sva, vsize, raw, rsize in self.secs:
            if sva <= va < sva + max(vsize, rsize):
                return raw + (va - sva)
        raise KeyError(hex(va))
    def sec(self, va):
        for name, sva, vsize, raw, rsize in self.secs:
            if sva <= va < sva + max(vsize, rsize):
                return name, sva, vsize
        return None
    def word(self, va):
        return struct.unpack_from('>I', self.d, self.off(va))[0]
    def bytes(self, va, n):
        o = self.off(va); return self.d[o:o+n]
    def dis(self, va, n, out=sys.stdout):
        code = self.bytes(va, n)
        for i in self.cs.disasm(code, va):
            w = struct.unpack_from('>I', self.d, self.off(i.address))[0]
            print(f'{i.address:08X}  {w:08X}  {i.mnemonic:8s} {i.op_str}', file=out)
    def text_range(self):
        for name, sva, vsize, raw, rsize in self.secs:
            if name == '.text':
                return sva, sva + vsize
    @staticmethod
    def branch_target(va, w):
        op = w >> 26
        if op == 18:
            li = w & 0x03FFFFFC
            if li & 0x02000000: li -= 0x04000000
            return (va + li) if not (w & 2) else li, w & 1
        if op == 16:
            bd = w & 0xFFFC
            if bd & 0x8000: bd -= 0x10000
            return (va + bd) if not (w & 2) else bd, w & 1
        return None, None

if __name__ == '__main__':
    im = Img(sys.argv[1])
    print(f'{im.path}: imgbase {im.imgbase:08X} entry {im.entry:08X}')
    for s in im.secs:
        print(f'  {s[0]:8s} va {s[1]:08X} vsize {s[2]:08X} raw {s[3]:08X} rsize {s[4]:08X}')

def pdata(im):
    """Yield (begin, end, flags, prolog) from the image's .pdata (BIG-ENDIAN)."""
    for name, sva, vsize, raw, rsize in im.secs:
        if name == '.pdata':
            d = im.d[raw:raw+vsize]
            for i in range(0, len(d)-7, 8):
                b, p = struct.unpack_from('>II', d, i)
                if b == 0: continue
                flags = p >> 30; ln = (p >> 8) & 0x3FFFFF; pro = p & 0xFF
                yield b, b + ln*4, flags, pro
def pdata_at(im, va):
    for b, e, f, p in pdata(im):
        if b <= va < e: return b, e, f, p
