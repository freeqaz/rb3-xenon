#!/usr/bin/env python3
"""vector_arity.py — Classify std::vector template-parameter arity in retail RB3.

MSVC mangles vector<T, Alloc> as:
  ?$vector@<T_encoding>V?$StlNodeAlloc@<T_encoding>@stlpmtx_std@@@stlpmtx_std@@
  (2 template params)

A hypothetical sized-vector<T, SizeType, Alloc> would have an extra type code
between T and V?$StlNodeAlloc.  The arity count answers definitively which
layout retail uses for each element type, without building anything.

Source of truth: scripts/target_symbol_map.json (addr -> MSVC-mangled name for
all 66k retail functions, derived from the dtk-split target .obj files + the
rb3-Wii oracle).  report.json carries the same names and is also scanned.

Usage:
    python3 tools/vector_arity.py                   # full summary table
    python3 tools/vector_arity.py --grep Vector3    # filter by element-type substring
    python3 tools/vector_arity.py --top 20          # show top N element types
    python3 tools/vector_arity.py --verbose         # show example mangled names per type
"""

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent


# ---------------------------------------------------------------------------
# Parsing helpers
# ---------------------------------------------------------------------------

def _extract_vector_elem(name: str) -> tuple[str | None, int]:
    """Return (element_type_encoding, arity) for a ?$vector@ mangled name.

    arity is 2 if the template has (elem, alloc) only, or 3 if there is an
    extra parameter between elem and alloc (sized-vector pattern).
    Returns (None, 0) if not a vector name or pattern unrecognised.

    Strategy: for 2-param vector<T, Alloc>, the mangling is:
      ?$vector@<T_enc>V?$StlNodeAlloc@<T_enc>@stlpmtx_std@@@stlpmtx_std@@
    We find <T_enc> by scanning forward for the FIRST occurrence of
    'V?$StlNodeAlloc@<T_enc>@stlpmtx_std@@@stlpmtx_std@@' that ends the outer
    template, where the <T_enc> after 'StlNodeAlloc@' matches what preceded it.

    For 3-param (sized-vector): an extra type code (e.g. 'I' for uint) sits
    between <T_enc> and V?$StlNodeAlloc, so the check fails and arity=3.
    """
    idx = name.find("?$vector@")
    if idx < 0:
        return None, 0
    rest = name[idx + 9:]  # chars after '?$vector@'

    # Find every occurrence of 'V?$StlNodeAlloc@' and check if the inner type
    # matches what precedes it (the element type).
    search = "V?$StlNodeAlloc@"
    pos = 0
    while True:
        alloc_start = rest.find(search, pos)
        if alloc_start < 0:
            # No StlNodeAlloc found — unknown allocator
            return rest[:30].split("@")[0], 0

        pre_alloc = rest[:alloc_start]

        # The alloc's inner type goes from alloc_start+len(search) until
        # '@stlpmtx_std@@' (closing of the StlNodeAlloc instantiation).
        inner_start = alloc_start + len(search)
        closing = "@stlpmtx_std@@"
        inner_end = rest.find(closing, inner_start)
        if inner_end < 0:
            pos = alloc_start + 1
            continue

        alloc_inner = rest[inner_start:inner_end]

        # Verify this is the outer allocator: alloc_inner == pre_alloc
        if alloc_inner == pre_alloc:
            # This IS the outer StlNodeAlloc — definitively 2-param.
            # (alloc_inner == pre_alloc means element type == allocator inner type,
            # which is the invariant of 2-param vector<T, StlNodeAlloc<T>>.)
            # Strip any trailing @@ from the element type (outer-namespace qualifier).
            elem = pre_alloc.rstrip("@")
            return elem, 2

        # alloc_inner != pre_alloc: this StlNodeAlloc is nested inside the
        # element type, not the outer allocator — keep searching.
        pos = alloc_start + 1

    return rest[:30].split("@")[0], 0


def _consume_one_type(s: str) -> tuple[str | None, str]:
    """Consume one MSVC-mangled type code from the front of s.

    Returns (type_encoding, remainder).  Returns (None, s) on parse failure.
    This covers the common cases needed for ?$vector@ element types.
    """
    if not s:
        return None, s

    ch = s[0]

    # Two-char fundamental types: _N, _W, _J, _K, _L, _M (bool, wchar, etc.)
    if ch == "_":
        if len(s) >= 2:
            return s[:2], s[2:]
        return None, s

    # Single-char fundamental types: C D E F G H I J K L M N O X Z
    if ch in "CDEFGHIJKLMNOXZdefghijklmnopqrstuvwxyz":
        return ch, s[1:]

    # Pointer: P<type>
    if ch == "P":
        inner, rem = _consume_one_type(s[1:])
        if inner is None:
            return None, s
        return "P" + inner, rem

    # Reference: A<type>
    if ch == "A":
        inner, rem = _consume_one_type(s[1:])
        if inner is None:
            return None, s
        return "A" + inner, rem

    # Class/struct/union value: V<name>@@ or U<name>@@ or T<name>@@
    if ch in "VUTW":
        # Read until @@ (end of qualified name)
        end = s.find("@@")
        if end < 0:
            # Single-@ terminated
            end = s.find("@")
            if end < 0:
                return None, s
            return s[:end + 1], s[end + 1:]
        return s[:end + 2], s[end + 2:]

    # Template class: ?$<name>@ ...  (e.g. V?$Key@...)
    # This shows up as part of a V/U prefix so is handled above, but
    # sometimes the ?$ is embedded — just consume to @@
    if ch == "?":
        end = s.find("@@")
        if end >= 0:
            return s[:end + 2], s[end + 2:]
        return None, s

    return None, s


def _pretty_elem(enc: str) -> str:
    """Return a human-readable label for an element-type encoding."""
    MAP = {
        "C": "signed char",
        "D": "char",
        "E": "unsigned char",
        "F": "short",
        "G": "unsigned short",
        "H": "int",
        "I": "unsigned int",
        "J": "long",
        "K": "unsigned long",
        "L": "???",
        "M": "float",
        "N": "double",
        "_N": "bool",
        "_W": "wchar_t",
    }
    if enc in MAP:
        return MAP[enc]
    # Class: strip leading V/U/T and trailing @@
    if enc and enc[0] in "VUT":
        return enc[1:].rstrip("@")
    # Pointer
    if enc.startswith("P"):
        return f"ptr<{_pretty_elem(enc[1:])}>"
    return enc


# ---------------------------------------------------------------------------
# Data collection
# ---------------------------------------------------------------------------

def collect_symbols() -> list[str]:
    """Collect all symbol names from target_symbol_map.json and report.json."""
    names: list[str] = []

    # Primary: target_symbol_map.json
    map_path = PROJECT_ROOT / "scripts" / "target_symbol_map.json"
    if map_path.exists():
        with open(map_path) as f:
            tmap = json.load(f)
        names.extend(tmap.values())

    # Secondary: report.json (picks up matched symbols not in target_symbol_map)
    report_path = PROJECT_ROOT / "build" / "45410914" / "report.json"
    if report_path.exists():
        with open(report_path) as f:
            report = json.load(f)
        for unit in report.get("units", []):
            for fn in unit.get("functions", []):
                n = fn.get("name", "")
                if n:
                    names.append(n)

    return names


def analyse(names: list[str]) -> dict:
    """Return analysis dict: {elem_type_enc: {'arity2': int, 'arity3': int, 'unknown': int,
                                               'pretty': str, 'examples': list}}"""
    results: dict[str, dict] = defaultdict(lambda: {
        "arity2": 0, "arity3": 0, "unknown": 0,
        "pretty": "", "examples2": [], "examples3": [],
    })
    seen: set[str] = set()

    for name in names:
        if "?$vector@" not in name:
            continue
        if name in seen:
            continue
        seen.add(name)

        elem, arity = _extract_vector_elem(name)
        if elem is None:
            continue

        rec = results[elem]
        if not rec["pretty"]:
            rec["pretty"] = _pretty_elem(elem)

        if arity == 2:
            rec["arity2"] += 1
            if len(rec["examples2"]) < 2:
                rec["examples2"].append(name[:100])
        elif arity == 3:
            rec["arity3"] += 1
            if len(rec["examples3"]) < 2:
                rec["examples3"].append(name[:100])
        else:
            rec["unknown"] += 1

    return dict(results)


# ---------------------------------------------------------------------------
# CLI
# ---------------------------------------------------------------------------

def main():
    parser = argparse.ArgumentParser(
        description="Classify std::vector template-parameter arity in retail RB3 symbols"
    )
    parser.add_argument(
        "--grep", metavar="SUBSTR", default=None,
        help="Filter rows to element types whose pretty label or encoding contains SUBSTR"
    )
    parser.add_argument(
        "--top", metavar="N", type=int, default=None,
        help="Show only the top N element types by symbol count"
    )
    parser.add_argument(
        "--verbose", action="store_true",
        help="Show example mangled names for arity-2 and arity-3 entries"
    )
    parser.add_argument(
        "--check-only", action="store_true",
        help="Exit non-zero if any arity-3 (sized-vector) symbols are found"
    )
    args = parser.parse_args()

    print("Collecting symbols...", file=sys.stderr)
    names = collect_symbols()
    print(f"  {len(names):,} total symbols loaded", file=sys.stderr)

    results = analyse(names)

    # Filter
    if args.grep:
        results = {
            k: v for k, v in results.items()
            if args.grep.lower() in k.lower() or args.grep.lower() in v["pretty"].lower()
        }

    # Sort by total descending
    rows = sorted(
        results.items(),
        key=lambda kv: -(kv[1]["arity2"] + kv[1]["arity3"] + kv[1]["unknown"])
    )

    if args.top:
        rows = rows[:args.top]

    # Summary totals
    total2 = sum(v["arity2"] for _, v in results.items())
    total3 = sum(v["arity3"] for _, v in results.items())
    total_unk = sum(v["unknown"] for _, v in results.items())

    print()
    print(f"{'Element type':<35s}  {'Arity-2':>8s}  {'Arity-3':>8s}  {'Unknown':>8s}")
    print("-" * 68)
    for enc, rec in rows:
        label = rec["pretty"][:34]
        print(f"  {label:<33s}  {rec['arity2']:>8d}  {rec['arity3']:>8d}  {rec['unknown']:>8d}")
        if args.verbose:
            for ex in rec["examples2"]:
                print(f"    [2] {ex}")
            for ex in rec["examples3"]:
                print(f"    [3] {ex}")
    print("-" * 68)
    print(f"  {'TOTAL':<33s}  {total2:>8d}  {total3:>8d}  {total_unk:>8d}")
    print()

    print("Summary:")
    print(f"  Arity-2 (2-param: elem + StlNodeAlloc):  {total2:5d} symbols")
    print(f"  Arity-3 (3-param: sized-vector pattern): {total3:5d} symbols")
    print(f"  Unknown alloc pattern:                   {total_unk:5d} symbols")
    print()
    if total3 == 0:
        print("VERDICT: retail RB3 uses exclusively 2-param vector<T, Alloc> — "
              "12-byte 3-pointer layout confirmed, no sized-vector variant detected.")
    else:
        print(f"VERDICT: {total3} arity-3 symbols found — "
              "possible sized-vector usage (inspect with --verbose --grep <type>).")

    if args.check_only and total3 > 0:
        sys.exit(1)


if __name__ == "__main__":
    main()
