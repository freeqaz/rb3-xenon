#!/usr/bin/env python3
"""Combined objdiff + unicorn diagnostic tool.

Shows objdiff analysis alongside unicorn behavioral comparison results,
producing actionable SKIP/FIX recommendations for each function.
"""

import argparse
import json
import os
import subprocess
import sys

from .coff import COFFParser
from .comparator import classify_divergence, format_result
from .run import (resolve_unit, list_functions, run_comparison_inner,
                  run_dual_comparison_inner, _run_comparison_core,
                  EXIT_EQUIVALENT, EXIT_DIVERGENT, EXIT_SKIPPED, EXIT_ERROR)
from .memory_map import FILL_BYTE


PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
OBJDIFF_CLI = os.path.join(PROJECT_ROOT, "bin", "objdiff-cli")


def get_objdiff_verdict(symbol):
    """Get objdiff verdict data for a symbol. Returns dict or None."""
    try:
        result = subprocess.run(
            [OBJDIFF_CLI, "diff", "-p", PROJECT_ROOT, symbol, "--verdict", "-f", "json"],
            capture_output=True, text=True, timeout=30,
        )
        if result.returncode != 0:
            return None
        return json.loads(result.stdout)
    except (subprocess.TimeoutExpired, json.JSONDecodeError, FileNotFoundError):
        return None


def format_unicorn_summary(exit_code, output_text):
    """Extract a one-line summary from unicorn results."""
    if exit_code == EXIT_EQUIVALENT:
        return "EQUIVALENT"
    if exit_code == EXIT_SKIPPED:
        return "SKIPPED"
    # Parse DIVERGENT output for key info
    for line in output_text.split("\n"):
        line = line.strip()
        if line.startswith("First mismatch:"):
            return "DIVERGENT — " + line
        if line.startswith("Call count mismatch:"):
            return "DIVERGENT — " + line
        if line.startswith("Return value mismatch"):
            return "DIVERGENT — return value differs"
        if line.startswith("Float return value mismatch"):
            return "DIVERGENT — float return differs"
        if line.startswith("Memory mismatch:"):
            return "DIVERGENT — " + line
        if "execution error:" in line:
            return "DIVERGENT — " + line
    return "DIVERGENT"


def diagnose_single(symbol, decomp_coff, orig_coff, verbose=False,
                    coload=True, coload_depth=None,
                    dual_fixture=True, fill_pattern=None):
    """Run combined diagnosis for a single function.

    Returns dict with diagnosis data, or None if symbol not found.
    """
    # Run 1: primary comparison (zero fill or custom fill)
    primary_fill = fill_pattern if not dual_fixture else None
    exit_code, bundle, verbose_lines, error_msg = _run_comparison_core(
        symbol, decomp_coff, orig_coff, timeout=5_000_000,
        coload=coload, coload_depth=coload_depth,
        fill_pattern=primary_fill)

    if exit_code == EXIT_SKIPPED:
        return None
    if bundle is None:
        return None

    # Dual-fixture confidence scoring
    confidence = None
    if dual_fixture and exit_code not in (EXIT_ERROR, EXIT_SKIPPED):
        code_cd, _, _, _ = _run_comparison_core(
            symbol, decomp_coff, orig_coff, timeout=5_000_000,
            coload=coload, coload_depth=coload_depth,
            fill_pattern=FILL_BYTE)
        if exit_code == code_cd:
            confidence = "high"
        else:
            confidence = "fixture_sensitive"

    # Classify divergence
    div_class = None
    if bundle.result.verdict == "DIVERGENT":
        div_class = classify_divergence(
            bundle.result, bundle.decomp_result, bundle.orig_result,
            bundle.decomp_relocs, bundle.orig_relocs)

    # Format output text for summaries
    output_text = format_result(
        bundle.result, bundle.decomp_result, bundle.orig_result,
        bundle.decomp_relocs, bundle.orig_relocs, verbose=verbose)

    # Get objdiff verdict
    objdiff = get_objdiff_verdict(symbol)

    # Extract fields
    match_pct = objdiff.get("fuzzy_match_percent", 0) if objdiff else 0
    demangled = objdiff.get("demangled", symbol) if objdiff else symbol
    classification = "UNKNOWN"
    if objdiff and objdiff.get("verdict"):
        classification = objdiff["verdict"].get("classification", "UNKNOWN")

    unicorn_verdict = "EQUIVALENT" if exit_code == EXIT_EQUIVALENT else "DIVERGENT"
    unicorn_summary = format_unicorn_summary(exit_code, output_text)

    # Determine recommendation with confidence and classification
    if match_pct >= 100.0:
        recommendation = "DONE"
    elif unicorn_verdict == "EQUIVALENT":
        if confidence == "fixture_sensitive":
            recommendation = "SKIP(?)"
        elif confidence == "high":
            recommendation = "SKIP(high)"
        else:
            recommendation = "SKIP"
    else:
        # Annotate FIX with divergence classification
        if div_class and div_class != "logic":
            recommendation = f"FIX({div_class})"
        else:
            recommendation = "FIX"

    return {
        "symbol": symbol,
        "demangled": demangled,
        "match_pct": match_pct,
        "objdiff_class": classification,
        "unicorn_verdict": unicorn_verdict,
        "unicorn_summary": unicorn_summary,
        "unicorn_detail": output_text if verbose else None,
        "recommendation": recommendation,
        "confidence": confidence,
        "divergence_class": div_class,
    }


def print_single(diag, verbose=False):
    """Print diagnosis for a single function."""
    print(f"=== {diag['demangled']} ===")
    print(f"  objdiff: {diag['match_pct']:.1f}% match, {diag['objdiff_class']}")
    print(f"  unicorn: {diag['unicorn_summary']}")
    if diag.get("divergence_class"):
        print(f"  divergence: {diag['divergence_class']}")
    print(f"  Recommendation: {diag['recommendation']}")
    if verbose and diag.get("unicorn_detail"):
        print()
        for line in diag["unicorn_detail"].split("\n"):
            print(f"    {line}")


def print_batch(results, unit_name):
    """Print batch diagnosis with summary."""
    # Sort: FIX (logic) first, then FIX(classified), then SKIP(?), then SKIP/SKIP(high), then DONE
    def sort_key(d):
        rec = d["recommendation"]
        if rec == "FIX":
            return (0, -d["match_pct"])
        if rec.startswith("FIX("):
            return (1, -d["match_pct"])
        order = {"SKIP(?)": 2, "SKIP": 3, "SKIP(high)": 4, "DONE": 5}
        return (order.get(rec, 6), -d["match_pct"])
    results.sort(key=sort_key)

    print(f"=== {unit_name} ({len(results)} functions) ===")

    skip_high = 0
    skip_sensitive = 0
    skip_plain = 0
    fix_logic = 0
    fix_build_env = 0
    fix_regalloc = 0
    done_count = 0
    objdiff_flagged = 0

    for d in results:
        rec = d["recommendation"]
        pct = d["match_pct"]
        # Truncate demangled name for readability
        name = d["demangled"]
        if len(name) > 50:
            name = name[:47] + "..."

        objdiff_label = d["objdiff_class"]
        unicorn_label = d["unicorn_verdict"]

        # Add brief divergence info for FIX items
        suffix = ""
        if rec.startswith("FIX") and d["unicorn_summary"].startswith("DIVERGENT — "):
            suffix = "  (" + d["unicorn_summary"][len("DIVERGENT — "):] + ")"

        print(f"  {rec:<15s}  {pct:5.1f}%  {name:<50s}  objdiff={objdiff_label:<20s}  unicorn={unicorn_label}{suffix}")

        if rec == "SKIP(high)":
            skip_high += 1
        elif rec == "SKIP(?)":
            skip_sensitive += 1
        elif rec == "SKIP":
            skip_plain += 1
        elif rec == "FIX":
            fix_logic += 1
        elif rec == "FIX(build_env)":
            fix_build_env += 1
        elif rec == "FIX(regalloc)":
            fix_regalloc += 1
        elif rec == "DONE":
            done_count += 1

        if objdiff_label in ("LIKELY_FIXABLE", "MAYBE_FIXABLE", "NEEDS_INVESTIGATION"):
            objdiff_flagged += 1

    total_skip = skip_high + skip_sensitive + skip_plain
    total_fix = fix_logic + fix_build_env + fix_regalloc
    print()

    # Build FIX breakdown
    fix_parts = []
    if fix_logic > 0:
        fix_parts.append(f"{fix_logic} logic")
    if fix_build_env > 0:
        fix_parts.append(f"{fix_build_env} build_env")
    if fix_regalloc > 0:
        fix_parts.append(f"{fix_regalloc} regalloc")

    if skip_high > 0 or skip_sensitive > 0:
        skip_detail = f" [{skip_high} high, {skip_sensitive} sensitive, {skip_plain} basic]"
    else:
        skip_detail = " (behaviorally equivalent)"

    fix_detail = f" [{', '.join(fix_parts)}]" if len(fix_parts) > 1 else ""
    print(f"  Summary: {done_count} DONE, {total_skip} SKIP{skip_detail}, {total_fix} FIX{fix_detail}")

    if total_skip > 0 and objdiff_flagged > 0:
        skip_recs = ("SKIP", "SKIP(high)", "SKIP(?)")
        false_positives = sum(1 for d in results if d["recommendation"] in skip_recs
                             and d["objdiff_class"] in ("LIKELY_FIXABLE", "MAYBE_FIXABLE", "NEEDS_INVESTIGATION"))
        if false_positives > 0:
            print(f"  Without unicorn: objdiff flagged {objdiff_flagged} as needing work → {false_positives} are actually equivalent (time saved)")
    if fix_build_env > 0 or fix_regalloc > 0:
        unfixable = fix_build_env + fix_regalloc
        print(f"  Unfixable divergences: {unfixable} ({fix_build_env} build_env, {fix_regalloc} regalloc)")


def main():
    parser = argparse.ArgumentParser(
        description="Combined objdiff + unicorn diagnostic — shows SKIP/FIX recommendations")
    parser.add_argument("--unit", required=True, help="Unit name (resolves paths from objdiff.json)")
    parser.add_argument("--symbol", help="Specific mangled symbol to diagnose")
    parser.add_argument("--batch", action="store_true", help="Diagnose all eligible functions in the unit")
    parser.add_argument("--verbose", "-v", action="store_true", help="Show detailed unicorn output")
    parser.add_argument("--no-coload", action="store_true",
                       help="Disable intra-TU callee co-loading")
    parser.add_argument("--coload-depth", type=int, default=None,
                       help="Limit callee co-loading recursion depth")
    parser.add_argument("--dual-fixture", action="store_true", default=True,
                       help="Run dual-fixture confidence scoring (default: on)")
    parser.add_argument("--no-dual-fixture", action="store_true",
                       help="Disable dual-fixture, use single zero-fill run")
    parser.add_argument("--fill-pattern", type=lambda x: int(x, 0), default=None,
                       help="Fill memory with byte pattern (only with --no-dual-fixture)")

    args = parser.parse_args()

    coload = not args.no_coload
    coload_depth = args.coload_depth
    dual_fixture = not args.no_dual_fixture
    fill_pattern = args.fill_pattern

    if not args.symbol and not args.batch:
        parser.error("Must provide either --symbol or --batch")

    # Resolve unit paths
    try:
        decomp_path, orig_path = resolve_unit(args.unit)
    except ValueError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        return 2

    if not os.path.exists(decomp_path):
        print(f"ERROR: Decomp .obj not found: {decomp_path}", file=sys.stderr)
        return 2
    if not os.path.exists(orig_path):
        print(f"ERROR: Original .obj not found: {orig_path}", file=sys.stderr)
        return 2

    # Parse COFF files once
    decomp_coff = COFFParser(decomp_path)
    orig_coff = COFFParser(orig_path)

    if args.symbol:
        diag = diagnose_single(args.symbol, decomp_coff, orig_coff, verbose=args.verbose,
                               coload=coload, coload_depth=coload_depth,
                               dual_fixture=dual_fixture, fill_pattern=fill_pattern)
        if diag is None:
            print(f"SKIPPED: Symbol not found or empty", file=sys.stderr)
            return 3
        print_single(diag, verbose=args.verbose)
        return 0 if diag["recommendation"] != "FIX" else 1

    # Batch mode
    import io
    old_stdout = sys.stdout
    sys.stdout = io.StringIO()
    try:
        eligible = list_functions(decomp_path, orig_path,
                                  decomp_coff=decomp_coff, orig_coff=orig_coff)
    finally:
        sys.stdout = old_stdout

    results = []
    for sym_name, d_size, o_size in eligible:
        diag = diagnose_single(sym_name, decomp_coff, orig_coff, verbose=args.verbose,
                               coload=coload, coload_depth=coload_depth,
                               dual_fixture=dual_fixture, fill_pattern=fill_pattern)
        if diag is not None:
            results.append(diag)

    print_batch(results, args.unit)

    fix_count = sum(1 for d in results if d["recommendation"] == "FIX")
    return 1 if fix_count > 0 else 0


if __name__ == "__main__":
    sys.exit(main())
