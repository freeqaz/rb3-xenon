#!/usr/bin/env python3
"""
MCP Server for RB3-Xenon Decomp Orchestrator.

Provides tools for sub-agents to:
- Report task completion results
- Query function database for work targets
- Get previous attempt history
- Lookup rb3-Wii or DC3 reference implementations
- Run objdiff with smart output handling

Run as: python3 -m scripts.orchestrator.mcp_server --db decomp.db
"""

import argparse
import asyncio
import json
import os
import re
import subprocess
import sys
import tempfile
from pathlib import Path
from typing import Any

# Maximum lines to return inline (larger outputs go to file)
MAX_INLINE_LINES = 500

# MCP protocol imports
try:
    from mcp.server import Server
    from mcp.server.stdio import stdio_server
    from mcp.types import Tool, TextContent
except ImportError:
    print("MCP package not installed. Install with: pip install mcp", file=sys.stderr)
    sys.exit(1)

# Add scripts and project root to path
sys.path.insert(0, str(Path(__file__).parent.parent))
sys.path.insert(0, str(Path(__file__).parent.parent.parent))

from orchestrator.database import (
    get_connection,
    get_function_by_symbol,
    query_functions as db_query_functions,
    get_attempts_for_function,
    record_attempt,
    update_function_status,
    get_file_pair,
    query_file_pairs,
    search_functions_by_name,
    normalize_unit_pattern,
    BOILERPLATE_SYMBOL_PREFIXES,
    DEFAULT_EXCLUDE_PATTERNS,
)
from tools.struct_db import StructDB
try:
    from tools.merged_symbols import MergedSymbolLookup
except ImportError:
    MergedSymbolLookup = None  # type: ignore[assignment,misc]


# Patterns for filtering noisy build output
_NINJA_PROGRESS = re.compile(r'^\s*\[\d+/\d+\]\s')
_NOISY_PREFIXES = (' INFO ', ' WARN ', 'xex: ', 'INFO ')
_NOISY_SUBSTRINGS = (
    'Skipping tail block merge',
    'Known functions complete',
    'Detected tail block',
    'Not a function @',
    'Found ',  # "Found N imps", "Found N known funcs"
)


def _stack_signal_summary(instrs: list) -> "str | None":
    """Compute a one-line stack-layout signal from already-parsed objdiff
    instructions. Returns None when no actionable signal exists (frame matches
    AND no user-slot mismatches). Otherwise returns "**Stack:** ..." for
    inline display in run_objdiff output. Pure JSON consumer — no recompile."""
    try:
        from analysis.stack_layout import (
            build_fingerprints, parse_prologue,
            classify_slots, dominant_delta_from_rows,
        )
    except ImportError:
        return None

    if not instrs:
        return None

    try:
        tgt_slots = build_fingerprints("target", instrs)
        base_slots = build_fingerprints("base", instrs)
        if not tgt_slots and not base_slots:
            return None
        tgt_prol = parse_prologue(instrs, "target")
        base_prol = parse_prologue(instrs, "base")
        dom = dominant_delta_from_rows(tgt_slots, base_slots)
        rows = classify_slots(
            tgt_slots, base_slots, dom,
            tgt_prol.callee_save_slots, base_prol.callee_save_slots,
        )
    except Exception:
        return None

    from collections import Counter as _Counter
    user_rows = [r for r in rows if not r.callee_save]
    counts = _Counter(r.verdict for r in user_rows)
    swapped = counts.get("SWAPPED", 0)
    shifted = counts.get("SHIFTED", 0)
    differ = counts.get("DIFFER", 0)
    tgt_only = counts.get("TGT_ONLY", 0)
    base_only = counts.get("BASE_ONLY", 0)
    actionable = swapped + shifted + differ + tgt_only + base_only
    frame_delta = base_prol.frame_size - tgt_prol.frame_size

    if actionable == 0 and frame_delta == 0:
        return None

    parts: list = []
    if frame_delta != 0:
        callee_bytes = (
            (base_prol.saved_gpr_count - tgt_prol.saved_gpr_count) * 8
            + (base_prol.saved_fpr_count - tgt_prol.saved_fpr_count) * 8
        )
        if callee_bytes == frame_delta:
            parts.append(f"frame Δ {frame_delta:+#x} (callee-save AT_LIMIT)")
        else:
            parts.append(f"frame Δ {frame_delta:+#x} (structural)")

    verdict_pieces = []
    if swapped:
        verdict_pieces.append(f"{swapped} SWAPPED")
    if shifted:
        verdict_pieces.append(f"{shifted} SHIFTED")
    if differ:
        verdict_pieces.append(f"{differ} DIFFER")
    if tgt_only or base_only:
        verdict_pieces.append(f"{tgt_only}/{base_only} TGT/BASE-only")
    if verdict_pieces:
        parts.append(", ".join(verdict_pieces))

    if not parts:
        return None

    hint = ""
    if swapped > 0:
        hint = " — reorder paired declarations"
    elif shifted > 0:
        hint = " — likely extra local on one side"
    elif differ > 0 and frame_delta == 0:
        hint = " — different variables in same slots"

    return (f"**Stack:** {' | '.join(parts)}{hint}. "
            f"Run `run_diff_inspect mode=stack-layout` for the full table.")


def _filter_build_output(text: str) -> str:
    """Filter noisy build/split output, keeping only meaningful lines."""
    if not text:
        return ""
    lines = text.strip().splitlines()
    filtered = []
    for line in lines:
        if _NINJA_PROGRESS.match(line):
            continue
        if any(line.startswith(p) for p in _NOISY_PREFIXES):
            continue
        if any(s in line for s in _NOISY_SUBSTRINGS):
            continue
        if re.match(r'^\s*Warning! Illegal inst found', line):
            continue
        filtered.append(line)
    return "\n".join(filtered)


def _extract_function_fallback(asm_lines: list[str], symbol: str) -> list[str] | None:
    """Fallback function extraction from /FAs listing using PROC/ENDP markers."""
    # Clean symbol for matching (strip leading ? for MSVC mangled names)
    search = symbol.lstrip("?")

    in_func = False
    func_lines = []
    for line in asm_lines:
        stripped = line.strip()
        if not in_func:
            if "PROC" in stripped and search in stripped:
                in_func = True
                func_lines.append(line)
        else:
            func_lines.append(line)
            if "ENDP" in stripped:
                return func_lines

    return func_lines if func_lines else None


# Regex to extract candidate lines from objdiff "Multiple matches" output
# Each line looks like: "  public: void __cdecl Class::Method(args) (unit/path)"
_MULTI_MATCH_LINE = re.compile(
    r'^\s+(?:public|protected|private):\s+.*?(\S+::\S+\(.*?\))\s+\(',
    re.MULTILINE,
)


def _extract_param_hint(symbol: str) -> tuple[str, str | None]:
    """Extract parameter type hint from a symbol string.

    Given 'Class::Method(TypeHint)', returns ('Class::Method', 'TypeHint').
    Given 'Class::Method', returns ('Class::Method', None).
    Handles nested templates/parens in the hint.
    """
    # Find the first '(' that's part of a user-provided hint (not MSVC mangling)
    if symbol.startswith("?") or "(" not in symbol:
        return symbol, None

    # Find matching parens
    paren_start = symbol.index("(")
    depth = 1
    pos = paren_start + 1
    while pos < len(symbol) and depth > 0:
        if symbol[pos] == "(":
            depth += 1
        elif symbol[pos] == ")":
            depth -= 1
        pos += 1

    if depth == 0:
        base = symbol[:paren_start].rstrip()
        hint = symbol[paren_start + 1 : pos - 1].strip()
        return base, hint if hint else None

    return symbol, None


def _resolve_ambiguous_symbol(output: str, hint: str | None) -> str | None:
    """Try to resolve an ambiguous symbol from objdiff's 'Multiple matches' output.

    Args:
        output: The full stdout from objdiff-cli containing the candidate list
        hint: Optional parameter type hint from user (e.g., 'BaseSkeleton*')

    Returns:
        The resolved "Class::Method(args)" string suitable for objdiff, or None.
    """
    if "Multiple matches" not in output:
        return None

    # Extract candidate demangled names from the output
    # Lines look like: "  public: void __cdecl Class::Method(args) (unit/path)"
    candidates = []
    for line in output.strip().splitlines():
        line = line.strip()
        if not line or line.startswith("Multiple matches") or line.startswith("Failed"):
            continue
        # Find the last " (" which starts the unit path
        last_paren = line.rfind(" (")
        if last_paren > 0:
            full_demangled = line[:last_paren].strip()
            # Extract "Class::Method(args)" from "access: rettype __cdecl Class::Method(args)"
            short = full_demangled
            for prefix in ("public: ", "protected: ", "private: "):
                if short.startswith(prefix):
                    short = short[len(prefix):]
            # Strip return type + calling convention
            cdecl_idx = short.find("__cdecl ")
            if cdecl_idx >= 0:
                short = short[cdecl_idx + 8:]
            elif "__thiscall " in short:
                short = short[short.find("__thiscall ") + 11:]
            candidates.append(short)

    if not candidates:
        return None

    if hint:
        # Filter candidates that contain the hint string (case-insensitive)
        hint_lower = hint.lower().rstrip("*& ").lstrip("const ").strip()
        matching = []
        for c in candidates:
            paren_idx = c.find("(", c.find("::") if "::" in c else 0)
            if paren_idx >= 0:
                params = c[paren_idx:]
                if hint_lower in params.lower():
                    matching.append(c)

        if len(matching) == 1:
            return matching[0]
        if matching:
            # Prefer shorter method name
            matching.sort(key=lambda c: len(c.split("(")[0]))
            return matching[0]

    # No hint: prefer the candidate whose method name (before "(") is shortest.
    # "Class::Set(" should rank before "Class::SetQuatBoneValue("
    candidates.sort(key=lambda c: len(c.split("(")[0]))
    return candidates[0]


class DecompMCPServer:
    """MCP Server providing decomp orchestration tools."""

    def __init__(self, db_path: str, rb3wii_path: str | None = None, dc3_path: str | None = None, record_attempts: bool = True):
        self.db_path = db_path
        # rb3-Wii dev decomp: richer source oracle for RB3 game code (named funcs + MILO_ASSERT strings)
        self.rb3wii_path = rb3wii_path or os.path.expanduser("~/code/milohax/rb3/src")
        # DC3 source: cleaner MSVC X360 reference (same compiler, same flags)
        self.dc3_path = dc3_path or os.path.expanduser("~/code/milohax/dc3-decomp/src")
        # DC3 build report: used to rank lookup_dc3 results by matched_functions_percent
        self.dc3_report_path = os.path.expanduser(
            "~/code/milohax/dc3-decomp/build/373307D9/report.json"
        )
        self._dc3_report_cache: dict[str, float] | None = None
        self._dc3_report_mtime: float | None = None
        self.record_attempts = record_attempts
        # Determine project root from script location (more reliable than cwd)
        self.project_root = Path(__file__).resolve().parent.parent.parent
        self.server = Server("decomp")
        self._setup_tools()

    def _setup_tools(self):
        """Register all MCP tools."""

        @self.server.list_tools()
        async def list_tools() -> list[Tool]:
            return [
                Tool(
                    name="report_result",
                    description="Report task completion. Call when done working on a function.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "symbol": {
                                "type": "string",
                                "description": "Function symbol (mangled name) being reported on",
                            },
                            "status": {
                                "type": "string",
                                "enum": ["complete", "at_limit", "stuck", "error"],
                                "description": "Exit status: complete (100%), at_limit (unfixable), stuck (need help), error",
                            },
                            "percent": {
                                "type": "number",
                                "description": "Final match percentage (0-100)",
                            },
                            "notes": {
                                "type": "string",
                                "description": "Summary of what was tried",
                            },
                            "model": {
                                "type": "string",
                                "description": "Model that worked on this (e.g., 'sonnet', 'haiku', 'opus')",
                            },
                        },
                        "required": ["symbol", "status", "percent", "notes"],
                    },
                ),
                Tool(
                    name="query_functions",
                    description="Query the function database for potential work targets.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "min_percent": {
                                "type": "number",
                                "description": "Minimum match percentage (default: 0)",
                            },
                            "max_percent": {
                                "type": "number",
                                "description": "Maximum match percentage (default: 100)",
                            },
                            "unit_pattern": {
                                "type": "string",
                                "description": "Glob pattern for unit path (e.g., 'src/system/char/*')",
                            },
                            "limit": {
                                "type": "integer",
                                "description": "Max results to return (default: 20)",
                            },
                            "status": {
                                "type": "string",
                                "description": "Filter by function status: 'workable' (default, excludes complete/at_limit), 'all' (no filtering), 'complete' (only complete), 'at_limit' (only at_limit)",
                                "enum": ["workable", "all", "complete", "at_limit"],
                            },
                            "skip_boilerplate": {
                                "type": "boolean",
                                "description": "Filter out boilerplate symbols: atexit destructors (??__F), dynamic initializers (??__E), MakeString templates, vcall thunks (??_9), vector ctor/dtor iterators. Default: true.",
                            },
                            "unicorn_verdict": {
                                "type": "string",
                                "description": "Filter by unicorn verdict: 'DIVERGENT' (behavior differs), 'EQUIVALENT' (behavior matches), 'SKIPPED', 'ERROR'",
                                "enum": ["DIVERGENT", "EQUIVALENT", "SKIPPED", "ERROR"],
                            },
                            "unicorn_class": {
                                "type": "string",
                                "description": (
                                    "Filter by divergence class (only when unicorn_verdict='DIVERGENT'). "
                                    "Real bugs: 'logic', 'call_count', 'call_arg', 'return_value', "
                                    "'object_memory', 'error', 'wild_jump_match', 'cap_exhausted', "
                                    "'cap_exhausted_decomp'. "
                                    "Unfixable artifacts: 'build_env', 'regalloc', 'merged_call', "
                                    "'merged_arg', 'stack_layout', 'fpr_precision', 'orig_error', "
                                    "'cap_exhausted_orig'."
                                ),
                                "enum": [
                                    "logic", "build_env", "regalloc",
                                    "call_count", "call_arg", "return_value",
                                    "object_memory", "error", "orig_error",
                                    "merged_call", "merged_arg",
                                    "stack_layout", "fpr_precision",
                                    "wild_jump_match",
                                    "cap_exhausted", "cap_exhausted_decomp",
                                    "cap_exhausted_orig",
                                ],
                            },
                            "unicorn_confidence": {
                                "type": "string",
                                "description": (
                                    "Filter by unicorn confidence label: 'high' (all probe runs agreed), "
                                    "'stable_divergent' (all runs divergent), 'input_sensitive' (mixed), "
                                    "'fixture_sensitive' (legacy dual-fixture label)."
                                ),
                                "enum": [
                                    "high", "stable_divergent",
                                    "input_sensitive", "fixture_sensitive",
                                ],
                            },
                            "is_stub": {
                                "type": "boolean",
                                "description": "Filter by stub status: true = only unimplemented stubs, false = only non-stubs. Omit to return all.",
                            },
                        },
                    },
                ),
                Tool(
                    name="get_attempts",
                    description="Get previous attempt history for a function to learn from.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "symbol": {
                                "type": "string",
                                "description": "Function symbol (mangled name)",
                            },
                        },
                        "required": ["symbol"],
                    },
                ),
                Tool(
                    name="lookup_rb3wii",
                    description="Search rb3-Wii dev decomp for similar implementation (richer source oracle: named funcs + MILO_ASSERT path strings).",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "symbol": {
                                "type": "string",
                                "description": "Function symbol or method name to search for",
                            },
                        },
                        "required": ["symbol"],
                    },
                ),
                Tool(
                    name="lookup_dc3",
                    description="Search DC3 decomp source for a method or class name (shared Milo engine, same MSVC X360 compiler/flags as rb3-xenon). Results are deduped per file and ranked by the DC3 unit's matched_functions_percent. Accepts a method name, qualified name, or MWCC mangled symbol — extraction is automatic.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "symbol": {
                                "type": "string",
                                "description": "Method name (e.g. 'Poll'), qualified name (e.g. 'CharServoBone::Poll'), or MWCC mangled symbol. Mangled symbols are auto-extracted to their method name.",
                            },
                            "min_match": {
                                "type": "number",
                                "description": "Only return files whose DC3 unit is at least this matched_functions_percent (0-100). Default: 0 (no filter).",
                            },
                        },
                        "required": ["symbol"],
                    },
                ),
                Tool(
                    name="run_objdiff",
                    description="Build and diff a function, returning match% and verdict. Handles large output automatically.\n\n⚠️ CRITICAL: Pass project_dir parameter when in a worktree or your edits won't be tested! Without project_dir, the tool tests the main repo code instead of your changes, making edits invisible to match%.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "symbol": {
                                "type": "string",
                                "description": "Function symbol (mangled name)",
                            },
                            "full_build": {
                                "type": "boolean",
                                "description": "Force full rebuild (slower but more accurate). Default: false (incremental)",
                            },
                            "project_dir": {
                                "type": "string",
                                "description": "Project directory to build from. Pass your worktree directory here to test your changes.",
                            },
                            "context": {
                                "type": "integer",
                                "description": "Show N instructions of context before/after each mismatch (like grep -C). Default: 3.",
                            },
                            "concise": {
                                "type": "boolean",
                                "description": "Concise output: match%, compact summary, patterns, verdict headline. Default: true. Set false for full instruction table + auto-diagnosis.",
                            },
                            "full_listing": {
                                "type": "boolean",
                                "description": "Show ALL instructions (matching + mismatched) instead of only mismatches. Use when diagnosing isolated mismatches where surrounding matching instructions provide data flow context. Default: false.",
                            },
                            "unit": {
                                "type": "string",
                                "description": "Unit name to disambiguate when a symbol exists in multiple units (e.g. 'default/link_glue'). Required when objdiff reports 'Multiple instances found'.",
                            },
                        },
                        "required": ["symbol", "project_dir"],
                    },
                ),
                Tool(
                    name="run_analyze_function",
                    description="Run enriched function analysis combining objdiff with struct offset resolution. Returns detailed diff with field names for offset mismatches. Detects unfixable patterns (struct offsets, merged calls, etc.).\n\n⚠️ CRITICAL: Pass project_dir parameter when in a worktree or your edits won't be tested!",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "symbol": {
                                "type": "string",
                                "description": "Function symbol (mangled or demangled name)",
                            },
                            "resolve_offsets": {
                                "type": "boolean",
                                "description": "Resolve struct field names for offset mismatches (default: true)",
                            },
                            "output_format": {
                                "type": "string",
                                "enum": ["markdown", "json"],
                                "description": "Output format (default: markdown)",
                            },
                            "project_dir": {
                                "type": "string",
                                "description": "Project directory to build from. Pass your worktree directory here to test your changes.",
                            },
                        },
                        "required": ["symbol", "project_dir"],
                    },
                ),
                Tool(
                    name="run_diff_inspect",
                    description="Deep analysis of WHY a function doesn't match. Provides root cause diagnosis, cluster analysis, register swap detection, offset analysis, replace categorization, and before/after comparison. Use after run_objdiff when you need deeper insight into mismatches.\n\n⚠️ CRITICAL: Pass project_dir parameter when in a worktree!",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "symbol": {
                                "type": "string",
                                "description": "Function symbol (mangled or demangled name)",
                            },
                            "mode": {
                                "type": "string",
                                "enum": ["diagnose", "clusters", "regswaps", "offsets", "replaces", "compare", "save_baseline", "mismatches", "asm_listing", "stack-layout"],
                                "description": "Analysis mode: diagnose (root cause), clusters (contiguous insert/delete groups), regswaps (register swap pairs), offsets (offset shift histogram), replaces (categorize noise vs real), compare (delta vs baseline), save_baseline (save current state), mismatches (list all mismatched instructions with target/base details), asm_listing (compile with /FAs and return source-annotated assembly with var->register mapping)",
                            },
                            "project_dir": {
                                "type": "string",
                                "description": "Project directory to build from. Pass your worktree directory here.",
                            },
                            "baseline_json": {
                                "type": "string",
                                "description": "Optional: path to baseline JSON file for compare mode. If omitted, auto-finds baseline saved by orchestrator.",
                            },
                            "unit": {
                                "type": "string",
                                "description": "Unit name to disambiguate when a symbol exists in multiple units (e.g. 'default/link_glue').",
                            },
                            "diff_mode": {
                                "type": "string",
                                "enum": ["normalized", "raw"],
                                "description": "Diff scoring mode. 'normalized' (default) ignores relocation address differences (functionRelocDiffs=none). 'raw' includes relocation diffs so you can inspect which relocations differ.",
                            },
                        },
                        "required": ["symbol", "mode", "project_dir"],
                    },
                ),
                Tool(
                    name="lookup_struct_offset",
                    description="Look up which struct field is at a given offset. Use when objdiff shows offset mismatches like 'stw r10, 0x118(r11)' vs 'stw r10, 0xf4(r11)' to identify which field is being accessed.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "class_name": {
                                "type": "string",
                                "description": "Class or struct name (e.g., 'Game', 'RndTransformable')",
                            },
                            "offset": {
                                "type": "string",
                                "description": "Offset to look up (hex with 0x prefix or decimal, e.g., '0x48' or '72')",
                            },
                        },
                        "required": ["class_name", "offset"],
                    },
                ),
                Tool(
                    name="lookup_merged_symbol",
                    description="Look up symbols at a merged address. When objdiff shows LINKER_MERGED pattern with 'merged_82331360', use this to find which actual symbols are at that address. ICF (Identical COMDAT Folding) merges functions with identical machine code.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "address": {
                                "type": "string",
                                "description": "Address to look up (e.g., '82331360' or 'merged_82331360')",
                            },
                        },
                        "required": ["address"],
                    },
                ),
                Tool(
                    name="mark_patch_result",
                    description="Mark a queued patch as applied, failed, or skipped. Used by the merger agent after attempting to apply a patch.",
                    inputSchema={
                        "type": "object",
                        "properties": {
                            "patch_queue_id": {
                                "type": "integer",
                                "description": "Patch queue ID from the manifest",
                            },
                            "status": {
                                "type": "string",
                                "enum": ["applied", "failed", "skipped"],
                                "description": "Result of applying the patch",
                            },
                            "reason": {
                                "type": "string",
                                "description": "Explanation if failed or skipped",
                            },
                        },
                        "required": ["patch_queue_id", "status"],
                    },
                ),
            ]

        @self.server.call_tool()
        async def call_tool(name: str, arguments: dict) -> list[TextContent]:
            if name == "report_result":
                return await self._report_result(arguments)
            elif name == "query_functions":
                return await self._query_functions(arguments)
            elif name == "get_attempts":
                return await self._get_attempts(arguments)
            elif name == "lookup_rb3wii":
                return await self._lookup_rb3wii(arguments)
            elif name == "lookup_dc3":
                return await self._lookup_dc3(arguments)
            elif name == "run_objdiff":
                return await self._run_objdiff(arguments)
            elif name == "run_analyze_function":
                return await self._run_analyze_function(arguments)
            elif name == "run_diff_inspect":
                return await self._run_diff_inspect(arguments)
            elif name == "lookup_struct_offset":
                return await self._lookup_struct_offset(arguments)
            elif name == "lookup_merged_symbol":
                return await self._lookup_merged_symbol(arguments)
            elif name == "mark_patch_result":
                return await self._mark_patch_result(arguments)
            else:
                return [TextContent(type="text", text=f"Unknown tool: {name}")]

    async def _report_result(self, args: dict) -> list[TextContent]:
        """Handle report_result tool call."""
        symbol = args.get("symbol", "")
        status = args.get("status", "unknown")
        percent = args.get("percent", 0)
        notes = args.get("notes", "")
        model = args.get("model", "unknown")

        # Store attempt in database if symbol is provided.
        # When record_attempts is False (orchestrator mode), skip DB writes —
        # the orchestrator records attempts itself after the agent returns,
        # preventing phantom attempts from crashes.
        db_stored = False
        if symbol and self.record_attempts:
            func = get_function_by_symbol(symbol, db_path=self.db_path)
            if func:
                start_percent = func.get("current_percent") or 0

                # Guard: validate base_size > 0 before accepting COMPLETE
                if status == "complete":
                    try:
                        check_result = subprocess.run(
                            [str(self.project_root / "bin" / "objdiff-cli"), "diff", "-p", str(self.project_root), symbol,
                             "-c", "functionRelocDiffs=none"],
                            capture_output=True, text=True, timeout=60,
                        )
                        stdout = check_result.stdout
                        json_start = stdout.find("{")
                        if json_start >= 0:
                            check_data = json.loads(stdout[json_start:])
                            if check_data.get("base_size", 0) == 0:
                                # Mark as stub and reject COMPLETE
                                conn = get_connection(self.db_path)
                                conn.execute(
                                    "UPDATE functions SET is_stub = 1, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
                                    (func["id"],),
                                )
                                conn.commit()
                                return [TextContent(
                                    type="text",
                                    text=f"Cannot mark as COMPLETE — base_size=0 (unimplemented stub). "
                                         f"Function `{symbol}` has no original code to compare against.",
                                )]
                    except Exception:
                        pass  # If check fails, allow the report through

                # Determine verdict from status
                verdict = None
                if status == "at_limit":
                    verdict = "AT_LIMIT"
                elif status == "complete":
                    verdict = "COMPLETE"

                # Record the attempt
                record_attempt(
                    function_id=func["id"],
                    session_id="mcp_direct",
                    model=model,
                    start_percent=start_percent,
                    end_percent=percent,
                    exit_status=status,
                    verdict=verdict,
                    notes=notes,
                    db_path=self.db_path,
                )

                # Update function status
                update_function_status(
                    function_id=func["id"],
                    current_percent=percent,
                    verdict=verdict,
                    db_path=self.db_path,
                )
                db_stored = True

        # Format response that orchestrator can parse
        result = {
            "_decomp_exit": True,  # Signal to orchestrator: agent is done
            "status": status,
            "percent": percent,
            "notes": notes,
        }

        status_msg = f"Result recorded: {status} at {percent}%"
        if db_stored:
            status_msg += " (stored to database)"
        elif symbol:
            status_msg += f" (function not found in database: {symbol})"

        return [
            TextContent(
                type="text",
                text=f"{status_msg}\n\n```json\n{json.dumps(result, indent=2)}\n```",
            )
        ]

    async def _query_functions(self, args: dict) -> list[TextContent]:
        """Handle query_functions tool call."""
        min_percent = args.get("min_percent", 0)
        max_percent = args.get("max_percent", 100)
        pattern = args.get("unit_pattern", "*")
        limit = args.get("limit", 20)
        status = args.get("status", "workable")
        skip_boilerplate = args.get("skip_boilerplate", True)
        unicorn_verdict = args.get("unicorn_verdict")
        unicorn_class = args.get("unicorn_class")
        unicorn_confidence = args.get("unicorn_confidence")
        is_stub = args.get("is_stub")

        # Map status filter to database query params
        if status == "all":
            exclude_complete = False
            exclude_at_limit = False
            verdict_filter = None
        elif status == "complete":
            exclude_complete = False
            exclude_at_limit = True
            verdict_filter = "COMPLETE"
        elif status == "at_limit":
            exclude_complete = True
            exclude_at_limit = False
            verdict_filter = "AT_LIMIT"
        else:  # "workable" (default)
            exclude_complete = True
            exclude_at_limit = True
            verdict_filter = None

        results = db_query_functions(
            pattern=pattern,
            min_percent=min_percent,
            max_percent=max_percent,
            exclude_complete=exclude_complete,
            exclude_at_limit=exclude_at_limit,
            verdict_filter=verdict_filter,
            limit=limit,
            db_path=self.db_path,
            skip_boilerplate=skip_boilerplate,
            unicorn_verdict=unicorn_verdict,
            unicorn_class=unicorn_class,
            unicorn_confidence=unicorn_confidence,
            is_stub=is_stub,
        )

        # When filtering by unit, check if there are hidden functions
        hidden_note = ""
        if status != "all" and pattern != "*":
            all_results = db_query_functions(
                pattern=pattern,
                min_percent=0,
                max_percent=100,
                exclude_complete=False,
                exclude_at_limit=False,
                verdict_filter=None,
                limit=9999,
                max_attempts=None,
                db_path=self.db_path,
            )
            total = len(all_results)
            if total > len(results):
                hidden_note = (
                    f"\n---\n"
                    f"Note: Showing {len(results)} of {total} functions "
                    f"(filtered by status='{status}'). "
                    f"Use status='all' to see all functions in this unit."
                )

        if not results:
            msg = "No functions found matching criteria."
            if hidden_note:
                msg += hidden_note
            return [TextContent(type="text", text=msg)]

        # Format results, capping output at 30 entries to avoid massive responses
        max_display = 30
        output = f"Found {len(results)} functions"
        if len(results) > max_display:
            output += f" (showing first {max_display})"
        output += ":\n\n"
        for func in results[:max_display]:
            pct = func.get("current_percent")
            pct_str = f"{pct:.1f}%" if pct is not None else "unimplemented"
            verdict = func.get("verdict")
            verdict_reason = func.get("verdict_reason")
            verdict_str = f" | Verdict: {verdict}" if verdict else ""
            if verdict_reason:
                verdict_str += f" ({verdict_reason})"
            output += f"- `{func['symbol']}` ({func.get('demangled', 'N/A')})\n"
            output += f"  Unit: {func.get('unit', 'unknown')} | Match: {pct_str}{verdict_str}\n"

        if len(results) > max_display:
            output += f"\n... and {len(results) - max_display} more (use a narrower unit_pattern or limit to see specific results)\n"

        if hidden_note:
            output += hidden_note

        return [TextContent(type="text", text=output)]

    async def _get_attempts(self, args: dict) -> list[TextContent]:
        """Handle get_attempts tool call."""
        symbol = args.get("symbol", "")

        func = get_function_by_symbol(symbol, db_path=self.db_path)
        if not func:
            return [TextContent(type="text", text=f"Function not found: {symbol}")]

        attempts = get_attempts_for_function(func["id"], limit=10, db_path=self.db_path)

        if not attempts:
            return [TextContent(type="text", text="No previous attempts for this function.")]

        output = f"## Previous Attempts for {symbol}\n\n"
        output += f"**Current Status:** {func.get('current_percent', 'unknown')}% match, Verdict: {func.get('verdict', 'unknown')}\n\n"

        for i, attempt in enumerate(attempts, 1):
            # Use 'or 0' instead of default param - .get() returns None if key exists with None value
            start_pct = attempt.get('start_percent') or 0
            end_pct = attempt.get('end_percent') or 0
            change = end_pct - start_pct
            change_str = f"+{change:.1f}%" if change >= 0 else f"{change:.1f}%"

            # Status interpretation for clarity
            status = attempt.get('exit_status', 'unknown')
            status_emoji = "✓" if status == "complete" else "✗" if status == "error" else "⊘"

            output += f"### Attempt {i}: {status_emoji} {status.upper()}\n"
            output += f"- **Model:** {attempt.get('model', 'unknown')}\n"
            output += f"- **Match:** {start_pct:.1f}% → {end_pct:.1f}% ({change_str})\n"
            if attempt.get("verdict"):
                output += f"- **Verdict:** {attempt['verdict']}\n"
            if attempt.get("iterations"):
                output += f"- **Iterations:** {attempt['iterations']} tool calls\n"
            if attempt.get("notes"):
                # Limit notes length in output
                notes = attempt['notes']
                if len(notes) > 200:
                    notes = notes[:200] + "..."
                output += f"- **Notes:** {notes}\n"
            output += "\n"

        output += "---\n\n**Strategy Tips:**\n"
        output += "- Review what previous attempts tried (notes field)\n"
        output += "- Avoid repeating the same changes\n"
        output += "- Look for patterns in what worked vs what didn't\n"
        output += "- If match% stopped improving, function may be at limit\n"

        return [TextContent(type="text", text=output)]

    # ========================================================================
    # Enrichment pipeline helpers for run_objdiff
    # ========================================================================

    # Regex for parsing PPC memory operands: rX, 0xOFF(rY) or rX, OFF(rY)
    _MEM_ARG_RE = re.compile(r'r(\d+),\s*(-?0x[0-9a-fA-F]+|-?\d+)\(r(\d+)\)')
    # Regex for parsing PPC immediate operands: rX, rY, IMM
    _SHIFT_ARG_RE = re.compile(r'r(\d+),\s*r(\d+),\s*(\d+)')
    # Memory opcodes that access struct fields
    _MEM_OPCODES = frozenset([
        'lwz', 'stw', 'lfs', 'stfs', 'lhz', 'sth', 'lbz', 'stb', 'lfd', 'stfd',
        'lwzu', 'stwu', 'lfsu', 'stfsu', 'lha', 'lhau',
    ])
    # Shift/rotate opcodes
    _SHIFT_OPCODES = frozenset(['slwi', 'srwi', 'slw', 'srw', 'rlwinm'])

    @staticmethod
    def _extract_class_from_demangled(demangled: str) -> str | None:
        """Extract class name from demangled symbol like 'ClassName::Method(...)'."""
        m = re.search(r'(\w+)::\w+\s*\(', demangled)
        return m.group(1) if m else None

    @staticmethod
    def _parse_hex_or_int(s: str) -> int:
        """Parse a string as hex (0x...) or decimal integer."""
        s = s.strip()
        if s.startswith(('0x', '0X', '-0x', '-0X')):
            return int(s, 16)
        return int(s)

    def _resolve_offset_mismatches(self, data: dict) -> list[dict]:
        """
        Scan instruction diffs for memory offset mismatches and resolve
        them to struct field names using StructDB.

        Returns list of offset mismatch records with field names.
        """
        instructions = data.get("instructions") or data.get("mismatch_instructions") or []
        demangled = data.get("demangled", "")
        class_name = self._extract_class_from_demangled(demangled)

        if not class_name and not instructions:
            return []

        struct_db_path = self.project_root / "struct_db.sqlite"
        if not struct_db_path.exists():
            return []

        mismatches = []
        try:
            with StructDB(str(struct_db_path)) as db:
                for instr in instructions:
                    match_type = instr.get("match_type")
                    if match_type != "diff_arg":
                        continue

                    target = instr.get("target", {})
                    base = instr.get("base", {})
                    opcode_t = target.get("opcode", "")
                    opcode_b = base.get("opcode", "")

                    # Both must be memory opcodes
                    if opcode_t not in self._MEM_OPCODES or opcode_b not in self._MEM_OPCODES:
                        continue

                    args_t = target.get("args", "")
                    args_b = base.get("args", "")
                    m_t = self._MEM_ARG_RE.search(args_t)
                    m_b = self._MEM_ARG_RE.search(args_b)

                    if not m_t or not m_b:
                        continue

                    off_t = self._parse_hex_or_int(m_t.group(2))
                    off_b = self._parse_hex_or_int(m_b.group(2))

                    if off_t == off_b:
                        continue  # Same offset, different register — not a struct mismatch

                    # Resolve field names
                    target_field = None
                    base_field = None

                    if class_name:
                        result_t = db.lookup(class_name, off_t)
                        result_b = db.lookup(class_name, off_b)
                        if result_t:
                            target_field = f"{result_t[0]}::{result_t[1]} ({result_t[2]})"
                        if result_b:
                            base_field = f"{result_b[0]}::{result_b[1]} ({result_b[2]})"

                    entry = {
                        "index": instr.get("index"),
                        "opcode": opcode_t,
                        "target_offset": f"0x{off_t:x}",
                        "base_offset": f"0x{off_b:x}",
                    }
                    if target_field:
                        entry["target_field"] = target_field
                    if base_field:
                        entry["base_field"] = base_field
                    if target_field and base_field:
                        entry["fix_hint"] = (
                            f"Source accesses '{base_field.split('::')[-1].split(' (')[0]}' "
                            f"but target accesses '{target_field.split('::')[-1].split(' (')[0]}' — wrong field?"
                        )

                    mismatches.append(entry)
        except Exception:
            pass  # Don't let struct DB errors break objdiff

        return mismatches

    @staticmethod
    def _detect_stack_copy_ref(data: dict) -> list[dict]:
        """
        Detect pass-by-reference via stack copy pattern.

        Target copies a member to stack then passes stack address,
        while base passes the member address directly.
        """
        instructions = data.get("instructions") or data.get("mismatch_instructions") or []
        if not instructions:
            return []

        patterns_found = []

        # Build index of instructions by position for context lookups
        instr_by_idx = {instr.get("index"): instr for instr in instructions}

        # Look for sequences: delete stw/stfs to r1 (stack store) near diff_arg addi with r1
        for instr in instructions:
            match_type = instr.get("match_type")
            target = instr.get("target", {})
            base = instr.get("base", {})

            if match_type != "diff_arg":
                continue

            # Check if this is an addi where target uses r1 (stack) but base doesn't
            if target.get("opcode") != "addi" or base.get("opcode") != "addi":
                continue

            t_args = target.get("args", "")
            b_args = base.get("args", "")

            # Target: addi rN, r1, stackoff (passing stack address)
            # Base: addi rN, rX, offset (passing member address directly)
            if ", r1," in t_args and ", r1," not in b_args:
                idx = instr.get("index", -1)
                # Look for nearby stack stores (delete instructions)
                nearby_stores = []
                for check_idx in range(max(0, idx - 5), idx):
                    nearby = instr_by_idx.get(check_idx)
                    if nearby and nearby.get("match_type") == "delete":
                        t = nearby.get("target", {})
                        op = t.get("opcode", "")
                        args = t.get("args", "")
                        if op in ("stw", "stfs", "sth", "stb", "stfd") and "r1" in args:
                            nearby_stores.append(check_idx)

                if nearby_stores:
                    patterns_found.append({
                        "pattern": "STACK_COPY_REF",
                        "confidence": "high",
                        "fixability": "likely_fixable",
                        "instruction_indices": nearby_stores + [idx],
                        "fix_hint": (
                            "Target copies member to stack before passing as const-ref. "
                            "Fix: assign to a local variable before the call."
                        ),
                    })

        return patterns_found

    @staticmethod
    def _annotate_shift_semantics(data: dict) -> list[dict]:
        """
        Annotate shift instructions with multiplication/division equivalents.

        slwi r10, r11, 3 → "×8", srwi r10, r11, 2 → "÷4"
        """
        instructions = data.get("instructions") or data.get("mismatch_instructions") or []
        annotations = []
        shift_re = re.compile(r'r(\d+),\s*r(\d+),\s*(\d+)')

        for instr in instructions:
            if instr.get("match_type") != "diff_arg":
                continue

            target = instr.get("target", {})
            base = instr.get("base", {})
            t_op = target.get("opcode", "")
            b_op = base.get("opcode", "")

            # At least one side must be a shift opcode
            if t_op not in ('slwi', 'srwi', 'slw', 'srw', 'rlwinm') and \
               b_op not in ('slwi', 'srwi', 'slw', 'srw', 'rlwinm'):
                continue

            def shift_meaning(opcode: str, args: str) -> str | None:
                m = shift_re.search(args)
                if not m:
                    return None
                amount = int(m.group(3))
                if opcode in ('slwi', 'slw'):
                    return f"×{1 << amount}"
                elif opcode in ('srwi', 'srw'):
                    return f"÷{1 << amount}"
                elif opcode == 'rlwinm':
                    return f"rotate/mask by {amount}"
                return None

            t_meaning = shift_meaning(t_op, target.get("args", ""))
            b_meaning = shift_meaning(b_op, base.get("args", ""))

            if t_meaning or b_meaning:
                ann = {
                    "index": instr.get("index"),
                    "target": {"opcode": t_op, "args": target.get("args", "")},
                    "base": {"opcode": b_op, "args": base.get("args", "")},
                    "match_type": "diff_arg",
                }
                parts = []
                if t_meaning:
                    parts.append(f"target: {t_meaning}")
                if b_meaning:
                    parts.append(f"base: {b_meaning}")
                ann["annotation"] = ", ".join(parts)
                annotations.append(ann)

        return annotations

    @staticmethod
    def _refine_register_swap_confidence(data: dict) -> None:
        """
        Refine REGISTER_SWAP pattern confidence based on register types.

        If all swapped registers are floating-point (f0-f31), downgrade
        fixability to unlikely_fixable since FP register allocation is
        rarely controllable from source.

        Mutates data["analysis"]["patterns"] in-place.
        """
        analysis = data.get("analysis", {})
        patterns = analysis.get("patterns", [])

        for pattern in patterns:
            if pattern.get("pattern") != "REGISTER_SWAP":
                continue

            details = pattern.get("details", {})
            swaps = details.get("swaps", [])
            if not swaps:
                continue

            fp_re = re.compile(r'^f\d+$')
            fp_count = 0
            int_count = 0

            for swap in swaps:
                t_reg = swap.get("target_reg", "")
                b_reg = swap.get("base_reg", "")
                if fp_re.match(t_reg) or fp_re.match(b_reg):
                    fp_count += 1
                else:
                    int_count += 1

            # Annotate register type
            if fp_count > 0 and int_count == 0:
                details["register_type"] = "float"
                # Check if there are other fixable patterns
                other_fixable = any(
                    p.get("pattern") != "REGISTER_SWAP"
                    and p.get("fixability") in ("fixable", "likely_fixable", "maybe_fixable")
                    for p in patterns
                )
                if not other_fixable:
                    pattern["fixability"] = "unlikely_fixable"
                    pattern["fix_hint"] = (
                        "FP register allocation — rarely fixable from source. "
                        "Consider accepting as at_limit."
                    )
            elif fp_count > 0 and int_count > 0:
                details["register_type"] = "mixed"
            else:
                details["register_type"] = "integer"

    def _inline_rb3_method_source(self, data: dict) -> dict | None:
        """
        Look up and return RB3 reference source for the method.

        Returns a dict with rb3_reference info including method_source,
        or None if not available.
        """
        demangled = data.get("demangled", "")
        if not demangled:
            return None

        # Extract class and method name
        class_name = self._extract_class_from_demangled(demangled)
        if not class_name:
            return None

        # Extract method name
        m = re.search(r'(\w+)::(\w+)\s*\(', demangled)
        if not m:
            return None
        method_name = m.group(2)

        # Find the unit for this symbol to look up the RB3 pair
        symbol = data.get("symbol", "")
        source_file = data.get("source_file", "")

        # Try to find RB3 file via unit
        rb3_file_path = None
        if source_file:
            # Convert source_file path to unit for file_pair lookup
            unit = source_file.replace("src/", "default/").rsplit(".", 1)[0]
            pair = get_file_pair(unit, db_path=self.db_path)
            if pair and pair.get("rb3_file"):
                rb3_file_path = Path(pair["rb3_file"])

        # Fallback: search by class name in rb3-Wii src
        if not rb3_file_path:
            rb3wii = Path(self.rb3wii_path)
            # Simple fallback: find a .cpp file named after the class
            candidates = list(rb3wii.rglob(f"{class_name}.cpp"))
            if candidates:
                rb3_file_path = candidates[0]

        if not rb3_file_path or not rb3_file_path.exists():
            return None

        try:
            source = rb3_file_path.read_text(errors="replace")
        except Exception:
            return None

        # Extract the specific method source
        method_source = self._extract_method_source(source, class_name, method_name)
        if not method_source:
            return {"available": True, "rb3_file": str(rb3_file_path), "method_found": False}

        # Cap at 60 lines
        lines = method_source.split("\n")
        if len(lines) > 60:
            method_source = "\n".join(lines[:60]) + "\n// ... (truncated)"

        return {
            "available": True,
            "rb3_file": str(rb3_file_path),
            "method_found": True,
            "method_source": method_source,
        }

    @staticmethod
    def _extract_method_source(source: str, class_name: str, method_name: str) -> str | None:
        """
        Extract a method's source code from a C++ file.

        Finds the method definition and extracts through its closing brace.
        """
        # Look for ClassName::MethodName pattern
        # Handle various return types before the class::method
        pattern = re.compile(
            rf'^[^\n]*\b{re.escape(class_name)}::{re.escape(method_name)}\s*\(',
            re.MULTILINE
        )
        match = pattern.search(source)
        if not match:
            return None

        start = match.start()

        # Find opening brace
        brace_pos = source.find('{', match.end())
        if brace_pos == -1:
            return None

        # Count braces to find the matching closing brace
        depth = 1
        pos = brace_pos + 1
        while pos < len(source) and depth > 0:
            ch = source[pos]
            if ch == '{':
                depth += 1
            elif ch == '}':
                depth -= 1
            pos += 1

        if depth != 0:
            return None

        return source[start:pos].strip()

    def _suggest_similar_symbols(self, symbol: str) -> list[str]:
        """
        When a symbol is not found, suggest similar symbols from the database.

        Returns formatted suggestion strings.
        """
        # Extract search term from mangled symbol
        search_term = symbol
        if "@" in symbol:
            # MSVC mangled: ?MethodName@ClassName@@...
            parts = symbol.split("@")
            method = parts[0].lstrip("?")
            if len(parts) >= 2:
                cls = parts[1]
                search_term = f"{cls}::{method}"
        elif "::" in symbol:
            search_term = symbol

        try:
            results = search_functions_by_name(search_term, limit=5, db_path=self.db_path)
            if not results:
                # Try just the method name
                method_only = search_term.split("::")[-1] if "::" in search_term else search_term
                results = search_functions_by_name(method_only, limit=5, db_path=self.db_path)

            suggestions = []
            for r in results:
                pct = r.get("current_percent")
                pct_str = f" ({pct:.1f}%)" if pct is not None else ""
                suggestions.append(f"`{r['symbol']}`{pct_str}")
            return suggestions
        except Exception:
            return []

    def _enrich_objdiff_data(self, data: dict) -> dict:
        """
        Run the full enrichment pipeline on parsed objdiff JSON data.

        Adds: offset_mismatches, shift_annotations, stack_copy_ref patterns,
        RB3 method source, and refined register swap confidence.

        Mutates and returns data.
        """
        # 1. Auto-resolve offset mismatches
        offset_mismatches = self._resolve_offset_mismatches(data)
        if offset_mismatches:
            data["offset_mismatches"] = offset_mismatches

        # 2. Detect stack copy ref pattern
        stack_copy_patterns = self._detect_stack_copy_ref(data)
        if stack_copy_patterns:
            # Add to existing analysis patterns if present
            analysis = data.setdefault("analysis", {})
            patterns = analysis.setdefault("patterns", [])
            patterns.extend(stack_copy_patterns)

        # 3. Annotate shift semantics
        shift_annotations = self._annotate_shift_semantics(data)
        if shift_annotations:
            data["shift_annotations"] = shift_annotations

        # 4. Refine REGISTER_SWAP confidence for FP registers
        self._refine_register_swap_confidence(data)

        # 5. Inline RB3 method source
        rb3_ref = self._inline_rb3_method_source(data)
        if rb3_ref:
            data["rb3_reference"] = rb3_ref

        return data

    def _format_enrichment_sections(self, data: dict, skip_rb3: bool = False) -> str:
        """
        Format enrichment annotations as markdown sections to append to
        the built-in objdiff markdown output.

        Covers: offset mismatches, shift semantics, detected patterns,
        and RB3 reference source.
        """
        lines = []

        # Offset mismatches with resolved field names
        offset_mismatches = data.get("offset_mismatches", [])
        if offset_mismatches:
            lines.append("")
            lines.append("## Offset Mismatches (resolved)")
            lines.append("")
            for om in offset_mismatches:
                idx = om.get("index", "?")
                opcode = om.get("opcode", "?")
                t_off = om.get("target_offset", "?")
                b_off = om.get("base_offset", "?")
                t_field = om.get("target_field", "")
                b_field = om.get("base_field", "")
                hint = om.get("fix_hint", "")
                line = f"- [{idx}] `{opcode}`: target {t_off}"
                if t_field:
                    line += f" ({t_field})"
                line += f" vs base {b_off}"
                if b_field:
                    line += f" ({b_field})"
                if hint:
                    line += f" -- {hint}"
                lines.append(line)

        # Shift annotations
        shift_annotations = data.get("shift_annotations", [])
        if shift_annotations:
            lines.append("")
            lines.append("## Shift Semantics")
            lines.append("")
            for sa in shift_annotations:
                idx = sa.get("index", "?")
                meaning = sa.get("meaning", "?")
                lines.append(f"- [{idx}] {meaning}")

        # Analysis patterns (stack_copy_ref, etc.)
        patterns = data.get("analysis", {}).get("patterns", [])
        if patterns:
            lines.append("")
            lines.append("## Detected Patterns")
            lines.append("")
            for pat in patterns:
                ptype = pat.get("type", "unknown")
                desc = pat.get("description", "")
                fixable = pat.get("fixable", "")
                line = f"- **{ptype}**"
                if desc:
                    line += f": {desc}"
                if fixable:
                    line += f" (fixable: {fixable})"
                lines.append(line)

        # Mismatch preview (adaptive limit based on match %)
        instrs = data.get("instructions", [])
        if instrs:
            mismatches = [ins for ins in instrs if ins.get("match_type") != "equal"]
            if mismatches:
                match_pct = data.get("fuzzy_match_percent", 0)
                total = len(instrs)

                # Adaptive limit
                if match_pct >= 98:
                    limit = len(mismatches)  # show ALL for near-matches
                elif match_pct >= 90:
                    limit = 15
                else:
                    limit = 8

                shown = mismatches[:limit]
                truncated = len(mismatches) > limit

                from analysis.diff_inspect import fmt_instr as _fmt_instr, diff_annotation as _diff_annotation

                if truncated:
                    lines.append("")
                    lines.append(f"## Key Mismatches ({len(shown)} of {len(mismatches)} shown)")
                else:
                    lines.append("")
                    lines.append(f"## Mismatches ({len(mismatches)} of {total} instructions)")

                lines.append("")
                for ins in shown:
                    idx = ins.get("index", "?")
                    mt = ins.get("match_type", "?")
                    t = ins.get("target")
                    b = ins.get("base")
                    t_op = t.get("opcode", "?") if t else "---"
                    b_op = b.get("opcode", "?") if b else "---"

                    if mt == "diff_arg":
                        ann = _diff_annotation(ins).strip()
                        lines.append(f"- [{idx}] {mt}: `{t_op}` {ann}")
                    elif mt == "replace":
                        t_str = _fmt_instr(t).strip()
                        b_str = _fmt_instr(b).strip()
                        lines.append(f"- [{idx}] {mt}: `{t_str}` vs `{b_str}`")
                    elif mt in ("insert", "delete"):
                        side = b if mt == "insert" else t
                        s_str = _fmt_instr(side).strip() if side else "---"
                        lines.append(f"- [{idx}] {mt}: `{s_str}`")
                    elif mt == "diff_op":
                        lines.append(f"- [{idx}] {mt}: `{t_op}` vs `{b_op}`")
                    else:
                        lines.append(f"- [{idx}] {mt}")

                if truncated:
                    lines.append("")
                    lines.append('*(Use `run_diff_inspect mode: "mismatches"` for full list)*')

        # RB3 reference (skip in concise mode)
        rb3_ref = data.get("rb3_reference", {})
        if rb3_ref and rb3_ref.get("available") and not skip_rb3:
            lines.append("")
            rb3_file = rb3_ref.get("rb3_file", "?")
            lines.append(f"## RB3 Reference ({rb3_file})")
            if rb3_ref.get("method_found") and rb3_ref.get("method_source"):
                lines.append("")
                lines.append("```cpp")
                lines.append(rb3_ref["method_source"])
                lines.append("```")

        return "\n".join(lines)

    async def _run_objdiff(self, args: dict) -> list[TextContent]:
        """
        Handle run_objdiff tool call.

        Runs objdiff-cli with smart output handling:
        - If output < 500 lines: return inline
        - If output >= 500 lines: write to file and return path + instructions
        """
        symbol = args.get("symbol", "")
        full_build = args.get("full_build", False)
        project_dir_arg = args.get("project_dir", None)
        context = args.get("context", 3)
        concise = args.get("concise", True)
        full_listing = args.get("full_listing", False)
        unit = args.get("unit", None)

        if not symbol:
            return [TextContent(type="text", text="Error: No symbol provided.")]

        if symbol.startswith("merged_"):
            return [TextContent(type="text", text=f"Error: {symbol} is a linker ICF artifact (merged symbol), not a real function. "
                                "Use lookup_merged_symbol to see what real symbols share this address.")]

        # Extract parameter type hint for disambiguation (e.g., "Set(BaseSkeleton*)" → "Set", hint="BaseSkeleton*")
        symbol, param_hint = _extract_param_hint(symbol)

        # Determine which project directory to use
        # Priority: explicit project_dir arg > REPO_ROOT env var > main repo fallback
        # REPO_ROOT is set by agent_runner.py to the agent's worktree, ensuring
        # builds test the agent's edits even if project_dir is omitted.
        if project_dir_arg:
            project_dir = Path(project_dir_arg)
            if not project_dir.exists():
                return [TextContent(
                    type="text",
                    text=f"Error: project_dir does not exist: {project_dir}"
                )]
        elif os.environ.get("REPO_ROOT"):
            project_dir = Path(os.environ["REPO_ROOT"])
        else:
            project_dir = self.project_root

        # Find objdiff-cli in the determined project directory
        objdiff_cli = project_dir / "bin" / "objdiff-cli"

        if not objdiff_cli.exists():
            return [TextContent(
                type="text",
                text=f"Error: objdiff-cli not found at {objdiff_cli}"
            )]

        # Common args for both runs
        # Use functionRelocDiffs=none to ignore address relocation noise
        # (lis/addi pairs with different link-time addresses for same symbol).
        # This matches the behavior of objdiff's report command.
        base_args = [
            str(objdiff_cli),
            "diff",
            "-p", str(project_dir),
            symbol,
            "--verdict",
            "-c", "functionRelocDiffs=none",
        ]
        if unit:
            base_args.extend(["-u", unit])

        build_flag = ["--build"]
        if full_build:
            build_flag.append("--full-build")

        # --include-instructions only for JSON run (enrichment/m2c pipeline).
        # The markdown run uses --verdict alone which already contains the
        # analysis, patterns, and suggestions without the bulky instruction table.
        json_extra = ["--include-instructions"]
        if full_listing:
            json_extra.append("--full-listing")
        elif context:
            json_extra.extend(["-C", str(context)])

        try:
            # 1) JSON run (with build) - for enrichment data
            json_cmd = base_args + json_extra + build_flag + ["-f", "json"]
            json_result = subprocess.run(
                json_cmd,
                capture_output=True,
                text=True,
                timeout=300,
                cwd=str(project_dir),
            )

            # Check for errors in JSON output
            json_output = json_result.stdout
            stderr_text = _filter_build_output(json_result.stderr)

            # Check for errors - look in stderr first (build failures),
            # then in stdout but only if no JSON object was found
            has_json = "{" in json_output
            stdout_has_error = "Symbol not found" in json_output or (
                "Failed" in json_output and not has_json
            )
            stderr_has_error = "Failed" in (json_result.stderr or "")

            if stdout_has_error or (stderr_has_error and not has_json):
                # Try to resolve ambiguous symbol before giving up
                combined_output = json_output + "\n" + (json_result.stderr or "")
                if "Ambiguous symbol" in combined_output or "Multiple matches" in combined_output:
                    resolved = _resolve_ambiguous_symbol(combined_output, param_hint)
                    if resolved:
                        # Update base_args with the resolved symbol
                        base_args = [
                            str(objdiff_cli),
                            "diff",
                            "-p", str(project_dir),
                            resolved,
                            "--verdict",
                            "-c", "functionRelocDiffs=none",
                        ]
                        if unit:
                            base_args.extend(["-u", unit])

                        # Retry JSON run
                        json_cmd = base_args + json_extra + build_flag + ["-f", "json"]
                        json_result = subprocess.run(
                            json_cmd,
                            capture_output=True,
                            text=True,
                            timeout=300,
                            cwd=str(project_dir),
                        )
                        json_output = json_result.stdout
                        stderr_text = _filter_build_output(json_result.stderr)
                        has_json = "{" in json_output
                        # Update symbol for downstream use
                        symbol = resolved

                        # If retry still fails, fall through to error handling below
                        stdout_has_error = "Symbol not found" in json_output or (
                            "Failed" in json_output and not has_json
                        )
                        stderr_has_error = "Failed" in (json_result.stderr or "")
                        if not (stdout_has_error or (stderr_has_error and not has_json)):
                            # Success! Continue with normal flow
                            stdout_has_error = False

            if stdout_has_error or (stderr_has_error and not has_json):
                suggestions = self._suggest_similar_symbols(symbol)
                # Don't dump raw dtk output - filter it
                error_msg = _filter_build_output(json_output)
                if stderr_text:
                    error_msg += f"\n\n[stderr]\n{stderr_text}"
                if suggestions:
                    error_msg += "\n\nDid you mean:\n" + "\n".join(
                        f"  - {s}" for s in suggestions
                    )
                return [TextContent(type="text", text=error_msg.strip())]

            # Strip ninja build preamble (e.g. "ninja: no work to do.\n")
            # that --build writes to stdout before the JSON
            _json_start = json_output.find("{")
            if _json_start > 0:
                json_output = json_output[_json_start:]

            # 2) Markdown run (no build, already built) - for display
            # Explicit -f markdown avoids TUI fallback when no TTY is present
            md_cmd = list(base_args) + ["-f", "markdown"]
            if full_listing:
                md_cmd.append("--full-listing")
            elif concise:
                md_cmd.append("--concise")
            md_result = subprocess.run(
                md_cmd,
                capture_output=True,
                text=True,
                timeout=60,
                cwd=str(project_dir),
            )
            output = md_result.stdout

            # 3) Enrich from JSON and append enrichment sections
            enrichment = ""
            try:
                data = json.loads(json_output)
                data = self._enrich_objdiff_data(data)
                enrichment = self._format_enrichment_sections(data, skip_rb3=concise)

                # Fix match% in markdown header: use fuzzy_match_percent from JSON
                # (which respects functionRelocDiffs=none) to override the markdown
                # header which may not apply the config consistently.
                fuzzy_pct = data.get("fuzzy_match_percent")
                raw_pct = data.get("raw_match_percent")
                if fuzzy_pct is not None and raw_pct is not None:
                    output = re.sub(
                        r"Match: [\d.]+% normalized \([\d.]+% raw\)",
                        f"Match: {fuzzy_pct:.1f}% normalized ({raw_pct:.1f}% raw)",
                        output,
                        count=1,
                    )
            except (json.JSONDecodeError, KeyError):
                pass

            if enrichment:
                output += "\n" + enrichment

            # 3b) Stack-layout one-liner (only when actionable signal exists)
            try:
                data = json.loads(json_output)
                stack_line = _stack_signal_summary(data.get("instructions", []))
                if stack_line:
                    output += f"\n\n{stack_line}"
            except (json.JSONDecodeError, KeyError):
                pass

            # 4) Auto-diagnose when not concise and match < 95%
            if not concise:
                try:
                    parsed = json.loads(json_output)
                    match_pct = parsed.get("fuzzy_match_percent", 100)
                    if match_pct < 95:
                        # Write JSON to temp file for diff_inspect
                        tmp_json = Path(tempfile.mktemp(suffix=".json", dir="/tmp/claude"))
                        tmp_json.parent.mkdir(parents=True, exist_ok=True)
                        with open(tmp_json, "w") as f:
                            f.write(json_output)

                        diff_inspect_script = self.project_root / "scripts" / "analysis" / "diff_inspect.py"
                        if diff_inspect_script.exists():
                            diag_result = subprocess.run(
                                [sys.executable, str(diff_inspect_script), str(tmp_json), "--diagnose"],
                                capture_output=True, text=True,
                                timeout=30,
                            )
                            if diag_result.returncode == 0 and diag_result.stdout.strip():
                                output += "\n\n## Auto-Diagnosis (diff_inspect)\n\n" + diag_result.stdout.strip()

                        # Clean up
                        try:
                            tmp_json.unlink()
                        except OSError:
                            pass
                except (json.JSONDecodeError, KeyError, subprocess.TimeoutExpired, Exception):
                    pass  # Best-effort, never break run_objdiff

            if stderr_text:
                output += f"\n\n[stderr]\n{stderr_text}"

            # Count lines
            lines = output.split("\n")
            line_count = len(lines)

            if line_count < MAX_INLINE_LINES:
                return [TextContent(type="text", text=output)]
            else:
                # Write to file in the project directory being tested
                analysis_dir = project_dir / "function_analysis"
                analysis_dir.mkdir(exist_ok=True, parents=True)

                safe_symbol = symbol.replace("?", "_Q_").replace("@", "_A_").replace("<", "_L_").replace(">", "_R_")
                output_file = analysis_dir / f"objdiff_{safe_symbol}.md"

                with open(output_file, "w") as f:
                    f.write(output)

                # Extract summary from JSON for the inline preview
                summary = ""
                try:
                    data = json.loads(json_output)
                    match_pct = data.get("fuzzy_match_percent", "?")
                    verdict = data.get("verdict", {}).get("classification", "UNKNOWN")
                    summary = f"**Match: {match_pct}% | Verdict: {verdict}**\n\n"
                except (json.JSONDecodeError, KeyError):
                    pass

                return [TextContent(
                    type="text",
                    text=f"""{summary}Output is large ({line_count} lines). Written to file.

**File:** `{output_file.relative_to(project_dir)}`

**When reading this file:**
- Never read the entire file at once.
- First estimate size via bash (`wc -l` / `wc -c`).
- If > 500 lines or > 200KB, read in chunks of 200 lines.
- After each chunk, produce <= 8 bullets summarizing what you learned, then continue.
- Keep each tool result compact; do not emit large verbatim excerpts.

**Next steps:**
1. If verdict is AT_LIMIT: Report with mcp__orchestrator__report_result
2. If LIKELY_FIXABLE/MAYBE_FIXABLE: Make edits and run this tool again
"""
                )]

        except subprocess.TimeoutExpired:
            return [TextContent(type="text", text="Error: objdiff timed out after 5 minutes.")]
        except Exception as e:
            return [TextContent(type="text", text=f"Error running objdiff: {e}")]

    async def _run_analyze_function(self, args: dict) -> list[TextContent]:
        """
        Handle run_analyze_function tool call.

        Runs analyze_function.py which combines objdiff output with struct
        offset resolution from the header database.
        """
        symbol = args.get("symbol", "")
        resolve_offsets = args.get("resolve_offsets", True)
        output_format = args.get("output_format", "markdown")
        project_dir_arg = args.get("project_dir", None)

        if not symbol:
            return [TextContent(type="text", text="Error: No symbol provided.")]

        if symbol.startswith("merged_"):
            return [TextContent(type="text", text=f"Error: {symbol} is a linker ICF artifact (merged symbol), not a real function. "
                                "Use lookup_merged_symbol to see what real symbols share this address.")]

        # Extract parameter type hint for disambiguation
        symbol, _param_hint = _extract_param_hint(symbol)

        # Determine which project directory to use
        # Priority: explicit project_dir arg > REPO_ROOT env var > main repo fallback
        # REPO_ROOT is set by agent_runner.py to the agent's worktree, ensuring
        # builds test the agent's edits even if project_dir is omitted.
        if project_dir_arg:
            project_dir = Path(project_dir_arg)
            if not project_dir.exists():
                return [TextContent(
                    type="text",
                    text=f"Error: project_dir does not exist: {project_dir}"
                )]
        elif os.environ.get("REPO_ROOT"):
            project_dir = Path(os.environ["REPO_ROOT"])
        else:
            project_dir = self.project_root

        # Find analyze-function script in the determined project directory
        analyze_script = project_dir / "bin" / "analyze-function"

        if not analyze_script.exists():
            return [TextContent(
                type="text",
                text=f"Error: analyze-function not found at {analyze_script}"
            )]

        # Build command
        cmd = [str(analyze_script), symbol]

        if resolve_offsets:
            cmd.append("--resolve-offsets")

        if output_format == "json":
            cmd.extend(["-f", "json"])
        else:
            cmd.extend(["-f", "markdown"])

        try:
            result = subprocess.run(
                cmd,
                capture_output=True,
                text=True,
                timeout=300,  # 5 minute timeout
                cwd=str(project_dir),
            )

            output = result.stdout
            if result.stderr:
                filtered_stderr = _filter_build_output(result.stderr)
                if filtered_stderr:
                    output += f"\n\n[stderr]\n{filtered_stderr}"

            if result.returncode != 0:
                return [TextContent(
                    type="text",
                    text=f"Error (exit code {result.returncode}):\n{output}"
                )]

            # Count lines
            lines = output.split("\n")
            line_count = len(lines)

            if line_count < MAX_INLINE_LINES:
                # Return inline
                if output_format == "json":
                    return [TextContent(type="text", text=f"```json\n{output}\n```")]
                else:
                    return [TextContent(type="text", text=output)]
            else:
                # Write to file in the project directory being tested
                analysis_dir = project_dir / "function_analysis"
                analysis_dir.mkdir(exist_ok=True, parents=True)

                safe_symbol = symbol.replace("?", "_Q_").replace("@", "_A_").replace("<", "_L_").replace(">", "_R_")
                ext = "json" if output_format == "json" else "md"
                output_file = analysis_dir / f"analyze_{safe_symbol}.{ext}"

                with open(output_file, "w") as f:
                    f.write(output)

                return [TextContent(
                    type="text",
                    text=f"""Output is large ({line_count} lines). Written to file.

**File:** `{output_file.relative_to(project_dir)}`

Use the Read tool to view: `Read {output_file.relative_to(project_dir)}`
"""
                )]

        except subprocess.TimeoutExpired:
            return [TextContent(type="text", text="Error: analyze-function timed out after 5 minutes.")]
        except Exception as e:
            return [TextContent(type="text", text=f"Error running analyze-function: {e}")]

    async def _run_diff_inspect(self, args: dict) -> list[TextContent]:
        """
        Handle run_diff_inspect tool call.

        Runs diff_inspect.py analysis modes, save_baseline, or compare workflow.
        """
        symbol = args.get("symbol", "")
        mode = args.get("mode", "")
        project_dir_arg = args.get("project_dir", None)
        baseline_json = args.get("baseline_json", None)
        unit = args.get("unit", None)
        diff_mode = args.get("diff_mode", "normalized")  # "normalized" or "raw"

        if not symbol:
            return [TextContent(type="text", text="Error: No symbol provided.")]
        if not mode:
            return [TextContent(type="text", text="Error: No mode provided.")]

        valid_modes = {"diagnose", "clusters", "regswaps", "offsets", "replaces", "compare", "save_baseline", "mismatches", "asm_listing", "stack-layout"}
        if mode not in valid_modes:
            return [TextContent(type="text", text=f"Error: Invalid mode '{mode}'. Valid: {', '.join(sorted(valid_modes))}")]

        # Extract parameter type hint for disambiguation
        symbol, _param_hint = _extract_param_hint(symbol)

        # Require project_dir — no silent fallback to main repo
        if not project_dir_arg:
            return [TextContent(type="text", text="Error: project_dir is required. Pass your worktree directory so builds test your changes.")]
        project_dir = Path(project_dir_arg)
        if not project_dir.exists():
            return [TextContent(type="text", text=f"Error: project_dir does not exist: {project_dir}")]

        # Safe symbol for filenames
        safe_symbol = symbol.replace("?", "_Q_").replace("@", "_A_").replace("<", "_L_").replace(">", "_R_")

        # diff_inspect.py is always in the main repo
        diff_inspect_script = self.project_root / "scripts" / "analysis" / "diff_inspect.py"
        if not diff_inspect_script.exists():
            return [TextContent(type="text", text=f"Error: diff_inspect.py not found at {diff_inspect_script}")]

        # objdiff-cli is always in the main repo bin/
        objdiff_cli = self.project_root / "bin" / "objdiff-cli"

        # In "normalized" mode (default), ignore relocation address noise.
        # In "raw" mode, include relocation diffs so they can be inspected.
        reloc_config = ["-c", "functionRelocDiffs=none"] if diff_mode != "raw" else []

        try:
            # ── save_baseline mode ──
            if mode == "save_baseline":
                if not objdiff_cli.exists():
                    return [TextContent(type="text", text=f"Error: objdiff-cli not found at {objdiff_cli}")]

                # Run objdiff to produce JSON
                cmd = [
                    str(objdiff_cli), "diff",
                    "-p", str(project_dir),
                    symbol,
                    "--include-instructions", "--build", "--incremental",
                    *reloc_config,
                    "-f", "json",
                ]
                if unit:
                    cmd.extend(["-u", unit])
                result = subprocess.run(
                    cmd, capture_output=True, text=True,
                    timeout=300, cwd=str(project_dir),
                )
                if result.returncode != 0:
                    return [TextContent(type="text", text=f"Error running objdiff: {result.stderr or result.stdout}")]

                # Save to baseline path
                analysis_dir = project_dir / "function_analysis"
                analysis_dir.mkdir(exist_ok=True, parents=True)
                baseline_file = analysis_dir / f"baseline_{safe_symbol}.json"
                with open(baseline_file, "w") as f:
                    f.write(result.stdout)

                return [TextContent(type="text", text=f"Baseline saved: `{baseline_file}`")]

            # ── compare mode ──
            elif mode == "compare":
                # Find baseline
                if baseline_json:
                    baseline_path = Path(baseline_json)
                else:
                    baseline_path = project_dir / "function_analysis" / f"baseline_{safe_symbol}.json"

                if not baseline_path.exists():
                    return [TextContent(type="text", text=f"Error: No baseline found at `{baseline_path}`.\n"
                                        "Use `save_baseline` mode first, or pass `baseline_json` parameter.")]

                # Run fresh objdiff to get current JSON
                if not objdiff_cli.exists():
                    return [TextContent(type="text", text=f"Error: objdiff-cli not found at {objdiff_cli}")]

                cmd = [
                    str(objdiff_cli), "diff",
                    "-p", str(project_dir),
                    symbol,
                    "--include-instructions", "--build", "--incremental",
                    *reloc_config,
                    "-f", "json",
                ]
                if unit:
                    cmd.extend(["-u", unit])
                result = subprocess.run(
                    cmd, capture_output=True, text=True,
                    timeout=300, cwd=str(project_dir),
                )
                if result.returncode != 0:
                    return [TextContent(type="text", text=f"Error running objdiff: {result.stderr or result.stdout}")]

                # Write current JSON to temp file
                current_file = Path(tempfile.mktemp(suffix=".json", dir="/tmp/claude"))
                current_file.parent.mkdir(parents=True, exist_ok=True)
                with open(current_file, "w") as f:
                    f.write(result.stdout)

                # Run diff_inspect --compare
                compare_cmd = [
                    sys.executable, str(diff_inspect_script),
                    "--compare", str(baseline_path), str(current_file),
                ]
                compare_result = subprocess.run(
                    compare_cmd, capture_output=True, text=True,
                    timeout=60,
                )

                # Clean up temp file
                try:
                    current_file.unlink()
                except OSError:
                    pass

                output = compare_result.stdout
                if compare_result.stderr:
                    output += f"\n[stderr] {compare_result.stderr.strip()}"
                if compare_result.returncode != 0:
                    return [TextContent(type="text", text=f"Error in compare:\n{output}")]

                return [TextContent(type="text", text=output)]

            # ── mismatches mode (compact table of non-matching instructions) ──
            elif mode == "mismatches":
                if not objdiff_cli.exists():
                    return [TextContent(type="text", text=f"Error: objdiff-cli not found at {objdiff_cli}")]

                # Run objdiff to get JSON with instructions
                cmd = [
                    str(objdiff_cli), "diff",
                    "-p", str(project_dir),
                    symbol,
                    "--include-instructions", "--build", "--incremental",
                    *reloc_config,
                    "-f", "json",
                ]
                if unit:
                    cmd.extend(["-u", unit])
                result = subprocess.run(
                    cmd, capture_output=True, text=True,
                    timeout=300, cwd=str(project_dir),
                )

                stderr_text = result.stderr.strip() if result.stderr else ""
                if result.returncode != 0:
                    return [TextContent(type="text", text=f"Error running objdiff (exit {result.returncode}):\n{result.stdout}\n{stderr_text}")]

                stdout_text = result.stdout
                if "Symbol not found" in stdout_text or "Failed" in stdout_text:
                    error_msg = stdout_text.strip()
                    suggestions = self._suggest_similar_symbols(symbol)
                    if suggestions:
                        error_msg += "\n\nDid you mean:\n" + "\n".join(
                            f"  - {s}" for s in suggestions
                        )
                    return [TextContent(type="text", text=error_msg)]

                # Strip ninja build preamble (e.g. "ninja: no work to do.\n")
                # that --build writes to stdout before the JSON
                json_start = stdout_text.find("{")
                if json_start < 0:
                    return [TextContent(type="text", text=f"No JSON in objdiff output.\n\nstdout: {stdout_text[:500]}\nstderr: {stderr_text[:500]}")]

                try:
                    data = json.loads(stdout_text[json_start:])
                except json.JSONDecodeError as e:
                    return [TextContent(type="text", text=f"Error parsing objdiff JSON: {e}\n\nstdout: {stdout_text[:500]}\nstderr: {stderr_text[:500]}")]

                instrs = data.get("instructions", [])
                if not instrs:
                    return [TextContent(type="text", text="No instructions found in objdiff output.")]

                # Filter non-equal instructions
                mismatches = [ins for ins in instrs if ins.get("match_type") != "equal"]
                total = len(instrs)

                if not mismatches:
                    match_pct = data.get("fuzzy_match_percent", 100)
                    return [TextContent(type="text", text=f"No mismatches — all {total} instructions match ({match_pct}%).")]

                # Cap at 30
                MAX_MISMATCHES = 30
                truncated = len(mismatches) > MAX_MISMATCHES
                shown = mismatches[:MAX_MISMATCHES]

                # Format as compact markdown table
                from analysis.diff_inspect import fmt_instr as _fmt_instr, diff_annotation as _diff_annotation

                raw_pct = data.get("raw_match_percent", data.get("fuzzy_match_percent", "?"))
                header = f"## Mismatched Instructions ({len(mismatches)} of {total} total) — {raw_pct}% raw match\n"
                if diff_mode == "raw":
                    header += "*Raw mode: relocation diffs included*\n"
                if truncated:
                    header += f"*Showing {MAX_MISMATCHES} of {len(mismatches)} mismatches*\n"

                lines = [header]
                lines.append("| Idx | Type | Target | Base | Note |")
                lines.append("|-----|------|--------|------|------|")

                for ins in shown:
                    idx = ins.get("index", "?")
                    mt = ins.get("match_type", "?")
                    t = ins.get("target")
                    b = ins.get("base")
                    t_str = _fmt_instr(t).strip()
                    b_str = _fmt_instr(b).strip()
                    note = _diff_annotation(ins).strip() if mt == "diff_arg" else ""
                    # In raw mode, extract relocation symbol info for diff_arg
                    if diff_mode == "raw" and mt == "diff_arg" and not note:
                        t_syms = [a["value"] for a in (t or {}).get("typed_args", []) if a.get("type") == "Symbol"]
                        b_syms = [a["value"] for a in (b or {}).get("typed_args", []) if a.get("type") == "Symbol"]
                        if t_syms and b_syms:
                            if t_syms == b_syms:
                                # Same symbol, different address — pure addr_reloc
                                note = f"addr_reloc: `{t_syms[0][:40]}`"
                            else:
                                note = f"sym_diff: `{t_syms[0][:30]}` vs `{b_syms[0][:30]}`"
                        elif t_syms or b_syms:
                            note = f"reloc: T={'`'+t_syms[0][:30]+'`' if t_syms else 'none'} B={'`'+b_syms[0][:30]+'`' if b_syms else 'none'}"
                    lines.append(f"| {idx} | {mt} | `{t_str}` | `{b_str}` | {note} |")

                if truncated:
                    lines.append(f"\n*{len(mismatches) - MAX_MISMATCHES} more mismatches not shown.*")

                output = "\n".join(lines)
                return [TextContent(type="text", text=output)]

            # ── asm_listing mode (compile with /FAs, return annotated assembly) ──
            elif mode == "asm_listing":
                return await self._run_asm_listing(symbol, project_dir)

            # ── stack-layout mode (per-slot diff with base-side variable names) ──
            elif mode == "stack-layout":
                stack_script = self.project_root / "scripts" / "analysis" / "stack_layout.py"
                if not stack_script.exists():
                    return [TextContent(type="text", text=f"Error: stack_layout.py not found at {stack_script}")]

                cmd = [
                    sys.executable, str(stack_script),
                    "--symbol", symbol,
                    "--project-dir", str(project_dir),
                ]
                if unit:
                    cmd.extend(["--unit", unit])
                result = subprocess.run(
                    cmd, capture_output=True, text=True,
                    timeout=300,
                )

                output = result.stdout
                if result.stderr:
                    filtered_stderr = _filter_build_output(result.stderr)
                    if filtered_stderr:
                        output += f"\n\n[stderr]\n{filtered_stderr}"

                if result.returncode != 0:
                    return [TextContent(type="text", text=f"Error (exit {result.returncode}):\n{output}")]

                lines = output.split("\n")
                if len(lines) < MAX_INLINE_LINES:
                    return [TextContent(type="text", text=output)]
                else:
                    analysis_dir = project_dir / "function_analysis"
                    analysis_dir.mkdir(exist_ok=True, parents=True)
                    output_file = analysis_dir / f"stack_layout_{safe_symbol}.txt"
                    with open(output_file, "w") as f:
                        f.write(output)
                    return [TextContent(type="text", text=f"Output is large ({len(lines)} lines). Written to file.\n\n"
                                        f"**File:** `{output_file.relative_to(project_dir)}`")]

            # ── analysis modes (diagnose/clusters/regswaps/offsets/replaces) ──
            else:
                cmd = [
                    sys.executable, str(diff_inspect_script),
                    "--symbol", symbol,
                    f"--{mode}",
                    "--project-dir", str(project_dir),
                ]
                if unit:
                    cmd.extend(["--unit", unit])
                result = subprocess.run(
                    cmd, capture_output=True, text=True,
                    timeout=300,
                )

                output = result.stdout
                if result.stderr:
                    filtered_stderr = _filter_build_output(result.stderr)
                    if filtered_stderr:
                        output += f"\n\n[stderr]\n{filtered_stderr}"

                if result.returncode != 0:
                    return [TextContent(type="text", text=f"Error (exit {result.returncode}):\n{output}")]

                # Handle large output
                lines = output.split("\n")
                if len(lines) < MAX_INLINE_LINES:
                    return [TextContent(type="text", text=output)]
                else:
                    analysis_dir = project_dir / "function_analysis"
                    analysis_dir.mkdir(exist_ok=True, parents=True)
                    output_file = analysis_dir / f"diff_inspect_{mode}_{safe_symbol}.txt"
                    with open(output_file, "w") as f:
                        f.write(output)
                    return [TextContent(type="text", text=f"Output is large ({len(lines)} lines). Written to file.\n\n"
                                        f"**File:** `{output_file.relative_to(project_dir)}`")]

        except subprocess.TimeoutExpired:
            return [TextContent(type="text", text=f"Error: diff_inspect timed out (mode={mode}).")]
        except Exception as e:
            return [TextContent(type="text", text=f"Error running diff_inspect: {e}")]

    async def _run_asm_listing(self, symbol: str, project_dir: Path) -> list[TextContent]:
        """Compile with /FAs and return source-annotated assembly listing with var->register mapping."""
        import tempfile as _tempfile

        # 1. Find the obj target for this symbol from report.json
        report_path = project_dir / "build" / "45410914" / "report.json"
        if not report_path.exists():
            return [TextContent(type="text", text=f"Error: report.json not found at {report_path}. Run 'ninja' first.")]

        try:
            with open(report_path) as f:
                report = json.load(f)
        except (json.JSONDecodeError, OSError) as e:
            return [TextContent(type="text", text=f"Error reading report.json: {e}")]

        # Find which unit contains this symbol
        source_path = None
        for unit in report.get("units", []):
            for func in unit.get("functions", []):
                if func.get("name") == symbol:
                    source_path = unit.get("metadata", {}).get("source_path")
                    break
            if source_path:
                break

        if not source_path:
            return [TextContent(type="text", text=f"Error: Symbol '{symbol}' not found in report.json. Cannot determine source file.")]

        # 2. Derive the obj target from source path (src/foo/Bar.cpp -> build/45410914/default/foo/Bar.obj)
        obj_target = source_path.replace("src/", "build/45410914/default/").rsplit(".", 1)[0] + ".obj"

        # 3. Extract compile command from ninja
        ninja_result = subprocess.run(
            ["ninja", "-t", "commands", obj_target],
            capture_output=True, text=True,
            cwd=str(project_dir),
        )
        if ninja_result.returncode != 0:
            return [TextContent(type="text", text=f"Error getting compile command: {ninja_result.stderr}")]

        # Find the last "cd " line (actual compile command)
        cmd_line = None
        for line in ninja_result.stdout.strip().splitlines():
            if line.startswith("cd "):
                cmd_line = line

        if cmd_line is None:
            lines = ninja_result.stdout.strip().splitlines()
            cmd_line = lines[-1] if lines else ""

        # Parse cd + command
        if cmd_line.startswith("cd "):
            parts = cmd_line.split(" && ", 1)
            compile_cwd = parts[0][3:]
            compile_cmd = parts[1] if len(parts) > 1 else ""
        else:
            compile_cwd = str(project_dir)
            compile_cmd = cmd_line

        # 4. Add /FAs flag and redirect output to temp file
        asm_dir = Path(_tempfile.mkdtemp(dir="/tmp/claude-1000"))
        asm_output = asm_dir / "listing.asm"

        # Add /FAs and /Fa<path> to the compile command
        compile_cmd = compile_cmd + f" /FAs /Fa{asm_output}"

        # 5. Run the compile
        result = subprocess.run(
            compile_cmd, shell=True, cwd=compile_cwd,
            capture_output=True, text=True, timeout=120,
        )

        if not asm_output.exists():
            error = result.stderr or result.stdout or "Unknown error"
            return [TextContent(type="text", text=f"Error: /FAs compilation produced no output.\n{error[:1000]}")]

        # 6. Read asm listing and extract the function's block
        asm_text = asm_output.read_text(errors="replace")
        asm_lines = asm_text.splitlines()

        # Try to find the function using extract_function from asm_regmap's dependency
        try:
            from tools.compiler_trace.asm_diff import extract_function
            from tools.compiler_trace.asm_regmap import parse_asm_listing
        except ImportError:
            # Fall back to manual extraction
            extract_function = None
            parse_asm_listing = None

        func_lines = None
        if extract_function:
            func_lines = extract_function(asm_lines, symbol)

        if not func_lines:
            # Fallback: search for PROC/ENDP markers
            func_lines = _extract_function_fallback(asm_lines, symbol)

        if not func_lines:
            # Return the full listing if function not found (trimmed)
            total = len(asm_lines)
            if total > 500:
                output = "\n".join(asm_lines[:500])
                output += f"\n\n... ({total - 500} more lines, function not isolated)"
            else:
                output = asm_text
            return [TextContent(type="text", text=f"## ASM Listing (function not isolated)\n\n```asm\n{output}\n```")]

        # 7. Build output with optional var->register mapping
        output_parts = []
        output_parts.append(f"## ASM Listing: {symbol}")
        output_parts.append(f"({len(func_lines)} lines)")
        output_parts.append("")
        output_parts.append("```asm")
        output_parts.extend(func_lines)
        output_parts.append("```")

        # 8. Try to get var->register mapping
        if parse_asm_listing:
            regmap = parse_asm_listing(asm_lines, symbol)
            if regmap and regmap.var_to_reg:
                output_parts.append("")
                output_parts.append("## Variable -> Register Mapping")
                output_parts.append("")
                output_parts.append(f"Callee-saved GPRs used: {regmap.callee_saved_count}")
                output_parts.append("")
                output_parts.append("| Variable | Register |")
                output_parts.append("|----------|----------|")
                for var, reg in sorted(regmap.var_to_reg.items(), key=lambda x: x[1]):
                    output_parts.append(f"| {var} | {reg} |")

        # Clean up temp files
        try:
            asm_output.unlink()
            asm_dir.rmdir()
        except OSError:
            pass

        output = "\n".join(output_parts)
        return [TextContent(type="text", text=output)]

    async def _lookup_rb3wii(self, args: dict) -> list[TextContent]:
        """Handle lookup_rb3wii tool call — searches rb3-Wii dev decomp."""
        symbol = args.get("symbol", "")

        if not symbol:
            return [TextContent(type="text", text="No symbol provided.")]

        # Extract method name from mangled symbol
        # e.g., "?Poll@CharMirror@@UAEXXZ" → "Poll"
        search_term = symbol
        if "::" in symbol:
            parts = symbol.split("::")
            if parts:
                search_term = parts[-1].split("@")[0]
        elif "@" in symbol:
            # MSVC mangled: ?MethodName@ClassName@@...
            parts = symbol.split("@")
            if len(parts) > 1:
                search_term = parts[0].lstrip("?")

        rb3wii_path = Path(self.rb3wii_path)
        if not rb3wii_path.exists():
            return [TextContent(
                type="text",
                text=f"rb3-Wii source path not found: {rb3wii_path}",
            )]

        try:
            result = subprocess.run(
                ["grep", "-rn", "--include=*.cpp", "--include=*.h", search_term, str(rb3wii_path)],
                capture_output=True,
                text=True,
                timeout=30,
            )

            if not result.stdout.strip():
                return [TextContent(type="text", text=f"No matches found in rb3-Wii for: {search_term}")]

            # Limit output
            lines = result.stdout.strip().split("\n")[:20]
            output = f"rb3-Wii matches for '{search_term}' ({len(lines)} shown):\n\n"
            output += "\n".join(lines)

            if len(result.stdout.strip().split("\n")) > 20:
                output += f"\n\n... and more matches"

            return [TextContent(type="text", text=output)]

        except subprocess.TimeoutExpired:
            return [TextContent(type="text", text="rb3-Wii search timed out.")]
        except Exception as e:
            return [TextContent(type="text", text=f"rb3-Wii search error: {e}")]

    def _load_dc3_report(self) -> dict[str, float]:
        """Parse DC3's report.json into {unit_name: matched_functions_percent}.

        Cached on the instance and keyed by the report file's mtime; the JSON is
        re-parsed only when the file changes. Returns an empty dict if the
        report does not exist or cannot be parsed.
        """
        report_path = Path(self.dc3_report_path)
        try:
            mtime = report_path.stat().st_mtime
        except OSError:
            return {}

        if self._dc3_report_cache is not None and self._dc3_report_mtime == mtime:
            return self._dc3_report_cache

        try:
            with open(report_path) as f:
                report = json.load(f)
        except (OSError, json.JSONDecodeError):
            return {}

        cache: dict[str, float] = {}
        for unit in report.get("units", []):
            name = unit.get("name")
            if not name:
                continue
            measures = unit.get("measures", {}) or {}
            pct = measures.get("matched_functions_percent")
            if pct is not None:
                cache[name] = pct

        self._dc3_report_cache = cache
        self._dc3_report_mtime = mtime
        return cache

    def _dc3_file_to_unit(self, file_path: str) -> str | None:
        """Map an absolute DC3 source path to its DC3 unit name.

        Strips the DC3 src-root prefix and the file extension, prepends
        'default/'. For a '.h' file, maps to the unit of the sibling '.cpp'
        when that file exists on disk. Returns None when no unit can be
        derived (header with no sibling .cpp).
        """
        p = Path(file_path)
        if p.suffix == ".h":
            sibling = p.with_suffix(".cpp")
            if not sibling.exists():
                return None
            p = sibling

        try:
            rel = p.resolve().relative_to(Path(self.dc3_path).resolve())
        except ValueError:
            return None

        rel_no_ext = rel.with_suffix("")
        return f"default/{rel_no_ext.as_posix()}"

    async def _lookup_dc3(self, args: dict) -> list[TextContent]:
        """Search DC3 source for a method/class name (shared Milo engine, same MSVC X360 flags)."""
        symbol = args.get("symbol", "")
        if not symbol:
            return [TextContent(type="text", text="No symbol provided.")]
        min_match = args.get("min_match", 0)

        # Extract a searchable name from MSVC mangling or qualified C++ name.
        search_term = symbol
        if "::" in symbol:
            search_term = symbol.split("::")[-1]
        elif "@" in symbol:
            # MSVC mangled: ?MethodName@ClassName@@...
            parts = symbol.split("@")
            search_term = parts[0].lstrip("?") if parts else symbol

        dc3_path = Path(self.dc3_path)
        if not dc3_path.exists():
            return [TextContent(
                type="text",
                text=f"DC3 source path not found: {dc3_path}\n"
                     f"Pass --dc3 <path> to mcp_server, or set the source dir under "
                     f"~/code/milohax/dc3-decomp/src.",
            )]

        try:
            result = subprocess.run(
                ["grep", "-rn", "--include=*.cpp", "--include=*.h",
                 search_term, str(dc3_path)],
                capture_output=True, text=True, timeout=30,
            )
        except subprocess.TimeoutExpired:
            return [TextContent(type="text", text="DC3 search timed out.")]
        except Exception as e:
            return [TextContent(type="text", text=f"DC3 search error: {e}")]

        stdout = result.stdout.strip()
        if not stdout:
            return [TextContent(type="text", text=f"No matches found in DC3 for: {search_term}")]

        # Dedup grep line-hits to one row per source file, counting hits.
        file_hits: dict[str, int] = {}
        for line in stdout.split("\n"):
            file_part = line.split(":", 1)[0]
            if file_part:
                file_hits[file_part] = file_hits.get(file_part, 0) + 1

        if not file_hits:
            return [TextContent(type="text", text=f"No matches found in DC3 for: {search_term}")]

        # Score each file by its DC3 unit's matched_functions_percent.
        dc3_report = self._load_dc3_report()
        rows = []
        for file_path, hits in file_hits.items():
            unit = self._dc3_file_to_unit(file_path)
            if unit is None:
                rows.append({"file": file_path, "unit": None, "percent": None, "hits": hits})
            else:
                pct = dc3_report.get(unit)
                rows.append({"file": file_path, "unit": unit, "percent": pct, "hits": hits})

        # Apply min_match filter
        filtered_out = 0
        if min_match and min_match > 0:
            kept = []
            for r in rows:
                pct = r["percent"]
                if pct is not None and pct >= min_match:
                    kept.append(r)
                else:
                    filtered_out += 1
            rows = kept

        # Sort by unit matched_functions_percent descending; unscored last.
        rows.sort(key=lambda r: (r["percent"] is None, -(r["percent"] or 0.0)))

        total_files = len(file_hits)
        shown = rows[:20]

        if not shown:
            msg = f"No DC3 files for '{search_term}' meet the criteria."
            if filtered_out:
                msg += f"\n{filtered_out} hits filtered out below {min_match}%."
            return [TextContent(type="text", text=msg)]

        output = (
            f"DC3 matches for '{search_term}' "
            f"({len(shown)} of {total_files} files, ranked by DC3 unit match%):\n\n"
        )
        for r in shown:
            if r["unit"] is None:
                unit_str = "(header, no unit)"
                pct_str = "n/a"
            else:
                unit_str = r["unit"]
                pct_str = f"{r['percent']:.1f}%" if r["percent"] is not None else "unknown"
            output += f"- {r['file']}\n"
            output += f"  Unit: {unit_str} | Match: {pct_str} | Hits: {r['hits']}\n"

        if filtered_out:
            output += f"\n{filtered_out} hits filtered out below {min_match}%."
        if total_files > 20:
            output += f"\n\n... and {total_files - 20} more files"

        return [TextContent(type="text", text=output)]

    async def _lookup_struct_offset(self, args: dict) -> list[TextContent]:
        """Handle lookup_struct_offset tool call.

        Enhanced with:
        - Range-based lookup: finds member containing the offset (not just exact match)
        - ObjPtr/ObjOwnerPtr/ObjPtrVec sub-offset reporting
        - RB2 DWARF fallback when struct_db doesn't have the class
        """
        class_name = args.get("class_name", "")
        offset_str = args.get("offset", "")

        if not class_name or not offset_str:
            return [TextContent(type="text", text="Error: class_name and offset are required.")]

        # Parse offset (hex or decimal)
        try:
            if offset_str.startswith("0x"):
                offset = int(offset_str, 16)
            else:
                offset = int(offset_str)
        except ValueError:
            return [TextContent(type="text", text=f"Error: Invalid offset format: {offset_str}")]

        # Use struct database from project root
        struct_db_path = self.project_root / "struct_db.sqlite"
        if not struct_db_path.exists():
            return [TextContent(
                type="text",
                text=f"Struct database not found at {struct_db_path}.\n\nBuild it with: ./tools/struct_db.py build src/"
            )]

        try:
            with StructDB(str(struct_db_path)) as db:
                result = db.lookup(class_name, offset)

                if result:
                    cls_name, member_name, type_str = result
                    text = f"**{cls_name}::{member_name}** (`{type_str}`) at offset 0x{offset:x}"
                    # Add wrapper sub-offset info
                    sub_info = self._describe_wrapper_suboffset(type_str, 0)
                    if sub_info:
                        text += f"\n{sub_info}"
                    return [TextContent(type="text", text=text)]

                # Exact match failed — try range-based lookup
                range_result = self._lookup_offset_in_range(db, class_name, offset)
                if range_result:
                    return [TextContent(type="text", text=range_result)]

            # Fall back to RB2 DWARF data
            rb2_result = self._lookup_rb2_offset(class_name, offset)
            if rb2_result:
                return [TextContent(type="text", text=rb2_result)]

            return [TextContent(
                type="text",
                text=f"No field found at offset 0x{offset:x} in {class_name} or its parent classes."
            )]
        except Exception as e:
            return [TextContent(type="text", text=f"Error looking up offset: {e}")]

    def _lookup_offset_in_range(self, db: "StructDB", class_name: str, offset: int) -> str | None:
        """Find which member contains the given offset using type size knowledge."""
        from tools.struct_db import TYPE_SIZES, TEMPLATE_SIZES

        cursor = db.conn.cursor()
        classes_to_check = [class_name] + db.resolve_inheritance_chain(class_name)

        for check_class in classes_to_check:
            # Get all members for this class, ordered by offset descending
            cursor.execute("""
                SELECT c.name, m.name, m.type_str, m.offset
                FROM members m
                JOIN classes c ON m.class_id = c.id
                WHERE c.name = ? AND m.offset <= ?
                ORDER BY m.offset DESC
                LIMIT 1
            """, (check_class, offset))

            row = cursor.fetchone()
            if not row:
                continue

            cls_name, member_name, type_str, member_offset = row[0], row[1], row[2], row[3]
            sub_offset = offset - member_offset

            # Estimate member size from type
            member_size = self._estimate_type_size(type_str)

            if member_size and sub_offset < member_size:
                text = f"**{cls_name}::{member_name}** (`{type_str}`) at base offset 0x{member_offset:x}"
                text += f"\n  Queried offset 0x{offset:x} is at +0x{sub_offset:x} within this member"
                # Describe wrapper sub-offset if applicable
                sub_info = self._describe_wrapper_suboffset(type_str, sub_offset)
                if sub_info:
                    text += f"\n  {sub_info}"
                return text

        return None

    @staticmethod
    def _estimate_type_size(type_str: str) -> int | None:
        """Estimate type size from type string using known sizes."""
        from tools.struct_db import TYPE_SIZES, TEMPLATE_SIZES

        # Direct lookup
        if type_str in TYPE_SIZES:
            return TYPE_SIZES[type_str]

        # Template types: "ObjPtr<RndMat>" → ObjPtr
        template_match = re.match(r'(\w+)<', type_str)
        if template_match:
            template_name = template_match.group(1)
            if template_name in TEMPLATE_SIZES:
                return TEMPLATE_SIZES[template_name]

        # Pointer types
        if type_str.endswith('*'):
            return 4  # 32-bit pointers on PPC

        return None

    @staticmethod
    def _describe_wrapper_suboffset(type_str: str, sub_offset: int) -> str | None:
        """Describe sub-offsets within ObjPtr/ObjOwnerPtr/ObjPtrVec wrapper types."""
        # ObjPtr<T> / ObjOwnerPtr<T> layout (0x14 bytes):
        #   0x00: vtable (from ObjRef)
        #   0x04: mRef (Hmx::Object*, from ObjRef)
        #   0x08: mOwner (from ObjRefConcrete)  -- or mObject? depends on version
        #   0x0C: mObject (the T* raw pointer)
        #   0x10: mOwner (Hmx::Object* or ObjRefOwner*)
        objptr_fields = {
            0x00: "vtable (ObjRef base)",
            0x04: "mRef (ObjRef::mRef)",
            0x08: "mOwner (ObjRefConcrete field)",
            0x0C: "mObject (the raw T* pointer — this is what Ghidra dereferences)",
            0x10: "mOwner (owner pointer)",
        }

        # ObjPtrVec<T1, T2> layout (0x1C bytes, inherits ObjRefOwner):
        #   0x00: vtable
        #   0x04: ObjRefOwner fields
        #   0x0C: mNodes (vector<Node>) — begin pointer
        #   0x10: mNodes — end pointer
        #   0x14: mNodes — capacity pointer
        #   0x18: mOwner
        objptrvec_fields = {
            0x00: "vtable (ObjRefOwner base)",
            0x04: "ObjRefOwner field",
            0x08: "ObjRefOwner field",
            0x0C: "mNodes.begin (vector data ptr)",
            0x10: "mNodes.end (vector size ptr)",
            0x14: "mNodes.capacity",
            0x18: "mOwner",
        }

        template_match = re.match(r'(\w+)<', type_str)
        if not template_match:
            return None

        template_name = template_match.group(1)

        if template_name in ('ObjPtr', 'ObjOwnerPtr'):
            desc = objptr_fields.get(sub_offset)
            if desc:
                return f"→ ObjPtr sub-offset +0x{sub_offset:x}: {desc}"
            return f"→ ObjPtr sub-offset +0x{sub_offset:x} (within {template_name}, size 0x14)"

        if template_name == 'ObjPtrVec':
            desc = objptrvec_fields.get(sub_offset)
            if desc:
                return f"→ ObjPtrVec sub-offset +0x{sub_offset:x}: {desc}"
            return f"→ ObjPtrVec sub-offset +0x{sub_offset:x} (within ObjPtrVec, size 0x1C)"

        if template_name == 'ObjDirPtr':
            fields = {0x00: "vtable", 0x04: "mRef", 0x08: "mLoader", 0x0C: "mDir (the raw ObjectDir* pointer)"}
            desc = fields.get(sub_offset)
            if desc:
                return f"→ ObjDirPtr sub-offset +0x{sub_offset:x}: {desc}"

        if template_name in ('ObjPtrList', 'ObjList'):
            fields = {0x00: "vtable", 0x04: "list head/size", 0x08: "list node ptr"}
            desc = fields.get(sub_offset)
            if desc:
                return f"→ {template_name} sub-offset +0x{sub_offset:x}: {desc}"

        return None

    def _lookup_rb2_offset(self, class_name: str, offset: int) -> str | None:
        """Fall back to RB2 DWARF data for offset lookup."""
        try:
            from orchestrator.rb2_dwarf import RB2DwarfParser
            parser = RB2DwarfParser()
            result = parser.get_member_at_offset(class_name, offset)
            if result:
                cls = result.get('class', class_name)
                member = result['member']
                type_str = result['type']
                member_offset = result['offset']
                text = f"**(RB2 DWARF)** {cls}::{member} (`{type_str}`) at offset 0x{member_offset:x}"
                sub_off = result.get('sub_offset')
                if sub_off is not None and sub_off > 0:
                    text += f"\n  Queried offset 0x{offset:x} is at +0x{sub_off:x} within this member"
                    sub_info = self._describe_wrapper_suboffset(type_str, sub_off)
                    if sub_info:
                        text += f"\n  {sub_info}"
                return text
        except Exception:
            pass
        return None

    async def _lookup_merged_symbol(self, args: dict) -> list[TextContent]:
        """Handle lookup_merged_symbol tool call."""
        address = args.get("address", "")

        if not address:
            return [TextContent(type="text", text="Error: address is required.")]

        if MergedSymbolLookup is None:
            return [TextContent(
                type="text",
                text="MergedSymbolLookup not available (tools/merged_symbols.py not present in rb3-xenon). "
                     "RB3 has no leaked linker map, so merged-symbol resolution is not supported.",
            )]

        # Find linker map file (RB3 has no leaked map — this will always fail gracefully)
        map_file = self.project_root / "orig" / "45410914" / "ham_xbox_r.map"
        if not map_file.exists():
            return [TextContent(
                type="text",
                text=f"Linker map file not found at {map_file}\n"
                     f"Note: RB3 (45410914) has no leaked linker map, unlike DC3."
            )]

        try:
            lookup = MergedSymbolLookup(map_file)
            symbols = lookup.lookup(address)

            if symbols is None:
                return [TextContent(
                    type="text",
                    text=f"No symbols found at address: {address}"
                )]

            # Normalize address for display
            addr_display = address.upper()
            if addr_display.lower().startswith('merged_'):
                addr_display = addr_display[7:]
            addr_display = addr_display.lstrip('0x')

            # Format output
            if len(symbols) == 1:
                output = f"**Address 0x{addr_display}**: 1 symbol (not merged)\n\n"
            else:
                output = f"**Address 0x{addr_display}**: {len(symbols)} symbols merged by ICF\n\n"

            for i, sym in enumerate(symbols, 1):
                mangled = sym['symbol']
                demangled = lookup.demangle(mangled)
                source = sym.get('source', '')

                src_suffix = f" ({source})" if source else ""
                output += f"{i}. **{demangled}**{src_suffix}\n"
                output += f"   - Mangled: `{mangled}`\n"

            # Add interpretation guidance for common patterns
            if len(symbols) > 1:
                output += "\n---\n"
                output += "**ICF Interpretation:**\n"
                # Check for destructor pattern
                has_scalar = any('??_G' in s['symbol'] for s in symbols)
                has_vector = any('??_E' in s['symbol'] for s in symbols)
                if has_scalar and has_vector:
                    output += "- Contains both scalar (`delete obj`) and vector (`delete[] arr`) deleting destructors\n"
                    output += "- These have identical code, so any call to either resolves to this address\n"
                # Check for template pattern
                template_count = sum(1 for s in symbols if '$' in s['symbol'])
                if template_count > 2:
                    output += f"- Contains {template_count} template instantiations with identical code\n"
                    output += "- The compiler generated the same machine code for different types\n"

            return [TextContent(type="text", text=output)]

        except Exception as e:
            return [TextContent(type="text", text=f"Error looking up merged symbol: {e}")]

    async def _mark_patch_result(self, args: dict) -> list[TextContent]:
        """Handle mark_patch_result tool call."""
        queue_id = args.get("patch_queue_id")
        status = args.get("status", "")
        reason = args.get("reason", "")

        if queue_id is None:
            return [TextContent(type="text", text="Error: patch_queue_id is required.")]
        if not status:
            return [TextContent(type="text", text="Error: status is required.")]

        try:
            from .merger_agent import mark_patch_result
            result = mark_patch_result(
                queue_id=int(queue_id),
                status=status,
                reason=reason,
                db_path=self.db_path,
            )
            return [TextContent(type="text", text=json.dumps(result))]
        except Exception as e:
            return [TextContent(type="text", text=f"Error marking patch result: {e}")]

    async def run(self):
        """Run the MCP server."""
        async with stdio_server() as (read_stream, write_stream):
            await self.server.run(read_stream, write_stream, self.server.create_initialization_options())


def main():
    parser = argparse.ArgumentParser(description="RB3-Xenon Decomp MCP Server")
    parser.add_argument("--db", default="decomp.db", help="Database path")
    parser.add_argument("--rb3wii", default=None, help="rb3-Wii source path (default: ~/code/milohax/rb3/src)")
    parser.add_argument("--dc3", default=None, help="DC3 source path (default: ~/code/milohax/dc3-decomp/src)")
    parser.add_argument(
        "--no-record-attempts",
        action="store_true",
        help="Don't record attempts in report_result (orchestrator records them after agent returns)",
    )
    args = parser.parse_args()

    server = DecompMCPServer(
        db_path=args.db,
        rb3wii_path=args.rb3wii,
        dc3_path=args.dc3,
        record_attempts=not args.no_record_attempts,
    )
    asyncio.run(server.run())


if __name__ == "__main__":
    main()
