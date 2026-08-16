#!/usr/bin/env python3
"""Tests for the IDENTITY_UNESTABLISHED verdict state.

Every test builds its own small SQLite DB via `init_database`, so the suite is
portable (no toolchain, no report.json) and can never touch the shared
decomp.db. Run:

    python3 -m pytest scripts/orchestrator/test_verdict_identity.py -q

The point of this file is NOT that the state can be written -- it is that the
state lands in exactly ONE bucket in every consumer, and specifically that it
never rejoins the work queue. A value that is declared but unwired behaves
exactly like the `''` that caused the incident while LOOKING like it has tool
support, so each filter is asserted individually rather than in aggregate.
"""

from __future__ import annotations

import sqlite3
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent))

from orchestrator import database as D  # noqa: E402

IU = D.VERDICT_IDENTITY_UNESTABLISHED


@pytest.fixture()
def db(tmp_path: Path) -> str:
    """A tiny DB with one row per verdict state, all in one unit."""
    path = str(tmp_path / "t.db")
    conn = D.init_database(path)
    rows = [
        (1, "?open@@QAAXXZ", "open", 90.0, None),
        (2, "?complete@@QAAXXZ", "complete", 100.0, "COMPLETE"),
        (3, "?atlimit@@QAAXXZ", "atlimit", 95.0, "AT_LIMIT"),
        (4, "?unident@@QAAXXZ", "unident", 91.2857, None),  # marked below
    ]
    for fid, sym, dem, pct, verdict in rows:
        conn.execute(
            "INSERT INTO functions (id, symbol, demangled, unit, size, "
            "current_percent, best_percent, verdict, attempt_count, excluded, live) "
            "VALUES (?,?,?,?,?,?,?,?,0,0,1)",
            (fid, sym, dem, "default/Test", 100, pct, pct, verdict),
        )
    conn.commit()
    # Row 4 goes through the real API, not hand-written SQL -- if the helper
    # is wrong, every assertion below is testing the wrong thing.
    D.mark_identity_unestablished(4, "test: target body not established", db_path=path)
    return path


# --------------------------------------------------------------------------
# What the state IS
# --------------------------------------------------------------------------

def test_mark_sets_both_axes_and_clears_percents(db):
    conn = sqlite3.connect(db)
    conn.row_factory = sqlite3.Row
    r = conn.execute("SELECT * FROM functions WHERE id=4").fetchone()
    assert r["verdict"] == IU
    assert r["excluded"] == 1, "secondary axis not set; 15 queries filter on it"
    assert r["current_percent"] is None
    assert r["best_percent"] is None, (
        "best_percent survives a monotone MAX() update forever; if the mark "
        "does not NULL it, nothing else can"
    )
    assert r["verdict_reason"]


def test_mark_requires_a_reason(db):
    with pytest.raises(ValueError):
        D.mark_identity_unestablished(1, "", db_path=db)
    with pytest.raises(ValueError):
        D.mark_identity_unestablished(1, "   ", db_path=db)


def test_state_is_not_in_done_verdicts():
    assert IU in D.KNOWN_VERDICTS
    assert IU not in D.DONE_VERDICTS, "must never count as decomp progress"
    assert IU in D.UNWORKABLE_VERDICTS


def test_empty_string_verdict_is_rejected(db):
    """The 2026-08-13 fourth value. It must not be writable through the API."""
    with pytest.raises(ValueError):
        D.update_function_status(1, verdict="", db_path=db)
    with pytest.raises(ValueError):
        D.update_function_status(1, verdict="MADE_UP", db_path=db)


# --------------------------------------------------------------------------
# The CLEAR sentinel (update_function_status could not write NULL)
# --------------------------------------------------------------------------

def test_clear_sentinel_writes_null(db):
    D.update_function_status(3, verdict=D.CLEAR, db_path=db)
    conn = sqlite3.connect(db)
    assert conn.execute("SELECT verdict FROM functions WHERE id=3").fetchone()[0] is None


def test_clear_sentinel_retracts_a_stale_best_percent(db):
    """The monotone MAX() cannot lower best_percent; CLEAR is the only way."""
    D.update_function_status(1, current_percent=10.0, db_path=db)
    conn = sqlite3.connect(db)
    assert conn.execute("SELECT best_percent FROM functions WHERE id=1").fetchone()[0] == 90.0
    D.update_function_status(1, best_percent=D.CLEAR, db_path=db)
    assert conn.execute("SELECT best_percent FROM functions WHERE id=1").fetchone()[0] is None


def test_none_still_means_do_not_update(db):
    """Backward compatibility: every existing caller depends on this."""
    D.update_function_status(2, current_percent=None, verdict=None, db_path=db)
    conn = sqlite3.connect(db)
    r = conn.execute("SELECT current_percent, verdict FROM functions WHERE id=2").fetchone()
    assert r == (100.0, "COMPLETE")


# --------------------------------------------------------------------------
# It must never be offered as work -- one assertion per selector
# --------------------------------------------------------------------------

def _ids(rows):
    return {r["id"] for r in rows}


def test_get_next_function_never_returns_it(db):
    # Bracket the percent so the row would be the sole candidate if admitted.
    # Its percent is NULL after marking, so also check the NULL-percent path.
    for kwargs in (
        dict(),
        dict(exclude_at_limit=True),
        dict(exclude_complete=False),
        dict(min_percent=91.0, max_percent=92.0),
    ):
        r = D.get_next_function(pattern="default/Test", db_path=db, **kwargs)
        assert r is None or r["id"] != 4, f"leaked via get_next_function({kwargs})"


def test_query_functions_never_returns_it(db):
    for kwargs in (
        dict(),
        dict(exclude_at_limit=True),
        dict(exclude_complete=False, exclude_at_limit=False),  # "status=all"
        dict(verdict_filter="COMPLETE"),
        dict(verdict_filter="AT_LIMIT"),
    ):
        rows = D.query_functions(pattern="default/Test", limit=100, db_path=db, **kwargs)
        assert 4 not in _ids(rows), f"leaked via query_functions({kwargs})"


def test_query_functions_can_audit_it_by_name(db):
    """The one view that must show it, or the state is unobservable."""
    rows = D.query_functions(pattern="default/Test", limit=100,
                             verdict_filter=IU, db_path=db)
    assert _ids(rows) == {4}


def test_audit_view_is_not_narrowed_by_worklist_noise_filters(db):
    """Regression: the audit view under-reported by half on the real DB.

    The worklist suppressions (stlpmtx_std templates, boilerplate prefixes,
    dead rows) exist to keep noise out of WORK selection. Applied to the audit
    path they hide rows that someone is specifically supposed to come and
    adjudicate -- measured against the live data, asking for
    IDENTITY_UNESTABLISHED returned 1 of the 2 rows in that state, because the
    other is `??$__destroy_aux@ULevelData@@@stlpmtx_std@@...`.
    """
    conn = sqlite3.connect(db)
    conn.execute(
        "INSERT INTO functions (id, symbol, demangled, unit, size, verdict, "
        "excluded, live) VALUES (5, '??$__destroy_aux@ULevelData@@@stlpmtx_std@@YAXXZ', "
        "'stlpmtx_std::__destroy_aux<LevelData>', 'default/Test', 8, NULL, 0, 0)"
    )
    conn.commit()
    D.mark_identity_unestablished(5, "test: STL template, dead, unadjudicable", db_path=db)

    audited = _ids(D.query_functions(pattern="default/Test", limit=100,
                                     verdict_filter=IU, skip_boilerplate=True,
                                     db_path=db))
    assert audited == {4, 5}, f"audit view under-reported: {audited}"

    # ...and it is still absent from every work view.
    assert not _ids(D.query_functions(pattern="default/Test", limit=100,
                                      db_path=db)) & {4, 5}


def test_query_batch_stats_does_not_count_it_as_available(db):
    s = D.query_batch_stats(pattern="default/Test", db_path=db)
    # ids 1 (open) is available; 2 COMPLETE excluded; 3 AT_LIMIT admitted by
    # default; 4 must not be.
    assert s["identity_unestablished"] == 1
    rows = D.query_functions(pattern="default/Test", limit=100, db_path=db)
    assert s["available"] == len(rows), (
        "available must agree with what query_functions would hand out"
    )


def test_priority_and_unit_completion_selectors_exclude_it(db):
    """⚠ Both selectors are DEAD CODE as shipped, on a fresh DB and on the real
    one: they SELECT ease_score / impact_score / confidence_score, which exist
    in neither (measured 2026-08-16 against a copy of the live decomp.db --
    both raise `no such column: ease_score`). The columns are provisioned here
    so the exclusion wiring is still verified; if the selectors are ever
    repaired, this test already covers them. Do not read a pass here as
    evidence that they run in production.
    """
    conn = sqlite3.connect(db)
    for col in ("ease_score", "impact_score", "confidence_score"):
        conn.execute(f"ALTER TABLE functions ADD COLUMN {col} REAL")
    conn.execute("UPDATE functions SET priority_score=99, reachable_100=1, "
                 "ease_score=1, impact_score=1, confidence_score=1")
    conn.commit()
    assert 4 not in _ids(D.query_functions_by_priority(db_path=db, limit=100))
    assert 4 not in _ids(
        D.query_functions_for_unit_completion(db_path=db, limit=100,
                                              min_completion_pct=0,
                                              max_completion_pct=100)
    )


def test_divergent_logic_selector_excludes_it(db):
    """`verdict IS NULL` gets this right by accident -- pin it so it stays right."""
    conn = sqlite3.connect(db)
    conn.execute(
        "UPDATE functions SET unicorn_verdict='DIVERGENT', unicorn_class='logic', "
        "has_linker_merged=0, excluded=0"
    )
    conn.commit()
    assert 4 not in _ids(D.query_divergent_logic(db_path=db, limit=100))


# --------------------------------------------------------------------------
# Aggregates: the meaningless percentage must not reach any average
# --------------------------------------------------------------------------

def test_avg_percent_excludes_it_even_with_a_stale_percent(db):
    """Belt and braces: force a percent back on and prove it still cannot skew."""
    before = D.get_stats(db)["avg_percent"]
    conn = sqlite3.connect(db)
    conn.execute("UPDATE functions SET current_percent=0.0 WHERE id=4")
    conn.commit()
    after = D.get_stats(db)["avg_percent"]
    assert before == after, (
        "a stale percent on an IDENTITY_UNESTABLISHED row reached avg_percent; "
        "this is exactly what '' did in 2026-08-13"
    )


def test_get_stats_counts_it_separately_and_not_as_done(db):
    s = D.get_stats(db)
    assert s["identity_unestablished"] == 1
    assert s["complete"] == 1
    assert s["at_limit"] == 1


# --------------------------------------------------------------------------
# Raw-SQL consumers: assert the shipped predicates verbatim
# --------------------------------------------------------------------------

def _load_real(name: str, relpath: str):
    """Load a shipped module BY PATH, so the test binds to real code.

    Re-spelling a consumer's SQL as a literal in the test and appending the
    clause here would pass even if someone deleted the clause from the shipped
    file -- it would only prove the clause works, which is not the claim. The
    claim is that the consumer USES it.
    """
    import importlib.util
    spec = importlib.util.spec_from_file_location(name, HERE.parent / relpath)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class _FakeRun:
    """Stand-in for subprocess.run that records the symbol argv and fails the
    call benignly (returncode 1), so the consumer's own error path runs and no
    toolchain is needed."""

    def __init__(self):
        self.symbols: list[str] = []

    def __call__(self, argv, *a, **k):
        self.symbols.extend(argv)
        return type("R", (), {"returncode": 1, "stdout": "", "stderr": ""})()


def test_batch_check_does_not_select_it(db, monkeypatch, tmp_path):
    """Drives the REAL scripts/batch_check.py, which auto-stamps COMPLETE at 100%.

    Carries a POSITIVE CONTROL. An earlier version of this test asserted only
    "nothing was selected" and passed even with the filter deleted, because
    `normalize_unit_pattern('Only')` returns 'Only' and never matches
    'default/Only' -- it was selecting zero rows for pattern reasons and
    proving nothing. Asserting that the open row IS reached makes that
    impossible.
    """
    conn = sqlite3.connect(db)
    conn.execute("DELETE FROM functions WHERE id NOT IN (1, 4)")
    conn.execute("UPDATE functions SET unit='default/Only'")
    conn.commit()
    iu_sym, open_sym = (conn.execute(
        "SELECT symbol FROM functions WHERE id=?", (i,)).fetchone()[0] for i in (4, 1))

    bc = _load_real("_laneS_batch_check", "batch_check.py")
    fake = _FakeRun()
    monkeypatch.setattr(bc, "DB_PATH", db)
    monkeypatch.setattr(bc, "OBJDIFF_CLI", tmp_path)  # must merely exist
    monkeypatch.setattr(bc, "subprocess", type("S", (), {"run": fake,
                                                         "TimeoutExpired": TimeoutError})())
    bc.batch_check("default/Only")

    assert open_sym in fake.symbols, (
        "positive control failed: batch_check never reached the ordinary open "
        "row, so this test cannot distinguish a working filter from a query "
        "that matched nothing"
    )
    assert iu_sym not in fake.symbols, "batch_check selected the IU row"


def test_atexit_verify_does_not_select_it(db, monkeypatch, tmp_path):
    """Drives the REAL scripts/atexit_fuzzy_verify.py.

    With --mark-at-limit this tool stamps AT_LIMIT, i.e. certifies a floor.
    Certifying a floor against a body not established to be the function is the
    2026-08-13 mis-certification exactly. Positive control as above.
    """
    conn = sqlite3.connect(db)
    conn.execute("DELETE FROM functions WHERE id NOT IN (1, 4)")
    conn.execute("UPDATE functions SET unit='default/Only', "
                 "symbol='??__F' || CAST(id AS TEXT) || '@@YAXXZ'")
    conn.commit()
    iu_sym, open_sym = (conn.execute(
        "SELECT symbol FROM functions WHERE id=?", (i,)).fetchone()[0] for i in (4, 1))

    av = _load_real("_laneS_atexit", "atexit_fuzzy_verify.py")
    fake = _FakeRun()
    monkeypatch.setattr(av, "DB_PATH", db)
    monkeypatch.setattr(av, "OBJDIFF_CLI", tmp_path)
    monkeypatch.setattr(av, "subprocess", type("S", (), {"run": fake,
                                                         "TimeoutExpired": TimeoutError})())
    av.verify(None, apply=False, mark_at_limit=True)

    assert open_sym in fake.symbols, "positive control failed: no row was reached"
    assert iu_sym not in fake.symbols, "atexit_fuzzy_verify selected the IU row"


def test_ghidra_export_query_as_shipped_excludes_it(db):
    """Executes the SQL LIFTED FROM the shipped build_symbol_map.py source.

    That file's main() needs report.json and built objects, so it cannot be
    driven here. Extracting its query text instead still binds the assertion to
    the real file: delete the guard there and this fails.
    """
    import re
    src = (HERE.parent.parent / "tools/ghidra/build_symbol_map.py").read_text()
    start = src.index("rows = con.execute(")
    end = src.index(".fetchall()", start)
    call = src[start:end]
    # Only the string literals inside the call itself -- not the comments above
    # it, which is what a looser pattern picked up.
    sql = "".join(re.findall(r'"([^"]*)"', call))
    assert sql.lstrip().startswith("SELECT"), f"extraction missed the query: {sql!r}"
    # We read raw source, so a Python-level "\\" is still two characters here;
    # SQLite's ESCAPE wants the one character Python would have produced.
    sql = sql.replace("\\\\", "\\")
    assert "IDENTITY_UNESTABLISHED" in sql, (
        f"shipped Ghidra export query has no identity guard: {sql!r}"
    )

    conn = sqlite3.connect(db)
    conn.execute("UPDATE functions SET current_percent=95.0 WHERE id=4")
    conn.commit()
    got = {r[0] for r in conn.execute(
        sql.replace("SELECT symbol, unit, current_percent", "SELECT id"), (80.0,))}
    assert 4 not in got


def test_search_by_name_does_not_suggest_it(db):
    """The suggestion surface behind the run_objdiff MCP tool.

    Not a worklist, which is why it was missed on the first pass: reached from
    `mcp_server._suggest_similar_symbols` when an agent's symbol lookup fails,
    so the server would answer "did you mean ...?" with the one target the
    agent must not work.
    """
    assert not [r for r in D.search_functions_by_name("unident", limit=10, db_path=db)]
    conn = sqlite3.connect(db)
    sym = conn.execute("SELECT symbol FROM functions WHERE id=4").fetchone()[0]
    assert not [r for r in D.search_functions_by_name(sym, limit=10, db_path=db)]


def test_merged_category_selector_excludes_it(db):
    conn = sqlite3.connect(db)
    conn.execute("INSERT INTO merged_symbols (function_id, symbol_name, category) "
                 "VALUES (4, 'x', 'addtostrings')")
    # Clear the belt so the primary axis is what is under test.
    conn.execute("UPDATE functions SET excluded=0 WHERE id=4")
    conn.commit()
    rows = D.query_functions_by_merged_category("addtostrings", db_path=db, limit=100)
    assert 4 not in _ids(rows)


def test_public_api_cannot_create_a_half_state(db):
    """update_function_status writes ONE column; this state needs four.

    Measured before the guard: update_function_status(verdict=IU) produced
    ('IDENTITY_UNESTABLISHED', 0, 91.2857, 91.2857) -- a row that
    obj_regswap_patcher (excluded=0-only) still offers and the near-100 band
    still counts.
    """
    with pytest.raises(ValueError, match="mark_identity_unestablished"):
        D.update_function_status(1, verdict=IU, db_path=db)
    conn = sqlite3.connect(db)
    assert conn.execute("SELECT verdict FROM functions WHERE id=1").fetchone()[0] is None


def test_grind_worklist_dead_verdicts_covers_it():
    """A closed allow-list: an unlisted verdict silently stays in the worklist."""
    # Loaded by path rather than by `sys.path.insert` + `import worklist`:
    # putting scripts/ or scripts/grind/ on sys.path shadows generically-named
    # third-party packages for every other test in the same pytest session
    # (scripts/unicorn/ vs the unicorn emulator being the live example here).
    import importlib.util
    spec = importlib.util.spec_from_file_location(
        "_laneS_worklist", HERE.parent / "grind" / "worklist.py")
    worklist = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(worklist)
    assert IU.lower() in worklist._DEAD_VERDICTS


def test_every_known_verdict_is_either_done_or_unworkable():
    """No verdict may sit in no bucket -- that is the '' defect, generalised."""
    for v in D.KNOWN_VERDICTS:
        assert v in D.DONE_VERDICTS or v in D.UNWORKABLE_VERDICTS, (
            f"{v!r} is in neither DONE_VERDICTS nor UNWORKABLE_VERDICTS, so it "
            f"sits in no bucket while still being a legal value"
        )


# --------------------------------------------------------------------------
# The inverse
# --------------------------------------------------------------------------

def test_clear_returns_row_to_work_and_leaves_percents_null(db):
    D.clear_identity_unestablished(4, "identity established: 0xDEADBEEF", db_path=db)
    conn = sqlite3.connect(db)
    conn.row_factory = sqlite3.Row
    r = conn.execute("SELECT * FROM functions WHERE id=4").fetchone()
    assert r["verdict"] is None
    assert r["excluded"] == 0
    assert r["current_percent"] is None, "must be re-measured, not restored"
    rows = D.query_functions(pattern="default/Test", limit=100, db_path=db)
    assert 4 in _ids(rows), "clearing must actually return the row to work"


def test_clear_refuses_a_row_that_is_not_in_the_state(db):
    """So the inverse cannot silently un-exclude a funclet or a floor."""
    with pytest.raises(ValueError):
        D.clear_identity_unestablished(3, "nope", db_path=db)  # AT_LIMIT
