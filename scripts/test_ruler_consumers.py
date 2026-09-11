#!/usr/bin/env python3
"""The ruler-consumer guard, generalised over `_CONSUMERS` (lane W4-F, 2026-09-11).

`scripts/analysis/ruler.py` keeps a `_CONSUMERS` tuple of the files that drive
objdiff-cli for a SCORE, because a hardcoded `-c functionRelocDiffs=...` is how
three separate tools silently diverged from the grader after `d04c83df` shipped
`name_check` (2026-08-12).  Its own comment states the problem with such a list:
"a guard that only checks the files someone remembered is a guard whose coverage
is a memory, not a property."

`scripts/orchestrator/test_atexit_ruler.py` carried a `test_no_hardcoded_ruler_in_source`
that applied exactly the right check to exactly ONE of the five entries -- the
`scripts/atexit_fuzzy_verify.py` retired in this lane.  Rather than delete that
assertion with its subject, it is generalised here to every entry, which is a
strictly stronger guard than the repo had before the retirement.

⚠ WHY THIS IS NOT A COVERAGE TEST, measured.  The tempting stronger version is
"every file mentioning `functionRelocDiffs` must be in `_CONSUMERS`".  Counted on
this tree: 59 non-build `.py` files mention the knob and 5 are listed, because
most are one-off harvest scripts that pass an EXPLICIT ruler on purpose (which is
correct and needs no registration).  Such a gate would need an exemption list of
~54 entries -- i.e. it would replace one memory with a bigger one.  `_CONSUMERS`
is checked by hand against
`command grep -rln "objdiff-cli" tools/ scripts/`, as ruler.py says.
"""
import re
from pathlib import Path
import sys

REPO = Path(__file__).resolve().parent.parent
sys.path.insert(0, str(REPO / "scripts"))

from analysis.ruler import _CONSUMERS  # noqa: E402

# The shape of the defect: a literal `-c`, then `functionRelocDiffs=<mode>`.
HARDCODED = re.compile(r"""["']-c["']\s*,\s*["']functionRelocDiffs=""")


def _hits(text):
    return [i + 1 for i, line in enumerate(text.splitlines()) if HARDCODED.search(line)]


def test_every_listed_consumer_exists():
    """A retired tool left in the list makes the guard lie about its coverage --
    and is the bookkeeping error this lane's own deletion would have introduced."""
    missing = [c for c in _CONSUMERS if not (REPO / c).exists()]
    assert not missing, f"_CONSUMERS names file(s) that do not exist: {missing}"


def test_no_consumer_hardcodes_a_ruler():
    """Generalises the retired per-tool assertion to all of `_CONSUMERS`."""
    assert _CONSUMERS, "_CONSUMERS is empty -- this test would be vacuous"
    bad = {}
    for c in _CONSUMERS:
        h = _hits((REPO / c).read_text())
        if h:
            bad[c] = h
    assert not bad, (
        f"hardcoded ruler(s): {bad}. Resolve the ruler at runtime via "
        f"scripts/analysis/ruler.py instead -- a constant is what rotted three "
        f"tools across the 2026-08-12 name_check flip."
    )


def test_the_detector_can_fail(tmp_path):
    """Must-be-able-to-fail: the pattern has to actually match the defect it
    describes, or the two tests above pass by being blind."""
    good = tmp_path / "good.py"
    good.write_text("args = [*ruler.config_args, '--verdict']\n")
    assert _hits(good.read_text()) == []
    bad = tmp_path / "bad.py"
    bad.write_text("cmd = [cli, 'diff',\n       '-c', 'functionRelocDiffs=none',\n]\n")
    assert _hits(bad.read_text()) == [2], 'the detector misses a hardcoded ruler'
