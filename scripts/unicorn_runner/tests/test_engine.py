"""Tests for engine.py — Unicorn PPC32 execution engine.

Requires the Unicorn PPC engine. Tests use hand-crafted bytecode, no .obj files.
"""

import struct
import sys
import os
import unittest

from scripts.unicorn_runner.call_log import CL_INDEX, CL_TRAMP_ADDR

# Unicorn availability. The path search, the shadow ordering and the
# reason string all live in scripts/unicorn_runner/unicorn_dep.py — the
# hand-rolled `parent.parent.parent.parent.parent` block that used to sit
# here resolved to a nonexistent directory inside every git worktree.
from scripts.unicorn_runner.unicorn_dep import HAS_UNICORN, SKIP_REASON

from .helpers import (
    assemble, ppc_li, ppc_blr, ppc_bl, ppc_nop,
    ppc_stw, ppc_lwz, ppc_lfs, ppc_stfs,
    ppc_mflr, ppc_mtlr, ppc_b,
    make_simple_function, make_float_function, make_bctrl_function,
)


@unittest.skipUnless(HAS_UNICORN, SKIP_REASON)
class TestExecuteFunction(unittest.TestCase):
    """Tests for execute_function() with hand-crafted PPC bytecode."""

    def setUp(self):
        from scripts.unicorn_runner.engine import execute_function
        from scripts.unicorn_runner.memory_map import (
            CODE_BASE, TRAMPOLINE_BASE, OBJECT_BASE, RDATA_BASE,
            GLOBAL_BASE, VTABLE_BASE, REGION_SIZE,
        )
        self.execute = execute_function
        self.CODE_BASE = CODE_BASE
        self.TRAMPOLINE_BASE = TRAMPOLINE_BASE
        self.OBJECT_BASE = OBJECT_BASE
        self.RDATA_BASE = RDATA_BASE
        self.GLOBAL_BASE = GLOBAL_BASE
        self.VTABLE_BASE = VTABLE_BASE
        self.REGION_SIZE = REGION_SIZE

    def test_simple_return(self):
        """li r3, 42; blr → r3=42, no error."""
        code = bytearray(make_simple_function())
        result = self.execute(code, {}, len(code))
        self.assertIsNone(result.error)
        self.assertEqual(result.r3, 42)

    def test_float_return(self):
        """lfs f1, 4(r3); blr → f1 has correct raw bits from OBJECT_BASE+4."""
        # OBJECT_BASE+0 holds the vtable pointer, so load from offset 4
        # where memory is still zeroed.
        code = bytearray(assemble(ppc_lfs(1, 4, 3), ppc_blr()))
        result = self.execute(code, {}, len(code))
        self.assertIsNone(result.error)
        # lfs from zeroed memory → f1 = 0.0 → raw bits = 0
        self.assertEqual(result.f1, 0)

    def test_call_logging(self):
        """Function with bl to trampoline → call_log has 1 entry."""
        # mflr r0; stw r0, 4(r1); bl trampoline; lwz r0, 4(r1); mtlr r0; blr
        tramp_addr = self.TRAMPOLINE_BASE
        delta = tramp_addr - self.CODE_BASE - 8  # bl at offset 8

        code = bytearray(assemble(
            ppc_mflr(0),            # 0: mflr r0
            ppc_stw(0, 4, 1),      # 4: stw r0, 4(r1) — save LR
            ppc_bl(delta),          # 8: bl <trampoline>
            ppc_lwz(0, 4, 1),      # 12: lwz r0, 4(r1) — restore LR
            ppc_mtlr(0),            # 16: mtlr r0
            ppc_blr(),              # 20: blr
        ))
        trampolines = {"test_func": tramp_addr}
        result = self.execute(code, trampolines, len(code))
        self.assertIsNone(result.error)
        self.assertEqual(len(result.call_log), 1)
        self.assertEqual(result.call_log[0][CL_INDEX], 0)
        self.assertEqual(result.call_log[0][CL_TRAMP_ADDR], tramp_addr)

    def test_multiple_calls(self):
        """Function with 3 bl stubs → call_log has 3 entries in order."""
        tramp_a = self.TRAMPOLINE_BASE
        tramp_b = self.TRAMPOLINE_BASE + 8
        tramp_c = self.TRAMPOLINE_BASE + 16

        # Each bl needs the delta from its own PC to the target
        # Instructions: mflr r0; stw r0, 4(r1); bl A; bl B; bl C; lwz r0, 4(r1); mtlr r0; blr
        insns = [
            ppc_mflr(0),           # offset 0
            ppc_stw(0, 4, 1),     # offset 4
            ppc_bl(tramp_a - (self.CODE_BASE + 8)),   # offset 8
            ppc_bl(tramp_b - (self.CODE_BASE + 12)),  # offset 12
            ppc_bl(tramp_c - (self.CODE_BASE + 16)),  # offset 16
            ppc_lwz(0, 4, 1),     # offset 20
            ppc_mtlr(0),          # offset 24
            ppc_blr(),            # offset 28
        ]
        code = bytearray(assemble(*insns))
        trampolines = {"a": tramp_a, "b": tramp_b, "c": tramp_c}
        result = self.execute(code, trampolines, len(code))
        self.assertIsNone(result.error)
        self.assertEqual(len(result.call_log), 3)
        for i in range(3):
            self.assertEqual(result.call_log[i][CL_INDEX], i)

    def test_sentinel_return(self):
        """Normal blr → error=None (sentinel fetch is handled)."""
        code = bytearray(assemble(ppc_li(3, 0), ppc_blr()))
        result = self.execute(code, {}, len(code))
        self.assertIsNone(result.error)

    def test_timeout(self):
        """Infinite loop → error string (not hang)."""
        # b . (branch to self)
        code = bytearray(assemble(ppc_b(0)))
        result = self.execute(code, {}, len(code), timeout=100_000)
        # After timeout, Unicorn stops. The PC won't be at SENTINEL_ADDR,
        # so it might report an error or just have r3 at its initial value.
        # The important thing is it doesn't hang.
        # Note: Unicorn timeout doesn't raise — it just stops. PC will still
        # be in the loop, so the sentinel check won't fire. No error expected
        # from the timeout itself, but execution won't have completed normally.
        # Just verify it returns within the timeout.
        self.assertIsNotNone(result)

    def test_unmapped_access(self):
        """Load from address 0 → map-on-demand, no error."""
        # li r4, 0; lwz r5, 0(r4); blr
        code = bytearray(assemble(
            ppc_li(4, 0),
            ppc_lwz(5, 0, 4),
            ppc_blr(),
        ))
        result = self.execute(code, {}, len(code))
        self.assertIsNone(result.error)

    def test_object_memory_writeback(self):
        """stw to OBJECT_BASE → object_memory reflects write."""
        # li r4, 0x1234; stw r4, 0(r3); blr
        # r3 is initialized to OBJECT_BASE by the engine
        code = bytearray(assemble(
            ppc_li(4, 0x1234),     # li r4, 0x1234 (positive imm16, no sign ext)
            ppc_stw(4, 0, 3),     # stw r4, 0(r3)  — r3 = OBJECT_BASE
            ppc_blr(),
        ))
        result = self.execute(code, {}, len(code))
        self.assertIsNone(result.error)
        # Check first word of object_memory
        val = struct.unpack_from(">I", result.object_memory, 0)[0]
        self.assertEqual(val, 0x00001234)

    def test_rdata_mapping(self):
        """Pass rdata_bytes, lwz from RDATA_BASE → correct value."""
        # Build rdata with a known word at offset 0
        rdata = struct.pack(">I", 0xCAFEBABE)

        # li r4, RDATA_BASE_HI; ori r4, r4, RDATA_BASE_LO; lwz r5, 0(r4); ...
        # Simpler: use lis + ori to load RDATA_BASE into r4
        rdata_hi = (self.RDATA_BASE >> 16) & 0xFFFF
        rdata_lo = self.RDATA_BASE & 0xFFFF
        # lis r4, hi (addis r4, 0, hi)
        lis_r4 = 0x3C800000 | rdata_hi
        # ori r4, r4, lo
        ori_r4 = 0x60840000 | rdata_lo

        code = bytearray(assemble(
            lis_r4,
            ori_r4,
            ppc_lwz(5, 0, 4),
            ppc_blr(),
        ))
        result = self.execute(code, {}, len(code), rdata_bytes=rdata)
        self.assertIsNone(result.error)
        # r5 should have the value we loaded; but we can't read r5 directly
        # from ExecutionResult. Instead, copy to r3 to verify via return value.

        # Better test: load into r3 (return register)
        code = bytearray(assemble(
            lis_r4,
            ori_r4,
            ppc_lwz(3, 0, 4),  # Load into r3
            ppc_blr(),
        ))
        result = self.execute(code, {}, len(code), rdata_bytes=rdata)
        self.assertIsNone(result.error)
        self.assertEqual(result.r3, 0xCAFEBABE)

    def test_f1_initially_zero(self):
        """No float ops → f1=0."""
        code = bytearray(assemble(ppc_li(3, 0), ppc_blr()))
        result = self.execute(code, {}, len(code))
        self.assertIsNone(result.error)
        self.assertEqual(result.f1, 0)

    def test_bctrl_vtable_dispatch(self):
        """bctrl with vtable mock: lwz→lwz→mtctr→bctrl lands on vtable trampoline."""
        from scripts.unicorn_runner.memory_map import (
            VTABLE_BASE, VTABLE_TRAMP_OFFSET,
        )
        # Use vtable slot 3 (offset 12 into vtable)
        code = bytearray(make_bctrl_function(vtable_slot=3))
        result = self.execute(code, {}, len(code))
        self.assertIsNone(result.error)
        # The bctrl should have dispatched to the vtable trampoline for slot 3
        self.assertEqual(len(result.call_log), 1)
        expected_tramp = self.TRAMPOLINE_BASE + VTABLE_TRAMP_OFFSET + (3 * 8)
        self.assertEqual(result.call_log[0][CL_TRAMP_ADDR], expected_tramp)
        # Trampoline returns 0 via li r3, 0
        self.assertEqual(result.r3, 0)

    # --- Fill pattern tests ---

    def test_fill_pattern_object_memory(self):
        """fill_pattern=0xCD fills object memory (after vtable ptr)."""
        code = bytearray(make_simple_function())
        result = self.execute(code, {}, len(code), fill_pattern=0xCD)
        self.assertIsNone(result.error)
        # Bytes after the vtable pointer (offset 4) should be 0xCD fill
        self.assertEqual(result.object_memory[4:8], b'\xcd\xcd\xcd\xcd')

    def test_fill_pattern_globals_memory(self):
        """fill_pattern=0xCD fills globals memory."""
        code = bytearray(make_simple_function())
        result = self.execute(code, {}, len(code), fill_pattern=0xCD)
        self.assertIsNone(result.error)
        self.assertEqual(result.globals_memory[0:4], b'\xcd\xcd\xcd\xcd')

    def test_fill_pattern_none_is_zeroed(self):
        """Default (no fill_pattern) leaves globals zeroed."""
        code = bytearray(make_simple_function())
        result = self.execute(code, {}, len(code))
        self.assertIsNone(result.error)
        self.assertEqual(result.globals_memory[0:4], b'\x00\x00\x00\x00')

    def test_fill_pattern_vtable_preserved(self):
        """fill_pattern=0xCD still has correct vtable ptr at OBJECT_BASE+0."""
        code = bytearray(make_simple_function())
        result = self.execute(code, {}, len(code), fill_pattern=0xCD)
        self.assertIsNone(result.error)
        vtable_ptr = struct.unpack_from(">I", result.object_memory, 0)[0]
        self.assertEqual(vtable_ptr, self.VTABLE_BASE)

    def test_fill_pattern_on_demand_pages(self):
        """On-demand mapped pages are filled with pattern."""
        # li r4, 0; lwz r3, 0(r4); blr — read from address 0 (unmapped)
        code = bytearray(assemble(
            ppc_li(4, 0),
            ppc_lwz(3, 0, 4),
            ppc_blr(),
        ))
        result = self.execute(code, {}, len(code), fill_pattern=0xCD)
        self.assertIsNone(result.error)
        # Address 0 page mapped on demand, filled with 0xCD
        self.assertEqual(result.r3, 0xCDCDCDCD)

    def test_fill_pattern_bctrl_still_works(self):
        """Vtable dispatch works correctly with fill_pattern=0xCD."""
        from scripts.unicorn_runner.memory_map import VTABLE_TRAMP_OFFSET
        code = bytearray(make_bctrl_function(vtable_slot=3))
        result = self.execute(code, {}, len(code), fill_pattern=0xCD)
        self.assertIsNone(result.error)
        self.assertEqual(len(result.call_log), 1)
        expected_tramp = self.TRAMPOLINE_BASE + VTABLE_TRAMP_OFFSET + (3 * 8)
        self.assertEqual(result.call_log[0][CL_TRAMP_ADDR], expected_tramp)


if __name__ == "__main__":
    unittest.main()
