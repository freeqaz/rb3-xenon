#!/usr/bin/env python3
"""Benchmark harness for unicorn runner performance testing.

Usage:
    # Quick wall-clock timing (10 units, no cache):
    python3 -m scripts.unicorn_runner.bench

    # With cProfile:
    python3 -m scripts.unicorn_runner.bench --profile

    # Custom unit count:
    python3 -m scripts.unicorn_runner.bench -n 10
"""

import argparse
import cProfile
import os
import pstats
import sys
import time

from .run import get_all_units, run_batch


def find_test_units(n=10):
    """Find n units that have both decomp and orig .obj files."""
    units = get_all_units()
    good = []
    for name, dp, op in units:
        if os.path.exists(dp) and os.path.exists(op):
            good.append((name, dp, op))
        if len(good) >= n:
            break
    return good


def run_benchmark(units, dual_fixture=True):
    """Run batch comparison on all units, return stats."""
    total_eq = 0
    total_div = 0
    total_err = 0
    total_sk = 0
    total_funcs = 0

    for name, dp, op in units:
        t0 = time.perf_counter()
        eq, div, err, sk, _cached = run_batch(
            dp, op, timeout=5_000_000, quiet=True,
            coload=True, dual_fixture=dual_fixture,
            cache=None)  # No cache — we want real execution
        elapsed = time.perf_counter() - t0
        funcs = eq + div + err + sk
        rate = funcs / elapsed if elapsed > 0 else 0
        total_eq += eq
        total_div += div
        total_err += err
        total_sk += sk
        total_funcs += funcs
        print(f"  {name}: {funcs} funcs in {elapsed:.1f}s ({rate:.1f} func/s) "
              f"[{eq}eq {div}div {err}err {sk}sk]")

    return total_eq, total_div, total_err, total_sk, total_funcs


def main():
    parser = argparse.ArgumentParser(description="Unicorn runner benchmark")
    parser.add_argument("-n", type=int, default=10, help="Number of units to test")
    parser.add_argument("--profile", action="store_true", help="Run with cProfile")
    parser.add_argument("--profile-sort", default="cumulative",
                        help="cProfile sort key (default: cumulative)")
    parser.add_argument("--profile-lines", type=int, default=40,
                        help="Number of cProfile lines to show")
    parser.add_argument("--no-dual", action="store_true",
                        help="Single fixture instead of dual")
    args = parser.parse_args()

    units = find_test_units(args.n)
    if not units:
        print("ERROR: No units found with both decomp and orig .obj files")
        return 1

    dual = not args.no_dual
    mode = "dual-fixture" if dual else "single-fixture"
    print(f"Benchmark: {len(units)} units, {mode}, no cache\n")

    if args.profile:
        profiler = cProfile.Profile()
        profiler.enable()

    t_start = time.perf_counter()
    eq, div, err, sk, total = run_benchmark(units, dual_fixture=dual)
    t_total = time.perf_counter() - t_start

    if args.profile:
        profiler.disable()

    rate = total / t_total if t_total > 0 else 0
    print(f"\n=== BENCHMARK RESULTS ===")
    print(f"Units: {len(units)}")
    print(f"Functions: {total}")
    print(f"  Equivalent: {eq}")
    print(f"  Divergent:  {div}")
    print(f"  Errors:     {err}")
    print(f"  Skipped:    {sk}")
    print(f"Wall time: {t_total:.2f}s")
    print(f"Throughput: {rate:.1f} func/s")

    if args.profile:
        print(f"\n=== PROFILE (top {args.profile_lines}, sort={args.profile_sort}) ===")
        stats = pstats.Stats(profiler, stream=sys.stdout)
        stats.strip_dirs()
        stats.sort_stats(args.profile_sort)
        stats.print_stats(args.profile_lines)

    return 0


if __name__ == "__main__":
    sys.exit(main())
