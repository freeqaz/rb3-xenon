#!/usr/bin/env python3
"""Disassemble a VA range from band.exe (decompressed RB3 PE) using capstone.

Ghidra-independent body inspection for the STEP-1 recon-gate. Reads the PE
section table to map VA->file offset, then disassembles N bytes (PPC32 BE).
"""
import struct
import sys

import capstone

PE = "orig/45410914/band.exe"


def load_sections(path):
    with open(path, "rb") as f:
        data = f.read()
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[e_lfanew:e_lfanew + 4] == b"PE\x00\x00", "not PE"
    coff = e_lfanew + 4
    num_sec = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    image_base = struct.unpack_from(">I", data, opt + 28)[0]  # PPC PE: BE? try both
    # ImageBase is stored little-endian in the optional header regardless of target
    image_base = struct.unpack_from("<I", data, opt + 28)[0]
    sec_tab = opt + opt_size
    secs = []
    for i in range(num_sec):
        off = sec_tab + i * 40
        name = data[off:off + 8].rstrip(b"\x00").decode("latin1")
        vsize = struct.unpack_from("<I", data, off + 8)[0]
        vaddr = struct.unpack_from("<I", data, off + 12)[0]
        rawsize = struct.unpack_from("<I", data, off + 16)[0]
        rawptr = struct.unpack_from("<I", data, off + 20)[0]
        secs.append((name, image_base + vaddr, vsize, rawptr, rawsize))
    return data, image_base, secs


def va_to_off(va, secs):
    for name, sva, vsize, rawptr, rawsize in secs:
        if sva <= va < sva + vsize:
            delta = va - sva
            if delta < rawsize:
                return rawptr + delta, name
    return None, None


def disasm(va, n):
    data, base, secs = load_sections(PE)
    off, secname = va_to_off(va, secs)
    if off is None:
        print(f"VA 0x{va:08x} not in any section (base 0x{base:08x})")
        for s in secs:
            print("  ", s[0], hex(s[1]), "vsize", hex(s[2]))
        return
    code = data[off:off + n]
    md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_BIG_ENDIAN | capstone.CS_MODE_32)
    md.detail = False
    print(f"# VA 0x{va:08x} in section {secname}, {n} bytes")
    for ins in md.disasm(code, va):
        print(f"/* {ins.address:08x} {ins.bytes.hex().upper():8} */\t{ins.mnemonic}\t{ins.op_str}")


if __name__ == "__main__":
    va = int(sys.argv[1], 16)
    n = int(sys.argv[2]) if len(sys.argv) > 2 else 64
    disasm(va, n)
