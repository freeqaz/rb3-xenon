#!/usr/bin/env python3
"""crossing_worklist: print symbols WHOLE, and key the diff cache on the INSTRUMENT.

TWO DEFECTS, both of which cost a lane real yield (lane W3-F, 2026-09-11).

1. THE SYMBOL COLUMN TRUNCATED AT 58 CHARS, SILENTLY.
   Lane L5-SYMBOLHEADS handoff 5 (docs/decomp/SYMBOL_HEADS_2026-09-10.md): row
   12's real name is
   `?SelectNode@MusicLibrary@@QAAXPAVSortNode@@PAVLocalBandUser@@_N@Z` (65
   chars). The cut landed mid-token at `...PAVLocalBandUse`, and because there
   was no ellipsis the name was RECONSTRUCTED to a plausible mangled terminator
   -- `...PAVLocalBandUser@@@Z`, well-formed, 63 chars, wrong in its last five.
   objdiff then answers "Symbol not found in target", which reads like a PHANTOM
   ROW (dtk mis-carve -- a class this project really has) rather than a copy
   error, so the row is written off instead of retried. The briefed string and
   the real one share exactly the first 58 characters; that is the proof of
   mechanism, and `test_the_reinstated_cut_reproduces_the_l5_prefix` pins it.

2. THE DIFF CACHE WAS KEYED ON THE RULER BUT NOT ON THE OBJDIFF BINARY.
   CACHE_FORMAT v3's own comment said "a cached measurement is only comparable
   to a fresh one if it was taken with the same instrument" and then keyed on
   the ruler -- the instrument's CONFIGURATION. `bin/objdiff-cli` is a symlink
   into a shared build tree and is swapped in place (main's log records two
   swaps in one day), and a swap changes mismatch counts and charged-site kinds
   while leaving `sym`, `unit` and the ruler key identical.

   The cache tests below prove the key by OBSERVED RE-MINT -- a stub objdiff
   that counts its own invocations -- not by inspecting the key string. A test
   that asserts on the digest would pass even if diff_one never consulted it.
"""

import json
import os
import sys
from pathlib import Path

import pytest

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))
sys.path.insert(0, str(TOOLS.parent / "scripts"))

import crossing_worklist as C  # noqa: E402


# ── 1. full names ────────────────────────────────────────────────────────────

def _row(sym):
    return {"size": 2260, "mm": 1, "arms": ["SYMBOL"], "fz": 99.5,
            "cls": "SYMBOL", "unit": "default/MusicLibrary", "sym": sym}


@pytest.fixture(autouse=True)
def _no_self_break():
    """The truncation knob is module state; never leak it between tests."""
    saved = C.SELF_BREAK_SYM_TRUNC
    C.SELF_BREAK_SYM_TRUNC = None
    yield
    C.SELF_BREAK_SYM_TRUNC = saved


def test_the_witness_is_longer_than_the_old_column():
    """Anti-vacuity: a witness shorter than 58 could not detect the defect."""
    assert len(C.L5_TRUNCATION_WITNESS) == 65
    assert len(C.L5_TRUNCATION_WITNESS) > C.OLD_SYM_COLUMN == 58


def test_full_name_survives_rendering():
    assert C.L5_TRUNCATION_WITNESS in C.format_worklist_row(
        _row(C.L5_TRUNCATION_WITNESS))


def test_the_reinstated_cut_reproduces_the_l5_prefix():
    """--self-break must reproduce the ACTUAL failure, not a lookalike.

    The briefed (wrong) name and the real one share exactly 58 characters, so
    the reinstated cut must end `...PAVLocalBandUse`.
    """
    C.SELF_BREAK_SYM_TRUNC = C.OLD_SYM_COLUMN
    line = C.format_worklist_row(_row(C.L5_TRUNCATION_WITNESS))
    assert C.L5_TRUNCATION_WITNESS not in line
    assert line.rstrip().endswith("PAVLocalBandUse")
    briefed = "?SelectNode@MusicLibrary@@QAAXPAVSortNode@@PAVLocalBandUser@@@Z"
    assert briefed[:58] == C.L5_TRUNCATION_WITNESS[:58], (
        "the two names must share exactly the first 58 chars -- that shared "
        "prefix IS the evidence that a 58-char column produced the briefed name")


def test_a_long_symbol_does_not_get_reordered_before_fixed_width_columns():
    """The symbol goes LAST, so a long one cannot misalign the columns."""
    line = C.format_worklist_row(_row(C.L5_TRUNCATION_WITNESS))
    assert line.index("default/MusicLibrary") < line.index(C.L5_TRUNCATION_WITNESS)
    assert line.rstrip().endswith(C.L5_TRUNCATION_WITNESS)


def test_no_symbol_slice_remains_in_the_source():
    """Guard the whole file, not just the one column that was reported.

    Four more truncations existed elsewhere (the STABLE MISS diagnostic, the pin
    nominee list, control 1b's subject, a control's FAIL list) -- all of them
    lines whose entire purpose is to name a symbol a human must re-run.
    """
    # CODE lines only. The file deliberately QUOTES the old slice in a comment
    # ("this column used to print `r['sym'][:58]`") because a fix whose reason
    # is not written down gets undone -- so a naive whole-file scan fails on the
    # documentation, which is how a guard gets weakened instead of a defect
    # fixed. Full-line comments are dropped; a real slice cannot hide in one.
    code = "\n".join(l for l in (TOOLS / "crossing_worklist.py").read_text()
                     .splitlines() if not l.lstrip().startswith("#"))
    for bad in ("r['sym'][:", 'r["sym"][:', "{s[:", "{sym[:"):
        assert bad not in code, f"a symbol truncation came back: {bad}"


# ── 2. the cache key carries the instrument ──────────────────────────────────

STUB = """#!/bin/sh
echo x >> "$COUNTER"
echo '{"sections":[],"functions":[]}'
"""


@pytest.fixture
def fake_project(tmp_path):
    """A project dir with a COUNTING stub objdiff-cli and a real report.json."""
    (tmp_path / "bin").mkdir()
    binp = tmp_path / "bin" / "objdiff-cli"
    binp.write_text(STUB)
    binp.chmod(0o755)
    rep = tmp_path / "build" / "45410914"
    rep.mkdir(parents=True)
    (rep / "report.json").write_text(json.dumps({
        "measures": {"total_code": "100"},
        "units": [],
        "provenance": {"tool_version": "4.2.8",
                       "tool_binary_hash": "aaaaaaaaaaaaaaaa",
                       "diff_config": ["functionRelocDiffs=name_check"]},
    }))
    return tmp_path


def _calls(counter):
    return sum(1 for _ in open(counter)) if os.path.exists(counter) else 0


def _diff(proj, cache, counter):
    os.environ["COUNTER"] = str(counter)
    C._INSTRUMENT_CACHE.clear()
    return C.diff_one(str(proj), "?Sym@@QAAXXZ", "default/U", str(cache))


def test_a_second_identical_call_is_served_from_cache(fake_project, tmp_path):
    """Control: without this, every re-mint test below is trivially true."""
    cache, counter = tmp_path / "c", tmp_path / "n"
    assert _diff(fake_project, cache, counter) is not None
    assert _calls(counter) == 1
    _diff(fake_project, cache, counter)
    assert _calls(counter) == 1, "the cache did not serve a repeat call"


def test_a_new_report_binary_hash_forces_a_re_mint(fake_project, tmp_path):
    """THE defect: objdiff swapped, report regenerated, same sym/unit/ruler."""
    cache, counter = tmp_path / "c", tmp_path / "n"
    _diff(fake_project, cache, counter)
    assert _calls(counter) == 1
    rp = fake_project / "build" / "45410914" / "report.json"
    d = json.loads(rp.read_text())
    d["provenance"]["tool_binary_hash"] = "bbbbbbbbbbbbbbbb"
    rp.write_text(json.dumps(d))
    _diff(fake_project, cache, counter)
    assert _calls(counter) == 2, (
        "an entry minted by a DIFFERENT objdiff binary was served as if it were "
        "a measurement of this one")


def test_a_swapped_live_binary_forces_a_re_mint_even_with_a_stale_report(
        fake_project, tmp_path):
    """The case keying on report.json ALONE would miss.

    A binary swapped under a report.json nobody has regenerated: the report's
    hash is unchanged, so only the LIVE binary's content distinguishes them.
    """
    cache, counter = tmp_path / "c", tmp_path / "n"
    _diff(fake_project, cache, counter)
    assert _calls(counter) == 1
    binp = fake_project / "bin" / "objdiff-cli"
    binp.write_text(STUB + "# a different binary\n")
    binp.chmod(0o755)
    _diff(fake_project, cache, counter)
    assert _calls(counter) == 2


def test_the_entry_records_the_instrument_it_was_minted_by(fake_project, tmp_path):
    cache, counter = tmp_path / "c", tmp_path / "n"
    _diff(fake_project, cache, counter)
    blobs = [json.load(open(p)) for p in cache.glob("*.json")]
    assert len(blobs) == 1
    assert blobs[0]["_cw_cache"] == C.CACHE_FORMAT == 4
    assert blobs[0]["instrument"] == C.instrument_key(str(fake_project))


def test_a_v3_entry_at_the_same_path_is_not_served(fake_project, tmp_path):
    """Belt and braces: even if a v3 blob landed at a v4 key, refuse it."""
    cache, counter = tmp_path / "c", tmp_path / "n"
    _diff(fake_project, cache, counter)
    path = next(cache.glob("*.json"))
    blob = json.load(open(path))
    blob["_cw_cache"] = 3
    del blob["instrument"]
    path.write_text(json.dumps(blob))
    _diff(fake_project, cache, counter)
    assert _calls(counter) == 2


def test_an_unknown_instrument_keys_as_unknown_not_as_absent(tmp_path):
    """A missing provenance must not collide with a known instrument's key."""
    (tmp_path / "bin").mkdir()
    b = tmp_path / "bin" / "objdiff-cli"
    b.write_text(STUB)
    b.chmod(0o755)
    (tmp_path / "build" / "45410914").mkdir(parents=True)
    (tmp_path / "build" / "45410914" / "report.json").write_text("{}")
    C._INSTRUMENT_CACHE.clear()
    k = C.instrument_key(str(tmp_path))
    assert k and len(k) == 10


def test_cache_note_reports_orphans_and_the_reclaim_command(tmp_path, capsys):
    d = tmp_path / "diffs"
    d.mkdir()
    (d / "a.json").write_text("{}")
    C._CACHE_NOTE_DONE.clear()
    C.cache_note(str(d), out=sys.stdout)
    out = capsys.readouterr().out
    assert "UNREACHABLE" in out and "rm -rf" in out
    # said ONCE per dir, not once per row
    C.cache_note(str(d), out=sys.stdout)
    assert capsys.readouterr().out == ""


if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-q"]))
