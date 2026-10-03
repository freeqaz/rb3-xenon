"""Pin that every placed alias group's survivor is the map's name at its address
(lane W16-OS, 2026-10-03), and that the check pinning it can fail.

`tools/alias_survivor_drift.py` is the predicate; it is enforced by an
always-dirty build edge and by `icf_alias_finder.py --validate`.  These tests
run without a built tree (two JSON files and the renamer's map loader).
"""
import copy
import json
import subprocess
import sys
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import alias_survivor_drift as D  # noqa: E402
import alias_survivor_relabel as R  # noqa: E402


@pytest.fixture(scope="module")
def groups():
    return json.loads((ROOT / "scripts/symbol_aliases.json").read_text())["groups"]


@pytest.fixture(scope="module")
def applied():
    return D.applied_names()


def test_frozen_selftest_passes():
    assert D.selftest() == 0


def test_shipped_ledger_has_no_drift(groups, applied):
    drift = D.find_drift(groups, applied)
    assert not drift, [(g["address"], why) for g, _w, why in drift[:10]]


def test_population_is_not_vacuous(groups, applied):
    """A pass over nothing is not a pass: most placed groups must carry a REAL
    map name, so the equality is actually being tested."""
    placed = [g for g in groups if g.get("address")]
    named = [g for g in placed if int(g["address"], 16) in applied]
    assert len(placed) > 1000 and len(named) > 0.9 * len(placed)


@pytest.mark.parametrize("mutate", ["unmapped", "other_address", "placeholder"])
def test_one_drifted_group_in_the_live_ledger_is_caught(groups, applied, mutate):
    gs = copy.deepcopy(groups)
    placed = [g for g in gs if g.get("address") and int(g["address"], 16) in applied]
    g, h = placed[0], placed[1]
    if mutate == "unmapped":
        g["survivor"] = g["survivor"] + "_STALE"
    elif mutate == "other_address":
        g["survivor"] = h["survivor"]
    else:
        g["survivor"] = "fn_%08X" % int(g["address"], 16)
    drift = D.find_drift(gs, applied)
    assert [x[0]["address"] for x in drift] == [g["address"]]


def test_address_less_groups_are_exempt(groups, applied):
    """They render into no map bucket (gen_symbol_alias_map skips them)."""
    unplaced = [g for g in groups if not g.get("address")]
    assert unplaced and D.find_drift(unplaced, applied) == []


def test_cli_exits_nonzero_on_drift(tmp_path, groups, applied):
    gs = copy.deepcopy(groups)
    g = next(g for g in gs if g.get("address") and int(g["address"], 16) in applied)
    g["survivor"] += "_STALE"
    p = tmp_path / "aliases.json"
    p.write_text(json.dumps({"groups": gs}))
    r = subprocess.run([sys.executable, str(ROOT / "tools/alias_survivor_drift.py"),
                        "--aliases", str(p)], capture_output=True, text=True)
    assert r.returncode == 1 and "ALIAS SURVIVOR DRIFT: 1 of" in r.stderr
    p2 = tmp_path / "empty.json"
    p2.write_text(json.dumps({"groups": []}))
    r2 = subprocess.run([sys.executable, str(ROOT / "tools/alias_survivor_drift.py"),
                         "--aliases", str(p2)], capture_output=True, text=True)
    assert r2.returncode == 2, "an empty population must REFUSE, not pass"


@pytest.mark.parametrize("a,b,differs", [
    ("?_M_copy_from@?$hashtable@X@@QAAXXZ", "?_M_initialize_buckets@?$hashtable@Y@@QAAXI@Z", True),
    ("??0?$ObjPtr@VA@@@@QAA@ABV0@@Z", "??0?$ObjOwnerPtr@VB@@@@QAA@ABV0@@Z", False),
    ("??$_Copy_Construct@VA@@@std@@YAXXZ", "??$_Copy_Construct@VB@@@std@@YAXXZ", False),
    ("??1A@@UAA@XZ", "??_GA@@UAAPAXI@Z", True),
    ("?ShowPartyUI@PlatformMgr@@QAA_NH@Z", "?ShowOfferUI@PlatformMgr@@QAAXH@Z", True),
])
def test_relabel_operation_discriminator(a, b, differs):
    """The callee-level refutation rule in alias_survivor_relabel.py treats two
    callees as different functions only when their OPERATION differs; a pure
    template-argument difference needs a retail-address anchor instead."""
    assert (R.operation(a) != R.operation(b)) is differs


def test_relabel_operation_unknown_for_placeholders():
    assert R.operation("fn_82000000") is None and R.operation("lbl_82000000") is None


if __name__ == "__main__":
    import pytest as _pytest
    sys.exit(_pytest.main([__file__, "-q", "-p", "no:cacheprovider"]))
