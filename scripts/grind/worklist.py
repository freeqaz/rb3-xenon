#!/usr/bin/env python3
"""worklist.py — the canonical vetted-pool generator for the grind loop.

Wave 3 wasted 4 of 8 matcher groups because its worklist was built straight from
``fuzzy_match_percent < 100`` and picked up two classes of non-targets:

  * **Already-matched functions.** ``report.json`` counts progress with
    ``match_percent_normalized`` (relocation-address differences ignored), NOT
    ``fuzzy_match_percent``. A function can sit at ``normalized == 100`` (fully
    matched, already counted) while ``fuzzy`` reads 99.x because of an unresolved
    extern reloc address. Selecting on fuzzy re-queues work that is already done.
  * **EH unwind funclets.** Anonymous ``fn_*`` cleanup funclets (see
    ``classify_funclets.py``) are not independent decomp targets; they match when
    their parent matches. They show up in the 99.x band and burn a whole group
    for zero yield.

This module is the single vetted entry point every future wave should build its
pool from. It filters out:
  (a) ``match_percent_normalized >= 100``      — already matched (the counting authority)
  (b) anonymous ``fn_*`` symbols               — not nameable/pinnable targets
  (c) EH funclets                              — DB ``primary_pattern = 'eh_funclet'``
  (d) DB verdict COMPLETE / AT_LIMIT, or DB ``excluded = 1``

and supports band / unit / game / size narrowing.

Input:  ``build/45410914/report.json`` (+ ``decomp.db`` for verdicts/exclusions).
Output: JSON ``{ "filters": {...}, "count": N, "functions": [ ... ] }`` on stdout
        (or ``--out FILE``). Each record carries symbol, demangled, unit,
        src_path, size, fuzzy_match_percent, match_percent_normalized, category.

Usage:
    venv/bin/python scripts/grind/worklist.py --min 85 --game-only
    venv/bin/python scripts/grind/worklist.py --min 90 --max 99.99 --out pool.json
    venv/bin/python scripts/grind/worklist.py --unit-pattern '*band3*' --min-size 32
"""

from __future__ import annotations

import argparse
import datetime as _dt
import fnmatch
import json
import os
import sqlite3
import sys

_HERE = os.path.dirname(os.path.abspath(__file__))
_REPO = os.path.dirname(os.path.dirname(_HERE))

# DB verdicts that mean "do not work this function" (case-insensitive match).
_DEAD_VERDICTS = {"complete", "at_limit"}


def load_db_index(db_path):
    """symbol -> (verdict_lower, excluded, primary_pattern) for every DB row.
    Empty dict if the DB is absent (detection still works off report.json)."""
    if not os.path.exists(db_path):
        return {}
    conn = sqlite3.connect(db_path)
    idx = {}
    for sym, verdict, excluded, pat in conn.execute(
        "SELECT symbol, verdict, excluded, primary_pattern FROM functions"
    ):
        idx[sym] = ((verdict or "").strip().lower(), int(excluded or 0), pat or "")
    conn.close()
    return idx


def generate(report_path, db_path, *, min_fuzzy, max_fuzzy, unit_pattern,
             game_only, min_size):
    """Return (functions, stats). ``functions`` is the vetted, filtered list
    sorted by fuzzy descending (near-misses first). ``stats`` counts drops per
    reason for transparency."""
    with open(report_path) as fh:
        report = json.load(fh)
    db = load_db_index(db_path)

    out = []
    drop = {"normalized_100": 0, "anon_fn": 0, "funclet": 0, "verdict": 0,
            "excluded": 0, "band": 0, "unit": 0, "game": 0, "size": 0,
            "no_fuzzy": 0}
    seen = 0

    for unit in report.get("units", []):
        meta = unit.get("metadata", {}) or {}
        unit_name = unit.get("name", "")
        src_path = meta.get("source_path", "")
        cats = meta.get("progress_categories") or []
        is_game = "game" in cats
        category = "game" if is_game else ("engine" if "engine" in cats
                                           else (cats[0] if cats else "?"))

        # Unit-level narrowing (applied once per unit).
        if unit_pattern and not (
            fnmatch.fnmatch(unit_name, unit_pattern)
            or fnmatch.fnmatch(src_path, unit_pattern)
        ):
            # count the whole unit's functions as unit-dropped for the summary
            drop["unit"] += len(unit.get("functions") or [])
            continue
        if game_only and not is_game:
            drop["game"] += len(unit.get("functions") or [])
            continue

        for func in unit.get("functions", []):
            seen += 1
            symbol = func.get("symbol") or func.get("name") or ""
            if not symbol:
                continue
            normalized = func.get("match_percent_normalized")
            fuzzy = func.get("fuzzy_match_percent")
            size = int(func.get("size", 0) or 0)

            # (a) already matched — normalized is the counting authority.
            if normalized is not None and normalized >= 100:
                drop["normalized_100"] += 1
                continue
            # (b) anonymous fn_* — not a nameable/pinnable target.
            if symbol.startswith("fn_"):
                drop["anon_fn"] += 1
                continue
            # (c)/(d) DB checks: funclet tag, dead verdict, explicit exclusion.
            verdict, excluded, pat = db.get(symbol, ("", 0, ""))
            if pat == "eh_funclet":
                drop["funclet"] += 1
                continue
            if excluded:
                drop["excluded"] += 1
                continue
            if verdict in _DEAD_VERDICTS:
                drop["verdict"] += 1
                continue
            # fuzzy band
            if fuzzy is None:
                drop["no_fuzzy"] += 1
                continue
            if not (min_fuzzy <= fuzzy <= max_fuzzy):
                drop["band"] += 1
                continue
            if size < min_size:
                drop["size"] += 1
                continue

            out.append({
                "symbol": symbol,
                "demangled": (func.get("metadata", {}) or {}).get("demangled_name", ""),
                "unit": unit_name,
                "src_path": src_path,
                "size": size,
                "fuzzy_match_percent": fuzzy,
                "match_percent_normalized": normalized,
                "category": category,
            })

    out.sort(key=lambda r: (r["fuzzy_match_percent"] is None,
                            -(r["fuzzy_match_percent"] or 0)))
    stats = {"seen": seen, "kept": len(out), "drop": drop,
             "game_kept": sum(1 for r in out if r["category"] == "game"),
             "engine_kept": sum(1 for r in out if r["category"] == "engine")}
    return out, stats


def main(argv=None):
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--report", default=os.path.join(_REPO, "build/45410914/report.json"),
                    help="path to report.json (default: build/45410914/report.json)")
    ap.add_argument("--db", default=os.path.join(_REPO, "decomp.db"),
                    help="orchestrator SQLite DB (default: decomp.db)")
    ap.add_argument("--min", type=float, default=0.0, dest="min_fuzzy",
                    help="minimum fuzzy_match_percent (default 0)")
    ap.add_argument("--max", type=float, default=100.0, dest="max_fuzzy",
                    help="maximum fuzzy_match_percent (default 100)")
    ap.add_argument("--unit-pattern", default=None,
                    help="fnmatch glob over unit name or source_path (e.g. '*band3*')")
    ap.add_argument("--game-only", action="store_true",
                    help="keep only units whose progress_categories include 'game'")
    ap.add_argument("--min-size", type=int, default=0,
                    help="minimum function size in bytes (default 0)")
    ap.add_argument("--out", default=None, help="write JSON here (default: stdout)")
    ap.add_argument("--quiet", action="store_true",
                    help="suppress the stderr filter summary")
    args = ap.parse_args(argv)

    if not os.path.exists(args.report):
        print(f"error: report not found: {args.report}", file=sys.stderr)
        return 1

    funcs, stats = generate(
        args.report, args.db,
        min_fuzzy=args.min_fuzzy, max_fuzzy=args.max_fuzzy,
        unit_pattern=args.unit_pattern, game_only=args.game_only,
        min_size=args.min_size)

    doc = {
        "generated_at": _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "report": os.path.relpath(args.report, _REPO),
        "filters": {
            "min_fuzzy": args.min_fuzzy, "max_fuzzy": args.max_fuzzy,
            "unit_pattern": args.unit_pattern, "game_only": args.game_only,
            "min_size": args.min_size,
        },
        "count": stats["kept"],
        "functions": funcs,
    }
    payload = json.dumps(doc, indent=2)
    if args.out:
        with open(args.out, "w") as fh:
            fh.write(payload + "\n")
    else:
        print(payload)

    if not args.quiet:
        d = stats["drop"]
        print(f"[worklist] seen={stats['seen']} kept={stats['kept']} "
              f"(game={stats['game_kept']} engine={stats['engine_kept']}) | "
              f"dropped: normalized100={d['normalized_100']} anon_fn={d['anon_fn']} "
              f"funclet={d['funclet']} verdict={d['verdict']} excluded={d['excluded']} "
              f"band={d['band']} no_fuzzy={d['no_fuzzy']} size={d['size']} "
              f"unit={d['unit']} game={d['game']}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
