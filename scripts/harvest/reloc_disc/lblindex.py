#!/usr/bin/env python3
"""Build a global `lbl_<VA> -> raw bytes` index from the dtk data asm listings.

Used to resolve a BASE-side relocation whose symbol is a content-carrying COMDAT
(`??_C@_0..` string literal, `__real@..` / `__realdb@..` FP constant) against the
TARGET-side `lbl_<VA>` operand: decode the mangled name to bytes and compare with
the bytes actually living at that VA.
"""
import json
import re
import struct
import sys
from pathlib import Path

OBJ_RX = re.compile(r"^\.obj (lbl_[0-9A-Fa-f]{8}|\S+),")
END_RX = re.compile(r"^\.endobj")

ESC = {"n": b"\n", "t": b"\t", "r": b"\r", "0": b"\0", "\\": b"\\", '"': b'"'}


def unescape(s: str) -> bytes:
    out = bytearray()
    i = 0
    while i < len(s):
        c = s[i]
        if c == "\\" and i + 1 < len(s):
            n = s[i + 1]
            if n in ESC:
                out += ESC[n]
                i += 2
                continue
            if n == "x":
                j = i + 2
                h = ""
                while j < len(s) and len(h) < 2 and s[j] in "0123456789abcdefABCDEF":
                    h += s[j]
                    j += 1
                out.append(int(h, 16))
                i = j
                continue
            if n.isdigit():
                j = i + 1
                o = ""
                while j < len(s) and len(o) < 3 and s[j] in "01234567":
                    o += s[j]
                    j += 1
                out.append(int(o, 8) & 0xFF)
                i = j
                continue
        out += c.encode("latin1", "replace")
        i += 1
    return bytes(out)


def build(worktree: Path, out_path: Path):
    asmdir = worktree / "build/45410914/asm"
    idx = {}
    files = [p for p in sorted(asmdir.glob("*.s"))]
    for p in files:
        txt = p.read_text(errors="replace")
        if ".obj lbl_" not in txt:
            continue
        cur = None
        buf = bytearray()
        for line in txt.splitlines():
            m = OBJ_RX.match(line)
            if m:
                cur = m.group(1)
                buf = bytearray()
                continue
            if END_RX.match(line):
                if cur and cur.startswith("lbl_") and buf:
                    va = int(cur[4:], 16)
                    idx.setdefault(va, bytes(buf[:64]).hex())
                cur = None
                continue
            if cur is None:
                continue
            ls = line.strip()
            try:
                if ls.startswith('.string16 "'):
                    s = ls[len('.string16 "'):].rsplit('"', 1)[0]
                    b = unescape(s)
                    buf += b"".join(struct.pack(">H", c) for c in b) + b"\0\0"
                elif ls.startswith('.string "'):
                    s = ls[len('.string "'):].rsplit('"', 1)[0]
                    buf += unescape(s) + b"\0"
                elif ls.startswith(".4byte"):
                    v = ls.split(None, 1)[1].split("/*")[0].strip()
                    if v.startswith(("0x", "0X")):
                        buf += struct.pack(">I", int(v, 16) & 0xFFFFFFFF)
                    else:
                        buf += b"\xff\xff\xff\xff"      # relocated pointer
                elif ls.startswith(".2byte"):
                    v = ls.split(None, 1)[1].split("/*")[0].strip()
                    buf += struct.pack(">H", int(v, 16) & 0xFFFF)
                elif ls.startswith(".byte"):
                    for v in ls.split(None, 1)[1].split("/*")[0].split(","):
                        buf.append(int(v.strip(), 16) & 0xFF)
                elif ls.startswith(".float"):
                    buf += struct.pack(">f", float(ls.split(None, 1)[1].split("/*")[0]))
                elif ls.startswith(".double"):
                    buf += struct.pack(">d", float(ls.split(None, 1)[1].split("/*")[0]))
                elif ls.startswith(".8byte"):
                    v = ls.split(None, 1)[1].split("/*")[0].strip()
                    buf += struct.pack(">Q", int(v, 16) & 0xFFFFFFFFFFFFFFFF)
                elif ls.startswith(".space") or ls.startswith(".skip"):
                    n = int(ls.split(None, 1)[1].split("/*")[0].strip(), 0)
                    buf += b"\0" * min(n, 64)
            except Exception:
                pass
            if len(buf) > 96:
                # long enough for any comparison we do
                pass
        # end file
    out_path.write_text(json.dumps(idx))
    print(f"lbl index: {len(idx)} entries -> {out_path}", file=sys.stderr)


if __name__ == "__main__":
    build(Path(sys.argv[1]), Path(sys.argv[2]))
