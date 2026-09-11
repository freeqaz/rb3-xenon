#!/usr/bin/env python3
"""Pin the SHAPE KEY of the two crossing_worklist importers (lane W4-F, 2026-09-11).

`tools/shape_families.py` and `tools/structural_decompose.py` both build an
instruction "shape" out of objdiff's `typed_args`, and both used to DISCARD the
`Symbol` (relocation) args on the rationale "masked by functionRelocDiffs=none".
That rationale died with `d04c83df` (2026-08-12, the name_check flip) and became
load-bearing with `d1b4f708`, which put crossing_worklist -- the module that FEEDS
both -- on the graded ruler at runtime.  `structural_decompose` additionally
filtered on ('Register','Signed','Unsigned','Opaque'), the refuted first draft in
which `Opaque` matches NOTHING objdiff emits.

These tests are deliberately build-free and cache-free: they run against synthetic
diffs, so they pin the KEY rather than a population that drifts every wave.  Every
assertion is paired with the OLD key's behaviour, so each one demonstrates that
something CHANGED -- a test that only asserts the new behaviour cannot tell a fix
from a no-op.
"""
import os
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import shape_families as SF            # noqa: E402
import structural_decompose as SD      # noqa: E402

# The arg-type tuples this lane replaced, kept ONLY as reference points.
OLD_SF = ('Register', 'Signed', 'Unsigned', 'Other', 'BranchDest')
OLD_SD = ('Register', 'Signed', 'Unsigned', 'Opaque')

# Measured ground truth: over 4,000 cached objdiff diffs these are the ONLY
# typed_arg types objdiff emits.  `Opaque` is absent -- it is not a spelling of
# anything, which is why the refuted draft failed silently instead of erroring.
OBJDIFF_ARG_TYPES = ('Register', 'Symbol', 'Signed', 'BranchDest', 'Other', 'Unsigned')


def _side(op, args):
    return {'opcode': op, 'typed_args': [{'type': t, 'value': str(v)} for t, v in args]}


def _reloc_charge_diff():
    """A diff whose ONE charged site is a relocation NAME -- the wrong-callee
    class, and the single most common shape in the live population (1,982 of
    3,226 sub-100 rows carry nothing else).  Both sides are otherwise identical."""
    return {'instructions': [
        {'match_type': 'equal', 'target': _side('stw', [('Register', 'r31')]),
         'base': _side('stw', [('Register', 'r31')])},
        {'match_type': 'diff_arg',
         'target': _side('bl', [('Symbol', '??1?$ObjDirPtr@VObjectDir@@@@UAA@XZ')]),
         'base': _side('bl', [('Symbol', '??1?$vector@URecordedFrame@@@@UAA@XZ')])},
    ]}


# --------------------------------------------------------------------------
# shape_families: a single Symbol-arg charge must be VISIBLE in the shape.
# --------------------------------------------------------------------------
def _sf_shapes(diff, argt):
    saved = SF.ARGT
    SF.ARGT = argt
    try:
        return SF.row_signature(diff, False)[1]
    finally:
        SF.ARGT = saved


def test_symbol_only_charge_is_invisible_under_the_old_key():
    """CONTROL (the `absent from the old ranking` half).  With Symbol dropped the
    two sides render IDENTICALLY, which is what collapsed 1,387 rows / 295,264 B
    into one manufactured family at the top of the ranking."""
    (mt, t, b), = _sf_shapes(_reloc_charge_diff(), OLD_SF)
    assert mt == 'diff_arg'
    assert t == b == ('bl', ()), (t, b)


def test_symbol_only_charge_is_visible_under_the_new_key():
    """The same row under the shipped key: the sides differ, so the row carries a
    real shape and can be ranked and grouped by WHICH callee pair diverges."""
    (mt, t, b), = _sf_shapes(_reloc_charge_diff(), SF.ARGT)
    assert mt == 'diff_arg'
    assert t != b, 'relocation-name charge still renders identically on both sides'
    assert t == ('bl', ('??1?$ObjDirPtr@VObjectDir@@@@UAA@XZ',)), t
    assert b == ('bl', ('??1?$vector@URecordedFrame@@@@UAA@XZ',)), b


def test_shipped_key_admits_symbol_and_nothing_bogus():
    assert 'Symbol' in SF.ARGT
    assert set(SF.ARGT) <= set(OBJDIFF_ARG_TYPES), 'ARGT names a type objdiff never emits'


def test_two_instantiations_of_one_callee_defect_still_group():
    """The exclusion's surviving rationale was "one defect across template
    instantiations must still group".  Literal names keep that WHEN THE CALLEE
    PAIR IS THE SAME, which is the case one source fix actually covers."""
    def d(sym_t, sym_b):
        return {'instructions': [{'match_type': 'diff_arg',
                                  'target': _side('bl', [('Symbol', sym_t)]),
                                  'base': _side('bl', [('Symbol', sym_b)])}]}
    a = SF.row_signature(d('?MemAlloc@@YAPAXHH@Z', '?MemAllocTemp@@YAPAXHH@Z'), False)[0]
    b = SF.row_signature(d('?MemAlloc@@YAPAXHH@Z', '?MemAllocTemp@@YAPAXHH@Z'), False)[0]
    c = SF.row_signature(d('?MemAlloc@@YAPAXHH@Z', '?Unrelated@@YAXXZ'), False)[0]
    assert a == b, 'rows sharing one callee-pair defect no longer group'
    assert a != c, 'rows with unrelated callee pairs are grouped anyway'


# --------------------------------------------------------------------------
# structural_decompose: the `Opaque` artifact, and the transposition detector.
# --------------------------------------------------------------------------
def test_opaque_matched_nothing_so_three_live_kinds_were_dropped():
    """CONTROL for the `Opaque` artifact.  The refuted tuple did not merely omit
    Symbol: because `Opaque` is not a real type, `Other` (shift/mask amounts) and
    `BranchDest` were silently dropped too."""
    assert 'Opaque' not in OBJDIFF_ARG_TYPES
    dropped = {t for t in OBJDIFF_ARG_TYPES if t not in OLD_SD}
    assert dropped == {'Symbol', 'Other', 'BranchDest'}, dropped
    assert 'Opaque' not in SD.ARGT, 'the refuted draft is back'
    assert set(SD.ARGT) == set(OBJDIFF_ARG_TYPES), SD.ARGT


def test_shift_amount_is_no_longer_invisible_to_the_key():
    """`Other` holds mask widths.  Under the old tuple two DIFFERENT mask widths
    produced the SAME key, so the instructions compared equal."""
    t = _side('clrlwi.', [('Register', 'r3'), ('Other', '24')])
    b = _side('clrlwi.', [('Register', 'r3'), ('Other', '16')])
    assert SD._old_key(t) == SD._old_key(b), 'reference old key does not reproduce the bug'
    assert SD.key(t) != SD.key(b), 'shift amount still invisible'


def test_transposition_detector_fixtures():
    """The detector's own fixtures: a genuine register-only transposition is still
    detected (anti-vacuity), while a reorder that also changes a callee / mask /
    branch target is refused where the old key called it `scheduling`."""
    ok, lines = SD.run_fixture_control(verbose=False)
    assert ok, '\n'.join(lines)


def test_the_fixture_control_can_fail():
    """Must-be-able-to-fail: neuter the key and the fixtures must go red.  Without
    this, all of the above could be passing vacuously."""
    saved = SD.key
    SD.key = lambda side: None if not side else (side.get('opcode'), ())
    try:
        ok, _ = SD.run_fixture_control(verbose=False)
    finally:
        SD.key = saved
    assert not ok, 'fixtures pass even with an arg-blind key -- they prove nothing'
