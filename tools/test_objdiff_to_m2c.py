"""tools/objdiff_to_m2c.py — objdiff JSON to GNU-as assembly, for m2c input.

These tests pin one specific defect: telling a numeric literal apart from a symbol
with

    not operand.startswith('0x') and not operand.lstrip('-').isdigit()

That pair misclassifies every NEGATIVE HEX operand as a symbol, because '-0x10'
neither starts with '0x' nor has an lstrip('-') form that isdigit(). The three
rewriters (lis, addi/subi, load/store) then append a relocation suffix and emit
`addi r3, r1, -0x10@l`.

Why it matters more than it looks: m2c does not reject the malformed operand, it
silently reinterprets the stack slot as an incoming stack argument. A leaf
function acquires a phantom parameter and the decompilation is quietly wrong with
no error anywhere. Negative displacements are pervasive on MSVC/Xenon — `stwu
r1, -0x70(r1)` opens essentially every non-leaf prologue and MSVC parks scratch
below SP — so this fired on most real functions.

The fix is _is_numeric_operand. When it was first written (in the cea-decomp copy
of this script) it landed at only ONE of the three call sites, the load/store
rewriter, and looked complete because the tests only exercised loads and stores.
That is why the negative and positive cases below are enumerated per opcode
family rather than spot-checked.

The symbol cases are not decoration. They are the overshoot guard — a predicate
made too permissive would stop tagging genuine relocations, which breaks m2c in
the opposite direction and is far less obvious than a stray @l.
"""

from __future__ import annotations

import pytest

from tools.objdiff_to_m2c import _is_numeric_operand, format_instruction


def fmt(opcode: str, args: str) -> str:
    """Format one instruction the way convert_objdiff_json() would."""
    return format_instruction({"target": {"opcode": opcode, "args": args}})


class TestIsNumericOperand:
    @pytest.mark.parametrize(
        "operand",
        ["0x10", "-0x10", "0X10", "-0X10", "16", "-16", "0", "-0x1", "0x0"],
    )
    def test_numeric_forms(self, operand: str) -> None:
        assert _is_numeric_operand(operand) is True

    @pytest.mark.parametrize(
        "operand",
        [
            "?gNullStr@@3PBDB",
            "?TheDebug@@3VDebug@@A",
            "lbl_82017228",
            "r3",
            "__imp_049E",
            "",
        ],
    )
    def test_symbol_forms(self, operand: str) -> None:
        assert _is_numeric_operand(operand) is False

    def test_negative_hex_is_the_regression(self) -> None:
        """The exact input the old predicate got wrong."""
        assert _is_numeric_operand("-0x10") is True


class TestNegativeHexKeepsNoRelocationSuffix:
    """Numeric displacements must pass through untouched at all three sites."""

    @pytest.mark.parametrize(
        "opcode,args,expected",
        [
            # addi/subi -- taking the address of a stack slot below SP
            ("addi", "r3, r1, -0x10", "addi r3, r1, -0x10"),
            ("subi", "r3, r1, -0x10", "subi r3, r1, -0x10"),
            ("addi", "r3, r1, 0x10", "addi r3, r1, 0x10"),
            ("addi", "r3, r1, -16", "addi r3, r1, -16"),
            # lis -- signed 16-bit immediate, so a negative hex form is legal
            ("lis", "r11, -0x1", "lis r11, -0x1"),
            ("lis", "r11, 0x8200", "lis r11, 0x8200"),
            # load/store -- the site the fix reached first
            ("stwu", "r1, -0x70, r1", "stwu r1, -0x70(r1)"),
            ("std", "r5, -0x10, r1", "std r5, -0x10(r1)"),
            ("lwz", "r4, 0x4c, r3", "lwz r4, 0x4c(r3)"),
        ],
    )
    def test_no_suffix(self, opcode: str, args: str, expected: str) -> None:
        assert fmt(opcode, args) == expected

    @pytest.mark.parametrize(
        "opcode,args",
        [
            ("addi", "r3, r1, -0x10"),
            ("subi", "r3, r1, -0x10"),
            ("lis", "r11, -0x1"),
            ("stwu", "r1, -0x70, r1"),
        ],
    )
    def test_never_emits_a_bare_suffixed_number(self, opcode: str, args: str) -> None:
        """Catches the malformed shapes directly, whatever else changes."""
        out = fmt(opcode, args)
        assert "@l" not in out, out
        assert "@ha" not in out, out


class TestSymbolsStillGetTheirRelocation:
    """The overshoot guard: genuine relocations must keep @l / @ha and quoting."""

    @pytest.mark.parametrize(
        "opcode,args,expected",
        [
            ("addi", "r3, r1, ?gFoo@@3HA", 'addi r3, r1, "?gFoo@@3HA"@l'),
            (
                "lis",
                "r11, ?TheDebug@@3VDebug@@A",
                'lis r11, "?TheDebug@@3VDebug@@A"@ha',
            ),
            (
                "lwz",
                "r4, ?gNullStr@@3PBDB, r11",
                'lwz r4, "?gNullStr@@3PBDB"@l(r11)',
            ),
        ],
    )
    def test_suffix_and_quoting_preserved(
        self, opcode: str, args: str, expected: str
    ) -> None:
        assert fmt(opcode, args) == expected
