#!/usr/bin/env python3
"""Read a retail factory-registration list AS STRINGS, straight out of band.exe.

Provenance: lane REGORDER-1 (2026-08-14), generalising the ad-hoc channel lane
WRONGCALL-2 used to land the ``Synth360::Init`` / ``Rnd::PreInit`` order fixes.

WHY THIS EXISTS -- THE MAP IS NOT AN ADMISSIBLE WITNESS HERE
------------------------------------------------------------
``REGISTER_OBJ_FACTORY(X)`` expands to
``Hmx::Object::RegisterFactory(X::StaticClassName(), X::NewObject)``, so a
registration list is a run of triples in ``.text``.  Reading which class each
slot registers *out of the dtk target obj's symbol names* routes the answer
through ``scripts/target_symbol_map.json`` -- and a lane that called two
map-derived channels "independent" paid -1,248 B for it.

This module bottoms out one level lower.  For each slot it resolves the
``StaticClassName`` callee's ADDRESS, decodes that function's guarded
function-local ``static Symbol``, and reads the C string it is built from out of
``.rdata``.  The result is a list of literals that exist in the shipped binary
whether or not any symbol was ever named.

⚠ THE CALLER MUST ANCHOR ON ``RegisterFactory`` SITES, NOT ON CALLEE NAMES.
An earlier version of the census keyed on ``?StaticClassName@X@@`` *names* in the
target obj and therefore silently DROPPED every slot the map has not named.  It
reported "ours registers 2 extra" for ``Rnd::PreInit`` and 1 for ``BandInit``;
counting ``RegisterFactory`` call sites instead gave 43 vs 43 and 25 vs 25, i.e.
all three were FALSE and acting on them would have deleted correct code.
``RegisterFactory`` is identified here by REPETITION, which needs no name at all.

⚠ SHARED STRINGS ARE NOT CLASS IDENTITY (trap inherited from
``classname_forwarder_audit``): ``RndMat``, ``DxMat`` and ``NgMat`` all spell
"Mat".  Within a single registration list that is harmless -- a list does not
register two classes under one string -- but do NOT use these literals to name a
class in isolation.

Run ``--selfcheck`` first.  It adjudicates a list whose slots the map DOES name
and reports agreement; a channel that resolves 0 slots reads exactly like a
clean "no defects found".
"""
from __future__ import annotations

import argparse
import collections
import os
import re
import sys

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
from tools.retail_rtti import RetailRtti  # noqa: E402


def bl_target(addr: int, w: int):
    """Decode a PowerPC ``bl`` (AA=0, LK=1) at `addr` -> absolute target."""
    if (w >> 26) != 18 or not (w & 1) or (w & 2):
        return None
    li = w & 0x03FFFFFC
    if li & 0x02000000:
        li -= 0x04000000
    return (addr + li) & 0xFFFFFFFF


class RegListReader:
    def __init__(self, path=None):
        self.R = RetailRtti(path)
        self._scn = {}

    # ---------------------------------------------------------------- W2/W3
    def static_classname_string(self, t: int):
        """Is `t` a ``StaticClassName()`` body?  -> the .rdata literal it builds.

        Shape: a guard-tested function-local ``static Symbol`` constructed from a
        string literal, so the body contains a static-init guard test and loads
        the literal's address into r4 (the ``Symbol(const char*)`` argument).
        Returns None when the shape does not hold -- callers must treat None as
        UNRESOLVED, never as "absent".
        """
        if t in self._scn:
            return self._scn[t]
        self._scn[t] = None
        n = self.R.extents.get(t)
        if n is None or not (48 <= n <= 256):
            return None
        words = [self.R.u32(t + i) for i in range(0, n, 4)]
        if any(w is None for w in words):
            return None
        if not any(w == 0x556907FF or ((w >> 26) == 21 and (w & 1) and (w & 0x7FE) == 0x7FE)
                   for w in words):
            return None                      # no static-init guard test
        regs, r4 = {}, None
        for w in words:
            op, rD, rA, imm = w >> 26, (w >> 21) & 31, (w >> 16) & 31, w & 0xFFFF
            if op == 15:                     # lis / addis
                regs[rD] = (imm << 16) if rA == 0 else (
                    ((regs[rA] + (imm << 16)) & 0xFFFFFFFF) if rA in regs else regs.get(rD))
            elif op == 14:                   # addi
                v = imm - 0x10000 if imm & 0x8000 else imm
                if rA in regs:
                    regs[rD] = (regs[rA] + v) & 0xFFFFFFFF
                    if rD == 4 and r4 is None:
                        r4 = regs[4]
        if r4 is None:
            return None
        s = self.R.cstr(r4, limit=64)
        if s and re.fullmatch(r"[A-Za-z_][\w ]*", s):
            self._scn[t] = s
        return self._scn[t]

    # ------------------------------------------------------------ the list
    def read(self, fn_va: int):
        """-> (registrations, diagnostics).

        `registrations` is an ordered list of dicts, one per RegisterFactory call
        site: {'site', 'scn_addr', 'string'}.  `string` is None when the callee
        did not decode -- reported, never dropped.
        """
        n = self.R.extents.get(fn_va)
        d = {"fn_va": fn_va, "size": n}
        if n is None:
            d["error"] = "no .pdata extent for %#x" % fn_va
            return [], d
        words = [(fn_va + i, self.R.u32(fn_va + i)) for i in range(0, n, 4)]
        bls = [(a, bl_target(a, w)) for a, w in words if w is not None]
        bls = [(a, t) for a, t in bls if t is not None]
        if not bls:
            d["error"] = "no bl instructions"
            return [], d
        # RegisterFactory is identified by REPETITION -- no symbol name involved.
        cnt = collections.Counter(t for _a, t in bls)
        rf, rfn = cnt.most_common(1)[0]
        d["register_factory"] = rf
        d["register_factory_calls"] = rfn
        d["total_bl"] = len(bls)
        if rfn < 2:
            d["error"] = "no repeated bl target -- not a registration list"
            return [], d
        out = []
        for i, (a, t) in enumerate(bls):
            if t != rf:
                continue
            prev = next((bls[j][1] for j in range(i - 1, -1, -1) if bls[j][1] != rf), None)
            out.append({"site": a, "scn_addr": prev,
                        "string": self.static_classname_string(prev) if prev else None})
        d["resolved"] = sum(1 for r in out if r["string"])
        return out, d


def _parse_va(s: str) -> int:
    s = s.strip()
    m = re.search(r"(?:fn_)?(?:0x)?([0-9A-Fa-f]{8})$", s)
    if not m:
        raise SystemExit("cannot parse address from %r" % s)
    return int(m.group(1), 16)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("addr", nargs="*", help="retail function VA(s), e.g. fn_824AABA8")
    ap.add_argument("--selfcheck", action="store_true")
    a = ap.parse_args()
    rd = RegListReader()

    if a.selfcheck:
        # A channel that resolves 0 slots reads exactly like "no defects found".
        # fn_8236CE30 (CharInit) is the highest-coverage list whose slots the map
        # also names, so agreement here exercises BOTH directions.
        regs, d = rd.read(0x8236CE30)
        print("SELFCHECK fn_8236CE30:", d)
        print("  slots=%d resolved=%d" % (len(regs), d.get("resolved", 0)))
        print("  strings:", [r["string"] for r in regs])
        # negative control: RegisterFactory itself is NOT a StaticClassName body.
        neg = rd.static_classname_string(d["register_factory"])
        print("  NEGATIVE CONTROL (RegisterFactory decodes as a class name?):", neg,
              "->", "OK, refuses" if neg is None else "BROKEN, accepts anything")
        ok = d.get("resolved", 0) > 0 and neg is None
        print("  VERDICT:", "CHANNEL LIVE" if ok else "CHANNEL VACUOUS -- do not use")
        return 0 if ok else 1

    for s in a.addr:
        va = _parse_va(s)
        regs, d = rd.read(va)
        print("\n=== %#x  %s" % (va, d))
        for i, r in enumerate(regs):
            print("  %2d  %-10s  scn=%s" % (
                i, r["string"] or "<UNRESOLVED>",
                ("%#x" % r["scn_addr"]) if r["scn_addr"] else "-"))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
