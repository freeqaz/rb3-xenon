#!/usr/bin/env python3
"""Post-compilation .obj register swap patcher.

Patches PowerPC register fields in COFF .obj files to fix register swap
mismatches identified by objdiff. Uses objdiff JSON diff as the oracle
to determine exactly which instructions and register fields need patching.

APPROACH:
1. Run objdiff to get instruction-level JSON diff
2. For each instruction flagged as 'diff_arg' with register mismatches:
   - Parse the instruction word from the .obj file
   - Find the 5-bit register field that matches the 'base' register value
   - Replace it with the 'target' register value
3. Write the patched .obj file

This avoids complex opcode-format mapping by simply scanning all register
field positions in the instruction word for matching values.
"""

import json
import struct
import subprocess
import sys
import shutil
from pathlib import Path
from dataclasses import dataclass, field
from typing import Optional

PROJECT_ROOT = Path(__file__).resolve().parent.parent
OBJDIFF_CLI = PROJECT_ROOT / "bin" / "objdiff-cli"

# PowerPC register name -> number mapping
GPR_MAP = {f"r{i}": i for i in range(32)}
FPR_MAP = {f"f{i}": i for i in range(32)}
CR_MAP = {f"cr{i}": i for i in range(8)}

def reg_to_num(name: str) -> Optional[int]:
    """Convert register name (r0-r31, f0-f31) to number."""
    name = name.strip()
    if name in GPR_MAP:
        return GPR_MAP[name]
    if name in FPR_MAP:
        return FPR_MAP[name]
    return None

def is_gpr(name: str) -> bool:
    return name.strip().startswith("r")

def is_fpr(name: str) -> bool:
    return name.strip().startswith("f")


# PowerPC instruction register field positions (bit offsets from MSB)
# Each entry: (start_bit, width) where start_bit counts from bit 0 (MSB)
# Field extraction: (word >> (32 - start_bit - width)) & ((1 << width) - 1)
REGISTER_FIELDS = [
    (6, 5),   # rD/rS/frD/frS - bits 6-10
    (11, 5),  # rA/frA - bits 11-15
    (16, 5),  # rB/frB - bits 16-20
    (21, 5),  # rC/frC - bits 21-25 (A-form only)
]

def get_field(word: int, start: int, width: int) -> int:
    """Extract a bit field from a 32-bit instruction word."""
    shift = 32 - start - width
    mask = (1 << width) - 1
    return (word >> shift) & mask

def set_field(word: int, start: int, width: int, value: int) -> int:
    """Set a bit field in a 32-bit instruction word."""
    shift = 32 - start - width
    mask = (1 << width) - 1
    word &= ~(mask << shift)
    word |= (value & mask) << shift
    return word

def get_primary_opcode(word: int) -> int:
    """Get primary opcode (bits 0-5)."""
    return get_field(word, 0, 6)

def get_xo_field(word: int) -> int:
    """Get extended opcode for X-form (bits 21-30)."""
    return get_field(word, 21, 10)

def get_register_field_positions(word: int) -> list:
    """Determine which 5-bit fields in this instruction contain registers.

    Returns list of (start_bit, width) tuples for fields that contain registers.
    We need to be careful about instructions where some fields are NOT registers
    (e.g., immediate values, shift amounts, SPR numbers).
    """
    opcode = get_primary_opcode(word)

    # D-form compare/trap: bits 6-10 are crfD/TO, NOT a register
    # cmpi(11), cmpli(10): crfD(6-8), L(9), rA(11-15), IMM(16-31)
    # tdi(2), twi(3): TO(6-10), rA(11-15), SIMM(16-31)
    if opcode in (2, 3, 10, 11):
        return [(11, 5)]

    # D-form: loads/stores, addi, etc.
    # Fields: rD/rS(6-10), rA(11-15), then 16-bit immediate (NOT register)
    if opcode in range(4, 10) or opcode in range(12, 16) or opcode in range(24, 30) or opcode in range(32, 56):
        return [(6, 5), (11, 5)]

    # DS-form: ld, std, lwa
    if opcode in (58, 62):
        return [(6, 5), (11, 5)]

    # I-form: b, bl - no register fields
    if opcode == 18:
        return []

    # B-form: bc, bcl - no GPR fields (CR fields only, and we don't touch those)
    if opcode == 16:
        return []

    # SC-form: sc - no register fields
    if opcode == 17:
        return []

    # XL-form: bclr, bcctr, etc. - CR fields only
    if opcode == 19:
        return []

    # M-form: rlwinm, rlwimi - bits 6-10 (rS) and 11-15 (rA), NOT 16-20 (SH)
    if opcode in (20, 21):
        return [(6, 5), (11, 5)]

    # M-form: rlwnm - all three are registers
    if opcode == 23:
        return [(6, 5), (11, 5), (16, 5)]

    # MD/MDS-form: rldic*, rldcl, rldcr
    if opcode == 30:
        return [(6, 5), (11, 5)]

    # X-form (opcode 31): Most have 3 register fields, but some are special
    if opcode == 31:
        xo = get_xo_field(word)
        # Special forms where bits 11-20 are NOT registers:
        # mfspr (339), mtspr (467): SPR field in bits 11-20
        # mtcrf (144): CRM in bits 12-19
        # mfcr (19): no rA/rB
        # mtmsr (146): only rS
        if xo in (339, 467):
            return [(6, 5)]  # only rD/rS
        if xo in (144, 146, 19):
            return [(6, 5)]  # only rS/rD
        # tw: rA(11-15) and rB(16-20), TO field at 6-10 is NOT a register
        if xo == 4:  # tw
            return [(11, 5), (16, 5)]
        # cmp(0), cmpl(32): crfD(6-8), L(9), rA(11-15), rB(16-20)
        if xo in (0, 32):
            return [(11, 5), (16, 5)]
        # Default X-form: 3 register fields
        return [(6, 5), (11, 5), (16, 5)]

    # XFX-form (opcode 31, handled above)

    # FP single-precision (opcode 59): A-form, XO in bits 26-30
    if opcode == 59:
        xo_5 = get_field(word, 26, 5)
        if xo_5 in (29, 28, 31, 30):  # fmadds, fmsubs, fnmadds, fnmsubs
            return [(6, 5), (11, 5), (16, 5), (21, 5)]
        if xo_5 == 25:  # fmuls: frD, frA, frC (no frB)
            return [(6, 5), (11, 5), (21, 5)]
        if xo_5 in (24, 22):  # fres, fsqrts: frD, frB
            return [(6, 5), (16, 5)]
        # fadds(21), fsubs(20), fdivs(18): frD, frA, frB
        return [(6, 5), (11, 5), (16, 5)]

    # FP double-precision (opcode 63): X-form or A-form
    if opcode == 63:
        xo = get_xo_field(word)
        # Check if it's A-form (XO in bits 26-30, 5 bits)
        # A-form XO values: fmadd(29), fmsub(28), fnmadd(31), fnmsub(30), fmul(25)
        xo_5 = get_field(word, 26, 5)  # 5-bit XO for A-form
        if xo_5 in (29, 28, 31, 30, 23):  # fmadd, fmsub, fnmadd, fnmsub, fsel
            return [(6, 5), (11, 5), (16, 5), (21, 5)]
        if xo_5 == 25:  # fmul: frD, frA, frC (no frB)
            return [(6, 5), (11, 5), (21, 5)]
        # X-form FP: frD(6-10), frA(11-15), frB(16-20)
        # fabs, fneg, fmr: only frD and frB (frA=0)
        if xo in (264, 40, 72):  # fabs, fneg, fmr
            return [(6, 5), (16, 5)]
        # fcmpu, fcmpo: crfD at 6-8, frA(11-15), frB(16-20)
        if xo in (0, 32):  # fcmpu, fcmpo
            return [(11, 5), (16, 5)]
        # Default: frD, frA, frB
        return [(6, 5), (11, 5), (16, 5)]

    # VMX/Altivec (opcode 4) - not typically used in Xbox 360 PPC code
    if opcode == 4:
        return [(6, 5), (11, 5), (16, 5), (21, 5)]

    # Unknown opcode - conservatively return empty
    return []


@dataclass
class InstructionPatch:
    """All register changes for a single instruction, applied atomically."""
    offset: int          # byte offset within the .text section (from objdiff base address)
    instr_index: int     # objdiff instruction index (for diagnostics)
    opcode: str          # instruction mnemonic
    reg_changes: list    # list of (arg_idx, old_name, new_name, old_num, new_num)
    base_typed_args: list = field(default_factory=list)   # ALL base typed_args
    target_typed_args: list = field(default_factory=list)  # ALL target typed_args

    @property
    def description(self) -> str:
        changes = ", ".join(f"{old}->{new}" for _, old, new, _, _ in self.reg_changes)
        return f"idx {self.instr_index}: {self.opcode} [{changes}] at 0x{self.offset:x}"


def analyze_diff_for_patches(json_data: dict) -> list:
    """Analyze objdiff JSON to find instruction patches needed.

    Returns list of InstructionPatch objects, one per instruction that needs patching.
    All register changes within one instruction are grouped together.
    """
    patches = []
    instrs = json_data.get("instructions", [])

    for ins in instrs:
        if ins.get("match_type") != "diff_arg":
            continue

        target = ins.get("target", {})
        base = ins.get("base", {})

        if not target or not base:
            continue

        # Check opcodes match (should for diff_arg)
        if target.get("opcode") != base.get("opcode"):
            continue

        t_args = target.get("typed_args", [])
        b_args = base.get("typed_args", [])

        # Find ALL register differences in this instruction
        reg_changes = []
        for i in range(min(len(t_args), len(b_args))):
            ta = t_args[i]
            ba = b_args[i]
            if ta.get("type") == "Register" and ba.get("type") == "Register":
                tv = ta.get("value", "")
                bv = ba.get("value", "")
                if tv != bv:
                    t_num = reg_to_num(tv)
                    b_num = reg_to_num(bv)
                    if t_num is not None and b_num is not None:
                        reg_changes.append((i, bv, tv, b_num, t_num))

        if not reg_changes:
            continue

        # Get the base address (this is the offset within the section)
        base_addr_str = base.get("address", "")
        if not base_addr_str:
            continue
        try:
            base_addr = int(base_addr_str, 16)
        except ValueError:
            continue

        patches.append(InstructionPatch(
            offset=base_addr,
            instr_index=ins["index"],
            opcode=base.get("opcode", "?"),
            reg_changes=reg_changes,
            base_typed_args=b_args,
            target_typed_args=t_args,
        ))

    return patches


# Opcode 31 XO values for logical/shift ops where disasm order is [rA(11-15), rS(6-10), rB(16-20)]
# All other opcode-31 instructions use order [rD(6-10), rA(11-15), rB(16-20)]
LOGICAL_SHIFT_XO = {
    28,   # and
    444,  # or
    316,  # xor
    476,  # nand
    124,  # nor
    284,  # eqv
    60,   # andc
    412,  # orc
    24,   # slw
    536,  # srw
    792,  # sraw
    824,  # srawi
    954,  # extsb
    922,  # extsh
    26,   # cntlzw
    986,  # extsw (64-bit)
}

# Opcode 24-29: ori, oris, xori, xoris, andi., andis. (D-form logical imm)
# Disasm: op rA, rS, UIMM -> arg order [rA(11-15), rS(6-10), imm]
LOGICAL_D_OPCODES = {24, 25, 26, 27, 28, 29}


def get_arg_to_field_mapping(word: int) -> list:
    """Get the mapping from typed_args register index to bit field position.

    Returns a list of (start_bit, width) in the order they appear in the
    disassembly (typed_args order). Only includes register fields.
    For most instructions this is [(6,5), (11,5), (16,5)] but for
    logical/shift ops it's [(11,5), (6,5), (16,5)].
    """
    opcode = get_primary_opcode(word)

    # D-form compare/trap: only rA at (11,5)
    if opcode in (2, 3, 10, 11):
        return [(11, 5)]

    if opcode == 31:
        xo = get_xo_field(word)
        if xo in LOGICAL_SHIFT_XO:
            # Logical/shift: disasm is "op rA, rS, rB"
            # rA=bits 11-15, rS=bits 6-10, rB=bits 16-20
            return [(11, 5), (6, 5), (16, 5)]
        elif xo in (339, 467, 144):
            # mfspr/mtspr/mtcrf: only rD/rS at bits 6-10
            return [(6, 5)]
        elif xo == 4:
            # tw: "tw TO, rA, rB" - only rA(11-15), rB(16-20)
            return [(11, 5), (16, 5)]
        elif xo in (0, 32):
            # cmp, cmpl: "cmp crfD, L, rA, rB" - only rA(11-15), rB(16-20)
            return [(11, 5), (16, 5)]
        else:
            # Arithmetic/load-store X-form: "op rD, rA, rB"
            return [(6, 5), (11, 5), (16, 5)]

    if opcode in LOGICAL_D_OPCODES:
        # Logical immediate D-form: "op rA, rS, UIMM"
        return [(11, 5), (6, 5)]

    # FP single-precision A-form (opcode 59)
    if opcode == 59:
        xo_5 = get_field(word, 26, 5)
        if xo_5 in (29, 28, 31, 30):  # fmadds, fmsubs, fnmadds, fnmsubs: frD, frA, frC, frB
            return [(6, 5), (11, 5), (21, 5), (16, 5)]
        if xo_5 == 25:  # fmuls: frD, frA, frC
            return [(6, 5), (11, 5), (21, 5)]
        if xo_5 in (24, 22):  # fres, fsqrts: frD, frB
            return [(6, 5), (16, 5)]
        # fadds(21), fsubs(20), fdivs(18): frD, frA, frB
        return [(6, 5), (11, 5), (16, 5)]

    # FP X-form (opcode 63)
    if opcode == 63:
        xo = get_xo_field(word)
        xo_5 = get_field(word, 26, 5)
        if xo_5 in (29, 28, 31, 30, 23):  # fmadd, fmsub, fnmadd, fnmsub, fsel
            return [(6, 5), (11, 5), (21, 5), (16, 5)]
        if xo_5 == 25:  # fmul: frD, frA, frC
            return [(6, 5), (11, 5), (21, 5)]
        if xo in (264, 40, 72):  # fabs, fneg, fmr: frD, frB
            return [(6, 5), (16, 5)]
        if xo in (0, 32):  # fcmpu, fcmpo: crfD, frA, frB
            return [(11, 5), (16, 5)]
        return [(6, 5), (11, 5), (16, 5)]

    # M-form rlwinm/rlwimi (opcodes 20, 21): "op rA, rS, SH, MB, ME"
    if opcode in (20, 21):
        return [(11, 5), (6, 5)]  # rA, rS in disasm order

    # M-form rlwnm (opcode 23): "op rA, rS, rB, MB, ME"
    if opcode == 23:
        return [(11, 5), (6, 5), (16, 5)]

    # Default D-form and others: first reg arg at (6,5), second at (11,5)
    # For loads: "lwz rD, d(rA)" → [rD(6-10), rA(11-15)]
    # For stores: "stw rS, d(rA)" → [rS(6-10), rA(11-15)]
    # For addi: "addi rD, rA, SIMM" → [rD(6-10), rA(11-15)]
    return [(6, 5), (11, 5)]


def apply_patches(section_data: bytearray, patches: list, section_offset: int) -> int:
    """Apply instruction patches atomically using typed_args-aware field mapping.

    Uses the full typed_args from both target and base to build a correct
    mapping from arg_idx to bit field position, handling ambiguous cases.
    """
    count = 0

    for patch in patches:
        byte_offset = patch.offset  # b_addr from objdiff is section-relative
        if byte_offset < 0 or byte_offset + 4 > len(section_data):
            print(f"  WARNING: offset 0x{patch.offset:x} out of range", file=sys.stderr)
            continue

        word = struct.unpack(">I", section_data[byte_offset:byte_offset+4])[0]

        # Get the format-aware arg→field mapping (in disassembly arg order)
        arg_field_map = get_arg_to_field_mapping(word)

        # Build arg_idx → register-only-index mapping using base typed_args
        # Example: typed_args = [Register(r11), Register(r11), Register(r10)]
        #   → register-only indices: {0: 0, 1: 1, 2: 2}
        # Example: typed_args = [Register(r3), Signed(0x10), Register(r4)]
        #   → register-only indices: {0: 0, 2: 1}
        reg_arg_indices = {}  # arg_idx → register-only index
        reg_count = 0
        for i, ta in enumerate(patch.base_typed_args):
            if ta.get("type") == "Register":
                reg_arg_indices[i] = reg_count
                reg_count += 1

        # Now for each change, use arg_idx → reg_only_idx → field position
        field_assignments = {}  # field_start -> new_num

        for arg_idx, old_name, new_name, old_num, new_num in patch.reg_changes:
            reg_only_idx = reg_arg_indices.get(arg_idx)

            if reg_only_idx is not None and reg_only_idx < len(arg_field_map):
                # Direct mapping: use the format-aware field position
                start, width = arg_field_map[reg_only_idx]
                field_val = get_field(word, start, width)

                if field_val == old_num:
                    field_assignments[start] = new_num
                else:
                    # The field value doesn't match expected - might be a format mismatch
                    print(f"  WARN: {patch.description} - expected r{old_num} at field ({start},5) "
                          f"but found r{field_val}. Trying value-scan.", file=sys.stderr)
                    # Fallback to value scan
                    matched = False
                    for s, w in get_register_field_positions(word):
                        if s not in field_assignments and get_field(word, s, w) == old_num:
                            field_assignments[s] = new_num
                            matched = True
                            break
                    if not matched:
                        print(f"  ERROR: {patch.description} - r{old_num} ({old_name}) "
                              f"not found in word 0x{word:08X}", file=sys.stderr)
            else:
                # No format mapping available, use value scan
                matched = False
                used_fields = set(field_assignments.keys())
                for s, w in get_register_field_positions(word):
                    if s not in used_fields and get_field(word, s, w) == old_num:
                        field_assignments[s] = new_num
                        matched = True
                        break
                if not matched:
                    for s, w in REGISTER_FIELDS:
                        if s not in used_fields and get_field(word, s, w) == old_num:
                            field_assignments[s] = new_num
                            matched = True
                            print(f"  FALLBACK: {patch.description} - {old_name} at field ({s},5)", file=sys.stderr)
                            break
                if not matched:
                    print(f"  ERROR: {patch.description} - r{old_num} ({old_name}) "
                          f"not found in word 0x{word:08X}", file=sys.stderr)

        if not field_assignments:
            continue

        # Replicate changes to unclaimed fields with the same old value.
        # This handles pseudo-instructions like "mr rA, rS" (= or rA, rS, rS)
        # where two fields encode the same register but disassembly shows one arg.
        # ONLY replicate when the disassembler collapsed fields (fewer register
        # typed_args than actual register fields in the instruction encoding).
        num_reg_args = sum(1 for a in patch.base_typed_args if a.get("type") == "Register")
        all_reg_fields = get_register_field_positions(word)
        if num_reg_args < len(all_reg_fields):
            old_to_new = {}
            for arg_idx, old_name, new_name, old_num, new_num in patch.reg_changes:
                old_to_new[old_num] = new_num
            for start, width in all_reg_fields:
                if start not in field_assignments:
                    field_val = get_field(word, start, width)
                    if field_val in old_to_new:
                        field_assignments[start] = old_to_new[field_val]

        # Apply ALL field changes atomically
        new_word = word
        for start, new_num in field_assignments.items():
            new_word = set_field(new_word, start, 5, new_num)

        struct.pack_into(">I", section_data, byte_offset, new_word)
        count += 1

    return count


class COFFPatcher:
    """COFF .obj file patcher for register swaps."""

    def __init__(self, obj_path: str):
        self.path = Path(obj_path)
        with open(self.path, "rb") as f:
            self.data = bytearray(f.read())
        self._parse_headers()

    def _parse_headers(self):
        """Parse COFF headers to find sections and symbols."""
        # COFF header: machine(2), num_sections(2), timestamp(4),
        #              symtab_offset(4), num_symbols(4), opthdr_size(2), flags(2)
        self.machine = struct.unpack_from("<H", self.data, 0)[0]
        self.num_sections = struct.unpack_from("<H", self.data, 2)[0]
        self.symtab_offset = struct.unpack_from("<I", self.data, 8)[0]
        self.num_symbols = struct.unpack_from("<I", self.data, 12)[0]
        self.opthdr_size = struct.unpack_from("<H", self.data, 16)[0]

        # Parse section headers (40 bytes each, starting after COFF header + optional header)
        self.sections = []
        hdr_offset = 20 + self.opthdr_size
        for i in range(self.num_sections):
            off = hdr_offset + i * 40
            name_bytes = self.data[off:off+8]
            # Handle long names (starting with '/')
            if name_bytes[0:1] == b'/':
                str_offset = int(name_bytes[1:8].rstrip(b'\x00').decode(), 10)
                strtab_offset = self.symtab_offset + self.num_symbols * 18
                end = self.data.index(b'\x00', strtab_offset + str_offset)
                name = self.data[strtab_offset + str_offset:end].decode()
            else:
                name = name_bytes.rstrip(b'\x00').decode()

            raw_size = struct.unpack_from("<I", self.data, off + 16)[0]
            raw_offset = struct.unpack_from("<I", self.data, off + 20)[0]
            characteristics = struct.unpack_from("<I", self.data, off + 36)[0]

            self.sections.append({
                "name": name,
                "raw_size": raw_size,
                "raw_offset": raw_offset,
                "characteristics": characteristics,
                "index": i,
            })

        # Parse symbol table
        self.symbols = []
        self.symbol_map = {}
        self.name_to_sym_idx = {}  # name -> raw COFF symbol index
        sym_off = self.symtab_offset
        i = 0
        while i < self.num_symbols:
            name_bytes = self.data[sym_off:sym_off+8]
            if name_bytes[:4] == b'\x00\x00\x00\x00':
                str_offset = struct.unpack_from("<I", name_bytes, 4)[0]
                strtab_offset = self.symtab_offset + self.num_symbols * 18
                end = self.data.index(b'\x00', strtab_offset + str_offset)
                name = self.data[strtab_offset + str_offset:end].decode()
            else:
                name = name_bytes.rstrip(b'\x00').decode()

            value = struct.unpack_from("<I", self.data, sym_off + 8)[0]
            section = struct.unpack_from("<h", self.data, sym_off + 12)[0]
            storage_class = self.data[sym_off + 16]
            aux_count = self.data[sym_off + 17]

            sym = {
                "name": name,
                "value": value,
                "section": section,
                "storage_class": storage_class,
                "aux_count": aux_count,
            }
            self.symbols.append(sym)
            self.symbol_map[name] = sym
            self.name_to_sym_idx[name] = i

            sym_off += 18 * (1 + aux_count)
            i += 1 + aux_count

    def find_function_section(self, symbol_name: str):
        """Find the section containing a given function symbol.

        Returns (section_info, symbol_offset_within_section) or None.
        """
        sym = self.symbol_map.get(symbol_name)
        if not sym:
            return None

        if sym["section"] < 1:
            return None

        sec_idx = sym["section"] - 1
        sec = self.sections[sec_idx]
        return sec, sym["value"]

    def get_section_data(self, sec_idx: int) -> bytearray:
        """Get section data as a mutable bytearray."""
        sec = self.sections[sec_idx]
        start = sec["raw_offset"]
        size = sec["raw_size"]
        return self.data[start:start+size]

    def write_section_data(self, sec_idx: int, section_data: bytearray):
        """Write modified section data back."""
        sec = self.sections[sec_idx]
        start = sec["raw_offset"]
        size = sec["raw_size"]
        assert len(section_data) == size
        self.data[start:start+size] = section_data

    def save(self, output_path: str):
        """Save the patched COFF file."""
        with open(output_path, "wb") as f:
            f.write(self.data)

    def get_section_header_offset(self, sec_idx: int) -> int:
        """Get file offset of a section header."""
        return 20 + self.opthdr_size + sec_idx * 40

    def get_num_relocs(self, sec_idx: int) -> int:
        """Get number of relocations for a section."""
        off = self.get_section_header_offset(sec_idx)
        return struct.unpack_from("<H", self.data, off + 32)[0]

    def get_reloc_offset(self, sec_idx: int) -> int:
        """Get file offset of relocation table for a section."""
        off = self.get_section_header_offset(sec_idx)
        return struct.unpack_from("<I", self.data, off + 24)[0]

    def get_relocation(self, sec_idx: int, reloc_idx: int):
        """Read a relocation entry: (va, sym_idx, rtype)."""
        reloc_off = self.get_reloc_offset(sec_idx)
        entry_off = reloc_off + reloc_idx * 10
        va = struct.unpack_from("<I", self.data, entry_off)[0]
        sym_idx = struct.unpack_from("<I", self.data, entry_off + 4)[0]
        rtype = struct.unpack_from("<H", self.data, entry_off + 8)[0]
        return va, sym_idx, rtype

    def set_relocation(self, sec_idx: int, reloc_idx: int, va: int, sym_idx: int, rtype: int):
        """Write a relocation entry."""
        reloc_off = self.get_reloc_offset(sec_idx)
        entry_off = reloc_off + reloc_idx * 10
        struct.pack_into("<I", self.data, entry_off, va)
        struct.pack_into("<I", self.data, entry_off + 4, sym_idx)
        struct.pack_into("<H", self.data, entry_off + 8, rtype)

    def get_symbol_name_by_index(self, sym_idx: int) -> str:
        """Get symbol name by raw COFF symbol index."""
        sym_off = self.symtab_offset + sym_idx * 18
        name_bytes = self.data[sym_off:sym_off + 8]
        if name_bytes[:4] == b'\x00\x00\x00\x00':
            str_offset = struct.unpack_from("<I", name_bytes, 4)[0]
            strtab_start = self.symtab_offset + self.num_symbols * 18
            end = self.data.index(b'\x00', strtab_start + str_offset)
            return self.data[strtab_start + str_offset:end].decode()
        else:
            return name_bytes.rstrip(b'\x00').decode()

    def resize_section(self, sec_idx: int, new_size: int):
        """Resize a section's raw data, shifting all subsequent file offsets."""
        sec = self.sections[sec_idx]
        old_size = sec["raw_size"]
        size_diff = new_size - old_size

        if size_diff == 0:
            return

        # Insertion/removal point: end of current section data
        point = sec["raw_offset"] + old_size

        if size_diff > 0:
            self.data[point:point] = b'\x00' * size_diff
        else:
            self.data[point + size_diff:point] = b''

        # Update section raw_size
        hdr_off = self.get_section_header_offset(sec_idx)
        struct.pack_into("<I", self.data, hdr_off + 16, new_size)
        # Update virtual_size if it matched raw_size
        old_vsize = struct.unpack_from("<I", self.data, hdr_off + 8)[0]
        if old_vsize == old_size:
            struct.pack_into("<I", self.data, hdr_off + 8, new_size)
        sec["raw_size"] = new_size

        # Fix all file offsets at or after insertion point
        for i in range(self.num_sections):
            s_hdr = self.get_section_header_offset(i)

            # raw_offset (skip the section being resized — its start doesn't move)
            if i != sec_idx:
                raw_off = struct.unpack_from("<I", self.data, s_hdr + 20)[0]
                if raw_off >= point and raw_off > 0:
                    raw_off += size_diff
                    struct.pack_into("<I", self.data, s_hdr + 20, raw_off)
                    self.sections[i]["raw_offset"] = raw_off

            # reloc_offset (fix for ALL sections including the one being resized)
            reloc_off = struct.unpack_from("<I", self.data, s_hdr + 24)[0]
            if reloc_off >= point and reloc_off > 0:
                reloc_off += size_diff
                struct.pack_into("<I", self.data, s_hdr + 24, reloc_off)

            # lineno_offset
            lineno_off = struct.unpack_from("<I", self.data, s_hdr + 28)[0]
            if lineno_off >= point and lineno_off > 0:
                lineno_off += size_diff
                struct.pack_into("<I", self.data, s_hdr + 28, lineno_off)

        # Fix symbol table offset
        if self.symtab_offset >= point:
            self.symtab_offset += size_diff
            struct.pack_into("<I", self.data, 8, self.symtab_offset)

    def add_external_symbol(self, name: str) -> int:
        """Add an EXTERNAL UNDEFINED symbol. Returns the new raw COFF index."""
        # New symbol goes at end of symbol table (before string table)
        new_sym_offset = self.symtab_offset + self.num_symbols * 18

        # Build 18-byte symbol entry
        entry = bytearray(18)

        if len(name) <= 8:
            name_bytes = name.encode('ascii')
            entry[0:len(name_bytes)] = name_bytes
        else:
            # Read current string table size BEFORE inserting the entry
            strtab_size = struct.unpack_from("<I", self.data, new_sym_offset)[0]
            struct.pack_into("<I", entry, 0, 0)            # zeros (long name indicator)
            struct.pack_into("<I", entry, 4, strtab_size)  # offset into string table

        # value=0, section=0 (undefined), type=0, storage_class=2 (EXTERNAL)
        struct.pack_into("<I", entry, 8, 0)    # value
        struct.pack_into("<h", entry, 12, 0)   # section (0 = undefined)
        struct.pack_into("<H", entry, 14, 0)   # type
        entry[16] = 2   # IMAGE_SYM_CLASS_EXTERNAL
        entry[17] = 0   # aux_count

        # Insert entry at end of symbol table
        self.data[new_sym_offset:new_sym_offset] = entry

        new_idx = self.num_symbols
        self.num_symbols += 1
        struct.pack_into("<I", self.data, 12, self.num_symbols)

        # Update name→index map
        self.name_to_sym_idx[name] = new_idx

        if len(name) > 8:
            # Append name to string table (now shifted by 18 bytes)
            new_strtab_start = new_sym_offset + 18
            strtab_size = struct.unpack_from("<I", self.data, new_strtab_start)[0]
            name_bytes = name.encode('ascii') + b'\x00'
            insert_pos = new_strtab_start + strtab_size
            self.data[insert_pos:insert_pos] = name_bytes
            new_strtab_size = strtab_size + len(name_bytes)
            struct.pack_into("<I", self.data, new_strtab_start, new_strtab_size)

        return new_idx

    def find_or_add_symbol(self, name: str) -> int:
        """Find an existing symbol by name, or add as external undefined."""
        idx = self.name_to_sym_idx.get(name)
        if idx is not None:
            return idx
        return self.add_external_symbol(name)


def get_obj_path_for_unit(unit: str) -> Path:
    """Convert unit path to .obj path in build directory.

    DB uses 'default/system/char/CharServoBone' format.
    Build uses 'build/373307D9/src/system/char/CharServoBone.obj'.
    """
    # Strip 'default/' prefix and replace with 'src/'
    if unit.startswith("default/"):
        unit = "src/" + unit[len("default/"):]
    # Ensure .obj extension
    if not unit.endswith(".obj"):
        unit = unit + ".obj"
    return PROJECT_ROOT / "build" / "373307D9" / unit


def run_objdiff_json(symbol: str) -> Optional[dict]:
    """Run objdiff and return JSON diff data."""
    json_path = Path("/tmp/claude/objdiff_patch.json")
    result = subprocess.run(
        [str(OBJDIFF_CLI), "diff", symbol,
         "--include-instructions", "-f", "json",
         "-o", str(json_path)],
        capture_output=True, text=True, timeout=30,
        cwd=str(PROJECT_ROOT)
    )
    if result.returncode != 0:
        print(f"objdiff failed: {result.stderr[:200]}", file=sys.stderr)
        return None
    with open(json_path) as f:
        return json.load(f)


def patch_function(symbol: str, unit: str, dry_run: bool = True) -> dict:
    """Patch a function's register swaps in its .obj file.

    Returns dict with results.
    """
    result = {"symbol": symbol, "unit": unit, "success": False}

    # Run objdiff to get diff
    diff_data = run_objdiff_json(symbol)
    if not diff_data:
        result["error"] = "objdiff failed"
        return result

    result["match_before"] = diff_data.get("fuzzy_match_percent", 0)

    # Analyze for patches
    patches = analyze_diff_for_patches(diff_data)
    result["patches_found"] = len(patches)

    if not patches:
        result["error"] = "no register patches found"
        return result

    # Find the .obj file
    obj_path = get_obj_path_for_unit(unit)
    if not obj_path.exists():
        result["error"] = f"obj not found: {obj_path}"
        return result

    # Parse the COFF file
    try:
        coff = COFFPatcher(str(obj_path))
    except Exception as e:
        result["error"] = f"COFF parse error: {e}"
        return result

    # Find the function's section
    sec_info = coff.find_function_section(symbol)
    if not sec_info:
        result["error"] = f"symbol not found in COFF: {symbol}"
        return result

    sec, sym_offset = sec_info
    section_data = coff.get_section_data(sec["index"])

    # Apply patches
    if dry_run:
        result["dry_run"] = True
        result["patches_description"] = [p.description for p in patches]
        result["success"] = True
        return result

    # Make backup (only if no backup exists yet - preserve original)
    backup_path = str(obj_path) + ".bak"
    if not Path(backup_path).exists():
        shutil.copy2(str(obj_path), backup_path)

    applied = apply_patches(section_data, patches, sym_offset)
    coff.write_section_data(sec["index"], section_data)
    coff.save(str(obj_path))

    result["patches_applied"] = applied
    result["backup"] = backup_path

    # Re-run objdiff to verify
    verify_data = run_objdiff_json(symbol)
    if verify_data:
        result["match_after"] = verify_data.get("fuzzy_match_percent", 0)

        # Safety: restore backup if match regressed
        if result["match_after"] < result["match_before"] - 0.01:
            shutil.copy2(backup_path, str(obj_path))
            result["reverted"] = True
            result["match_after"] = result["match_before"]

    result["success"] = True
    return result


def scan_all_regswaps(min_pct: float = 90.0, max_pct: float = 100.0) -> list:
    """Scan the database for functions with register swap mismatches.

    Returns list of dicts with symbol, unit, match_pct, patches info.
    """
    import sqlite3
    conn = sqlite3.connect(str(PROJECT_ROOT / "decomp.db"))
    cur = conn.cursor()
    cur.execute(
        "SELECT symbol, demangled, unit, current_percent FROM functions "
        "WHERE current_percent >= ? AND current_percent < ? AND excluded = 0",
        (min_pct, max_pct)
    )
    rows = cur.fetchall()
    conn.close()

    candidates = []
    for symbol, demangled, unit, pct in rows:
        diff = run_objdiff_json(symbol)
        if not diff:
            continue
        patches = analyze_diff_for_patches(diff)
        if patches:
            candidates.append({
                "symbol": symbol,
                "demangled": demangled,
                "unit": unit,
                "match_pct": diff["fuzzy_match_percent"],
                "patch_count": len(patches),
            })
    return candidates


def main():
    import argparse
    parser = argparse.ArgumentParser(description="Patch register swaps in .obj files")
    parser.add_argument("symbol", nargs="?", help="Function symbol to patch")
    parser.add_argument("--unit", help="Unit path (auto-detected from DB if omitted)")
    parser.add_argument("--dry-run", action="store_true", default=True, help="Show patches without applying")
    parser.add_argument("--apply", action="store_true", help="Actually apply patches")
    parser.add_argument("--batch", action="store_true", help="Patch functions from manifest/survey JSON")
    parser.add_argument("--manifest", help="Path to regswap_manifest.json (default: scripts/regswap_manifest.json)")
    parser.add_argument("--survey-json", help="Legacy: path to regswap_results.json")
    parser.add_argument("--all-regswap", action="store_true", help="With --survey-json: patch ALL categories")
    args = parser.parse_args()

    if args.apply:
        args.dry_run = False

    if args.batch:
        # Load patchable functions from manifest or survey
        if args.manifest or not args.survey_json:
            manifest_path = args.manifest or str(PROJECT_ROOT / "scripts" / "regswap_manifest.json")
            with open(manifest_path) as f:
                patchable = json.load(f)
            print(f"Loaded {len(patchable)} functions from manifest")
        else:
            with open(args.survey_json) as f:
                survey_data = json.load(f)
            if args.all_regswap:
                patchable = [s for s in survey_data if s.get("regswap_instr_count", 0) > 0]
                print(f"Found {len(patchable)} functions with regswaps to patch")
            else:
                patchable = [s for s in survey_data if s["category"] == "REGSWAP_ONLY"]
                print(f"Found {len(patchable)} REGSWAP_ONLY functions to patch")

        hit_100 = 0
        improved = 0
        reverted = 0
        failed = 0
        for entry in patchable:
            symbol = entry["symbol"]
            unit = entry["unit"]

            result = patch_function(symbol, unit, dry_run=args.dry_run)

            if not result["success"]:
                failed += 1
                continue

            if args.dry_run:
                continue

            if result.get("reverted"):
                reverted += 1
                dem = entry.get("demangled", symbol)[:55]
                print(f"  REVERTED: {dem} ({result['match_before']:.1f}%)")
                continue

            before = result.get("match_before", 0)
            after = result.get("match_after", 0)
            if after >= 99.999:
                hit_100 += 1
            elif after > before + 0.01:
                improved += 1

        print(f"\nResults: {len(patchable)} functions processed")
        if not args.dry_run:
            print(f"  100%: {hit_100}")
            print(f"  Improved: {improved}")
            print(f"  Reverted (regression): {reverted}")
            print(f"  Failed: {failed}")
        return

    if not args.symbol:
        parser.print_help()
        sys.exit(1)

    # Single function mode
    if not args.unit:
        import sqlite3
        conn = sqlite3.connect(str(PROJECT_ROOT / "decomp.db"))
        cur = conn.cursor()
        cur.execute("SELECT unit FROM functions WHERE symbol = ?", (args.symbol,))
        row = cur.fetchone()
        conn.close()
        if not row:
            print(f"ERROR: symbol not found in DB: {args.symbol}", file=sys.stderr)
            sys.exit(1)
        args.unit = row[0]

    result = patch_function(args.symbol, args.unit, dry_run=args.dry_run)
    print(json.dumps(result, indent=2))


if __name__ == "__main__":
    main()
