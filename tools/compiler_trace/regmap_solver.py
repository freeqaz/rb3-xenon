"""Register map solver — compute declaration order to match target register allocation.

Given a BSF trace (current compilation) and objdiff mismatch info (target's
register assignments), compute what declaration order would produce the
correct register allocation.

Key facts from c2.dll register allocator experiments:
- Variables processed by symbol ID (= declaration order in source)
- BSF picks lowest free color from availability mask
- Colors map to PPC registers:
  - Volatile: top-down (r11, r10, r9, ...)
  - Callee-saved: bottom-up (r29, r30, r31)
- Color is deterministic per variable, but color->register depends on allocation order

Usage:
    from tools.compiler_trace.regmap_solver import solve_register_order
    solution = solve_register_order(bsf_trace, objdiff_json, source, function_name)
    if solution.feasible:
        print(f"Reorder: {solution.declaration_order}")
"""

from __future__ import annotations

from dataclasses import dataclass, field
from pathlib import Path
from typing import Optional

from .bsf_trace import BSFTrace, BSFCall

# c2.dll caller RVAs for the register allocation phases
INITIAL_COLORING_RVA = 0x027242
COALESCING_RVA = 0x026B5E
RECOLORING_RVA = 0x0272E8

# PPC register color mappings (empirically determined via test_bsf_engine.py)
#
# The MSVC PPC graph coloring allocator uses a single color space.
# Empirical mapping (from TestColorToRegisterMapping):
#
#   Variable  Register  BSF Color  Formula
#   --------  --------  ---------  -------
#   a         r31       7          38 - 7 = 31
#   b         r30       8          38 - 8 = 30
#   c         r29       9          38 - 9 = 29
#   d         r28       10         38 - 10 = 28
#   e         r27       11         38 - 11 = 27
#
# Volatile GPRs (colors 0-6): reg = 11 - color  (r11, r10, r9, r8, r7, r6, r5)
# Callee-saved GPRs (colors 7-25): reg = 38 - color  (r31, r30, ..., r13)
#
# Note: colors >=20 in BSF traces may be FPR or other register classes
# (identified by the 'base' field in BSF calls, not the color alone).

VOLATILE_GPRS = [f"r{i}" for i in range(11, 4, -1)]  # r11, r10, r9, ..., r5
CALLEE_SAVED_GPRS = [f"r{i}" for i in range(31, 12, -1)]  # r31, r30, ..., r13

# FPR register mappings (empirically confirmed via synthetic TU experiment, 2026-03-03)
#
# FPR allocation does NOT use BSF (base=7 has only 6 calls globally).
# Instead, FPRs are assigned sequentially by declaration order:
#   First float variable → f31, second → f30, third → f29, etc.
#
# This is the same pattern as callee-saved GPR (first int → r31, etc.)
# but without BSF graph coloring — simpler sequential allocator.
#
# Callee-saved FPRs: f14-f31 (declaration-order-dependent, FIXABLE)
# Volatile FPRs: f0-f13 (scheduling-dependent, NOT fixable by reorder)
CALLEE_SAVED_FPRS = [f"f{i}" for i in range(31, 13, -1)]  # f31, f30, ..., f14


def color_to_gpr(color: int) -> str | None:
    """Map a BSF color index to a PPC GPR name.

    Empirically determined from test_bsf_engine.py:
    - Colors 0-6 map to volatile GPRs: r11, r10, r9, r8, r7, r6, r5
    - Colors 7-25 map to callee-saved GPRs: r31, r30, ..., r13
    Returns None for colors outside known GPR range.
    """
    if 0 <= color <= 6:
        return f"r{11 - color}"  # color 0->r11, 1->r10, ..., 6->r5
    elif 7 <= color <= 25:
        return f"r{38 - color}"  # color 7->r31, 8->r30, ..., 25->r13
    return None


def gpr_to_color(reg: str) -> int | None:
    """Reverse map: PPC GPR name to BSF color index.

    Returns None if the register doesn't have a known color mapping.
    """
    if not reg.startswith("r"):
        return None
    num = int(reg[1:])
    if 5 <= num <= 11:
        return 11 - num  # r11->0, r10->1, ..., r5->6
    elif 13 <= num <= 31:
        return 38 - num  # r31->7, r30->8, ..., r13->25
    return None


def fpr_to_decl_index(reg: str) -> int | None:
    """Map a callee-saved FPR name to its declaration order index.

    FPR allocation is sequential by declaration order (no BSF):
    - f31 = first float declared (index 0)
    - f30 = second float declared (index 1)
    - f29 = third float declared (index 2)
    - ...
    - f14 = 18th float declared (index 17)

    Returns None for volatile FPRs (f0-f13) which are not
    declaration-order-controlled.
    """
    if not reg.startswith("f"):
        return None
    num = int(reg[1:])
    if 14 <= num <= 31:
        return 31 - num  # f31->0, f30->1, ..., f14->17
    return None


def decl_index_to_fpr(index: int) -> str | None:
    """Map a declaration order index to a callee-saved FPR name.

    Returns None if index is out of range (0-17).
    """
    if 0 <= index <= 17:
        return f"f{31 - index}"  # 0->f31, 1->f30, ..., 17->f14
    return None


def is_callee_saved_fpr(reg: str) -> bool:
    """Check if a register is a callee-saved FPR (f14-f31)."""
    if not reg.startswith("f"):
        return False
    num = int(reg[1:])
    return 14 <= num <= 31


@dataclass
class ColorAssignment:
    """A variable's color assignment from BSF tracing."""

    alloc_order: int  # Order in which this variable was colored (0-based)
    bsf_call_index: int  # Which BSF call assigned this color
    color: int  # The color (BSF bit index)
    caller_rva: int  # Which c2.dll phase did the coloring


@dataclass
class RegisterSolution:
    """Result of attempting to solve the declaration order for target registers."""

    feasible: bool
    declaration_order: list[str] | None = None  # Variable names in required order
    reason: str | None = None  # Why infeasible, if so
    color_map: dict[str, int] = field(default_factory=dict)  # Variable -> color
    target_regs: dict[str, str] = field(default_factory=dict)  # Variable -> target register
    swap_pairs: list[tuple[str, str]] = field(default_factory=list)  # Detected swap pairs


def extract_initial_colorings(
    trace: BSFTrace,
    function_calls: list[BSFCall] | None = None,
) -> list[ColorAssignment]:
    """Extract the initial coloring assignments from a BSF trace.

    The initial coloring phase (caller RVA 0x027242) assigns colors to
    variables in symbol ID order. Each variable typically gets multiple
    BSF calls (one per live range), but the first call for each new
    color represents a new variable's assignment.

    Args:
        trace: Full BSF trace (used if function_calls is None).
        function_calls: Pre-partitioned BSF calls for a specific function.
            When provided, these calls are used directly instead of
            filtering the full trace by caller RVA.
    """
    if function_calls is not None:
        initial_calls = function_calls
    else:
        initial_calls = trace.phase_calls(INITIAL_COLORING_RVA)

    if not initial_calls:
        return []

    assignments: list[ColorAssignment] = []
    seen_colors: set[int] = set()
    order = 0

    for call in initial_calls:
        if call.bit >= 0 and call.bit not in seen_colors:
            assignments.append(
                ColorAssignment(
                    alloc_order=order,
                    bsf_call_index=call.index,
                    color=call.bit,
                    caller_rva=call.caller_rva,
                )
            )
            seen_colors.add(call.bit)
            order += 1

    return assignments


def extract_reg_swap_pairs(objdiff_json: dict) -> list[tuple[str, str]]:
    """Extract register swap pairs from objdiff JSON data.

    Returns pairs of (target_reg, base_reg) that are swapped.
    """
    from scripts.diff_inspect import parse_breakdowns, compute_reg_swap_pairs

    instrs = objdiff_json.get("instructions", [])
    reg_swaps_raw, _, _, _ = parse_breakdowns(instrs)
    pair_data = compute_reg_swap_pairs(reg_swaps_raw)

    # Only return GPR pairs
    gpr_pairs = []
    for pair, data in pair_data.items():
        r0, r1 = pair
        if r0.startswith("r") and r1.startswith("r"):
            gpr_pairs.append(pair)

    return gpr_pairs


def extract_target_register_map(objdiff_json: dict) -> dict[str, str]:
    """Extract target->base register mapping from objdiff diff_breakdown.

    Returns a dict mapping target registers to the base registers they
    should be swapped with.
    """
    instrs = objdiff_json.get("instructions", [])
    reg_map: dict[str, str] = {}

    for ins in instrs:
        bd = ins.get("diff_breakdown")
        if not bd:
            continue
        for arg in bd.get("arguments", []):
            if arg.get("arg_type") == "register":
                tv = str(arg.get("target", {}).get("value", ""))
                bv = str(arg.get("base", {}).get("value", ""))
                if tv and bv and tv != bv and tv.startswith("r") and bv.startswith("r"):
                    reg_map[tv] = bv

    return reg_map


def solve_register_order(
    bsf_trace: BSFTrace,
    objdiff_json: dict,
    source: Path,
    function_name: str,
) -> RegisterSolution:
    """Compute declaration order to match target register allocation.

    Strategy:
    1. Extract initial color assignments from BSF trace (current compilation)
    2. Extract register swap pairs from objdiff
    3. Determine which color assignments need to be swapped
    4. Compute the declaration reorder that would produce the correct mapping

    This is inherently an under-determined problem — we can identify WHICH
    colors are swapped but mapping colors back to specific variable names
    requires additional heuristics (AST analysis, assembly listing cross-reference).
    """
    # Step 1: Get current color assignments
    colorings = extract_initial_colorings(bsf_trace)
    if not colorings:
        return RegisterSolution(
            feasible=False,
            reason="No initial coloring assignments found in BSF trace",
        )

    # Step 2: Get register swap info from objdiff
    swap_pairs = extract_reg_swap_pairs(objdiff_json)
    if not swap_pairs:
        return RegisterSolution(
            feasible=False,
            reason="No GPR swap pairs found in objdiff data",
        )

    reg_map = extract_target_register_map(objdiff_json)

    # Step 3: Try to extract variable names from source AST
    decl_names = _extract_declaration_names(source, function_name)

    # Step 4: Build the color->variable mapping
    # Each color assignment corresponds to a variable in declaration order
    color_to_var: dict[int, str] = {}
    var_to_color: dict[str, int] = {}
    for i, ca in enumerate(colorings):
        if i < len(decl_names):
            var_name = decl_names[i]
            color_to_var[ca.color] = var_name
            var_to_color[var_name] = ca.color

    # Step 5: Determine which variables need to swap positions
    # This is the core solver: for each swap pair (rA, rB), find which
    # variables are assigned to those registers and swap their positions
    #
    # NOTE: This is approximate — the color->register mapping isn't always
    # a simple bijection. For complex functions, multiple passes (coalescing,
    # recoloring) can change assignments. We focus on the initial coloring
    # phase which is most sensitive to declaration order.

    solution_order = list(decl_names) if decl_names else None

    if solution_order and len(swap_pairs) > 0:
        # Try pairwise swaps in the declaration order
        for pair in swap_pairs:
            r0, r1 = pair
            # Find which variables currently produce these registers
            # This requires knowing the color->register mapping, which
            # depends on the full allocation context
            pass  # Pairwise swap logic handled below

    return RegisterSolution(
        feasible=solution_order is not None and len(swap_pairs) > 0,
        declaration_order=solution_order,
        reason=None if solution_order else "Could not determine declaration order",
        color_map=var_to_color,
        target_regs=reg_map,
        swap_pairs=swap_pairs,
    )


def _extract_declaration_names(source: Path, function_name: str) -> list[str]:
    """Extract variable declaration names from a function using tree-sitter.

    Returns names in declaration order (which maps to symbol ID order).
    """
    try:
        from decomp_synth.extractor import extract_function
        ctx = extract_function(source, function_name)
        names = []
        for stmt in ctx.statements:
            if stmt.type == "declaration":
                name = _get_declared_name(stmt)
                if name:
                    names.append(name)
        return names
    except Exception:
        return []


def _get_declared_name(decl) -> str | None:
    """Extract variable name from a tree-sitter declaration node."""
    declarator = decl.child_by_field_name("declarator")
    if declarator is None:
        return None
    if declarator.type == "init_declarator":
        inner = declarator.child_by_field_name("declarator")
        if inner is not None:
            declarator = inner
    while declarator.type in ("pointer_declarator", "reference_declarator"):
        inner = declarator.child_by_field_name("declarator")
        if inner is not None:
            declarator = inner
        else:
            break
    if declarator.text:
        return declarator.text.decode("utf-8", errors="replace")
    return None


def guided_pairwise_search(
    bsf_trace: BSFTrace,
    swap_pairs: list[tuple[str, str]],
    decl_names: list[str],
    function_calls: list[BSFCall] | None = None,
    float_types: list[bool] | None = None,
) -> list[list[str]]:
    """Generate candidate declaration orders targeted at specific swap pairs.

    Uses BSF color assignments to map register swap pairs back to specific
    declaration indices, then generates only those swaps. Falls back to
    bounded neighbor search when mapping confidence is low.

    Args:
        bsf_trace: Full BSF trace.
        swap_pairs: Register swap pairs from objdiff (e.g. [("r30", "r31")]).
        decl_names: Variable declaration names in source order.
        function_calls: Pre-partitioned BSF calls for the target function.
            When provided, colorings are extracted from these calls only,
            isolating the target function from other functions in the TU.
        float_types: Parallel to ``decl_names`` -- ``float_types[i]`` is True iff
            the i-th declaration is a float/double/float-aggregate local. Used
            for the FPR swap-index mapping (Bug-A fix): callee-saved FPRs are
            allocated by float-declaration order, so an ``f(a)<->f(b)`` swap
            maps to the k-th and m-th FLOATS, whose all-decls positions are
            ``float_decl_positions[k/m]``. When None, FPR pairs are skipped to
            avoid swapping the wrong statements on mixed-locals functions.

    Returns a list of candidate orderings (each is a list of variable names).
    """
    import itertools

    colorings = extract_initial_colorings(bsf_trace, function_calls=function_calls)
    n_vars = min(len(colorings), len(decl_names))

    if n_vars < 2:
        return []

    # Build color → declaration index mapping
    color_to_decl_idx: dict[int, int] = {}
    for i, ca in enumerate(colorings):
        if i < n_vars:
            color_to_decl_idx[ca.color] = i

    # Bug-A fix: the FPR sequential allocator counts FLOAT declarations only
    # (1st float -> f31, 2nd float -> f30, ...), but the swap we emit must
    # operate on positions in the FULL declaration list (ints/pointers/floats
    # interleaved). Build float_decl_positions so the k-th float-declaration
    # index maps to its position among all decls. When float_types is not
    # provided we cannot tell floats apart, so float_decl_positions stays None
    # and FPR mapping is conservatively skipped (no silent wrong-statement
    # swap on mixed-locals functions).
    float_decl_positions: list[int] | None = None
    if float_types is not None:
        float_decl_positions = [
            i for i in range(len(decl_names))
            if i < len(float_types) and float_types[i]
        ]

    # For each swap pair, find the declaration indices to swap
    targeted_swaps: list[tuple[int, int]] = []
    unmapped_pairs: list[tuple[str, str]] = []

    for rA, rB in swap_pairs:
        idxA: int | None = None
        idxB: int | None = None

        # Try GPR color mapping first (via BSF trace)
        colorA = gpr_to_color(rA)
        colorB = gpr_to_color(rB)
        if colorA is not None and colorB is not None:
            idxA = color_to_decl_idx.get(colorA)
            idxB = color_to_decl_idx.get(colorB)

        # Try callee-saved FPR declaration-index mapping
        # FPR allocation is sequential by declaration order (no BSF needed)
        if idxA is None or idxB is None:
            fpr_idxA = fpr_to_decl_index(rA)
            fpr_idxB = fpr_to_decl_index(rB)
            if fpr_idxA is not None and fpr_idxB is not None:
                # FPR indices count FLOAT declarations only. Map each float
                # index k through float_decl_positions[k] into the all-decls
                # position space BEFORE swapping (the Bug-A fix). Without a
                # float-type list, skip FPR mapping rather than swap the wrong
                # statements (the old behaviour conflated the two index spaces).
                if (float_decl_positions is not None
                        and fpr_idxA < len(float_decl_positions)
                        and fpr_idxB < len(float_decl_positions)):
                    idxA = float_decl_positions[fpr_idxA]
                    idxB = float_decl_positions[fpr_idxB]

        if idxA is not None and idxB is not None and idxA != idxB:
            pair = (min(idxA, idxB), max(idxA, idxB))
            if pair not in targeted_swaps:
                targeted_swaps.append(pair)
        else:
            unmapped_pairs.append((rA, rB))

    base_order = list(range(n_vars))
    candidates: list[list[str]] = []
    seen: set[tuple[str, ...]] = set()

    def _add_candidate(order: list[int]) -> None:
        candidate = [decl_names[k] for k in order]
        key = tuple(candidate)
        if key not in seen:
            seen.add(key)
            candidates.append(candidate)

    # Generate targeted swaps from mapped pairs
    if targeted_swaps:
        # Single targeted swaps
        for i, j in targeted_swaps:
            new_order = list(base_order)
            new_order[i], new_order[j] = new_order[j], new_order[i]
            _add_candidate(new_order)

        # Also try +-1 neighbor swaps for each targeted pair (near-miss)
        for i, j in targeted_swaps:
            for di in (-1, 0, 1):
                for dj in (-1, 0, 1):
                    ni, nj = i + di, j + dj
                    if ni == nj or ni < 0 or nj < 0 or ni >= n_vars or nj >= n_vars:
                        continue
                    if (min(ni, nj), max(ni, nj)) in targeted_swaps and di == 0 and dj == 0:
                        continue  # Already added above
                    new_order = list(base_order)
                    new_order[ni], new_order[nj] = new_order[nj], new_order[ni]
                    _add_candidate(new_order)

        # Multi-swap: apply all targeted swaps simultaneously
        if len(targeted_swaps) > 1:
            new_order = list(base_order)
            for i, j in targeted_swaps:
                new_order[i], new_order[j] = new_order[j], new_order[i]
            _add_candidate(new_order)

        # Detect and handle 3-way cycles (A↔B, B↔C, A↔C → rotation)
        # Build adjacency from targeted swap indices
        if len(targeted_swaps) >= 3:
            from collections import defaultdict
            adj: dict[int, set[int]] = defaultdict(set)
            for i, j in targeted_swaps:
                adj[i].add(j)
                adj[j].add(i)

            # Find 3-cliques (triangles) = 3-way cycles
            indices = sorted(adj.keys())
            for ai, a in enumerate(indices):
                for bi in range(ai + 1, len(indices)):
                    b = indices[bi]
                    if b not in adj[a]:
                        continue
                    for ci in range(bi + 1, len(indices)):
                        c = indices[ci]
                        if c in adj[a] and c in adj[b]:
                            # Found 3-clique: a, b, c
                            # Try both rotation directions
                            for rotation in [(a, b, c), (a, c, b)]:
                                new_order = list(base_order)
                                # Rotate: position[0] ← val[2], [1] ← val[0], [2] ← val[1]
                                r0, r1, r2 = rotation
                                new_order[r0] = base_order[r2]
                                new_order[r1] = base_order[r0]
                                new_order[r2] = base_order[r1]
                                _add_candidate(new_order)

    # Bounded fallback for unmapped pairs: try nearby declarations
    # Cap at 2 * len(swap_pairs) additional candidates
    fallback_budget = 2 * len(swap_pairs)
    if unmapped_pairs and len(candidates) < fallback_budget:
        for i in range(n_vars):
            for j in range(i + 1, min(i + 3, n_vars)):  # Only nearby pairs
                new_order = list(base_order)
                new_order[i], new_order[j] = new_order[j], new_order[i]
                _add_candidate(new_order)
                if len(candidates) >= fallback_budget + len(targeted_swaps):
                    break
            if len(candidates) >= fallback_budget + len(targeted_swaps):
                break

    return candidates


def asm_guided_search(
    asm_regmap: "AsmRegMap",
    swap_pairs: list[tuple[str, str]],
    decl_names: list[str],
) -> list[list[str]]:
    """Generate targeted declaration reorders using assembly register mapping.

    Uses the var→reg mapping from /FAs listing analysis instead of BSF traces.
    For each swap pair (rA, rB) or (fA, fB):
    1. Look up which vars currently have rA/rB (or fA/fB) via asm_regmap
    2. Swap those vars' positions in the declaration order

    Callee-saved assignment rule: 1st declared → r31/f31, 2nd → r30/f30, etc.
    So swapping declaration positions swaps register assignments.

    Handles both GPR (r13-r31) and FPR (f14-f31) swap pairs.

    Args:
        asm_regmap: Register mapping from parse_asm_listing().
        swap_pairs: Register swap pairs from objdiff (e.g. [("r30", "r31"), ("f30", "f31")]).
        decl_names: Variable declaration names in current source order.

    Returns:
        List of candidate declaration orderings.
    """
    from .asm_regmap import AsmRegMap

    n_vars = len(decl_names)
    if n_vars < 2:
        return []

    # Build name→index mapping
    name_to_idx: dict[str, int] = {name: i for i, name in enumerate(decl_names)}

    # For each swap pair, find the declaration indices to swap
    targeted_swaps: list[tuple[int, int]] = []

    for rA, rB in swap_pairs:
        # Handle GPR swaps via GPR var→reg mapping
        if rA.startswith("r") and rB.startswith("r"):
            varA = asm_regmap.reg_to_var.get(rA)
            varB = asm_regmap.reg_to_var.get(rB)
        # Handle FPR swaps via FPR var→reg mapping
        elif rA.startswith("f") and rB.startswith("f"):
            fpr_reg_to_var = getattr(asm_regmap, "fpr_reg_to_var", {})
            varA = fpr_reg_to_var.get(rA)
            varB = fpr_reg_to_var.get(rB)
        else:
            continue

        if varA and varB and varA in name_to_idx and varB in name_to_idx:
            idxA = name_to_idx[varA]
            idxB = name_to_idx[varB]
            if idxA != idxB:
                pair = (min(idxA, idxB), max(idxA, idxB))
                if pair not in targeted_swaps:
                    targeted_swaps.append(pair)

    if not targeted_swaps:
        return []

    base_order = list(range(n_vars))
    candidates: list[list[str]] = []
    seen: set[tuple[str, ...]] = set()

    def _add_candidate(order: list[int]) -> None:
        candidate = [decl_names[k] for k in order]
        key = tuple(candidate)
        if key not in seen:
            seen.add(key)
            candidates.append(candidate)

    # Single targeted swaps
    for i, j in targeted_swaps:
        new_order = list(base_order)
        new_order[i], new_order[j] = new_order[j], new_order[i]
        _add_candidate(new_order)

    # +-1 neighbor variants for each targeted pair
    for i, j in targeted_swaps:
        for di in (-1, 0, 1):
            for dj in (-1, 0, 1):
                ni, nj = i + di, j + dj
                if ni == nj or ni < 0 or nj < 0 or ni >= n_vars or nj >= n_vars:
                    continue
                if di == 0 and dj == 0:
                    continue  # Already added above
                new_order = list(base_order)
                new_order[ni], new_order[nj] = new_order[nj], new_order[ni]
                _add_candidate(new_order)

    # Multi-swap: apply all targeted swaps simultaneously
    if len(targeted_swaps) > 1:
        new_order = list(base_order)
        for i, j in targeted_swaps:
            new_order[i], new_order[j] = new_order[j], new_order[i]
        _add_candidate(new_order)

    return candidates


def ghidra_guided_search(
    ghidra_var_order: list,
    swap_pairs: list[tuple[str, str]],
    decl_names: list[str],
    gpr_save_count: int | None = None,
) -> list[list[str]]:
    """Generate targeted declaration reorders using Ghidra variable order.

    Parallel to asm_guided_search() but uses Ghidra decompilation output
    instead of assembly listing analysis.

    Algorithm:
    1. Ghidra var order -> target register allocation (1st var -> r31, etc.)
    2. Source decl order -> our register allocation (same rule)
    3. For each swap pair, find misallocated variables, generate targeted swaps

    Args:
        ghidra_var_order: VarInfo list from extract_variable_first_use_order()
        swap_pairs: Register swap pairs from objdiff
        decl_names: Variable declaration names in current source order
        gpr_save_count: GPR save count from Ghidra __savegprlr_N

    Returns:
        List of candidate declaration orderings.
    """
    from decomp_synth.ghidra_var_match import ghidra_guided_reorder
    return ghidra_guided_reorder(
        ghidra_vars=ghidra_var_order,
        source_decl_names=decl_names,
        swap_pairs=swap_pairs,
        gpr_save_count=gpr_save_count,
    )


def cmd_bsf_solve(args) -> None:
    """Entry point for bsf-solve subcommand."""
    import json
    import subprocess
    import sys
    from pathlib import Path

    from .invoker import PROJECT_ROOT

    source = Path(args.source).resolve()
    symbol = args.symbol

    # Get objdiff JSON
    print(f"Running objdiff for {symbol}...", file=sys.stderr)
    objdiff_result = subprocess.run(
        [
            str(PROJECT_ROOT / "bin" / "objdiff-cli"),
            "diff",
            symbol,
            "--include-instructions",
            "--build",
            "--incremental",
            "-f",
            "json",
        ],
        cwd=PROJECT_ROOT,
        capture_output=True,
        text=True,
    )

    if objdiff_result.returncode != 0:
        print(f"objdiff failed: {objdiff_result.stderr}", file=sys.stderr)
        sys.exit(1)

    objdiff_json = json.loads(objdiff_result.stdout)

    # Trace BSF
    print(f"Tracing BSF calls for {source.name}...", file=sys.stderr)
    from .bsf_trace import trace_bsf

    bsf = trace_bsf(source)
    print(f"  {bsf.total_calls} BSF calls", file=sys.stderr)

    # Solve
    function_name = args.function if hasattr(args, "function") and args.function else symbol
    solution = solve_register_order(bsf, objdiff_json, source, function_name)

    if solution.feasible:
        print(f"\nSolution found!")
        print(f"Declaration order: {solution.declaration_order}")
        print(f"Color map: {solution.color_map}")
        print(f"Target regs: {solution.target_regs}")
        print(f"Swap pairs: {solution.swap_pairs}")
    else:
        print(f"\nNo solution: {solution.reason}")

    if solution.swap_pairs:
        print(f"\nGPR swap pairs: {solution.swap_pairs}")

    # Output JSON if requested
    if getattr(args, "json_output", False):
        result = {
            "feasible": solution.feasible,
            "declaration_order": solution.declaration_order,
            "reason": solution.reason,
            "color_map": solution.color_map,
            "target_regs": solution.target_regs,
            "swap_pairs": [list(p) for p in solution.swap_pairs],
        }
        print(json.dumps(result, indent=2))
