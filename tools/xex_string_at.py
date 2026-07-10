#!/usr/bin/env python3
"""xex_string_at.py — read a C string (or raw bytes) at a retail virtual address.

The retail RB3 XEX (title 45410914) decodes to an ordinary big-endian PowerPC
PE32 image which jeff/dtk already extracts to `orig/45410914/band.exe`
(`dtk xex extract`). This tool parses that PE's section table, maps a loader
virtual address (VA, e.g. 0x825C8058) to a file offset, and prints the
NUL-terminated ASCII string that lives there — the reliable path for reading
retail .rdata/.data symbol-token strings.

Why not the Ghidra MCP `read_bytes`? For these .rdata addresses it returned
obfuscated/wrong bytes (the decompiler renders the strings fine, so the data is
present, but the raw-read endpoint was serving a different/relocated view). The
extracted PE is ground truth: VA -> RVA (VA - ImageBase) -> file offset via the
section whose [VirtualAddress, VirtualAddress+VirtualSize) range contains the RVA.

Usage:
    tools/xex_string_at.py 0x825C8058             # print C string at VA
    tools/xex_string_at.py 0x825C8058 --len 64    # hexdump 64 raw bytes at VA
    tools/xex_string_at.py 0x825C8058 --exe path/to/band.exe

Verified against known .rdata token strings referenced by matched functions.
"""
import argparse
import os
import struct
import sys

DEFAULT_EXE = os.path.join(
    os.path.dirname(os.path.abspath(__file__)),
    "..", "orig", "45410914", "band.exe",
)


def load_sections(path):
    with open(path, "rb") as f:
        data = f.read()
    if data[:2] != b"MZ":
        raise ValueError("not a PE (missing MZ)")
    pe_off = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe_off:pe_off + 4] != b"PE\0\0":
        raise ValueError("bad PE signature")
    coff = pe_off + 4
    num_sections = struct.unpack_from("<H", data, coff + 2)[0]
    opt_size = struct.unpack_from("<H", data, coff + 16)[0]
    opt_off = coff + 20
    # ImageBase: PE32 magic 0x10b -> offset 28 in optional header (32-bit field).
    magic = struct.unpack_from("<H", data, opt_off)[0]
    if magic == 0x20B:  # PE32+
        image_base = struct.unpack_from("<Q", data, opt_off + 24)[0]
    else:               # PE32
        image_base = struct.unpack_from("<I", data, opt_off + 28)[0]
    sec_off = opt_off + opt_size
    sections = []
    for i in range(num_sections):
        base = sec_off + i * 40
        name = data[base:base + 8].rstrip(b"\0").decode("latin1")
        vsize, vaddr, rawsize, rawptr = struct.unpack_from("<IIII", data, base + 8)
        sections.append((name, vaddr, vsize, rawptr, rawsize))
    return data, image_base, sections


def va_to_offset(va, image_base, sections):
    rva = va - image_base
    for name, vaddr, vsize, rawptr, rawsize in sections:
        if vaddr <= rva < vaddr + max(vsize, rawsize):
            file_off = rawptr + (rva - vaddr)
            return file_off, name
    return None, None


def read_cstring(data, off, max_len=4096):
    end = data.find(b"\0", off, off + max_len)
    if end < 0:
        end = off + max_len
    return data[off:end]


def main():
    ap = argparse.ArgumentParser(description="Read a C string at a retail VA.")
    ap.add_argument("va", help="virtual address (hex, e.g. 0x825C8058)")
    ap.add_argument("--exe", default=DEFAULT_EXE, help="extracted PE basefile")
    ap.add_argument("--len", type=int, default=0,
                    help="if >0, hexdump N raw bytes instead of a C string")
    args = ap.parse_args()

    va = int(args.va, 0)
    data, image_base, sections = load_sections(args.exe)
    off, sect = va_to_offset(va, image_base, sections)
    if off is None:
        print(f"VA {va:#x} not in any section", file=sys.stderr)
        return 1

    if args.len > 0:
        chunk = data[off:off + args.len]
        for i in range(0, len(chunk), 16):
            row = chunk[i:i + 16]
            hexs = " ".join(f"{b:02x}" for b in row)
            ascii_ = "".join(chr(b) if 32 <= b < 127 else "." for b in row)
            print(f"{va + i:08x}  {hexs:<47}  {ascii_}")
    else:
        s = read_cstring(data, off)
        print(s.decode("latin1"))
    return 0


if __name__ == "__main__":
    sys.exit(main())
