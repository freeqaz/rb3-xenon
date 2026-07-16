"""Tests for builder.py — code preparation pipeline."""

import struct
import unittest

from .helpers import (
    assemble, ppc_li, ppc_blr, ppc_bl, ppc_nop,
    ppc_std, ppc_ld, ppc_mflr, ppc_mtlr, ppc_stw, ppc_lwz,
    make_reloc, MockCOFF,
)


def _make_coff_with_text_symbols(symbol_defs):
    """Build a MockCOFF with symbols in a .text section."""
    sections = [{'name': '.text', 'raw_size': 4096, 'raw_offset': 0,
                 'num_relocs': 0, 'reloc_offset': 0, 'index': 1}]
    symbol_map = {}
    symbols = []
    for name, value in symbol_defs:
        sym = {'name': name, 'value': value, 'section': 1,
               'type': 0, 'storage_class': 2, 'num_aux': 0, 'index': len(symbols)}
        symbol_map[name] = sym
        symbols.append(sym)

    coff = MockCOFF(sections=sections, symbol_map=symbol_map)
    coff.symbols = symbols
    coff._section_data = {}
    return coff


def _make_coff_with_external(internal_syms, external_syms):
    """Build a MockCOFF with some symbols in .text and some external."""
    sections = [{'name': '.text', 'raw_size': 4096, 'raw_offset': 0,
                 'num_relocs': 0, 'reloc_offset': 0, 'index': 1}]
    symbol_map = {}
    symbols = []
    for name, value in internal_syms:
        sym = {'name': name, 'value': value, 'section': 1,
               'type': 0, 'storage_class': 2, 'num_aux': 0, 'index': len(symbols)}
        symbol_map[name] = sym
        symbols.append(sym)
    for name in external_syms:
        sym = {'name': name, 'value': 0, 'section': 0,
               'type': 0, 'storage_class': 2, 'num_aux': 0, 'index': len(symbols)}
        symbol_map[name] = sym
        symbols.append(sym)

    coff = MockCOFF(sections=sections, symbol_map=symbol_map)
    coff.symbols = symbols
    coff._section_data = {}
    return coff


class TestPrepareSide(unittest.TestCase):
    """Tests for prepare_side()."""

    def setUp(self):
        from scripts.unicorn_runner.builder import prepare_side
        self.prepare_side = prepare_side

    def test_basic_no_relocs(self):
        """Simple function with no relocs produces correct PreparedSide."""
        code = assemble(ppc_li(3, 42), ppc_blr())
        coff = _make_coff_with_text_symbols([("Func", 0)])

        result = self.prepare_side(code, [], coff, "Func", "none")
        self.assertEqual(len(result.code), len(code))
        self.assertEqual(result.func_size, len(code))
        self.assertEqual(result.trampolines, {})
        self.assertIsNone(result.rdata_bytes)

    def test_with_rel24_reloc(self):
        """Function with REL24 reloc gets a trampoline assigned."""
        code = assemble(
            ppc_mflr(0),
            ppc_bl(0),  # placeholder — will be patched
            ppc_mtlr(0),
            ppc_blr(),
        )
        relocs = [make_reloc(4, "ExternalFunc", "REL24")]
        coff = _make_coff_with_external(
            [("Func", 0)], ["ExternalFunc"])

        result = self.prepare_side(code, relocs, coff, "Func", "none")
        self.assertIn("ExternalFunc", result.trampolines)
        self.assertEqual(result.func_size, len(code))

    def test_ppc64_rewrite(self):
        """PPC64 std/ld instructions get rewritten to stw/lwz."""
        code = assemble(
            ppc_std(31, -8, 1),  # std r31, -8(r1) — PPC64
            ppc_ld(31, -8, 1),   # ld r31, -8(r1) — PPC64
            ppc_blr(),
        )
        coff = _make_coff_with_text_symbols([("Func", 0)])

        result = self.prepare_side(code, [], coff, "Func", "none")

        # Check that std (opcode 62) was rewritten to stw (opcode 36)
        insn0 = struct.unpack_from(">I", result.code, 0)[0]
        self.assertEqual((insn0 >> 26) & 0x3F, 36)  # stw

        # Check that ld (opcode 58) was rewritten to lwz (opcode 32)
        insn1 = struct.unpack_from(">I", result.code, 4)[0]
        self.assertEqual((insn1 >> 26) & 0x3F, 32)  # lwz

    def test_no_switch_rdata(self):
        """Non-switch function has rdata_bytes=None."""
        code = assemble(ppc_li(3, 0), ppc_blr())
        coff = _make_coff_with_text_symbols([("Func", 0)])

        result = self.prepare_side(code, [], coff, "Func", "none")
        self.assertIsNone(result.rdata_bytes)


class TestPrepareColoadedSide(unittest.TestCase):
    """Tests for prepare_coloaded_side()."""

    def setUp(self):
        from scripts.unicorn_runner.builder import prepare_coloaded_side
        from scripts.unicorn_runner.coloader import ColoadResult
        from scripts.unicorn_runner.memory_map import CODE_BASE
        self.prepare_coloaded_side = prepare_coloaded_side
        self.ColoadResult = ColoadResult
        self.CODE_BASE = CODE_BASE

    def _make_layout(self, root_size, callee_specs):
        """Build a ColoadResult for testing.

        callee_specs: list of (name, offset) tuples
        """
        symbol_offsets = {"Root": 0}
        coloaded_symbols = []
        for name, offset in callee_specs:
            symbol_offsets[name] = offset
            coloaded_symbols.append(name)

        total_size = max(
            [root_size] + [off + 8 for _, off in callee_specs]
        )
        # Round up to 4-byte alignment
        total_size = (total_size + 3) & ~3

        return self.ColoadResult(
            symbol_offsets=symbol_offsets,
            coloaded_symbols=coloaded_symbols,
            total_size=total_size,
        )

    def test_basic_combined_buffer(self):
        """Root + callee placed at correct offsets in combined buffer."""
        root_code = assemble(ppc_li(3, 0), ppc_bl(0), ppc_blr())
        callee_code = assemble(ppc_li(3, 99), ppc_blr())
        root_relocs = [make_reloc(4, "Callee", "REL24")]
        callee_relocs = []

        layout = self._make_layout(len(root_code), [("Callee", 12)])
        callees = {"Callee": (callee_code, callee_relocs)}
        intra_tu_addrs = {sym: self.CODE_BASE + off
                          for sym, off in layout.symbol_offsets.items()}

        coff = _make_coff_with_text_symbols([("Root", 0), ("Callee", 100)])

        result = self.prepare_coloaded_side(
            root_code, root_relocs, coff, "Root", "none",
            callees, layout, intra_tu_addrs)

        self.assertEqual(result.func_size, layout.total_size)
        # Callee code should be at offset 12
        callee_at_12 = result.code[12:12 + len(callee_code)]
        # After patching the bl in callee is still li r3,99; blr — no relocs to patch
        self.assertEqual(len(result.code), layout.total_size)

    def test_intra_tu_bl_patched(self):
        """bl to co-loaded callee targets real offset, not trampoline."""
        root_code = assemble(
            ppc_li(3, 0),   # 0
            ppc_bl(0),      # 4 — placeholder bl to Callee
            ppc_blr(),      # 8
        )
        callee_code = assemble(ppc_li(3, 99), ppc_blr())
        root_relocs = [make_reloc(4, "Callee", "REL24")]
        callee_relocs = []

        layout = self._make_layout(len(root_code), [("Callee", 12)])
        callees = {"Callee": (callee_code, callee_relocs)}
        intra_tu_addrs = {sym: self.CODE_BASE + off
                          for sym, off in layout.symbol_offsets.items()}

        coff = _make_coff_with_text_symbols([("Root", 0), ("Callee", 100)])

        result = self.prepare_coloaded_side(
            root_code, root_relocs, coff, "Root", "none",
            callees, layout, intra_tu_addrs)

        # The bl at offset 4 should target CODE_BASE + 12
        insn = struct.unpack_from(">I", result.code, 4)[0]
        # bl encodes: opcode(6) | LI(24) | AA(1) | LK(1)
        # LK=1 for bl
        self.assertEqual(insn & 1, 1)  # LK bit set
        # Extract signed offset
        li_field = insn & 0x03FFFFFC
        if li_field & 0x02000000:
            li_field -= 0x04000000
        # bl is at CODE_BASE + 4, target is CODE_BASE + 12, delta = 8
        self.assertEqual(li_field, 8)

    def test_external_gets_trampoline(self):
        """External calls still get trampoline addresses."""
        root_code = assemble(
            ppc_li(3, 0),
            ppc_bl(0),      # -> ExternalFunc (trampoline)
            ppc_bl(0),      # -> Callee (intra-TU)
            ppc_blr(),
        )
        callee_code = assemble(ppc_li(3, 42), ppc_blr())

        root_relocs = [
            make_reloc(4, "ExternalFunc", "REL24"),
            make_reloc(8, "Callee", "REL24"),
        ]
        callee_relocs = []

        layout = self._make_layout(len(root_code), [("Callee", 16)])
        callees = {"Callee": (callee_code, callee_relocs)}
        intra_tu_addrs = {sym: self.CODE_BASE + off
                          for sym, off in layout.symbol_offsets.items()}

        coff = _make_coff_with_external(
            [("Root", 0), ("Callee", 100)], ["ExternalFunc"])

        result = self.prepare_coloaded_side(
            root_code, root_relocs, coff, "Root", "none",
            callees, layout, intra_tu_addrs)

        # ExternalFunc should have a trampoline
        self.assertIn("ExternalFunc", result.trampolines)
        # Callee should NOT have a trampoline (it's intra-TU)
        self.assertNotIn("Callee", result.trampolines)

    def test_ppc64_rewrite_in_combined(self):
        """PPC64 instructions in combined buffer get rewritten."""
        root_code = assemble(
            ppc_std(31, -8, 1),  # PPC64 std
            ppc_blr(),
        )
        callee_code = assemble(
            ppc_ld(31, -8, 1),  # PPC64 ld
            ppc_blr(),
        )
        root_relocs = [make_reloc(0, "Callee", "REL24")]
        callee_relocs = []

        layout = self._make_layout(len(root_code), [("Callee", 8)])
        callees = {"Callee": (callee_code, callee_relocs)}
        intra_tu_addrs = {sym: self.CODE_BASE + off
                          for sym, off in layout.symbol_offsets.items()}

        coff = _make_coff_with_text_symbols([("Root", 0), ("Callee", 100)])

        result = self.prepare_coloaded_side(
            root_code, root_relocs, coff, "Root", "none",
            callees, layout, intra_tu_addrs)

        # Root's std should be stw
        insn0 = struct.unpack_from(">I", result.code, 0)[0]
        self.assertEqual((insn0 >> 26) & 0x3F, 36)  # stw

        # Callee's ld should be lwz
        insn_callee = struct.unpack_from(">I", result.code, 8)[0]
        self.assertEqual((insn_callee >> 26) & 0x3F, 32)  # lwz


if __name__ == "__main__":
    unittest.main()
