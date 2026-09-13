#!/usr/bin/env python3
"""thunk_dc3_crosscheck.py -- adjudicate EVERY map-named single-branch thunk
at once, against dc3's REAL linker map.

WHY THIS TOOL EXISTS
--------------------
A 4-byte `b <dest>` thunk is RELOCATION-MASKED BYTE-IDENTICAL to every other
single-`b` thunk in the binary: the branch destination IS the relocation, so
the four bytes carry no identity at all. Every byte-identity bijection lane
that put a name on one (62098fc5 laneAK, f38d088e laneAT) was flipping a coin
over one giant equivalence class. Such a thunk can only be named by its
DESTINATION's shape plus its CALLER distribution.

THE CROSS-CHECK, which needs no prior belief about which rows are suspect:

    rb3 names N at a thunk to D, and names D as M
    dc3 names N at a thunk to D', and names D' as M'
    => M and M' are the same function, or N is on the wrong VA.

It reads no rb3 name twice -- the rb3 name under test (N) is used only to
*index into dc3*; the comparison itself is between two DESTINATION names. So a
row cannot agree with itself.

dc3's `orig/373307D9/ham_xbox_r.map` is a genuine leaked Microsoft linker map
for the same Milo engine; rb3's `scripts/target_symbol_map.json` is a
RECONSTRUCTION. dc3 therefore wins a disagreement.

TWO TRAPS THIS TOOL IS BUILT AROUND (both cost the previous lane time):

 1. A word-wise search for `b` also finds TAIL BRANCHES INSIDE BODIES. A
    function whose last instruction is `b <somewhere>` is not a thunk. The
    discriminator is a `.fn` symbol AT that address whose body is exactly one
    instruction -- which is why this reads the split `.s` and not the image.
 2. The `.s` address COLUMN is synthetic (carve-relative). The `.fn fn_<VA>`
    symbol is the only trustworthy address. See the map's own
    `_single_branch_thunk_misnames_comment`.

VERDICTS
  AGREE                dc3's destination name equals ours -- N corroborated.
  DEST_NAME_DISAGREE   both sides name the destination, and differently.
                       Either N is on the wrong VA, or our name for D is.
  OUR_DEST_UNNAMED     dc3 names the destination, we do not -- a free naming
                       opportunity, and the *only* verdict that is a gain
                       rather than a repair.
  DC3_DEST_UNNAMED     we name it, dc3 does not. No information.
  DC3_NOT_THUNK        dc3 has N, but not as a single-`b` thunk. Weak signal:
                       version skew inlines and un-inlines freely.
  NOT_IN_DC3           dc3's map has no symbol N at all. No information --
                       most rb3-only game code lands here.

Usage:
  python3 tools/thunk_dc3_crosscheck.py                 # counts + findings
  python3 tools/thunk_dc3_crosscheck.py --json out.json # full rows
  python3 tools/thunk_dc3_crosscheck.py --all           # print every row
"""
from __future__ import annotations

import argparse
import collections
import json
import os
import re
import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
BUILD_ID = "45410914"
IMAGE_BASE = 0x82000000

DC3_ROOT = ROOT.parent / "dc3-decomp" / "orig" / "373307D9"
DC3_MAP = DC3_ROOT / "ham_xbox_r.map"
DC3_EXE = DC3_ROOT / "ham_xbox_r.exe"

FN_RE = re.compile(r"^\.fn\s+fn_([0-9A-Fa-f]{8})\s*,")
ENDFN_RE = re.compile(r"^\.endfn\b")
INSN_RE = re.compile(r"^/\*\s*[0-9A-Fa-f]{8}\s+[0-9A-Fa-f]{8}\s+"
                     r"((?:[0-9A-Fa-f]{2}\s+){3}[0-9A-Fa-f]{2})\s*\*/\s*(\S+)")


class Image:
    """Minimal PE reader for a decompressed Xbox 360 image."""

    def __init__(self, path):
        self.data = open(path, "rb").read()
        pe = struct.unpack_from("<I", self.data, 0x3C)[0]
        nsec = struct.unpack_from("<H", self.data, pe + 6)[0]
        opt = struct.unpack_from("<H", self.data, pe + 20)[0]
        self.secs = []
        for i in range(nsec):
            o = pe + 24 + opt + i * 40
            name = self.data[o:o + 8].rstrip(b"\0").decode("ascii", "replace")
            vsz, va, rsz, praw = struct.unpack_from("<IIII", self.data, o + 8)
            self.secs.append((name, va, vsz, praw, rsz))

    def offset(self, va):
        rva = va - IMAGE_BASE
        for _name, sva, vsz, praw, rsz in self.secs:
            if sva <= rva < sva + max(vsz, rsz):
                return praw + (rva - sva)
        return None

    def word(self, va):
        o = self.offset(va)
        if o is None or o + 4 > len(self.data):
            return None
        return struct.unpack_from(">I", self.data, o)[0]


def branch_dest(word, va):
    """Destination of an unconditional, non-linking, relative `b`, else None.

    Primary opcode 18, AA=0 (relative), LK=0 (plain branch, not `bl`).
    Rejecting LK matters: a 4-byte `bl` body is not a forwarding thunk.
    """
    if word is None or (word >> 26) != 18:
        return None
    if word & 0x2:                      # AA -- absolute
        return None
    if word & 0x1:                      # LK -- bl
        return None
    li = word & 0x03FFFFFC
    if li & 0x02000000:                 # sign-extend the 26-bit displacement
        li -= 0x04000000
    return (va + li) & 0xFFFFFFFF


def enumerate_thunks(asm_root: Path):
    """{va: dest_va} for every `.fn` body that is exactly one relative `b`.

    Keys on the `.fn fn_<VA>` symbol, never the synthetic address column.
    """
    thunks, tails_rejected, multi = {}, 0, 0
    for path in sorted(asm_root.rglob("*.s")):
        cur_va, body = None, []
        with open(path, errors="replace") as fh:
            for line in fh:
                line = line.rstrip("\n")
                m = FN_RE.match(line)
                if m:
                    cur_va, body = int(m.group(1), 16), []
                    continue
                if cur_va is None:
                    continue
                if ENDFN_RE.match(line):
                    # Strip TRAILING alignment padding carved inside the .fn.
                    # dtk puts `.4byte 0x00000000 /* invalid */` sometimes
                    # inside the symbol and sometimes after it; a thunk is a
                    # thunk either way. Stripping only from the END cannot
                    # admit a tail branch, whose `b` is preceded by real work.
                    while body and body[-1][1] == ".4byte" and \
                            body[-1][0].replace(" ", "") == "00000000":
                        body.pop()
                    if len(body) == 1:
                        raw, mnem = body[0]
                        w = int(raw.replace(" ", ""), 16)
                        d = branch_dest(w, cur_va)
                        if d is not None:
                            thunks[cur_va] = d
                    elif body and body[-1][1] == "b":
                        tails_rejected += 1
                        multi += 1
                    cur_va, body = None, []
                    continue
                mi = INSN_RE.match(line)
                if mi:
                    body.append((mi.group(1), mi.group(2)))
    return thunks, tails_rejected


def parse_dc3_map(path: Path):
    """(byname {name: [va]}, byva {va: [name]}) from the leaked MSVC map.

    Section-0005 (.text) lines only -- a data symbol cannot be a thunk. ICF
    puts several names at one VA; keep them all, that fold is the point.
    """
    rx = re.compile(r"^\s*(\d{4}):[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]{8})\s")
    byname = collections.defaultdict(set)
    byva = collections.defaultdict(set)
    with open(path, errors="replace") as fh:
        for line in fh:
            m = rx.match(line)
            if not m:
                continue
            va = int(m.group(3), 16)
            name = m.group(2)
            byname[name].add(va)
            byva[va].add(name)
    return ({k: sorted(v) for k, v in byname.items()},
            {k: sorted(v) for k, v in byva.items()})


def load_rb3_map():
    raw = json.loads((ROOT / "scripts" / "target_symbol_map.json").read_text())
    byva, byname = {}, collections.defaultdict(list)
    for k, v in raw.items():
        if not k.startswith("0x") or not isinstance(v, str):
            continue
        va = int(k, 16)
        byva[va] = v
        byname[v].append(va)
    return byva, byname


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--json", help="write full rows here")
    ap.add_argument("--all", action="store_true", help="print every row")
    ap.add_argument("--asm", default=str(ROOT / "build" / BUILD_ID / "asm"))
    args = ap.parse_args()

    asm_root = Path(args.asm)
    if not asm_root.is_dir():
        sys.exit("no split asm at %s -- run the split first" % asm_root)
    for p in (DC3_MAP, DC3_EXE):
        if not p.is_file():
            sys.exit("missing dc3 ground truth: %s" % p)

    thunks, tails = enumerate_thunks(asm_root)
    rb3_byva, rb3_byname = load_rb3_map()
    dc3_byname, dc3_byva = parse_dc3_map(DC3_MAP)
    dc3img = Image(DC3_EXE)

    named = {va: d for va, d in thunks.items() if va in rb3_byva}

    rows, counts = [], collections.Counter()
    for va in sorted(named):
        dest = named[va]
        N = rb3_byva[va]
        M = rb3_byva.get(dest)
        row = dict(va="0x%08x" % va, name=N, dest="0x%08x" % dest, our_dest_name=M)

        dc3_addrs = dc3_byname.get(N)
        if not dc3_addrs:
            row["verdict"] = "NOT_IN_DC3"
        else:
            hits = []
            for a in dc3_addrs:
                d2 = branch_dest(dc3img.word(a), a)
                if d2 is not None:
                    hits.append((a, d2, dc3_byva.get(d2, [])))
            if not hits:
                row["verdict"] = "DC3_NOT_THUNK"
                row["dc3_addrs"] = ["0x%08x" % a for a in dc3_addrs]
            else:
                row["dc3_thunks"] = [
                    dict(at="0x%08x" % a, dest="0x%08x" % d2, dest_names=nm)
                    for a, d2, nm in hits]
                allnames = {n for _a, _d, nm in hits for n in nm}
                row["dc3_dest_names"] = sorted(allnames)
                if not allnames:
                    row["verdict"] = "DC3_DEST_UNNAMED"
                elif M is None:
                    row["verdict"] = "OUR_DEST_UNNAMED"
                elif M in allnames:
                    row["verdict"] = "AGREE"
                else:
                    row["verdict"] = "DEST_NAME_DISAGREE"
        counts[row["verdict"]] += 1
        rows.append(row)

    print("split asm                        %s" % asm_root)
    print("single-`b` .fn thunks            %d" % len(thunks))
    print("  (tail branches rejected)       %d  <- multi-insn bodies ending in `b`"
          % tails)
    print("map-named single-branch thunks   %d" % len(named))
    for v in ("AGREE", "DEST_NAME_DISAGREE", "OUR_DEST_UNNAMED",
              "DC3_DEST_UNNAMED", "DC3_NOT_THUNK", "NOT_IN_DC3"):
        print("  %-24s %5d" % (v, counts[v]))

    findings = [r for r in rows
                if r["verdict"] in ("DEST_NAME_DISAGREE", "OUR_DEST_UNNAMED")]
    if findings:
        print("\nFINDINGS (%d):" % len(findings))
        for r in findings:
            print("  %s  %s" % (r["va"], r["verdict"]))
            print("      our:  %s" % r["name"])
            print("      dest %s = %s" % (r["dest"], r["our_dest_name"]))
            print("      dc3 dest name(s): %s" % ", ".join(r["dc3_dest_names"]))
    if args.all:
        print("\nALL ROWS:")
        for r in rows:
            print("  %s %-22s %s -> %s" % (r["va"], r["verdict"], r["name"],
                                           r.get("our_dest_name")))
    if args.json:
        Path(args.json).write_text(json.dumps(rows, indent=1))
        print("\nwrote %s (%d rows)" % (args.json, len(rows)))


if __name__ == "__main__":
    main()
