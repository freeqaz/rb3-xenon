#!/usr/bin/env python3
"""Get decomp progress summary.

Returns total/complete/at_limit counts, percentages, pattern breakdown,
and top units with remaining work.

Usage:
    python3 scripts/get_progress.py
"""

import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from orchestrator.database import get_connection, get_stats

DB_PATH = str(PROJECT_ROOT / "decomp.db")


def get_progress() -> str:
    conn = get_connection(DB_PATH)

    stats = get_stats(DB_PATH)
    total = stats["total_functions"]

    excluded = conn.execute(
        "SELECT COUNT(*) FROM functions WHERE unit LIKE '%xdk%'"
    ).fetchone()[0]
    non_excluded = total - excluded

    # Count only non-xdk verdicts to avoid inflating Done when xdk functions
    # accidentally get classified
    complete = conn.execute(
        "SELECT COUNT(*) FROM functions WHERE verdict = 'COMPLETE' AND unit NOT LIKE '%xdk%'"
    ).fetchone()[0]
    at_limit = conn.execute(
        "SELECT COUNT(*) FROM functions WHERE verdict = 'AT_LIMIT' AND unit NOT LIKE '%xdk%'"
    ).fetchone()[0]
    remaining = non_excluded - complete - at_limit

    complete_pct = (complete / non_excluded * 100) if non_excluded else 0
    done_pct = ((complete + at_limit) / non_excluded * 100) if non_excluded else 0

    output = "## Decomp Progress\n\n"
    output += "| Metric | Count | % of non-excluded |\n"
    output += "|--------|------:|---:|\n"
    output += f"| Total functions | {total:,} | - |\n"
    output += f"| Excluded (SDK) | {excluded:,} | - |\n"
    output += f"| Non-excluded | {non_excluded:,} | 100% |\n"
    output += f"| COMPLETE | {complete:,} | {complete_pct:.1f}% |\n"
    output += f"| AT_LIMIT | {at_limit:,} | {at_limit / non_excluded * 100:.1f}% |\n"
    output += f"| Remaining | {remaining:,} | {remaining / non_excluded * 100:.1f}% |\n"
    output += f"| **Done (COMPLETE + AT_LIMIT)** | **{complete + at_limit:,}** | **{done_pct:.1f}%** |\n"

    # Pattern counts
    pattern_keys = [
        ("pattern_merged", "Linker merged"),
        ("pattern_bool_mask", "Bool mask"),
        ("pattern_makestring_mismatch", "MakeString mismatch"),
        ("pattern_address_relocation", "Address relocation"),
        ("pattern_boolean_negation", "Boolean negation"),
        ("pattern_float_precision", "Float precision"),
        ("pattern_fsel_ternary", "fsel ternary"),
        ("pattern_float_to_int_to_float", "Float-int-float"),
        ("pattern_register_swap", "Register swap"),
        ("pattern_comparison_style", "Comparison style"),
        ("pattern_control_flow", "Control flow"),
        ("pattern_commutative_op_order", "Commutative op order"),
        ("pattern_offset_swap", "Offset swap"),
        ("pattern_anonymous_namespace_hash", "Anon namespace hash"),
        ("pattern_static_guard_counter", "Static guard counter"),
        ("pattern_dynamic_cast_mismatch", "dynamic_cast mismatch"),
        ("pattern_dead_store_elimination", "Dead store elimination"),
        ("pattern_prologue_mismatch", "Prologue mismatch"),
        ("pattern_alloca_mismatch", "alloca mismatch"),
        ("pattern_scope_counter_mismatch", "Scope counter mismatch"),
    ]
    has_patterns = any(stats.get(k, 0) > 0 for k, _ in pattern_keys)
    if has_patterns:
        output += "\n### Detected Patterns\n\n"
        output += "| Pattern | Count |\n"
        output += "|---------|------:|\n"
        for key, label in pattern_keys:
            count = stats.get(key, 0)
            if count > 0:
                output += f"| {label} | {count:,} |\n"

    # Top units with remaining work
    rows = conn.execute("""
        SELECT unit, COUNT(*) as cnt
        FROM functions
        WHERE verdict IS NULL
          AND unit NOT LIKE '%xdk%'
          AND symbol NOT LIKE 'merged_%'
          AND demangled NOT LIKE '%stlpmtx_std::%'
        GROUP BY unit
        ORDER BY cnt DESC
        LIMIT 15
    """).fetchall()

    if rows:
        output += "\n### Top Units with Remaining Work\n\n"
        output += "| Unit | Remaining |\n"
        output += "|------|----------:|\n"
        for row in rows:
            unit = row["unit"].replace("default/", "")
            output += f"| {unit} | {row['cnt']} |\n"

    return output


if __name__ == "__main__":
    print(get_progress())
