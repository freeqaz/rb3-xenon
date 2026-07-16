"""Shared test utilities for unicorn_runner tests.

PPC instruction builders (hand-assembled) and mock factories.
"""

import struct


# ---------------------------------------------------------------------------
# PPC instruction builders — return a 32-bit int (big-endian word)
# ---------------------------------------------------------------------------

def ppc_li(rd, imm16):
    """li rD, imm  (alias for addi rD, 0, imm)"""
    return 0x38000000 | (rd << 21) | (imm16 & 0xFFFF)


def ppc_addi(rd, ra, imm16):
    """addi rD, rA, imm"""
    return 0x38000000 | (rd << 21) | (ra << 16) | (imm16 & 0xFFFF)


def ppc_blr():
    """blr — branch to link register"""
    return 0x4E800020


def ppc_bl(offset):
    """bl offset — branch and link (relative)"""
    return 0x48000001 | (offset & 0x03FFFFFC)


def ppc_bctr():
    """bctr — branch to count register"""
    return 0x4E800420


def ppc_bctrl():
    """bctrl — branch to count register and link"""
    return 0x4E800421


def ppc_lwz(rd, offset, ra):
    """lwz rD, offset(rA)"""
    return (32 << 26) | (rd << 21) | (ra << 16) | (offset & 0xFFFF)


def ppc_stw(rs, offset, ra):
    """stw rS, offset(rA)"""
    return (36 << 26) | (rs << 21) | (ra << 16) | (offset & 0xFFFF)


def ppc_std(rs, offset, ra):
    """std rS, offset(rA) — PPC64 DS-form, opcode 62"""
    # DS-form: opcode(6) | RS(5) | RA(5) | DS(14) | XO(2)
    # DS field is offset / 4 (must be 4-byte aligned)
    ds = (offset // 4) & 0x3FFF
    return (62 << 26) | (rs << 21) | (ra << 16) | (ds << 2) | 0


def ppc_ld(rd, offset, ra):
    """ld rD, offset(rA) — PPC64 DS-form, opcode 58"""
    ds = (offset // 4) & 0x3FFF
    return (58 << 26) | (rd << 21) | (ra << 16) | (ds << 2) | 0


def ppc_mflr(rd):
    """mflr rD — move from link register"""
    return 0x7C0802A6 | (rd << 21)


def ppc_mtlr(rs):
    """mtlr rS — move to link register"""
    return 0x7C0803A6 | (rs << 21)


def ppc_mtctr(rs):
    """mtctr rS — move to count register"""
    return 0x7C0903A6 | (rs << 21)


def ppc_lfs(frd, offset, ra):
    """lfs frD, offset(rA)"""
    return (48 << 26) | (frd << 21) | (ra << 16) | (offset & 0xFFFF)


def ppc_stfs(frs, offset, ra):
    """stfs frS, offset(rA)"""
    return (52 << 26) | (frs << 21) | (ra << 16) | (offset & 0xFFFF)


def ppc_nop():
    """nop (ori 0,0,0)"""
    return 0x60000000


def ppc_b(offset):
    """b offset — unconditional branch (no link)"""
    return 0x48000000 | (offset & 0x03FFFFFC)


def assemble(*insns):
    """Pack multiple 32-bit PPC instructions into big-endian bytes."""
    return b"".join(struct.pack(">I", i) for i in insns)


# ---------------------------------------------------------------------------
# Mock factories
# ---------------------------------------------------------------------------

class MockExecutionResult:
    """Lightweight stand-in for engine.ExecutionResult.

    terminated_normally and cap_exhausted default to safe values:
    - When `error` is None, terminated_normally defaults to True (normal
      return path).
    - When `error` is set, terminated_normally defaults to False (a wild
      jump produced the error).
    - cap_exhausted always defaults to False unless overridden.
    """

    def __init__(self, r3=0, f1=0, call_log=None, object_memory=None,
                 globals_memory=None, error=None,
                 terminated_normally=None, cap_exhausted=False,
                 final_pc=0):
        self.r3 = r3
        self.f1 = f1
        self.call_log = call_log or []
        self.object_memory = object_memory or (b"\x00" * 0x10000)
        self.globals_memory = globals_memory or (b"\x00" * 0x10000)
        self.error = error
        if terminated_normally is None:
            terminated_normally = error is None
        self.terminated_normally = terminated_normally
        self.cap_exhausted = cap_exhausted
        self.final_pc = final_pc


def make_reloc(offset, symbol_name, type_name):
    """Create a relocation dict matching COFFParser format."""
    type_map = {
        "ABSOLUTE": 0x0000,
        "ADDR32": 0x0002,
        "REL24": 0x0006,
        "REFHI": 0x0010,
        "REFLO": 0x0011,
        "PAIR": 0x0012,
    }
    return {
        "offset": offset,
        "symbol_index": 0,
        "symbol_name": symbol_name,
        "type": type_map.get(type_name, 0),
        "type_name": type_name,
    }


def make_call_log_entry(call_index, r3=0, r4=0, r5=0, r6=0,
                        trampoline_addr=0x80010000, source_offset=0):
    """Create a call log entry matching engine tuple format.

    Returns (index, tramp_addr, src_offset, r3, r4, r5, r6).
    """
    return (call_index, trampoline_addr, source_offset, r3, r4, r5, r6)


def make_simple_function():
    """li r3, 42; blr — minimal test function returning 42."""
    return assemble(ppc_li(3, 42), ppc_blr())


def make_float_function():
    """lfs f1, 0(r3); blr — returns float from memory pointed to by r3."""
    return assemble(ppc_lfs(1, 0, 3), ppc_blr())


def make_call_function(bl_offset):
    """mflr r0; bl <offset>; mtlr r0; blr — calls one trampoline."""
    return assemble(
        ppc_mflr(0),
        ppc_bl(bl_offset),
        ppc_mtlr(0),
        ppc_blr(),
    )


def make_bctrl_function(vtable_slot=0):
    """Build a function that does a virtual dispatch via bctrl.

    lwz r12, 0(r3)           ; r12 = vtable ptr from this+0
    lwz r12, N(r12)          ; r12 = vtable[slot]
    mtctr r12                ; CTR = virtual method address
    bctrl                    ; call through CTR
    blr
    """
    offset = vtable_slot * 4
    return assemble(
        ppc_lwz(12, 0, 3),       # lwz r12, 0(r3)
        ppc_lwz(12, offset, 12),  # lwz r12, N(r12)
        ppc_mtctr(12),            # mtctr r12
        ppc_bctrl(),              # bctrl
        ppc_blr(),                # blr
    )


class MockCOFF:
    """Minimal mock of COFFParser for extractor tests."""

    def __init__(self, sections=None, symbol_map=None):
        self.sections = sections or []
        self.symbol_map = symbol_map or {}
        self.symbols = []
        self._rebuild_caches()

    def _rebuild_caches(self):
        """Build lookup caches matching COFFParser._parse_symbols()."""
        self._section_names = frozenset(s['name'] for s in self.sections)
        self._symbols_by_section_offset = {}
        for sym in self.symbols:
            if sym.get('section', 0) > 0 and not sym['name'].startswith('$'):
                key = (sym['section'], sym['value'])
                if key not in self._symbols_by_section_offset:
                    self._symbols_by_section_offset[key] = sym['name']
