#!/usr/bin/env python3
"""The container census must select on COVERAGE, not on a mismatch-count band.

WHAT THIS PINS (lane W3-F, 2026-09-11, adopting CONTAINER2 §1)
-------------------------------------------------------------
`matched_code` is ALL-OR-NOTHING per row, so a row crosses only when EVERY
charged site on it resolves. The band criterion ("mismatches <= 3") is a
different question and is wrong in BOTH directions:

  * it ADMITS a 3-charge row with one container site, which cannot cross when
    the pair is fixed, and
  * it MISSES an 8-charge row whose every site is a container site, which can.

Lane L7-CONTAINER2 measured the band 3.1x too small on this class (19,792 B
briefed vs 61,952 B realisable at its base `3f9619c5`).

THE TWO FIXTURES BELOW ARE THOSE TWO SENTENCES, MECHANISED. They are synthetic
on purpose: the criterion must be assertable without a built tree, a 7-minute
diff pass, or the live population -- which moves under every map repair, so a
test keyed on it would rot into a tautology or a false red.

⚠ The site cells are real objdiff INSTRUCTION TEXT (`bl ?insert@...`), not bare
symbols. Feeding bare symbols here would make these tests pass while the real
parser fails, which is the exact vacuity the census's own header warns about --
its first full run over 8,191 rows reported a clean, decisive ZERO for it.
"""

import json
import subprocess
import sys
from pathlib import Path

import pytest

TOOLS = Path(__file__).resolve().parent
sys.path.insert(0, str(TOOLS))

import container_type_census as CC  # noqa: E402

# real mangled sibling instantiations, as they appear in a `bl` cell
LIST_CHARCLIP = "bl ?insert@?$list@PAVCharClip@@V?$allocator@PAVCharClip@@@@@@QAAXXZ"
LIST_OBJECT = "bl ?insert@?$list@PAVObject@@V?$allocator@PAVObject@@@@@@QAAXXZ"
# a charge this class canNOT close
REG_SITE = ("add r3, r4, r5", "add r3, r5, r4", "diff_reg")
IMM_SITE = ("li r3, 12", "li r3, 24", "diff_arg")


def container_site():
    return (LIST_CHARCLIP, LIST_OBJECT, "diff_arg")


# ── the criterion itself ─────────────────────────────────────────────────────

def test_a_container_site_is_recognised_from_instruction_text():
    """Anti-vacuity: if this is False every test below passes for free."""
    assert CC.is_container_site(*container_site())


def test_three_charges_one_container_does_NOT_cross():
    """The row the BAND admits and reality refuses (CONTAINER2 §1, sentence 1)."""
    sites = [container_site(), REG_SITE, IMM_SITE]
    total, cont, covered = CC.row_coverage(sites)
    assert (total, cont) == (3, 1)
    assert covered is False, "a row with a non-container charge cannot cross"


def test_eight_charges_all_container_DOES_cross():
    """The row the BAND misses and reality allows (CONTAINER2 §1, sentence 2)."""
    sites = [container_site()] * 8
    total, cont, covered = CC.row_coverage(sites)
    assert (total, cont) == (8, 8)
    assert covered is True


def test_zero_charges_is_not_vacuously_covered():
    """`all([])` is True; a row objdiff returned nothing for must NOT be a prize."""
    assert CC.row_coverage([]) == (0, 0, False)


def test_a_non_diff_arg_charge_blocks_coverage():
    assert CC.row_coverage([REG_SITE])[2] is False


def test_identical_spellings_are_not_a_container_site():
    """Same instantiation on both sides is not a divergence this class owns."""
    assert not CC.is_container_site(LIST_OBJECT, LIST_OBJECT, "diff_arg")


def test_different_class_templates_are_not_siblings():
    vec = "bl ?insert@?$vector@PAVObject@@V?$allocator@PAVObject@@@@@@QAAXXZ"
    assert not CC.is_container_site(LIST_CHARCLIP, vec, "diff_arg")


# ── the summary must report both criteria, and the gap between them ──────────

def _rec(sym, size, sites_total, container, unit="default/U"):
    return {"unit": unit, "sym": sym, "size": size,
            "target": "T", "base": "B", "fold": "NOT_REFUTED_BY_MAP",
            "caller_family": None, "callee_family": "list",
            "caller_class": "CALLER_ORDINARY",
            "row_sites": sites_total, "row_container_sites": container,
            "fully_covered": sites_total > 0 and sites_total == container}


def test_summary_separates_band_from_crossing(capsys):
    recs = [
        _rec("?banded_but_blocked@@", 1000, 3, 1),   # band YES, crossing NO
        _rec("?wide_but_covered@@", 5000, 8, 8),     # band NO,  crossing YES
        _rec("?narrow_and_covered@@", 200, 1, 1),    # both
    ]
    got = CC.summarise(recs, total_code=1_000_000)
    assert got["cand_rows"] == 3
    # crossing = the two fully-covered rows
    assert got["cross_rows"] == 2
    assert got["cross_bytes"] == 5200
    # band = the two rows with <= 3 charges
    assert got["band_rows"] == 2
    assert got["band_bytes"] == 1200
    # and the overlap is only the narrow covered row
    assert got["band_and_covered_rows"] == 1
    assert got["band_and_covered_bytes"] == 200
    # the direction of L7's finding: crossing outprices the band on this shape
    assert got["cross_bytes"] > got["band_bytes"]
    out = capsys.readouterr().out
    assert "CROSSING population" in out
    assert "43-75%" in out, "the map-error rate must travel WITH the label"
    assert "NOT_REFUTED_BY_MAP" in out


def test_rows_are_deduped_by_row_not_counted_per_site():
    """Two charged sites on ONE row are one row and its size counted ONCE.

    A per-site sum would double-count bytes, which is how a class gets briefed
    at several times its real size.
    """
    recs = [_rec("?same@@", 900, 2, 2), _rec("?same@@", 900, 2, 2)]
    got = CC.summarise(recs, total_code=1_000_000)
    assert got["cand_rows"] == 1 and got["cand_bytes"] == 900
    assert got["cross_rows"] == 1 and got["cross_bytes"] == 900


def test_empty_candidate_class_over_nonempty_sites_is_flagged(capsys):
    recs = [dict(_rec("?x@@", 100, 1, 1), fold="UNKNOWN")]
    CC.summarise(recs, total_code=1_000_000)
    assert "candidate class is EMPTY" in capsys.readouterr().out


# ── the label rename ─────────────────────────────────────────────────────────

def test_the_nofold_label_is_gone_from_the_code_path():
    """`NOFOLD` asserted "did not fold"; the evidence only supports "not refuted".

    Checked against the SOURCE, because the label is a string the tool emits and
    a reader acts on -- and a reader who stopped at "NOFOLD" would have
    concluded the exact opposite of L7's headline finding.
    """
    src = (TOOLS / "container_type_census.py").read_text()
    assert 'fold = "NOT_REFUTED_BY_MAP"' in src
    assert 'fold = "NOFOLD"' not in src


# ── --from refuses a band-era dump rather than mislabelling it ───────────────

def test_from_refuses_a_dump_that_predates_the_criterion(tmp_path):
    old = [{"unit": "u", "sym": "s", "size": 10, "fold": "NOFOLD",
            "caller_class": "CALLER_ORDINARY"}]
    f = tmp_path / "old.json"
    f.write_text(json.dumps(old))
    r = subprocess.run(
        [sys.executable, str(TOOLS / "container_type_census.py"),
         "--project", str(TOOLS.parent), "--out", str(tmp_path / "o.json"),
         "--from", str(f)],
        capture_output=True, text=True)
    assert r.returncode != 0
    assert "predates the crossing criterion" in (r.stdout + r.stderr)


if __name__ == "__main__":
    sys.exit(pytest.main([__file__, "-q"]))
