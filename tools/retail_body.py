#!/usr/bin/env python3
"""Read a retail .text body: bl callees (annotated with target_symbol_map names),
li/lis+addi immediates, and any .rdata string operands it forms.

WHY (lane W9-FALSECREDIT, 2026-08-17): adjudicating a map defect needs evidence
that does NOT read the map, or it is a fixed-point problem rather than a proof.
The retail BYTES are that evidence -- an allocation immediate, a comparator
being signed vs unsigned, an .rdata string operand. This is the reader for them.
Companion to tools/retail_callers.py (which answers "who calls this address").

Usage:
    python3 tools/retail_body.py <hexva> [size]          # annotated decode
    python3 tools/retail_body.py <hexva> <size> --dis    # real capstone PPC32-BE

Run from the repo root (or set RB3_ROOT). Also importable: `Img` gives
read()/cstr()/secname() over the PE by virtual address.

NOTE: scripts/target_symbol_map.json carries non-address keys (e.g.
`_bijection_arbitrary`); always filter on k.startswith("0x") before int().
"""
import json
import os
import struct
import sys

ROOT = os.environ.get("RB3_ROOT", ".")
PE = os.path.join(ROOT, "orig/45410914/band.exe")
MAP = os.path.join(ROOT, "scripts/target_symbol_map.json")


def sections(buf):
    pe_off = struct.unpack_from("<I", buf, 0x3C)[0]
    assert buf[pe_off:pe_off + 4] == b"PE\0\0"
    coff = pe_off + 4
    nsec = struct.unpack_from("<H", buf, coff + 2)[0]
    optsz = struct.unpack_from("<H", buf, coff + 16)[0]
    opt = coff + 20
    imgbase = struct.unpack_from("<I", buf, opt + 28)[0]
    secs = []
    off = opt + optsz
    for _ in range(nsec):
        raw = buf[off:off + 40]
        name = raw[:8].rstrip(b"\0").decode("latin1")
        vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", raw, 8)
        secs.append((name, imgbase + vaddr, vsize, rawptr, rawsize))
        off += 40
    return imgbase, secs


class Img:
    def __init__(self):
        self.buf = open(PE, "rb").read()
        _, self.secs = sections(self.buf)

    def read(self, va, n):
        for name, vaddr, vsize, rawptr, rawsize in self.secs:
            if vaddr <= va < vaddr + vsize:
                off = rawptr + (va - vaddr)
                return self.buf[off:off + n]
        return b""

    def secname(self, va):
        for name, vaddr, vsize, rawptr, rawsize in self.secs:
            if vaddr <= va < vaddr + vsize:
                return name
        return "?"

    def cstr(self, va, maxn=96):
        b = self.read(va, maxn)
        if not b:
            return None
        z = b.split(b"\0")[0]
        return z.decode("latin1", "replace")


def disasm(va, size, smap, img):
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_PPC,
                     capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
    for ins in md.disasm(img.read(va, size), va):
        ann = ""
        if ins.mnemonic in ("bl", "b", "ba", "bla"):
            try:
                t = int(ins.op_str.strip(), 0)
                if t in smap:
                    ann = "   ; " + smap[t][:110]
            except ValueError:
                pass
        print(f"{ins.address:08x}  {ins.mnemonic:<8} {ins.op_str}{ann}")


def main():
    va = int(sys.argv[1], 16)
    size = int(sys.argv[2], 0) if len(sys.argv) > 2 and not sys.argv[2].startswith("-") else 0x200
    img = Img()
    smap = {int(k, 16): v for k, v in json.load(open(MAP)).items() if v and k.startswith('0x')}
    if "--dis" in sys.argv:
        return disasm(va, size, smap, img)
    data = img.read(va, size)
    hi = {}
    for i in range(0, len(data) // 4 * 4, 4):
        w = struct.unpack_from(">I", data, i)[0]
        pc = va + i
        op = w >> 26
        line = f"  {pc:08x}  {w:08x}  "
        if op == 18:
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            dest = li if ((w >> 1) & 1) else pc + li
            kind = "bl" if (w & 1) else "b "
            nm = smap.get(dest, "")
            line += f"{kind}    0x{dest:08x}  {nm}"
        elif op == 15:  # addis
            rd = (w >> 21) & 31
            ra = (w >> 16) & 31
            imm = w & 0xFFFF
            if ra == 0:
                hi[rd] = imm << 16
                line += f"lis   r{rd}, 0x{imm:04x}"
            else:
                line += f"addis r{rd}, r{ra}, 0x{imm:04x}"
        elif op == 14:  # addi
            rd = (w >> 21) & 31
            ra = (w >> 16) & 31
            imm = w & 0xFFFF
            if imm & 0x8000:
                imm -= 0x10000
            if ra == 0:
                line += f"li    r{rd}, 0x{imm:x} ({imm})"
            else:
                ea = hi.get(ra)
                extra = ""
                if ea is not None:
                    tgt = (ea + imm) & 0xFFFFFFFF
                    s = img.cstr(tgt)
                    extra = f"   -> 0x{tgt:08x} [{img.secname(tgt)}]"
                    if s and s.isprintable() and len(s) > 2:
                        extra += f'  "{s}"'
                line += f"addi  r{rd}, r{ra}, {imm}{extra}"
        elif op == 32:  # lwz
            rd = (w >> 21) & 31
            ra = (w >> 16) & 31
            imm = w & 0xFFFF
            if imm & 0x8000:
                imm -= 0x10000
            extra = ""
            ea = hi.get(ra)
            if ea is not None and ra != 1:
                tgt = (ea + imm) & 0xFFFFFFFF
                extra = f"   -> 0x{tgt:08x} [{img.secname(tgt)}]"
            line += f"lwz   r{rd}, {imm}(r{ra}){extra}"
        elif op == 36:  # stw
            rs = (w >> 21) & 31
            ra = (w >> 16) & 31
            imm = w & 0xFFFF
            if imm & 0x8000:
                imm -= 0x10000
            line += f"stw   r{rs}, {imm}(r{ra})"
        elif w == 0x4E800020:
            line += "blr"
        else:
            line += f"op{op}"
        print(line)


if __name__ == "__main__":
    main()
