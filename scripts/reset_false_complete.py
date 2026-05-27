#!/usr/bin/env python3
"""One-time reset of false COMPLETE functions caused by base_size=0 objdiff bug.

Functions were falsely marked COMPLETE when objdiff reported 100% match for
functions where the original binary's section had 0 bytes. This script resets
them to workable state.
"""
import sqlite3
import sys
from collections import Counter
from pathlib import Path

DB_PATH = Path(__file__).resolve().parent.parent / "decomp.db"


def main():
    if not DB_PATH.exists():
        print(f"Error: Database not found: {DB_PATH}", file=sys.stderr)
        sys.exit(1)

    conn = sqlite3.connect(str(DB_PATH))
    conn.row_factory = sqlite3.Row

    # Find affected functions
    rows = conn.execute(
        "SELECT id, symbol, unit FROM functions "
        "WHERE verdict_reason LIKE '%base_size=0%' AND verdict = 'COMPLETE'"
    ).fetchall()

    if not rows:
        print("No false COMPLETE functions found. Already reset?")
        return

    print(f"Found {len(rows)} false COMPLETE functions to reset.\n")

    # Show top affected units
    unit_counts = Counter(r["unit"] for r in rows)
    print("Top affected units:")
    for unit, count in unit_counts.most_common(15):
        print(f"  {count:4d}  {unit}")
    print()

    # Reset them
    conn.execute(
        "UPDATE functions "
        "SET verdict = NULL, "
        "    verdict_reason = 'reset: was false COMPLETE from base_size=0 objdiff bug', "
        "    current_percent = NULL "
        "WHERE verdict_reason LIKE '%base_size=0%' AND verdict = 'COMPLETE'"
    )
    conn.commit()
    conn.close()

    print(f"Reset {len(rows)} functions to workable state.")


if __name__ == "__main__":
    main()
