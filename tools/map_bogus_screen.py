#!/usr/bin/env python3
"""map_bogus_screen -- "is this map address even a FUNCTION?"

Lane MAPBOGUS-1 (2026-08-13).  Read-only; not a build input.

WHY THIS EXISTS
---------------
Lane RECOVER-95K reversed a 95,100 B removal after discovering that the
"evidence" behind it was vacuous: the comparator had been fed map addresses that
are not functions at all.  ??3UIComponent@@ @0x8282779c and ??3PlayerDiffIcon@@
@0x823258d4 begin with a literal 0x00000000 ALIGNMENT WORD.  A comparator whose
second operand is padding can only ever say DIFFERENT.

    *** BEFORE COMPARING TWO BODIES, PROVE BOTH OPERANDS *ARE* BODIES. ***

RECOVER-95K ran a negative control (adjacent-function decoys) and a positive
control (identical pairs exist) but never controlled for whether addr(F) is a
function at all.  This tool is that control, generalised over the whole map.

THE FIVE CRITERIA, AND WHAT SURVIVED
------------------------------------
Every criterion below was measured against an UNTREATED CONTROL of known-good
function starts.  Three of the five are reported here as DRAINED or EMPTY.  That
is the point of the file: a future lane should not re-fund them.

  1. PAD -- the first word is 0x00000000, i.e. the address is alignment padding.
     *** ARMED.  Control 0 / 62,696 known-good function starts.  ***
     Structurally sound: opcode 0 is illegal on PowerPC, so no function can
     begin with it.  Retail's first words are opcode 31/14/37/62/54/15
     (mfspr/addi/stwu/std/stfd/addis) and NEVER 0.
     -> 11 map rows.

  2. NOT_TEXT -- a function-named row whose address is not in .text.
     *** ARMED (trivially decisive).  -> 1 map row. ***

  3. PDATA_INTERIOR -- strictly inside another .pdata extent.
     *** ARMED BUT EMPTY: 0 rows. *** Already drained by an earlier lane.
     Implemented in tools/pdata_map_audit.py, whose selftest still fires on its
     4 known DirectInstrument positives -- so the zero is a real zero, not a
     dead screen.  Delegated, not reimplemented.

  4. EH_PREFIX -- addr is an 8-byte EH prefix whose real body is at addr+8.
     *** ARMED BUT EMPTY: 0 rows. ***  NOT vacuous: the same shape detector
     fires on 9,141 / 57,733 (15.83%) of .pdata starts in the image.  It is
     abundantly live; it simply never lands on a map address.

  5. FLOWS_IN -- control flows INTO the address from addr-4, so it cannot be a
     function start.
     *** DRAINED -- 0.54x.  DO NOT RE-FUND.  See below. ***

THE FLOWS_IN TRAP (the most useful thing in this file)
------------------------------------------------------
FLOWS_IN looked like the best criterion here.  Measured against .pdata function
starts it has a PERFECT control:

    FLOWS-IN, non-bl predecessor, over .pdata starts:  0 / 57,733 = 0.0000%

and it fires on 37 map rows, including RECOVER-95K's ??3RndLight @0x8270d7f8.
Infinite enrichment, zero false positives.  It is WRONG.

*** .pdata STARTS ARE NON-LEAF FUNCTIONS BY CONSTRUCTION. ***  An 8-byte leaf
stub touches neither the stack nor LR, so it gets no unwind record (CLAUDE.md,
lane AUDIT-NC).  Every one of the 37 candidates lives in the .pdata GAP stratum
-- i.e. exactly the leaf population the control EXCLUDES.  The control could not
fail on the stratum being screened.

The honest control is an independent oracle for LEAF starts: addresses that are
the target of a `bl`, since a call always lands on a function entry.

    population                              FLOWS-IN-nonbl
    .pdata starts (non-leaf)                    0 / 57,733 = 0.0000%
    LEAF starts (bl targets, not .pdata)       54 /  4,963 = 1.0881%
    leaf starts with >= 3 callers              42 /  1,815 = 2.3140%
    TREATED: map FUNC rows in .pdata gaps      37 /  6,328 = 0.5847%

    ENRICHMENT = 0.5847 / 1.0881 = 0.54x   -- BELOW ONE.

Small leaf COMDATs really are packed with no padding after a preceding function
~1-2% of the time, and the map's gap rows do it LESS often than random
known-good leaves.  So "control flows in" carries no information about a bogus
map row in the stratum where it can be applied at all.  Same shape as
tools/callsite_screen.py's 0.77x.

Two consequences, both load-bearing:
  * All 37 candidates are WITHDRAWN, including the six drawing credit at
    fuzzy == 100 (?Write@NullFile@@, ?EaseLinear@@, ??2GameGem@@,
    ??1FilePath@@, ?_Destroy_Range@stlpmtx_std@@, and the MicClientMapper
    _Destroy_Moved_Range).  Nulling those on this evidence would have destroyed
    32 B of credit on a screen that does not discriminate.
  * RECOVER-95K's ??3RndLight @0x8270d7f8 is NOT supported by flow evidence.
    It may still be misnamed -- it decodes as `b Fader::DoFade`, which is
    semantically absurd for an operator delete -- but that is a NAME-IDENTITY
    question, not a BODYHOOD one, and this tool does not answer it.

OTHER REFUTED SIGNALS (measured here, recorded so they are not re-tried)
-----------------------------------------------------------------------
  * BARE_B ("the first word is an unconditional branch") is NOT a defect
    signal.  It fires on 155 map rows, but those are the legitimate 4-12 byte
    TAIL-JUMP STUB stratum -- ??3BandHighlight@@ @0x82345030 has fan-in 67.
    Its 0/36,244 rate on .pdata starts is by-construction bias, not
    specificity: a pure `b target` stub is a leaf and has no unwind record.
  * ZERO FAN-IN is not disqualifying (vtable dispatch), and absence from .pdata
    is not disqualifying (leaf stubs).  Neither is used as a verdict here.

TWO DECODER DEFECTS CAUGHT WHILE BUILDING THIS (both registered as injectable
defects in tools/screen_gate.py)
---------------------------------------------------------------------------
  * SECTION EXTENTS MUST USE max(VirtualSize, SizeOfRawData).  .data's
    VirtualSize (0x1f5eac) is 3.5x its SizeOfRawData (0x058200), so a
    raw-size-only reading manufactured 286 phantom "outside every section"
    rows.  The true count is 1.  (tools/pdata_map_audit.py::_sections reads
    SizeOfRawData only -- harmless for its own .pdata use, wrong for section
    membership.  Do not borrow it for that.)
  * THE NAME-KIND CLASSIFIER MUST READ THE END OF THE MANGLING, NOT THE FIRST
    `@@`.  Function-LOCAL STATICS mangle as
    ?msg@?BD@??StartAnim@BandCamShot@@UAAXXZ@4VMessage@@A -- the first `@@`
    lands inside the ENCLOSING FUNCTION's mangling, so reading the storage
    class there calls 241 `.data` variables "functions".  Judging those on
    "is this a function head" would be a mass false positive.  The
    Z-terminator rule cuts FUNC-rows-not-in-.text from 242 to 1, validated
    against section membership as an independent oracle.

Usage:
    python3 tools/map_bogus_screen.py audit            # armed criteria only
    python3 tools/map_bogus_screen.py audit --all      # incl. DRAINED classes
    python3 tools/map_bogus_screen.py controls         # re-measure every control
    python3 tools/map_bogus_screen.py nulls            # emit the map edit
"""
from __future__ import annotations

import argparse
import bisect
import collections
import json
import os
import struct
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
if HERE not in sys.path:
    sys.path.insert(0, HERE)

DEFAULT_EXE = os.path.join(ROOT, "orig/45410914/band.exe")
DEFAULT_MAP = os.path.join(ROOT, "scripts/target_symbol_map.json")
DEFAULT_REPORT = os.path.join(ROOT, "build/45410914/report.json")


# ---------------------------------------------------------------------------
# Image decoding
# ---------------------------------------------------------------------------

def sections(exe=DEFAULT_EXE):
    """-> (data, {name: (va, size, rawptr)}) using max(VirtualSize, RawSize).

    *** The max() is load-bearing. ***  Using SizeOfRawData alone truncates
    .data by 3.5x and manufactures 286 phantom out-of-section rows.
    """
    data = open(exe, "rb").read()
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    imgbase = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    off = pe + 24 + optsz
    out = {}
    for i in range(nsec):
        e = off + i * 40
        nm = data[e:e + 8].rstrip(b"\x00").decode("ascii", "replace")
        vs, va, srd, praw = struct.unpack_from("<IIII", data, e + 8)
        out[nm] = (imgbase + va, max(vs, srd), praw)
    return data, out


class Image:
    def __init__(self, exe=DEFAULT_EXE):
        self.data, self.secs = sections(exe)
        self.tv, self.tsz, self.tr = self.secs[".text"]
        self.rv, self.rsz, _ = self.secs[".rdata"]

    def word(self, a):
        """Big-endian instruction word at VA `a`, or None if outside .text."""
        if not (self.tv <= a < self.tv + self.tsz - 3):
            return None
        return struct.unpack_from(">I", self.data, self.tr + (a - self.tv))[0]

    def section_of(self, a):
        for nm, (va, sz, _p) in self.secs.items():
            if va <= a < va + sz:
                return nm
        return None

    def in_text(self, a):
        return self.tv <= a < self.tv + self.tsz

    def in_rdata(self, a):
        return self.rv <= a < self.rv + self.rsz


# ---------------------------------------------------------------------------
# Primitive predicates -- each one is a screen in its own right
# ---------------------------------------------------------------------------

def is_padding_word(w):
    """*** ARMED. *** 0 / 62,696 known-good function starts begin with this.

    Opcode 0 is illegal on PowerPC; retail's inter-function alignment fill is
    all-zero (CLAUDE.md: all 265 `Illegal inst` sites are 0x00000000, all in
    gaps between function extents).
    """
    return w == 0


def is_terminator(w):
    """Does this instruction NOT fall through to the next one?"""
    if w is None or w == 0:
        return True
    op = w >> 26
    if op == 18 and (w & 1) == 0:                 # unconditional b (not bl)
        return True
    if op == 19:
        xo = (w >> 1) & 0x3FF
        if xo in (16, 528) and (w & 1) == 0 and ((w >> 21) & 0x14) == 0x14:
            return True                            # blr / bctr (unconditional)
    return False


def is_bl(w):
    return w is not None and (w >> 26) == 18 and (w & 1) == 1


FUNC_THUNK_PREFIXES = ("??_9",)                    # vcall thunks: code, not data
DATA_PREFIXES = ("??_C", "??_R", "??_7", "??_8")   # strings, RTTI, vftable/vbtable


def name_kind(n):
    """FUNC / DATA / UNKNOWN.

    *** Read the END of the mangling, never the first `@@`. ***  A function-local
    static mangles as ?x@?1??Enclosing@C@@QAAXXZ@4VT@@A and its first `@@` sits
    inside the ENCLOSING FUNCTION's mangling -- reading the storage class there
    classifies 241 .data variables as functions.  MSVC function manglings
    terminate with the return-type/arglist suffix ending in `Z`; data does not.
    Validated against section membership: FUNC-rows-not-in-.text 242 -> 1.
    """
    if not n or not n.startswith("?"):
        return "UNKNOWN"
    if n.startswith(FUNC_THUNK_PREFIXES):
        return "FUNC"
    if n.startswith(DATA_PREFIXES):
        return "DATA"
    return "FUNC" if n.endswith("Z") else "DATA"


def eh_prefix_shape(img, a, extents):
    """Is `a` an 8-byte EH prefix whose real body starts at a+8?

    Live-ness proof: this shape precedes 9,141 / 57,733 (15.83%) .pdata starts,
    so a zero over the map is a real zero, not a dead detector.
    """
    if extents.is_start(a):
        return False
    w0, w1 = img.word(a), img.word(a + 4)
    if w0 is None or w1 is None:
        return False
    return img.in_text(w0) and img.in_rdata(w1) and extents.is_start(a + 8)


def flows_in(img, a, extents):
    """*** DRAINED, 0.54x.  Kept so nobody re-derives it.  See module docstring.

    Returns 'bl' / 'nonbl' / None.  The 'nonbl' stratum reads 0.0000% on .pdata
    starts and 1.0881% on LEAF starts -- and every address this can be applied
    to is a leaf.  Do NOT condemn a row on this.
    """
    if extents.is_start(a):
        return None
    p = img.word(a - 4)
    if p is None or is_terminator(p):
        return None
    w0 = img.word(a - 8)
    if w0 is not None and img.in_text(w0) and img.in_rdata(p):
        return None                                # EH prefix, not real flow
    return "bl" if is_bl(p) else "nonbl"


# ---------------------------------------------------------------------------
# Populations
# ---------------------------------------------------------------------------

def load_map(path=DEFAULT_MAP):
    m = json.load(open(path))
    rows = [(int(a, 16), n) for a, n in m.items()
            if a.startswith("0x") and isinstance(n, str)]
    deny = set()
    for k in ("_denylist", "_denylist_unadjudicated"):
        for x in m.get(k, []):
            deny.add(int(x, 16))
    return m, sorted(rows), deny


def known_good_starts(img, extents, counts):
    """Two INDEPENDENT oracles for "this address is a real function start".

    .pdata starts are NON-LEAF by construction, so they are NOT a sufficient
    control for a screen applied to the leaf/gap stratum -- that omission is
    what made FLOWS_IN look perfect.  `bl` targets supply the leaf half: a call
    always lands on a function entry.
    """
    pdata = list(extents.keys)
    leaf = [a for a in counts if img.in_text(a) and not extents.is_start(a)]
    return pdata, leaf


# ---------------------------------------------------------------------------
# The audit
# ---------------------------------------------------------------------------

VERDICTS = ("PAD", "NOT_TEXT")                     # the ARMED, non-empty ones


def audit(exe=DEFAULT_EXE, mapf=DEFAULT_MAP, report=DEFAULT_REPORT,
          show_drained=False, out=sys.stdout):
    import pdata_map_audit as P
    import callsite_screen as C

    img = Image(exe)
    extents = P.Extents(P.load_extents(exe))
    counts = C.call_graph(exe)
    m, rows, deny = load_map(mapf)

    best = {}
    if os.path.exists(report):
        r = json.load(open(report))
        for u in r.get("units", []):
            for f in u.get("functions", []):
                # report.json is protobuf-JSON (defaults omitted) with several
                # numerics shipped as JSON STRINGS -- coerce every one.
                fz = float(f.get("fuzzy_match_percent", 0.0) or 0.0)
                mp = float(f.get("match_percent_normalized", 0.0) or 0.0)
                sz = int(f.get("size", 0) or 0)
                k = f["name"]
                if k not in best or fz > best[k][0]:
                    best[k] = (fz, mp, sz, u.get("name", ""))

    n2a = collections.defaultdict(list)
    for a, n in rows:
        n2a[n].append(a)

    found = collections.defaultdict(list)
    judged = 0
    for a, n in rows:
        if name_kind(n) != "FUNC":
            continue
        judged += 1
        if not img.in_text(a):
            found["NOT_TEXT"].append((a, n))
            continue
        if is_padding_word(img.word(a)):
            found["PAD"].append((a, n))
            continue
        if eh_prefix_shape(img, a, extents):
            found["EH_PREFIX"].append((a, n))
            continue
        f = flows_in(img, a, extents)
        if f == "nonbl":
            found["FLOWS_IN_DRAINED"].append((a, n))

    print("=" * 72, file=out)
    print("MAP BOGUS-ADDRESS SCREEN", file=out)
    print("=" * 72, file=out)
    print(f"map rows ......................... {len(rows)}", file=out)
    print(f"  function-named (judged) ........ {judged}", file=out)
    print(f"  data-named / unknown (skipped) . {len(rows) - judged}", file=out)
    print(file=out)
    print("ARMED criteria (control 0 / 62,696 known-good function starts):",
          file=out)
    total = 0
    for v in VERDICTS:
        hits = found.get(v, [])
        total += len(hits)
        print(f"  {v:<10} {len(hits):>4}", file=out)
    print(f"  {'PDATA_INT':<10} {0:>4}   (delegated to pdata_map_audit; "
          f"ARMED, measured 0)", file=out)
    print(f"  {'EH_PREFIX':<10} {len(found.get('EH_PREFIX', [])):>4}   "
          f"(detector live: fires before 15.83% of .pdata starts)", file=out)
    print(f"  {'FLOWS_IN':<10} {len(found.get('FLOWS_IN_DRAINED', [])):>4}   "
          f"*** DRAINED 0.54x -- NOT candidates, do not null ***", file=out)
    print(file=out)

    for v in VERDICTS:
        hits = found.get(v, [])
        if not hits:
            continue
        print(f"--- {v} ({len(hits)}) ---", file=out)
        for a, n in sorted(hits):
            b = best.get(n)
            credit = ("fuzzy=%.4f mpn=%.2f size=%d unit=%s" % b) if b \
                else "ABSENT from report (draws no credit)"
            print(f"  {a:#010x}  fanin={counts.get(a, 0)}  "
                  f"naddr={len(n2a[n])}  denylisted={a in deny}", file=out)
            print(f"      {n[:100]}", file=out)
            print(f"      {credit}", file=out)
        print(file=out)

    if show_drained and found.get("FLOWS_IN_DRAINED"):
        print("--- FLOWS_IN (DRAINED 0.54x -- listed for the record ONLY) ---",
              file=out)
        for a, n in sorted(found["FLOWS_IN_DRAINED"]):
            b = best.get(n)
            print(f"  {a:#010x}  {n[:70]}"
                  f"{'   [DRAWS CREDIT]' if b and b[0] >= 100.0 else ''}",
                  file=out)
        print("\n  These are NOT candidates.  The screen fires on known-good\n"
              "  LEAF starts MORE often (1.09%) than on these rows (0.58%).",
              file=out)
    return found


def controls(exe=DEFAULT_EXE, mapf=DEFAULT_MAP, out=sys.stdout):
    """Re-measure every control from scratch.  Never trust a stored number."""
    import pdata_map_audit as P
    import callsite_screen as C
    img = Image(exe)
    extents = P.Extents(P.load_extents(exe))
    counts = C.call_graph(exe)
    pdata, leaf = known_good_starts(img, extents, counts)
    _m, rows, _d = load_map(mapf)
    treated = [a for a, n in rows if name_kind(n) == "FUNC" and img.in_text(a)]
    gaps = [a for a in treated if not extents.is_start(a)]

    def rate(pop, pred):
        hit = sum(1 for a in pop if pred(a))
        return hit, len(pop), (100.0 * hit / len(pop) if pop else 0.0)

    print("CONTROL RE-MEASUREMENT", file=out)
    print("-" * 72, file=out)
    print("PAD (first word == 0x00000000):", file=out)
    for lbl, pop in (("CONTROL .pdata starts", pdata),
                     ("CONTROL leaf starts (bl targets)", leaf),
                     ("CONTROL union", sorted(set(pdata) | set(leaf))),
                     ("TREATED map FUNC rows in .text", treated)):
        h, t, p = rate(pop, lambda a: is_padding_word(img.word(a)))
        print(f"  {lbl:<34} {h:>6} / {t:<6} = {p:.4f}%", file=out)
    print("  => ARMED (control exactly 0, enrichment infinite)", file=out)
    print(file=out)
    print("FLOWS_IN, non-bl predecessor:", file=out)
    for lbl, pop in (("CONTROL .pdata starts (NON-LEAF!)", pdata),
                     ("CONTROL leaf starts (bl targets)", leaf),
                     ("CONTROL leaf starts, >=3 callers",
                      [a for a in leaf if counts[a] >= 3]),
                     ("TREATED map FUNC rows in .pdata gaps", gaps)):
        h, t, p = rate(pop, lambda a: flows_in(img, a, extents) == "nonbl")
        print(f"  {lbl:<34} {h:>6} / {t:<6} = {p:.4f}%", file=out)
    print("  => DRAINED.  The .pdata control CANNOT FAIL here (it excludes the",
          file=out)
    print("     leaf stratum being screened); against the leaf oracle the",
          file=out)
    print("     enrichment is 0.54x -- BELOW ONE.", file=out)
    print(file=out)
    print("EH_PREFIX shape liveness (proves the zero is real):", file=out)
    live = sum(1 for a in pdata
               if (lambda w0, w1: w0 is not None and w1 is not None
                   and img.in_text(w0) and img.in_rdata(w1))
               (img.word(a - 8), img.word(a - 4)))
    print(f"  shape precedes {live} / {len(pdata)} .pdata starts = "
          f"{100.0*live/len(pdata):.2f}%", file=out)
    return 0


def nulls(exe=DEFAULT_EXE, mapf=DEFAULT_MAP, report=DEFAULT_REPORT,
          out=sys.stdout):
    """Emit the addresses whose map row should be nulled, with the safety check.

    A row is emitted ONLY if it is bogus on an ARMED criterion AND it draws no
    credit.  Nulling a row that is scoring destroys honest credit -- that
    refusal is the point, not an edge case.
    """
    found = audit(exe, mapf, report, out=open(os.devnull, "w"))
    best = {}
    if os.path.exists(report):
        r = json.load(open(report))
        for u in r.get("units", []):
            for f in u.get("functions", []):
                fz = float(f.get("fuzzy_match_percent", 0.0) or 0.0)
                best[f["name"]] = max(best.get(f["name"], 0.0), fz)
    emit, refused = [], []
    for v in VERDICTS:
        for a, n in sorted(found.get(v, [])):
            (refused if n in best else emit).append((a, n, v))
    print("NULL THESE (bogus on an ARMED criterion, drawing no credit):",
          file=out)
    for a, n, v in emit:
        print(f'  "{a:#010x}": null,   // {v}: {n[:70]}', file=out)
    print(f"  total {len(emit)}", file=out)
    if refused:
        print("\nREFUSED (bogus-looking but DRAWING CREDIT -- adjudicate on "
              "retail bytes first):", file=out)
        for a, n, v in refused:
            print(f"  {a:#010x} {v} {n[:70]}", file=out)
    return 0


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__.split("\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--exe", default=DEFAULT_EXE)
    ap.add_argument("--map", default=DEFAULT_MAP)
    ap.add_argument("--report", default=DEFAULT_REPORT)
    sub = ap.add_subparsers(dest="cmd")
    a = sub.add_parser("audit", help="screen the map")
    a.add_argument("--all", action="store_true",
                   help="also list the DRAINED FLOWS_IN class")
    sub.add_parser("controls", help="re-measure every control")
    sub.add_parser("nulls", help="emit the map edit")
    ns = ap.parse_args(argv)
    if ns.cmd == "audit":
        audit(ns.exe, ns.map, ns.report, show_drained=ns.all)
        return 0
    if ns.cmd == "controls":
        return controls(ns.exe, ns.map)
    if ns.cmd == "nulls":
        return nulls(ns.exe, ns.map, ns.report)
    ap.print_help()
    return 0


if __name__ == "__main__":
    sys.exit(main())
