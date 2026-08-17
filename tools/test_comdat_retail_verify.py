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

3. THE OVERSTATED CLAIM (lane task110, 2026-08-17). A row on
   `_icf_arbitrary` / `_bijection_arbitrary` is byte-witnessed but its NAME is
   an arbitrary pick inside a fold class -- the byte claim holds under any
   member of the class, so "instantiation N lives at 0xVA" does not. Refusing
   those rows was measured and rejected (task100: -957 strict-100 matches), so
   the fix is per-consumer LABELLING, and the failure mode it guards is a
   summary that adds 967 unpinned rows into one `byte-identical` number and
   reads as 967 established identities. The assertions below pin that the two
   classes are counted APART -- from each other, not just from pinned rows,
   because their repair anchors differ.

So the assertions below are about MEANING, not about not-throwing.
"""
import json
import re
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
MAP_PATH = ROOT / "scripts" / "target_symbol_map.json"

sys.path.insert(0, str(Path(__file__).resolve().parent))
from comdat_retail_verify import (  # noqa: E402
    NAME_PINNED,
    _print_grain_split,
    classify_map_rows,
    label_rows,
    name_grain_index,
    resolve_addresses,
    tally_by_grain,
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
                       nonstring=0, metadata=0, denied_absent=0,
                       claimed_by_grain={"name_pinned": 0,
                                         "icf_arbitrary": 0,
                                         "bijection_arbitrary": 0})


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
    addr_of, stats, _grain = resolve_addresses(p, "?A@")
    assert addr_of == {NAME_A: 0x82000000}
    assert stats["unclaimed"] == 1


def test_a_null_row_is_not_an_empty_string(tmp_path):
    """`''` is a substring of every pattern. A null must match NOTHING, so the
    empty pattern -- which claims every named row -- must still not claim it.
    This is the assertion that fails if someone 'fixes' the crash with
    `if v` / `str(v)` / `v or ''`."""
    p = write_map(tmp_path, {A: NAME_A, B: None})
    addr_of, _stats, _grain = resolve_addresses(p, "")
    assert list(addr_of) == [NAME_A]
    assert None not in addr_of and "" not in addr_of


def test_a_denied_row_supplies_no_address(tmp_path):
    p = write_map(tmp_path, {"_denylist": [B], A: NAME_A, B: NAME_B})
    addr_of, stats, _grain = resolve_addresses(p, "")
    assert addr_of == {NAME_A: 0x82000000}
    assert stats["denied"] == 1


def test_claimed_rows_still_resolve_to_their_address(tmp_path):
    """Positive control: the filter must not be vacuously empty."""
    p = write_map(tmp_path, {A: NAME_A, B: NAME_B, C: None})
    addr_of, _stats, _grain = resolve_addresses(p, "")
    assert addr_of == {NAME_A: 0x82000000, NAME_B: 0x82000010}


def test_pattern_is_a_substring_match_over_claimed_names_only(tmp_path):
    p = write_map(tmp_path, {A: NAME_A, B: NAME_B, C: None})
    assert list(resolve_addresses(p, "?B@")[0]) == [NAME_B]
    assert resolve_addresses(p, "?Z@")[0] == {}


@pytest.mark.parametrize("pattern", ["_Copy_Construct@", "??0", "", "?Null@"])
def test_the_real_map_resolves_for_every_pattern(pattern):
    """The regression, bound to the real producer output rather than a
    fabrication: on the tree's own map this raised for ANY pattern."""
    addr_of, stats, _grain = resolve_addresses(MAP_PATH, pattern)
    assert isinstance(addr_of, dict)
    assert all(isinstance(n, str) and isinstance(a, int)
               for n, a in addr_of.items())
    assert stats["unclaimed"] == 27 or stats["unclaimed"] > 0


# --------------------------------------------------------------------------
# name grain -- the third defect class: a scored row whose NAME is arbitrary
# --------------------------------------------------------------------------
def test_grain_null_vector_is_empty_and_does_not_crash():
    assert name_grain_index({}) == {}


def test_the_two_arbitrary_classes_are_not_collapsed_into_one_bucket():
    """THE assertion of lane task110. `_icf_arbitrary` and
    `_bijection_arbitrary` are different findings -- one is retail's linker
    folding identical bodies, the other is a chosen bijection over a
    reloc-masked byte-identical class -- and a summary that adds them is a
    summary you cannot act on, because the repair anchor differs per class.
    Fails if either label is dropped or the two are summed."""
    raw = {"_icf_arbitrary": [A], "_bijection_arbitrary": [B],
           A: NAME_A, B: NAME_B, C: "?C@@YAXXZ"}
    assert name_grain_index(raw) == {0x82000000: "icf_arbitrary",
                                     0x82000010: "bijection_arbitrary"}
    bg = classify_map_rows(raw)["claimed_by_grain"]
    assert bg["icf_arbitrary"] == 1
    assert bg["bijection_arbitrary"] == 1
    assert bg[NAME_PINNED] == 1


def test_an_arbitrary_row_is_still_CLAIMED_and_still_supplies_its_address(
        tmp_path):
    """The behaviour half. Labelling must not become refusing: task100
    measured that refusing destroys 957 strict-100 matches, and this pins
    that the label changed reporting only."""
    p = write_map(tmp_path, {"_icf_arbitrary": [A],
                             "_bijection_arbitrary": [B],
                             A: NAME_A, B: NAME_B})
    addr_of, stats, grain = resolve_addresses(p, "")
    assert addr_of == {NAME_A: 0x82000000, NAME_B: 0x82000010}
    assert stats["claimed"] == 2
    assert grain[0x82000000] == "icf_arbitrary"


def test_grain_counts_sum_to_claimed_on_the_real_map():
    """Same shape as the partition test above, one level down: a grain that
    fell into no label would understate `fold-arbitrary` silently."""
    raw = json.loads(MAP_PATH.read_text())
    s = classify_map_rows(raw)
    assert sum(s["claimed_by_grain"].values()) == s["claimed"]


def test_the_real_map_populations_are_bound_as_floors():
    """Measured 2026-08-17 on the checked-in map: 28 icf, 939 bijection
    (the `_bijection_arbitrary` LIST holds 1025 addresses -- 85 are absent
    from the map body and one more is a null row, so the list length is NOT
    the scored population and must not be quoted as one)."""
    raw = json.loads(MAP_PATH.read_text())
    bg = classify_map_rows(raw)["claimed_by_grain"]
    assert bg["icf_arbitrary"] == 28
    assert bg["bijection_arbitrary"] == 939
    assert bg[NAME_PINNED] > 20000
    assert len(raw["_bijection_arbitrary"]) > bg["bijection_arbitrary"]


def _row(nbad, grain):
    return dict(sym="?x@@YAXXZ", nbad=nbad, name_grain=grain)


def test_emitted_rows_are_labelled_from_the_grain_index():
    """The seam main used to hold inline, where it was reachable only with a
    built tree and a retail PE. A row's label must come from the index, and a
    row the index does not know must come back pinned -- not unlabelled, which
    would make the tally silently undercount."""
    rows = [dict(addr=A, nbad=0), dict(addr=B, nbad=1), dict(addr=C, nbad=0)]
    out = label_rows(rows, {0x82000000: "icf_arbitrary",
                            0x82000010: "bijection_arbitrary"})
    assert out is rows
    assert [r["name_grain"] for r in rows] == [
        "icf_arbitrary", "bijection_arbitrary", NAME_PINNED]


def test_labelling_does_not_disturb_the_byte_evidence_on_a_row():
    """Behaviour guard at row grain: labelling adds a key and touches nothing
    else. If `nbad`/`bad`/`addr` can move here, the reporting change has
    become a measurement change."""
    before = dict(addr=A, nbad=2, bad=[["0x00", "1", "2"]], sym="?x@@YAXXZ")
    after = dict(label_rows([dict(before)], {0x82000000: "icf_arbitrary"})[0])
    assert after.pop("name_grain") == "icf_arbitrary"
    assert after == before


def test_the_verdict_tally_splits_each_byte_verdict_by_grain():
    """The number a reader actually quotes. `byte-identical: 397` on
    ?SyncProperty@ is 338 pinned + 59 bijection-arbitrary (measured
    2026-08-17); a tally that reports 397 alone claims 59 identities the map
    does not."""
    ident, diff = tally_by_grain([
        _row(0, NAME_PINNED), _row(0, NAME_PINNED),
        _row(0, "icf_arbitrary"),
        _row(0, "bijection_arbitrary"), _row(0, "bijection_arbitrary"),
        _row(3, NAME_PINNED), _row(7, "icf_arbitrary"),
    ])
    assert ident == {NAME_PINNED: 2, "icf_arbitrary": 1,
                     "bijection_arbitrary": 2}
    assert diff == {NAME_PINNED: 1, "icf_arbitrary": 1}


def test_the_verdict_tally_totals_still_equal_the_byte_verdicts():
    """Behaviour guard: the split may not move a row across the byte verdict.
    `nbad` decides, and only `nbad`."""
    rows = [_row(0, NAME_PINNED), _row(0, "icf_arbitrary"),
            _row(1, "bijection_arbitrary")]
    ident, diff = tally_by_grain(rows)
    assert sum(ident.values()) == sum(1 for r in rows if not r["nbad"]) == 2
    assert sum(diff.values()) == sum(1 for r in rows if r["nbad"]) == 1


def test_the_verdict_tally_defaults_an_unlabelled_row_to_pinned_not_dropped():
    ident, diff = tally_by_grain([dict(nbad=0)])
    assert ident == {NAME_PINNED: 1} and diff == {}


def test_the_verdict_tally_on_the_null_vector():
    assert tally_by_grain([]) == ({}, {})


def test_grain_split_names_both_classes_in_the_printed_summary(capsys):
    """The reader-facing surface: the two classes must appear as separate
    printed lines, not one 'fold-arbitrary' total."""
    _print_grain_split({NAME_PINNED: 7, "icf_arbitrary": 2,
                        "bijection_arbitrary": 3})
    out = capsys.readouterr().out
    assert "_icf_arbitrary" in out and "_bijection_arbitrary" in out
    assert re.search(r"_icf_arbitrary\s*:\s*2\b", out)
    assert re.search(r"_bijection_arbitrary\s*:\s*3\b", out)
    assert re.search(r"fold-arbitrary[^\n]*:\s*5\b", out)
    assert re.search(r"name pinned[^\n]*:\s*7\b", out)


def test_grain_split_is_silent_when_every_row_is_pinned(capsys):
    _print_grain_split({NAME_PINNED: 7})
    assert capsys.readouterr().out == ""


def test_an_address_on_both_lists_keeps_both_labels():
    """Dead on today's map (the intersection is empty) and asserted anyway:
    first-key-wins here would make the per-label counts stop summing to
    `claimed` the day retail folds a bijection row, which is the silent-drop
    shape this whole file exists to catch."""
    raw = {"_icf_arbitrary": [A], "_bijection_arbitrary": [A], A: NAME_A}
    assert name_grain_index(raw) == {0x82000000:
                                     "bijection_arbitrary+icf_arbitrary"}
    assert sum(classify_map_rows(raw)["claimed_by_grain"].values()) == 1


def test_the_real_map_arbitrary_lists_do_not_overlap():
    """The premise the branch above is dead under. If this ever goes red the
    combined label starts appearing in summaries -- that is correct, not a
    bug, but it should be noticed."""
    raw = json.loads(MAP_PATH.read_text())
    icf = {a.lower() for a in raw["_icf_arbitrary"]}
    bij = {a.lower() for a in raw["_bijection_arbitrary"]}
    assert not (icf & bij)


def test_junk_under_a_grain_key_is_ignored_not_crashed():
    """Total on every value type the map has ever held, same bar as
    classify_map_rows."""
    assert name_grain_index({"_icf_arbitrary": "0x82000000"}) == {}
    assert name_grain_index({"_icf_arbitrary": [None, 5, "nope", "0xzz"]}) == {}


def test_the_real_map_scores_no_denied_address():
    """Named because it is the one a crash-proof-only fix gets wrong: four
    names on the checked-in map carry a refused address."""
    raw = json.loads(MAP_PATH.read_text())
    denied = {int(a, 16) for a in raw.get("_denylist", [])}
    addr_of, _stats, _grain = resolve_addresses(MAP_PATH, "")
    assert denied and not (set(addr_of.values()) & denied)
