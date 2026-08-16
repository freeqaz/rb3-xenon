#!/usr/bin/env python3
"""Compare stack-frame layouts between target and base compilations (MSVC X360).

For a given function, walk the objdiff target+base instruction stream and
build per-offset "slot fingerprints" (opcode family, inferred size, access
count, instruction-index span). Diff the two fingerprint maps to identify:

  MATCH      -- same offset, same fingerprint
  SHIFTED    -- same fingerprint, offset differs by the dominant frame Δ
  SWAPPED    -- two slots' fingerprints exchanged (declaration reorder lever)
  DIFFER     -- same offset, different fingerprint (unresolved)
  TGT_ONLY   -- offset present only on target side
  BASE_ONLY  -- offset present only on our build

Also parses the prologue to report frame size and callee-saved register counts;
if the frame delta is fully explained by callee-saved counts, flags AT_LIMIT.

Toolchain: MSVC PPC (Xbox 360 / Xenon). Prologue convention:
  mflr r12
  [stw r12, -8(r1); std rN, -off(r1); stfd fN, -off(r1)]  OR
  bl __savegprlr_NN / bl __savefpr_NN
  stwu r1, -FRAMESIZE(r1)

Usage:
    python3 scripts/analysis/stack_layout.py --symbol "Class::Method(...)" [-u UNIT]
"""

import argparse
import json
import os
import re
import sys
from collections import Counter, defaultdict
from dataclasses import dataclass, field
from typing import Optional


# Lazy import: codeview_locals depends on a debug recompile + CodeView parser.
# If unavailable, we degrade to no-names mode without crashing.
def _try_extract_locals(symbol, project_dir):
    try:
        sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
        import codeview_locals  # type: ignore
    except ImportError as exc:
        print(f"  [stack_layout] codeview_locals unavailable: {exc}", file=sys.stderr)
        return {}
    try:
        return codeview_locals.extract_locals(symbol, project_dir)
    except Exception as exc:
        print(f"  [stack_layout] name extraction failed: {exc}", file=sys.stderr)
        return {}


# ── Opcode classification (PowerPC, including Xenon 64-bit) ──────────────────

LOAD_OPCODES = {
    "lwz": ("int", 4), "lwzu": ("int", 4), "lwzx": ("int", 4),
    "lha": ("int", 2), "lhau": ("int", 2),
    "lhz": ("int", 2), "lhzu": ("int", 2),
    "lbz": ("int", 1), "lbzu": ("int", 1),
    "lmw": ("int", 4),
    "ld": ("int", 8),   # Xenon 64-bit
    "lfs": ("float", 4), "lfsu": ("float", 4),
    "lfd": ("float", 8), "lfdu": ("float", 8),
    "lvx": ("vec", 16), "lvxl": ("vec", 16),       # VMX128
    "lvewx": ("vec", 4),
}

STORE_OPCODES = {
    "stw": ("int", 4), "stwu": ("int", 4),
    "sth": ("int", 2), "sthu": ("int", 2),
    "stb": ("int", 1), "stbu": ("int", 1),
    "stmw": ("int", 4),
    "std": ("int", 8),  # Xenon 64-bit
    "stfs": ("float", 4), "stfsu": ("float", 4),
    "stfd": ("float", 8), "stfdu": ("float", 8),
    "stvx": ("vec", 16), "stvxl": ("vec", 16),     # VMX128
    "stvewx": ("vec", 4),
}

ADDR_OPCODES = {"addi", "addic", "addic."}

LS_OPCODES = {**LOAD_OPCODES, **STORE_OPCODES}


# MSVC X360 commonly aliases r31 to (new r1) via `subi r31, r1, FRAMESIZE`
# before stwu — body code may address locals via either r1 or r31.
FRAME_BASE_REGS = {"r1", "r31"}


# ── Operand splitting: objdiff renders d-form TWO ways ───────────────────────
#
# ★ 2026-08-16. objdiff-cli's `args` field is a DISPLAY string and its spelling
#   is not stable across objdiff revisions. Two spellings exist in the wild for
#   the same instruction:
#
#       stwu r1, -0x80, r1      (flat; objdiff <= 4.2.2, "ruler H" and earlier)
#       stwu r1, -0x80(r1)      (parenthesised d-form; objdiff 4.2.3 fdc5113+)
#
#   Every parser in this module used to do `args.split(",")` and gate on
#   `len(parts) >= 3`. Under the paren spelling a d-form instruction yields TWO
#   comma-parts, so every gate failed closed and the module reported a confident
#   `(0, "frameless/leaf")` for real 0x80 / 0x70 / 0x60 frames. MEASURED on 7
#   real functions (ARGS_READER_AUDIT, task #96): parse_prologue frame sizes read
#   0/0/0/272/0/0/160 where the true sizes are 128/112/96/272/112/0/160, and
#   parse_stack_ref found 18 frame-slot references where the flat spelling finds
#   49 (-63%). This is rendered inline in every MCP `run_objdiff` response.
#
#   `split_operands` accepts BOTH spellings, so the rest of the module keeps its
#   canonical flat operand list (`rD, imm, rBase`) and none of the `len(parts)`
#   gates had to move. It works on typed_args-free rows too, which is what keeps
#   the in-memory selftest fixtures usable.
#
#   Deliberately NOT applied to `_scan_frame_size_v1`, which is a frozen
#   wrong-on-purpose control — see its docstring.
#
#   The *structural* guard against the NEXT unmodelled spelling is not this
#   regex: it is `_scan_frame_size` setting `saw_alloc_form` from the OPCODE
#   before decoding any operand, so an allocation we cannot read reports UNKNOWN
#   rather than a confident zero. See the note there.
#
# The `[^()]+` guard keeps this from mangling a symbol that itself contains
# parentheses; a relocated d-form like `sym@l(r30)` splits to `['sym@l','r30']`,
# which is exactly the pre-4.2.3 flat spelling and is correctly rejected as a
# frame reference by `int(..., 0)` further down.
_DFORM_RE = re.compile(r"^([^()]+)\(\s*(r\d+)\s*\)$")


def split_operands(args: str) -> list:
    """Split an objdiff `args` display string into a flat operand list.

    Accepts both the flat (`0x374, r1`) and parenthesised (`0x374(r1)`)
    d-form spellings; the returned list is always the flat form.

        >>> split_operands("r0, 0x374(r1)")
        ['r0', '0x374', 'r1']
        >>> split_operands("r0, 0x374, r1")
        ['r0', '0x374', 'r1']
    """
    out = []
    for part in (args or "").split(","):
        part = part.strip()
        m = _DFORM_RE.match(part)
        if m:
            out.append(m.group(1).strip())
            out.append(m.group(2))
        else:
            out.append(part)
    return out


def frame_base_regs(instrs: list, side_key: str) -> set:
    """Which registers actually address the FRAME in this function.

    ★ r31 is a frame base ONLY when the prologue derives it from r1. Lane DP-2
    measured that across decidable retail functions r31 is the frame base just
    ~55% of the time -- the other ~45% it holds an incoming object pointer, and
    `lwz r3, 0x50, r31` is then a CLASS MEMBER load, not a stack slot.

    v1 hardcoded {"r1", "r31"} unconditionally. Measured consequence over 519
    diffed functions: 140 of them (27%) have r31 as an object pointer, and 1,586
    member accesses were being tabulated as stack slots -- i.e. the stack-layout
    table was silently mixing class layout into a stack report, which is exactly
    the frame-vs-object confusion DP-2's refutation was about.

    Same 20-instruction prologue window and same derivation rule as
    tools/r31_role_census.py, which hard-asserts 4 known positives.
    """
    for ins in instrs[:20]:
        side = ins.get(side_key)
        if not side:
            continue
        op = side.get("opcode", "")
        parts = split_operands(side.get("args", ""))
        if not parts or parts[0] != "r31":
            continue
        if op == "subf":                       # subf r31, rA, rB => rB - rA
            return {"r1", "r31"} if parts[-1] == "r1" else {"r1"}
        if op in ("li", "lis", "lwz"):         # constant / loaded pointer
            return {"r1"}
        if op in ("subi", "addi", "add", "mr", "or"):
            return {"r1", "r31"} if "r1" in parts[1:] else {"r1"}
    return {"r1"}                              # r31 never established => not a frame base


def parse_stack_ref(opcode: str, args: str,
                    frame_regs: Optional[set] = None) -> Optional[tuple[int, str, int]]:
    """Return (offset, kind, size) if this instruction references the frame
    base (r1 or r31), else None.

    objdiff arg format examples — BOTH spellings are accepted (see
    `split_operands`; objdiff <= 4.2.2 emits the flat form, 4.2.3+ the paren
    form, and this parser must not care which):

      stw  r0, 0x374, r1  / stw  r0, 0x374(r1)   -> (0x374, 'int',   4)
      stfd f31, 0x360, r1 / stfd f31, 0x360(r1)  -> (0x360, 'float', 8)
      std  r29, 0x88, r1  / std  r29, 0x88(r1)   -> (0x88,  'int',   8)  (Xenon 64-bit)
      stw  r11, 0x58, r31 / stw  r11, 0x58(r31)  -> (0x58,  'int',   4)  (MSVC frame-ptr alias)
      addi r11, r1, 0x270                        -> filtered (r1/r11/r31 frame arithmetic)
      lwz  r3, sym@l(r30)                        -> None (relocated global, not a frame ref)
    """
    parts = split_operands(args)
    if len(parts) < 3:
        return None

    if opcode in LS_OPCODES:
        try:
            offset = int(parts[1], 0)
        except ValueError:
            return None
        base = parts[2]
        kind, size = LS_OPCODES[opcode]
    elif opcode in ADDR_OPCODES:
        dest = parts[0]
        if dest in ("r1", "r11", "r12", "r31"):  # frame-pointer reg families
            return None
        base = parts[1]
        try:
            offset = int(parts[2], 0)
        except ValueError:
            return None
        kind, size = "addr", 0
    else:
        return None

    if base not in (FRAME_BASE_REGS if frame_regs is None else frame_regs):
        return None
    if offset < 0:
        return None  # frame-setup negative offsets, not a body-slot reference
    return offset, kind, size


# ── Slot fingerprint ─────────────────────────────────────────────────────────

@dataclass
class SlotFingerprint:
    offset: int
    accesses: int = 0
    loads: int = 0
    stores: int = 0
    addrs: int = 0
    kinds: Counter = field(default_factory=Counter)
    sizes: Counter = field(default_factory=Counter)
    indices: list = field(default_factory=list)
    opcodes: Counter = field(default_factory=Counter)

    @property
    def first_idx(self) -> int:
        return min(self.indices) if self.indices else -1

    @property
    def last_idx(self) -> int:
        return max(self.indices) if self.indices else -1

    @property
    def inferred_size(self) -> int:
        if self.sizes:
            return self.sizes.most_common(1)[0][0]
        return 0

    @property
    def dominant_kind(self) -> str:
        if self.kinds:
            return self.kinds.most_common(1)[0][0]
        return "?"

    def fingerprint(self) -> tuple:
        return (self.dominant_kind, self.inferred_size, self.loads, self.stores)

    def index_set(self) -> frozenset:
        """Aligned-row indices at which this slot is touched.

        ★ THIS is the discriminator v1 threw away. objdiff aligns target and
        base into ONE row stream, so row N names the same program point on both
        sides. Two slots at the same offset holding the SAME variable are
        touched at the SAME rows; a permutation of variables across a set of
        identically-shaped slots keeps every fingerprint equal but moves the
        rows. v1's fingerprint() is (kind,size,loads,stores) only -- which for a
        run of same-typed locals is CONSTANT, so v1 could not tell "same
        variable" from "some other variable that happens to look alike".
        short_repr() has been PRINTING [first..last] the whole time; the verdict
        just never consulted it.
        """
        return frozenset(self.indices)

    def short_repr(self) -> str:
        return (f"{self.dominant_kind:6s} sz={self.inferred_size:<2d} "
                f"L={self.loads:<2d} S={self.stores:<2d} A={self.accesses:<3d} "
                f"[{self.first_idx}..{self.last_idx}]")

    def opcodes_repr(self) -> str:
        return ", ".join(f"{op}×{n}" for op, n in self.opcodes.most_common(3))


def build_fingerprints(side_key: str, instrs: list,
                       frame_regs: Optional[set] = None) -> dict[int, SlotFingerprint]:
    """frame_regs defaults to auto-detection from this side's prologue, so an
    r31 that holds an object pointer no longer contributes phantom stack slots."""
    if frame_regs is None:
        frame_regs = frame_base_regs(instrs, side_key)
    slots: dict[int, SlotFingerprint] = {}
    for ins in instrs:
        idx = ins.get("index", -1)
        side = ins.get(side_key)
        if not side:
            continue
        opcode = side.get("opcode", "")
        args = side.get("args", "")
        ref = parse_stack_ref(opcode, args, frame_regs)
        if not ref:
            continue
        offset, kind, size = ref
        slot = slots.setdefault(offset, SlotFingerprint(offset))
        slot.accesses += 1
        slot.indices.append(idx)
        slot.opcodes[opcode] += 1
        slot.kinds[kind] += 1
        if size:
            slot.sizes[size] += 1
        if opcode in LOAD_OPCODES:
            slot.loads += 1
        elif opcode in STORE_OPCODES:
            slot.stores += 1
        elif opcode in ADDR_OPCODES:
            slot.addrs += 1
    return slots


# ── Prologue analysis (MSVC X360) ────────────────────────────────────────────

@dataclass
class Prologue:
    # ★ TRI-STATE, and the whole point of the 2026-08-03 repair (lane DQ-2).
    #   None  = COULD NOT DETERMINE (an allocation form we do not model)
    #   0     = scanned, positively found NO frame allocation (frameless leaf)
    #   N > 0 = measured frame size
    # v1 used a plain `int = 0`, conflating "unparsed" with "frameless", so an
    # unparsed prologue on BOTH sides printed "→ Frame sizes match." off 0 == 0.
    frame_size: Optional[int] = None
    frame_evidence: str = "not scanned"
    # Same tri-state for the callee-save counts: a bare `bl __savegprlr` used to
    # read 0 against a base-side 18, fabricating a +18 delta out of pure symbol
    # naming (see SAVE_HELPER_BARE_NN).
    saved_gpr_count: Optional[int] = None
    saved_fpr_count: Optional[int] = None
    saved_vmx_count: Optional[int] = None
    callee_save_slots: set = field(default_factory=set)  # offsets, new-r1-relative
    raw_savegprlr: Optional[int] = None
    raw_savefpr: Optional[int] = None
    unparsed_saves: list = field(default_factory=list)

    @property
    def frame_known(self) -> bool:
        return self.frame_size is not None


def _try_int(s: str) -> Optional[int]:
    try:
        return int(s, 0)
    except (ValueError, TypeError):
        return None


def _maxi(a: Optional[int], b: Optional[int]) -> Optional[int]:
    """max() that treats None as 'no value yet' instead of crashing."""
    if a is None:
        return b
    if b is None:
        return a
    return max(a, b)


# A `bl __savegprlr` / `bl __savefpr` with NO _NN suffix is the run's first
# entry point. symbols.txt gives both names to the same address, so bare == _14.
SAVE_HELPER_BARE_NN = 14


# ── Frame-size scanning ──────────────────────────────────────────────────────
#
# MEASURED over 95,222 functions in 8,753 fresh target .s files (lane DQ-2;
# 4,390 stale pre-2026-07-15 generations skipped per CLAUDE.md):
#
#     STWU   79,330  83.31%   stwu  r1, -N, r1            <- v1 handled only this
#     NONE   15,889  16.69%   no frame allocation (leaf)
#     STWUX       3   0.00%   lis/ori rX + stwux r1,r1,rX <- v1 read 0 here
#
# So the *coverage* gap DP-2 filed is 3 functions. The *representation* bug
# (0 == unparsed) is the one that matters, because it is unbounded: any future
# unmodelled form silently reads 0 and prints "frames match".
#
# Independent corroboration channel: MSVC also aliases r31 to the new r1 via
# `subi r31, r1, N` before the stwu. Where both channels exist they agreed
# 11,613 / 11,613 with ZERO disagreements, so a disagreement is a real anomaly
# and is reported rather than silently resolved.

_CONST_LIS = re.compile(r"^(r\d+)$")


def _scan_frame_size_v1(instrs: list, side_key: str, horizon: int) -> int:
    """SUPERSEDED 2026-08-03 (lane DQ-2). Retained ONLY so the selftest can
    assert it STAYS WRONG. Handles `stwu r1, -N, r1` and nothing else, and
    returns a plain 0 -- indistinguishable from a frameless leaf -- otherwise.

    ★ Deliberately NOT given `split_operands` (2026-08-16). This function is a
    frozen control, and its raw `.split(",")` is now doing double duty: on the
    objdiff-4.2.3 paren fixtures it reproduces, in-tree, the exact confident-zero
    that shipped for the whole of 2026-08-16. Do not "fix" it -- the selftest
    asserts it stays 0 there."""
    for ins in instrs[:horizon]:
        side = ins.get(side_key)
        if not side:
            continue
        op = side.get("opcode", "")
        parts = [a.strip() for a in side.get("args", "").split(",")]
        if op == "stwu" and len(parts) >= 3 and parts[0] == "r1" and parts[2] == "r1":
            n = _try_int(parts[1])
            if n is not None and n < 0:
                return -n
    return 0


def _scan_frame_size(instrs: list, side_key: str, horizon: int) -> tuple:
    """-> (size_or_None, evidence_str).

    Preference order: stwu > stwux > r31-alias > positively-frameless(0).
    """
    consts: dict = {}          # reg -> last materialized constant
    stwu_size = None
    stwux_size = None
    stwux_note = None
    alias_size = None
    alias_note = None
    saw_alloc_form = False

    for ins in instrs[:horizon]:
        side = ins.get(side_key)
        if not side:
            continue
        op = side.get("opcode", "")
        args = side.get("args", "")
        parts = split_operands(args)

        # --- constant materialization: lis / ori / li ------------------------
        if op == "lis" and len(parts) == 2:
            v = _try_int(parts[1])
            if v is not None:
                consts[parts[0]] = (v << 16) & 0xFFFFFFFF
            continue
        if op == "ori" and len(parts) == 3 and parts[0] == parts[1]:
            v = _try_int(parts[2])
            if v is not None and parts[0] in consts:
                consts[parts[0]] |= v & 0xFFFF
            continue
        if op == "li" and len(parts) == 2:
            v = _try_int(parts[1])
            if v is not None:
                consts[parts[0]] = v & 0xFFFFFFFF
            continue

        # --- the allocation forms: stwu / stwux, writing back to r1 ----------
        #
        # ★ 2026-08-16. `saw_alloc_form` is set from the OPCODE + destination,
        #   BEFORE any operand is decoded, and this ordering is the load-bearing
        #   part of the tri-state. It used to be set inside the fully-decoded
        #   gates below (`len(parts) >= 3 and parts[2] == "r1"`). objdiff 4.2.3
        #   re-spelled the d-form as `stwu r1, -0x80(r1)`, those gates stopped
        #   matching, the flag never set, and the function fell through to the
        #   confident-zero `else` — reporting a real 0x80 frame as a frameless
        #   leaf. The module's own note above says this is THE bug that matters
        #   because it is unbounded, and it recurred anyway, so the recognition
        #   is now decoupled from the decode: if we see an allocation we cannot
        #   read, we return UNKNOWN. `split_operands` fixes today's spelling;
        #   this makes the NEXT one loud instead of silent.
        if op in ("stwu", "stwux") and parts and parts[0] == "r1":
            saw_alloc_form = True

        # --- stwu r1, -N, r1  (83.31% of functions) --------------------------
        if op == "stwu" and len(parts) >= 3 and parts[0] == "r1" and parts[2] == "r1":
            n = _try_int(parts[1])
            if n is not None and n < 0 and stwu_size is None:
                stwu_size = -n
            continue

        # --- stwux r1, r1, rX  (big frames; 3 functions in the whole binary) --
        if op == "stwux" and len(parts) >= 3 and parts[0] == "r1" and parts[1] == "r1":
            reg = parts[2]
            raw = consts.get(reg)
            if raw is None:
                stwux_note = f"stwux via {reg} (constant not resolvable)"
            else:
                sv = raw - (1 << 32) if raw >= (1 << 31) else raw
                if sv < 0 and stwux_size is None:
                    stwux_size = -sv
                    stwux_note = f"stwux r1,r1,{reg}={sv:#x}"
                else:
                    stwux_note = f"stwux via {reg}={sv:#x} (not a negative allocation)"
            continue

        # --- r31 frame-base alias (independent corroboration) ----------------
        if alias_size is None and len(parts) >= 3 and parts[0] == "r31":
            # subi r31, r1, N   =>  r31 = r1 - N
            if op == "subi" and parts[1] == "r1":
                n = _try_int(parts[2])
                if n is not None and n > 0:
                    alias_size, alias_note = n, f"subi r31, r1, {n:#x}"
            # addi r31, r1, -N
            elif op == "addi" and parts[1] == "r1":
                n = _try_int(parts[2])
                if n is not None and n < 0:
                    alias_size, alias_note = -n, f"addi r31, r1, {n:#x}"
            # subf r31, rX, r1  =>  r31 = r1 - rX
            elif op == "subf" and parts[2] == "r1":
                raw = consts.get(parts[1])
                if raw is not None:
                    sv = raw - (1 << 32) if raw >= (1 << 31) else raw
                    if sv > 0:
                        alias_size = sv
                        alias_note = f"subf r31, {parts[1]}={sv:#x}, r1"

    primary, evidence = None, None
    if stwu_size is not None:
        primary, evidence = stwu_size, f"stwu r1, -{stwu_size:#x}, r1"
    elif stwux_size is not None:
        primary, evidence = stwux_size, stwux_note
    elif alias_size is not None:
        primary, evidence = alias_size, f"{alias_note} (r31 frame alias)"
    elif saw_alloc_form:
        # We SAW an allocation but could not decode it. Never report 0 here.
        return None, (stwux_note or "frame allocation present but not decodable")
    else:
        return 0, "no frame allocation found in prologue (frameless/leaf)"

    # Consistency control: two independent channels must agree where both exist.
    if (primary is not None and alias_size is not None
            and stwu_size is not None and alias_size != stwu_size):
        evidence += f"  ⚠ r31 alias disagrees ({alias_size:#x})"
    return primary, evidence


def parse_prologue(instrs: list, side_key: str) -> Prologue:
    """Walk the prologue to extract frame size + callee-saved counts.

    MSVC X360 patterns:
      mflr r12                          (LR -> r12)
      [bl __savegprlr_NN]               (helper: r12 -> -8(r1), r31..rNN -> -0x10, -0x18, ...)
      [bl __savefpr_NN]                 (helper: f31..fNN -> -0x10, -0x18, ...)
      [stw r12, -8, r1]                 (manual LR save, 32-bit)
      [std rN, -off, r1]                (manual GPR save, 64-bit pre-stwu)
      [stfd fN, -off, r1]               (manual FPR save, 64-bit pre-stwu)
      [stvx vN, r0, r1]                 (manual VMX save, 128-bit, indexed)
      stwu r1, -FRAMESIZE, r1           (allocate frame)

    After stwu, the saved slots live at:
      LR slot:  frame_size - 8
      GPR/FPR:  frame_size + neg_offset  (where neg_offset is the pre-stwu negative offset)
    """
    p = Prologue()

    # Pass 1: frame size (tri-state; see _scan_frame_size for the measured forms).
    horizon = min(80, len(instrs))
    p.frame_size, p.frame_evidence = _scan_frame_size(instrs, side_key, horizon)

    # Pass 2: classify saves + helpers.
    callee_gprs: set[int] = set()
    callee_fprs: set[int] = set()
    callee_vmx: set[int] = set()
    saw_stwu = False
    seen_stmw_gpr: Optional[int] = None

    for ins in instrs[:horizon]:
        side = ins.get(side_key)
        if not side:
            continue
        op = side.get("opcode", "")
        args = side.get("args", "")
        parts = split_operands(args)

        # bl __save(gpr|fpr|gprlr)[_NN]  (one or two leading underscores)
        #
        # ★ The _NN is OPTIONAL. dtk renders the run's first entry point with the
        #   FUNCTION symbol, which carries no number, while our own build renders
        #   the identical instruction with the LABEL symbol that does:
        #
        #     UIStats::MaybePublish  TGT: bl __savegprlr     BASE: bl __savegprlr_14
        #
        #   config/45410914/symbols.txt settles it as ground truth, not inference:
        #     __savegprlr    = .text:0x82829220; // type:function size:0x50
        #     __savegprlr_14 = .text:0x82829220; // type:label
        #     __savegprlr_15 = .text:0x82829224; // type:label
        #   i.e. bare == _14 (the helpers ascend 4 bytes per entry, saving one
        #   fewer register each). v1's regex REQUIRED the _NN, so 341 bare
        #   __savegprlr + 20 bare __savefpr call sites silently scored 0 saved
        #   registers -- which is exactly the "GPRs TGT 0 / BASE 18" that DP-2
        #   quoted as corroborating evidence for the frame bug. It is a separate
        #   bug, and once fixed both sides read 18.
        if op == "bl":
            m = re.search(r"_+save(gpr|fpr|gprlr|vmx)(?:_(\d+))?\b", args)
            if m:
                kind = m.group(1)
                nn = int(m.group(2)) if m.group(2) else SAVE_HELPER_BARE_NN
                count = 32 - nn
                if kind in ("gpr", "gprlr"):
                    p.raw_savegprlr = nn
                    p.saved_gpr_count = _maxi(p.saved_gpr_count, count)
                    if p.frame_size:
                        if kind == "gprlr":
                            # MSVC X360 __savegprlr_NN layout:
                            #   r12 (=LR) at -8(r1)
                            #   r31 at -0x10, r30 at -0x18, ... in 8-byte slots
                            p.callee_save_slots.add(p.frame_size - 8)
                            for i, reg in enumerate(range(31, nn - 1, -1)):
                                slot = p.frame_size - 0x10 - i * 8
                                if slot >= 0:
                                    p.callee_save_slots.add(slot)
                        else:
                            # __savegpr_NN (no LR): r31..rNN in 8-byte slots
                            for i, reg in enumerate(range(31, nn - 1, -1)):
                                slot = p.frame_size - 8 - i * 8
                                if slot >= 0:
                                    p.callee_save_slots.add(slot)
                elif kind == "fpr":
                    p.raw_savefpr = nn
                    p.saved_fpr_count = _maxi(p.saved_fpr_count, count)
                    if p.frame_size:
                        # __savefpr_NN: f31..fNN in 8-byte slots below the GPR area
                        # (offset depends on whether gprlr was also called); we mark
                        # the relative band so the user-slot filter is conservative.
                        base = p.frame_size - 8 * ((p.saved_gpr_count or 0) + 1)
                        for i, reg in enumerate(range(31, nn - 1, -1)):
                            slot = base - i * 8
                            if slot >= 0:
                                p.callee_save_slots.add(slot)
                elif kind == "vmx":
                    p.saved_vmx_count = _maxi(p.saved_vmx_count, count)
            else:
                # First non-save bl ends the prologue scan window.
                break

        # stwu r1, -N, r1  → mark we've crossed into the post-stwu region.
        if op == "stwu" and len(parts) >= 3 and parts[0] == "r1" and parts[2] == "r1":
            saw_stwu = True
            continue

        # ── Pre-stwu (negative offset, OLD-r1-relative) ──────────────────────
        if not saw_stwu and p.frame_size:
            # stw r12, -8, r1  → LR save (32-bit form on X360)
            if op == "stw" and len(parts) >= 3 and parts[0] == "r12" and parts[2] == "r1":
                off = _try_int(parts[1])
                if off is not None and off < 0:
                    p.callee_save_slots.add(p.frame_size + off)

            # std rN, -off, r1  → 64-bit GPR save
            if op == "std" and len(parts) >= 3 and parts[2] == "r1":
                r = _try_int(parts[0].lstrip("r"))
                off = _try_int(parts[1])
                if r is not None and off is not None and off < 0 and 13 <= r <= 31:
                    callee_gprs.add(r)
                    p.callee_save_slots.add(p.frame_size + off)

            # stfd fN, -off, r1  → 64-bit FPR save
            if op == "stfd" and len(parts) >= 3 and parts[2] == "r1":
                f = _try_int(parts[0].lstrip("f").lstrip("r"))
                off = _try_int(parts[1])
                if f is not None and off is not None and off < 0 and 14 <= f <= 31:
                    callee_fprs.add(f)
                    p.callee_save_slots.add(p.frame_size + off)

        # ── Post-stwu callee-save (rare on X360 but possible) ────────────────
        if saw_stwu:
            # Manual stfd fN, off, r1 (post-stwu, positive offset)
            if op == "stfd" and len(parts) >= 3 and parts[2] == "r1":
                f = _try_int(parts[0].lstrip("f").lstrip("r"))
                off = _try_int(parts[1])
                if (f is not None and off is not None and 14 <= f <= 31
                        and off >= 0 and p.frame_size and off >= p.frame_size - 0x200):
                    callee_fprs.add(f)
                    p.callee_save_slots.add(off)
            # stmw rN, off, r1  (less common in MSVC X360, but legal PPC)
            if op == "stmw" and len(parts) >= 3 and parts[0].startswith("r") and parts[2] == "r1":
                r = _try_int(parts[0][1:])
                off = _try_int(parts[1])
                if r is not None and off is not None:
                    seen_stmw_gpr = max(seen_stmw_gpr or 0, 32 - r)
                    for i, reg in enumerate(range(r, 32)):
                        p.callee_save_slots.add(off + i * 4)

    if seen_stmw_gpr is not None:
        p.saved_gpr_count = _maxi(p.saved_gpr_count, seen_stmw_gpr)
    if callee_fprs:
        p.saved_fpr_count = _maxi(p.saved_fpr_count, len(callee_fprs))
    if callee_gprs:
        p.saved_gpr_count = _maxi(p.saved_gpr_count, len(callee_gprs))

    # We scanned the whole prologue window. If nothing at all was found, that is
    # a positive "no callee saves", not an unknown -- so settle it to 0 here
    # rather than leaving None to propagate as a fake delta.
    if p.saved_gpr_count is None:
        p.saved_gpr_count = 0
    if p.saved_fpr_count is None:
        p.saved_fpr_count = 0
    if p.saved_vmx_count is None:
        p.saved_vmx_count = 0

    return p


# ── Diff classification ─────────────────────────────────────────────────────

@dataclass
class Row:
    tgt_off: Optional[int]
    base_off: Optional[int]
    tgt: Optional[SlotFingerprint]
    base: Optional[SlotFingerprint]
    verdict: str
    note: str = ""
    callee_save: bool = False


def _positional_partner(t: SlotFingerprint,
                        base_slots: dict[int, SlotFingerprint]) -> Optional[int]:
    """Base offset whose access rows overlap this target slot's the most.

    Fingerprint-free, so it works precisely where fingerprints are degenerate.
    Returns None when nothing overlaps at all.
    """
    ts = t.index_set()
    if not ts:
        return None
    best_off, best_n = None, 0
    for off, b in base_slots.items():
        n = len(ts & b.index_set())
        if n > best_n:
            best_off, best_n = off, n
    return best_off


def classify_slots_v1(tgt_slots, base_slots, dominant_delta,
                      tgt_callee_save, base_callee_save) -> list[Row]:
    """SUPERSEDED 2026-08-03 (lane DQ-2). Retained ONLY so the selftest can
    assert it STAYS WRONG -- it reports MATCH on a pure variable permutation.
    Do not call it for analysis."""
    return _classify_impl(tgt_slots, base_slots, dominant_delta,
                          tgt_callee_save, base_callee_save, positional=False)


def classify_slots(tgt_slots: dict[int, SlotFingerprint],
                   base_slots: dict[int, SlotFingerprint],
                   dominant_delta: int,
                   tgt_callee_save: set[int],
                   base_callee_save: set[int]) -> list[Row]:
    return _classify_impl(tgt_slots, base_slots, dominant_delta,
                          tgt_callee_save, base_callee_save, positional=True)


def _classify_impl(tgt_slots: dict[int, SlotFingerprint],
                   base_slots: dict[int, SlotFingerprint],
                   dominant_delta: int,
                   tgt_callee_save: set[int],
                   base_callee_save: set[int],
                   positional: bool = True) -> list[Row]:
    rows: list[Row] = []
    seen_base: set[int] = set()

    def is_cs(t_off: Optional[int], b_off: Optional[int]) -> bool:
        return ((t_off is not None and t_off in tgt_callee_save) or
                (b_off is not None and b_off in base_callee_save))

    # Pass 1: exact-offset pairing.
    for off in sorted(tgt_slots):
        tfp = tgt_slots[off]
        if off in base_slots:
            bfp = base_slots[off]
            note = ""
            if tfp.fingerprint() != bfp.fingerprint():
                verdict = "DIFFER"
            elif not positional:
                verdict = "MATCH"          # ← v1: the vacuous verdict
            elif tfp.index_set() == bfp.index_set():
                verdict = "MATCH"          # same shape AND same program points
            else:
                # Same offset, indistinguishable fingerprints, but the two sides
                # touch the slot at DIFFERENT program points => it does not hold
                # the same variable. Never call this MATCH.
                verdict = "PERMUTED"
                partner = _positional_partner(tfp, base_slots)
                if partner is not None and partner != off:
                    note = f"target's slot ↔ base 0x{partner:x}"
                else:
                    note = "same offset, accesses do not line up"
            rows.append(Row(off, off, tfp, bfp, verdict, note=note,
                            callee_save=is_cs(off, off)))
            seen_base.add(off)
        else:
            rows.append(Row(off, None, tfp, None, "TGT_ONLY", callee_save=is_cs(off, None)))

    for off in sorted(base_slots):
        if off not in seen_base:
            rows.append(Row(None, off, None, base_slots[off], "BASE_ONLY",
                            callee_save=is_cs(None, off)))

    # Pass 2: SWAPPED detection (DIFFER pairs with exchanged fingerprints).
    differ_rows = [r for r in rows if r.verdict == "DIFFER" and not r.callee_save]
    for i, r1 in enumerate(differ_rows):
        if r1.verdict != "DIFFER":
            continue
        for r2 in differ_rows[i + 1:]:
            if r2.verdict != "DIFFER":
                continue
            assert r1.tgt is not None and r1.base is not None
            assert r2.tgt is not None and r2.base is not None
            if (r1.tgt.fingerprint() == r2.base.fingerprint() and
                    r2.tgt.fingerprint() == r1.base.fingerprint()):
                r1.verdict = "SWAPPED"
                r1.note = f"with 0x{r2.tgt_off:x}"
                r2.verdict = "SWAPPED"
                r2.note = f"with 0x{r1.tgt_off:x}"
                break

    # Pass 3: SHIFTED detection (fingerprint-matched TGT_ONLY+BASE_ONLY pairs).
    tgt_only = [r for r in rows if r.verdict == "TGT_ONLY" and not r.callee_save]
    base_only = [r for r in rows if r.verdict == "BASE_ONLY" and not r.callee_save]
    used_base_idxs: set[int] = set()

    for tr in tgt_only:
        assert tr.tgt is not None and tr.tgt_off is not None
        best = None
        for j, br in enumerate(base_only):
            if j in used_base_idxs:
                continue
            assert br.base is not None and br.base_off is not None
            if tr.tgt.fingerprint() != br.base.fingerprint():
                continue
            delta = br.base_off - tr.tgt_off
            score = (0 if delta == dominant_delta else abs(delta - dominant_delta))
            if best is None or score < best[0]:
                best = (score, j, br, delta)
        if best is not None:
            _, j, br, delta = best
            tr.verdict = "SHIFTED"
            tr.base_off = br.base_off
            tr.base = br.base
            tr.note = (f"Δ{delta:+#x} (dominant)" if delta == dominant_delta
                       else f"Δ{delta:+#x}")
            # ★ Ambiguity disclosure: this pairing is by FINGERPRINT, so when
            # several base-only slots share it the choice is arbitrary. Say so
            # rather than presenting an arbitrary pick as a finding.
            rivals = sum(1 for k, b2 in enumerate(base_only)
                         if k not in used_base_idxs and b2.base is not None
                         and b2.base.fingerprint() == tr.tgt.fingerprint())
            if rivals > 1:
                tr.note += f"  ⚠ ambiguous: {rivals} equal-fingerprint candidates"
            used_base_idxs.add(j)

    base_only_drop = {id(base_only[j]) for j in used_base_idxs}
    rows = [r for r in rows if id(r) not in base_only_drop]

    return rows


# ── Printing ─────────────────────────────────────────────────────────────────

VERDICT_ORDER = ["SWAPPED", "DIFFER", "PERMUTED", "SHIFTED",
                 "TGT_ONLY", "BASE_ONLY", "MATCH"]


def print_report(rows: list[Row], tgt_prol: Prologue, base_prol: Prologue,
                 show_equal: bool, show_callee_save: bool,
                 dominant_delta: int, base_names: dict | None = None) -> bool:
    """Print the report. Returns True if the frame comparison is TRUSTWORTHY,
    False if it had to refuse (caller turns that into a nonzero exit)."""
    print("=" * 84)
    print("STACK LAYOUT DIFF")
    print("=" * 84)
    print()

    frame_ok = tgt_prol.frame_known and base_prol.frame_known
    gpr_delta = base_prol.saved_gpr_count - tgt_prol.saved_gpr_count
    fpr_delta = base_prol.saved_fpr_count - tgt_prol.saved_fpr_count

    def fs(p):
        return f"0x{p.frame_size:x}" if p.frame_known else "UNKNOWN"

    if frame_ok:
        frame_delta = base_prol.frame_size - tgt_prol.frame_size
        print(f"  Frame size:          TGT {fs(tgt_prol):<9s} "
              f"BASE {fs(base_prol):<9s} Δ {frame_delta:+#x}")
    else:
        frame_delta = None
        print(f"  Frame size:          TGT {fs(tgt_prol):<9s} "
              f"BASE {fs(base_prol):<9s} Δ UNKNOWN")
    print(f"  Callee-saved GPRs:   TGT {tgt_prol.saved_gpr_count:<5d}   "
          f"BASE {base_prol.saved_gpr_count:<5d}   Δ {gpr_delta:+d}")
    print(f"  Callee-saved FPRs:   TGT {tgt_prol.saved_fpr_count:<5d}   "
          f"BASE {base_prol.saved_fpr_count:<5d}   Δ {fpr_delta:+d}")
    print(f"    frame evidence: TGT {tgt_prol.frame_evidence}")
    print(f"                    BASE {base_prol.frame_evidence}")

    # Xenon GPRs are 64-bit (std), so each GPR slot = 8 bytes
    callee_bytes = gpr_delta * 8 + fpr_delta * 8
    if not frame_ok:
        # ★ REFUSE. v1 defaulted an unparsed frame to 0 and then printed
        #   "→ Frame sizes match." off 0 == 0 -- a vacuous success. A frame we
        #   could not read is NOT a frame that matches.
        print()
        print("  ⛔ REFUSED: frame size could not be determined on "
              + ("both sides." if not (tgt_prol.frame_known or base_prol.frame_known)
                 else ("the TARGET side." if not tgt_prol.frame_known
                       else "the BASE side.")))
        print("     No frame verdict is reported. Note the callee-save slot filter")
        print("     is derived from the frame size, so the table below is NOT")
        print("     filtered for prologue slots and may contain them.")
    elif frame_delta == 0:
        print("  → Frame sizes match.")
    elif callee_bytes == frame_delta:
        print(f"  → Frame Δ fully explained by callee-saved counts ({gpr_delta} GPR + "
              f"{fpr_delta} FPR = {callee_bytes:+#x} bytes). AT_LIMIT (not source-fixable).")
    else:
        leftover = frame_delta - callee_bytes
        print(f"  → Callee-saved Δ = {callee_bytes:+#x}; structural Δ remaining = {leftover:+#x}.")
    if dominant_delta:
        print(f"  Dominant body-offset shift: {dominant_delta:+#x}")
    print()

    rows_sorted = sorted(rows, key=lambda r: (
        VERDICT_ORDER.index(r.verdict) if r.verdict in VERDICT_ORDER else 99,
        r.tgt_off if r.tgt_off is not None else r.base_off or 0,
    ))
    rows_visible = list(rows_sorted)
    if not show_equal:
        rows_visible = [r for r in rows_visible if r.verdict != "MATCH"]
    if not show_callee_save:
        rows_visible = [r for r in rows_visible if not r.callee_save]

    name_col = bool(base_names)
    if name_col:
        print(f"  {'TGT':>6s}  {'BASE':>6s}  {'verdict':9s}  "
              f"{'target slot':36s}  {'base slot':36s}  {'base var':20s}  note")
        print(f"  {'-' * 6}  {'-' * 6}  {'-' * 9}  {'-' * 36}  {'-' * 36}  {'-' * 20}  ----")
    else:
        print(f"  {'TGT':>6s}  {'BASE':>6s}  {'verdict':9s}  "
              f"{'target slot':36s}  {'base slot':36s}  note")
        print(f"  {'-' * 6}  {'-' * 6}  {'-' * 9}  {'-' * 36}  {'-' * 36}  ----")

    def name_for(off):
        if base_names is None or off is None:
            return ""
        info = base_names.get(off)
        return info.name if info else ""

    for r in rows_visible:
        t_off = f"0x{r.tgt_off:x}" if r.tgt_off is not None else "—"
        b_off = f"0x{r.base_off:x}" if r.base_off is not None else "—"
        t_fp = r.tgt.short_repr() if r.tgt else "—"
        b_fp = r.base.short_repr() if r.base else "—"
        tag = " [CS]" if r.callee_save else ""
        note = (r.note + tag).strip()
        if name_col:
            name = name_for(r.base_off) or ""
            print(f"  {t_off:>6s}  {b_off:>6s}  {r.verdict:9s}  {t_fp:36s}  {b_fp:36s}  {name:20s}  {note}")
        else:
            print(f"  {t_off:>6s}  {b_off:>6s}  {r.verdict:9s}  {t_fp:36s}  {b_fp:36s}  {note}")

    user_rows = [r for r in rows if not r.callee_save]
    cs_rows = [r for r in rows if r.callee_save]
    counts = Counter(r.verdict for r in user_rows)
    cs_counts = Counter(r.verdict for r in cs_rows)
    print()
    print("  Summary (user slots):")
    for v in VERDICT_ORDER:
        if counts[v]:
            print(f"    {v:10s} {counts[v]}")

    # ★ Discriminating power of the per-slot signature. If many user slots share
    #   one fingerprint, any fingerprint-based pairing among them is arbitrary,
    #   and a bare "MATCH n" would be reporting a coincidence as a finding.
    fp_counts = Counter()
    for r in rows:
        if r.callee_save:
            continue
        if r.tgt is not None:
            fp_counts[r.tgt.fingerprint()] += 1
    degenerate = {f: n for f, n in fp_counts.items() if n > 1}
    if degenerate:
        worst = max(degenerate.values())
        n_amb = sum(degenerate.values())
        print(f"    ── signature discriminating power: {n_amb} of "
              f"{sum(fp_counts.values())} target slots share a fingerprint with "
              f"another (largest group {worst}).")
        print("       Fingerprint-only pairing is ARBITRARY within those groups; "
              "MATCH/PERMUTED\n       above is decided by aligned access rows, "
              "not by fingerprint.")
    if cs_rows:
        cs_non_match = sum(c for v, c in cs_counts.items() if v != "MATCH")
        if cs_non_match:
            print(f"  Callee-save slots: {cs_non_match} non-matching (filtered; use --show-callee-save to inspect)")

    print()
    print("  Action hints:")
    if counts["SWAPPED"]:
        swap_rows = [r for r in user_rows if r.verdict == "SWAPPED"]
        seen_swap_pairs = set()
        pair_lines = []
        for r in swap_rows:
            if r.tgt_off is None:
                continue
            m = re.search(r"0x([0-9a-fA-F]+)", r.note)
            if not m:
                continue
            other = int(m.group(1), 16)
            key = tuple(sorted([r.tgt_off, other]))
            if key in seen_swap_pairs:
                continue
            seen_swap_pairs.add(key)
            a_name = (base_names.get(key[0]).name if base_names and base_names.get(key[0]) else "")
            b_name = (base_names.get(key[1]).name if base_names and base_names.get(key[1]) else "")
            if a_name or b_name:
                pair_lines.append(f"0x{key[0]:x} ({a_name or '?'}) ↔ 0x{key[1]:x} ({b_name or '?'})")
            else:
                pair_lines.append(f"0x{key[0]:x} ↔ 0x{key[1]:x}")
        if pair_lines:
            print(f"    • {len(seen_swap_pairs)} swap pair(s) — reorder the named declarations:")
            for line in pair_lines[:6]:
                print(f"        {line}")
            if len(pair_lines) > 6:
                print(f"        ... and {len(pair_lines) - 6} more")
        else:
            print(f"    • {counts['SWAPPED']} user slot(s) appear SWAPPED — reorder paired decls.")
    if counts["SHIFTED"] and dominant_delta:
        print(f"    • {counts['SHIFTED']} user slot(s) SHIFTED by {dominant_delta:+#x} — usually "
              "one side has an extra local that pushes the rest.")
    if counts["DIFFER"]:
        differ_rows = [r for r in user_rows if r.verdict == "DIFFER"]
        named = []
        for r in differ_rows:
            if base_names and r.base_off is not None:
                info = base_names.get(r.base_off)
                if info:
                    named.append(f"0x{r.base_off:x} ({info.name})")
        if named:
            print(f"    • {counts['DIFFER']} user slot(s) DIFFER — different variable lives there. "
                  f"Base vars: {', '.join(named[:4])}{'...' if len(named) > 4 else ''}")
        else:
            print(f"    • {counts['DIFFER']} user slot(s) with same offset but different fingerprint "
                  "— different variable lives in that slot on each side; reorder candidates.")
    if counts["PERMUTED"]:
        print(f"    • {counts['PERMUTED']} user slot(s) PERMUTED — both sides use the same "
              "slot at\n      different program points, i.e. the SAME SET of slots with "
              "variables\n      assigned differently. This is MSVC temporary/slot allocation "
              "shaping,\n      not a declaration count difference. Read the "
              "'↔ base 0x..' notes for the mapping.")
    if counts["TGT_ONLY"]:
        print(f"    • {counts['TGT_ONLY']} slot(s) only on target — a source local our build "
              "elides, or different spill choice.")
    if counts["BASE_ONLY"]:
        print(f"    • {counts['BASE_ONLY']} slot(s) only on our build — extra spill or "
              "compiler temp. Often correlates with register pressure.")
    if not any(counts[v] for v in ("SWAPPED", "SHIFTED", "DIFFER", "PERMUTED",
                                   "TGT_ONLY", "BASE_ONLY")):
        if frame_delta is None:
            print("    • No user-slot mismatches, but the frame size is UNKNOWN — this is "
                  "NOT a clean bill of health.")
        elif frame_delta == 0:
            print("    • User-slot layouts match. If diff is still poor, root cause is not "
                  "stack-layout (check regswaps / replaces / inserts).")
        elif callee_bytes == frame_delta:
            print("    • Pure callee-saved-register shift. AT_LIMIT.")
        else:
            print("    • Frame Δ exists but no user-slot mismatches surfaced — fingerprint "
                  "matching may be too coarse; inspect --show-callee-save and --show-equal.")
    return frame_ok


def dominant_delta_from_rows(tgt_slots: dict[int, SlotFingerprint],
                              base_slots: dict[int, SlotFingerprint]) -> int:
    deltas: Counter = Counter()
    base_by_fp: dict[tuple, list[int]] = defaultdict(list)
    for off, b in base_slots.items():
        base_by_fp[b.fingerprint()].append(off)
    for off, t in tgt_slots.items():
        for b_off in base_by_fp.get(t.fingerprint(), []):
            deltas[b_off - off] += 1
    if not deltas:
        return 0
    delta, _ = deltas.most_common(1)[0]
    return delta


# ── objdiff invocation ──────────────────────────────────────────────────────

def _find_objdiff_cli(project_dir: str) -> str:
    """Locate objdiff-cli. Project-local 'bin/objdiff-cli' wins; falls back to
    the sibling ../objdiff fork checkout used by some milohax repos."""
    candidates = [
        os.path.join(project_dir, "bin", "objdiff-cli"),
        os.path.join(project_dir, "build", "tools", "objdiff-cli"),
        os.path.join(project_dir, "..", "objdiff", "target", "release", "objdiff-cli"),
    ]
    for c in candidates:
        c = os.path.abspath(c)
        if os.path.exists(c) and os.access(c, os.X_OK):
            return c
    raise RuntimeError(
        f"objdiff-cli not found. Tried: {candidates}")


def run_objdiff_for_symbol(symbol: str, project_dir: Optional[str] = None,
                            unit: Optional[str] = None,
                            ruler: str = "graded") -> str:
    """Run objdiff-cli diff and return path to JSON output.

    ★ The ruler is passed EXPLICITLY and self-labelled (lane MCPRULER-1,
    2026-08-14). This used to pass no `-c` at all; see the long note on
    diff_inspect.run_objdiff_for_symbol for why "no `-c`" stopped meaning
    `DataValue` on 2026-08-12 and silently started meaning `name_check`.
    """
    import hashlib
    import subprocess

    if not project_dir:
        project_dir = os.path.dirname(os.path.dirname(
            os.path.dirname(os.path.abspath(__file__))))

    h = hashlib.md5(symbol.encode()).hexdigest()[:12]
    slug = re.sub(r"[^a-zA-Z0-9]+", "_", symbol)[:40].strip("_").lower()
    json_path = f"/tmp/claude/diff_{slug}_{h}.json"
    os.makedirs("/tmp/claude", exist_ok=True)

    objdiff = _find_objdiff_cli(project_dir)

    print(f"Running objdiff for: {symbol}", file=sys.stderr)

    ruler_args: list = []
    try:
        try:
            from analysis.ruler import resolve_ruler
        except ImportError:
            from ruler import resolve_ruler  # same-directory fallback
        resolved = resolve_ruler(project_dir, ruler)
        print(f"[ruler] {resolved.banner()}", file=sys.stderr)
        ruler_args = resolved.args
    except ImportError:
        print("[ruler] WARNING: could not resolve ruler (analysis.ruler not "
              "importable) — objdiff defaults apply, percent is UNLABELLED",
              file=sys.stderr)

    cmd = [
        objdiff, "diff",
        "-p", project_dir,
        symbol,
        "--include-instructions", "--build", "--incremental",
        *ruler_args,
        "-f", "json", "-o", json_path,
    ]
    if unit:
        cmd.extend(["-u", unit])

    result = subprocess.run(cmd, cwd=project_dir, capture_output=True, text=True)
    if result.returncode != 0:
        print(f"objdiff-cli failed (exit {result.returncode}):", file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        if result.stdout:
            print(result.stdout, file=sys.stderr)
        sys.exit(1)
    if result.stderr:
        print(result.stderr, file=sys.stderr, end="")
    return json_path


# ── Selftest / regression fixtures ───────────────────────────────────────────
#
# INSTRUMENT_DESIGN rules 1, 2, 4, 6. Every fixture is built IN MEMORY: no
# toolchain, no objdiff, no filesystem, so this can never silently SKIP.
#
# ★ The load-bearing property is NOT "the new code passes". It is that the OLD
#   code is asserted to STAY WRONG on the same fixtures. A selftest that only
#   demonstrates the new comparator would pass just as happily after somebody
#   "simplified" the fix back out.
#
# ★ 2026-08-16, and this is the second lesson: fixtures 1-6 are HAND-AUTHORED,
#   and every one of them hand-writes the FLAT operand spelling
#   (`("stwu", "r1, -0x150, r1")`). objdiff 4.2.3 emits `r1, -0x80(r1)`. So the
#   whole battery ran green for the entire day on which the parser was, in the
#   field, reading a confident 0 for every framed function it was handed. A
#   synthetic fixture can only test the input shape its author imagined.
#   Fixture 7 below is therefore a VERBATIM dump of live `bin/objdiff-cli`
#   output, not a hand-written approximation, and fixture 7's first assertion is
#   that the fixture still literally contains a parenthesis.

def _ins(idx, t=None, b=None):
    """One objdiff-shaped aligned row. t/b are (opcode, args) or None."""
    d = {"index": idx}
    if t is not None:
        d["target"] = {"opcode": t[0], "args": t[1]}
    if b is not None:
        d["base"] = {"opcode": b[0], "args": b[1]}
    return d


def _bigframe_prologue(hi, lo, idx0=0):
    """The MaybePublish shape: lis/ori + subf r31 + lis/ori + stwux."""
    neg = (-((hi << 16) | lo)) & 0xFFFFFFFF
    return [
        _ins(idx0 + 0, ("mflr", "r12"), ("mflr", "r12")),
        _ins(idx0 + 1, ("bl", "__savegprlr"), ("bl", "__savegprlr_14")),
        _ins(idx0 + 2, ("lis", f"r12, {hi:#x}"), ("lis", f"r12, {hi:#x}")),
        _ins(idx0 + 3, ("ori", f"r12, r12, {lo:#x}"), ("ori", f"r12, r12, {lo:#x}")),
        _ins(idx0 + 4, ("subf", "r31, r12, r1"), ("subf", "r31, r12, r1")),
        _ins(idx0 + 5, ("lis", f"r12, {neg >> 16:#x}"), ("lis", f"r12, {neg >> 16:#x}")),
        _ins(idx0 + 6, ("ori", f"r12, r12, {neg & 0xFFFF:#x}"),
                       ("ori", f"r12, r12, {neg & 0xFFFF:#x}")),
        _ins(idx0 + 7, ("bl", "_RtlCheckStack12"), ("bl", "_RtlCheckStack12")),
        _ins(idx0 + 8, ("stwux", "r1, r1, r12"), ("stwux", "r1, r1, r12")),
    ]


# ── Fixture 7 data: VERBATIM live objdiff-cli output ─────────────────────────
#
# Dumped 2026-08-16 with `objdiff-cli 4.2.3 (0fd82159607c)` (the repo's
# bin/objdiff-cli symlink), against this repo's own build:
#
#   bin/objdiff-cli diff -p . '<symbol>' --include-instructions -f json
#
# and transcribed opcode-for-opcode, args-for-args. NOTHING here is normalised
# or prettied; the `-0x80(r1)` spellings are what the tool actually printed.
# Regenerate the same way if objdiff's display changes again — and if you find
# yourself editing a paren out of this list to make a test pass, the test is
# telling you the truth and the parser is the thing that is wrong.

# ?PoolFree@@YAXHPAX@Z — 0x80 frame, `subi r31, r1, 0x80` alias (so r31 IS a
# frame base here), `bl __savegprlr_28`, one real r31 slot at 0x50, and two
# relocated `sym@l(rN)` loads that must NOT be read as frame references.
_RULER_J_POOLFREE = [
    _ins(0, ("mflr", "r12"), ("mflr", "r12")),
    _ins(1, ("bl", "__savegprlr_28"), ("bl", "__savegprlr_28")),
    _ins(2, ("subi", "r31, r1, 0x80"), ("subi", "r31, r1, 0x80")),
    _ins(3, ("stwu", "r1, -0x80(r1)"), ("stwu", "r1, -0x80(r1)")),
    _ins(4, ("lis", "r11, ?gMemLock@@3PAVCriticalSection@@A@h"),
            ("lis", "r11, ?gMemLock@@3PAVCriticalSection@@A@h")),
    _ins(5, ("mr", "r28, r3"), ("mr", "r28, r3")),
    _ins(6, ("mr", "r30, r4"), ("mr", "r30, r4")),
    _ins(7, ("lwz", "r3, ?gMemLock@@3PAVCriticalSection@@A@l(r11)"),
            ("lwz", "r3, ?gMemLock@@3PAVCriticalSection@@A@l(r11)")),
    _ins(8, ("mr", "r29, r3"), ("mr", "r29, r3")),
    _ins(9, ("cmplwi", "cr6, r3, 0x0"), ("cmplwi", "cr6, r3, 0x0")),
    _ins(10, ("stw", "r3, 0x50(r31)"), ("stw", "r3, 0x50(r31)")),
    _ins(11, ("beq", "cr6, 0x34"), ("beq", "cr6, 0x3c")),
    _ins(12, ("bl", "?Enter@CriticalSection@@QAAXXZ"),
             ("bl", "?Enter@CriticalSection@@QAAXXZ")),
    _ins(13, ("mr", "r3, r30"), ("mr", "r3, r30")),
    _ins(14, ("bl", "?MemTrackFree@@YAXPAX@Z"), ("bl", "?MemTrackFree@@YAXPAX@Z")),
    _ins(15, ("subi", "r11, r28, 0x1"), ("subi", "r11, r28, 0x1")),
    _ins(16, ("lis", "r10, lbl_82E0699C@h"),
             ("lis", "r10, ?gChunkAlloc@@3PAVChunkAllocator@@A@h")),
    _ins(17, ("srawi", "r11, r11, 4"), ("srawi", "r11, r11, 4")),
    _ins(18, ("slwi", "r9, r11, 2"), ("slwi", "r9, r11, 2")),
    _ins(19, ("lwz", "r11, lbl_82E0699C@l(r10)"),
             ("lwz", "r11, ?gChunkAlloc@@3PAVChunkAllocator@@A@l(r10)")),
    _ins(20, ("lwzx", "r10, r9, r11"), ("lwzx", "r10, r9, r11")),
    _ins(21, ("lwz", "r11, 0x14(r10)"), ("lwz", "r11, 0x14(r10)")),
    _ins(22, ("stw", "r11, 0x0(r30)"), ("stw", "r11, 0x0(r30)")),
    _ins(23, ("lwz", "r11, 0x8(r10)"), ("lwz", "r11, 0x8(r10)")),
    _ins(24, ("subi", "r11, r11, 0x1"), ("subi", "r11, r11, 0x1")),
    _ins(25, ("stw", "r30, 0x14(r10)"), ("stw", "r30, 0x14(r10)")),
    _ins(26, ("stw", "r11, 0x8(r10)"), ("stw", "r11, 0x8(r10)")),
    _ins(27, ("cmplwi", "cr6, r29, 0x0"), ("cmplwi", "cr6, r29, 0x0")),
    _ins(28, ("beq", "cr6, 0x7c"), ("beq", "cr6, 0x84")),
    _ins(29, ("mr", "r3, r29"), ("mr", "r3, r29")),
    _ins(30, ("bl", "?Exit@CriticalSection@@QAAXXZ"),
             ("bl", "?Exit@CriticalSection@@QAAXXZ")),
    _ins(31, ("addi", "r1, r31, 0x80"), ("addi", "r1, r31, 0x80")),
    _ins(32, ("b", "__restgprlr_28"), ("b", "__restgprlr_28")),
]

# ?Release@DataArray@@QAAXXZ — 0x60 frame with MANUAL pre-stwu callee saves in
# the paren spelling (`stw r12, -0x8(r1)`, `std r31, -0x10(r1)`) and `mr r31, r3`,
# i.e. r31 holds `this` and is NOT a frame base. This is fixture 6's shape as it
# actually occurs in the binary, rather than as it was imagined.
# The base side is absent on every row (this symbol had no base object at dump
# time); that is verbatim too, and it exercises the None-side skip.
_RULER_J_DATAARRAY_RELEASE = [
    _ins(0, ("mflr", "r12"), None),
    _ins(1, ("stw", "r12, -0x8(r1)"), None),
    _ins(2, ("std", "r31, -0x10(r1)"), None),
    _ins(3, ("stwu", "r1, -0x60(r1)"), None),
    _ins(4, ("lhz", "r11, 0xa(r3)"), None),
    _ins(5, ("mr", "r31, r3"), None),
    _ins(6, ("subi", "r11, r11, 0x1"), None),
    _ins(7, ("extsh.", "r11, r11"), None),
    _ins(8, ("sth", "r11, 0xa(r3)"), None),
    _ins(9, ("bne", "0x38"), None),
    _ins(10, ("bl", "??1DataArray@@AAA@XZ"), None),
    _ins(11, ("mr", "r4, r31"), None),
    _ins(12, ("li", "r3, 0x10"), None),
    _ins(13, ("bl", "?PoolFree@@YAXHPAX@Z"), None),
    _ins(14, ("addi", "r1, r1, 0x60"), None),
    _ins(15, ("lwz", "r12, -0x8(r1)"), None),
    _ins(16, ("mtlr", "r12"), None),
    _ins(17, ("ld", "r31, -0x10(r1)"), None),
    _ins(18, ("blr", ""), None),
]


def _selftest_ruler_fixtures(check, out, H):
    """Fixture 7 — the objdiff-4.2.3 paren spelling, on VERBATIM tool output.

    Every assertion in here fails if `split_operands` loses its paren branch,
    which is the mutation this fixture exists to catch.
    """
    out.append("fixture 7 — REAL objdiff-4.2.3 output, parenthesised d-form:")

    # 7.0 — the fixture must still BE the thing it claims to be. Without this,
    # a future "cleanup" that rewrites `-0x80(r1)` to `-0x80, r1` silently turns
    # fixture 7 back into fixture 3 and the battery goes vacuous again.
    paren_rows = sum(1 for r in _RULER_J_POOLFREE + _RULER_J_DATAARRAY_RELEASE
                     for k in ("target", "base")
                     if r.get(k) and "(" in r[k]["args"])
    check("fixture is still in the PAREN spelling (do not normalise it)",
          paren_rows, 25)

    # 7.1 — the shipped bug, reproduced in-tree. v1 has no paren branch, so it
    # reads a confident 0 on a real 0x80 frame: exactly what every MCP
    # run_objdiff response printed on 2026-08-16.
    check("v1 reads a confident 0 on a real 0x80 frame (STAYS WRONG)",
          _scan_frame_size_v1(_RULER_J_POOLFREE, "target", H), 0)
    check("v1 verdict is a FALSE 'frameless leaf'",
          _scan_frame_size_v1(_RULER_J_POOLFREE, "target", H)
          == _scan_frame_size_v1(_RULER_J_DATAARRAY_RELEASE, "target", H) == 0,
          True)

    # 7.2 — v2 must read the real frame sizes off the paren spelling.
    check("v2 PoolFree TARGET frame", _scan_frame_size(_RULER_J_POOLFREE, "target", H)[0], 128)
    check("v2 PoolFree BASE   frame", _scan_frame_size(_RULER_J_POOLFREE, "base", H)[0], 128)
    check("v2 Release  TARGET frame",
          _scan_frame_size(_RULER_J_DATAARRAY_RELEASE, "target", H)[0], 96)
    check("v2 evidence names the stwu, not the r31 alias",
          _scan_frame_size(_RULER_J_POOLFREE, "target", H)[1], "stwu r1, -0x80, r1")

    # 7.3 — prologue classification off the paren spelling.
    pp = parse_prologue(_RULER_J_POOLFREE, "target")
    check("PoolFree frame_size", pp.frame_size, 128)
    check("PoolFree saved GPRs (__savegprlr_28)", pp.saved_gpr_count, 4)
    pr = parse_prologue(_RULER_J_DATAARRAY_RELEASE, "target")
    check("Release frame_size", pr.frame_size, 96)
    check("Release manual pre-stwu saves seen through `stw r12, -0x8(r1)` / "
          "`std r31, -0x10(r1)`", sorted(pr.callee_save_slots), [0x50, 0x58])

    # 7.4 — slot references. This is the -63% the audit measured.
    check("PoolFree r31 IS a frame base (subi r31, r1, 0x80)",
          sorted(frame_base_regs(_RULER_J_POOLFREE, "target")), ["r1", "r31"])
    check("PoolFree finds the real r31 slot at 0x50",
          sorted(build_fingerprints("target", _RULER_J_POOLFREE)), [0x50])
    check("Release r31 holds `this`, so it is NOT a frame base",
          sorted(frame_base_regs(_RULER_J_DATAARRAY_RELEASE, "target")), ["r1"])
    check("Release finds no stack slots (all r3/r31 refs are members)",
          sorted(build_fingerprints("target", _RULER_J_DATAARRAY_RELEASE)), [])

    # 7.5 — CONTROL (rule 4): a relocated d-form is not a frame reference.
    # `lwz r3, sym@l(r11)` splits to ['r3','sym@l','r11'] and must be REJECTED,
    # or the paren branch would be manufacturing slots out of globals.
    check("relocated `sym@l(r11)` load is NOT a frame reference",
          parse_stack_ref("lwz", "r3, ?gMemLock@@3PAVCriticalSection@@A@l(r11)",
                          {"r1", "r31", "r11"}),
          None)
    check("a plain paren d-form on a frame base IS one",
          parse_stack_ref("stw", "r3, 0x50(r31)", {"r1", "r31"}), (0x50, "int", 4))

    # 7.6 — the honest branch, in the paren spelling (rules 2 & 4).
    # `saw_alloc_form` is set from the OPCODE, so an allocation whose operands we
    # cannot decode reports UNKNOWN. If this ever reads 0 again, the 2026-08-16
    # failure mode is back.
    undecodable = [_ins(0, ("mflr", "r12"), None),
                   _ins(1, ("stwu", "r1, __chkstk_frame@l(r1)"), None)]
    check("undecodable displacement in a paren d-form is UNKNOWN, never 0",
          _scan_frame_size(undecodable, "target", H)[0], None)
    unmodelled = [_ins(0, ("stwu", "r1, -0x80(r1) [scaled]"), None)]
    check("an allocation spelling we do not model at all is UNKNOWN, never 0",
          _scan_frame_size(unmodelled, "target", H)[0], None)
    # ...and the CONTROL: a genuinely frameless leaf must still read 0-KNOWN,
    # or the two clauses above are just `return None`.
    leaf7 = [_ins(0, ("lwz", "r3, 0x10(r3)"), None), _ins(1, ("blr", ""), None)]
    check("a real frameless leaf still reads KNOWN-zero",
          _scan_frame_size(leaf7, "target", H)[0], 0)


def selftest():
    out = []
    ok = True

    def check(label, got, want):
        nonlocal ok
        good = got == want
        if not good:
            ok = False
        out.append(f"  [{'ok' if good else 'FAIL'}] {label}: got {got!r}, want {want!r}")

    H = 80

    # ── Fixture 1: big frame, and the two sides genuinely DIFFER ─────────────
    # This is the killer. v1 reads 0 on both sides and therefore declares the
    # frames EQUAL, when they in fact differ by 0x10. A wrong number would be
    # bad; a confident "match" on a real difference is the vacuous-success shape.
    tgt = _bigframe_prologue(0x1, 0x2f0)                       # 0x102f0
    base = _bigframe_prologue(0x1, 0x300)                      # 0x10300
    mixed = [_ins(i["index"], i.get("target") and (i["target"]["opcode"], i["target"]["args"]),
                  b.get("base") and (b["base"]["opcode"], b["base"]["args"]))
             for i, b in zip(tgt, base)]

    out.append("fixture 1 — big frame (lis/ori/stwux), sides differ by 0x10:")
    check("v1 TARGET frame (STAYS WRONG)", _scan_frame_size_v1(mixed, "target", H), 0)
    check("v1 BASE   frame (STAYS WRONG)", _scan_frame_size_v1(mixed, "base", H), 0)
    check("v1 verdict is a FALSE 'frames match'",
          _scan_frame_size_v1(mixed, "target", H) == _scan_frame_size_v1(mixed, "base", H),
          True)
    check("v2 TARGET frame", _scan_frame_size(mixed, "target", H)[0], 0x102f0)
    check("v2 BASE   frame", _scan_frame_size(mixed, "base", H)[0], 0x10300)
    check("v2 correctly sees a difference",
          _scan_frame_size(mixed, "target", H)[0] != _scan_frame_size(mixed, "base", H)[0],
          True)

    # ── Fixture 2: bare vs numbered save helper ──────────────────────────────
    # Same instruction, different symbol naming. v1 fabricates a +18 GPR delta.
    out.append("fixture 2 — `bl __savegprlr` (target) vs `bl __savegprlr_14` (base):")
    pt = parse_prologue(mixed, "target")
    pb = parse_prologue(mixed, "base")
    check("v2 TARGET saved GPRs", pt.saved_gpr_count, 18)
    check("v2 BASE   saved GPRs", pb.saved_gpr_count, 18)
    check("v2 GPR delta is 0 (was a fabricated +18)",
          pb.saved_gpr_count - pt.saved_gpr_count, 0)

    # ── Fixture 3: CONTROL — v2 must not over-fire (rules 2 & 4) ─────────────
    # A plain stwu frame must still parse identically to v1, and a frameless
    # leaf must read 0-KNOWN, not UNKNOWN. Without this, v2 could be
    # `return None` and fixture 1 would still pass.
    out.append("fixture 3 — CONTROL: v2 must agree with v1 where v1 was right:")
    plain = [_ins(0, ("mflr", "r12"), ("mflr", "r12")),
             _ins(1, ("bl", "__savegprlr_24"), ("bl", "__savegprlr_24")),
             _ins(2, ("subi", "r31, r1, 0x150"), ("subi", "r31, r1, 0x150")),
             _ins(3, ("stwu", "r1, -0x150, r1"), ("stwu", "r1, -0x150, r1"))]
    check("v1 plain stwu frame", _scan_frame_size_v1(plain, "target", H), 0x150)
    check("v2 plain stwu frame (must AGREE)", _scan_frame_size(plain, "target", H)[0], 0x150)
    check("v2 saved GPRs for _24", parse_prologue(plain, "target").saved_gpr_count, 8)
    leaf = [_ins(0, ("mr", "r11, r3"), ("mr", "r11, r3")),
            _ins(1, ("blr", ""), ("blr", ""))]
    check("v2 frameless leaf is KNOWN-zero, not UNKNOWN",
          _scan_frame_size(leaf, "target", H)[0], 0)
    # ...and an allocation form we do NOT model must be UNKNOWN, never 0.
    weird = [_ins(0, ("stwux", "r1, r1, r7"), ("stwux", "r1, r1, r7"))]
    check("v2 undecodable allocation is UNKNOWN (None), never 0",
          _scan_frame_size(weird, "target", H)[0], None)

    # ── Fixture 4: the degenerate slot comparison ────────────────────────────
    # Two same-shaped locals at 0x60 and 0x68, with the ASSIGNMENT SWAPPED
    # between the sides. Every fingerprint is equal, so v1 reports MATCH twice
    # and zero actionable rows -- while the two builds demonstrably put
    # different variables in those slots.
    out.append("fixture 4 — variable permutation across identical-shaped slots:")
    # NB the `subi r31, r1, 0x150` is load-bearing: without it the r31 gate
    # (fixture 6) correctly refuses to treat r31 as a frame base at all.
    perm = [
        _ins(0, ("subi", "r31, r1, 0x150"), ("subi", "r31, r1, 0x150")),
        _ins(1, ("lwz", "r3, 0x60, r31"), ("lwz", "r3, 0x68, r31")),
        _ins(2, ("lwz", "r4, 0x68, r31"), ("lwz", "r4, 0x60, r31")),
    ]
    ts = build_fingerprints("target", perm)
    bs = build_fingerprints("base", perm)
    v1_rows = classify_slots_v1(ts, bs, 0, set(), set())
    v2_rows = classify_slots(ts, bs, 0, set(), set())
    v1c = Counter(r.verdict for r in v1_rows)
    v2c = Counter(r.verdict for r in v2_rows)
    check("v1 says MATCH x2 (STAYS WRONG)", v1c["MATCH"], 2)
    check("v1 finds ZERO actionable rows (STAYS WRONG)",
          sum(v for k, v in v1c.items() if k != "MATCH"), 0)
    check("v2 says MATCH x0", v2c["MATCH"], 0)
    check("v2 says PERMUTED x2", v2c["PERMUTED"], 2)
    check("v2 reports the positional mapping 0x60 ↔ base 0x68",
          any("0x68" in r.note for r in v2_rows if r.tgt_off == 0x60), True)

    # ── Fixture 5: CONTROL — a genuinely identical layout ────────────────────
    # Without this, v2 could be `return PERMUTED` and fixture 4 would pass.
    # This is INSTRUMENT_DESIGN rule 4: produce the other label on a
    # known-opposite case, or the classifier is a constant function.
    out.append("fixture 5 — CONTROL: identical layout must still read MATCH:")
    same = [
        _ins(0, ("subi", "r31, r1, 0x150"), ("subi", "r31, r1, 0x150")),
        _ins(1, ("lwz", "r3, 0x60, r31"), ("lwz", "r3, 0x60, r31")),
        _ins(2, ("lwz", "r4, 0x68, r31"), ("lwz", "r4, 0x68, r31")),
    ]
    ts2 = build_fingerprints("target", same)
    bs2 = build_fingerprints("base", same)
    v2c2 = Counter(r.verdict for r in classify_slots(ts2, bs2, 0, set(), set()))
    check("v2 says MATCH x2 on an identical layout", v2c2["MATCH"], 2)
    check("v2 says PERMUTED x0 on an identical layout", v2c2["PERMUTED"], 0)

    # ── Fixture 6: r31 is an OBJECT pointer, not the frame base ──────────────
    # `mr r31, r3` => r31 holds the incoming `this`. `lwz r3, 0x50, r31` is then
    # a CLASS MEMBER load. v1's hardcoded {"r1","r31"} tabulated it as a stack
    # slot, mixing class layout into a stack report.
    out.append("fixture 6 — r31 holds `this`, so r31-relative refs are NOT stack slots:")
    obj = [_ins(0, ("mflr", "r12"), ("mflr", "r12")),
           _ins(1, ("mr", "r31, r3"), ("mr", "r31, r3")),
           _ins(2, ("stwu", "r1, -0x40, r1"), ("stwu", "r1, -0x40, r1")),
           _ins(3, ("lwz", "r4, 0x50, r31"), ("lwz", "r4, 0x50, r31")),
           _ins(4, ("stw", "r5, 0x20, r1"), ("stw", "r5, 0x20, r1"))]
    check("v1 regs (STAYS WRONG: r31 unconditionally a frame base)",
          sorted(FRAME_BASE_REGS), ["r1", "r31"])
    check("v2 detects r31 as NOT a frame base", sorted(frame_base_regs(obj, "target")), ["r1"])
    v1_slots = {o for o in build_fingerprints("target", obj, FRAME_BASE_REGS)}
    v2_slots = {o for o in build_fingerprints("target", obj)}
    check("v1 counts the member load 0x50 as a stack slot (STAYS WRONG)",
          sorted(v1_slots), [0x20, 0x50])
    check("v2 keeps only the real r1 stack slot", sorted(v2_slots), [0x20])

    # CONTROL for fixture 6 (rule 4): when r31 IS derived from r1, it must be
    # KEPT. Otherwise the gate is just `return {"r1"}` and fixture 6 is vacuous.
    out.append("fixture 6b — CONTROL: r31 derived from r1 must still count:")
    frm = [_ins(0, ("mflr", "r12"), ("mflr", "r12")),
           _ins(1, ("subi", "r31, r1, 0x150"), ("subi", "r31, r1, 0x150")),
           _ins(2, ("stwu", "r1, -0x150, r1"), ("stwu", "r1, -0x150, r1")),
           _ins(3, ("lwz", "r4, 0x50, r31"), ("lwz", "r4, 0x50, r31"))]
    check("v2 detects r31 AS a frame base", sorted(frame_base_regs(frm, "target")),
          ["r1", "r31"])
    check("v2 keeps the r31 stack slot", sorted(build_fingerprints("target", frm)), [0x50])
    # and the subf form, which is the one UIStats uses
    check("v2 detects `subf r31, r12, r1` as a frame base",
          sorted(frame_base_regs(_bigframe_prologue(0x1, 0x2f0), "target")), ["r1", "r31"])

    # ── Fixture 7: the spelling fixtures 1-6 could not see ───────────────────
    _selftest_ruler_fixtures(check, out, H)

    return ok, out


# ── Main ─────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--symbol", help="Symbol (e.g. 'Foo::Bar(int)')")
    parser.add_argument("--selftest", action="store_true",
                        help="Run the in-memory regression fixtures (no toolchain, "
                             "no objdiff, no filesystem) and exit.")
    parser.add_argument("--allow-unknown-frame", action="store_true",
                        help="Exit 0 even when the frame size could not be determined. "
                             "Default is to REFUSE with exit 2.")
    parser.add_argument("--unit", default=None, help="Unit for objdiff disambiguation")
    parser.add_argument("--project-dir", default=None, help="Project root")
    parser.add_argument("--ruler", default="graded",
                        choices=["graded", "none", "data_value"],
                        help="Diff ruler. 'graded' (default) matches "
                             "report.json's provenance.diff_config. 'none' "
                             "ignores relocation names. 'data_value' charges "
                             "relocation addresses too. Always printed to stderr.")
    parser.add_argument("--show-equal", action="store_true",
                        help="Also list MATCH rows (default: hide)")
    parser.add_argument("--show-callee-save", action="store_true",
                        help="Also list prologue/epilogue callee-save slots (default: hide)")
    parser.add_argument("--no-names", action="store_true",
                        help="Skip CodeView debug recompile + name extraction.")
    parser.add_argument("--json-file", default=None,
                        help="Skip objdiff invocation; load diff JSON from this path")
    args = parser.parse_args()

    if args.selftest:
        ok, lines = selftest()
        print("# stack_layout selftest — frame-parse + slot-comparator regression")
        for ln in lines:
            print(ln)
        print("PASS" if ok else "FAIL")
        sys.exit(0 if ok else 1)

    if not args.symbol:
        parser.error("--symbol is required (or use --selftest)")

    if args.json_file:
        json_path = args.json_file
    else:
        json_path = run_objdiff_for_symbol(
            args.symbol, project_dir=args.project_dir, unit=args.unit,
            ruler=args.ruler)

    with open(json_path) as f:
        data = json.load(f)

    instrs = data.get("instructions", [])
    if not instrs:
        print("No instructions in JSON.", file=sys.stderr)
        sys.exit(1)

    tgt_slots = build_fingerprints("target", instrs)
    base_slots = build_fingerprints("base", instrs)

    tgt_prol = parse_prologue(instrs, "target")
    base_prol = parse_prologue(instrs, "base")

    dominant_delta = dominant_delta_from_rows(tgt_slots, base_slots)
    rows = classify_slots(tgt_slots, base_slots, dominant_delta,
                          tgt_prol.callee_save_slots, base_prol.callee_save_slots)

    base_names: dict | None = None
    if not args.no_names:
        base_names = _try_extract_locals(args.symbol, args.project_dir)
        if not base_names:
            base_names = None

    frame_ok = print_report(rows, tgt_prol, base_prol, args.show_equal,
                            args.show_callee_save, dominant_delta, base_names)
    if not frame_ok and not args.allow_unknown_frame:
        sys.exit(2)


if __name__ == "__main__":
    main()
