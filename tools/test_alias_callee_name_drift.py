"""Pin the alias callee-name drift check (lane W16-TV, 2026-10-07) and show it
can fail.  Frozen fixtures plus the live snapshot; no built tree needed.  The
planted group-771 timeline needs a built tree and runs from
`python3 tools/alias_callee_name_drift.py --selftest`."""
import json
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools"))
import alias_callee_name_drift as D  # noqa: E402


def test_frozen_fixtures():
    assert D.selftest(with_tree=False) == 0


def test_live_snapshot_is_green():
    groups = json.loads(D.ALIASES.read_text())["groups"]
    assert D.find_stale(groups, D.applied_names(), D.load_snapshot()) == {}


def test_live_snapshot_goes_red_on_a_renamed_callee():
    groups = json.loads(D.ALIASES.read_text())["groups"]
    snap = D.load_snapshot()
    applied = dict(D.applied_names())
    rec = next(r for r in snap["groups"].values() if r["callees"])
    va = int(rec["callees"][0], 16)
    applied[va] = "?PlantedRename@@YAXXZ"
    assert D.find_stale(groups, applied, snap), "a renamed callee must make its group stale"


def test_live_snapshot_goes_red_on_an_unproven_admission():
    groups = json.loads(D.ALIASES.read_text())["groups"]
    snap = D.load_snapshot()
    k, g = next(D.placed(groups))
    g2 = dict(g, folded=list(g["folded"]) + ["?NeverProven@@YAXXZ"])
    stale = D.find_stale([g2], D.applied_names(), snap)
    assert k in stale and "without a recorded proof" in "; ".join(stale[k])


if __name__ == "__main__":
    import pytest as _pytest
    sys.exit(_pytest.main([__file__, "-q", "-p", "no:cacheprovider"]))
