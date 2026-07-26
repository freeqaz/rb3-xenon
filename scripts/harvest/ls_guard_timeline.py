#!/usr/bin/env python3
"""ls_guard_timeline.py -- read a TARGET function's function-local-static timeline.

For a given target VA (or fn_XXXXXXXX), walk the dtk-emitted .s body and print, in
guard-BIT order (== source declaration order), each function-local static: its guard
bit, the object's data address, and the string literal handed to Symbol::Symbol.

This is the "GUARD-BIT TIMELINE" read described in
docs/plans/decomp-state-2026-07-19.md TRANSFERABLE LEVERS #2.

Usage:  venv/bin/python scripts/harvest/ls_guard_timeline.py 0x825EE158 [...]
Read-only.
"""
import glob
import os
import re
import sys

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
ASM = os.path.join(REPO, "build", "45410914", "asm")

FN_HDR = re.compile(r"^\.fn (fn_[0-9A-Fa-f]+),")
LBL = re.compile(r"lbl_([0-9A-Fa-f]{8})")
COMMENT_VA = re.compile(r"^#\s+\.rdata:\S+\s+\|\s+0x([0-9A-Fa-f]{8})\s+\|")
STRLINE = re.compile(r'^\s*\.string\s+"(.*)"\s*$')

_strings = None


def strings():
    global _strings
    if _strings is None:
        _strings = {}
        for s in glob.glob(os.path.join(ASM, "**", "*.s"), recursive=True):
            cur = None
            for ln in open(s, errors="replace"):
                mc = COMMENT_VA.match(ln)
                if mc:
                    cur = int(mc.group(1), 16)
                    continue
                ms = STRLINE.match(ln)
                if ms and cur is not None:
                    _strings[cur] = ms.group(1)
                    cur = None
    return _strings


def body(fnname):
    for p in glob.glob(os.path.join(ASM, "**", "*.s"), recursive=True):
        txt = open(p, errors="ignore").read()
        tag = ".fn %s," % fnname
        if tag in txt:
            i = txt.index(tag)
            j = txt.index(".endfn", i)
            return p, txt[i:j].splitlines()
    return None, None


def analyse(va):
    fn = "fn_%08X" % int(va, 16) if va.lower().startswith("0x") else va
    path, lines = body(fn)
    if lines is None:
        fn = fn.lower().replace("fn_", "fn_")
        path, lines = body(fn)
    if lines is None:
        print("%s: not found in asm" % va)
        return
    print("== %s  (%s)" % (fn, os.path.basename(path)))
    S = strings()
    # walk: find `ori rX,rX,imm` / `oris rX,rX,imm` followed within 8 insns by a
    # `bl` whose preceding lis/addi pair names a string.
    out = []
    for i, ln in enumerate(lines):
        m = re.search(r"\b(ori|oris)\s+r(\d+), r(\d+), 0x([0-9a-fA-F]+)", ln)
        if not m:
            continue
        bit = int(m.group(4), 16) << (16 if m.group(1) == "oris" else 0)
        win = lines[i:i + 10]
        if not any("\tbl " in w for w in win):
            continue
        addrs = []
        for w in win:
            if "\tbl " in w:
                break
            for lm in LBL.finditer(w):
                addrs.append(int(lm.group(1), 16))
        sa = None
        for a in addrs:
            if a in S:
                sa = a
        obj = None
        # the static's own address: the `addi rX, rY, lbl_...` just BEFORE the guard test
        for w in lines[max(0, i - 12):i]:
            if re.search(r"\baddi\s+r\d+, r\d+, lbl_", w):
                mm = LBL.search(w)
                if mm:
                    obj = int(mm.group(1), 16)
        out.append((bit, obj, S.get(sa) if sa else None, sa))
    out.sort(key=lambda t: t[0])
    for n, (bit, obj, s, sa) in enumerate(out, 1):
        print("  #%-2d bit 0x%-8X obj %s  str %-14s (%s)" %
              (n, bit, ("0x%08X" % obj) if obj else "?", repr(s),
               ("0x%08X" % sa) if sa else "?"))
    print("  total %d local statics" % len(out))


if __name__ == "__main__":
    for a in sys.argv[1:]:
        analyse(a)
