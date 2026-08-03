#!/usr/bin/env python3
"""pdata_map_audit -- retail .pdata function-extent table + a PROVABLY-FALSE
map-row detector built on it.  Read-only; not a build input.

Provenance: lane DK-4 (2026-08-03), promoted from ~/tmp/laneDK4/.

WHY .pdata
----------
`fingerprints.json`'s function list is INCOMPLETE -- 67,285 entries against
report.json's 69,353, with multi-KB gaps (nothing between 0x827af160 and
0x827b1938).  Using it for function extents silently yields size 0 and wrong
block ends.  The retail .pdata RUNTIME_FUNCTION table is authoritative for
every non-leaf function.

X360/PPC RUNTIME_FUNCTION, 8 bytes, big-endian:
    DWORD BeginAddress
    DWORD PrologLen:8 (bits 0-7) | FunctionLen:22 (bits 8-29) | 32bit:1 | EH:1
FunctionLen counts INSTRUCTIONS => bytes = FunctionLen * 4.
  ! The first decode written for this agreed with fingerprints on 0 of 55,999
    addresses using (w>>2); (w>>8) agrees on 100%.  The control INVERTED the
    assumption rather than merely confirming it -- which is why `--selftest`
    keeps the wrong shift as a live sabotage leg.

THE DETECTOR
------------
A RUNTIME_FUNCTION extent covers exactly one function, so a map address x with
    f.start < x < f.start + f.len   and   x not itself a .pdata start
cannot be a function start.  In practice it is an INLINED call site that was
mapped as though it were a standalone body.  Worked instance: DirectInstrument's
four accessors mapped at 0x826c48f8/4908/4910/4958, all strictly inside
?Handle@GemPlayer@@ [0x826c44f8,0x826c5ae4) -- the compiler inlined them.

  * The verdict is NON-METRIC and map-independent: .pdata is retail's own table.
  * SUFFICIENT, never NECESSARY (INSTRUMENT_DESIGN rule 8).  A leaf function has
    no .pdata entry, so rows landing in .pdata GAPS are reported separately and
    NOT judged.
  * Excluded false-positive class: the PPC CRT register save/restore CHAINS
    (__savefpr_N / __restfpr_N / __savegpr_N / __restvmx_N ...) legitimately
    expose one entry label per register inside a SINGLE .pdata range.  34 of the
    first 42 raw hits were these; they are real entry points, not mismaps.

Usage:
    python3 tools/pdata_map_audit.py --selftest
    python3 tools/pdata_map_audit.py --selftest --sabotage shift  # MUST fail
    python3 tools/pdata_map_audit.py audit [--json out.json]
"""
from __future__ import annotations

import argparse
import bisect
import json
import os
import random
import re
import struct
import sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
DEFAULT_EXE = os.path.join(ROOT, "orig/45410914/band.exe")
DEFAULT_MAP = os.path.join(ROOT, "scripts/target_symbol_map.json")
DEFAULT_REPORT = os.path.join(ROOT, "build/45410914/report.json")

CRT_CHAIN = re.compile(r"^__(save|rest)(fpr|gpr|vmx)_\d+$")


def _sections(data: bytes):
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    imgbase = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    off = pe + 24 + optsz
    out = []
    for i in range(nsec):
        e = off + i * 40
        name = data[e:e + 8].rstrip(b"\x00").decode("ascii", "replace")
        va = struct.unpack_from("<I", data, e + 12)[0]
        rsize = struct.unpack_from("<I", data, e + 16)[0]
        raw = struct.unpack_from("<I", data, e + 20)[0]
        out.append((name, imgbase + va, raw, rsize))
    return imgbase, out


def load_extents(exe: str = DEFAULT_EXE, shift: int = 8) -> dict[int, int]:
    """address -> function length in bytes, from .pdata."""
    data = open(exe, "rb").read()
    _, secs = _sections(data)
    pd = [s for s in secs if s[0] == ".pdata"]
    if not pd:
        raise SystemExit("no .pdata section")
    _, _, praw, psz = pd[0]
    ext: dict[int, int] = {}
    for off in range(praw, praw + psz, 8):
        begin, w = struct.unpack_from(">II", data, off)
        if begin == 0 or not (0x82000000 <= begin < 0x83000000):
            continue
        ext[begin] = ((w >> shift) & 0x3FFFFF) * 4
    return ext


class Extents:
    def __init__(self, ext: dict[int, int]):
        self.ext = ext
        self.keys = sorted(ext)

    def interior_of(self, a: int):
        """owning fn start if `a` is STRICTLY inside another fn, else None.

        None also means 'undecidable' when `a` falls in a .pdata gap -- callers
        must not read None as 'valid'.
        """
        if a in self.ext:
            return None
        i = bisect.bisect_right(self.keys, a) - 1
        if i < 0:
            return None
        s = self.keys[i]
        return s if a < s + self.ext[s] else None

    def is_start(self, a: int) -> bool:
        return a in self.ext


def selftest(exe=DEFAULT_EXE, sabotage=None) -> int:
    shift = 2 if sabotage == "shift" else 8
    ok = True

    def chk(name, cond, detail=""):
        nonlocal ok
        print(f"  [{'PASS' if cond else 'FAIL'}] {name}")
        if detail:
            print(f"         {detail}")
        ok = ok and bool(cond)

    print(f"pdata_map_audit selftest  exe={exe}  shift={shift}"
          + ("  (SABOTAGED)" if sabotage else ""))
    ext = load_extents(exe, shift=shift)
    chk("pdata parsed", len(ext) > 40000, f"entries={len(ext)}")

    # cross-check sizes against fingerprints.json where available
    fpp = os.path.join(ROOT, "fingerprints.json")
    if os.path.exists(fpp):
        fp = json.load(open(fpp))
        fsz = {int(a, 16): v.get("size", 0)
               for a, v in fp.items() if isinstance(v, dict)}
        common = set(ext) & set(fsz)
        agree = sum(1 for a in common if ext[a] == fsz[a])
        rate = agree / max(1, len(common))
        chk("extent sizes agree with fingerprints.json", rate > 0.9,
            f"{agree}/{len(common)} = {100*rate:.2f}%")
    else:
        print("  [SKIP] fingerprints.json absent -- size cross-check not run")

    E = Extents(ext)
    # known-positive: the four inlined DirectInstrument accessors
    pos = [0x826C48F8, 0x826C4908, 0x826C4910, 0x826C4958]
    got = [E.interior_of(a) for a in pos]
    chk("known inlined sites flagged interior to ?Handle@GemPlayer@@",
        all(g == 0x826C44F8 for g in got), f"{[hex(g) if g else None for g in got]}")
    # known-negative: a real function start is never flagged
    chk("real function start not flagged", E.interior_of(0x826C44F8) is None)
    # null: .pdata starts must flag at 0%
    random.seed(3)
    sample = random.sample(E.keys, min(2000, len(E.keys)))
    nulls = sum(1 for a in sample if E.interior_of(a))
    chk("null over .pdata starts flags nothing", nulls == 0, f"{nulls} flagged")

    print(f"  {'OK' if ok else 'FAILED'}")
    return 0 if ok else 1


def audit(exe=DEFAULT_EXE, mapf=DEFAULT_MAP, report=DEFAULT_REPORT, out=None) -> int:
    E = Extents(load_extents(exe))
    m = json.load(open(mapf))
    rows = [(int(a, 16), n) for a, n in m.items()
            if a.startswith("0x") and isinstance(n, str)]
    bad, gap, at_start, crt = [], 0, 0, 0
    for a, n in rows:
        o = E.interior_of(a)
        if o is not None:
            if CRT_CHAIN.match(n):
                crt += 1
            else:
                bad.append((a, n, o))
        elif E.is_start(a):
            at_start += 1
        else:
            gap += 1

    print(f"map rows ............................. {len(rows)}")
    print(f"  at a .pdata function start ......... {at_start}")
    print(f"  in a .pdata GAP (NOT judged) ....... {gap}")
    print(f"  CRT save/restore chain labels ...... {crt}  (legitimate, excluded)")
    print(f"  STRICTLY INSIDE another function ... {len(bad)}  <- PROVABLY FALSE")

    scoring = []
    if os.path.exists(report):
        r = json.load(open(report))
        best = {}
        for u in r.get("units", []):
            for f in u.get("functions", []):
                p = f.get("fuzzy_match_percent", 0.0) or 0.0
                if f["name"] not in best or p > best[f["name"]]:
                    best[f["name"]] = p
        scoring = [(a, n, o) for a, n, o in bad if best.get(n, -1) >= 100.0]
        print(f"  of those, currently at fuzzy 100 ... {len(scoring)}  "
              f"(deleting those lowers fuzzy: credit never earned, DC-1 class)")
    else:
        print("  (report.json absent -- scoring status not evaluated)")

    a2n = dict(rows)
    for a, n, o in sorted(bad):
        print(f"    {a:#010x} inside {o:#010x} ({a2n.get(o,'<anon>')[:52]})  {n[:64]}")

    if out:
        json.dump({"false_rows": [[hex(a), n, hex(o)] for a, n, o in sorted(bad)],
                   "scoring_100": [[hex(a), n] for a, n, o in scoring],
                   "counts": {"map_rows": len(rows), "at_start": at_start,
                              "in_gap": gap, "crt_chain_excluded": crt,
                              "provably_false": len(bad),
                              "scoring_100": len(scoring)}},
                  open(out, "w"), indent=1)
        print(f"wrote {out}")
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", default=DEFAULT_EXE)
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--sabotage", choices=["shift"],
                    help="deliberately break the bit field; --selftest MUST then fail")
    sub = ap.add_subparsers(dest="cmd")
    a = sub.add_parser("audit", help="scan the map for provably-false rows")
    a.add_argument("--map", default=DEFAULT_MAP)
    a.add_argument("--report", default=DEFAULT_REPORT)
    a.add_argument("--json", dest="out", default=None)
    ns = ap.parse_args(argv)
    if ns.selftest:
        return selftest(ns.exe, ns.sabotage)
    if ns.cmd == "audit":
        return audit(ns.exe, ns.map, ns.report, ns.out)
    ap.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
