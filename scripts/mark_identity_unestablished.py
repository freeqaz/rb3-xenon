#!/usr/bin/env python3
"""Mark (or clear) rows whose TARGET BODY is not established to be that function.

Why this script exists
----------------------
`decomp.db.functions.verdict` had three values: NULL (open work), COMPLETE
(byte-exact) and AT_LIMIT (our source is at its floor, the residual is
intrinsic). There was no way to say the fourth true thing: *we cannot attribute
a target body to this symbol at all*.

Every existing value gets that case wrong. AT_LIMIT asserts a floor we have not
established. COMPLETE asserts byte-exactness against a body that may not be the
function. NULL returns the row to the work queue -- and that is the dangerous
one, because a one-token edit can drive a wrong-target row to byte-exact, and
byte-exactness is this project's SOLE admission gate. It would say yes.

On 2026-08-13 three rows had to be forced to NULL by hand for exactly this
reason (`?Null@Symbol@@QBA_NXZ` became the top near-miss in `default/FilePath`,
one edit from a false crack). An earlier attempt wrote `''`, a fourth undesigned
value that passed the `verdict NOT IN (...)` filters and was excluded by the
`verdict IS NULL` ones, so those rows sat in NO bucket while still counting
toward `avg_percent`. Full account: docs/decomp/VERDICT_STATES.md.

What it writes
--------------
`database.mark_identity_unestablished` sets, atomically:

    verdict         = 'IDENTITY_UNESTABLISHED'   (primary axis)
    excluded        = 1                          (secondary axis, belt+braces)
    current_percent = NULL
    best_percent    = NULL                       (monotone MAX() can't lower it)
    verdict_reason  = <required, non-empty>

Both axes are set because this repo has two "is this workable" idioms and
different consumers use different ones -- 15 queries filter `excluded = 0`
without looking at verdict, while the primary work selectors do the reverse.
Setting both means a query written by someone unaware of this state is caught
whichever idiom it happened to use.

Usage
-----
    # preview (default -- writes nothing)
    python3 scripts/mark_identity_unestablished.py --id 130424 --reason '...'

    # apply
    python3 scripts/mark_identity_unestablished.py --id 130424 --reason '...' --apply

    # the 2026-08-13 incident rows, with their recorded reasons
    python3 scripts/mark_identity_unestablished.py --incident-2026-08-13 --apply

    # audit
    python3 scripts/mark_identity_unestablished.py --list

    # inverse, once identity IS established (evidence work done + map landed)
    python3 scripts/mark_identity_unestablished.py --clear --id 130424 \
        --reason 'homed to 0x8227c70c, map commit <sha>' --apply

★ Run it against a SCRATCH COPY first (`--db /path/to/copy.db`). Peer agents
are usually live in the shared decomp.db, and the recorded failure being closed
here is precisely a clearance written to shared state ahead of the repair that
justified it.
"""

from __future__ import annotations

import argparse
import sqlite3
import sys
from pathlib import Path

REPO_ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO_ROOT / "scripts"))

from orchestrator.database import (  # noqa: E402
    VERDICT_IDENTITY_UNESTABLISHED,
    clear_identity_unestablished,
    mark_identity_unestablished,
)

DEFAULT_DB = REPO_ROOT / "decomp.db"

# The 2026-08-13 rows, with the reason each is in this state.
#
# ⚠ id 130115 (?DataDir@UIPanel@@$4...) is DELIBERATELY ABSENT. The incident
# listed it, but its identity has since been ESTABLISHED: commit `eb1518e7`
# (2026-08-13, "re-home 3 disproved $4 thunk names onto their real bodies")
# replaced the value at "0x826412e0" with that symbol, on evidence -- the `EM@`
# displacement is 0x4c = 76, equal to the body's own `addi`; the branch target
# is `?DataDir@UIPanel@@UAA...`; and the name is required to be *defined* in the
# owning unit's obj. Verified by loading the map through the renamer's own
# `load_address_map`. It is ordinary open work now, and marking it would assert
# something false.
#
# ⚠ Do NOT confirm this with `git log -S'"0x826412e0"'`. That reports only
# `62098fc5`, which put a DIFFERENT name there by arbitrary byte-class
# bijection (establishing nothing). `-S` tracks the occurrence COUNT of the
# string, and REPLACING a value leaves the count of the key unchanged, so the
# commit that actually made the identification is invisible to it. Use
# `git log -p -- scripts/target_symbol_map.json | grep -n 826412e0`, or
# `git log -G`.
INCIDENT_2026_08_13 = {
    130424: (
        "?Null@Symbol@@QBA_NXZ",
        "2026-08-13 ICF/alias map-injectivity repair, reinforced 2026-08-16 by "
        "lane R (9fe65045): the retail body this row was scored against was "
        "disproved as this function, and the symbol is absent from "
        "scripts/target_symbol_map.json, so no target body is assigned. The one "
        "candidate home ever proposed, 0x8227c70c, is REFUTED: it is interior "
        "code of ??8Symbol@@QBA_NPBD@Z at 0x8227c6d0 (0x8227c708 branches INTO "
        "it, which cannot cross a COMDAT boundary under /Gy; and it has zero "
        "inbound bl against 297 for 0x8227c6d0, so an out-of-line COMDAT there "
        "would have been /OPT:REF'd away). Note WHY that matters: our build does "
        "emit a standalone ?Null@Symbol@@QBA_NXZ COMDAT byte-identical to that "
        "retail fragment, so re-homing it would have scored a clean byte-exact "
        "100% AGAINST A NON-FUNCTION -- arriving with a correct body argument "
        "and a uniqueness proof attached. A shape match, even a unique one, is "
        "not a function-identity proof. Was AT_LIMIT @ 91.2857%, a certification "
        "of a floor that did not exist. See docs/decomp/VERDICT_STATES.md."
    ),
    130132: (
        "??$__destroy_aux@ULevelData@@@stlpmtx_std@@YAXPAULevelData@@ABU__false_type@0@@Z",
        "2026-08-13 ICF/alias map-injectivity repair: genuinely unadjudicable. "
        "Both candidate VAs (0x82b5b1d0, 0x82b63ec8) tail-jump the same unnamed "
        "fn_82B69220, so the relocation test that separated the other rows "
        "cannot distinguish them; both are on the map _denylist, i.e. the map "
        "deliberately claims neither. Denied rather than guessed. Needs retail "
        "layout for LevelData. Was AT_LIMIT @ 99.5%. "
        "See docs/decomp/VERDICT_STATES.md."
    ),
}


def _row(conn: sqlite3.Connection, fid: int) -> sqlite3.Row | None:
    conn.row_factory = sqlite3.Row
    return conn.execute(
        "SELECT id, symbol, unit, current_percent, best_percent, verdict, "
        "       verdict_reason, excluded FROM functions WHERE id = ?",
        (fid,),
    ).fetchone()


def cmd_list(db: Path) -> int:
    conn = sqlite3.connect(f"file:{db}?mode=ro", uri=True)
    conn.row_factory = sqlite3.Row
    rows = conn.execute(
        "SELECT id, symbol, unit, current_percent, best_percent, excluded, "
        "       verdict_reason FROM functions WHERE verdict = ? ORDER BY id",
        (VERDICT_IDENTITY_UNESTABLISHED,),
    ).fetchall()
    print(f"{VERDICT_IDENTITY_UNESTABLISHED}: {len(rows)} row(s) in {db}\n")
    for r in rows:
        print(f"  id={r['id']}  excluded={r['excluded']}  "
              f"pct={r['current_percent']}  best={r['best_percent']}")
        print(f"    {r['symbol']}")
        print(f"    unit: {r['unit']}")
        print(f"    why : {(r['verdict_reason'] or '')[:160]}")
        print()
    # Integrity: these two must agree, or the belt has come off the braces.
    bad = conn.execute(
        "SELECT COUNT(*) FROM functions WHERE verdict = ? AND "
        "(excluded != 1 OR current_percent IS NOT NULL OR best_percent IS NOT NULL)",
        (VERDICT_IDENTITY_UNESTABLISHED,),
    ).fetchone()[0]
    if bad:
        print(f"  ⚠ {bad} row(s) INCONSISTENT (excluded != 1, or a percent "
              f"survived). Re-run the mark for those ids.")
    empty = conn.execute("SELECT COUNT(*) FROM functions WHERE verdict = ''").fetchone()[0]
    if empty:
        print(f"  ⚠ {empty} row(s) hold the undesigned empty-string verdict.")
    conn.close()
    return 1 if bad else 0


def main(argv: list[str] | None = None) -> int:
    ap = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--db", type=Path, default=DEFAULT_DB,
                    help="Database path (default: repo decomp.db). Point at a "
                         "SCRATCH COPY unless you are the lander.")
    ap.add_argument("--id", type=int, action="append", default=[],
                    help="Function id to act on (repeatable).")
    ap.add_argument("--symbol", action="append", default=[],
                    help="Mangled symbol to act on (repeatable).")
    ap.add_argument("--reason", help="Why. Required when marking or clearing.")
    ap.add_argument("--incident-2026-08-13", action="store_true",
                    help="Apply the two rows from the ICF/alias repair, each "
                         "with its recorded reason. (NOT 130115 -- its identity "
                         "has since been established; see the source.)")
    ap.add_argument("--clear", action="store_true",
                    help="Inverse: return rows to open work (identity established).")
    ap.add_argument("--list", action="store_true", help="Show current rows and exit.")
    ap.add_argument("--apply", action="store_true",
                    help="Actually write. Without this, previews only.")
    args = ap.parse_args(argv)

    if not args.db.exists():
        print(f"Error: database not found: {args.db}", file=sys.stderr)
        return 2

    if args.list:
        return cmd_list(args.db)

    # Resolve the target set.
    targets: dict[int, str | None] = {}
    if args.incident_2026_08_13:
        if args.clear:
            print("Error: --incident-2026-08-13 marks; it cannot clear.", file=sys.stderr)
            return 2
        targets.update({fid: reason for fid, (_sym, reason) in INCIDENT_2026_08_13.items()})
    for fid in args.id:
        targets[fid] = args.reason
    if args.symbol:
        conn = sqlite3.connect(f"file:{args.db}?mode=ro", uri=True)
        for sym in args.symbol:
            row = conn.execute("SELECT id FROM functions WHERE symbol = ?", (sym,)).fetchone()
            if row is None:
                print(f"Error: no row for symbol {sym!r}", file=sys.stderr)
                return 2
            targets[row[0]] = args.reason
        conn.close()

    if not targets:
        print("Nothing to do: pass --id / --symbol / --incident-2026-08-13, "
              "or --list.", file=sys.stderr)
        return 2
    if any(r is None or not r.strip() for r in targets.values()):
        print("Error: --reason is required (and must be non-empty). It is the "
              "only evidence trail for why a row left the work queue.",
              file=sys.stderr)
        return 2

    verb = "CLEAR" if args.clear else "MARK"
    mode = "APPLY" if args.apply else "DRY RUN — nothing will be written"
    print(f"{verb} {len(targets)} row(s) in {args.db}   [{mode}]\n")

    ro = sqlite3.connect(f"file:{args.db}?mode=ro", uri=True)
    for fid in sorted(targets):
        before = _row(ro, fid)
        if before is None:
            print(f"  id={fid}: NO SUCH ROW", file=sys.stderr)
            return 2
        # Loud, because a byte-exact verdict against an unestablished body is
        # the exact thing this state exists to stop -- if one is already
        # recorded, someone should look at it rather than let it be overwritten
        # quietly.
        if not args.clear and before["verdict"] == "COMPLETE":
            print(f"  ⚠ id={fid} is currently COMPLETE. A byte-exact verdict "
                  f"was recorded against a body whose identity you are now "
                  f"saying is unestablished. Marking it retracts that claim.")
        print(f"  id={fid}  {before['symbol'][:78]}")
        print(f"    unit    : {before['unit']}")
        print(f"    before  : verdict={before['verdict']!r} excluded={before['excluded']} "
              f"pct={before['current_percent']} best={before['best_percent']}")
        if args.clear:
            print(f"    after   : verdict=None excluded=0 pct=None best=None")
        else:
            print(f"    after   : verdict={VERDICT_IDENTITY_UNESTABLISHED!r} "
                  f"excluded=1 pct=None best=None")
        print(f"    reason  : {targets[fid][:150]}")
        print()
    ro.close()

    if not args.apply:
        print("DRY RUN — re-run with --apply to write.")
        return 0

    changed = 0
    for fid in sorted(targets):
        if args.clear:
            clear_identity_unestablished(fid, targets[fid], db_path=str(args.db))
        else:
            mark_identity_unestablished(fid, targets[fid], db_path=str(args.db))
        changed += 1
    print(f"{verb}ED {changed} row(s).")

    # Verify what we just wrote rather than trusting the return codes.
    conn = sqlite3.connect(f"file:{args.db}?mode=ro", uri=True)
    conn.row_factory = sqlite3.Row
    for fid in sorted(targets):
        r = _row(conn, fid)
        expect = None if args.clear else VERDICT_IDENTITY_UNESTABLISHED
        ok = (r["verdict"] == expect
              and r["current_percent"] is None
              and r["best_percent"] is None
              and r["excluded"] == (0 if args.clear else 1))
        print(f"  verify id={fid}: verdict={r['verdict']!r} "
              f"excluded={r['excluded']} pct={r['current_percent']} "
              f"best={r['best_percent']}  {'OK' if ok else 'MISMATCH'}")
        if not ok:
            conn.close()
            return 1
    conn.close()
    return 0


if __name__ == "__main__":
    sys.exit(main())
