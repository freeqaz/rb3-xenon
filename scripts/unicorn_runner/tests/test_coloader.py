"""Tests for coloader.py — intra-TU callee co-loading logic.

Pure-logic tests use MockCOFF. Integration tests use Unicorn PPC32.
"""

import struct
import sys
import os
import unittest

from scripts.unicorn_runner.call_log import CL_TRAMP_ADDR

# Unicorn availability. The path search, the shadow ordering and the
# reason string all live in scripts/unicorn_runner/unicorn_dep.py — the
# hand-rolled `parent.parent.parent.parent.parent` block that used to sit
# here resolved to a nonexistent directory inside every git worktree.
from scripts.unicorn_runner.unicorn_dep import HAS_UNICORN, SKIP_REASON

from .helpers import (
    assemble, ppc_li, ppc_blr, ppc_bl, ppc_nop,
    ppc_stw, ppc_lwz, ppc_mflr, ppc_mtlr,
    make_reloc, MockCOFF,
)


def _make_coff_with_text_symbols(symbol_defs, section_data=None):
    """Build a MockCOFF with symbols in a .text section.

    symbol_defs: list of (name, value) tuples — value is offset within .text
    section_data: optional dict of section_idx -> bytes
    """
    sections = [{'name': '.text', 'raw_size': 1024, 'raw_offset': 0,
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
    coff._rebuild_caches()
    coff._section_data = section_data or {}
    return coff


def _make_coff_with_external(internal_syms, external_syms):
    """Build a MockCOFF with some symbols in .text and some external (section <= 0).

    internal_syms: list of (name, value) in .text
    external_syms: list of names (section=0, external)
    """
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
    coff._rebuild_caches()
    coff._section_data = {}
    return coff


class TestIsIntraTuCallee(unittest.TestCase):
    """Tests for is_intra_tu_callee()."""

    def setUp(self):
        from scripts.unicorn_runner.coloader import is_intra_tu_callee
        self.is_intra_tu_callee = is_intra_tu_callee

    def test_positive_text_symbol(self):
        """Symbol in .text section returns True."""
        coff = _make_coff_with_text_symbols([("Foo", 0), ("Bar", 100)])
        self.assertTrue(self.is_intra_tu_callee(coff, "Bar"))

    def test_negative_external_symbol(self):
        """External symbol (section=0) returns False."""
        coff = _make_coff_with_external([("Foo", 0)], ["ExtFunc"])
        self.assertFalse(self.is_intra_tu_callee(coff, "ExtFunc"))

    def test_negative_missing_symbol(self):
        """Symbol not in symbol_map returns False."""
        coff = _make_coff_with_text_symbols([("Foo", 0)])
        self.assertFalse(self.is_intra_tu_callee(coff, "NonExistent"))

    def test_negative_rdata_symbol(self):
        """Symbol in .rdata section returns False."""
        sections = [
            {'name': '.text', 'raw_size': 1024, 'raw_offset': 0,
             'num_relocs': 0, 'reloc_offset': 0, 'index': 1},
            {'name': '.rdata', 'raw_size': 256, 'raw_offset': 1024,
             'num_relocs': 0, 'reloc_offset': 0, 'index': 2},
        ]
        sym = {'name': 'rdata_sym', 'value': 0, 'section': 2,
               'type': 0, 'storage_class': 2, 'num_aux': 0, 'index': 0}
        coff = MockCOFF(sections=sections, symbol_map={'rdata_sym': sym})
        coff.symbols = [sym]
        self.assertFalse(self.is_intra_tu_callee(coff, "rdata_sym"))


class TestCollectCallees(unittest.TestCase):
    """Tests for collect_intra_tu_callees()."""

    def setUp(self):
        from scripts.unicorn_runner.coloader import collect_intra_tu_callees
        self.collect = collect_intra_tu_callees

    def _make_extract_fn(self, func_data):
        """Create a mock extract_fn: symbol_name -> (bytes, relocs).

        func_data: dict of symbol_name -> (bytes, relocs)
        """
        def extract(coff, symbol):
            if symbol in func_data:
                return func_data[symbol]
            return None, None
        return extract

    def test_simple_two_callees(self):
        """Root with 2 REL24 intra-TU targets collects both."""
        coff = _make_coff_with_text_symbols([
            ("Root", 0), ("CalleeA", 100), ("CalleeB", 200)])

        root_code = assemble(ppc_li(3, 0), ppc_blr())
        callee_a_code = assemble(ppc_li(3, 1), ppc_blr())
        callee_b_code = assemble(ppc_li(3, 2), ppc_blr())

        root_relocs = [make_reloc(0, "CalleeA", "REL24"), make_reloc(4, "CalleeB", "REL24")]
        callee_a_relocs = []
        callee_b_relocs = []

        func_data = {
            "Root": (root_code, root_relocs),
            "CalleeA": (callee_a_code, callee_a_relocs),
            "CalleeB": (callee_b_code, callee_b_relocs),
        }

        result = self.collect(coff, "Root", self._make_extract_fn(func_data))
        self.assertEqual(set(result.keys()), {"CalleeA", "CalleeB"})

    def test_transitive_callees(self):
        """Root -> A -> B collects both A and B."""
        coff = _make_coff_with_text_symbols([
            ("Root", 0), ("A", 100), ("B", 200)])

        root_code = assemble(ppc_li(3, 0), ppc_blr())
        a_code = assemble(ppc_li(3, 1), ppc_blr())
        b_code = assemble(ppc_li(3, 2), ppc_blr())

        func_data = {
            "Root": (root_code, [make_reloc(0, "A", "REL24")]),
            "A": (a_code, [make_reloc(0, "B", "REL24")]),
            "B": (b_code, []),
        }

        result = self.collect(coff, "Root", self._make_extract_fn(func_data))
        self.assertEqual(set(result.keys()), {"A", "B"})

    def test_cycle_terminates(self):
        """Root -> A -> Root terminates without infinite loop."""
        coff = _make_coff_with_text_symbols([("Root", 0), ("A", 100)])

        root_code = assemble(ppc_li(3, 0), ppc_blr())
        a_code = assemble(ppc_li(3, 1), ppc_blr())

        func_data = {
            "Root": (root_code, [make_reloc(0, "A", "REL24")]),
            "A": (a_code, [make_reloc(0, "Root", "REL24")]),
        }

        result = self.collect(coff, "Root", self._make_extract_fn(func_data))
        self.assertEqual(set(result.keys()), {"A"})  # Root not included

    def test_depth_limit(self):
        """max_depth=1 gets only direct callees."""
        coff = _make_coff_with_text_symbols([
            ("Root", 0), ("A", 100), ("B", 200)])

        root_code = assemble(ppc_li(3, 0), ppc_blr())
        a_code = assemble(ppc_li(3, 1), ppc_blr())
        b_code = assemble(ppc_li(3, 2), ppc_blr())

        func_data = {
            "Root": (root_code, [make_reloc(0, "A", "REL24")]),
            "A": (a_code, [make_reloc(0, "B", "REL24")]),
            "B": (b_code, []),
        }

        result = self.collect(coff, "Root", self._make_extract_fn(func_data),
                              max_depth=1)
        self.assertEqual(set(result.keys()), {"A"})  # B not included

    def test_external_not_collected(self):
        """External REL24 targets are not collected."""
        coff = _make_coff_with_external(
            [("Root", 0), ("InternalA", 100)], ["ExternalFunc"])

        root_code = assemble(ppc_li(3, 0), ppc_blr())
        a_code = assemble(ppc_li(3, 1), ppc_blr())

        func_data = {
            "Root": (root_code, [
                make_reloc(0, "InternalA", "REL24"),
                make_reloc(4, "ExternalFunc", "REL24"),
            ]),
            "InternalA": (a_code, []),
        }

        result = self.collect(coff, "Root", self._make_extract_fn(func_data))
        self.assertEqual(set(result.keys()), {"InternalA"})

    def test_section_symbol_targets_resolved(self):
        """REL24 targets to .text section symbol are resolved to actual functions."""
        # Simulate original .obj: one big .text, relocs target '.text' symbol
        # FuncA at offset 0, FuncB at offset 100
        coff = _make_coff_with_text_symbols([
            ('.text', 0),       # section symbol
            ("Root", 0),        # root function at offset 0
            ("FuncB", 100),     # callee at offset 100
        ])

        # Root code: li r3, 0 | bl +96 | blr
        # bl at function offset 4, section offset 4, target section offset 100
        # displacement = 100 - 4 = 96
        root_code = assemble(ppc_li(3, 0), ppc_bl(96), ppc_blr())
        funcb_code = assemble(ppc_li(3, 1), ppc_blr())

        # Reloc targets '.text' (section symbol), not 'FuncB'
        root_relocs = [make_reloc(4, ".text", "REL24")]

        func_data = {
            "Root": (root_code, root_relocs),
            "FuncB": (funcb_code, []),
        }

        result = self.collect(coff, "Root", self._make_extract_fn(func_data))
        # Should discover FuncB via displacement resolution
        self.assertEqual(set(result.keys()), {"FuncB"})

    def test_section_symbol_transitive(self):
        """Section-symbol resolution works transitively: Root ->.text-> A ->.text-> B."""
        # A at offset 200, B at offset 400
        coff = _make_coff_with_text_symbols([
            ('.text', 0),
            ("Root", 0),
            ("A", 200),
            ("B", 400),
        ])

        # Root bl at offset 4: target = 0+4+196 = 200 (A), disp = 196
        root_code = assemble(ppc_li(3, 0), ppc_bl(196), ppc_blr())
        # A bl at offset 4: target = 200+4+196 = 400 (B), disp = 196
        a_code = assemble(ppc_li(3, 1), ppc_bl(196), ppc_blr())
        b_code = assemble(ppc_li(3, 2), ppc_blr())

        func_data = {
            "Root": (root_code, [make_reloc(4, ".text", "REL24")]),
            "A": (a_code, [make_reloc(4, ".text", "REL24")]),
            "B": (b_code, []),
        }

        result = self.collect(coff, "Root", self._make_extract_fn(func_data))
        self.assertEqual(set(result.keys()), {"A", "B"})

    def test_section_symbol_self_call_skipped(self):
        """REL24 to .text resolving to root itself is not collected."""
        coff = _make_coff_with_text_symbols([
            ('.text', 0),
            ("Root", 0),
        ])

        # bl at offset 4, disp = -4, target = 0+4+(-4) = 0 = Root itself
        root_code = assemble(ppc_li(3, 0), ppc_bl(-4), ppc_blr())

        func_data = {
            "Root": (root_code, [make_reloc(4, ".text", "REL24")]),
        }

        result = self.collect(coff, "Root", self._make_extract_fn(func_data))
        self.assertEqual(set(result.keys()), set())


class TestResolveSectionRel24(unittest.TestCase):
    """Tests for resolve_section_rel24_targets()."""

    def setUp(self):
        from scripts.unicorn_runner.coloader import resolve_section_rel24_targets
        self.resolve = resolve_section_rel24_targets

    def test_resolves_text_target(self):
        """REL24 targeting .text is resolved to actual function symbol."""
        coff = _make_coff_with_text_symbols([
            ('.text', 0),
            ("Caller", 0),
            ("Callee", 200),
        ])

        # bl at offset 4: section_offset=4, disp=196, target=200
        code = assemble(ppc_li(3, 0), ppc_bl(196), ppc_blr())
        relocs = [make_reloc(4, ".text", "REL24")]

        resolved = self.resolve(coff, "Caller", code, relocs)
        self.assertEqual(len(resolved), 1)
        self.assertEqual(resolved[0]['symbol_name'], "Callee")
        self.assertEqual(resolved[0]['offset'], 4)

    def test_non_text_relocs_unchanged(self):
        """REFHI/REFLO relocs are not modified."""
        coff = _make_coff_with_text_symbols([('.text', 0), ("Caller", 0)])
        code = assemble(ppc_li(3, 0), ppc_nop(), ppc_blr())
        relocs = [
            make_reloc(0, "some_global", "REFHI"),
            make_reloc(4, "some_global", "REFLO"),
        ]

        resolved = self.resolve(coff, "Caller", code, relocs)
        self.assertEqual(len(resolved), 2)
        self.assertEqual(resolved[0]['symbol_name'], "some_global")
        self.assertEqual(resolved[1]['symbol_name'], "some_global")

    def test_named_rel24_unchanged(self):
        """REL24 targeting a named function (not section symbol) is unchanged."""
        coff = _make_coff_with_text_symbols([("Caller", 0), ("Target", 100)])
        code = assemble(ppc_li(3, 0), ppc_bl(96), ppc_blr())
        relocs = [make_reloc(4, "Target", "REL24")]

        resolved = self.resolve(coff, "Caller", code, relocs)
        self.assertEqual(len(resolved), 1)
        self.assertEqual(resolved[0]['symbol_name'], "Target")

    def test_unresolvable_left_unchanged(self):
        """REL24 to .text that doesn't match any function is left as .text."""
        coff = _make_coff_with_text_symbols([
            ('.text', 0),
            ("Caller", 0),
            # No function at offset 200
        ])

        code = assemble(ppc_li(3, 0), ppc_bl(196), ppc_blr())
        relocs = [make_reloc(4, ".text", "REL24")]

        resolved = self.resolve(coff, "Caller", code, relocs)
        self.assertEqual(len(resolved), 1)
        self.assertEqual(resolved[0]['symbol_name'], ".text")  # unchanged

    def test_multiple_targets_resolved(self):
        """Multiple .text REL24 relocs each resolved independently."""
        coff = _make_coff_with_text_symbols([
            ('.text', 0),
            ("Caller", 0),
            ("FuncA", 100),
            ("FuncB", 300),
        ])

        # bl at offset 4: target = 0+4+96 = 100 (FuncA)
        # bl at offset 8: target = 0+8+292 = 300 (FuncB)
        code = assemble(ppc_li(3, 0), ppc_bl(96), ppc_bl(292), ppc_blr())
        relocs = [
            make_reloc(4, ".text", "REL24"),
            make_reloc(8, ".text", "REL24"),
        ]

        resolved = self.resolve(coff, "Caller", code, relocs)
        self.assertEqual(len(resolved), 2)
        self.assertEqual(resolved[0]['symbol_name'], "FuncA")
        self.assertEqual(resolved[1]['symbol_name'], "FuncB")


class TestBuildLayout(unittest.TestCase):
    """Tests for build_coload_layout()."""

    def setUp(self):
        from scripts.unicorn_runner.coloader import build_coload_layout
        self.build_layout = build_coload_layout

    def test_basic_layout(self):
        """Correct offsets with 4-byte alignment."""
        coff = _make_coff_with_text_symbols([
            ("Root", 0), ("CalleeA", 100), ("CalleeB", 200)])

        root_bytes = assemble(ppc_li(3, 0), ppc_blr())  # 8 bytes
        a_bytes_d = assemble(ppc_li(3, 1), ppc_blr())  # 8 bytes
        a_bytes_o = assemble(ppc_li(3, 1), ppc_blr())  # 8 bytes
        b_bytes_d = assemble(ppc_li(3, 2), ppc_blr())  # 8 bytes
        b_bytes_o = assemble(ppc_li(3, 2), ppc_blr())  # 8 bytes

        decomp_callees = {
            "CalleeA": (a_bytes_d, []),
            "CalleeB": (b_bytes_d, []),
        }
        orig_callees = {
            "CalleeA": (a_bytes_o, []),
            "CalleeB": (b_bytes_o, []),
        }
        common = {"CalleeA", "CalleeB"}

        layout = self.build_layout("Root", root_bytes, common,
                                    decomp_callees, orig_callees, coff, coff)
        self.assertIsNotNone(layout)
        self.assertEqual(layout.symbol_offsets["Root"], 0)
        self.assertEqual(layout.symbol_offsets["CalleeA"], 8)  # right after root
        self.assertEqual(layout.symbol_offsets["CalleeB"], 16)  # after CalleeA
        self.assertEqual(layout.total_size, 24)
        self.assertEqual(len(layout.coloaded_symbols), 2)

    def test_alignment_with_odd_sizes(self):
        """Non-multiple-of-4 function sizes get padded."""
        coff = _make_coff_with_text_symbols([("Root", 0), ("A", 100)])

        # 10-byte root (not aligned to 4)
        root_bytes = b'\x00' * 10
        a_bytes = assemble(ppc_li(3, 1), ppc_blr())  # 8 bytes

        decomp_callees = {"A": (a_bytes, [])}
        orig_callees = {"A": (a_bytes, [])}

        layout = self.build_layout("Root", root_bytes, {"A"},
                                    decomp_callees, orig_callees, coff, coff)
        self.assertIsNotNone(layout)
        self.assertEqual(layout.symbol_offsets["Root"], 0)
        self.assertEqual(layout.symbol_offsets["A"], 12)  # 10 aligned up to 12
        self.assertEqual(layout.total_size, 20)  # 12 + 8

    def test_uses_max_size(self):
        """Layout uses max(decomp_size, orig_size) per callee."""
        coff = _make_coff_with_text_symbols([("Root", 0), ("A", 100)])

        root_bytes = assemble(ppc_li(3, 0), ppc_blr())  # 8 bytes
        a_decomp = b'\x00' * 12  # 12 bytes
        a_orig = b'\x00' * 20    # 20 bytes

        decomp_callees = {"A": (a_decomp, [])}
        orig_callees = {"A": (a_orig, [])}

        layout = self.build_layout("Root", root_bytes, {"A"},
                                    decomp_callees, orig_callees, coff, coff)
        self.assertIsNotNone(layout)
        self.assertEqual(layout.symbol_offsets["A"], 8)
        self.assertEqual(layout.total_size, 28)  # 8 + 20

    def test_overflow_returns_none(self):
        """>64KB combined returns None."""
        coff = _make_coff_with_text_symbols([("Root", 0), ("Big", 100)])

        root_bytes = assemble(ppc_li(3, 0), ppc_blr())
        big_bytes = b'\x00' * 70000  # > 64KB by itself

        decomp_callees = {"Big": (big_bytes, [])}
        orig_callees = {"Big": (big_bytes, [])}

        layout = self.build_layout("Root", root_bytes, {"Big"},
                                    decomp_callees, orig_callees, coff, coff)
        self.assertIsNone(layout)

    def test_empty_common_returns_none(self):
        """No common callees returns None."""
        coff = _make_coff_with_text_symbols([("Root", 0)])
        root_bytes = assemble(ppc_li(3, 0), ppc_blr())
        layout = self.build_layout("Root", root_bytes, set(), {}, {}, coff, coff)
        self.assertIsNone(layout)


class TestPartitionRelocs(unittest.TestCase):
    """Tests for partition_relocs()."""

    def setUp(self):
        from scripts.unicorn_runner.coloader import partition_relocs
        self.partition = partition_relocs

    def test_filters_intra_tu_rel24(self):
        """REL24 relocs targeting intra-TU symbols are removed."""
        relocs = [
            make_reloc(0, "InternalA", "REL24"),
            make_reloc(4, "ExternalB", "REL24"),
            make_reloc(8, "InternalC", "REL24"),
        ]
        result = self.partition(relocs, {"InternalA", "InternalC"})
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]["symbol_name"], "ExternalB")

    def test_keeps_refhi_reflo(self):
        """Non-REL24 relocs (REFHI, REFLO) pass through even if symbol is intra-TU."""
        relocs = [
            make_reloc(0, "Sym", "REL24"),
            make_reloc(4, "Sym", "REFHI"),
            make_reloc(8, "Sym", "REFLO"),
        ]
        result = self.partition(relocs, {"Sym"})
        self.assertEqual(len(result), 2)
        self.assertEqual(result[0]["type_name"], "REFHI")
        self.assertEqual(result[1]["type_name"], "REFLO")

    def test_keeps_addr32(self):
        """ADDR32 relocs pass through even if symbol is intra-TU."""
        relocs = [
            make_reloc(0, "Sym", "ADDR32"),
            make_reloc(4, "Sym", "REL24"),
        ]
        result = self.partition(relocs, {"Sym"})
        self.assertEqual(len(result), 1)
        self.assertEqual(result[0]["type_name"], "ADDR32")

    def test_empty_intra_tu_set(self):
        """Empty intra-TU set returns all relocs unchanged."""
        relocs = [
            make_reloc(0, "A", "REL24"),
            make_reloc(4, "B", "REFHI"),
        ]
        result = self.partition(relocs, set())
        self.assertEqual(len(result), 2)


class TestAdjustRelocs(unittest.TestCase):
    """Tests for adjust_relocs_to_layout()."""

    def setUp(self):
        from scripts.unicorn_runner.coloader import adjust_relocs_to_layout
        self.adjust = adjust_relocs_to_layout

    def test_adjusts_offsets(self):
        """Reloc offsets are shifted by base_offset."""
        relocs = [make_reloc(0, "A", "REL24"), make_reloc(8, "B", "REFHI")]
        result = self.adjust(relocs, 100)
        self.assertEqual(result[0]["offset"], 100)
        self.assertEqual(result[1]["offset"], 108)

    def test_does_not_modify_original(self):
        """Original reloc list is not modified."""
        relocs = [make_reloc(0, "A", "REL24")]
        self.adjust(relocs, 50)
        self.assertEqual(relocs[0]["offset"], 0)


# ---------------------------------------------------------------------------
# Unicorn integration tests
# ---------------------------------------------------------------------------

@unittest.skipUnless(HAS_UNICORN, SKIP_REASON)
class TestColoadedExecution(unittest.TestCase):
    """Integration tests verifying co-loaded callees execute correctly."""

    def setUp(self):
        from scripts.unicorn_runner.engine import execute_function
        from scripts.unicorn_runner.patcher import patch_rel24, assign_addresses, patch_function
        from scripts.unicorn_runner.memory_map import CODE_BASE, TRAMPOLINE_BASE
        self.execute = execute_function
        self.patch_rel24 = patch_rel24
        self.assign_addresses = assign_addresses
        self.patch_function = patch_function
        self.CODE_BASE = CODE_BASE
        self.TRAMPOLINE_BASE = TRAMPOLINE_BASE

    def test_coloaded_callee_executes(self):
        """Root bl's to co-loaded callee, callee sets r3=99, verify r3=99 (not 0)."""
        # Layout: root at 0, callee at offset 16
        #
        # Root (offset 0):
        #   mflr r0          ; 0
        #   stw r0, 4(r1)    ; 4
        #   bl +8            ; 8  -> callee at offset 16
        #   lwz r0, 4(r1)    ; 12
        #   mtlr r0          ; 16 -- wait, this overlaps callee!
        # Need to adjust: callee at offset 24 or root must be smaller

        # Root (16 bytes at offset 0):
        #   mflr r0
        #   stw r0, 4(r1)
        #   bl <callee>       (offset 8, delta = 16 - 8 = 8 bytes forward)
        #   mtlr r0            (but we need to restore LR first...)
        #
        # Actually, bl sets LR = PC+4. The callee does blr which returns to PC+4.
        # After bl returns, we just need lwz r0; mtlr r0; blr.
        #
        # Root (24 bytes at offset 0):
        callee_offset = 24

        root_code = assemble(
            ppc_mflr(0),               # 0: save LR
            ppc_stw(0, 4, 1),          # 4: stw r0, 4(r1)
            ppc_bl(callee_offset - 8), # 8: bl to callee (delta from PC at offset 8)
            ppc_lwz(0, 4, 1),          # 12: restore LR
            ppc_mtlr(0),               # 16: mtlr r0
            ppc_blr(),                 # 20: return
        )
        # Callee (8 bytes at offset 24):
        callee_code = assemble(
            ppc_li(3, 99),             # li r3, 99
            ppc_blr(),                 # blr
        )

        # Build combined buffer
        combined = bytearray(callee_offset + len(callee_code))
        combined[:len(root_code)] = root_code
        combined[callee_offset:callee_offset + len(callee_code)] = callee_code

        # No trampolines needed — the bl is already patched to the right offset
        result = self.execute(combined, {}, len(combined))
        self.assertIsNone(result.error)
        self.assertEqual(result.r3, 99)

    def test_coloaded_callee_chain(self):
        """Root -> A -> B, B sets r3=77, verify r3=77."""
        # Layout: root at 0, A at 32, B at 48
        a_offset = 32
        b_offset = 48

        # Root calls A
        root_code = assemble(
            ppc_mflr(0),
            ppc_stw(0, 4, 1),
            ppc_bl(a_offset - 8),       # offset 8 -> A at 32 (delta=24)
            ppc_lwz(0, 4, 1),
            ppc_mtlr(0),
            ppc_blr(),
        )
        # Pad root to 24 bytes (6 insns)
        assert len(root_code) == 24

        # A calls B
        a_code = assemble(
            ppc_mflr(0),
            ppc_stw(0, 8, 1),           # save at different stack offset
            ppc_bl(b_offset - (a_offset + 8)),  # delta from A's bl PC to B
            ppc_lwz(0, 8, 1),
            ppc_mtlr(0),
            ppc_blr(),
        )
        assert len(a_code) == 24

        # Pad A to fill up to offset 48 (16 bytes of padding between A end and B)
        # Actually A starts at 32 and is 24 bytes, so A ends at 56. That's past B!
        # Let me recalculate: A at 24, B at 48
        a_offset = 24
        b_offset = 48

        root_code = assemble(
            ppc_mflr(0),
            ppc_stw(0, 4, 1),
            ppc_bl(a_offset - 8),
            ppc_lwz(0, 4, 1),
            ppc_mtlr(0),
            ppc_blr(),
        )

        a_code = assemble(
            ppc_mflr(0),
            ppc_stw(0, 8, 1),
            ppc_bl(b_offset - (a_offset + 8)),
            ppc_lwz(0, 8, 1),
            ppc_mtlr(0),
            ppc_blr(),
        )

        # B returns 77
        b_code = assemble(
            ppc_li(3, 77),
            ppc_blr(),
        )

        combined = bytearray(b_offset + len(b_code))
        combined[:len(root_code)] = root_code
        combined[a_offset:a_offset + len(a_code)] = a_code
        combined[b_offset:b_offset + len(b_code)] = b_code

        result = self.execute(combined, {}, len(combined))
        self.assertIsNone(result.error)
        self.assertEqual(result.r3, 77)

    def test_coloaded_with_external_call(self):
        """Co-loaded callee calls external -> trampoline logged, returns 0."""
        # Layout: root at 0, callee at 24
        # Callee calls external trampoline, then returns that result
        callee_offset = 24
        tramp_addr = self.TRAMPOLINE_BASE

        root_code = assemble(
            ppc_mflr(0),
            ppc_stw(0, 4, 1),
            ppc_bl(callee_offset - 8),
            ppc_lwz(0, 4, 1),
            ppc_mtlr(0),
            ppc_blr(),
        )

        # Callee: saves LR, calls external trampoline, restores LR, returns
        # The bl to trampoline needs patching. We'll build it with a raw delta.
        callee_bl_pc = self.CODE_BASE + callee_offset + 8  # bl is at callee+8
        ext_delta = tramp_addr - callee_bl_pc

        callee_code = assemble(
            ppc_mflr(0),
            ppc_stw(0, 8, 1),
            ppc_bl(ext_delta),          # bl to external trampoline
            ppc_lwz(0, 8, 1),
            ppc_mtlr(0),
            ppc_blr(),
        )

        combined = bytearray(callee_offset + len(callee_code))
        combined[:len(root_code)] = root_code
        combined[callee_offset:callee_offset + len(callee_code)] = callee_code

        trampolines = {"external_func": tramp_addr}
        result = self.execute(combined, trampolines, len(combined))
        self.assertIsNone(result.error)
        # External trampoline returns 0
        self.assertEqual(result.r3, 0)
        # The external call should be logged
        self.assertEqual(len(result.call_log), 1)
        self.assertEqual(result.call_log[0][CL_TRAMP_ADDR], tramp_addr)


if __name__ == "__main__":
    unittest.main()
