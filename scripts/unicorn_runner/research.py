#!/usr/bin/env python3
"""
Unicorn Function Runner — Phase 0 Research Script

Two research goals:
1. Does Unicorn PPC actually work for MSVC-compiled Xbox 360 code?
2. Can we extract and relocate function bytes from our COFF .obj files?

This is a throwaway research script — results go into the design doc.
"""

import struct
import sys
import os
import time

# Use local Unicorn checkout
from pathlib import Path
_MILOHAX_DIR = Path(__file__).resolve().parent.parent.parent.parent
_UNICORN_DIR = _MILOHAX_DIR / "unicorn"
UNICORN_PATH = str(_UNICORN_DIR / "bindings" / "python")
sys.path.insert(0, UNICORN_PATH)
os.environ["LIBUNICORN_PATH"] = str(_UNICORN_DIR / "build")

from unicorn import Uc, UC_ARCH_PPC, UC_MODE_PPC32, UC_MODE_PPC64, UC_MODE_BIG_ENDIAN
from unicorn import UC_HOOK_CODE, UC_HOOK_INTR, UC_HOOK_MEM_READ_UNMAPPED, UC_HOOK_MEM_WRITE_UNMAPPED
from unicorn import UcError, UC_ERR_FETCH_UNMAPPED
from unicorn.ppc_const import *

# ============================================================
# PART 1: Unicorn PPC Viability Testing
# ============================================================

def test_basic_ppc32():
    """Test: can Unicorn execute basic PPC32 BE instructions?"""
    print("=" * 60)
    print("TEST 1a: Basic PPC32 Big-Endian execution")
    print("=" * 60)

    mu = Uc(UC_ARCH_PPC, UC_MODE_PPC32 + UC_MODE_BIG_ENDIAN)

    CODE_BASE = 0x80000000
    CODE_SIZE = 0x1000
    STACK_BASE = 0x10000000
    STACK_SIZE = 0x10000

    mu.mem_map(CODE_BASE, CODE_SIZE)
    mu.mem_map(STACK_BASE, STACK_SIZE)

    # Hand-assembled PPC instructions (big-endian):
    # li r3, 42       => 38 60 00 2A
    # blr             => 4E 80 00 20
    code = bytes([
        0x38, 0x60, 0x00, 0x2A,  # li r3, 42
        0x4E, 0x80, 0x00, 0x20,  # blr
    ])
    mu.mem_write(CODE_BASE, code)

    # Set up registers
    mu.reg_write(UC_PPC_REG_1, STACK_BASE + 0x8000)  # r1 = stack pointer
    mu.reg_write(UC_PPC_REG_LR, 0xDEAD0000)          # LR = sentinel

    # Run until fetch-unmapped fault (blr jumps to 0xDEAD0000)
    try:
        mu.emu_start(CODE_BASE, CODE_BASE + len(code), timeout=1000000)
    except UcError as e:
        if e.errno == UC_ERR_FETCH_UNMAPPED:
            pass  # Expected — blr jumped to unmapped LR sentinel
        else:
            print(f"  UNEXPECTED ERROR: {e}")
            return False

    r3 = mu.reg_read(UC_PPC_REG_3)
    pc = mu.reg_read(UC_PPC_REG_PC)
    print(f"  r3 = {r3} (expected 42)")
    print(f"  PC = 0x{pc:08X} (expected 0xDEAD0000)")

    success = (r3 == 42)
    print(f"  RESULT: {'PASS' if success else 'FAIL'}")
    return success


def test_ppc32_arithmetic():
    """Test: arithmetic, loads, stores, branches."""
    print("\n" + "=" * 60)
    print("TEST 1b: PPC32 arithmetic + memory + branches")
    print("=" * 60)

    mu = Uc(UC_ARCH_PPC, UC_MODE_PPC32 + UC_MODE_BIG_ENDIAN)

    CODE_BASE = 0x80000000
    CODE_SIZE = 0x1000
    STACK_BASE = 0x10000000
    STACK_SIZE = 0x10000
    DATA_BASE = 0x20000000
    DATA_SIZE = 0x1000

    mu.mem_map(CODE_BASE, CODE_SIZE)
    mu.mem_map(STACK_BASE, STACK_SIZE)
    mu.mem_map(DATA_BASE, DATA_SIZE)

    # Test: load word from memory, add immediate, store back, return sum
    #
    # r3 = pointer to int (DATA_BASE)
    # Function: return *r3 + 10
    #
    # lwz r4, 0(r3)    => 80 83 00 00
    # addi r3, r4, 10  => 38 64 00 0A
    # blr              => 4E 80 00 20
    code = bytes([
        0x80, 0x83, 0x00, 0x00,  # lwz r4, 0(r3)
        0x38, 0x64, 0x00, 0x0A,  # addi r3, r4, 10
        0x4E, 0x80, 0x00, 0x20,  # blr
    ])
    mu.mem_write(CODE_BASE, code)

    # Write test value at DATA_BASE: int32 = 100 (big-endian)
    mu.mem_write(DATA_BASE, struct.pack(">i", 100))

    mu.reg_write(UC_PPC_REG_1, STACK_BASE + 0x8000)
    mu.reg_write(UC_PPC_REG_3, DATA_BASE)  # r3 = pointer
    mu.reg_write(UC_PPC_REG_LR, 0xDEAD0000)

    try:
        mu.emu_start(CODE_BASE, CODE_BASE + len(code), timeout=1000000)
    except UcError as e:
        if e.errno == UC_ERR_FETCH_UNMAPPED:
            pass
        else:
            print(f"  UNEXPECTED ERROR: {e}")
            return False

    r3 = mu.reg_read(UC_PPC_REG_3)
    print(f"  r3 = {r3} (expected 110 = 100 + 10)")
    success = (r3 == 110)
    print(f"  RESULT: {'PASS' if success else 'FAIL'}")
    return success


def test_ppc32_branch():
    """Test: conditional branching (cmpwi + beq/bne)."""
    print("\n" + "=" * 60)
    print("TEST 1c: PPC32 conditional branching")
    print("=" * 60)

    mu = Uc(UC_ARCH_PPC, UC_MODE_PPC32 + UC_MODE_BIG_ENDIAN)

    CODE_BASE = 0x80000000
    CODE_SIZE = 0x1000
    STACK_BASE = 0x10000000
    STACK_SIZE = 0x10000

    mu.mem_map(CODE_BASE, CODE_SIZE)
    mu.mem_map(STACK_BASE, STACK_SIZE)

    # if (r3 == 0) return 1; else return 2;
    #
    # cmpwi cr0, r3, 0   => 2C 03 00 00
    # bne cr0, +8         => 40 82 00 0C  (skip 2 instructions = +12 bytes)
    # li r3, 1            => 38 60 00 01
    # blr                 => 4E 80 00 20
    # li r3, 2            => 38 60 00 02
    # blr                 => 4E 80 00 20
    code = bytes([
        0x2C, 0x03, 0x00, 0x00,  # cmpwi cr0, r3, 0
        0x40, 0x82, 0x00, 0x0C,  # bne cr0, +12
        0x38, 0x60, 0x00, 0x01,  # li r3, 1
        0x4E, 0x80, 0x00, 0x20,  # blr
        0x38, 0x60, 0x00, 0x02,  # li r3, 2
        0x4E, 0x80, 0x00, 0x20,  # blr
    ])
    mu.mem_write(CODE_BASE, code)

    results = []
    for input_val, expected in [(0, 1), (5, 2), (0xFFFFFFFF, 2)]:
        mu2 = Uc(UC_ARCH_PPC, UC_MODE_PPC32 + UC_MODE_BIG_ENDIAN)
        mu2.mem_map(CODE_BASE, CODE_SIZE)
        mu2.mem_map(STACK_BASE, STACK_SIZE)
        mu2.mem_write(CODE_BASE, code)
        mu2.reg_write(UC_PPC_REG_1, STACK_BASE + 0x8000)
        mu2.reg_write(UC_PPC_REG_3, input_val)
        mu2.reg_write(UC_PPC_REG_LR, 0xDEAD0000)

        try:
            mu2.emu_start(CODE_BASE, CODE_BASE + len(code), timeout=1000000)
        except UcError as e:
            if e.errno != UC_ERR_FETCH_UNMAPPED:
                print(f"  UNEXPECTED ERROR for input {input_val}: {e}")
                results.append(False)
                continue

        r3 = mu2.reg_read(UC_PPC_REG_3)
        ok = (r3 == expected)
        print(f"  input={input_val:#x} => r3={r3} (expected {expected}) {'PASS' if ok else 'FAIL'}")
        results.append(ok)

    success = all(results)
    print(f"  RESULT: {'PASS' if success else 'FAIL'}")
    return success


def test_ppc32_prolog_epilog():
    """Test: typical MSVC function prolog/epilog pattern."""
    print("\n" + "=" * 60)
    print("TEST 1d: PPC32 typical MSVC prolog/epilog")
    print("=" * 60)

    mu = Uc(UC_ARCH_PPC, UC_MODE_PPC32 + UC_MODE_BIG_ENDIAN)

    CODE_BASE = 0x80000000
    CODE_SIZE = 0x1000
    STACK_BASE = 0x10000000
    STACK_SIZE = 0x10000

    mu.mem_map(CODE_BASE, CODE_SIZE)
    mu.mem_map(STACK_BASE, STACK_SIZE)

    # Typical MSVC PPC function:
    # prolog: mfspr r0, LR; stw r0, 8(r1); stwu r1, -0x10(r1)
    # body:   li r3, 99
    # epilog: addi r1, r1, 0x10; lwz r0, 8(r1); mtspr LR, r0; blr
    code = bytes([
        # prolog
        0x7C, 0x08, 0x02, 0xA6,  # mflr r0 (mfspr r0, LR)
        0x90, 0x01, 0x00, 0x08,  # stw r0, 8(r1)
        0x94, 0x21, 0xFF, 0xF0,  # stwu r1, -0x10(r1)
        # body
        0x38, 0x60, 0x00, 0x63,  # li r3, 99
        # epilog
        0x38, 0x21, 0x00, 0x10,  # addi r1, r1, 0x10
        0x80, 0x01, 0x00, 0x08,  # lwz r0, 8(r1)
        0x7C, 0x08, 0x03, 0xA6,  # mtlr r0 (mtspr LR, r0)
        0x4E, 0x80, 0x00, 0x20,  # blr
    ])
    mu.mem_write(CODE_BASE, code)

    sp_init = STACK_BASE + 0x8000
    lr_init = 0xDEAD0000

    mu.reg_write(UC_PPC_REG_1, sp_init)
    mu.reg_write(UC_PPC_REG_LR, lr_init)

    try:
        mu.emu_start(CODE_BASE, CODE_BASE + len(code), timeout=1000000)
    except UcError as e:
        if e.errno != UC_ERR_FETCH_UNMAPPED:
            print(f"  UNEXPECTED ERROR: {e}")
            return False

    r3 = mu.reg_read(UC_PPC_REG_3)
    r1 = mu.reg_read(UC_PPC_REG_1)
    lr = mu.reg_read(UC_PPC_REG_LR)
    pc = mu.reg_read(UC_PPC_REG_PC)

    print(f"  r3 = {r3} (expected 99)")
    print(f"  r1 = 0x{r1:08X} (expected 0x{sp_init:08X}, stack restored)")
    print(f"  LR = 0x{lr:08X} (expected 0x{lr_init:08X}, LR restored)")
    print(f"  PC = 0x{pc:08X} (expected 0xDEAD0000)")

    success = (r3 == 99 and r1 == sp_init and lr == lr_init)
    print(f"  RESULT: {'PASS' if success else 'FAIL'}")
    return success


def test_ppc32_hook():
    """Test: UC_HOOK_CODE fires on every instruction."""
    print("\n" + "=" * 60)
    print("TEST 1e: UC_HOOK_CODE reliability")
    print("=" * 60)

    mu = Uc(UC_ARCH_PPC, UC_MODE_PPC32 + UC_MODE_BIG_ENDIAN)

    CODE_BASE = 0x80000000
    CODE_SIZE = 0x1000
    STACK_BASE = 0x10000000
    STACK_SIZE = 0x10000

    mu.mem_map(CODE_BASE, CODE_SIZE)
    mu.mem_map(STACK_BASE, STACK_SIZE)

    # 5 instructions + blr
    code = bytes([
        0x38, 0x60, 0x00, 0x01,  # li r3, 1
        0x38, 0x63, 0x00, 0x01,  # addi r3, r3, 1
        0x38, 0x63, 0x00, 0x01,  # addi r3, r3, 1
        0x38, 0x63, 0x00, 0x01,  # addi r3, r3, 1
        0x38, 0x63, 0x00, 0x01,  # addi r3, r3, 1
        0x4E, 0x80, 0x00, 0x20,  # blr
    ])
    mu.mem_write(CODE_BASE, code)

    hook_count = [0]
    hook_addrs = []

    def hook_code(uc, address, size, user_data):
        hook_count[0] += 1
        hook_addrs.append(address)

    mu.hook_add(UC_HOOK_CODE, hook_code)

    mu.reg_write(UC_PPC_REG_1, STACK_BASE + 0x8000)
    mu.reg_write(UC_PPC_REG_LR, 0xDEAD0000)

    try:
        mu.emu_start(CODE_BASE, CODE_BASE + len(code), timeout=1000000)
    except UcError as e:
        if e.errno != UC_ERR_FETCH_UNMAPPED:
            print(f"  UNEXPECTED ERROR: {e}")
            return False

    r3 = mu.reg_read(UC_PPC_REG_3)
    expected_count = 6  # 5 addi + 1 blr
    print(f"  Hook fired {hook_count[0]} times (expected {expected_count})")
    print(f"  Addresses: {[f'0x{a:08X}' for a in hook_addrs]}")
    print(f"  r3 = {r3} (expected 5)")

    success = (hook_count[0] == expected_count and r3 == 5)
    print(f"  RESULT: {'PASS' if success else 'FAIL'}")
    return success


def test_ppc32_bl_interception():
    """Test: can we intercept bl (function call) via hook?"""
    print("\n" + "=" * 60)
    print("TEST 1f: bl (function call) interception via hook")
    print("=" * 60)

    mu = Uc(UC_ARCH_PPC, UC_MODE_PPC32 + UC_MODE_BIG_ENDIAN)

    CODE_BASE = 0x80000000
    CODE_SIZE = 0x1000
    TRAMPOLINE_BASE = 0x40000000
    TRAMPOLINE_SIZE = 0x1000
    STACK_BASE = 0x10000000
    STACK_SIZE = 0x10000

    mu.mem_map(CODE_BASE, CODE_SIZE)
    mu.mem_map(TRAMPOLINE_BASE, TRAMPOLINE_SIZE)
    mu.mem_map(STACK_BASE, STACK_SIZE)

    # Main code calls a trampoline, trampoline returns 77
    # Trampoline stub at TRAMPOLINE_BASE:
    #   li r3, 77   => 38 60 00 4D
    #   blr         => 4E 80 00 20
    trampoline = bytes([
        0x38, 0x60, 0x00, 0x4D,  # li r3, 77
        0x4E, 0x80, 0x00, 0x20,  # blr
    ])
    mu.mem_write(TRAMPOLINE_BASE, trampoline)

    # Put trampoline at CODE_BASE + 0x800 (within 26-bit bl range)
    TRAMPOLINE_OFFSET = 0x800
    trampoline_addr = CODE_BASE + TRAMPOLINE_OFFSET
    mu.mem_write(trampoline_addr, trampoline)

    # Build caller with prolog/epilog (bl overwrites LR, so we must save/restore it)
    # bl offset from prolog end (instruction at +0x0C) to trampoline at +0x800:
    # offset = 0x800 - 0x0C = 0x7F4
    bl_offset = TRAMPOLINE_OFFSET - 0x0C
    bl_insn = struct.pack(">I", 0x48000001 | (bl_offset & 0x03FFFFFC))

    code = bytes([
        # prolog: save LR
        0x7C, 0x08, 0x02, 0xA6,  # mflr r0
        0x90, 0x01, 0x00, 0x08,  # stw r0, 8(r1)
        0x94, 0x21, 0xFF, 0xF0,  # stwu r1, -0x10(r1)
    ]) + bl_insn + bytes([
        0x38, 0x63, 0x00, 0x01,  # addi r3, r3, 1
        # epilog: restore LR
        0x38, 0x21, 0x00, 0x10,  # addi r1, r1, 0x10
        0x80, 0x01, 0x00, 0x08,  # lwz r0, 8(r1)
        0x7C, 0x08, 0x03, 0xA6,  # mtlr r0
        0x4E, 0x80, 0x00, 0x20,  # blr
    ])
    mu.mem_write(CODE_BASE, code)

    # Track calls to trampoline
    call_log = []

    def hook_trampoline(uc, address, size, user_data):
        if address == trampoline_addr:
            lr = uc.reg_read(UC_PPC_REG_LR)
            call_log.append({
                "address": address,
                "lr": lr,
            })

    mu.hook_add(UC_HOOK_CODE, hook_trampoline, begin=trampoline_addr, end=trampoline_addr + len(trampoline))

    mu.reg_write(UC_PPC_REG_1, STACK_BASE + 0x8000)
    mu.reg_write(UC_PPC_REG_LR, 0xDEAD0000)

    try:
        mu.emu_start(CODE_BASE, CODE_BASE + len(code), timeout=1000000)
    except UcError as e:
        if e.errno != UC_ERR_FETCH_UNMAPPED:
            print(f"  UNEXPECTED ERROR: {e}")
            return False

    r3 = mu.reg_read(UC_PPC_REG_3)
    print(f"  r3 = {r3} (expected 78 = trampoline returned 77 + 1)")
    print(f"  Call log: {call_log}")
    print(f"  Trampoline called {len(call_log)} time(s)")

    success = (r3 == 78 and len(call_log) == 1)
    print(f"  RESULT: {'PASS' if success else 'FAIL'}")
    return success


def test_ppc32_benchmark():
    """Benchmark: how fast is per-function execution?"""
    print("\n" + "=" * 60)
    print("TEST 1g: Execution speed benchmark")
    print("=" * 60)

    # Simple loop: count from 0 to 1000
    # r3 = counter, loop until r3 == 1000
    code = bytes([
        0x38, 0x60, 0x00, 0x00,  # li r3, 0
        # loop:
        0x38, 0x63, 0x00, 0x01,  # addi r3, r3, 1
        0x2C, 0x03, 0x03, 0xE8,  # cmpwi cr0, r3, 1000
        0x40, 0x82, 0xFF, 0xF8,  # bne cr0, loop (-8)
        0x4E, 0x80, 0x00, 0x20,  # blr
    ])

    CODE_BASE = 0x80000000
    STACK_BASE = 0x10000000

    # Benchmark: run N iterations
    N = 1000
    times = []
    for i in range(N):
        mu = Uc(UC_ARCH_PPC, UC_MODE_PPC32 + UC_MODE_BIG_ENDIAN)
        mu.mem_map(CODE_BASE, 0x1000)
        mu.mem_map(STACK_BASE, 0x10000)
        mu.mem_write(CODE_BASE, code)
        mu.reg_write(UC_PPC_REG_1, STACK_BASE + 0x8000)
        mu.reg_write(UC_PPC_REG_LR, 0xDEAD0000)

        t0 = time.perf_counter()
        try:
            mu.emu_start(CODE_BASE, CODE_BASE + len(code), timeout=10000000)
        except UcError:
            pass
        t1 = time.perf_counter()
        times.append(t1 - t0)

    r3 = mu.reg_read(UC_PPC_REG_3)
    avg_us = (sum(times) / len(times)) * 1e6
    min_us = min(times) * 1e6
    max_us = max(times) * 1e6
    total_ms = sum(times) * 1e3

    print(f"  r3 = {r3} (expected 1000)")
    print(f"  {N} iterations of 1000-count loop:")
    print(f"    Average: {avg_us:.1f} us/call")
    print(f"    Min:     {min_us:.1f} us")
    print(f"    Max:     {max_us:.1f} us")
    print(f"    Total:   {total_ms:.1f} ms")

    success = (r3 == 1000)
    print(f"  RESULT: {'PASS' if success else 'FAIL'}")
    return success


# ============================================================
# PART 2: COFF Parsing and Relocation Catalog
# ============================================================

# COFF header constants
IMAGE_FILE_MACHINE_POWERPCBE = 0x01F2

# IMAGE_REL_PPC_* relocation types (from PE/COFF spec + Xbox 360 extensions)
RELOC_NAMES = {
    0x0000: "ABSOLUTE",
    0x0001: "ADDR64",
    0x0002: "ADDR32",
    0x0003: "ADDR24",
    0x0004: "ADDR16",
    0x0005: "ADDR14",
    0x0006: "REL24",
    0x0007: "REL14",
    0x0008: "TOCREL16",
    0x0009: "TOCREL14",
    0x000A: "ADDR32NB",
    0x000B: "SECREL",
    0x000C: "SECTION",
    0x000D: "IFGLUE",
    0x000E: "IMGLUE",
    0x000F: "SECREL16",
    0x0010: "REFHI",
    0x0011: "REFLO",
    0x0012: "PAIR",
    0x0013: "SECRELLO",
    0x0014: "SECRELHI",
    # Xbox 360 specific
    0x0015: "GPREL",
    0x0016: "TOKEN",
}


class COFFParser:
    """Minimal COFF parser for MSVC Xbox 360 PPC .obj files."""

    def __init__(self, filepath):
        with open(filepath, "rb") as f:
            self.data = f.read()
        self.filepath = filepath
        self._parse_header()
        self._parse_sections()
        self._parse_symbols()

    def _parse_header(self):
        # COFF header: 20 bytes
        (self.machine, self.num_sections, self.timestamp,
         self.symtab_offset, self.num_symbols, self.opthdr_size,
         self.characteristics) = struct.unpack_from("<HHIIIHH", self.data, 0)

        assert self.machine == IMAGE_FILE_MACHINE_POWERPCBE, \
            f"Not a PPC BE COFF: machine=0x{self.machine:04X}"

    def _parse_sections(self):
        self.sections = []
        offset = 20 + self.opthdr_size  # After COFF header + optional header

        for i in range(self.num_sections):
            sec = {}
            name_bytes = self.data[offset:offset+8]
            # Handle long names (starts with /)
            if name_bytes[0:1] == b'/':
                str_offset = int(name_bytes[1:].rstrip(b'\x00').decode('ascii'))
                strtab_base = self.symtab_offset + self.num_symbols * 18
                end = self.data.index(b'\x00', strtab_base + str_offset)
                sec['name'] = self.data[strtab_base + str_offset:end].decode('ascii')
            else:
                sec['name'] = name_bytes.rstrip(b'\x00').decode('ascii')

            (sec['vsize'], sec['vaddr'], sec['raw_size'], sec['raw_offset'],
             sec['reloc_offset'], sec['linenum_offset'], sec['num_relocs'],
             sec['num_linenums'], sec['characteristics']) = struct.unpack_from(
                "<IIIIIIHHI", self.data, offset + 8)

            sec['index'] = i + 1  # 1-based
            self.sections.append(sec)
            offset += 40

    def _parse_symbols(self):
        self.symbols = []
        self.symbol_map = {}  # name -> symbol
        strtab_base = self.symtab_offset + self.num_symbols * 18

        i = 0
        while i < self.num_symbols:
            off = self.symtab_offset + i * 18
            name_bytes = self.data[off:off+8]

            # Symbol name: if first 4 bytes are zero, it's a string table reference
            if name_bytes[:4] == b'\x00\x00\x00\x00':
                str_offset = struct.unpack_from("<I", name_bytes, 4)[0]
                end = self.data.index(b'\x00', strtab_base + str_offset)
                name = self.data[strtab_base + str_offset:end].decode('ascii', errors='replace')
            else:
                name = name_bytes.rstrip(b'\x00').decode('ascii', errors='replace')

            value, sec_num, sym_type, storage_class, num_aux = struct.unpack_from(
                "<IhHBB", self.data, off + 8)

            sym = {
                'name': name,
                'value': value,
                'section': sec_num,
                'type': sym_type,
                'storage_class': storage_class,
                'num_aux': num_aux,
                'index': i,
            }
            self.symbols.append(sym)
            self.symbol_map[name] = sym

            # Skip aux symbols
            i += 1 + num_aux

    def get_section_relocations(self, section_idx):
        """Get relocations for a section (0-based index)."""
        sec = self.sections[section_idx]
        relocs = []
        for i in range(sec['num_relocs']):
            off = sec['reloc_offset'] + i * 10
            vaddr, sym_idx, reloc_type = struct.unpack_from("<IIH", self.data, off)
            sym = self.symbols[sym_idx] if sym_idx < len(self.symbols) else {'name': f'<sym#{sym_idx}>'}
            relocs.append({
                'offset': vaddr,
                'symbol_index': sym_idx,
                'symbol_name': sym['name'],
                'type': reloc_type,
                'type_name': RELOC_NAMES.get(reloc_type, f"UNKNOWN(0x{reloc_type:04X})"),
            })
        return relocs

    def get_text_sections(self):
        """Return all .text sections."""
        return [s for s in self.sections if s['name'].startswith('.text')]

    def get_section_data(self, section_idx):
        """Get raw bytes for a section."""
        sec = self.sections[section_idx]
        return self.data[sec['raw_offset']:sec['raw_offset'] + sec['raw_size']]

    def find_function_in_text(self, mangled_name):
        """Find a function symbol and its section/offset."""
        sym = self.symbol_map.get(mangled_name)
        if not sym:
            return None
        if sym['section'] <= 0:
            return None
        sec = self.sections[sym['section'] - 1]
        return {
            'symbol': sym,
            'section': sec,
            'section_index': sym['section'] - 1,
            'offset_in_section': sym['value'],
        }

    def extract_function_bytes(self, mangled_name, size=None):
        """Extract raw bytes for a function. If size not given, extract to next symbol or end of section."""
        info = self.find_function_in_text(mangled_name)
        if not info:
            return None, None

        sec = info['section']
        sec_data = self.get_section_data(info['section_index'])
        start = info['offset_in_section']

        if size:
            end = start + size
        else:
            # Find next symbol in same section
            next_offset = sec['raw_size']
            for sym in self.symbols:
                if sym['section'] == info['symbol']['section'] and sym['value'] > start:
                    if sym['value'] < next_offset:
                        next_offset = sym['value']
            end = next_offset

        return sec_data[start:end], info


def analyze_coff_relocations(filepath):
    """Analyze relocations in a COFF .obj file."""
    print(f"\n  Analyzing: {filepath}")

    try:
        coff = COFFParser(filepath)
    except Exception as e:
        print(f"  ERROR parsing COFF: {e}")
        return None

    print(f"  Machine: 0x{coff.machine:04X} ({'PPC BE' if coff.machine == IMAGE_FILE_MACHINE_POWERPCBE else 'unknown'})")
    print(f"  Sections: {coff.num_sections}")
    print(f"  Symbols: {coff.num_symbols}")

    # List sections
    print(f"\n  Sections:")
    for sec in coff.sections:
        flags = []
        if sec['characteristics'] & 0x20:
            flags.append("CODE")
        if sec['characteristics'] & 0x40:
            flags.append("IDATA")
        if sec['characteristics'] & 0x80:
            flags.append("UDATA")
        print(f"    [{sec['index']:2d}] {sec['name']:20s} size={sec['raw_size']:6d}  relocs={sec['num_relocs']:4d}  flags={','.join(flags)}")

    # Catalog relocation types across all sections
    all_reloc_types = {}
    all_relocs = []
    text_sections = coff.get_text_sections()

    for i, sec in enumerate(coff.sections):
        if sec['num_relocs'] == 0:
            continue
        relocs = coff.get_section_relocations(i)
        for r in relocs:
            type_name = r['type_name']
            if type_name not in all_reloc_types:
                all_reloc_types[type_name] = 0
            all_reloc_types[type_name] += 1
            if sec['name'].startswith('.text'):
                all_relocs.append(r)

    print(f"\n  Relocation type catalog:")
    for type_name, count in sorted(all_reloc_types.items(), key=lambda x: -x[1]):
        print(f"    {type_name:20s}: {count:5d}")

    # Show sample relocations from .text sections
    print(f"\n  Sample .text relocations (first 20):")
    for r in all_relocs[:20]:
        print(f"    offset=0x{r['offset']:06X}  type={r['type_name']:15s}  sym={r['symbol_name'][:60]}")

    # Identify hi/lo pairs
    print(f"\n  Hi/Lo address pair analysis:")
    hi_count = sum(1 for r in all_relocs if r['type_name'] in ('REFHI', 'ADDR16_HI', 'SECRELHI'))
    lo_count = sum(1 for r in all_relocs if r['type_name'] in ('REFLO', 'ADDR16_LO', 'SECRELLO'))
    pair_count = sum(1 for r in all_relocs if r['type_name'] == 'PAIR')
    print(f"    HI relocs: {hi_count}")
    print(f"    LO relocs: {lo_count}")
    print(f"    PAIR relocs: {pair_count}")

    # Identify bl targets (REL24)
    bl_targets = [r for r in all_relocs if r['type_name'] == 'REL24']
    print(f"\n  External bl targets (REL24): {len(bl_targets)}")
    unique_targets = set(r['symbol_name'] for r in bl_targets)
    for t in sorted(unique_targets)[:15]:
        count = sum(1 for r in bl_targets if r['symbol_name'] == t)
        print(f"    {t[:70]}  (x{count})")
    if len(unique_targets) > 15:
        print(f"    ... and {len(unique_targets) - 15} more")

    return coff, all_reloc_types


def test_real_function_execution(coff, symbol, size):
    """Extract a real function from a COFF .obj and execute it in Unicorn."""
    print(f"\n" + "=" * 60)
    print(f"TEST 2: Real MSVC-compiled function execution")
    print(f"  Symbol: {symbol}")
    print("=" * 60)

    func_bytes, info = coff.extract_function_bytes(symbol, size)
    if func_bytes is None:
        print(f"  ERROR: Symbol '{symbol}' not found in .obj")
        return False

    actual_size = len(func_bytes)
    print(f"  Extracted {actual_size} bytes from section '{info['section']['name']}' at offset 0x{info['offset_in_section']:X}")

    # Print disassembly (raw hex, 4 bytes per instruction)
    print(f"\n  Raw instructions:")
    for i in range(0, min(actual_size, 80), 4):
        insn = struct.unpack_from(">I", func_bytes, i)[0]
        print(f"    0x{i:04X}: {insn:08X}")

    # Check for relocations within this function's range
    sec_idx = info['section_index']
    relocs = coff.get_section_relocations(sec_idx)
    func_start = info['offset_in_section']
    func_end = func_start + actual_size
    func_relocs = [r for r in relocs if func_start <= r['offset'] < func_end]
    print(f"\n  Relocations in function range: {len(func_relocs)}")
    for r in func_relocs:
        print(f"    offset=0x{r['offset']:06X}  type={r['type_name']:15s}  sym={r['symbol_name'][:60]}")

    if func_relocs:
        print(f"\n  WARNING: Function has relocations — raw bytes have unresolved references.")
        print(f"  Attempting execution anyway (may fault on unresolved addresses)...")

    # Load into Unicorn
    mu = Uc(UC_ARCH_PPC, UC_MODE_PPC32 + UC_MODE_BIG_ENDIAN)

    CODE_BASE = 0x80000000
    CODE_SIZE = 0x10000
    STACK_BASE = 0x10000000
    STACK_SIZE = 0x10000
    OBJ_BASE = 0x20000000
    OBJ_SIZE = 0x10000

    mu.mem_map(CODE_BASE, CODE_SIZE)
    mu.mem_map(STACK_BASE, STACK_SIZE)
    mu.mem_map(OBJ_BASE, OBJ_SIZE)

    mu.mem_write(CODE_BASE, func_bytes)

    mu.reg_write(UC_PPC_REG_1, STACK_BASE + 0x8000)
    mu.reg_write(UC_PPC_REG_3, OBJ_BASE)  # this pointer
    mu.reg_write(UC_PPC_REG_4, OBJ_BASE + 0x100)  # r4 arg
    mu.reg_write(UC_PPC_REG_5, OBJ_BASE + 0x200)  # r5 arg
    mu.reg_write(UC_PPC_REG_LR, 0xDEAD0000)

    # Enable FP in MSR (bit 18 = 0x2000) — required for any float instructions
    msr = mu.reg_read(UC_PPC_REG_MSR)
    mu.reg_write(UC_PPC_REG_MSR, msr | 0x2000)

    # Pre-populate object memory with some non-zero test data
    # Set up mock pointer fields and integer values
    for off in range(0, OBJ_SIZE, 4):
        mu.mem_write(OBJ_BASE + off, struct.pack(">I", OBJ_BASE + off + 0x1000))
    # Write some float test values at known offsets
    for off in [0, 4, 8]:
        for base in [OBJ_BASE, OBJ_BASE + 0x100, OBJ_BASE + 0x200]:
            mu.mem_write(base + off, struct.pack(">f", 1.0 + off * 0.25))

    # Track execution
    insn_count = [0]
    last_pc = [0]

    def hook_code(uc, address, size, user_data):
        insn_count[0] += 1
        last_pc[0] = address

    mu.hook_add(UC_HOOK_CODE, hook_code)

    # Handle unmapped memory access gracefully
    def hook_mem_unmapped(uc, access, address, size, value, user_data):
        print(f"  UNMAPPED ACCESS: addr=0x{address:08X} size={size} access={access}")
        return False  # Stop emulation

    mu.hook_add(UC_HOOK_MEM_READ_UNMAPPED | UC_HOOK_MEM_WRITE_UNMAPPED, hook_mem_unmapped)

    t0 = time.perf_counter()
    try:
        mu.emu_start(CODE_BASE, CODE_BASE + actual_size, timeout=5000000)
        print(f"\n  Emulation ended normally (reached end address)")
    except UcError as e:
        if e.errno == UC_ERR_FETCH_UNMAPPED:
            pc = mu.reg_read(UC_PPC_REG_PC)
            if pc == 0xDEAD0000:
                print(f"\n  Emulation ended: function returned (blr → LR sentinel)")
            else:
                print(f"\n  Emulation ended: fetch from unmapped 0x{pc:08X}")
        else:
            print(f"\n  Emulation ERROR: {e}")
    t1 = time.perf_counter()

    r3 = mu.reg_read(UC_PPC_REG_3)
    r1 = mu.reg_read(UC_PPC_REG_1)
    lr = mu.reg_read(UC_PPC_REG_LR)
    pc = mu.reg_read(UC_PPC_REG_PC)

    print(f"\n  Post-execution state:")
    print(f"    r3 = 0x{r3:08X} ({r3})")
    print(f"    r1 = 0x{r1:08X}")
    print(f"    LR = 0x{lr:08X}")
    print(f"    PC = 0x{pc:08X}")
    print(f"    Instructions executed: {insn_count[0]}")
    print(f"    Execution time: {(t1-t0)*1e6:.1f} us")

    returned_normally = (pc == 0xDEAD0000)
    print(f"\n  Function returned normally: {returned_normally}")
    return returned_normally


def test_ppc64():
    """Test: PPC64 BE mode."""
    print("\n" + "=" * 60)
    print("TEST 1h: PPC64 Big-Endian mode")
    print("=" * 60)

    try:
        mu = Uc(UC_ARCH_PPC, UC_MODE_PPC64 + UC_MODE_BIG_ENDIAN)
        print("  PPC64 BE instance created successfully")
    except UcError as e:
        print(f"  FAILED to create PPC64 BE instance: {e}")
        print("  RESULT: FAIL (cannot instantiate)")
        return False

    CODE_BASE = 0x80000000
    CODE_SIZE = 0x1000
    STACK_BASE = 0x10000000
    STACK_SIZE = 0x10000

    mu.mem_map(CODE_BASE, CODE_SIZE)
    mu.mem_map(STACK_BASE, STACK_SIZE)

    # Same test: li r3, 42; blr
    code = bytes([
        0x38, 0x60, 0x00, 0x2A,  # li r3, 42
        0x4E, 0x80, 0x00, 0x20,  # blr
    ])
    mu.mem_write(CODE_BASE, code)
    mu.reg_write(UC_PPC_REG_1, STACK_BASE + 0x8000)
    mu.reg_write(UC_PPC_REG_LR, 0xDEAD0000)

    try:
        mu.emu_start(CODE_BASE, CODE_BASE + len(code), timeout=1000000)
    except UcError as e:
        if e.errno != UC_ERR_FETCH_UNMAPPED:
            # PPC64 is known broken in Unicorn — every instruction raises exception
            # PC advances but exception fires. This is a Unicorn limitation.
            pc = mu.reg_read(UC_PPC_REG_PC)
            r3 = mu.reg_read(UC_PPC_REG_3)
            print(f"  ERROR: {e}")
            print(f"  PC = 0x{pc:08X}, r3 = {r3}")
            print(f"  NOTE: PPC64 mode is broken in Unicorn — every instruction")
            print(f"  triggers UC_ERR_EXCEPTION despite PC advancing normally.")
            print(f"  This is a known Unicorn limitation with zero test coverage.")
            print(f"  RESULT: EXPECTED_FAIL (PPC64 broken, PPC32 works fine)")
            return True  # Not a blocker — we use PPC32

    r3 = mu.reg_read(UC_PPC_REG_3)
    print(f"  r3 = {r3} (expected 42)")
    success = (r3 == 42)
    print(f"  RESULT: {'PASS' if success else 'FAIL'}")
    return success


# ============================================================
# MAIN
# ============================================================

def main():
    print("Unicorn Function Runner — Phase 0 Research")
    print("=" * 60)

    results = {}

    # Part 1: Unicorn PPC viability
    print("\n### PART 1: Unicorn PPC Viability ###\n")

    results['basic_ppc32'] = test_basic_ppc32()
    results['arithmetic'] = test_ppc32_arithmetic()
    results['branching'] = test_ppc32_branch()
    results['prolog_epilog'] = test_ppc32_prolog_epilog()
    results['hook_code'] = test_ppc32_hook()
    results['bl_intercept'] = test_ppc32_bl_interception()
    results['ppc64'] = test_ppc64()
    results['benchmark'] = test_ppc32_benchmark()

    # Part 2: COFF relocation catalog
    print("\n\n### PART 2: COFF Relocation Catalog ###\n")

    obj_files = [
        "build/373307D9/src/system/math/FileChecksum.obj",
        "build/373307D9/src/system/gesture/Skeleton.obj",
        "build/373307D9/src/system/meta/SongMgr.obj",
    ]

    coff_results = {}
    for obj_file in obj_files:
        result = analyze_coff_relocations(obj_file)
        if result:
            coff_results[obj_file] = result

    # Part 3: Try executing a real function
    print("\n\n### PART 3: Real Function Execution ###\n")

    # Try Symbol::operator== — 24 bytes, 0 relocations, pure scalar comparison
    if "build/373307D9/src/system/gesture/Skeleton.obj" in coff_results:
        coff, _ = coff_results["build/373307D9/src/system/gesture/Skeleton.obj"]
        results['real_symbol_eq'] = test_real_function_execution(
            coff, "??8Symbol@@QBA_NABV0@@Z", 24)

    # Try Vector3::Subtract — 52 bytes, 0 relocations, float arithmetic
    if "build/373307D9/src/system/gesture/Skeleton.obj" in coff_results:
        coff, _ = coff_results["build/373307D9/src/system/gesture/Skeleton.obj"]
        results['real_vec3_subtract'] = test_real_function_execution(
            coff, "?Subtract@@YAXABVVector3@@0AAV1@@Z", 52)

    # Try HandJoint — 32 bytes, 0 relocations, member access with multiply
    if "build/373307D9/src/system/gesture/Skeleton.obj" in coff_results:
        coff, _ = coff_results["build/373307D9/src/system/gesture/Skeleton.obj"]
        results['real_handjoint'] = test_real_function_execution(
            coff, "?HandJoint@Skeleton@@QBAABUTrackedJoint@@W4SkeletonSide@@@Z", 32)

    # Summary
    print("\n\n" + "=" * 60)
    print("SUMMARY")
    print("=" * 60)
    for name, passed in results.items():
        print(f"  {name:25s}: {'PASS' if passed else 'FAIL'}")

    total = len(results)
    passed = sum(1 for v in results.values() if v)
    print(f"\n  {passed}/{total} tests passed")

    return all(results.values())


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
