#!/usr/bin/env python3
"""Batch-check all untracked functions in a unit.

Runs objdiff on each, auto-reports 100% matches as COMPLETE.
Returns summary with counts and partial-match details.

Usage:
    python3 scripts/batch_check.py 'system/char/*'
    python3 scripts/batch_check.py 'system/rndobj/Text' --dry-run
    python3 scripts/batch_check.py 'system/*' --skip-boilerplate
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

# Add project root to path
PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from orchestrator.database import (
    get_connection,
    normalize_unit_pattern,
    update_function_status,
    BOILERPLATE_SYMBOL_PREFIXES,
    DEFAULT_EXCLUDE_PATTERNS,
)

try:
    from orchestrator.mcp_server import _demangle_itanium_to_qualified
except ImportError:
    # rb3-xenon uses MSVC mangling, not Itanium — no demangling needed for symbol lookup
    def _demangle_itanium_to_qualified(symbol: str):  # type: ignore[misc]
        return None

DB_PATH = str(PROJECT_ROOT / "decomp.db")
OBJDIFF_CLI = PROJECT_ROOT / "bin" / "objdiff-cli"


def batch_check(unit_pattern: str, dry_run: bool = False, skip_boilerplate: bool = False) -> str:
    """Run batch check and return formatted results."""
    conn = get_connection(DB_PATH)
    norm_pattern = normalize_unit_pattern(unit_pattern)

    query = """
        SELECT id, symbol, demangled, unit, current_percent
        FROM functions
        WHERE unit GLOB ?
          AND (verdict IS NULL OR verdict NOT IN ('COMPLETE', 'AT_LIMIT'))
          AND symbol NOT LIKE 'merged_%'
    """
    params: list = [norm_pattern]

    for ep in DEFAULT_EXCLUDE_PATTERNS:
        norm_ep = normalize_unit_pattern(ep)
        query += " AND unit NOT GLOB ?"
        params.append(norm_ep)

    if skip_boilerplate:
        for prefix in BOILERPLATE_SYMBOL_PREFIXES:
            query += f" AND symbol NOT LIKE '{prefix}%'"

    rows = conn.execute(query, params).fetchall()
    functions = [dict(row) for row in rows]

    if not functions:
        return f"No unchecked functions found for pattern: {unit_pattern}"

    if not OBJDIFF_CLI.exists():
        return f"Error: objdiff-cli not found at {OBJDIFF_CLI}"

    checked = 0
    newly_complete = 0
    unimplemented = 0
    partial = []
    failed = []
    errors = []

    for func in functions:
        symbol = func["symbol"]

        if symbol.startswith("merged_"):
            continue

        lookup_symbol = symbol
        demangled = _demangle_itanium_to_qualified(symbol)
        if demangled is not None:
            lookup_symbol = demangled

        try:
            result = subprocess.run(
                [str(OBJDIFF_CLI), "diff", "-p", str(PROJECT_ROOT),
                 lookup_symbol, "--build", "--verdict", "-f", "json"],
                capture_output=True, text=True, timeout=90,
                cwd=str(PROJECT_ROOT),
            )

            if result.returncode != 0 or "Symbol not found" in result.stdout:
                failed.append(symbol)
                continue

            stdout = result.stdout
            json_start = stdout.find("{")
            if json_start > 0:
                stdout = stdout[json_start:]

            data = json.loads(stdout)
            checked += 1

            instr_summary = data.get("instruction_summary", {})
            equal_pct = instr_summary.get("equal_percent")
            match_pct = data.get("fuzzy_match_percent")
            if equal_pct is not None:
                match_pct = equal_pct
            elif match_pct is None:
                match_pct = 0

            verdict_data = data.get("verdict", {})
            classification = verdict_data.get("classification", "")

            base_size = data.get("base_size", 0)
            target_size = data.get("target_size", 0)
            diff_score = data.get("diff_score", {})
            max_score = diff_score.get("max_score", 0) if diff_score else 0
            is_stub = (classification == "STUB" or (base_size == 0 and target_size > 0)) and max_score == 0

            if is_stub:
                unimplemented += 1
                if not dry_run:
                    conn.execute(
                        "UPDATE functions SET is_stub = 1, updated_at = CURRENT_TIMESTAMP WHERE id = ?",
                        (func["id"],),
                    )
                    conn.commit()
            elif match_pct == 100.0 or classification == "COMPLETE":
                newly_complete += 1
                if not dry_run:
                    update_function_status(
                        function_id=func["id"],
                        current_percent=100.0,
                        verdict="COMPLETE",
                        db_path=DB_PATH,
                    )
            elif match_pct > 0:
                partial.append({
                    "symbol": symbol,
                    "demangled": func.get("demangled", ""),
                    "percent": round(match_pct, 1),
                })
                if not dry_run:
                    update_function_status(
                        function_id=func["id"],
                        current_percent=match_pct,
                        db_path=DB_PATH,
                    )

        except subprocess.TimeoutExpired:
            errors.append(f"{symbol}: timeout")
        except json.JSONDecodeError:
            errors.append(f"{symbol}: invalid JSON output")
        except Exception as e:
            errors.append(f"{symbol}: {e}")

    # Format summary
    mode = " (DRY RUN)" if dry_run else ""
    output = f"## Batch Check Results{mode}\n\n"
    output += f"**Pattern:** `{unit_pattern}`\n"
    output += f"**Checked:** {checked} | **Newly COMPLETE:** {newly_complete} | **Partial:** {len(partial)} | **Unimplemented:** {unimplemented} | **Failed:** {len(failed)}\n"

    if partial:
        output += f"\n### Partial Matches ({len(partial)})\n\n"
        partial.sort(key=lambda x: x["percent"], reverse=True)
        for p in partial:
            output += f"- `{p['symbol']}` ({p['demangled']}) — {p['percent']}%\n"

    if failed and len(failed) <= 20:
        output += f"\n### Not Found ({len(failed)})\n\n"
        for f in failed:
            output += f"- `{f}`\n"
    elif failed:
        output += f"\n### Not Found: {len(failed)} symbols (too many to list)\n"

    if errors:
        output += f"\n### Errors ({len(errors)})\n\n"
        for e in errors[:10]:
            output += f"- {e}\n"

    return output


def main():
    parser = argparse.ArgumentParser(description="Batch-check functions in a unit")
    parser.add_argument("unit_pattern", help="Unit glob pattern (e.g., 'system/char/*')")
    parser.add_argument("--dry-run", action="store_true", help="Check but don't update DB")
    parser.add_argument("--skip-boilerplate", action="store_true", help="Skip atexit/MakeString/thunks")
    args = parser.parse_args()

    result = batch_check(args.unit_pattern, dry_run=args.dry_run, skip_boilerplate=args.skip_boilerplate)
    print(result)


if __name__ == "__main__":
    main()
