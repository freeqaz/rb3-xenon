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
#
# This is a closed allow-list, so a verdict value added elsewhere and NOT added
# here silently FALLS THROUGH and stays in the worklist -- measured, not
# assumed: a probe against a scratch DB confirmed an unlisted verdict leaks
# here while every other selector caught it. Adding a verdict to
# database.KNOWN_VERDICTS is not enough; if it means "not work", add it here.
#
# `identity_unestablished`: the target body is not established to be this
# function. Not open work, not done, not at a floor -- and the one state where
# leaving it in the worklist is actively dangerous, because a one-token edit
# can drive it to byte-exact against a body that is not the function, which is
# this project's sole admission gate saying yes to a false crack.
_DEAD_VERDICTS = {"complete", "at_limit", "identity_unestablished"}


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


# --- struct-size-delta sub-classifier -------------------------------------
# Wave-6 discovery (DepthBuffer3DAttachment, commit 79bb233, +7): a whole band
# of engine STLport template instantiations (vector/list grow/fill/copy/destroy/
# create_node) and compiler dtors (`scalar/vector deleting destructor') differ
# from retail by exactly ONE mismatched immediate (li/mulli/slwi/addi) encoding
# sizeof(element) or sizeof(node). When retail sizeof > ours AND the class's head
# member offsets are already retail-correct (its other functions match), padding
# the struct TAIL to the retail size closes the whole template family at zero
# regression risk.
#
# report.json alone cannot see the immediate, so this is a *candidate* pre-filter
# on the static signature (engine unit + STL/dtor template demangle + small size
# band + fuzzy in [99,100)); confirmation of the single diff_arg immediate — and
# the retail>ours direction with matching heads — is a downstream objdiff step
# (run_diff_inspect mismatches). Families (>=2 funcs sharing an element type) are
# real size deltas; lone functions with a huge stride delta are usually ICF folds
# (retail merged the identical memcpy-body function with a different-size element)
# and are NOT tail-pad-fixable.
_STL_MARKERS = (
    "stlpmtx_std::", "stlp_std::",
    "`scalar deleting destructor'", "`vector deleting destructor'",
    "__uninitialized_", "_Destroy_Range", "_List_base", "_List_node",
    "::vector<", "::list<", "::deque<", "_M_",
)


def is_struct_size_delta_candidate(demangled, category, size, fuzzy, normalized,
                                   *, size_lo=40, size_hi=200):
    """True if a function has the static signature of the tail-pad / struct-size-
    delta band: an *engine* STLport-template or compiler-dtor instantiation, in
    the small size band, sitting just under 100% fuzzy but not yet normalized-100.

    This is a *candidate* tag only — the single differing immediate (and its
    retail>ours direction) must be confirmed downstream via objdiff. The default
    72-140B window matches the observed template-instantiation body sizes; the
    caller may widen via size_lo/size_hi."""
    if category != "engine":
        return False
    if normalized is not None and normalized >= 100:
        return False
    if fuzzy is None or not (99.0 <= fuzzy < 100.0):
        return False
    if not (size_lo <= size <= size_hi):
        return False
    dn = demangled or ""
    return any(m in dn for m in _STL_MARKERS)


def generate(report_path, db_path, *, min_fuzzy, max_fuzzy, unit_pattern,
             game_only, min_size, struct_size_delta_only=False):
    """Return (functions, stats). ``functions`` is the vetted, filtered list
    sorted by fuzzy descending (near-misses first). ``stats`` counts drops per
    reason for transparency."""
    with open(report_path) as fh:
        report = json.load(fh)
    db = load_db_index(db_path)

    out = []
    drop = {"normalized_100": 0, "anon_fn": 0, "funclet": 0, "verdict": 0,
            "excluded": 0, "band": 0, "unit": 0, "game": 0, "size": 0,
            "no_fuzzy": 0, "not_ssd": 0}
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

            demangled = (func.get("metadata", {}) or {}).get("demangled_name", "")
            ssd = is_struct_size_delta_candidate(
                demangled, category, size, fuzzy, normalized)
            if struct_size_delta_only and not ssd:
                drop["not_ssd"] += 1
                continue

            out.append({
                "symbol": symbol,
                "demangled": demangled,
                "unit": unit_name,
                "src_path": src_path,
                "size": size,
                "fuzzy_match_percent": fuzzy,
                "match_percent_normalized": normalized,
                "category": category,
                "struct_size_delta": ssd,
            })

    out.sort(key=lambda r: (r["fuzzy_match_percent"] is None,
                            -(r["fuzzy_match_percent"] or 0)))
    stats = {"seen": seen, "kept": len(out), "drop": drop,
             "game_kept": sum(1 for r in out if r["category"] == "game"),
             "engine_kept": sum(1 for r in out if r["category"] == "engine"),
             "ssd_kept": sum(1 for r in out if r.get("struct_size_delta"))}
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
    ap.add_argument("--struct-size-delta", action="store_true",
                    dest="struct_size_delta_only",
                    help="keep only tail-pad / struct-size-delta candidates "
                         "(engine STLport-template or compiler-dtor instantiations "
                         "in the 40-200B band at fuzzy [99,100); the single "
                         "immediate diff is confirmed downstream via objdiff)")
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
        min_size=args.min_size,
        struct_size_delta_only=args.struct_size_delta_only)

    doc = {
        "generated_at": _dt.datetime.now(_dt.timezone.utc).strftime("%Y-%m-%dT%H:%M:%SZ"),
        "report": os.path.relpath(args.report, _REPO),
        "filters": {
            "min_fuzzy": args.min_fuzzy, "max_fuzzy": args.max_fuzzy,
            "unit_pattern": args.unit_pattern, "game_only": args.game_only,
            "min_size": args.min_size,
            "struct_size_delta_only": args.struct_size_delta_only,
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
              f"(game={stats['game_kept']} engine={stats['engine_kept']} "
              f"struct_size_delta={stats['ssd_kept']}) | "
              f"dropped: normalized100={d['normalized_100']} anon_fn={d['anon_fn']} "
              f"funclet={d['funclet']} verdict={d['verdict']} excluded={d['excluded']} "
              f"band={d['band']} no_fuzzy={d['no_fuzzy']} size={d['size']} "
              f"unit={d['unit']} game={d['game']} not_ssd={d['not_ssd']}",
              file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
