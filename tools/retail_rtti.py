#!/usr/bin/env python3
"""Retail big-endian PE / MSVC-RTTI library + CLI for RB3 (orig/45410914/band.exe).

WHY THIS EXISTS
---------------
At least three lanes in one session independently re-wrote a retail RTTI
resolver, and two of them baked in the *wrong* address arithmetic.  This is the
unified, controlled version.  Import it, don't re-derive it.

    from tools.retail_rtti import RetailRtti
    R = RetailRtti()                       # defaults to orig/45410914/band.exe
    R.class_of_vtable(0x820F1400)          # -> '.?AVHitSink@@'
    R.hierarchy_of_class('RndText')        # -> [(col_va, [BaseClassDescriptor,...]), ...]
    R.classes_installed_by(0x823F6198)     # -> (status, scanned, [(addr, cls), ...])
                                           #    scan bounded by retail .pdata; see
                                           #    "THE SECOND TRAP" below.

★ THE ADDRESS TRAP THIS TOOL EXISTS TO KILL
-------------------------------------------
`raw = va - 0x82000000` is **valid only for .rdata** (.rdata va=0x82000400,
rawptr=0x400 — the shortcut is exact there and NOWHERE ELSE).  MSVC puts many
TypeDescriptors in **.data** (va=0x82c64400, rawptr=0xc52000), where the true
skew is **-0x12400**.  Reading a .data TypeDescriptor with the shortcut yields
garbage bytes, the class-name lookup fails, and the caller records a *false
absence* — a verdict shaped exactly like a decisive negative.  That is the
de044702 defect.  This module resolves VA<->raw from the PE **section headers**
only; the shortcut appears nowhere except in the sabotage leg of --selftest.

Everything is pure stdlib Python (no capstone, no grep) so it is immune to the
binary-blind `grep` shim documented in CLAUDE.md.

SCOPE / OVERLAP WITH EXISTING scripts/  (checked 2026-08-02, no duplication)
---------------------------------------------------------------------------
  * scripts/rtti_probe.py + scripts/batch_rtti_probe.py — DIFFERENT SCOPE.
    They drive the **Ghidra MCP service** (port 8002) to decompile a function
    and pull the type-descriptor argument out of an `__RTDynamicCast` call,
    i.e. they recover a *template type argument T* from decompiled C.  They
    need a live Ghidra server and do not read the PE.  Not subsumed.
  * scripts/dump_vtable.py — OTHER SIDE OF THE DIFF.  It reads **our compiled
    COFF .obj** symbol+relocation tables to lay out a vtable we produced.
    This module reads the **retail PE**.  Complementary: dump_vtable answers
    "what did we emit", retail_rtti answers "what does retail contain".
  * scripts/harvest/size_order_automap.py — unrelated (byte-identity pairing);
    see tools/masked_byte_identity.py.

SUBSUMES (lane-local scratch, now retired):
  ~/tmp/laneDF1/tools/rtti.py   — section-aware decoder + full COL/CHD/BCD
                                  hierarchy walk + vtable back-reference scan.
                                  Structurally correct; taken as the base.
  ~/tmp/laneDE3/sweep.py        — vtable -> class fast path w/ memo cache.
  ~/tmp/laneDF4/rtti.py         — hardcoded .data skew (the defect); its
                                  *interface* is kept, its arithmetic replaced.
  ~/tmp/laneDG2/adjudicate.py   — lis/addi/ori address reconstruction to find
                                  which vtable a function installs.  Re-done in
                                  pure Python (it required capstone).
  ~/tmp/laneDG2/ownership.py    — vtable-membership ownership scan (which
                                  class's vtable *contains* a function).

★★ THE SECOND TRAP: `installs` USED TO OVER-SCAN (lane DL-2, 2026-08-03)
------------------------------------------------------------------------
`installs` took a fixed `--size`, defaulting to **256 bytes**.  On the
`??_G`/`??_E` population, bodies are **68-80 B**, so the scan ran 3-4x past the
end of the function and decoded **the next function's** vtable stores as though
they belonged to this one.  Reproduced before the fix on `0x82634FE0`, whose
true `.pdata` extent is **68 B**::

    installs 0x82634FE0            -> 0x820aa02c -> .?AVBasicStartLockMsg@@   (WRONG)
    installs 0x82634FE0 --size 68  -> constructs no address that resolves      (RIGHT)

Lane DK-1 was about to act on 5 such rows; every one would have been a
confident wrong repoint.

The scan is now bounded by **retail's own function-extent table** -- the
`.pdata` RUNTIME_FUNCTION array, via `tools/pdata_map_audit.load_extents`.  And
because a **leaf function has no `.pdata` entry**, the tool now returns THREE
labels instead of silently guessing (the same shape as rule 8: a *sufficient*
test used as a *necessary* one):

    BOUNDED    extent known; the scan is confined to it.  An EMPTY result is a
               REAL ANSWER -- "this function stores no vtable" -- which is the
               correct verdict for a vbase-adjustor thunk that tail-calls the
               real dtor (13 of DK-1's 18 candidates were exactly that).
    UNBOUNDED  no `.pdata` entry (leaf).  The tool REFUSES (exit 4) rather than
               over-scan.  `--allow-unbounded` opts in and LABELS the result.
    NO_CODE    the address is not in a section with data.

⇒ A caller can now distinguish "no vtable stored" from "could not determine".
That distinction did not previously exist: both printed the same sentence.

⚠ The INVERSE direction still has known false negatives and is NOT fixed here:
lane DK-4's vtable-*installer* scanner found **0 installers** for a vtable that
`installs` locates from the other direction.  Recorded, deliberately untouched.

CLI
---
    python3 tools/retail_rtti.py sections
    python3 tools/retail_rtti.py class RndText
    python3 tools/retail_rtti.py vtable 0x820F1400
    python3 tools/retail_rtti.py col 0x821dae8c
    python3 tools/retail_rtti.py installs 0x823F6198          # .pdata-bounded
    python3 tools/retail_rtti.py installs 0x82634FE0          # -> BOUNDED, empty
    python3 tools/retail_rtti.py installs 0x826e34d0          # -> UNBOUNDED, exit 4
    python3 tools/retail_rtti.py owner 0x823F6198
    python3 tools/retail_rtti.py --selftest          # exits non-zero on failure
    python3 tools/retail_rtti.py --selftest --sabotage naive-va   # MUST fail
    python3 tools/retail_rtti.py --selftest --sabotage overscan   # MUST fail
"""
from __future__ import annotations

import argparse
import struct
import sys
from dataclasses import dataclass, field
from pathlib import Path
from typing import Dict, List, Optional, Sequence, Tuple

DEFAULT_EXE = "orig/45410914/band.exe"
#: the .rdata-only shortcut.  Present ONLY so the sabotage leg can reproduce
#: the historical defect; never used on the real path.
NAIVE_IMAGE_BASE = 0x82000000

#: The pre-DL-2 fixed scan window.  Present ONLY so `--sabotage overscan` can
#: reproduce the over-scan defect; never used on the real path.
LEGACY_OVERSCAN_SIZE = 256

#: `installs` result labels.  Three, because the world has three answers and
#: collapsing them is what shipped five wrong repoints.
BOUNDED = "BOUNDED"        # extent known -- an empty hit list is a REAL answer
UNBOUNDED = "UNBOUNDED"    # no .pdata entry (leaf) -- undecidable, refuse
NO_CODE = "NO_CODE"        # address not backed by section data


def _find_default_exe() -> Path:
    """Locate band.exe relative to this file's repo root, then CWD."""
    here = Path(__file__).resolve()
    for root in (here.parent.parent, Path.cwd()):
        p = root / DEFAULT_EXE
        if p.exists():
            return p
    raise SystemExit(f"cannot locate {DEFAULT_EXE} (looked from {here.parent.parent} and {Path.cwd()})")


@dataclass(frozen=True)
class Section:
    name: str
    va: int          # virtual address (image base already added)
    vsize: int
    rawptr: int
    rawsize: int

    @property
    def has_data(self) -> bool:
        return self.rawsize > 0


@dataclass
class COL:
    """RTTI Complete Object Locator."""
    va: int
    signature: int
    offset: int          # offset of this vftable within the complete object
    cd_offset: int       # constructor displacement
    ptd: int             # -> TypeDescriptor
    pchd: int            # -> ClassHierarchyDescriptor


@dataclass
class CHD:
    """RTTI Class Hierarchy Descriptor."""
    va: int
    signature: int
    attributes: int
    num_base_classes: int
    pbca: int            # -> base class array


@dataclass
class BCD:
    """RTTI Base Class Descriptor (one entry of the hierarchy)."""
    va: int
    ptd: int
    num_contained_bases: int
    mdisp: int           # PMD: member displacement
    pdisp: int           # PMD: vbtable displacement (-1 == NOT a virtual base)
    vdisp: int           # PMD: displacement inside the vbtable
    attributes: int
    name: Optional[str] = None

    @property
    def is_virtual_base(self) -> bool:
        return self.pdisp != -1


class RetailPE:
    """Big-endian Xbox 360 PE reader with SECTION-AWARE VA<->raw mapping."""

    def __init__(self, path: Optional[Path] = None, sabotage: Optional[str] = None):
        self.path = Path(path) if path else _find_default_exe()
        self.data = self.path.read_bytes()
        self.sabotage = sabotage
        d = self.data
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        if d[pe:pe + 4] != b"PE\0\0":
            raise SystemExit(f"{self.path}: not a PE (no PE signature at {pe:#x})")
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        optsz = struct.unpack_from("<H", d, pe + 20)[0]
        self.image_base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
        self.sections: List[Section] = []
        for i in range(nsec):
            o = pe + 24 + optsz + i * 40
            name = d[o:o + 8].rstrip(b"\0").decode("latin1")
            vsz, rva, rawsz, rawptr = struct.unpack_from("<IIII", d, o + 8)
            self.sections.append(Section(name, self.image_base + rva, vsz, rawptr, rawsz))
        if sabotage == "naive-va":
            # Reproduce the de044702 defect: pretend the whole image obeys the
            # .rdata-only shortcut.  Used ONLY by the vacuity control.
            self.sections = [Section(".ALL", NAIVE_IMAGE_BASE, len(d), 0, len(d))]

    # -- address mapping ---------------------------------------------------
    def va2raw(self, va: int) -> Optional[int]:
        for s in self.sections:
            if not s.has_data:
                continue
            if s.va <= va < s.va + max(s.vsize, s.rawsize):
                off = va - s.va
                if off < s.rawsize:      # tail beyond rawsize is uninitialised
                    return s.rawptr + off
                return None
        return None

    def raw2va(self, raw: int) -> Optional[int]:
        for s in self.sections:
            if s.has_data and s.rawptr <= raw < s.rawptr + s.rawsize:
                return s.va + (raw - s.rawptr)
        return None

    def section_of(self, va: int) -> str:
        for s in self.sections:
            if s.va <= va < s.va + max(s.vsize, s.rawsize):
                return s.name
        return "?"

    def naive_va2raw(self, va: int) -> int:
        """The .rdata-only shortcut.  EXPORTED SO CONTROLS CAN SHOW IT WRONG."""
        return va - NAIVE_IMAGE_BASE

    def skew(self, va: int) -> Optional[int]:
        """section-aware raw minus naive raw; 0 for .rdata, nonzero elsewhere."""
        r = self.va2raw(va)
        return None if r is None else r - self.naive_va2raw(va)

    # -- primitive reads ---------------------------------------------------
    def u32(self, va: int) -> Optional[int]:
        r = self.va2raw(va)
        if r is None or r + 4 > len(self.data):
            return None
        return struct.unpack_from(">I", self.data, r)[0]

    def i32(self, va: int) -> Optional[int]:
        r = self.va2raw(va)
        if r is None or r + 4 > len(self.data):
            return None
        return struct.unpack_from(">i", self.data, r)[0]

    def cstr(self, va: int, limit: int = 512) -> Optional[str]:
        r = self.va2raw(va)
        if r is None:
            return None
        e = self.data.find(b"\0", r, r + limit)
        if e < 0:
            return None
        return self.data[r:e].decode("latin1")

    def is_image_va(self, va: Optional[int]) -> bool:
        if not va:
            return False
        return self.image_base <= va < self.image_base + 0x01000000

    def find_bytes(self, pat: bytes, sections: Optional[Sequence[str]] = None) -> List[int]:
        """VAs of every occurrence of `pat` (optionally restricted to sections)."""
        out: List[int] = []
        for s in self.sections:
            if not s.has_data:
                continue
            if sections is not None and s.name not in sections:
                continue
            blk = self.data[s.rawptr:s.rawptr + s.rawsize]
            i = 0
            while True:
                j = blk.find(pat, i)
                if j < 0:
                    break
                out.append(s.va + j)
                i = j + 1
        return out

    def count_bytes(self, pat: bytes) -> int:
        """Whole-file literal occurrences.  Pure Python => immune to the
        binary-blind `grep` shim that yields silent false negatives."""
        return self.data.count(pat)

    def word_refs(self, target_va: int,
                  sections: Optional[Sequence[str]] = None) -> List[int]:
        """VAs of aligned big-endian words equal to target_va (all sections by
        default; pass e.g. ('.rdata', '.data') to restrict)."""
        secs = None if sections is None else set(sections)
        pat = struct.pack(">I", target_va)
        out: List[int] = []
        for s in self.sections:
            if not s.has_data or (secs is not None and s.name not in secs):
                continue
            blk = self.data[s.rawptr:s.rawptr + s.rawsize]
            i = 0
            while True:
                j = blk.find(pat, i)
                if j < 0:
                    break
                i = j + 1
                if j % 4 == 0:
                    out.append(s.va + j)
        return out


class RetailRtti(RetailPE):
    """MSVC RTTI decoding on top of the section-aware PE reader."""

    def __init__(self, path: Optional[Path] = None, sabotage: Optional[str] = None):
        super().__init__(path, sabotage)
        self._vt_cache: Dict[int, Optional[str]] = {}

    # -- descriptors -------------------------------------------------------
    def td_name(self, td_va: int) -> Optional[str]:
        """TypeDescriptor::name lives at +8 (vfptr, spare, name[])."""
        return self.cstr(td_va + 8)

    def find_type_descriptors(self, mangled: str) -> List[Tuple[int, str]]:
        """'.?AVRndFont@@' -> [(TypeDescriptor VA, section), ...]."""
        out = []
        for name_va in self.find_bytes(mangled.encode() + b"\0"):
            out.append((name_va - 8, self.section_of(name_va)))
        return out

    def decode_col(self, va: int) -> Optional[COL]:
        f = [self.u32(va + 4 * i) for i in range(5)]
        if any(x is None for x in f):
            return None
        return COL(va, f[0], f[1], f[2], f[3], f[4])

    def decode_chd(self, va: int) -> Optional[CHD]:
        f = [self.u32(va + 4 * i) for i in range(4)]
        if any(x is None for x in f):
            return None
        return CHD(va, f[0], f[1], f[2], f[3])

    def decode_bcd(self, va: int) -> Optional[BCD]:
        ptd = self.u32(va)
        ncb = self.u32(va + 4)
        mdisp = self.i32(va + 8)
        pdisp = self.i32(va + 12)
        vdisp = self.i32(va + 16)
        attr = self.u32(va + 20)
        if None in (ptd, ncb, mdisp, pdisp, vdisp, attr):
            return None
        b = BCD(va, ptd, ncb, mdisp, pdisp, vdisp, attr)
        b.name = self.td_name(ptd)
        return b

    def _col_is_plausible(self, c: Optional[COL]) -> bool:
        return bool(c and c.signature == 0 and self.is_image_va(c.ptd)
                    and self.is_image_va(c.pchd))

    # -- the three headline capabilities ----------------------------------
    def class_of_vtable(self, vt_va: int) -> Optional[str]:
        """vtable VA -> '.?AVFoo@@'.  The COL pointer sits at vtable[-1]."""
        if vt_va in self._vt_cache:
            return self._vt_cache[vt_va]
        name = None
        col_ptr = self.u32(vt_va - 4)
        if self.is_image_va(col_ptr):
            c = self.decode_col(col_ptr)
            if self._col_is_plausible(c):
                n = self.td_name(c.ptd)
                if n and n.startswith(".?A"):
                    name = n
        self._vt_cache[vt_va] = name
        return name

    def bases_of_col(self, col_va: int) -> Optional[Tuple[COL, CHD, List[BCD]]]:
        """Decode a COL's full class hierarchy (numBaseClasses + PMD/vdisp)."""
        c = self.decode_col(col_va)
        if not self._col_is_plausible(c):
            return None
        chd = self.decode_chd(c.pchd)
        if chd is None or chd.num_base_classes > 4096:
            return None
        bases: List[BCD] = []
        for i in range(chd.num_base_classes):
            p = self.u32(chd.pbca + 4 * i)
            if not self.is_image_va(p):
                break
            b = self.decode_bcd(p)
            if b is None:
                break
            bases.append(b)
        return c, chd, bases

    def cols_for_class(self, cls: str) -> List[int]:
        """Class name (bare 'RndText' or full '.?AVRndText@@') -> COL VAs."""
        mangled = cls if cls.startswith(".?A") else f".?AV{cls}@@"
        cols: List[int] = []
        for td_va, _sec in self.find_type_descriptors(mangled):
            # a COL references the TypeDescriptor at +12
            for ref in self.word_refs(td_va, sections=None):
                c = self.decode_col(ref - 12)
                if self._col_is_plausible(c) and c.ptd == td_va:
                    chd = self.decode_chd(c.pchd)
                    if chd and chd.signature == 0 and 0 < chd.num_base_classes < 4096:
                        cols.append(ref - 12)
        return sorted(set(cols))

    def hierarchy_of_class(self, cls: str) -> List[Tuple[int, CHD, List[BCD]]]:
        out = []
        for col_va in self.cols_for_class(cls):
            r = self.bases_of_col(col_va)
            if r:
                _c, chd, bases = r
                out.append((col_va, chd, bases))
        return out

    def vtables_for_col(self, col_va: int) -> List[int]:
        """A vtable head is 4 bytes after the slot holding its COL pointer."""
        return [r + 4 for r in self.word_refs(col_va, sections=None)]

    # -- "which class's vtable does this function install?" ----------------
    #
    # Pure-Python PPC decode of the only forms that build an address:
    #   lis  rD, imm       == addis rD,0,imm   opcode 15, rA==0
    #   addi rD, rA, imm                        opcode 14
    #   ori  rA, rS, imm                        opcode 24
    def constructed_addresses(self, fn_va: int, size: int) -> List[int]:
        raw = self.va2raw(fn_va)
        if raw is None:
            return []
        code = self.data[raw:raw + size]
        regs: Dict[int, int] = {}
        addrs: List[int] = []
        for off in range(0, len(code) - 3, 4):
            w = struct.unpack_from(">I", code, off)[0]
            op = w >> 26
            if op == 15:                                  # addis / lis
                d, a, imm = (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
                base = 0 if a == 0 else regs.get(a)
                if base is None:
                    regs.pop(d, None)
                else:
                    regs[d] = (base + (imm << 16)) & 0xFFFFFFFF
            elif op == 14:                                # addi
                d, a, imm = (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
                if imm & 0x8000:
                    imm -= 0x10000
                base = 0 if a == 0 else regs.get(a)
                if base is None:
                    regs.pop(d, None)
                else:
                    regs[d] = v = (base + imm) & 0xFFFFFFFF
                    addrs.append(v)
            elif op == 24:                                # ori
                s, a, imm = (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
                base = regs.get(s)
                if base is None:
                    regs.pop(a, None)
                else:
                    regs[a] = v = base | imm
                    addrs.append(v)
            else:
                # Any other instruction with a plausible rD write invalidates it.
                if op in (7, 8, 12, 13, 28, 29, 32, 34, 40, 42, 46, 56, 58, 60):
                    regs.pop((w >> 21) & 31, None)
        return addrs

    # -- retail's own function-extent table (.pdata) -----------------------
    #
    # Deliberately DELEGATED to tools/pdata_map_audit rather than re-decoded
    # here: this module exists because three lanes independently re-wrote the
    # same resolver and two baked in wrong arithmetic.  A second .pdata decoder
    # would be that mistake again -- and pdata_map_audit's decode is the one
    # with the inverted-assumption control (it keeps the wrong `>>2` shift as a
    # live sabotage leg).
    @property
    def extents(self) -> Dict[int, int]:
        ext = getattr(self, "_extents", None)
        if ext is None:
            import importlib.util
            p = Path(__file__).resolve().parent / "pdata_map_audit.py"
            spec = importlib.util.spec_from_file_location("_pdata_dl2", p)
            mod = importlib.util.module_from_spec(spec)
            spec.loader.exec_module(mod)
            ext = mod.load_extents(str(self.path))
            self._extents = ext
        return ext

    def function_extent(self, fn_va: int) -> Optional[int]:
        """Byte length of the function starting at `fn_va`, from retail .pdata.

        None means 'retail has no RUNTIME_FUNCTION for this address' -- almost
        always a LEAF function.  It does NOT mean 'not a function' (band.exe
        read trap).  Callers must not treat None as a length.
        """
        return self.extents.get(fn_va)

    def classes_installed_by(self, fn_va: int, size: Optional[int] = None,
                             allow_unbounded: bool = False,
                             overscan: bool = False,
                             ) -> Tuple[str, Optional[int], List[Tuple[int, str]]]:
        """-> (status, scanned_bytes, [(vtable_va, class), ...]).

        `size=None` (the default) bounds the scan by retail's `.pdata` extent.
        An explicit `size` is honoured but still reported, so an over-scan is
        at least VISIBLE.  With no extent and no explicit size the answer is
        UNBOUNDED and the hit list is EMPTY BY REFUSAL -- never by measurement.
        """
        if self.va2raw(fn_va) is None:
            return NO_CODE, None, []

        if overscan:                       # sabotage leg only
            size = LEGACY_OVERSCAN_SIZE

        status = BOUNDED
        if size is None:
            size = self.function_extent(fn_va)
            if size is None:
                if not allow_unbounded:
                    return UNBOUNDED, None, []
                status = UNBOUNDED
                size = LEGACY_OVERSCAN_SIZE
        else:
            ext = self.function_extent(fn_va)
            # An explicit size larger than retail's own extent is the exact
            # defect this fix exists to stop -- surface it, do not silence it.
            if ext is not None and size > ext:
                status = "OVERSCAN"
            elif ext is None:
                status = UNBOUNDED if not allow_unbounded else UNBOUNDED

        out = []
        for a in dict.fromkeys(self.constructed_addresses(fn_va, size)):
            n = self.class_of_vtable(a)
            if n:
                out.append((a, n))
        return status, size, out

    def owning_vtables(self, fn_va: int, max_slots: int = 256) -> List[Tuple[str, int, int]]:
        """Which class's vtable CONTAINS this function -> (class, vtable, slot).

        Complements classes_installed_by: that answers "what does this ctor
        install", this answers "whose virtual method is this".
        """
        out = []
        for slot_va in self.word_refs(fn_va, sections=None):
            for back in range(0, max_slots * 4, 4):
                head = slot_va - back
                n = self.class_of_vtable(head)
                if n:
                    out.append((n, head, back // 4))
                    break
        return out


# ==========================================================================
# CONTROLS.  A selftest that cannot fail is vacuous; this one is *proven* able
# to fail by `--selftest --sabotage naive-va` (see the module docstring).
# ==========================================================================
def _controls(R: RetailRtti, overscan: bool = False) -> List[Tuple[str, bool, str]]:
    res: List[Tuple[str, bool, str]] = []

    # (A) THE EXTENT TABLE MUST ACTUALLY LOAD.  If it came back tiny/empty every
    #     address would read UNBOUNDED and `installs` would refuse everything --
    #     a different silent failure, shaped like caution instead of like a zero.
    try:
        ext = R.extents
        ok = len(ext) > 40000 and ext.get(0x82634FE0) == 68
        res.append(("retail .pdata extent table loaded", ok,
                    f"entries={len(ext)} extent(0x82634FE0)={ext.get(0x82634FE0)} "
                    f"(want >40000 and 68)"))
    except Exception as e:
        res.append(("retail .pdata extent table loaded", False, f"EXC {e!r}"))

    # (B) ★★ THE REGRESSION PIN for the DL-2 over-scan defect, asserted FROM BOTH
    #     SIDES so it cannot be silently reverted:
    #       - bounded by retail's 68-byte extent  => NO hits (the true answer)
    #       - the old fixed 256-byte window       => a hit, and a WRONG one
    #     Requiring the two to DISAGREE is what makes this control able to fail:
    #     if someone removes the bounding, the first half goes red; if someone
    #     removes the over-scan reproduction, the second half goes red.
    try:
        st_b, sc_b, hits_b = R.classes_installed_by(0x82634FE0, overscan=overscan)
        st_o, sc_o, hits_o = R.classes_installed_by(0x82634FE0, overscan=True)
        wrong = [n for _a, n in hits_o]
        ok = (st_b == BOUNDED and sc_b == 68 and hits_b == []
              and sc_o == LEGACY_OVERSCAN_SIZE
              and ".?AVBasicStartLockMsg@@" in wrong)
        res.append(("installs 0x82634FE0: bounded=EMPTY vs overscan=WRONG HIT", ok,
                    f"bounded(status={st_b}, scanned={sc_b}, hits={hits_b}) "
                    f"vs overscan(scanned={sc_o}, hits={wrong}) "
                    f"(want BOUNDED/68/[] and 256/BasicStartLockMsg)"))
    except Exception as e:
        res.append(("installs 0x82634FE0: bounded=EMPTY vs overscan=WRONG HIT",
                    False, f"EXC {e!r}"))

    # (C) KNOWN POSITIVE for `installs` (rule 1): the bounding must not be
    #     vacuous.  A tool that refuses or returns [] for EVERYTHING would pass
    #     control (B)'s first half trivially.  ??0XLSPConnection@@ is 120 B and
    #     installs its OWN vtable INSIDE that extent.
    try:
        st, sc, hits = R.classes_installed_by(0x827D9998, overscan=overscan)
        ok = (st == BOUNDED and sc == 120
              and ".?AVXLSPConnection@@" in [n for _a, n in hits])
        res.append(("installs known-positive ??0XLSPConnection@@ within extent", ok,
                    f"status={st} scanned={sc} hits={[n for _a,n in hits]} "
                    f"(want BOUNDED/120/contains .?AVXLSPConnection@@)"))
    except Exception as e:
        res.append(("installs known-positive ??0XLSPConnection@@ within extent",
                    False, f"EXC {e!r}"))

    # (D) THE THIRD LABEL (rule 4 + rule 8).  A leaf function has NO .pdata
    #     entry, so "installs nothing" is UNDECIDABLE there, not false.  The
    #     classifier must produce UNBOUNDED -- distinct from BOUNDED-with-no-hits
    #     -- or the tool is back to one label for two different worlds.
    try:
        leaf = 0x826E34D0                      # ??0HitSink@@QAA@XZ, no RUNTIME_FUNCTION
        st, sc, hits = R.classes_installed_by(leaf, overscan=overscan)
        st_ok, _sc2, _h2 = R.classes_installed_by(leaf, allow_unbounded=True,
                                                  overscan=overscan)
        ok = (R.function_extent(leaf) is None and st == UNBOUNDED
              and sc is None and hits == [] and st_ok == UNBOUNDED)
        res.append(("leaf w/o .pdata -> UNBOUNDED (undecidable), not empty-BOUNDED",
                    ok, f"extent={R.function_extent(leaf)} status={st} scanned={sc} "
                        f"hits={hits} (want None/UNBOUNDED/None/[])"))
    except Exception as e:
        res.append(("leaf w/o .pdata -> UNBOUNDED (undecidable), not empty-BOUNDED",
                    False, f"EXC {e!r}"))

    # (0) SKEW: the section-aware map and the naive shortcut must DISAGREE on a
    #     .data TypeDescriptor, and only the section-aware one may be right.
    td = 0x82C6CCF8
    try:
        aware = R.va2raw(td)
        naive = R.naive_va2raw(td)
        name = R.td_name(td)
        ok = (aware is not None and aware != naive and name == ".?AVRndText@@")
        res.append(("skew/.data TypeDescriptor 0x82c6ccf8",
                    ok,
                    f"section-aware raw={aware if aware is None else hex(aware)} "
                    f"naive={hex(naive)} skew="
                    f"{'None' if aware is None else hex(aware - naive)} name={name!r} "
                    f"(want != and '.?AVRndText@@')"))
    except Exception as e:
        res.append(("skew/.data TypeDescriptor 0x82c6ccf8", False, f"EXC {e!r}"))

    # (1) POSITIVE: RndText decodes to 9 bases across FOUR COLs, with the
    #     virtual bases carrying pdisp=4 and vdisp in {4,8}.  (lane DF-1)
    try:
        h = R.hierarchy_of_class("RndText")
        ncols = len(h)
        counts = sorted({chd.num_base_classes for _v, chd, _b in h})
        vb = {(b.name, b.pdisp, b.vdisp) for _v, _c, bs in h for b in bs if b.is_virtual_base}
        want_vb = {(".?AVRndHighlightable@@", 4, 8),
                   (".?AVObject@Hmx@@", 4, 4)}
        ok = (ncols == 4 and counts == [9] and want_vb <= vb)
        res.append(("RndText hierarchy (4 COLs x 9 bases, vbase PMDs)", ok,
                    f"cols={ncols} numBaseClasses={counts} vbases={sorted(vb)} "
                    f"(want 4, [9], superset of {sorted(want_vb)})"))
    except Exception as e:
        res.append(("RndText hierarchy (4 COLs x 9 bases, vbase PMDs)", False, f"EXC {e!r}"))

    # (2) POSITIVE: a known vtable resolves to its class.  (commit 47907c6f)
    try:
        n = R.class_of_vtable(0x820F1400)
        ok = (n == ".?AVHitSink@@")
        res.append(("vtable 0x820F1400 -> .?AVHitSink@@", ok, f"got {n!r}"))
    except Exception as e:
        res.append(("vtable 0x820F1400 -> .?AVHitSink@@", False, f"EXC {e!r}"))

    # (3) NEGATIVE / ABSENCE: RndFont's COL has exactly 3 bases and NO
    #     RndFontBase, and the literal 'FontBase' occurs zero times in the
    #     image.  An absence claim needs the decoder to be working, which is
    #     why it is paired with (1)/(2) — those failing invalidates this.
    try:
        r = R.bases_of_col(0x821DAE8C)
        if r is None:
            res.append(("RndFont COL absence of RndFontBase", False, "COL did not decode"))
        else:
            _c, chd, bases = r
            names = [b.name for b in bases]
            nfb = R.count_bytes(b"FontBase")
            ok = (chd.num_base_classes == 3
                  and names == [".?AVRndFont@@", ".?AVObject@Hmx@@", ".?AVObjRef@@"]
                  and not any("FontBase" in (n or "") for n in names)
                  and nfb == 0)
            res.append(("RndFont COL absence of RndFontBase", ok,
                        f"n={chd.num_base_classes} bases={names} "
                        f"literal b'FontBase' count={nfb} (want 3, no FontBase, 0)"))
    except Exception as e:
        res.append(("RndFont COL absence of RndFontBase", False, f"EXC {e!r}"))

    return res


def run_selftest(path: Optional[Path], sabotage: Optional[str]) -> int:
    try:
        R = RetailRtti(path, sabotage=sabotage)
    except SystemExit as e:
        print(f"SELFTEST SETUP FAILED: {e}")
        return 2
    print(f"retail_rtti selftest  binary={R.path}  sections={len(R.sections)}"
          + (f"  SABOTAGE={sabotage}" if sabotage else ""))
    if sabotage:
        print("  (sabotage leg: this run is EXPECTED to FAIL — it proves the "
              "selftest is not vacuous)")
    rows = _controls(R, overscan=(sabotage == "overscan"))
    nfail = 0
    for name, ok, detail in rows:
        print(f"  [{'PASS' if ok else 'FAIL'}] {name}\n         {detail}")
        nfail += (not ok)
    print(f"  {len(rows) - nfail}/{len(rows)} controls passed")
    return 1 if nfail else 0


# ==========================================================================
def _fmt_bcd(b: BCD) -> str:
    kind = "vbase" if b.is_virtual_base else "     "
    return (f"{kind} {b.name}  ncb={b.num_contained_bases} "
            f"PMD(m={b.mdisp},p={b.pdisp},v={b.vdisp}) attr={b.attributes}")


def main(argv: Optional[List[str]] = None) -> int:
    ap = argparse.ArgumentParser(
        description=__doc__.split("\n")[0],
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="VA<->raw is resolved from PE section headers; the "
               "`va-0x82000000` shortcut is valid ONLY for .rdata and is never "
               "used except in the --sabotage vacuity control.")
    ap.add_argument("--exe", type=Path, default=None, help="retail PE (default orig/45410914/band.exe)")
    ap.add_argument("--selftest", action="store_true", help="run hard-wired controls; non-zero exit on failure")
    ap.add_argument("--sabotage", choices=["naive-va", "overscan"], default=None,
                    help="deliberately break VA mapping ('naive-va') or restore the "
                         "pre-DL-2 fixed 256-byte installs window ('overscan'). "
                         "Vacuity controls: --selftest MUST then fail.")
    sub = ap.add_subparsers(dest="cmd")
    sub.add_parser("sections", help="print the PE section table + VA/raw skews")
    p = sub.add_parser("class", help="full RTTI dump for a class"); p.add_argument("name")
    p = sub.add_parser("vtable", help="vtable VA -> class (+ slots)"); p.add_argument("va"); p.add_argument("-n", type=int, default=0)
    p = sub.add_parser("col", help="decode a COL's hierarchy"); p.add_argument("va")
    p = sub.add_parser("installs", help="which class's vtable a function installs")
    p.add_argument("va")
    p.add_argument("--size", type=int, default=None,
                   help="explicit scan window; DEFAULT IS RETAIL'S .pdata EXTENT. "
                        "A size exceeding that extent is reported as OVERSCAN.")
    p.add_argument("--allow-unbounded", action="store_true",
                   help="for leaf functions with no .pdata entry: scan anyway and "
                        "LABEL the result UNBOUNDED instead of refusing (exit 4)")
    p = sub.add_parser("owner", help="which class's vtable CONTAINS this function"); p.add_argument("va")
    a = ap.parse_args(argv)

    if a.selftest:
        return run_selftest(a.exe, a.sabotage)
    if not a.cmd:
        ap.print_help()
        return 0
    R = RetailRtti(a.exe, sabotage=a.sabotage)

    if a.cmd == "sections":
        print(f"{'name':10s} {'va':>10s} {'vsize':>9s} {'rawptr':>9s} {'rawsize':>9s}  skew-vs-naive")
        for s in R.sections:
            sk = R.skew(s.va)
            print(f"{s.name:10s} {s.va:#010x} {s.vsize:#9x} {s.rawptr:#9x} {s.rawsize:#9x}  "
                  + ("(no data)" if not s.has_data else
                     f"{'0 (shortcut exact here)' if sk == 0 else hex(sk) if sk is not None else '?'}"))
        return 0

    if a.cmd == "class":
        h = R.hierarchy_of_class(a.name)
        if not h:
            print(f"{a.name}: no COL found")
            return 1
        for col_va, chd, bases in h:
            c = R.decode_col(col_va)
            print(f"COL @ {col_va:#x} offset={c.offset:#x} cdOffset={c.cd_offset:#x} "
                  f"CHD@{chd.va:#x} attr={chd.attributes} numBaseClasses={chd.num_base_classes}")
            for i, b in enumerate(bases):
                print(f"   [{i}] {_fmt_bcd(b)}")
            for vt in R.vtables_for_col(col_va):
                print(f"   vtable @ {vt:#x} (sec {R.section_of(vt)})")
        return 0

    if a.cmd == "vtable":
        va = int(a.va, 16)
        n = R.class_of_vtable(va)
        print(f"{va:#x} -> {n}")
        for i in range(a.n):
            e = R.u32(va + 4 * i)
            if e is None:
                break
            s = R.section_of(e)
            print(f"  [{i:3}] {e:#010x} <{s}>" + ("  STOP" if s != ".text" else ""))
            if s != ".text":
                break
        return 0 if n else 1

    if a.cmd == "col":
        r = R.bases_of_col(int(a.va, 16))
        if r is None:
            print("not a plausible COL")
            return 1
        c, chd, bases = r
        print(f"COL @ {c.va:#x} offset={c.offset:#x} class={R.td_name(c.ptd)} "
              f"numBaseClasses={chd.num_base_classes}")
        for i, b in enumerate(bases):
            print(f"   [{i}] {_fmt_bcd(b)}")
        return 0

    if a.cmd == "installs":
        va = int(a.va, 16)
        status, scanned, hits = R.classes_installed_by(
            va, a.size, allow_unbounded=a.allow_unbounded,
            overscan=(a.sabotage == "overscan"))
        ext = R.function_extent(va)
        print(f"{va:#x}  status={status}  .pdata extent="
              f"{'NONE (leaf / no RUNTIME_FUNCTION)' if ext is None else f'{ext} bytes'}"
              f"  scanned={scanned}")
        if status == NO_CODE:
            print("  REFUSED: address is not backed by section data.")
            return 4
        if status == UNBOUNDED and not a.allow_unbounded:
            print("  REFUSED: retail has no .pdata extent for this address, so any\n"
                  "  scan window would be a guess.  Scanning past the end decodes the\n"
                  "  NEXT function's vtable stores and yields a confident wrong answer\n"
                  "  (reproduced on 0x82634FE0 -> .?AVBasicStartLockMsg@@).\n"
                  "  Pass --size N if you know the extent, or --allow-unbounded to\n"
                  "  accept an explicitly-labelled guess.")
            return 4
        if status == "OVERSCAN":
            print(f"  ** WARNING: --size {a.size} EXCEEDS retail's own extent ({ext}). "
                  f"Hits beyond\n  **          offset {ext} belong to the NEXT function.")
        if status == UNBOUNDED:
            print("  ** UNBOUNDED (--allow-unbounded): no retail extent; these hits are\n"
                  "  **            NOT attributable to this function with confidence.")
        if not hits:
            # ★ A REAL ANSWER, not a failure -- and the two must not print alike.
            print(f"  no vtable stored: this function constructs no address that "
                  f"resolves as a vtable\n  within its own {scanned}-byte body "
                  f"(expected for a vbase-adjustor thunk that\n  tail-calls the real "
                  f"dtor -- 13 of DK-1's 18 candidates were exactly that).")
            return 1
        for addr, n in hits:
            print(f"  installs {addr:#x} -> {n}")
        return 0

    if a.cmd == "owner":
        va = int(a.va, 16)
        own = R.owning_vtables(va)
        if not own:
            print(f"{va:#x}: no vtable contains this address")
            return 1
        for n, vt, slot in own:
            print(f"{va:#x} is {n} slot {slot} (vtable {vt:#x})")
        return 0

    return 0


if __name__ == "__main__":
    sys.exit(main())
