#!/usr/bin/env python3
"""Tests for `objdiff_to_m2c.py` -- the objdiff-JSON -> m2c-assembly converter.

Doc 43 (`docs/plans/reverse-compilation/data/training-methods-review/
43_READOUT_V7_GENERATOR_REUSE_2026-08-05.md` §4) found m2c seed generation
0/8 on GC/Wii mwcc objects: dtk-split ELF disassembly spells small-data-area
(SDA) constant-pool references as a bare `@NNN` (e.g. `lfd f0, @158`) or a
bare named SDA global (e.g. `stw r3, sSkidMarkRaster`) with the base register
omitted -- the converter passed these through unchanged and m2c rejected the
malformed address-mode syntax (no `(register)`).

Every args string exercised below was captured from REAL objdiff-cli 4.2.3
JSON self-diff output against real built clone objects (not synthesized from
the doc's two examples alone):
  - `lfd f0, @158` from doldecomp_melee's `MSL/ansi_fp.o :: __num2dec`
  - `stw r3, paremit_sd_pawprint` / `lis r3, @stringBase0` /
    `addi r31, r3, @stringBase0` from bfbbdecomp_bfbb's
    `SB/Game/zFeet.o :: zFeetGetIDs__Fv`

No toolchain, no clone checkout, no objdiff-cli/m2c binary needed for the
portable tests below -- everything is a plain dict/bytes literal. The
clone-checkout end-to-end check lives in decomp-synth's copy of this file
(`tools/il_witness/test_objdiff_to_m2c.py::test_objdiff_to_m2c_clones_e2e`,
`@pytest.mark.integration`) and is the ONE block dropped from this vendored
copy -- it drives a built mwcc/ELF clone checkout that this repo does not
have, and would only ever skip here.

VENDORED COPY. Canonical home is decomp-synth
`tools/il_witness/test_objdiff_to_m2c.py`; fix there first, then re-vendor
alongside `tools/objdiff_to_m2c.py`. The mwcc/SDA cases below are retained
deliberately even though this repo is MSVC/COFF-only: they are the overshoot
guard on shared code paths (`format_instruction`, `_is_reloc_symbol`,
`quote_symbol`), so a local edit that breaks them breaks this repo too.
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))
import objdiff_to_m2c as m  # noqa: E402


def _instr(opcode: str, args: str) -> dict:
    """Build a minimal objdiff-JSON instruction dict for `format_instruction`."""
    return {"target": {"opcode": opcode, "args": args}}


# ---------------------------------------------------------------------------
# the bug: bare SDA-relative memory ops (base register omitted by objdiff)
# ---------------------------------------------------------------------------

def test_bare_pool_ref_memory_op_gets_base_register():
    """`lfd f0, @158` (real melee `__num2dec` output) must gain an explicit
    base register and m2c-valid `@sda21` syntax -- previously passed through
    unchanged (`lfd f0, @158`), which m2c rejects for lacking `(register)`.
    """
    out = m.format_instruction(_instr("lfd", "f0, @158"))
    assert out == 'lfd f0, "@158"@sda21(r2)'


def test_bare_pool_ref_store_double_also_fixed():
    out = m.format_instruction(_instr("stfd", "f31, @212"))
    assert out == 'stfd f31, "@212"@sda21(r2)'


def test_bare_named_sda_global_memory_op_gets_base_register():
    """`stw r3, paremit_sd_pawprint` (real bfbb `zFeetGetIDs__Fv` output) --
    a bare *named* SDA global, not a `@NNN` pool id, hits the same omitted-
    base-register shape and needs the same fix. No quoting needed since the
    symbol itself has no special characters.
    """
    out = m.format_instruction(_instr("stw", "r3, paremit_sd_pawprint"))
    assert out == "stw r3, paremit_sd_pawprint@sda21(r13)"


def test_bare_sda_global_integer_load_uses_r13_not_r2():
    out = m.format_instruction(_instr("lwz", "r0, sSkidMarkRaster"))
    assert out == "lwz r0, sSkidMarkRaster@sda21(r13)"


def test_float_ops_use_r2_integer_ops_use_r13():
    # Real ABI convention: r2 backs the read-only .sdata2/.sbss2 area (float
    # constant pools), r13 backs read-write .sdata/.sbss -- m2c itself
    # discards the register (see the docstring in format_instruction), this
    # only matters for human-readability/fidelity to the real toolchain.
    for op in ("lfd", "lfs", "stfd", "stfs"):
        assert "(r2)" in m.format_instruction(_instr(op, "f0, @1"))
    for op in ("lwz", "stw", "lbz", "stb", "lhz", "sth", "lha"):
        assert "(r13)" in m.format_instruction(_instr(op, "r0, @1"))


def test_bare_memory_op_numeric_offset_falls_through_unchanged():
    """Fail-closed guard: if the would-be-symbol operand actually looks
    numeric (a shape we don't understand for a 2-part memory op), emit it
    unchanged rather than guessing -- m2c will reject it, which is the
    correct behavior for an unrecognized shape (refuse the row, don't
    silently emit something semantically wrong).
    """
    out = m.format_instruction(_instr("lwz", "r3, 0x10"))
    assert out == "lwz r3, 0x10"


# ---------------------------------------------------------------------------
# already-working paths (regression guards -- must not break)
# ---------------------------------------------------------------------------

def test_stringbase_lis_already_worked_before_this_fix():
    """`lis r3, @stringBase0` (real bfbb output) already converted correctly
    via the lis-specific branch before this fix; must keep working.
    """
    out = m.format_instruction(_instr("lis", "r3, @stringBase0"))
    assert out == 'lis r3, "@stringBase0"@ha'


def test_stringbase_addi_already_worked_before_this_fix():
    out = m.format_instruction(_instr("addi", "r31, r3, @stringBase0"))
    assert out == 'addi r31, r3, "@stringBase0"@l'


def test_stringbase_mr_annotation_already_stripped():
    out = m.format_instruction(_instr("mr", "r4, r31, @stringBase0"))
    assert out == "mr r4, r31"


def test_three_part_memory_op_with_symbol_offset_unaffected():
    # e.g. "lwz r4, ?gNullStr@@3PBDB, r11" -> quoted symbol + @l(reg) -- the
    # pre-existing MSVC/COFF path, must not regress.
    out = m.format_instruction(_instr("lwz", "r4, ?gNullStr@@3PBDB, r11"))
    assert out == 'lwz r4, "?gNullStr@@3PBDB"@l(r11)'


def test_four_part_memory_op_strips_trailing_reloc_annotation():
    out = m.format_instruction(_instr("stw", "r3, 0x0, r30, sSurfaceSoundIDStep"))
    assert out == "stw r3, 0x0(r30)"


# ---------------------------------------------------------------------------
# _is_reloc_symbol / quote_symbol widening
# ---------------------------------------------------------------------------

def test_is_reloc_symbol_recognizes_bare_pool_refs():
    assert m._is_reloc_symbol("@158")
    assert m._is_reloc_symbol("@stringBase0")
    # pre-existing recognized shapes must still be recognized
    assert m._is_reloc_symbol("?TheDebug@@3VDebug@@A")
    assert m._is_reloc_symbol('"?TheDebug@@3VDebug@@A"')
    assert not m._is_reloc_symbol("r3")
    assert not m._is_reloc_symbol("0x10")


def test_general_fallback_strips_bare_pool_ref_on_other_opcodes():
    """Any opcode not given a specific branch above still benefits from the
    widened `_is_reloc_symbol`: a trailing bare `@NNN` decoration gets
    stripped by the generic last-resort handler.
    """
    out = m.format_instruction(_instr("add", "r3, r4, @158"))
    assert out == "add r3, r4"


def test_quote_symbol_quotes_bare_at_prefixed_names():
    assert m.quote_symbol("@158") == '"@158"'
    assert m.quote_symbol("@stringBase0") == '"@stringBase0"'
    # a plain named SDA global has no special chars -- must NOT be quoted
    assert m.quote_symbol("paremit_sd_pawprint") == "paremit_sd_pawprint"


# ---------------------------------------------------------------------------
# full convert_objdiff_json -- real captured melee/bfbb snippets
# ---------------------------------------------------------------------------

def test_convert_full_melee_snippet_has_no_unresolved_pool_ref():
    """A trimmed real excerpt of doldecomp_melee `__num2dec` around its two
    `lfd f0, @158` sites (objdiff-cli 4.2.3 JSON shape). The previous
    passthrough bug left `@158` bare in the output with no addressing mode;
    the fix must leave no such unresolved bare-`@`-with-no-parens token.
    """
    data = {
        "symbol": "__num2dec",
        "instructions": [
            _instr("stb", "r3, 0x4, r30"),
            _instr("lfd", "f0, @158"),
            _instr("fcmpu", "cr0, f0, f31"),
            _instr("bne", "0x68"),
        ],
    }
    out = m.convert_objdiff_json(data)
    assert '"@158"@sda21(r2)' in out
    # no bare, un-parenthesized `@158` should remain
    import re
    assert not re.search(r'@158(?!"@sda21)', out.replace('"@158"@sda21(r2)', ''))


def test_convert_full_bfbb_snippet_all_sda_forms_resolved():
    """Trimmed real excerpt of bfbbdecomp_bfbb `zFeetGetIDs__Fv`, exercising
    all three real spellings found: bare named SDA global on a memory op,
    `@stringBaseN` on lis/addi (already-working), and the same string-pool
    symbol reused across two call sites.
    """
    data = {
        "symbol": "zFeetGetIDs__Fv",
        "instructions": [
            _instr("lis", "r3, @stringBase0"),
            _instr("addi", "r31, r3, @stringBase0"),
            _instr("bl", "zSceneFindObject__FUi"),
            _instr("stw", "r3, paremit_sd_pawprint"),
            _instr("lwz", "r0, sSkidMarkRaster"),
        ],
    }
    out = m.convert_objdiff_json(data)
    assert 'lis r3, "@stringBase0"@ha' in out
    assert 'addi r31, r3, "@stringBase0"@l' in out
    assert "stw r3, paremit_sd_pawprint@sda21(r13)" in out
    assert "lwz r0, sSkidMarkRaster@sda21(r13)" in out


# ---------------------------------------------------------------------------
# jump-table reader: documented COFF-only gap degrades safely on ELF
# ---------------------------------------------------------------------------

def _synthetic_elf32be_ppc_header() -> bytes:
    """A minimal, syntactically-valid ELF32-BE/PPC file header (52-byte
    e_ident+header, no sections) -- enough to prove
    `read_jump_table_from_obj` (a COFF-specific byte-layout reader) degrades
    to `None` on ELF input rather than raising or hanging, without needing a
    real clone-repo `.o` on disk.
    """
    e_ident = b"\x7fELF" + bytes([1, 2, 1]) + b"\x00" * 9  # ELFCLASS32, ELFDATA2MSB
    assert len(e_ident) == 16
    rest = struct.pack(
        ">HHIIIIIHHHHHH",
        2,          # e_type: ET_EXEC (arbitrary)
        20,         # e_machine: EM_PPC
        1,          # e_version
        0, 0, 0,    # e_entry, e_phoff, e_shoff
        0,          # e_flags
        52,         # e_ehsize
        0, 0,       # e_phentsize, e_phnum
        0, 0, 0,    # e_shentsize, e_shnum, e_shstrndx
    )
    return e_ident + rest


def test_jump_table_reader_degrades_safely_on_elf_input(tmp_path):
    obj = tmp_path / "fake.o"
    obj.write_bytes(_synthetic_elf32be_ppc_header())
    # Must return None (treated as "unreadable", the jump table is skipped
    # and its case labels stay unresolved) -- never raise, never hang.
    result = m.read_jump_table_from_obj(str(obj), "jumptable_800F0000", 5, 4)
    assert result is None


# ---------------------------------------------------------------------------
# ruler independence: objdiff-cli fdc5113 respelled the flat `args` string
# ---------------------------------------------------------------------------
#
# fdc5113 ("ruler I", 2026-08-16) started building the JSON `args` string from
# the DISPLAY parts rather than the comparison arg list, so `lwz r0, 0x0, r5`
# became `lwz r0, 0x0(r5)` and a bare `sym` became `sym@h`. Everything in this
# file was written against the old spelling, and the old rules did not error on
# the new one -- they silently produced corrupt m2c input.
#
# The repair is `flat_args_from_typed`: rebuild the flat spelling from
# `typed_args`, which is structured and did NOT move. These tests pin the
# property that matters -- output is the same whichever objdiff wrote the JSON
# -- rather than pinning either spelling.


def _instr_typed(opcode: str, args: str, typed: list) -> dict:
    return {"target": {"opcode": opcode, "args": args, "typed_args": typed}}


def _reg(v):
    return {"type": "Register", "value": v}


def _sym(v):
    return {"type": "Symbol", "value": v}


def _signed(v):
    return {"type": "Signed", "value": v}


# (label, new-format args, typed_args, old-format args the converter must see)
_RESPELLINGS = [
    ("d-form positive offset", "r0, 0x0(r5)",
     [_reg("r0"), _signed(0), _reg("r5")], "r0, 0x0, r5"),
    ("d-form negative offset", "r1, -0x120(r1)",
     [_reg("r1"), _signed(-288), _reg("r1")], "r1, -0x120, r1"),
    ("COFF REFHI suffix", "r11, ?TheDebug@@3VDebug@@A@h",
     [_reg("r11"), _sym("?TheDebug@@3VDebug@@A")], "r11, ?TheDebug@@3VDebug@@A"),
    ("COFF REFLO in a d-form", "r11, ?sTopSaveDir@@0PAVObjectDir@@A@l(r22)",
     [_reg("r11"), _sym("?sTopSaveDir@@0PAVObjectDir@@A"), _reg("r22")],
     "r11, ?sTopSaveDir@@0PAVObjectDir@@A, r22"),
    ("ELF ADDR16_HA suffix", "r3, @stringBase0@ha",
     [_reg("r3"), _sym("@stringBase0")], "r3, @stringBase0"),
    ("ELF SDA21 suffix", "r3, paremit_sd_pawprint@sda21",
     [_reg("r3"), _sym("paremit_sd_pawprint")], "r3, paremit_sd_pawprint"),
    ("fake-pool bracket form", "r5, r6",
     [_reg("r5"), _reg("r6"), _sym("sDevices__6UsbWii")],
     "r5, r6, sDevices__6UsbWii"),
]


@pytest.mark.parametrize("label,new_args,typed,old_args",
                         _RESPELLINGS, ids=[r[0] for r in _RESPELLINGS])
def test_flat_args_rebuilt_from_typed_args(label, new_args, typed, old_args):
    """The rebuild reproduces the pre-fdc5113 spelling exactly."""
    assert m.flat_args_from_typed(typed) == old_args


@pytest.mark.parametrize("label,new_args,typed,old_args",
                         _RESPELLINGS, ids=[r[0] for r in _RESPELLINGS])
def test_normalize_makes_converter_ruler_independent(label, new_args, typed, old_args):
    """Same instruction, both rulers, one output.

    This is the whole point: an m2c seed that changes because objdiff was
    rebuilt is a silent change to the proposer's input distribution.
    """
    new_side = [_instr_typed("lwz", new_args, typed)]
    old_side = [_instr_typed("lwz", old_args, typed)]
    m.normalize_instruction_args(new_side)
    m.normalize_instruction_args(old_side)
    assert new_side[0]["target"]["args"] == old_side[0]["target"]["args"]


def test_normalize_is_a_noop_on_pre_fdc5113_json():
    """Old JSON must pass through untouched -- the count reports 0 rewrites."""
    instrs = [_instr_typed("lwz", "r0, 0x0, r5",
                           [_reg("r0"), _signed(0), _reg("r5")])]
    assert m.normalize_instruction_args(instrs) == 0
    assert instrs[0]["target"]["args"] == "r0, 0x0, r5"


def test_normalize_leaves_rows_without_typed_args_alone():
    """`bctrl` and friends carry no args at all; do not invent any."""
    instrs = [{"target": {"opcode": "bctrl", "args": None}}]
    assert m.normalize_instruction_args(instrs) == 0
    assert instrs[0]["target"]["args"] is None


def test_normalize_rewrites_the_base_side_too():
    """Both sides feed the converter; normalizing only `target` would leave a
    half-converted row for any consumer that reads `base`."""
    typed = [_reg("r1"), _signed(-16), _reg("r1")]
    instrs = [{
        "target": {"opcode": "stwu", "args": "r1, -0x10(r1)", "typed_args": typed},
        "base": {"opcode": "stwu", "args": "r1, -0x10(r1)", "typed_args": typed},
    }]
    assert m.normalize_instruction_args(instrs) == 2
    assert instrs[0]["base"]["args"] == "r1, -0x10, r1"


def test_the_three_corruptions_fdc5113_caused_are_gone():
    """Regression pins on the exact garbage measured on real objects.

    Before the repair, on ruler I: a stack-frame store came out as
    `stwu r1, -0x120(r1)@sda21(r13)`, a `lis` swallowed the relocation suffix
    into the quoted symbol as `lis r11, "sym@h"@ha`, and a symbolic load became
    `lwz r4, "sym@l(r11)"@sda21(r13)`.

    The stack-frame store's expected value moved once more, in the follow-on
    negative-hex repair below: the ruler-I fix left it at `-0x120@l(r1)` (the
    displacement still on the symbol branch, a pre-existing miss deliberately
    kept so the ruler-I differential stayed a clean no-op), and it is now the
    correct `-0x120(r1)`.
    """
    instrs = [
        _instr_typed("stwu", "r1, -0x120(r1)",
                     [_reg("r1"), _signed(-288), _reg("r1")]),
        _instr_typed("lis", "r11, ?TheDebug@@3VDebug@@A@h",
                     [_reg("r11"), _sym("?TheDebug@@3VDebug@@A")]),
        _instr_typed("lwz", "r4, ?gNullStr@@3PBDB@l(r11)",
                     [_reg("r4"), _sym("?gNullStr@@3PBDB"), _reg("r11")]),
    ]
    m.normalize_instruction_args(instrs)
    out = [m.format_instruction(i) for i in instrs]

    assert out[0] == "stwu r1, -0x120(r1)"
    assert out[1] == 'lis r11, "?TheDebug@@3VDebug@@A"@ha'
    assert out[2] == 'lwz r4, "?gNullStr@@3PBDB"@l(r11)'
    for line in out:
        assert "@h\"" not in line and "@l\"" not in line, f"suffix inside symbol: {line}"
        assert line.count("@sda21") == 0, f"spurious sda21: {line}"


def test_jump_table_symbol_survives_the_respelling():
    """`detect_jump_tables` matches `lis`/`jumptable_` on the raw string; under
    the new spelling the captured name carried a trailing `@ha` and the later
    object-file lookup missed, silently dropping the jump table."""
    instrs = [_instr_typed("lis", "r11, jumptable_800a1234@ha",
                           [_reg("r11"), _sym("jumptable_800a1234")])]
    m.normalize_instruction_args(instrs)
    assert instrs[0]["target"]["args"] == "r11, jumptable_800a1234"


# ---------------------------------------------------------------------------
# negative-hex operands are numbers, not symbols
# ---------------------------------------------------------------------------
#
# The symbol-vs-number guard used to read
# `not s.startswith('0x') and not s.lstrip('-').isdigit()`, which is blind to
# negative hex: `'-0x120'.lstrip('-')` is `'0x120'` (not `isdigit()`) and the
# `0x` prefix test fails on the leading `-`. Every negative hex operand took
# the symbol branch and came out with a relocation macro glued to a literal.
# POSITIVE hex was never affected (`startswith('0x')` caught it) and neither
# was decimal of either sign -- negative hex is the whole of the miss, but it
# hits all four guard sites, not just the d-form.


_NEGATIVE_HEX_SITES = [
    # (label, opcode, args, was -- pre-fix output, now -- post-fix output)
    ("d-form displacement (stack frame)", "stwu", "r1, -0x120, r1",
     "stwu r1, -0x120@l(r1)", "stwu r1, -0x120(r1)"),
    ("d-form displacement (load)", "lwz", "r3, -0x8, r31",
     "lwz r3, -0x8@l(r31)", "lwz r3, -0x8(r31)"),
    ("addi immediate", "addi", "r1, r1, -0x120",
     "addi r1, r1, -0x120@l", "addi r1, r1, -0x120"),
    ("subi immediate", "subi", "r3, r4, -0x1",
     "subi r3, r4, -0x1@l", "subi r3, r4, -0x1"),
    ("lis immediate", "lis", "r11, -0x1",
     "lis r11, -0x1@ha", "lis r11, -0x1"),
    ("two-part memory op", "lfd", "f0, -0x8",
     "lfd f0, -0x8@sda21(r2)", "lfd f0, -0x8"),
]


@pytest.mark.parametrize("label,opcode,args,was,now", _NEGATIVE_HEX_SITES,
                         ids=[r[0] for r in _NEGATIVE_HEX_SITES])
def test_negative_hex_operand_is_a_number_at_every_guard_site(
        label, opcode, args, was, now):
    """A negative hex immediate/displacement must never grow a `@ha`/`@l`/
    `@sda21` macro -- those name a relocated symbol, and there is no symbol
    here. `stwu r1, -0x120@l(r1)` is not assembly m2c can read."""
    out = m.format_instruction(_instr(opcode, args))
    assert out == now, f"{label}: expected {now!r}, got {out!r} (was {was!r})"
    assert "@" not in out


_POSITIVE_HEX_SITES = [
    ("d-form displacement", "stwu", "r1, 0x120, r1", "stwu r1, 0x120(r1)"),
    ("addi immediate", "addi", "r1, r1, 0x120", "addi r1, r1, 0x120"),
    ("lis immediate", "lis", "r11, 0x8020", "lis r11, 0x8020"),
    ("negative decimal d-form", "lwz", "r4, -120, r11", "lwz r4, -120(r11)"),
    ("positive decimal d-form", "lwz", "r4, 120, r11", "lwz r4, 120(r11)"),
]


@pytest.mark.parametrize("label,opcode,args,expected", _POSITIVE_HEX_SITES,
                         ids=[r[0] for r in _POSITIVE_HEX_SITES])
def test_positive_hex_and_decimal_were_never_affected(label, opcode, args, expected):
    """Scope pin: the old guard already handled these (via `startswith('0x')`
    and `isdigit()`), so the fix must leave them exactly where they were. If
    this ever goes red the blast-radius statement in the commit is wrong."""
    assert m.format_instruction(_instr(opcode, args)) == expected


def test_symbol_operands_still_take_the_symbol_branch():
    """The other half of the guard: real relocated symbols must KEEP their
    macro. Widening the numeric test must not swallow any of these."""
    assert (m.format_instruction(_instr("lwz", "r4, ?gNullStr@@3PBDB, r11"))
            == 'lwz r4, "?gNullStr@@3PBDB"@l(r11)')
    assert (m.format_instruction(_instr("addi", "r31, r3, @stringBase0"))
            == 'addi r31, r3, "@stringBase0"@l')
    assert (m.format_instruction(_instr("lis", "r3, @stringBase0"))
            == 'lis r3, "@stringBase0"@ha')
    assert (m.format_instruction(_instr("stw", "r3, paremit_sd_pawprint"))
            == "stw r3, paremit_sd_pawprint@sda21(r13)")
    assert (m.format_instruction(_instr("lfd", "f0, @158"))
            == 'lfd f0, "@158"@sda21(r2)')


_NUMERIC_OPERANDS = [
    # numbers
    ("-0x120", True), ("0x120", True), ("-0X1F", True), ("0X1f", True),
    ("120", True), ("-120", True), ("0", True), ("0120", True),
    # not numbers -- symbols, macros, malformed, or already-composed operands
    ("sym", False), ("@158", False), ("@stringBase0", False),
    ("?gNullStr@@3PBDB", False), ("0x", False), ("0xfoo", False),
    ("0x120(r3)", False), ("-", False), ("", False), ("r3", False),
]


@pytest.mark.parametrize("operand,expected", _NUMERIC_OPERANDS,
                         ids=[o[0] or "empty" for o in _NUMERIC_OPERANDS])
def test_is_numeric_operand_truth_table(operand, expected):
    assert m._is_numeric_operand(operand) is expected


def test_negative_hex_survives_the_ruler_i_respelling_end_to_end():
    """The real path: ruler-I display spelling -> `normalize_instruction_args`
    -> `format_instruction`. This is how a stack-frame store actually arrives
    from objdiff, and the only thing the seed may contain is `-0x120(r1)`."""
    instrs = [_instr_typed("stwu", "r1, -0x120(r1)",
                           [_reg("r1"), _signed(-288), _reg("r1")])]
    m.normalize_instruction_args(instrs)
    assert instrs[0]["target"]["args"] == "r1, -0x120, r1"
    assert m.format_instruction(instrs[0]) == "stwu r1, -0x120(r1)"


# ---------------------------------------------------------------------------
# the branch-label rewrite must fire ONLY on a branch (`_is_branch_opcode`)
#
# `convert_objdiff_json` respells a branch's hex destination as a `.L_` label.
# Until this suite existed that respelling ran on EVERY formatted instruction
# with no opcode test, so any non-branch whose trailing immediate happened to
# equal a branch destination in the same function was corrupted into assembly
# m2c cannot mean -- `addi r3, r11, 0x34` -> `addi r3, r11, .L_00000034`.
#
# Measured on rb3-xenon build/45410914, all 1948 functions in [90,100):
# 453 corrupted lines across 147 functions (addi 293, li 43, cmplwi 42,
# subi 37, ori 18, cmpwi 8, oris 6, mulli 4, lis 1, subfic 1) against 6931
# legitimate branch respellings across 475 functions; gating drops 453 and
# loses 0. It never surfaced as an error: m2c ACCEPTS the corrupt input
# (rc=0 on all 147, before and after) and silently mis-decompiles it --
# `&unksp-B0 + (s32) &.L_00000058` for a stack offset, or an
# `M2C_ERROR(/* unknown instruction: subi $r31, $r1, .L_000000C0 */)` that
# poisons every later use of that register. 141 of the 147 produce different
# C; the `.L_`-in-an-expression signature went from 142 functions to 8, and
# `M2C_ERROR` from 54 to 21.
#
# Every `(address, opcode, args)` row below is VERBATIM objdiff-cli 4.2.3
# `diff --include-instructions` output against rb3-xenon build/45410914 --
# whole functions except the BandCrowdMeter one, which is a contiguous
# verbatim window (0x400..0x454) of a 347-instruction function.
# ---------------------------------------------------------------------------

def _fn(symbol: str, rows) -> dict:
    """An objdiff-JSON function body from verbatim (address, opcode, args) rows."""
    return {
        "symbol": symbol,
        "instructions": [
            {"target": ({"address": a, "opcode": o} if g is None
                        else {"address": a, "opcode": o, "args": g})}
            for a, o, g in rows
        ],
    }


# `beq 0x34` mints `.L_00000034`; the `addi` at 0x2c carries 0x34 as a plain
# immediate and was corrupted. Note `stw r12, -0x8(r1)` at 0x8: the frame
# setup's negative displacement sits inside `(r1)` and can never reach a
# `$`-anchored regex -- which is why the negative-immediate half of this line
# was never the bug.
FN_823C605C = _fn("fn_823C605C", [
    ('0x0', 'subi', 'r31, r12, 0x80'),
    ('0x4', 'mflr', 'r12'),
    ('0x8', 'stw', 'r12, -0x8(r1)'),
    ('0xc', 'stwu', 'r1, -0x60(r1)'),
    ('0x10', 'lwz', 'r11, 0x50(r31)'),
    ('0x14', 'clrlwi.', 'r11, r11, 31'),
    ('0x18', 'beq', '0x34'),
    ('0x1c', 'lwz', 'r11, 0x50(r31)'),
    ('0x20', 'clrrwi', 'r11, r11, 1'),
    ('0x24', 'stw', 'r11, 0x50(r31)'),
    ('0x28', 'lwz', 'r11, 0x94(r31)'),
    ('0x2c', 'addi', 'r3, r11, 0x34'),
    ('0x30', 'bl', '??1Object@Hmx@@UAA@XZ'),
    ('0x34', 'addi', 'r1, r1, 0x60'),
    ('0x38', 'lwz', 'r12, -0x8(r1)'),
    ('0x3c', 'mtlr', 'r12'),
    ('0x40', 'blr', None),
])

# `beq 0x38` mints `.L_00000038`; BOTH the `subi` at 0xc and the `addi` at
# 0x14 carry 0x38 as an immediate (a `this`-adjustment thunk pair) and were
# corrupted -- one function, two opcodes, two mangled lines.
FN_FLOWTRIGGER_DTOR = _fn("??_GFlowTrigger@@UAAPAXI@Z", [
    ('0x0', 'mflr', 'r12'),
    ('0x4', 'bl', '__savegprlr_29'),
    ('0x8', 'stwu', 'r1, -0x70(r1)'),
    ('0xc', 'subi', 'r30, r3, 0x38'),
    ('0x10', 'mr', 'r29, r4'),
    ('0x14', 'addi', 'r31, r30, 0x38'),
    ('0x18', 'mr', 'r3, r31'),
    ('0x1c', 'bl', 'fn_823EB610'),
    ('0x20', 'mr', 'r3, r31'),
    ('0x24', 'bl', '??1Object@Hmx@@UAA@XZ'),
    ('0x28', 'clrlwi.', 'r11, r29, 31'),
    ('0x2c', 'beq', '0x38'),
    ('0x30', 'mr', 'r3, r30'),
    ('0x34', 'bl', '??3BinStream@@SAXPAX@Z'),
    ('0x38', 'mr', 'r3, r30'),
    ('0x3c', 'addi', 'r1, r1, 0x70'),
    ('0x40', 'b', '__restgprlr_29'),
])

# The `b 0x0` at 0x84 and `bdnzf lt, 0x0` at 0x6c are a real loop back to the
# function entry, so 0x0 is a branch target -- and address 0 is the single
# most collision-prone value there is: `li rX, 0x0` and `cmplwi ..., 0x0` are
# everywhere. Three corrupted lines here (li at 0x1c, cmplwi at 0x3c, li at
# 0x74); `li r11, 0x1` / `li r9, 0x1` are untouched because 0x1 is not a
# target. This is why `li` and `cmplwi` rank 2nd and 3rd in the census.
FN_BIT_FILL = _fn(
    "??$fill@U?$_Bit_iter@U_Bit_reference@stlpmtx_std@@PAU12@@stlpmtx_std@@_N"
    "@stlpmtx_std@@YAXU?$_Bit_iter@U_Bit_reference@stlpmtx_std@@PAU12@@0@0AB_N@Z",
    [
        ('0x0', 'lwz', 'r10, 0x0(r3)'),
        ('0x4', 'lwz', 'r11, 0x0(r4)'),
        ('0x8', 'cmplw', 'cr6, r11, r10'),
        ('0xc', 'bne', 'cr6, 0x24'),
        ('0x10', 'lwz', 'r11, 0x4(r4)'),
        ('0x14', 'lwz', 'r9, 0x4(r3)'),
        ('0x18', 'cmplw', 'cr6, r11, r9'),
        ('0x1c', 'li', 'r11, 0x0'),
        ('0x20', 'beq', 'cr6, 0x28'),
        ('0x24', 'li', 'r11, 0x1'),
        ('0x28', 'clrlwi.', 'r11, r11, 24'),
        ('0x2c', 'beqlr', None),
        ('0x30', 'lbz', 'r11, 0x0(r5)'),
        ('0x34', 'li', 'r9, 0x1'),
        ('0x38', 'lwz', 'r8, 0x4(r3)'),
        ('0x3c', 'cmplwi', 'r11, 0x0'),
        ('0x40', 'slw', 'r11, r9, r8'),
        ('0x44', 'lwz', 'r9, 0x0(r10)'),
        ('0x48', 'beq', '0x54'),
        ('0x4c', 'or', 'r11, r9, r11'),
        ('0x50', 'b', '0x58'),
        ('0x54', 'andc', 'r11, r9, r11'),
        ('0x58', 'stw', 'r11, 0x0(r10)'),
        ('0x5c', 'lwz', 'r11, 0x4(r3)'),
        ('0x60', 'addi', 'r10, r11, 0x1'),
        ('0x64', 'cmplwi', 'cr6, r11, 0x1f'),
        ('0x68', 'stw', 'r10, 0x4(r3)'),
        ('0x6c', 'bdnzf', 'lt, 0x0'),
        ('0x70', 'lwz', 'r11, 0x0(r3)'),
        ('0x74', 'li', 'r10, 0x0'),
        ('0x78', 'addi', 'r11, r11, 0x4'),
        ('0x7c', 'stw', 'r10, 0x4(r3)'),
        ('0x80', 'stw', 'r11, 0x0(r3)'),
        ('0x84', 'b', '0x0'),
        ('0x88', 'blr', None),
    ],
)

# Contiguous verbatim window 0x400..0x454 of `BandCrowdMeter::Handle`. The
# `b 0x400` at 0x454 mints `.L_00000400`; `ori r11, r11, 0x400` at 0x418 is a
# bit-set immediate that happens to equal it. The window also carries a real
# negative displacement inside parens (`lwz r3, -0x60(r25)`).
FN_BANDCROWDMETER_HANDLE_WINDOW = _fn(
    "?Handle@BandCrowdMeter@@UAA?AVDataNode@@PAVDataArray@@_N@Z", [
        ('0x400', 'bctrl', None),
        ('0x404', 'b', '0xdc'),
        ('0x408', 'lis', 'r10, lbl_82CBCF9C@h'),
        ('0x40c', 'rlwinm.', 'r9, r11, 0, 21, 21'),
        ('0x410', 'addi', 'r28, r10, lbl_82CBCF9C@l'),
        ('0x414', 'bne', '0x434'),
        ('0x418', 'ori', 'r11, r11, 0x400'),
        ('0x41c', 'stw', 'r11, lbl_82CBCFC8@l(r30)'),
        ('0x420', 'lis', 'r11, lbl_8201E094@h'),
        ('0x424', 'mr', 'r3, r28'),
        ('0x428', 'addi', 'r4, r11, lbl_8201E094@l'),
        ('0x42c', 'bl', '??0Symbol@@QAA@PBD@Z'),
        ('0x430', 'lwz', 'r11, lbl_82CBCFC8@l(r30)'),
        ('0x434', 'lwz', 'r10, 0x0(r28)'),
        ('0x438', 'lwz', 'r9, 0x50(r31)'),
        ('0x43c', 'cmplw', 'cr6, r9, r10'),
        ('0x440', 'bne', 'cr6, 0x458'),
        ('0x444', 'lwz', 'r3, -0x60(r25)'),
        ('0x448', 'lwz', 'r11, 0x0(r3)'),
        ('0x44c', 'lwz', 'r11, 0x24(r11)'),
        ('0x450', 'mtctr', 'r11'),
        ('0x454', 'b', '0x400'),
    ])

ALL_FIXTURES = [
    ("fn_823C605C", FN_823C605C),
    ("FlowTrigger_scalar_deleting_dtor", FN_FLOWTRIGGER_DTOR),
    ("stlpmtx_std_fill_Bit_iter", FN_BIT_FILL),
    ("BandCrowdMeter_Handle_window", FN_BANDCROWDMETER_HANDLE_WINDOW),
]


def _body(out: str):
    """The emitted instruction lines (tab-indented), minus label definitions."""
    return [l[1:] for l in out.split('\n') if l.startswith('\t')]


# --- one case per corrupted opcode, on the real function it came from ------

@pytest.mark.parametrize("opcode,line", [
    ("addi", "addi r3, r11, 0x34"),
])
def test_addi_immediate_colliding_with_a_branch_target_survives(opcode, line):
    out = m.convert_objdiff_json(FN_823C605C)
    assert line in _body(out)
    assert "addi r3, r11, .L_00000034" not in _body(out)


@pytest.mark.parametrize("line", [
    "subi r30, r3, 0x38",
    "addi r31, r30, 0x38",
])
def test_subi_and_addi_immediates_colliding_with_a_target_survive(line):
    out = m.convert_objdiff_json(FN_FLOWTRIGGER_DTOR)
    assert line in _body(out)
    assert ".L_00000038" not in line
    assert not [b for b in _body(out)
                if b.startswith(("subi ", "addi ")) and ".L_" in b]


@pytest.mark.parametrize("line", [
    "li r11, 0x0",
    "li r10, 0x0",
    "cmplwi r11, 0x0",
])
def test_li_and_cmplwi_immediates_colliding_with_entry_address_survive(line):
    """Address 0x0 is the worst collision: a loop back to the function entry
    makes `.L_00000000` a real label, and every `li rX, 0x0` in the function
    then matched the ungated regex."""
    out = m.convert_objdiff_json(FN_BIT_FILL)
    assert line in _body(out)
    assert not [b for b in _body(out)
                if b.startswith(("li ", "cmplwi ")) and ".L_" in b]


def test_ori_bitmask_immediate_colliding_with_a_target_survives():
    out = m.convert_objdiff_json(FN_BANDCROWDMETER_HANDLE_WINDOW)
    assert "ori r11, r11, 0x400" in _body(out)
    assert "ori r11, r11, .L_00000400" not in _body(out)


# --- the other direction: legitimate branch rewrites must still happen -----

@pytest.mark.parametrize("name,data,expected", [
    ("fn_823C605C", FN_823C605C, ["beq .L_00000034"]),
    ("FlowTrigger", FN_FLOWTRIGGER_DTOR, ["beq .L_00000038"]),
    ("bit_fill", FN_BIT_FILL, ["b .L_00000000", "bdnzf lt, .L_00000000",
                               "bne cr6, .L_00000024", "beq .L_00000054"]),
    ("BandCrowdMeter", FN_BANDCROWDMETER_HANDLE_WINDOW,
     ["b .L_00000400", "bne .L_00000434"]),
])
def test_legitimate_branch_rewrites_still_happen(name, data, expected):
    body = _body(m.convert_objdiff_json(data))
    for line in expected:
        assert line in body, f"{name}: lost the branch rewrite {line!r}"


@pytest.mark.parametrize("name,data", ALL_FIXTURES)
def test_the_label_definition_is_still_emitted(name, data):
    """Every respelled destination that lies inside the fixture must also have
    its `.L_xxxxxxxx:` definition emitted. Scoped to addresses present because
    the BandCrowdMeter fixture is a WINDOW of a larger function and two of its
    branches legitimately leave it (0xdc, 0x458)."""
    out = m.convert_objdiff_json(data)
    present = {int(i["target"]["address"], 16) for i in data["instructions"]}
    refs = {l.split('.L_')[1][:8] for l in _body(out) if '.L_' in l}
    refs = {r for r in refs if int(r, 16) in present}
    defs = {l[3:11] for l in out.split('\n') if l.startswith('.L_')}
    assert refs, f"{name}: no branch was respelled at all"
    assert refs <= defs, f"{name}: dangling label refs {refs - defs}"


@pytest.mark.parametrize("name,data", ALL_FIXTURES)
def test_only_branches_are_ever_relabelled(name, data):
    """The invariant, stated once: a `.L_` reference may appear only on a line
    whose opcode `_is_branch_opcode` accepts."""
    for line in _body(m.convert_objdiff_json(data)):
        if '.L_' in line:
            assert m._is_branch_opcode(line.split()[0]), \
                f"{name}: non-branch line was relabelled: {line!r}"


@pytest.mark.parametrize("name,data", ALL_FIXTURES)
def test_no_negated_label_is_ever_emitted(name, data):
    assert '-.L_' not in m.convert_objdiff_json(data)


def test_minting_and_consuming_use_the_same_predicate():
    """`parse_branch_targets` mints a label for anything `_is_branch_opcode`
    accepts, including the bare `startswith('b')` arm (`bnl` is not in
    BRANCH_OPCODES). If the emit loop gated on a narrower set instead, the
    label would be defined and never referenced."""
    data = _fn("sym", [
        ('0x0', 'bnl', '0x8'),
        ('0x4', 'li', 'r3, 0x8'),
        ('0x8', 'blr', None),
    ])
    body = _body(m.convert_objdiff_json(data))
    assert "bnl .L_00000008" in body
    assert "li r3, 0x8" in body


@pytest.mark.parametrize("opcode,expected", [
    ('b', True), ('bl', True), ('beq', True), ('bne', True), ('bdnz', True),
    ('bdnzf', True), ('blr', True), ('bctr', True), ('bctrl', True),
    ('bnl', True), ('bcl', True),
    ('addi', False), ('subi', False), ('li', False), ('lis', False),
    ('cmplwi', False), ('cmpwi', False), ('ori', False), ('oris', False),
    ('mulli', False), ('subfic', False), ('lwz', False), ('stw', False),
    ('', False),
])
def test_is_branch_opcode_truth_table(opcode, expected):
    assert m._is_branch_opcode(opcode) is expected


# --- the lookbehind: a hex tail glued to its left neighbour is not an operand

@pytest.mark.parametrize("asm,expected", [
    ("b 0x34", "0x34"),
    ("bne cr6, 0x34", "0x34"),
    ("bdnzf lt, 0x0", "0x0"),
    # glued on the left -- not a whole operand, so not an address
    ("b -0x34", None),
    ("bl lbl_0x34", None),
    ("bl foo0x34", None),
    ("b sym.0x34", None),
    # not at the end at all
    ("stw r12, -0x8(r1)", None),
    ("lwz r11, 0x50(r31)", None),
])
def test_trailing_hex_regex_refuses_a_glued_left_neighbour(asm, expected):
    match = m._TRAILING_HEX_RE.search(asm)
    assert (match.group(1) if match else None) == expected


def test_a_negative_branch_operand_is_not_read_as_an_address():
    """SYNTHETIC, and deliberately so: objdiff spells branch destinations as
    absolute addresses and never emits a negative one. Measured over the same
    1948 rb3-xenon functions, all 85 lines ending in `-0xN` are `li`/`cmpwi`/
    `twi` immediates (81 of them `-0x1`, plus `-0x2`, `-0x5` and two `-0x780`)
    and 0 of the 85 have a branch-target magnitude, so no function emits
    `-.L_`. This guard is prophylactic: if that ever changes, the magnitude
    must not be mistaken for the address."""
    data = _fn("sym", [
        ('0x0', 'b', '0x8'),
        ('0x4', 'bdnzf', 'lt, -0x8'),
        ('0x8', 'blr', None),
    ])
    body = _body(m.convert_objdiff_json(data))
    assert "b .L_00000008" in body      # the real one still fires
    assert "bdnzf lt, -0x8" in body     # the negative magnitude does not
    assert "-.L_" not in "\n".join(body)


def test_the_ungated_regex_really_would_have_fired_here():
    """Pins the mutant. The pre-fix rule was a bare `(0x[0-9a-fA-F]+)$` applied
    to every formatted instruction; this asserts it matches the `addi` line
    above, so the opcode gate -- not some incidental spelling -- is what keeps
    that line intact."""
    import re as _re
    assert _re.search(r'(0x[0-9a-fA-F]+)$', 'addi r3, r11, 0x34')
    assert 0x34 in m.parse_branch_targets(FN_823C605C["instructions"])
    assert not m._is_branch_opcode('addi')

