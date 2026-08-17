#!/usr/bin/env python3
"""vtable_claim_audit -- decide MECHANICALLY whether a `0x82......` address cited
in a source comment as "the retail vtable" actually IS a vtable.

WHY THIS EXISTS (lane W16-HEADERTUTH, 2026-08-17)
-------------------------------------------------
Lane W13-CHARINFO found three false "retail fact" header comments in one pass,
and **two of the three were the same mistake: a `.rdata` SWITCH JUMP TABLE
eyeballed as a vtable.**  A vtable claim is load-bearing -- one of them had been
hardened into a vtable *slot-ordering* decision -- so a wrong one is a false
premise that later lanes reason from, not a cosmetic defect.

That particular failure mode is the only one in the class that is **mechanically
decidable**, which is why it is worth a tool:

    a VTABLE's entries are FUNCTION STARTS, and it carries an `??_R4` COL.
    the CONFUSABLE TABLES' entries are INTERIOR BRANCH TARGETS
    WITHIN A SINGLE FUNCTION.

⛔ CORRECTION TO THE PREMISE THIS LANE WAS BRIEFED WITH
------------------------------------------------------
The brief said the confusable structure is a **switch jump table**.  On THIS
compiler it is not, and the difference is mechanical, not pedantic:
**MSVC X360 emits switch tables as COMPACT BYTE/SHORT *OFFSET* tables**, e.g.
`00 08 10 18 20 28 30 60 ...` at 0x82028e50 -- small integers, never an array
of code addresses.  Harvesting 208 real switch tables from the instruction
stream (`bctr` dispatch sites, see `harvest_switch_tables`) yields **0** with
any interior-pointer shape, which is the CORRECT answer rather than a detector
bug.  A switch table on this target simply cannot be mistaken for a vtable.

What IS confusable -- and what the two W13 falses actually are -- is the MSVC
**C++ EH IP-to-state map** (`FuncInfo::pIPtoStateMap`): an array of
`{ void *pc; int state; }` pairs whose `pc` fields are INTERIOR addresses in
one function.  Read one word out of phase it looks exactly like a table of
code pointers with small ints interleaved.  Both W13 falses have that shape.

TWO INDEPENDENT LEGS (deliberately not one)
-------------------------------------------
  (1) RTTI  -- under `/GR` (verified ON for retail: 2,220 `??_R4` Complete
      Object Locators, see CLAUDE.md) every vftable is preceded by a COL
      pointer at `vtable[-1]`.  `retail_rtti.class_of_vtable()` decodes it.
      This is a DECISIVE POSITIVE: a plausible COL naming a `.?A...` class is
      evidence no jump table can manufacture.
  (2) .pdata interiority -- `pdata_map_audit.Extents` gives retail's own
      authoritative function-extent table.  Words that are STRICTLY INSIDE one
      function are branch targets, not function pointers.  This is a DECISIVE
      NEGATIVE for "vtable".

The legs share no arithmetic and no input table: leg (1) reads `.rdata`/`.data`
RTTI structures, leg (2) reads the `.pdata` RUNTIME_FUNCTION array.  When they
agree, two unrelated instruments agree.

★ WHY "no COL" IS NOT BY ITSELF A REFUTATION
--------------------------------------------
`/GR` is on for Harmonix code but the binary also carries vendor/CRT objects,
and a non-polymorphic function-pointer table is not a vtable but is not a jump
table either.  So a missing COL yields UNDECIDED, never a false INTERIOR_TABLE.
Only leg (2) firing -- >= MIN_INTERIOR of the leading words interior to the
*same* owning function -- produces the INTERIOR_TABLE verdict.

★ THE `.pdata`-GAP TRAP (W13's own methodological point, preserved here)
------------------------------------------------------------------------
"no symbol at that address" is a WEAK test: the corpus has genuine multi-KB
holes, so absence usually just means "uncovered".  `Extents.interior_of()`
returns None for BOTH "is a function start" and "falls in a gap", so this
module never reads None as evidence.  Gap words are classed CODEGAP and are
explicitly excluded from the INTERIOR_TABLE quorum.

★ THE UNMAPPED BUCKET IS A DIFFERENT DEFECT
-------------------------------------------
An address in no PE section at all is not a mis-read structure -- it is almost
always a **stale TU0-era address** (main rebased to TU5 on 2026-07-15; every
TU0 address is invalid).  Reported separately as UNMAPPED so it is never
conflated with the jump-table mode this tool was built to size.

CONTROLS -- run `--selftest`; it must both FIRE and CLEAR
--------------------------------------------------------
  CLEAR: every vtable harvested by an independent full-`.rdata` COL sweep must
         verdict VTABLE, and NONE may verdict INTERIOR_TABLE.
  FIRE : every IP-to-state map harvested by an independent **EH-structure**
         oracle -- `FuncInfo` magic 0x19930522, then its OWN `pIPtoStateMap`
         pointer at +0x18 -- must verdict INTERIOR_TABLE, and NONE may verdict
         VTABLE.  That oracle never consults `.pdata`, so the control is not
         the detector re-run on itself.
  PREMISE: switch tables harvested from the INSTRUCTION STREAM must show ~0
         interior-pointer shape, mechanising the correction above.
  A one-sided control is a dead control: a detector that says INTERIOR_TABLE to
  everything passes FIRE and fails CLEAR, and vice-versa.  Both legs required.

Usage:
    python3 tools/vtable_claim_audit.py --selftest
    python3 tools/vtable_claim_audit.py addr 0x8211D4A4 [0x82026D3C ...]
    python3 tools/vtable_claim_audit.py sweep [--json out.json]   # scan src/**
"""
from __future__ import annotations

import argparse
import json
import os
import re
import struct
import sys
from dataclasses import dataclass, field
from typing import Optional

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from tools.pdata_map_audit import Extents, load_extents  # noqa: E402
from tools.retail_rtti import RetailRtti  # noqa: E402

ROOT = os.environ.get("RB3_ROOT", os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

# How many leading words to classify when judging a table's shape.
WINDOW = 8
# Quorum for the INTERIOR_TABLE verdict: this many of the leading words must be
# interior to the SAME owning function.  3 is deliberately > 2 so that a pair of
# coincidental interior words (e.g. two unrelated data constants that happen to
# look like code addresses) cannot carry the verdict on its own.
MIN_INTERIOR = 3

W_START, W_INTERIOR, W_CODEGAP, W_DATA, W_UNMAPPED = "START", "INTERIOR", "CODEGAP", "DATA", "UNMAPPED"


@dataclass
class Verdict:
    va: int
    verdict: str
    col_class: Optional[str] = None
    section: Optional[str] = None
    owner: Optional[int] = None          # owning fn when INTERIOR_TABLE
    n_interior: int = 0
    kinds: list = field(default_factory=list)
    words: list = field(default_factory=list)

    def shape(self) -> str:
        code = {W_START: "S", W_INTERIOR: "I", W_CODEGAP: "g", W_DATA: "d", W_UNMAPPED: "u", "?": "?"}
        return "".join(code[k] for k in self.kinds)


class Auditor:
    def __init__(self, rtti: Optional[RetailRtti] = None, ext: Optional[Extents] = None):
        self.R = rtti or RetailRtti()
        self.E = ext or Extents(load_extents())
        self._exec = self._exec_spans()

    def _exec_spans(self):
        """VA spans of sections that hold code.  Bounding on `.text` alone is
        wrong (see pdata_map_audit): BINK ships its own code section."""
        out = []
        for s in self.R.sections:
            if s.name in (".text", "BINK", "BINKCONS"):
                out.append((s.va, s.va + s.vsize))
        return out

    def _in_exec(self, a: int) -> bool:
        return any(lo <= a < hi for lo, hi in self._exec)

    def classify_word(self, w: Optional[int]) -> str:
        if w is None:
            return "?"
        if self.R.section_of(w) == "?":
            return W_UNMAPPED
        if not self._in_exec(w):
            return W_DATA
        if self.E.is_start(w):
            return W_START
        if self.E.interior_of(w) is not None:
            return W_INTERIOR
        return W_CODEGAP

    def audit(self, va: int, window: int = WINDOW) -> Verdict:
        sec = self.R.section_of(va)
        if sec == "?":
            return Verdict(va, "UNMAPPED", section=sec)

        words = [self.R.u32(va + 4 * i) for i in range(window)]
        kinds = [self.classify_word(w) for w in words]
        v = Verdict(va, "?", section=sec, kinds=kinds, words=words)

        # --- leg 2: interiority quorum (decisive NEGATIVE for "vtable") ------
        owners: dict[int, int] = {}
        for w, k in zip(words, kinds):
            if k == W_INTERIOR:
                o = self.E.interior_of(w)
                owners[o] = owners.get(o, 0) + 1
        if owners:
            owner, n = max(owners.items(), key=lambda kv: kv[1])
            v.owner, v.n_interior = owner, n

        # --- leg 1: RTTI COL (decisive POSITIVE) -----------------------------
        v.col_class = self.R.class_of_vtable(va)

        if v.col_class:
            v.verdict = "VTABLE"
        elif v.n_interior >= MIN_INTERIOR:
            v.verdict = "INTERIOR_TABLE"
        elif any(k in (W_DATA, W_UNMAPPED) for k in kinds[:4]):
            # leading words are not code at all -- whatever this is, a vtable
            # whose first slots are non-code is not a thing.
            v.verdict = "NOT_VTABLE"
        else:
            v.verdict = "UNDECIDED"
        return v


# ---------------------------------------------------------------------------
# INDEPENDENT ORACLES for the controls.  Neither reads the verdict logic.
# ---------------------------------------------------------------------------

def harvest_vtables(R: RetailRtti, limit: Optional[int] = None) -> list[int]:
    """Every VA in `.rdata` whose preceding word is a plausible COL.

    Independent of `.pdata` entirely: pure RTTI structure walk.
    """
    sec = next(s for s in R.sections if s.name == ".rdata")
    buf = open(R.path, "rb").read()
    out = []
    for off in range(sec.rawptr, sec.rawptr + min(sec.rawsize, sec.vsize), 4):
        va = sec.va + (off - sec.rawptr)
        (w,) = struct.unpack_from(">I", buf, off)
        if not (0x82000000 <= w < 0x83000000):
            continue
        c = R.decode_col(w)
        if c and c.signature == 0 and R.is_image_va(c.ptd) and R.is_image_va(c.pchd):
            n = R.td_name(c.ptd)
            if n and n.startswith(".?A"):
                out.append(va + 4)
                if limit and len(out) >= limit:
                    break
    return out


# PPC big-endian encodings used by the instruction-stream oracle.
_BCTR = 0x4E800420


def harvest_switch_tables(R: RetailRtti, limit: Optional[int] = None) -> list[int]:
    """Jump-table VAs recovered from MACHINE CODE, not from .pdata.

    MSVC/PPC switch dispatch materialises the table address with a
    `lis rX, hi` + (`addi`|`ori`) `rX, rX, lo` pair, indexes it, then
    `mtctr`/`bctr`.  We scan for `bctr`, walk back a bounded window, and
    reconstruct any lis/addi|ori pair on a common register that lands in
    `.rdata`.  Reading the instruction stream makes this oracle INDEPENDENT of
    the `.pdata` interiority test the verdict uses.
    """
    sec = next(s for s in R.sections if s.name == ".text")
    buf = open(R.path, "rb").read()
    n = min(sec.rawsize, sec.vsize) // 4
    words = struct.unpack_from(f">{n}I", buf, sec.rawptr)
    rdata = next(s for s in R.sections if s.name == ".rdata")
    out: list[int] = []
    seen: set[int] = set()

    for i, w in enumerate(words):
        if w != _BCTR:
            continue
        his: dict[int, int] = {}
        for j in range(max(0, i - 24), i):
            ins = words[j]
            op = ins >> 26
            if op == 15:  # addis / lis
                rd, ra = (ins >> 21) & 31, (ins >> 16) & 31
                if ra == 0:
                    his[rd] = (ins & 0xFFFF) << 16
            elif op == 14:  # addi
                rd, ra, imm = (ins >> 21) & 31, (ins >> 16) & 31, ins & 0xFFFF
                if ra in his:
                    if imm & 0x8000:
                        imm -= 0x10000
                    cand = (his[ra] + imm) & 0xFFFFFFFF
                    if rdata.va <= cand < rdata.va + rdata.vsize and cand not in seen:
                        seen.add(cand)
                        out.append(cand)
            elif op == 24:  # ori
                rs, rd, imm = (ins >> 21) & 31, (ins >> 16) & 31, ins & 0xFFFF
                if rs in his:
                    cand = his[rs] | imm
                    if rdata.va <= cand < rdata.va + rdata.vsize and cand not in seen:
                        seen.add(cand)
                        out.append(cand)
        if limit and len(out) >= limit:
            break
    return out


# ---------------------------------------------------------------------------
# Source-comment harvesting
# ---------------------------------------------------------------------------

ADDR_RE = re.compile(r"0x8[0-9a-fA-F]{7}")
# A claim counts as a VTABLE claim when the word vtable/vftable/vtbl appears in
# the same comment neighbourhood as the address.  A window is used because the
# claims wrap across lines constantly.
VT_RE = re.compile(r"\bv-?f?tables?\b|\bvtbl\b|\bvftable\b", re.I)


def harvest_claims(root: str, window: int = 2):
    """[(path, lineno, addr, line)] for every vtable-context 0x82 address."""
    claims = []
    src = os.path.join(root, "src")
    for dirpath, _dirs, files in os.walk(src):
        for fn in files:
            if not fn.endswith((".h", ".cpp", ".hpp", ".c")):
                continue
            p = os.path.join(dirpath, fn)
            try:
                lines = open(p, encoding="utf-8", errors="replace").read().splitlines()
            except OSError:
                continue
            for i, ln in enumerate(lines):
                addrs = ADDR_RE.findall(ln)
                if not addrs:
                    continue
                lo, hi = max(0, i - window), min(len(lines), i + window + 1)
                ctx = "\n".join(lines[lo:hi])
                if not VT_RE.search(ctx):
                    continue
                for a in addrs:
                    claims.append((os.path.relpath(p, root), i + 1, int(a, 16), ln.strip()))
    return claims


# ---------------------------------------------------------------------------


def harvest_ipstate_maps(R: RetailRtti, limit=None, min_entries: int = 1) -> list[int]:
    """`FuncInfo::pIPtoStateMap` VAs, read from the EH structures themselves.

    INDEPENDENT of `.pdata` and of RTTI: scans `.rdata` for the MSVC FuncInfo
    magic 0x19930522, then follows that record's OWN nIPMapEntries(+0x14) /
    pIPtoStateMap(+0x18) fields.  This is the FIRE oracle -- the structure the
    two W13 falses actually are.
    """
    sec = next(s for s in R.sections if s.name == ".rdata")
    buf = open(R.path, "rb").read()
    n = min(sec.rawsize, sec.vsize) // 4
    words = struct.unpack_from(f">{n}I", buf, sec.rawptr)
    out = []
    for i, w in enumerate(words):
        if w != 0x19930522 or i + 6 >= n:
            continue
        nip, pip = words[i + 5], words[i + 6]
        if nip >= min_entries and nip < 100000 and sec.va <= pip < sec.va + sec.vsize:
            out.append(pip)
            if limit and len(out) >= limit:
                break
    return out


def selftest(n_clear: int = 400, n_fire: int = 400) -> int:
    A = Auditor()
    ok = True

    vts = harvest_vtables(A.R, limit=n_clear)
    # >=3 entries: a map with 1-2 entries has FEWER THAN 3 interior pointers in
    # it at all, so it cannot reach MIN_INTERIOR by construction.  Measured step
    # function over 1,200 maps: n<=2 fires 0/563, n>=3 fires 637/637.  The
    # control therefore demands TOTALITY on the population the detector can
    # decide, and separately demands CONSERVATISM (never VTABLE) on the rest --
    # strictly stronger than a fractional bar, and not threshold-fitted.
    ips = harvest_ipstate_maps(A.R, limit=n_fire, min_entries=3)
    ips_short = harvest_ipstate_maps(A.R, limit=200, min_entries=1)
    ips_short = [a for a in ips_short if a not in set(ips)]
    sws = harvest_switch_tables(A.R, limit=250)
    print(f"oracles: {len(vts)} RTTI vtables | {len(ips)} EH IP-to-state maps "
          f"| {len(sws)} bctr switch tables")
    if len(vts) < 50 or len(ips) < 50 or len(sws) < 50:
        print("  [FAIL] an oracle returned too few items to be a control")
        return 1

    # --- CLEAR --------------------------------------------------------------
    vr = [A.audit(a) for a in vts]
    n_vt = sum(1 for v in vr if v.verdict == "VTABLE")
    bad = [v for v in vr if v.verdict == "INTERIOR_TABLE"]
    print(f"  [{'PASS' if n_vt == len(vr) else 'FAIL'}] CLEAR: {n_vt}/{len(vr)} RTTI vtables verdict VTABLE")
    ok &= n_vt == len(vr)
    print(f"  [{'PASS' if not bad else 'FAIL'}] CLEAR: 0 RTTI vtables misread as INTERIOR_TABLE (got {len(bad)})")
    ok &= not bad

    # --- FIRE ---------------------------------------------------------------
    ir = [A.audit(a) for a in ips]
    n_it = sum(1 for v in ir if v.verdict == "INTERIOR_TABLE")
    vfalse = [v for v in ir if v.verdict == "VTABLE"]
    print(f"  [{'PASS' if n_it == len(ir) else 'FAIL'}] FIRE: {n_it}/{len(ir)} EH maps (>=3 entries) verdict INTERIOR_TABLE")
    ok &= n_it == len(ir)
    shortv = [A.audit(a).verdict for a in ips_short]
    n_sv = sum(1 for v in shortv if v == "VTABLE")
    print(f"  [{'PASS' if n_sv == 0 else 'FAIL'}] FIRE(conservatism): 0/{len(shortv)} SHORT EH maps (<3 entries) claimed VTABLE")
    ok &= n_sv == 0
    print(f"  [{'PASS' if not vfalse else 'FAIL'}] FIRE: 0 EH maps misread as VTABLE (got {len(vfalse)})")
    ok &= not vfalse

    # --- PREMISE: switch tables are OFFSET tables on this compiler ----------
    sr = [A.audit(a) for a in sws]
    n_sit = sum(1 for v in sr if v.verdict == "INTERIOR_TABLE")
    print(f"  [{'PASS' if n_sit == 0 else 'FAIL'}] PREMISE: {n_sit}/{len(sr)} bctr switch tables have interior-pointer shape "
          f"(MSVC X360 emits OFFSET tables; the brief's 'switch jump table' framing is wrong)")
    ok &= n_sit == 0

    # --- separation ---------------------------------------------------------
    overlap = set(vts) & set(ips)
    print(f"  [{'PASS' if not overlap else 'FAIL'}] vtable / EH-map populations disjoint ({len(overlap)} shared)")
    ok &= not overlap

    print("  OK" if ok else "  FAILED")
    return 0 if ok else 1


def main(argv=None) -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", nargs="?", default="sweep", choices=["sweep", "addr"])
    ap.add_argument("addrs", nargs="*")
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--json")
    a = ap.parse_args(argv)

    if a.selftest:
        return selftest()

    A = Auditor()
    if a.cmd == "addr":
        for s in a.addrs:
            v = A.audit(int(s, 16))
            print(f"0x{v.va:08x}  {v.verdict:10s} sec={v.section:8s} shape={v.shape()} "
                  f"col={v.col_class} owner={'0x%08x' % v.owner if v.owner else None} nint={v.n_interior}")
        return 0

    claims = harvest_claims(ROOT)
    rows = []
    for path, ln, addr, text in claims:
        v = A.audit(addr)
        rows.append(dict(path=path, line=ln, addr=f"0x{addr:08x}", verdict=v.verdict,
                         col=v.col_class, shape=v.shape(),
                         owner=(f"0x{v.owner:08x}" if v.owner else None), text=text))
    counts: dict[str, int] = {}
    for r in rows:
        counts[r["verdict"]] = counts.get(r["verdict"], 0) + 1
    print(f"{len(rows)} vtable-context address claims across "
          f"{len({r['path'] for r in rows})} files")
    for k in sorted(counts, key=lambda x: -counts[x]):
        print(f"  {k:12s} {counts[k]}")
    for r in rows:
        if r["verdict"] != "VTABLE":
            print(f"  {r['verdict']:10s} {r['addr']} {r['path']}:{r['line']}  shape={r['shape']}")
    if a.json:
        json.dump(rows, open(a.json, "w"), indent=1)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
