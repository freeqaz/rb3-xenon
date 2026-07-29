#!/usr/bin/env python3
"""byte_locate.py -- COMPILE-AND-BYTE-SEARCH TU locator.

Inverts the decomp pipeline for TUs that have NO positional signal (no selective
string literals, no own-class RTTI, 100%-claimed spans):

        port (speculatively)  ->  compile (UNPINNED)  ->  LOCATE  ->  pin

Instead of asking "where does the string/vtable evidence say this TU lives?", it
asks "where in retail `.text` do the *bytes we just compiled* actually occur?".
Under `/O1` with no LTCG the compiler is deterministic enough that a correctly
ported function is frequently byte-identical to retail **modulo relocations** --
laneBD's `UIProxy` port hit 13 such functions on the first compile, laneBL's
`UIGridProvider` 17 of 26.  This turns that observation into an instrument.

HOW IT WORKS
------------
1. Parse our compiled COFF `.obj`.  For every code symbol, slice the body
   between consecutive symbols in its section (so EH funclets come out as their
   own bodies, matching retail's per-funclet `.pdata` records).
2. Build a MASK: every 4-byte instruction word that carries a COFF relocation is
   masked out wholesale.  This is deliberately the *same, conservative*
   convention `size_order_automap.py` uses (mask the full word, not just the
   operand field) so results are directly comparable across the tool stack; it
   over-masks, which costs precision, never recall.
3. Search retail `.text` (from `orig/45410914/band.exe`, the DECOMPRESSED retail
   PE, imagebase 0x82000000) for a position where every UNMASKED word matches.
   Implementation: take the longest contiguous unmasked run as an anchor, use
   `bytes.find` to enumerate its occurrences, then verify the remaining runs.
4. Aggregate the per-function hits into a positional hypothesis: N functions of
   one TU landing in one contiguous `.text` neighbourhood *is* the TU's location.
   Emit a proposed `splits.txt` span.

WHAT IT IS NOT
--------------
It is not a similarity metric.  A hit is an exact masked-byte identity, so the
failure mode is not "wrong-ish", it is (a) MISS -- the port diverges from retail
by >=1 unmasked byte -- or (b) COLLISION -- the body is genuinely present at more
than one address because the linker's ICF folded it, or because two distinct
templates over same-sized elements produce reloc-masked byte twins (laneBL
Sec 4.2).  Both are reported, never silently resolved.

CALIBRATION (`calibrate` mode)
------------------------------
Ground truth = the unit's own pinned `.text` span(s) in `config/45410914/splits.txt`.
A located VA is CORRECT iff it lies inside the unit's pinned span.  Reported:
  * precision(unique)  -- of the functions where the tool commits to exactly one
                          VA, the fraction that are inside the true span.
  * recall             -- fraction of the obj's functions that got >=1 hit.
  * chance baseline    -- |span| / |.text|, i.e. the probability a VA drawn
                          uniformly from `.text` would land in the true span.
  * rotation control   -- an EMPIRICAL false-positive rate.  Each body has its
                          instruction sequence cyclically rotated by one word
                          (mask rotated with it), which preserves length, mask
                          density, opcode histogram and register usage but is
                          not a real function.  Any hit is a false positive.

USAGE
-----
    # locate one freshly compiled, unpinned obj
    byte_locate.py locate --obj build/45410914/src/system/beatmatch/KeyboardController.obj

    # locate every obj of a unit path, several at once
    byte_locate.py locate --unit system/beatmatch/KeyboardController --json out.json

    # positive control + chance baseline + rotation control
    byte_locate.py calibrate --units system/ui/UIGridProvider,system/utl/LogFile
    byte_locate.py calibrate --auto 25          # 25 random already-pinned units

    # the FP rate of the matcher alone, at this mask convention
    byte_locate.py control --auto 40

Read-only.  Never writes splits.txt, objects.json or the symbol map; it prints a
PROPOSED span for a human to land.
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
from collections import defaultdict
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# ---------------------------------------------------------------------------
# Paths
# ---------------------------------------------------------------------------
PROJECT_ROOT = Path(__file__).resolve().parents[2]
BUILD = PROJECT_ROOT / "build" / "45410914"
BANDEXE = PROJECT_ROOT / "orig" / "45410914" / "band.exe"
SPLITS = PROJECT_ROOT / "config" / "45410914" / "splits.txt"

IMAGE_BASE = 0x82000000
IMAGE_SCN_CNT_CODE = 0x20

# A word is "information" only if it is unmasked.  Below these thresholds the
# body cannot discriminate and is reported UNSEARCHABLE rather than guessed at.
DEFAULT_ANCHOR_MIN = 12      # bytes: shortest contiguous unmasked run usable as
                             # a `bytes.find` seed
DEFAULT_MIN_UNMASKED = 24    # bytes: total unmasked payload required to accept
                             # any hit at all
MAX_HITS = 4096              # runaway guard for pathologically generic bodies


# ---------------------------------------------------------------------------
# Retail PE
# ---------------------------------------------------------------------------
class Retail:
    """Decompressed retail PE: `.text` bytes plus the `.pdata` function table."""

    def __init__(self, path: Path = BANDEXE):
        data = path.read_bytes()
        pe = struct.unpack_from("<I", data, 0x3C)[0]
        nsec = struct.unpack_from("<H", data, pe + 6)[0]
        optsz = struct.unpack_from("<H", data, pe + 20)[0]
        off = pe + 24 + optsz
        self.secs = {}
        for i in range(nsec):
            e = data[off + 40 * i: off + 40 * i + 40]
            name = e[:8].rstrip(b"\0").decode("latin1")
            vs, va, rs, pr = struct.unpack_from("<IIII", e, 8)
            self.secs[name] = (IMAGE_BASE + va, vs, pr, rs)
        tva, tvs, tpr, trs = self.secs[".text"]
        self.text_va = tva
        self.text = data[tpr: tpr + min(tvs, trs)]
        # .pdata: big-endian RUNTIME_FUNCTION pairs (BeginAddress, packed word).
        pva, pvs, ppr, prs = self.secs[".pdata"]
        fns: List[Tuple[int, int]] = []
        for i in range(pvs // 8):
            a, w1 = struct.unpack_from(">II", data, ppr + 8 * i)
            if a == 0:
                continue
            ln = ((w1 >> 8) & 0x3FFFFF) * 4
            if ln == 0:
                continue
            fns.append((a, a + ln))
        fns.sort()
        self.fns = fns
        self.fstart = [a for a, _ in fns]
        self._selfcheck()
        self._call_targets: Optional[set] = None

    # ★★ VA -> FILE OFFSET IS **NOT** `va - 0x82000000`.
    # `.text` is RVA 0x00270000 but PointerToRawData 0x00264E00 -- a 0xB200
    # delta.  The naive mapping happens to be right for `.rdata` (where
    # PointerToRawData == VirtualAddress) and WRONG for `.text`; a sibling lane
    # disassembled the wrong bytes and published a refutation it had to retract.
    # This class reads `.text` as data[PointerToRawData : +size] and indexes it
    # by (va - text_va), i.e. the correct per-section mapping, everywhere.
    # Anchor (assert-checked below): off(0x824DAAD0) == 0x004CF8D0.
    def file_off(self, va: int) -> int:
        return self.secs[".text"][2] + (va - self.text_va)

    def _selfcheck(self):
        got = self.file_off(0x824DAAD0)
        if got != 0x004CF8D0:
            raise SystemExit(
                "byte_locate: FATAL -- .text VA->file-offset mapping is wrong "
                "(off(0x824DAAD0)=0x%08X, expected 0x004CF8D0). Every byte read "
                "from .text would be from the wrong location." % got)

    def fn_of(self, va: int) -> Optional[Tuple[int, int]]:
        i = bisect.bisect_right(self.fstart, va) - 1
        if i < 0:
            return None
        a, b = self.fns[i]
        return (a, b) if a <= va < b else None

    def is_fn_start(self, va: int) -> bool:
        i = bisect.bisect_left(self.fstart, va)
        return i < len(self.fstart) and self.fstart[i] == va

    # ★★ `.pdata` ABSENCE IS NOT A "NOT A FUNCTION" TEST.
    # Frameless leaf functions are systematically absent from the X360 `.pdata`
    # table, so anchoring candidate entry points on `.pdata` alone is
    # STRUCTURALLY BLIND to every frameless leaf -- exactly the small-function
    # population that a 40-byte-class body would live in.  The conclusive entry
    # proof for such a function is that something CALLS it, so we enumerate
    # `bl` targets across the whole of `.text` as a second, independent source
    # of entry points.
    def call_targets(self) -> set:
        if self._call_targets is None:
            tgts = set()
            t = self.text
            base = self.text_va
            for off in range(0, len(t) - 3, 4):
                w = (t[off] << 24) | (t[off + 1] << 16) | (t[off + 2] << 8) | t[off + 3]
                if (w >> 26) != 18 or not (w & 1) or (w & 2):
                    continue            # want bl, relative (AA=0), LK=1
                disp = w & 0x03FFFFFC
                if disp & 0x02000000:
                    disp -= 0x04000000
                tv = base + off + disp
                if base <= tv < base + len(t):
                    tgts.add(tv)
            self._call_targets = tgts
        return self._call_targets

    def is_entry(self, va: int, use_calls: bool) -> bool:
        if self.is_fn_start(va):
            return True
        return bool(use_calls) and va in self.call_targets()


_RETAIL: Optional[Retail] = None


def retail() -> Retail:
    global _RETAIL
    if _RETAIL is None:
        _RETAIL = Retail()
    return _RETAIL


# ---------------------------------------------------------------------------
# splits.txt -- claim table + ground truth for calibration
# ---------------------------------------------------------------------------
def load_claims() -> List[Tuple[int, int, str]]:
    claims: List[Tuple[int, int, str]] = []
    unit = None
    for line in SPLITS.read_text().splitlines():
        if not line.strip() or line.lstrip().startswith("#"):
            continue
        if not line[0].isspace() and line.rstrip().endswith(":"):
            unit = line.strip()[:-1]
            continue
        t = line.strip()
        if t.startswith(".text"):
            m = re.search(r"start:(0x[0-9a-fA-F]+)\s+end:(0x[0-9a-fA-F]+)", t)
            if m and unit:
                claims.append((int(m.group(1), 16), int(m.group(2), 16), unit))
    claims.sort()
    return claims


_CLAIMS: Optional[List[Tuple[int, int, str]]] = None
_CSTART: List[int] = []


def claims() -> List[Tuple[int, int, str]]:
    global _CLAIMS, _CSTART
    if _CLAIMS is None:
        _CLAIMS = load_claims()
        _CSTART = [c[0] for c in _CLAIMS]
    return _CLAIMS


def claim_of(va: int) -> Optional[str]:
    claims()
    i = bisect.bisect_right(_CSTART, va) - 1
    if i < 0:
        return None
    a, b, u = _CLAIMS[i]
    return u if a <= va < b else None


def spans_of_unit(unit: str) -> List[Tuple[int, int]]:
    """`unit` may be a splits key ('default/system/utl/LogFile.cpp') or a bare
    path stem ('system/utl/LogFile')."""
    out = []
    for a, b, u in claims():
        if u == unit or _stem(u) == _stem(unit):
            out.append((a, b))
    return sorted(out)


def _stem(u: str) -> str:
    u = u.strip()
    for pre in ("default/",):
        if u.startswith(pre):
            u = u[len(pre):]
    for ext in (".cpp", ".obj", ".c"):
        if u.endswith(ext):
            u = u[:-len(ext)]
            break
    return u


# ---------------------------------------------------------------------------
# COFF obj -> per-function masked bodies
# ---------------------------------------------------------------------------
def parse_coff(path: Path):
    """-> (data, secs, syms).  `secs[n]['sel']` is the COMDAT Selection byte,
    which is the load-bearing discriminator for this tool:

        1 = IMAGE_COMDAT_SELECT_NO_DUPLICATES  -> the function is DEFINED BY THIS
            .cpp (a normal out-of-line definition under /Gy).  Retail's copy of it
            can only have come from this TU, so it MUST lie inside the TU's span.
        2 = IMAGE_COMDAT_SELECT_ANY            -> vague linkage: an inline member,
            a template instantiation, or a compiler-generated `??_G`/`??__E`.
            Every TU that uses it emits a copy and the linker keeps ONE, chosen
            arbitrarily -- so retail's copy legitimately lives in SOME OTHER TU's
            span.  A hit on one of these is a true location of a real body but
            says nothing about where THIS TU is.

    Measured on UIGridProvider.obj: 14 sections sel=1, 129 sel=2.  Scoring
    without this split is what made the first calibration read 0.67 precision --
    the "wrong" answers were vague-linkage bodies correctly located in the TU
    that actually supplied them."""
    data = path.read_bytes()
    nsec = struct.unpack_from("<H", data, 2)[0]
    symoff = struct.unpack_from("<I", data, 8)[0]
    nsym = struct.unpack_from("<I", data, 12)[0]
    optsz = struct.unpack_from("<H", data, 16)[0]
    str_start = symoff + nsym * 18
    secs = {}
    base = 20 + optsz
    for i in range(nsec):
        o = base + i * 40
        raw_size = struct.unpack_from("<I", data, o + 16)[0]
        praw = struct.unpack_from("<I", data, o + 20)[0]
        preloc = struct.unpack_from("<I", data, o + 24)[0]
        nreloc = struct.unpack_from("<H", data, o + 32)[0]
        chars = struct.unpack_from("<I", data, o + 36)[0]
        relocs = [struct.unpack_from("<I", data, preloc + j * 10)[0]
                  for j in range(nreloc)]
        relocs_full = [struct.unpack_from("<IIH", data, preloc + j * 10)
                       for j in range(nreloc)]
        secs[i + 1] = dict(raw_size=raw_size, praw=praw, relocs=relocs,
                           relocs_full=relocs_full, chars=chars, sel=0)
    syms = []
    i = 0
    while i < nsym:
        o = symoff + i * 18
        nm = data[o:o + 8]
        if nm[:4] == b"\0\0\0\0":
            so = struct.unpack_from("<I", nm, 4)[0]
            e = data.index(b"\0", str_start + so)
            name = data[str_start + so:e].decode("latin1")
        else:
            name = nm.split(b"\0")[0].decode("latin1")
        val = struct.unpack_from("<I", data, o + 8)[0]
        secn = struct.unpack_from("<h", data, o + 12)[0]
        typ = struct.unpack_from("<H", data, o + 14)[0]
        sc = data[o + 16]
        naux = data[o + 17]
        # section-definition symbol: its aux record carries the COMDAT selection
        if sc == 3 and typ == 0 and naux >= 1 and secn > 0 and secn in secs:
            secs[secn]["sel"] = data[symoff + (i + 1) * 18 + 14]
        syms.append(dict(name=name, val=val, secn=secn, typ=typ, sc=sc,
                         idx=i))
        i += 1 + naux
    return data, secs, syms


PAD_WORDS = (b"\x00\x00\x00\x00", b"\x60\x00\x00\x00")  # zero fill / `nop`


# ---------------------------------------------------------------------------
# Tree-wide definition multiplicity
#
# COMDAT selection alone is NOT a sufficient ownership test.  Measured on
# ViewSetting.obj: 452 sections carry Selection=1, but several of those bodies
# (`PropSync(String&,...)`, the whole `CriticalUserListener` group) are ALSO
# defined by another .cpp in our tree -- our headers define some functions
# out-of-line, which never surfaces because we do not link.  Retail's single
# copy of such a body can have come from either TU, so it cannot place this one.
#
# So: a body is TU-OWNED for locating purposes iff it is COMDAT Selection=1
# AND our tree defines it in exactly one obj.
# ---------------------------------------------------------------------------
DEFMAP_CACHE = BUILD / "byte_locate_defmap.json"


def build_defmap(rebuild: bool = False) -> Dict[str, int]:
    """★ STALENESS FOOTGUN: this map is only as good as the tree's build
    completeness.  It is built from `build/45410914/src/**/*.obj`, so in a
    partially-built worktree it UNDER-counts duplicate definitions, which makes
    the tool OVER-claim ownership and silently costs precision.  Always run a
    full `./tools/ninja-locked` first, and rebuild the cache (`rebuild=True`)
    after adding objs.  The cache records the obj count it was built from and
    warns on stderr if the tree has grown since."""
    nobj = sum(1 for _ in (BUILD / "src").rglob("*.obj"))
    if DEFMAP_CACHE.exists() and not rebuild:
        try:
            blob = json.loads(DEFMAP_CACHE.read_text())
            counts = blob.get("counts", blob)
            built_from = blob.get("nobj")
            if built_from is not None and built_from != nobj:
                print("byte_locate: WARNING -- defmap cache was built from %d "
                      "objs, tree now has %d; rerun with a full build and "
                      "delete %s" % (built_from, nobj, DEFMAP_CACHE),
                      file=sys.stderr)
            return counts
        except Exception:
            pass
    counts: Dict[str, int] = defaultdict(int)
    root = BUILD / "src"
    for o in sorted(root.rglob("*.obj")):
        try:
            data, secs, syms = parse_coff(o)
        except Exception:
            continue
        names = set()
        for sy in syms:
            if (sy["typ"] == 0x20 and sy["sc"] in (2, 3) and sy["secn"] > 0
                    and sy["secn"] in secs
                    and secs[sy["secn"]]["chars"] & IMAGE_SCN_CNT_CODE):
                names.add(sy["name"])
        for n in names:
            counts[n] += 1
    out = {k: v for k, v in counts.items() if v > 1}   # only the ambiguous ones
    DEFMAP_CACHE.write_text(json.dumps(dict(nobj=nobj, counts=out)))
    return out


_DEFMAP: Optional[Dict[str, int]] = None


def defmap() -> Dict[str, int]:
    global _DEFMAP
    if _DEFMAP is None:
        _DEFMAP = build_defmap()
    return _DEFMAP


class Func:
    __slots__ = ("name", "size", "body", "masked_words", "obj", "sel")

    def __init__(self, name: str, body: bytes, masked_words: set, obj: str,
                 sel: int = 0):
        self.name = name
        self.body = body
        self.size = len(body)
        self.masked_words = masked_words
        self.obj = obj
        self.sel = sel          # COMDAT Selection; 1 == TU-owned definition

    @property
    def is_funclet(self) -> bool:
        """MSVC X360 emits a separate unwind/catch funclet per EH scope.  There
        are 39,735 of them in retail; the overwhelming majority are 32-48 byte
        `subi r31,r12,N` / member-teardown shapes that are mask-identical binary
        wide.  They can neither anchor nor be scored -- measured as ~87% of this
        tool's residual per-function errors before they were excluded."""
        n = self.name
        return (n.startswith("__unwind$") or n.startswith("__catch$")
                or n.startswith("__ehhandler$") or n.startswith("__tryblocktable")
                or n.startswith("__unwindfunclet$"))

    @property
    def owned(self) -> bool:
        """True iff retail's copy of this body can ONLY have come from this TU:
        a Selection=1 COMDAT that no other obj in our tree also defines, and not
        an EH funclet."""
        return (self.sel == 1 and not self.is_funclet
                and defmap().get(self.name, 1) == 1)

    # --- derived views -----------------------------------------------------
    @property
    def nwords(self) -> int:
        return self.size // 4

    def unmasked_runs(self) -> List[Tuple[int, int]]:
        """[(byte_off, byte_len)] of maximal unmasked stretches."""
        runs = []
        start = None
        for w in range(self.nwords):
            if w in self.masked_words:
                if start is not None:
                    runs.append((start * 4, (w - start) * 4))
                    start = None
            elif start is None:
                start = w
        if start is not None:
            runs.append((start * 4, (self.nwords - start) * 4))
        return runs

    @property
    def unmasked_bytes(self) -> int:
        return sum(n for _, n in self.unmasked_runs())

    def mutated(self, rng) -> Optional["Func"]:
        """Flip ONE bit inside ONE unmasked instruction word.  This is the
        adversarial control: length, mask, prologue, opcode stream and every
        other word are untouched, so a hit means the matcher tolerated a
        one-instruction difference -- which it must not."""
        cand = [w for w in range(self.nwords) if w not in self.masked_words]
        if not cand:
            return None
        w = rng.choice(cand)
        b = bytearray(self.body)
        # flip a bit in the register/immediate half, not the opcode field
        b[4 * w + 3] ^= 1 << rng.randrange(8)
        return Func(self.name + "#mut", bytes(b), set(self.masked_words),
                    self.obj, self.sel)

    def rotated(self, k: int = 1) -> "Func":
        """Cyclically rotate the instruction sequence (and its mask) by k words.
        Same length, same mask density, same opcode/register histogram -- but not
        a real function.  Used as the negative control."""
        n = self.nwords
        if n == 0:
            return self
        words = [self.body[4 * i:4 * i + 4] for i in range(n)]
        rot = words[k % n:] + words[:k % n]
        newmask = {(w - k) % n for w in self.masked_words}
        return Func(self.name + "#rot%d" % k, b"".join(rot), newmask, self.obj,
                    self.sel)


def obj_functions(path: Path, trim_pad: bool = True) -> List[Func]:
    data, secs, syms = parse_coff(path)
    by_sec: Dict[int, List[dict]] = defaultdict(list)
    for sy in syms:
        if (sy["secn"] > 0 and sy["secn"] in secs
                and sy["typ"] == 0x20 and sy["sc"] in (2, 3)):
            if secs[sy["secn"]]["chars"] & IMAGE_SCN_CNT_CODE:
                by_sec[sy["secn"]].append(sy)
    out: List[Func] = []
    objname = path.name
    for secn in sorted(by_sec):
        s = secs[secn]
        members = sorted(by_sec[secn], key=lambda x: x["val"])
        for k, sy in enumerate(members):
            start = sy["val"]
            end = members[k + 1]["val"] if k + 1 < len(members) else s["raw_size"]
            if s["praw"] == 0 or end <= start:
                continue
            body = bytearray(data[s["praw"] + start:s["praw"] + end])
            masked = set()
            for rva in s["relocs"]:
                if start <= rva < end:
                    off = rva - start
                    masked.add(off // 4)
            # Trailing COMDAT alignment fill is not part of the function and does
            # not survive into retail's layout; drop it (masked words at the tail
            # are dropped too).
            if trim_pad:
                while len(body) >= 4 and bytes(body[-4:]) in PAD_WORDS \
                        and (len(body) // 4 - 1) not in masked:
                    del body[-4:]
                masked = {w for w in masked if w < len(body) // 4}
            if len(body) < 4:
                continue
            out.append(Func(sy["name"], bytes(body), masked, objname,
                            s.get("sel", 0)))
    return out


# ---------------------------------------------------------------------------
# The search
# ---------------------------------------------------------------------------
class Locator:
    def __init__(self, anchor_min: int = DEFAULT_ANCHOR_MIN,
                 min_unmasked: int = DEFAULT_MIN_UNMASKED,
                 pdata_aligned: bool = True, use_calls: bool = True):
        self.r = retail()
        self.anchor_min = anchor_min
        self.min_unmasked = min_unmasked
        self.pdata_aligned = pdata_aligned
        self.use_calls = use_calls

    def search(self, f: Func) -> Tuple[str, List[int]]:
        """-> (status, [VAs]).  status in
           OK | UNSEARCHABLE_LOWINFO | UNSEARCHABLE_NOANCHOR | SATURATED"""
        runs = f.unmasked_runs()
        if not runs:
            return ("UNSEARCHABLE_NOANCHOR", [])
        if f.unmasked_bytes < self.min_unmasked:
            return ("UNSEARCHABLE_LOWINFO", [])
        runs.sort(key=lambda r: -r[1])
        aoff, alen = runs[0]
        if alen < self.anchor_min:
            return ("UNSEARCHABLE_NOANCHOR", [])
        anchor = f.body[aoff:aoff + alen]
        rest = runs[1:]
        text = self.r.text
        base = self.r.text_va
        L = f.size
        hits: List[int] = []
        pos = 0
        while True:
            o = text.find(anchor, pos)
            if o < 0:
                break
            pos = o + 4          # instructions are 4-aligned; step one word
            s = o - aoff
            if s < 0 or s + L > len(text) or (s & 3):
                continue
            ok = True
            for roff, rlen in rest:
                if text[s + roff:s + roff + rlen] != f.body[roff:roff + rlen]:
                    ok = False
                    break
            if not ok:
                continue
            va = base + s
            if self.pdata_aligned and not self.r.is_entry(va, self.use_calls):
                continue
            hits.append(va)
            if len(hits) >= MAX_HITS:
                return ("SATURATED", hits)
        return ("OK", hits)


# ---------------------------------------------------------------------------
# Clustering -> positional hypothesis
# ---------------------------------------------------------------------------
MAX_ANCHOR_HITS = 4     # a body present at more than this many addresses is an
                        # ICF fold / masked twin and carries no positional info


def cluster(hits_by_fn: Dict[str, List[int]],
            meta: Dict[str, Func],
            gap: int = 0x2000,
            max_anchor_hits: int = MAX_ANCHOR_HITS):
    """Group hit VAs into neighbourhoods and score each one.

    Rationale: a single byte-identical body can be an ICF fold of an unrelated
    function, but N functions of one TU do not independently fold into one
    small window by accident.  laneBL Sec 3.5: the tell is ISLAND DISTANCE.

    Scoring is deliberately NOT "number of hits":
      * a body found at > `max_anchor_hits` addresses is not an anchor at all --
        `??_G` vector-deleting destructors and `__unwind$` funclets are
        mask-identical across the whole binary and would otherwise dominate;
      * an anchor whose COMDAT selection is 1 (OWNED, defined by this .cpp) is
        the only kind of evidence that can place THIS TU.  Vague-linkage anchors
        corroborate but cannot decide.
    Rank key = (#owned anchors, #anchors, summed 1/multiplicity weight)."""
    pts: List[Tuple[int, str]] = []
    for fn, vas in hits_by_fn.items():
        for va in vas:
            pts.append((va, fn))
    pts.sort()
    groups: List[List[Tuple[int, str]]] = []
    cur: List[Tuple[int, str]] = []
    for va, fn in pts:
        if cur and va - cur[-1][0] > gap:
            groups.append(cur)
            cur = []
        cur.append((va, fn))
    if cur:
        groups.append(cur)
    out = []
    for g in groups:
        fns = {fn for _, fn in g}
        anchors = {fn for fn in fns if len(hits_by_fn[fn]) <= max_anchor_hits}
        owned = {fn for fn in anchors if meta[fn].owned}
        weight = sum(1.0 / len(hits_by_fn[fn]) for _, fn in g)
        # Span extent comes from UNIQUE anchors only.  A body with 2-4 hits is
        # still admissible as evidence THAT the TU is here (its other copies are
        # ICF folds elsewhere), but letting it set an endpoint stretches the span
        # onto a sibling's fold -- measured on KeyboardController, whose dtor is
        # folded with RealGuitarController's and JoypadGuitarController's and
        # dragged the proposed hi 0x1500 past the truth.
        uvas = sorted(va for va, fn in g
                      if fn in anchors and len(hits_by_fn[fn]) == 1)
        avas = uvas if len(uvas) >= 2 else sorted(
            va for va, fn in g if fn in anchors)
        out.append(dict(lo=g[0][0], hi=max(va for va, _ in g),
                        anchor_lo=(avas[0] if avas else g[0][0]),
                        anchor_hi=(avas[-1] if avas else g[-1][0]),
                        nfns=len(fns), nhits=len(g),
                        nanchor=len(anchors), nowned=len(owned),
                        weight=round(weight, 3),
                        owned_fns=sorted(owned),
                        anchor_fns=sorted(anchors)))
    out.sort(key=lambda d: (-d["nowned"], -d["nanchor"], -d["weight"]))
    return out


def span_from_cluster(c: dict, r: Retail) -> Tuple[int, int]:
    """Proposed `.text` span: the ANCHOR extent (never the noise extent),
    extended to whole `.pdata` function boundaries."""
    lo, hi = c["anchor_lo"], c["anchor_hi"]
    fl = r.fn_of(lo)
    if fl:
        lo = fl[0]
    fh = r.fn_of(hi)
    if fh:
        hi = fh[1]
    return lo, hi


# ---------------------------------------------------------------------------
# Near-miss diagnosis
#
# ★ THE FALSE-NEGATIVE RATE IS DOMINATED BY OUR SOURCE, NOT BY RETAIL.
# A MISS means our compiled body differs from retail by >= 1 unmasked byte.  For
# a speculative port off the rb3-Wii DEV oracle that is usually a SOURCE-DIALECT
# DIVERGENCE, not absence -- so a bare "0 hits" line throws away the most useful
# thing the search learned.  This turns each miss into a divergence lead: find
# the retail function in the proposed span with the longest shared masked prefix
# AND suffix, and report the size delta and the byte offset where they part.
#
# Dialect classes that are known to miss by construction (laneBO6, measured):
#   * `Load`/`PreLoad`/`PostLoad` written in the `d.` / `BinStreamRev` dialect --
#     0 matched, 58 failed, zero counterexamples.  Retail constructs plain
#     `BinStream` in Load bodies; `BinStreamRev` is real but only for NESTED
#     loaders (`PropKeys::Load`, `ObjVector`/`ObjList` `operator>>`).  Normalise
#     to `bs >>` / `gRev` / `gAltRev` or discount these bodies as evidence.
#   * `Handle` where `obj/ObjMacros.h` is included AFTER `obj/Object.h`: its bare
#     `END_HANDLERS` (plain `return`) overrides Object.h's `_warn`/`PathName(this)`
#     form and the retail tail `clrlwi./beq/bl PathName/li 6` goes missing.
#     (An explicit `HANDLE_CHECK(line)` restores that arm, and its `line_num` /
#     `__FILE__` are comma-discarded, so Wii line numbers cost nothing.)
#   * `Save` -- a Wii `SAVE_OBJ` is frequently a real `SAVE_REVS` body in retail.
# ---------------------------------------------------------------------------
DIALECT_RISK = (
    ("Load", "BinStreamRev-vs-BinStream dialect (0/58 matched -- normalise first)"),
    ("PreLoad", "BinStreamRev-vs-BinStream dialect"),
    ("PostLoad", "BinStreamRev-vs-BinStream dialect"),
    ("Save", "Wii SAVE_OBJ is often a real SAVE_REVS body in retail"),
    ("Handle", "END_HANDLERS PathName(this) tail / handler-arm list"),
    ("SyncProperty", "local-static Symbol spelling is codegen-load-bearing"),
)

# A 316-byte retail body is an OBJ_SET_TYPE `SetType` -- 91 distinct classes, no
# other family shares that size (laneBO6, verified on the 79-opcode sequence).
SETTYPE_SIZE = 316


def dialect_of(name: str) -> Optional[str]:
    for key, why in DIALECT_RISK:
        if ("?%s@" % key) in name:
            return why
    return None


def _common_masked(a: bytes, b: bytes, mask: set, frm_end: bool) -> int:
    """Words of agreement from one end, skipping masked words."""
    n = min(len(a), len(b)) // 4
    k = 0
    for i in range(n):
        w = (len(a) // 4 - 1 - i) if frm_end else i
        wb = (len(b) // 4 - 1 - i) if frm_end else i
        if w in mask:
            k += 1
            continue
        if a[4 * w:4 * w + 4] != b[4 * wb:4 * wb + 4]:
            break
        k += 1
    return k


def nearmiss(f: Func, lo: int, hi: int, r: Retail):
    """Best partial counterpart for a MISSED body inside [lo,hi)."""
    best = None
    for a, b in r.fns:
        if not (lo <= a < hi):
            continue
        body = r.text[a - r.text_va: b - r.text_va]
        pre = _common_masked(f.body, body, f.masked_words, False)
        suf = _common_masked(f.body, body, f.masked_words, True)
        score = pre + suf
        if best is None or score > best[0]:
            best = (score, a, b - a, pre, suf)
    return best


# ---------------------------------------------------------------------------
# Reporting helpers
# ---------------------------------------------------------------------------
def resolve_objs(args) -> List[Path]:
    out: List[Path] = []
    if args.obj:
        for o in args.obj:
            p = Path(o)
            if not p.is_absolute():
                p = PROJECT_ROOT / o
            out.append(p)
    for u in (args.unit or []):
        stem = _stem(u)
        p = BUILD / "src" / (stem + ".obj")
        out.append(p)
    return out


def run_locate(args) -> dict:
    r = retail()
    loc = Locator(args.anchor_min, args.min_unmasked, not args.free,
                  not args.no_call_entries)
    result = dict(objs=[], clusters=[], functions=[])
    hits_by_fn: Dict[str, List[int]] = {}
    meta: Dict[str, Func] = {}
    missed: List[Func] = []
    stats = defaultdict(int)
    for p in resolve_objs(args):
        if not p.exists():
            print("MISSING OBJ: %s" % p, file=sys.stderr)
            continue
        result["objs"].append(str(p))
        for f in obj_functions(p, trim_pad=not args.no_trim):
            status, hits = loc.search(f)
            tag = "owned" if f.owned else "vague"
            stats[status] += 1
            stats["n_" + tag] += 1
            if status == "OK":
                stats["hit" if hits else "miss"] += 1
                if hits:
                    stats[tag + "_hit"] += 1
                    stats["unique" if len(hits) == 1 else "multi"] += 1
            key = f.obj + "!" + f.name
            if hits:
                hits_by_fn[key] = hits
                meta[key] = f
            result["functions"].append(dict(
                obj=f.obj, name=f.name, size=f.size, owned=f.owned,
                unmasked=f.unmasked_bytes, status=status,
                hits=["0x%08X" % v for v in hits[:16]], nhits=len(hits),
                claims=[claim_of(v) for v in hits[:16]]))
            if f.owned and not hits:
                missed.append(f)
    cl = cluster(hits_by_fn, meta, gap=args.gap,
                 max_anchor_hits=args.max_anchor_hits)
    for c in cl:
        lo, hi = span_from_cluster(c, r)
        c2 = dict(c)
        for k in ("lo", "hi", "anchor_lo", "anchor_hi"):
            c2[k] = "0x%08X" % c[k]
        c2["proposed_span"] = "0x%08X..0x%08X" % (lo, hi)
        owners = defaultdict(int)
        va = lo
        while va < hi:
            f = r.fn_of(va)
            if not f:
                va += 4
                continue
            owners[claim_of(va) or "<UNCLAIMED>"] += f[1] - f[0]
            va = f[1]
        c2["claimers"] = sorted(((n, "0x%X" % b) for n, b in owners.items()),
                                key=lambda t: -int(t[1], 16))
        result["clusters"].append(c2)
    result["stats"] = dict(stats)
    # ---- divergence worklist: what MISSED, and how close it came -------------
    nm = []
    if result["clusters"] and missed:
        top = result["clusters"][0]
        lo, hi = (int(top["proposed_span"].split("..")[0], 16),
                  int(top["proposed_span"].split("..")[1], 16))
        pad = args.nearmiss_pad
        for f in sorted(missed, key=lambda x: -x.size):
            if f.size < 32:
                continue        # a stub cannot near-miss anything meaningfully
            best = nearmiss(f, lo - pad, hi + pad, r)
            if not best:
                continue
            score, va, rsize, pre, suf = best
            if pre + suf < 4:
                continue        # no shared end -- not a near-miss, just absent
            nm.append(dict(name=f.name, size=f.size, best_va="0x%08X" % va,
                           retail_size=rsize, delta=f.size - rsize,
                           words_pre=pre, words_suf=suf,
                           dialect=dialect_of(f.name),
                           settype=(rsize == SETTYPE_SIZE)))
    # Rank by how close the miss came: exact size first, then shared end words.
    nm.sort(key=lambda d: (d["delta"] != 0, -(d["words_pre"] + d["words_suf"])))
    result["nearmiss"] = nm
    return result


def print_locate(res: dict, verbose: bool):
    st = res["stats"]
    tot = sum(v for k, v in st.items()
              if k.startswith("UNSEARCHABLE") or k in ("OK", "SATURATED"))
    print("=" * 78)
    print("objs: %s" % ", ".join(os.path.basename(o) for o in res["objs"]))
    print("functions: %d  (TU-owned %d / vague-linkage %d)"
          % (tot, st.get("n_owned", 0), st.get("n_vague", 0)))
    print("searchable: %d   hit: %d (owned %d / vague %d; unique %d / multi %d)   miss: %d"
          % (st.get("OK", 0), st.get("hit", 0), st.get("owned_hit", 0),
             st.get("vague_hit", 0), st.get("unique", 0),
             st.get("multi", 0), st.get("miss", 0)))
    print("unsearchable: lowinfo %d  noanchor %d  saturated %d"
          % (st.get("UNSEARCHABLE_LOWINFO", 0),
             st.get("UNSEARCHABLE_NOANCHOR", 0), st.get("SATURATED", 0)))
    print("-" * 78)
    print("CLUSTERS ranked by TU-OWNED anchors (an anchor is a body found at "
          "<= %d addresses)" % MAX_ANCHOR_HITS)
    for c in res["clusters"][:12]:
        if c["nanchor"] == 0:
            continue
        print("  owned=%-2d anchors=%-3d w=%-7s  %s  hits=%d"
              % (c["nowned"], c["nanchor"], c["weight"],
                 c["proposed_span"], c["nhits"]))
        print("       claimers: %s"
              % ", ".join("%s(%s)" % (n, b) for n, b in c["claimers"][:6]))
        if verbose:
            for fn in c["owned_fns"]:
                print("       OWNED  %s" % fn.split("!", 1)[-1][:100])
            for fn in c["anchor_fns"]:
                if fn not in c["owned_fns"]:
                    print("       vague  %s" % fn.split("!", 1)[-1][:100])
    if res.get("nearmiss"):
        print("-" * 78)
        print("DIVERGENCE WORKLIST -- TU-owned bodies that MISSED, with their")
        print("closest retail counterpart inside the proposed span.  A miss is")
        print("evidence of a SOURCE-DIALECT divergence, not of absence.")
        for d in res["nearmiss"][:20]:
            tag = " [SetType-316]" if d["settype"] else ""
            print("  %-6d vs %-6d (%+d)  %s  pre=%dw suf=%dw%s"
                  % (d["size"], d["retail_size"], d["delta"], d["best_va"],
                     d["words_pre"], d["words_suf"], tag))
            print("        %s" % d["name"][:96])
            if d["dialect"]:
                print("        ^ DIALECT RISK: %s" % d["dialect"])
        deltas = [d["delta"] for d in res["nearmiss"] if abs(d["delta"]) > 4]
        if len(deltas) >= 3:
            from statistics import median
            md = median(deltas)
            near = [x for x in deltas if abs(x - md) <= 16]
            if len(near) >= 3:
                print("  ** %d of %d misses share a ~%+d byte delta -- that is ONE"
                      % (len(near), len(deltas), int(md)))
                print("     shared source divergence, not N separate bugs. Fix it once.")
    if verbose:
        print("-" * 78)
        print("PER-FUNCTION")
        for f in sorted(res["functions"], key=lambda d: (not d["owned"], d["nhits"])):
            if not f["nhits"]:
                continue
            print("  %s %-6d %-5d n=%-3d %s"
                  % ("OWN" if f["owned"] else "vag", f["size"], f["unmasked"],
                     f["nhits"], f["name"][:88]))
            for h, cl in list(zip(f["hits"], f["claims"]))[:6]:
                print("           %s  %s" % (h, cl or "<UNCLAIMED>"))


# ---------------------------------------------------------------------------
# Calibration
# ---------------------------------------------------------------------------
def pinned_units_with_objs() -> List[str]:
    out = []
    seen = set()
    for a, b, u in claims():
        s = _stem(u)
        if s in seen:
            continue
        seen.add(s)
        if (BUILD / "src" / (s + ".obj")).exists():
            out.append(s)
    return out


def calibrate(args):
    r = retail()
    loc = Locator(args.anchor_min, args.min_unmasked, not args.free,
                  not args.no_call_entries)
    units = list(args.units or [])
    if args.auto:
        pool = pinned_units_with_objs()
        random.seed(args.seed)
        random.shuffle(pool)
        units += pool[:args.auto]
    text_len = len(r.text)

    tot_fn = tot_searchable = tot_hit = tot_unique = 0
    uniq_correct = uniq_wrong = 0
    any_correct = 0
    chance_num = 0.0
    rot_searchable = rot_hit = 0
    per_unit = []

    for u in units:
        p = BUILD / "src" / (_stem(u) + ".obj")
        if not p.exists():
            continue
        spans = spans_of_unit(u)
        if not spans:
            continue
        span_bytes = sum(b - a for a, b in spans)
        chance = span_bytes / float(text_len)

        def inside(va):
            return any(a <= va < b for a, b in spans)

        fns = obj_functions(p, trim_pad=not args.no_trim)
        u_fn = u_search = u_hit = u_uniq = u_uc = u_uw = u_ac = 0
        hits_by_fn: Dict[str, List[int]] = {}
        meta: Dict[str, Func] = {}
        for f in fns:
            if args.owned_only and not f.owned:
                continue
            u_fn += 1
            tot_fn += 1
            status, hits = loc.search(f)
            if status != "OK":
                continue
            u_search += 1
            tot_searchable += 1
            chance_num += chance
            if hits:
                hits_by_fn[f.name] = hits
                meta[f.name] = f
                u_hit += 1
                tot_hit += 1
                if any(inside(v) for v in hits):
                    u_ac += 1
                    any_correct += 1
                if len(hits) == 1:
                    u_uniq += 1
                    tot_unique += 1
                    if inside(hits[0]):
                        u_uc += 1
                        uniq_correct += 1
                    else:
                        u_uw += 1
                        uniq_wrong += 1
                        if args.show_wrong:
                            print("  WRONG %-46s 0x%08X claimed-by %s | %s"
                                  % (u[-46:], hits[0], claim_of(hits[0]),
                                     f.name[:70]))
            # negative control on the SAME body
            rf = f.rotated(1)
            rs, rh = loc.search(rf)
            if rs == "OK":
                rot_searchable += 1
                if rh:
                    rot_hit += 1
        # ---- TU-level verdict: does the top-ranked cluster overlap the truth?
        cl = cluster(hits_by_fn, meta, gap=args.gap,
                     max_anchor_hits=args.max_anchor_hits)
        cl = [c for c in cl if c["nanchor"] > 0]
        tu_ok = None
        prop = None
        if cl:
            lo, hi = span_from_cluster(cl[0], r)
            prop = "0x%08X..0x%08X" % (lo, hi)
            tu_ok = any(not (hi <= a or lo >= b) for a, b in spans)
        per_unit.append(dict(unit=u, span_bytes=span_bytes, nfn=u_fn,
                             searchable=u_search, hit=u_hit, unique=u_uniq,
                             uniq_correct=u_uc, uniq_wrong=u_uw,
                             any_correct=u_ac, chance=chance,
                             tu_ok=tu_ok, proposed=prop,
                             true_span="0x%08X..0x%08X" % (spans[0][0], spans[-1][1]),
                             top_owned=(cl[0]["nowned"] if cl else 0),
                             top_anchors=(cl[0]["nanchor"] if cl else 0)))

    print("=" * 78)
    print("CALIBRATION  (positive control: %d pinned units%s)"
          % (len(per_unit), ", TU-OWNED bodies only" if args.owned_only else ""))
    print("=" * 78)
    print("%-38s %4s %4s %4s %4s %4s %4s %s" %
          ("unit", "fns", "srch", "hit", "uniq", "u.ok", "anch", "TU"))
    for d in sorted(per_unit, key=lambda x: -x["hit"]):
        print("%-38s %4d %4d %4d %4d %4d %4d %s" %
              (d["unit"][-38:], d["nfn"], d["searchable"], d["hit"],
               d["unique"], d["uniq_correct"], d["top_owned"],
               {True: "HIT", False: "MISS", None: "-"}[d["tu_ok"]]))
    print("-" * 78)
    tu_dec = [d for d in per_unit if d["tu_ok"] is not None]
    tu_hit = [d for d in tu_dec if d["tu_ok"]]
    print("TU-LEVEL VERDICT (top-ranked cluster overlaps the true pinned span)")
    print("  units where the tool proposed a span : %d / %d"
          % (len(tu_dec), len(per_unit)))
    print("  proposal overlaps truth              : %d  (TU PRECISION %.4f)"
          % (len(tu_hit), len(tu_hit) / max(len(tu_dec), 1)))
    for d in tu_dec:
        if not d["tu_ok"]:
            print("    MISS %-34s proposed %s  true %s"
                  % (d["unit"][-34:], d["proposed"], d["true_span"]))
    print("-" * 78)
    print("functions in objs             : %d" % tot_fn)
    print("searchable (mask has payload) : %d  (%.1f%% of functions)"
          % (tot_searchable, 100.0 * tot_searchable / max(tot_fn, 1)))
    print("got >=1 hit                   : %d  (RECALL %.4f of searchable)"
          % (tot_hit, tot_hit / max(tot_searchable, 1)))
    print("committed to a UNIQUE VA      : %d" % tot_unique)
    print("  correct (inside true span)  : %d" % uniq_correct)
    print("  wrong                       : %d" % uniq_wrong)
    print("  PRECISION(unique)           : %.4f"
          % (uniq_correct / max(tot_unique, 1)))
    print("any-hit-correct / hit         : %.4f" % (any_correct / max(tot_hit, 1)))
    print("-" * 78)
    print("CHANCE BASELINE")
    print("  mean |span|/|.text| over searchable fns : %.6f"
          % (chance_num / max(tot_searchable, 1)))
    print("  i.e. a VA drawn uniformly from .text is 'correct' this often")
    print("  precision lift over chance              : %.1fx"
          % ((uniq_correct / max(tot_unique, 1))
             / max(chance_num / max(tot_searchable, 1), 1e-12)))
    print("-" * 78)
    print("NEGATIVE CONTROL (instruction-rotation: same length, same mask")
    print("density, same opcode/register histogram, not a real function)")
    print("  rotated bodies searched : %d" % rot_searchable)
    print("  rotated bodies with >=1 hit (FALSE POSITIVES) : %d" % rot_hit)
    print("  empirical FP rate       : %.6f"
          % (rot_hit / max(rot_searchable, 1)))
    if args.json:
        Path(args.json).write_text(json.dumps(dict(
            per_unit=per_unit, tot_fn=tot_fn, searchable=tot_searchable,
            hit=tot_hit, unique=tot_unique, uniq_correct=uniq_correct,
            uniq_wrong=uniq_wrong, any_correct=any_correct,
            chance=chance_num / max(tot_searchable, 1),
            rot_searchable=rot_searchable, rot_hit=rot_hit), indent=1))


def audit(args):
    """Sweep every COMPILED but UNPINNED obj and propose a location for it.

    This is free yield: those objs already exist in `build/45410914/src`, so the
    expensive half of the pipeline (port + compile) is already paid for.  Only
    objs whose winning cluster carries >= --min-owned TU-owned anchors are
    reported, which is the operating point measured in `calibrate`.

    ★ splits.txt names units inconsistently -- some entries are full paths
    (`system/beatmatch/BeatMatchUtl.cpp`), some are bare basenames
    (`TrackWatcherImpl.cpp`).  The pinned set is therefore taken as
    {full stem} UNION {basename}, which can only ever call an unpinned unit
    'pinned' (conservative), never the reverse.  Do not invert this join --
    a bare-basename join in the other direction is the documented
    `live_units.py` defect."""
    r = retail()
    loc = Locator(args.anchor_min, args.min_unmasked, not args.free,
                  not args.no_call_entries)
    pinned = set()
    for _, _, u in claims():
        s = _stem(u)
        pinned.add(s)
        pinned.add(s.rsplit("/", 1)[-1])
    objs = sorted((BUILD / "src").rglob("*.obj"))
    rows = []
    nswept = 0
    for o in objs:
        stem = str(o.relative_to(BUILD / "src"))[:-4]
        if stem in pinned or stem.rsplit("/", 1)[-1] in pinned:
            continue
        nswept += 1
        try:
            fns = obj_functions(o)
        except Exception:
            continue
        hits_by_fn: Dict[str, List[int]] = {}
        meta: Dict[str, Func] = {}
        nowned = 0
        for f in fns:
            if not f.owned:
                continue
            nowned += 1
            st, h = loc.search(f)
            if st == "OK" and h:
                hits_by_fn[f.name] = h
                meta[f.name] = f
        if not hits_by_fn:
            continue
        cl = cluster(hits_by_fn, meta, gap=args.gap,
                     max_anchor_hits=args.max_anchor_hits)
        cl = [c for c in cl if c["nowned"] >= args.min_owned]
        if not cl:
            continue
        c = cl[0]
        lo, hi = span_from_cluster(c, r)
        owners = defaultdict(int)
        va = lo
        while va < hi:
            f = r.fn_of(va)
            if not f:
                va += 4
                continue
            owners[claim_of(va) or "<UNCLAIMED>"] += f[1] - f[0]
            va = f[1]
        rows.append(dict(unit=stem, owned_fns=nowned, anchors=c["nanchor"],
                         owned_anchors=c["nowned"],
                         span="0x%08X..0x%08X" % (lo, hi), size=hi - lo,
                         second=(cl[1]["nowned"] if len(cl) > 1 else 0),
                         claimers=sorted(((n, b) for n, b in owners.items()),
                                         key=lambda t: -t[1])[:4]))
    rows.sort(key=lambda d: -d["owned_anchors"])
    print("%-46s %5s %5s  %-24s %s" %
          ("unit", "own", "anch", "proposed .text span", "claimers"))
    for d in rows:
        print("%-46s %5d %5d  %-24s %s"
              % (d["unit"][-46:], d["owned_fns"], d["owned_anchors"],
                 d["span"],
                 ", ".join("%s(0x%X)" % (n, b) for n, b in d["claimers"])))
    print("\n%d of %d unpinned compiled objs placed with >= %d TU-owned anchors"
          % (len(rows), nswept, args.min_owned))
    if args.json:
        Path(args.json).write_text(json.dumps(rows, indent=1))


# ---------------------------------------------------------------------------
# Phantom pre-flight
# ---------------------------------------------------------------------------
WII_SRC = Path("/home/free/code/milohax/rb3/src")
_LIT = re.compile(r'"((?:[^"\\\n]|\\.){4,})"')
_CLS = re.compile(r'^\s*(?:class|struct)\s+([A-Za-z_]\w*)\s*(?::|\{)', re.M)


OUR_SRC = PROJECT_ROOT / "src"


def classes_from_obj(path: Path) -> set:
    """Class names a compiled obj DEFINES, read from its MSVC mangled symbols.

    ★ This is the authoritative source of a TU's class names.  A Milo TU is
    frequently named after none of the classes it declares -- `UISyncNetMsgs`
    declares `ComponentFocusNetMsg` / `ComponentScrollNetMsg` /
    `ComponentSelectNetMsg` and no `UISyncNetMsgs` at all.  Probing the binary
    for the FILE name therefore yields a SILENT FALSE NEGATIVE, which in a
    locator is indistinguishable from "this TU is unlocatable" -- precisely the
    conclusion this tool exists to stop people drawing.  Measured: a file-name
    probe refuted `UISyncNetMsgs`, which in fact reads name=6 / rtti=3 and
    locates cleanly with 8 TU-owned anchors.
    """
    out = set()
    try:
        _d, secs, syms = parse_coff(path)
    except Exception:
        return out
    for sy in syms:
        n = sy["name"]
        if not n.startswith("?") or sy["secn"] <= 0:
            continue
        body = n[2:] if n.startswith("??") else n[1:]
        toks = body.split("@@")[0].split("@")
        if not toks:
            continue
        # `??0Class@@…` / `??1Class@@…` / `??_GClass@@…` -> toks[0] is the class
        # `?Method@Class@@…`                             -> toks[1] is the class
        # `?FreeFunc@@YA…`                               -> no class
        cand = toks[0] if n.startswith("??") else (toks[1] if len(toks) > 1 else None)
        if cand and re.match(r"^[A-Za-z_]\w*$", cand):
            out.add(cand)
    return out


_MEMBER = re.compile(r"^\s*(?:mutable\s+)?[A-Za-z_][\w:<>,\s\*&]*?"
                     r"([A-Za-z_]\w*)\s*(?:\[[^\]]*\])?\s*;\s*(?://.*)?$", re.M)


def _class_body(txt: str, cls: str) -> Optional[str]:
    m = re.search(r"(?:class|struct)\s+%s\s*(?::[^{]*)?\{" % re.escape(cls), txt)
    if not m:
        return None
    i = m.end() - 1
    depth = 0
    for j in range(i, len(txt)):
        if txt[j] == "{":
            depth += 1
        elif txt[j] == "}":
            depth -= 1
            if depth == 0:
                return txt[i:j]
    return None


def _find_decl(root: Path, cls: str):
    """-> (path, class-body text) for the first header declaring `cls`."""
    pat = re.compile(r"(?:class|struct)\s+%s\s*(?::|\{)" % re.escape(cls))
    for h in root.rglob("*.h"):
        try:
            txt = h.read_text(encoding="utf-8", errors="replace")
        except Exception:
            continue
        if pat.search(txt):
            b = _class_body(txt, cls)
            if b is not None:
                return h, b, txt
    return None, None, None


def _resolve_obj(canon: str) -> Optional[Path]:
    """Locate a compiled obj for a canonical stem, case-insensitively (the
    unlocated-TU worklist is lowercased; the build tree is not)."""
    p = BUILD / "src" / (canon + ".obj")
    if p.exists():
        return p
    parts = canon.split("/")
    d = BUILD / "src"
    for seg in parts[:-1]:
        nxt = None
        for c in (d.iterdir() if d.is_dir() else []):
            if c.is_dir() and c.name.lower() == seg.lower():
                nxt = c
                break
        if nxt is None:
            return None
        d = nxt
    if d.is_dir():
        for c in d.iterdir():
            if c.suffix == ".obj" and c.stem.lower() == parts[-1].lower():
                return c
    return None


def design(args):
    """★ FOURTH CHANNEL -- a free pre-filter that costs no compile at all.

    If our 360 class and the Wii oracle's class of the same name describe
    DIFFERENT DESIGNS, the byte search cannot succeed and the TU should be
    declined immediately with that reason recorded -- it is "our source is a
    different program", not "we have not found it yet".  Those two failures are
    otherwise indistinguishable in the output and would both land in the
    false-negative bucket.

    Measured case: RB3-360's `StoreOffer` is DataArray-driven
    (`DataArray *mStoreOfferData`); rb3-Wii's is a `#pragma pack(1)` view onto a
    packed binary blob (`mPackedData`).  Two different store implementations,
    visible from the headers alone."""
    for cls in args.cls:
        ho, bo, to = _find_decl(OUR_SRC, cls)
        hw, bw, tw = _find_decl(WII_SRC, cls)
        if bo is None or bw is None:
            print("%-34s %-9s ours=%s wii=%s" % (
                cls, "NO-PAIR", "yes" if bo is not None else "MISSING",
                "yes" if bw is not None else "MISSING"))
            continue
        mo = set(_MEMBER.findall(bo))
        mw = set(_MEMBER.findall(bw))
        inter = mo & mw
        union = mo | mw
        jac = len(inter) / max(len(union), 1)
        packed_w = "#pragma pack" in (tw or "")
        packed_o = "#pragma pack" in (to or "")
        verdict = ("DIVERGENT-DESIGN" if jac < 0.25 or packed_w != packed_o
                   else "COMPATIBLE" if jac >= 0.6 else "PARTIAL")
        print("%-34s %-17s members ours=%d wii=%d shared=%d  jaccard=%.2f%s"
              % (cls, verdict, len(mo), len(mw), len(inter), jac,
                 "  PACK-MISMATCH" if packed_w != packed_o else ""))
        if verdict == "DIVERGENT-DESIGN":
            print("     ours: %s" % ", ".join(sorted(mo - mw)[:6]))
            print("     wii : %s" % ", ".join(sorted(mw - mo)[:6]))
            print("     ^ DECLINE: a byte search cannot succeed across a design "
                  "divergence. Not 'unlocated' -- a different program.")


def _wii_files(canon: str) -> List[Path]:
    parts = canon.lower().split("/")
    d = WII_SRC.joinpath(*parts[:-1])
    out = []
    if d.is_dir():
        for f in d.iterdir():
            if f.stem.lower() == parts[-1] and f.suffix in (".cpp", ".h"):
                out.append(f)
    return out


def phantom(args):
    """★ THREE-channel absence test -- run this BEFORE spending a port.

    A TU whose class does not exist in RB3-360 retail cannot be located by any
    instrument, and porting it is pure waste.  laneBL established the test on
    two channels (class-name byte string + `.?AV` RTTI type descriptor).  THAT
    IS NOT ENOUGH and this lane measured the counterexample: `ArpeggioShape`
    reads 0 on BOTH channels yet laneBL located it at 0x82356118 on 5/5
    selective literals -- a non-polymorphic or never-name-registered class has
    no name string and no COL and is still perfectly real.

    So the third channel is decisive and must be run: do the TU's own SOURCE
    STRING LITERALS appear in the binary?  Only `ABSENT` on all three is a
    phantom verdict; two-of-three is `SUSPECT`, no more."""
    d = BANDEXE.read_bytes()
    for canon in args.tu:
        canon_case = canon
        files = _wii_files(canon)
        if not files:
            print("%-44s  NO WII SOURCE" % canon)
            continue
        classes, lits = set(), set()
        # ★ Prefer class names read from the COMPILED OBJ when one exists -- the
        # obj is what the compiler actually emitted, so it cannot disagree with
        # the source the way a header regex can.
        objp = _resolve_obj(canon_case)
        if objp is not None:
            classes |= classes_from_obj(objp)
        for f in files:
            txt = f.read_text(encoding="utf-8", errors="replace")
            # ★ HEADER ONLY for class names.  A .cpp routinely re-opens or
            # locally declares a class belonging to ANOTHER, real TU, and that
            # produces a false PRESENT -- measured: StorePackedMetadata.cpp
            # declares `StorePanel`, which is real (name=5, rtti=1) and owned by
            # StorePanel.cpp, while all 14 classes in StorePackedMetadata.h read
            # 0/0.  Scanning the .cpp would have kept a dead 124-function lead
            # alive.
            if f.suffix == ".h":
                classes |= set(_CLS.findall(txt))
            if True:
                for m in _LIT.finditer(txt):
                    v = m.group(1)
                    if (len(v) >= 6 and all(32 <= ord(c) < 127 for c in v)
                            and "%" not in v and " " not in v):
                        lits.add(v)
        if not classes:
            # NEVER fall back to probing the FILE name -- that is the silent
            # false negative this mode exists to prevent.
            print("%-44s  REFUSED: no class names derivable from the header or a "
                  "compiled obj. Probing the FILE name would be a silent false "
                  "negative -- supply --obj or port the header first." % canon)
            continue
        name_hits = sum(d.count(c.encode()) for c in classes)
        rtti_hits = sum(d.count((".?AV" + c).encode()) for c in classes)
        lit_present = [L for L in lits if d.count(L.encode()) > 0]
        chans = sum([name_hits > 0, rtti_hits > 0, len(lit_present) > 0])
        verdict = ("PHANTOM" if chans == 0 else
                   "SUSPECT" if chans == 1 else "PRESENT")
        print("%-44s %-8s name=%-4d rtti=%-3d lits=%d/%d  %s"
              % (canon, verdict, name_hits, rtti_hits, len(lit_present),
                 len(lits), ", ".join(sorted(lit_present)[:3])))
        if verdict == "SUSPECT":
            print("     ^ one channel only -- NOT a verdict. Check whether the "
                  "present literals are generic vocabulary another TU owns.")


# ---------------------------------------------------------------------------
# Reloc-target consistency -- the AT-100% defect detector
# ---------------------------------------------------------------------------
MAP_PATH = PROJECT_ROOT / "scripts" / "target_symbol_map.json"


def load_symbol_map():
    """-> (va2name, name2va).  Map keys are hex strings and 264 legacy entries
    are uppercase `0X…`, so everything is normalised case-insensitively."""
    va2name, name2va = {}, {}
    try:
        raw = json.loads(MAP_PATH.read_text())
    except Exception:
        return va2name, name2va
    for k, v in raw.items():
        if not isinstance(v, str):
            continue
        try:
            va = int(k, 16)
        except ValueError:
            continue
        va2name[va] = v
        name2va.setdefault(v, va)
    return va2name, name2va


def _bl_target(word: int, va: int, require_lk: bool = True) -> Optional[int]:
    """Absolute target of a relative branch, else None.

    ★ `require_lk=False` also accepts `b` (LK=0) -- a TAIL CALL.  The at-100%
    defect that motivated this audit is a tail call (`??1NewAwardPanel@@` tail-
    calls its base dtor), so a bl-only filter misses the entire class it was
    built to find."""
    if (word >> 26) != 18 or (word & 2):
        return None
    if require_lk and not (word & 1):
        return None
    disp = word & 0x03FFFFFC
    if disp & 0x02000000:
        disp -= 0x04000000
    return va + disp


def _same_retail_body(r: Retail, a: int, b: int) -> bool:
    """True if two retail addresses hold byte-identical code (an ICF fold)."""
    if a == b:
        return True
    fa, fb = r.fn_of(a), r.fn_of(b)
    if not fa or not fb:
        # ★ .pdata ABSENCE IS NOT "not a function" -- frameless leaves are
        # systematically absent.  Fall back to a fixed window rather than
        # silently declining to compare.
        n = 64
        return (r.text[a - r.text_va: a - r.text_va + n]
                == r.text[b - r.text_va: b - r.text_va + n])
    la, lb = fa[1] - fa[0], fb[1] - fb[0]
    if la != lb:
        return False
    return (r.text[a - r.text_va: a - r.text_va + la]
            == r.text[b - r.text_va: b - r.text_va + lb])


def relocaudit(args):
    """★★ Detect the AT-100% defect class: a function scoring 100 % against a
    retail body that is provably a DIFFERENT function.

    `functionRelocDiffs=none` masks the operands of relocated instructions, so a
    body whose only difference is *which* function it tail-calls scores a clean
    100 % and NO sub-100 scanner can ever surface it.  Worked example (laneBO6):
    `??1NewAwardPanel@@` reads 100.0 %, but ours tail-calls `??1TexLoadPanel@@`
    while retail's tail-calls `??1VoiceoverPanel@@` -- a different base class.

    The masked bytes agree; the masked RELOCATION TARGETS do not.  So: decode
    retail's actual `bl` target at each masked site, resolve it through
    `target_symbol_map.json`, and compare against the symbol OUR relocation
    names.  A contradiction is reported only when our symbol is ITSELF mapped to
    a different VA -- that rules out ICF aliasing, where two names legitimately
    share one address."""
    r = retail()
    va2name, name2va = load_symbol_map()
    if not va2name:
        print("no target_symbol_map.json -- nothing to audit")
        return
    pct = {}
    if args.only_100:
        try:
            rep = json.loads((BUILD / "report.json").read_text())
            for u in rep["units"]:
                for f in (u.get("functions") or []):
                    if f.get("fuzzy_match_percent") == 100.0:
                        pct[f["name"]] = True
        except Exception:
            pass
    objs = []
    if args.unit:
        for u in args.unit:
            q = _resolve_obj(_stem(u))
            if q:
                objs.append(q)
    else:
        objs = sorted((BUILD / "src").rglob("*.obj"))
    nfn = nsite = nbad = nsoft = 0
    # ★ A symbol defined in N objs would otherwise be audited N times and every
    # finding counted N times.  Site count is BLAST RADIUS, never defect count.
    seen_syms = set()
    bad_syms, soft_syms = set(), set()
    for o in objs:
        try:
            data, secs, syms = parse_coff(o)
        except Exception:
            continue
        idx2name = {sy["idx"]: sy["name"] for sy in syms}
        bysec = defaultdict(list)
        for sy in syms:
            if (sy["typ"] == 0x20 and sy["sc"] in (2, 3) and sy["secn"] > 0
                    and sy["secn"] in secs
                    and secs[sy["secn"]]["chars"] & IMAGE_SCN_CNT_CODE):
                bysec[sy["secn"]].append(sy)
        for secn, members in bysec.items():
            sec = secs[secn]
            members.sort(key=lambda x: x["val"])
            for k, sy in enumerate(members):
                name = sy["name"]
                if name not in name2va:
                    continue
                if args.only_100 and name not in pct:
                    continue
                start = sy["val"]
                end = (members[k + 1]["val"] if k + 1 < len(members)
                       else sec["raw_size"])
                va = name2va[name]
                if not (r.text_va <= va < r.text_va + len(r.text)):
                    continue
                if name in seen_syms:
                    continue
                seen_syms.add(name)
                nfn += 1
                bad = []
                soft = []
                for rva, symidx, rtyp in sec["relocs_full"]:
                    if not (start <= rva < end):
                        continue
                    off = rva - start
                    ow = struct.unpack_from(">I", data,
                                            sec["praw"] + start + off)[0]
                    if (ow >> 26) != 18 or (ow & 2):
                        continue        # not a relative branch (bl OR tail-b)
                    ro = va - r.text_va + off
                    if ro + 4 > len(r.text):
                        continue
                    rw = struct.unpack_from(">I", r.text, ro)[0]
                    tgt = _bl_target(rw, va + off, require_lk=False)
                    if tgt is None:
                        continue
                    nsite += 1
                    oursym = idx2name.get(symidx)
                    if not oursym or oursym.startswith("@comp.id"):
                        continue
                    theirs = va2name.get(tgt)
                    if theirs is None or theirs == oursym:
                        continue        # unknown callee, or same name
                    if oursym not in name2va:
                        # ★ NAME tier.  Our callee has no VA in the map, so the
                        # VA comparison is unavailable -- but the NAMES are still
                        # comparable, and this is the tier the motivating defect
                        # lives in: ??1NewAwardPanel@@ tail-calls
                        # ??1TexLoadPanel@@ where retail tail-calls
                        # ??1VoiceoverPanel@@, and ??1TexLoadPanel@@ is unmapped.
                        # Lower confidence than HARD: an ICF fold can put our
                        # callee and retail's at one VA under one name.
                        soft.append((off, oursym, theirs, tgt))
                        continue
                    expect = name2va[oursym]
                    if expect == tgt:
                        continue
                    # ★ DECISIVE ICF FILTER.  Two different names can be two
                    # valid names for BYTE-IDENTICAL retail bodies -- the linker
                    # folded them and simply kept one.  That is not a defect.
                    # Measured false positives without this filter:
                    # ??3@YAXPAX@Z vs ??3BinStream@@SAXPAX@Z, and
                    # ??$Find@VRndMat@@ vs ??$Find@VRndCam@@ (same-size template
                    # twins -- laneBL Sec 4.2's third fake-match mechanism).
                    # Compare the two candidate callees' actual retail bodies;
                    # only DIFFERENT code is a contradiction.
                    if _same_retail_body(r, expect, tgt):
                        continue
                    bad.append((off, oursym, theirs, tgt))
                for tier, rows in (("HARD", bad), ("NAME", soft)):
                    if not rows:
                        continue
                    if tier == "HARD":
                        nbad += 1
                        bad_syms.add(name)
                    else:
                        nsoft += 1
                        soft_syms.add(name)
                    print("%s CONTRADICTION  %s" % (tier, name[:90]))
                    print("   our VA 0x%08X (%s)%s"
                          % (va, o.name,
                             "  [100%]" if name in pct else ""))
                    for off, oursym, theirs, tgt in rows[:6]:
                        print("   +0x%03X  we call %s" % (off, oursym[:78]))
                        print("           retail calls 0x%08X = %s"
                              % (tgt, theirs[:70]))
    print("-" * 78)
    print("audited %d DISTINCT mapped functions, %d branch sites"
          % (nfn, nsite))
    print("  HARD (both callees mapped, retail bodies differ): %d functions"
          % len(bad_syms))
    print("  NAME (our callee unmapped; ICF aliasing possible): %d functions"
          % len(soft_syms))
    print("  NB these are UPPER BOUNDS needing per-name adjudication -- a hit is")
    print("  either (a) our code calls the wrong function or (b) the symbol map")
    print("  names the callee wrongly. Both are worth fixing; they are not the")
    print("  same finding, and neither is a 'defect count' until adjudicated.")
    if args.only_100:
        print("  (restricted to functions currently scoring exactly 100%)")


def control(args):
    """Negative control only, over an arbitrary sample of compiled objs."""
    loc = Locator(args.anchor_min, args.min_unmasked, not args.free,
                  not args.no_call_entries)
    pool = pinned_units_with_objs()
    random.seed(args.seed)
    random.shuffle(pool)
    rng = random.Random(args.seed)
    n_s = n_h = m_s = m_h = 0
    for u in pool[:args.auto]:
        p = BUILD / "src" / (u + ".obj")
        for f in obj_functions(p, trim_pad=not args.no_trim):
            for k in (1, 2):
                rf = f.rotated(k)
                st, hits = loc.search(rf)
                if st == "OK":
                    n_s += 1
                    if hits:
                        n_h += 1
            mf = f.mutated(rng)
            if mf is not None:
                st, hits = loc.search(mf)
                if st == "OK":
                    m_s += 1
                    if hits:
                        m_h += 1
    print("rotation control : %d searched, %d hit, FP rate %.7f"
          % (n_s, n_h, n_h / max(n_s, 1)))
    print("mutation control : %d searched, %d hit, FP rate %.7f"
          % (m_s, m_h, m_h / max(m_s, 1)))


# ---------------------------------------------------------------------------
def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    sub = ap.add_subparsers(dest="cmd", required=True)

    def common(p):
        p.add_argument("--anchor-min", type=int, default=DEFAULT_ANCHOR_MIN)
        p.add_argument("--min-unmasked", type=int, default=DEFAULT_MIN_UNMASKED)
        p.add_argument("--no-call-entries", action="store_true",
                       help="do NOT treat bl targets as entry points; .pdata "
                            "only (blind to frameless leaves)")
        p.add_argument("--free", action="store_true",
                       help="allow hits at any 4-aligned VA, not only .pdata "
                            "function starts")
        p.add_argument("--no-trim", action="store_true",
                       help="do not trim trailing COMDAT alignment padding")
        p.add_argument("--json", help="write machine-readable output here")
        p.add_argument("--seed", type=int, default=1)
        p.add_argument("--gap", type=lambda s: int(s, 0), default=0x2000,
                       help="max VA gap inside one cluster (default 0x2000)")
        p.add_argument("--max-anchor-hits", type=int, default=MAX_ANCHOR_HITS,
                       help="a body found at more addresses than this is ICF "
                            "noise and is excluded from cluster scoring")

    pl = sub.add_parser("locate")
    common(pl)
    pl.add_argument("--obj", action="append")
    pl.add_argument("--unit", action="append")
    pl.add_argument("-v", "--verbose", action="store_true")
    pl.add_argument("--nearmiss-pad", type=lambda s: int(s, 0), default=0x800,
                    help="widen the proposed span by this much when hunting a "
                         "missed body's closest retail counterpart")

    pc = sub.add_parser("calibrate")
    common(pc)
    pc.add_argument("--units", type=lambda s: s.split(","))
    pc.add_argument("--auto", type=int, default=0)
    pc.add_argument("--owned-only", action="store_true",
                    help="score only COMDAT-selection-1 bodies, i.e. functions "
                         "this .cpp actually defines (the honest ground truth: "
                         "vague-linkage bodies legitimately live in another TU)")
    pc.add_argument("--show-wrong", action="store_true")

    pa = sub.add_parser("audit")
    common(pa)
    pa.add_argument("--min-owned", type=int, default=3,
                    help="minimum TU-owned anchors in the winning cluster "
                         "before a location is proposed (default 3)")

    pp = sub.add_parser("phantom")
    common(pp)
    pp.add_argument("tu", nargs="+",
                    help="canonical Wii source stems, e.g. "
                         "system/meta/storepackedmetadata")

    pd = sub.add_parser("design")
    common(pd)
    pd.add_argument("cls", nargs="+", help="class names to compare 360 vs Wii")

    pr = sub.add_parser("relocaudit")
    common(pr)
    pr.add_argument("--unit", action="append")
    pr.add_argument("--only-100", action="store_true",
                    help="audit only functions currently scoring exactly 100%%")

    pn = sub.add_parser("control")
    common(pn)
    pn.add_argument("--auto", type=int, default=20)

    args = ap.parse_args()
    if args.cmd == "locate":
        res = run_locate(args)
        print_locate(res, args.verbose)
        if args.json:
            Path(args.json).write_text(json.dumps(res, indent=1))
    elif args.cmd == "calibrate":
        calibrate(args)
    elif args.cmd == "audit":
        audit(args)
    elif args.cmd == "phantom":
        phantom(args)
    elif args.cmd == "design":
        design(args)
    elif args.cmd == "relocaudit":
        relocaudit(args)
    elif args.cmd == "control":
        control(args)


if __name__ == "__main__":
    main()
