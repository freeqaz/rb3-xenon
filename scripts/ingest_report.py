#!/usr/bin/env python3
"""
Ingest report.json into the orchestrator database.

Usage:
    python3 scripts/ingest_report.py build/45410914/report.json
    python3 scripts/ingest_report.py build/45410914/report.json --db decomp.db
"""

import argparse
import sys
from pathlib import Path

# Add scripts to path for imports
sys.path.insert(0, str(Path(__file__).parent))

from orchestrator.database import init_database, ingest_report, get_stats


def main():
    parser = argparse.ArgumentParser(
        description="Ingest report.json into orchestrator database"
    )
    parser.add_argument(
        "report_path",
        type=Path,
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

    args = parser.parse_args()

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
    )

    print(f"\nIngestion complete:")
    print(f"  Inserted: {result['inserted']}")
    print(f"  Updated:  {result['updated']}")
    print(f"  Skipped:  {result['skipped']}")

    stats = get_stats(args.db)
    print(f"\nDatabase statistics:")
    print(f"  Total functions:   {stats['total_functions']}")
    print(f"  With match %:      {stats['with_percent']}")
    print(f"  Complete (100%):   {stats['complete']}")
    print(f"  At limit:          {stats['at_limit']}")
    if stats['avg_percent']:
        print(f"  Average match %:   {stats['avg_percent']:.1f}%")


if __name__ == "__main__":
    main()
