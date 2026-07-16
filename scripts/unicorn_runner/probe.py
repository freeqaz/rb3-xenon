#!/usr/bin/env python3
"""Multi-input probing CLI.

Usage:
    python3 -m scripts.unicorn_runner.probe --unit Foo --symbol Bar --runs 16
    python3 -m scripts.unicorn_runner.probe --unit Foo --batch --runs 8
"""

import argparse
import io
import os
import sys

from .coff import COFFParser
from .prober import probe_function, format_probe_result
from .run import resolve_unit, list_functions


def main():
    parser = argparse.ArgumentParser(
        description="Multi-input probing — run functions N times with varied inputs")
    parser.add_argument("--unit", required=True, help="Unit name (resolves paths from objdiff.json)")
    parser.add_argument("--symbol", help="Specific mangled symbol to probe")
    parser.add_argument("--batch", action="store_true", help="Probe all eligible functions in the unit")
    parser.add_argument("--runs", type=int, default=8, help="Number of probe runs per function (default: 8)")
    parser.add_argument("--no-coload", action="store_true", help="Disable intra-TU callee co-loading")
    parser.add_argument("--coload-depth", type=int, default=None, help="Limit callee co-loading recursion depth")
    parser.add_argument("--seed", type=int, default=42, help="Random seed for reproducibility (default: 42)")
    parser.add_argument("--typed", action="store_true",
                       help="Use type-aware object memory from struct_db")
    parser.add_argument("--no-early-exit", action="store_true",
                       help="Disable early exit optimization (run all N iterations)")

    args = parser.parse_args()

    coload = not args.no_coload

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

    decomp_coff = COFFParser(decomp_path)
    orig_coff = COFFParser(orig_path)

    early_exit = not args.no_early_exit

    if args.symbol:
        probe = probe_function(
            args.symbol, decomp_coff, orig_coff,
            runs=args.runs, coload=coload, coload_depth=args.coload_depth,
            seed=args.seed, typed=args.typed, unit_class=args.unit,
            early_exit=early_exit)
        if probe is None:
            print("SKIPPED: Symbol not found or empty", file=sys.stderr)
            return 3
        print(format_probe_result(probe, symbol=args.symbol))
        return 0 if probe.stable_equiv else 1

    # Batch mode — pre-generate typed memory once for the unit
    typed_mem_zero = None
    typed_mem_cd = None
    if args.typed:
        from .typed_fixture import extract_class_from_unit, generate_typed_object
        unit_class = extract_class_from_unit(args.unit)
        if unit_class:
            from .prober import _load_struct_db
            db, ok = _load_struct_db()
            if ok:
                import random
                rng = random.Random(args.seed)
                typed_mem_zero = generate_typed_object(unit_class, db, rng, fill_byte=0x00)
                typed_mem_cd = generate_typed_object(unit_class, db, rng, fill_byte=0xCD)
                db.close()
                if typed_mem_zero:
                    print(f"  Typed fixtures: class={unit_class}")

    old_stdout = sys.stdout
    sys.stdout = io.StringIO()
    try:
        eligible = list_functions(decomp_path, orig_path,
                                  decomp_coff=decomp_coff, orig_coff=orig_coff)
    finally:
        sys.stdout = old_stdout

    print(f"=== {args.unit} — {args.runs}-run probe ({len(eligible)} functions) ===")

    stable_equiv = 0
    stable_div = 0
    input_sensitive = 0
    skipped = 0

    div_class_totals = {}

    for sym_name, d_size, o_size in eligible:
        probe = probe_function(
            sym_name, decomp_coff, orig_coff,
            runs=args.runs, coload=coload, coload_depth=args.coload_depth,
            seed=args.seed, typed=args.typed, unit_class=args.unit,
            typed_mem_zero=typed_mem_zero, typed_mem_cd=typed_mem_cd,
            early_exit=early_exit)
        if probe is None:
            skipped += 1
            continue

        # Aggregate divergence classes
        for cls, count in probe.divergence_classes.items():
            div_class_totals[cls] = div_class_totals.get(cls, 0) + 1

        label = probe.confidence
        eq_frac = f"{probe.equiv_runs}/{probe.total_runs}"

        if probe.stable_equiv:
            stable_equiv += 1
            status = "EQUIV"
        elif probe.stable_divergent:
            stable_div += 1
            classes = ",".join(probe.divergence_classes.keys()) if probe.divergence_classes else "?"
            status = f"DIV({classes})"
        else:
            input_sensitive += 1
            status = "SENSITIVE"

        print(f"  {status:<20s}  {eq_frac:>5s} equiv  {sym_name}")

    total = stable_equiv + stable_div + input_sensitive
    print()
    print(f"  Summary: {stable_equiv} stable equiv, {stable_div} stable div, "
          f"{input_sensitive} input-sensitive, {skipped} skipped ({total} tested)")
    if div_class_totals:
        classes = ", ".join(f"{k}: {v}" for k, v in sorted(div_class_totals.items()))
        print(f"  Divergence classes: {classes}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
