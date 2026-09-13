"""Pin: the alias generator must consult the withdrawal ledger.

`tools/icf_alias_build.py` had ZERO references to `withdrawn` until lane W8-A --
the ledger was not an input, and `--merge` is additive with no suppression list.
Every withdrawal ever adjudicated was durable only for as long as nobody re-ran
the generator, and `scripts/symbol_aliases.json`'s own `_comment` says a
regeneration "grows them back".

The direction matters: an alias is pure forgiveness under `name_check`, so a
re-fabricated one lifts `matched_code` BY CONSTRUCTION. A regeneration would not
present as a regression somebody chases -- it presents as a silent gain.

These are cheap structural pins, run by scripts/test_tools.py. The behavioural
negative control is tools/sabotage_withdrawal_guard.py, which needs a built tree
and several minutes; this file exists so a refactor that drops the guard goes
red in seconds instead of surviving until someone next runs the slow one.

No objects, no toolchain, no image.
"""
import ast
import json
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
GEN = ROOT / "tools" / "icf_alias_build.py"
MOD = ROOT / "tools" / "alias_withdrawals.py"
LEDGER = ROOT / "scripts" / "symbol_aliases.json"


def test_the_generator_still_references_the_ledger():
    """The measured defect was literally `grep -c withdrawn` -> 0."""
    src = GEN.read_text()
    assert "withdrawn" in src, (
        "tools/icf_alias_build.py no longer mentions `withdrawn`. That is the "
        "exact state lane W8-A found and fixed: the withdrawal ledger is not an "
        "input, so a regeneration silently re-fabricates every withdrawn alias.")
    assert "load_ledger" in src and "ledger.lookup" in src, (
        "the generator imports/uses of the withdrawal denylist are gone")


def test_the_denylist_gate_precedes_accept():
    """Placed after ACCEPT the gate is dead code; placed first its census bucket
    would absorb pairs another gate would have rejected anyway and overstate
    what the ledger holds back."""
    src = GEN.read_text()
    gate = src.index('why[(t, b)] = "reject_withdrawn"')
    accept = src.index('stats[f"ACCEPT_T{tier}"] += 1')
    assert gate < accept, (
        "the withdrawal gate no longer runs before ACCEPT -- it cannot suppress "
        "anything from that position")


def test_the_merge_carry_forward_is_gated_too():
    """Without this, `--merge` launders the shipped file's own
    live-and-withdrawn memberships forward on every run."""
    src = GEN.read_text()
    assert "_wd_denied" in src, "the --merge carry-forward denylist gate is gone"
    # NB: count the CALL, not the substring -- `def _wd_denied(g, f):` also
    # contains "_wd_denied(g, f)", and counting that made this assertion survive
    # the deletion of a call site (caught by mutating it, 2026-09-13).
    assert src.count("if _wd_denied(g, f):") >= 2, (
        "the carry-forward gate is applied on fewer than both merge paths "
        "(re-derived-survivor and whole-group)")


def test_the_ledger_is_read_independently_of_merge():
    """A run without --merge must still be protected, or the guard is one
    forgotten flag away from vacuous."""
    tree = ast.parse(GEN.read_text())
    defaults = [n for n in ast.walk(tree)
                if isinstance(n, ast.Call)
                and getattr(n.func, "attr", "") == "add_argument"
                and n.args and getattr(n.args[0], "value", "") == "--withdrawals"]
    assert defaults, "--withdrawals is gone; the ledger path is not configurable"
    kw = {k.arg: k for k in defaults[0].keywords}
    assert "default" in kw, (
        "--withdrawals has no default, so a run that omits it is unprotected")
    assert not isinstance(kw["default"].value, ast.Constant) or \
        kw["default"].value.value not in (None, ""), \
        "--withdrawals defaults to nothing, which disables the guard silently"


def test_an_empty_ledger_refuses_rather_than_running_unprotected():
    """'0 suppressed' is exactly what a vacuous load looks like."""
    import sys
    sys.path.insert(0, str(ROOT / "tools"))
    from alias_withdrawals import load_ledger, VacuousLedger
    doc = json.loads(LEDGER.read_text())
    for g in doc["groups"]:
        g.pop("withdrawn", None)
    tmp = Path(__file__).parent / ".pytest_empty_ledger.json"
    tmp.write_text(json.dumps(doc))
    try:
        with pytest.raises(VacuousLedger):
            load_ledger(tmp)
    finally:
        tmp.unlink(missing_ok=True)


def test_there_is_no_blanket_allow_all():
    """An override must NAME the record it overrides -- `overrides_class` must
    match, and a `reason` is mandatory. A blanket flag would defeat the guard."""
    src = MOD.read_text()
    assert "overrides_class" in src and "reason" in src, (
        "the override path no longer requires naming the record it overrides")


def test_the_ledger_keys_do_not_alias_two_groups_together():
    """The denylist keys on (survivor, spelling) OR (address, spelling). Both
    keys must be injective over groups or a denial could over-block."""
    groups = json.loads(LEDGER.read_text())["groups"]
    survs = [g["survivor"] for g in groups]
    addrs = [g["address"] for g in groups if g.get("address")]
    assert len(survs) == len(set(survs)), "survivor is no longer unique per group"
    assert len(addrs) == len(set(addrs)), "address is no longer unique per group"
