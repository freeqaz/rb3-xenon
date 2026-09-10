"""Relocation patching for the Unicorn Function Runner."""

import re
import struct

from .image import get_global_image
from .memory_map import TRAMPOLINE_BASE, GLOBAL_BASE, RDATA_BASE, REGION_SIZE
from .save_helpers import helper_address


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

    MSVC's register save/restore helpers are the one exception: they resolve
    to a fixed address in the HELPER region, where save_helpers has installed
    the real body. They are NOT given a trampoline slot, for two reasons.
    A `li r3,0; blr` stub is catastrophic for them (see save_helpers), and the
    slot numbering itself is per-side: the two sides do not agree on which
    functions need `__savegprlr_N`, so letting the helpers consume slots
    shifted every later symbol's trampoline address on one side only, and the
    prologue call showed up in the call log as a call the function had made.
    """
    trampolines = {}   # symbol_name -> trampoline address
    globals_map = {}    # symbol_name -> global slot address
    next_trampoline = TRAMPOLINE_BASE
    next_global = GLOBAL_BASE

    for reloc in relocs:
        sym = reloc["symbol_name"]
        if reloc["type_name"] in ("REL24", "REL14"):
            if sym not in trampolines:
                helper = helper_address(sym)
                if helper is not None:
                    trampolines[sym] = helper
                    continue
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

    _synthesize_float_constants(coff, relocs, base_rdata, globals_override)

    if len(base_rdata) == 0:
        return None, globals_override

    return bytes(base_rdata), globals_override


# MSVC names a floating-point literal pool entry after its own bytes:
# `__real@3f800000` IS 1.0f, `__real@3ff0000000000000` IS 1.0. The hex digits
# are the big-endian value, which is what the Xenon loads.
_REAL_RE = re.compile(r'^__real@([0-9a-fA-F]{8}|[0-9a-fA-F]{16})$')


def _synthesize_float_constants(coff, relocs, base_rdata, globals_override):
    """Materialise `__real@…` constants that this COFF does not define.

    The decomp .obj defines its float literals in a real `.rdata` section, so
    the loop above maps them and the decomp side loads the right values. The
    *original* .obj — reconstructed by the splitter from the XEX — carries them
    as UNDEFINED externals (section 0). Nothing mapped them, so they landed in a
    plain zero-filled GLOBAL slot and the original side loaded 0.0f wherever the
    decomp loaded 1.0f, -1.0f, 1e30f, …

    That is a harness asymmetry, not a decomp bug, and it was the single largest
    source of DIVERGENT verdicts in the database: it makes the original compute
    different arithmetic, take different branches (whole `wild_jump_match`
    clusters), and store different words into the object. Since the mangled name
    carries the bytes, we can rebuild the constant exactly without the section.

    Only fills symbols that are undefined here AND not already mapped, so the
    decomp side — where these are real defined data — is untouched.
    """
    wanted = []
    seen = set()
    for reloc in relocs:
        if reloc["type_name"] not in ("REFHI", "REFLO", "ADDR32"):
            continue
        name = reloc["symbol_name"]
        if name in globals_override or name in seen:
            continue
        m = _REAL_RE.match(name)
        if not m:
            continue
        sym = coff.symbol_map.get(name)
        if sym and sym.get('section', 0) > 0:
            continue          # genuinely defined in this object; leave it alone
        seen.add(name)
        wanted.append((name, bytes.fromhex(m.group(1))))

    for name, raw in wanted:
        align = len(raw)      # 4 for float, 8 for double
        pad = (-len(base_rdata)) % align
        base_rdata.extend(b'\x00' * pad)
        globals_override[name] = RDATA_BASE + len(base_rdata)
        base_rdata.extend(raw)


# Symbols this big are not "a constant the two sides disagree about"; they are
# tables, and copying one into a 64KB region would crowd out everything else.
_MAX_SEEDED_SYMBOL = 4096

# Leave the tail of the RDATA region free so a later switch table or string
# still fits after seeding.
_RDATA_SEED_BUDGET = REGION_SIZE - 0x2000


def seed_image_globals(coff, relocs, globals_map, rdata_bytes, image=None):
    """Give symbols this .obj does not define their content from the image.

    The harness's picture of a global comes from whichever .obj is in front of
    it, and the two sides do not define the same set. The decomp compiles one
    .cpp, so `static float kSampleRate = 48000.0f` is real .data. The splitter,
    carving the same function out of the linked image, emits kSampleRate as an
    UNDEFINED external because the word lives in some other split object. The
    undefined side got a zero-filled slot, so the original divided by zero
    while the decomp divided by 48000, and the runner blamed the decomp.

    So: any REFHI/REFLO/ADDR32 target that this .obj leaves undefined, and that
    the shipped image knows, is seeded with the image's bytes at that symbol's
    address. Both sides then start from one initial global image.

    NOT seeded:

    * Symbols this .obj DEFINES -- including in .bss. `float sZoom;` compiled to
      a zero .bss word while the original holds 0x3F800000 is a real dropped
      initializer (4f8b6e036 found seven of them, all behaviourally live and
      all invisible to objdiff). Overwriting our own definition with the
      image's would erase exactly that signal, so a defined symbol always
      keeps the bytes its own .obj gave it.
    * Symbols whose image content is all zero -- seeding is a no-op, and not
      moving them keeps the address assignment untouched.
    * Symbols whose content is (or contains) a pointer into the image. The
      harness maps no image memory, so such a pointer aims at an on-demand
      zero page; null is the truer answer and is what every other pointer in
      the fixture already is. See GlobalImage.contains_image_pointer.

    Placement mirrors what the value is for:

    * <= 4 bytes stays in its GLOBAL slot, seeded in place, so that a STORE to
      the global still lands in the region the comparator diffs. Scalars are
      the overwhelming majority (723 of 749 decomp-side seeds in a 60-unit
      survey) and are the shape that gets written.
    * larger objects -- in practice string literals, since anything made of
      pointers is excluded above -- move into the RDATA buffer, the only place
      with room for them. Writes there are not compared, which is acceptable
      for string literals and much cheaper than the alternative: resizing
      GLOBAL slots would perturb the per-side address assignment that the
      cross-side call-arg alignment depends on.

    Mutates globals_map for relocated symbols. Returns (rdata_bytes,
    globals_init) where globals_init maps a GLOBAL address to the bytes to
    write there before execution.
    """
    if image is None:
        image = get_global_image()
    globals_init = {}
    if not getattr(image, "available", False):
        return rdata_bytes, globals_init

    buf = bytearray(rdata_bytes) if rdata_bytes else bytearray()
    started_empty = not rdata_bytes
    grew = False

    seen = set()
    for reloc in relocs:
        if reloc["type_name"] not in ("REFHI", "REFLO", "ADDR32"):
            continue
        name = reloc["symbol_name"]
        if name in seen:
            continue
        seen.add(name)

        addr = globals_map.get(name)
        if addr is None or not (GLOBAL_BASE <= addr < GLOBAL_BASE + REGION_SIZE):
            continue          # already backed by real data on this side

        sym = coff.symbol_map.get(name)
        if sym is not None and sym.get("section", 0) > 0:
            continue          # this .obj defines it — its bytes win

        entry = image.lookup(name)
        if entry is None or not (0 < entry.size <= _MAX_SEEDED_SYMBOL):
            continue

        content = image.read(entry.address, entry.size)
        if not content or not any(content):
            continue          # zero in the image: the slot is already right
        if image.contains_image_pointer(content):
            continue          # a pointer we cannot honour; zero is truer

        if entry.size <= 4:
            globals_init[addr] = content
            continue

        align = 8 if entry.size >= 8 else 4
        pad = (-len(buf)) % align
        if len(buf) + pad + entry.size > _RDATA_SEED_BUDGET:
            continue          # no room; leave it zero rather than corrupt data
        buf.extend(b"\x00" * pad)
        globals_map[name] = RDATA_BASE + len(buf)
        buf.extend(content)
        grew = True

    if started_empty and not grew:
        return rdata_bytes, globals_init
    return bytes(buf), globals_init


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
