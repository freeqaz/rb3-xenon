"""Tests for the global map NAME-injectivity gate.

The gate's whole value is that it FIRES, so the thing under test is the verdict
function, not the plumbing. `--selftest` exercises the same properties from the
command line (for the operator, in a build log); these run in CI/pytest so a
refactor cannot quietly turn the gate into a function that always says OK --
which is exactly the defect class the gate was written for (`_denylist` sat
declared-and-ignored in the map until f3fe9ab1).
"""
import json
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from map_name_injectivity import (  # noqa: E402
    MAP_PATH,
    applied_name_to_addrs,
    find_violations,
    stale_allow_entries,
)

A, B, C = 0x82000000, 0x82000010, 0x82000020


def test_null_vector_is_green():
    """The verdict on nothing must be 'nothing', not a crash and not a finding."""
    assert find_violations({}, []) == []
    assert stale_allow_entries({}, []) == []


def test_injective_map_is_green():
    assert find_violations({"?A@@YAXXZ": [A], "?B@@YAXXZ": [B]}, []) == []


def test_duplicate_is_red_and_names_itself():
    got = find_violations({"?A@@YAXXZ": [A], "?B@@YAXXZ": [B, C]}, [])
    assert got == [("?B@@YAXXZ", [B, C])]


def test_repeated_address_for_one_name_is_not_a_duplicate():
    """Two spellings of the same VA are one claim, not two."""
    assert find_violations({"?A@@YAXXZ": [A, A]}, []) == []


def test_count_neutral_swap_still_reads_as_a_change():
    """2eb6307a: a plan that retires one duplicate and introduces another keeps
    the COUNT identical. A count check passes it; the set comparison does not."""
    before = find_violations({"?A@@YAXXZ": [A], "?B@@YAXXZ": [B, C]}, [])
    after = find_violations({"?A@@YAXXZ": [A, C], "?B@@YAXXZ": [B]}, [])
    assert len(before) == len(after) == 1
    assert before != after


def test_internal_linkage_allow_is_by_name_and_exact():
    dup = {"?B@@YAXXZ": [B, C]}
    assert find_violations(dup, ["?B@@YAXXZ"]) == []
    assert find_violations(dup, ["?A@@YAXXZ"]) == [("?B@@YAXXZ", [B, C])]


def test_stale_allow_entry_is_reported_but_is_not_a_violation():
    single = {"?A@@YAXXZ": [A]}
    assert stale_allow_entries(single, ["?A@@YAXXZ"]) == ["?A@@YAXXZ"]
    assert find_violations(single, ["?A@@YAXXZ"]) == []


def test_checked_in_map_is_injective():
    """The tree's own map, scored the way the renamer scores it."""
    raw = json.loads(Path(MAP_PATH).read_text())
    violations = find_violations(applied_name_to_addrs(MAP_PATH),
                                 raw.get("_internal_linkage_allow", []))
    assert violations == [], (
        "scripts/target_symbol_map.json claims a name at more than one VA: "
        + "; ".join(f"{n} @ {[hex(a) for a in addrs]}" for n, addrs in violations))


def test_cli_selftest_exits_zero():
    rc = subprocess.run(
        [sys.executable, str(Path(__file__).resolve().parent / "map_name_injectivity.py"),
         "--selftest"], capture_output=True).returncode
    assert rc == 0
