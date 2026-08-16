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

THE ANTI-VACUITY CONTRACT (lane P, 2026-08-16)
----------------------------------------------
`--selftest --sabotage shift` MUST fail.  That is the control which makes every
green `--selftest` mean something.  It was VACUOUS IN EVERY WORKTREE until this
lane, and it reported success while dead:

  * `fingerprints.json` is gitignored (15.9 MB, regenerable) so it never travels
    to a worktree, and the size cross-check `[SKIP]`ped when it was absent.
  * That cross-check was THE ONLY ONE OF THE FIVE THAT DISCRIMINATED.  Measured
    on main: under `--sabotage shift` the other four all still PASS.
  * So in a worktree the sabotage leg printed a bare `OK` and exited 0 -- the
    clean and sabotaged runs were byte-identical but for the `shift=` header.
  * Worse, "null over .pdata starts flags nothing" was a TAUTOLOGY: it sampled
    `Extents.keys` and called `interior_of()`, whose first line is
    `if a in self.ext: return None`.  It returned 0 for ANY table -- verified
    PASSing on an all-zero-length table and on random garbage.

The fix is NOT merely to report the skip.  It is that the discriminating
controls are now INTRINSIC -- checkable from the retail binary alone, which
does travel (`setup_worktree.sh` reflinks `orig/`).  A RUNTIME_FUNCTION table
partitions code, so its extents must be DISJOINT and must lie inside an
executable section.  Measured separation is total: 0/57,732 neighbour pairs
overlap under the real shift, 57,730/57,732 (100.00%) under the sabotage.

  ! The first cut of that bound used `.text` alone and FIRED ON THE REAL
    TABLE (106/57,733).  Those 106 are genuine -- they live in `BINK`, the
    video codec's own code section.  An over-strict control is its own kind of
    dead control, so the bound is the union of EXECUTE-flagged sections.

Two rules this file now obeys, and callers should rely on:
  * It NEVER prints `OK` when a control did not run.  A skipped control yields
    `INCOMPLETE ... NOT a clean bill of health` and **exit 2** -- distinct from
    exit 1 (a control FAILED) and exit 0 (every control ran and passed).
  * `--strict` turns any skip into a failure, for callers that want fail-closed.

Usage:
    python3 tools/pdata_map_audit.py --selftest
    python3 tools/pdata_map_audit.py --selftest --strict     # skips become FAIL
    python3 tools/pdata_map_audit.py --selftest --sabotage shift  # MUST fail
    python3 tools/pdata_map_audit.py audit [--json out.json]

Exit codes (--selftest):  0 = all controls ran and passed
                          1 = a control FAILED (expected under --sabotage)
                          2 = INCOMPLETE, a control could not run
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
        chars = struct.unpack_from("<I", data, e + 36)[0]
        out.append((name, imgbase + va, raw, rsize, chars))
    return imgbase, out


IMAGE_SCN_MEM_EXECUTE = 0x20000000


def code_spans(exe: str) -> list[tuple[int, int]]:
    """[(lo, hi)] of every EXECUTABLE section, merged where adjacent.

    ! Bounding .pdata extents against `.text` ALONE is WRONG and this was
      measured, not assumed: 106 of 57,733 real extents land in `BINK`, the
      Bink video codec's own code section (0x82c4d000-0x82c5d200), which
      carries legitimate unwind records.  Keying on the section CHARACTERISTICS
      bit rather than the name keeps that correct if another codec is linked in.
    """
    data = open(exe, "rb").read()
    _, secs = _sections(data)
    spans = sorted((va, va + rsize) for _n, va, _r, rsize, ch in secs
                   if ch & IMAGE_SCN_MEM_EXECUTE and rsize)
    if not spans:
        raise SystemExit("no executable sections")
    merged = [list(spans[0])]
    for lo, hi in spans[1:]:
        if lo <= merged[-1][1]:
            merged[-1][1] = max(merged[-1][1], hi)
        else:
            merged.append([lo, hi])
    return [(lo, hi) for lo, hi in merged]


def load_extents(exe: str = DEFAULT_EXE, shift: int = 8) -> dict[int, int]:
    """address -> function length in bytes, from .pdata."""
    data = open(exe, "rb").read()
    _, secs = _sections(data)
    pd = [s for s in secs if s[0] == ".pdata"]
    if not pd:
        raise SystemExit("no .pdata section")
    _, _, praw, psz, _ = pd[0]
    ext: dict[int, int] = {}
    for off in range(praw, praw + psz, 8):
        begin, w = struct.unpack_from(">II", data, off)
        if begin == 0 or not (0x82000000 <= begin < 0x83000000):
            continue
        ext[begin] = ((w >> shift) & 0x3FFFFF) * 4
    return ext


def check_extents(ext: dict[int, int], spans: list[tuple[int, int]]) -> dict:
    """INTRINSIC consistency of the decoded .pdata table -- no external fixture.

    A RUNTIME_FUNCTION table partitions code, so two properties hold for any
    correct decode and are checkable from the retail binary ALONE:
        (1) extents are DISJOINT           -- no body runs into the next
        (2) extents lie inside CODE        -- some executable section
    These are what make `--sabotage shift` discriminate in a worktree, where
    `fingerprints.json` is absent.  Do not replace them with a fixture-dependent
    check: that is precisely the regression this function exists to prevent.
    """
    keys = sorted(ext)

    def inside(a: int, b: int) -> bool:
        return any(lo <= a and b <= hi for lo, hi in spans)

    overlaps = [k for i, k in enumerate(keys[:-1]) if k + ext[k] > keys[i + 1]]
    outside = [k for k in keys if not inside(k, k + ext[k])]
    return {
        "n": len(keys),
        "pairs": max(1, len(keys) - 1),
        "overlaps": len(overlaps),
        "first_overlap": overlaps[0] if overlaps else None,
        "outside": len(outside),
        "first_outside": outside[0] if outside else None,
        "covered": sum(ext.values()),
        "code_size": sum(hi - lo for lo, hi in spans),
    }


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

    def covered_by_predecessor(self, a: int):
        """Owning start if the extent immediately BEFORE `a` runs past `a`.

        Deliberately does NOT short-circuit on `a in self.ext` the way
        interior_of() does.  That short-circuit is correct for the detector --
        a .pdata start is by definition a real function start -- but it makes
        "sample .pdata starts, assert none are flagged" a question that cannot
        return anything but 0.  The null uses THIS predicate so it can fail.
        """
        i = bisect.bisect_left(self.keys, a)
        if i <= 0:
            return None
        p = self.keys[i - 1]
        return p if p + self.ext[p] > a else None


def selftest(exe=DEFAULT_EXE, sabotage=None, strict=False) -> int:
    shift = 2 if sabotage == "shift" else 8
    ok = True
    ran = 0
    skipped: list[tuple[str, str, str]] = []

    def chk(name, cond, detail=""):
        nonlocal ok, ran
        ran += 1
        print(f"  [{'PASS' if cond else 'FAIL'}] {name}")
        if detail:
            print(f"         {detail}")
        ok = ok and bool(cond)

    def skip(name, why, howto):
        """Record a control that COULD NOT RUN.  Never silently.

        Under --strict this is a failure outright; otherwise it downgrades the
        verdict to INCOMPLETE (exit 2).  It must never leave the run printing OK.
        """
        nonlocal ok
        skipped.append((name, why, howto))
        print(f"  [{'FAIL' if strict else 'SKIP'}] {name}")
        print(f"         DID NOT RUN: {why}")
        if strict:
            ok = False

    print(f"pdata_map_audit selftest  exe={exe}  shift={shift}"
          + ("  (SABOTAGED)" if sabotage else "")
          + ("  [strict]" if strict else ""))
    ext = load_extents(exe, shift=shift)
    chk("pdata parsed", len(ext) > 40000, f"entries={len(ext)}")

    # ---- INTRINSIC controls: binary-only, so they run in EVERY worktree -----
    # These are the discriminating legs.  See THE ANTI-VACUITY CONTRACT above:
    # the fingerprints cross-check used to be the only one that noticed the
    # sabotage, and it is exactly the one that cannot run in a worktree.
    st = check_extents(ext, code_spans(exe))
    chk("extents are DISJOINT (intrinsic -- no fixture needed)",
        st["overlaps"] == 0,
        f'{st["overlaps"]}/{st["pairs"]} neighbour pairs overlap'
        + (f' (first at {st["first_overlap"]:#010x})' if st["first_overlap"] else ""))
    chk("extents lie inside executable sections (intrinsic)", st["outside"] == 0,
        f'{st["outside"]}/{st["n"]} extents fall outside code'
        + (f' (first at {st["first_outside"]:#010x})' if st["first_outside"] else ""))
    chk("total covered <= code size (intrinsic)",
        st["covered"] <= st["code_size"],
        f'covered={st["covered"]:,} B  code={st["code_size"]:,} B')

    # ---- corroborating control: needs gitignored, regenerable fingerprints --
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
        skip("extent sizes agree with fingerprints.json",
             f"fingerprints.json absent at {fpp} (gitignored -- it does not "
             "travel to a worktree)",
             "re-run scripts/setup_worktree.sh (it now reflink-copies the file), "
             "or: cp <main-repo>/fingerprints.json .  / regenerate with "
             "tools/fingerprint_match.py extract")

    E = Extents(ext)
    # known-positive: the four inlined DirectInstrument accessors
    pos = [0x826C48F8, 0x826C4908, 0x826C4910, 0x826C4958]
    got = [E.interior_of(a) for a in pos]
    chk("known inlined sites flagged interior to ?Handle@GemPlayer@@",
        all(g == 0x826C44F8 for g in got), f"{[hex(g) if g else None for g in got]}")
    # known-negative: a real function start is never flagged
    chk("real function start not flagged", E.interior_of(0x826C44F8) is None)
    # null: no .pdata start may be swallowed by its predecessor's extent.
    # Uses covered_by_predecessor(), NOT interior_of() -- the latter's
    # `if a in self.ext: return None` made this leg a tautology that PASSed on
    # an all-zero table and on random garbage alike.
    random.seed(3)
    sample = random.sample(E.keys, min(2000, len(E.keys)))
    nulls = sum(1 for a in sample if E.covered_by_predecessor(a))
    chk("null: sampled .pdata starts not swallowed by predecessor",
        nulls == 0, f"{nulls}/{len(sample)} flagged")

    if not ok:
        print("  FAILED")
        return 1
    if skipped:
        # NEVER print OK here.  A control that did not run is not evidence of
        # absence -- this is the failure class the whole file guards against.
        print(f"  INCOMPLETE -- {ran} controls ran, {len(skipped)} COULD NOT RUN"
              "  (NOT a clean bill of health)")
        for name, why, howto in skipped:
            print(f"    not examined: {name}")
            print(f"      why: {why}")
            print(f"      fix: {howto}")
        print("  exit 2 = incomplete coverage; use --strict to make this fatal")
        return 2
    print(f"  OK ({ran}/{ran} controls ran)")
    return 0


def audit(exe=DEFAULT_EXE, mapf=DEFAULT_MAP, report=DEFAULT_REPORT, out=None) -> int:
    ext = load_extents(exe)
    # The audit's whole output is the word "PROVABLY FALSE".  That claim is only
    # as good as the extent table under it, so refuse outright rather than
    # emit confident rows off a table that fails its own intrinsic invariants.
    st = check_extents(ext, code_spans(exe))
    if st["overlaps"] or st["outside"]:
        raise SystemExit(
            f"REFUSING to audit: decoded .pdata table is self-inconsistent "
            f'({st["overlaps"]}/{st["pairs"]} neighbour pairs overlap, '
            f'{st["outside"]} extents outside code).  Every "PROVABLY FALSE" '
            f"row would be an artefact of the decode.  Run --selftest.")
    E = Extents(ext)
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
    ap.add_argument("--strict", action="store_true",
                    help="fail closed: a control that cannot run is a FAILURE "
                         "(exit 1) rather than INCOMPLETE (exit 2)")
    sub = ap.add_subparsers(dest="cmd")
    a = sub.add_parser("audit", help="scan the map for provably-false rows")
    a.add_argument("--map", default=DEFAULT_MAP)
    a.add_argument("--report", default=DEFAULT_REPORT)
    a.add_argument("--json", dest="out", default=None)
    ns = ap.parse_args(argv)
    if ns.selftest:
        return selftest(ns.exe, ns.sabotage, ns.strict)
    if ns.cmd == "audit":
        return audit(ns.exe, ns.map, ns.report, ns.out)
    ap.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
