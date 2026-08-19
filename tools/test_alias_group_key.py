"""Pin which field identifies an alias group in scripts/symbol_aliases.json.

`name` does not: it is a display label, it is optional, and it is shared by up
to 69 groups. `survivor` and `address` each identify a group exactly. A
consumer that keys acceptance, dedupe, a join or a census on `name` conflates
groups whose verdicts differ -- measured in decomp-bench
`archive/runs/2026-08-19-namekey-audit/`.
"""
import json
import subprocess
import sys
from collections import Counter
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parent.parent
ALIASES = ROOT / "scripts" / "symbol_aliases.json"


@pytest.fixture(scope="module")
def groups():
    return json.loads(ALIASES.read_text())["groups"]


def test_survivor_identifies_a_group(groups):
    assert len({g["survivor"] for g in groups}) == len(groups)


def test_address_identifies_a_group(groups):
    assert len({g["address"] for g in groups}) == len(groups)


def test_name_does_not_identify_a_group(groups):
    """Not a wish -- a property of the shipped file, asserted so that a reader
    who reaches for `name` as a key finds this test instead of a wrong join."""
    names = Counter(g.get("name") for g in groups)
    assert len(names) < len(groups)
    assert max(names.values()) > 1


def test_some_groups_carry_no_name_at_all(groups):
    """The fold-thunk and COMDAT-identity tiers mint groups without a `name`,
    so `g["name"]` raises and `g.get("name") in accepted` drops them."""
    assert any("name" not in g for g in groups)


def test_the_rendered_map_never_keys_on_name(groups):
    """gen_symbol_alias_map puts `name` only in a `;` comment, and objdiff's
    parse_msvc_map buckets by ADDRESS. So two groups sharing a label stay two
    equivalence classes -- the shipped map is a 1:1 rendering of the file, and
    the label does no work in it."""
    sys.path.insert(0, str(ROOT / "tools"))
    import gen_symbol_alias_map as G

    text = G.render_map(groups)
    data = [l for l in text.splitlines() if l.startswith(" 0001:")]
    assert len(data) == sum(1 + len(g.get("folded", [])) for g in groups)
    assert len({l.split()[2] for l in data}) == len(groups)

    twins = G.render_map([
        {"name": "Replace", "address": "0x82000010",
         "survivor": "?a@@YAXXZ", "folded": ["?b@@YAXXZ"]},
        {"name": "Replace", "address": "0x82000020",
         "survivor": "?c@@YAXXZ", "folded": ["?d@@YAXXZ"]}])
    assert len({l.split()[2] for l in twins.splitlines()
                if l.startswith(" 0001:")}) == 2


def test_a_name_less_group_does_not_crash_the_renderer():
    sys.path.insert(0, str(ROOT / "tools"))
    import gen_symbol_alias_map as G

    G.render_map([{"address": "0x82000010", "survivor": "?s@@YAXXZ",
                   "folded": ["?f@@YAXXZ"]}])


def test_no_alias_tool_selects_groups_by_name():
    """A grep-level tripwire. `[g for g in groups if g.get("name") in ...]` is
    the shape that admitted 186 refused groups in decomp-synth's --emit-map;
    nothing in this repo may grow one."""
    hits = subprocess.run(
        ["grep", "-rn", "--include=*.py", "--exclude", Path(__file__).name, "-E",
         r"if +g(\[|\.get\()['\"]name['\"](\]|\)) +(in|not in) ",
         str(ROOT / "tools"), str(ROOT / "scripts")],
        capture_output=True, text=True).stdout.strip()
    assert not hits, "alias group selected by `name`:\n" + hits
