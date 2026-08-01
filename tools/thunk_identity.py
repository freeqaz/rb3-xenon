#!/usr/bin/env python3
"""Adjudicate vtordisp/adjustor thunk names by what they BRANCH TO.

Why this exists
---------------
The usual decisive standard for a map repair -- paired body size read back
from report.json -- is STRUCTURALLY INAPPLICABLE to vtordisp thunks. Every
`$4PPPPPPPM@` thunk in retail is 12 bytes (report.json population: 1436@12,
539@16, 1@56, 1@8). Comparing 12 against 12 passes no matter which name sits
on which row, so that check cannot fail and verifies nothing.

A thunk's identity is not its vtable slot and not its size -- it is its branch
target. These stubs are straight-line code terminating in an unconditional `b`
(three words, or FOUR when a nonzero static adjustment adds an `addi`), so the
target decodes straight out of retail band.exe. The target body's plain
(non-thunk) map row then names the method, and the thunk's name must agree
with it.

THREE earlier revisions of this check were VACUOUS; all failure modes are
guarded against here, and the anti-vacuity control below is not optional.

  v1  followed "the first branch found within 24 bytes" with no stopping
      predicate. Once it reached a real body it followed a branch *inside*
      that body; 26 of 29 rows converged on one trampoline region. A
      recursive follower needs a TERMINATION TEST, not a depth limit.

  v2  used .pdata extents to decide "is this a stub?". 12-byte thunks have no
      .pdata record at all, so it bailed at hop 0, reported the thunk as its
      own body, and produced AGREE=0/DISAGREE=29 -- the null answer merely
      restating old != new. (.pdata-absence is not a "not a function" test.)

  v3  decoded only THREE words, so the terminal `b` of every 16-byte thunk
      (at word index 3) was missed -- 539 rows, exactly report.json's 539@16
      population, silently reported "no-branch" and were excluded from every
      sweep. The symptom was visible as a large no-branch count in the control
      line and was read as noise rather than as a structural blind spot.
      See thunk_target() for the fix and its superset validation.

This version decodes up to four words directly and needs no extent source.

Anti-vacuity control
--------------------
`--control N` runs the same instrument over N randomly chosen $4 thunks the
caller is NOT proposing to change. A trustworthy run shows a healthy mix of
AGREE and DISAGREE. If the control returns all-AGREE the instrument is
rubber-stamping; if it returns all-DISAGREE it is miscalibrated. Either way
its verdict on the proposed rows is worthless. Measured baseline on 60
untouched thunks: 27 AGREE / 10 DISAGREE / 1 body-unnamed / 22 no-branch.

Usage
-----
  tools/thunk_identity.py --audit                 # sweep every $4 row in the map
  tools/thunk_identity.py --patch p.patch         # adjudicate a proposed patch
  tools/thunk_identity.py --addrs 0x8231a950,...  # specific rows
  tools/thunk_identity.py --audit --control 60 --json out.json
"""

import argparse
import collections
import json
import random
import struct
import sys

IMAGE_BASE = 0x82000000
DEFAULT_EXE = "orig/45410914/band.exe"
DEFAULT_MAP = "scripts/target_symbol_map.json"
THUNK_MARK = "$4PPPPPPPM@"


class Image:
    """Minimal PE reader for the decompressed retail band.exe."""

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


def _is_branch(w):
    # PowerPC primary opcode 18 == b / bl / ba / bla
    return w is not None and (w >> 26) == 18


def _branch_target(img, va):
    w = img.word(va)
    li = w & 0x03FFFFFC
    if li & 0x02000000:          # sign-extend the 26-bit displacement
        li -= 0x04000000
    absolute = (w >> 1) & 1      # AA bit
    return (li if absolute else va + li) & 0xFFFFFFFF


def thunk_target(img, va):
    """Return (target_va, form) for a stub at va, or (None, reason).

    A $4 vtordisp thunk is three or FOUR words ending in `b`:
        lwz   r11, -4(rN)
        subf  rN, r11, rN
        addi  rN, rN, -adj     <-- present only when the static adjustment != 0
        b     target

    v3 fixed two defects in the v2 window (lane CF-5):

    ** The 4-word form was invisible.** The window was three words wide and the
    16-byte thunks put their terminal `b` at word index 3, so the ENTIRE 16-byte
    family -- 539 rows, matching report.json's 539@16 population exactly -- came
    back "no-branch" and was silently excluded from every sweep. Measured: 539 of
    the 540 no-branch rows have a branch at word 3.

    ** "Prefer the longest form" could OVER-READ.** Scanning i=2,1,0 and taking
    the last branch means a genuine 12-byte thunk whose successor happens to
    begin with a branch is decoded against the NEXT function's first word. Since
    a thunk is straight-line code terminating in `b`, the FIRST branch is by
    construction the terminal one, and it can never read past the end. Widening
    to 4 words under prefer-longest flipped one row's verdict; under first-branch
    it flips none.

    Validated as a pure superset before adoption: over the 1,439 rows the v2
    window could resolve at all, first-branch-over-4-words yields an IDENTICAL
    target and verdict for every one (0 differences), and additionally resolves
    the 539 it could not see.
    """
    for i in range(4):
        w = img.word(va + 4 * i)
        if _is_branch(w):
            return _branch_target(img, va + 4 * i), f"{i + 1}w"
    return None, "no-branch"


def method_sig(name):
    """'?Method@Class@@...' -> ('Method', 'Class'); None if not a mangled member."""
    if not isinstance(name, str) or not name.startswith("?"):
        return None
    parts = name[1:].split("@@")[0].split("@")
    return (parts[0], parts[1]) if len(parts) >= 2 else None


def adjudicate(img, tmap, addr, proposed):
    """Compare a proposed name for `addr` against its branch target's row."""
    va = int(addr, 16)
    target, form = thunk_target(img, va)
    if target is None:
        return {"addr": addr, "verdict": "NO_BRANCH", "form": form}
    row = tmap.get("0x%08x" % target)
    rec = {"addr": addr, "target": "0x%08x" % target, "form": form,
           "target_row": row, "proposed": proposed}
    if row is None:
        rec["verdict"] = "TARGET_UNNAMED"
    elif method_sig(row) == method_sig(proposed):
        rec["verdict"] = "AGREE"
    else:
        rec["verdict"] = "DISAGREE"
    return rec


def parse_patch(path):
    """Pull the '+' side of a target_symbol_map.json patch into {addr: name}."""
    out = {}
    for line in open(path):
        if line.startswith("+ ") and '": "' in line:
            a, n = line[2:].strip().rstrip(",").split('": "')
            out[a.strip().strip('"')] = n.rstrip('"')
    return out


def summarize(records, label):
    c = collections.Counter(r["verdict"] for r in records)
    print("%-38s AGREE=%-5d DISAGREE=%-5d unnamed=%-5d no-branch=%d"
          % (label, c["AGREE"], c["DISAGREE"], c["TARGET_UNNAMED"], c["NO_BRANCH"]))
    return c


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", default=DEFAULT_EXE)
    ap.add_argument("--map", dest="tmap", default=DEFAULT_MAP)
    ap.add_argument("--patch", help="adjudicate the + side of a map patch")
    ap.add_argument("--addrs", help="comma-separated thunk addresses to check")
    ap.add_argument("--audit", action="store_true",
                    help="sweep every $4 row currently in the map")
    ap.add_argument("--control", type=int, default=60, metavar="N",
                    help="anti-vacuity control over N untouched $4 rows (0 disables)")
    ap.add_argument("--seed", type=int, default=7)
    ap.add_argument("--json", help="write full records here")
    ap.add_argument("-v", "--verbose", action="store_true")
    args = ap.parse_args()

    img = Image(args.exe)
    tmap = json.load(open(args.tmap))

    if args.patch:
        proposals = parse_patch(args.patch)
    elif args.addrs:
        proposals = {a.strip(): tmap.get(a.strip())
                     for a in args.addrs.split(",") if a.strip()}
    elif args.audit:
        proposals = {a: n for a, n in tmap.items()
                     if isinstance(n, str) and THUNK_MARK in n}
    else:
        ap.error("need one of --patch / --addrs / --audit")

    records = [adjudicate(img, tmap, a, n) for a, n in sorted(proposals.items())]

    if args.verbose or args.addrs:
        for r in records:
            print("%s [%s] -> %s  %-46s %s"
                  % (r["addr"], r.get("form", "-"), r.get("target", "-"),
                     str(r.get("target_row"))[:44], r["verdict"]))
        print()

    summarize(records, "PROPOSED")

    # --- anti-vacuity control -------------------------------------------------
    if args.control:
        pool = [a for a, n in tmap.items()
                if isinstance(n, str) and THUNK_MARK in n and a not in proposals]
        random.seed(args.seed)
        random.shuffle(pool)
        sample = pool[:args.control]
        ctl = [adjudicate(img, tmap, a, tmap[a]) for a in sample]
        c = summarize(ctl, "CONTROL (untouched $4 rows)")
        # An EMPTY control is the most dangerous outcome of all: it prints a
        # control line, every counter reads 0, and the all-AGREE / all-DISAGREE
        # guards below are both False on zeros -- so the tool exits 0 and the run
        # LOOKS controlled. This is exactly what --audit did (lane CF-5): audit
        # proposes every $4 row, so the pool (rows NOT proposed) is the empty set,
        # and `--audit --control 60` sampled 0 rows and passed. A control that
        # cannot fail is not a control.
        if not sample:
            print("\n*** CONTROL IS EMPTY (%d rows available outside the proposed "
                  "set): the anti-vacuity check made NO observation, so it cannot "
                  "fail and this run is uncontrolled. In --audit mode every $4 row "
                  "is proposed, which leaves no pool by construction."
                  % len(pool), file=sys.stderr)
            return 2
        if c["AGREE"] and not c["DISAGREE"]:
            print("\n*** CONTROL IS VACUOUS: all-AGREE. The instrument cannot "
                  "fail here, so its verdict on the proposed rows means nothing.",
                  file=sys.stderr)
            return 2
        if c["DISAGREE"] and not c["AGREE"]:
            print("\n*** CONTROL MISCALIBRATED: all-DISAGREE on untouched rows.",
                  file=sys.stderr)
            return 2

    if args.json:
        json.dump(records, open(args.json, "w"), indent=1)
        print("wrote %s (%d records)" % (args.json, len(records)))
    return 0


if __name__ == "__main__":
    sys.exit(main())
