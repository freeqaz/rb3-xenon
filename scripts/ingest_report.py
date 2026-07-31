#!/usr/bin/env python3
"""
Ingest report.json into the orchestrator database.

Usage:
    python3 scripts/ingest_report.py build/45410914/report.json
    python3 scripts/ingest_report.py build/45410914/report.json --db decomp.db
    python3 scripts/ingest_report.py --self-test

⚠ ALWAYS ingest a report measured at CURRENT HEAD in a CLEAN WORKTREE.
Lanes land by patch and never rebuild main, so main's build/45410914/report.json
is chronically stale (measured once at 7 landings behind). Ingesting it
REFRESHES the DB from old data, which is worse than not running the ingest.
Prefer the leg-B report of the A/B you just ran.

Pruning: symbols absent from the ingested report are MARKED ``live = 0``, never
DELETEd — the attempts table (per-attempt reasoning logs) and merged_symbols
hang off those rows and must outlive the symbol.
"""

import argparse
import json
import sys
import tempfile
from pathlib import Path

# Add scripts to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from orchestrator.database import init_database, ingest_report, get_stats


def _self_test() -> int:
    """Execute the coverage guard and the prune, with negative controls.

    A guard that has never been run is not a guard. This builds a throwaway DB,
    then checks: (1) a full report prunes nothing, (2) a TRUNCATED report is
    REFUSED rather than allowed to mark the table dead, (3) --force overrides
    it, (4) a revived symbol comes back, and (5) attempt history SURVIVES a
    prune (the whole reason we mark instead of delete).
    """
    def mkreport(path, symbols):
        json.dump(
            {
                "measures": {"matched_functions": 1, "total_functions": len(symbols)},
                "units": [
                    {
                        "name": "default/Test",
                        "functions": [
                            {"symbol": s, "size": 16, "fuzzy_match_percent": 50.0}
                            for s in symbols
                        ],
                    }
                ],
            },
            open(path, "w"),
        )

    failures = []

    def check(label, got, want):
        ok = got == want
        print(f"  [{'PASS' if ok else 'FAIL'}] {label}: got {got!r}, want {want!r}")
        if not ok:
            failures.append(label)

    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        db = td / "t.db"
        full = [f"fn_{i:04d}" for i in range(100)]

        mkreport(td / "a.json", full)
        r = ingest_report(td / "a.json", db_path=db)
        check("initial ingest inserts all", r["inserted"], 100)
        check("initial ingest prunes nothing", r["marked_dead"], 0)

        # Attach an attempt to a row that is about to die, so we can prove the
        # history survives.
        conn = init_database(db)
        fid = conn.execute(
            "SELECT id FROM functions WHERE symbol = 'fn_0099'"
        ).fetchone()[0]
        conn.execute(
            "INSERT INTO attempts (function_id, notes) VALUES (?, ?)",
            (fid, "reasoning log that must survive the prune"),
        )
        conn.commit()

        # (2) NEGATIVE CONTROL: a truncated report must be REFUSED.
        mkreport(td / "trunc.json", full[:10])
        r = ingest_report(td / "trunc.json", db_path=db)
        check("truncated report marks nothing dead", r["marked_dead"], 0)
        check(
            "truncated report is refused",
            r["prune_refused_reason"] is not None,
            True,
        )

        # (3) A small, in-tolerance drop IS pruned (95 of 100 = 95% > 90%).
        mkreport(td / "b.json", full[:95])
        r = ingest_report(td / "b.json", db_path=db)
        check("in-tolerance drop prunes the 5 absentees", r["marked_dead"], 5)

        conn = init_database(db)
        live = conn.execute("SELECT COUNT(*) FROM functions WHERE live = 1").fetchone()[0]
        check("live count after prune", live, 95)
        total = conn.execute("SELECT COUNT(*) FROM functions").fetchone()[0]
        check("rows are MARKED not DELETED", total, 100)

        # (5) The reasoning log on the now-dead row survived.
        kept = conn.execute(
            "SELECT COUNT(*) FROM attempts WHERE function_id = ?", (fid,)
        ).fetchone()[0]
        check("attempt history survives the prune", kept, 1)

        # (4) Revival.
        mkreport(td / "c.json", full)
        r = ingest_report(td / "c.json", db_path=db)
        check("reappearing symbols are revived", r["revived"], 5)

        # (3b) force overrides the guard.
        mkreport(td / "trunc2.json", full[:10])
        r = ingest_report(td / "trunc2.json", db_path=db, force_prune=True)
        check("force_prune overrides the guard", r["marked_dead"], 90)

    print()
    if failures:
        print(f"SELF-TEST FAILED: {len(failures)} check(s): {', '.join(failures)}")
        return 1
    print("SELF-TEST PASSED")
    return 0


def main():
    parser = argparse.ArgumentParser(
        description="Ingest report.json into orchestrator database"
    )
    parser.add_argument(
        "report_path",
        type=Path,
        nargs="?",
        help="Path to report.json (e.g., build/45410914/report.json)",
    )
    parser.add_argument(
        "--db",
        type=Path,
        default=Path("decomp.db"),
        help="Database path (default: decomp.db)",
    )
    parser.add_argument(
        "--no-update",
        action="store_true",
        help="Skip updating existing functions (only insert new)",
    )
    parser.add_argument(
        "--no-prune",
        action="store_true",
        help="Do not mark absent symbols dead (legacy upsert-only behaviour)",
    )
    parser.add_argument(
        "--force-prune",
        action="store_true",
        help="Bypass the coverage guard. Only for a report confirmed complete.",
    )
    parser.add_argument(
        "--self-test",
        action="store_true",
        help="Execute the prune + its guard against a throwaway DB, then exit",
    )

    args = parser.parse_args()

    if args.self_test:
        sys.exit(_self_test())

    if args.report_path is None:
        parser.error("report_path is required (or pass --self-test)")

    if not args.report_path.exists():
        print(f"Error: Report file not found: {args.report_path}")
        sys.exit(1)

    print(f"Initializing database: {args.db}")
    init_database(args.db)

    print(f"Ingesting report: {args.report_path}")
    result = ingest_report(
        args.report_path,
        db_path=args.db,
        update_existing=not args.no_update,
        prune=not args.no_prune,
        force_prune=args.force_prune,
    )

    # Provenance FIRST. The DB-wide aggregate below prints identically for a
    # stale and a fresh ingest, so these are the numbers that tell you WHICH
    # build you just indexed — check them against the HEAD you expect.
    print(f"\nReport provenance (check this against current HEAD!):")
    print(f"  matched_functions:     {result['report_matched_functions']}")
    print(f"  total_functions:       {result['report_total_functions']}")
    print(f"  matched_code_percent:  {result['report_matched_code_percent']}")

    print(f"\nIngestion complete:")
    print(f"  Symbols in report: {result['symbols_in_report']}")
    print(f"  Inserted: {result['inserted']}")
    print(f"  Updated:  {result['updated']}")
    print(f"  Skipped:  {result['skipped']}")
    print(f"  Marked dead: {result['marked_dead']}")
    print(f"  Revived:     {result['revived']}")
    if result["prune_refused_reason"]:
        print(f"\n  ⚠ PRUNE REFUSED: {result['prune_refused_reason']}")

    stats = get_stats(args.db)
    print(f"\nDatabase statistics (DB-wide, includes dead rows):")
    print(f"  Total functions:   {stats['total_functions']}")
    print(f"  With match %:      {stats['with_percent']}")
    print(f"  Complete (100%):   {stats['complete']}")
    print(f"  At limit:          {stats['at_limit']}")
    if stats['avg_percent']:
        print(f"  Average match %:   {stats['avg_percent']:.1f}%")


if __name__ == "__main__":
    main()
