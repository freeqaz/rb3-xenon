#!/usr/bin/env python3
"""funclet_cascade_rank.py — rank parent functions by EH-funclet cascade yield.

WHY
---
MSVC X360 emits a separate *EH funclet* (unwind action / catch handler) for each
cleanup state and catch block of a function compiled with ``/EHsc``.  A funclet's
first instruction is ``subi rX, r12, <PARENT FRAME>`` — r12 holds the parent's
pre-``stwu`` stack pointer, so the funclet literally *encodes its parent's frame
size in its own machine code*.

Empirically (lanes C and F, 2026-07-24) a funclet flips to a strict 100% match as
soon as the PARENT's

  1. frame size (``stwu r1, -N(r1)`` immediate), and
  2. saved-register range (``bl __savegprlr_N``)

are both exact — **independent of whether the parent's body matches at all**.
Lane F flipped 145 funclets in one build from a 6-line source diff with the
parent still at 0%.

So "which parent, if I fixed only its frame, would flip the most functions?" is a
high-yield question.  This tool answers it.

HOW
---
Ground truth is the extracted retail PE (``orig/45410914/band.exe``), NOT the dtk
asm listings:

* ``.pdata`` gives every function's start / length / prolog length, plus an
  *exception flag* (bit 31 of word 1).
* For an exception-flagged function the two DWORDs immediately **before** its
  entry point are ``{ handler, handlerData }`` (the classic MIPS/PPC PE
  convention).  ``handlerData`` points at an MSVC ``_s_FuncInfo``
  (magic ``0x199305xx``), whose *unwind map* actions and *try-block map* catch
  handlers are exactly the funclet entry points.

  That gives **exact** parent -> funclet association.  No frame-size guessing, no
  spatial heuristics.
* An independent *prologue screen* (``subi rX, r12, imm`` as instruction 0) over
  every ``.pdata`` entry provides the total funclet census and cross-checks the
  EH-derived set.

  ⚠ Do NOT screen the dtk ``build/45410914/asm/*.s`` address column: for units
  whose COMDATs are scattered, dtk labels ranges 2..n with synthetic
  ``unit_base + obj_offset`` addresses that are not real VAs (only ~20% of the
  asm-listed addresses agree with the PE).  The ``.fn fn_<VA>`` *label* is the
  real VA; the comment column is not.

The VA -> unit join uses ``config/45410914/splits.txt``'s pinned ``.pdata``
ranges: each pinned 8-byte ``.pdata`` entry names exactly one function VA, which
survives COMDAT scatter.

Match state comes from ``build/45410914/report.json``; VA -> report symbol name
is ``scripts/target_symbol_map.json`` (mangled) else ``fn_%08X``.

Base-side (our compiled) frame + ``__savegprlr_N`` are read straight out of the
COFF object in ``build/45410914/src/...``, so no objdiff run is needed.

USAGE
-----
    python3 scripts/harvest/funclet_cascade_rank.py            # ranked markdown
    python3 scripts/harvest/funclet_cascade_rank.py --json out.json
    python3 scripts/harvest/funclet_cascade_rank.py --census   # pool sizing only
    python3 scripts/harvest/funclet_cascade_rank.py --dump 0x82550880
"""
from __future__ import annotations

import argparse
import json
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

# __savegprlr_14 / __restgprlr_14 in the retail TU5 image.  Verified at runtime.
SAVEGPRLR_14 = 0x82829220
RESTGPRLR_14 = 0x82829270

# auto_03_* spans are XDK vendor + Quazal, hard-skipped by the project owner.
VENDOR_UNIT_RX = re.compile(r"^(?:default/)?auto_\d+_")


# --------------------------------------------------------------------------- PE
class PE:
    """Minimal big-endian-payload PE reader (LE headers, BE section data)."""

    def __init__(self, path: Path):
        self.d = open(path, "rb").read()
        pe = struct.unpack_from("<I", self.d, 0x3C)[0]
        nsec = struct.unpack_from("<H", self.d, pe + 6)[0]
        optsz = struct.unpack_from("<H", self.d, pe + 20)[0]
        self.base = struct.unpack_from("<I", self.d, pe + 24 + 28)[0]
        so = pe + 24 + optsz
        self.secs = []
        for i in range(nsec):
            nm = self.d[so + i * 40 : so + i * 40 + 8].rstrip(b"\0").decode("latin1")
            vs, va, rs, ro = struct.unpack_from("<IIII", self.d, so + i * 40 + 8)
            self.secs.append((nm, self.base + va, vs, ro, rs))

    def off(self, va: int):
        for _nm, v, vs, ro, rs in self.secs:
            if ro and v <= va < v + min(vs, rs) if rs else False:
                return ro + (va - v)
        # second pass: allow vs>rs sections but clamp to raw size
        for _nm, v, vs, ro, rs in self.secs:
            if ro and v <= va < v + vs and (va - v) < rs:
                return ro + (va - v)
        return None

    def u32(self, va: int):
        o = self.off(va)
        if o is None or o + 4 > len(self.d):
            return None
        return struct.unpack_from(">I", self.d, o)[0]

    def words(self, va: int, n: int):
        o = self.off(va)
        if o is None or o + 4 * n > len(self.d):
            return []
        return list(struct.unpack_from(">%dI" % n, self.d, o))

    def section(self, name: str):
        for s in self.secs:
            if s[0] == name:
                return s
        return None


def s16(x: int) -> int:
    return x - 0x10000 if x >= 0x8000 else x


def s32(x):
    return None if x is None else (x - (1 << 32) if x >= (1 << 31) else x)


# ------------------------------------------------------------------ instructions
def bl_target(word: int, va: int):
    """Return the target of an unconditional ``bl`` (primary 18, LK=1, AA=0)."""
    if (word >> 26) != 18 or not (word & 1) or (word & 2):
        return None
    li = word & 0x03FFFFFC
    if li >= 0x02000000:
        li -= 0x04000000
    return (va + li) & 0xFFFFFFFF


def savegpr_n(word: int, va: int):
    t = bl_target(word, va)
    if t is None:
        return None
    for base in (SAVEGPRLR_14, RESTGPRLR_14):
        if base <= t <= base + 17 * 4 and (t - base) % 4 == 0:
            return 14 + (t - base) // 4
    return None


def decode_frame(words: list[int], va: int, limit: int = 20):
    """Frame size from ``stwu r1, -N(r1)`` (or the lis/ori + stwux big-frame form)."""
    for i, w in enumerate(words[:limit]):
        if (w >> 26) == 37 and ((w >> 21) & 31) == 1 and ((w >> 16) & 31) == 1:
            return -s16(w & 0xFFFF)
        # stwux r1, r1, rT  (primary 31, xo 183) -> frame built in rT by lis/ori
        if (w >> 26) == 31 and ((w >> 1) & 0x3FF) == 183 and ((w >> 21) & 31) == 1:
            rt = (w >> 11) & 31
            hi = lo = None
            for j in range(max(0, i - 6), i):
                v = words[j]
                if (v >> 26) == 15 and ((v >> 21) & 31) == rt:  # lis
                    hi = (v & 0xFFFF) << 16
                elif (v >> 26) == 24 and ((v >> 21) & 31) == rt:  # ori
                    lo = v & 0xFFFF
            if hi is not None:
                val = hi | (lo or 0)
                return (1 << 32) - val if val >= (1 << 31) else -val
    return None


def find_savegpr(words: list[int], va: int, limit: int = 8):
    for i, w in enumerate(words[:limit]):
        n = savegpr_n(w, va + 4 * i)
        if n is not None:
            return n
    return None


def funclet_parent_frame(words: list[int]):
    """If instruction 0 is ``subi rX, r12, imm``, return (imm, rX); else None."""
    if not words:
        return None
    w = words[0]
    if (w >> 26) == 14 and ((w >> 16) & 31) == 12:
        simm = s16(w & 0xFFFF)
        if simm < 0:
            return (-simm, (w >> 21) & 31)
    return None


# ------------------------------------------------------------------------ .pdata
def parse_pdata(pe: PE) -> dict:
    sec = pe.section(".pdata")
    if not sec:
        raise SystemExit("no .pdata section")
    _nm, va, vs, ro, rs = sec
    n = min(vs, rs) // 8
    out = {}
    for i in range(n):
        b, w = struct.unpack_from(">II", pe.d, ro + i * 8)
        if not b:
            continue
        out[b] = {
            "size": ((w >> 8) & 0x3FFFFF) * 4,
            "prolog": (w & 0xFF) * 4,
            "eh": (w >> 31) & 1,
            "pdata_va": va + i * 8,
        }
    return out


# ------------------------------------------------------------------- EH FuncInfo
def parse_eh(pe: PE, funcs: dict):
    """Exact parent -> funclet association from MSVC ``_s_FuncInfo``.

    Returns (funclets_of: parent -> [(kind, va)], parent_of: funclet -> parent,
             stats).
    """
    raw_children = defaultdict(list)
    stats = defaultdict(int)
    for va, info in funcs.items():
        if not info["eh"]:
            continue
        stats["eh_functions"] += 1
        hdata = pe.u32(va - 4)
        if not hdata:
            stats["no_handler_data"] += 1
            continue
        magic = pe.u32(hdata)
        if magic is None or (magic >> 8) != 0x199305:
            stats["bad_magic"] += 1
            continue
        max_state = s32(pe.u32(hdata + 4)) or 0
        p_unwind = pe.u32(hdata + 8)
        n_try = pe.u32(hdata + 12) or 0
        p_try = pe.u32(hdata + 16)
        if p_unwind and max_state > 0:
            for i in range(min(max_state, 4096)):
                act = pe.u32(p_unwind + i * 8 + 4)
                if act:
                    raw_children[va].append(("unwind", act))
        if p_try and n_try:
            for i in range(min(n_try, 1024)):
                e = p_try + i * 20
                n_catch = pe.u32(e + 12) or 0
                p_ha = pe.u32(e + 16)
                if not p_ha:
                    continue
                for j in range(min(n_catch, 256)):
                    h = pe.u32(p_ha + j * 16 + 12)
                    if h:
                        raw_children[va].append(("catch", h))

    # A catch funclet may itself carry EH data; its funclets still live in the
    # ROOT function's frame (r12 is the root frame pointer).  Collapse to roots.
    direct_parent = {}
    for p, kids in raw_children.items():
        for kind, k in kids:
            if k != p:
                direct_parent.setdefault(k, (p, kind))

    def root_of(va, seen=None):
        seen = seen or set()
        while va in direct_parent and va not in seen:
            seen.add(va)
            va = direct_parent[va][0]
        return va

    funclets_of = defaultdict(list)
    parent_of = {}
    for k, (p, kind) in direct_parent.items():
        r = root_of(p)
        if r == k:
            continue
        funclets_of[r].append((kind, k))
        parent_of[k] = r
    for k in list(funclets_of):
        funclets_of[k] = sorted(set(funclets_of[k]), key=lambda t: t[1])
    stats["eh_derived_funclets"] = len(parent_of)
    stats["eh_parents"] = len(funclets_of)
    return funclets_of, parent_of, stats


def prologue_screen(pe: PE, funcs: dict):
    """Independent census: every .pdata function whose instr 0 is subi rX,r12,imm."""
    out = {}
    for va, info in funcs.items():
        w = pe.words(va, 1)
        r = funclet_parent_frame(w)
        if r:
            out[va] = r[0]
    return out


# ------------------------------------------------------------------------- joins
def parse_splits(path: Path):
    """unit -> {'pdata': [(s,e)], 'text': [(s,e)]}"""
    units = defaultdict(lambda: defaultdict(list))
    cur = None
    rx = re.compile(r"^\s+(\.\w+)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)")
    for line in open(path):
        if line.rstrip().endswith(":") and not line.startswith((" ", "\t")):
            cur = line.strip()[:-1]
            continue
        m = rx.match(line)
        if m and cur:
            units[cur][m.group(1)].append((int(m.group(2), 16), int(m.group(3), 16)))
    return units


def unit_function_vas(pe: PE, units: dict):
    """VA -> unit, using each unit's pinned .pdata entries (survives scatter)."""
    va2unit = {}
    unit_vas = defaultdict(set)
    for unit, secs in units.items():
        for s, e in secs.get(".pdata", []):
            for p in range(s, e, 8):
                fva = pe.u32(p)
                if fva:
                    va2unit.setdefault(fva, unit)
                    unit_vas[unit].add(fva)
    return va2unit, unit_vas


def unit_text_spans(units: dict):
    spans = []
    for unit, secs in units.items():
        for s, e in secs.get(".text", []):
            spans.append((s, e, unit))
    spans.sort()
    return spans


def span_unit(spans, va):
    lo, hi = 0, len(spans) - 1
    while lo <= hi:
        mid = (lo + hi) // 2
        s, e, u = spans[mid]
        if va < s:
            hi = mid - 1
        elif va >= e:
            lo = mid + 1
        else:
            return u
    return None


# ---------------------------------------------------------------- report.json
def load_report(path: Path):
    """(unit,name) -> match%, plus unit -> source_path, and name index per unit."""
    r = json.load(open(path))
    match = {}
    src = {}
    for u in r["units"]:
        un = u["name"]
        md = u.get("metadata") or {}
        if md.get("source_path"):
            src[un] = md["source_path"]
        for f in u.get("functions", []):
            match[(un, f["name"])] = f.get("match_percent_normalized")
    return match, src


def load_symbol_map(path: Path):
    m = json.load(open(path))
    return {k.lower(): v for k, v in m.items() if isinstance(v, str)}


def report_name(symmap: dict, va: int) -> str:
    return symmap.get("0x%08x" % va, "fn_%08X" % va)


def unit_report_key(unit_cpp: str, report_units: set) -> str | None:
    """'MetaPanel.cpp' -> 'default/MetaPanel' (report.json unit id)."""
    stem = unit_cpp[:-4] if unit_cpp.endswith(".cpp") else unit_cpp
    for cand in ("default/" + stem, stem):
        if cand in report_units:
            return cand
    return None


# ------------------------------------------------------------------- COFF (ours)
class Coff:
    """Just enough MS-COFF to read a function's prologue + its relocations."""

    def __init__(self, path: Path):
        self.d = d = open(path, "rb").read()
        nsec, symptr, nsym = struct.unpack_from("<HxxxxII", d, 2)
        nsec = struct.unpack_from("<H", d, 2)[0]
        symptr, nsym = struct.unpack_from("<II", d, 8)
        self.strtab = symptr + nsym * 18
        self.secs = []
        for i in range(nsec):
            o = 20 + i * 40
            vs, va, sz, ptr, rptr, lptr, nr, nl, fl = struct.unpack_from(
                "<IIIIIIHHI", d, o + 8
            )
            self.secs.append({"size": sz, "ptr": ptr, "rptr": rptr, "nreloc": nr})
        self.syms = {}
        self._symlist = []
        i = 0
        while i < nsym:
            o = symptr + i * 18
            raw = d[o : o + 8]
            if raw[:4] == b"\0\0\0\0":
                off = struct.unpack_from("<I", raw, 4)[0]
                name = self._str(off)
            else:
                name = raw.rstrip(b"\0").decode("latin1")
            val, sec, typ, cls, naux = struct.unpack_from("<IhHBB", d, o + 8)
            self.syms.setdefault(name, (val, sec))
            self._symlist.append(name)
            for _ in range(naux):
                self._symlist.append(None)
            i += 1 + naux

    def _str(self, off):
        e = self.d.index(b"\0", self.strtab + off)
        return self.d[self.strtab + off : e].decode("latin1")

    def func_words(self, name: str, n: int = 24):
        ent = self.syms.get(name)
        if not ent:
            return None, None, None
        val, sec = ent
        if sec <= 0 or sec > len(self.secs):
            return None, None, None
        s = self.secs[sec - 1]
        o = s["ptr"] + val
        avail = min(n, max(0, (s["size"] - val) // 4))
        if avail <= 0:
            return None, None, None
        return list(struct.unpack_from(">%dI" % avail, self.d, o)), sec, val

    def reloc_syms(self, sec_idx: int, start: int, end: int):
        """{section-relative address: symbol name} for relocs in [start,end)."""
        s = self.secs[sec_idx - 1]
        out = {}
        for i in range(s["nreloc"]):
            o = s["rptr"] + i * 10
            addr, symidx = struct.unpack_from("<II", self.d, o)
            if start <= addr < end and symidx < len(self._symlist):
                nm = self._symlist[symidx]
                if nm:
                    out[addr] = nm
        return out


SAVEGPR_RX = re.compile(r"__savegprlr_(\d+)")


def base_frame(objdir: Path, source_path: str, sym: str, cache: dict):
    """(frame, savegprlr_N) for our compiled version of ``sym``; None if absent."""
    if source_path is None:
        return None, None, "no-source"
    op = objdir / (source_path[4:] if source_path.startswith("src/") else source_path)
    op = op.with_suffix(".obj")
    key = str(op)
    if key not in cache:
        cache[key] = Coff(op) if op.exists() else None
    c = cache[key]
    if c is None:
        return None, None, "no-obj"
    words, sec, val = c.func_words(sym)
    if not words:
        return None, None, "no-sym"
    frame = decode_frame(words, 0)
    sg = None
    rl = c.reloc_syms(sec, val, val + 4 * len(words))
    for i, w in enumerate(words[:8]):
        if (w >> 26) == 18 and (w & 1):
            nm = rl.get(val + 4 * i, "")
            m = SAVEGPR_RX.search(nm)
            if m:
                sg = int(m.group(1))
                break
    return frame, sg, "ok"


# ------------------------------------------------------------------------- build
def build(repo: Path, exe: Path | None = None):
    exe = exe or repo / "orig/45410914/band.exe"
    pe = PE(exe)
    # sanity: the __savegprlr_14 thunk must be `std r14, -0x98(r1)`
    w = pe.words(SAVEGPRLR_14, 1)
    if not w or (w[0] >> 26) != 62 or ((w[0] >> 21) & 31) != 14:
        raise SystemExit(
            "__savegprlr_14 sanity check failed at %#x — wrong binary?" % SAVEGPRLR_14
        )

    funcs = parse_pdata(pe)
    funclets_of, parent_of, ehstats = parse_eh(pe, funcs)
    screened = prologue_screen(pe, funcs)

    units = parse_splits(repo / "config/45410914/splits.txt")
    va2unit, _unit_vas = unit_function_vas(pe, units)
    spans = unit_text_spans(units)
    match, srcmap = load_report(repo / "build/45410914/report.json")
    report_units = {u for (u, _n) in match}
    symmap = load_symbol_map(repo / "scripts/target_symbol_map.json")
    objdir = repo / "build/45410914/src"
    coff_cache = {}

    ucache = {}

    def unit_of(va):
        if va in ucache:
            return ucache[va]
        u = va2unit.get(va) or span_unit(spans, va)
        r = unit_report_key(u, report_units) if u else None
        ucache[va] = r
        return r

    def match_of(va):
        u = unit_of(va)
        if not u:
            return None, None
        return u, match.get((u, report_name(symmap, va)))

    rows = []
    for parent, kids in funclets_of.items():
        # Only r12-frame-encoding funclets participate in the cascade.  The EH
        # maps also list ordinary out-of-line cleanup functions (own `stwu`
        # prologue, no r12 dependency) — those are normal decomp targets, not
        # cascade beneficiaries, so drop them here.
        kid_vas = [k for _kind, k in kids if k in screened]
        if not kid_vas:
            continue
        # target-side truth for the parent frame comes from the funclets themselves
        imms = [screened.get(k) for k in kid_vas]
        imms = [i for i in imms if i]
        enc_frame = max(set(imms), key=imms.count) if imms else None

        pw = pe.words(parent, 24)
        p_frame = decode_frame(pw, parent)
        p_sg = find_savegpr(pw, parent)

        punit, pmatch = match_of(parent)
        pname = report_name(symmap, parent)

        n_pinned = n_unmatched = 0
        kid_units = set()
        for k in kid_vas:
            ku, km = match_of(k)
            if ku:
                kid_units.add(ku)
                n_pinned += 1
                if km is None or km < 100.0:
                    n_unmatched += 1

        b_frame = b_sg = None
        base_status = "not-pinned"
        if punit:
            b_frame, b_sg, base_status = base_frame(
                objdir, srcmap.get(punit), pname, coff_cache
            )

        flags = []
        if not punit:
            flags.append("PARENT_UNPINNED")
        elif kid_units and punit not in kid_units:
            flags.append("PARENT_OFFUNIT")
        if enc_frame is not None and p_frame is not None and enc_frame != p_frame:
            flags.append("ENCFRAME_NE_PROLOGUE")
        if pname.startswith("fn_"):
            flags.append("UNNAMED")

        rows.append(
            {
                "parent_va": "0x%08X" % parent,
                "parent_name": pname,
                "parent_unit": punit,
                "parent_match": pmatch,
                "kid_units": sorted(kid_units),
                "funclets": len(kid_vas),
                "funclets_pinned": n_pinned,
                "funclets_unmatched": n_unmatched,
                "tgt_frame": p_frame if p_frame is not None else enc_frame,
                "enc_frame": enc_frame,
                "tgt_savegpr": p_sg,
                "base_frame": b_frame,
                "base_savegpr": b_sg,
                "frame_delta": (b_frame - p_frame)
                if (b_frame is not None and p_frame is not None)
                else None,
                "base_status": base_status,
                "flags": flags,
                "funclet_vas": ["0x%08X" % k for k in kid_vas],
            }
        )
    rows.sort(key=lambda r: (-r["funclets_unmatched"], -r["funclets"]))
    census = census_stats(pe, funcs, screened, parent_of, unit_of, match_of, ehstats)
    return rows, census, dict(ehstats)


def census_stats(pe, funcs, screened, parent_of, unit_of, match_of, ehstats):
    c = {
        "pdata_functions": len(funcs),
        "eh_flagged_functions": ehstats["eh_functions"],
        "funclets_prologue_screen": len(screened),
        "funclets_eh_derived": len(parent_of),
        "funclets_both": len(set(screened) & set(parent_of)),
        "funclets_screen_only": len(set(screened) - set(parent_of)),
        "funclets_eh_only": len(set(parent_of) - set(screened)),
    }
    pinned = matched = vendor = 0
    for k in screened:
        u = unit_of(k)
        if not u:
            continue
        if VENDOR_UNIT_RX.match(u):
            vendor += 1
            continue
        pinned += 1
        _u, m = match_of(k)
        if m == 100.0:
            matched += 1
    c["funclets_pinned_nonvendor"] = pinned
    c["funclets_pinned_vendor"] = vendor
    c["funclets_pinned_matched"] = matched
    c["funclets_addressable"] = pinned - matched
    return c


# -------------------------------------------------------------------------- main
def fmt_hex(v):
    return "—" if v is None else ("0x%X" % v if v >= 0 else "-0x%X" % -v)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--repo", default=str(Path(__file__).resolve().parents[2]))
    ap.add_argument("--exe", default=None)
    ap.add_argument("--json", help="write full rows to this path")
    ap.add_argument("--top", type=int, default=40)
    ap.add_argument("--census", action="store_true", help="pool sizing only")
    ap.add_argument(
        "--calibrate",
        action="store_true",
        help="measured flip-rate of the lever, bucketed by frame/savegprlr state",
    )
    ap.add_argument("--include-vendor", action="store_true")
    ap.add_argument("--dump", help="explain one parent VA")
    ap.add_argument("--min-unmatched", type=int, default=1)
    a = ap.parse_args()

    repo = Path(a.repo)
    rows, census, ehstats = build(repo, Path(a.exe) if a.exe else None)

    print("## Funclet census (whole binary)\n")
    for k, v in census.items():
        print("* `%s` = **%s**" % (k, f"{v:,}"))
    print()
    if a.calibrate:
        buckets = {}
        for r in rows:
            if not r["funclets_pinned"]:
                continue
            if r["base_frame"] is None:
                k = "parent has no base symbol"
            elif r["frame_delta"] != 0:
                k = "frame MISMATCH"
            elif r["base_savegpr"] != r["tgt_savegpr"]:
                k = "frame ok, savegprlr MISMATCH"
            else:
                k = "frame ok + savegprlr ok"
            b = buckets.setdefault(k, [0, 0, 0])
            b[0] += r["funclets_pinned"]
            b[1] += r["funclets_pinned"] - r["funclets_unmatched"]
            b[2] += 1
        print("## Measured lever calibration\n")
        print("| bucket | parents | funclets | matched | rate |")
        print("|---|--:|--:|--:|--:|")
        for k, (t, m, p) in sorted(buckets.items(), key=lambda x: -x[1][0]):
            print("| %s | %d | %d | %d | %.1f%% |" % (k, p, t, m, 100 * m / t if t else 0))
        return

    if a.census:
        return

    if a.dump:
        want = int(a.dump, 16)
        for r in rows:
            if int(r["parent_va"], 16) == want:
                print(json.dumps(r, indent=2))
                return
        print("no funclet parent at %s" % a.dump)
        return

    sel = [
        r
        for r in rows
        if r["funclets_unmatched"] >= a.min_unmatched
        and (a.include_vendor or not any(VENDOR_UNIT_RX.match(u) for u in r["kid_units"]))
    ]
    if a.json:
        Path(a.json).write_text(json.dumps(rows, indent=1))
        print("wrote %d rows -> %s\n" % (len(rows), a.json))

    print("## Ranked parent worklist (top %d of %d actionable)\n" % (a.top, len(sel)))
    print(
        "| # | parent VA | parent | unit | funclets (unmatched/pinned/total) | "
        "tgt frame | our frame | Δ | tgt sgpr | our sgpr | parent % | flags |"
    )
    print("|--:|---|---|---|---|--:|--:|--:|--:|--:|--:|---|")
    for i, r in enumerate(sel[: a.top], 1):
        nm = r["parent_name"]
        nm = nm if len(nm) <= 46 else nm[:43] + "..."
        unit = (r["parent_unit"] or (r["kid_units"][0] if r["kid_units"] else "—")) or "—"
        print(
            "| %d | `%s` | `%s` | %s | %d/%d/%d | %s | %s | %s | %s | %s | %s | %s |"
            % (
                i,
                r["parent_va"],
                nm,
                unit.replace("default/", ""),
                r["funclets_unmatched"],
                r["funclets_pinned"],
                r["funclets"],
                fmt_hex(r["tgt_frame"]),
                fmt_hex(r["base_frame"]),
                fmt_hex(r["frame_delta"]),
                r["tgt_savegpr"] if r["tgt_savegpr"] is not None else "—",
                r["base_savegpr"] if r["base_savegpr"] is not None else "—",
                "%.1f" % r["parent_match"] if r["parent_match"] is not None else "—",
                ",".join(r["flags"]) or "",
            )
        )

    off = [r for r in sel if "PARENT_OFFUNIT" in r["flags"] or "PARENT_UNPINNED" in r["flags"]]
    tot = sum(r["funclets_unmatched"] for r in off)
    print(
        "\n### Splits signal: %d parents live outside their funclets' pinned unit "
        "(%d unmatched funclets)\n" % (len(off), tot)
    )


if __name__ == "__main__":
    main()
