#!/usr/bin/env python3
"""switch_frame_census.py — the switch-frame lever, automated.

MSVC X360 at /O1 gives every ``case`` body its own stack slots and never reuses
them across arms of a switch.  A switch function's stack frame is therefore a
*census of its arms*: when our frame differs from retail's, the delta names the
missing or surplus case bodies.

Getting the frame exact (frame immediate AND ``__savegprlr_N`` range) is what
flips a function's EH funclets — 145 of them flipped from a 6-line diff on
``SaveLoadManager::GetDialogMsg`` while the body was still at 0%.  This script
turns that analysis from manual asm archaeology into one command.

Modes
-----
``census``   side-by-side slot census, retail vs ours, for one function.
``find``     scan pinned in-scope units for switch functions ranked by the
             number of EH funclets that would flip on an exact frame.

Retail side is read straight out of the extracted PE (``orig/45410914/band.exe``);
no Ghidra, no objdiff.  Our side is read out of the compiled COFF object.

Examples
--------
    python3 scripts/harvest/switch_frame_census.py census \\
        --symbol '?SetState@SaveLoadManager@@IAAXW4State@1@@Z'

    python3 scripts/harvest/switch_frame_census.py census --va 0x82550880

    python3 scripts/harvest/switch_frame_census.py find --limit 40
"""

from __future__ import annotations

import argparse
import json
import os
import re
import struct
import sys
from collections import defaultdict
from dataclasses import dataclass, field
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
TITLE = "45410914"
IMAGE = REPO / "orig" / TITLE / "band.exe"
SYMBOLS = REPO / "config" / TITLE / "symbols.txt"
SPLITS = REPO / "config" / TITLE / "splits.txt"
TARGET_MAP = REPO / "scripts" / "target_symbol_map.json"
REPORT = REPO / "build" / TITLE / "report.json"

# ─────────────────────────────────────────────────────────────────────────────
# PE reader
# ─────────────────────────────────────────────────────────────────────────────


class PEImage:
    """Minimal PE32 reader that maps virtual addresses to file offsets."""

    def __init__(self, path: Path):
        self.data = path.read_bytes()
        d = self.data
        pe = struct.unpack_from("<I", d, 0x3C)[0]
        assert d[pe : pe + 4] == b"PE\0\0", "not a PE"
        nsec = struct.unpack_from("<H", d, pe + 6)[0]
        optsz = struct.unpack_from("<H", d, pe + 20)[0]
        self.image_base = struct.unpack_from("<I", d, pe + 24 + 28)[0]
        self.sections = []  # (name, va, vsize, raw, rsize)
        off = pe + 24 + optsz
        for _ in range(nsec):
            name = d[off : off + 8].rstrip(b"\0").decode("ascii", "replace")
            vsz, rva, rsz, rp = struct.unpack_from("<IIII", d, off + 8)
            self.sections.append((name, self.image_base + rva, vsz, rp, rsz))
            off += 40

    def _map(self, va: int) -> int | None:
        for _name, sva, vsz, rp, rsz in self.sections:
            if rsz and sva <= va < sva + max(vsz, rsz):
                delta = va - sva
                if delta < rsz:
                    return rp + delta
        return None

    def read(self, va: int, n: int) -> bytes:
        off = self._map(va)
        if off is None:
            raise KeyError(f"VA {va:#x} not backed by file data")
        return self.data[off : off + n]

    def word(self, va: int) -> int:
        return struct.unpack(">I", self.read(va, 4))[0]

    def u16(self, va: int) -> int:
        return struct.unpack(">H", self.read(va, 2))[0]

    def section_of(self, va: int) -> str | None:
        for name, sva, vsz, _rp, rsz in self.sections:
            if sva <= va < sva + max(vsz, rsz):
                return name
        return None


# ─────────────────────────────────────────────────────────────────────────────
# symbols.txt / splits.txt
# ─────────────────────────────────────────────────────────────────────────────

_SYM_RE = re.compile(
    r"^(?P<name>[^\s=]+)\s*=\s*(?P<sec>[.\w]+):(?P<va>0x[0-9A-Fa-f]+);"
    r"(?:.*?\btype:(?P<type>\w+))?(?:.*?\bsize:(?P<size>0x[0-9A-Fa-f]+))?"
)


class SymbolDB:
    def __init__(self, path: Path = SYMBOLS):
        self.by_name: dict[str, tuple[int, int, str]] = {}
        self.funcs: list[tuple[int, int, str]] = []  # (va, size, name), sorted
        self.labels: dict[int, str] = {}
        for line in path.read_text(errors="replace").splitlines():
            m = _SYM_RE.match(line.strip())
            if not m:
                continue
            va = int(m.group("va"), 16)
            size = int(m.group("size"), 16) if m.group("size") else 0
            name = m.group("name")
            typ = m.group("type") or ""
            self.by_name[name] = (va, size, typ)
            if typ == "function":
                self.funcs.append((va, size, name))
            elif typ == "label":
                self.labels.setdefault(va, name)
        self.funcs.sort()
        self.func_va = {va: (size, name) for va, size, name in self.funcs}

    def func_at(self, va: int):
        return self.func_va.get(va)

    def name_at(self, va: int) -> str | None:
        hit = self.func_va.get(va)
        if hit:
            return hit[1]
        return self.labels.get(va)


class Splits:
    """unit path -> list of .text (start, end) spans."""

    def __init__(self, path: Path = SPLITS):
        self.units: dict[str, list[tuple[int, int]]] = defaultdict(list)
        cur = None
        for raw in path.read_text(errors="replace").splitlines():
            line = raw.rstrip()
            if not line.strip() or line.lstrip().startswith("#"):
                continue
            if not line.startswith((" ", "\t")):
                cur = line.rstrip(":").strip()
                continue
            parts = line.split()
            if cur and parts and parts[0] == ".text":
                kv = dict(p.split(":", 1) for p in parts[1:] if ":" in p)
                if "start" in kv and "end" in kv:
                    self.units[cur].append((int(kv["start"], 16), int(kv["end"], 16)))
        self.spans = sorted(
            (s, e, u) for u, lst in self.units.items() for s, e in lst
        )

    def unit_of(self, va: int) -> str | None:
        lo, hi = 0, len(self.spans)
        while lo < hi:
            mid = (lo + hi) // 2
            if self.spans[mid][0] <= va:
                lo = mid + 1
            else:
                hi = mid
        if lo == 0:
            return None
        s, e, u = self.spans[lo - 1]
        return u if s <= va < e else None


# ─────────────────────────────────────────────────────────────────────────────
# PowerPC decode (only the forms this analysis needs)
# ─────────────────────────────────────────────────────────────────────────────


def _s16(v: int) -> int:
    return v - 0x10000 if v & 0x8000 else v


D_LOADS = {32: "lwz", 33: "lwzu", 34: "lbz", 40: "lhz", 42: "lha", 48: "lfs", 50: "lfd"}
D_STORES = {36: "stw", 37: "stwu", 38: "stb", 44: "sth", 52: "stfs", 54: "stfd"}


@dataclass
class Insn:
    va: int
    word: int
    op: int = 0
    rd: int = 0
    ra: int = 0
    rb: int = 0
    simm: int = 0
    uimm: int = 0
    xo: int = 0
    mnem: str = "?"
    target: int | None = None  # branch/call target VA

    @property
    def is_frame_ref(self) -> bool:
        return self.mnem in ("D_LOAD", "D_STORE", "addi") and self.ra in (1, 31)


def decode(va: int, w: int) -> Insn:
    i = Insn(va=va, word=w)
    op = (w >> 26) & 0x3F
    i.op = op
    i.rd = (w >> 21) & 0x1F
    i.ra = (w >> 16) & 0x1F
    i.rb = (w >> 11) & 0x1F
    i.simm = _s16(w & 0xFFFF)
    i.uimm = w & 0xFFFF
    if op == 14:
        i.mnem = "addi"
    elif op == 15:
        i.mnem = "addis"
    elif op == 10:
        i.mnem = "cmpli"  # cmplwi crD, rA, uimm
    elif op == 11:
        i.mnem = "cmpi"
    elif op == 21:
        i.mnem = "rlwinm"
    elif op in D_LOADS:
        i.mnem = "D_LOAD"
    elif op in D_STORES:
        i.mnem = "D_STORE"
    elif op == 18:  # b / bl
        li = w & 0x03FFFFFC
        if li & 0x02000000:
            li -= 0x04000000
        aa = w & 2
        i.target = li if aa else va + li
        i.mnem = "bl" if (w & 1) else "b"
    elif op == 16:  # bc
        bd = w & 0xFFFC
        if bd & 0x8000:
            bd -= 0x10000
        i.target = bd if (w & 2) else va + bd
        i.mnem = "bcl" if (w & 1) else "bc"
    elif op == 19:
        xo = (w >> 1) & 0x3FF
        i.xo = xo
        if w == 0x4E800420:
            i.mnem = "bctr"
        elif w == 0x4E800020:
            i.mnem = "blr"
        elif xo == 528:
            i.mnem = "bcctr"
        elif xo == 16:
            i.mnem = "bclr"
    elif op == 31:
        xo = (w >> 1) & 0x3FF
        i.xo = xo
        i.mnem = {
            279: "lhzx",
            23: "lwzx",
            87: "lbzx",
            151: "stwx",
            444: "or",
            266: "add",
            467: "mtspr",
            339: "mfspr",
        }.get(xo, "x%d" % xo)
    return i


# ─────────────────────────────────────────────────────────────────────────────
# Code views: retail (PE) and ours (COFF object)
# ─────────────────────────────────────────────────────────────────────────────


class CodeView:
    """word(va)/u16(va) over a code+data space, plus symbol resolution of bl."""

    def word(self, va: int) -> int:
        raise NotImplementedError

    def u16(self, va: int) -> int:
        raise NotImplementedError

    def u8(self, va: int) -> int:
        raise NotImplementedError

    def call_name(self, ins: Insn) -> str | None:
        raise NotImplementedError

    def reloc_target(self, va: int) -> tuple[str, int] | None:
        """For lis/addi @ha/@l pairs: (symbol name, addend) if relocated."""
        return None


class RetailView(CodeView):
    def __init__(self, pe: PEImage, syms: SymbolDB):
        self.pe, self.syms = pe, syms

    def word(self, va):
        return self.pe.word(va)

    def u16(self, va):
        return self.pe.u16(va)

    def u8(self, va):
        return self.pe.read(va, 1)[0]

    def call_name(self, ins):
        if ins.target is None:
            return None
        return self.syms.name_at(ins.target) or ("sub_%08X" % ins.target)


class CoffView(CodeView):
    """Read our own compiled function out of build/45410914/src/<unit>.obj."""

    def __init__(self, obj_path: Path, symbol: str):
        self.path = obj_path
        d = obj_path.read_bytes()
        self.data = d
        nsec, symptr, nsym = struct.unpack_from("<HxxxxII", d, 2)
        nsec = struct.unpack_from("<H", d, 2)[0]
        symptr = struct.unpack_from("<I", d, 8)[0]
        nsym = struct.unpack_from("<I", d, 12)[0]
        opt = struct.unpack_from("<H", d, 16)[0]
        secbase = 20 + opt
        self.sections = []
        for i in range(nsec):
            o = secbase + i * 40
            name = d[o : o + 8].rstrip(b"\0").decode("ascii", "replace")
            vsz, va, rsz, rp, rrel, _rln, nrel, _nln, chars = struct.unpack_from(
                "<IIIIIIHHI", d, o + 8
            )
            self.sections.append(
                dict(name=name, size=rsz, raw=rp, relptr=rrel, nrel=nrel, chars=chars)
            )
        strtab = symptr + nsym * 18
        self.symnames: list[str] = []
        self.symrec: list[tuple[str, int, int, int]] = []  # name, value, secnum, cls
        i = 0
        while i < nsym:
            o = symptr + i * 18
            raw = d[o : o + 8]
            if raw[:4] == b"\0\0\0\0":
                off = struct.unpack_from("<I", raw, 4)[0]
                end = d.index(b"\0", strtab + off)
                name = d[strtab + off : end].decode("ascii", "replace")
            else:
                name = raw.rstrip(b"\0").decode("ascii", "replace")
            value, secnum, _typ, cls, naux = struct.unpack_from("<IhHBB", d, o + 8)
            self.symrec.append((name, value, secnum, cls))
            for _ in range(naux):
                self.symrec.append((name, value, secnum, cls))
            i += 1 + naux
        # locate the function
        hit = None
        for name, value, secnum, _cls in self.symrec:
            if name == symbol and secnum > 0:
                hit = (secnum - 1, value)
                break
        if hit is None:
            raise KeyError(f"{symbol} not found in {obj_path.name}")
        self.sec_idx, self.func_off = hit
        sec = self.sections[self.sec_idx]
        self.base_va = 0x10000000  # synthetic base for the function's section
        self.func_va = self.base_va + self.func_off
        self.sec_raw = sec["raw"]
        self.sec_size = sec["size"]
        self.func_size = self._size_from_pdata(symbol)
        if self.func_size is None:
            self.func_size = self._size_from_scan(sec)
        # relocations of this section: va -> (symbol name, type)
        self.relocs: dict[int, tuple[str, int]] = {}
        for j in range(sec["nrel"]):
            o = sec["relptr"] + j * 10
            rva, symidx, rtype = struct.unpack_from("<IIH", d, o)
            if rtype == 0x12:      # IMAGE_REL_PPC_PAIR — addend carrier, not a target
                continue
            key = self.base_va + rva
            if symidx < len(self.symrec) and key not in self.relocs:
                self.relocs[key] = (self.symrec[symidx][0], rtype)
        # data sections by name for jump-table reads
        self._sec_by_index = {i: s for i, s in enumerate(self.sections)}
        self._symtab_by_name: dict[str, tuple[int, int]] = {}
        for name, value, secnum, _cls in self.symrec:
            if secnum > 0 and name not in self._symtab_by_name:
                self._symtab_by_name[name] = (secnum - 1, value)
        self._jt_bases: dict[int, tuple[int, int]] = {}  # synthetic va -> (sec, off)
        self._next_synth = 0x20000000

    # --- sizing ------------------------------------------------------------
    def _size_from_pdata(self, symbol: str) -> int | None:
        """X360 RUNTIME_FUNCTION: BeginAddress, then LSB-first bitfields in a BE
        dword — PrologLen:8, FunctionLen:22, ThirtyTwoBit:1, ExceptionFlag:1.

        The entry belonging to the function's own COMDAT is the one whose raw
        BeginAddress field (the reloc addend) is 0.
        """
        d = self.data
        best = None
        for sec in self.sections:
            if sec["name"] != ".pdata" or not sec["size"]:
                continue
            for j in range(sec["nrel"]):
                o = sec["relptr"] + j * 10
                rva, symidx, _rt = struct.unpack_from("<IIH", d, o)
                if symidx >= len(self.symrec):
                    continue
                if self.symrec[symidx][0] != symbol:
                    continue
                ent = rva - (rva % 8)
                if ent + 8 > sec["size"]:
                    continue
                w1, w2 = struct.unpack_from(">II", d, sec["raw"] + ent)
                if w1 != 0:
                    continue
                flen = (w2 >> 8) & 0x3FFFFF
                if flen:
                    best = flen * 4 if best is None else min(best, flen * 4)
        return best

    def _size_from_scan(self, sec) -> int:
        """Fallback: stop at the first EH funclet prologue (subi r31, r12, N)."""
        end = sec["size"]
        off = self.func_off + 4
        while off + 4 <= sec["size"]:
            w = struct.unpack_from(">I", self.data, sec["raw"] + off)[0]
            i = decode(off, w)
            if i.mnem == "addi" and i.rd == 31 and i.ra == 12 and i.simm < 0:
                end = off
                break
            off += 4
        return end - self.func_off

    # --- code access -------------------------------------------------------
    def word(self, va):
        if self.base_va <= va < self.base_va + self.sec_size:
            off = self.sec_raw + (va - self.base_va)
            return struct.unpack_from(">I", self.data, off)[0]
        sec, off = self._resolve_synth(va)
        return struct.unpack_from(">I", self.data, sec["raw"] + off)[0]

    def u16(self, va):
        if self.base_va <= va < self.base_va + self.sec_size:
            off = self.sec_raw + (va - self.base_va)
            return struct.unpack_from(">H", self.data, off)[0]
        sec, off = self._resolve_synth(va)
        return struct.unpack_from(">H", self.data, sec["raw"] + off)[0]

    def u8(self, va):
        if self.base_va <= va < self.base_va + self.sec_size:
            return self.data[self.sec_raw + (va - self.base_va)]
        sec, off = self._resolve_synth(va)
        return self.data[sec["raw"] + off]

    def _resolve_synth(self, va):
        for base, (sidx, soff) in self._jt_bases.items():
            sec = self.sections[sidx]
            if base <= va < base + sec["size"]:
                return sec, soff + (va - base)
        raise KeyError(f"unmapped synthetic VA {va:#x}")

    def synth_for_symbol(self, name: str) -> int | None:
        hit = self._symtab_by_name.get(name)
        if hit is None:
            return None
        sidx, off = hit
        if sidx == self.sec_idx:
            return self.base_va + off
        for base, (s2, o2) in self._jt_bases.items():
            if (s2, o2) == (sidx, off):
                return base
        base = self._next_synth
        self._next_synth += 0x00100000
        self._jt_bases[base] = (sidx, off)
        return base

    def call_name(self, ins):
        hit = self.relocs.get(ins.va)
        if hit:
            return hit[0]
        return None

    def reloc_target(self, va):
        hit = self.relocs.get(va)
        return (hit[0], 0) if hit else None


# ─────────────────────────────────────────────────────────────────────────────
# Function analysis
# ─────────────────────────────────────────────────────────────────────────────


@dataclass
class JumpTable:
    table_va: int
    base_va: int
    bound: int          # cmplwi immediate (last valid index)
    bias: int           # subtracted from the switch value before the compare
    entry_bytes: int
    scale: int = 1      # loaded entry is multiplied by this (slwi after the load)
    labels: dict[int, int] = field(default_factory=dict)   # case value -> target VA


@dataclass
class Block:
    start: int
    end: int = 0
    labels: list[int] = field(default_factory=list)
    calls: list[str] = field(default_factory=list)
    slots: set[int] = field(default_factory=set)
    ninsn: int = 0
    nullchecks: int = 0

    @property
    def key(self) -> tuple[int, ...]:
        return tuple(sorted(self.labels))


@dataclass
class FuncInfo:
    va: int
    size: int
    name: str
    frame: int = 0
    savegpr: int | None = None
    savefpr: int | None = None
    savevmx: int | None = None
    fp_delta: int | None = None      # `subi r31, r1, N`
    jts: list[JumpTable] = field(default_factory=list)
    blocks: list[Block] = field(default_factory=list)
    slots: dict[int, set[int]] = field(default_factory=dict)   # offset -> block idx set
    prologue_slots: set[int] = field(default_factory=set)
    insns: list[Insn] = field(default_factory=list)

    @property
    def label_to_block(self) -> dict[int, int]:
        out = {}
        for bi, b in enumerate(self.blocks):
            for L in b.labels:
                out[L] = bi
        return out

    @property
    def n_labels(self) -> int:
        return sum(len(b.labels) for b in self.blocks)


_SAVE_RE = re.compile(r"^_?_?(savegprlr|savefpr|savevmx|restgprlr)_(\d+)$")


def _imm_pair(view: CodeView, insns: list[Insn], idx: int) -> tuple[int, int] | None:
    """Given index of a `lis rX, hi`, find the matching addi/ori low half.

    Returns (absolute value, index of the low half) or None.
    """
    lis = insns[idx]
    if lis.mnem != "addis" or lis.ra != 0:
        return None
    reg = lis.rd
    for j in range(idx + 1, min(idx + 10, len(insns))):
        k = insns[j]
        if k.mnem == "addi" and k.ra == reg and k.rd == reg:
            val = (lis.uimm << 16) + k.simm
            rel = view.reloc_target(lis.va)
            if rel:
                base = None
                if isinstance(view, CoffView):
                    base = view.synth_for_symbol(rel[0])
                if base is not None:
                    return (base + val, j)
                return None
            return (val & 0xFFFFFFFF, j)
        if k.mnem in ("D_LOAD",) and k.ra == reg:
            # lwz rY, lo(rX) — absolute data reference, not a pointer materialise
            return None
    return None


def analyze(view: CodeView, va: int, size: int, name: str, syms: SymbolDB | None = None) -> FuncInfo:
    fi = FuncInfo(va=va, size=size, name=name)
    n = size // 4
    insns = [decode(va + 4 * i, view.word(va + 4 * i)) for i in range(n)]
    fi.insns = insns

    # ── prologue ────────────────────────────────────────────────────────────
    for i, ins in enumerate(insns[:24]):
        if ins.mnem == "bl":
            cn = view.call_name(ins) or ""
            m = _SAVE_RE.match(cn)
            if m:
                kind, num = m.group(1), int(m.group(2))
                if kind == "savegprlr":
                    fi.savegpr = num
                elif kind == "savefpr":
                    fi.savefpr = num
                elif kind == "savevmx":
                    fi.savevmx = num
        if ins.mnem == "D_STORE" and ins.op == 37 and ins.rd == 1 and ins.ra == 1:
            fi.frame = -ins.simm
            break
        if ins.mnem == "addi" and ins.rd == 31 and ins.ra == 1 and ins.simm < 0:
            fi.fp_delta = -ins.simm
    if fi.frame == 0:
        # large frame: lis/ori r12, -N ; stwux r1, r1, r12
        for i, ins in enumerate(insns[:24]):
            if ins.op == 31 and ins.xo == 183 and ins.rd == 1 and ins.ra == 1:
                for j in range(max(0, i - 6), i):
                    p = _imm_pair(view, insns, j)
                    if p:
                        v = p[0]
                        fi.frame = (0x100000000 - v) & 0xFFFFFFFF
                        break
                break

    # ── jump tables ─────────────────────────────────────────────────────────
    for i, ins in enumerate(insns):
        if ins.mnem != "bctr":
            continue
        win = insns[max(0, i - 24) : i]
        tbl = base = None
        entry_bytes = 0
        scale = 1
        for j, k in enumerate(win):
            if k.mnem in ("lhzx", "lwzx", "lbzx"):
                entry_bytes = {"lbzx": 1, "lhzx": 2, "lwzx": 4}[k.mnem]
                loadreg = k.rd
                # a slwi on the LOADED value scales it (1-byte tables store
                # instruction counts, not byte offsets)
                for jj in range(j + 1, len(win)):
                    q = win[jj]
                    if q.mnem == "rlwinm" and q.rd == loadreg:
                        sh = (q.word >> 11) & 0x1F
                        me = (q.word >> 1) & 0x1F
                        if me == 31 - sh:
                            scale = 1 << sh
                        break
                    if q.mnem == "add" and q.rd == 12:
                        break
                # rA of the X-form load holds the table pointer
                for jj in range(j - 1, -1, -1):
                    if win[jj].mnem == "addis" and win[jj].rd in (k.ra, k.rb):
                        p = _imm_pair(view, win, jj)
                        if p:
                            tbl = p[0]
                        break
                # the *later* lis/addi pair materialises the block base
                for jj in range(j + 1, len(win)):
                    if win[jj].mnem == "addis":
                        p = _imm_pair(view, win, jj)
                        if p:
                            base = p[0]
                        break
        if tbl is None:
            continue
        bound, bias = None, 0
        for k in reversed(win):
            if k.mnem == "cmpli" and bound is None:
                bound = k.uimm
            if k.mnem == "addi" and k.simm < 0 and bound is not None:
                bias = -k.simm
                break
        if bound is None:
            continue
        if base is None:
            base = insns[i + 1].va if i + 1 < len(insns) else va
        jt = JumpTable(table_va=tbl, base_va=base, bound=bound, bias=bias,
                       entry_bytes=entry_bytes or 2, scale=scale)
        for e in range(bound + 1):
            try:
                if jt.entry_bytes == 2:
                    off = view.u16(tbl + 2 * e)
                elif jt.entry_bytes == 1:
                    off = view.u8(tbl + e)
                else:
                    off = view.word(tbl + 4 * e)
            except Exception:
                break
            jt.labels[e + bias] = base + off * jt.scale
        fi.jts.append(jt)

    # ── blocks ──────────────────────────────────────────────────────────────
    tgt2labels: dict[int, list[int]] = defaultdict(list)
    for jt in fi.jts:
        for lab, t in jt.labels.items():
            tgt2labels[t].append(lab)
    starts = sorted(tgt2labels)
    for bi, s in enumerate(starts):
        e = starts[bi + 1] if bi + 1 < len(starts) else va + size
        fi.blocks.append(Block(start=s, end=e, labels=sorted(tgt2labels[s])))

    def block_of(a: int) -> int | None:
        lo, hi = 0, len(fi.blocks)
        while lo < hi:
            mid = (lo + hi) // 2
            if fi.blocks[mid].start <= a:
                lo = mid + 1
            else:
                hi = mid
        if lo == 0:
            return None
        b = fi.blocks[lo - 1]
        return lo - 1 if b.start <= a < b.end else None

    for ins in insns:
        bi = block_of(ins.va)
        if bi is not None:
            fi.blocks[bi].ninsn += 1
        if ins.mnem == "bl":
            cn = view.call_name(ins)
            if cn and not _SAVE_RE.match(cn) and bi is not None:
                fi.blocks[bi].calls.append(cn)
        if ins.mnem == "cmpli" and ins.uimm == 0 and bi is not None:
            fi.blocks[bi].nullchecks += 1
        if ins.is_frame_ref or (ins.mnem in ("D_LOAD", "D_STORE") and ins.ra in (1, 31)):
            if ins.mnem == "D_STORE" and ins.op == 37 and ins.rd == 1:
                continue
            off = ins.simm
            if off < 0:
                continue
            fi.slots.setdefault(off, set())
            if bi is None:
                fi.prologue_slots.add(off)
            else:
                fi.slots[off].add(bi)
                fi.blocks[bi].slots.add(off)
    return fi


# ─────────────────────────────────────────────────────────────────────────────
# Slot-ladder helpers
# ─────────────────────────────────────────────────────────────────────────────


def slot_sizes(fi: FuncInfo) -> dict[int, int]:
    offs = sorted(fi.slots)
    out = {}
    for i, o in enumerate(offs):
        nxt = offs[i + 1] if i + 1 < len(offs) else fi.frame
        out[o] = max(0, nxt - o)
    return out


def block_exclusive_bytes(fi: FuncInfo, bi: int, sizes: dict[int, int]) -> int:
    tot = 0
    for o in fi.blocks[bi].slots:
        if fi.slots.get(o) == {bi}:
            tot += sizes.get(o, 0)
    return tot


def local_floor(fi: FuncInfo) -> int:
    """Rough boundary between outgoing-arg area and the locals region."""
    if not fi.slots:
        return 0
    used_by_arms = [o for o, s in fi.slots.items() if s]
    return min(used_by_arms) if used_by_arms else min(fi.slots)


# ─────────────────────────────────────────────────────────────────────────────
# census
# ─────────────────────────────────────────────────────────────────────────────


def load_target_map() -> tuple[dict[str, str], dict[str, int]]:
    m = json.loads(TARGET_MAP.read_text())
    va2sym = {int(k, 16): v for k, v in m.items()
              if k.startswith("0x") or k.startswith("0X")}
    sym2va = {}
    for va, sym in va2sym.items():
        sym2va.setdefault(sym, va)
    return va2sym, sym2va



def report_percents(project_dir: Path) -> dict[tuple[str, str], float]:
    """(unit, function name) -> match_percent_normalized, from report.json."""
    out: dict[tuple[str, str], float] = {}
    rp = project_dir / "build" / TITLE / "report.json"
    if not rp.exists():
        return out
    for u in json.loads(rp.read_text())["units"]:
        for f in u.get("functions", []):
            out[(u["name"], f["name"])] = f.get("match_percent_normalized", 0.0)
    return out


def unit_report_name(unit: str) -> str:
    return "default/" + (unit[:-4] if unit.endswith(".cpp") else unit)


def obj_path_for(unit: str, project_dir: Path) -> Path:
    stem = unit[:-4] if unit.endswith(".cpp") else unit
    return project_dir / "build" / TITLE / "src" / (stem + ".obj")


def fmt_labels(labels: list[int], limit: int = 10) -> str:
    s = " ".join("%X" % L for L in labels[:limit])
    if len(labels) > limit:
        s += " …+%d" % (len(labels) - limit)
    return s


def census(args) -> int:
    pe = PEImage(IMAGE)
    syms = SymbolDB()
    splits = Splits()
    va2sym, sym2va = load_target_map()

    if args.va:
        va = int(args.va, 16)
        symbol = args.symbol or va2sym.get(va)
    else:
        symbol = args.symbol
        va = sym2va.get(symbol)
        if va is None:
            print(f"!! {symbol} has no entry in scripts/target_symbol_map.json;"
                  f" pass --va to analyse the retail side anyway", file=sys.stderr)
            return 2
    hit = syms.func_at(va)
    if not hit:
        print(f"!! no retail function at {va:#x}", file=sys.stderr)
        return 2
    size, rname = hit
    unit = splits.unit_of(va)

    R = analyze(RetailView(pe, syms), va, size, rname, syms)

    O = None
    err = None
    project_dir = Path(args.project_dir).resolve()
    if symbol and unit:
        op = obj_path_for(unit, project_dir)
        if op.exists():
            try:
                cv = CoffView(op, symbol)
                O = analyze(cv, cv.func_va, cv.func_size, symbol)
            except Exception as e:  # noqa: BLE001
                err = f"{type(e).__name__}: {e}"
        else:
            err = f"missing obj {op}"
    else:
        err = "no symbol mapping / unit"

    print("=" * 78)
    print(f"SWITCH FRAME CENSUS  {rname} @ {va:#x}  size {size:#x}")
    print(f"  unit   : {unit}")
    print(f"  symbol : {symbol}")
    if err:
        print(f"  ours   : UNAVAILABLE ({err})")
    print("=" * 78)

    def col(v, w=18):
        return str(v).ljust(w)

    print("\n-- FRAME ---------------------------------------------------------")
    print(f"  {'':10s} {col('retail')} {col('ours')} delta")
    rows = [
        ("frame", R.frame, O.frame if O else None),
        ("savegpr", R.savegpr, O.savegpr if O else None),
        ("savefpr", R.savefpr, O.savefpr if O else None),
        ("r31 delta", R.fp_delta, O.fp_delta if O else None),
    ]
    for label, rv, ov in rows:
        d = ""
        if isinstance(rv, int) and isinstance(ov, int) and rv != ov:
            d = f"{ov - rv:+#x}" if label in ("frame", "r31 delta") else f"{ov - rv:+d}"
        rs = f"{rv:#x}" if isinstance(rv, int) and label != "savegpr" and label != "savefpr" else rv
        os_ = f"{ov:#x}" if isinstance(ov, int) and label not in ("savegpr", "savefpr") else ov
        print(f"  {label:10s} {col(rs)} {col(os_)} {d}")
    if O and R.frame != O.frame:
        print(f"\n  >> FRAME DELTA {O.frame - R.frame:+#x} ({O.frame - R.frame:+d} bytes)"
              f" — the census below must account for it.")
    if O and R.savegpr != O.savegpr:
        print(f"  >> SAVEGPR DELTA: retail __savegprlr_{R.savegpr} vs ours"
              f" __savegprlr_{O.savegpr}. EH funclets encode the parent's saved-register"
              f" range — this alone holds them at 99.9%.")

    print("\n-- ARMS ----------------------------------------------------------")
    for tag, F in (("retail", R), ("ours", O)):
        if F is None:
            continue
        if not F.jts:
            print(f"  {tag:6s}: no jump table found (binary-search lowering or no switch)")
            continue
        for jt in F.jts:
            print(f"  {tag:6s}: table {jt.table_va:#x} base {jt.base_va:#x} "
                  f"bound {jt.bound:#x} bias {jt.bias:#x} entry {jt.entry_bytes}Bx{jt.scale} "
                  f"-> {len(jt.labels)} labels")
        shared = [b for b in F.blocks if len(b.labels) > 1]
        print(f"          {F.n_labels} labels -> {len(F.blocks)} distinct blocks "
              f"({len(shared)} shared)")

    if R.blocks and O and O.blocks:
        rl = {L for b in R.blocks for L in b.labels}
        ol = {L for b in O.blocks for L in b.labels}
        miss = sorted(rl - ol)
        surp = sorted(ol - rl)
        if miss:
            print(f"  MISSING labels (retail has, we don't): {fmt_labels(miss, 24)}")
        if surp:
            print(f"  SURPLUS labels (we have, retail doesn't): {fmt_labels(surp, 24)}")

    if R.blocks and O and O.blocks:
        print("\n-- ARM ORDER (physical block order == SOURCE order) --------------")
        ro = [b.key for b in R.blocks]
        oo = [b.key for b in O.blocks]
        if ro == oo:
            print("  identical — our `case` bodies are declared in retail's order")
        else:
            rset, oset = set(ro), set(oo)
            common_r = [k for k in ro if k in oset]
            common_o = [k for k in oo if k in rset]
            diffs = [(i, a, b) for i, (a, b) in enumerate(zip(common_r, common_o)) if a != b]
            print(f"  {len(diffs)} of {len(common_r)} shared arms are out of order.")
            for i, a, b in diffs[: args.top]:
                print(f"    slot {i:3d}: retail {fmt_labels(list(a), 6)}"
                      f"  vs ours {fmt_labels(list(b), 6)}")
            print("  >> MSVC emits case bodies in SOURCE order, and assigns each arm's "
                  "stack\n     slots in that order too — so reordering the `case` blocks "
                  "in our source\n     to retail's physical order is a mechanical, "
                  "behaviour-neutral edit.")

    print("\n-- SHARED ARM BLOCKS (retail) ------------------------------------")
    rshared = [b for b in R.blocks if len(b.labels) > 1]
    if not rshared:
        print("  (none)")
    for b in sorted(rshared, key=lambda b: -len(b.labels)):
        print(f"  {b.start:#010x}  {len(b.labels):3d} labels: {fmt_labels(b.labels, 16)}")

    # ── slot ladder ────────────────────────────────────────────────────────
    print("\n-- SLOT LADDER (locals region, low -> high) -----------------------")
    rs = slot_sizes(R)
    osz = slot_sizes(O) if O else {}
    rfloor = local_floor(R)
    ofloor = local_floor(O) if O else 0
    rlist = [(o, rs[o]) for o in sorted(R.slots) if o >= rfloor]
    olist = [(o, osz[o]) for o in sorted(O.slots) if o >= ofloor] if O else []
    print(f"  {'retail':>30s}    | {'ours':<30s}")
    for i in range(max(len(rlist), len(olist))):
        lhs = rhs = ""
        if i < len(rlist):
            o, s = rlist[i]
            arms = sorted(R.slots[o])
            lhs = f"{o:#06x} +{s:<4d} arms={len(arms):<3d} {fmt_labels([L for a in arms[:2] for L in R.blocks[a].labels], 4)}"
        if i < len(olist):
            o, s = olist[i]
            arms = sorted(O.slots[o])
            rhs = f"{o:#06x} +{s:<4d} arms={len(arms):<3d} {fmt_labels([L for a in arms[:2] for L in O.blocks[a].labels], 4)}"
        print(f"  {lhs:>44s} | {rhs}")

    # ── ranked candidate explanations ──────────────────────────────────────
    print("\n-- RANKED CANDIDATE EXPLANATIONS ---------------------------------")
    cands: list[tuple[int, str]] = []
    if O:
        delta = O.frame - R.frame
        rl2b = R.label_to_block
        ol2b = O.label_to_block
        # (1) shared blocks retail has that we split
        for b in R.blocks:
            if len(b.labels) < 2:
                continue
            ourbs = {ol2b[L] for L in b.labels if L in ol2b}
            if len(ourbs) > 1:
                extra = 0
                for bi in sorted(ourbs)[1:]:
                    extra += block_exclusive_bytes(O, bi, osz)
                cands.append((1000 + extra, (
                    f"MERGE arms {fmt_labels(b.labels, 8)} into ONE body — retail's jump "
                    f"table points them all at {b.start:#x}; we emit {len(ourbs)} bodies. "
                    f"Surplus frame from the extras ~{extra:#x} ({extra} B).")))
        # (2) labels we have that retail doesn't
        for L in sorted(set(ol2b) - set(rl2b)):
            bi = ol2b[L]
            ex = block_exclusive_bytes(O, bi, osz)
            cands.append((900 + ex, (
                f"DELETE case {L:#x} — retail's jump table has no such label. "
                f"Our body owns ~{ex:#x} ({ex} B) of exclusive frame.")))
        # (3) labels retail has that we don't
        for L in sorted(set(rl2b) - set(ol2b)):
            bi = rl2b[L]
            ex = block_exclusive_bytes(R, bi, rs)
            cands.append((880 + ex, (
                f"ADD case {L:#x} — retail dispatches it to {R.blocks[bi].start:#x} "
                f"({R.blocks[bi].ninsn} instrs, calls: "
                f"{', '.join(R.blocks[bi].calls[:3]) or 'none'}). Missing arms also change "
                f"MSVC's switch lowering shape, not just the frame.")))
        # (4) per-label body footprint deltas
        for L in sorted(set(rl2b) & set(ol2b)):
            rb, ob = R.blocks[rl2b[L]], O.blocks[ol2b[L]]
            if rl2b[L] != rl2b.get(min(rb.labels)):
                pass
            rbytes = sum(rs.get(o, 0) for o in rb.slots)
            obytes = sum(osz.get(o, 0) for o in ob.slots)
            d = obytes - rbytes
            if d == 0:
                continue
            extra_nc = ob.nullchecks - rb.nullchecks
            why = []
            if extra_nc > 0:
                why.append(f"{extra_nc} extra null-check/assert branch(es)")
            if len(ob.calls) != len(rb.calls):
                why.append(f"call count {len(ob.calls)} vs retail {len(rb.calls)}"
                           f" (retail: {', '.join(rb.calls[:4]) or 'none'})")
            cands.append((abs(d) * 2, (
                f"case {L:#x}: our arm touches {d:+#x} ({d:+d} B) more frame than retail "
                f"({obytes:#x} vs {rbytes:#x})"
                + ("; " + "; ".join(why) if why else ""))))
        cands.sort(key=lambda t: -t[0])
        if not cands:
            print("  (no arm-level explanation found — frame delta is not arm-shaped)")
        for score, text in cands[: args.top]:
            print(f"  [{score:5d}] {text}")
        print(f"\n  Frame delta to explain: {delta:+#x} ({delta:+d} B)")
    else:
        print("  (ours unavailable)")
    return 0


# ─────────────────────────────────────────────────────────────────────────────
# funclets — harvest EH-funclet map entries once the parent is byte-identical
# ─────────────────────────────────────────────────────────────────────────────

FUNCLET_PREFIXES = ("__unwind$", "__catch$", "__ehhandler$", "?dtor$", "?catch$",
                    "__jump_unwind$")


def funclets(args) -> int:
    """MSVC keeps a function and its EH funclets in ONE COMDAT, in source order.

    So once our parent function is byte-identical to retail's, the whole COMDAT
    maps linearly: funclet VA = parent VA - parent section offset + funclet
    section offset.  Every pair is still verified reloc-masked byte-identical
    before an entry is emitted — a positional guess is never enough.

    These funclets are individually far too small and too repetitive to home by
    global byte-identity (the standard homing scan returns all-MULTI on them),
    which is exactly why the parent-anchored form is worth having.
    """
    pe = PEImage(IMAGE)
    syms = SymbolDB()
    splits = Splits()
    va2sym, sym2va = load_target_map()
    project_dir = Path(args.project_dir).resolve()

    va = int(args.va, 16) if args.va else sym2va.get(args.symbol)
    symbol = args.symbol or va2sym.get(va)
    if va is None or symbol is None:
        print("!! need both a retail VA and our symbol (--va / --symbol)", file=sys.stderr)
        return 2
    unit = splits.unit_of(va)
    if unit is None:
        print(f"!! {va:#x} is not inside a pinned split", file=sys.stderr)
        return 2
    cv = CoffView(obj_path_for(unit, project_dir), symbol)
    sec = cv.sections[cv.sec_idx]
    delta = va - cv.func_off

    # parent must itself be byte-identical, else the linear map is meaningless
    reloff = {r - cv.base_va for r in cv.relocs}

    def masked_eq(off: int, size: int, tva: int) -> bool:
        try:
            theirs = bytearray(pe.read(tva, size))
        except KeyError:
            return False
        ours = bytearray(cv.data[sec["raw"] + off: sec["raw"] + off + size])
        if len(ours) != len(theirs):
            return False
        for r in reloff:
            if off <= r < off + size:
                for b in range(4):
                    if r - off + b < size:
                        ours[r - off + b] = 0
                        theirs[r - off + b] = 0
        return bytes(ours) == bytes(theirs)

    phit = syms.func_at(va)
    if not phit or phit[0] != cv.func_size:
        print(f"!! parent size mismatch: retail {phit[0]:#x} vs ours "
              f"{cv.func_size:#x} — fix the body/frame first")
        return 1
    if not masked_eq(cv.func_off, cv.func_size, va):
        print("!! parent is NOT reloc-masked byte-identical — fix it first; the "
              "positional funclet map is only sound behind an identical parent")
        return 1
    print(f"parent {symbol}\n  @ {va:#x} size {cv.func_size:#x} — byte-identical OK")

    pctmap = report_percents(project_dir)
    urn = unit_report_name(unit)

    names: dict[int, str] = {}
    for n, v, sn, _c in cv.symrec:
        if sn - 1 == cv.sec_idx and v > cv.func_off and n.startswith(FUNCLET_PREFIXES):
            names.setdefault(v, n)

    emit: dict[str, str] = {}
    for off, name in sorted(names.items()):
        tva = delta + off
        hit = syms.func_at(tva)
        if not hit:
            print(f"  {name:24s} -> {tva:#010x}  SKIP (no retail function there)")
            continue
        size, _rn = hit
        ok = masked_eq(off, size, tva)
        already = va2sym.get(tva)
        anon_pct = pctmap.get((urn, "fn_%08X" % tva))
        state = "IDENTICAL" if ok else "DIFFERS"
        if already:
            state += f" (already mapped -> {already})"
        elif anon_pct == 100.0:
            state += " (ALREADY 100% anonymously — naming it REGRESSES; skipped)"
        elif ok:
            emit[f"0x{tva:08x}"] = name
        print(f"  {name:24s} -> {tva:#010x} size {size:#6x}  {state}")

    print(f"\n{len(emit)} new map entries")
    if not emit:
        return 0
    if args.apply:
        path = REPO / "scripts" / "target_symbol_map.json" if args.project_dir is None \
            else project_dir / "scripts" / "target_symbol_map.json"
        text = path.read_text()
        anchor = text.rindex("\n}")
        ins = "".join(f',\n  "{k}": "{v}"' for k, v in emit.items())
        # keep the file textual (never json.dump — it reorders/reformats)
        head = text[:anchor].rstrip()
        text = head + ins + text[anchor:]
        json.loads(text)
        path.write_text(text)
        print(f"applied to {path}")
    else:
        for k, v in emit.items():
            print(f'  "{k}": "{v}",')
        print("(re-run with --apply to write them into target_symbol_map.json)")
    return 0


# ─────────────────────────────────────────────────────────────────────────────
# find — scan pinned in-scope units for switch functions
# ─────────────────────────────────────────────────────────────────────────────

BCTR = 0x4E800420

# XDK vendor + Quazal were hard-skipped by the project owner.
SCOPE_PREFIXES = ("band3/", "network/", "system/")
SCOPE_DENY = ("auto_", "vendor/", "xdk/")


def in_scope(unit: str) -> bool:
    if unit.startswith(SCOPE_DENY):
        return False
    return unit.startswith(SCOPE_PREFIXES) or "/" not in unit


def funclet_index(pe: PEImage, syms: SymbolDB, spans: list[tuple[int, int]]):
    """frame size -> [funclet VAs] for funclets living in these spans."""
    out: dict[int, list[int]] = defaultdict(list)
    for s, e in spans:
        for va, size, _n in syms.funcs:
            if va < s:
                continue
            if va >= e:
                break
            if size < 8:
                continue
            try:
                w = pe.word(va)
            except KeyError:
                continue
            i = decode(va, w)
            if i.mnem == "addi" and i.rd == 31 and i.ra == 12 and i.simm < 0:
                out[-i.simm].append(va)
    return out


def find(args) -> int:
    pe = PEImage(IMAGE)
    syms = SymbolDB()
    splits = Splits()
    va2sym, _ = load_target_map()

    pct_by_unit = report_percents(Path(args.project_dir).resolve())
    pct: dict[str, float] = {}
    for (_u, fn), v in pct_by_unit.items():
        pct[fn] = max(v, pct.get(fn, 0.0))

    rows = []
    view = RetailView(pe, syms)
    for unit, spans in sorted(splits.units.items()):
        if not in_scope(unit):
            continue
        fidx = funclet_index(pe, syms, spans)
        # how many parents in this unit share each frame size (ambiguity guard)
        frame_owners: dict[int, int] = defaultdict(int)
        parents = []
        for s, e in spans:
            for va, size, name in syms.funcs:
                if va < s:
                    continue
                if va >= e:
                    break
                if size < args.min_size:
                    continue
                try:
                    words = [pe.word(va + 4 * i) for i in range(size // 4)]
                except KeyError:
                    continue
                if BCTR not in words:
                    continue
                parents.append((va, size, name))
        for va, size, name in parents:
            try:
                fi = analyze(view, va, size, name, syms)
            except Exception:
                continue
            if not fi.jts:
                continue
            frame_owners[fi.frame] += 1
        for va, size, name in parents:
            try:
                fi = analyze(view, va, size, name, syms)
            except Exception:
                continue
            if not fi.jts:
                continue
            fl = fidx.get(fi.frame, [])
            urn = unit_report_name(unit)
            open_fl = [f for f in fl
                       if pct_by_unit.get((urn, "fn_%08X" % f), 0.0) != 100.0]
            sym = va2sym.get(va)
            p = pct.get(sym) if sym else None
            if args.max_percent is not None and p is not None and p > args.max_percent:
                continue
            rows.append(dict(
                unit=unit, va=va, size=size, symbol=sym, percent=p,
                frame=fi.frame, savegpr=fi.savegpr,
                labels=fi.n_labels, blocks=len(fi.blocks),
                shared=sum(1 for b in fi.blocks if len(b.labels) > 1),
                funclets=len(open_fl), funclets_total=len(fl),
                ambiguous=frame_owners.get(fi.frame, 0) > 1,
            ))

    rows.sort(key=lambda r: (-r["funclets"], -r["size"]))
    if args.json:
        print(json.dumps(rows, indent=1))
        return 0
    print(f"{len(rows)} switch functions in pinned in-scope units "
          f"(min size {args.min_size:#x}"
          + (f", match<={args.max_percent}" if args.max_percent is not None else "")
          + ")")
    print()
    hdr = (f"{'VA':>10} {'size':>6} {'frame':>6} {'sgpr':>4} {'arms':>5} "
           f"{'blk':>4} {'shr':>4} {'open/tot':>9} {'%':>7}  unit / symbol")
    print(hdr)
    print("-" * len(hdr))
    for r in rows[: args.limit]:
        p = f"{r['percent']:.1f}" if r["percent"] is not None else "-"
        amb = "~" if r["ambiguous"] else " "
        print(f"{r['va']:#010x} {r['size']:#6x} {r['frame']:#6x} "
              f"{str(r['savegpr']):>4} {r['labels']:5d} {r['blocks']:4d} {r['shared']:4d} "
              f"{r['funclets']:4d}/{r['funclets_total']:<4d}{amb}{p:>7}  {r['unit']}")
        if r["symbol"]:
            print(f"{'':>46}   {r['symbol']}")
    print("\n(open/tot = EH funclets with that parent frame that are NOT yet at "
          "strict 100% / all of them.  objdiff pairs funclets positionally even "
          "when they are anonymous, so a funclet whose body already matches is "
          "already banked — only the OPEN column is available work, and naming an "
          "already-matched funclet in target_symbol_map.json REGRESSES it.)")
    print("(~ = more than one switch parent in the unit shares that frame size, "
          "so the funclet attribution is a lower-confidence guess)")
    return 0


# ─────────────────────────────────────────────────────────────────────────────


def main(argv=None) -> int:
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--project-dir", default=str(REPO),
                    help="worktree to read our compiled objs / report.json from")
    sub = ap.add_subparsers(dest="mode", required=True)

    c = sub.add_parser("census", help="retail-vs-ours slot census for one function")
    c.add_argument("--symbol", help="our mangled symbol")
    c.add_argument("--va", help="retail VA (hex), e.g. 0x82550880")
    c.add_argument("--top", type=int, default=15)
    c.set_defaults(func=census)

    fc = sub.add_parser("funclets",
                        help="harvest EH-funclet map entries behind an identical parent")
    fc.add_argument("--symbol", help="our mangled symbol")
    fc.add_argument("--va", help="retail VA (hex)")
    fc.add_argument("--apply", action="store_true",
                    help="write the entries into scripts/target_symbol_map.json")
    fc.set_defaults(func=funclets)

    f = sub.add_parser("find", help="scan for switch functions worth the frame lever")
    f.add_argument("--limit", type=int, default=40)
    f.add_argument("--min-size", type=lambda s: int(s, 0), default=0x100)
    f.add_argument("--max-percent", type=float, default=None,
                   help="only functions at or below this match%% (unmapped count as -)")
    f.add_argument("--json", action="store_true")
    f.set_defaults(func=find)

    args = ap.parse_args(argv)
    return args.func(args)


if __name__ == "__main__":
    sys.exit(main())
