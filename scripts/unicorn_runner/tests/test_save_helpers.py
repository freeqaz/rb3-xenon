"""Tests for save_helpers.py — MSVC's out-of-line register save/restore.

These pin the fix for the harness's largest single defect: `__savegprlr_N` and
friends were given the generic `li r3,0; blr` trampoline stub, which killed
`this` at the second instruction of every helper-using function and, because
`b __restgprlr_N` is a tail branch whose target is the thing that reloads LR,
turned every such function that made a call into an infinite loop.

Four properties are load-bearing and each has a test here:

  1. A helper-using function that calls something must RETURN, not spin.
  2. r3 must survive the prologue.
  3. A restore must never be mistaken for a save. MSVC emits
     `bl __restfpr_28` (LK=1) immediately before `b __restgprlr_26` (LK=0);
     an earlier prototype keyed on the link bit, read the FPR restore as a
     save, corrupted the LR slot and manufactured error-class divergences in
     exactly the FPR-spilling functions. Classification is by symbol name.
  4. cap_exhausted must fire wherever in the loaded image PC happens to be,
     not only inside the root function's byte range.
"""

import os
import re
import struct
import unittest

from scripts.unicorn_runner.unicorn_dep import HAS_UNICORN, SKIP_REASON
from scripts.unicorn_runner import save_helpers as SH
from scripts.unicorn_runner.memory_map import (
    CODE_BASE, TRAMPOLINE_BASE, HELPER_BASE, HELPER_SLOT_SIZE, REGION_SIZE,
    OBJECT_BASE, SENTINEL_ADDR, RDATA_BASE,
)
from scripts.unicorn_runner.patcher import assign_addresses, patch_function

from .helpers import (
    assemble, ppc_li, ppc_blr, ppc_bl, ppc_b, ppc_stw, ppc_lwz,
    ppc_mflr, ppc_mtlr, ppc_addi,
)


# --- local encoders (kept out of the shared helpers module: only used here) --

def ppc_mr(rd, rs):
    """mr rD, rS  (or rD, rS, rS)"""
    return (31 << 26) | (rs << 21) | (rd << 16) | (rs << 11) | (444 << 1)


def ppc_stwu(rs, offset, ra):
    return (37 << 26) | (rs << 21) | (ra << 16) | (offset & 0xFFFF)


def reloc(offset, symbol, type_name="REL24"):
    return {"offset": offset, "symbol_name": symbol, "type_name": type_name}


def words(blob):
    return [struct.unpack_from(">I", blob, i)[0] for i in range(0, len(blob), 4)]


def opcodes(blob):
    return [(w >> 26) & 0x3F for w in words(blob)]


STORE_OPCODES = {36, 37, 38, 39, 44, 45, 52, 53, 54, 55, 62}   # stw/stb/sth/stf*/std
LOAD_OPCODES = {32, 33, 34, 35, 40, 41, 48, 49, 50, 51, 58}    # lwz/lbz/lhz/lf*/ld


class TestClassification(unittest.TestCase):
    """Names in, (op, bank, n) out — and nothing else decides."""

    def test_all_six_banks_recognised(self):
        self.assertEqual(SH.classify("__savegprlr_29"), ("save", "gprlr", 29))
        self.assertEqual(SH.classify("__restgprlr_14"), ("rest", "gprlr", 14))
        self.assertEqual(SH.classify("__savefpr_28"), ("save", "fpr", 28))
        self.assertEqual(SH.classify("__restfpr_31"), ("rest", "fpr", 31))
        self.assertEqual(SH.classify("__savevmx_127"), ("save", "vmx", 127))
        self.assertEqual(SH.classify("__restvmx_64"), ("rest", "vmx", 64))

    def test_bare_name_is_the_first_entry_point(self):
        """The linker map gives `__savegprlr` and `__savegprlr_14` one address."""
        self.assertEqual(SH.classify("__savegprlr"), ("save", "gprlr", 14))
        self.assertEqual(SH.helper_address("__savegprlr"),
                         SH.helper_address("__savegprlr_14"))

    def test_non_helpers_rejected(self):
        for name in ("?Poll@Game@@QAAXXZ", "__savegprlr_13", "__savegprlr_32",
                     "__savefpr_64", "__savevmx_63", "__savegprlr_x",
                     "memcpy", "", None):
            self.assertIsNone(SH.classify(name), name)
            self.assertFalse(SH.is_helper(name), name)
            self.assertIsNone(SH.helper_address(name), name)

    def test_restore_is_a_restore_regardless_of_how_it_is_reached(self):
        """The regression guard for the link-bit misclassification.

        MSVC reaches __restfpr_N with `bl` (LK=1) and __restgprlr_N with `b`
        (LK=0) in the same epilogue. Nothing here may consult LK, so a restore
        body must contain only loads, and a save body only stores.
        """
        for n in range(14, 32):
            rest_fpr = SH.emulated_body(f"__restfpr_{n}")
            self.assertFalse(STORE_OPCODES & set(opcodes(rest_fpr)),
                             f"__restfpr_{n} stores something")
            save_fpr = SH.emulated_body(f"__savefpr_{n}")
            self.assertFalse(LOAD_OPCODES & set(opcodes(save_fpr)),
                             f"__savefpr_{n} loads something")

    def test_restfpr_never_touches_the_lr_slot(self):
        """-0x8(r1) holds the caller's return address. A restore must not write it.

        The prototype that keyed on the link bit wrote `stw r12,-0x8(r1)` here
        and the function returned into hyperspace.
        """
        lr_slot_store = ppc_stw(12, -0x8, 1)
        for n in range(14, 32):
            self.assertNotIn(lr_slot_store,
                             words(SH.emulated_body(f"__restfpr_{n}")))


class TestLayout(unittest.TestCase):

    def test_addresses_are_distinct_aligned_and_in_region(self):
        seen = {}
        for sym in SH.all_helper_symbols():
            addr = SH.helper_address(sym)
            self.assertNotIn(addr, seen, f"{sym} collides with {seen.get(addr)}")
            seen[addr] = sym
            self.assertGreaterEqual(addr, HELPER_BASE)
            self.assertLess(addr, HELPER_BASE + REGION_SIZE)
            self.assertEqual((addr - HELPER_BASE) % HELPER_SLOT_SIZE, 0)

    def test_no_body_overruns_its_slot(self):
        for sym in SH.all_helper_symbols():
            self.assertLessEqual(len(SH.emulated_body(sym)), HELPER_SLOT_SIZE, sym)

    def test_addresses_do_not_collide_with_other_regions(self):
        """REL24 reach and region separation."""
        for base in (CODE_BASE, TRAMPOLINE_BASE, RDATA_BASE):
            self.assertFalse(base <= HELPER_BASE < base + REGION_SIZE)
        self.assertLess(abs(HELPER_BASE - CODE_BASE), 0x2000000)

    def test_unmapped_guard_between_rdata_and_the_helper_bodies(self):
        """Nothing upstream clamps the packed rdata buffer to its window.

        If HELPER sat immediately above RDATA, an oversized buffer would
        overwrite executable helper bodies and present as a decomp bug. Keep at
        least one region-sized hole so the write faults instead.
        """
        self.assertGreaterEqual(HELPER_BASE - (RDATA_BASE + REGION_SIZE),
                                REGION_SIZE)

    def test_region_contains_every_body(self):
        region = SH.helper_region()
        self.assertEqual(len(region), REGION_SIZE)
        for sym in SH.all_helper_symbols():
            off = SH.helper_address(sym) - HELPER_BASE
            body = SH.emulated_body(sym)
            self.assertEqual(region[off:off + len(body)], body, sym)


class TestBodies(unittest.TestCase):

    def test_emulated_is_exactly_the_ppc64_rewrite_of_raw(self):
        from scripts.unicorn_runner.patcher import rewrite_ppc64_insns
        for sym in SH.all_helper_symbols():
            buf = bytearray(SH.raw_body(sym))
            rewrite_ppc64_insns(buf)
            self.assertEqual(bytes(buf), SH.emulated_body(sym), sym)

    def test_no_ppc64_insns_survive(self):
        """Unicorn PPC32 raises UC_ERR_EXCEPTION on std/ld."""
        for sym in SH.all_helper_symbols():
            self.assertFalse({58, 62} & set(opcodes(SH.emulated_body(sym))), sym)

    def test_gpr_save_and_restore_are_mirror_images(self):
        """Same registers, same offsets, opposite direction."""
        for n in range(14, 32):
            save = words(SH.emulated_body(f"__savegprlr_{n}"))
            rest = words(SH.emulated_body(f"__restgprlr_{n}"))
            # save: stw r_n..r31, stw r12 (LR), blr
            # rest: lwz r_n..r31, lwz r12 (LR), mtlr r12, blr
            self.assertEqual(len(save), (32 - n) + 2)
            self.assertEqual(len(rest), (32 - n) + 3)
            for i in range(32 - n + 1):
                s, r = save[i], rest[i]
                self.assertEqual(s & 0x03FFFFFF, r & 0x03FFFFFF)  # reg/base/off
                self.assertEqual((s >> 26) & 0x3F, 36)            # stw
                self.assertEqual((r >> 26) & 0x3F, 32)            # lwz

    def test_restgprlr_reloads_lr(self):
        """The whole point: the epilogue's tail branch must return correctly."""
        body = words(SH.emulated_body("__restgprlr_29"))
        self.assertEqual(body[-3], ppc_lwz(12, -0x8, 1))
        self.assertEqual(body[-2], ppc_mtlr(12))
        self.assertEqual(body[-1], ppc_blr())

    def test_savegprlr_spills_lr_from_r12(self):
        body = words(SH.emulated_body("__savegprlr_29"))
        self.assertEqual(body[-2], ppc_stw(12, -0x8, 1))
        self.assertEqual(body[-1], ppc_blr())

    def test_vmx_is_the_documented_approximation(self):
        """No vector state is modelled; only r11's final value is reproduced.

        r11, not r0: the shipped routines end `li r11,-0x10; <vector op>; blr`
        in both banks. The first cut of save_helpers misread rD in 0x3960FFF0
        as 0, so this test pins the register against the encoding below.
        """
        self.assertEqual(ppc_li(11, -0x10), 0x3960FFF0)     # as in the image
        for sym in ("__savevmx_14", "__restvmx_127", "__savevmx_64"):
            body = words(SH.emulated_body(sym))
            self.assertEqual(body, [ppc_li(11, -0x10), ppc_blr()], sym)


class TestAgainstShippedImage(unittest.TestCase):
    """The bodies are not invented — they are what the console ran.

    Recovered from `orig/45410914/band.exe` at the addresses
    `config/45410914/symbols.txt` gives, so a typo in the offset arithmetic
    cannot pass. VMX is excluded: it is an approximation on purpose.

    ⚠ Retitled from dc3 (373307D9/ham_xbox_r.exe) by lane U1-DEEPSCHED. This
    is the test that makes the helper bodies EVIDENCE rather than assertion:
    it reads RB3 retail's own bytes at RB3 retail's own addresses. If it
    skips, the bodies are unverified in THIS binary no matter how well they
    are verified in dc3 -- so a skip here must be reported, never ignored.
    """

    @classmethod
    def setUpClass(cls):
        from scripts.unicorn_runner.image import project_root, get_global_image
        cls.root = project_root()
        cls.image = get_global_image(cls.root)
        cls.addrs = {}
        syms = os.path.join(cls.root, "config", "45410914", "symbols.txt")
        if os.path.exists(syms):
            pat = re.compile(
                r"^(__(?:save|rest)(?:gprlr|fpr|vmx)(?:_\d+)?)\s*=\s*"
                r"\.text:0x([0-9A-Fa-f]+);")
            with open(syms, errors="replace") as f:
                for line in f:
                    m = pat.match(line.strip())
                    if m:
                        cls.addrs[m.group(1)] = int(m.group(2), 16)

    def test_gpr_and_fpr_bodies_match_the_image_byte_for_byte(self):
        if not self.image.available or not self.addrs:
            self.skipTest("shipped image or symbols.txt unavailable")
        checked = 0
        for sym in SH.all_helper_symbols():
            if SH.classify(sym)[1] == "vmx":
                continue
            addr = self.addrs.get(sym)
            self.assertIsNotNone(addr, f"{sym} missing from symbols.txt")
            body = SH.raw_body(sym)
            self.assertEqual(self.image.read(addr, len(body)), body, sym)
            checked += 1
        self.assertEqual(checked, 72)      # 18 entry points x 4 banks

    def test_vmx_r11_matches_the_word_the_image_ends_each_bank_with(self):
        """The VMX body is an approximation, but its one word is not invented.

        Every bank of __savevmx/__restvmx ends `li r11,-0x10; <vector op>; blr`.
        Locate those `blr`s in the image and check the word two back is exactly
        what emulated_body emits. This is what catches an rD misread.
        """
        if not self.image.available or not self.addrs:
            self.skipTest("shipped image or symbols.txt unavailable")
        expected = words(SH.emulated_body("__savevmx_14"))[0]
        found = 0
        for base in ("__savevmx", "__restvmx"):
            addr = self.addrs[base]
            blob = self.image.read(addr, 0x298)
            ws = words(blob)
            for i, w in enumerate(ws):
                if w == 0x4E800020 and i >= 2:      # blr ends a bank
                    self.assertEqual(ws[i - 2], expected,
                                     f"{base} bank ending at 0x{addr + i * 4:08X}")
                    found += 1
        self.assertEqual(found, 4)                  # two banks x two routines


class TestAddressAssignment(unittest.TestCase):
    """Helpers resolve to the helper region and consume no trampoline slots."""

    def test_helper_routed_to_helper_region(self):
        tramps, _ = assign_addresses([reloc(4, "__savegprlr_29")])
        self.assertEqual(tramps["__savegprlr_29"],
                         SH.helper_address("__savegprlr_29"))

    def test_helper_does_not_shift_later_trampoline_slots(self):
        """The call-log off-by-one corollary.

        The two sides disagree about which functions need __savegprlr_N. When
        the helper took a slot, the side that used it pushed every later
        symbol's trampoline address up by 8 — and the prologue `bl` landed in
        the hooked region, so it was logged as a call the function made. The
        comparator then reported a bogus arg mismatch at the first real call.
        """
        with_helper, _ = assign_addresses([
            reloc(4, "__savegprlr_29"),
            reloc(16, "?Foo@@YAXXZ"),
            reloc(24, "__restgprlr_29"),
            reloc(28, "?Bar@@YAXXZ"),
        ])
        without_helper, _ = assign_addresses([
            reloc(16, "?Foo@@YAXXZ"),
            reloc(28, "?Bar@@YAXXZ"),
        ])
        self.assertEqual(with_helper["?Foo@@YAXXZ"], TRAMPOLINE_BASE)
        self.assertEqual(with_helper["?Bar@@YAXXZ"], TRAMPOLINE_BASE + 8)
        self.assertEqual(with_helper["?Foo@@YAXXZ"], without_helper["?Foo@@YAXXZ"])
        self.assertEqual(with_helper["?Bar@@YAXXZ"], without_helper["?Bar@@YAXXZ"])

    def test_non_helper_double_underscore_symbols_still_get_stubs(self):
        tramps, _ = assign_addresses([reloc(0, "__CxxFrameHandler")])
        self.assertEqual(tramps["__CxxFrameHandler"], TRAMPOLINE_BASE)


@unittest.skipUnless(HAS_UNICORN, SKIP_REASON)
class TestExecution(unittest.TestCase):
    """End-to-end: assemble the real MSVC prologue/epilogue shape and run it."""

    def setUp(self):
        from scripts.unicorn_runner.engine import execute_function
        self.execute = execute_function

    def _build(self, insns, relocs):
        code = bytearray(assemble(*insns))
        tramps, globals_map = assign_addresses(relocs)
        patch_function(code, relocs, tramps, globals_map, CODE_BASE)
        return code, tramps

    def test_helper_using_function_with_a_call_returns(self):
        """The defect in one test: this used to spin to the cap.

        `b __restgprlr_29` is a tail branch. Stubbed, its `blr` returned to
        whatever LR held — the address just past `bl ?Foo`, four instructions
        back inside the function — so the epilogue re-entered the body.
        """
        insns = [
            ppc_mflr(12),               # +00
            ppc_bl(0),                  # +04  bl __savegprlr_29
            ppc_stwu(1, -0x40, 1),      # +08
            ppc_mr(29, 3),              # +0C  stash `this` in a callee-save
            ppc_bl(0),                  # +10  bl ?Foo — stub clobbers r3 and LR
            ppc_mr(3, 29),              # +14  hand `this` back as the result
            ppc_addi(1, 1, 0x40),       # +18
            ppc_b(0),                   # +1C  b __restgprlr_29 (LK=0, tail)
        ]
        relocs = [reloc(0x04, "__savegprlr_29"), reloc(0x10, "?Foo@@YAXXZ"),
                  reloc(0x1C, "__restgprlr_29")]
        code, tramps = self._build(insns, relocs)

        result = self.execute(code, tramps, len(code), max_insns=2000)
        self.assertIsNone(result.error)
        self.assertTrue(result.terminated_normally)
        self.assertFalse(result.cap_exhausted)
        self.assertEqual(result.final_pc, SENTINEL_ADDR)
        # r3 came back through r29, which means the object pointer survived
        # both the prologue helper and the stubbed call.
        self.assertEqual(result.r3, OBJECT_BASE)
        # Exactly one call logged: ?Foo. The prologue and epilogue helpers are
        # not calls the function made and must not appear.
        self.assertEqual(len(result.call_log), 1)

    def test_r3_survives_the_prologue(self):
        """`li r3,0` at instruction #2 was the other half of the defect."""
        insns = [ppc_mflr(12), ppc_bl(0), ppc_b(0)]
        relocs = [reloc(0x04, "__savegprlr_31"), reloc(0x08, "__restgprlr_31")]
        code, tramps = self._build(insns, relocs)

        result = self.execute(code, tramps, len(code), max_insns=2000)
        self.assertIsNone(result.error)
        self.assertEqual(result.r3, OBJECT_BASE)
        self.assertEqual(result.final_pc, SENTINEL_ADDR)
        self.assertEqual(result.call_log, [])

    def test_callee_saved_registers_are_actually_restored(self):
        """Why the bodies are real instead of the prototypes' `nop`.

        Both earlier prototypes turned the spill into a nop, which is
        invisible for a leaf and wrong under co-loading: the callee keeps the
        caller's r29 on return. Outer stashes a value in r29, calls Inner
        (which uses the helpers and clobbers r29), then returns r29.
        """
        MARK, CLOBBER = 0x1234, 0x5678
        insns = [
            # ---- outer, open-coded prologue so only Inner uses the helper --
            ppc_mflr(12),               # +00
            ppc_stw(12, -0x8, 1),       # +04
            ppc_stwu(1, -0x20, 1),      # +08
            ppc_li(29, MARK),           # +0C
            ppc_bl(0x18),               # +10  bl inner (+0x28)
            ppc_addi(1, 1, 0x20),       # +14
            ppc_mr(3, 29),              # +18  return r29
            ppc_lwz(12, -0x8, 1),       # +1C
            ppc_mtlr(12),               # +20
            ppc_blr(),                  # +24
            # ---- inner --------------------------------------------------
            ppc_mflr(12),               # +28
            ppc_bl(0),                  # +2C  bl __savegprlr_29
            # The frame must clear the r29..r31 + LR save area at -0x28(r1),
            # exactly as MSVC sizes it; a smaller stwu parks the back chain on
            # top of the r29 slot and the "restore" reads its own back chain.
            ppc_stwu(1, -0x40, 1),      # +30
            ppc_li(29, CLOBBER),        # +34
            ppc_addi(1, 1, 0x40),       # +38
            ppc_b(0),                   # +3C  b __restgprlr_29
        ]
        relocs = [reloc(0x2C, "__savegprlr_29"), reloc(0x3C, "__restgprlr_29")]
        code, tramps = self._build(insns, relocs)

        result = self.execute(code, tramps, len(code), max_insns=2000)
        self.assertIsNone(result.error)
        self.assertEqual(result.final_pc, SENTINEL_ADDR)
        self.assertEqual(result.r3, MARK,
                         "callee-saved r29 was not restored by __restgprlr_29")

    def test_fpr_restore_reached_by_bl_does_not_corrupt_the_return(self):
        """The full four-helper epilogue MSVC actually emits.

        `bl __restfpr_28` (LK=1) then `b __restgprlr_28` (LK=0). Reading the
        first as a save — as the link-bit prototype did — overwrites -0x8(r1)
        and the function returns to garbage.
        """
        insns = [
            ppc_mflr(12),               # +00
            ppc_bl(0),                  # +04  bl __savegprlr_28
            ppc_addi(12, 1, -0x28),     # +08  r12 = bottom of the GPR area
            ppc_bl(0),                  # +0C  bl __savefpr_28
            ppc_stwu(1, -0x80, 1),      # +10
            ppc_mr(28, 3),              # +14
            ppc_bl(0),                  # +18  bl ?Foo
            ppc_mr(3, 28),              # +1C
            ppc_addi(1, 1, 0x80),       # +20
            ppc_addi(12, 1, -0x28),     # +24
            ppc_bl(0),                  # +28  bl __restfpr_28   (LK=1!)
            ppc_b(0),                   # +2C  b  __restgprlr_28 (LK=0)
        ]
        relocs = [reloc(0x04, "__savegprlr_28"), reloc(0x0C, "__savefpr_28"),
                  reloc(0x18, "?Foo@@YAXXZ"), reloc(0x28, "__restfpr_28"),
                  reloc(0x2C, "__restgprlr_28")]
        code, tramps = self._build(insns, relocs)

        result = self.execute(code, tramps, len(code), max_insns=2000)
        self.assertIsNone(result.error)
        self.assertTrue(result.terminated_normally)
        self.assertFalse(result.cap_exhausted)
        self.assertEqual(result.final_pc, SENTINEL_ADDR)
        self.assertEqual(result.r3, OBJECT_BASE)
        self.assertEqual(len(result.call_log), 1)


@unittest.skipUnless(HAS_UNICORN, SKIP_REASON)
class TestRdataIsClamped(unittest.TestCase):
    """An oversized rdata buffer must fail loudly, not spill into a neighbour."""

    def test_oversized_rdata_raises(self):
        from scripts.unicorn_runner.engine import execute_function
        code = bytearray(assemble(ppc_blr()))
        with self.assertRaises(ValueError):
            execute_function(code, {}, len(code),
                             rdata_bytes=b"\x00" * (REGION_SIZE + 4))

    def test_exactly_region_sized_rdata_is_accepted(self):
        from scripts.unicorn_runner.engine import execute_function
        code = bytearray(assemble(ppc_blr()))
        result = execute_function(code, {}, len(code),
                                  rdata_bytes=b"\x00" * REGION_SIZE)
        self.assertIsNone(result.error)


@unittest.skipUnless(HAS_UNICORN, SKIP_REASON)
class TestCapExhaustionIsNotPcGated(unittest.TestCase):
    """cap_exhausted must not depend on where in the loop PC happened to be.

    It was set only when PC sat inside the ROOT function's byte range at the
    instant the cap fired. A side spinning just as hard but caught inside a
    trampoline stub was recorded terminated_normally=True instead — which is
    how 52 of 59 symmetric infinite loops were filed as one-sided
    `cap_exhausted_decomp`.
    """

    def setUp(self):
        from scripts.unicorn_runner.engine import execute_function
        self.execute = execute_function

    def test_in_executable_image_covers_code_trampolines_and_helpers(self):
        from scripts.unicorn_runner.engine import _in_executable_image
        for base in (CODE_BASE, TRAMPOLINE_BASE, HELPER_BASE):
            self.assertTrue(_in_executable_image(base))
            self.assertTrue(_in_executable_image(base + REGION_SIZE - 4))
        for addr in (SENTINEL_ADDR, OBJECT_BASE, 0, 0x50000000):
            self.assertFalse(_in_executable_image(addr))

    def _spin(self, max_insns):
        """`bl ?Foo; b -4` — a 4-instruction cycle through a trampoline stub."""
        code = bytearray(assemble(ppc_bl(0), ppc_b(-4)))
        relocs = [reloc(0, "?Foo@@YAXXZ")]
        tramps, globals_map = assign_addresses(relocs)
        patch_function(code, relocs, tramps, globals_map, CODE_BASE)
        return self.execute(code, tramps, len(code), max_insns=max_insns)

    def test_cap_in_a_trampoline_is_still_cap_exhausted(self):
        """The cycle visits the trampoline for two of its four instructions.

        Whichever phase the cap lands in, the run is truncated and its state
        untrustworthy — so every phase must report cap_exhausted.
        """
        seen_regions = set()
        for count in range(1, 9):
            result = self._spin(count)
            self.assertIsNone(result.error, f"count={count}")
            self.assertTrue(result.cap_exhausted,
                            f"count={count} pc=0x{result.final_pc:08X}")
            self.assertFalse(result.terminated_normally, f"count={count}")
            pc = result.final_pc & 0xFFFFFFFF
            if TRAMPOLINE_BASE <= pc < TRAMPOLINE_BASE + REGION_SIZE:
                seen_regions.add("trampoline")
            elif CODE_BASE <= pc < CODE_BASE + REGION_SIZE:
                seen_regions.add("code")
        # If the spin never parked in a trampoline this test proves nothing.
        self.assertIn("trampoline", seen_regions)
        self.assertIn("code", seen_regions)


if __name__ == "__main__":
    unittest.main()
