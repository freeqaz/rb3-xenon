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
    R.classes_installed_by(0x823F6198, 92) # -> [(addr, '.?AVXboxJob@@'), ...]

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

CLI
---
    python3 tools/retail_rtti.py sections
    python3 tools/retail_rtti.py class RndText
    python3 tools/retail_rtti.py vtable 0x820F1400
    python3 tools/retail_rtti.py col 0x821dae8c
    python3 tools/retail_rtti.py installs 0x823F6198 --size 92
    python3 tools/retail_rtti.py owner 0x823F6198
    python3 tools/retail_rtti.py --selftest          # exits non-zero on failure
    python3 tools/retail_rtti.py --selftest --sabotage naive-va   # MUST fail
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

    def classes_installed_by(self, fn_va: int, size: int) -> List[Tuple[int, str]]:
        """Addresses this function materialises that resolve as vtables."""
        out = []
        for a in dict.fromkeys(self.constructed_addresses(fn_va, size)):
            n = self.class_of_vtable(a)
            if n:
                out.append((a, n))
        return out

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
def _controls(R: RetailRtti) -> List[Tuple[str, bool, str]]:
    res: List[Tuple[str, bool, str]] = []

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
    rows = _controls(R)
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
    ap.add_argument("--sabotage", choices=["naive-va"], default=None,
                    help="deliberately break VA mapping (vacuity control: --selftest MUST then fail)")
    sub = ap.add_subparsers(dest="cmd")
    sub.add_parser("sections", help="print the PE section table + VA/raw skews")
    p = sub.add_parser("class", help="full RTTI dump for a class"); p.add_argument("name")
    p = sub.add_parser("vtable", help="vtable VA -> class (+ slots)"); p.add_argument("va"); p.add_argument("-n", type=int, default=0)
    p = sub.add_parser("col", help="decode a COL's hierarchy"); p.add_argument("va")
    p = sub.add_parser("installs", help="which class's vtable a function installs")
    p.add_argument("va"); p.add_argument("--size", type=int, default=256)
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
        hits = R.classes_installed_by(va, a.size)
        if not hits:
            print(f"{va:#x}: constructs no address that resolves as a vtable "
                  f"(scanned {a.size} bytes)")
            return 1
        for addr, n in hits:
            print(f"{va:#x} installs {addr:#x} -> {n}")
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
