"""Which `target_symbol_map.json` keys refuse an address, and which do not.

`load_address_map` is the shared gate: it feeds the build's symbol renamer,
tools/map_name_injectivity.py and tools/comdat_retail_verify.py (whose
"SECTION byte-identical to retail" is an identity claim). So "is this list
honoured?" is a load-bearing question, and it has been answered wrongly before
-- `_denylist` sat DECLARED AND IGNORED by this loader until `f3fe9ab1`, a
safeguard that silently did nothing.

The map declares four more list-valued keys in roughly that shape. Lane task100
(2026-08-17) adjudicated all four and the answer was NO for every one, for two
different reasons -- so the risk this file pins is the MIRROR of the `f3fe9ab1`
defect: someone reads the loader, pattern-matches "another ignored list", and
"fixes" it. Doing that would destroy 957 strict-100 name-checked matches.

The asserts are therefore about MEANING, in both directions:
  * `_denylist` really does refuse (else we have re-broken `f3fe9ab1`), and
  * the four siblings really are still applied (else someone has "fixed" a
    non-defect and silently deleted byte-witnessed evidence).
"""
import json
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
MAP_PATH = ROOT / "scripts" / "target_symbol_map.json"

sys.path.insert(0, str(Path(__file__).resolve().parent))
from obj_target_symbol_renamer import (  # noqa: E402
    REFUSAL_KEYS,
    load_address_map,
)

A, B = "0x82000000", "0x82000010"
NAME_A, NAME_B = "?A@@YAXXZ", "?B@@YAXXZ"

# Every list-valued metadata key the map declares, and whether the loader
# refuses the addresses on it. Written out rather than derived so that ADDING a
# key to the map forces a deliberate decision here.
NOT_REFUSAL_KEYS = (
    "_denylist_unadjudicated",
    "_icf_arbitrary",
    "_bijection_arbitrary",
    "_internal_linkage_allow",
)


def write_map(tmp_path, obj):
    p = tmp_path / "target_symbol_map.json"
    p.write_text(json.dumps(obj))
    return p


def addrs(m):
    """Distinct addresses claimed by a loader result (it emits fn_ AND lbl_)."""
    return {int(k[3:], 16) for k in m if k.startswith("fn_")}


# --------------------------------------------------------------------------
# The honoured key. Mutation control: delete the `_denylist` filter from
# load_address_map and this goes red.
# --------------------------------------------------------------------------
def test_denylist_is_the_refusal_key():
    assert REFUSAL_KEYS == ("_denylist",)


def test_a_denylisted_address_is_refused_even_carrying_a_name(tmp_path):
    p = write_map(tmp_path, {"_denylist": [B], A: NAME_A, B: NAME_B})
    assert addrs(load_address_map(p)) == {0x82000000}


# --------------------------------------------------------------------------
# The keys deliberately NOT honoured. Mutation control: add any one of them to
# REFUSAL_KEYS and the matching case goes red.
# --------------------------------------------------------------------------
@pytest.mark.parametrize("key", NOT_REFUSAL_KEYS)
def test_sibling_lists_do_not_refuse_an_address(tmp_path, key):
    """A row listed here keeps its name. `_denylist_unadjudicated` records a
    suspicion nobody has adjudicated; the two `_arbitrary` lists record that
    WHICH of N ICF-folded names sits on the VA is unresolved while the BYTES
    are witnessed; `_internal_linkage_allow` is an allow list, opposite
    polarity. None of the four is a refusal, and the remedy for the first is to
    MOVE the address into `_denylist` -- not to honour a second key here.
    """
    p = write_map(tmp_path, {key: [B], A: NAME_A, B: NAME_B})
    assert addrs(load_address_map(p)) == {0x82000000, 0x82000010}


@pytest.mark.parametrize("key", NOT_REFUSAL_KEYS)
def test_sibling_lists_are_not_in_refusal_keys(key):
    assert key not in REFUSAL_KEYS


# --------------------------------------------------------------------------
# Bound to the real producer output, not a fabrication: the checked-in map.
# These are the populations lane task100 measured, and they are what a
# "fix" would delete.
# --------------------------------------------------------------------------
def test_the_real_maps_arbitrary_rows_are_still_applied():
    """939 `_bijection_arbitrary` + 28 `_icf_arbitrary` live rows are APPLIED.

    929 + 28 of them score strict-100 under `functionRelocDiffs=name_check`,
    where a wrong name is charged at every referencing call site -- so these are
    byte-witnessed matches, not free passes. Refusing them was measured and
    rejected.
    """
    raw = json.loads(MAP_PATH.read_text())
    applied = addrs(load_address_map(MAP_PATH))
    for key, floor in (("_icf_arbitrary", 20), ("_bijection_arbitrary", 800)):
        listed = {int(a, 16) for a in raw[key]}
        live = {a for a in listed if a in applied}
        assert len(live) >= floor, (
            f"{key}: only {len(live)} of {len(listed)} listed addresses are "
            f"applied -- has the loader started refusing this list? See "
            f"load_address_map's docstring before 'fixing' it."
        )


def test_the_real_maps_unadjudicated_row_is_still_applied():
    """Honouring it costs -1 matched / -1 honest / 0 code bytes (`f3fe9ab1`,
    reproduced by lane task100: the single live row is strict-100 today)."""
    raw = json.loads(MAP_PATH.read_text())
    listed = {int(a, 16) for a in raw["_denylist_unadjudicated"]}
    assert listed
    assert listed <= addrs(load_address_map(MAP_PATH))


def test_the_real_maps_denylist_is_refused():
    """Positive control for the same map, so the four asserts above cannot
    pass merely because the loader refuses nothing at all."""
    raw = json.loads(MAP_PATH.read_text())
    denied = {int(a, 16) for a in raw["_denylist"]}
    assert denied and not (denied & addrs(load_address_map(MAP_PATH)))


def test_refusal_and_non_refusal_keys_are_disjoint_and_cover_the_map():
    """A new list-valued metadata key must be classified, not left to default
    into "applied" unnoticed -- that default is exactly the `f3fe9ab1` bug."""
    raw = json.loads(MAP_PATH.read_text())
    listy = {k for k, v in raw.items()
             if k.startswith("_") and isinstance(v, list)}
    assert set(REFUSAL_KEYS).isdisjoint(NOT_REFUSAL_KEYS)
    assert listy == set(REFUSAL_KEYS) | set(NOT_REFUSAL_KEYS), (
        "unclassified list-valued map key(s): "
        f"{sorted(listy - set(REFUSAL_KEYS) - set(NOT_REFUSAL_KEYS))}"
    )
