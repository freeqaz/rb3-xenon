#!/usr/bin/env python3
"""eh_signature_match.py — identify unnamed retail functions by their C++ EH metadata.

WHY
---
`docs/plans/identification-discriminators-2026-07-25.md` (lane K) proved that every
probe computed *from a function's own machine bytes* is dead as a discriminator:
hit sets are byte-identical by construction, so `.pdata` prolog shape, call-graph
shape, frame size and friends cannot separate them.  The one channel it flagged as
unexplored is the **C++ EH metadata chain**, because it lives *outside* the
function body on both sides:

  retail  `.pdata` eh-flag -> {handler, handlerData} -> `_s_FuncInfo`
          -> unwind map / try-block map -> `HandlerType` -> `TypeDescriptor`
          -> RTTI class-name string
  ours    `__ehfuncinfo$<mangled>` / `__unwindtable$<mangled>` /
          `__tryblocktable$<mangled>` / `__catchsym$<mangled>$N` COMDAT symbols,
          whose relocations name `??_R0?AV<Class>@@@8` type descriptors

The joinable signature is therefore *structural* (derived from the source's
try/catch nesting and destructible-temporary states), not from the body's bytes:

    (maxState, nTryBlocks, total catches, sorted multiset of catch RTTI class
     names, per-tryblock (tryLow,tryHigh,catchHigh,nCatches) tuples,
     count of unwind entries with a non-null action)

optionally extended with the unwind map's `toState` vector (`--sig s2`) or with
`nIPMapEntries` (`--sig s3`).

TARGET POOL
-----------
`funclet_cascade_rank.py --calibrate` shows funclets behind a parent we have never
named match at 56.9%, versus 96.7% once the parent's frame + `__savegprlr_N` are
exact.  1,809 pinned funclet parents are unnamed and 2,052 unmatched funclets sit
behind them.  Naming a parent is an *identification* problem — this tool.

Every such parent is EH-flagged by construction (it has funclets), so the EH chain
always exists on the retail side.

USAGE
-----
    eh_signature_match.py --worktree WT --census
    eh_signature_match.py --worktree WT --validate [--sig s1|s2|s3]
    eh_signature_match.py --worktree WT --propose --out prop.json

`--propose` writes span_predictor.py's schema: ``{tu: [{name, va, ...}]}``.

MEASURED RESULT (2026-07-26, lane L, worktree ~/tmp/wt-laneL-eh @ 27,629)
------------------------------------------------------------------------
**The RTTI content channel does not exist in this binary.**  Decoded from both
sides: retail has 1,074 ``HandlerType`` entries across 9,145 exception-flagged
functions and **every single one** has ``adjectives == 0x40`` (catch-ellipsis)
and a NULL ``pType``; our own objects agree (3,542 of 3,548 are ``catch(...)``,
the 6 typed ones are STLport ``bad_alloc`` handlers retail does not contain).
Rock Band 3 is written entirely with ``catch (...)``, so there are **no catch
class-name strings to join on**.  Lane K's "one unexplored content channel" is
empty; do not rebuild it.

What is left is purely *structural*, and it is highly degenerate: 28,931 of our
compiled functions carry ``__ehfuncinfo$``, they produce only **79 distinct
signatures**, and the modal signature covers 18,031 of them.  Only 6.4 % are
signature-unique within their own unit.

Held-out precision (`--validate`, sig=s4, all in-unit candidates, truth hidden):

    all mapped EH functions       1729/1742 = 99.25 %   (refused 1612 TIE, 98 NO-MATCH)
      candidates 1               257/257   = 100.00 %   (degenerate: forced pick)
      candidates 2-4             399/400   =  99.75 %
      candidates 5-16            601/604   =  99.50 %
      candidates 17+             472/481   =  98.13 %
    restricted to bodies < 100 %   153/166 =  92.17 %   <- the production regime
      candidates 2-4              29/30    =  96.67 %
      candidates 5-16             52/55    =  94.55 %
      candidates 17+              52/61    =  85.25 %
    reloc-masked byte-identical  1564/1569 =  99.68 %   <- the only bankable tier

The all-mapped number is optimistic: an *unnamed* parent is unnamed precisely
because we have not got its body right, and in that regime the discriminator
falls to 92 %.  The misses are not template/ICF swarms, they are ordinary
distinct functions (``GetBandLogo`` vs ``SetDircuts``) that happen to share
``maxState=1, nTry=0``.  That is the discriminator being low-information, and no
cap fixes it.

Reach on the real pool (2,740 unnamed retail EH parents in pinned non-vendor
units): 176 unique resolutions at ``--max-cands 16``, 1,600 refused by the cap,
274 TIE, 519 NO-MATCH.  Of the 176, only **13** are reloc-masked byte-identical
at the derived VA.  A sibling lane measured that a parent's map entry has **no
causal effect on its funclets**, so those 13 are the entire bankable payoff:
applied, they converted **+13 strict, 0 LOST** (27,629 -> 27,642).  The other
163 are evidence-only, flip nothing, and carry a measured ~1-in-13 in-regime
error rate -- they were deliberately NOT applied.

VERDICT: do not fund this as a general identification discriminator.  Keep it as
the byte-identity-gated tie-breaker it turned out to be (`--bankable-only`), and
re-run it after waves that add named parents.
"""
from __future__ import annotations

import argparse
import json
import os
import re
import struct
import sys
from collections import Counter, defaultdict
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import funclet_cascade_rank as fcr  # noqa: E402  (PE/.pdata/_s_FuncInfo/COFF reuse)
from multi_content_disambiguate import Band, func_table  # noqa: E402
from caller_side_invert import masked_eq  # noqa: E402

VENDOR_UNIT_RX = fcr.VENDOR_UNIT_RX
EH_PFX = "__ehfuncinfo$"
UW_PFX = "__unwindtable$"
TB_PFX = "__tryblocktable$"
CS_PFX = "__catchsym$"
R0_RX = re.compile(r"^\?\?_R0(.+)@8$")


# --------------------------------------------------------------- retail EH chain
def cstr(pe: fcr.PE, va: int, limit: int = 512) -> str | None:
    o = pe.off(va)
    if o is None:
        return None
    e = pe.d.find(b"\0", o, o + limit)
    if e < 0:
        return None
    return pe.d[o:e].decode("latin1")


def retail_eh_sig(pe: fcr.PE, va: int) -> dict | None:
    """Decode the full `_s_FuncInfo` chain of an exception-flagged retail func."""
    hdata = pe.u32(va - 4)
    if not hdata:
        return None
    magic = pe.u32(hdata)
    if magic is None or (magic >> 8) != 0x199305:
        return None
    max_state = fcr.s32(pe.u32(hdata + 4))
    p_unwind = pe.u32(hdata + 8)
    n_try = pe.u32(hdata + 12) or 0
    p_try = pe.u32(hdata + 16)
    n_ip = pe.u32(hdata + 20) or 0
    if max_state is None or max_state < 0 or max_state > 4096 or n_try > 1024:
        return None

    states, n_action = [], 0
    if p_unwind and max_state:
        for i in range(max_state):
            ts = fcr.s32(pe.u32(p_unwind + i * 8))
            act = pe.u32(p_unwind + i * 8 + 4)
            if ts is None:
                return None
            states.append(ts)
            if act:
                n_action += 1

    tries, catches = [], []
    if p_try and n_try:
        for i in range(n_try):
            e = p_try + i * 20
            lo, hi, ch, nc = (fcr.s32(pe.u32(e + k * 4)) for k in range(4))
            p_ha = pe.u32(e + 16)
            if None in (lo, hi, ch, nc) or nc is None or nc < 0 or nc > 256:
                return None
            tries.append((lo, hi, ch, nc))
            if not p_ha:
                continue
            for j in range(nc):
                p_ty = pe.u32(p_ha + j * 16 + 4)
                nm = cstr(pe, p_ty + 8) if p_ty else "..."
                catches.append(nm if nm is not None else "?")
    return dict(max_state=max_state, n_try=n_try, tries=tries,
                catches=catches, n_action=n_action, n_ip=n_ip)


# ------------------------------------------------------------------ our COFF side
class ObjEH:
    """EH tables of one of our compiled objects, keyed by function mangled name.

    MSVC packs a function's whole EH group into a single COMDAT section:
    ``__unwindtable$F`` at 0, ``__catchsym$F$N``, ``__tryblocktable$F`` then
    ``__ehfuncinfo$F`` — so a symbol's extent is [its value, next symbol's value),
    NOT the section size.
    """

    def __init__(self, path: Path):
        self.c = fcr.Coff(path)
        by_sec = defaultdict(list)
        for name, (val, sec) in self.c.syms.items():
            if sec > 0:
                by_sec[sec].append((val, name))
        self.extent = {}
        for sec, lst in by_sec.items():
            lst.sort()
            size = self.c.secs[sec - 1]["size"]
            for i, (val, name) in enumerate(lst):
                nxt = size
                for v2, _n2 in lst[i + 1:]:
                    if v2 > val:
                        nxt = min(nxt, v2)
                        break
                self.extent[name] = (sec, val, nxt)

    def words(self, name):
        ent = self.extent.get(name)
        if not ent:
            return None, None
        sec, val, end = ent
        s = self.c.secs[sec - 1]
        n = max(0, (end - val) // 4)
        if n == 0:
            return None, None
        w = list(struct.unpack_from(">%dI" % n, self.c.d, s["ptr"] + val))
        rl = self.c.reloc_syms(sec, val, end)
        return w, {(a - val) // 4: nm for a, nm in rl.items()}

    def eh_functions(self):
        return [n[len(EH_PFX):] for n in self.c.syms if n.startswith(EH_PFX)]

    def sig(self, fn: str) -> dict | None:
        w, _r = self.words(EH_PFX + fn)
        if not w or len(w) < 6 or w[0] != 0x19930522:
            return None
        max_state = fcr.s32(w[1])
        n_try = w[3]
        n_ip = w[5]
        if max_state is None or max_state < 0 or max_state > 4096 or n_try > 1024:
            return None

        states, n_action = [], 0
        uw, uwr = self.words(UW_PFX + fn)
        if uw:
            for i in range(min(max_state, len(uw) // 2)):
                states.append(fcr.s32(uw[i * 2]))
                if uwr.get(i * 2 + 1) or uw[i * 2 + 1]:
                    n_action += 1

        tries, catches = [], []
        tb, tbr = self.words(TB_PFX + fn)
        if tb:
            for i in range(min(n_try, len(tb) // 5)):
                b = i * 5
                lo, hi, ch, nc = (fcr.s32(tb[b + k]) for k in range(4))
                tries.append((lo, hi, ch, nc))
                ha = tbr.get(b + 4)
                if not ha:
                    continue
                cw, cr = self.words(ha)
                if not cw:
                    continue
                for j in range(min(nc or 0, len(cw) // 4)):
                    ty = cr.get(j * 4 + 1)
                    if not ty:
                        catches.append("...")
                        continue
                    m = R0_RX.match(ty)
                    catches.append("." + m.group(1) if m else ty)
        return dict(max_state=max_state, n_try=n_try, tries=tries,
                    catches=catches, n_action=n_action, n_ip=n_ip)


# ------------------------------------------------------------------- the signature
def sigkey(s: dict, mode: str):
    base = (s["max_state"], s["n_try"], len(s["catches"]),
            tuple(sorted(s["catches"])), tuple(s["tries"]), s["n_action"])
    if mode == "s2":
        return base + (tuple(s.get("states") or ()),)
    if mode == "s3":
        return base + (s["n_ip"],)
    if mode == "s4":
        return base + (tuple(s.get("states") or ()), s["n_ip"])
    return base


def resolve(want, cands: dict):
    """cands: name -> sigkey.  RESOLVED only when exactly one candidate matches."""
    win = [n for n, k in cands.items() if k == want]
    if len(win) == 1:
        return "RESOLVED", win[0]
    if not win:
        return "NO-MATCH", None
    return "TIE", None


def bucket(n):
    return "1" if n <= 1 else "2-4" if n <= 4 else "5-16" if n <= 16 else "17+"


# ------------------------------------------------------------------------- context
class Ctx:
    def __init__(self, wt: Path, sig_mode: str):
        self.wt = wt
        self.sig_mode = sig_mode
        self.pe = fcr.PE(wt / "orig/45410914/band.exe")
        self.funcs = fcr.parse_pdata(self.pe)
        units = fcr.parse_splits(wt / "config/45410914/splits.txt")
        self.va2unit_cpp, _ = fcr.unit_function_vas(self.pe, units)
        self.match, self.srcmap = fcr.load_report(wt / "build/45410914/report.json")
        self.report_units = {u for (u, _n) in self.match}
        self.symmap = fcr.load_symbol_map(wt / "scripts/target_symbol_map.json")
        self.name2va = defaultdict(set)
        for k, v in self.symmap.items():
            if k.startswith("0x"):
                self.name2va[v].add(int(k, 16))
        self.objdir = wt / "build/45410914/src"
        self._obj = {}
        self._coff = {}
        self._frame = {}
        self._ft = {}

        self.va2unit = {}
        for va, cpp in self.va2unit_cpp.items():
            u = fcr.unit_report_key(cpp, self.report_units)
            if u:
                self.va2unit[va] = u

    def obj(self, unit) -> ObjEH | None:
        if unit in self._obj:
            return self._obj[unit]
        sp = self.srcmap.get(unit)
        o = None
        if sp:
            p = self.objdir / (sp[4:] if sp.startswith("src/") else sp)
            p = p.with_suffix(".obj")
            if p.exists():
                try:
                    o = ObjEH(p)
                except Exception:
                    o = None
        self._obj[unit] = o
        return o

    # ---- reloc-masked byte identity (the only thing that actually banks a flip;
    # measured by a sibling lane: a parent's map entry has NO causal effect on
    # its funclets, so the payoff of a naming wave is exactly the picks whose
    # compiled body is already byte-identical at the derived VA).
    def band(self):
        if not hasattr(self, "_band"):
            self._band = Band(str(self.wt / "orig/45410914/band.exe"))
        return self._band

    def ftab(self, unit):
        if unit in self._ft:
            return self._ft[unit]
        sp = self.srcmap.get(unit)
        t = {}
        if sp:
            p = self.objdir / (sp[4:] if sp.startswith("src/") else sp)
            p = p.with_suffix(".obj")
            if p.exists():
                try:
                    t = func_table(str(p))
                except Exception:
                    t = {}
        self._ft[unit] = t
        return t

    def byte_identical(self, unit, name, va):
        f = self.ftab(unit).get(name)
        if not f:
            return False
        return masked_eq(f["body"], self.band().text_bytes(va, f["size"]), f["offs"])

    def base_frame_of(self, unit, name):
        k = (unit, name)
        if k not in self._frame:
            self._frame[k] = fcr.base_frame(
                self.objdir, self.srcmap.get(unit), name, self._coff)[:2]
        return self._frame[k]

    def tgt_frame_of(self, va):
        if va not in self._frame:
            pw = self.pe.words(va, 24)
            self._frame[va] = (fcr.decode_frame(pw, va), fcr.find_savegpr(pw, va))
        return self._frame[va]

    def frame_agrees(self, unit, name, va, want_sgpr):
        bf, bsg = self.base_frame_of(unit, name)
        tf, tsg = self.tgt_frame_of(va)
        if bf is None or tf is None or bf != tf:
            return False
        return not want_sgpr or (bsg is not None and bsg == tsg)

    # retail EH functions grouped per pinned unit
    def retail_by_unit(self):
        out = defaultdict(list)
        for va, info in self.funcs.items():
            if not info["eh"]:
                continue
            u = self.va2unit.get(va)
            if not u or VENDOR_UNIT_RX.match(u):
                continue
            out[u].append(va)
        return out

    def base_sigs(self, unit):
        """{mangled name: (sigkey, sig)} for every EH function in our obj."""
        o = self.obj(unit)
        if o is None:
            return {}
        out = {}
        for fn in o.eh_functions():
            s = o.sig(fn)
            if s is None:
                continue
            st = None
            if self.sig_mode in ("s2", "s4"):
                uw, uwr = o.words(UW_PFX + fn)
                if uw:
                    st = tuple(fcr.s32(uw[i * 2])
                               for i in range(min(s["max_state"], len(uw) // 2)))
                s = dict(s, states=st)
            out[fn] = (sigkey(s, self.sig_mode), s)
        return out

    def retail_sig(self, va):
        s = retail_eh_sig(self.pe, va)
        if s is None:
            return None, None
        if self.sig_mode in ("s2", "s4"):
            hdata = self.pe.u32(va - 4)
            p_unwind = self.pe.u32(hdata + 8)
            st = ()
            if p_unwind and s["max_state"]:
                st = tuple(fcr.s32(self.pe.u32(p_unwind + i * 8))
                           for i in range(s["max_state"]))
            s = dict(s, states=st)
        return sigkey(s, self.sig_mode), s


# --------------------------------------------------------------------- subcommands
def cmd_census(ctx: Ctx):
    n_eh = sum(1 for i in ctx.funcs.values() if i["eh"])
    ok = bad = 0
    withtry = withcatchname = 0
    for va, info in ctx.funcs.items():
        if not info["eh"]:
            continue
        s = retail_eh_sig(ctx.pe, va)
        if s is None:
            bad += 1
            continue
        ok += 1
        if s["n_try"]:
            withtry += 1
        if any(c != "..." for c in s["catches"]):
            withcatchname += 1
    print("## Retail side")
    print("* `.pdata` functions            = %s" % f"{len(ctx.funcs):,}")
    print("* exception-flagged             = %s" % f"{n_eh:,}")
    print("* full `_s_FuncInfo` decoded    = %s" % f"{ok:,}")
    print("* undecodable                   = %s" % f"{bad:,}")
    print("* with >=1 try block            = %s" % f"{withtry:,}")
    print("* with >=1 NAMED catch type     = %s" % f"{withcatchname:,}")

    rbu = ctx.retail_by_unit()
    pinned = sum(len(v) for v in rbu.values())
    unnamed = sum(1 for u, vs in rbu.items() for v in vs
                  if fcr.report_name(ctx.symmap, v).startswith("fn_"))
    print("* in a pinned non-vendor unit   = %s (unnamed %s)"
          % (f"{pinned:,}", f"{unnamed:,}"))

    tot = uniq = 0
    persig = Counter()
    for u in sorted(rbu):
        bs = ctx.base_sigs(u)
        tot += len(bs)
        c = Counter(k for k, _ in bs.values())
        uniq += sum(1 for k, n in c.items() if n == 1)
        persig[u] = len(bs)
    print("\n## Our side")
    print("* compiled functions with `__ehfuncinfo$` in a pinned unit = %s"
          % f"{tot:,}")
    print("* of those, signature unique WITHIN their unit             = %s (%.1f%%)"
          % (f"{uniq:,}", 100.0 * uniq / tot if tot else 0))

    # degeneracy of the signature over the whole binary
    allc = Counter()
    for u in sorted(rbu):
        for k, _ in ctx.base_sigs(u).values():
            allc[k] += 1
    print("* distinct signatures (whole binary, ours)                 = %s"
          % f"{len(allc):,}")
    print("* most common signature covers                             = %s functions"
          % f"{allc.most_common(1)[0][1] if allc else 0:,}")


def _pool(ctx: Ctx, rbu):
    """UNNAMED retail EH VAs in pinned non-vendor units (the lane's pool)."""
    out = defaultdict(list)
    for u, vas in rbu.items():
        for va in vas:
            if fcr.report_name(ctx.symmap, va).startswith("fn_"):
                out[u].append(va)
    return out


def cmd_validate(ctx: Ctx, args):
    """Held-out: hide a KNOWN retail home, rebuild the production candidate set."""
    rbu = ctx.retail_by_unit()
    val = Counter()
    buck = defaultdict(Counter)
    misses = []
    for u in sorted(rbu):
        bs = ctx.base_sigs(u)
        if not bs:
            continue
        # production candidate set = our EH names in this unit that are UNMAPPED
        unmapped = {n: k for n, (k, _s) in bs.items() if n not in ctx.name2va}
        for va in rbu[u]:
            truth = ctx.symmap.get("0x%08x" % va)
            if truth is None or truth not in bs:
                continue
            # the truth must be uniquely homed, exactly like caller_side_invert
            if len(ctx.name2va.get(truth, ())) != 1:
                continue
            # distribution-shift control: production targets are functions whose
            # body we have NOT got right yet (that is why they are unnamed).  This
            # restricts the held-out set to the same regime.
            if args.truth_max_match is not None:
                mp = ctx.match.get((u, truth))
                if mp is None or mp > args.truth_max_match:
                    continue
            want, _s = ctx.retail_sig(va)
            if want is None:
                val["NO-RETAIL-SIG"] += 1
                continue
            cands = dict(unmapped)
            cands[truth] = bs[truth][0]          # hide the truth among unmapped
            nb = bucket(len(cands))
            verdict, pick = resolve(want, cands)
            if verdict == "RESOLVED" and args.require_frame and not ctx.frame_agrees(
                    u, pick, va, args.require_sgpr):
                verdict = "FRAME-DISAGREE"
            val[verdict] += 1
            if verdict != "RESOLVED":
                buck[nb][verdict] += 1
                continue
            ok = pick == truth
            val["HIT" if ok else "MISS"] += 1
            buck[nb]["HIT" if ok else "MISS"] += 1
            bi = ctx.byte_identical(u, pick, va)
            val["BYTES-%s/%s" % ("EQ" if bi else "NE", "HIT" if ok else "MISS")] += 1
            if not ok:
                misses.append((u, "0x%08X" % va, truth, pick, len(cands)))
    h, m = val["HIT"], val["MISS"]
    print("held-out validation (sig=%s):" % ctx.sig_mode,
          dict(sorted(val.items(), key=lambda kv: -kv[1])))
    print("  EH-SIGNATURE precision %d/%d = %s"
          % (h, h + m, "%.2f%%" % (100.0 * h / (h + m)) if h + m else "n/a"))
    print("  refused: TIE=%d NO-MATCH=%d" % (val["TIE"], val["NO-MATCH"]))
    for t in ("EQ", "NE"):
        hh, mm = val["BYTES-%s/HIT" % t], val["BYTES-%s/MISS" % t]
        if hh + mm:
            print("  reloc-masked bytes %s: %d/%d = %.2f%%%s"
                  % (t, hh, hh + mm, 100.0 * hh / (hh + mm),
                     "   <- bankable tier" if t == "EQ" else ""))
    print("\n  by candidate-set size:")
    print("  | candidates | hit | miss | precision | tie | no-match |")
    print("  |---|--:|--:|--:|--:|--:|")
    for b in ("1", "2-4", "5-16", "17+"):
        c = buck[b]
        hh, mm = c["HIT"], c["MISS"]
        print("  | %s | %d | %d | %s | %d | %d |"
              % (b, hh, mm, "%.2f%%" % (100.0 * hh / (hh + mm)) if hh + mm else "—",
                 c["TIE"], c["NO-MATCH"]))
    if misses and args.show_misses:
        print("\n  misses:")
        for u, va, t, p, n in misses[:args.show_misses]:
            print("   %-40s %s  truth=%s  got=%s  (%d cands)" % (u, va, t[:60], p[:60], n))
    return val


def cmd_propose(ctx: Ctx, args):
    rbu = ctx.retail_by_unit()
    pool = _pool(ctx, rbu)
    stats = Counter()
    out = defaultdict(list)
    frame_ok = frame_ne = frame_unk = 0
    for u in sorted(pool):
        bs = ctx.base_sigs(u)
        if not bs:
            stats["NO-OBJ"] += len(pool[u])
            continue
        unmapped = {n: k for n, (k, _s) in bs.items() if n not in ctx.name2va}
        if not unmapped:
            stats["NO-CANDIDATE"] += len(pool[u])
            continue
        for va in pool[u]:
            want, s = ctx.retail_sig(va)
            if want is None:
                stats["NO-RETAIL-SIG"] += 1
                continue
            nb = bucket(len(unmapped))
            if args.max_cands and len(unmapped) > args.max_cands:
                stats["OVER-CAP"] += 1
                continue
            verdict, pick = resolve(want, unmapped)
            if verdict == "RESOLVED" and args.require_frame and not ctx.frame_agrees(
                    u, pick, va, args.require_sgpr):
                verdict = "FRAME-DISAGREE"
            stats["%s/%s" % (verdict, nb)] += 1
            stats[verdict] += 1
            if verdict != "RESOLVED":
                continue
            bi = ctx.byte_identical(u, pick, va)
            stats["BYTES-EQ" if bi else "BYTES-NE"] += 1
            # corroboration only (never a gate): retail vs our frame + savegprlr
            pw = ctx.pe.words(va, 24)
            tf, tsg = fcr.decode_frame(pw, va), fcr.find_savegpr(pw, va)
            bf, bsg, _st = fcr.base_frame(ctx.objdir, ctx.srcmap.get(u), pick, {})
            if bf is None or tf is None:
                frame_unk += 1
            elif bf == tf:
                frame_ok += 1
            else:
                frame_ne += 1
            out[u.replace("default/", "")].append(dict(
                name=pick, va="0x%08x" % va, size=ctx.funcs[va]["size"],
                cls="UNIQUE", disambig="EH-SIGNATURE", bytes_eq=bi,
                n_candidates=len(unmapped),
                evidence=dict(max_state=s["max_state"], n_try=s["n_try"],
                              catches=s["catches"], tries=s["tries"],
                              n_action=s["n_action"], n_ip=s["n_ip"]),
                tgt_frame=tf, base_frame=bf, tgt_savegpr=tsg, base_savegpr=bsg,
            ))
    # global name-collision guard: one name may only be proposed once
    seen = Counter(r["name"] for recs in out.values() for r in recs)
    dropped = 0
    for u in list(out):
        keep = [r for r in out[u] if seen[r["name"]] == 1]
        dropped += len(out[u]) - len(keep)
        if keep:
            out[u] = keep
        else:
            del out[u]
    print("pool: %d unnamed retail EH parents in %d pinned units"
          % (sum(len(v) for v in pool.values()), len(pool)))
    print("verdicts:", dict(sorted(stats.items(), key=lambda kv: -kv[1])))
    print("proposals: %d across %d units (name-collision dropped %d)"
          % (sum(len(v) for v in out.values()), len(out), dropped))
    print("frame corroboration: equal=%d differ=%d unknown=%d"
          % (frame_ok, frame_ne, frame_unk))
    banked = {u: [r for r in recs if r["bytes_eq"]] for u, recs in out.items()}
    banked = {u: v for u, v in banked.items() if v}
    work = {u: [r for r in recs if not r["bytes_eq"]] for u, recs in out.items()}
    work = {u: v for u, v in work.items() if v}
    print("bankable (reloc-masked byte-identical at the derived VA): %d"
          % sum(len(v) for v in banked.values()))
    print("evidence-only (NOT byte-identical -> body-port worklist, do NOT apply): %d"
          % sum(len(v) for v in work.values()))
    if args.out:
        json.dump(banked if args.bankable_only else dict(out),
                  open(args.out, "w"), indent=1)
        wp = args.out.replace(".json", "_worklist.json")
        json.dump(work, open(wp, "w"), indent=1)
        print("->", args.out, "(worklist ->", wp + ")")


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    ap.add_argument("--worktree", required=True)
    ap.add_argument("--sig", default="s1", choices=("s1", "s2", "s3", "s4"))
    ap.add_argument("--census", action="store_true")
    ap.add_argument("--validate", action="store_true")
    ap.add_argument("--propose", action="store_true")
    ap.add_argument("--out")
    ap.add_argument("--max-cands", type=int, default=0,
                    help="refuse when the unit has more than N candidates (0=off)")
    ap.add_argument("--show-misses", type=int, default=0)
    ap.add_argument("--bankable-only", action="store_true",
                    help="--out keeps only the byte-identical (guaranteed-flip) tier")
    ap.add_argument("--require-frame", action="store_true",
                    help="post-filter: our compiled frame must equal retail's")
    ap.add_argument("--require-sgpr", action="store_true",
                    help="with --require-frame, also demand __savegprlr_N equality")
    ap.add_argument("--truth-max-match", type=float, default=None,
                    help="--validate only: keep held-out cases whose current "
                         "match%% is <= this, mirroring the production regime "
                         "(unnamed parents are unnamed because we got them wrong)")
    a = ap.parse_args()

    ctx = Ctx(Path(a.worktree), a.sig)
    if a.census:
        cmd_census(ctx)
    if a.validate:
        cmd_validate(ctx, a)
    if a.propose:
        cmd_propose(ctx, a)


if __name__ == "__main__":
    main()
