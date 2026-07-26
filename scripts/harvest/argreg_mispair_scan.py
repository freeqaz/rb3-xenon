#!/usr/bin/env python3
"""argreg_mispair_scan.py -- ABI argument-register evidence for symbol-map mispairs.

THE TEST
--------
Count the argument registers the retail TARGET body actually READS before
defining them.  Compare that against the argument count implied by the mapped
symbol's own signature.

  * A target that reads an argument register the symbol does not declare is a
    MAP MISPAIR ("forward" signal).  No codegen theory required -- the body
    simply cannot be that symbol.
  * A target that never touches argument registers the symbol *does* declare is
    a weaker "inverse" signal (unused parameters are legal), reported apart.

PowerPC / MSVC X360 ABI: integer & pointer arguments arrive in r3..r10 (r3 =
`this` for non-static member functions), float/double arguments in f1..f8 while
still consuming their GPR slot.  A read of an argument register that happens
before any write to it, on the fall-through path out of the entry block, is an
argument use.

INPUTS (no build required)
--------------------------
  build/45410914/report.json      unit/function table, match%, demangled names
  scripts/target_symbol_map.json  {VA: mangled_name}  (value may be a list)
  build/45410914/asm/**/*.s       dtk-split retail TARGET listings

OUTPUT
------
Ranked, evidence-carrying candidate list (text + optional JSON).  This tool
produces EVIDENCE ONLY.  It never edits target_symbol_map.json.

Usage:
  python3 scripts/harvest/argreg_mispair_scan.py                  # main scan
  python3 scripts/harvest/argreg_mispair_scan.py --fp-control     # FP rate on 100%
  python3 scripts/harvest/argreg_mispair_scan.py --self-test      # ground truth
  python3 scripts/harvest/argreg_mispair_scan.py --json out.json
"""

from __future__ import annotations

import argparse
import glob
import json
import os
import re
import sys
from collections import Counter, defaultdict

REPO = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
REPORT = os.path.join(REPO, "build/45410914/report.json")
SYMMAP = os.path.join(REPO, "scripts/target_symbol_map.json")
ASMDIR = os.path.join(REPO, "build/45410914/asm")

GPR_ARGS = [f"r{i}" for i in range(3, 11)]      # r3..r10
FPR_ARGS = [f"f{i}" for i in range(1, 9)]       # f1..f8
VOLATILE = set(f"r{i}" for i in range(3, 13)) | set(f"f{i}" for i in range(0, 14))
# Volatile registers that are NEVER argument registers under this ABI.  A
# read-before-def of one of these on the entry path means the listing does not
# start at a real function entry -- i.e. a dtk over-carve / rotated fragment,
# not a map mispair.  This is the one failure mode the strict-100 control
# population cannot exercise (a 100%-matching function is never mis-carved).
NEVER_ARG = {"r0", "r11", "r12"} | {f"f{i}" for i in range(9, 14)}

# ---------------------------------------------------------------------------
# 1.  Instruction decoding
# ---------------------------------------------------------------------------

REG_RE = re.compile(r"\b([rf])(\d+)\b")

# Stores: every register operand is a USE (the base-update forms also define the
# base, but the base is r1/rN and is read first anyway).
_STORE_PFX = ("stw", "stb", "sth", "std", "stf", "stv", "stm", "stq")
# Compares: cr operand is the def; the GPR/FPR operands are USEs.
_CMP = {"cmpw", "cmplw", "cmpwi", "cmplwi", "cmpd", "cmpld", "cmpdi", "cmpldi",
        "cmp", "cmpl", "cmpi", "cmpli", "fcmpu", "fcmpo"}
# move-to-SPR / CR: USEs only.
_MT = {"mtlr", "mtctr", "mtxer", "mtspr", "mtmsr", "mtmsrd", "mtcrf", "mtcr",
       "mtfsf", "mtfsb0", "mtfsb1", "mtvscr"}
# Read-modify-write on the destination operand.
_RMW = {"rlwimi", "rlwimi.", "rldimi", "rldimi.", "insrwi", "insrwi.",
        "insrdi", "insrdi.", "insslwi", "isel"}

CALL_MNEM = {"bl", "bla", "blrl", "bctrl", "bclrl", "bcctrl"}
UNCOND_MNEM = {"b", "ba", "blr", "bctr", "bclr", "bcctr", "rfid", "rfi"}


class UnknownInsn(Exception):
    pass


def decode(mnem: str, ops: str):
    """Return (defs, uses) as sets of canonical register names ('r4', 'f1')."""
    regs = [f"{k}{int(n)}" for k, n in REG_RE.findall(ops)]
    base = mnem.rstrip(".")

    if base in _CMP:
        return set(), set(regs)
    if base in _MT:
        return set(), set(regs)
    if base.startswith("tw") or base.startswith("td"):
        return set(), set(regs)
    if base.startswith(_STORE_PFX):
        return set(), set(regs)
    if base.startswith("dcb") or base.startswith("icb"):
        return set(), set(regs)
    if not regs:
        return set(), set()
    if base == "mr" and len(regs) == 2 and regs[0] == regs[1]:
        # `mr rN, rN` (or rN,rN,rN) is the Xbox 360 no-op / instrumentation
        # padding idiom.  It moves no data and must not count as a read.
        return set(), set()
    if base == "subfe" and len(regs) == 3 and regs[1] == regs[2]:
        # subfe rD,rA,rA == (CA - 1): a carry-materialisation idiom whose source
        # operand value is irrelevant.  Counting it as a read produced the only
        # forward false positives observed on the 100%-matching population.
        return {regs[0]}, set()
    if base in _RMW:
        return {regs[0]}, set(regs)
    # default: first operand is the destination, the rest are sources.
    return {regs[0]}, set(regs[1:])


# ---------------------------------------------------------------------------
# 2.  Target .s parsing
# ---------------------------------------------------------------------------

HDR_RE = re.compile(r"^#\s*\.text:0x[0-9A-Fa-f]+\s*\|\s*(0x[0-9A-Fa-f]+)\s*\|\s*size:\s*(0x[0-9A-Fa-f]+)")
FN_RE = re.compile(r"^\.fn\s+([A-Za-z0-9_$@?]+)\s*,")
INSN_RE = re.compile(r"^/\*\s*([0-9A-Fa-f]{8})\s+[0-9A-Fa-f]+\s+[0-9A-Fa-f ]+\*/\s*(.*)$")


class Fn:
    __slots__ = ("label", "hdr_va", "hdr_size", "insns", "path", "contiguous", "idx")

    def __init__(self, label, hdr_va, hdr_size, insns, path):
        self.label = label
        self.hdr_va = hdr_va
        self.hdr_size = hdr_size
        self.insns = insns          # list of (addr, mnem, ops)
        self.path = path
        self.idx = {a: i for i, (a, _, _) in enumerate(insns)}
        # A listing we trust: the first instruction IS the entry point and the
        # span exactly covers the declared size.  Anything else may be a
        # rotated / over-carved fragment and is not safe to reason about.
        self.contiguous = bool(
            insns and hdr_va is not None and hdr_size is not None
            and insns[0][0] == hdr_va
            and (insns[-1][0] - insns[0][0] + 4) == hdr_size
        )


_file_cache: dict[str, dict[str, Fn]] = {}


def parse_asm(path: str) -> dict[str, Fn]:
    cached = _file_cache.get(path)
    if cached is not None:
        return cached
    out: dict[str, Fn] = {}
    try:
        lines = open(path, "r", errors="replace").read().splitlines()
    except OSError:
        _file_cache[path] = out
        return out
    pending_hdr = None
    cur = None
    for line in lines:
        s = line.strip()
        m = HDR_RE.match(s)
        if m:
            pending_hdr = (int(m.group(1), 16), int(m.group(2), 16))
            continue
        m = FN_RE.match(s)
        if m:
            cur = [m.group(1), pending_hdr, []]
            pending_hdr = None
            continue
        if s.startswith(".endfn"):
            if cur:
                hv, hs = cur[1] if cur[1] else (None, None)
                out[cur[0]] = Fn(cur[0], hv, hs, cur[2], path)
            cur = None
            continue
        if cur is None:
            continue
        m = INSN_RE.match(line)
        if m:
            addr = int(m.group(1), 16)
            body = m.group(2).strip()
            if not body:
                continue
            parts = body.split(None, 1)
            mnem = parts[0]
            ops = parts[1] if len(parts) > 1 else ""
            cur[2].append((addr, mnem, ops))
    _file_cache[path] = out
    return out


_asm_index: dict[str, list[str]] | None = None


def asm_index() -> dict[str, list[str]]:
    """basename (no .s) -> list of full paths."""
    global _asm_index
    if _asm_index is None:
        _asm_index = defaultdict(list)
        for p in glob.glob(os.path.join(ASMDIR, "**", "*.s"), recursive=True):
            _asm_index[os.path.basename(p)[:-2]].append(p)
    return _asm_index


def find_fn(unit: str, va: int):
    """Locate the target listing for `va`, preferring the .s of `unit`."""
    labels = (f"fn_{va:08X}", f"fn_{va:08x}")
    base = unit.split("/")[-1]
    cands = list(asm_index().get(base, []))
    if not cands:
        return None
    # unit paths in report.json look like 'default/CharDriver' or
    # 'system/rndobj/Utl'; prefer a path whose tail matches the unit exactly.
    tail = unit.split("/", 1)[-1] if "/" in unit else unit
    cands.sort(key=lambda p: (0 if p.endswith(tail + ".s") else 1, len(p)))
    for p in cands:
        fns = parse_asm(p)
        for lab in labels:
            if lab in fns:
                return fns[lab]
    return None


# ---------------------------------------------------------------------------
# 3.  Signature -> declared argument registers
# ---------------------------------------------------------------------------

BASIC_1GPR = {
    "int", "unsigned int", "signed int", "char", "unsigned char", "signed char",
    "short", "unsigned short", "short int", "unsigned short int", "long",
    "unsigned long", "long int", "unsigned long int", "bool", "wchar_t",
    "__int64", "unsigned __int64", "signed __int64", "__int32", "unsigned __int32",
    "__int16", "unsigned __int16", "__int8", "unsigned __int8", "long long",
    "unsigned long long", "size_t", "unsigned", "signed",
}
FLOATY = {"float", "double", "long double"}
ACCESS = ("public: ", "protected: ", "private: ")


class Punt(Exception):
    def __init__(self, reason):
        super().__init__(reason)
        self.reason = reason


def _strip_cv(t: str) -> str:
    t = t.strip()
    for kw in ("const ", "volatile ", "__unaligned "):
        while t.startswith(kw):
            t = t[len(kw):].strip()
    for kw in (" const", " volatile", " __ptr64"):
        while t.endswith(kw):
            t = t[: -len(kw)].strip()
    return t


def split_top(s: str) -> list[str]:
    out, depth, cur = [], 0, []
    for ch in s:
        if ch in "(<[":
            depth += 1
        elif ch in ")>]":
            depth -= 1
            if depth < 0:
                raise Punt("paren_depth")
        if ch == "," and depth == 0:
            out.append("".join(cur))
            cur = []
        else:
            cur.append(ch)
    if depth != 0:
        raise Punt("paren_depth")
    out.append("".join(cur))
    return [x.strip() for x in out if x.strip() != ""]


def last_top_group(s: str):
    """Index range of the last depth-0 (...) group."""
    depth = 0
    start = None
    best = None
    for i, ch in enumerate(s):
        if ch == "(":
            if depth == 0:
                start = i
            depth += 1
        elif ch == ")":
            depth -= 1
            if depth == 0 and start is not None:
                best = (start, i)
            elif depth < 0:
                raise Punt("paren_depth")
        elif ch == "<":
            # angle brackets can hide parens in template args; only track them
            # when we are already inside no paren group.
            pass
    if best is None:
        raise Punt("no_param_list")
    return best


def param_kind(t: str) -> str:
    """'gpr' | 'fpr' | raises Punt for by-value aggregates / unknowns."""
    t = t.strip()
    if t.endswith("__ptr64"):
        t = t[: -len("__ptr64")].strip()
    if t == "...":
        raise Punt("variadic")
    if t.endswith("*") or t.endswith("&") or t.endswith("&&"):
        return "gpr"
    if "(" in t and "*" in t:            # function / member pointer
        return "gpr"
    if "[" in t:                          # array decays to pointer
        return "gpr"
    b = _strip_cv(t)
    if b in FLOATY:
        return "fpr"
    if b in BASIC_1GPR:
        return "gpr"
    if b.startswith("enum "):
        return "gpr"
    if b.startswith(("class ", "struct ", "union ")):
        raise Punt("byval_aggregate")
    raise Punt("unknown_param:" + b[:40])


def parse_signature(demangled: str, mangled: str):
    """-> dict(has_this, gprs=[reg...], fprs=[...], reserved=[...], nparams)."""
    d = demangled.strip()
    if d.startswith("[thunk]"):
        raise Punt("thunk")
    if "`vftable'" in d or "`vbtable'" in d or "`RTTI" in d:
        raise Punt("data_symbol")

    access = None
    for a in ACCESS:
        if d.startswith(a):
            access = a
            d = d[len(a):].strip()
            break
    is_static = False
    is_virtual = False
    while True:
        if d.startswith("static "):
            is_static = True
            d = d[7:].strip()
        elif d.startswith("virtual "):
            is_virtual = True
            d = d[8:].strip()
        else:
            break

    has_this = bool(access) and not is_static

    # NOTE: we deliberately do NOT re-derive this-ness from the mangled
    # access/storage letter.  Locating that letter requires knowing where the
    # qualified-name section ends, which template arguments and reference
    # parameters make ambiguous (`@@AAV...` inside a parameter list looks
    # exactly like "private, non-cv, virtual").  undname's access specifier is
    # authoritative and is what we use.  Vcall/adjustor thunks are punted below
    # by their mangling prefix instead.
    if mangled.startswith("??_9") or mangled.startswith("??_7") or \
            mangled.startswith("??_8") or mangled.startswith("??_R"):
        raise Punt("thunk_or_data")

    # return type: everything before the first depth-0 calling convention token
    cc = None
    for base in ("__cdecl", "__stdcall", "__fastcall", "__thiscall", "__clrcall"):
        tok = " " + base + " "
        i = d.find(tok)
        if i < 0 and d.startswith(base + " "):
            # ctors/dtors have no return type: the convention starts at 0
            i, tok = 0, base + " "
        if i >= 0 and (cc is None or i < cc[0]):
            cc = (i, tok)
    if cc is None:
        raise Punt("no_callconv")
    rettype = d[: cc[0]].strip()
    rest = d[cc[0] + len(cc[1]):].strip()

    # A by-value aggregate return may add a hidden sret pointer argument, and we
    # do not know whether MSVC X360 places it before or after `this`.  Rather
    # than punt we reserve one extra leading slot: that only WIDENS the declared
    # register set, which is the safe direction for the forward signal.  The
    # inverse signal is disabled for these.
    sret = bool(rettype) and not rettype.endswith(("*", "&")) and _strip_cv(
        rettype).startswith(("class ", "struct ", "union "))

    a, b = last_top_group(rest)
    trailing = rest[b + 1:].strip()
    for kw in ("const", "volatile", "&", "&&", "__ptr64"):
        trailing = trailing.replace(kw, "")
    if trailing.strip():
        raise Punt("trailing_after_params")
    inner = rest[a + 1: b]

    params = split_top(inner)
    if len(params) == 1 and params[0] == "void":
        params = []

    gprs, fprs, reserved = [], [], []
    slot = 0
    if has_this:
        gprs.append("r3")
        slot = 1
    if sret:
        reserved.append(f"r{3 + slot}")
        slot += 1
    fi = 1
    for p in params:
        kind = param_kind(p)
        if slot < 8:
            reg = f"r{3 + slot}"
        else:
            reg = None          # spilled to the parameter save area
        if kind == "fpr":
            if fi <= 8:
                fprs.append(f"f{fi}")
            fi += 1
            if reg:
                reserved.append(reg)
        else:
            if reg:
                gprs.append(reg)
        slot += 1

    # MSVC gives constructors/destructors of classes with virtual bases a hidden
    # trailing "construct the virtual bases" int flag that never appears in the
    # demangled signature.  Reserve one extra trailing slot for ctors/dtors: it
    # only widens the declared set (safe for the forward signal) and it removed
    # a whole family of forward false positives on the 100% population.
    ctordtor = bool(re.match(r"\?\?(0|1|_D|_G|_E|_F)", mangled))
    if ctordtor and slot < 8:
        reserved.append(f"r{3 + slot}")
        slot += 1

    return {
        "has_this": has_this,
        "is_virtual": is_virtual,
        "sret": sret,
        "ctordtor": ctordtor,
        "gprs": gprs,
        "fprs": fprs,
        "reserved": reserved,
        "nparams": len(params),
        "slots": slot,
        "overflow": slot > 8,
    }


# ---------------------------------------------------------------------------
# 4.  Entry-block argument-register read analysis
# ---------------------------------------------------------------------------

LBL_RE = re.compile(r"\.L_([0-9A-Fa-f]{8})\b")


def branch_target(fn: Fn, ops: str):
    """Index of the in-function target of a branch, or None."""
    m = LBL_RE.search(ops)
    if not m:
        return None
    return fn.idx.get(int(m.group(1), 16))


def is_tail_call(fn: Fn) -> bool:
    """Does the body forward control (and therefore its argument registers) to
    another function without redefining them?  `b fn_x` / `bctr` / `b __rest*`."""
    for _, mnem, ops in fn.insns:
        base = mnem.rstrip(".")
        if base in ("bctr", "bcctr"):
            return True
        if base in ("b", "ba") and branch_target(fn, ops) is None:
            # branch out of the function that is not a stack-restore thunk
            if "__restgpr" in ops or "__restfpr" in ops or "__savegpr" in ops:
                continue
            return True
    return False


def entry_arg_reads(fn: Fn):
    """Walk ONE concrete path out of the entry point: fall through conditional
    branches, follow unconditional branches, stop at the first call.  Every
    read-before-def seen on this path is a genuine argument use, which is what
    makes the forward signal safe."""
    defined = set()
    reads = []
    evidence = {}
    scratch = []
    i = 0
    seen = set()
    n = 0
    while 0 <= i < len(fn.insns) and i not in seen:
        seen.add(i)
        _, mnem, ops = fn.insns[i]
        base = mnem.rstrip(".")
        if base in CALL_MNEM:
            break
        if base in ("blr", "bclr", "rfi", "rfid"):
            break
        defs, uses = decode(mnem, ops)
        for u in sorted(uses):
            if u in defined:
                continue
            if (u in GPR_ARGS or u in FPR_ARGS) and u not in evidence:
                reads.append(u)
                evidence[u] = f"{mnem} {ops}".strip()
            elif u in NEVER_ARG and u not in scratch:
                scratch.append(f"{u}<-[{mnem} {ops}]".strip())
        defined |= defs
        n += 1
        if base in ("b", "ba", "bctr", "bcctr"):
            t = branch_target(fn, ops)
            if t is None:
                break
            i = t
        else:
            i += 1
    return reads, evidence, n, scratch


def cfg_arg_reads(fn: Fn):
    """Over-approximate the set of argument registers read anywhere on any path
    from entry.  Used only to SUPPRESS the inverse signal, so over-approximating
    reads is the safe direction."""
    n = len(fn.insns)
    state = [None] * n            # defined-set on entry to each instruction
    reads = set()
    work = [(0, frozenset())]
    while work:
        i, dfn = work.pop()
        if not (0 <= i < n):
            continue
        cur = state[i]
        if cur is not None:
            merged = cur & dfn    # a reg counts as defined only on ALL paths
            if merged == cur:
                continue
            dfn = merged
        state[i] = dfn
        _, mnem, ops = fn.insns[i]
        base = mnem.rstrip(".")
        defs, uses = decode(mnem, ops)
        for u in uses:
            if u not in dfn and (u in GPR_ARGS or u in FPR_ARGS):
                reads.add(u)
        nd = set(dfn) | defs
        if base in CALL_MNEM:
            nd |= VOLATILE                      # calls clobber the volatiles
            work.append((i + 1, frozenset(nd)))
            continue
        if base in ("blr", "bclr", "rfi", "rfid"):
            continue
        if base in ("b", "ba", "bctr", "bcctr"):
            t = branch_target(fn, ops)
            if t is not None:
                work.append((t, frozenset(nd)))
            continue
        if base.startswith("b"):                # conditional
            t = branch_target(fn, ops)
            if t is not None:
                work.append((t, frozenset(nd)))
            work.append((i + 1, frozenset(nd)))
            continue
        work.append((i + 1, frozenset(nd)))
    return reads


def all_arg_touch(fn: Fn) -> bool:
    """Does any instruction anywhere in the body read an argument register?"""
    for _, mnem, ops in fn.insns:
        base = mnem.rstrip(".")
        if base in CALL_MNEM or base in UNCOND_MNEM:
            continue
        _, uses = decode(mnem, ops)
        if uses & (set(GPR_ARGS) | set(FPR_ARGS)):
            return True
    return False


def is_coverage_stub(fn: Fn) -> bool:
    """Retail coverage-breadcrumb stub: tiny, callless, never reads an argument
    register, and writes a global flag.  ~17.7k of these exist binary-wide;
    they are genuinely stubbed, not mispaired."""
    if len(fn.insns) == 0 or len(fn.insns) > 16:
        return False
    mnems = [m.rstrip(".") for _, m, _ in fn.insns]
    if any(m in CALL_MNEM for m in mnems):
        return False
    if all_arg_touch(fn):
        return False
    if not any(m == "blr" for m in mnems):
        return False
    # breadcrumb signature: address a global (lis) and store to it
    has_lis = any(m in ("lis", "addis") for m in mnems)
    has_store = any(m.startswith(("stw", "stb", "sth")) for m in mnems)
    return has_lis and has_store


def summarize(fn: Fn, limit: int = 7) -> str:
    """One-line human description of the target body."""
    out = []
    for _, mnem, ops in fn.insns:
        base = mnem.rstrip(".")
        if base in ("mflr", "mfspr") or (base in ("stw", "std", "stfd") and "(r1)" in ops) \
                or base == "stwu":
            continue
        out.append(f"{mnem} {ops}".strip())
        if len(out) >= limit:
            out.append("...")
            break
    return "; ".join(out)


# ---------------------------------------------------------------------------
# 5.  Scan driver
# ---------------------------------------------------------------------------

def load_pool(include_zero: bool, want_100: bool):
    report = json.load(open(REPORT))
    raw = json.load(open(SYMMAP))
    sym2va = defaultdict(list)
    for k, v in raw.items():
        if not k.lower().startswith("0x"):
            continue                      # '_comment' and friends
        for name in (v if isinstance(v, list) else [v]):
            if isinstance(name, str):
                sym2va[name].append(int(k, 16))

    pool = []
    for u in report["units"]:
        uname = u["name"]
        for f in u.get("functions") or []:
            name = f["name"]
            vas = sym2va.get(name)
            if not vas:
                continue
            dm = (f.get("metadata") or {}).get("demangled_name")
            if not dm:
                continue
            pct = f.get("match_percent_normalized")
            pct = 0.0 if pct is None else float(pct)
            if want_100:
                if pct < 100.0:
                    continue
            else:
                if pct >= 100.0:
                    continue
                if pct == 0.0 and not include_zero:
                    continue
            pool.append({
                "unit": uname, "sym": name, "dm": dm, "pct": pct,
                "size": int(f.get("size") or 0), "vas": vas,
            })
    return pool


def analyze(entry, min_inverse_gap=2):
    """-> record dict with 'verdict' in
       MISPAIR_FORWARD / INVERSE_WEAK / STUB / OK / PUNT / NOASM"""
    rec = dict(entry)
    rec["verdict"] = "PUNT"
    rec["reason"] = ""
    try:
        sig = parse_signature(entry["dm"], entry["sym"])
    except Punt as p:
        rec["reason"] = "sig:" + p.reason
        return rec
    except Exception as e:                                    # pragma: no cover
        rec["reason"] = "sig:exc:" + type(e).__name__
        return rec

    rec["declared_gprs"] = sig["gprs"]
    rec["declared_fprs"] = sig["fprs"]
    rec["nparams"] = sig["nparams"]
    rec["has_this"] = sig["has_this"]

    fn = None
    for va in entry["vas"]:
        fn = find_fn(entry["unit"], va)
        if fn is not None:
            rec["va"] = va
            break
    if fn is None:
        rec["verdict"] = "NOASM"
        rec["reason"] = "target listing not found"
        return rec
    rec["multi_va"] = len(entry["vas"]) > 1

    if not fn.contiguous:
        rec["reason"] = "asm:noncontiguous_or_rotated"
        return rec

    if is_coverage_stub(fn):
        rec["verdict"] = "STUB"
        rec["body"] = summarize(fn)
        return rec

    reads, evidence, nwalk, scratch = entry_arg_reads(fn)
    if scratch:
        rec["verdict"] = "FRAGMENT"
        rec["reason"] = "entry reads non-arg scratch: " + "; ".join(scratch[:3])
        rec["reads"] = reads
        rec["body"] = summarize(fn)
        rec["va"] = rec.get("va")
        return rec
    rec["reads"] = reads
    rec["evidence"] = evidence
    rec["body"] = summarize(fn)
    rec["n_entry_insns"] = nwalk

    declared_gpr = set(sig["gprs"]) | set(sig["reserved"])
    declared_fpr = set(sig["fprs"])
    max_slot = 3 + sig["slots"] - 1        # highest GPR index consumed
    beyond = []
    for r in reads:
        if r[0] == "r":
            if int(r[1:]) > max_slot:
                beyond.append(r)
        else:
            if r not in declared_fpr:
                beyond.append(r)
    rec["beyond"] = beyond

    if beyond and not sig["overflow"]:
        gap = max(int(r[1:]) for r in beyond if r[0] == "r") - max_slot if any(
            r[0] == "r" for r in beyond) else 1
        rec["verdict"] = "MISPAIR_FORWARD"
        rec["gap"] = gap
        rec["confidence"] = "high" if (gap >= 2 or len(beyond) >= 2) else "medium"
        return rec

    # ---- inverse signal --------------------------------------------------
    # Uses a full-CFG over-approximation of the reads (suppression direction)
    # plus two structural suppressions:
    #   * tail calls / vtable forwarders pass their arguments through untouched
    #   * an sret'd signature has an unknown hidden-argument position
    allreads = cfg_arg_reads(fn)
    rec["cfg_reads"] = sorted(allreads, key=lambda r: (r[0], int(r[1:])))
    unread = [r for r in sig["gprs"] if r not in allreads]
    unread += [r for r in sig["fprs"] if r not in allreads]
    rec["unread"] = unread
    gpr_unread = [r for r in sig["gprs"] if r not in allreads]
    tail = is_tail_call(fn)
    rec["tail_call"] = tail
    if (len(unread) >= min_inverse_gap and sig["gprs"] and "r3" in allreads
            and not sig["overflow"] and not sig["sret"] and not tail
            and len(fn.insns) >= 6
            and gpr_unread == sig["gprs"][len(sig["gprs"]) - len(gpr_unread):]):
        rec["verdict"] = "INVERSE_WEAK"
        # Tiering measured against the strict-100 control population:
        #  high   -- free function (no `this`): a non-member that ignores every
        #            argument but the first is almost always the wrong body.
        #  medium -- member, not a ctor/dtor, >=3 ignored argument registers.
        #  low    -- everything else.  Dominated by two legitimate families:
        #            retail-stripped MILO_DEBUG file/line arguments, and base
        #            constructors that receive their arguments by pass-through.
        if not sig["has_this"]:
            rec["confidence"] = "high"
        elif not sig["ctordtor"] and len(unread) >= 3:
            rec["confidence"] = "medium"
        else:
            rec["confidence"] = "low"
        return rec

    rec["verdict"] = "OK"
    return rec


# ---------------------------------------------------------------------------
# 6.  Reporting
# ---------------------------------------------------------------------------

def run(pool, label, min_inverse_gap=2):
    recs = [analyze(e, min_inverse_gap) for e in pool]
    counts = Counter(r["verdict"] for r in recs)
    print(f"=== {label}: {len(pool)} functions ===")
    for k, v in counts.most_common():
        print(f"  {k:18s} {v:6d}   ({100.0*v/max(1,len(pool)):5.2f}%)")
    punts = Counter(r["reason"].split(":", 2)[1] if r["verdict"] == "PUNT" and ":" in r["reason"]
                    else r["reason"] for r in recs if r["verdict"] == "PUNT")
    if punts:
        print("  punt breakdown:")
        for k, v in punts.most_common(12):
            print(f"      {k:34s} {v:6d}")
    return recs


def fmt(r, i=None):
    pre = f"{i:3d}. " if i is not None else ""
    va = r.get("va")
    vas = f"0x{va:08x}" if isinstance(va, int) else "?"
    return (f"{pre}{r['sym'][:96]}\n"
            f"     unit={r['unit']}  va={vas}  pct={r['pct']:.3f}  size={r['size']}"
            f"  verdict={r['verdict']} {r.get('reason','')}\n"
            f"     decl: this={r.get('has_this')} nparams={r.get('nparams')} "
            f"gprs={r.get('declared_gprs')} fprs={r.get('declared_fprs')}\n"
            f"     read: {r.get('reads')}   beyond={r.get('beyond')} "
            f"unread={r.get('unread')} conf={r.get('confidence','')}\n"
            f"     evid: " + "; ".join(f"{k}<-[{v}]" for k, v in (r.get('evidence') or {}).items()
                                       if k in (r.get('beyond') or [])) + "\n"
            f"     body: {r.get('body','')[:180]}")


def family_key(demangled: str) -> str:
    """Collapse a demangled name to Class::Method with template args dropped."""
    s = demangled
    i = s.find("(")
    if i > 0:
        s = s[:i]
    out, depth = [], 0
    for ch in s:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth = max(0, depth - 1)
        elif depth == 0:
            out.append(ch)
    s = "".join(out)
    s = s.split(" ")[-1]
    return s


SELF_TEST = [
    "?SetObj@?$ObjRefConcrete@VCharClip@@VObjectDir@@@@QAAPAVObject@Hmx@@PAV23@@Z",
    "??$__uninitialized_copy@PAVMoveFrame@@PAV1@@stlpmtx_std@@YAPAVMoveFrame@@PAV1@00ABU__false_type@0@@Z",
    "??$__uninitialized_copy@PAVRangeShift@GemTrack@@PAV12@@stlpmtx_std@@YAPAVRangeShift@GemTrack@@PAV12@00ABU__false_type@0@@Z",
]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--fp-control", action="store_true",
                    help="run over the strict-100.0 population and report the FP rate")
    ap.add_argument("--self-test", action="store_true", help="check known ground truth")
    ap.add_argument("--include-zero", action="store_true",
                    help="also scan the 0%% (unpaired-body) sub-pool")
    ap.add_argument("--top", type=int, default=30)
    ap.add_argument("--json", default=None)
    ap.add_argument("--min-inverse-gap", type=int, default=2)
    args = ap.parse_args()

    if args.self_test:
        pool = load_pool(include_zero=True, want_100=False)
        by = {e["sym"]: e for e in pool}
        ok = True
        for s in SELF_TEST:
            e = by.get(s)
            if e is None:
                print(f"MISSING FROM POOL: {s}")
                ok = False
                continue
            r = analyze(e, args.min_inverse_gap)
            flagged = r["verdict"] in ("MISPAIR_FORWARD", "INVERSE_WEAK")
            print(f"[{'PASS' if flagged else 'FAIL'}] {r['verdict']}")
            print(fmt(r))
            print()
            ok = ok and flagged
        print("SELF-TEST", "PASS" if ok else "FAIL")
        return 0 if ok else 1

    if args.fp_control:
        pool = load_pool(include_zero=False, want_100=True)
        recs = run(pool, "FP CONTROL (strict 100.0)", args.min_inverse_gap)
        fwd = [r for r in recs if r["verdict"] == "MISPAIR_FORWARD"]
        inv = [r for r in recs if r["verdict"] == "INVERSE_WEAK"]
        judged = [r for r in recs if r["verdict"] in
                  ("MISPAIR_FORWARD", "INVERSE_WEAK", "OK", "STUB")]
        print()
        print(f"FALSE POSITIVE RATE (forward) : {len(fwd)}/{len(judged)} = "
              f"{100.0*len(fwd)/max(1,len(judged)):.4f}%")
        print(f"FALSE POSITIVE RATE (inverse) : {len(inv)}/{len(judged)} = "
              f"{100.0*len(inv)/max(1,len(judged)):.4f}%")
        for tier in ("high", "medium", "low"):
            k = sum(1 for r in inv if r.get("confidence") == tier)
            print(f"   inverse[{tier:6s}]           : {k}/{len(judged)} = "
                  f"{100.0*k/max(1,len(judged)):.4f}%")
        for r in fwd[:20]:
            print()
            print(fmt(r))
        if args.json:
            json.dump([r for r in recs if r["verdict"] in ("MISPAIR_FORWARD", "INVERSE_WEAK")],
                      open(args.json, "w"), indent=1, default=str)
        return 0

    pool = load_pool(include_zero=args.include_zero, want_100=False)
    recs = run(pool, "SUB-100 NAMED PAIRED POOL", args.min_inverse_gap)
    fwd = sorted([r for r in recs if r["verdict"] == "MISPAIR_FORWARD"],
                 key=lambda r: (-r.get("gap", 0), -len(r.get("beyond") or []), -r["size"]))
    inv = sorted([r for r in recs if r["verdict"] == "INVERSE_WEAK"],
                 key=lambda r: (-len(r.get("unread") or []), -r["size"]))
    print(f"\n--- TOP FORWARD (hard mispair evidence), {len(fwd)} total ---")
    for i, r in enumerate(fwd[: args.top], 1):
        print(fmt(r, i))
    print(f"\n--- TOP INVERSE (weak), {len(inv)} total ---")
    for i, r in enumerate(inv[: args.top], 1):
        print(fmt(r, i))

    for lbl, group in (("FORWARD", fwd), ("INVERSE", inv)):
        fams = Counter()
        for r in group:
            fams[family_key(r["dm"])] += 1
        print(f"\n--- {lbl} families (>1 member) ---")
        for k, v in fams.most_common(40):
            if v > 1:
                print(f"  {v:4d}  {k}")

    if args.json:
        json.dump({"forward": fwd, "inverse": inv}, open(args.json, "w"),
                  indent=1, default=str)
        print(f"\nwrote {args.json}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
