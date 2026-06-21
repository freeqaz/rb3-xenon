#!/usr/bin/env python3
"""Find the true size of a function at a VA from band.exe .pdata.

The Xbox360 PE has a .pdata section: RUNTIME_FUNCTION array sorted by BeginAddress.
Each entry: BeginAddress (4), Flags+other (4)... actually X360 uses a 2-word format:
  DWORD BeginAddress; DWORD: bit0..1 flags, then PrologLen(8), FuncLen(22), ...
We just need the start delta to the NEXT entry's BeginAddress for size.
"""
import struct
import sys

PE = "orig/45410914/band.exe"


def load(path):
    with open(path, "rb") as f:
        data = f.read()
    e = struct.unpack_from("<I", data, 0x3C)[0]
    coff = e + 4
    num = struct.unpack_from("<H", data, coff + 2)[0]
    optsz = struct.unpack_from("<H", data, coff + 16)[0]
    opt = coff + 20
    base = struct.unpack_from("<I", data, opt + 28)[0]
    st = opt + optsz
    secs = {}
    for i in range(num):
        o = st + i * 40
        name = data[o:o + 8].rstrip(b"\x00").decode("latin1")
        va = base + struct.unpack_from("<I", data, o + 12)[0]
        vsize = struct.unpack_from("<I", data, o + 8)[0]
        rawptr = struct.unpack_from("<I", data, o + 20)[0]
        rawsize = struct.unpack_from("<I", data, o + 16)[0]
        secs[name] = (va, vsize, rawptr, rawsize)
    return data, base, secs


def pdata_starts(data, secs):
    if ".pdata" not in secs:
        return []
    va, vsize, rawptr, rawsize = secs[".pdata"]
    starts = []
    n = rawsize // 8
    for i in range(n):
        begin = struct.unpack_from(">I", data, rawptr + i * 8)[0]
        if begin:
            starts.append(begin)
    starts.sort()
    return starts


def size_of(va):
    data, base, secs = load(PE)
    starts = pdata_starts(data, secs)
    # find va in starts
    import bisect
    idx = bisect.bisect_left(starts, va)
    if idx < len(starts) and starts[idx] == va:
        if idx + 1 < len(starts):
            return starts[idx + 1] - va
        return None
    # not exactly a pdata start: report nearest
    near = starts[idx] if idx < len(starts) else None
    prev = starts[idx - 1] if idx > 0 else None
    return ("NOEXACT", prev, near)


if __name__ == "__main__":
    for a in sys.argv[1:]:
        va = int(a, 16)
        print(f"0x{va:08x} -> {size_of(va)}")
