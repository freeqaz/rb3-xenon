#!/usr/bin/env python3
"""Clang launcher supplying ELF --wrap semantics for rb3-app's Mach-O objects."""

import struct
import subprocess
import sys
from pathlib import Path

# mach-o/loader.h and mach-o/nlist.h, for thin little-endian Mach-O 64-bit objects.
MH_MAGIC_64 = 0xFEEDFACF
MH_OBJECT = 1
LC_SYMTAB = 2
N_UNDEFINED_EXTERNAL = 0x01


def redirect_symbols(path, redirects):
    data = bytearray(path.read_bytes())
    magic, _, _, filetype, command_count = struct.unpack_from("<5I", data)
    if magic != MH_MAGIC_64 or filetype != MH_OBJECT:
        raise ValueError(f"Expected a thin 64-bit Mach-O object: {path}")
    command = 32
    for _ in range(command_count):
        kind, size = struct.unpack_from("<2I", data, command)
        if kind == LC_SYMTAB:
            break
        command += size
    else:
        return
    symoff, count, stroff, strsize = struct.unpack_from("<4I", data, command + 8)
    if stroff + strsize != len(data):
        raise ValueError(f"Expected the string table at the end of {path}")

    # nlist_64 entries keep their order, so relocation symbol indices still apply.
    added = {}
    for index in range(count):
        entry = symoff + index * 16
        string_index, kind = struct.unpack_from("<IB", data, entry)
        if kind != N_UNDEFINED_EXTERNAL:
            continue
        start = stroff + string_index
        name = bytes(data[start:data.index(0, start, stroff + strsize)])
        replacement = redirects.get(name)
        if replacement is None:
            continue
        if replacement not in added:
            added[replacement] = len(data) - stroff
            data.extend(replacement + b"\0")
        struct.pack_into("<I", data, entry, added[replacement])

    if added:
        data.extend(b"\0" * (-len(data) % 8))
        struct.pack_into("<I", data, command + 20, len(data) - stroff)
        path.write_bytes(data)


def main(args):
    separator = args.index("--")
    compile_command = args[separator + 1:]
    result = subprocess.run(compile_command)
    if result.returncode:
        sys.exit(result.returncode)
    redirects = {}
    for spec in args[:separator]:
        name, _, actual = spec.partition("=")
        original = ("_" + (actual or name)).encode()
        redirects[original] = ("___wrap_" + name).encode()
        redirects[("___real_" + name).encode()] = original
    path = Path(compile_command[compile_command.index("-o") + 1])
    try:
        redirect_symbols(path, redirects)
    except Exception:
        path.unlink(missing_ok=True)
        raise


if __name__ == "__main__":
    main(sys.argv[1:])
