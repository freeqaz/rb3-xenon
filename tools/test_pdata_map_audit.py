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
    assert r.returncode == 2, r.stdout
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
    assert r.returncode == 1, r.stdout
    assert "FAILED" in r.stdout
