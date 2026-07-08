#!/usr/bin/env python3
"""Snapshot the per-function match% state of a landed commit into decomp.db.

RFC-16 Phase A (docs/plans/paths-to-100/16-auto-landing-pipeline.md §3).

Given a report.json and the `main` SHA it corresponds to, write the full
strict-100 set (and, by default, the >=50 near-miss band for fuzzy-regression
forensics) into the `landing_snapshot` table. This is the authoritative record
of "what was 100% as of commit X" that the regression-lock check
(check_regression_lock.py) compares a candidate landing against.

Keying: the (unit, fn_name, occurrence) tuple is IDENTICAL to
measure_delta.py::pct_map — the occurrence index disambiguates the ~11
binary-wide duplicate function names within a unit. `unit` is the raw
report.json unit name (e.g. "default/MasterAudio"), exactly as measure_delta
uses it, so the two tools agree on identity.

Usage (land-lane step i, after the ff-merge):

  scripts/harvest/snapshot_landing.py \
      --report build/45410914/report.json \
      --commit "$(git rev-parse HEAD)" \
      --db decomp.db

By default only rows with match_pct >= 50 are stored (strict-100 + near-miss
band). Pass --min-pct 99.999 for a strict-only (smaller) snapshot, or
--min-pct 0 to store every function.

Exit 0 on success; nonzero on error.
"""
import argparse
import json
import sqlite3
import sys
import time

STRICT = 99.999
NEAR_MISS = 50.0


def pct_rows(report_path):
    """Yield (unit, fn_name, occurrence, match_pct) for every function.

    Mirrors measure_delta.py::pct_map exactly (same occurrence bookkeeping and
    the same raw report unit name), so the snapshot and the A/B measure agree on
    function identity.
    """
    d = json.load(open(report_path))
    seen = {}
    for u in d["units"]:
        un = u.get("name")
        for f in (u.get("functions") or []):
            k = (un, f["name"])
            i = seen.get(k, 0)
            seen[k] = i + 1
            yield (un, f["name"], i, f["match_percent_normalized"])


def snapshot(db_path, report_path, commit, min_pct=NEAR_MISS, landed_at=None,
             replace=True):
    """Write the snapshot rows for `commit` into landing_snapshot.

    Returns (total_written, strict_count).
    """
    if landed_at is None:
        landed_at = int(time.time())

    conn = sqlite3.connect(db_path)
    try:
        # Fail loudly if the migration has not been applied.
        row = conn.execute("SELECT version FROM schema_version").fetchone()
        if row is None or row[0] < 17:
            raise SystemExit(
                f"decomp.db at {db_path} is schema v{row[0] if row else '?'}; "
                "landing_snapshot needs v17 — run the orchestrator migration first."
            )

        if replace:
            conn.execute("DELETE FROM landing_snapshot WHERE merge_commit = ?",
                         (commit,))

        rows = []
        strict = 0
        for unit, fn, occ, pct in pct_rows(report_path):
            if pct < min_pct:
                continue
            if pct >= STRICT:
                strict += 1
            rows.append((commit, landed_at, unit, fn, occ, pct))

        conn.executemany(
            "INSERT OR REPLACE INTO landing_snapshot "
            "(merge_commit, landed_at, unit, fn_name, occurrence, match_pct) "
            "VALUES (?, ?, ?, ?, ?, ?)",
            rows,
        )
        conn.commit()
        return len(rows), strict
    finally:
        conn.close()


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--report", required=True, help="report.json to snapshot")
    ap.add_argument("--commit", required=True,
                    help="main SHA this report corresponds to")
    ap.add_argument("--db", default="decomp.db", help="path to decomp.db")
    ap.add_argument("--min-pct", type=float, default=NEAR_MISS,
                    help="store only functions >= this match%% "
                         "(default 50 = strict-100 + near-miss band; "
                         "use 99.999 for strict-only, 0 for all)")
    ap.add_argument("--landed-at", type=int, default=None,
                    help="unix ts to record (default: now; use the commit time "
                         "for historical backfill)")
    args = ap.parse_args()

    total, strict = snapshot(args.db, args.report, args.commit,
                             min_pct=args.min_pct, landed_at=args.landed_at)
    print(f"snapshot {args.commit[:12]}: wrote {total} rows "
          f"({strict} strict-100) at min_pct={args.min_pct} -> {args.db}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
