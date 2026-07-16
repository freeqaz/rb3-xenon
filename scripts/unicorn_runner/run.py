#!/usr/bin/env python3
"""Unicorn Function Runner — CLI entry point.

Compares function behavior between decomp and original .obj files
by executing both in Unicorn PPC32 BE and comparing observable output.
"""

import argparse
import json
import os
import sys
from concurrent.futures import ProcessPoolExecutor

from dataclasses import dataclass

from .builder import prepare_side, prepare_coloaded_side
from .coff import COFFParser
from .coloader import collect_intra_tu_callees, build_coload_layout, resolve_section_rel24_targets
from .extractor import (
    extract_from_decomp, extract_from_original,
    classify_indirect_branch,
)
from .memory_map import CODE_BASE, FILL_BYTE
from .engine import execute_function, UnicornEngine
from .comparator import compare, format_result, format_json_result, classify_divergence


@dataclass
class ComparisonBundle:
    """Raw comparison data for downstream analysis (classification, probing)."""
    result: object           # ComparisonResult
    decomp_result: object    # ExecutionResult
    orig_result: object      # ExecutionResult
    decomp_relocs: list
    orig_relocs: list

# Exit codes
EXIT_EQUIVALENT = 0
EXIT_DIVERGENT = 1
EXIT_ERROR = 2
EXIT_SKIPPED = 3


def resolve_unit(unit_name, project_root=None):
    """Resolve unit name to (decomp_obj_path, orig_obj_path) via objdiff.json.

    Returns (base_path, target_path) — i.e. (decomp, original).
    """
    if project_root is None:
        project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    objdiff_path = os.path.join(project_root, "objdiff.json")
    with open(objdiff_path) as f:
        config = json.load(f)

    # Find unit where name ends with the given unit_name
    for entry in config.get("units", []):
        name = entry.get("name", "")
        if name.endswith("/" + unit_name) or name == unit_name:
            target_path = entry.get("target_path")
            base_path = entry.get("base_path")
            if not target_path:
                raise ValueError(f"Unit '{unit_name}' has no target_path")
            if not base_path:
                raise ValueError(f"Unit '{unit_name}' has no base_path (original .obj)")
            return (
                os.path.join(project_root, base_path),
                os.path.join(project_root, target_path),
            )

    raise ValueError(f"Unit '{unit_name}' not found in objdiff.json")


def list_functions(decomp_path, orig_path, decomp_coff=None, orig_coff=None):
    """List eligible functions in both .obj files."""
    if decomp_coff is None:
        decomp_coff = COFFParser(decomp_path)
    if orig_coff is None:
        orig_coff = COFFParser(orig_path)

    # Build sets of function symbols from each side
    decomp_syms = set()
    for sym in decomp_coff.symbols:
        if sym['section'] > 0:
            sec = decomp_coff.sections[sym['section'] - 1]
            if sec['name'].startswith('.text'):
                decomp_syms.add(sym['name'])

    orig_syms = set()
    for sym in orig_coff.symbols:
        if sym['section'] > 0:
            sec = orig_coff.sections[sym['section'] - 1]
            if sec['name'].startswith('.text'):
                orig_syms.add(sym['name'])

    # Find symbols present in both
    common = sorted(decomp_syms & orig_syms)

    eligible = []
    skipped = []
    for sym_name in common:
        # Extract from both sides
        d_bytes, d_relocs = extract_from_decomp(decomp_coff, sym_name)
        o_bytes, o_relocs = extract_from_original(orig_coff, sym_name)

        if d_bytes is None or o_bytes is None or len(d_bytes) == 0 or len(o_bytes) == 0:
            continue

        # Classify indirect branches
        d_class = classify_indirect_branch(d_bytes, d_relocs, decomp_coff)
        o_class = classify_indirect_branch(o_bytes, o_relocs, orig_coff)

        eligible.append((sym_name, len(d_bytes), len(o_bytes)))

    print(f"Eligible functions ({len(eligible)}):")
    for name, d_size, o_size in eligible:
        size_match = "=" if d_size == o_size else "!"
        print(f"  {name}  (decomp={d_size}B, orig={o_size}B) {size_match}")

    if skipped:
        print(f"\nSkipped ({len(skipped)}, indirect branches):")
        for name, reason, d_size, o_size in skipped:
            print(f"  {name}  ({reason}, decomp={d_size}B, orig={o_size}B)")

    return eligible


def _run_comparison_core(symbol, decomp_coff, orig_coff, timeout=5_000_000,
                         coload=True, coload_depth=None,
                         fill_pattern=None, engine=None, object_memory=None,
                         arg_registers=None, max_insns=None):
    """Core comparison logic returning raw structured data.

    Returns (exit_code, ComparisonBundle_or_None, verbose_lines, error_message).
    On success, bundle contains the ComparisonResult and ExecutionResults.
    On skip/error, bundle is None and error_message explains why.
    arg_registers: optional dict mapping register IDs to values for both executions.
    max_insns: override instruction count limit (default: engine default of 50_000).
    """
    # 1. Extract function bytes and relocations
    decomp_bytes, decomp_relocs = extract_from_decomp(decomp_coff, symbol)
    orig_bytes, orig_relocs = extract_from_original(orig_coff, symbol)

    if decomp_bytes is None:
        return EXIT_SKIPPED, None, [], f"SKIPPED: Symbol '{symbol}' not found in decomp .obj"
    if orig_bytes is None:
        return EXIT_SKIPPED, None, [], f"SKIPPED: Symbol '{symbol}' not found in original .obj"
    if len(decomp_bytes) == 0 or len(orig_bytes) == 0:
        return EXIT_SKIPPED, None, [], f"SKIPPED: Symbol '{symbol}' has zero size"

    # Phase 2.3: refuse to compare stub-vs-real implementations. A 4x size
    # ratio is the threshold — well outside legitimate codegen variation
    # (typically <1.2x) but inside enough to catch a `return;` stub against
    # a real 200-line method. See docs/sessions/2026-02-19-unicorn-
    # reteleport-callcount-anomaly.md for the bug that motivates this.
    smaller = min(len(decomp_bytes), len(orig_bytes))
    larger = max(len(decomp_bytes), len(orig_bytes))
    if larger >= 64 and smaller / larger < 0.25:
        return EXIT_SKIPPED, None, [], (
            f"SKIPPED: size mismatch — decomp={len(decomp_bytes)}B "
            f"orig={len(orig_bytes)}B (ratio {smaller/larger:.2f})"
        )

    # Resolve section-symbol REL24 targets (e.g. .text -> actual function name)
    # Critical for original .obj files where all functions share one .text section
    decomp_relocs = resolve_section_rel24_targets(
        decomp_coff, symbol, decomp_bytes, decomp_relocs)
    orig_relocs = resolve_section_rel24_targets(
        orig_coff, symbol, orig_bytes, orig_relocs)

    verbose_lines = [
        f"Symbol: {symbol}",
        f"  Decomp: {len(decomp_bytes)} bytes, {len(decomp_relocs)} relocs",
        f"  Original: {len(orig_bytes)} bytes, {len(orig_relocs)} relocs",
    ]

    # 2. Classify indirect branches
    d_class = classify_indirect_branch(decomp_bytes, decomp_relocs, decomp_coff)
    o_class = classify_indirect_branch(orig_bytes, orig_relocs, orig_coff)

    # 3. Co-load discovery
    layout = None
    coloaded_count = 0

    if coload:
        d_callees = collect_intra_tu_callees(
            decomp_coff, symbol, extract_from_decomp, max_depth=coload_depth)
        o_callees = collect_intra_tu_callees(
            orig_coff, symbol, extract_from_original, max_depth=coload_depth)

        common = set(d_callees.keys()) & set(o_callees.keys())
        if common:
            layout = build_coload_layout(
                symbol, decomp_bytes, common, d_callees, o_callees,
                decomp_coff, orig_coff)

    # 4. Prepare both sides
    if layout:
        coloaded_count = len(layout.coloaded_symbols)
        intra_tu_addrs = {sym: CODE_BASE + off
                          for sym, off in layout.symbol_offsets.items()}

        verbose_lines.append(f"  Co-loaded callees: {coloaded_count} ({layout.total_size}B combined)")
        for csym in layout.coloaded_symbols:
            verbose_lines.append(f"    {csym} @ offset 0x{layout.symbol_offsets[csym]:X}")

        try:
            decomp_side = prepare_coloaded_side(
                decomp_bytes, decomp_relocs, decomp_coff, symbol, d_class,
                d_callees, layout, intra_tu_addrs)
            orig_side = prepare_coloaded_side(
                orig_bytes, orig_relocs, orig_coff, symbol, o_class,
                o_callees, layout, intra_tu_addrs)
        except Exception as e:
            return EXIT_ERROR, None, [], f"ERROR: Co-load patching failed: {e}"
    else:
        try:
            decomp_side = prepare_side(
                decomp_bytes, decomp_relocs, decomp_coff, symbol, d_class)
            orig_side = prepare_side(
                orig_bytes, orig_relocs, orig_coff, symbol, o_class)
        except Exception as e:
            return EXIT_ERROR, None, [], f"ERROR: Patching failed: {e}"

    verbose_lines.append(f"  Decomp trampolines: {len(decomp_side.trampolines)}")
    verbose_lines.append(f"  Original trampolines: {len(orig_side.trampolines)}")

    # 5. Execute both sides
    _exec = engine.execute if engine else execute_function
    exec_kwargs = {}
    if max_insns is not None:
        exec_kwargs['max_insns'] = max_insns
    try:
        decomp_result = _exec(
            decomp_side.code, decomp_side.trampolines, decomp_side.func_size,
            timeout=timeout, verbose=False, rdata_bytes=decomp_side.rdata_bytes,
            fill_pattern=fill_pattern, object_memory=object_memory,
            arg_registers=arg_registers, **exec_kwargs)
        orig_result = _exec(
            orig_side.code, orig_side.trampolines, orig_side.func_size,
            timeout=timeout, verbose=False, rdata_bytes=orig_side.rdata_bytes,
            fill_pattern=fill_pattern, object_memory=object_memory,
            arg_registers=arg_registers, **exec_kwargs)
    except Exception as e:
        return EXIT_ERROR, None, [], f"ERROR: Execution failed: {e}"

    # 6. Compare
    result = compare(decomp_result, orig_result, decomp_relocs, orig_relocs)

    if coloaded_count > 0:
        verbose_lines.append(f"  Co-loaded: {coloaded_count} callees, {decomp_side.func_size}B combined code")

    bundle = ComparisonBundle(
        result=result,
        decomp_result=decomp_result,
        orig_result=orig_result,
        decomp_relocs=decomp_relocs,
        orig_relocs=orig_relocs,
    )
    exit_code = EXIT_EQUIVALENT if result.verdict == "EQUIVALENT" else EXIT_DIVERGENT
    return exit_code, bundle, verbose_lines, None


def run_comparison_inner(symbol, decomp_coff, orig_coff, verbose=False, timeout=5_000_000,
                         json_output=False, coload=True, coload_depth=None,
                         fill_pattern=None, engine=None, object_memory=None):
    """Comparison with formatted output, operating on pre-parsed COFF instances.

    Returns (exit_code, output_text) without printing anything.
    If json_output=True, output_text is a JSON string.
    If engine is provided, uses it for execution (avoids Uc() init/teardown).
    """
    exit_code, bundle, verbose_lines, error_msg = _run_comparison_core(
        symbol, decomp_coff, orig_coff, timeout=timeout,
        coload=coload, coload_depth=coload_depth,
        fill_pattern=fill_pattern, engine=engine, object_memory=object_memory)

    if bundle is None:
        return exit_code, error_msg

    result = bundle.result
    decomp_result = bundle.decomp_result
    orig_result = bundle.orig_result
    decomp_relocs = bundle.decomp_relocs
    orig_relocs = bundle.orig_relocs

    # Format output
    if json_output:
        decomp_bytes, _ = extract_from_decomp(decomp_coff, symbol)
        orig_bytes, _ = extract_from_original(orig_coff, symbol)
        metadata = {
            "symbol": symbol,
            "decomp_size": len(decomp_bytes),
            "orig_size": len(orig_bytes),
            "coloaded_callees": 0,  # approximate
            "combined_code_size": 0,
        }
        output = format_json_result(
            result, decomp_result, orig_result, orig_relocs, metadata)
        return exit_code, output

    output = format_result(
        result, decomp_result, orig_result,
        decomp_relocs, orig_relocs, verbose=verbose)
    if verbose:
        output = "\n".join(verbose_lines) + "\n" + output

    return exit_code, output


def run_dual_comparison_inner(symbol, decomp_coff, orig_coff, verbose=False,
                               timeout=5_000_000, json_output=False,
                               coload=True, coload_depth=None, engine=None):
    """Run comparison twice (zero + 0xCD fill), combine verdicts with confidence."""
    # Run 1: zero fill (baseline)
    code_zero, output_zero = run_comparison_inner(
        symbol, decomp_coff, orig_coff, verbose=verbose, timeout=timeout,
        json_output=json_output, coload=coload, coload_depth=coload_depth,
        fill_pattern=None, engine=engine)

    # Skip second run for non-comparable results
    if code_zero in (EXIT_ERROR, EXIT_SKIPPED):
        return code_zero, output_zero

    # Run 2: 0xCD fill
    code_cd, _ = run_comparison_inner(
        symbol, decomp_coff, orig_coff, verbose=False, timeout=timeout,
        json_output=False, coload=coload, coload_depth=coload_depth,
        fill_pattern=FILL_BYTE, engine=engine)

    # Combine: both agree → high confidence; disagree → fixture_sensitive
    if code_zero == code_cd:
        confidence = "high"
    else:
        confidence = "fixture_sensitive"

    # Annotate the zero-fill output (always the primary result)
    if json_output:
        data = json.loads(output_zero)
        data["confidence"] = confidence
        data["fixture_mode"] = "dual"
        return code_zero, json.dumps(data)
    else:
        tag = f"[confidence={confidence}] "
        return code_zero, tag + output_zero


def run_comparison(symbol, decomp_path, orig_path, verbose=False, timeout=5_000_000,
                   decomp_coff=None, orig_coff=None, json_output=False,
                   coload=True, coload_depth=None,
                   fill_pattern=None, dual_fixture=False,
                   field_access=True):
    """Run the full comparison pipeline for a single function.

    Returns exit code. Accepts optional pre-parsed COFF instances.
    If field_access=True (default), appends struct field access map to output.
    """
    # Parse COFF files if not provided
    if decomp_coff is None or orig_coff is None:
        try:
            if decomp_coff is None:
                decomp_coff = COFFParser(decomp_path)
            if orig_coff is None:
                orig_coff = COFFParser(orig_path)
        except Exception as e:
            if json_output:
                print(json.dumps({"error": str(e)}))
            else:
                print(f"ERROR: Failed to parse .obj files: {e}", file=sys.stderr)
            return EXIT_ERROR

    if dual_fixture:
        code, output = run_dual_comparison_inner(
            symbol, decomp_coff, orig_coff, verbose=verbose, timeout=timeout,
            json_output=json_output, coload=coload, coload_depth=coload_depth)
    else:
        code, output = run_comparison_inner(
            symbol, decomp_coff, orig_coff, verbose=verbose, timeout=timeout,
            json_output=json_output, coload=coload, coload_depth=coload_depth,
            fill_pattern=fill_pattern)
    if code == EXIT_SKIPPED:
        if json_output:
            print(json.dumps({"verdict": "SKIPPED", "message": output}))
        else:
            print(output, file=sys.stderr)
    else:
        print(output)

    # Append field access map for single-function mode
    if field_access and code not in (EXIT_SKIPPED, EXIT_ERROR):
        try:
            from .prober import probe_field_access, format_field_access_map
            from .typed_fixture import extract_class_from_symbol
            cls_name = extract_class_from_symbol(symbol)
            sdb = None
            try:
                from tools.struct_db import StructDB
                db_path = os.path.join(
                    os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))),
                    "struct_db.sqlite")
                if os.path.exists(db_path):
                    sdb = StructDB(db_path)
                    sdb.connect()
            except ImportError:
                pass
            accesses = probe_field_access(
                symbol, decomp_coff, orig_coff,
                timeout=timeout, coload=coload, coload_depth=coload_depth,
                struct_db=sdb, class_name=cls_name)
            if sdb is not None:
                sdb.close()
            if accesses:
                print(f"\n{format_field_access_map(accesses, symbol)}")
        except Exception:
            pass  # field-access is best-effort, don't fail the comparison

    return code


def _find_common_text_symbols(decomp_coff, orig_coff):
    """Find symbol names present in .text sections of both COFFs.

    Lightweight alternative to list_functions() — no extraction or classification.
    """
    decomp_syms = set()
    for sym in decomp_coff.symbols:
        if sym['section'] > 0:
            sec = decomp_coff.sections[sym['section'] - 1]
            if sec['name'].startswith('.text'):
                decomp_syms.add(sym['name'])

    orig_syms = set()
    for sym in orig_coff.symbols:
        if sym['section'] > 0:
            sec = orig_coff.sections[sym['section'] - 1]
            if sec['name'].startswith('.text'):
                orig_syms.add(sym['name'])

    return sorted(decomp_syms & orig_syms)


def run_batch(decomp_path, orig_path, verbose=False, timeout=5_000_000, quiet=False,
              coload=True, coload_depth=None, fill_pattern=None, dual_fixture=False,
              cache=None, typed=False, unit_name=None, emit_file=None):
    """Run comparison for all eligible functions in a unit.

    Parses COFF files once and reuses a single Unicorn engine for all functions.
    If quiet=True, suppresses per-function output (for multiprocessing).
    If typed=True, generates type-aware object memory from struct_db.
    If unit_name is provided, uses it to extract the primary class for typed fixtures.
    If emit_file is a file object, writes one JSON line per function result.

    Returns (equivalent, divergent, errors, skipped, cached_count) counts.
    """
    import random
    from .typed_fixture import extract_class_from_symbol, extract_class_from_unit, generate_typed_object

    decomp_coff = COFFParser(decomp_path)
    orig_coff = COFFParser(orig_path)

    # Find common symbols directly (avoids redundant extraction in list_functions)
    common = _find_common_text_symbols(decomp_coff, orig_coff)

    # Load struct_db if typed mode requested
    db = None
    if typed:
        try:
            from tools.struct_db import StructDB
            project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
            db_path = os.path.join(project_root, "struct_db.sqlite")
            if os.path.exists(db_path):
                db = StructDB(db_path)
                db.connect()
        except ImportError:
            pass

    # Generate typed memory once for the unit's primary class
    rng = random.Random(42)
    unit_typed_mem_zero = None
    unit_typed_mem_cd = None
    if db is not None:
        unit_class = extract_class_from_unit(unit_name) if unit_name else None
        if unit_class:
            unit_typed_mem_zero = generate_typed_object(
                unit_class, db, rng, fill_byte=0x00)
            unit_typed_mem_cd = generate_typed_object(
                unit_class, db, rng, fill_byte=0xCD)

    equivalent = 0
    divergent = 0
    errors = 0
    skipped = 0
    cached_count = 0

    for sym_name in common:
        div_class = None  # divergence classification

        # Check cache first
        if cache is not None:
            cached = cache.lookup(sym_name, decomp_path, orig_path)
            if cached is not None:
                code = cached[0]
                cached_count += 1
                if code == EXIT_EQUIVALENT:
                    equivalent += 1
                elif code == EXIT_DIVERGENT:
                    divergent += 1
                elif code == EXIT_SKIPPED:
                    skipped += 1
                else:
                    errors += 1
                if not quiet:
                    status = {EXIT_EQUIVALENT: "EQUIVALENT", EXIT_DIVERGENT: "DIVERGENT",
                              EXIT_SKIPPED: "SKIPPED"}.get(code, "ERROR")
                    print(f"  {status:11s}  {sym_name}  (cached)")
                # Emit cached results (no classification available from cache)
                if emit_file is not None:
                    verdict_str = {EXIT_EQUIVALENT: "EQUIVALENT", EXIT_DIVERGENT: "DIVERGENT",
                                   EXIT_SKIPPED: "SKIPPED"}.get(code, "ERROR")
                    emit_file.write(json.dumps({
                        "symbol": sym_name, "verdict": verdict_str,
                        "class": None, "confidence": cached[1],
                    }) + "\n")
                continue

        if dual_fixture:
            code, _output = run_dual_comparison_inner(
                sym_name, decomp_coff, orig_coff, verbose=False, timeout=timeout,
                coload=coload, coload_depth=coload_depth)
            # Extract confidence from output
            confidence = None
            if _output.startswith("[confidence="):
                tag_end = _output.index("] ")
                confidence = _output[len("[confidence="):tag_end]
        else:
            # Standard comparison (with typed memory if available)
            code, _output = run_comparison_inner(
                sym_name, decomp_coff, orig_coff, verbose=False, timeout=timeout,
                coload=coload, coload_depth=coload_depth, fill_pattern=fill_pattern,
                object_memory=unit_typed_mem_zero)
            confidence = None

            # If divergent and we have typed memory, retry with CD fill + typed
            if code == EXIT_DIVERGENT and unit_typed_mem_cd is not None:
                code2, _output2 = run_comparison_inner(
                    sym_name, decomp_coff, orig_coff, verbose=False, timeout=timeout,
                    coload=coload, coload_depth=coload_depth, fill_pattern=0xCD,
                    object_memory=unit_typed_mem_cd)
                if code2 == EXIT_EQUIVALENT:
                    code = code2
                    _output = _output2

        # Classify divergent results
        if code == EXIT_DIVERGENT:
            exit_code_d, bundle_d, _, _ = _run_comparison_core(
                sym_name, decomp_coff, orig_coff, timeout=timeout,
                coload=coload, coload_depth=coload_depth,
                fill_pattern=fill_pattern, object_memory=unit_typed_mem_zero)
            if bundle_d is not None:
                div_class = classify_divergence(
                    bundle_d.result, bundle_d.decomp_result, bundle_d.orig_result,
                    bundle_d.decomp_relocs, bundle_d.orig_relocs)

        # Store in cache
        if cache is not None and code not in (EXIT_ERROR, EXIT_SKIPPED):
            cache.store(sym_name, decomp_path, orig_path, code, confidence)

        if code == EXIT_EQUIVALENT:
            equivalent += 1
            status = "EQUIVALENT"
        elif code == EXIT_DIVERGENT:
            divergent += 1
            status = "DIVERGENT"
        elif code == EXIT_SKIPPED:
            skipped += 1
            status = "SKIPPED"
        else:
            errors += 1
            status = "ERROR"

        if not quiet:
            print(f"  {status:11s}  {sym_name}")

        # Emit per-function result
        if emit_file is not None:
            emit_file.write(json.dumps({
                "symbol": sym_name, "verdict": status,
                "class": div_class, "confidence": confidence,
            }) + "\n")

    if db is not None:
        db.close()

    return equivalent, divergent, errors, skipped, cached_count


def get_all_units(project_root=None):
    """Get all units from objdiff.json that have both target_path and base_path."""
    if project_root is None:
        project_root = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

    objdiff_path = os.path.join(project_root, "objdiff.json")
    with open(objdiff_path) as f:
        config = json.load(f)

    units = []
    for entry in config.get("units", []):
        target_path = entry.get("target_path")
        base_path = entry.get("base_path")
        if target_path and base_path:
            units.append((
                entry.get("name", ""),
                os.path.join(project_root, base_path),
                os.path.join(project_root, target_path),
            ))

    return units


def _process_unit(args):
    """Worker function for multiprocessing batch-all.

    Must be top-level (not a closure) so it can be pickled.
    Returns (name, equivalent, divergent, errors, skipped, cached, tested).

    For multiprocessing safety, each worker loads a read-only cache snapshot.
    New results are NOT saved by workers — the main process handles saving
    via a follow-up single-threaded pass (or the next run picks them up).
    """
    name, decomp_path, orig_path, timeout, coload, coload_depth, fill_pattern, dual_fixture, cache_path, typed, emit_path = args
    if not os.path.exists(decomp_path) or not os.path.exists(orig_path):
        return (name, 0, 0, 0, 0, 0, False)
    # Each worker loads a read-only cache snapshot for lookups
    cache = None
    if cache_path:
        from .cache import ResultCache
        cache = ResultCache(cache_path)
    emit_file = None
    if emit_path:
        emit_file = open(emit_path, "a")
    try:
        eq, div, err, sk, cached = run_batch(decomp_path, orig_path,
                                              timeout=timeout, quiet=True,
                                              coload=coload, coload_depth=coload_depth,
                                              fill_pattern=fill_pattern,
                                              dual_fixture=dual_fixture,
                                              cache=cache, typed=typed,
                                              unit_name=name,
                                              emit_file=emit_file)
        # Don't save from workers — race condition with other workers.
        # Cache still provides lookup hits from previous runs.
        return (name, eq, div, err, sk, cached, True)
    except Exception as e:
        return (name, 0, 0, 1, 0, 0, True)
    finally:
        if emit_file is not None:
            emit_file.close()


def main():
    parser = argparse.ArgumentParser(
        description="Unicorn Function Runner — compare decomp vs original function behavior")
    parser.add_argument("--symbol", help="Mangled C++ symbol name")
    parser.add_argument("--decomp-obj", help="Path to decomp .obj file")
    parser.add_argument("--orig-obj", help="Path to original .obj file")
    parser.add_argument("--unit", help="Unit name (resolves paths from objdiff.json)")
    parser.add_argument("--verbose", "-v", action="store_true", help="Print detailed execution trace")
    parser.add_argument("--list-functions", action="store_true", help="List eligible functions in the unit")
    parser.add_argument("--batch", action="store_true",
                       help="Run comparison for all eligible functions in the unit")
    parser.add_argument("--batch-all", action="store_true",
                       help="Run batch comparison across all units in objdiff.json")
    parser.add_argument("--timeout", type=int, default=5_000_000,
                       help="Execution timeout in microseconds (default: 5000000)")
    parser.add_argument("--jobs", "-j", type=int, default=os.cpu_count() or 8,
                       help="Number of parallel workers for --batch-all (default: cpu_count)")
    parser.add_argument("--json", action="store_true",
                       help="Output structured JSON instead of human-readable text")
    parser.add_argument("--no-coload", action="store_true",
                       help="Disable intra-TU callee co-loading")
    parser.add_argument("--coload-depth", type=int, default=None,
                       help="Limit callee co-loading recursion depth (default: unlimited)")
    parser.add_argument("--fill-pattern", type=lambda x: int(x, 0), default=None,
                       help="Fill memory with byte pattern instead of zeros (e.g., 0xCD)")
    parser.add_argument("--dual-fixture", action="store_true",
                       help="Run twice (zero + 0xCD fill) for confidence scoring")
    parser.add_argument("--no-cache", action="store_true",
                       help="Disable result caching for batch modes")
    parser.add_argument("--typed", action="store_true",
                       help="Use type-aware object memory from struct_db")
    parser.add_argument("--emit-results", type=str, default=None,
                       help="Write per-function JSON lines to this path (batch modes)")
    parser.add_argument("--field-access", action="store_true",
                       help="Probe struct field access patterns only (no comparison)")
    parser.add_argument("--no-field-access", action="store_true",
                       help="Disable field access map in single-function output")

    args = parser.parse_args()

    coload = not args.no_coload
    coload_depth = args.coload_depth

    # Batch-all mode: iterate all units
    if args.batch_all:
        # Set up cache
        use_cache = not args.no_cache
        cache_path = None
        if use_cache:
            from .cache import ResultCache, DEFAULT_CACHE_PATH
            cache_path = DEFAULT_CACHE_PATH

        emit_path = args.emit_results

        units = get_all_units()
        work = [(name, dp, op, args.timeout, coload, coload_depth,
                 args.fill_pattern, args.dual_fixture,
                 cache_path if use_cache else None, args.typed,
                 emit_path) for name, dp, op in units]

        cache_label = "enabled" if use_cache else "disabled"
        print(f"Batch-all: {len(units)} units with both target and base paths")
        print(f"Workers: {args.jobs}, Cache: {cache_label}\n")

        total_equiv = 0
        total_div = 0
        total_err = 0
        total_skip = 0
        total_cached = 0
        units_tested = 0
        done = 0

        def _handle_result(result):
            nonlocal total_equiv, total_div, total_err, total_skip, total_cached, units_tested, done
            name, eq, div, err, sk, cached, tested = result
            done += 1
            if not tested:
                return
            total = eq + div + err + sk
            if total == 0:
                return
            units_tested += 1
            total_equiv += eq
            total_div += div
            total_err += err
            total_skip += sk
            total_cached += cached
            cache_note = f" ({cached} cached)" if cached > 0 else ""
            print(f"  [{done}/{len(work)}] {name}: {eq}eq {div}div {err}err {sk}sk{cache_note}",
                  flush=True)

        if args.jobs == 1:
            # Single-threaded: use a shared cache that saves at the end
            main_cache = None
            if use_cache:
                main_cache = ResultCache(cache_path)
            emit_fh = open(emit_path, "w") if emit_path else None
            for name, dp, op, *rest in work:
                if not os.path.exists(dp) or not os.path.exists(op):
                    _handle_result((name, 0, 0, 0, 0, 0, False))
                    continue
                try:
                    eq, div, err, sk, cached = run_batch(
                        dp, op, timeout=args.timeout, quiet=True,
                        coload=coload, coload_depth=coload_depth,
                        fill_pattern=args.fill_pattern,
                        dual_fixture=args.dual_fixture,
                        cache=main_cache, typed=args.typed,
                        unit_name=name, emit_file=emit_fh)
                    _handle_result((name, eq, div, err, sk, cached, True))
                except Exception:
                    _handle_result((name, 0, 0, 1, 0, 0, True))
            if main_cache is not None:
                main_cache.save()
            if emit_fh is not None:
                emit_fh.close()
        else:
            with ProcessPoolExecutor(max_workers=args.jobs) as pool:
                for result in pool.map(_process_unit, work):
                    _handle_result(result)

        total_funcs = total_equiv + total_div + total_err + total_skip
        total_fresh = total_funcs - total_cached
        print(f"\n=== BATCH-ALL SUMMARY ===")
        print(f"Units tested: {units_tested}")
        print(f"Functions: {total_funcs} total ({total_fresh} fresh, {total_cached} cached)")
        print(f"  Equivalent: {total_equiv}")
        print(f"  Divergent:  {total_div}")
        print(f"  Errors:     {total_err}")
        print(f"  Skipped:    {total_skip}")

        if total_div > 0:
            return EXIT_DIVERGENT
        if total_err > 0:
            return EXIT_ERROR
        return EXIT_EQUIVALENT

    # Resolve paths
    if args.unit:
        try:
            decomp_path, orig_path = resolve_unit(args.unit)
        except ValueError as e:
            print(f"ERROR: {e}", file=sys.stderr)
            return EXIT_ERROR
    elif args.decomp_obj and args.orig_obj:
        decomp_path = args.decomp_obj
        orig_path = args.orig_obj
    else:
        parser.error("Must provide either --unit or both --decomp-obj and --orig-obj")
        return EXIT_ERROR

    # Check files exist
    if not os.path.exists(decomp_path):
        print(f"ERROR: Decomp .obj not found: {decomp_path}", file=sys.stderr)
        return EXIT_ERROR
    if not os.path.exists(orig_path):
        print(f"ERROR: Original .obj not found: {orig_path}", file=sys.stderr)
        return EXIT_ERROR

    # List functions mode
    if args.list_functions:
        list_functions(decomp_path, orig_path)
        return EXIT_EQUIVALENT

    # Batch mode: all eligible functions in the unit
    if args.batch:
        cache = None
        if not args.no_cache:
            from .cache import ResultCache
            cache = ResultCache()
        emit_fh = open(args.emit_results, "w") if args.emit_results else None
        print(f"Batch: {decomp_path}")
        eq, div, err, sk, cached = run_batch(
            decomp_path, orig_path,
            verbose=args.verbose, timeout=args.timeout,
            coload=coload, coload_depth=coload_depth,
            fill_pattern=args.fill_pattern, dual_fixture=args.dual_fixture,
            cache=cache, typed=args.typed,
            unit_name=args.unit, emit_file=emit_fh)
        if cache is not None:
            cache.save()
        if emit_fh is not None:
            emit_fh.close()
        total = eq + div + err + sk
        fresh = total - cached
        cache_note = f", {fresh} fresh, {cached} cached" if cached > 0 else ""
        print(f"\nSummary: {eq} equivalent, {div} divergent, {err} errors, {sk} skipped ({total} total{cache_note})")
        if div > 0:
            return EXIT_DIVERGENT
        if err > 0:
            return EXIT_ERROR
        return EXIT_EQUIVALENT

    # Field access probing mode
    if args.field_access:
        if not args.symbol:
            parser.error("--symbol is required for --field-access mode")
            return EXIT_ERROR
        from .coff import COFFParser as _COFF
        from .prober import probe_field_access, format_field_access_map
        from .typed_fixture import extract_class_from_symbol
        decomp_coff = _COFF(decomp_path)
        orig_coff = _COFF(orig_path)
        # Try to load struct_db for field name resolution
        sdb = None
        cls_name = extract_class_from_symbol(args.symbol)
        try:
            from tools.struct_db import StructDB
            db_path = os.path.join(
                os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                "struct_db.sqlite")
            if os.path.exists(db_path):
                sdb = StructDB(db_path)
                sdb.connect()
        except ImportError:
            pass
        accesses = probe_field_access(
            args.symbol, decomp_coff, orig_coff,
            timeout=args.timeout, coload=coload, coload_depth=coload_depth,
            struct_db=sdb, class_name=cls_name)
        if sdb is not None:
            sdb.close()
        if accesses is None:
            print(f"SKIPPED: Could not probe {args.symbol}", file=sys.stderr)
            return EXIT_SKIPPED
        print(format_field_access_map(accesses, args.symbol))
        return EXIT_EQUIVALENT

    # Single function comparison
    if not args.symbol:
        parser.error("--symbol is required (unless using --list-functions, --batch, or --batch-all)")
        return EXIT_ERROR

    return run_comparison(
        args.symbol, decomp_path, orig_path,
        verbose=args.verbose, timeout=args.timeout,
        json_output=args.json,
        coload=coload, coload_depth=coload_depth,
        fill_pattern=args.fill_pattern, dual_fixture=args.dual_fixture,
        field_access=not args.no_field_access)


if __name__ == "__main__":
    sys.exit(main())
