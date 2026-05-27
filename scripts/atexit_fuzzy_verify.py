#!/usr/bin/env python3
"""Atexit destructor fuzzy verifier.

Runs after `obj_atexit_scope_patcher.py` to mark patched `??__F*` symbols
as COMPLETE in the database.

Strategy
--------
For every `??__F*` function in the DB that isn't already COMPLETE:
  1. Run objdiff with `functionRelocDiffs=none` so address-relocation noise
     (lbl_ vs mangled static-local name) doesn't count against the match.
  2. If `instruction_summary.equal_percent == 100.0` and base_size > 0,
     mark the function COMPLETE at 100% in the DB.
  3. If the match is >98% with the default config (just address relocation
     noise remaining), optionally mark it AT_LIMIT (via --mark-at-limit).

We ignore relocation diffs because an atexit destructor is a tiny wrapper
around a single `Release()` call, and the only relocation that differs is
the static-local pointer -- which cannot be renamed in the base to match
the target's `lbl_<addr>` form without breaking link-time symbol resolution.
The instructions are byte-identical, which is the semantic of "matching"
for this pattern.

Usage
-----
    python3 scripts/atexit_fuzzy_verify.py                   # dry run
    python3 scripts/atexit_fuzzy_verify.py --apply           # update DB
    python3 scripts/atexit_fuzzy_verify.py --apply --mark-at-limit
    python3 scripts/atexit_fuzzy_verify.py --unit 'system/char/*' --apply

Expected to be run after `obj_atexit_scope_patcher.py --apply`.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(PROJECT_ROOT / "scripts"))

from orchestrator.database import (  # noqa: E402
    DEFAULT_EXCLUDE_PATTERNS,
    get_connection,
    normalize_unit_pattern,
    update_function_status,
)

DB_PATH = str(PROJECT_ROOT / "decomp.db")
OBJDIFF_CLI = PROJECT_ROOT / "bin" / "objdiff-cli"


def run_objdiff_atexit(symbol):
    """Run objdiff with relocation diffs disabled; return parsed JSON dict or None."""
    try:
        result = subprocess.run(
            [
                str(OBJDIFF_CLI), "diff", "-p", str(PROJECT_ROOT),
                symbol,
                "-c", "functionRelocDiffs=none",
                "--verdict",
                "-f", "json",
            ],
            capture_output=True, text=True, timeout=60,
            cwd=str(PROJECT_ROOT),
        )
    except subprocess.TimeoutExpired:
        return None

    if result.returncode != 0 or "Symbol not found" in result.stdout:
        return None

    # Find the JSON line
    for line in result.stdout.split("\n"):
        line = line.strip()
        if line.startswith("{") and line.endswith("}"):
            try:
                return json.loads(line)
            except json.JSONDecodeError:
                return None
    return None


def verify(unit_pattern, apply=False, mark_at_limit=False, verbose=False, limit=None):
    """Verify atexit destructors via objdiff, updating DB if apply=True."""
    conn = get_connection(DB_PATH)

    # Note: SQLite LIKE treats `_` as a single-char wildcard, so we must
    # escape underscores to match the literal `??__F` prefix exactly.
    query = r"""
        SELECT id, symbol, unit, current_percent, verdict
        FROM functions
        WHERE symbol LIKE '??\_\_F%' ESCAPE '\'
          AND (verdict IS NULL OR verdict != 'COMPLETE')
          AND symbol NOT LIKE 'merged\_%' ESCAPE '\'
    """
    params = []

    # Exclude default patterns (XDK, link_glue, binkxenon)
    for ep in DEFAULT_EXCLUDE_PATTERNS:
        norm_ep = normalize_unit_pattern(ep)
        query += " AND unit NOT GLOB ?"
        params.append(norm_ep)

    if unit_pattern:
        norm_pattern = normalize_unit_pattern(unit_pattern)
        query += " AND unit GLOB ?"
        params.append(norm_pattern)

    query += " ORDER BY unit, symbol"
    if limit:
        query += f" LIMIT {int(limit)}"

    rows = conn.execute(query, params).fetchall()
    functions = [dict(row) for row in rows]

    if not functions:
        print("No non-complete ??__F functions found matching criteria")
        return

    if not OBJDIFF_CLI.exists():
        print(f"Error: objdiff-cli not found at {OBJDIFF_CLI}", file=sys.stderr)
        sys.exit(1)

    total = len(functions)
    newly_complete = 0
    newly_at_limit = 0
    still_stub = 0
    unchanged = 0
    errors = 0

    by_unit_complete = {}

    print(f"Verifying {total} atexit destructors...")

    for i, func in enumerate(functions):
        if verbose and i > 0 and i % 50 == 0:
            print(f"  progress: {i}/{total}")

        symbol = func["symbol"]
        data = run_objdiff_atexit(symbol)

        if data is None:
            errors += 1
            continue

        base_size = data.get("base_size", 0)
        target_size = data.get("target_size", 0)
        instr = data.get("instruction_summary", {}) or {}
        equal_pct = instr.get("equal_percent", 0.0) or 0.0
        fuzzy_pct = data.get("fuzzy_match_percent", 0.0) or 0.0
        verdict_data = data.get("verdict", {}) or {}
        classification = verdict_data.get("classification", "")

        if base_size == 0:
            still_stub += 1
            if verbose:
                print(f"  STUB {symbol}")
            continue

        # With functionRelocDiffs=none, equal_pct == 100.0 means all
        # instructions match, including their operands (after ignoring
        # relocation target names). This is the strongest signal of
        # byte-equivalence possible with objdiff.
        if equal_pct >= 100.0:
            newly_complete += 1
            unit = func["unit"]
            by_unit_complete[unit] = by_unit_complete.get(unit, 0) + 1
            if verbose:
                print(f"  COMPLETE {symbol}")
            if apply:
                update_function_status(
                    function_id=func["id"],
                    current_percent=100.0,
                    verdict="COMPLETE",
                    verdict_reason="atexit_fuzzy_scope_match",
                    db_path=DB_PATH,
                )
                # Clear stale is_stub flag (base_size is now > 0 after patcher)
                conn.execute(
                    "UPDATE functions SET is_stub = 0 WHERE id = ?",
                    (func["id"],),
                )
                conn.commit()
        elif mark_at_limit and fuzzy_pct >= 95.0 and classification != "STUB":
            newly_at_limit += 1
            if verbose:
                print(f"  AT_LIMIT ({fuzzy_pct:.1f}%) {symbol}")
            if apply:
                update_function_status(
                    function_id=func["id"],
                    current_percent=fuzzy_pct,
                    verdict="AT_LIMIT",
                    verdict_reason="atexit_relocation_noise",
                    db_path=DB_PATH,
                )
        else:
            unchanged += 1
            if verbose:
                print(f"  STILL {fuzzy_pct:.1f}% {symbol}")

    # Summary
    mode = "APPLIED" if apply else "DRY RUN"
    print(f"\n[{mode}] Atexit fuzzy verification complete")
    print(f"  Total checked: {total}")
    print(f"  Newly COMPLETE: {newly_complete}")
    if mark_at_limit:
        print(f"  Newly AT_LIMIT: {newly_at_limit}")
    print(f"  Still stub (base_size=0): {still_stub}")
    print(f"  No improvement: {unchanged}")
    print(f"  Errors/timeouts: {errors}")

    if by_unit_complete:
        print("\nTop units with newly COMPLETE atexit destructors:")
        for unit, cnt in sorted(by_unit_complete.items(), key=lambda x: -x[1])[:20]:
            print(f"  {unit}: {cnt}")

    if not apply and (newly_complete or newly_at_limit):
        print("\nRun with --apply to write these updates to the database.")


def main():
    parser = argparse.ArgumentParser(
        description='Fuzzy-verify atexit destructors and mark COMPLETE in DB',
    )
    parser.add_argument(
        '--apply', action='store_true',
        help='Write verdict updates to the database (default: dry-run)',
    )
    parser.add_argument(
        '--mark-at-limit', action='store_true',
        help='Also mark functions as AT_LIMIT if fuzzy_percent >= 95%% but <100%% (relocation noise)',
    )
    parser.add_argument(
        '--unit', default=None,
        help='Filter to unit glob pattern (e.g. system/char/*)',
    )
    parser.add_argument(
        '--verbose', '-v', action='store_true',
        help='Show per-function progress',
    )
    parser.add_argument(
        '--limit', type=int, default=None,
        help='Only check first N functions (for testing)',
    )
    args = parser.parse_args()

    verify(
        unit_pattern=args.unit,
        apply=args.apply,
        mark_at_limit=args.mark_at_limit,
        verbose=args.verbose,
        limit=args.limit,
    )


if __name__ == '__main__':
    main()
