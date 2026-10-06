#!/usr/bin/env python3
"""Word-level diff of two section-mapped Xbox 360 PEs with identical section tables.

Written for the W16-PT retarget (docs/decomp/W16PT_CLEAN_TU5_RETARGET_2026-10-06.md):
the RB3 Deluxe PE vs the clean retail TU5 PE differ in exactly 53 aligned words /
170 bytes, all inside file-backed sections, none in the PE headers.

Usage:
  tools/pe_word_diff.py <old.exe> <new.exe> [--json out.json]

Prints one line per differing 4-byte word (VA, section, old, new) and a total.
Exits 3 if the section tables or file sizes differ -- the VA mapping would not be
shared and a word diff would be meaningless.
"""
import json
import struct
import sys


def sections(d):
    e = struct.unpack_from("<I", d, 0x3C)[0]
    c = e + 4
    n = struct.unpack_from("<H", d, c + 2)[0]
    osz = struct.unpack_from("<H", d, c + 16)[0]
    base = struct.unpack_from("<I", d, c + 20 + 28)[0]
    t = c + 20 + osz
    out = []
    for i in range(n):
        o = t + i * 40
        name = d[o:o + 8].rstrip(b"\0").decode()
        vsize, va, rsize, rptr = struct.unpack_from("<IIII", d, o + 8)
        out.append((name, base + va, vsize, rptr, rsize))
    return out


def word_diff(a, b):
    sa, sb = sections(a), sections(b)
    if sa != sb or len(a) != len(b):
        return None
    words = []
    for i in range(0, len(a), 4):
        if a[i:i + 4] != b[i:i + 4]:
            sec = va = None
            for name, sva, _vs, rp, rs in sa:
                if rp <= i < rp + rs:
                    sec, va = name, sva + (i - rp)
            words.append(dict(off=i, va=va, sec=sec, old=a[i:i + 4].hex(), new=b[i:i + 4].hex()))
    return dict(n_words=len(words), n_bytes=sum(x != y for x, y in zip(a, b)), words=words)


def main(argv):
    if len(argv) < 2:
        print(__doc__, file=sys.stderr)
        return 2
    a = open(argv[0], "rb").read()
    b = open(argv[1], "rb").read()
    r = word_diff(a, b)
    if r is None:
        print("pe_word_diff: section tables or file sizes differ", file=sys.stderr)
        return 3
    for w in r["words"]:
        va = f"{w['va']:#010x}" if w["va"] is not None else f"off {w['off']:#x}"
        print(f"{va}  {w['sec'] or '-':8s} {w['old']} -> {w['new']}")
    print(f"total: {r['n_words']} words / {r['n_bytes']} bytes")
    if "--json" in argv:
        json.dump(r, open(argv[argv.index("--json") + 1], "w"), indent=1)
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv[1:]))
