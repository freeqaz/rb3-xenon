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
