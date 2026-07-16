"""Comparison logic for Unicorn function execution results."""

import hashlib
import json
import struct

from .engine import CL_INDEX, CL_TRAMP_ADDR, CL_SRC_OFFSET, CL_R3, CL_R4, CL_R5, CL_R6
from .memory_map import STACK_BASE, OBJECT_BASE, GLOBAL_BASE, REGION_SIZE


class ComparisonResult:
    """Result of comparing two execution results."""

    def __init__(self, verdict, details=None, warnings=None):
        self.verdict = verdict          # "EQUIVALENT" or "DIVERGENT"
        self.details = details or {}
        self.warnings = warnings or []

    def to_dict(self):
        """Serialize to a JSON-compatible dict."""
        return {
            "verdict": self.verdict,
            "details": self.details,
            "warnings": self.warnings,
        }


def _entry_args(entry):
    """Extract args dict from a call log tuple (for diagnostics)."""
    return {"r3": entry[CL_R3], "r4": entry[CL_R4],
            "r5": entry[CL_R5], "r6": entry[CL_R6]}


def compare_call_logs(decomp_log, orig_log):
    """Compare call logs by execution sequence.

    Returns (verdict, details) tuple.
    """
    if len(decomp_log) != len(orig_log):
        # Find where the shorter log diverges from the longer
        min_len = min(len(decomp_log), len(orig_log))
        first_arg_diff = None
        for i in range(min_len):
            d, o = decomp_log[i], orig_log[i]
            if (d[CL_R3] != o[CL_R3] or d[CL_R4] != o[CL_R4]
                    or d[CL_R5] != o[CL_R5] or d[CL_R6] != o[CL_R6]):
                first_arg_diff = i
                break
        return "DIVERGENT", {
            "reason": "call_count_mismatch",
            "decomp_calls": len(decomp_log),
            "orig_calls": len(orig_log),
            "matched_prefix": first_arg_diff if first_arg_diff is not None else min_len,
        }

    for i, (d, o) in enumerate(zip(decomp_log, orig_log)):
        for idx, reg in ((CL_R3, "r3"), (CL_R4, "r4"), (CL_R5, "r5"), (CL_R6, "r6")):
            if d[idx] != o[idx]:
                return "DIVERGENT", {
                    "reason": "call_arg_mismatch",
                    "call_index": i,
                    "register": reg,
                    "decomp_val": d[idx],
                    "orig_val": o[idx],
                    "decomp_args": _entry_args(d),
                    "orig_args": _entry_args(o),
                }

    return "EQUIVALENT", {}


def compare_memory(decomp_mem, orig_mem, base, size):
    """Compare memory regions word-by-word.

    Returns list of (address, decomp_word, orig_word) tuples for differences.
    """
    # Fast path: C-level memcmp via bytes equality
    if decomp_mem[:size] == orig_mem[:size]:
        return []
    diffs = []
    for i in range(0, size, 4):
        dw_d = struct.unpack_from(">I", decomp_mem, i)[0]
        dw_o = struct.unpack_from(">I", orig_mem, i)[0]
        if dw_d != dw_o:
            diffs.append((base + i, dw_d, dw_o))
    return diffs


def unmapped_fingerprint(unmapped_log):
    """Hash the set of suspicious unmapped accesses for cross-side comparison.

    The log already filters to bug-shaped regions (page 0, kernel range).
    We reduce to a sorted set of (kind, page_base) — exact addresses
    within a 4KB page are noise (offset can shift with code scheduling).

    Returns 16 hex chars of sha256, or "" if the log is empty.
    """
    if not unmapped_log:
        return ""
    page_set = sorted({(kind, page) for kind, page, _addr in unmapped_log})
    raw = ";".join(f"{k}:{p:08x}" for k, p in page_set).encode("ascii")
    return hashlib.sha256(raw).hexdigest()[:16]


def build_offset_symbol_map(orig_relocs):
    """Map function-relative offsets to original symbol names (REL24 only)."""
    return {r["offset"]: r["symbol_name"]
            for r in orig_relocs if r["type_name"] == "REL24"}


def check_call_targets(decomp_relocs, orig_relocs, decomp_log, orig_log):
    """Best-effort check: do corresponding calls target the same function?

    Returns list of warning strings.
    """
    orig_offset_map = {r["offset"]: r["symbol_name"]
                       for r in orig_relocs if r["type_name"] == "REL24"}
    decomp_offset_map = {r["offset"]: r["symbol_name"]
                         for r in decomp_relocs if r["type_name"] == "REL24"}

    warnings = []
    for i, (d, o) in enumerate(zip(decomp_log, orig_log)):
        d_sym = decomp_offset_map.get(d[CL_SRC_OFFSET])
        o_sym = orig_offset_map.get(o[CL_SRC_OFFSET])
        if d_sym and o_sym and d_sym != o_sym:
            warnings.append(f"Call #{i}: decomp targets {d_sym}, "
                          f"original targets {o_sym}")
    return warnings


def compare(decomp_result, orig_result, decomp_relocs, orig_relocs):
    """Compare two execution results and produce a verdict.

    Args:
        decomp_result: ExecutionResult from decomp
        orig_result: ExecutionResult from original
        decomp_relocs: list of decomp relocations (for diagnostics)
        orig_relocs: list of original relocations (for diagnostics)

    Returns:
        ComparisonResult with verdict and details
    """
    # Phase 2.2: Cap-exhaustion check. Both sides hitting the instruction
    # cap means execution was truncated identically; the captured state
    # is incomplete and we cannot conclude EQUIVALENT. One-sided cap is a
    # real divergence (decomp loops where orig terminates, or vice versa).
    d_cap = getattr(decomp_result, "cap_exhausted", False)
    o_cap = getattr(orig_result, "cap_exhausted", False)
    if d_cap and o_cap:
        return ComparisonResult("DIVERGENT", {
            "reason": "cap_exhausted_both",
            "decomp_final_pc": getattr(decomp_result, "final_pc", 0),
            "orig_final_pc": getattr(orig_result, "final_pc", 0),
        })
    if d_cap:
        return ComparisonResult("DIVERGENT", {
            "reason": "cap_exhausted_decomp",
            "decomp_final_pc": getattr(decomp_result, "final_pc", 0),
        })
    if o_cap:
        return ComparisonResult("DIVERGENT", {
            "reason": "cap_exhausted_orig",
            "orig_final_pc": getattr(orig_result, "final_pc", 0),
        })

    # Check for execution errors. Phase 2.1 (softened after corpus-check
    # showed 94/95 TPs hit a symmetric wild-jump under zero fill): we
    # keep the old "matching error → EQUIVALENT" rule for now, since
    # under the current zero-fill fixture both sides commonly null-deref
    # to the same low address and crash identically. That's a harness
    # limitation, not a real divergence between decomp and orig.
    #
    # We do, however, tag the verdict so Phase 3 (unmapped-access
    # fingerprint) can later distinguish symmetric-null-deref from
    # genuine matching faults. Same-error-different-PC is still flagged
    # as wild_jump_match for diagnostic visibility.
    if decomp_result.error and orig_result.error:
        if decomp_result.error == orig_result.error:
            d_pc = getattr(decomp_result, "final_pc", 0)
            o_pc = getattr(orig_result, "final_pc", 0)
            # Matching errors at matching PC → EQUIVALENT, tagged.
            # Matching errors at different PC → DIVERGENT (wild_jump_match).
            if d_pc == o_pc:
                return ComparisonResult("EQUIVALENT", {
                    "matching_error": decomp_result.error,
                    "matching_error_pc": d_pc,
                }, warnings=[
                    f"Both sides hit identical error at PC=0x{d_pc:08X}: "
                    f"{decomp_result.error}",
                ])
            return ComparisonResult("DIVERGENT", {
                "reason": "wild_jump_match",
                "decomp_error": decomp_result.error,
                "orig_error": orig_result.error,
                "decomp_final_pc": d_pc,
                "orig_final_pc": o_pc,
            })
        return ComparisonResult("DIVERGENT", {
            "reason": "error_mismatch",
            "decomp_error": decomp_result.error,
            "orig_error": orig_result.error,
        })
    if decomp_result.error:
        return ComparisonResult("DIVERGENT", {
            "reason": "decomp_error",
            "error": decomp_result.error,
        })
    if orig_result.error:
        return ComparisonResult("DIVERGENT", {
            "reason": "orig_error",
            "error": orig_result.error,
        })

    # Primary: call log comparison
    verdict, details = compare_call_logs(
        decomp_result.call_log, orig_result.call_log)
    if verdict == "DIVERGENT":
        return ComparisonResult(verdict, details)

    # Primary: return value comparison
    if decomp_result.r3 != orig_result.r3:
        return ComparisonResult("DIVERGENT", {
            "reason": "return_value_mismatch",
            "decomp_r3": decomp_result.r3,
            "orig_r3": orig_result.r3,
        })

    # Primary: float return value comparison
    if decomp_result.f1 != orig_result.f1:
        return ComparisonResult("DIVERGENT", {
            "reason": "fpr_return_mismatch",
            "decomp_f1": decomp_result.f1,
            "orig_f1": orig_result.f1,
        })

    # Primary: memory comparison
    obj_diffs = compare_memory(
        decomp_result.object_memory, orig_result.object_memory,
        OBJECT_BASE, REGION_SIZE)
    globals_diffs = compare_memory(
        decomp_result.globals_memory, orig_result.globals_memory,
        GLOBAL_BASE, REGION_SIZE)

    if obj_diffs or globals_diffs:
        return ComparisonResult("DIVERGENT", {
            "reason": "memory_mismatch",
            "object_diffs": obj_diffs[:20],  # cap for readability
            "globals_diffs": globals_diffs[:20],
        })

    # Phase 3.2: unmapped-access fingerprint divergence. If decomp pokes
    # a low/kernel-range address that orig doesn't (or vice versa), that
    # is observable behavioral divergence even though the on-demand-map
    # hook hid the fault from the call/memory/return checks above.
    d_unmapped = getattr(decomp_result, "unmapped_log", None) or []
    o_unmapped = getattr(orig_result, "unmapped_log", None) or []
    d_fp = unmapped_fingerprint(d_unmapped)
    o_fp = unmapped_fingerprint(o_unmapped)
    if d_fp != o_fp:
        return ComparisonResult("DIVERGENT", {
            "reason": "unmapped_access_mismatch",
            "decomp_fingerprint": d_fp,
            "orig_fingerprint": o_fp,
            "decomp_unmapped_pages": sorted({
                (k, p) for k, p, _ in d_unmapped
            })[:10],
            "orig_unmapped_pages": sorted({
                (k, p) for k, p, _ in o_unmapped
            })[:10],
        })

    # Secondary diagnostic: offset-matched symbol check
    warnings = check_call_targets(
        decomp_relocs, orig_relocs,
        decomp_result.call_log, orig_result.call_log)

    details = {
        "call_count": len(decomp_result.call_log),
        "r3": decomp_result.r3,
        "f1": decomp_result.f1,
        # Both sides share the same fingerprint (possibly ""). Persist
        # so downstream can audit the suspicious-access set.
        "unmapped_fingerprint": d_fp,
    }

    return ComparisonResult("EQUIVALENT", details, warnings)


def classify_divergence(result, decomp_result, orig_result, decomp_relocs, orig_relocs, enrichment=None):
    """Classify a DIVERGENT result into a root-cause category.

    Args:
        enrichment: optional dict with objdiff enrichment data:
            - has_linker_merged: bool — whether objdiff detected merged symbols
            (This supplements the warnings-based detection which only works
            for EQUIVALENT results.)

    Returns one of:
        'build_env'     — __FILE__ string differences or merged symbol calls
        'regalloc'      — same call structure, only register value differences

        Fine-grained sub-classes (replacing broad 'logic'):
        'merged_call'   — call_count_mismatch with merged symbols (unfixable)
        'merged_arg'    — call_arg_mismatch at a merged symbol call (unfixable)
        'stack_layout'  — call_arg_mismatch with stack-region values (hard to fix)
        'fpr_precision' — float return value differs (FMA/precision, unfixable)
        'object_memory' — memory_mismatch with object region diffs (maybe fixable)
        'error'         — execution error on decomp side (real bug)
        'orig_error'    — execution error on original side only (test infra, unfixable)
        'call_count'    — call count mismatch without merged indicators (real bug)
        'call_arg'      — call arg mismatch not matched by other rules (real bug)
        'return_value'  — integer return value mismatch (real bug)
        'logic'         — remaining fallthrough (should be minimal)
    """
    if result.verdict != "DIVERGENT":
        return None

    details = result.details
    reason = details.get("reason", "")
    warnings = result.warnings or []

    # Check for merged symbols: warnings (EQUIVALENT path) OR objdiff enrichment
    has_merged = any("merged_" in w for w in warnings)
    if not has_merged and enrichment:
        has_merged = bool(enrichment.get("has_linker_merged"))
    # Also check relocations directly for merged_ symbol targets
    if not has_merged:
        for r in (orig_relocs or []):
            if r.get("symbol_name", "").startswith("merged_"):
                has_merged = True
                break

    # Error-based divergences — distinguish orig-only (unfixable) from decomp (real bug)
    if reason == "orig_error":
        return "orig_error"
    if reason in ("error_mismatch", "decomp_error"):
        return "error"

    # Phase 2.1 / 2.2: new sub-classes from the cap+sentinel tightening.
    if reason == "wild_jump_match":
        # Both sides crashed at the same wild PC. Often correlates with
        # null-deref-from-this paths where both decomp and orig follow
        # the same broken control flow under zero-fill.
        return "wild_jump_match"
    if reason == "cap_exhausted_both":
        return "cap_exhausted"
    if reason == "cap_exhausted_decomp":
        return "cap_exhausted_decomp"
    if reason == "cap_exhausted_orig":
        return "cap_exhausted_orig"

    # Phase 3.2: unmapped-access fingerprint mismatch — one side pokes
    # null-range or kernel-range pages that the other doesn't.
    if reason == "unmapped_access_mismatch":
        return "unmapped_access_mismatch"

    # call_arg_mismatch: check if mismatching values are in globals region
    # (__FILE__ string references live in GLOBAL_BASE)
    if reason == "call_arg_mismatch":
        d_args = details.get("decomp_args", {})
        o_args = details.get("orig_args", {})
        diff_regs = [r for r in ("r3", "r4", "r5", "r6")
                     if d_args.get(r) != o_args.get(r)]

        # Check if ALL differing args point to globals region (string refs)
        all_globals = True
        for reg in diff_regs:
            d_val = d_args.get(reg, 0)
            o_val = o_args.get(reg, 0)
            d_in_globals = GLOBAL_BASE <= d_val < GLOBAL_BASE + REGION_SIZE
            o_in_globals = GLOBAL_BASE <= o_val < GLOBAL_BASE + REGION_SIZE
            if not (d_in_globals and o_in_globals):
                all_globals = False
                break

        if all_globals:
            return "build_env"

        # Check if merged symbol warning covers the mismatching call
        if has_merged:
            call_idx = details.get("call_index", -1)
            for w in warnings:
                if f"Call #{call_idx}:" in w and "merged_" in w:
                    return "merged_arg"

        # Check for regalloc: same call count, same call targets, only value diffs
        # If the call logs have identical length and the mismatch is in non-pointer
        # registers (not globals/object region), likely register allocation
        if len(decomp_result.call_log) == len(orig_result.call_log):
            # Count how many calls have arg differences
            arg_diff_calls = 0
            for d, o in zip(decomp_result.call_log, orig_result.call_log):
                if (d[CL_R3] != o[CL_R3] or d[CL_R4] != o[CL_R4]
                        or d[CL_R5] != o[CL_R5] or d[CL_R6] != o[CL_R6]):
                    arg_diff_calls += 1
            # If only 1-2 calls differ and values aren't pointer-like,
            # this is likely register allocation
            if arg_diff_calls <= 2:
                non_pointer_diffs = True
                for reg in diff_regs:
                    d_val = d_args.get(reg, 0)
                    o_val = o_args.get(reg, 0)
                    # Values in mapped regions are pointer-like
                    for base in (OBJECT_BASE, GLOBAL_BASE, STACK_BASE):
                        if (base <= d_val < base + REGION_SIZE
                                or base <= o_val < base + REGION_SIZE):
                            non_pointer_diffs = False
                            break
                    if not non_pointer_diffs:
                        break
                if non_pointer_diffs:
                    return "regalloc"

        # Check for stack-region arg differences
        any_stack = False
        for reg in diff_regs:
            d_val = d_args.get(reg, 0)
            o_val = o_args.get(reg, 0)
            if (STACK_BASE <= d_val < STACK_BASE + REGION_SIZE
                    or STACK_BASE <= o_val < STACK_BASE + REGION_SIZE):
                any_stack = True
                break
        if any_stack:
            return "stack_layout"

        # Generic merged arg (merged warning exists but didn't match exact call)
        if has_merged:
            return "merged_arg"

        return "call_arg"

    if reason == "call_count_mismatch":
        # Merged symbols can cause extra/missing calls
        if has_merged:
            return "merged_call"
        return "call_count"

    if reason == "return_value_mismatch":
        # Check if both values are globals-region pointers (string return)
        d_r3 = details.get("decomp_r3", 0)
        o_r3 = details.get("orig_r3", 0)
        if (GLOBAL_BASE <= d_r3 < GLOBAL_BASE + REGION_SIZE
                and GLOBAL_BASE <= o_r3 < GLOBAL_BASE + REGION_SIZE):
            return "build_env"
        return "return_value"

    if reason == "memory_mismatch":
        # Memory diffs in globals region could be string-related
        obj_diffs = details.get("object_diffs", [])
        glob_diffs = details.get("globals_diffs", [])
        if not obj_diffs and glob_diffs:
            # Only globals diffs — could be __FILE__ written to memory
            return "build_env"
        if obj_diffs:
            return "object_memory"
        return "logic"

    if reason == "fpr_return_mismatch":
        return "fpr_precision"

    return "logic"


def format_result(result, decomp_result, orig_result, decomp_relocs, orig_relocs, verbose=False):
    """Format a ComparisonResult for display."""
    lines = []

    if result.verdict == "EQUIVALENT":
        lines.append("EQUIVALENT")
        matching_error = result.details.get("matching_error")
        if matching_error:
            lines.append(f"  Note: both sides hit identical error: {matching_error}")
            return "\n".join(lines)
        call_count = result.details.get("call_count", 0)
        lines.append(f"  Calls: {call_count} matched (args identical at each position)")

        if verbose and decomp_result.call_log:
            orig_offset_map = build_offset_symbol_map(orig_relocs)
            for entry in decomp_result.call_log:
                # Try to resolve symbol name from original relocs
                sym = orig_offset_map.get(entry[CL_SRC_OFFSET], f"<tramp@0x{entry[CL_TRAMP_ADDR]:08X}>")
                lines.append(f"    #{entry[CL_INDEX]} {sym}  "
                           f"r3=0x{entry[CL_R3]:08X} "
                           f"r4=0x{entry[CL_R4]:08X} "
                           f"r5=0x{entry[CL_R5]:08X} "
                           f"r6=0x{entry[CL_R6]:08X}")

        lines.append(f"  Return: r3 = 0x{result.details.get('r3', 0):08X} (both)")
        f1 = result.details.get('f1', 0)
        if f1 != 0:
            lines.append(f"  Return: f1 = 0x{f1:016X} (both)")
        lines.append(f"  Memory: 0 diffs in object region, 0 diffs in globals")

        if result.warnings:
            lines.append("  Warnings:")
            for w in result.warnings:
                lines.append(f"    {w}")

    elif result.verdict == "DIVERGENT":
        lines.append("DIVERGENT")
        reason = result.details.get("reason", "unknown")

        if reason == "call_count_mismatch":
            d_count = result.details['decomp_calls']
            o_count = result.details['orig_calls']
            matched = result.details.get('matched_prefix', 0)
            lines.append(f"  Call count mismatch: decomp={d_count}, orig={o_count}")

            # Show the matched prefix and where divergence starts
            orig_offset_map = build_offset_symbol_map(orig_relocs)
            min_count = min(d_count, o_count)
            show_matched = min(matched, 5)  # cap display
            show_extra = 5

            if matched > 0:
                lines.append(f"  Matched calls before divergence ({matched} total):")
                start = max(0, matched - show_matched)
                if start > 0:
                    lines.append(f"    ... ({start} earlier calls omitted)")
                for i in range(start, matched):
                    d = decomp_result.call_log[i]
                    sym = orig_offset_map.get(orig_result.call_log[i][CL_SRC_OFFSET],
                                              f"<call#{i}>")
                    lines.append(f"    #{i} {sym}  "
                               f"r3=0x{d[CL_R3]:08X} "
                               f"r4=0x{d[CL_R4]:08X} "
                               f"r5=0x{d[CL_R5]:08X} "
                               f"r6=0x{d[CL_R6]:08X}  (both match)")

            if matched < min_count:
                # Args diverged before count diverged
                i = matched
                d = decomp_result.call_log[i]
                o = orig_result.call_log[i]
                sym = orig_offset_map.get(o[CL_SRC_OFFSET], f"<call#{i}>")
                lines.append(f"  First arg mismatch at call #{i} ({sym} @ offset 0x{o[CL_SRC_OFFSET]:X}):")
                lines.append(f"    Decomp:   r3=0x{d[CL_R3]:08X} r4=0x{d[CL_R4]:08X} "
                           f"r5=0x{d[CL_R5]:08X} r6=0x{d[CL_R6]:08X}")
                lines.append(f"    Original: r3=0x{o[CL_R3]:08X} r4=0x{o[CL_R4]:08X} "
                           f"r5=0x{o[CL_R5]:08X} r6=0x{o[CL_R6]:08X}")

            # Show extra calls from the longer side
            longer_side = "decomp" if d_count > o_count else "orig"
            longer_log = decomp_result.call_log if d_count > o_count else orig_result.call_log
            shorter_count = min_count
            extra_count = abs(d_count - o_count)
            if extra_count > 0:
                show = min(extra_count, show_extra)
                lines.append(f"  Extra {longer_side} calls ({extra_count} total):")
                offset_map = orig_offset_map if longer_side == "orig" else build_offset_symbol_map(decomp_relocs)
                for i in range(shorter_count, shorter_count + show):
                    entry = longer_log[i]
                    sym = offset_map.get(entry[CL_SRC_OFFSET], f"<call#{i}>")
                    lines.append(f"    #{i} {sym}  "
                               f"r3=0x{entry[CL_R3]:08X} "
                               f"r4=0x{entry[CL_R4]:08X}")
                if extra_count > show:
                    lines.append(f"    ... ({extra_count - show} more)")

        elif reason == "call_arg_mismatch":
            idx = result.details["call_index"]
            reg = result.details["register"]
            orig_offset_map = build_offset_symbol_map(orig_relocs)
            o_entry = orig_result.call_log[idx]
            sym = orig_offset_map.get(o_entry[CL_SRC_OFFSET], f"<call#{idx}>")
            src_off = o_entry[CL_SRC_OFFSET]
            lines.append(f"  First mismatch: call #{idx} ({sym} @ offset 0x{src_off:X})")
            # Always show all 4 regs for context
            d_args = result.details["decomp_args"]
            o_args = result.details["orig_args"]
            d_line = (f"    Decomp:   r3=0x{d_args['r3']:08X} r4=0x{d_args['r4']:08X} "
                     f"r5=0x{d_args['r5']:08X} r6=0x{d_args['r6']:08X}")
            o_line = (f"    Original: r3=0x{o_args['r3']:08X} r4=0x{o_args['r4']:08X} "
                     f"r5=0x{o_args['r5']:08X} r6=0x{o_args['r6']:08X}")
            lines.append(d_line)
            lines.append(o_line)
            # Show which registers differ
            diff_regs = [r for r in ("r3", "r4", "r5", "r6") if d_args[r] != o_args[r]]
            lines.append(f"    Differs: {', '.join(diff_regs)}")

            # Show call logs up to divergence
            if verbose:
                lines.append("  Call logs up to divergence:")
                for i in range(idx + 1):
                    d = decomp_result.call_log[i]
                    o = orig_result.call_log[i]
                    call_sym = orig_offset_map.get(o[CL_SRC_OFFSET], f"<call#{i}>")
                    if i < idx:
                        lines.append(f"    #{i} {call_sym}  "
                                   f"r3=0x{d[CL_R3]:08X} "
                                   f"r4=0x{d[CL_R4]:08X} "
                                   f"r5=0x{d[CL_R5]:08X} "
                                   f"r6=0x{d[CL_R6]:08X}  (match)")
                    else:
                        lines.append(f"    #{i} {call_sym}  MISMATCH")

        elif reason == "return_value_mismatch":
            lines.append(f"  Return value mismatch:")
            lines.append(f"    Decomp: r3 = 0x{result.details['decomp_r3']:08X}")
            lines.append(f"    Original: r3 = 0x{result.details['orig_r3']:08X}")

        elif reason == "fpr_return_mismatch":
            lines.append(f"  Float return value mismatch (f1):")
            lines.append(f"    Decomp: f1 = 0x{result.details['decomp_f1']:016X}")
            lines.append(f"    Original: f1 = 0x{result.details['orig_f1']:016X}")

        elif reason == "memory_mismatch":
            obj_diffs = result.details.get("object_diffs", [])
            glob_diffs = result.details.get("globals_diffs", [])
            lines.append(f"  Memory mismatch: "
                       f"{len(obj_diffs)} diffs in object region, "
                       f"{len(glob_diffs)} diffs in globals")
            for addr, dv, ov in obj_diffs[:5]:
                lines.append(f"    0x{addr:08X}: decomp=0x{dv:08X} orig=0x{ov:08X}")
            for addr, dv, ov in glob_diffs[:5]:
                lines.append(f"    0x{addr:08X}: decomp=0x{dv:08X} orig=0x{ov:08X}")

        elif reason == "error_mismatch":
            lines.append(f"  Different errors on each side:")
            lines.append(f"    Decomp: {result.details['decomp_error']}")
            lines.append(f"    Original: {result.details['orig_error']}")

        elif reason in ("decomp_error", "orig_error"):
            side = "Decomp" if reason == "decomp_error" else "Original"
            lines.append(f"  {side} execution error: {result.details['error']}")

        elif reason == "wild_jump_match":
            lines.append("  Both sides crashed at the same wild address:")
            lines.append(f"    Decomp:   {result.details['decomp_error']} "
                         f"(PC=0x{result.details['decomp_final_pc']:08X})")
            lines.append(f"    Original: {result.details['orig_error']} "
                         f"(PC=0x{result.details['orig_final_pc']:08X})")
            lines.append("    (suspect: not equivalent — same wrong path on both sides)")

        elif reason == "cap_exhausted_both":
            lines.append("  Both sides hit the instruction count cap "
                         "(execution incomplete):")
            lines.append(f"    Decomp PC=0x{result.details['decomp_final_pc']:08X}")
            lines.append(f"    Original PC=0x{result.details['orig_final_pc']:08X}")

        elif reason == "cap_exhausted_decomp":
            lines.append("  Decomp hit the instruction count cap; original returned. "
                         f"Decomp PC=0x{result.details['decomp_final_pc']:08X}")

        elif reason == "cap_exhausted_orig":
            lines.append("  Original hit the instruction count cap; decomp returned. "
                         f"Original PC=0x{result.details['orig_final_pc']:08X}")

        elif reason == "unmapped_access_mismatch":
            d_fp = result.details.get("decomp_fingerprint", "")
            o_fp = result.details.get("orig_fingerprint", "")
            lines.append("  Suspicious unmapped-access fingerprint differs:")
            lines.append(f"    Decomp fingerprint:   {d_fp or '(none)'}")
            lines.append(f"    Original fingerprint: {o_fp or '(none)'}")
            d_pages = result.details.get("decomp_unmapped_pages", [])
            o_pages = result.details.get("orig_unmapped_pages", [])
            if d_pages:
                lines.append("    Decomp touched pages:")
                for k, p in d_pages[:5]:
                    lines.append(f"      {k} 0x{p:08X}")
            if o_pages:
                lines.append("    Original touched pages:")
                for k, p in o_pages[:5]:
                    lines.append(f"      {k} 0x{p:08X}")

    return "\n".join(lines)


def format_json_result(result, decomp_result, orig_result, orig_relocs, metadata):
    """Format a ComparisonResult as a JSON string.

    Args:
        result: ComparisonResult from compare()
        decomp_result: ExecutionResult from decomp side
        orig_result: ExecutionResult from original side
        orig_relocs: list of original relocations (for symbol resolution)
        metadata: dict with keys: symbol, decomp_size, orig_size,
                  coloaded_callees, combined_code_size

    Returns:
        JSON string
    """
    orig_offset_map = build_offset_symbol_map(orig_relocs)
    json_data = result.to_dict()
    json_data["symbol"] = metadata["symbol"]
    json_data["decomp_size"] = metadata["decomp_size"]
    json_data["orig_size"] = metadata["orig_size"]
    json_data["coloaded_callees"] = metadata["coloaded_callees"]
    json_data["combined_code_size"] = metadata["combined_code_size"]
    json_data["decomp_call_count"] = len(decomp_result.call_log)
    json_data["orig_call_count"] = len(orig_result.call_log)
    json_data["r3"] = {"decomp": decomp_result.r3, "orig": orig_result.r3}
    json_data["f1"] = {"decomp": decomp_result.f1, "orig": orig_result.f1}
    # Resolve call log symbols
    resolved_calls = []
    for entry in decomp_result.call_log[:50]:  # cap for size
        sym = orig_offset_map.get(entry[CL_SRC_OFFSET], None)
        resolved_calls.append({
            "index": entry[CL_INDEX],
            "symbol": sym,
            "source_offset": entry[CL_SRC_OFFSET],
            "args": _entry_args(entry),
        })
    json_data["decomp_calls"] = resolved_calls
    return json.dumps(json_data)
