#!/usr/bin/env python3
"""circular_pin_census.py -- "does this function's CALLEE SET agree with the
unit it is pinned to?"  (lane W15-A, 2026-09-14.  READ-ONLY.)

WHY THIS EXISTS
---------------
Lane W14-C (docs/decomp/SWAPPED_NAMES_2026-09-14.md) caught a CIRCULAR PIN in
the wild: `0x8251CBE0` was pinned to UsbMidiGuitarMsgs.cpp *because* someone
believed the map name there, and the map name looked plausible *because* it was
pinned into a unit declaring that class.  Name and pin corroborated each other
and were both wrong.  Such a self-consistent pair is very unlikely to be unique,
and neither channel can detect it -- each is the other's evidence.

This tool adds a THIRD channel that is independent of both: the retail CALL
GRAPH, decoded from `orig/45410914/band.exe` bytes.  A function's callees are a
fact about retail, not about our splits or our map.

THE SIGNAL, AND WHY A LEGITIMATE CROSS-UNIT CALL DOES NOT FIRE
--------------------------------------------------------------
Cross-unit calls are the normal case -- every public API call is one.  What is
NOT normal is calling a function that *cannot* be, or demonstrably is not,
reached from outside one TU:

  S1 ANON_NS (a PROOF, not a heuristic).  A function in an anonymous namespace
     (`?A0x<hash>@` in the mangled name) has INTERNAL LINKAGE: the linker cannot
     resolve a reference to it from another TU.  So if F calls C and C is
     anon-ns, F and C are in the SAME TU, full stop.  If our pins put them in
     different units, one of those two pins is WRONG.  No judgement involved.

  S2 SOLE_FOREIGN_CALLER.  Callee C is pinned to V; every caller of C *other
     than F* also lives in V; F (pinned to U != V) is the lone outsider.  C
     behaves like V-private code and F is the only thing reaching in.

  S3 FANIN_1_FOREIGN.  C's only caller in the whole binary is F, yet C is
     pinned to a different unit.  Weakest of the three -- a one-shot helper is
     usually emitted next to its only caller, but not always.

A function scores only on callees that survive the rarity filter below, and a
BLOCK score aggregates consecutive same-unit functions agreeing on one foreign
destination -- because a mis-pin moves a contiguous run, not one function.

⛔ THE FILTER IS NOT OPTIONAL -- THE NAIVE VERSION IS VACUOUS
-------------------------------------------------------------
Lane L2-BODYTRIAGE (docs/decomp/MISPIN_SUSPECTS_2026-09-10.md) measured that the
obvious "what does this block reference" channel is dominated by artifacts:
`?OnHit@KeyboardTrackWatcherImpl@@...` has **644 callers binary-wide** because it
is an ICF fold survivor wearing whichever spelling the linker's coin-flip kept.
Reading that as "this block is track-watcher code" reads the linker's arbitrary
choice as a fact about the TU.  Unfiltered, this produced confident mis-pin
verdicts on three units, two of which did not survive.

So a callee counts as evidence only if its fan-in is below --max-fanin (default
20, L2's calibration).  Suppressed callees are COUNTED AND REPORTED, never
silently dropped.

CORROBORATION (never the primary signal)
----------------------------------------
`--dc3-map` reads DC3's leaked `ham_xbox_r.map`, whose `Lib:Object` column names
the TU each symbol was linked from.  DC3 is the same Milo engine built with the
same flags, so for a NON-TEMPLATE symbol its TU attribution is an independent
opinion about where the code lives.  Templates/COMDATs are excluded: any
instantiating TU can emit them and the linker keeps an arbitrary one, so their
attribution is a coin-flip (the same reason `_icf_arbitrary` exists in the map).
DC3 is also NEWER than RB3, so code may genuinely have moved between TUs --
which is why this is corroboration and never proof.

WHAT IS CIRCULAR AND MUST NOT BE USED AS EVIDENCE
--------------------------------------------------
The dtk-split target `.obj` files and the `.s` files are GENERATED FROM
splits.txt.  Using them to judge splits.txt is circular by construction.  They
are used here ONLY for (a) function extents (dtk's carve, from retail .pdata and
control flow -- not from the pin) and (b) to READ BACK the attribution under
test.  Every piece of EVIDENCE comes from retail bytes, the map, or DC3.

TRAPS OBSERVED BY OTHER LANES AND HONOURED HERE
------------------------------------------------
* Keyed on FULL PATH, never `basename()`.  `Movie.obj` genuinely collides
  between `rnddx9/` and `rndobj/`; four consecutive lanes' scans broke on this.
* The `.s` ADDRESS COLUMN IS SYNTHETIC for multi-block units (dtk computes
  `first_block_start + cumulative offset`).  Every extent here is derived from
  the `.fn fn_<ADDR>` label plus a 4-bytes-per-instruction count.  The address
  column is never read.
* A fresh worktree's reflinked target objs are PRE-RENAMER, so mangled-name
  lookups read absent until a full build.  --selftest asserts a name-population
  floor rather than trusting a negative.
* capstone's PPC decoder halts at the first undecodable word, which is how an
  earlier lane got a vacuous "0 bl callees" in a 4,260 B function.  The decoder
  here is hand-written and cannot halt: it reads every 4-byte word in the extent
  and only classifies `bl`.
"""
from __future__ import annotations

import argparse
import bisect
import collections
import json
import os
import re
import struct
import sys
from pathlib import Path

# ---------------------------------------------------------------- PE / retail

class Image:
    """Minimal big-endian-aware PE reader for the decrypted retail image."""

    def __init__(self, path: Path):
        self.raw = path.read_bytes()
        pe = struct.unpack_from("<I", self.raw, 0x3C)[0]
        if self.raw[pe:pe + 4] != b"PE\0\0":
            raise SystemExit(f"{path}: not a PE image")
        nsec = struct.unpack_from("<H", self.raw, pe + 6)[0]
        optsz = struct.unpack_from("<H", self.raw, pe + 20)[0]
        self.imagebase = struct.unpack_from("<I", self.raw, pe + 24 + 28)[0]
        so = pe + 24 + optsz
        self.sections = {}
        for i in range(nsec):
            o = so + 40 * i
            name = self.raw[o:o + 8].rstrip(b"\0").decode("ascii", "replace")
            vsz, va, rawsz, rawoff = struct.unpack_from("<IIII", self.raw, o + 8)
            self.sections[name] = (self.imagebase + va, vsz, rawoff, rawsz)

    def text_span(self):
        va, vsz, rawoff, rawsz = self.sections[".text"]
        return va, vsz, rawoff, rawsz

    def word(self, va: int):
        tva, tvsz, traw, trawsz = self.text_span()
        off = va - tva
        if off < 0 or off + 4 > trawsz:
            return None
        return struct.unpack_from(">I", self.raw, traw + off)[0]


def decode_bl(image: Image, lo: int, hi: int):
    """Every `bl <rel>` target in [lo,hi).  Hand-written: cannot halt early."""
    out = []
    tva, tvsz, traw, trawsz = image.text_span()
    for va in range(lo, hi, 4):
        off = va - tva
        if off < 0 or off + 4 > trawsz:
            break
        w = struct.unpack_from(">I", image.raw, traw + off)[0]
        if (w >> 26) != 18:
            continue
        if (w & 1) != 1 or ((w >> 1) & 1) != 0:   # LK=1 (call), AA=0 (relative)
            continue
        li = w & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        out.append(((va + li) & 0xFFFFFFFF, va))
    return out


# --------------------------------------------------------------- splits.txt

HEADING_RE = re.compile(r"^(\S.*?):\s*$")
RANGE_RE = re.compile(r"^\s+(\.\w+)\s+start:(0x[0-9A-Fa-f]+)\s+end:(0x[0-9A-Fa-f]+)")


def parse_splits(path: Path):
    """unit heading (FULL PATH, exactly as written) -> list of (start,end) .text.

    The heading is kept verbatim.  707 headings are bare basenames and 569 are
    path-qualified; collapsing them with basename() is the defect that broke
    four consecutive lanes' scans, so it is not done anywhere in this file.
    """
    units = collections.OrderedDict()
    cur = None
    in_sections = False
    for line in path.read_text().splitlines():
        if line.startswith("Sections:"):
            in_sections = True
            continue
        m = HEADING_RE.match(line)
        if m:
            cur = m.group(1)
            in_sections = False
            units.setdefault(cur, [])
            continue
        if in_sections or cur is None:
            continue
        m = RANGE_RE.match(line)
        if m and m.group(1) == ".text":
            units[cur].append((int(m.group(2), 16), int(m.group(3), 16)))
    return units


class PinIndex:
    """Address -> pinned unit, by bisection over .text ranges."""

    def __init__(self, units):
        pts = []
        for unit, ranges in units.items():
            for lo, hi in ranges:
                pts.append((lo, hi, unit))
        pts.sort()
        self.lo = [p[0] for p in pts]
        self.pts = pts
        self.overlaps = []
        for i in range(len(pts) - 1):
            if pts[i][1] > pts[i + 1][0]:
                self.overlaps.append((pts[i], pts[i + 1]))

    def unit_of(self, addr: int):
        i = bisect.bisect_right(self.lo, addr) - 1
        if i < 0:
            return None
        lo, hi, unit = self.pts[i]
        return unit if lo <= addr < hi else None


# ------------------------------------------------------------------ asm (.s)

FN_RE = re.compile(r"^\.fn\s+(\S+?),")
ENDFN_RE = re.compile(r"^\.endfn")
INSN_RE = re.compile(r"^/\*\s*[0-9A-F]{8}\s+[0-9A-F]+\s+(?:[0-9A-F]{2} ){3}[0-9A-F]{2}\s*\*/")
FNADDR_RE = re.compile(r"^fn_([0-9A-Fa-f]{8})$")


def parse_asm(asm_dir: Path):
    """fn start addr -> (size_bytes, asm-relative unit path).

    Extents come from the `.fn fn_<ADDR>` label plus a count of instruction
    lines x 4.  The `.s` ADDRESS COLUMN IS NEVER READ: dtk synthesizes it as
    `first_block_start + cumulative section offset` for multi-block units, so it
    names addresses the function does not live at.
    """
    size = {}
    unit = {}
    dup = []
    for p in sorted(asm_dir.rglob("*.s")):
        u = str(p.relative_to(asm_dir))[:-2]
        cur = None
        n = 0
        for raw in p.read_text(errors="replace").splitlines():
            ls = raw.strip()
            m = FN_RE.match(ls)
            if m:
                if cur is not None:
                    size[cur] = n * 4
                a = FNADDR_RE.match(m.group(1))
                cur = int(a.group(1), 16) if a else None
                n = 0
                if cur is not None:
                    if cur in unit:
                        dup.append((cur, unit[cur], u))
                    unit[cur] = u
                continue
            if ENDFN_RE.match(ls):
                if cur is not None:
                    size[cur] = n * 4
                cur = None
                n = 0
                continue
            if cur is not None and INSN_RE.match(ls):
                n += 1
        if cur is not None:
            size[cur] = n * 4
    return size, unit, dup


# ------------------------------------------------------------------ dc3 map

DC3_ROW_RE = re.compile(
    r"^\s+[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{8})\s+\S*\s*(\S+)\s*$"
)


def parse_dc3_map(path: Path):
    """mangled name -> object file token, from `Publics by Value`."""
    out = {}
    started = False
    for line in path.read_text(errors="replace").splitlines():
        if "Publics by Value" in line:
            started = True
            continue
        if not started:
            continue
        m = DC3_ROW_RE.match(line)
        if not m:
            continue
        name, _rva, obj = m.group(1), m.group(2), m.group(3)
        out.setdefault(name, obj)
    return out


# ------------------------------------------------------------------- naming

ANON_NS_RE = re.compile(r"\?A0x[0-9a-fA-F]+@")
TEMPLATE_RE = re.compile(r"\?\$")


def is_anon_ns(name: str) -> bool:
    return bool(name) and bool(ANON_NS_RE.search(name))


def is_template(name: str) -> bool:
    return bool(name) and bool(TEMPLATE_RE.search(name))


ANON_HASH_RE = re.compile(r"\?A0x([0-9a-fA-F]+)@")


def anon_scope_hash(name: str):
    """The anon-namespace hash iff it is the SYMBOL'S OWN scope.

    ⛔⛔ THE "ONE TU = ONE ANON HASH" MODEL IS REFUTED ON THIS MAP -- MEASURED,
    AND IT WAS THIS TOOL'S OWN FOUNDING PREMISE (lane W15-A, 2026-09-14).
    In principle MSVC derives `?A0x<hash>` per TU, so a hash should be a TU
    fingerprint.  Measured here it is not: **10 of 39 units carrying file-static
    anon symbols carry TWO OR MORE DISTINCT hashes**, and `BandMachineMgr.cpp`
    carries **SIX** for methods of the same two classes (`SyncMachineMsg`,
    `SyncLocalMachineMsg`) at ADJACENT addresses 0x825C1F28..0x825C36D8 -- which
    one TU's emission cannot produce.

    The reason is structural and unfixable: retail `band.exe` HAS NO SYMBOL
    TABLE, so no `?A0x` hash in `scripts/target_symbol_map.json` was ever read
    from retail.  Each was inherited from whichever ORACLE supplied that name
    (DC3 bindiff, the rb3-Wii oracle, fingerprint_match).  Hash EQUALITY is
    therefore a statement about the oracle's TU layout, NOT about retail's, and
    it cannot be validated against retail even in principle.

    ⇒ hash grouping is CORROBORATION ONLY.  It is kept because it is a cheap way
    to SURFACE candidates, and it is reported, never asserted.

    ★ What survives, and what every verdict in this tool actually rests on, is a
    property of the NAME'S STRUCTURE and not of the hash's VALUE: a function in
    an anonymous namespace has INTERNAL LINKAGE, so the linker cannot resolve a
    reference to it from another TU.  Therefore ITS CALLER IS IN ITS TU.  That
    argument is unaffected by the refutation above, and it is what proved
    0x82529890 (see commit "0x82529890 is Joypad_Xbox.cpp").

    ⛔ BUT ONLY IF THE HASH IS THE SYMBOL'S OWN SCOPE.  In
    `??$__destroy_range_aux@V?$reverse_iterator@PAULabel@?A0x81ddebd1@@...` the
    hash belongs to the TYPE ARGUMENT `Label`, and the function is an STL
    template COMDAT that is byte-identical for every POD `T`.  The linker FOLDS
    those and keeps one arbitrary spelling, so their addresses say nothing
    about any TU.  Treating them as evidence is the `_icf_arbitrary` mistake.

    Discriminated on the qualified-name prefix (up to the first `@@`): if a
    template opener `?$` precedes the hash there, the function is a template
    instantiation -> return None.
    """
    m = ANON_HASH_RE.search(name or "")
    if not m:
        return None
    end = name.find("@@")
    prefix = name[:end] if end >= 0 else name
    if m.start() > len(prefix):
        return None                      # hash lives in the type/arg portion
    t = prefix.find("?$")
    if 0 <= t < m.start():
        return None                      # template instantiation -> foldable
    return m.group(1)


def anon_hash_groups(c):
    """hash -> members, and the subset whose members span >1 pinned unit."""
    groups = collections.defaultdict(list)
    for a, nm in c.name.items():
        if a in c.unassignable:
            continue
        h = anon_scope_hash(nm)
        if h:
            groups[h].append((a, nm))
    split = []
    for h, mem in groups.items():
        units = {c.fn_unit.get(a) for a, _ in mem}
        units.discard(None)
        if len(units) > 1:
            split.append((h, mem, units))
    split.sort(key=lambda t: -len(t[1]))
    return groups, split


def is_thunkish(name: str) -> bool:
    if not name:
        return False
    return name.startswith(("??_G", "??_E", "??_9", "??_7", "??_8", "??_B", "??__E", "??__F"))


# ------------------------------------------------------------------- corpus

def unit_kind(unit):
    if unit is None:
        return "none"
    if unit.startswith("auto_"):
        return "auto"
    if unit.startswith("xdk/"):
        return "xdk"
    return "real"


class Corpus:
    def __init__(self, args):
        self.args = args
        root = Path(args.root)
        self.image = Image(root / args.exe)
        self.units = parse_splits(root / args.splits)
        self.pin = PinIndex(self.units)
        self.size, self.asm_unit, self.asm_dup = parse_asm(root / args.asm)
        raw_map = json.loads((root / args.map).read_text())
        self.name = {}
        for k, v in raw_map.items():
            if k.startswith("0x") and isinstance(v, str):
                self.name[int(k, 16)] = v
        # ⛔ The map DECLARES which of its own names are not assignable.  An
        # `_icf_arbitrary` name is one arbitrary pick among linker-folded twins
        # and a `_bijection_arbitrary` name was assigned by a bijection over a
        # reloc-masked byte-identical class: the BYTES are true, WHICH NAME sits
        # on WHICH VA is not established.  Reading either as TU evidence reads
        # the linker's coin-flip as a fact.  (Measured: one of this lane's first
        # hits, 0x82654260 ?Register@KickPlayerMsg@?A0x9d4e879f@@, is
        # `_icf_arbitrary` -- it would have been briefed as a proof.)
        self.unassignable = set()
        for key in ("_icf_arbitrary", "_bijection_arbitrary", "_denylist",
                    "_denylist_unadjudicated"):
            for a in raw_map.get(key, []) or []:
                if isinstance(a, str) and a.startswith("0x"):
                    self.unassignable.add(int(a, 16))
        self.dc3 = {}
        if args.dc3_map and Path(args.dc3_map).exists():
            self.dc3 = parse_dc3_map(Path(args.dc3_map))

        tva, tvsz, _, _ = self.image.text_span()
        self.text_lo, self.text_hi = tva, tva + tvsz
        self.starts = sorted(a for a in self.asm_unit if tva <= a < tva + tvsz)
        self.start_set = set(self.starts)

        # Extent: the unit's own carve (instruction count).  Tiling is kept only
        # as a diagnostic -- where they disagree, two units' claims interleave.
        self.extent = {}
        self.tiled = {}
        for i, a in enumerate(self.starts):
            nxt = self.starts[i + 1] if i + 1 < len(self.starts) else self.text_hi
            self.tiled[a] = nxt
            sz = self.size.get(a, 0)
            self.extent[a] = a + sz if sz > 0 else nxt

        # Pinned unit per function, and the asm's own opinion (must agree).
        # The asm path carries no extension while the splits heading does, so
        # the two are compared on the extension-stripped FULL PATH.  (Comparing
        # them raw reports ~86% "disagreement" -- a control that cannot pass,
        # which is worse than no control at all.)
        def stem(u):
            return u.rsplit(".", 1)[0] if u and "." in u.rsplit("/", 1)[-1] else u

        self.fn_unit = {}
        self.attr_disagree = []
        for a in self.starts:
            pu = self.pin.unit_of(a)
            au = self.asm_unit.get(a)
            self.fn_unit[a] = pu if pu is not None else au
            if pu is not None and au is not None and stem(pu) != stem(au):
                self.attr_disagree.append((a, pu, au))

        self._build_callgraph()

    def _build_callgraph(self):
        self.callees = collections.defaultdict(set)     # fn -> {callee fn}
        self.callers = collections.defaultdict(set)     # fn -> {caller fn}
        self.unresolved = 0
        self.edges = 0
        for a in self.starts:
            hi = min(self.extent[a], self.tiled[a])
            for tgt, _site in decode_bl(self.image, a, hi):
                self.edges += 1
                if tgt in self.start_set:
                    if tgt != a:
                        self.callees[a].add(tgt)
                        self.callers[tgt].add(a)
                else:
                    self.unresolved += 1
        self.fanin = {t: len(v) for t, v in self.callers.items()}


# ------------------------------------------------------------------- signals

class Finding:
    __slots__ = ("fn", "unit", "dest", "kind", "callee", "callee_name",
                 "fanin", "fn_name")

    def __init__(self, fn, unit, dest, kind, callee, callee_name, fanin, fn_name):
        self.fn, self.unit, self.dest, self.kind = fn, unit, dest, kind
        self.callee, self.callee_name = callee, callee_name
        self.fanin, self.fn_name = fanin, fn_name


def analyse(c: Corpus, fn_unit=None, max_fanin=20):
    """Per-function findings.  `fn_unit` may be overridden (used by --selftest)."""
    fu = c.fn_unit if fn_unit is None else fn_unit
    findings = []
    suppressed = collections.Counter()
    for f in c.starts:
        u = fu.get(f)
        if u is None:
            continue
        for callee in c.callees.get(f, ()):
            v = fu.get(callee)
            if v is None or v == u:
                continue
            nm = c.name.get(callee, "")
            fi = c.fanin.get(callee, 0)
            # ---- rarity filter (L2-BODYTRIAGE): ICF fold survivors have
            # hundreds of callers and are evidence of nothing.
            if fi >= max_fanin:
                suppressed[(nm or "fn_%08X" % callee, fi)] += 1
                continue
            other = {fu.get(x) for x in c.callers.get(callee, ()) if x != f}
            other.discard(None)
            if is_anon_ns(nm) and callee not in c.unassignable:
                kind = "ANON_NS"            # PROOF: internal linkage crossed
            elif other == {v}:
                kind = "SOLE_FOREIGN_CALLER"
            elif not other:
                kind = "FANIN_1_FOREIGN"
            else:
                continue                    # >=2 foreign caller units => public
            findings.append(
                Finding(f, u, v, kind, callee, nm, fi, c.name.get(f, ""))
            )
    return findings, suppressed


STRENGTH = {"ANON_NS": 3, "SOLE_FOREIGN_CALLER": 2, "FANIN_1_FOREIGN": 1}


def group_blocks(c: Corpus, findings, fu=None):
    """Aggregate to (unit -> dest) claims and to contiguous address blocks."""
    fu = c.fn_unit if fu is None else fu
    by_fn = collections.defaultdict(list)
    for fd in findings:
        by_fn[fd.fn].append(fd)
    claims = collections.defaultdict(lambda: {"fns": set(), "kinds": collections.Counter()})
    for fn, fds in by_fn.items():
        dests = collections.Counter(fd.dest for fd in fds)
        # a function's claim is the destination its private-shaped callees agree on
        dest, _n = dests.most_common(1)[0]
        if len([d for d, n in dests.items() if n == _n]) > 1:
            pass  # ambiguous plurality still recorded; strength handles it
        key = (fu[fn], dest)
        claims[key]["fns"].add(fn)
        for fd in fds:
            if fd.dest == dest:
                claims[key]["kinds"][fd.kind] += 1
    return by_fn, claims


def dc3_verdict(c: Corpus, fn):
    """Independent TU opinion for a NON-TEMPLATE symbol, or None."""
    nm = c.name.get(fn, "")
    if not nm or is_template(nm) or is_thunkish(nm) or is_anon_ns(nm):
        return None
    obj = c.dc3.get(nm)
    if not obj:
        return None
    return obj.split(":")[-1]


# ------------------------------------------------------------------ controls

def load_report_units(root: Path, version="45410914"):
    """report unit -> (matched_functions, total_functions).  Numerics in
    report.json are protobuf-JSON: several are STRINGS and defaults are OMITTED,
    so every read is int()-coerced with a default."""
    p = root / "build" / version / "report.json"
    if not p.exists():
        return {}
    rep = json.loads(p.read_text())
    out = {}
    for u in rep.get("units", []):
        m = u.get("measures", {}) or {}
        out[u.get("name", "")] = (
            int(m.get("matched_functions", 0) or 0),
            int(m.get("total_functions", 0) or 0),
        )
    return out


def heading_to_report_unit(heading: str) -> str:
    stem = heading.rsplit(".", 1)[0] if "." in heading else heading
    return "default/" + stem


def run_controls(c: Corpus, findings, out):
    root = Path(c.args.root)
    p = out.append
    p("## CONTROLS")
    p("")
    n = len(c.starts)
    p(f"functions with `.fn fn_<ADDR>` labels inside .text : {n}")
    p(f"splits-vs-asm attribution disagreements            : {len(c.attr_disagree)}")
    p(f"duplicate .fn labels across units                  : {len(c.asm_dup)}")
    p(f"overlapping .text pin ranges                       : {len(c.pin.overlaps)}")
    p(f"bl edges decoded from retail bytes                 : {c.edges}")
    p(f"  of which callee is a known fn start              : {c.edges - c.unresolved}"
      f" ({100.0*(c.edges-c.unresolved)/max(1,c.edges):.2f}%)")
    p(f"map rows (address -> mangled name)                 : {len(c.name)}")
    p(f"dc3 ham_xbox_r.map symbols                         : {len(c.dc3)}")
    p("")

    # (a) units already at 100% must be ~clean
    rep = load_report_units(root)
    at100 = {u for u, (m, t) in rep.items() if t > 0 and m == t}
    fired_units = collections.Counter()
    for fd in findings:
        fired_units[fd.unit] += 1
    hits_in_100 = []
    for h in c.units:
        ru = heading_to_report_unit(h)
        if ru in at100 and fired_units.get(h):
            hits_in_100.append((h, fired_units[h]))
    p(f"### (a) units already at 100% (mpn ruler): {len(at100)}")
    p(f"units at 100% that are ALSO pinned and fire >=1 finding: "
      f"{len(hits_in_100)}  (expect ~0)")
    for h, k in sorted(hits_in_100, key=lambda x: -x[1])[:15]:
        p(f"    {h}  findings={k}")
    p("")
    return at100


def run_priors(c: Corpus, out):
    """(b) reproduce the recorded priors, verbatim ranges from CLAUDE.md."""
    p = out.append
    p("### (b) recorded priors")
    p("")
    QLO, QHI = 0x82A6D168, 0x82B54190          # measured /Od (Quazal) band
    inband = collections.Counter()
    for a in c.starts:
        if QLO <= a < QHI:
            u = c.fn_unit.get(a)
            if u and unit_kind(u) == "real" and not u.startswith("network/"):
                inband[u] += 1
    p(f"non-`network/` real units claiming functions inside the /Od band "
      f"0x{QLO:08X}-0x{QHI:08X}: {len(inband)}")
    for u, k in inband.most_common(20):
        lo = min(a for a in c.starts if QLO <= a < QHI and c.fn_unit.get(a) == u)
        hi = max(a for a in c.starts if QLO <= a < QHI and c.fn_unit.get(a) == u)
        p(f"    {k:3d}  {u}   0x{lo:08X}..0x{hi:08X}")
    ones = [u for u, k in inband.items() if k == 1]
    p(f"  units claiming EXACTLY ONE function there: {len(ones)}")
    p("")
    return inband


# ------------------------------------------------------------------ selftest

def selftest(c: Corpus) -> int:
    """Positive AND negative leg.  A one-leg test cannot discriminate: a
    detector that fires on everything passes a positive-only test, and one that
    fires on nothing passes a negative-only test."""
    fail = 0

    def check(ok, label, detail=""):
        nonlocal fail
        print(f"  [{'PASS' if ok else 'FAIL'}] {label}{(' -- ' + detail) if detail else ''}")
        if not ok:
            fail += 1

    # vacuity guards: a fresh worktree's reflinked objs are pre-renamer and a
    # name lookup would read absent, making every negative meaningless.
    check(len(c.name) > 20000, "map name population", f"{len(c.name)} rows")
    check(c.edges > 100000, "call graph non-vacuous", f"{c.edges} bl edges")
    anon = [a for a in c.starts if is_anon_ns(c.name.get(a, ""))]
    check(len(anon) > 50, "anon-namespace population non-empty", f"{len(anon)} fns")

    # find a clean witness: F calls an anon-ns callee in F's OWN unit
    witness = None
    for f in c.starts:
        u = c.fn_unit.get(f)
        if u is None or unit_kind(u) != "real":
            continue
        for callee in c.callees.get(f, ()):
            nm = c.name.get(callee, "")
            if is_anon_ns(nm) and c.fn_unit.get(callee) == u \
               and c.fanin.get(callee, 0) < 20:
                witness = (f, callee, u)
                break
        if witness:
            break
    check(witness is not None, "found an intra-unit anon-ns witness")
    if witness is None:
        return 1
    f, callee, u = witness
    print(f"    witness: fn_{f:08X} -> fn_{callee:08X} ({c.name[callee][:60]}) in {u}")

    # NEGATIVE leg: unsabotaged, this witness must NOT fire
    base, _ = analyse(c, max_fanin=c.args.max_fanin)
    fired = [x for x in base if x.fn == f and x.callee == callee]
    check(not fired, "negative leg: clean tree does not flag the witness")

    # POSITIVE leg: move F's pin to a foreign real unit -> must fire ANON_NS
    other = next(x for x in c.units
                 if unit_kind(x) == "real" and x != u
                 and any(c.fn_unit.get(a) == x for a in c.starts))
    sab = dict(c.fn_unit)
    sab[f] = other
    sfind, _ = analyse(c, fn_unit=sab, max_fanin=c.args.max_fanin)
    hit = [x for x in sfind if x.fn == f and x.callee == callee and x.kind == "ANON_NS"]
    check(bool(hit), "positive leg: sabotaged pin IS detected",
          f"re-pinned fn_{f:08X} to {other}")

    print(f"\nselftest: {'OK' if not fail else str(fail) + ' FAILURE(S)'}")
    return 1 if fail else 0


# ---------------------------------------------------------------------- main

def main(argv=None):
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--root", default=".", help="repo/worktree root")
    ap.add_argument("--exe", default="orig/45410914/band.exe")
    ap.add_argument("--splits", default="config/45410914/splits.txt")
    ap.add_argument("--asm", default="build/45410914/asm")
    ap.add_argument("--map", default="scripts/target_symbol_map.json")
    ap.add_argument("--dc3-map",
                    default="/home/free/code/milohax/dc3-decomp/orig/373307D9/ham_xbox_r.map")
    ap.add_argument("--max-fanin", type=int, default=20,
                    help="callees with fan-in >= this are suppressed as ICF/"
                         "common-pool noise (L2-BODYTRIAGE calibration)")
    ap.add_argument("--min-strength", type=int, default=2,
                    help="minimum claim strength to list as a suspect")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--json", help="write findings to this path")
    ap.add_argument("--out", help="write the report to this path")
    args = ap.parse_args(argv)

    c = Corpus(args)
    if args.selftest:
        return selftest(c)

    findings, suppressed = analyse(c, max_fanin=args.max_fanin)
    by_fn, claims = group_blocks(c, findings)

    out = []
    a = out.append
    a("# CIRCULAR-PIN CENSUS -- does a function's callee set agree with its unit?")
    a("")
    run_controls(c, findings, out)
    run_priors(c, out)

    a("### suppressed as ICF / common-pool noise (fan-in >= %d)" % args.max_fanin)
    a(f"distinct suppressed callees: {len(suppressed)}; "
      f"suppressed cross-unit edges: {sum(suppressed.values())}")
    for (nm, fi), k in suppressed.most_common(10):
        a(f"    {k:6d} edges  fan-in {fi:4d}  {nm[:78]}")
    a("")

    a("## S0 -- ANON-HASH GROUPING (CORROBORATION ONLY -- premise refuted, see "
      "anon_scope_hash docstring: 10 of 39 units carry >=2 hashes)")
    a("")
    groups, split = anon_hash_groups(c)
    a(f"file-static anon-ns hashes (symbol's own scope, unassignable excluded): {len(groups)}")
    a(f"hashes whose members span MORE THAN ONE pinned unit: {len(split)}")
    a("")
    for h, mem, units in split:
        a(f"#### `?A0x{h}` -- {len(mem)} symbols across {len(units)} units")
        for addr, nm in sorted(mem):
            a(f"- `0x{addr:08X}` sz=0x{c.size.get(addr,0):x} "
              f"fanin={c.fanin.get(addr,0)} **{c.fn_unit.get(addr)}**  `{nm[:88]}`")
        a("")

    a("## FINDINGS")
    a("")
    kinds = collections.Counter(f.kind for f in findings)
    a(f"total findings: {len(findings)}  ({dict(kinds)})")
    a(f"functions implicated: {len(by_fn)}")
    a("")

    rows = []
    for (unit, dest), d in claims.items():
        if unit_kind(unit) != "real":
            kindtag = "ATTRIB" if unit_kind(unit) == "auto" else unit_kind(unit).upper()
        else:
            kindtag = "MISPIN" if unit_kind(dest) == "real" else "OTHER"
        strength = sum(STRENGTH[k] * n for k, n in d["kinds"].items())
        rows.append((strength, len(d["fns"]), unit, dest, kindtag, d))
    rows.sort(key=lambda r: (-r[0], -r[1]))

    a("### claims (unit -> destination implied by private-shaped callees)")
    a("")
    a("| strength | fns | pinned unit | implied unit | class | kinds |")
    a("|---:|---:|---|---|---|---|")
    for strength, nfn, unit, dest, tag, d in rows:
        if strength < args.min_strength:
            continue
        a(f"| {strength} | {nfn} | `{unit}` | `{dest}` | {tag} | "
          f"{dict(d['kinds'])} |")
    a("")

    a("### per-function detail for MISPIN-class claims")
    a("")
    for strength, nfn, unit, dest, tag, d in rows:
        if tag != "MISPIN" or strength < args.min_strength:
            continue
        a(f"#### `{unit}` -> `{dest}`  (strength {strength}, {nfn} fn)")
        for fn in sorted(d["fns"]):
            nm = c.name.get(fn, "")
            dv = dc3_verdict(c, fn)
            a(f"- fn_{fn:08X} sz=0x{c.size.get(fn,0):x} `{nm[:70]}`"
              + (f"  [dc3 obj: {dv}]" if dv else ""))
            for fd in by_fn[fn]:
                a(f"    - {fd.kind} -> fn_{fd.callee:08X} fanin={fd.fanin} "
                  f"`{fd.callee_name[:64]}` in `{fd.dest}`")
        a("")

    text = "\n".join(out)
    if args.out:
        Path(args.out).write_text(text)
    else:
        print(text)
    if args.json:
        Path(args.json).write_text(json.dumps([{
            "fn": "%08X" % f.fn, "unit": f.unit, "dest": f.dest, "kind": f.kind,
            "callee": "%08X" % f.callee, "callee_name": f.callee_name,
            "fanin": f.fanin, "fn_name": f.fn_name,
        } for f in findings], indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())
