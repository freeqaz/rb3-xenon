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


def parse_stack_ref(opcode: str, args: str) -> Optional[tuple[int, str, int]]:
    """Return (offset, kind, size) if this instruction references the frame
    base (r1 or r31), else None.

    objdiff arg format examples:
      stw  r0, 0x374, r1          -> (0x374, 'int',   4)
      stfd f31, 0x360, r1         -> (0x360, 'float', 8)
      std  r29, 0x88, r1          -> (0x88,  'int',   8)  (Xenon 64-bit)
      stw  r11, 0x58, r31         -> (0x58,  'int',   4)  (MSVC frame-ptr alias)
      addi r11, r1, 0x270         -> filtered (r1/r11/r31 frame arithmetic)
    """
    parts = [p.strip() for p in args.split(",")]
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

    if base not in FRAME_BASE_REGS:
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

    def short_repr(self) -> str:
        return (f"{self.dominant_kind:6s} sz={self.inferred_size:<2d} "
                f"L={self.loads:<2d} S={self.stores:<2d} A={self.accesses:<3d} "
                f"[{self.first_idx}..{self.last_idx}]")

    def opcodes_repr(self) -> str:
        return ", ".join(f"{op}×{n}" for op, n in self.opcodes.most_common(3))


def build_fingerprints(side_key: str, instrs: list) -> dict[int, SlotFingerprint]:
    slots: dict[int, SlotFingerprint] = {}
    for ins in instrs:
        idx = ins.get("index", -1)
        side = ins.get(side_key)
        if not side:
            continue
        opcode = side.get("opcode", "")
        args = side.get("args", "")
        ref = parse_stack_ref(opcode, args)
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
    frame_size: int = 0
    saved_gpr_count: int = 0
    saved_fpr_count: int = 0
    saved_vmx_count: int = 0
    callee_save_slots: set = field(default_factory=set)  # offsets, new-r1-relative
    raw_savegprlr: Optional[int] = None
    raw_savefpr: Optional[int] = None


def _try_int(s: str) -> Optional[int]:
    try:
        return int(s, 0)
    except (ValueError, TypeError):
        return None


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

    # Pass 1: locate stwu to learn frame_size.
    horizon = min(80, len(instrs))
    for ins in instrs[:horizon]:
        side = ins.get(side_key)
        if not side:
            continue
        op = side.get("opcode", "")
        args = side.get("args", "")
        parts = [a.strip() for a in args.split(",")]
        if op == "stwu" and len(parts) >= 3 and parts[0] == "r1" and parts[2] == "r1":
            n = _try_int(parts[1])
            if n is not None and n < 0:
                p.frame_size = -n
                break

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
        parts = [a.strip() for a in args.split(",")]

        # bl __save(gpr|fpr|gprlr)_NN  (one or two leading underscores)
        if op == "bl":
            m = re.search(r"_+save(gpr|fpr|gprlr|vmx)_(\d+)", args)
            if m:
                kind = m.group(1)
                nn = int(m.group(2))
                count = 32 - nn
                if kind in ("gpr", "gprlr"):
                    p.raw_savegprlr = nn
                    p.saved_gpr_count = max(p.saved_gpr_count, count)
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
                    p.saved_fpr_count = max(p.saved_fpr_count, count)
                    if p.frame_size:
                        # __savefpr_NN: f31..fNN in 8-byte slots below the GPR area
                        # (offset depends on whether gprlr was also called); we mark
                        # the relative band so the user-slot filter is conservative.
                        base = p.frame_size - 8 * (p.saved_gpr_count + 1)
                        for i, reg in enumerate(range(31, nn - 1, -1)):
                            slot = base - i * 8
                            if slot >= 0:
                                p.callee_save_slots.add(slot)
                elif kind == "vmx":
                    p.saved_vmx_count = max(p.saved_vmx_count, count)
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
        p.saved_gpr_count = max(p.saved_gpr_count, seen_stmw_gpr)
    if callee_fprs:
        p.saved_fpr_count = max(p.saved_fpr_count, len(callee_fprs))
    if callee_gprs:
        p.saved_gpr_count = max(p.saved_gpr_count, len(callee_gprs))

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


def classify_slots(tgt_slots: dict[int, SlotFingerprint],
                   base_slots: dict[int, SlotFingerprint],
                   dominant_delta: int,
                   tgt_callee_save: set[int],
                   base_callee_save: set[int]) -> list[Row]:
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
            verdict = "MATCH" if tfp.fingerprint() == bfp.fingerprint() else "DIFFER"
            rows.append(Row(off, off, tfp, bfp, verdict, callee_save=is_cs(off, off)))
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
            used_base_idxs.add(j)

    base_only_drop = {id(base_only[j]) for j in used_base_idxs}
    rows = [r for r in rows if id(r) not in base_only_drop]

    return rows


# ── Printing ─────────────────────────────────────────────────────────────────

VERDICT_ORDER = ["SWAPPED", "DIFFER", "SHIFTED", "TGT_ONLY", "BASE_ONLY", "MATCH"]


def print_report(rows: list[Row], tgt_prol: Prologue, base_prol: Prologue,
                 show_equal: bool, show_callee_save: bool,
                 dominant_delta: int, base_names: dict | None = None) -> None:
    print("=" * 84)
    print("STACK LAYOUT DIFF")
    print("=" * 84)
    print()

    frame_delta = base_prol.frame_size - tgt_prol.frame_size
    gpr_delta = base_prol.saved_gpr_count - tgt_prol.saved_gpr_count
    fpr_delta = base_prol.saved_fpr_count - tgt_prol.saved_fpr_count

    print(f"  Frame size:          TGT 0x{tgt_prol.frame_size:x}     "
          f"BASE 0x{base_prol.frame_size:x}     Δ {frame_delta:+#x}")
    print(f"  Callee-saved GPRs:   TGT {tgt_prol.saved_gpr_count:<5d}   "
          f"BASE {base_prol.saved_gpr_count:<5d}   Δ {gpr_delta:+d}")
    print(f"  Callee-saved FPRs:   TGT {tgt_prol.saved_fpr_count:<5d}   "
          f"BASE {base_prol.saved_fpr_count:<5d}   Δ {fpr_delta:+d}")

    # Xenon GPRs are 64-bit (std), so each GPR slot = 8 bytes
    callee_bytes = gpr_delta * 8 + fpr_delta * 8
    if frame_delta == 0:
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
    if counts["TGT_ONLY"]:
        print(f"    • {counts['TGT_ONLY']} slot(s) only on target — a source local our build "
              "elides, or different spill choice.")
    if counts["BASE_ONLY"]:
        print(f"    • {counts['BASE_ONLY']} slot(s) only on our build — extra spill or "
              "compiler temp. Often correlates with register pressure.")
    if not any(counts[v] for v in ("SWAPPED", "SHIFTED", "DIFFER", "TGT_ONLY", "BASE_ONLY")):
        if frame_delta == 0:
            print("    • User-slot layouts match. If diff is still poor, root cause is not "
                  "stack-layout (check regswaps / replaces / inserts).")
        elif callee_bytes == frame_delta:
            print("    • Pure callee-saved-register shift. AT_LIMIT.")
        else:
            print("    • Frame Δ exists but no user-slot mismatches surfaced — fingerprint "
                  "matching may be too coarse; inspect --show-callee-save and --show-equal.")


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
                            unit: Optional[str] = None) -> str:
    """Run objdiff-cli diff and return path to JSON output."""
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
    cmd = [
        objdiff, "diff",
        "-p", project_dir,
        symbol,
        "--include-instructions", "--build", "--incremental",
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


# ── Main ─────────────────────────────────────────────────────────────────────

def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__,
                                     formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("--symbol", required=True, help="Symbol (e.g. 'Foo::Bar(int)')")
    parser.add_argument("--unit", default=None, help="Unit for objdiff disambiguation")
    parser.add_argument("--project-dir", default=None, help="Project root")
    parser.add_argument("--show-equal", action="store_true",
                        help="Also list MATCH rows (default: hide)")
    parser.add_argument("--show-callee-save", action="store_true",
                        help="Also list prologue/epilogue callee-save slots (default: hide)")
    parser.add_argument("--no-names", action="store_true",
                        help="Skip CodeView debug recompile + name extraction.")
    parser.add_argument("--json-file", default=None,
                        help="Skip objdiff invocation; load diff JSON from this path")
    args = parser.parse_args()

    if args.json_file:
        json_path = args.json_file
    else:
        json_path = run_objdiff_for_symbol(
            args.symbol, project_dir=args.project_dir, unit=args.unit)

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

    print_report(rows, tgt_prol, base_prol, args.show_equal,
                 args.show_callee_save, dominant_delta, base_names)


if __name__ == "__main__":
    main()
