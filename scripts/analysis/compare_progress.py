#!/usr/bin/env python3
"""
Compare decomp progress between two report.json files, or show current snapshot.

Usage:
    python3 scripts/analysis/compare_progress.py <baseline_report> <current_report>
    python3 scripts/analysis/compare_progress.py --snapshot [report.json]

Examples:
    # Compare against baseline
    python3 scripts/analysis/compare_progress.py ../og-rb3-xenon/build/45410914/report.json build/45410914/report.json

    # Show detailed unit breakdown
    python3 scripts/analysis/compare_progress.py --detailed ../og-rb3-xenon/build/45410914/report.json build/45410914/report.json

    # Show function-level changes (regressions and improvements)
    python3 scripts/analysis/compare_progress.py --functions baseline.json current.json

    # Only show regressions across all views
    python3 scripts/analysis/compare_progress.py --regressions --functions --detailed baseline.json current.json

    # Show current snapshot (all subsystems)
    python3 scripts/analysis/compare_progress.py --snapshot
    python3 scripts/analysis/compare_progress.py --snapshot --sort=percent

    # Filter to specific paths using glob patterns
    python3 scripts/analysis/compare_progress.py --snapshot --filter 'system/ui/*'
    python3 scripts/analysis/compare_progress.py --filter 'system/char/*' --functions baseline.json current.json
    python3 scripts/analysis/compare_progress.py --snapshot --filter '*/synth/*' --filter '*/midi/*'
"""

import argparse
import fnmatch
import json
import re
import sys
from pathlib import Path


# Subsystems to exclude by default (third-party, XDK, tiny standalone files)
EXCLUDED_PREFIXES = ("xdk/", "lib/", "default/")
DEFAULT_MIN_SIZE = 10240  # 10KB


# Default map file for merged symbol resolution
SCRIPT_DIR = Path(__file__).parent
PROJECT_ROOT = SCRIPT_DIR.parent.parent
DEFAULT_MAP_FILE = PROJECT_ROOT / "orig" / "45410914" / "ham_xbox_r.map"


class MergedSymbolResolver:
    """Resolve merged_<addr> names to actual mangled symbol names via the linker map."""

    def __init__(self, map_file: Path):
        self._address_to_symbols: dict[str, list[str]] = {}
        self._loaded = False
        self._map_file = map_file

    def _ensure_loaded(self):
        if self._loaded:
            return
        if not self._map_file.exists():
            self._loaded = True
            return
        pattern = re.compile(
            r'^\s*\d{4}:[0-9a-fA-F]+\s+'
            r'(\S+)\s+'
            r'([0-9a-fA-F]{8})\s+'
        )
        with open(self._map_file, 'r') as f:
            for line in f:
                match = pattern.match(line)
                if match:
                    symbol = match.group(1)
                    address = match.group(2).upper()
                    if address not in self._address_to_symbols:
                        self._address_to_symbols[address] = []
                    self._address_to_symbols[address].append(symbol)
        self._loaded = True

    def resolve(self, merged_name: str) -> list[str]:
        """Given 'merged_825FDA60', return list of mangled symbol names at that address."""
        self._ensure_loaded()
        addr = merged_name[7:].upper() if merged_name.startswith("merged_") else merged_name.upper()
        return self._address_to_symbols.get(addr, [])


def count_matched_functions(unit: dict) -> tuple[int, int]:
    """Count matched/total functions using normalized match percent.

    Uses match_percent_normalized (which excludes arg-only diffs like
    register/offset swaps) if available, otherwise falls back to
    fuzzy_match_percent.
    """
    functions = unit.get("functions", [])
    total = len(functions)
    matched = 0
    for f in functions:
        pct = f.get("match_percent_normalized")
        if pct is None:
            pct = f.get("fuzzy_match_percent") or 0
        if pct >= 100.0:
            matched += 1
    return matched, total


def get_subsystem(name: str) -> str:
    """Extract subsystem from unit name."""
    parts = name.split("/")
    if len(parts) >= 3:
        return f"{parts[1]}/{parts[2]}"
    return name


def fmt_bytes(b: int) -> str:
    """Format byte count with sign and units."""
    if abs(b) >= 1024:
        return f"{b/1024:+.1f} KB"
    return f"{b:+d} B"


def fmt_bytes_plain(b: int) -> str:
    """Format byte count without sign."""
    if abs(b) >= 1024 * 1024:
        return f"{b/1024/1024:.2f} MB"
    if abs(b) >= 1024:
        return f"{b/1024:.1f} KB"
    return f"{b} B"


def expand_filter_patterns(patterns: list) -> list:
    """Expand filter patterns for convenience.

    If a pattern doesn't start with '*' or contain '/' at the start,
    prepend '*/' so 'system/ui/*' matches 'default/system/ui/Foo'.
    """
    expanded = []
    for p in patterns:
        expanded.append(p)
        # Also try with '*/' prefix if pattern doesn't already start with '*' or '/'
        if not p.startswith("*") and not p.startswith("/"):
            expanded.append("*/" + p)
    return expanded


def filter_units(units: list, patterns: list) -> list:
    """Filter units by glob patterns matched against their name."""
    if not patterns:
        return units
    expanded = expand_filter_patterns(patterns)
    filtered = []
    for u in units:
        name = u["name"]
        if any(fnmatch.fnmatch(name, p) for p in expanded):
            filtered.append(u)
    return filtered


def load_report(path: Path) -> dict:
    """Load a report.json file."""
    with open(path) as f:
        return json.load(f)


def get_unit_match_percent(measures: dict) -> float | None:
    """Get the best available match percentage for a unit.

    Prefers fuzzy_match_percent, falls back to matched_code_percent.
    Returns None if no match data is available.
    """
    fp = measures.get("fuzzy_match_percent", None)
    if fp is not None:
        return fp
    mcp = measures.get("matched_code_percent", None)
    if mcp is not None:
        return mcp
    # If we have matched_code/total_code, compute it
    tc = int(measures.get("total_code", 0) or 0)
    mc = int(measures.get("matched_code", 0) or 0)
    if tc > 0 and mc > 0:
        return 100.0 * mc / tc
    return None


def aggregate_by_subsystem(units: list) -> dict:
    """Aggregate unit stats by subsystem using best available match percentages.

    Prefers fuzzy_match_percent, falls back to matched_code_percent.
    Units without any match data are counted for total_functions but not
    for percentage calculations.
    """
    agg = {}
    for u in units:
        sub = get_subsystem(u["name"])
        if sub not in agg:
            agg[sub] = {
                "fuzzy_code": 0,
                "weighted_fuzzy": 0.0,
                "total_code": 0,
                "total_functions": 0,
                "matched_functions": 0,
            }
        measures = u.get("measures", {})
        tc = int(measures.get("total_code", 0) or 0)
        pct = get_unit_match_percent(measures)
        agg[sub]["total_code"] += tc
        if pct is not None and tc > 0:
            agg[sub]["fuzzy_code"] += tc
            agg[sub]["weighted_fuzzy"] += pct * tc
        matched, total = count_matched_functions(u)
        agg[sub]["total_functions"] += total
        agg[sub]["matched_functions"] += matched
    return agg


def compare_subsystems(baseline: dict, current: dict) -> list:
    """Compare aggregated subsystem stats using fuzzy match percentages."""
    baseline_agg = aggregate_by_subsystem(baseline["units"])
    current_agg = aggregate_by_subsystem(current["units"])

    results = []
    for sub, curr in current_agg.items():
        if sub in baseline_agg and curr["fuzzy_code"] > 0 and baseline_agg[sub]["fuzzy_code"] > 0:
            base = baseline_agg[sub]
            base_pct = base["weighted_fuzzy"] / base["fuzzy_code"] if base["fuzzy_code"] > 0 else 0
            curr_pct = curr["weighted_fuzzy"] / curr["fuzzy_code"] if curr["fuzzy_code"] > 0 else 0
            diff_pct = curr_pct - base_pct
            diff_funcs = curr["matched_functions"] - base["matched_functions"]

            if abs(diff_pct) > 0.005:
                results.append({
                    "subsystem": sub,
                    "base_pct": base_pct,
                    "curr_pct": curr_pct,
                    "diff_pct": diff_pct,
                    "diff_funcs": diff_funcs,
                    "total_code": curr["total_code"],
                })

    results.sort(key=lambda x: x["diff_pct"], reverse=True)
    return results


def compare_units(baseline: dict, current: dict, min_diff: float = 0.01) -> list:
    """Compare individual unit stats."""
    baseline_units = {u["name"]: u for u in baseline["units"]}
    current_units = {u["name"]: u for u in current["units"]}

    results = []
    for name, curr in current_units.items():
        if name in baseline_units:
            base = baseline_units[name]
            base_measures = base.get("measures", {})
            curr_measures = curr.get("measures", {})
            base_pct = get_unit_match_percent(base_measures) or 0
            curr_pct = get_unit_match_percent(curr_measures) or 0
            diff = curr_pct - base_pct

            if abs(diff) > min_diff:
                base_matched, base_total = count_matched_functions(base)
                curr_matched, curr_total = count_matched_functions(curr)
                results.append({
                    "name": name,
                    "base_pct": base_pct,
                    "curr_pct": curr_pct,
                    "diff_pct": diff,
                    "base_funcs": f"{base_matched}/{base_total}",
                    "curr_funcs": f"{curr_matched}/{curr_total}",
                })

    results.sort(key=lambda x: x["diff_pct"], reverse=True)
    return results


def compare_functions(baseline: dict, current: dict, min_diff: float = 0.5,
                      merged_resolver: MergedSymbolResolver = None) -> list:
    """Compare individual function match percentages between two reports.

    Returns a list of functions whose fuzzy_match_percent changed, sorted by
    regression severity (most regressed first).
    """
    # Build function lookup: (unit_name, func_name) -> fuzzy_match_percent
    def build_func_map(report):
        fmap = {}
        merged_entries = []  # (unit_name, merged_name, entry) for second pass
        for unit in report.get("units", []):
            unit_name = unit["name"]
            for func in unit.get("functions", []):
                fname = func.get("name", "")
                pct = func.get("fuzzy_match_percent", None)
                demangled = func.get("metadata", {}).get("demangled_name", "")
                entry = {
                    "pct": pct,
                    "size": int(func.get("size", 0)),
                    "demangled": demangled,
                }
                fmap[(unit_name, fname)] = entry
                if fname.startswith("merged_"):
                    merged_entries.append((unit_name, fname, entry))
        # Resolve merged_<addr> names to actual symbols
        if merged_resolver and merged_entries:
            for unit_name, merged_name, entry in merged_entries:
                for symbol in merged_resolver.resolve(merged_name):
                    alt_key = (unit_name, symbol)
                    if alt_key not in fmap:
                        fmap[alt_key] = entry
        return fmap

    base_funcs = build_func_map(baseline)
    curr_funcs = build_func_map(current)

    results = []
    for key, curr in curr_funcs.items():
        if key in base_funcs:
            base = base_funcs[key]
            # Skip functions with no match data in either
            if base["pct"] is None and curr["pct"] is None:
                continue
            base_pct = base["pct"] or 0
            curr_pct = curr["pct"] or 0
            diff = curr_pct - base_pct

            if abs(diff) >= min_diff:
                unit_name, func_name = key
                display = curr["demangled"] or base["demangled"] or func_name
                results.append({
                    "unit": unit_name,
                    "name": func_name,
                    "display": display,
                    "base_pct": base_pct,
                    "curr_pct": curr_pct,
                    "diff_pct": diff,
                    "size": curr["size"],
                })

    # Sort: most regressed first, then most improved
    results.sort(key=lambda x: x["diff_pct"])
    return results


def print_subsystem_table(results: list, baseline: dict, current: dict):
    """Print subsystem comparison table."""
    total_funcs = sum(r["diff_funcs"] for r in results)
    base_total = baseline.get("measures", {}).get("fuzzy_match_percent", 0)
    curr_total = current.get("measures", {}).get("fuzzy_match_percent", 0)

    print()
    print(f"Overall fuzzy: {base_total:.2f}% -> {curr_total:.2f}% ({curr_total-base_total:+.2f}%)")
    print(f"Subsystems changed: {len(results)}, {total_funcs:+d} matched functions")
    print()

    # Calculate column widths
    headers = ["Subsystem", "Baseline", "Current", "Change", "Size"]
    rows = []
    for r in results:
        rows.append([
            r["subsystem"],
            f"{r['base_pct']:.2f}%",
            f"{r['curr_pct']:.2f}%",
            f"{r['diff_pct']:+.2f}%",
            fmt_bytes_plain(r["total_code"]),
        ])

    widths = [len(h) for h in headers]
    for row in rows:
        for i, cell in enumerate(row):
            widths[i] = max(widths[i], len(cell))

    def fmt_row(cells, align_right=None):
        if align_right is None:
            align_right = [False] * len(cells)
        parts = []
        for i, cell in enumerate(cells):
            if align_right[i]:
                parts.append(cell.rjust(widths[i]))
            else:
                parts.append(cell.ljust(widths[i]))
        return "| " + " | ".join(parts) + " |"

    align = [False, True, True, True, True]  # Right-align numeric columns
    print(fmt_row(headers))
    print("|" + "|".join("-" * (w + 2) for w in widths) + "|")
    for row in rows:
        print(fmt_row(row, align))


def count_100pct_units(baseline: dict, current: dict) -> tuple:
    """Count units at 100% in current and how many are new since baseline."""
    baseline_units = {u["name"]: u for u in baseline.get("units", [])}
    current_units = {u["name"]: u for u in current.get("units", [])}

    at_100 = 0
    newly_100 = 0
    for name, cu in current_units.items():
        curr_pct = get_unit_match_percent(cu.get("measures", {}))
        if curr_pct is not None and curr_pct >= 99.99:
            at_100 += 1
            if name in baseline_units:
                base_pct = get_unit_match_percent(baseline_units[name].get("measures", {})) or 0
                if base_pct < 99.99:
                    newly_100 += 1
            else:
                newly_100 += 1  # new unit not in baseline
    return at_100, newly_100


def print_unit_table(results: list, limit: int = 50, baseline: dict = None, current: dict = None):
    """Print detailed unit comparison table."""
    count = min(limit, len(results))
    print()
    print(f"Top {count} Unit Changes")
    print()

    # Show 100% summary from raw reports
    if baseline is not None and current is not None:
        at_100, newly_100 = count_100pct_units(baseline, current)
        if at_100:
            print(f"  Units at 100%: {at_100} ({newly_100} new since baseline)")
            print()

    headers = ["Unit Path", "Baseline", "Current", "Change", "Funcs (base)", "Funcs (curr)"]
    rows = []
    for r in results[:limit]:
        rows.append([
            r["name"].replace("default/", ""),
            f"{r['base_pct']:.2f}%",
            f"{r['curr_pct']:.2f}%",
            f"{r['diff_pct']:+.2f}%",
            r["base_funcs"],
            r["curr_funcs"],
        ])

    widths = [len(h) for h in headers]
    for row in rows:
        for i, cell in enumerate(row):
            widths[i] = max(widths[i], len(cell))

    def fmt_row(cells, align_right=None):
        if align_right is None:
            align_right = [False] * len(cells)
        parts = []
        for i, cell in enumerate(cells):
            if align_right[i]:
                parts.append(cell.rjust(widths[i]))
            else:
                parts.append(cell.ljust(widths[i]))
        return "| " + " | ".join(parts) + " |"

    align = [False, True, True, True, True, True]  # Right-align numeric columns
    print(fmt_row(headers))
    print("|" + "|".join("-" * (w + 2) for w in widths) + "|")
    for row in rows:
        print(fmt_row(row, align))


def print_function_table(results: list, limit: int = 100):
    """Print function-level comparison table."""
    count = min(limit, len(results))
    if not results:
        print("\nNo function-level changes found.")
        return

    # Separate regressions and improvements
    regressions = [r for r in results if r["diff_pct"] < 0]
    improvements = [r for r in results if r["diff_pct"] > 0]

    if regressions:
        print()
        reg_count = min(limit, len(regressions))
        print(f"Regressions ({len(regressions)} functions, showing top {reg_count}):")
        print()
        _print_func_rows(regressions[:limit])

    if improvements:
        print()
        imp_count = min(limit, len(improvements))
        # Show improvements sorted best-first
        imp_sorted = sorted(improvements, key=lambda x: x["diff_pct"], reverse=True)
        print(f"Improvements ({len(improvements)} functions, showing top {imp_count}):")
        print()
        _print_func_rows(imp_sorted[:limit])

    # Summary
    total_reg = len(regressions)
    total_imp = len(improvements)
    reg_bytes = sum(r["size"] for r in regressions)
    imp_bytes = sum(r["size"] for r in improvements)
    print()
    print(f"Summary: {total_reg} regressions ({fmt_bytes_plain(reg_bytes)} affected), "
          f"{total_imp} improvements ({fmt_bytes_plain(imp_bytes)} affected)")


def _print_func_rows(results: list):
    """Print rows for function comparison."""
    headers = ["Function", "Unit", "Base", "Curr", "Change", "Size"]
    rows = []
    for r in results:
        # Truncate long demangled names
        display = r["display"]
        if len(display) > 60:
            display = display[:57] + "..."
        unit = r["unit"].replace("default/", "")
        # Shorten unit path
        if len(unit) > 30:
            unit = "..." + unit[-27:]
        rows.append([
            display,
            unit,
            f"{r['base_pct']:.1f}%",
            f"{r['curr_pct']:.1f}%",
            f"{r['diff_pct']:+.1f}%",
            str(r["size"]),
        ])

    widths = [len(h) for h in headers]
    for row in rows:
        for i, cell in enumerate(row):
            widths[i] = max(widths[i], len(cell))

    def fmt_row(cells, align_right=None):
        if align_right is None:
            align_right = [False] * len(cells)
        parts = []
        for i, cell in enumerate(cells):
            if align_right[i]:
                parts.append(cell.rjust(widths[i]))
            else:
                parts.append(cell.ljust(widths[i]))
        return "| " + " | ".join(parts) + " |"

    align = [False, False, True, True, True, True]
    print(fmt_row(headers))
    print("|" + "|".join("-" * (w + 2) for w in widths) + "|")
    for row in rows:
        print(fmt_row(row, align))


def get_category(subsystem: str) -> str:
    """Categorize a subsystem."""
    if subsystem.startswith("xdk/"):
        return "XDK"
    if subsystem.startswith("lib/"):
        return "Third-Party"
    if subsystem.startswith("lazer/"):
        return "Game Code"
    if subsystem.startswith("default/"):
        return "Standalone"
    return "Milo Engine"


def print_overview(report: dict):
    """Print complete overview grouped by category."""
    agg = aggregate_by_subsystem(report.get("units", []))

    # Build results with category
    results = []
    for sub, stats in agg.items():
        if stats["total_code"] > 0:
            pct = stats["weighted_fuzzy"] / stats["fuzzy_code"] if stats["fuzzy_code"] > 0 else 0
            results.append({
                "subsystem": sub,
                "category": get_category(sub),
                "total_code": stats["total_code"],
                "percent": pct,
                "matched_funcs": stats["matched_functions"],
                "total_funcs": stats["total_functions"],
            })

    # Overall stats
    measures = report.get("measures", {})
    total_pct = measures.get("fuzzy_match_percent", 0) or 0
    total_code = int(measures.get("total_code", 0) or 0)
    total_funcs_matched = int(measures.get("matched_functions", 0) or 0)
    total_funcs = int(measures.get("total_functions", 0) or 0)

    print()
    print(f"{'='*70}")
    print(f"  DECOMP OVERVIEW: {total_pct:.2f}% fuzzy match")
    print(f"  Code: {fmt_bytes_plain(total_code)}")
    print(f"  Functions: {total_funcs_matched:,} / {total_funcs:,}")
    print(f"{'='*70}")

    # Group by category
    categories = ["Game Code", "Milo Engine", "Third-Party", "XDK", "Standalone"]
    for cat in categories:
        cat_results = [r for r in results if r["category"] == cat]
        if not cat_results:
            continue

        # Sort by size within category
        cat_results.sort(key=lambda x: x["total_code"], reverse=True)

        # Category totals
        cat_total = sum(r["total_code"] for r in cat_results)
        cat_weighted = sum(r["percent"] * r["total_code"] for r in cat_results)
        cat_pct = cat_weighted / cat_total if cat_total > 0 else 0
        cat_funcs_matched = sum(r["matched_funcs"] for r in cat_results)
        cat_funcs_total = sum(r["total_funcs"] for r in cat_results)

        print()
        print(f"## {cat}: {cat_pct:.1f}% ({fmt_bytes_plain(cat_total)}, {cat_funcs_matched}/{cat_funcs_total} funcs)")
        print()

        headers = ["Subsystem", "Fuzzy %", "Total", "Funcs"]
        rows = []
        for r in cat_results:
            # Trim category prefix for cleaner display
            name = r["subsystem"]
            if name.startswith("system/"):
                name = name[7:]
            elif name.startswith("lazer/"):
                name = name[6:]
            elif name.startswith("xdk/"):
                name = name[4:]
            elif name.startswith("lib/"):
                name = name[4:]
            elif name.startswith("default/"):
                name = name[8:]

            rows.append([
                name,
                f"{r['percent']:.1f}%",
                fmt_bytes_plain(r["total_code"]),
                f"{r['matched_funcs']}/{r['total_funcs']}",
            ])

        widths = [len(h) for h in headers]
        for row in rows:
            for i, cell in enumerate(row):
                widths[i] = max(widths[i], len(cell))

        def fmt_row(cells, align_right=None):
            if align_right is None:
                align_right = [False] * len(cells)
            parts = []
            for i, cell in enumerate(cells):
                if align_right[i]:
                    parts.append(cell.rjust(widths[i]))
                else:
                    parts.append(cell.ljust(widths[i]))
            return "  " + " | ".join(parts)

        align = [False, True, True, True]
        print(fmt_row(headers))
        print("  " + "-+-".join("-" * w for w in widths))
        for row in rows:
            print(fmt_row(row, align))

    print()


def print_snapshot(report: dict, sort_by: str = "percent", show_all: bool = False):
    """Print current snapshot of all subsystems."""
    agg = aggregate_by_subsystem(report.get("units", []))

    # Build results list with filtering
    results = []
    for sub, stats in agg.items():
        if stats["total_code"] > 0:
            # Filter out excluded prefixes and small subsystems unless --all
            if not show_all:
                if any(sub.startswith(prefix) for prefix in EXCLUDED_PREFIXES):
                    continue
                if stats["total_code"] < DEFAULT_MIN_SIZE:
                    continue

            pct = stats["weighted_fuzzy"] / stats["fuzzy_code"] if stats["fuzzy_code"] > 0 else 0
            results.append({
                "subsystem": sub,
                "total_code": stats["total_code"],
                "percent": pct,
                "matched_funcs": stats["matched_functions"],
                "total_funcs": stats["total_functions"],
            })

    # Sort
    if sort_by == "percent":
        results.sort(key=lambda x: x["percent"], reverse=True)
    elif sort_by == "size":
        results.sort(key=lambda x: x["total_code"], reverse=True)
    elif sort_by == "matched":
        results.sort(key=lambda x: x["percent"] * x["total_code"], reverse=True)
    else:  # name
        results.sort(key=lambda x: x["subsystem"])

    # Overall stats
    measures = report.get("measures", {})
    total_pct = measures.get("fuzzy_match_percent", 0) or 0
    total_code = int(measures.get("total_code", 0) or 0)
    total_funcs_matched = int(measures.get("matched_functions", 0) or 0)
    total_funcs = int(measures.get("total_functions", 0) or 0)

    print()
    print(f"Overall fuzzy: {total_pct:.2f}% ({fmt_bytes_plain(total_code)})")
    print(f"Functions: {total_funcs_matched}/{total_funcs}")
    print()

    headers = ["Subsystem", "Fuzzy %", "Total", "Functions"]
    rows = []
    for r in results:
        rows.append([
            r["subsystem"],
            f"{r['percent']:.2f}%",
            fmt_bytes_plain(r["total_code"]),
            f"{r['matched_funcs']}/{r['total_funcs']}",
        ])

    widths = [len(h) for h in headers]
    for row in rows:
        for i, cell in enumerate(row):
            widths[i] = max(widths[i], len(cell))

    def fmt_row(cells, align_right=None):
        if align_right is None:
            align_right = [False] * len(cells)
        parts = []
        for i, cell in enumerate(cells):
            if align_right[i]:
                parts.append(cell.rjust(widths[i]))
            else:
                parts.append(cell.ljust(widths[i]))
        return "| " + " | ".join(parts) + " |"

    align = [False, True, True, True]
    print(fmt_row(headers))
    print("|" + "|".join("-" * (w + 2) for w in widths) + "|")
    for row in rows:
        print(fmt_row(row, align))
    print()


def main():
    parser = argparse.ArgumentParser(
        description="Compare decomp progress between two report.json files, or show snapshot",
        epilog="""examples:
  %(prog)s --snapshot                           Show all subsystems
  %(prog)s --snapshot --filter 'system/ui/*'    Show only system/ui subsystems
  %(prog)s --snapshot -g '*/char/*' -g '*/anim/*'  Multiple filters
  %(prog)s -g 'system/synth/*' --functions baseline.json current.json
  %(prog)s --overview --filter 'lazer/*'        Overview filtered to game code
""",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    parser.add_argument(
        "baseline",
        nargs="?",
        type=Path,
        help="Path to baseline report.json (or report for --snapshot)",
    )
    parser.add_argument(
        "current",
        nargs="?",
        type=Path,
        help="Path to current report.json",
    )
    parser.add_argument(
        "--overview", "-o",
        action="store_true",
        help="Show complete overview grouped by category (Game/Milo/XDK)",
    )
    parser.add_argument(
        "--snapshot", "-s",
        action="store_true",
        help="Show current snapshot of all subsystems (no comparison)",
    )
    parser.add_argument(
        "--sort",
        choices=["name", "percent", "size", "matched"],
        default="percent",
        help="Sort snapshot by: name, percent (default), size, or matched bytes",
    )
    parser.add_argument(
        "--all", "-a",
        action="store_true",
        help="Show all subsystems (including xdk, lib, tiny ones)",
    )
    parser.add_argument(
        "--detailed",
        action="store_true",
        help="Show detailed per-unit breakdown",
    )
    parser.add_argument(
        "--functions", "-f",
        action="store_true",
        help="Show function-level changes (most useful for finding regressions)",
    )
    parser.add_argument(
        "--regressions", "-r",
        action="store_true",
        help="Only show regressions (negative changes) in all views",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=50,
        help="Max items to show in detailed/function view (default: 50)",
    )
    parser.add_argument(
        "--filter", "-g",
        action="append",
        default=[],
        metavar="PATTERN",
        help="Filter units by glob pattern (e.g. 'system/ui/*', '*/char/*'). Can be repeated.",
    )
    parser.add_argument(
        "--map-file",
        type=Path,
        default=DEFAULT_MAP_FILE,
        help=f"Linker map file for resolving merged symbols (default: {DEFAULT_MAP_FILE})",
    )
    parser.add_argument(
        "--no-merged-resolution",
        action="store_true",
        help="Disable merged symbol resolution (skip map file parsing)",
    )

    args = parser.parse_args()

    # Overview mode - grouped by category
    if args.overview:
        if args.baseline:
            report_path = args.baseline
        else:
            report_path = Path("build/45410914/report.json")

        if not report_path.exists():
            print(f"Error: Report not found: {report_path}")
            print("Run 'ninja' first to generate the report.")
            sys.exit(1)

        report = load_report(report_path)
        if args.filter:
            report["units"] = filter_units(report.get("units", []), args.filter)
        print_overview(report)
        return

    # Snapshot mode - show current state without comparison
    if args.snapshot:
        if args.baseline:
            report_path = args.baseline
        else:
            report_path = Path("build/45410914/report.json")

        if not report_path.exists():
            print(f"Error: Report not found: {report_path}")
            print("Run 'ninja' first to generate the report.")
            sys.exit(1)

        report = load_report(report_path)
        if args.filter:
            report["units"] = filter_units(report.get("units", []), args.filter)
        print_snapshot(report, sort_by=args.sort, show_all=args.all)
        return

    # Comparison mode - need both reports
    if not args.baseline or not args.current:
        parser.error("comparison mode requires both baseline and current reports (or use --snapshot)")

    if not args.baseline.exists():
        print(f"Error: Baseline report not found: {args.baseline}")
        sys.exit(1)
    if not args.current.exists():
        print(f"Error: Current report not found: {args.current}")
        sys.exit(1)

    baseline = load_report(args.baseline)
    current = load_report(args.current)

    if args.filter:
        baseline["units"] = filter_units(baseline.get("units", []), args.filter)
        current["units"] = filter_units(current.get("units", []), args.filter)

    # Set up merged symbol resolver for function-level comparison
    merged_resolver = None
    if not args.no_merged_resolution and args.map_file.exists():
        merged_resolver = MergedSymbolResolver(args.map_file)

    # Always show subsystem summary
    subsystem_results = compare_subsystems(baseline, current)
    if args.regressions:
        subsystem_results = [r for r in subsystem_results if r["diff_pct"] < 0]
    print_subsystem_table(subsystem_results, baseline, current)

    # Optionally show detailed unit breakdown
    if args.detailed:
        unit_results = compare_units(baseline, current)
        if args.regressions:
            unit_results = [r for r in unit_results if r["diff_pct"] < 0]
        print_unit_table(unit_results, args.limit, baseline=baseline, current=current)

    # Optionally show function-level breakdown
    if args.functions:
        func_results = compare_functions(baseline, current, merged_resolver=merged_resolver)
        if args.regressions:
            func_results = [r for r in func_results if r["diff_pct"] < 0]
        print_function_table(func_results, args.limit)


if __name__ == "__main__":
    main()
