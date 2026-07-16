"""Relocation patching for the Unicorn Function Runner."""

import struct

from .memory_map import TRAMPOLINE_BASE, GLOBAL_BASE, RDATA_BASE


def rewrite_ppc64_insns(code):
    """Replace std/ld (PPC64) with stw/lwz (PPC32) in-place.

    The Xbox 360 Xenon CPU uses std/ld (DS-form, opcodes 62/58) for
    callee-saved register preservation. Unicorn PPC32 mode doesn't
    support these. We rewrite them to stw/lwz (D-form, opcodes 36/32)
    which are the 32-bit equivalents.

    Both sides get the same rewriting, preserving equivalence testing validity.

    Returns the number of instructions rewritten.
    """
    count = 0
    for i in range(0, len(code), 4):
        insn = struct.unpack_from(">I", code, i)[0]
        opcode = (insn >> 26) & 0x3F
        if opcode not in (58, 62):  # ld, std
            continue
        rs = (insn >> 21) & 0x1F
        ra = (insn >> 16) & 0x1F
        ds = (insn >> 2) & 0x3FFF
        if ds >= 0x2000:
            ds -= 0x4000  # sign extend 14-bit
        actual_offset = ds * 4
        new_opcode = 36 if opcode == 62 else 32  # stw or lwz
        new_insn = (new_opcode << 26) | (rs << 21) | (ra << 16) | (actual_offset & 0xFFFF)
        struct.pack_into(">I", code, i, new_insn)
        count += 1
    return count


def assign_addresses(relocs):
    """Assign Unicorn addresses to all relocation targets.

    Returns (trampolines, globals_map) dicts mapping symbol_name -> address.
    """
    trampolines = {}   # symbol_name -> trampoline address
    globals_map = {}    # symbol_name -> global slot address
    next_trampoline = TRAMPOLINE_BASE
    next_global = GLOBAL_BASE

    for reloc in relocs:
        sym = reloc["symbol_name"]
        if reloc["type_name"] in ("REL24", "REL14"):
            if sym not in trampolines:
                trampolines[sym] = next_trampoline
                next_trampoline += 8   # each stub is 8 bytes
        elif reloc["type_name"] in ("REFHI", "REFLO"):
            if sym not in globals_map:
                globals_map[sym] = next_global
                next_global += 4       # each global is 4 bytes
        elif reloc["type_name"] == "ADDR32":
            if sym not in globals_map:
                globals_map[sym] = next_global
                next_global += 4

    return trampolines, globals_map


def patch_rel24(code, offset, trampoline_addr, code_base):
    """Patch a REL24 branch-and-link instruction."""
    insn = struct.unpack_from(">I", code, offset)[0]
    pc = code_base + offset
    delta = trampoline_addr - pc

    assert -0x2000000 <= delta <= 0x1FFFFFC, f"REL24 out of range: {delta}"

    # Clear bits [6:29], preserve opcode (bits [0:5]) and AA/LK (bits [30:31])
    insn = (insn & 0xFC000003) | (delta & 0x03FFFFFC)
    struct.pack_into(">I", code, offset, insn)


def patch_refhi(code, offset, target_addr):
    """Patch REFHI — upper 16 bits with @ha adjustment."""
    insn = struct.unpack_from(">I", code, offset)[0]
    ha = (target_addr >> 16) + ((target_addr & 0x8000) >> 15)
    insn = (insn & 0xFFFF0000) | (ha & 0xFFFF)
    struct.pack_into(">I", code, offset, insn)


def patch_reflo(code, offset, target_addr):
    """Patch REFLO — lower 16 bits."""
    insn = struct.unpack_from(">I", code, offset)[0]
    lo = target_addr & 0xFFFF
    insn = (insn & 0xFFFF0000) | lo
    struct.pack_into(">I", code, offset, insn)


def patch_addr32(code, offset, target_addr):
    """Patch ADDR32 — absolute 32-bit address."""
    struct.pack_into(">I", code, offset, target_addr & 0xFFFFFFFF)


def patch_function(code_bytearray, relocs, trampolines, globals_map, code_base):
    """Apply all relocation patches to a function's code bytes.

    REL14 relocations use relay stubs appended after the function code,
    since the 14-bit displacement (±32KB) can't reach TRAMPOLINE_BASE.
    Each relay is a 4-byte unconditional branch to the real trampoline.
    """
    # Collect REL14 relocs — these need relay stubs
    rel14_relocs = [(r["symbol_name"], r["offset"])
                    for r in relocs if r["type_name"] == "REL14"]
    relay_map = {}  # sym -> relay_offset (in code buffer)

    if rel14_relocs:
        # Align relay region to 4 bytes
        relay_start = (len(code_bytearray) + 3) & ~3
        if relay_start > len(code_bytearray):
            code_bytearray.extend(b'\x00' * (relay_start - len(code_bytearray)))

        for sym, _ in rel14_relocs:
            if sym in relay_map:
                continue
            relay_offset = len(code_bytearray)
            relay_map[sym] = relay_offset
            # Emit: b <trampoline> — patched below as REL24
            code_bytearray.extend(b'\x48\x00\x00\x00')  # b +0 (placeholder)

        # Patch relay stubs to jump to their trampolines
        for sym, relay_offset in relay_map.items():
            target = trampolines[sym]
            patch_rel24(code_bytearray, relay_offset, target, code_base)

    for reloc in relocs:
        sym = reloc["symbol_name"]
        off = reloc["offset"]
        rtype = reloc["type_name"]

        if rtype == "REL24":
            target = trampolines[sym]
            patch_rel24(code_bytearray, off, target, code_base)
        elif rtype == "REL14":
            # Patch conditional branch to point to relay stub
            relay_addr = code_base + relay_map[sym]
            insn = struct.unpack_from(">I", code_bytearray, off)[0]
            pc = code_base + off
            delta = relay_addr - pc
            assert -0x8000 <= delta <= 0x7FFC, f"REL14 relay out of range: {delta}"
            # BD field is bits [16:29], preserve opcode/BO/BI (bits [0:15]) and AA/LK (bits [30:31])
            insn = (insn & 0xFFFF0003) | (delta & 0x0000FFFC)
            struct.pack_into(">I", code_bytearray, off, insn)
        elif rtype == "REFHI":
            target = globals_map[sym]
            patch_refhi(code_bytearray, off, target)
        elif rtype == "REFLO":
            target = globals_map[sym]
            patch_reflo(code_bytearray, off, target)
        elif rtype == "PAIR":
            pass
        elif rtype == "ADDR32":
            target = globals_map[sym]
            patch_addr32(code_bytearray, off, target)
        else:
            raise ValueError(f"Unknown relocation type: {rtype}")


def prepare_data_sections(coff, relocs, existing_rdata_bytes=None, existing_override=None):
    """Load initialized data sections (.data*, .rdata*) referenced by REFHI/REFLO relocs.

    Scans relocations for REFHI/REFLO targets whose sections are initialized data
    (names starting with '.data' or '.rdata'). Loads those section bytes and maps
    referenced symbols to RDATA_BASE + offset.

    If existing_rdata_bytes / existing_override are provided (e.g. from
    prepare_switch_tables), the new data is appended after the existing data
    and offsets are adjusted accordingly.

    Returns:
        (rdata_bytes, globals_override) — bytes to map at RDATA_BASE,
        dict of symbol_name -> RDATA_BASE+offset.  Returns (None, {}) if
        no data sections are referenced.
    """
    # Find data sections referenced by REFHI/REFLO relocs
    data_sections = {}  # sec_idx -> set of symbol names
    for reloc in relocs:
        if reloc["type_name"] in ("REFHI", "REFLO"):
            sym = coff.symbol_map.get(reloc["symbol_name"])
            if sym and sym['section'] > 0:
                sec_idx = sym['section'] - 1
                sec = coff.sections[sec_idx]
                if sec['name'].startswith('.data') or sec['name'].startswith('.rdata'):
                    if sec_idx not in data_sections:
                        data_sections[sec_idx] = set()
                    data_sections[sec_idx].add(reloc["symbol_name"])

    if not data_sections:
        return existing_rdata_bytes, existing_override or {}

    # Start after any existing rdata content
    base_rdata = bytearray()
    if existing_rdata_bytes:
        base_rdata.extend(existing_rdata_bytes)

    globals_override = dict(existing_override) if existing_override else {}

    # Check which sections are already covered by existing overrides
    already_mapped = set()
    for sym_name in globals_override:
        sym = coff.symbol_map.get(sym_name)
        if sym and sym['section'] > 0:
            already_mapped.add(sym['section'] - 1)

    for sec_idx in sorted(data_sections.keys()):
        if sec_idx in already_mapped:
            continue

        sec_data = coff.get_section_data(sec_idx)
        sec_offset = len(base_rdata)

        # Map referenced symbols to their RDATA_BASE addresses
        for sym_name in data_sections[sec_idx]:
            sym = coff.symbol_map[sym_name]
            globals_override[sym_name] = RDATA_BASE + sec_offset + sym['value']

        base_rdata.extend(sec_data)

    if len(base_rdata) == 0:
        return None, globals_override

    return bytes(base_rdata), globals_override


def prepare_switch_tables(coff, func_symbol, relocs, code_base):
    """Load and rebase .rdata switch table data for a bctr_switch function.

    Finds .rdata sections referenced by the function's REFHI/REFLO relocs,
    loads their data, and rebases ADDR32 entries (case labels) to point into
    the loaded code at code_base.

    Args:
        coff: COFFParser instance
        func_symbol: mangled function symbol name
        relocs: function-relative relocations
        code_base: address where function code is mapped

    Returns:
        (rdata_bytes, globals_override) or (None, None)
        - rdata_bytes: bytes to map at RDATA_BASE
        - globals_override: dict of symbol_name -> RDATA_BASE+offset
    """
    func_sym = coff.symbol_map.get(func_symbol)
    if not func_sym or func_sym['section'] <= 0:
        return None, None

    func_sec_idx = func_sym['section'] - 1
    func_start = func_sym['value']

    # Find .rdata sections referenced by the function's relocs
    rdata_sections = {}  # sec_idx -> set of symbol names
    for reloc in relocs:
        if reloc["type_name"] in ("REFHI", "REFLO", "ADDR32"):
            sym = coff.symbol_map.get(reloc["symbol_name"])
            if sym and sym['section'] > 0:
                sec_idx = sym['section'] - 1
                sec = coff.sections[sec_idx]
                if sec['name'].startswith('.rdata'):
                    if sec_idx not in rdata_sections:
                        rdata_sections[sec_idx] = set()
                    rdata_sections[sec_idx].add(reloc["symbol_name"])

    if not rdata_sections:
        return None, None

    # Load and concatenate .rdata sections, rebase jump table entries
    all_rdata = bytearray()
    globals_override = {}

    for sec_idx in sorted(rdata_sections.keys()):
        sec_data = bytearray(coff.get_section_data(sec_idx))
        sec_offset = len(all_rdata)

        # Map referenced symbols to their RDATA_BASE addresses
        for sym_name in rdata_sections[sec_idx]:
            sym = coff.symbol_map[sym_name]
            globals_override[sym_name] = RDATA_BASE + sec_offset + sym['value']

        # Rebase ADDR32 relocs: case labels in the function's .text section
        sec_relocs = coff.get_section_relocations(sec_idx)
        for reloc in sec_relocs:
            if reloc['type_name'] == 'ADDR32':
                off = reloc['offset']
                target_sym = coff.symbol_map.get(reloc['symbol_name'])
                if target_sym and target_sym['section'] - 1 == func_sec_idx:
                    label_func_offset = target_sym['value'] - func_start
                    new_addr = code_base + label_func_offset
                    struct.pack_into(">I", sec_data, off, new_addr)

        all_rdata.extend(sec_data)

    return bytes(all_rdata), globals_override
