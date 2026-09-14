#!/usr/bin/env python3
"""Replay the W16-J landing through both alias resolvers.

WHY THIS TEST EXISTS
--------------------
On the W16-J landing the v1 resolver silently dropped -3,728 B of main's
concurrent alias memberships: it replaced a both-sides-changed group with the
lane's copy, and treated a survivor rename as delete+add. Nothing caught it --
the rebase was clean, the build was green, and the loss was found by hand.
v2 pairs by composite key, re-pairs by unique address, and merges field by
field. This replays the real commits and requires v2 to reproduce the
hand-repaired file STRUCTURALLY, and requires v1 NOT to.

WHAT IS ASSERTED, AND WHAT IS DELIBERATELY NOT
----------------------------------------------
Asserted: ``survivor``, ``folded``, ``withdrawn``, ``address`` on every group.
Those are the fields that decide which spellings objdiff forgives -- i.e. the
ones whose loss costs bytes.

NOT asserted: ``evidence``. It is a human-written prose note. The hand-repaired
commit wrote its notes by hand where v2 appends both sides' notes, so two of the
1,632 groups differ in that string and in nothing else. Treating that as a
failure would make the test red for a non-defect; `--test`'s own exit code does
exactly that, which is why this file calls three_way() instead of shelling out.

⚠ THE v1 ARM IS THE POINT. A control that cannot fail proves nothing, and the
structural check is only worth something because v1 measurably trips it.

Needs full git history (the four refs below). A shallow checkout SKIPS, loudly.
"""
import json
import subprocess
import sys
from pathlib import Path

import pytest

HERE = Path(__file__).resolve().parent
sys.path.insert(0, str(HERE))

import resolve_aliases                      # noqa: E402
from _v1_frozen import three_way_v1         # noqa: E402

P = 'scripts/symbol_aliases.json'

# The real W16-J landing, verified by the coordinator 2026-09-14.
OURS   = 'f29132d7'   # main at the moment of the W16-J rebase
BASE   = 'a8f9e92b'   # the lane's base
THEIRS = '95ebc590'   # the lane's pre-rebase tip
EXPECT = 'dcd8d6fe'   # the hand-repaired file

STRUCT_FIELDS = ('survivor', 'folded', 'withdrawn', 'address')


def repo_root() -> Path:
    out = subprocess.run(['git', '-C', str(HERE), 'rev-parse', '--show-toplevel'],
                         capture_output=True, text=True)
    if out.returncode:
        pytest.skip('not a git checkout; the replay needs the repository')
    return Path(out.stdout.strip())


def _show(root: Path, ref: str):
    out = subprocess.run(['git', '-C', str(root), 'show', f'{ref}:{P}'],
                         capture_output=True)
    if out.returncode:
        pytest.skip(
            f'ref {ref} (or {P} in it) is unreachable -- this replay needs full '
            f'git history. Fetch the full repository to run it.')
    return json.loads(out.stdout)


@pytest.fixture(scope='module')
def fixture():
    root = repo_root()
    return {name: _show(root, ref) for name, ref in
            (('ours', OURS), ('base', BASE), ('theirs', THEIRS), ('expect', EXPECT))}


def _struct(groups):
    """{group key -> the structural fields only}. `evidence` is dropped."""
    return {resolve_aliases.key(g): {f: g.get(f) for f in STRUCT_FIELDS}
            for g in groups}


def _structural_diff(got_groups, expect_groups):
    got, exp = _struct(got_groups), _struct(expect_groups)
    # keys carry None in the `name` slot for unnamed groups, so sort on the
    # stringified key rather than the tuple (None < str raises).
    return sorted((k for k in set(got) | set(exp) if got.get(k) != exp.get(k)),
                  key=lambda k: tuple('' if x is None else str(x) for x in k))


def test_v2_reproduces_the_hand_repaired_file_structurally(fixture):
    ours = json.loads(json.dumps(fixture['ours']))
    n = resolve_aliases.three_way(ours, fixture['base'], fixture['theirs'])
    assert n == 4, f'expected 4 deltas on the W16-J replay, got {n}'
    assert len(ours['groups']) == len(fixture['expect']['groups']), (
        f"group count {len(ours['groups'])} != expect "
        f"{len(fixture['expect']['groups'])}")
    diff = _structural_diff(ours['groups'], fixture['expect']['groups'])
    assert diff == [], (
        'v2 diverges from the hand-repaired file on '
        f'{len(diff)} group(s): {diff[:5]}')


def test_v1_fails_the_same_structural_check(fixture):
    """The control. If this ever passes, the test above stopped discriminating."""
    ours = json.loads(json.dumps(fixture['ours']))
    three_way_v1(ours, fixture['base'], fixture['theirs'])
    diff = _structural_diff(ours['groups'], fixture['expect']['groups'])
    assert diff != [], (
        'v1 PASSED the structural check -- the check no longer discriminates, '
        'so test_v2_... above proves nothing. Do not silence this: find out '
        'why the fixture stopped exercising the v1 defect.')


def test_evidence_is_the_only_non_structural_difference(fixture):
    """Documents the 2 known differences so nobody reads them as a defect."""
    ours = json.loads(json.dumps(fixture['ours']))
    resolve_aliases.three_way(ours, fixture['base'], fixture['theirs'])
    emap = {resolve_aliases.key(g): g for g in fixture['expect']['groups']}
    omap = {resolve_aliases.key(g): g for g in ours['groups']}
    fields = set()
    for k in set(emap) | set(omap):
        e, o = emap.get(k), omap.get(k)
        if e is None or o is None:
            fields.add('<group missing>')
            continue
        fields |= {f for f in set(e) | set(o) if e.get(f) != o.get(f)}
    assert fields <= {'evidence'}, (
        f'differences outside `evidence`: {sorted(fields)}')
