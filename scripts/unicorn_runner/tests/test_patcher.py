"""Tests for patcher.py — relocation patching and PPC64 rewriting."""

import struct
import unittest

from .helpers import (
    assemble, ppc_li, ppc_blr, ppc_stw, ppc_lwz, ppc_std, ppc_ld, ppc_nop,
    make_reloc,
)


class TestRewritePpc64(unittest.TestCase):
    """Tests for rewrite_ppc64_insns()."""

    def setUp(self):
        from scripts.unicorn_runner.patcher import rewrite_ppc64_insns
        self.rewrite = rewrite_ppc64_insns

    def test_rewrite_std_to_stw(self):
        """std r31, -8(r1) → stw r31, -8(r1)."""
        code = bytearray(assemble(ppc_std(31, -8, 1)))
        count = self.rewrite(code)
        self.assertEqual(count, 1)
        insn = struct.unpack_from(">I", code, 0)[0]
        opcode = (insn >> 26) & 0x3F
        self.assertEqual(opcode, 36)  # stw
        # Check rd=31, ra=1
        rd = (insn >> 21) & 0x1F
        ra = (insn >> 16) & 0x1F
        self.assertEqual(rd, 31)
        self.assertEqual(ra, 1)
        # Check offset field encodes -8
        offset = insn & 0xFFFF
        if offset >= 0x8000:
            offset -= 0x10000
        self.assertEqual(offset, -8)

    def test_rewrite_ld_to_lwz(self):
        """ld r31, -8(r1) → lwz r31, -8(r1)."""
        code = bytearray(assemble(ppc_ld(31, -8, 1)))
        count = self.rewrite(code)
        self.assertEqual(count, 1)
        insn = struct.unpack_from(">I", code, 0)[0]
        opcode = (insn >> 26) & 0x3F
        self.assertEqual(opcode, 32)  # lwz

    def test_rewrite_preserves_non_ppc64(self):
        """Normal PPC32 instructions are not modified."""
        orig_bytes = assemble(ppc_li(3, 42), ppc_stw(3, 0, 1), ppc_blr())
        code = bytearray(orig_bytes)
        count = self.rewrite(code)
        self.assertEqual(count, 0)
        self.assertEqual(bytes(code), orig_bytes)

    def test_rewrite_count(self):
        """Returns correct count of rewritten instructions."""
        code = bytearray(assemble(
            ppc_std(31, -8, 1),
            ppc_std(30, -16, 1),
            ppc_li(3, 0),
            ppc_ld(30, -16, 1),
            ppc_ld(31, -8, 1),
            ppc_blr(),
        ))
        count = self.rewrite(code)
        self.assertEqual(count, 4)

    def test_rewrite_positive_offset(self):
        """std r3, 16(r1) → stw r3, 16(r1)."""
        code = bytearray(assemble(ppc_std(3, 16, 1)))
        self.rewrite(code)
        insn = struct.unpack_from(">I", code, 0)[0]
        offset = insn & 0xFFFF
        self.assertEqual(offset, 16)


class TestPatchRel24(unittest.TestCase):
    """Tests for patch_rel24()."""

    def setUp(self):
        from scripts.unicorn_runner.patcher import patch_rel24
        self.patch_rel24 = patch_rel24

    def test_forward_branch(self):
        """Positive offset encoded correctly."""
        from .helpers import ppc_bl
        code_base = 0x80000000
        trampoline_addr = 0x80010000  # +0x10000 from code_base

        # bl 0 (placeholder, will be patched)
        code = bytearray(assemble(ppc_bl(0)))
        self.patch_rel24(code, 0, trampoline_addr, code_base)

        insn = struct.unpack_from(">I", code, 0)[0]
        # LK bit should be preserved (1 for bl)
        self.assertEqual(insn & 1, 1)
        # Extract delta: bits [6:29]
        delta = insn & 0x03FFFFFC
        if delta >= 0x02000000:
            delta -= 0x04000000
        self.assertEqual(delta, 0x10000)

    def test_negative_branch(self):
        """Negative offset (backward branch) encoded correctly."""
        from .helpers import ppc_bl
        code_base = 0x80010000
        trampoline_addr = 0x80000000  # -0x10000 from code_base

        code = bytearray(assemble(ppc_bl(0)))
        self.patch_rel24(code, 0, trampoline_addr, code_base)

        insn = struct.unpack_from(">I", code, 0)[0]
        delta = insn & 0x03FFFFFC
        if delta >= 0x02000000:
            delta -= 0x04000000
        self.assertEqual(delta, -0x10000)


class TestPatchRefHiRefLo(unittest.TestCase):
    """Tests for patch_refhi() and patch_reflo()."""

    def setUp(self):
        from scripts.unicorn_runner.patcher import patch_refhi, patch_reflo
        self.patch_refhi = patch_refhi
        self.patch_reflo = patch_reflo

    def test_simple_address(self):
        """addr 0x30000004 → hi=0x3000, lo=0x0004."""
        target = 0x30000004
        # lis r3, 0 (placeholder)
        code = bytearray(4)
        struct.pack_into(">I", code, 0, 0x3C600000)  # lis r3, 0
        self.patch_refhi(code, 0, target)
        insn = struct.unpack_from(">I", code, 0)[0]
        hi_field = insn & 0xFFFF
        self.assertEqual(hi_field, 0x3000)

        # addi r3, r3, 0 (placeholder)
        code2 = bytearray(4)
        struct.pack_into(">I", code2, 0, 0x38630000)  # addi r3, r3, 0
        self.patch_reflo(code2, 0, target)
        insn2 = struct.unpack_from(">I", code2, 0)[0]
        lo_field = insn2 & 0xFFFF
        self.assertEqual(lo_field, 0x0004)

    def test_carry_adjustment(self):
        """@ha with carry: addr 0x3000FFFF → hi=0x3001, lo=0xFFFF."""
        target = 0x3000FFFF
        code = bytearray(4)
        struct.pack_into(">I", code, 0, 0x3C600000)
        self.patch_refhi(code, 0, target)
        insn = struct.unpack_from(">I", code, 0)[0]
        hi_field = insn & 0xFFFF
        # @ha: (0x3000FFFF >> 16) + ((0x3000FFFF & 0x8000) >> 15) = 0x3000 + 1 = 0x3001
        self.assertEqual(hi_field, 0x3001)

    def test_no_carry_when_bit15_clear(self):
        """@ha with no carry: addr 0x30007000 → hi=0x3000, lo=0x7000."""
        target = 0x30007000
        code = bytearray(4)
        struct.pack_into(">I", code, 0, 0x3C600000)
        self.patch_refhi(code, 0, target)
        insn = struct.unpack_from(">I", code, 0)[0]
        hi_field = insn & 0xFFFF
        self.assertEqual(hi_field, 0x3000)


class TestPatchAddr32(unittest.TestCase):
    """Tests for patch_addr32()."""

    def setUp(self):
        from scripts.unicorn_runner.patcher import patch_addr32
        self.patch_addr32 = patch_addr32

    def test_writes_big_endian(self):
        code = bytearray(4)
        self.patch_addr32(code, 0, 0x30000004)
        val = struct.unpack_from(">I", code, 0)[0]
        self.assertEqual(val, 0x30000004)


class TestAssignAddresses(unittest.TestCase):
    """Tests for assign_addresses()."""

    def setUp(self):
        from scripts.unicorn_runner.patcher import assign_addresses
        from scripts.unicorn_runner.memory_map import TRAMPOLINE_BASE, GLOBAL_BASE
        self.assign_addresses = assign_addresses
        self.TRAMPOLINE_BASE = TRAMPOLINE_BASE
        self.GLOBAL_BASE = GLOBAL_BASE

    def test_rel24_to_trampolines(self):
        relocs = [make_reloc(0, "func_a", "REL24"), make_reloc(4, "func_b", "REL24")]
        trampolines, globals_map = self.assign_addresses(relocs)
        self.assertIn("func_a", trampolines)
        self.assertIn("func_b", trampolines)
        self.assertEqual(trampolines["func_a"], self.TRAMPOLINE_BASE)
        self.assertEqual(trampolines["func_b"], self.TRAMPOLINE_BASE + 8)
        self.assertEqual(len(globals_map), 0)

    def test_refhi_reflo_to_globals(self):
        relocs = [
            make_reloc(0, "g_var", "REFHI"),
            make_reloc(4, "g_var", "REFLO"),
        ]
        trampolines, globals_map = self.assign_addresses(relocs)
        self.assertEqual(len(trampolines), 0)
        self.assertIn("g_var", globals_map)
        self.assertEqual(globals_map["g_var"], self.GLOBAL_BASE)

    def test_addr32_to_globals(self):
        relocs = [make_reloc(0, "g_ptr", "ADDR32")]
        trampolines, globals_map = self.assign_addresses(relocs)
        self.assertIn("g_ptr", globals_map)
        self.assertEqual(globals_map["g_ptr"], self.GLOBAL_BASE)

    def test_deduplication(self):
        """Same symbol referenced multiple times gets one address."""
        relocs = [
            make_reloc(0, "func_a", "REL24"),
            make_reloc(8, "func_a", "REL24"),
        ]
        trampolines, _ = self.assign_addresses(relocs)
        self.assertEqual(len(trampolines), 1)


if __name__ == "__main__":
    unittest.main()
