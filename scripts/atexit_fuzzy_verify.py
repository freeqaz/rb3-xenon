#!/usr/bin/env python3
"""Atexit destructor verifier.

Runs after `obj_atexit_scope_patcher.py` to mark patched `??__F*` symbols
as COMPLETE in the database.

★ THE RULER (lane ATEXIT-RULER, 2026-09-11) — read before changing the gate
────────────────────────────────────────────────────────────────────────────
This tool used to drive objdiff with a HARDCODED `-c functionRelocDiffs=none`
and write `verdict=COMPLETE` when `instruction_summary.equal_percent >= 100`.
It was ported from DC3 on 2026-05-27 (`d97d0985`) and never revisited when the
project shipped `functionRelocDiffs=name_check` as the graded ruler on
2026-08-12 (`d04c83df`). `verdict=COMPLETE` CLOSES a row, so a permissive
ruler was able to close rows the grader scores below 100.

⚠ The old docstring justified `none` like this: "an atexit destructor is a tiny
wrapper around a single Release() call, and the only relocation that differs is
the static-local pointer -- which cannot be renamed in the base to match the
target's `lbl_<addr>` form." **THAT PREMISE IS FALSE, measured on this tree.**

  * objdiff ALREADY forgives a genuine `lbl_<addr>` target under `name_check`:
    `is_placeholder_symbol_name` (objdiff-core `diff/code.rs:998`) covers
    `fn_`/`lbl_`/`jumptable_`/`code_`/`data_`/`bss_`/`rdata_`/`vftable_`, and
    `reloc_eq` returns true on a placeholder LEFT (= TARGET) name regardless of
    what we spell. So if the premise held, `none` would buy nothing at all.
  * It does not hold. Measured over all 56 `??__F` rows in report.json, the two
    rulers disagree on 2, and on `??__FsFrames@@YAXXZ` (SkeletonClip, 28 B) the
    disagreement manufactures a false verdict:

        ruler `none`       instruction_summary.equal_percent = 100.00  ⇒ COMPLETE
        ruler `name_check` instruction_summary.equal_percent =  71.43
                           fuzzy_match_percent               =  98.571  ⇒ NOT complete

    The two charged sites are `diff_arg`, and they are not naming noise:

        target: lis  r11, ??1?$ObjDirPtr@VObjectDir@@@@UAA@XZ@h
        base:   lis  r11, ??1?$vector@URecordedFrame@@...@XZ@h

    Retail registers the destructor of `ObjDirPtr<ObjectDir>`; we register the
    destructor of `vector<RecordedFrame>`. A REAL, NAMED, DIFFERENT callee --
    exactly the class `none` is structurally blind to ("a wrong callee and a
    folded callee both read as equal").

⇒ The ruler is now RESOLVED AT RUNTIME from `report.json`'s
  `provenance.diff_config` via `scripts/analysis/ruler.py`. It is deliberately
  NOT a second hardcoded constant -- a constant is what rotted the first time,
  on a silent schedule, with every test still passing.

⇒ And the gate REFUSES to write when the ruler is not authoritative. This is
  not hypothetical: `objdiff.json` is gitignored, so on a tree carrying neither
  it nor a `report.json`, `resolve_ruler` falls back to `report generate`'s base
  config -- whose `functionRelocDiffs` is `none`. Without this refusal, a fresh
  checkout would silently restore the exact defect this rewrite removes.

The gate
────────
COMPLETE requires ALL of, on the GRADED ruler:
  * `fuzzy_match_percent >= 100` -- the key `matched_code` is computed on, and
    the stricter of the grader's two rulers (`mpn >= fuzzy` always).
  * `instruction_summary.equal_percent >= 100` -- belt-and-braces against the
    converse hazard CLAUDE.md records (an "all instructions equal" reading
    coexisting with argument-level charges). Measured to cost nothing here:
    all 3 genuinely-complete rows read 100 on both.
  * `base_size > 0` -- unchanged; a 0-size base is a stub, not a match.

A PERMISSIVE RULER MUST NEVER BE ABLE TO CLOSE A ROW. The `none` reading is
still taken, but only as a CONTROL: rows it would have promoted and the graded
ruler withholds are counted and reported, so this defect stays visible instead
of being silently re-introduced.

⚠ REDUNDANCY, recorded so it can be retired deliberately: every row this tool
can legitimately promote is already promoted by
`scripts/sync_match_percent.py --promote`, which keys on `report.json`'s own
`fuzzy_match_percent == 100` and is therefore graded by construction. The
patcher this tool chases runs as a wired post-compile ninja step, so report.json
already reflects the patched objects. This tool's only unique capability was the
false promotion. Consider deleting it; it is kept for `--mark-at-limit` and
because it is a documented write seam.

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

from analysis.ruler import RULER_GRADED, RULER_NONE, resolve_ruler  # noqa: E402
from orchestrator.database import (  # noqa: E402
    DEFAULT_EXCLUDE_PATTERNS,
    get_connection,
    normalize_unit_pattern,
    unworkable_verdict_clause,
    update_function_status,
)

DB_PATH = str(PROJECT_ROOT / "decomp.db")
OBJDIFF_CLI = PROJECT_ROOT / "bin" / "objdiff-cli"


def run_objdiff_atexit(symbol, config_args):
    """Run objdiff on `symbol` with `config_args`; return parsed JSON or None.

    `config_args` is a flat `-c key=value` argv supplied by the caller from
    `scripts/analysis/ruler.py`. It is NOT defaulted: a default here is the
    hardcoded constant this module was rewritten to remove.
    """
    try:
        result = subprocess.run(
            [
                str(OBJDIFF_CLI), "diff", "-p", str(PROJECT_ROOT),
                symbol,
                *config_args,
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


def _equal_percent(data):
    instr = data.get("instruction_summary", {}) or {}
    return float(instr.get("equal_percent", 0.0) or 0.0)


def _fuzzy(data):
    return float(data.get("fuzzy_match_percent", 0.0) or 0.0)


def verify(unit_pattern, apply=False, mark_at_limit=False, verbose=False,
           limit=None, skip_control=False):
    """Verify atexit destructors via objdiff, updating DB if apply=True."""
    # ── Resolve the GRADED ruler from the grading run's own artifact ─────────
    ruler = resolve_ruler(PROJECT_ROOT, RULER_GRADED)
    print(ruler.banner())
    print()

    if apply and not ruler.authoritative:
        print(
            "REFUSING to --apply: the ruler was not read from a grading run.\n"
            "  `verdict=COMPLETE` CLOSES a row, and the fallback ruler is\n"
            "  `functionRelocDiffs=none`, which is blind to relocation-name\n"
            "  divergence (a wrong callee reads as equal). Generate a report\n"
            "  first:  ./tools/ninja-locked\n"
            "  Dry-run (no --apply) is still available and safe.",
            file=sys.stderr,
        )
        sys.exit(2)

    control = None if skip_control else resolve_ruler(PROJECT_ROOT, RULER_NONE)

    conn = get_connection(DB_PATH)

    # Note: SQLite LIKE treats `_` as a single-char wildcard, so we must
    # escape underscores to match the literal `??__F` prefix exactly.
    # `verdict != 'COMPLETE'` alone would ADMIT an IDENTITY_UNESTABLISHED row,
    # and with --mark-at-limit this loop would then certify a floor on a body
    # not established to be this function -- the exact mis-certification of
    # 2026-08-13. Excluded explicitly.
    query = r"""
        SELECT id, symbol, unit, current_percent, verdict
        FROM functions
        WHERE symbol LIKE '??\_\_F%' ESCAPE '\'
          AND (verdict IS NULL OR verdict != 'COMPLETE')
          AND symbol NOT LIKE 'merged\_%' ESCAPE '\'
    """ + unworkable_verdict_clause()
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
    withheld = []          # permissive ruler would promote, graded does not

    by_unit_complete = {}

    print(f"Verifying {total} atexit destructors...")

    for i, func in enumerate(functions):
        if verbose and i > 0 and i % 50 == 0:
            print(f"  progress: {i}/{total}")

        symbol = func["symbol"]
        data = run_objdiff_atexit(symbol, ruler.args)

        if data is None:
            errors += 1
            continue

        base_size = data.get("base_size", 0)
        equal_pct = _equal_percent(data)
        fuzzy_pct = _fuzzy(data)
        verdict_data = data.get("verdict", {}) or {}
        classification = verdict_data.get("classification", "")

        if base_size == 0:
            still_stub += 1
            if verbose:
                print(f"  STUB {symbol}")
            continue

        # ── The gate. Graded ruler, BOTH measures, non-zero base. ────────────
        is_complete = fuzzy_pct >= 100.0 and equal_pct >= 100.0

        # ── CONTROL: would the old permissive ruler have promoted this? ──────
        # Purely diagnostic -- it can never promote, only report. This is what
        # keeps the defect visible if anyone ever reaches for `none` again.
        if control is not None and not is_complete:
            cdata = run_objdiff_atexit(symbol, control.args)
            if cdata is not None and int(cdata.get("base_size", 0) or 0) > 0 \
                    and _equal_percent(cdata) >= 100.0:
                withheld.append((symbol, fuzzy_pct, equal_pct))

        if is_complete:
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
            # AT_LIMIT certifies a FLOOR, i.e. "our source cannot do better".
            # That is a closing verdict too, so it is priced on the graded
            # ruler for the same reason COMPLETE is.
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
    print(f"\n[{mode}] Atexit verification complete")
    print(f"  Ruler:            functionRelocDiffs={ruler.reloc_mode} (graded)")
    print(f"  Total checked: {total}")
    print(f"  Newly COMPLETE: {newly_complete}")
    if mark_at_limit:
        print(f"  Newly AT_LIMIT: {newly_at_limit}")
    print(f"  Still stub (base_size=0): {still_stub}")
    print(f"  No improvement: {unchanged}")
    print(f"  Errors/timeouts: {errors}")

    if withheld:
        print(f"\n⚠ WITHHELD: {len(withheld)} row(s) would have been marked "
              f"COMPLETE by the permissive `functionRelocDiffs=none` ruler "
              f"this tool used before 2026-09-11, and are NOT complete on the "
              f"graded ruler. `none` is blind to relocation-NAME divergence, "
              f"so these are candidate WRONG-CALLEE defects -- adjudicate on "
              f"retail bytes, do not close them:")
        for sym, fz, eq in withheld:
            print(f"    {sym}  graded fuzzy={fz:.3f} equal={eq:.2f}")

    if by_unit_complete:
        print("\nTop units with newly COMPLETE atexit destructors:")
        for unit, cnt in sorted(by_unit_complete.items(), key=lambda x: -x[1])[:20]:
            print(f"  {unit}: {cnt}")

    if not apply and (newly_complete or newly_at_limit):
        print("\nRun with --apply to write these updates to the database.")


def main():
    parser = argparse.ArgumentParser(
        description='Verify atexit destructors and mark COMPLETE in DB',
    )
    parser.add_argument(
        '--apply', action='store_true',
        help='Write verdict updates to the database (default: dry-run)',
    )
    parser.add_argument(
        '--mark-at-limit', action='store_true',
        help='Also mark functions as AT_LIMIT if graded fuzzy >= 95%% but <100%%',
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
    parser.add_argument(
        '--skip-control', action='store_true',
        help='Skip the permissive-ruler control leg (halves objdiff runs; '
             'you lose the WITHHELD report that makes the 2026-09-11 defect '
             'visible)',
    )
    args = parser.parse_args()

    verify(
        unit_pattern=args.unit,
        apply=args.apply,
        mark_at_limit=args.mark_at_limit,
        verbose=args.verbose,
        limit=args.limit,
        skip_control=args.skip_control,
    )


if __name__ == '__main__':
    main()
