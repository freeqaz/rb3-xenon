#!/usr/bin/env python3
"""Refresh decomp.db for the permuter: re-ingest report.json AND populate the
per-function objdiff metadata (current_percent / best_percent / verdict) that
scan_and_permute consumes for candidate selection + ranking.

WHY this is separate from scripts/ingest_report.py:
  ingest_report() deliberately leaves current_percent NULL (it was waiting on a
  sync_objdiff.py that does not exist in this repo). That makes the permuter's
  candidate query (scan_and_permute._resolve_*: "SELECT ... current_percent ...
  ORDER BY current_percent") see no near-misses -> it can't prioritize.

  The report's per-function fuzzy_match_percent / match_percent_normalized IS the
  objdiff ground truth (objdiff produced report.json), so we copy it into
  current_percent. The permuter still RE-RUNS objdiff per function for the actual
  baseline it climbs from; current_percent is only used to find + rank candidates.

Percent source per function (in priority order):
  fuzzy_match_percent      (present only on units that have a target<->base pair;
                            this is the raw objdiff scalar the permuter climbs)
  match_percent_normalized (present on all; the fork's normalized scalar)

Verdict is set conservatively:
  COMPLETE if percent >= 99.995 (rounds to 100)
  else NULL (leave 'workable' so the permuter's --status workable picks it up)
  (We do NOT invent AT_LIMIT — that's an agent/human judgement.)

Usage:
  tools/refresh_permuter_db.py                          # main repo decomp.db + build report
  tools/refresh_permuter_db.py --db decomp.db --report build/45410914/report.json
"""
import argparse, json, sqlite3, sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "scripts"))
from orchestrator.database import init_database, ingest_report, get_stats  # noqa: E402


def fn_percent(f):
    p = f.get("fuzzy_match_percent")
    if p is None:
        p = f.get("match_percent_normalized")
    return p


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--db", default=str(ROOT / "decomp.db"))
    ap.add_argument("--report", default=str(ROOT / "build/45410914/report.json"))
    a = ap.parse_args()

    db = Path(a.db)
    report = Path(a.report)
    if not report.exists():
        sys.exit(f"report not found: {report}")

    print(f"init + ingest: {db}  <-  {report}")
    init_database(db)
    res = ingest_report(report, db_path=db, update_existing=True)
    print(f"  ingest: inserted={res['inserted']} updated={res['updated']} skipped={res['skipped']}")

    # Now populate current_percent / best_percent / verdict from the report's
    # objdiff metadata. Join on symbol == report function 'name'.
    rep = json.load(open(report))
    conn = sqlite3.connect(str(db))
    updated = matched = nearmiss = 0
    for unit in rep.get("units", []):
        for f in unit.get("functions", []):
            sym = f.get("symbol") or f.get("name")
            if not sym:
                continue
            pct = fn_percent(f)
            if pct is None:
                continue
            verdict = "COMPLETE" if pct >= 99.995 else None
            # best_percent = max(existing best, pct)
            row = conn.execute(
                "SELECT best_percent FROM functions WHERE symbol=?", (sym,)
            ).fetchone()
            if row is None:
                continue
            prev_best = row[0] if row[0] is not None else 0.0
            best = max(prev_best, pct)
            # Only stamp COMPLETE; never downgrade an existing agent verdict to NULL.
            if verdict == "COMPLETE":
                conn.execute(
                    "UPDATE functions SET current_percent=?, best_percent=?, verdict=?, "
                    "updated_at=CURRENT_TIMESTAMP WHERE symbol=?",
                    (pct, best, verdict, sym),
                )
            else:
                conn.execute(
                    "UPDATE functions SET current_percent=?, best_percent=?, "
                    "updated_at=CURRENT_TIMESTAMP WHERE symbol=?",
                    (pct, best, sym),
                )
            updated += 1
            if pct >= 99.995:
                matched += 1
            elif pct >= 50.0:
                nearmiss += 1
    conn.commit()
    conn.close()

    print(f"  metadata: set current_percent on {updated} fns "
          f"(>=100: {matched}, near-miss[50,100): {nearmiss})")

    st = get_stats(db)
    print("\nstats:")
    print(f"  total functions: {st['total_functions']}")
    print(f"  with match %:    {st['with_percent']}")
    print(f"  complete(100%):  {st['complete']}")
    if st.get("avg_percent"):
        print(f"  avg %:           {st['avg_percent']:.1f}%")


if __name__ == "__main__":
    main()
