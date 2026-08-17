#!/usr/bin/env python3
"""Regression-lock check: hard-fail a landing that drops a previously-100% fn.

RFC-16 Phase A, gate (g) (docs/plans/paths-to-100/16-auto-landing-pipeline.md §3).

Loads the strict-100 set from the *latest* landing_snapshot in decomp.db (the
authoritative "what was 100% as of the last landed main"), then compares it
against a candidate landing's freshly-built report.json. Any
(unit, fn_name, occurrence) that was >= 99.999% in the snapshot and is now
< 99.999% is a REGRESSION → hard fail (exit 1), printing the exact functions.

This is stronger than measure_delta.py's in-worktree A/B: A/B compares the lane
against its OWN rebase base, but this lock compares against the authoritative
last-landed main, so it catches a regression introduced by a bad union-resolve
*during* landing (the objects.json replace-not-merge drop) — a class the per-lane
A/B is blind to.

Keying and threshold are identical to measure_delta.py: (unit, fn_name,
occurrence) with STRICT = 99.999. The raw report unit name is used verbatim.

Escape hatch: a landing may intentionally drop a strict-100 (e.g. a wider
correct span replacing a fake ICF-stub-fold). Pass --allow-drop
<unit>:<fn>[:<occ>] (repeatable) or --allow-drop-file <path> (one entry per
line, '#' comments ok). Occurrence is optional; if omitted, ALL occurrences of
that (unit, fn) are allowed to drop. Allowed drops are reported but do not fail
the gate.

Usage (land-lane, against the candidate's cold report):

  scripts/harvest/check_regression_lock.py \
      --report ~/tmp/land-verify/build/45410914/report.json \
      --db decomp.db

  # with an explicit intentional drop:
  scripts/harvest/check_regression_lock.py --report ... --db decomp.db \
      --allow-drop default/Foo:SomeFn::__flt

Exit codes:
  0  clean (no unlisted strict-100 drops)
  1  regression (>=1 unlisted strict-100 drop) OR usage/data error
"""
import argparse
import json
import sqlite3
import sys

STRICT = 99.999


def pct_map(report_path):
    """(unit, fn, occurrence) -> normalized match percent. Mirrors measure_delta."""
    d = json.load(open(report_path))
    m = {}
    seen = {}
    for u in d["units"]:
        un = u.get("name")
        for f in (u.get("functions") or []):
            k = (un, f["name"])
            i = seen.get(k, 0)
            seen[k] = i + 1
            m[(un, f["name"], i)] = f["match_percent_normalized"]
    return m


def latest_snapshot_strict(db_path):
    """Return (commit, {(unit, fn, occ): pct}) for the latest snapshot's strict set.

    'Latest' = the merge_commit of the row with the max landed_at (ties broken by
    rowid, i.e. most-recently inserted).
    """
    conn = sqlite3.connect(db_path)
    try:
        row = conn.execute("SELECT version FROM schema_version").fetchone()
        if row is None or row[0] < 17:
            raise SystemExit(
                f"decomp.db at {db_path} is schema v{row[0] if row else '?'}; "
                "landing_snapshot needs v17 — run the orchestrator migration first."
            )
        head = conn.execute(
            "SELECT merge_commit FROM landing_snapshot "
            "ORDER BY landed_at DESC, rowid DESC LIMIT 1"
        ).fetchone()
        if head is None:
            raise SystemExit(
                "landing_snapshot is empty — no baseline to lock against. "
                "Snapshot a landed commit first (snapshot_landing.py)."
            )
        commit = head[0]
        strict = {}
        # The ix_snapshot_strict partial index serves this exact predicate.
        for unit, fn, occ, pct in conn.execute(
            "SELECT unit, fn_name, occurrence, match_pct FROM landing_snapshot "
            "WHERE merge_commit = ? AND match_pct >= ?",
            (commit, STRICT),
        ):
            strict[(unit, fn, occ)] = pct

        # ── EMPTY-BASELINE REFUSAL (lane W38-GATES, 2026-08-17) ─────────────────
        # The guard above catches an ABSENT snapshot (`head is None`). It does not
        # catch a snapshot that EXISTS but contributes ZERO strict-100 rows -- and
        # with `strict = {}` the comparison loop below runs zero times and this
        # tool prints "VERDICT: CLEAN (no unlisted strict-100 regressions)" and
        # exits 0. Reproduced against a synthetic db holding one sub-strict row:
        # rc=0, CLEAN, over a baseline containing no functions at all.
        #
        # That is a rubber stamp on the landing pipeline's hard gate, and it is
        # reachable without anything looking broken: a snapshot written before the
        # report was built, a partial/aborted insert, a keying or unit-name
        # convention change, or a merge_commit that does not match the rows.
        #
        # The floor is ZERO -- not a chosen threshold. A snapshot whose whole point
        # is to enumerate the strict-100 set cannot legitimately enumerate none of
        # it: this repo carries tens of thousands of matched functions, so an empty
        # strict set means the snapshot did not record what it claims to record.
        if not strict:
            total = conn.execute(
                "SELECT COUNT(*) FROM landing_snapshot WHERE merge_commit = ?",
                (commit,),
            ).fetchone()[0]
            raise SystemExit(
                f"REFUSING: snapshot @ {commit[:12]} contributes ZERO strict-100 "
                f"rows ({total} row(s) total for that commit, none >= {STRICT}).\n"
                "  The baseline is EMPTY, so every comparison below would be "
                "vacuous and the gate would print CLEAN having checked nothing.\n"
                "  This is NOT a clean landing -- it is a broken baseline. Re-run "
                "snapshot_landing.py against a freshly built report.json, and "
                "check that the snapshot's unit/fn keying matches the report's."
            )
        return commit, strict
    finally:
        conn.close()


def parse_allow_drop(entries, files):
    """Return a set of allowed-drop keys.

    Each entry is 'unit:fn[:occ]'. Because unit names and mangled fn names can
    themselves contain ':', we split from the RIGHT: the trailing field is
    treated as an occurrence index ONLY if it is a bare integer; otherwise the
    whole tail is the fn name and occurrence is a wildcard.

    A key with occ=None wildcards all occurrences of (unit, fn).
    Returns a set of (unit, fn, occ_or_None).
    """
    raw = list(entries or [])
    for fpath in (files or []):
        with open(fpath) as fh:
            for line in fh:
                line = line.split("#", 1)[0].strip()
                if line:
                    raw.append(line)

    allowed = set()
    for spec in raw:
        # unit:fn  or  unit:fn:occ  — unit is the first ':'-field, the rest is fn
        # (which may contain ':'), with an optional trailing integer occurrence.
        parts = spec.split(":")
        if len(parts) < 2:
            raise SystemExit(f"--allow-drop '{spec}' must be unit:fn[:occ]")
        occ = None
        if len(parts) >= 3 and parts[-1].isdigit():
            occ = int(parts[-1])
            unit = parts[0]
            fn = ":".join(parts[1:-1])
        else:
            unit = parts[0]
            fn = ":".join(parts[1:])
        allowed.add((unit, fn, occ))
    return allowed


def is_allowed(key, allowed):
    unit, fn, occ = key
    return (unit, fn, occ) in allowed or (unit, fn, None) in allowed


def check(db_path, report_path, allowed):
    """Return (commit, regressed, allowed_drops, checked, found).

    regressed / allowed_drops are lists of (unit, fn, occ, old_pct, new_pct).
    `checked` is how many baseline keys were compared; `found` how many of them
    resolved in the new report (a collapse there means keying drift, not health).
    """
    commit, strict = latest_snapshot_strict(db_path)
    now = pct_map(report_path)

    regressed = []
    allowed_drops = []
    checked = 0
    found = 0
    for key, old in sorted(strict.items()):
        new = now.get(key)
        checked += 1
        found += new is not None
        # Missing from the new report OR below strict = a drop. A function that
        # vanished entirely (unit dropped from the build / renamed) counts as a
        # drop, treated as new_pct = 0.0 (the safe/loud interpretation).
        new_pct = 0.0 if new is None else new
        if new_pct < STRICT:
            rec = (key[0], key[1], key[2], old, new_pct)
            if is_allowed(key, allowed):
                allowed_drops.append(rec)
            else:
                regressed.append(rec)
    return commit, regressed, allowed_drops, checked, found


def main():
    ap = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )
    ap.add_argument("--report", required=True,
                    help="candidate landing's freshly-built report.json")
    ap.add_argument("--db", default="decomp.db", help="path to decomp.db")
    ap.add_argument("--allow-drop", action="append", default=[],
                    metavar="unit:fn[:occ]",
                    help="intentional strict-100 drop to allow (repeatable)")
    ap.add_argument("--allow-drop-file", action="append", default=[],
                    metavar="PATH",
                    help="file of allow-drop entries, one per line ('#' comments)")
    args = ap.parse_args()

    allowed = parse_allow_drop(args.allow_drop, args.allow_drop_file)
    commit, regressed, allowed_drops, checked, found = check(
        args.db, args.report, allowed)

    # Report what was REACHED, not just what passed: "CLEAN" over 0 compared rows
    # and "CLEAN" over 40,000 are the same sentence otherwise. `found` is how many
    # baseline keys resolved in the new report at all -- if that collapses, the
    # keying convention has drifted and the run is measuring absence, not health.
    print(f"regression-lock: comparing against snapshot @ {commit[:12]} "
          f"(strict-100 baseline)")
    print(f"  baseline strict-100 rows compared: {checked}  "
          f"(resolved in the new report: {found})")

    if allowed_drops:
        print(f"\nALLOWED DROPS ({len(allowed_drops)}) — intentional, audit-logged:")
        for unit, fn, occ, old, new in allowed_drops:
            print(f"  ~ {unit}  {fn}  [occ {occ}]  ({old:.3f} -> {new:.3f})")

    if regressed:
        print(f"\nREGRESSION-LOCK FAIL: {len(regressed)} strict-100 function(s) "
              "dropped below 99.999% and are NOT allow-listed:")
        for unit, fn, occ, old, new in regressed:
            print(f"  - {unit}  {fn}  [occ {occ}]  ({old:.3f} -> {new:.3f})")
        print("\nVERDICT: BLOCK (main untouched; DEFER this landing)")
        return 1

    print("\nVERDICT: CLEAN (no unlisted strict-100 regressions)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
