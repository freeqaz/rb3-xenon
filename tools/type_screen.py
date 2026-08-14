#!/usr/bin/env python3
"""type_screen.py -- map-independent mispair detector via argument TYPE.

RELATION TO tools/arity_screen.py
=================================
arity_screen asks "how MANY argument registers does this body consume?" and flags
a name whose declared parameter list cannot account for the highest one read.
This tool asks the orthogonal question:

    Given that the body reads argument slot s, is it using it in a way that is
    POSSIBLE for the TYPE the mangled name declares for slot s?

Same epistemic shape -- (name, bytes) only, no oracle, no retail-vs-ours
comparison, no scoring -- so it is both map-independent and metric-independent.
That is what the mispair class needs: lane BODYPORT-3 found a row named
?SyncEnableBlinks@@ sitting on retail's SyncVignetteInterest body, scoring 82.5%
(reads as "nearly done", not as "wrong function") while the true target sat
unpaired at 0.0%.  Nothing in the metric can see that.

THE TESTS
=========
T_DEREF   A parameter declared BY VALUE as an integral/enum/bool scalar has its
          entry value used as the BASE REGISTER of a load or store.  You cannot
          dereference an `int`.  Flags "this slot really holds a pointer".
          This is the test the map's own _denylist already applied by hand once:
          0x82553fc8 was evicted because "the target reads a POINTER IN r3,
          which a static void() signature cannot have".

T_FPR     The body reads f(j) live-in for j greater than the number of
          float/double parameters the name declares.  Floats travel in f1..fN
          numbered by FP-argument counter (measured, lane CE-2), so reading f2
          when the name declares one float means the name is wrong.
          NOTE: arity_screen's evaluate() accepts an `fprs` argument and NEVER
          READS IT -- the FPR constraint has never been exercised.

T_PTRW    (ADVISORY) A pointer/reference to a 1-byte type dereferenced with a
          wider or floating load, or a pointer to float/double dereferenced
          sub-word.  Reported separately and hand-adjudicated: an inlined
          word-at-a-time copy of a bool array is a legal counter-example, so
          this one is NOT claimed to be sound.

T_FPSLOT  PRIOR ART, re-run here for completeness only: a slot occupied by a
          float has its GPR burned and never read (measured), so reading it is
          anomalous.  Already implemented as arity_screen's FLAG_FPSLOT; counted
          separately and NOT claimed as this lane's yield.

ONE-SIDEDNESS
=============
Every test flags only the IMPOSSIBLE direction.  Ignoring a parameter is legal
C++, so "declares more than it uses" is never flagged.  Wherever a slot count is
uncertain but bounded we OVER-predict (sret, vectors, ctor/dtor hidden flag),
which can only lose flags; the unbounded case (by-value aggregates) is EXCLUDED.

THE LANDMINE THAT WOULD HAVE MADE THIS VACUOUS
==============================================
capstone's PPC backend represents D-form memory (`lwz r3,8(r4)`) as a MEM
operand but X-form indexed memory (`lwzx r3,r4,r5`) as THREE PLAIN REGISTERS --
there is no MEM operand at all.  A screen that looked for `op.mem.base` would
therefore see ZERO indexed dereferences, which is precisely the access form that
discriminated Symbol[] from bool[] in BODYPORT-3.  X-form is handled explicitly
(operand[1] = base rA, None when the field is r0; operand[2] = index rB),
verified against hand-assembled words before any of this was written.

CONTROLS (run them; they are the whole point)
=============================================
  control-pos     our own compiled .obj files.  Name<->body correspondence is
                  correct BY CONSTRUCTION, so every flag is a FALSE POSITIVE.
  control-pos --shuffle
                  the SAME corpus with the name<->body pairing randomised.  This
                  exists to prove the positive control CAN FIRE: a control whose
                  population is defined by the absence of what you are measuring
                  cannot fail, and a clean 0 from such a control means nothing.
  control-null    shuffled pairing on the RETAIL subject.  Prices discriminating
                  power: the real rate must sit far below this.
  strata          real run split into a TRUSTED stratum (name byte-corroborated
                  by a fuzzy==100 pairing in report.json, and not listed in the
                  map's own _bijection_arbitrary / _icf_arbitrary) and an
                  UNTRUSTED stratum (everything else).  Reports the enrichment.

CHECKABLE POPULATION
====================
Most rows carry NO discriminating signal: a void() method, or one whose
parameters are all pointers, is unfalsifiable by T_DEREF.  Every command reports
the checkable denominator explicitly.  An FP rate quoted over a silently-narrow
population is not a rate.

USAGE
=====
  python3 tools/type_screen.py control-pos
  python3 tools/type_screen.py control-pos --shuffle
  python3 tools/type_screen.py screen --json ~/tmp/ts.json
  python3 tools/type_screen.py control-null --trials 3
  python3 tools/type_screen.py strata --report build/45410914/report.json
"""

from __future__ import annotations

import argparse
import bisect
import collections
import json
import random
import re
import sys
from pathlib import Path

import capstone

sys.path.insert(0, str(Path(__file__).resolve().parent))

from arity_screen import (  # noqa: E402
    Retail, Analyzer, Bail, CoffObj, semantics, demangle_batch,
    strip_templates, split_params, extract_param_text, RET_SPLIT,
    SCALAR_GPR, SCALAR_FP, AGG_PREFIX, trim_decodable,
    MAP_PATH, OBJ_ROOT, GPR_ARGS, FPR_ARGS, CALL_MN, CALL_CLOBBER,
    RET_MN, INDIRECT_MN, UNCOND_MN,
)

ROOT = Path(__file__).resolve().parent.parent


# ---------------------------------------------------------------------------
# 1. Load/store classification  --  width, signedness of access, and the
#    D-form vs X-form operand-shape split that capstone does not abstract.
# ---------------------------------------------------------------------------

def _mem_family(mn):
    """-> (width_bytes, 'int'|'fp'|'vec') or None if not a load/store."""
    m = mn.rstrip(".")
    if m in ("li", "lis", "la"):
        return None
    if m.startswith("lv") or m.startswith("stv"):
        return (16, "vec")
    if m.startswith("lfd") or m.startswith("stfd") or m == "stfiwx":
        return (8, "fp")
    if m.startswith("lfs") or m.startswith("stfs"):
        return (4, "fp")
    if not (m.startswith("l") or m.startswith("st")):
        return None
    body = m[1:] if m.startswith("l") else m[2:]
    if body.startswith("b"):
        return (1, "int")
    if body.startswith("h"):
        return (2, "int")
    if body.startswith("w"):
        return (4, "int")
    if body.startswith("d"):
        return (8, "int")
    if body.startswith("mw") or body.startswith("swi") or body.startswith("sw"):
        return (4, "int")          # lmw/stmw/lswi: multiple words
    return None


def mem_operands(insn):
    """-> (form, base|None, index|None, disp) for a load/store, else None.

    form is 'D' or 'X', and the DISTINCTION IS LOAD-BEARING.

    D-form (`lwz r3,8(r4)`) computes EA = (rA|0) + disp.  rA is the only
    register involved, so a value appearing there IS being dereferenced.

    X-form (`lwzx r3,r4,r5`) computes EA = (rA|0) + rB and RA AND RB ARE
    SYMMETRIC -- the architecture draws no base/index distinction and the
    compiler picks either order freely.  So an X-form appearance proves only
    "this value contributes to an address", NOT "this value is a pointer".
    The positive control caught exactly this: GemManager::CheckRemoveChordBracket(int)
    computes r4+1 and passes it as the rA field of `lbzx r10, r11, r10` while
    the actual array pointer sits in rB.  Treating rA as "the base" made a
    perfectly ordinary array index read as a dereferenced int.
    X-form therefore never produces a T_DEREF flag.

    capstone represents D-form as operand[1] of type MEM and X-form as THREE
    PLAIN REG operands with no MEM operand at all -- verified against
    hand-assembled words.  An rA field of r0 means "literal zero" and capstone
    surfaces it as reg_name(None); we propagate None, never 'r0'.
    """
    ops = insn.operands
    if len(ops) >= 2 and ops[1].type == capstone.ppc.PPC_OP_MEM:
        b = ops[1].mem.base
        idx = getattr(ops[1].mem, "index", 0)
        return ("D", insn.reg_name(b) if b else None,
                insn.reg_name(idx) if idx else None,
                ops[1].mem.disp)
    if len(ops) >= 3 and ops[1].type == capstone.ppc.PPC_OP_REG \
            and ops[2].type == capstone.ppc.PPC_OP_REG:
        bn = insn.reg_name(ops[1].reg) if ops[1].reg else None
        ix = insn.reg_name(ops[2].reg) if ops[2].reg else None
        return ("X", bn, ix, 0)
    return None


# ---------------------------------------------------------------------------
# 2. Mangled name -> PER-SLOT TYPE
#
# The slot arithmetic is deliberately IDENTICAL to arity_screen.build_decl (same
# measured ABI, lane CE-2): positional 8-byte slots, slot n -> r(2+n) for n<=8;
# float/double take a slot whose GPR is burned and never read, travelling in
# frN numbered by FP-argument counter; `this` is a slot; a hidden sret pointer
# precedes `this`; ctors/dtors carry a trailing hidden flag.  What is new here
# is that we keep WHICH TYPE landed in each slot instead of collapsing to a max.
# ---------------------------------------------------------------------------

PTR_RE = re.compile(r"[*&]")


class TypedDecl:
    __slots__ = ("mangled", "demangled", "slots", "slot_cat", "slot_text",
                 "slot_ord", "fp_count", "has_this", "excl", "kind",
                 "sret_ambiguous")

    def __init__(self, **kw):
        for k in self.__slots__:
            setattr(self, k, kw.get(k))

    def scalar_slots(self):
        """Slots holding a by-value integral/enum scalar -- the T_DEREF population."""
        return [s for s, c in self.slot_cat.items() if c == "scalar"]

    def reg_of(self, slot):
        return f"r{2 + slot}" if slot <= 8 else None


def pointee_class(p):
    """For a pointer/reference param, classify the POINTEE.

    Only the classes with a hard, non-negotiable access width are named; every
    other pointee is 'other' and never produces a flag.
    """
    b = strip_templates(p)
    b = re.sub(r"\b(const|volatile|__restrict)\b", " ", b)
    b = re.sub(r"\s+", " ", b).strip()
    if not PTR_RE.search(b):
        return "other"
    stars = b.count("*")
    if stars > 1:                      # T** -> pointee is a pointer, 4 bytes
        return "other"
    core = PTR_RE.sub("", b).strip()
    if core == "bool":
        return "bool"
    if core in ("float",):
        return "float"
    if core in ("double", "long double"):
        return "double"
    return "other"


def classify_typed_param(p):
    """-> (category, slots, detail).  category in
       {'void','gpr_scalar','gpr_ptr','fp','vec','excl'}"""
    p = p.strip()
    if p == "...":
        return ("excl", 0, "varargs")
    if p in ("void", ""):
        return ("void", 0, None)
    bare = strip_templates(p)
    if PTR_RE.search(bare):
        return ("gpr_ptr", 1, p)          # raw text; pointee classified at test time
    b = re.sub(r"\b(const|volatile|__restrict)\b", " ", bare)
    b = re.sub(r"\s+", " ", b).strip()
    if b in SCALAR_GPR:
        return ("gpr_scalar", 1, b)
    if b in SCALAR_FP:
        return ("fp", 1, b)
    if b.startswith("enum "):
        return ("gpr_scalar", 1, b)
    if "__vector4" in b or "__vector" in b or b.endswith("__m128"):
        return ("vec", 1, b)
    if b.startswith(AGG_PREFIX):
        return ("excl", 0, "byval-aggregate")
    if b.startswith("`") or "'" in b:
        return ("excl", 0, "odd-type")
    return ("excl", 0, f"unknown-type:{b[:32]}")


def build_typed_decl(mangled, demangled, assume_sret=False):
    d = TypedDecl(mangled=mangled, demangled=demangled, excl=None, kind="fn",
                  slot_cat={}, slot_text={}, slot_ord={}, fp_count=0,
                  sret_ambiguous=False)
    if demangled is None or demangled == mangled:
        d.excl = "demangle-failed"
        return d
    if demangled.startswith("[thunk]"):
        d.excl = "thunk"
        return d
    if mangled.endswith("ZZ"):
        d.excl = "varargs"
        return d
    if mangled.startswith(("??_7", "??_8", "??_R", "??_C", "?$", "??_B")):
        d.excl = "data-symbol"
        return d

    m = RET_SPLIT.match(demangled)
    access, static_kw, rest = m.group(1), m.group(2), m.group(4)
    d.has_this = bool(access) and not static_kw

    ptext = extract_param_text(demangled)
    if ptext is None:
        d.excl = "param-list-unparsable"
        return d
    parts = split_params(ptext)
    if parts is None:
        d.excl = "param-split-unbalanced"
        return d

    hidden_tail = mangled.startswith(("??0", "??1", "??_G", "??_E"))

    head = rest[:rest.find("(")] if "(" in rest else rest
    hb = strip_templates(head)
    # An aggregate return MAY be passed via a hidden sret pointer, which comes
    # BEFORE `this` and therefore SHIFTS EVERY SUBSEQUENT SLOT.  We cannot see
    # sizeof, so we cannot tell.  A per-slot TYPE screen cannot tolerate that
    # off-by-one the way a max-register screen can, so instead of guessing we
    # build BOTH slot maps and flag only what is impossible under BOTH
    # (see resolve_findings).  Sound, and it recovers ~14% of the corpus that
    # an outright exclusion would have thrown away.
    d.sret_ambiguous = bool(hb.strip().startswith(AGG_PREFIX)
                            and not PTR_RE.search(hb))

    slots = 0
    if assume_sret:
        if not d.sret_ambiguous:
            d.excl = "sret-hypothesis-inapplicable"
            return d
        slots += 1
        d.slot_cat[slots] = "sret"
        d.slot_text[slots] = "__sret"
        d.slot_ord[slots] = -1
        d.kind = "sret"

    if d.has_this:
        slots += 1
        d.slot_cat[slots] = "this"
        d.slot_text[slots] = "this"
        d.slot_ord[slots] = 0

    for ordinal, p in enumerate(parts, start=1):
        cat, n, detail = classify_typed_param(p)
        if cat == "void":
            continue
        if cat == "excl":
            d.excl = detail
            return d
        slots += n
        if cat == "gpr_scalar":
            d.slot_cat[slots] = "scalar"
        elif cat == "gpr_ptr":
            d.slot_cat[slots] = "ptr"
        elif cat == "fp":
            d.slot_cat[slots] = "fp"
            d.fp_count += 1
        elif cat == "vec":
            d.slot_cat[slots] = "vec"
        d.slot_text[slots] = detail if detail else p
        d.slot_ord[slots] = ordinal

    if hidden_tail:
        # __$isMostDerived / deallocation flag: an int APPENDED after the
        # declared parameters.  It is a scalar, but it is also invisible in the
        # name, so we mark it 'hidden' and never test it.
        slots += 1
        d.slot_cat[slots] = "hidden"
        d.slot_text[slots] = "__$hidden"
        d.slot_ord[slots] = 99
        d.kind = "ctor/dtor+hidden"
    d.slots = slots
    return d


# ---------------------------------------------------------------------------
# 3. Entry-value taint: forward MUST-analysis over the CFG
#
# For each argument register we ask not "is it read" (arity_screen's question)
# but "how is the value it arrived with actually USED".  A register holds token
# ('arg', k) at entry; the token survives `mr`, a same-source `or`, and `addi`
# (recorded with an offset, so struct member access through a parameter still
# counts as a dereference of that parameter).  Every other write kills it.
#
# Joins take the INTERSECTION (must-analysis): if two predecessors disagree
# about what a register holds, it holds nothing.  A may-analysis would let a
# path that redefines the register donate a dereference to a path that did not,
# and that would INVENT flags.  Must-analysis loses flags instead, which is the
# direction every choice in this file is biased toward.
# ---------------------------------------------------------------------------

class Roles:
    """Observed uses of one argument's entry value."""

    def __init__(self):
        self.base = []       # (width, kind, disp, addr, mnemonic) -- dereferenced
        self.index = []      # (width, kind, addr, mnemonic)

    def __bool__(self):
        return bool(self.base or self.index)


class TaintAnalyzer(Analyzer):

    def entry_roles(self, code, start, end, argregs):
        """-> (roles: reg -> Roles, flags:set).  Raises Bail if unanalysable."""
        insns = self.decode(code, start)
        if not insns:
            raise Bail("empty")
        addrs = [i.address for i in insns]
        byaddr = {i.address: i for i in insns}
        flags = set()

        sem = {}
        for ins in insns:
            mn = ins.mnemonic.rstrip("+-")
            r, w, ok = semantics(ins)
            if not ok:
                raise Bail(f"unknown mnemonic {mn}")
            if mn in CALL_MN:
                if ins.address in self.preserve_at:
                    w = set()
                    flags.add("HELPER_CALL")
                elif mn == "bl" and ins.operands and \
                        ins.operands[0].type == capstone.ppc.PPC_OP_IMM:
                    if self.preserves_args(ins.operands[0].imm):
                        w = set()
                        flags.add("HELPER_CALL")
                    else:
                        flags.add("CALL")
                else:
                    flags.add("ICALL")
            sem[ins.address] = (r, w)

        # -- basic blocks (same construction as arity_screen.live_in)
        leaders = {start}
        for ins in insns:
            mn = ins.mnemonic.rstrip("+-")
            nxt = ins.address + 4
            if mn in RET_MN or mn in INDIRECT_MN or mn in UNCOND_MN or \
                    (mn.startswith("b") and mn not in CALL_MN):
                if nxt < end:
                    leaders.add(nxt)
                for o in ins.operands:
                    if o.type == capstone.ppc.PPC_OP_IMM and start <= o.imm < end:
                        leaders.add(o.imm)
        leaders = sorted(x for x in leaders if start <= x < end)

        def block_of(a):
            return leaders[bisect.bisect_right(leaders, a) - 1]

        blocks = {}
        for bi, ld in enumerate(leaders):
            nxt = leaders[bi + 1] if bi + 1 < len(leaders) else end
            blocks[ld] = [byaddr[a] for a in addrs if ld <= a < nxt]

        succ = {}
        for ld, body in blocks.items():
            last = body[-1]
            mn = last.mnemonic.rstrip("+-")
            fall = last.address + 4
            s = []
            if mn in RET_MN:
                pass
            elif mn in INDIRECT_MN:
                flags.add("INDIRECT_BR")
            elif mn in UNCOND_MN:
                t = next((o.imm for o in last.operands
                          if o.type == capstone.ppc.PPC_OP_IMM), None)
                if t is not None and start <= t < end:
                    s.append(t)
                else:
                    flags.add("TAILCALL")
            elif mn.startswith("b") and mn not in CALL_MN:
                if mn.endswith("lr"):
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

        preds = collections.defaultdict(list)
        for ld, ss in succ.items():
            for x in ss:
                preds[x].append(ld)

        entry_state = {r: (r, 0) for r in argregs}      # reg -> (arg_reg, offset)
        roles = {r: Roles() for r in argregs}

        def transfer(ld, state):
            st = dict(state)
            for ins in blocks[ld]:
                mn = ins.mnemonic.rstrip("+-")
                r, w = sem[ins.address]
                fam = _mem_family(mn)
                if fam is not None:
                    mo = mem_operands(ins)
                    if mo:
                        form, b, ix, disp = mo
                        if form == "D" and b in st:
                            a, off = st[b]
                            roles[a].base.append(
                                (fam[0], fam[1], off + disp, ins.address,
                                 f"{ins.mnemonic} {ins.op_str}"))
                        elif form == "X":
                            # rA and rB are SYMMETRIC here: record as
                            # address-contribution only, never as a deref.
                            for reg in (b, ix):
                                if reg in st:
                                    a, off = st[reg]
                                    roles[a].index.append(
                                        (fam[0], fam[1], ins.address,
                                         f"{ins.mnemonic} {ins.op_str}"))
                # -- propagation, then kill
                prop = None
                ops = ins.operands
                if mn in ("mr", "mr.") and len(ops) == 2 and \
                        ops[0].type == capstone.ppc.PPC_OP_REG and \
                        ops[1].type == capstone.ppc.PPC_OP_REG:
                    src = ins.reg_name(ops[1].reg)
                    if src in st:
                        prop = (ins.reg_name(ops[0].reg), st[src])
                elif mn in ("or", "or.") and len(ops) == 3 and \
                        all(o.type == capstone.ppc.PPC_OP_REG for o in ops):
                    a1, a2 = ins.reg_name(ops[1].reg), ins.reg_name(ops[2].reg)
                    if a1 == a2 and a1 in st:
                        prop = (ins.reg_name(ops[0].reg), st[a1])
                elif mn in ("addi", "addic", "addic.") and len(ops) == 3 and \
                        ops[1].type == capstone.ppc.PPC_OP_REG and \
                        ops[2].type == capstone.ppc.PPC_OP_IMM:
                    src = ins.reg_name(ops[1].reg)
                    if src in st:
                        a, off = st[src]
                        prop = (ins.reg_name(ops[0].reg), (a, off + ops[2].imm))
                for x in w:
                    st.pop(x, None)
                if prop and prop[0]:
                    st[prop[0]] = prop[1]
            return st

        state_in = {ld: None for ld in leaders}
        state_in[start] = entry_state
        work = collections.deque([start])
        guard = 0
        while work:
            guard += 1
            if guard > 40 * (len(leaders) + 4):
                raise Bail("taint did not converge")
            ld = work.popleft()
            out = transfer(ld, state_in[ld])
            for s in succ[ld]:
                cur = state_in[s]
                if cur is None:
                    state_in[s] = dict(out)
                    work.append(s)
                else:
                    # MUST-merge: keep only agreeing entries.
                    new = {k: v for k, v in cur.items()
                           if k in out and out[k] == v}
                    if new != cur:
                        state_in[s] = new
                        work.append(s)
        return roles, flags


# ---------------------------------------------------------------------------
# 4. The tests
# ---------------------------------------------------------------------------

NARROW_OK = {"bool": 1, "float": 4, "double": 8}


def apply_tests(decl, roles, fprs):
    """-> list of (test, severity, detail) findings."""
    out = []
    if decl.excl:
        return out

    # ---- T_DEREF: a by-value integral scalar used as a memory base ----------
    for slot in decl.scalar_slots():
        reg = decl.reg_of(slot)
        if reg is None:
            continue
        rl = roles.get(reg)
        if rl and rl.base:
            w, kind, disp, addr, txt = rl.base[0]
            out.append(("T_DEREF", decl.slot_ord.get(slot), "hard", {
                "slot": slot, "reg": reg, "declared": decl.slot_text.get(slot),
                "witness_addr": f"{addr:#010x}", "witness": txt,
                "disp": disp, "n_derefs": len(rl.base),
            }))

    # ---- T_FPR: reads a float argument the name does not declare -----------
    if fprs:
        hi = max(fprs)
        if hi > decl.fp_count:
            out.append(("T_FPR", -2, "hard", {
                "declared_fp_params": decl.fp_count,
                "observed_fprs": [f"f{x}" for x in fprs],
                "highest_legit": f"f{decl.fp_count}" if decl.fp_count else "(none)",
            }))

    # ---- T_PTRW (ADVISORY): pointee width contradiction --------------------
    for slot, cat in decl.slot_cat.items():
        if cat != "ptr":
            continue
        pc = pointee_class(decl.slot_text.get(slot, ""))
        if pc == "other":
            continue
        reg = decl.reg_of(slot)
        rl = roles.get(reg)
        if not rl or not rl.base:
            continue
        for w, kind, disp, addr, txt in rl.base:
            bad = False
            if pc == "bool" and disp == 0 and (w > 1 or kind == "fp"):
                bad = True
            if pc in ("float", "double") and disp == 0 and w < 4:
                bad = True
            if bad:
                out.append(("T_PTRW", decl.slot_ord.get(slot), "advisory", {
                    "slot": slot, "reg": reg,
                    "declared": decl.slot_text.get(slot), "pointee": pc,
                    "witness_addr": f"{addr:#010x}", "witness": txt,
                    "access_width": w, "access_kind": kind,
                }))
                break
    return out


# ---------------------------------------------------------------------------
# 5. Runner
# ---------------------------------------------------------------------------

def build_decl_pair(mangled, demangled):
    """-> (primary, alternate|None).

    `alternate` exists only when an aggregate return makes a hidden sret
    pointer possible.  A finding must hold under BOTH to be reported.
    """
    a = build_typed_decl(mangled, demangled, assume_sret=False)
    b = None
    if not a.excl and a.sret_ambiguous:
        b = build_typed_decl(mangled, demangled, assume_sret=True)
        if b.excl:
            b = None
    return a, b


class TypeScreen:
    def __init__(self, retail=None):
        self.retail = retail
        self.an = TaintAnalyzer(retail=retail)
        self.stats = collections.Counter()

    def run_one(self, mangled, pair, code, start, end, preserve_offsets=None):
        decl, alt = pair if isinstance(pair, tuple) else (pair, None)
        if decl.excl:
            self.stats["excl:" + str(decl.excl)[:40]] += 1
            return []
        argregs = {decl.reg_of(s) for s in decl.slot_cat if decl.reg_of(s)}
        if alt:
            argregs |= {alt.reg_of(s) for s in alt.slot_cat if alt.reg_of(s)}
        argregs = sorted(r for r in argregs if r)
        self.an.preserve_at = preserve_offsets or set()
        try:
            roles, flags = self.an.entry_roles(code, start, end, argregs)
            _g, fprs, _f, _c, _i = self.an.consumed(code, start, end)
        except Bail as e:
            self.stats["bail:" + str(e)[:28]] += 1
            return []
        except Exception as e:
            self.stats["error:" + type(e).__name__] += 1
            return []
        self.stats["analysed"] += 1

        # checkable populations, reported separately per test
        if decl.scalar_slots():
            self.stats["checkable_T_DEREF"] += 1
        self.stats["checkable_T_FPR"] += 1
        if fprs:
            self.stats["checkable_T_FPR_active"] += 1
        if any(pointee_class(decl.slot_text.get(s, "")) != "other"
               for s, c in decl.slot_cat.items() if c == "ptr"):
            self.stats["checkable_T_PTRW"] += 1

        # SELF-VALIDATION ANCHOR: for a member function `this` is r3 and is
        # almost always dereferenced.  If this rate collapses, the slot->reg
        # mapping or the taint analysis is broken -- NOT the map.
        if decl.has_this and 1 in decl.slot_cat and decl.slot_cat[1] == "this":
            self.stats["member_fns"] += 1
            if roles.get("r3") and roles["r3"].base:
                self.stats["member_this_derefd"] += 1

        found = apply_tests(decl, roles, fprs)
        if alt is not None:
            # Sound resolution of the sret ambiguity: keep only findings that
            # are impossible under BOTH candidate slot maps, matched on the
            # PARAMETER ORDINAL (the slot index differs between hypotheses).
            alt_keys = {(t, o) for t, o, _s, _d in apply_tests(alt, roles, fprs)}
            before = len(found)
            found = [f for f in found if (f[0], f[1]) in alt_keys]
            if before != len(found):
                self.stats["sret_ambiguity_suppressed"] += before - len(found)
        rows = []
        for test, _ordinal, sev, detail in found:
            self.stats[test] += 1
            rows.append({
                "test": test, "severity": sev, "symbol": mangled,
                "demangled": decl.demangled, "addr": f"{start:#010x}",
                "size": end - start, "slots": decl.slots,
                "slot_cat": {str(k): v for k, v in decl.slot_cat.items()},
                "flags": sorted(flags), "detail": detail,
            })
        return rows


def load_map(honour_denylist=True):
    """Read the map as the RENAMER sees it.

    Two filters that are easy to forget and both produce wrong output:
      * a value may be null or a list, not a name.  arity_screen omitted this
        check and died outright on the 27 nulled rows.
      * _denylist rows are NOT emitted by scripts/obj_target_symbol_renamer.py,
        so screening them re-reports names that are already retired -- including
        ones this very screen retired.
    """
    m = json.load(open(MAP_PATH))
    meta = {k: v for k, v in m.items() if not k.startswith("0x")}
    denied = set(meta.get("_denylist", [])) if honour_denylist else set()
    named = {k: v for k, v in m.items()
             if k.startswith("0x") and isinstance(v, str) and k not in denied}
    return m, meta, named


def load_subject(retail, named):
    out = []
    for k, v in named.items():
        va = int(k, 16)
        if not retail.in_text(va):
            continue
        sz = retail.extent(va)
        if not sz or sz < 4 or sz > 0x20000:
            continue
        out.append((va, v, sz))
    out.sort()
    return out


def run_retail(subject, shuffle_seed=None):
    retail = Retail()
    names = [n for _, n, _ in subject]
    dm = demangle_batch(sorted(set(names)))
    decls = {n: build_decl_pair(n, dm.get(n)) for n in set(names)}
    if shuffle_seed is not None:
        rnd = random.Random(shuffle_seed)
        sh = names[:]
        rnd.shuffle(sh)
        subject = [(va, sh[i], sz) for i, (va, _n, sz) in enumerate(subject)]
    sc = TypeScreen(retail=retail)
    rows = []
    for va, name, sz in subject:
        code = retail.read(va, sz)
        if len(code) != sz:
            sc.stats["bail:short-read"] += 1
            continue
        rows.extend(sc.run_one(name, decls[name], code, va, va + sz))
    return sc.stats, rows


def run_objs(limit=None, shuffle_seed=None):
    md = capstone.Cs(capstone.CS_ARCH_PPC,
                     capstone.CS_MODE_32 | capstone.CS_MODE_BIG_ENDIAN)
    md.detail = True
    objs = sorted(OBJ_ROOT.rglob("*.obj"))
    if limit:
        objs = objs[:limit]
    pending = []
    for p in objs:
        try:
            o = CoffObj(p)
        except Exception:
            continue
        for name, si, s, e in o.functions():
            pending.append((p, o, name, si, s, e))
    allnames = sorted({x[2] for x in pending})
    dm = demangle_batch(allnames)
    decls = {n: build_decl_pair(n, dm.get(n)) for n in allnames}
    if shuffle_seed is not None:
        rnd = random.Random(shuffle_seed)
        sh = [x[2] for x in pending]
        rnd.shuffle(sh)
        pending = [(p, o, sh[i], si, s, e)
                   for i, (p, o, _n, si, s, e) in enumerate(pending)]
    sc = TypeScreen(retail=None)
    rows, cache = [], {}
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
        for row in r:
            row["unit"] = str(p.relative_to(OBJ_ROOT))
        rows.extend(r)
    return sc.stats, rows


def summarise(title, stats, rows, top=0, jsonpath=None):
    an = stats["analysed"]
    print(f"\n===== {title} =====")
    print(f"  analysed                 : {an}")
    for t, ck in (("T_DEREF", "checkable_T_DEREF"),
                  ("T_FPR", "checkable_T_FPR_active"),
                  ("T_PTRW", "checkable_T_PTRW")):
        c, f = stats[ck], stats[t]
        pct = f"{f / c * 100:.4f}%" if c else "n/a"
        print(f"  {t:8s} checkable={c:<7d} fired={f:<5d}  rate={pct}")
    mf, md_ = stats["member_fns"], stats["member_this_derefd"]
    if mf:
        print(f"  ANCHOR this-deref rate   : {md_}/{mf} = {md_/mf*100:.2f}%"
              "   (must be high; a collapse means the taint model is broken)")
    print("  -- top exclusions / bails:")
    for k, c in stats.most_common():
        if k.startswith(("excl:", "bail:", "error:")):
            print(f"       {k:46s} {c}")
    if top and rows:
        print(f"  -- {min(top, len(rows))} flagged rows:")
        for r in rows[:top]:
            print(f"    [{r['test']}] {r['addr']} sz={r['size']:<6d} {r['symbol']}")
            print(f"        {r['demangled']}")
            print(f"        {r['detail']}")
    if jsonpath:
        json.dump({"stats": dict(stats), "rows": rows},
                  open(jsonpath, "w"), indent=1)
        print(f"  -> {jsonpath}")
    return stats, rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("cmd", choices=["screen", "control-pos", "control-null",
                                    "strata"])
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--top", type=int, default=0)
    ap.add_argument("--json")
    ap.add_argument("--shuffle", action="store_true")
    ap.add_argument("--trials", type=int, default=1)
    ap.add_argument("--seed", type=int, default=1234)
    ap.add_argument("--report", default="build/45410914/report.json")
    a = ap.parse_args()

    if a.cmd == "control-pos":
        seed = a.seed if a.shuffle else None
        st, rows = run_objs(limit=a.limit or None, shuffle_seed=seed)
        title = ("POSITIVE CONTROL, NAMES SHUFFLED (proves it can fire)"
                 if a.shuffle else
                 "POSITIVE CONTROL: our own .obj -- every flag is a FALSE POSITIVE")
        summarise(title, st, rows, a.top, a.json)
        return

    _m, meta, named = load_map()
    retail = Retail()
    subj = load_subject(retail, named)
    if a.limit:
        subj = subj[:a.limit]

    if a.cmd == "screen":
        st, rows = run_retail(subj)
        rows.sort(key=lambda r: (r["test"], -r["size"]))
        summarise("REAL RUN (retail bodies, map names)", st, rows, a.top, a.json)
        return

    if a.cmd == "control-null":
        for t in range(a.trials):
            st, rows = run_retail(subj, shuffle_seed=a.seed + t)
            summarise(f"PERMUTATION NULL trial {t + 1} (pairing randomised)",
                      st, rows, 0, None)
        return

    if a.cmd == "strata":
        arb = set(meta.get("_bijection_arbitrary", [])) | \
            set(meta.get("_icf_arbitrary", []))
        trusted_names = set()
        try:
            rep = json.load(open(a.report))
            for u in rep.get("units", []):
                for fn in u.get("functions", []):
                    if float(fn.get("fuzzy_match_percent", 0) or 0) == 100.0:
                        nm = fn.get("name")
                        if nm:
                            trusted_names.add(nm)
        except Exception as e:
            print(f"!! could not read {a.report}: {e}")
            return
        print(f"  fuzzy==100 names in report : {len(trusted_names)}")
        T = [(va, n, sz) for va, n, sz in subj
             if n in trusted_names and f"{va:#010x}" not in arb]
        U = [(va, n, sz) for va, n, sz in subj
             if not (n in trusted_names and f"{va:#010x}" not in arb)]
        print(f"  TRUSTED stratum            : {len(T)}")
        print(f"  UNTRUSTED stratum          : {len(U)}")
        stT, rT = run_retail(T)
        summarise("TRUSTED stratum (byte-corroborated names)", stT, rT, a.top)
        stU, rU = run_retail(U)
        summarise("UNTRUSTED stratum", stU, rU, a.top, a.json)
        # can-it-fire demonstration on the trusted stratum itself
        stS, rS = run_retail(T, shuffle_seed=a.seed)
        summarise("TRUSTED stratum, NAMES SHUFFLED (proves the control can fire)",
                  stS, rS, 0)
        print("\n  ---- ENRICHMENT ----")
        for t, ck in (("T_DEREF", "checkable_T_DEREF"),
                      ("T_FPR", "checkable_T_FPR_active")):
            rt = stT[t] / stT[ck] if stT[ck] else 0
            ru = stU[t] / stU[ck] if stU[ck] else 0
            rs = stS[t] / stS[ck] if stS[ck] else 0
            print(f"  {t:8s} trusted={rt*100:.4f}%  untrusted={ru*100:.4f}%  "
                  f"shuffled-trusted={rs*100:.4f}%  "
                  f"enrichment(U/T)={'inf' if rt == 0 and ru else f'{ru/rt:.2f}x' if rt else 'n/a'}")
        return


if __name__ == "__main__":
    main()
