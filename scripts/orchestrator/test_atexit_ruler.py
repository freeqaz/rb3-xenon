#!/usr/bin/env python3
"""Tests for the RULER `scripts/atexit_fuzzy_verify.py` closes rows on.

Lane ATEXIT-RULER, 2026-09-11.

The defect: the tool drove objdiff with a hardcoded
`-c functionRelocDiffs=none` and wrote `verdict=COMPLETE` when
`instruction_summary.equal_percent >= 100`. `none` is blind to relocation-NAME
divergence, and COMPLETE closes a row -- so a permissive ruler could close rows
the grader scores below 100.

Every payload below is the REAL measurement from this tree, not an invention:

    ??__FsFrames@@YAXXZ  (default/SkeletonClip, 28 B)
        functionRelocDiffs=none        equal_percent = 100.00, fuzzy = 100.000
        functionRelocDiffs=name_check  equal_percent =  71.43, fuzzy =  98.571

    The 2 charged sites are `diff_arg` and are a genuine callee divergence:
        target: lis r11, ??1?$ObjDirPtr@VObjectDir@@@@UAA@XZ@h
        base:   lis r11, ??1?$vector@URecordedFrame@@...@XZ@h

So a fake objdiff that answers differently per ruler reproduces the defect
exactly, with no toolchain and no real DB.

⚠ Each test carries a control that can FAIL, because the failure mode of this
whole family of tests is passing vacuously -- "nothing was promoted" is the
trivially-passing state for a tool whose bug is promoting too much.
"""

from __future__ import annotations

import importlib.util
import json
import re
import sqlite3
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
REPO = HERE.parent.parent
sys.path.insert(0, str(HERE.parent))

from orchestrator import database as D  # noqa: E402

TOOL = REPO / "scripts" / "atexit_fuzzy_verify.py"

# The false row (real measurement) and a genuinely-complete row.
FALSE_SYM = "??__FsFrames@@YAXXZ"
GOOD_SYM = "??__FgHiResScreen@@YAXXZ"

PAYLOADS = {
    # symbol -> (permissive `none` payload, graded `name_check` payload)
    FALSE_SYM: (
        {"base_size": 28, "target_size": 28, "fuzzy_match_percent": 100.0,
         "instruction_summary": {"equal_percent": 100.0}, "verdict": {}},
        {"base_size": 28, "target_size": 28, "fuzzy_match_percent": 98.571,
         "instruction_summary": {"equal_percent": 71.43}, "verdict": {}},
    ),
    GOOD_SYM: (
        {"base_size": 28, "target_size": 28, "fuzzy_match_percent": 100.0,
         "instruction_summary": {"equal_percent": 100.0}, "verdict": {}},
        {"base_size": 28, "target_size": 28, "fuzzy_match_percent": 100.0,
         "instruction_summary": {"equal_percent": 100.0}, "verdict": {}},
    ),
}


def _load_tool():
    """Load the SHIPPED tool by path, so the test binds to real code.

    Re-implementing the gate in the test would prove the gate works, which is
    not the claim. The claim is that the SHIPPED tool uses it.
    """
    spec = importlib.util.spec_from_file_location("_atexit_ruler_tool", TOOL)
    mod = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(mod)
    return mod


class _RulerAwareObjdiff:
    """Fake subprocess.run that answers according to the ruler in argv.

    This is the whole point of the fixture: a tool that asks for `none` gets
    the permissive answer, a tool that asks for the graded ruler gets the
    honest one. A tool that hardcodes `none` therefore CANNOT pass.
    """

    def __init__(self):
        self.calls: list[list[str]] = []

    def __call__(self, argv, *a, **k):
        self.calls.append(list(argv))
        symbol = next((x for x in argv if x.startswith("??__F")), None)
        if symbol not in PAYLOADS:
            return type("R", (), {"returncode": 1, "stdout": "", "stderr": ""})()
        permissive, graded = PAYLOADS[symbol]
        asked_none = any("functionRelocDiffs=none" in str(x) for x in argv)
        payload = permissive if asked_none else graded
        return type("R", (), {"returncode": 0,
                              "stdout": json.dumps(payload) + "\n",
                              "stderr": ""})()

    def decision_calls(self, symbol: str) -> list[list[str]]:
        """Calls for `symbol` that were NOT the permissive control leg."""
        return [c for c in self.calls
                if symbol in c
                and not any("functionRelocDiffs=none" in str(x) for x in c)]


@pytest.fixture()
def db(tmp_path: Path) -> str:
    path = str(tmp_path / "t.db")
    conn = D.init_database(path)
    for fid, sym in ((1, FALSE_SYM), (2, GOOD_SYM)):
        conn.execute(
            "INSERT INTO functions (id, symbol, demangled, unit, size, "
            "current_percent, best_percent, verdict, attempt_count, excluded, live) "
            "VALUES (?,?,?,?,?,?,?,NULL,0,0,1)",
            (fid, sym, sym, "default/Test", 28, 90.0, 90.0),
        )
    conn.commit()
    conn.close()
    return path


def _verdict(db_path: str, symbol: str):
    conn = sqlite3.connect(db_path)
    row = conn.execute("SELECT verdict FROM functions WHERE symbol=?",
                       (symbol,)).fetchone()
    conn.close()
    return row[0] if row else None


def _run(mod, db_path, tmp_path, monkeypatch, **kw):
    fake = _RulerAwareObjdiff()
    monkeypatch.setattr(mod, "DB_PATH", db_path)
    monkeypatch.setattr(mod, "OBJDIFF_CLI", tmp_path)  # must merely exist
    monkeypatch.setattr(mod, "subprocess",
                        type("S", (), {"run": fake,
                                       "TimeoutExpired": TimeoutError})())
    mod.verify(None, apply=True, **kw)
    return fake


# ---------------------------------------------------------------------------
# THE DEFECT
# ---------------------------------------------------------------------------

def test_permissive_ruler_cannot_close_a_row(db, monkeypatch, tmp_path):
    """A row that only reads 100% under `none` must NOT be marked COMPLETE.

    Pre-fix this FAILS: the tool asks for `functionRelocDiffs=none`, gets
    equal_percent=100.0, and writes verdict=COMPLETE on a row the grader
    scores at fuzzy 98.571.

    The GOOD_SYM assertion is the positive control: without it this test would
    also pass against a tool that promotes nothing at all, which is the
    trivially-passing state and proves nothing.
    """
    mod = _load_tool()
    _run(mod, db, tmp_path, monkeypatch)

    assert _verdict(db, GOOD_SYM) == "COMPLETE", (
        "positive control failed: the genuinely-complete row was not promoted, "
        "so this test cannot distinguish a correct gate from a tool that never "
        "promotes anything"
    )
    assert _verdict(db, FALSE_SYM) != "COMPLETE", (
        f"{FALSE_SYM} was closed as COMPLETE, but it only reads 100% under the "
        f"permissive `functionRelocDiffs=none` ruler; the graded ruler scores "
        f"it fuzzy=98.571 (2 diff_arg sites -- retail destroys "
        f"ObjDirPtr<ObjectDir>, we destroy vector<RecordedFrame>). A permissive "
        f"ruler must never be able to close a row."
    )


def test_decision_leg_runs_on_the_graded_ruler(db, monkeypatch, tmp_path):
    """The leg the verdict is taken from must carry the GRADED reloc mode."""
    from analysis.ruler import resolve_ruler
    graded = resolve_ruler(REPO, "graded")

    mod = _load_tool()
    fake = _run(mod, db, tmp_path, monkeypatch)

    decision = fake.decision_calls(GOOD_SYM)
    assert decision, (
        "no non-permissive objdiff call was made for the control symbol, so "
        "the verdict cannot have been taken on the graded ruler"
    )
    flat = " ".join(decision[0])
    assert f"functionRelocDiffs={graded.reloc_mode}" in flat, (
        f"decision leg did not carry the graded ruler "
        f"(functionRelocDiffs={graded.reloc_mode}); argv was: {flat}"
    )


def test_at_limit_is_also_priced_on_the_graded_ruler(db, monkeypatch, tmp_path):
    """AT_LIMIT certifies a FLOOR, so it is a closing verdict too.

    FALSE_SYM reads graded fuzzy 98.571 (>= 95), so with --mark-at-limit it is
    legitimately AT_LIMIT -- but it must never be COMPLETE.
    """
    mod = _load_tool()
    _run(mod, db, tmp_path, monkeypatch, mark_at_limit=True)
    assert _verdict(db, FALSE_SYM) == "AT_LIMIT"
    assert _verdict(db, GOOD_SYM) == "COMPLETE"


def test_refuses_to_apply_when_ruler_is_not_authoritative(db, monkeypatch, tmp_path):
    """`objdiff.json` is gitignored: a tree with no report.json falls back to
    `report generate`'s base config, whose functionRelocDiffs is `none`.

    Without this refusal a fresh checkout silently restores the defect.
    """
    mod = _load_tool()
    from analysis.ruler import Ruler

    def fake_resolve(_project, selector="graded"):
        return Ruler(reloc_mode="none",
                     config={"functionRelocDiffs": "none"},
                     source="report-generate base only (FALLBACK)",
                     selector=selector,
                     graded_reloc_mode="none",
                     authoritative=False,
                     warning="THE RULER IS UNVERIFIED")

    monkeypatch.setattr(mod, "resolve_ruler", fake_resolve)
    monkeypatch.setattr(mod, "DB_PATH", db)
    monkeypatch.setattr(mod, "OBJDIFF_CLI", tmp_path)

    with pytest.raises(SystemExit) as exc:
        mod.verify(None, apply=True)
    assert exc.value.code == 2
    assert _verdict(db, GOOD_SYM) is None, (
        "refusal must happen BEFORE any write; a row was already closed"
    )


# ---------------------------------------------------------------------------
# STRUCTURAL -- the constant that rotted
# ---------------------------------------------------------------------------

def test_no_hardcoded_ruler_in_source():
    """The original defect was a CONSTANT, not a logic error.

    Mirrors `scripts/analysis/ruler.py`'s own regression guard, which this tool
    is now registered with in `_CONSUMERS`.
    """
    text = TOOL.read_text()
    pattern = re.compile(r"""["']-c["']\s*,\s*["']functionRelocDiffs=""")
    hits = [i + 1 for i, line in enumerate(text.splitlines())
            if pattern.search(line)]
    assert not hits, (
        f"{TOOL.name} hardcodes a ruler at line(s) {hits}; resolve it at "
        f"runtime via scripts/analysis/ruler.py instead"
    )


def test_tool_is_registered_with_the_ruler_guard():
    """An omission from `_CONSUMERS` is invisible -- that is how this tool and
    `tools/crossing_worklist.py` both survived the flip."""
    from analysis.ruler import _CONSUMERS
    assert "scripts/atexit_fuzzy_verify.py" in _CONSUMERS
