#!/usr/bin/env python3
"""Section-mapped VA reader/disassembler for the TU5 (v0.0.5.1) RB3 PE.

TU5's XEX is "basic"-format: the loaded image is SECTION-MAPPED, so the flat
`0x3000 + (VA - image_base)` offset used against the TU0 base xex DRIFTS and
yields garbage. This tool maps a VA -> correct file offset through the PE
section table (COFF headers) and disassembles N bytes as PPC32 big-endian
(capstone), exactly like tools/va_disasm.py but pointed at band_tu5.exe.

Usage:
  tools/tu5_va.py 0x8283cd20 8          # disassemble N bytes at VA (default 64)
  tools/tu5_va.py --sections            # dump the PE section table
  tools/tu5_va.py --raw 0x8283cd20 8    # hex-dump raw bytes (no disasm)
  tools/tu5_va.py --pe path 0x... N     # override the PE (defaults to band_tu5.exe)

Import surface (reused by skel_match / map builders):
  load_sections(path) -> (data, image_base, secs[(name, sva, vsize, rawptr, rawsize)])
  va_to_off(va, secs) -> (file_off, section_name)
"""
import os
import struct
import sys

# Default PE = the TU5 section-mapped image. Resolve relative to repo root so the
# tool works from any cwd inside the worktree.
_HERE = os.path.dirname(os.path.abspath(__file__))
_ROOT = os.path.dirname(_HERE)
PE = os.path.join(_ROOT, "orig", "45410914", "band_tu5.exe")


def load_sections(path):
    with open(path, "rb") as f:
        data = f.read()
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    assert data[e_lfanew:e_lfanew + 4] == b"PE\x00\x00", "not PE"
    coff = e_lfanew + 4
    num_sec = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    # ImageBase is little-endian in the optional header regardless of target.
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


def dump_sections(path=PE):
    data, base, secs = load_sections(path)
    print(f"# PE {path}")
    print(f"# image_base 0x{base:08x}")
    print(f"# {'name':<10} {'VA':<12} {'vsize':<10} {'rawptr':<12} {'rawsize':<10}")
    for name, sva, vsize, rawptr, rawsize in secs:
        print(f"  {name:<10} 0x{sva:08x}   0x{vsize:<8x} 0x{rawptr:<10x} 0x{rawsize:x}")


def disasm(va, n, path=PE, raw=False):
    data, base, secs = load_sections(path)
    off, secname = va_to_off(va, secs)
    if off is None:
        print(f"VA 0x{va:08x} not in any mapped section (base 0x{base:08x})")
        for s in secs:
            print("  ", s[0], hex(s[1]), "vsize", hex(s[2]))
        return
    code = data[off:off + n]
    if raw:
        print(f"# VA 0x{va:08x} in section {secname}, file_off 0x{off:x}, {n} bytes")
        for i in range(0, len(code), 4):
            w = code[i:i + 4]
            print(f"/* {va + i:08x} */ {w.hex().upper()}")
        return
    import capstone
    md = capstone.Cs(capstone.CS_ARCH_PPC, capstone.CS_MODE_BIG_ENDIAN | capstone.CS_MODE_32)
    md.detail = False
    print(f"# VA 0x{va:08x} in section {secname}, file_off 0x{off:x}, {n} bytes")
    for ins in md.disasm(code, va):
        print(f"/* {ins.address:08x} {ins.bytes.hex().upper():8} */\t{ins.mnemonic}\t{ins.op_str}")


if __name__ == "__main__":
    args = sys.argv[1:]
    pe = PE
    raw = False
    rest = []
    i = 0
    while i < len(args):
        a = args[i]
        if a == "--pe":
            pe = args[i + 1]; i += 2; continue
        if a == "--raw":
            raw = True; i += 1; continue
        if a == "--sections":
            dump_sections(pe); sys.exit(0)
        rest.append(a); i += 1
    if not rest:
        print(__doc__); sys.exit(2)
    va = int(rest[0], 16)
    n = int(rest[1]) if len(rest) > 1 else 64
    disasm(va, n, path=pe, raw=raw)
