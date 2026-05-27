#!/usr/bin/env python3
"""
Compare RB3-xenon and DC3 decomp databases to find porting opportunities.

Finds functions in the shared system/ engine that DC3 has matched but RB3 hasn't.
Both repos are MSVC X360 builds, so symbol-name mangling is symmetric — exact
symbol matches are reliable. Demangled-name matching catches any residual
differences in template instantiations or static/anonymous symbols.

DC3 database default path: ~/code/milohax/dc3-decomp/decomp.db
(DC3 is the primary cross-binary oracle for rb3-xenon: same toolchain, same
/O1 /Oi /GR /EHsc flags, same Milo engine era.)
"""

import csv
import io
import json
import sqlite3
import argparse
import re
import sys
from pathlib import Path
from collections import defaultdict


RB3_ROOT = Path(__file__).parent.parent
DC3_ROOT = Path(__file__).parent.parent.parent / "dc3-decomp"

RB3_DB = RB3_ROOT / "decomp.db"
DC3_DB = DC3_ROOT / "decomp.db"

RB3_SRC = RB3_ROOT / "src"
DC3_SRC = DC3_ROOT / "src"


def connect(path: Path) -> sqlite3.Connection:
    if not path.exists():
        print(f"ERROR: database not found: {path}", file=sys.stderr)
        sys.exit(1)
    con = sqlite3.connect(path)
    con.row_factory = sqlite3.Row
    return con


def strip_prefix(unit: str, prefix: str) -> str:
    if unit and unit.startswith(prefix):
        return unit[len(prefix):]
    return unit


def norm_key(demangled: str) -> str:
    """
    Extract a normalized 'ClassName::FuncName' key from a demangled MSVC name.
    Both RB3-xenon and DC3 use MSVC mangling, so normalization is symmetric.
    Returns empty string if extraction fails.
    """
    if not demangled:
        return ''
    s = demangled.strip()
    # Strip MSVC access/calling-conv qualifiers
    s = re.sub(r'\b(public|private|protected):\s*', '', s)
    s = re.sub(r'\bstatic\s+', '', s)
    s = re.sub(r'\b(__cdecl|__thiscall|__stdcall|__fastcall|__clrcall)\b\s*', '', s)
    s = re.sub(r'\bvirtual\s+', '', s)
    # Find the function name (class::method or just method) before first '('
    # Handle templates: Class<T>::Method<U>(args)
    m = re.search(r'((?:[\w~]+::)*[\w~]+)(?:<[^(]*)?\s*\(', s)
    if m:
        return m.group(1)
    return ''


def unit_to_src_path(unit_bare: str, src_root: Path) -> Path | None:
    """
    Convert a bare unit name (e.g. 'system/char/CharBones') to a source file path.
    Returns the Path if it exists on disk, else None.
    """
    for ext in ('.cpp', '.c', '.h'):
        p = src_root / (unit_bare + ext)
        if p.exists():
            return p
    return None


def fmt_path(path: Path | None, relative_to: Path | None = None) -> str:
    if path is None:
        return "(not found)"
    if relative_to is not None:
        try:
            return str(path.relative_to(relative_to))
        except ValueError:
            pass
    return str(path)


def main():
    parser = argparse.ArgumentParser(
        description="Compare RB3-xenon vs DC3 decomp progress",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog="""
Examples:
  # Show all system/ functions DC3 has at 100%% but RB3 doesn't (exact symbol match)
  python3 scripts/dc3_compare.py

  # Show stubs and low-%% using demangled-name matching (much more results)
  python3 scripts/dc3_compare.py --demangled --max-rb3 30 --sort size

  # Show RB3 stubs (0%%) where DC3 has >= 80%%
  python3 scripts/dc3_compare.py --demangled --stubs --min-dc3 80

  # Show unit-level summary of porting opportunities
  python3 scripts/dc3_compare.py --demangled --units --max-rb3 50

  # Drill into a specific unit
  python3 scripts/dc3_compare.py --demangled --filter system/char/ --sort size

  # Show functions missing from RB3 entirely
  python3 scripts/dc3_compare.py --show-missing --filter system/math/

  # Show DC3 source paths for easy reference
  python3 scripts/dc3_compare.py --filter system/char/CharBones --dc3-src

  # Show both source paths
  python3 scripts/dc3_compare.py --filter system/char/ --dc3-src --rb3-src

  # Output just source file paths (for xargs/editor integration)
  python3 scripts/dc3_compare.py --filter system/char/CharBones --paths dc3

  # Only functions of a certain size range
  python3 scripts/dc3_compare.py --filter system/char/ --min-size 64 --max-size 256

  # Export results as JSON
  python3 scripts/dc3_compare.py --filter system/char/ --export json

  # Export results as CSV
  python3 scripts/dc3_compare.py --filter system/char/ --export csv

  # Use a custom DC3 database path
  python3 scripts/dc3_compare.py --dc3-db /path/to/dc3-decomp/decomp.db
""",
    )
    parser.add_argument(
        "--filter", "-f", default="system/",
        help="Unit prefix filter (default: 'system/')",
    )
    parser.add_argument(
        "--min-dc3", type=float, default=100.0,
        help="Minimum DC3 match%%%% to include (default: 100.0)",
    )
    parser.add_argument(
        "--max-rb3", type=float, default=99.9,
        help="Maximum RB3 match%%%% to include (default: 99.9)",
    )
    parser.add_argument(
        "--min-rb3", type=float, default=0.0,
        help="Minimum RB3 match%%%% to include (default: 0.0)",
    )
    parser.add_argument(
        "--stubs", action="store_true",
        help="Show only RB3 stub functions (0%%%% match); implies --max-rb3 0",
    )
    parser.add_argument(
        "--demangled", "-d", action="store_true", default=True,
        help="Use demangled-name matching (catches C++ funcs with different symbols) (default: on)",
    )
    parser.add_argument(
        "--no-demangled", dest="demangled", action="store_false",
        help="Disable demangled-name matching (exact symbol match only)",
    )
    parser.add_argument(
        "--show-missing", "-m", action="store_true",
        help="Also show functions present in DC3 but missing from RB3",
    )
    parser.add_argument(
        "--units", "-u", action="store_true",
        help="Show unit-level summary instead of per-function listing",
    )
    parser.add_argument(
        "--sort",
        choices=["unit", "symbol", "rb3", "gap", "size", "dc3"],
        default="unit",
        help="Sort per-function output (default: unit)",
    )
    parser.add_argument(
        "--limit", "-n", type=int, default=0,
        help="Limit rows (0 = unlimited)",
    )
    # --- Additional flags ---
    parser.add_argument(
        "--dc3-src", action="store_true",
        help="Show DC3 source file path for each function",
    )
    parser.add_argument(
        "--rb3-src", action="store_true",
        help="Show RB3 source file path for each function",
    )
    parser.add_argument(
        "--show-dc3-unit", action="store_true",
        help="Show DC3 unit column in per-function listing",
    )
    parser.add_argument(
        "--min-size", type=int, default=0,
        help="Minimum function size in bytes (default: 0)",
    )
    parser.add_argument(
        "--max-size", type=int, default=0,
        help="Maximum function size in bytes (default: 0 = unlimited)",
    )
    parser.add_argument(
        "--paths", choices=["dc3", "rb3", "both"],
        metavar="TARGET",
        help="Output mode: print unique source file paths only (dc3/rb3/both). "
             "Useful for piping to xargs or an editor. TARGET must be dc3, rb3, or both.",
    )
    parser.add_argument(
        "--export", choices=["json", "csv"],
        metavar="FORMAT",
        help="Export results as json or csv instead of human-readable table",
    )
    parser.add_argument(
        "--dc3-db", type=Path, default=DC3_DB,
        help=f"Path to DC3 decomp.db (default: {DC3_DB})",
    )
    parser.add_argument(
        "--rb3-db", type=Path, default=RB3_DB,
        help=f"Path to RB3 decomp.db (default: {RB3_DB})",
    )
    args = parser.parse_args()

    if args.stubs:
        args.max_rb3 = 0.0
        if args.min_dc3 == 100.0:
            args.min_dc3 = 80.0

    rb3 = connect(args.rb3_db)
    dc3 = connect(args.dc3_db)

    # Both rb3-xenon and DC3 are MSVC X360 builds — both use "default/" prefix
    rb3_prefix = "default/"
    dc3_prefix = "default/"
    filt = args.filter

    # Load all RB3 functions in filter range
    rb3_funcs: dict[str, dict] = {}
    for row in rb3.execute(
        "SELECT symbol, demangled, unit, size, current_percent, best_percent, verdict "
        "FROM functions WHERE unit LIKE ?",
        (f"{rb3_prefix}{filt}%",),
    ):
        rb3_funcs[row["symbol"]] = dict(row)

    # Load all DC3 functions in filter range
    dc3_funcs: dict[str, dict] = {}
    for row in dc3.execute(
        "SELECT symbol, demangled, unit, size, current_percent, best_percent, verdict "
        "FROM functions WHERE unit LIKE ?",
        (f"{dc3_prefix}{filt}%",),
    ):
        dc3_funcs[row["symbol"]] = dict(row)

    # Only print header when not in paths/export mode
    silent = args.paths or args.export
    if not silent:
        print(f"RB3-xenon '{filt}' functions : {len(rb3_funcs)}")
        print(f"DC3       '{filt}' functions : {len(dc3_funcs)}")
        print()

    # Build demangled-name index for DC3 if requested
    dc3_by_dem: dict[str, list] = defaultdict(list)
    if args.demangled:
        for sym, row in dc3_funcs.items():
            key = norm_key(row.get("demangled", ""))
            if key:
                dc3_by_dem[key].append(row)

    def find_dc3_matches(rb3row: dict) -> list[dict]:
        """Find DC3 entries matching this RB3 function."""
        sym = rb3row["symbol"]
        results = []
        # Exact symbol match first
        if sym in dc3_funcs:
            results.append(dc3_funcs[sym])
        # Demangled-name match
        if args.demangled:
            key = norm_key(rb3row.get("demangled", ""))
            if key:
                for dc3row in dc3_by_dem.get(key, []):
                    if dc3row["symbol"] != sym:  # avoid duplicate
                        results.append(dc3row)
        return results

    def resolve_src(unit_with_prefix: str, prefix: str, src_root: Path) -> Path | None:
        bare = strip_prefix(unit_with_prefix, prefix)
        return unit_to_src_path(bare, src_root)

    # Build per-function results
    shared_fixable = []
    missing_in_rb3 = []

    for rb3sym, rb3row in rb3_funcs.items():
        rb3_pct = rb3row["current_percent"] or 0.0
        if rb3_pct > args.max_rb3:
            continue
        if rb3_pct < args.min_rb3:
            continue
        size = rb3row["size"] or 0
        if args.min_size and size < args.min_size:
            continue
        if args.max_size and size > args.max_size:
            continue
        dc3_matches = find_dc3_matches(rb3row)
        if not dc3_matches:
            continue
        best_dc3 = max(dc3_matches, key=lambda r: r["current_percent"] or 0.0)
        dc3_pct = best_dc3["current_percent"] or 0.0
        if dc3_pct < args.min_dc3:
            continue
        rb3_unit = strip_prefix(rb3row["unit"], rb3_prefix)
        dc3_unit = strip_prefix(best_dc3["unit"], dc3_prefix)
        rb3_src_path = resolve_src(rb3row["unit"], rb3_prefix, RB3_SRC)
        dc3_src_path = resolve_src(best_dc3["unit"], dc3_prefix, DC3_SRC)
        shared_fixable.append({
            "symbol": rb3sym,
            "demangled": rb3row["demangled"] or best_dc3["demangled"] or rb3sym,
            "rb3_unit": rb3_unit,
            "dc3_unit": dc3_unit,
            "rb3_pct": rb3_pct,
            "dc3_pct": dc3_pct,
            "gap": dc3_pct - rb3_pct,
            "size": size,
            "is_stub": rb3_pct == 0.0 and rb3row["current_percent"] is None,
            "rb3_src": str(rb3_src_path) if rb3_src_path else None,
            "dc3_src": str(dc3_src_path) if dc3_src_path else None,
        })

    # Also find DC3 functions missing from RB3 (only for exact symbol match mode)
    if args.show_missing:
        rb3_syms = set(rb3_funcs.keys())
        for sym, dc3row in dc3_funcs.items():
            dc3_pct = dc3row["current_percent"] or 0.0
            if dc3_pct < args.min_dc3:
                continue
            if sym not in rb3_syms:
                dc3_src_path = resolve_src(dc3row["unit"], dc3_prefix, DC3_SRC)
                missing_in_rb3.append({
                    "symbol": sym,
                    "demangled": dc3row["demangled"] or sym,
                    "dc3_unit": strip_prefix(dc3row["unit"], dc3_prefix),
                    "dc3_pct": dc3_pct,
                    "size": dc3row["size"] or 0,
                    "dc3_src": str(dc3_src_path) if dc3_src_path else None,
                })

    # ---- Paths-only mode ----
    if args.paths:
        paths: set[str] = set()
        for r in shared_fixable:
            if args.paths in ("dc3", "both") and r["dc3_src"]:
                paths.add(r["dc3_src"])
            if args.paths in ("rb3", "both") and r["rb3_src"]:
                paths.add(r["rb3_src"])
        for p in sorted(paths):
            print(p)
        return

    # ---- Export mode ----
    if args.export:
        sort_key = {
            "unit":   lambda r: (r["rb3_unit"], r["symbol"]),
            "symbol": lambda r: r["symbol"],
            "rb3":    lambda r: (r["rb3_pct"], r["symbol"]),
            "dc3":    lambda r: (-r["dc3_pct"], r["symbol"]),
            "gap":    lambda r: (-r["gap"], r["symbol"]),
            "size":   lambda r: (-r["size"], r["symbol"]),
        }[args.sort]
        shared_fixable.sort(key=sort_key)
        rows = shared_fixable[:args.limit] if args.limit else shared_fixable
        if args.export == "json":
            print(json.dumps(rows, indent=2))
        elif args.export == "csv":
            fields = ["symbol", "demangled", "rb3_unit", "dc3_unit",
                      "rb3_pct", "dc3_pct", "gap", "size",
                      "is_stub", "rb3_src", "dc3_src"]
            out = io.StringIO()
            writer = csv.DictWriter(out, fieldnames=fields, extrasaction="ignore")
            writer.writeheader()
            writer.writerows(rows)
            print(out.getvalue(), end="")
        return

    # ---- Unit-level summary mode ----
    if args.units:
        rb3_by_unit: dict[str, dict] = defaultdict(lambda: {
            "total": 0, "matched": 0, "fixable": 0, "fixable_size": 0, "stubs": 0
        })
        for sym, row in rb3_funcs.items():
            unit = strip_prefix(row["unit"], rb3_prefix)
            rb3_by_unit[unit]["total"] += 1
            pct = row["current_percent"] or 0.0
            if pct >= 100.0:
                rb3_by_unit[unit]["matched"] += 1

        for r in shared_fixable:
            rb3_by_unit[r["rb3_unit"]]["fixable"] += 1
            rb3_by_unit[r["rb3_unit"]]["fixable_size"] += r["size"]
            if r["is_stub"]:
                rb3_by_unit[r["rb3_unit"]]["stubs"] += 1

        summary = []
        for unit, d in rb3_by_unit.items():
            if d["fixable"] == 0:
                continue
            pct_matched = d["matched"] / d["total"] * 100 if d["total"] else 0
            summary.append((unit, d["total"], pct_matched,
                             d["fixable"], d["stubs"], d["fixable_size"]))

        summary.sort(key=lambda x: -x[5])  # sort by fixable bytes

        if args.limit:
            summary = summary[:args.limit]

        print(f"{'Unit':<50} {'Total':>7} {'Match%':>7} {'DC3Fix':>7} {'Stubs':>6} {'Bytes':>8}")
        print("-" * 90)
        for unit, total, pct_m, fixable, stubs, fix_size in summary:
            stub_str = f"{stubs}" if stubs else ""
            print(f"{unit:<50} {total:>7} {pct_m:>6.1f}% {fixable:>7} {stub_str:>6} {fix_size:>8}")
        print()
        total_stubs = sum(1 for r in shared_fixable if r["is_stub"])
        total_fixable_size = sum(r["size"] for r in shared_fixable)
        print(f"Total fixable (DC3>={args.min_dc3:.0f}%, RB3<={args.max_rb3:.0f}%): {len(shared_fixable)} functions, {total_fixable_size:,} bytes potential gain")
        if total_stubs:
            print(f"  of which true stubs (RB3 unimplemented): {total_stubs}")
        if args.show_missing:
            print(f"Total missing from RB3: {len(missing_in_rb3)}")
        return

    # ---- Per-function listing ----
    sort_key = {
        "unit":   lambda r: (r["rb3_unit"], r["symbol"]),
        "symbol": lambda r: r["symbol"],
        "rb3":    lambda r: (r["rb3_pct"], r["symbol"]),
        "dc3":    lambda r: (-r["dc3_pct"], r["symbol"]),
        "gap":    lambda r: (-r["gap"], r["symbol"]),
        "size":   lambda r: (-r["size"], r["symbol"]),
    }[args.sort]
    shared_fixable.sort(key=sort_key)

    rows = shared_fixable[:args.limit] if args.limit else shared_fixable

    if rows:
        show_dc3_unit = args.show_dc3_unit
        show_dc3_src = args.dc3_src
        show_rb3_src = args.rb3_src

        mode = "demangled" if args.demangled else "symbol"
        print(f"Functions DC3>={args.min_dc3:.0f}% and RB3<={args.max_rb3:.0f}% ({mode} match, sorted by {args.sort}):")
        print()

        # Build header dynamically
        header = f"{'RB3 Unit':<40} {'RB3%':>6} {'DC3%':>6} {'Size':>6}  "
        if show_dc3_unit:
            header += f"{'DC3 Unit':<40}  "
        header += "Demangled"
        sep_len = max(100, len(header) + 20)
        print(header)
        print("-" * sep_len)

        for r in rows:
            dem = r["demangled"] or r["symbol"]
            stub_marker = "* " if r["is_stub"] else "  "
            if len(dem) > 60:
                dem = dem[:57] + "..."
            line = f"{r['rb3_unit']:<40} {r['rb3_pct']:>6.1f} {r['dc3_pct']:>6.1f} {r['size']:>6}  "
            if show_dc3_unit:
                dc3u = r["dc3_unit"]
                if len(dc3u) > 38:
                    dc3u = dc3u[:35] + "..."
                line += f"{dc3u:<40}  "
            line += f"{stub_marker}{dem}"
            print(line)
            if show_dc3_src and r["dc3_src"]:
                print(f"    DC3: {r['dc3_src']}")
            if show_rb3_src and r["rb3_src"]:
                print(f"    RB3: {r['rb3_src']}")

        print()
        total_stubs = sum(1 for r in rows if r["is_stub"])
        total_fixable_size = sum(r["size"] for r in shared_fixable)
        shown_size = sum(r["size"] for r in rows)
        print(f"Total: {len(shared_fixable)} functions, {total_fixable_size:,} bytes potential gain (showing {len(rows)}, {shown_size:,} bytes)")
        if total_stubs:
            print(f"  * = true stub (unimplemented in RB3): {total_stubs}")

    if args.show_missing and missing_in_rb3:
        missing_in_rb3.sort(key=lambda r: (-r["size"], r["dc3_unit"], r["symbol"]))
        if args.limit:
            missing_in_rb3 = missing_in_rb3[:args.limit]
        print()
        print(f"Functions in DC3>={args.min_dc3:.0f}% but MISSING from RB3:")
        print()
        print(f"{'DC3 Unit':<50} {'DC3%':>6} {'Size':>6}  Demangled")
        print("-" * 120)
        for r in missing_in_rb3:
            dem = (r["demangled"] or r["symbol"])[:60]
            print(f"{r['dc3_unit']:<50} {r['dc3_pct']:>6.1f} {r['size']:>6}  {dem}")
            if args.dc3_src and r["dc3_src"]:
                print(f"    DC3: {r['dc3_src']}")
        print()
        missing_size = sum(r["size"] for r in missing_in_rb3)
        print(f"Total: {len(missing_in_rb3)} missing, {missing_size:,} bytes")


if __name__ == "__main__":
    main()
