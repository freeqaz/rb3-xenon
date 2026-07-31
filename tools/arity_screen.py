#!/usr/bin/env python3
"""arity_screen.py -- map-independent mispair detector via argument-register arity.

WHY THIS EXISTS
===============
objdiff's default scoring masks relocation ARGUMENTS (report.rs hard-sets them to
None), so a function that calls the WRONG callee still scores 100% and is fully
credited.  `-c functionRelocDiffs=name_check` closes part of that hole, but only
when OUR callee name and the RETAIL callee name DISAGREE.  When a row is mispaired
AND its callee is mispaired in a CORRELATED way, both sides agree and the row reads
100/100 under BOTH scorings.  tools/namecheck_spread.py structurally cannot see
that class.

The arity screen catches it by a route that touches neither the map's internal
consistency nor the metric:

    A function's MANGLED NAME encodes its parameter list.  The MACHINE CODE
    reveals how many argument registers the body actually consumes.  If a symbol
    claims N parameters but its body READS argument registers that a function
    with N parameters could never legitimately read, the LABEL IS WRONG --
    regardless of what any map or score says.

No oracle, no retail-vs-ours comparison, no scoring.  Just (name, bytes).

THE TEST IS DELIBERATELY ONE-SIDED
==================================
We flag only  observed_max_arg_reg > declared_max_arg_reg  ("reads an argument
that does not exist").  The converse (declares more than it reads) is NOT
flagged: ignoring a parameter is perfectly legal C++ and would be a firehose of
false positives.

THREE LANDMINES, AND WHAT WE DO ABOUT THEM
==========================================
1. "Does register rN appear anywhere in the body" is a VACUOUS test -- the callee
   freely uses r3..r10 as scratch AFTER defining them.  We therefore compute
   LIVE-IN AT FUNCTION ENTRY by proper backward dataflow over the CFG
   (live_in = use U (live_out - def)).  A register is a consumed argument only if
   some path from entry READS it BEFORE writing it.

2. A `bl` clobbers all volatiles (r0, r3-r12, f0-f13).  Modelling that naively
   makes the screen VACUOUS, because MSVC X360 prologues begin with
   `mflr r12 ; bl __savegprlr_N`, which would kill every argument register before
   its first use and leave live-in empty for ~every function in the binary.
   Those helpers are UNNAMED in our map (and have NO .pdata entry).  We therefore
   detect them STRUCTURALLY: a call target "preserves args" iff its body decodes
   cleanly to a terminal `blr`, contains no other branch, and writes none of
   r3-r10 / f1-f13.  This is a sound property, not a heuristic -- if the callee
   provably never writes an argument register, that register really does survive
   the call.  (Verified: 0x82829240..0x8282926c is the __savegprlr_* fall-through
   block; 0x82270560 is a frequently-first-called LEAF that is correctly REJECTED
   by the structural test because it has a `beqlr` and writes r3.)

3. Capstone's PPC backend supplies NO operand access flags and NO regs_access()
   on this build, so read/write semantics are ours to define.  A wrong default
   silently manufactures flags.  We use an explicit family classifier with
   exception sets, and any function containing a mnemonic we cannot classify is
   EXCLUDED, never flagged.

Every direction of imprecision above is chosen to LOSE flags rather than INVENT
them (unresolved `bctr` successors, tail-call exits, and unknown callee arity all
shrink live-in).  The screen therefore under-reports; it should not over-report.
That claim is not taken on faith -- see the controls.

CONTROLS (run them; they are the whole point)
=============================================
  positive : our own compiled .obj files.  Name<->body correspondence there is
             correct BY CONSTRUCTION (MSVC mangled the name from the very
             declaration it generated the code for), so every flag is a FALSE
             POSITIVE.  This prices the ABI model + liveness + semantics table
             end to end.
  null     : shuffle the name<->body assignment on the retail subject (each body
             gets a RANDOM other function's mangled name) and re-run.  The real
             flag rate must substantially exceed this or the screen measures
             nothing.

USAGE
=====
  python3 tools/arity_screen.py control-pos            # false-positive rate
  python3 tools/arity_screen.py screen                 # the real run
  python3 tools/arity_screen.py control-null --trials 3
  python3 tools/arity_screen.py screen --json out.json --top 60
"""

from __future__ import annotations

import argparse
import bisect
import collections
import json
import os
import random
import re
import struct
import subprocess
import sys
from pathlib import Path

import capstone

ROOT = Path(__file__).resolve().parent.parent
PE_PATH = ROOT / "orig/45410914/band.exe"
MAP_PATH = ROOT / "scripts/target_symbol_map.json"
OBJ_ROOT = ROOT / "build/45410914/src"

GPR_ARGS = [f"r{i}" for i in range(3, 11)]      # r3..r10
FPR_ARGS = [f"f{i}" for i in range(1, 14)]      # f1..f13
VOLATILE_GPR = {"r0"} | {f"r{i}" for i in range(3, 13)}
VOLATILE_FPR = {f"f{i}" for i in range(0, 14)}
CALL_CLOBBER = VOLATILE_GPR | VOLATILE_FPR

REG_RE = re.compile(r"^(r|f|v)\d+$")


# ---------------------------------------------------------------------------
# 1. band.exe (decompressed retail PE) -- sections, .pdata extents, code bytes
# ---------------------------------------------------------------------------

class Retail:
    """Reader for the decompressed retail PE.

    TRAP (project memory): `va - 0x82000000` is a valid file offset ONLY for
    .rdata.  .text lives at RVA 0x270000 but raw 0x264E00, so the section table
    must actually be consulted.  We do.
    """

    def __init__(self, path=PE_PATH):
        self.data = open(path, "rb").read()
        d = self.data
        e = struct.unpack_from("<I", d, 0x3C)[0]
        assert d[e:e + 4] == b"PE\0\0", "not a PE"
        coff = e + 4
        nsec = struct.unpack_from("<H", d, coff + 2)[0]
        optsz = struct.unpack_from("<H", d, coff + 16)[0]
        opt = coff + 20
        self.base = struct.unpack_from("<I", d, opt + 28)[0]
        st = opt + optsz
        self.secs = {}
        for i in range(nsec):
            o = st + i * 40
            name = d[o:o + 8].rstrip(b"\0").decode("latin1")
            vsize = struct.unpack_from("<I", d, o + 8)[0]
            va = self.base + struct.unpack_from("<I", d, o + 12)[0]
            rawsz = struct.unpack_from("<I", d, o + 16)[0]
            rawptr = struct.unpack_from("<I", d, o + 20)[0]
            self.secs[name] = (va, vsize, rawptr, rawsz)
        self.tva, self.tvsz, self.traw, _ = self.secs[".text"]
        self._load_pdata()

    def _load_pdata(self):
        va, vsize, rawptr, rawsz = self.secs[".pdata"]
        ents = []
        for i in range(rawsz // 8):
            b, w = struct.unpack_from(">II", self.data, rawptr + i * 8)
            if not b:
                continue
            # MSVC bitfields allocate from the LSB; the DWORD is stored BE.
            #   prologLength : 8   functionLength : 22   flag32 : 1   eh : 1
            # VERIFIED: funclen*4 reproduces the delta-to-next exactly on 61.4%
            # of entries and is <= it on the rest (the remainder is inter-function
            # alignment padding), with zero negatives.
            ents.append((b, ((w >> 8) & 0x3FFFFF) * 4))
        ents.sort()
        self.pdata = dict(ents)
        self.pstarts = [a for a, _ in ents]

    def in_text(self, va):
        return self.tva <= va < self.tva + self.tvsz

    def read(self, va, n):
        o = self.traw + (va - self.tva)
        return self.data[o:o + n]

    def extent(self, va):
        """Authoritative function size from .pdata, else None.

        NOTE (project memory): .pdata-ABSENCE is NOT a 'not a function' test --
        e.g. the __savegprlr_* helpers are real code with no unwind record.  We
        use .pdata only where it exists and fall back to a scan elsewhere.
        """
        return self.pdata.get(va)


# ---------------------------------------------------------------------------
# 2. PowerPC read/write semantics  (capstone gives us nothing here)
# ---------------------------------------------------------------------------

NOP_LIKE = {"sync", "lwsync", "isync", "eieio", "nop", "msync", "ptesync", "slbia"}
CACHE_OPS = {"dcbt", "dcbtst", "dcbf", "dcbz", "dcbzl", "dcbst", "dcbi", "icbi"}
INSERT_OPS = {"rlwimi", "rlwimi.", "rldimi", "rldimi."}
MFSPR = ("mflr", "mfctr", "mfcr", "mfspr", "mfmsr", "mftb", "mfxer", "mffs", "mfocrf")
MTSPR = ("mtlr", "mtctr", "mtcr", "mtcrf", "mtspr", "mtmsr", "mtxer", "mtfsf", "mtocrf")

RET_MN = {"blr", "bclr"}
CALL_MN = {"bl", "bla", "bctrl", "bcctrl", "blrl", "bclrl", "bdnzl", "bdzl",
           "bdnzlrl", "bla+", "bl+"}
INDIRECT_MN = {"bctr", "bcctr", "bctra"}
UNCOND_MN = {"b", "ba"}


def _regs(insn, op):
    """Register names touched by one capstone operand (reg or memory base/index)."""
    out = []
    if op.type == capstone.ppc.PPC_OP_REG:
        n = insn.reg_name(op.reg)
        if n and REG_RE.match(n):
            out.append(n)
    elif op.type == capstone.ppc.PPC_OP_MEM:
        for r in (op.mem.base, getattr(op.mem, "index", 0)):
            if r:
                n = insn.reg_name(r)
                if n and REG_RE.match(n):
                    out.append(n)
    return out


def semantics(insn):
    """Return (reads, writes, ok).  ok=False => unclassified, caller must bail.

    Family classifier.  Every ambiguous case is resolved toward NOT inventing a
    read, because an invented read is what turns this screen into a false-positive
    generator.
    """
    mn = insn.mnemonic.rstrip("+-")
    ops = insn.operands
    allregs = []
    for o in ops:
        allregs.extend(_regs(insn, o))

    if mn in NOP_LIKE:
        return set(), set(), True
    if mn in CACHE_OPS:
        return set(allregs), set(), True

    # --- control flow: branches never write a GPR/FPR we care about
    if mn in RET_MN or mn in INDIRECT_MN or mn in UNCOND_MN or mn in CALL_MN \
            or (mn.startswith("b") and mn not in ("bpermd",)):
        if mn in CALL_MN:
            # An ordinary call clobbers all volatiles.  (Arg-preserving stubs are
            # handled by the caller, which rewrites the write-set.)
            return set(), set(CALL_CLOBBER), True
        return set(allregs), set(), True

    # --- traps
    if mn.startswith("tw") or mn.startswith("td") or mn == "trap":
        return set(allregs), set(), True

    # --- compares: every register operand is READ; only CR is written
    if mn.startswith("cmp") or mn.startswith("fcmp"):
        return set(allregs), set(), True

    # --- special-purpose register moves
    if mn.startswith(MTSPR):
        return set(allregs), set(), True
    if mn.startswith(MFSPR):
        w = _regs(insn, ops[0]) if ops else []
        r = [x for o in ops[1:] for x in _regs(insn, o)]
        return set(r), set(w), True

    # --- rlwimi & friends: destination is READ-MODIFY-WRITE
    if mn in INSERT_OPS:
        d = _regs(insn, ops[0]) if ops else []
        r = [x for o in ops[1:] for x in _regs(insn, o)]
        return set(r) | set(d), set(d), True

    # --- stores: operand 0 is a SOURCE (read), not a destination
    if mn.startswith("st"):
        if not ops:
            return set(), set(), False
        r = list(allregs)
        w = set()
        if mn.endswith("u") or mn.endswith("ux"):      # update form writes base
            for o in ops[1:]:
                if o.type == capstone.ppc.PPC_OP_MEM and o.mem.base:
                    n = insn.reg_name(o.mem.base)
                    if n and REG_RE.match(n):
                        w.add(n)
                elif o.type == capstone.ppc.PPC_OP_REG:
                    n = insn.reg_name(o.reg)
                    if n and REG_RE.match(n):
                        w.add(n)
                break
        return set(r), w, True

    # --- loads (but li/lis are load-IMMEDIATE, i.e. plain destination writes)
    if mn.startswith("l") and mn not in ("li", "lis", "la"):
        if not ops:
            return set(), set(), False
        w = set(_regs(insn, ops[0]))
        r = set(x for o in ops[1:] for x in _regs(insn, o))
        if mn.endswith("u") or mn.endswith("ux"):
            for o in ops[1:]:
                if o.type == capstone.ppc.PPC_OP_MEM and o.mem.base:
                    n = insn.reg_name(o.mem.base)
                    if n and REG_RE.match(n):
                        w.add(n)
                break
        return r, w, True

    # --- PowerPC HINT-NOP: `or Rx,Rx,Rx` (capstone prints it as `mr rX, rX`) is
    # the architected no-op/priority hint.  MSVC X360 sprinkles it as call-site
    # padding -- e.g. _inconsistency@0x82831d70 has five around one `bctrl`.
    # Treating it as a read of rX makes rX spuriously live-in and was the single
    # largest false-positive class on the RETAIL corpus.
    # NOTE: our own compiled objects contain none of these, so the positive
    # control could not have caught it -- a real COVERAGE GAP in that control.
    if mn in ("mr", "or") and len(ops) >= 2:
        rr = [x for o in ops for x in _regs(insn, o)]
        if rr and all(x == rr[0] for x in rr):
            return set(), set(), True

    # --- self-cancelling idioms: architecturally these READ the register, but
    # the result is value-INDEPENDENT, so MSVC emits them on undefined inputs.
    #   subfe rD,rA,rA -> CA-1  (materialise carry; real ctor + deque codegen)
    #   subf  rD,rA,rA -> 0       xor rD,rA,rA -> 0
    # The condition is that the two SOURCE operands are the same register; the
    # destination is irrelevant (an earlier, narrower rule requiring all three to
    # match missed `subfe r11,r7,r7` in deque<T>::_M_reallocate_map).
    if mn.rstrip(".") in ("subf", "subfe", "xor", "subfc", "eqv") and len(ops) == 3:
        rr = [x for o in ops for x in _regs(insn, o)]
        if len(rr) == 3 and rr[1] == rr[2]:
            return set(), {rr[0]}, True

    # --- default: destination-first ALU / FPU / vector form
    if mn in KNOWN_ALU:
        if not ops:
            return set(), set(), True
        w = set(_regs(insn, ops[0]))
        r = set(x for o in ops[1:] for x in _regs(insn, o))
        return r, w, True

    return set(), set(), False


# Destination-first opcodes seen in retail .text.  Anything outside this set
# makes the containing function UNCLASSIFIED (excluded), never flagged.
KNOWN_ALU = set("""
li lis la mr mr. add add. addi addic addic. addis adde adde. addme addme. addze addze.
addc addc. subf subf. subfc subfc. subfe subfe. subfic subfme subfze subfze. sub subi
neg neg. mulli mullw mullw. mulhw mulhwu mulld mulldu mulhd mulhdu divw divw. divwu
divwu. divd divd. divdu divdu. and and. andc andc. andi. andis. or or. orc orc. ori
oris xor xor. xori xoris nand nand. nor nor. eqv eqv. extsb extsb. extsh extsh. extsw
extsw. cntlzw cntlzw. cntlzd cntlzd. popcntb popcntw rlwinm rlwinm. rlwnm rlwnm.
rldicl rldicl. rldicr rldicr. rldic rldic. rldcl rldcr slw slw. srw srw. sraw sraw.
srawi srawi. sld sld. srd srd. srad srad. sradi sradi. slwi slwi. srwi srwi. clrlwi
clrlwi. clrrwi clrrwi. clrlslwi extlwi extrwi inslwi insrwi rotlw rotlw. rotlwi
rotlwi. rotrwi sldi sldi. srdi srdi. clrldi clrldi. clrrdi rotldi rotld
fmr fmr. fabs fabs. fnabs fneg fneg. fadd fadd. fadds fadds. fsub fsub. fsubs fsubs.
fmul fmul. fmuls fmuls. fdiv fdiv. fdivs fdivs. fmadd fmadd. fmadds fmadds. fmsub
fmsub. fmsubs fmsubs. fnmadd fnmadds fnmsub fnmsubs fsqrt fsqrts frsqrte frsqrtes
fres fsel fsel. frsp frsp. fctiw fctiwz fctid fctidz fcfid fcfids fmr frin friz frip
frim
vor vand vandc vnor vxor vaddubm vadduhm vadduwm vsububm vsubuhm vsubuwm vspltb vsplth
vspltw vspltisb vspltish vspltisw vslb vslh vslw vsrb vsrh vsrw vsrab vsrah vsraw
vperm vsel vmrghb vmrghh vmrghw vmrglb vmrglh vmrglw vcmpequb vcmpequh vcmpequw
vcmpequd vcmpgtub vcmpgtsw vcmpneb vcmpneh vcmpnew vcmpnezb vcmpnezh vcmpneb.
vcmpneh. vcmpnew. vcmpnezb. vcmpnezh. vcmpequb. vcmpequh. vcmpequw. vcmpequd. vupkhsb
vupklsb vrlb vrlh vrlw vmaxub vminub vavgub
""".split())


# ---------------------------------------------------------------------------
# 3. CFG + backward liveness  ->  registers live on entry
# ---------------------------------------------------------------------------

class Bail(Exception):
    """Function cannot be analysed soundly; it is excluded, never flagged."""


class Analyzer:
    def __init__(self, retail=None, arg_preserving=None):
        self.md = capstone.Cs(capstone.CS_ARCH_PPC,
                              capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
        self.md.detail = True
        self.retail = retail
        self._preserve_cache = {}
        self.arg_preserving = arg_preserving if arg_preserving is not None else set()
        # Call-SITE addresses known to target an arg-preserving helper.  Needed
        # for our own .obj files, where a `bl` carries displacement 0 and the
        # callee is only identifiable through the REL24 relocation.
        self.preserve_at = set()

    # -- the landmine-2 defence -------------------------------------------
    def preserves_args(self, target):
        """True iff calling `target` provably clobbers no argument register.

        Sound, not heuristic: we decode the callee to its terminal `blr` and
        require that it contains no other branch and writes none of r3-r10 /
        f1-f13.  Used to keep `bl __savegprlr_N` from erasing every argument.
        """
        if target in self.arg_preserving:
            return True
        if self.retail is None:
            return False
        if target in self._preserve_cache:
            return self._preserve_cache[target]
        ok = False
        try:
            if self.retail.in_text(target):
                code = self.retail.read(target, 64 * 4)
                n = 0
                for ins in self.md.disasm(code, target):
                    n += 1
                    mn = ins.mnemonic.rstrip("+-")
                    if mn == "blr":
                        ok = True
                        break
                    if mn.startswith("b"):          # any other branch -> reject
                        break
                    r, w, good = semantics(ins)
                    if not good:
                        break
                    if w & (set(GPR_ARGS) | set(FPR_ARGS)):
                        break
                    if n >= 64:
                        break
        except Exception:
            ok = False
        self._preserve_cache[target] = ok
        return ok

    def decode(self, code, start):
        insns = list(self.md.disasm(code, start))
        if len(insns) * 4 != len(code):
            raise Bail("incomplete disassembly")
        return insns

    def live_in(self, insns, start, end):
        """Backward dataflow.  Returns (live_in_at_entry, flags, index)."""
        addrs = [i.address for i in insns]
        byaddr = {i.address: i for i in insns}
        flags = set()

        # -- per-instruction semantics, with call-clobber refinement
        sem = {}
        for ins in insns:
            mn = ins.mnemonic.rstrip("+-")
            r, w, ok = semantics(ins)
            if not ok:
                raise Bail(f"unknown mnemonic {mn}")
            if mn in CALL_MN:
                if ins.address in self.preserve_at:
                    w = set()                   # obj side: REL24 -> save/rest helper
                    flags.add("HELPER_CALL")
                elif mn == "bl" and ins.operands and \
                        ins.operands[0].type == capstone.ppc.PPC_OP_IMM:
                    tgt = ins.operands[0].imm
                    if self.preserves_args(tgt):
                        w = set()               # prologue save/restore helper
                        flags.add("HELPER_CALL")
                    else:
                        flags.add("CALL")
                else:
                    flags.add("ICALL")
            sem[ins.address] = (r, w)

        # -- leaders
        leaders = {start}
        for idx, ins in enumerate(insns):
            mn = ins.mnemonic.rstrip("+-")
            nxt = ins.address + 4
            if mn in RET_MN or mn in INDIRECT_MN or mn in UNCOND_MN or \
                    (mn.startswith("b") and mn not in CALL_MN):
                if nxt < end:
                    leaders.add(nxt)
                for o in ins.operands:
                    if o.type == capstone.ppc.PPC_OP_IMM:
                        t = o.imm
                        if start <= t < end:
                            leaders.add(t)
        leaders = sorted(x for x in leaders if start <= x < end)

        def block_of(a):
            return leaders[bisect.bisect_right(leaders, a) - 1]

        blocks = {}
        for bi, ld in enumerate(leaders):
            nxt = leaders[bi + 1] if bi + 1 < len(leaders) else end
            blocks[ld] = [byaddr[a] for a in addrs if ld <= a < nxt]

        # -- successors
        succ = {}
        for ld, body in blocks.items():
            last = body[-1]
            mn = last.mnemonic.rstrip("+-")
            fall = last.address + 4
            s = []
            if mn in RET_MN:
                pass
            elif mn in INDIRECT_MN:
                flags.add("INDIRECT_JUMP")          # unresolved -> shrinks live-in
            elif mn in UNCOND_MN:
                t = next((o.imm for o in last.operands
                          if o.type == capstone.ppc.PPC_OP_IMM), None)
                if t is None:
                    raise Bail("unresolved uncond branch")
                if start <= t < end:
                    s.append(t)
                else:
                    flags.add("TAILCALL")           # exit; reads nothing (FN-safe)
            elif mn.startswith("b") and mn not in CALL_MN:
                if mn.endswith("lr"):               # conditional return
                    if fall < end:
                        s.append(fall)
                else:
                    t = next((o.imm for o in last.operands
                              if o.type == capstone.ppc.PPC_OP_IMM), None)
                    if t is None:
                        raise Bail(f"unresolved branch {mn}")
                    if start <= t < end:
                        s.append(t)
                    else:
                        flags.add("TAILCALL")
                    if fall < end:
                        s.append(fall)
            else:
                if fall < end:
                    s.append(fall)
            succ[ld] = [block_of(x) for x in s]

        # -- use/def per block
        use, dfn = {}, {}
        for ld, body in blocks.items():
            u, d = set(), set()
            for ins in body:
                r, w = sem[ins.address]
                u |= (r - d)
                d |= w
            use[ld], dfn[ld] = u, d

        # -- fixpoint (reverse order converges fast)
        lin = {ld: set(use[ld]) for ld in blocks}
        order = list(reversed(leaders))
        for _ in range(len(leaders) + 3):
            changed = False
            for ld in order:
                lo = set()
                for s in succ[ld]:
                    lo |= lin[s]
                new = use[ld] | (lo - dfn[ld])
                if new != lin[ld]:
                    lin[ld] = new
                    changed = True
            if not changed:
                break
        else:
            raise Bail("liveness did not converge")

        return lin[start], flags, (blocks, succ, sem, leaders)

    def witness(self, reg, start, ctx):
        """First instruction on some path from entry that READS reg before defining it."""
        blocks, succ, sem, leaders = ctx
        seen = set()
        stack = [(start, frozenset())]
        while stack:
            ld, killed = stack.pop(0)
            if (ld, killed) in seen:
                continue
            seen.add((ld, killed))
            k = set(killed)
            for ins in blocks[ld]:
                r, w = sem[ins.address]
                if reg in r and reg not in k:
                    return ins
                k |= w
                if reg in k:
                    break
            if reg not in k:
                for s in succ[ld]:
                    stack.append((s, frozenset(k)))
        return None

    def consumed(self, code, start, end):
        insns = self.decode(code, start)
        if not insns:
            raise Bail("empty")
        lin, flags, ctx = self.live_in(insns, start, end)
        gprs = sorted((int(x[1:]) for x in lin if x in set(GPR_ARGS)))
        fprs = sorted((int(x[1:]) for x in lin if x in set(FPR_ARGS)))
        return gprs, fprs, flags, ctx, insns


# ---------------------------------------------------------------------------
# 4. Mangled name -> declared argument slots
#
# ABI MEASURED (lane CE-2 subagent, 30 probe TUs through the retail 10224 cl.exe
# with the real /O1 /Oi /GR /EHsc flags; listings in ~/tmp/ce2abi):
#   * Arguments occupy POSITIONAL 8-byte slots; slot n -> r(2+n) for n=1..8,
#     slots >=9 spill to the caller's outgoing area.  Home area is r1+0x10,
#     stride 8.  Triangulated three ways (9th int at 0x54, 11th at 0x64).
#   * bool/char/short/int/long/enum/pointer/reference/long long = 1 slot, READ.
#     (`long long` is a full 64-bit register; GPRs are 64-bit, so it is 1 slot.)
#   * float/double = 1 slot, but the slot's GPR is BURNED AND NEVER READ; the
#     value travels in frN, numbered by FP-arg counter, not slot index.
#     Clincher: void f(7 ints, float, int) puts the trailing int on the STACK.
#   * `this` = 1 slot.  Hidden sret pointer = 1 slot, and it comes BEFORE `this`.
#   * POD struct by value = ceil(sizeof/8) slots; a user copy-ctor or a virtual
#     function collapses it to a 1-slot hidden pointer.  NEITHER sizeof NOR that
#     collapse is recoverable from the mangled name => genuinely unbounded =>
#     such signatures are EXCLUDED, not approximated.
#   * __vector4 consumes ZERO GPR and zero stack slots (vrN).
#   * varargs spill r3..r10 unconditionally => excluded.
#
# SAFETY RULE used throughout: OVER-predicting the slot count raises the highest
# legitimately-readable register, which can only LOSE flags.  So wherever the
# count is uncertain but BOUNDED (sret, vectors) we take the maximum and keep the
# row; only the UNBOUNDED case (by-value aggregates) forces exclusion.
# ---------------------------------------------------------------------------

SCALAR_GPR = {
    "bool", "char", "signed char", "unsigned char", "short", "unsigned short",
    "int", "unsigned int", "long", "unsigned long", "wchar_t", "__int64",
    "unsigned __int64", "long long", "unsigned long long", "__int32",
    "unsigned __int32", "short int", "long int", "unsigned short int",
    "unsigned long int", "signed int", "signed short", "signed long",
    "__int16", "unsigned __int16", "__int8", "unsigned __int8", "char8_t",
    "char16_t", "char32_t",
}
SCALAR_FP = {"float", "double", "long double"}
AGG_PREFIX = ("class ", "struct ", "union ")


def strip_templates(s):
    out, depth = [], 0
    for ch in s:
        if ch == "<":
            depth += 1
        elif ch == ">":
            depth = max(0, depth - 1)
        elif depth == 0:
            out.append(ch)
    return "".join(out)


def split_params(s):
    """Depth-aware split of a parameter list.  Returns None if unbalanced."""
    parts, buf, depth = [], [], 0
    for ch in s:
        if ch in "<([{":
            depth += 1
        elif ch in ">)]}":
            depth -= 1
            if depth < 0:
                return None
        if ch == "," and depth == 0:
            parts.append("".join(buf).strip())
            buf = []
        else:
            buf.append(ch)
    if depth != 0:
        return None
    last = "".join(buf).strip()
    if last:
        parts.append(last)
    return parts


CV_TAIL = re.compile(r"(\s+(const|volatile|__restrict|&|&&))+\s*$")


def extract_param_text(demangled):
    """Return the top-level parameter list text, or None if we cannot be sure."""
    s = CV_TAIL.sub("", demangled.strip())
    if not s.endswith(")"):
        return None
    depth = 0
    for i in range(len(s) - 1, -1, -1):
        if s[i] == ")":
            depth += 1
        elif s[i] == "(":
            depth -= 1
            if depth == 0:
                # A ')' or '*' immediately before '(' means this paren group
                # belongs to a function-POINTER return type, not to our params.
                j = i - 1
                while j >= 0 and s[j] == " ":
                    j -= 1
                if j >= 0 and s[j] in ")*":
                    return None
                return s[i + 1:-1]
    return None


class Decl:
    __slots__ = ("mangled", "demangled", "slots", "gpr_slots", "fp_slots",
                 "has_this", "excl", "kind")

    def __init__(self, **kw):
        for k in self.__slots__:
            setattr(self, k, kw.get(k))


def classify_param(p):
    """-> ('gpr'|'fp'|'vec', slots) or ('excl', reason)"""
    p = p.strip()
    if p in ("...", "..."):
        return ("excl", "varargs")
    if p == "void" or p == "":
        return ("void", 0)
    bare = strip_templates(p)
    if "*" in bare or "&" in bare:
        return ("gpr", 1)                       # pointer / reference / fn-ptr
    b = bare.strip()
    b = re.sub(r"\b(const|volatile|__restrict)\b", " ", b)
    b = re.sub(r"\s+", " ", b).strip()
    if b in SCALAR_GPR:
        return ("gpr", 1)
    if b in SCALAR_FP:
        return ("fp", 1)
    if b.startswith("enum "):
        return ("gpr", 1)
    if "__vector4" in b or "__vector" in b or b.endswith("__m128"):
        return ("vec", 1)                       # over-predict; true cost is 0 slots
    if b.startswith(AGG_PREFIX):
        return ("excl", "byval-aggregate")      # ceil(sizeof/8): UNBOUNDED
    if b.startswith("`") or "'" in b:
        return ("excl", "odd-type")
    return ("excl", f"unknown-type:{b[:32]}")


RET_SPLIT = re.compile(r"^(?:\[thunk\]:)?\s*(?:(public|protected|private):\s*)?"
                       r"(?:(static)\s+)?(?:(virtual)\s+)?(.*)$")


def build_decl(mangled, demangled):
    d = Decl(mangled=mangled, demangled=demangled, excl=None, kind="fn")
    if demangled is None or demangled == mangled:
        d.excl = "demangle-failed"
        return d
    if demangled.startswith("[thunk]"):
        d.excl = "thunk"
        return d
    if mangled.endswith("ZZ"):
        d.excl = "varargs"
        return d
    # data symbols (vtables, RTTI, string literals) have no parameter list
    if mangled.startswith(("??_7", "??_8", "??_R", "??_C", "?$", "??_B")):
        d.excl = "data-symbol"
        return d

    m = RET_SPLIT.match(demangled)
    access, static_kw = m.group(1), m.group(2)
    rest = m.group(4)
    # `this` iff MSVC's function-class char was a MEMBER one.  llvm-undname
    # surfaces exactly that as the access specifier: free functions (mangling
    # class Y/Z) NEVER get one, even inside a namespace -- which is why we do
    # not try to infer member-ness from the presence of '::'.
    d.has_this = bool(access) and not static_kw

    ptext = extract_param_text(demangled)
    if ptext is None:
        d.excl = "param-list-unparsable"
        return d
    parts = split_params(ptext)
    if parts is None:
        d.excl = "param-split-unbalanced"
        return d

    slots = 0
    gpr_ok = []          # slot indices (1-based) that may legitimately be READ
    fp_n = 0
    if d.has_this:
        slots += 1
        gpr_ok.append(slots)

    hidden_tail = mangled.startswith(("??0", "??1", "??_G", "??_E"))

    # Return type: a class/struct return MAY be sret (slot 1, before `this`).
    # We cannot see sizeof, so we OVER-PREDICT by assuming sret whenever the
    # return type is an aggregate -- that only ever loses flags.
    head = rest[:rest.find("(")] if "(" in rest else rest
    hb = strip_templates(head)
    if hb.strip().startswith(AGG_PREFIX) and "*" not in hb and "&" not in hb:
        slots += 1
        gpr_ok.append(slots)
        d.kind = "sret?"

    for p in parts:
        k, n = classify_param(p)
        if k == "void":
            continue
        if k == "excl":
            d.excl = n
            return d
        slots += n
        if k == "gpr":
            gpr_ok.append(slots)
        elif k == "fp":
            fp_n += 1

    # MSVC passes a HIDDEN flag to ctors/dtors of classes with virtual bases
    # (`__$isMostDerived`) and a deallocation flag to `??_G`/`??_E`.  It appears
    # in NEITHER the mangled parameter list NOR the demangled text, and it is
    # APPENDED AFTER the declared parameters -- putting it anywhere else shifts
    # every real parameter's slot and corrupts the per-slot mapping.  (That
    # mistake produced all 34 residual false positives in the first control run:
    # AutoTimer(Timer*, float, fnptr, void*) reads r3,r4,r6,r7, which is exactly
    # the true layout with the float burning an unread r5.)  Over-predicting one
    # trailing slot can only lose flags.
    if hidden_tail:
        slots += 1
        gpr_ok.append(slots)
        d.kind = "ctor/dtor+hidden"
    d.slots = slots
    d.gpr_slots = gpr_ok
    d.fp_slots = fp_n
    return d


def demangle_batch(names, chunk=4000):
    """llvm-undname in batches.  Echoes input then output, blank-line separated."""
    out = {}
    names = list(names)
    for i in range(0, len(names), chunk):
        part = names[i:i + chunk]
        try:
            p = subprocess.run(["llvm-undname"], input="\n".join(part) + "\n",
                               capture_output=True, text=True, timeout=600)
        except Exception:
            for n in part:
                out[n] = None
            continue
        lines = [l for l in p.stdout.split("\n")]
        j = 0
        cur = None
        for l in lines:
            if not l.strip():
                continue
            if cur is None:
                cur = l.strip()
            else:
                out[cur] = l.strip()
                cur = None
                j += 1
        for n in part:
            out.setdefault(n, None)
    return out


# ---------------------------------------------------------------------------
# 5. Our compiled COFF .obj files  --  the POSITIVE CONTROL corpus
#
# Name<->body correspondence here is correct BY CONSTRUCTION: MSVC mangled the
# symbol from the very declaration it generated the code for.  So every flag the
# screen raises on this corpus is, by definition, a FALSE POSITIVE.  That makes
# it a direct end-to-end price on the ABI model + liveness + semantics table --
# a much sharper control than "rows that happen to score 100".
# ---------------------------------------------------------------------------

IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3
ARG_PRESERVING_NAMES = re.compile(r"__(save|rest)(gprlr|gpr|fpr|vmx)")


class CoffObj:
    def __init__(self, path):
        self.path = path
        d = open(path, "rb").read()
        self.d = d
        if len(d) < 20:
            raise Bail("short obj")
        self.machine, nsec, _ts, symptr, nsym, optsz, _ch = \
            struct.unpack_from("<HHIIIHH", d, 0)
        self.secs = []
        for i in range(nsec):
            o = 20 + optsz + i * 40
            name = d[o:o + 8].rstrip(b"\0").decode("latin1")
            rawsz = struct.unpack_from("<I", d, o + 16)[0]
            rawptr = struct.unpack_from("<I", d, o + 20)[0]
            relptr = struct.unpack_from("<I", d, o + 24)[0]
            nrel = struct.unpack_from("<H", d, o + 32)[0]
            self.secs.append((name, rawsz, rawptr, relptr, nrel))
        # symbol table + string table
        self.syms = []
        strbase = symptr + nsym * 18
        i = 0
        while i < nsym:
            o = symptr + i * 18
            raw = d[o:o + 8]
            if raw[:4] == b"\0\0\0\0":
                off = struct.unpack_from("<I", d, o + 4)[0]
                e = d.index(b"\0", strbase + off)
                name = d[strbase + off:e].decode("latin1")
            else:
                name = raw.rstrip(b"\0").decode("latin1")
            val, secnum, typ, sclass, naux = struct.unpack_from("<IhHBB", d, o + 8)
            self.syms.append((name, val, secnum, typ, sclass))
            i += 1 + naux

    def functions(self):
        """Yield (name, section_index, start_off, end_off)."""
        bysec = collections.defaultdict(list)
        for name, val, secnum, typ, sclass in self.syms:
            if secnum <= 0 or secnum > len(self.secs):
                continue
            if typ != 0x20:                     # DTYPE_FUNCTION
                continue
            if sclass not in (IMAGE_SYM_CLASS_EXTERNAL, IMAGE_SYM_CLASS_STATIC):
                continue
            if not self.secs[secnum - 1][0].startswith(".text"):
                continue
            bysec[secnum - 1].append((val, name))
        for si, lst in bysec.items():
            lst.sort()
            rawsz = self.secs[si][1]
            for k, (val, name) in enumerate(lst):
                end = lst[k + 1][0] if k + 1 < len(lst) else rawsz
                if end > val:
                    yield name, si, val, end

    def code(self, si):
        name, rawsz, rawptr, _rp, _nr = self.secs[si]
        return self.d[rawptr:rawptr + rawsz]

    def arg_preserving_targets(self, si):
        """Byte offsets of REL24 call sites whose callee is a save/rest helper."""
        _n, _rs, _rp, relptr, nrel = self.secs[si]
        out = set()
        for i in range(nrel):
            o = relptr + i * 10
            vaddr, symidx, rtype = struct.unpack_from("<IIH", self.d, o)
            if symidx < len(self.syms) and ARG_PRESERVING_NAMES.search(self.syms[symidx][0] or ""):
                out.add(vaddr)
        return out


# ---------------------------------------------------------------------------
# 6. Drivers
# ---------------------------------------------------------------------------

def evaluate(decl, gprs, fprs):
    """Return (verdict, detail).  verdict in {'consistent','FLAG_BEYOND','FLAG_FPSLOT'}"""
    if not gprs:
        return "consistent", None
    maxobs = max(gprs)
    allowed_max_slot = min(decl.slots, 8)
    highest_legit = 2 + allowed_max_slot          # slot n -> r(2+n)
    if maxobs > highest_legit:
        return "FLAG_BEYOND", (maxobs, highest_legit)
    # tighter, still-sound test: a slot occupied by a float/double param has its
    # GPR BURNED AND NEVER READ (measured).  Reading it is anomalous.
    okregs = {2 + s for s in decl.gpr_slots}
    bad = [g for g in gprs if g not in okregs and g <= highest_legit]
    if bad:
        return "FLAG_FPSLOT", (bad, sorted(okregs))
    return "consistent", None


class Screen:
    def __init__(self, retail=None):
        self.retail = retail
        self.an = Analyzer(retail=retail)
        self.stats = collections.Counter()

    def run_one(self, mangled, decl, code, start, end, preserve_offsets=None):
        if decl.excl:
            self.stats["excl:" + decl.excl] += 1
            return None
        try:
            self.an.preserve_at = preserve_offsets or set()
            gprs, fprs, flags, ctx, insns = self.an.consumed(code, start, end)
        except Bail as e:
            self.stats["bail:" + str(e)[:28]] += 1
            return None
        except Exception as e:
            self.stats["error:" + type(e).__name__] += 1
            return None
        self.stats["analysed"] += 1
        verdict, detail = evaluate(decl, gprs, fprs)
        self.stats[verdict] += 1
        if verdict == "consistent":
            return None
        reg = f"r{detail[0]}" if verdict == "FLAG_BEYOND" else f"r{detail[0][0]}"
        w = self.an.witness(reg, start, ctx)
        return {
            "symbol": mangled,
            "demangled": decl.demangled,
            "verdict": verdict,
            "declared_slots": decl.slots,
            "has_this": decl.has_this,
            "gpr_slots_readable": [2 + s for s in decl.gpr_slots],
            "observed_gprs": [f"r{g}" for g in gprs],
            "highest_legit": f"r{detail[1]}" if verdict == "FLAG_BEYOND" else None,
            "witness": (f"{w.address:#010x}  {w.mnemonic} {w.op_str}" if w else None),
            "flags": sorted(flags),
            "size": end - start,
            "addr": f"{start:#010x}",
        }


def trim_decodable(md, code, start):
    """Largest prefix of `code` that fully disassembles (COMDAT tail padding)."""
    n = len(code) // 4 * 4
    insns = list(md.disasm(code[:n], start))
    if len(insns) * 4 == n:
        return n
    return len(insns) * 4


def load_subject(retail, mapping):
    """(addr, name, code, start, end) for every mapped retail function we can read."""
    out = []
    for k, v in mapping.items():
        if not k.startswith("0x"):
            continue
        va = int(k, 16)
        if not retail.in_text(va):
            continue
        sz = retail.extent(va)
        if not sz or sz < 4 or sz > 0x20000:
            continue
        out.append((va, v, sz))
    out.sort()
    return out


def report(title, stats, rows, top, jsonpath=None, quiet=False):
    an = stats["analysed"]
    fb, ff = stats["FLAG_BEYOND"], stats["FLAG_FPSLOT"]
    print(f"\n===== {title} =====")
    print(f"  analysed          : {an}")
    print(f"  FLAG_BEYOND       : {fb}   ({fb/an*100:.4f}% of analysed)" if an else "  (none)")
    print(f"  FLAG_FPSLOT       : {ff}   ({ff/an*100:.4f}% of analysed)" if an else "")
    print(f"  TOTAL FLAGGED     : {fb+ff} ({(fb+ff)/an*100:.4f}%)" if an else "")
    if not quiet:
        print("  -- exclusions / bails (top 14):")
        for k, c in stats.most_common():
            if k.startswith(("excl:", "bail:", "error:")):
                print(f"       {k:44s} {c}")
    if rows and top:
        print(f"  -- top {min(top,len(rows))} flagged rows:")
        for r in rows[:top]:
            print(f"    [{r['verdict']}] {r['addr']} sz={r['size']:<6d} "
                  f"slots={r['declared_slots']} legit<={r['highest_legit']} "
                  f"obs={','.join(r['observed_gprs'])}")
            print(f"        {r['symbol']}")
            print(f"        {r['demangled']}")
            print(f"        witness: {r['witness']}   flags={r['flags']}")
    if jsonpath:
        json.dump({"stats": dict(stats), "rows": rows}, open(jsonpath, "w"), indent=1)
        print(f"  -> {jsonpath}")
    return fb + ff, an


def cmd_screen(a, shuffle_seed=None, quiet=False):
    retail = Retail()
    mapping = json.load(open(MAP_PATH))
    subj = load_subject(retail, mapping)
    if a.limit:
        subj = subj[:a.limit]
    names = [n for _, n, _ in subj]
    dm = demangle_batch(sorted(set(names)))
    decls = {n: build_decl(n, dm.get(n)) for n in set(names)}

    if shuffle_seed is not None:
        # PERMUTATION NULL: every body keeps its bytes, every name keeps its
        # declared arity, only the PAIRING is randomised.
        rnd = random.Random(shuffle_seed)
        shuffled = names[:]
        rnd.shuffle(shuffled)
        subj = [(va, shuffled[i], sz) for i, (va, _n, sz) in enumerate(subj)]

    sc = Screen(retail=retail)
    rows = []
    for va, name, sz in subj:
        code = retail.read(va, sz)
        if len(code) != sz:
            sc.stats["bail:short-read"] += 1
            continue
        r = sc.run_one(name, decls[name], code, va, va + sz)
        if r:
            rows.append(r)
    rows.sort(key=lambda r: (-r["size"], r["symbol"]))
    return sc.stats, rows


def cmd_control_pos(a):
    objs = sorted(OBJ_ROOT.rglob("*.obj"))
    if a.limit:
        objs = objs[:a.limit]
    md = capstone.Cs(capstone.CS_ARCH_PPC,
                     capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
    md.detail = True
    pending = []
    for p in objs:
        try:
            o = CoffObj(p)
        except Exception:
            continue
        for name, si, s, e in o.functions():
            pending.append((p, o, name, si, s, e))
    dm = demangle_batch(sorted({x[2] for x in pending}))
    decls = {n: build_decl(n, dm.get(n)) for n in {x[2] for x in pending}}
    sc = Screen(retail=None)
    rows = []
    cache = {}
    for p, o, name, si, s, e in pending:
        key = (str(p), si)
        if key not in cache:
            cache[key] = (o.code(si), o.arg_preserving_targets(si))
        blob, pres = cache[key]
        code = blob[s:e]
        n = trim_decodable(md, code, s)
        if n < 8:
            sc.stats["bail:tiny-or-undecodable"] += 1
            continue
        r = sc.run_one(name, decls[name], code[:n], s, s + n,
                       preserve_offsets={x for x in pres if s <= x < s + n})
        if r:
            r["unit"] = str(p.relative_to(OBJ_ROOT))
            rows.append(r)
    rows.sort(key=lambda r: (-r["size"], r["symbol"]))
    return sc.stats, rows



# ---------------------------------------------------------------------------
# 7. CALLER-SIDE CORROBORATION  --  a second, fully independent instrument
#
# The screen reads the CALLEE.  This reads the CALLERS: at every direct `bl`
# site targeting a flagged address, how many argument registers does the caller
# set up?  If the callers ALSO exceed the declared arity, two independent sides
# of the ABI agree that the label is wrong.
#
# `bl` is decoded by its fixed encoding (primary opcode 18, AA=0, LK=1) rather
# than by disassembling .text linearly -- capstone's disasm() STOPS at the first
# undecodable word, and .text begins with non-code padding before 0x82270018, so
# a linear sweep silently yields ZERO call sites and every row falsely reads
# "no call sites".  That bug looked exactly like corroborating evidence.
#
# ASYMMETRY, and it matters: this instrument is strong when POSITIVE (a caller
# demonstrably setting r6 is hard to explain away) and WEAK when negative -- the
# backward scan is a crude 16-instruction window that restarts at branches, so
# args set up earlier or in another block are missed.  Separately, "no call
# sites" is EXPECTED for virtual functions (dispatched through `bctrl`) and is
# NOT evidence of anything.
# ---------------------------------------------------------------------------

def index_bl_sites(retail):
    sites = collections.defaultdict(list)
    n = retail.tvsz // 4
    for i in range(n):
        w = struct.unpack_from(">I", retail.data, retail.traw + i * 4)[0]
        if (w >> 26) == 18 and (w & 3) == 1:          # bl, relative, link
            li = w & 0x03FFFFFC
            if li & 0x02000000:
                li -= 0x04000000
            a = retail.tva + i * 4
            sites[a + li].append(a)
    return sites


def caller_max_arg(md, retail, site, back=16):
    st = site - back * 4
    if not retail.in_text(st):
        return 0
    w = set()
    for ins in md.disasm(retail.read(st, back * 4), st):
        mn = ins.mnemonic
        if mn.startswith("b"):
            w = set()                                  # crude basic-block guard
            continue
        if mn.startswith(("st", "cmp", "mt", "tw", "td")):
            continue
        ops = ins.operands
        if ops and ops[0].type == capstone.ppc.PPC_OP_REG:
            nm = ins.reg_name(ops[0].reg)
            if nm and nm[0] == "r" and nm[1:].isdigit() and 3 <= int(nm[1:]) <= 10:
                w.add(int(nm[1:]))
    return max(w) if w else 0


def cmd_corroborate(a):
    retail = Retail()
    rows = json.load(open(a.rows))["rows"]
    sites = index_bl_sites(retail)
    md = capstone.Cs(capstone.CS_ARCH_PPC,
                     capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
    md.detail = True
    agree = weak = nocall = 0
    print(f"{'addr':<12}{'#bl':>5} {'callerMAX':>18} {'declMax':>8} {'obs':>5}  verdict / symbol")
    for r in rows:
        va = int(r["addr"], 16)
        cs = sites.get(va, [])
        dm = 2 + min(r["declared_slots"], 8)
        obs = max(int(x[1:]) for x in r["observed_gprs"])
        if not cs:
            nocall += 1
            continue
        c = collections.Counter(caller_max_arg(md, retail, x) for x in cs[:80])
        cm = c.most_common(1)[0][0]
        if cm > dm:
            agree += 1
            v = "CALLER_CONFIRMS"
        else:
            weak += 1
            v = "caller-inconclusive"
        print(f"{r['addr']:<12}{len(cs):>5} {str(c.most_common(2)):>18} "
              f"{'r'+str(dm):>8} {'r'+str(obs):>5}  {v}  {r['demangled'][:44]}")
    print(f"\n  CALLER CONFIRMS declared-arity violation : {agree}")
    print(f"  caller inconclusive (weak-negative only) : {weak}")
    print(f"  no direct bl sites (EXPECTED for virtuals): {nocall}")


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("cmd", choices=["screen", "control-pos", "control-null", "all",
                                    "corroborate"])
    ap.add_argument("--rows", default=None, help="screen JSON for `corroborate`")
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--top", type=int, default=25)
    ap.add_argument("--trials", type=int, default=3)
    ap.add_argument("--json", default=None)
    a = ap.parse_args()

    if a.cmd == "corroborate":
        cmd_corroborate(a)
        return
    if a.cmd in ("control-pos", "all"):
        st, rows = cmd_control_pos(a)
        report("POSITIVE CONTROL -- our own .obj files (every flag is a FALSE POSITIVE)",
               st, rows, a.top, a.json and a.json + ".pos.json")
    if a.cmd in ("screen", "all"):
        st, rows = cmd_screen(a)
        report("SCREEN -- retail band.exe bodies + target_symbol_map.json names",
               st, rows, a.top, a.json)
    if a.cmd in ("control-null", "all"):
        tot_f, tot_n = 0, 0
        for t in range(a.trials):
            st, rows = cmd_screen(a, shuffle_seed=1000 + t, quiet=True)
            f, n = report(f"PERMUTATION NULL trial {t} (name<->body randomised)",
                          st, rows, 0, None, quiet=True)
            tot_f += f
            tot_n += n
        print(f"\n  NULL MEAN over {a.trials} trials: {tot_f/max(1,tot_n)*100:.4f}% "
              f"({tot_f}/{tot_n})")


if __name__ == "__main__":
    main()
