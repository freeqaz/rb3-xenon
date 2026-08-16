"""Tests for pdata_map_audit's ANTI-VACUITY CONTRACT.

`--selftest --sabotage shift` MUST fail.  That leg is the control that makes
every green `--selftest` mean anything, and it was VACUOUS IN EVERY WORKTREE
(lane P, 2026-08-16): its only discriminating check compared extents against
the gitignored `fingerprints.json`, which never travels, so the check `[SKIP]`ped
and the run printed a bare `OK` and exited 0.  Clean and sabotaged output were
byte-identical but for the `shift=` header.

So the property under test is not "the audit works" -- it is "the audit still
NOTICES when you break it, in the environment where lanes actually run it, and
it never says OK about a control that did not run."

Deliberately, almost everything here runs on SYNTHETIC extent tables and needs
no retail binary: a test suite that skipped wholesale when `orig/` is absent
would reproduce the very defect it exists to pin.  Only the two end-to-end
subprocess tests need `band.exe`, and pytest reports their absence as a visible
SKIP rather than as a pass.
"""
import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
from pdata_map_audit import (  # noqa: E402
    DEFAULT_EXE,
    EXIT_CANNOT_RUN,
    EXIT_FAILED,
    EXIT_INCOMPLETE,
    EXPECTED_CONTROLS,
    MIN_COVERED_FRAC,
    Extents,
    check_extents,
)

TOOL = Path(__file__).resolve().parent / "pdata_map_audit.py"
SPANS = [(0x1000, 0x2000)]


# --------------------------------------------------------------------------
# check_extents: the intrinsic, fixture-free controls
# --------------------------------------------------------------------------

def test_null_vector_is_green():
    """The verdict on nothing must be 'nothing', not a crash and not a finding."""
    st = check_extents({}, SPANS)
    assert st["overlaps"] == 0 and st["outside"] == 0 and st["covered"] == 0


def test_disjoint_in_range_table_is_clean():
    ext = {0x1000: 0x10, 0x1010: 0x20, 0x1030: 0x08}
    st = check_extents(ext, SPANS)
    assert st["overlaps"] == 0
    assert st["outside"] == 0


def test_overlapping_extents_are_caught():
    """A body running into its successor is the shape a bad bit-shift produces."""
    ext = {0x1000: 0x100, 0x1010: 0x10}
    st = check_extents(ext, SPANS)
    assert st["overlaps"] == 1
    assert st["first_overlap"] == 0x1000


def test_extent_running_past_the_section_is_caught():
    st = check_extents({0x1F00: 0x400}, SPANS)
    assert st["outside"] == 1


def test_extent_below_all_code_is_caught():
    st = check_extents({0x0800: 0x10}, SPANS)
    assert st["outside"] == 1


def test_multiple_code_spans_are_all_honoured():
    """.pdata legitimately covers non-.text code (BINK), so the bound is the
    UNION of executable sections -- bounding on .text alone fired on 106 real
    extents."""
    spans = [(0x1000, 0x2000), (0x5000, 0x6000)]
    assert check_extents({0x5000: 0x10}, spans)["outside"] == 0
    assert check_extents({0x5000: 0x10}, SPANS)["outside"] == 1


# --------------------------------------------------------------------------
# the null must be capable of FAILING (it used to be a tautology)
# --------------------------------------------------------------------------

def test_interior_of_short_circuits_on_a_pdata_start():
    """Documents the short-circuit that made the old null vacuous.

    This behaviour is CORRECT for the detector -- a .pdata start is by
    definition a real function start -- but it means `interior_of` can never
    flag a sampled key, so the old null returned 0 for ANY table.
    """
    E = Extents({0x1000: 0x100, 0x1010: 0x10})
    assert E.interior_of(0x1010) is None


def test_covered_by_predecessor_does_not_short_circuit():
    """The honest predicate: 0x1010 IS swallowed by 0x1000's bogus extent."""
    E = Extents({0x1000: 0x100, 0x1010: 0x10})
    assert E.covered_by_predecessor(0x1010) == 0x1000


def test_covered_by_predecessor_is_silent_on_a_sane_table():
    E = Extents({0x1000: 0x10, 0x1010: 0x10})
    assert E.covered_by_predecessor(0x1010) is None
    assert E.covered_by_predecessor(0x1000) is None


@pytest.mark.parametrize("ext", [
    {0x1000 + 4 * i: 0 for i in range(200)},           # all lengths zero
    {0x1000 + 4 * i: 1 << 20 for i in range(200)},     # all lengths absurd
])
def test_old_null_passed_on_garbage_tables(ext):
    """Regression pin: the OLD null said PASS on both of these.

    Kept as an explicit record of why the null was replaced -- if someone
    reverts to sampling `interior_of`, this documents what that buys.
    """
    E = Extents(ext)
    assert all(E.interior_of(a) is None for a in E.keys)


def test_new_null_catches_the_absurd_table():
    """...whereas the replacement notices the absurd one."""
    ext = {0x1000 + 4 * i: 1 << 20 for i in range(200)}
    E = Extents(ext)
    assert any(E.covered_by_predecessor(a) for a in E.keys)


# --------------------------------------------------------------------------
# end-to-end: the contract, in the environment where it was dead
# --------------------------------------------------------------------------

def _tool_in_fresh_root(tmp_path: Path) -> Path:
    """Copy the tool under a bare root, so ROOT/fingerprints.json is ABSENT.

    This reproduces a fresh worktree without touching any real one: the tool
    derives ROOT from its own path and is otherwise stdlib-only.
    """
    tools = tmp_path / "tools"
    tools.mkdir()
    dst = tools / "pdata_map_audit.py"
    shutil.copy2(TOOL, dst)
    assert not (tmp_path / "fingerprints.json").exists()
    return dst


needs_exe = pytest.mark.skipif(
    not os.path.exists(DEFAULT_EXE),
    reason=f"retail binary absent ({DEFAULT_EXE}) -- gitignored; end-to-end legs "
           "cannot run (this is a visible SKIP, not a pass)")


@needs_exe
def test_sabotage_fails_even_with_fingerprints_absent(tmp_path):
    """THE regression. This exact case exited 0 and printed OK before lane P."""
    tool = _tool_in_fresh_root(tmp_path)
    r = subprocess.run([sys.executable, str(tool), "--selftest",
                        "--sabotage", "shift", "--exe", DEFAULT_EXE],
                       capture_output=True, text=True)
    assert r.returncode == 1, f"sabotage did not fail!\n{r.stdout}"
    assert "FAILED" in r.stdout
    assert "OK" not in r.stdout.splitlines()[-1]


@needs_exe
def test_clean_run_without_fingerprints_is_incomplete_not_ok(tmp_path):
    """A skipped control must never render as OK, and must not exit 0."""
    tool = _tool_in_fresh_root(tmp_path)
    r = subprocess.run([sys.executable, str(tool), "--selftest",
                        "--exe", DEFAULT_EXE], capture_output=True, text=True)
    assert r.returncode == EXIT_INCOMPLETE, r.stdout
    assert "INCOMPLETE" in r.stdout
    assert "NOT a clean bill of health" in r.stdout
    assert "not examined: extent sizes agree with fingerprints.json" in r.stdout
    # the operator must be told how to make the control run
    assert "fix:" in r.stdout


@needs_exe
def test_strict_turns_a_skipped_control_into_a_failure(tmp_path):
    tool = _tool_in_fresh_root(tmp_path)
    r = subprocess.run([sys.executable, str(tool), "--selftest", "--strict",
                        "--exe", DEFAULT_EXE], capture_output=True, text=True)
    assert r.returncode == EXIT_FAILED, r.stdout
    assert "FAILED" in r.stdout


# --------------------------------------------------------------------------
# the ONE-SIDEDNESS gap: intrinsic legs cannot see an UNDER-decode
# --------------------------------------------------------------------------

def test_intrinsic_legs_are_monotone_and_miss_an_underdecode():
    """Halving every length keeps disjointness/containment/upper-bound green.

    Pins WHY the coverage floor exists: shrinking lengths only makes those
    three easier, so as a set they are blind on the low side.
    """
    spans = [(0x1000, 0x2000)]
    full = {0x1000 + 0x10 * i: 0x10 for i in range(0x100)}   # 100% covered
    half = {a: v // 2 for a, v in full.items()}
    for ext in (full, half):
        st = check_extents(ext, spans)
        assert st["overlaps"] == 0
        assert st["outside"] == 0
        assert st["covered"] <= st["code_size"]
    assert check_extents(full, spans)["covered_frac"] == 1.0
    assert check_extents(half, spans)["covered_frac"] == 0.5


def test_coverage_floor_is_the_leg_that_catches_it():
    spans = [(0x1000, 0x2000)]
    full = {0x1000 + 0x10 * i: 0x10 for i in range(0x100)}
    half = {a: v // 2 for a, v in full.items()}
    assert check_extents(full, spans)["covered_frac"] >= MIN_COVERED_FRAC
    assert check_extents(half, spans)["covered_frac"] < MIN_COVERED_FRAC


@needs_exe
def test_real_binary_underdecodes_are_caught_only_by_the_floor():
    """The two variants review found, on the real table: 23.6% and 47.1%.

    Both satisfy disjointness, containment, the upper bound AND the
    known-positive leg. Only the floor separates them from retail's 94.3%.
    """
    import struct

    from pdata_map_audit import _sections, code_spans

    data = open(DEFAULT_EXE, "rb").read()
    _, secs = _sections(data)
    _, _, praw, psz, _ = [s for s in secs if s[0] == ".pdata"][0]
    spans = code_spans(DEFAULT_EXE)

    def decode(scale):
        ext = {}
        for off in range(praw, praw + psz, 8):
            b, w = struct.unpack_from(">II", data, off)
            if b == 0 or not (0x82000000 <= b < 0x83000000):
                continue
            ext[b] = ((w >> 8) & 0x3FFFFF) * scale
        return ext

    pos = [0x826C48F8, 0x826C4908, 0x826C4910, 0x826C4958]
    for scale, want_ok in ((4, True), (2, False), (1, False)):
        ext = decode(scale)
        st = check_extents(ext, spans)
        # the one-sided legs are green for ALL THREE
        assert st["overlaps"] == 0 and st["outside"] == 0
        assert st["covered"] <= st["code_size"]
        # ...so is the known-positive leg
        E = Extents(ext)
        assert all(E.interior_of(a) == 0x826C44F8 for a in pos)
        # only the floor discriminates
        assert (st["covered_frac"] >= MIN_COVERED_FRAC) is want_ok, (
            f"scale={scale} covered_frac={st['covered_frac']:.3f}")


# --------------------------------------------------------------------------
# the binary is gitignored too: a missing one must not read as "control fired"
# --------------------------------------------------------------------------

def test_missing_binary_is_cannot_run_not_a_traceback(tmp_path):
    tool = _tool_in_fresh_root(tmp_path)
    r = subprocess.run([sys.executable, str(tool), "--selftest",
                        "--exe", str(tmp_path / "nope.exe")],
                       capture_output=True, text=True)
    assert r.returncode == EXIT_CANNOT_RUN, r.stdout + r.stderr
    assert "Traceback" not in r.stderr
    assert "CANNOT RUN" in r.stdout
    assert f"0 of {EXPECTED_CONTROLS}" in r.stdout


def test_missing_binary_does_not_satisfy_the_sabotage_contract(tmp_path):
    """THE re-created defect. `--sabotage` MUST fail -- but a crash exiting 1
    would satisfy that while examining nothing, which is the original bug one
    dependency to the left. A missing binary must be its own outcome."""
    tool = _tool_in_fresh_root(tmp_path)
    r = subprocess.run([sys.executable, str(tool), "--selftest",
                        "--sabotage", "shift",
                        "--exe", str(tmp_path / "nope.exe")],
                       capture_output=True, text=True)
    assert r.returncode == EXIT_CANNOT_RUN, r.stdout + r.stderr
    assert r.returncode != EXIT_FAILED


def test_usage_error_does_not_collide_with_a_verdict(tmp_path):
    """argparse owns exit 2; no verdict may share it."""
    tool = _tool_in_fresh_root(tmp_path)
    r = subprocess.run([sys.executable, str(tool), "--selftest", "--nope"],
                       capture_output=True, text=True)
    assert r.returncode == 2
    assert 2 not in (EXIT_FAILED, EXIT_INCOMPLETE, EXIT_CANNOT_RUN)


@needs_exe
def test_verdict_reports_against_the_pinned_census_not_n_over_n(tmp_path):
    """`N/N` reads complete for any N, including a set someone deleted from."""
    r = subprocess.run([sys.executable, str(TOOL), "--selftest",
                        "--exe", DEFAULT_EXE], capture_output=True, text=True)
    assert f"/{EXPECTED_CONTROLS} controls ran" in r.stdout, r.stdout
