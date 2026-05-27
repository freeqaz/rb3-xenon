#!/usr/bin/env python3
"""Inspect objdiff JSON output for specific mismatch types with context.

Usage:
    # Generate JSON diff first:
    ./bin/objdiff-cli diff "symbol_name" --include-instructions --build --incremental -f json -o /tmp/claude/diff.json

    # Then inspect it:
    python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json                  # show all non-equal
    python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json diff_op          # only diff_op
    python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json replace          # only replace
    python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json insert,delete    # insert and delete
    python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json diff_op -C 8     # 8 lines context
    python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json all              # every instruction
    python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json --range 950-970  # specific index range
    python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json --summary        # count by match type

    # Analysis modes:
    python3 scripts/analysis/diff_inspect.py --symbol "?Foo@@QAAXXZ" --attributed   # source-attributed regions
    python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json --diagnose       # root cause analysis
    python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json --clusters       # insert/delete clusters
    python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json --regswaps       # register swap pairs
    python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json --offsets        # offset shift analysis
    python3 scripts/analysis/diff_inspect.py /tmp/claude/diff.json --replaces       # replace categorization
    python3 scripts/analysis/diff_inspect.py --compare base.json cand.json           # delta table for two diffs

    # Direct invocation (runs objdiff internally):
    python3 scripts/analysis/diff_inspect.py --symbol "symbol_name" --diagnose
"""

import argparse
import hashlib
import json
import os
import re
import subprocess
import sys
from collections import Counter, defaultdict
from pathlib import Path


# ── Formatting helpers ──────────────────────────────────────────────────────

def fmt_instr(side: dict | None) -> str:
    if not side:
        return "---"
    op = side.get("opcode", "???")
    args = side.get("args", "")
    return f"{op:8s} {args}"


def diff_annotation(ins: dict) -> str:
    """Return a short annotation describing what changed in a diff_arg instruction."""
    bd = ins.get("diff_breakdown")
    if not bd:
        return ""
    parts = []
    for arg in bd.get("arguments", []):
        at = arg.get("arg_type", "")
        tgt = arg.get("target", {})
        base = arg.get("base", {})
        tv = tgt.get("value")
        bv = base.get("value")
        if at == "register":
            parts.append(f"reg:{tv}->{bv}")
        elif at == "symbol":
            # Shorten symbol names
            ts = str(tv)[:20] if tv else "?"
            bs = str(bv)[:20] if bv else "?"
            if ts != bs:
                parts.append("sym")
        elif at == "immediate":
            if isinstance(tv, (int, float)) and isinstance(bv, (int, float)):
                delta = bv - tv
                parts.append(f"off:{delta:+d}")
            else:
                parts.append("imm")
        elif at == "branch_dest":
            parts.append("br")
    return " [" + ", ".join(parts) + "]" if parts else ""


def print_instr(ins: dict, highlight: bool = False, annotate: bool = False):
    idx = ins["index"]
    mt = ins.get("match_type", "")
    t = ins.get("target")
    b = ins.get("base")
    marker = ">>>" if highlight else "   "
    t_str = fmt_instr(t)
    b_str = fmt_instr(b)
    ann = diff_annotation(ins) if annotate and mt == "diff_arg" else ""
    print(f"{marker} {idx:4d} {mt:12s}  TGT: {t_str:40s}  SRC: {b_str}{ann}")


# ── Analysis: parse diff_breakdown ──────────────────────────────────────────

def parse_breakdowns(instrs):
    """Extract structured data from all diff_breakdown entries."""
    reg_swaps = []      # (index, target_reg, base_reg)
    offset_diffs = []   # (index, target_val, base_val, delta)
    symbol_diffs = []   # (index, target_sym, base_sym)
    branch_diffs = []   # (index,)

    for ins in instrs:
        bd = ins.get("diff_breakdown")
        if not bd:
            continue
        idx = ins["index"]
        for arg in bd.get("arguments", []):
            at = arg.get("arg_type", "")
            tgt = arg.get("target", {})
            base = arg.get("base", {})
            tv = tgt.get("value")
            bv = base.get("value")

            if at == "register":
                if tv and bv and tv != bv:
                    reg_swaps.append((idx, str(tv), str(bv)))
            elif at == "immediate":
                if isinstance(tv, (int, float)) and isinstance(bv, (int, float)) and tv != bv:
                    offset_diffs.append((idx, tv, bv, bv - tv))
                elif tv != bv:
                    # String values (symbol embedded in immediate)
                    symbol_diffs.append((idx, str(tv), str(bv)))
            elif at == "symbol":
                if tv != bv:
                    symbol_diffs.append((idx, str(tv)[:60], str(bv)[:60]))
            elif at == "branch_dest":
                branch_diffs.append((idx,))

    return reg_swaps, offset_diffs, symbol_diffs, branch_diffs


def compute_reg_swap_pairs(reg_swaps):
    """Group register swaps into pairs with counts and index ranges."""
    pair_data = defaultdict(lambda: {"count": 0, "first": 99999, "last": 0})
    for idx, tgt_reg, base_reg in reg_swaps:
        # Normalize pair order for grouping
        pair = tuple(sorted([tgt_reg, base_reg]))
        pair_data[pair]["count"] += 1
        pair_data[pair]["first"] = min(pair_data[pair]["first"], idx)
        pair_data[pair]["last"] = max(pair_data[pair]["last"], idx)
    return pair_data


def compute_offset_histogram(offset_diffs):
    """Build a histogram of offset deltas."""
    deltas = Counter()
    for idx, tv, bv, delta in offset_diffs:
        deltas[delta] += 1
    return deltas


def categorize_replaces(instrs):
    """Categorize replace instructions into symbol-reloc noise vs real structural differences."""
    static_sym = 0
    real_replace = 0
    real_examples = []
    for ins in instrs:
        if ins.get("match_type") != "replace":
            continue
        t = ins.get("target", {})
        b = ins.get("base", {})
        t_args = t.get("typed_args", [])
        b_args = b.get("typed_args", [])
        # If same opcode and base has more Symbol-type args, it's a relocation difference
        if t.get("opcode") == b.get("opcode") and len(b_args) > len(t_args):
            extra_b = [a for a in b_args if a.get("type") == "Symbol"]
            extra_t = [a for a in t_args if a.get("type") == "Symbol"]
            if len(extra_b) > len(extra_t):
                static_sym += 1
                continue
        real_replace += 1
        real_examples.append(ins)
    return static_sym, real_replace, real_examples


def find_clusters(instrs, match_types=("insert", "delete"), gap=2):
    """Group instructions of given match_types into contiguous clusters."""
    targets = [(i, ins) for i, ins in enumerate(instrs)
                if ins.get("match_type") in match_types]
    if not targets:
        return []

    clusters = []
    current = [targets[0]]
    for t in targets[1:]:
        if t[0] - current[-1][0] <= gap + 1:
            current.append(t)
        else:
            clusters.append(current)
            current = [t]
    clusters.append(current)
    return clusters


def parse_hotspot_ranges(spec: str):
    """Parse hotspot range list like '930-1075,1235-1415'."""
    ranges = []
    for chunk in spec.split(","):
        chunk = chunk.strip()
        if not chunk:
            continue
        if "-" not in chunk:
            raise ValueError(f"Invalid hotspot range '{chunk}' (expected N-M)")
        lo_s, hi_s = chunk.split("-", 1)
        lo, hi = int(lo_s), int(hi_s)
        if lo > hi:
            lo, hi = hi, lo
        ranges.append((lo, hi))
    return ranges


def count_indel_clusters_in_range(instrs, lo, hi, gap=2):
    """Count insert/delete clusters in an index range."""
    idxs = [
        ins["index"]
        for ins in instrs
        if lo <= ins["index"] <= hi and ins.get("match_type") in ("insert", "delete")
    ]
    if not idxs:
        return 0
    idxs.sort()
    clusters = 1
    for i in range(1, len(idxs)):
        if idxs[i] - idxs[i - 1] > gap + 1:
            clusters += 1
    return clusters


def bool_mask_proxy_count(instrs):
    """Heuristic count for bool-mask style mismatches from instruction stream only."""
    mask_roots = {"clrlwi", "rlwinm", "srwi", "extrwi", "andi", "cntlzw", "subfe", "addze"}
    bit_re = re.compile(r"(?:^|[,\s])(24|31)(?:$|[,\s])")
    count = 0
    for ins in instrs:
        if ins.get("match_type") not in ("diff_op", "replace"):
            continue
        t = ins.get("target") or {}
        b = ins.get("base") or {}
        t_op = t.get("opcode", "")
        b_op = b.get("opcode", "")
        t_root = t_op.rstrip(".")
        b_root = b_op.rstrip(".")
        text = f"{t_op} {t.get('args','')} || {b_op} {b.get('args','')}"

        has_mask_root = t_root in mask_roots or b_root in mask_roots
        dot_flip = (t_root == b_root) and (t_op.endswith(".") != b_op.endswith("."))
        has_bit = bool(bit_re.search(text))
        if has_mask_root and (dot_flip or has_bit):
            count += 1
    return count


def summarize_for_compare(instrs):
    """Collect compact metrics for compare mode."""
    counts = Counter(ins.get("match_type") for ins in instrs)
    reg_swaps, _, _, _ = parse_breakdowns(instrs)
    pair_data = compute_reg_swap_pairs(reg_swaps)
    reg_pair_counts = {pair: data["count"] for pair, data in pair_data.items()}
    return {
        "counts": counts,
        "reg_pair_counts": reg_pair_counts,
        "bool_mask_proxy": bool_mask_proxy_count(instrs),
    }


def cmd_compare(base_data, cand_data, hotspot_spec):
    """Compare two objdiff JSON payloads and print metric deltas."""
    base_instrs = base_data.get("instructions", [])
    cand_instrs = cand_data.get("instructions", [])
    if not base_instrs or not cand_instrs:
        print("Error: both baseline and candidate must contain instructions", file=sys.stderr)
        sys.exit(1)

    base = summarize_for_compare(base_instrs)
    cand = summarize_for_compare(cand_instrs)
    metrics = ["insert", "delete", "replace", "diff_op"]

    print("=" * 70)
    print("COMPARE TWO DIFFS")
    print("=" * 70)
    print()

    base_match = base_data.get("fuzzy_match_percent")
    cand_match = cand_data.get("fuzzy_match_percent")
    if isinstance(base_match, (int, float)) and isinstance(cand_match, (int, float)):
        print(f"Match %: baseline {base_match:.2f}  candidate {cand_match:.2f}  "
              f"delta {cand_match - base_match:+.2f}")
        print()

    print("Mismatch deltas (candidate - baseline):")
    print(f"{'Metric':16s} {'Baseline':>9s} {'Candidate':>10s} {'Delta':>8s}")
    print(f"{'─' * 16} {'─' * 9} {'─' * 10} {'─' * 8}")
    for m in metrics:
        b = base["counts"].get(m, 0)
        c = cand["counts"].get(m, 0)
        print(f"{m:16s} {b:9d} {c:10d} {c - b:+8d}")
    b_noneq = len(base_instrs) - base["counts"].get("equal", 0)
    c_noneq = len(cand_instrs) - cand["counts"].get("equal", 0)
    print(f"{'total_non_equal':16s} {b_noneq:9d} {c_noneq:10d} {c_noneq - b_noneq:+8d}")
    print()

    b_bool = base["bool_mask_proxy"]
    c_bool = cand["bool_mask_proxy"]
    print(f"BOOL_MASK proxy delta: baseline {b_bool}, candidate {c_bool}, delta {c_bool - b_bool:+d}")
    print("  (proxy from instruction patterns; final BOOL_MASK verdict should come from run_objdiff)")
    print()

    print("Top register-pair deltas (candidate - baseline):")
    pair_keys = set(base["reg_pair_counts"]) | set(cand["reg_pair_counts"])
    pair_deltas = []
    for pair in pair_keys:
        b = base["reg_pair_counts"].get(pair, 0)
        c = cand["reg_pair_counts"].get(pair, 0)
        d = c - b
        if d:
            pair_deltas.append((abs(d), d, pair, b, c))
    if not pair_deltas:
        print("  No register-pair count changes.")
    else:
        print(f"  {'Pair':16s} {'Baseline':>9s} {'Candidate':>10s} {'Delta':>8s}")
        print(f"  {'─' * 16} {'─' * 9} {'─' * 10} {'─' * 8}")
        for _, d, pair, b, c in sorted(pair_deltas, reverse=True)[:12]:
            p0, p1 = pair
            print(f"  {p0}<->{p1:11s} {b:9d} {c:10d} {d:+8d}")
    print()

    try:
        hotspots = parse_hotspot_ranges(hotspot_spec)
    except ValueError as exc:
        print(f"Error: {exc}", file=sys.stderr)
        sys.exit(1)

    print("Hotspot deltas (candidate - baseline):")
    print(f"{'Range':14s} {'I+D Δ':>8s} {'Clusters Δ':>11s} {'Replace Δ':>10s} {'diff_op Δ':>10s}")
    print(f"{'─' * 14} {'─' * 8} {'─' * 11} {'─' * 10} {'─' * 10}")
    for lo, hi in hotspots:
        def in_range(ins):
            idx = ins["index"]
            return lo <= idx <= hi

        b_region = [ins for ins in base_instrs if in_range(ins)]
        c_region = [ins for ins in cand_instrs if in_range(ins)]
        b_counts = Counter(ins.get("match_type") for ins in b_region)
        c_counts = Counter(ins.get("match_type") for ins in c_region)

        b_indel = b_counts.get("insert", 0) + b_counts.get("delete", 0)
        c_indel = c_counts.get("insert", 0) + c_counts.get("delete", 0)
        b_clusters = count_indel_clusters_in_range(base_instrs, lo, hi)
        c_clusters = count_indel_clusters_in_range(cand_instrs, lo, hi)
        b_rep = b_counts.get("replace", 0)
        c_rep = c_counts.get("replace", 0)
        b_op = b_counts.get("diff_op", 0)
        c_op = c_counts.get("diff_op", 0)

        label = f"{lo}-{hi}"
        print(f"{label:14s} {c_indel - b_indel:+8d} {c_clusters - b_clusters:+11d} "
              f"{c_rep - b_rep:+10d} {c_op - b_op:+10d}")

# ── Diagnose mode ───────────────────────────────────────────────────────────

def cmd_diagnose(instrs):
    """Root cause analysis: why doesn't this function match?"""
    counts = Counter(ins["match_type"] for ins in instrs)
    total = len(instrs)
    equal_count = counts.get("equal", 0)
    match_pct = 100.0 * equal_count / total if total else 0

    # ── 1. Match Summary ──
    print("=" * 70)
    print("DIAGNOSIS REPORT")
    print("=" * 70)
    print()
    print(f"Total instructions: {total}")
    print(f"Match estimate:     ~{match_pct:.1f}% ({equal_count}/{total} equal)")
    print()
    print("Instruction breakdown:")
    for mt, count in counts.most_common():
        pct = 100.0 * count / total
        print(f"  {mt:12s}: {count:5d} ({pct:5.1f}%)")

    # ── Parse all diff_breakdown data ──
    reg_swaps, offset_diffs, symbol_diffs, branch_diffs = parse_breakdowns(instrs)
    pair_data = compute_reg_swap_pairs(reg_swaps)
    delta_hist = compute_offset_histogram(offset_diffs)

    # ── 2. Root Causes ──
    print()
    print("-" * 70)
    print("ROOT CAUSES")
    print("-" * 70)

    # Stack frame / offset shift
    if delta_hist:
        dominant_delta, dominant_count = delta_hist.most_common(1)[0]
        print()
        print(f"  Stack/offset shift: dominant delta = {dominant_delta:+d} "
              f"({dominant_count} instructions)")
        print("  Top offset deltas:")
        for delta, count in delta_hist.most_common(8):
            print(f"    {delta:+6d}: {count:4d} instructions")
        total_offset_explained = sum(delta_hist.values())
    else:
        dominant_delta = None
        total_offset_explained = 0

    # Register swaps
    if pair_data:
        print()
        print(f"  Register swaps: {len(reg_swaps)} instructions across "
              f"{len(pair_data)} pairs")
        print("  Top swap pairs:")
        for pair, data in sorted(pair_data.items(),
                                  key=lambda x: -x[1]["count"]):
            if data["count"] < 2:
                continue
            p0, p1 = pair
            kind = "GPR" if p0.startswith("r") else "FPR" if p0.startswith("f") else "???"
            print(f"    {p0:4s} <-> {p1:4s}: {data['count']:4d} "
                  f"(idx {data['first']}-{data['last']}) [{kind}]")
        total_reg_explained = len(reg_swaps)
    else:
        total_reg_explained = 0

    # Symbol relocations
    if symbol_diffs:
        print()
        print(f"  Symbol relocations: {len(symbol_diffs)} arg differences")
        # Count how many instructions have at least one symbol diff
        sym_instrs = len(set(idx for idx, _, _ in symbol_diffs))
        print(f"    Across {sym_instrs} instructions")
    else:
        sym_instrs = 0

    # Branch dest diffs
    if branch_diffs:
        print()
        print(f"  Branch destination diffs: {len(branch_diffs)} (address relocation noise)")

    # ── 3. Actionable Mismatches ──
    print()
    print("-" * 70)
    print("ACTIONABLE MISMATCHES")
    print("-" * 70)

    # Compute what diff_arg instructions are NOT explained by root causes
    explained_indices = set()

    # An instruction is "explained" if ALL its diff_breakdown args are
    # pure register swaps, offset shifts, symbols, or branch_dests
    for ins in instrs:
        if ins.get("match_type") != "diff_arg":
            continue
        bd = ins.get("diff_breakdown")
        if not bd:
            continue
        idx = ins["index"]
        all_explained = True
        for arg in bd.get("arguments", []):
            at = arg.get("arg_type", "")
            if at in ("symbol", "branch_dest"):
                continue  # Always noise
            elif at == "register":
                continue  # Register alloc noise
            elif at == "immediate":
                tv = arg.get("target", {}).get("value")
                bv = arg.get("base", {}).get("value")
                if isinstance(tv, (int, float)) and isinstance(bv, (int, float)):
                    continue  # Offset shift noise
                elif isinstance(tv, str) or isinstance(bv, str):
                    continue  # Symbol in immediate
                else:
                    all_explained = False
            else:
                all_explained = False
        if all_explained:
            explained_indices.add(idx)

    # diff_op instructions (always actionable)
    diff_ops = [ins for ins in instrs if ins.get("match_type") == "diff_op"]
    if diff_ops:
        print()
        print(f"  diff_op (opcode mismatches): {len(diff_ops)}")
        for ins in diff_ops:
            idx = ins["index"]
            t = ins.get("target", {})
            b = ins.get("base", {})
            print(f"    idx {idx:4d}: TGT {t.get('opcode','?'):10s} {t.get('args','')}")
            print(f"             SRC {b.get('opcode','?'):10s} {b.get('args','')}")
    else:
        print()
        print("  diff_op: none (good!)")

    # insert/delete clusters
    clusters = find_clusters(instrs, ("insert", "delete"))
    if clusters:
        print()
        total_indel = counts.get("insert", 0) + counts.get("delete", 0)
        print(f"  insert/delete: {total_indel} instructions in "
              f"{len(clusters)} clusters")
        for i, cluster in enumerate(clusters):
            indices = [ins["index"] for _, ins in cluster]
            lo, hi = min(indices), max(indices)
            size = len(cluster)
            ins_count = sum(1 for _, ins in cluster if ins["match_type"] == "insert")
            del_count = size - ins_count
            print(f"    cluster {i+1}: idx {lo}-{hi} "
                  f"({size} instrs: {ins_count}I/{del_count}D)")
    else:
        print()
        print("  insert/delete: none")

    # replace instructions
    replaces = [ins for ins in instrs if ins.get("match_type") == "replace"]
    if replaces:
        sym_noise, real_count, real_examples = categorize_replaces(instrs)
        print()
        print(f"  replace: {len(replaces)} instructions "
              f"({sym_noise} symbol-reloc noise, {real_count} real)")
        if real_examples:
            show = real_examples[:8]
            for ins in show:
                idx = ins["index"]
                t = ins.get("target", {})
                b = ins.get("base", {})
                print(f"    idx {idx:4d}: TGT {fmt_instr(t)}")
                print(f"             SRC {fmt_instr(b)}")
            if len(real_examples) > 8:
                print(f"    ... and {len(real_examples) - 8} more real replaces")

    # Unexplained diff_arg
    diff_arg_instrs = [ins for ins in instrs if ins.get("match_type") == "diff_arg"]
    unexplained = [ins for ins in diff_arg_instrs
                   if ins["index"] not in explained_indices]
    if unexplained:
        print()
        print(f"  Unexplained diff_arg: {len(unexplained)} "
              f"(of {len(diff_arg_instrs)} total)")
        # These are diff_arg without breakdown data — likely still noise
        # but worth flagging
        no_breakdown = [ins for ins in unexplained if not ins.get("diff_breakdown")]
        has_breakdown = [ins for ins in unexplained if ins.get("diff_breakdown")]
        if no_breakdown:
            print(f"    {len(no_breakdown)} without diff_breakdown (no detail available)")
        if has_breakdown:
            print(f"    {len(has_breakdown)} with diff_breakdown (unusual arg types)")
            for ins in has_breakdown[:5]:
                print(f"      idx {ins['index']}: {ins.get('diff_breakdown')}")

    # ── 4. Noise Budget ──
    print()
    print("-" * 70)
    print("NOISE BUDGET")
    print("-" * 70)

    total_diff_arg = len(diff_arg_instrs)
    n_explained = len(explained_indices)
    n_unexplained = total_diff_arg - n_explained
    total_nonequal = total - equal_count

    print()
    print(f"  diff_arg instructions: {total_diff_arg}")
    print(f"    Explained by root causes: {n_explained}")
    print(f"      Offset shifts:     {total_offset_explained} arg diffs")
    print(f"      Register swaps:    {total_reg_explained} arg diffs")
    print(f"      Symbol relocs:     {len(symbol_diffs)} arg diffs")
    print(f"      Branch dests:      {len(branch_diffs)} arg diffs")
    print(f"    Unexplained:         {n_unexplained}")
    print()
    print(f"  Other non-equal: {total_nonequal - total_diff_arg}")
    print(f"    diff_op:   {counts.get('diff_op', 0)}")
    print(f"    replace:   {counts.get('replace', 0)}")
    print(f"    insert:    {counts.get('insert', 0)}")
    print(f"    delete:    {counts.get('delete', 0)}")


# ── Clusters mode ───────────────────────────────────────────────────────────

def _suggest_cluster_pattern(opcodes: Counter, cluster: list, instrs: list) -> str | None:
    """Heuristic pattern suggestions based on opcode composition of a cluster.

    Returns a suggestion string or None.
    """
    ops = set(opcodes.keys())
    top_ops = [op for op, _ in opcodes.most_common(5)]

    # Pointer arithmetic / index computation
    # srawi + addi + slwi/lwzx → "pointer subtraction not strength-reduced"
    if ops & {"srawi", "slwi"} and ops & {"addi", "add", "subf"}:
        return "Likely pointer arithmetic / index computation — try hardcoded byte offset or `(int)` cast"

    # Struct copy / block move
    # Batched lwz/stw with callee-saved GPRs
    lwz_stw = opcodes.get("lwz", 0) + opcodes.get("stw", 0) + opcodes.get("stfs", 0) + opcodes.get("lfs", 0)
    if lwz_stw >= 4 and lwz_stw >= sum(opcodes.values()) * 0.5:
        return "Likely struct copy or field initialization — check member order, try aggregate vs field-by-field"

    # Boolean materialization
    # li + cmpwi/cmplwi + bne/beq + li → bool cond pattern
    if ops & {"li"} and ops & {"cmpwi", "cmplwi", "cmpw", "cmplw"} and ops & {"bne", "beq", "ble", "bge", "blt", "bgt"}:
        return "Likely boolean materialization — try `bool b = (expr); if (b)` or signed/unsigned cast"

    # Floating-point multiply-add / cross product
    if opcodes.get("fmsubs", 0) + opcodes.get("fmadds", 0) + opcodes.get("fnmsubs", 0) >= 3:
        return "Likely cross product or matrix computation — verify all components, check operand order"

    # Floating-point subtract chain (distance/delta computation)
    if opcodes.get("fsubs", 0) >= 3:
        return "Likely vector subtraction — check Vector3 component order, try `v1 - v2` vs manual subtraction"

    # Prologue/epilogue (save/restore helper calls)
    if ops & {"mflr", "stw", "stwu"} or ops & {"lwz", "mtlr", "addi", "blr"}:
        if any(op.startswith("__save") or op.startswith("__rest") for op in ops):
            return "Prologue/epilogue mismatch — different callee-saved register count (declaration order issue)"

    # Callee-saved register save block
    if opcodes.get("stw", 0) >= 3 and all(
        _is_callee_saved_reg(ins) for _, ins in cluster[:4]
    ):
        return "Callee-saved register save block — declaration order or variable count mismatch"

    # addi-heavy (address computation)
    if opcodes.get("addi", 0) >= 3 and opcodes.get("addi", 0) >= sum(opcodes.values()) * 0.4:
        return "Heavy address computation — try inlining member access or caching differently"

    # Branch-heavy (control flow difference)
    branch_ops = sum(opcodes.get(op, 0) for op in ("b", "beq", "bne", "blt", "bge", "ble", "bgt", "bdnz"))
    if branch_ops >= 2 and branch_ops >= sum(opcodes.values()) * 0.3:
        return "Control flow difference — try if/else inversion, loop form change, or guard rewrite"

    # mr chain (register shuffle)
    if opcodes.get("mr", 0) >= 2:
        return "Register shuffle — try variable declaration reorder or caching pattern change"

    return None


def _is_callee_saved_reg(ins: dict) -> bool:
    """Check if an instruction operates on callee-saved GPRs (r14-r31)."""
    for side in ("target", "base"):
        s = ins.get(side)
        if s:
            args = s.get("args", "")
            for r in range(14, 32):
                if f"r{r}" in args:
                    return True
    return False


def cmd_clusters(instrs, context=2):
    """Show insert/delete clusters with context and pattern suggestions."""
    clusters = find_clusters(instrs, ("insert", "delete"))
    if not clusters:
        print("No insert/delete instructions found.")
        return

    total_indel = sum(len(c) for c in clusters)
    print(f"Found {total_indel} insert/delete instructions in "
          f"{len(clusters)} clusters (gap <= 2)")
    print()

    for i, cluster in enumerate(clusters):
        indices = [ins["index"] for _, ins in cluster]
        lo_idx, hi_idx = min(indices), max(indices)
        size = len(cluster)
        ins_count = sum(1 for _, ins in cluster if ins["match_type"] == "insert")
        del_count = size - ins_count

        # Count dominant opcodes
        opcodes = Counter()
        for _, ins in cluster:
            t = ins.get("target")
            b = ins.get("base")
            if t:
                opcodes[t.get("opcode", "?")] += 1
            if b:
                opcodes[b.get("opcode", "?")] += 1

        top_ops = ", ".join(f"{op}({c})" for op, c in opcodes.most_common(3))

        print(f"{'=' * 60}")
        print(f"Cluster {i+1}: idx {lo_idx}-{hi_idx} | "
              f"{size} instrs ({ins_count}I/{del_count}D) | ops: {top_ops}")

        # Pattern suggestion
        suggestion = _suggest_cluster_pattern(opcodes, cluster, instrs)
        if suggestion:
            print(f"  >> {suggestion}")

        print(f"{'=' * 60}")

        # Show with surrounding context
        show_lo = max(0, lo_idx - context)
        show_hi = min(len(instrs) - 1, hi_idx + context)
        cluster_indices = set(indices)
        for j in range(show_lo, show_hi + 1):
            if j < len(instrs):
                ins = instrs[j]
                highlight = ins["index"] in cluster_indices
                print_instr(ins, highlight=highlight, annotate=True)
        print()


# ── Register swaps mode ─────────────────────────────────────────────────────

def cmd_regswaps(instrs):
    """Analyze register swap pairs."""
    reg_swaps, _, _, _ = parse_breakdowns(instrs)
    if not reg_swaps:
        print("No register swaps found in diff_breakdown data.")
        return

    pair_data = compute_reg_swap_pairs(reg_swaps)

    print(f"Register swap analysis: {len(reg_swaps)} swapped args across "
          f"{len(pair_data)} pairs")
    print()

    # Separate GPR and FPR
    gpr_pairs = {}
    fpr_pairs = {}
    other_pairs = {}
    for pair, data in pair_data.items():
        p0, _ = pair
        if p0.startswith("r"):
            gpr_pairs[pair] = data
        elif p0.startswith("f"):
            fpr_pairs[pair] = data
        else:
            other_pairs[pair] = data

    def print_pair_table(pairs, label):
        if not pairs:
            return
        print(f"  {label}:")
        print(f"    {'Pair':>12s}  {'Count':>6s}  {'First':>6s}  {'Last':>6s}  {'Span':>6s}")
        print(f"    {'─' * 12}  {'─' * 6}  {'─' * 6}  {'─' * 6}  {'─' * 6}")
        for pair, data in sorted(pairs.items(), key=lambda x: -x[1]["count"]):
            p0, p1 = pair
            span = data["last"] - data["first"]
            print(f"    {p0:>4s} <-> {p1:<4s}  {data['count']:6d}  "
                  f"{data['first']:6d}  {data['last']:6d}  {span:6d}")
        total = sum(d["count"] for d in pairs.values())
        print(f"    {'Total':>12s}  {total:6d}")
        print()

    print_pair_table(gpr_pairs, "GPR (general purpose — may be fixable via declaration reorder)")
    print_pair_table(fpr_pairs, "FPR (floating point — usually unfixable)")
    print_pair_table(other_pairs, "Other")

    # Summary
    gpr_total = sum(d["count"] for d in gpr_pairs.values())
    fpr_total = sum(d["count"] for d in fpr_pairs.values())
    other_total = sum(d["count"] for d in other_pairs.values())
    print(f"Summary: {gpr_total} GPR + {fpr_total} FPR + {other_total} other "
          f"= {len(reg_swaps)} total register arg diffs")


# ── Offsets mode ────────────────────────────────────────────────────────────

def cmd_offsets(instrs):
    """Analyze offset/immediate differences."""
    _, offset_diffs, _, _ = parse_breakdowns(instrs)
    if not offset_diffs:
        print("No offset differences found in diff_breakdown data.")
        return

    delta_hist = compute_offset_histogram(offset_diffs)

    print(f"Offset analysis: {len(offset_diffs)} immediate/offset arg differences")
    print()

    # Histogram
    dominant_delta, dominant_count = delta_hist.most_common(1)[0]
    print("  Offset delta histogram (base - target):")
    print(f"    {'Delta':>8s}  {'Count':>6s}  {'Bar'}")
    print(f"    {'─' * 8}  {'─' * 6}  {'─' * 30}")
    max_count = dominant_count
    for delta, count in delta_hist.most_common(20):
        bar_len = int(30 * count / max_count) if max_count else 0
        bar = "█" * bar_len
        flag = " ◄ dominant" if delta == dominant_delta else ""
        print(f"    {delta:+8d}  {count:6d}  {bar}{flag}")

    total_explained = sum(delta_hist.values())
    print()
    print(f"  Total: {total_explained} offset arg diffs across "
          f"{len(delta_hist)} distinct deltas")
    print(f"  Dominant delta: {dominant_delta:+d} ({dominant_count} instructions, "
          f"{100.0 * dominant_count / total_explained:.1f}%)")

    # Find instructions with non-dominant deltas (interesting ones)
    print()
    print("-" * 60)
    print("OUTLIER OFFSETS (not the dominant delta)")
    print("-" * 60)

    outliers = [(idx, tv, bv, delta)
                for idx, tv, bv, delta in offset_diffs
                if delta != dominant_delta]

    if not outliers:
        print("  All offsets explained by dominant delta — likely pure stack frame shift.")
        return

    print(f"  {len(outliers)} instructions with non-dominant offset deltas:")
    print()

    # Group outliers by delta
    by_delta = defaultdict(list)
    for idx, tv, bv, delta in outliers:
        by_delta[delta].append((idx, tv, bv))

    for delta in sorted(by_delta.keys(), key=lambda d: -len(by_delta[d])):
        entries = by_delta[delta]
        print(f"  delta={delta:+d} ({len(entries)} instructions):")
        for idx, tv, bv in entries[:8]:
            ins = instrs[idx]
            t = ins.get("target", {})
            b = ins.get("base", {})
            print(f"    idx {idx:4d}: TGT {fmt_instr(t)[:50]}")
            print(f"             SRC {fmt_instr(b)[:50]}")
        if len(entries) > 8:
            print(f"    ... and {len(entries) - 8} more")
        print()


# ── Replaces mode ──────────────────────────────────────────────────────────

def cmd_replaces(instrs):
    """Categorize replace instructions into symbol-reloc noise vs real structural differences."""
    sym_noise, real_count, real_examples = categorize_replaces(instrs)
    total = sym_noise + real_count

    print(f"Replace breakdown: {total} total")
    print(f"  Symbol-reloc noise (SRC has extra sym arg): {sym_noise}")
    print(f"  Real structural replaces: {real_count}")
    print()

    if real_examples:
        print("Real replace examples:")
        for ins in real_examples:
            idx = ins["index"]
            t = ins.get("target", {})
            b = ins.get("base", {})
            print(f"  idx {idx:4d}: TGT {fmt_instr(t)}")
            print(f"           SRC {fmt_instr(b)}")


# ── Attribution ────────────────────────────────────────────────────────────

def _find_source_for_symbol(symbol, project_dir=None):
    """Look up source file path for a mangled symbol via report.json.

    Returns (source_path, unit_name) or (None, None).
    """
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(os.path.dirname(script_dir))
    effective_dir = project_dir or repo_root

    report_path = os.path.join(effective_dir, "build", "45410914", "report.json")
    if not os.path.exists(report_path):
        return None, None

    with open(report_path) as f:
        report = json.load(f)

    for unit in report.get("units", []):
        for fn in unit.get("functions", []):
            if fn.get("name") == symbol:
                unit_name = unit.get("name", "")
                # Strip default/ prefix
                name = unit_name
                if name.startswith("default/"):
                    name = name[len("default/"):]
                # Try to find .cpp
                for ext in (".cpp", ".c"):
                    src = os.path.join(effective_dir, "src", name + ext)
                    if os.path.exists(src):
                        return src, unit_name
                return None, unit_name

    return None, None


def _compile_with_listing(source_path, project_dir=None):
    """Compile a source file with /FAs and return the listing text.

    Returns the listing text or None on failure.
    """
    try:
        from tools.compiler_trace.invoker import CompilerInvoker
    except ImportError:
        print("Error: cannot import CompilerInvoker", file=sys.stderr)
        return None

    invoker = CompilerInvoker()
    src = Path(source_path)
    output_dir = Path("/tmp/claude") / "attribution" / src.stem
    output_dir.mkdir(parents=True, exist_ok=True)

    result = invoker.compile_with_asm(src, output_dir, listing_type="/FAs")
    if result.returncode != 0:
        print(f"Compilation failed (exit {result.returncode}):", file=sys.stderr)
        if result.stderr:
            print(result.stderr[:500], file=sys.stderr)
        return None

    # Find the .cod or .asm listing file
    for ext in (".cod", ".asm"):
        listing_path = output_dir / (src.stem + ext)
        if listing_path.exists():
            return listing_path.read_text(errors="replace")

    # Try any file in output_dir
    for p in output_dir.iterdir():
        if p.suffix in (".cod", ".asm"):
            return p.read_text(errors="replace")

    print(f"No listing file found in {output_dir}", file=sys.stderr)
    return None


def _objdiff_to_diff_instructions(instrs):
    """Convert objdiff JSON instructions to the format expected by attribution.

    Maps objdiff's match_type/target/base format to the attribution module's
    index/diff_kind/target_opcode/base_opcode format.
    """
    result = []
    for ins in instrs:
        idx = ins.get("index", -1)
        mt = ins.get("match_type", "equal")

        # Map objdiff match_type to diff_kind
        if mt == "equal":
            kind = "match"
        elif mt in ("diff_op", "replace"):
            kind = "replace"
        elif mt == "insert":
            kind = "insert"
        elif mt == "delete":
            kind = "delete"
        elif mt == "diff_arg":
            kind = "replace"  # args differ but opcode may match
        else:
            kind = "match"

        target = ins.get("target") or {}
        base = ins.get("base") or {}

        result.append({
            "index": idx,
            "diff_kind": kind,
            "target_opcode": target.get("opcode", ""),
            "base_opcode": base.get("opcode", ""),
        })
    return result


def cmd_attributed(instrs, symbol, project_dir=None):
    """Source-attributed mismatch analysis.

    Compiles the source with /FAs, parses the listing, and joins with
    objdiff instruction data to show which source lines cause mismatches.
    """
    # Lazy import
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(os.path.dirname(script_dir))
    if repo_root not in sys.path:
        sys.path.insert(0, repo_root)

    from scripts.permuter.attribution import (
        attribute_function,
    )

    # 1. Find source file
    source_path, unit_name = _find_source_for_symbol(symbol, project_dir)
    if not source_path:
        print(f"Error: cannot resolve source file for symbol: {symbol}", file=sys.stderr)
        if unit_name:
            print(f"  Unit: {unit_name}", file=sys.stderr)
        sys.exit(1)

    print(f"Source: {source_path}", file=sys.stderr)
    print(f"Unit:   {unit_name}", file=sys.stderr)

    # 2. Compile with /FAs
    print("Compiling with /FAs...", file=sys.stderr)
    listing_text = _compile_with_listing(source_path, project_dir)
    if not listing_text:
        print("Error: failed to generate assembly listing", file=sys.stderr)
        sys.exit(1)

    # 3. Extract function name from mangled symbol for listing lookup
    # The listing uses full mangled names in PROC/ENDP markers
    func_name = symbol

    # 4. Convert objdiff instructions to attribution format
    diff_instrs = _objdiff_to_diff_instructions(instrs)

    # 5. Run attribution pipeline
    listing, attributed, regions = attribute_function(listing_text, func_name, diff_instrs)

    if listing is None:
        print(f"Warning: function not found in /FAs listing: {func_name}", file=sys.stderr)
        print("  The listing may use a different mangling. Trying partial match...", file=sys.stderr)
        # Try with just the class::method part
        # e.g., ?BurnXfm@RndMesh@@QAAXXZ -> RndMesh or BurnXfm
        parts = symbol.split("@")
        if len(parts) >= 2:
            short_name = parts[0].lstrip("?")
            from scripts.permuter.attribution import parse_asm_listing
            listing = parse_asm_listing(listing_text, short_name)
            if listing:
                from scripts.permuter.attribution import attribute_mismatches, aggregate_regions
                attributed = attribute_mismatches(listing, diff_instrs)
                regions = aggregate_regions(attributed, listing)

    if listing is None:
        print("Error: could not find function in assembly listing", file=sys.stderr)
        sys.exit(1)

    # 6. Display results
    total = len(instrs)
    equal_count = sum(1 for i in instrs if i.get("match_type") == "equal")
    match_pct = 100.0 * equal_count / total if total else 0

    print()
    print("=" * 70)
    print("SOURCE ATTRIBUTION REPORT")
    print("=" * 70)
    print()
    print(f"Function:      {func_name}")
    print(f"Source:        {os.path.basename(source_path)}")
    print(f"Match:         ~{match_pct:.1f}% ({equal_count}/{total} instructions)")
    print(f"Listing:       {listing.instruction_count()} instructions parsed")
    if listing.prologue_helper:
        print(f"Prologue:      {listing.prologue_helper} ({listing.callee_saved_count} callee-saved)")
    print(f"Attributed:    {len(attributed)} mismatches → {len(regions)} region(s)")

    if not regions:
        print()
        print("  No mismatch regions found (function may match perfectly)")
        return

    print()
    print("-" * 70)
    print("MISMATCH REGIONS (sorted by impact)")
    print("-" * 70)

    for i, region in enumerate(regions):
        print()
        if region.source_file == "<unknown>":
            print(f"  Region {i+1}: <unattributed> ({region.unattributed_count} mismatches)")
            for m in region.mismatches[:5]:
                print(f"    idx {m.instruction_index:4d}: {m.mismatch_type:8s}  "
                      f"TGT: {m.target_opcode:8s}  SRC: {m.base_opcode}")
            if len(region.mismatches) > 5:
                print(f"    ... and {len(region.mismatches) - 5} more")
            continue

        line_range = (f"L{region.start_line}" if region.start_line == region.end_line
                      else f"L{region.start_line}-{region.end_line}")
        ratio_str = f"{region.match_ratio*100:.0f}%" if region.total_instructions > 0 else "?"

        print(f"  Region {i+1}: {os.path.basename(region.source_file)}:{line_range} "
              f"({region.mismatch_count} mismatches, {region.total_instructions} instrs, "
              f"{ratio_str} local match)")
        print(f"  Dominant type: {region.dominant_type}")
        print(f"  Source:")
        for src_line in region.source_lines[:5]:
            print(f"    | {src_line}")
        if len(region.source_lines) > 5:
            print(f"    | ... ({len(region.source_lines) - 5} more lines)")

        # Show individual mismatches grouped by type
        by_type = defaultdict(list)
        for m in region.mismatches:
            by_type[m.mismatch_type].append(m)

        print(f"  Mismatches:")
        for mtype, mlist in sorted(by_type.items(), key=lambda x: -len(x[1])):
            print(f"    {mtype} ({len(mlist)}):")
            for m in mlist[:4]:
                confidence = f" [{m.confidence:.0%}]" if m.confidence < 0.9 else ""
                print(f"      idx {m.instruction_index:4d}: "
                      f"TGT {m.target_opcode:8s} vs SRC {m.base_opcode:8s}{confidence}")
            if len(mlist) > 4:
                print(f"      ... and {len(mlist) - 4} more")


# ── Stack layout diff ────────────────────────────────────────────────────────

OPCODE_SIZE = {
    'stb': 1, 'lbz': 1, 'lbzu': 1, 'stbu': 1,
    'sth': 2, 'lhz': 2, 'lha': 2, 'lhau': 2, 'sthu': 2,
    'stw': 4, 'lwz': 4, 'stfs': 4, 'lfs': 4,
    'stwu': 4, 'lwzu': 4, 'lfsu': 4, 'stfsu': 4,
    'stfd': 8, 'lfd': 8, 'lfdu': 8, 'stfdu': 8,
}

# Match both formats: "offset(r1)" and "offset, r1" (objdiff comma-separated)
_STACK_RE = re.compile(r'(-?0x[0-9a-fA-F]+|-?\d+)(?:\(r1\)|,\s*r1\b)')
_STWU_RE = re.compile(r'stwu\s+r1,\s*(-?0x[0-9a-fA-F]+|-?\d+)(?:\(r1\)|,\s*r1\b)')


def _extract_frame_size_from_instrs(instrs, side='target'):
    """Find stwu r1, -N(r1) in prologue and return N (positive)."""
    for ins in instrs[:20]:
        s = ins.get(side)
        if not s:
            continue
        op = s.get('opcode', '')
        args = s.get('args', '')
        if op == 'stwu':
            m = _STWU_RE.search(f"{op} {args}")
            if m:
                val = int(m.group(1), 0)
                return abs(val)
    return None


def _collect_stack_accesses(instrs, side='target'):
    """Extract all r1-relative memory accesses from one side of the diff."""
    accesses = []
    for ins in instrs:
        s = ins.get(side)
        if not s:
            continue
        op = s.get('opcode', '')
        args = s.get('args', '')
        size = OPCODE_SIZE.get(op)
        if size is None:
            continue

        full = f"{op} {args}"
        m = _STACK_RE.search(full)
        if not m:
            continue
        offset = int(m.group(1), 0)
        if offset <= 0:
            continue  # Skip back chain and negative offsets (callee saves)

        # Extract register from args
        reg = args.split(',')[0].strip()

        accesses.append({
            'offset': offset,
            'size': size,
            'opcode': op,
            'register': reg,
            'index': ins.get('index', -1),
        })
    return accesses


def _cluster_into_slots(accesses):
    """Group stack accesses into logical slots by offset proximity."""
    if not accesses:
        return []

    # Sort by offset
    by_offset = sorted(accesses, key=lambda a: a['offset'])

    slots = []
    current_base = by_offset[0]['offset']
    current_accesses = [by_offset[0]]

    for acc in by_offset[1:]:
        # Merge if within 12 bytes of current base (covers Vector3 = 3x4)
        if acc['offset'] < current_base + 16 and acc['offset'] - current_accesses[-1]['offset'] <= 4:
            current_accesses.append(acc)
        else:
            # Emit slot
            slot_size = max(a['offset'] + a['size'] for a in current_accesses) - current_base
            ops = Counter(a['opcode'] for a in current_accesses)
            slots.append({
                'offset': current_base,
                'size': slot_size,
                'access_count': len(current_accesses),
                'ops_summary': ', '.join(f"{op} x{n}" for op, n in ops.most_common()),
                'first_index': min(a['index'] for a in current_accesses),
                'type_hint': _infer_type(current_accesses, slot_size),
            })
            current_base = acc['offset']
            current_accesses = [acc]

    # Final slot
    if current_accesses:
        slot_size = max(a['offset'] + a['size'] for a in current_accesses) - current_base
        ops = Counter(a['opcode'] for a in current_accesses)
        slots.append({
            'offset': current_base,
            'size': slot_size,
            'access_count': len(current_accesses),
            'ops_summary': ', '.join(f"{op} x{n}" for op, n in ops.most_common()),
            'first_index': min(a['index'] for a in current_accesses),
            'type_hint': _infer_type(current_accesses, slot_size),
        })

    return slots


def _infer_type(accesses, size):
    """Infer type from access pattern and size."""
    ops = set(a['opcode'] for a in accesses)
    has_float = bool(ops & {'stfs', 'lfs', 'lfsu', 'stfsu'})
    has_double = bool(ops & {'stfd', 'lfd'})

    if has_double and size == 8:
        return 'double'
    if has_float:
        if size == 4:
            return 'float'
        if size in (12, 16):
            return 'Vector3'
        if size == 64:
            return 'Transform'
    if size == 4:
        return 'int/ptr'
    if size == 2:
        return 'short'
    if size == 1:
        return 'byte'
    return f'{size}B struct'


def _diff_layouts(target_slots, source_slots):
    """Compare stack layouts, identify matches, swaps, and frame shift."""
    if not target_slots and not source_slots:
        return {'status': 'empty', 'rows': []}

    # Build offset maps
    tgt_by_off = {s['offset']: s for s in target_slots}
    src_by_off = {s['offset']: s for s in source_slots}

    all_offsets = sorted(set(tgt_by_off.keys()) | set(src_by_off.keys()))

    rows = []
    for off in all_offsets:
        tgt = tgt_by_off.get(off)
        src = src_by_off.get(off)
        if tgt and src:
            status = 'MATCH'
        elif tgt and not src:
            status = 'TGT_ONLY'
        elif src and not tgt:
            status = 'SRC_ONLY'
        else:
            status = 'UNKNOWN'
        rows.append({
            'offset': off,
            'target': tgt,
            'source': src,
            'status': status,
        })

    # Try to detect swaps: find TGT_ONLY/SRC_ONLY pairs with matching size
    tgt_only = [r for r in rows if r['status'] == 'TGT_ONLY']
    src_only = [r for r in rows if r['status'] == 'SRC_ONLY']

    for t_row in tgt_only:
        for s_row in src_only:
            if (t_row['target']['size'] == s_row['source']['size']
                    and t_row['status'] == 'TGT_ONLY'
                    and s_row['status'] == 'SRC_ONLY'):
                t_row['status'] = 'SWAPPED'
                s_row['status'] = 'SWAPPED'
                t_row['swap_partner'] = s_row['offset']
                s_row['swap_partner'] = t_row['offset']
                break

    return {'rows': rows}


def cmd_stack_layout(instrs, symbol=None, project_dir=None):
    """Stack frame layout comparison between target and base."""
    # Frame sizes
    tgt_frame = _extract_frame_size_from_instrs(instrs, 'target')
    src_frame = _extract_frame_size_from_instrs(instrs, 'base')

    # Collect accesses
    tgt_accesses = _collect_stack_accesses(instrs, 'target')
    src_accesses = _collect_stack_accesses(instrs, 'base')

    # Cluster into slots
    tgt_slots = _cluster_into_slots(tgt_accesses)
    src_slots = _cluster_into_slots(src_accesses)

    # Print frame summary
    print("=" * 70)
    print("STACK LAYOUT DIFF")
    if symbol:
        print(f"Function: {symbol}")
    print("=" * 70)
    print()

    tgt_str = f"0x{tgt_frame:x} ({tgt_frame} bytes)" if tgt_frame else "unknown"
    src_str = f"0x{src_frame:x} ({src_frame} bytes)" if src_frame else "unknown"
    print(f"  Target frame: {tgt_str}")
    print(f"  Source frame: {src_str}")
    if tgt_frame and src_frame:
        delta = src_frame - tgt_frame
        if delta != 0:
            bigger = "source" if delta > 0 else "target"
            print(f"  Frame delta:  {delta:+d} bytes ({bigger} larger)")
        else:
            print(f"  Frame delta:  0 (frames match)")
    print()

    # Diff the layouts
    diff = _diff_layouts(tgt_slots, src_slots)
    rows = diff['rows']

    if not rows:
        print("  No stack accesses found.")
        return

    # Print side-by-side table
    print(f"{'Offset':>8s}  {'Size':>4s}  {'Target Ops':^24s}  {'Source Ops':^24s}  {'Type':^12s}  Status")
    print("-" * 100)

    for row in rows:
        off_str = f"0x{row['offset']:04x}"
        tgt = row.get('target')
        src = row.get('source')
        tgt_ops = tgt['ops_summary'][:24] if tgt else "---"
        src_ops = src['ops_summary'][:24] if src else "---"
        size = tgt['size'] if tgt else (src['size'] if src else 0)
        size_str = str(size)
        type_hint = tgt['type_hint'] if tgt else (src['type_hint'] if src else "")
        status = row['status']
        swap_info = ""
        if 'swap_partner' in row:
            swap_info = f" <-> 0x{row['swap_partner']:04x}"

        print(f"  {off_str}  {size_str:>4s}  {tgt_ops:<24s}  {src_ops:<24s}  {type_hint:<12s}  {status}{swap_info}")

    # Diagnosis
    print()
    print("-" * 70)
    print("DIAGNOSIS")
    print("-" * 70)

    swapped = [r for r in rows if r['status'] == 'SWAPPED']
    tgt_only = [r for r in rows if r['status'] == 'TGT_ONLY']
    src_only = [r for r in rows if r['status'] == 'SRC_ONLY']
    matched = [r for r in rows if r['status'] == 'MATCH']

    if not swapped and not tgt_only and not src_only:
        print("  All stack slots match. No layout mismatches.")
        if tgt_frame and src_frame and tgt_frame != src_frame:
            print(f"  Frame size difference ({src_frame - tgt_frame:+d}) is likely from "
                  f"callee-saved register count difference (AT_LIMIT).")
    elif swapped:
        print(f"  {len(swapped) // 2} variable swap(s) detected:")
        seen = set()
        for r in swapped:
            if r['offset'] in seen:
                continue
            partner = r.get('swap_partner')
            if partner is not None:
                seen.add(r['offset'])
                seen.add(partner)
                t = r['target'] or r['source']
                print(f"    0x{r['offset']:04x} <-> 0x{partner:04x} "
                      f"({t['type_hint']}, {t['size']}B)")
        print()
        print("  Fix: Try reordering variable declarations in source.")
    elif tgt_only or src_only:
        print(f"  {len(tgt_only)} target-only slot(s), {len(src_only)} source-only slot(s)")
        if tgt_frame and src_frame and tgt_frame != src_frame:
            print(f"  Frame size differs by {abs(src_frame - tgt_frame)} bytes — "
                  "may be callee-saved register count mismatch (AT_LIMIT)")


# ── Compare ASM ──────────────────────────────────────────────────────────────

_TARGET_FN_RE = re.compile(r'^\.fn\s+"([^"]+)"')
_TARGET_ENDFN_RE = re.compile(r'^\.endfn\s+"([^"]+)"')
_TARGET_INSTR_RE = re.compile(
    r'/\*\s*[0-9A-Fa-f]+\s+[0-9A-Fa-f]+\s+(?:[0-9A-Fa-f]{2}\s*)+\*/\s*(.*)'
)
_LABEL_TARGET_RE = re.compile(r'\.L_[0-9A-Fa-f]+')
_LABEL_BASE_RE = re.compile(r'\$LN\d+@\w+')
_SYM_RTTI_RE = re.compile(r'"?\?\?_R0\?AV(\w+)@?(\w*)@*8"?')
_SYM_FUNC_RE = re.compile(r'"?\?(\w+)@(\w+)@@[^"]*"?')
_SYM_STR_RE = re.compile(r'"?\?\?_C@[^"]*"?')


def _find_target_asm_file(symbol, project_dir=None):
    """Find the target .s file and extract function lines."""
    script_dir = os.path.dirname(os.path.abspath(__file__))
    repo_root = os.path.dirname(os.path.dirname(script_dir))
    effective_dir = project_dir or repo_root
    asm_dir = os.path.join(effective_dir, "build", "45410914", "asm")

    # Find the .s file by searching for the symbol
    report_path = os.path.join(effective_dir, "build", "45410914", "report.json")
    if not os.path.exists(report_path):
        return None

    with open(report_path) as f:
        report = json.load(f)

    # Find unit name for this symbol
    unit_name = None
    for unit in report.get("units", []):
        for fn in unit.get("functions", []):
            if fn.get("name") == symbol:
                unit_name = unit.get("name", "")
                break
        if unit_name:
            break

    if not unit_name:
        return None

    # Map unit to .s file path
    name = unit_name
    if name.startswith("default/"):
        name = name[len("default/"):]

    # Try different category directories
    for cat in ["", "system/", "lazer/", "lib/"]:
        asm_path = os.path.join(asm_dir, cat + name + ".s")
        if os.path.exists(asm_path):
            return _extract_fn_from_asm(asm_path, symbol)

    # Also try just the basename
    basename = name.rsplit("/", 1)[-1] if "/" in name else name
    for cat in ["", "system/", "lazer/", "lib/"]:
        asm_path = os.path.join(asm_dir, cat + basename + ".s")
        if os.path.exists(asm_path):
            return _extract_fn_from_asm(asm_path, symbol)

    # Brute force: search all .s files
    import glob as globmod
    for asm_path in globmod.glob(os.path.join(asm_dir, "**/*.s"), recursive=True):
        result = _extract_fn_from_asm(asm_path, symbol)
        if result is not None:
            return result

    return None


def _extract_fn_from_asm(asm_path, symbol):
    """Extract function lines between .fn/.endfn markers."""
    try:
        with open(asm_path, 'r') as f:
            lines = f.readlines()
    except OSError:
        return None

    in_fn = False
    result = []
    for line in lines:
        if not in_fn:
            m = _TARGET_FN_RE.match(line)
            if m and m.group(1) == symbol:
                in_fn = True
                continue
        else:
            m = _TARGET_ENDFN_RE.match(line)
            if m:
                return result
            result.append(line.rstrip())

    return None if not result else result


def _normalize_symbol(text):
    """Shorten mangled MSVC symbol references."""
    # RTTI: ??_R0?AVObject@Hmx@@@8 -> Object::Hmx
    text = _SYM_RTTI_RE.sub(lambda m: f"RTTI:{m.group(1)}" + (f"::{m.group(2)}" if m.group(2) else ""), text)
    # String constants
    text = _SYM_STR_RE.sub("<str>", text)
    # Function symbols: ?Method@Class@@... -> Class::Method
    text = _SYM_FUNC_RE.sub(lambda m: f"{m.group(2)}::{m.group(1)}", text)
    # Strip remaining quotes
    text = text.replace('"', '')
    return text


def _normalize_target_line(line):
    """Normalize a raw target .s line to just opcode + operands."""
    m = _TARGET_INSTR_RE.match(line)
    if m:
        return m.group(1).strip()
    # It might be a label or directive
    stripped = line.strip()
    if stripped.startswith('.') or stripped.endswith(':'):
        return None  # Skip labels and directives
    return stripped if stripped else None


def cmd_compare_asm(instrs, symbol=None, project_dir=None):
    """Side-by-side target vs base assembly with annotations."""
    total = len(instrs)
    equal_count = sum(1 for i in instrs if i.get("match_type") == "equal")
    match_pct = 100.0 * equal_count / total if total else 0

    # Try to load target asm
    target_lines = None
    if symbol:
        target_lines = _find_target_asm_file(symbol, project_dir)

    # Renumber labels
    tgt_labels = {}
    src_labels = {}
    tgt_label_counter = 0
    src_label_counter = 0

    # Find cluster boundaries
    clusters = find_clusters(instrs, match_types=("insert", "delete"))
    cluster_ranges = {}
    for ci, cluster in enumerate(clusters):
        for _, ins in cluster:
            cluster_ranges[ins["index"]] = ci

    # Parse target lines into indexed map
    tgt_norm = {}
    if target_lines:
        tgt_idx = 0
        for line in target_lines:
            norm = _normalize_target_line(line)
            if norm is not None:
                tgt_norm[tgt_idx] = _normalize_symbol(norm)
                tgt_idx += 1

    # Header
    print(f"## Compare ASM: {symbol or '(unknown)'}")
    print(f"**Match**: {match_pct:.1f}% ({equal_count}/{total} instructions)")
    print()

    # Build output rows
    prev_cluster = None
    consecutive_equal = 0
    equal_buffer = []

    def flush_equal_buffer():
        nonlocal equal_buffer
        if not equal_buffer:
            return
        if len(equal_buffer) <= 6:
            for row in equal_buffer:
                print(row)
        else:
            # Show first 2, collapse, show last 2
            for row in equal_buffer[:2]:
                print(row)
            print(f"       ... ({len(equal_buffer) - 4} equal instructions) ...")
            for row in equal_buffer[-2:]:
                print(row)
        equal_buffer = []

    row_count = 0
    for ins in instrs:
        idx = ins.get("index", 0)
        mt = ins.get("match_type", "equal")
        t = ins.get("target") or {}
        b = ins.get("base") or {}

        # Cluster boundary
        ci = cluster_ranges.get(idx)
        if ci is not None and ci != prev_cluster:
            flush_equal_buffer()
            cluster = clusters[ci]
            insert_count = sum(1 for _, i in cluster if i.get("match_type") == "insert")
            delete_count = sum(1 for _, i in cluster if i.get("match_type") == "delete")
            print(f"  ; --- CLUSTER {ci + 1} ({insert_count}I/{delete_count}D) ---")
            prev_cluster = ci
        elif ci is None and prev_cluster is not None:
            flush_equal_buffer()
            print(f"  ; --- END CLUSTER {prev_cluster + 1} ---")
            prev_cluster = None

        # Format instruction
        match_marker = {
            'equal': '=', 'diff_arg': '~', 'diff_op': '!',
            'insert': '+', 'delete': '-', 'replace': 'X',
        }.get(mt, '?')

        t_str = f"{t.get('opcode', ''):8s} {_normalize_symbol(t.get('args', ''))}" if t else "---"
        b_str = f"{b.get('opcode', ''):8s} {_normalize_symbol(b.get('args', ''))}" if b else "---"

        # Annotation for diff_arg
        ann = diff_annotation(ins) if mt == "diff_arg" else ""

        # Cap at 150 rows for large functions
        row_count += 1
        if row_count > 150 and mt == "equal":
            continue

        row = f"  {match_marker}  | {idx:4d} | {t_str:<38s} | {b_str:<38s} |{ann}"

        if mt == "equal":
            equal_buffer.append(row)
        else:
            flush_equal_buffer()
            print(row)

    flush_equal_buffer()

    if row_count > 150:
        print(f"\n  ... (output truncated, {row_count} total instructions)")

    # Compact diagnosis
    print()
    _, offset_diffs, _, _ = parse_breakdowns(instrs)
    reg_swaps_raw, _, _, _ = parse_breakdowns(instrs)
    reg_pairs = compute_reg_swap_pairs(reg_swaps_raw)

    counts = Counter(ins["match_type"] for ins in instrs)
    non_equal = total - equal_count
    if non_equal > 0:
        print("### Diagnosis Summary")
        types = [(mt, c) for mt, c in counts.items() if mt != "equal"]
        types.sort(key=lambda x: -x[1])
        for mt, c in types:
            print(f"  {mt}: {c}")
        if reg_pairs:
            print(f"  Register swap pairs: {len(reg_pairs)}")
            for pair, data in sorted(reg_pairs.items(), key=lambda x: -x[1]['count'])[:5]:
                print(f"    {pair[0]} <-> {pair[1]}: {data['count']}x")
        if offset_diffs:
            hist = compute_offset_histogram(offset_diffs)
            dom_delta, dom_count = hist.most_common(1)[0]
            print(f"  Dominant offset delta: {dom_delta:+d} ({dom_count}x)")


# ── Symbol invocation ───────────────────────────────────────────────────────

def _resolve_ambiguous_objdiff(output: str, symbol: str) -> str | None:
    """Try to resolve ambiguous symbol from objdiff 'Multiple matches' output.

    Extracts candidate list, picks the shortest method name match
    (most likely the exact overload the user wanted).
    Returns the resolved demangled name, or None.
    """
    if "Multiple matches" not in output:
        return None

    candidates = []
    for line in output.strip().splitlines():
        line = line.strip()
        if not line or line.startswith("Multiple matches") or line.startswith("Failed"):
            continue
        # "access: rettype __cdecl Class::Method(args) (unit/path)"
        last_paren = line.rfind(" (")
        if last_paren > 0:
            demangled = line[:last_paren].strip()
            # Extract "Class::Method(args)" from full demangled
            for prefix in ("public: ", "protected: ", "private: "):
                if demangled.startswith(prefix):
                    demangled = demangled[len(prefix):]
            cdecl_idx = demangled.find("__cdecl ")
            if cdecl_idx >= 0:
                demangled = demangled[cdecl_idx + 8:]
            elif "__thiscall " in demangled:
                demangled = demangled[demangled.find("__thiscall ") + 11:]
            candidates.append(demangled)

    if not candidates:
        return None

    # Extract param hint if user provided one: "Class::Method(hint)"
    param_hint = None
    if "(" in symbol and not symbol.startswith("?"):
        paren_idx = symbol.index("(")
        param_hint = symbol[paren_idx + 1:].rstrip(")")
        symbol_base = symbol[:paren_idx]
    else:
        symbol_base = symbol

    if param_hint:
        hint_lower = param_hint.lower().strip()
        matching = [c for c in candidates
                    if hint_lower in c[c.find("(", c.find("::")) :].lower()]
        if len(matching) == 1:
            return matching[0]
        if matching:
            matching.sort(key=len)
            return matching[0]

    # No hint: prefer shortest (most exact method name match)
    candidates.sort(key=len)
    return candidates[0]


def run_objdiff_for_symbol(symbol, project_dir=None, unit=None):
    """Run objdiff-cli diff and return path to JSON output."""
    # Extract param hint for disambiguation: "Class::Method(Hint)" → base + hint
    param_hint = None
    if "(" in symbol and not symbol.startswith("?"):
        paren_idx = symbol.index("(")
        param_hint = symbol[paren_idx + 1:].rstrip(")")
        symbol = symbol[:paren_idx]

    # Deterministic filename from symbol
    h = hashlib.md5(symbol.encode()).hexdigest()[:12]
    # Also create a readable slug
    slug = re.sub(r'[^a-zA-Z0-9]+', '_', symbol)[:40].strip('_').lower()
    json_path = f"/tmp/claude/diff_{slug}_{h}.json"

    # Find project root (where objdiff.json lives)
    # objdiff binary is always resolved from the project root
    # (bin/ doesn't exist in worktrees)
    # Script is at scripts/analysis/diff_inspect.py, so go up 2 levels
    script_dir = os.path.dirname(os.path.abspath(__file__))
    script_repo_root = os.path.dirname(os.path.dirname(script_dir))
    objdiff_bin = os.path.join(script_repo_root, "bin", "objdiff-cli")

    if not os.path.exists(objdiff_bin):
        print(f"Error: objdiff-cli not found at {objdiff_bin}", file=sys.stderr)
        sys.exit(1)

    # Use project_dir for builds if specified, otherwise script's repo root
    effective_project_dir = project_dir if project_dir else script_repo_root

    print(f"Running objdiff for: {symbol}", file=sys.stderr)
    print(f"Output: {json_path}", file=sys.stderr)

    cmd = [
        objdiff_bin, "diff",
        "-p", str(effective_project_dir),
        symbol,
        "--include-instructions", "--build", "--incremental",
        "-f", "json", "-o", json_path
    ]
    if unit:
        cmd.extend(["-u", unit])

    result = subprocess.run(cmd, cwd=str(effective_project_dir),
                            capture_output=True, text=True)
    if result.returncode != 0:
        # Try to resolve ambiguous symbol
        combined = (result.stdout or "") + "\n" + (result.stderr or "")
        if "Ambiguous symbol" in combined or "Multiple matches" in combined:
            # Reconstruct symbol with hint for disambiguation
            hint_symbol = f"{symbol}({param_hint})" if param_hint else symbol
            resolved = _resolve_ambiguous_objdiff(combined, hint_symbol)
            if resolved:
                print(f"Resolved ambiguous symbol to: {resolved}", file=sys.stderr)
                cmd[cmd.index(symbol)] = resolved
                result = subprocess.run(cmd, cwd=str(effective_project_dir),
                                        capture_output=True, text=True)
                if result.returncode == 0:
                    if result.stderr:
                        print(result.stderr, file=sys.stderr, end="")
                    return json_path

        print(f"objdiff-cli failed (exit {result.returncode}):", file=sys.stderr)
        print(result.stderr, file=sys.stderr)
        if result.stdout:
            print(result.stdout, file=sys.stderr)
        sys.exit(1)

    if result.stderr:
        # Print objdiff stderr (build progress etc) but don't fail
        print(result.stderr, file=sys.stderr, end="")

    return json_path


# ── Main ────────────────────────────────────────────────────────────────────

def main():
    parser = argparse.ArgumentParser(
        description="Inspect objdiff JSON diffs",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Analysis modes (pick one):
  --diagnose      Root cause analysis: why doesn't this match?
  --attributed    Source-attributed mismatch regions (requires --symbol)
  --clusters      Group insert/delete into contiguous clusters
  --regswaps      Register swap pair analysis
  --offsets       Offset/immediate shift analysis
  --replaces      Categorize replaces (symbol-reloc noise vs real)
  --stack-layout  Stack frame layout comparison (target vs base)
  --compare-asm   Side-by-side assembly with annotations
  --compare       Compare baseline and candidate JSONs (delta table)

Filter modes:
  diff_op       Only opcode mismatches
  replace       Only replaced instructions
  insert,delete Insert and delete instructions
  all           Every instruction
  --range N-M   Specific index range
  --summary     Match type counts
""")
    parser.add_argument(
        "json_file", nargs="?", default=None,
        help="Path to objdiff JSON output (optional if --symbol is used)")
    parser.add_argument(
        "match_types", nargs="?", default=None,
        help="Comma-separated match types to filter (e.g. diff_op,replace). "
        "'all' shows everything. Default: all non-equal types.")
    parser.add_argument(
        "-C", "--context", type=int, default=5,
        help="Lines of context around each match (default: 5)")
    parser.add_argument(
        "--range", type=str, default=None,
        help="Show specific index range (e.g. 950-970)")
    parser.add_argument(
        "--summary", action="store_true", help="Show match type counts only")
    parser.add_argument(
        "--diagnose", action="store_true",
        help="Root cause analysis mode")
    parser.add_argument(
        "--clusters", action="store_true",
        help="Show insert/delete clusters")
    parser.add_argument(
        "--regswaps", action="store_true",
        help="Register swap pair analysis")
    parser.add_argument(
        "--offsets", action="store_true",
        help="Offset/immediate shift analysis")
    parser.add_argument(
        "--replaces", action="store_true",
        help="Categorize replace instructions (noise vs real)")
    parser.add_argument(
        "--attributed", action="store_true",
        help="Source-attributed mismatch regions (compile with /FAs, join with objdiff)")
    parser.add_argument(
        "--stack-layout", action="store_true",
        help="Stack frame layout comparison (target vs base)")
    parser.add_argument(
        "--compare-asm", action="store_true",
        help="Side-by-side target vs base assembly with cluster markers and annotations")
    parser.add_argument(
        "--symbol", type=str, default=None,
        help="Run objdiff-cli diff internally for this symbol")
    parser.add_argument(
        "--project-dir", type=str, default=None,
        help="Project directory for objdiff builds (worktree support)")
    parser.add_argument(
        "--unit", type=str, default=None,
        help="Unit name for objdiff disambiguation (e.g. 'default/link_glue')")
    parser.add_argument(
        "--compare", nargs=2, metavar=("BASELINE_JSON", "CANDIDATE_JSON"),
        help="Compare two objdiff JSON files")
    parser.add_argument(
        "--hotspots", type=str,
        default="930-1075,1235-1415,3088-3290,3628-3642",
        help="Comma-separated hotspot ranges for --compare (default matches OnBeat campaign)")
    args = parser.parse_args()

    # Compare mode uses two JSON files directly.
    if args.compare:
        base_path, cand_path = args.compare
        with open(base_path) as f:
            base_data = json.load(f)
        with open(cand_path) as f:
            cand_data = json.load(f)
        cmd_compare(base_data, cand_data, args.hotspots)
        return

    # Resolve JSON input
    json_file = args.json_file
    if args.symbol:
        json_file = run_objdiff_for_symbol(args.symbol, project_dir=args.project_dir, unit=args.unit)
    if not json_file:
        parser.error("Either json_file or --symbol is required")

    with open(json_file) as f:
        data = json.load(f)

    instrs = data.get("instructions", [])
    if not instrs:
        print("No instructions found in JSON.", file=sys.stderr)
        sys.exit(1)

    # ── Analysis modes ──
    if args.attributed:
        if not args.symbol:
            parser.error("--attributed requires --symbol")
        cmd_attributed(instrs, args.symbol, project_dir=args.project_dir)
        return
    if args.diagnose:
        cmd_diagnose(instrs)
        return
    if args.clusters:
        cmd_clusters(instrs, context=args.context)
        return
    if args.regswaps:
        cmd_regswaps(instrs)
        return
    if args.offsets:
        cmd_offsets(instrs)
        return
    if args.replaces:
        cmd_replaces(instrs)
        return
    if args.stack_layout:
        cmd_stack_layout(instrs, symbol=args.symbol, project_dir=args.project_dir)
        return
    if args.compare_asm:
        cmd_compare_asm(instrs, symbol=args.symbol, project_dir=args.project_dir)
        return

    # ── Summary mode ──
    if args.summary:
        counts = Counter(ins["match_type"] for ins in instrs)
        total = len(instrs)
        print(f"Total instructions: {total}")
        print()
        for mt, count in counts.most_common():
            pct = 100.0 * count / total
            print(f"  {mt:12s}: {count:5d} ({pct:5.1f}%)")
        return

    # ── Range mode ──
    if args.range:
        lo, hi = args.range.split("-")
        lo, hi = int(lo), int(hi)
        for ins in instrs:
            idx = ins["index"]
            if lo <= idx <= hi:
                highlight = ins["match_type"] not in ("equal", "diff_arg")
                print_instr(ins, highlight=highlight, annotate=True)
        return

    # ── Filter mode ──
    if args.match_types == "all":
        for ins in instrs:
            highlight = ins["match_type"] not in ("equal", "diff_arg")
            print_instr(ins, highlight=highlight, annotate=True)
        return

    if args.match_types:
        wanted = set(args.match_types.split(","))
    else:
        wanted = {"diff_op", "replace", "insert", "delete"}

    # Find matching instructions and show with context
    matches = [ins for ins in instrs if ins["match_type"] in wanted]

    if not matches:
        print(f"No instructions with match type(s): {wanted}", file=sys.stderr)
        sys.exit(0)

    print(f"Found {len(matches)} instruction(s) matching {wanted}")
    print()

    # Group nearby matches to avoid redundant context
    printed = set()
    for match in matches:
        idx = match["index"]
        lo = max(0, idx - args.context)
        hi = min(len(instrs) - 1, idx + args.context)

        # Skip if already printed in a previous group's context
        if idx in printed:
            continue

        print(f"--- index {idx}: {match['match_type']} ---")
        for i in range(lo, hi + 1):
            ins = instrs[i]
            highlight = ins["index"] == idx or ins["match_type"] in wanted
            print_instr(ins, highlight=highlight, annotate=True)
            printed.add(ins["index"])
        print()


if __name__ == "__main__":
    main()
