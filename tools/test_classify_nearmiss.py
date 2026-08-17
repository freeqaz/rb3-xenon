#!/usr/bin/env python3
"""Tests for `classify_nearmiss.py` -- the near-miss mismatch-CAUSE classifier.

Regression pin for the NEGATIVE-HEX defect (task #116, fixed 2026-08-17).

`tok_syms` strips bare registers and immediates so that what remains is the set
of SYMBOL tokens, and `classify_insn` compares those sets to decide between
NAME_RELOC / WRONG_PAIR before falling through to the typed_args scan that
produces OFFSET / REG. Its immediate alternation was::

    r\\d+|f\\d+|cr\\d+|0x[0-9A-Fa-f]+|-?\\d+

which covers positive hex and both signs of DECIMAL but not negative HEX.
`flat_args` renders a Signed operand as `-0x%x`, and negative displacements are
the ordinary spelling of a stack slot below the frame pointer and of a negative
struct offset -- so `-0x8` was not stripped, entered the symbol set, and two
sides differing only in that displacement were reported as WRONG_PAIR
("objdiff mis-paired the top-level symbol; needs re-pin") instead of OFFSET
("struct layout / stack bug"). The two verdicts route to opposite kinds of
work, which is why this is a repair and not cosmetics.

The defect is SPELLING-INDEPENDENT and PREDATES the objdiff-cli fdc5113 args
change; it is not part of the regression described in the module banner.

Every `typed_args` shape below matches objdiff-cli JSON as emitted with
`--include-instructions` for this repo's PPC/COFF objects (Signed carries a
Python int, negative for a below-frame displacement; Opaque carries the
register spelling; Symbol carries the relocation name).
"""
from __future__ import annotations

import importlib.util
import re
import sys
from pathlib import Path

import pytest

_MOD = Path(__file__).resolve().parent / "classify_nearmiss.py"
_spec = importlib.util.spec_from_file_location("classify_nearmiss", _MOD)
cn = importlib.util.module_from_spec(_spec)
sys.modules["classify_nearmiss"] = cn
_spec.loader.exec_module(cn)


def _side(opcode, typed_args):
    return {"opcode": opcode, "typed_args": typed_args}


def _sig(v):
    return {"type": "Signed", "value": v}


def _reg(r):
    # objdiff-cli spells this exactly `Register`, and classify_insn's typed_args
    # scan tests that literal to separate REG from OFFSET. Verified against real
    # `--include-instructions` output for this repo's objects, whose observed
    # type vocabulary is Register / Signed / Symbol / BranchDest / Unsigned /
    # Other. Spelling it anything else silently reroutes a register diff to
    # OFFSET (which is how the first draft of this file failed).
    return {"type": "Register", "value": r}


# --------------------------------------------------------------------------
# tok_syms: the stripping set itself
# --------------------------------------------------------------------------

@pytest.mark.parametrize("tok", [
    # THE defect: negative hex, the spelling flat_args emits for Signed < 0.
    "-0x8",
    "-0x120",
    "-0xdeadbeef",
    "-0XABC",     # uppercase prefix, negative
    # Already covered before the fix -- scope pin, must not regress.
    "0x38",
    "0xDEADBEEF",
    "42",
    "-42",
    "r3",
    "r31",
    "f0",
    "cr7",
])
def test_tok_syms_strips_bare_operand(tok):
    """A bare register/immediate is NOT a symbol token."""
    assert cn.tok_syms(tok) == set(), tok
    assert cn.tok_syms(f"r3, {tok}") == set(), tok


@pytest.mark.parametrize("tok", [
    "?Foo@Bar@@QAAXXZ",
    "fn_82345678",
    "lbl_82345678",
    "sym@h",
    "0x38(r4)",      # display spelling -- flat_args must undo this, see its docstring
    "-0x8(r1)",
    "0xzz",
    "-0x",
    "--0x8",
    "0x8-",
])
def test_tok_syms_keeps_non_operand(tok):
    """Anything that is not a WHOLE bare operand stays a symbol token."""
    assert cn.tok_syms(tok) == {tok}, tok


# --------------------------------------------------------------------------
# classify_insn: the verdict the defect inverted
# --------------------------------------------------------------------------

def test_negative_displacement_diff_is_offset_not_wrong_pair():
    """The defect, end to end.

    Two `lwz` rows identical but for a NEGATIVE stack displacement. Before the
    fix `-0x8` and `-0xc` were symbol tokens, the sets differed, neither matched
    UNNAMED, and classify_insn returned WRONG_PAIR. It is an immediate diff:
    OFFSET.
    """
    ins = {
        "match_type": "replace",
        "target": _side("lwz", [_reg("r3"), _sig(-8), _reg("r1")]),
        "base": _side("lwz", [_reg("r3"), _sig(-12), _reg("r1")]),
    }
    assert cn.classify_insn(ins) == "OFFSET"


def test_positive_displacement_diff_still_offset():
    """Scope pin: positive hex was never affected and must not move."""
    ins = {
        "match_type": "replace",
        "target": _side("lwz", [_reg("r3"), _sig(8), _reg("r4")]),
        "base": _side("lwz", [_reg("r3"), _sig(12), _reg("r4")]),
    }
    assert cn.classify_insn(ins) == "OFFSET"


def test_negative_stwu_frame_size_diff_is_offset():
    """`stwu r1, -0x100(r1)` vs `-0xe0`: a frame-size delta, not a mis-pair."""
    ins = {
        "match_type": "replace",
        "target": _side("stwu", [_reg("r1"), _sig(-256), _reg("r1")]),
        "base": _side("stwu", [_reg("r1"), _sig(-224), _reg("r1")]),
    }
    assert cn.classify_insn(ins) == "OFFSET"


def test_register_only_diff_with_negative_displacement_is_reg():
    """A negative displacement present on BOTH sides must not mask a REG diff.

    Before the fix the shared `-0x8` landed in both symbol sets, so the sets
    still compared equal here and this case happened to survive -- but only by
    accident. Pin it so the strip cannot start dropping the register instead.
    """
    ins = {
        "match_type": "replace",
        "target": _side("lwz", [_reg("r3"), _sig(-8), _reg("r1")]),
        "base": _side("lwz", [_reg("r4"), _sig(-8), _reg("r1")]),
    }
    assert cn.classify_insn(ins) == "REG"


def test_real_symbol_difference_still_wrong_pair():
    """Scope pin: the fix must not swallow a genuine two-named-symbols mis-pair."""
    ins = {
        "match_type": "replace",
        "target": _side("bl", [{"type": "Symbol", "value": "?Poll@RndMat@@UAAXXZ"}]),
        "base": _side("bl", [{"type": "Symbol", "value": "?Poll@CharPollableSorter@@UAAXXZ"}]),
    }
    assert cn.classify_insn(ins) == "WRONG_PAIR"


def test_unnamed_target_symbol_still_name_reloc():
    """Scope pin: the NAME_RELOC arm is untouched."""
    ins = {
        "match_type": "replace",
        "target": _side("bl", [{"type": "Symbol", "value": "fn_82345678"}]),
        "base": _side("bl", [{"type": "Symbol", "value": "?Poll@RndMat@@UAAXXZ"}]),
    }
    assert cn.classify_insn(ins) == "NAME_RELOC"


def test_flat_args_emits_negative_hex():
    """The producer half of the defect: flat_args really does spell Signed<0 as -0x.

    If this ever changes, the tok_syms alternation above is guarding a spelling
    nothing emits and this whole file is measuring the wrong thing.
    """
    assert cn.flat_args(_side("lwz", [_reg("r3"), _sig(-8), _reg("r1")])) == "r3, -0x8, r1"


def test_bare_operand_re_rejects_the_old_alternation_gap():
    """Direct pin on the regex, so a mutant that reverts it is red here too."""
    assert cn._BARE_OPERAND_RE.fullmatch("-0x8")
    assert cn._BARE_OPERAND_RE.fullmatch("-0X8")
    assert not cn._BARE_OPERAND_RE.fullmatch("-0x8(r1)")
    assert not cn._BARE_OPERAND_RE.fullmatch("sym@l")
