"""Tests for how comdat_retail_verify reads scripts/target_symbol_map.json.

The tool's verdict line says "SECTION byte-identical to retail", which is an
IDENTITY claim, so the only thing worth testing without a build is which map
rows are allowed to supply an address -- and, equally, that the rows refused
are COUNTED rather than dropped in silence.

Two defect classes are pinned here, both of which this repo has actually
shipped:

1. THE CRASH. `addr_of = {v: int(k, 16) for k, v in tmap.items()
   if args.pattern in v}` raised `TypeError: argument of type 'NoneType' is
   not iterable` for EVERY `--pattern` as soon as the map held one null row.
   The map holds 27. So the repo's own retail-byte verifier was unusable on
   precisely the unestablished-identity population it is most needed for, and
   the reviewer who hit it worked around it in scratch instead.

2. THE SILENT SUBSTITUTE. Making that crash-proof with `if v` or `str(v)`
   would be worse than the crash: `''` is a substring of every pattern, so a
   deliberately-unclaimed row would start MATCHING, and `_denylist` rows carry
   real name strings that a bare isinstance guard leaves in play. Measured on
   the checked-in map, a crash-proof-only fix still hands the tool four names
   whose addresses the map explicitly refuses, one of them
   `??$__destroy_aux@ULevelData@@...` -- the canonical unadjudicable pair from
   docs/decomp/VERDICT_STATES.md.

So the assertions below are about MEANING, not about not-throwing.
"""
import json
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
MAP_PATH = ROOT / "scripts" / "target_symbol_map.json"

sys.path.insert(0, str(Path(__file__).resolve().parent))
from comdat_retail_verify import (  # noqa: E402
    classify_map_rows,
    resolve_addresses,
)

A, B, C = "0x82000000", "0x82000010", "0x82000020"
NAME_A = "?A@@YAXXZ"
NAME_B = "?B@@YAXXZ"


def write_map(tmp_path, obj):
    p = tmp_path / "target_symbol_map.json"
    p.write_text(json.dumps(obj))
    return p


# --------------------------------------------------------------------------
# classify_map_rows -- pure, so it is driven on the null vector first
# --------------------------------------------------------------------------
def test_null_vector_counts_nothing_and_does_not_crash():
    got = classify_map_rows({})
    assert got == dict(total=0, claimed=0, unclaimed=0, denied=0,
                       nonstring=0, metadata=0, denied_absent=0)


def test_a_null_row_is_unclaimed_not_claimed():
    got = classify_map_rows({A: NAME_A, B: None})
    assert (got["claimed"], got["unclaimed"]) == (1, 1)


def test_denied_row_is_denied_even_though_it_carries_a_name():
    """The 2026-08-13 shape: the map still holds the string, and refuses it."""
    got = classify_map_rows({"_denylist": [B], A: NAME_A, B: NAME_B})
    assert (got["claimed"], got["denied"], got["denied_absent"]) == (1, 1, 0)


def test_denied_address_absent_from_the_map_body_is_still_disclosed():
    got = classify_map_rows({"_denylist": [B], A: NAME_A})
    assert (got["denied"], got["denied_absent"]) == (0, 1)


def test_non_string_value_under_an_address_key_is_not_claimed():
    got = classify_map_rows({A: [NAME_A]})
    assert (got["claimed"], got["nonstring"]) == (0, 1)


def test_buckets_partition_the_real_map():
    """No row may fall into no bucket -- that is the defect class itself."""
    raw = json.loads(MAP_PATH.read_text())
    s = classify_map_rows(raw)
    assert (s["claimed"] + s["unclaimed"] + s["denied"]
            + s["nonstring"] + s["metadata"]) == s["total"] == len(raw)
    assert s["unclaimed"] > 0, (
        "the checked-in map no longer holds a null row; this suite is then "
        "only exercising fabricated input -- do not delete it, but say so")


# --------------------------------------------------------------------------
# resolve_addresses -- the seam that crashed
# --------------------------------------------------------------------------
def test_a_null_row_does_not_crash_pattern_matching(tmp_path):
    """The reported bug, at its own grain: ANY pattern, one null, TypeError."""
    p = write_map(tmp_path, {A: NAME_A, B: None})
    addr_of, stats = resolve_addresses(p, "?A@")
    assert addr_of == {NAME_A: 0x82000000}
    assert stats["unclaimed"] == 1


def test_a_null_row_is_not_an_empty_string(tmp_path):
    """`''` is a substring of every pattern. A null must match NOTHING, so the
    empty pattern -- which claims every named row -- must still not claim it.
    This is the assertion that fails if someone 'fixes' the crash with
    `if v` / `str(v)` / `v or ''`."""
    p = write_map(tmp_path, {A: NAME_A, B: None})
    addr_of, _ = resolve_addresses(p, "")
    assert list(addr_of) == [NAME_A]
    assert None not in addr_of and "" not in addr_of


def test_a_denied_row_supplies_no_address(tmp_path):
    p = write_map(tmp_path, {"_denylist": [B], A: NAME_A, B: NAME_B})
    addr_of, stats = resolve_addresses(p, "")
    assert addr_of == {NAME_A: 0x82000000}
    assert stats["denied"] == 1


def test_claimed_rows_still_resolve_to_their_address(tmp_path):
    """Positive control: the filter must not be vacuously empty."""
    p = write_map(tmp_path, {A: NAME_A, B: NAME_B, C: None})
    addr_of, _ = resolve_addresses(p, "")
    assert addr_of == {NAME_A: 0x82000000, NAME_B: 0x82000010}


def test_pattern_is_a_substring_match_over_claimed_names_only(tmp_path):
    p = write_map(tmp_path, {A: NAME_A, B: NAME_B, C: None})
    assert list(resolve_addresses(p, "?B@")[0]) == [NAME_B]
    assert resolve_addresses(p, "?Z@")[0] == {}


@pytest.mark.parametrize("pattern", ["_Copy_Construct@", "??0", "", "?Null@"])
def test_the_real_map_resolves_for_every_pattern(pattern):
    """The regression, bound to the real producer output rather than a
    fabrication: on the tree's own map this raised for ANY pattern."""
    addr_of, stats = resolve_addresses(MAP_PATH, pattern)
    assert isinstance(addr_of, dict)
    assert all(isinstance(n, str) and isinstance(a, int)
               for n, a in addr_of.items())
    assert stats["unclaimed"] == 27 or stats["unclaimed"] > 0


def test_the_real_map_scores_no_denied_address():
    """Named because it is the one a crash-proof-only fix gets wrong: four
    names on the checked-in map carry a refused address."""
    raw = json.loads(MAP_PATH.read_text())
    denied = {int(a, 16) for a in raw.get("_denylist", [])}
    addr_of, _ = resolve_addresses(MAP_PATH, "")
    assert denied and not (set(addr_of.values()) & denied)
