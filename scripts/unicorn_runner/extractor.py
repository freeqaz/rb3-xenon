"""Function extraction from COFF .obj files (decomp and original)."""

import struct


def _is_internal_label(name):
    """Check if a symbol is a compiler-internal label, not a function boundary.

    MSVC generates these symbols within function bodies:
      $M<digits>  - branch/switch case labels
      $T<digits>  - compiler temporaries
      $LN<digits> - local numeric labels
      .text       - section name pseudo-symbol (always at offset 0)
    """
    return name.startswith('$') or name == '.text'


def _find_function_end(coff, sym):
    """Find the end boundary for a function symbol within its section.

    Scans for the next meaningful symbol (skipping compiler-internal labels)
    in the same section. Falls back to section end for COMDAT-style sections.
    """
    sec = coff.sections[sym['section'] - 1]
    start = sym['value']
    end = sec['raw_size']

    for s in coff.symbols:
        if (s['section'] == sym['section']
                and s['value'] > start
                and s['value'] < end
                and not _is_internal_label(s['name'])):
            end = s['value']

    return end


def _extract_with_relocs(coff, sym, start, end):
    """Extract bytes and relocations for a function range within its section."""
    sec_idx = sym['section'] - 1
    sec_data = coff.get_section_data(sec_idx)
    func_bytes = bytearray(sec_data[start:end])

    all_relocs = coff.get_section_relocations(sec_idx)
    relocs = []
    for r in all_relocs:
        if r['offset'] >= start and r['offset'] < end:
            adj = dict(r)
            adj['offset'] = r['offset'] - start
            relocs.append(adj)

    return func_bytes, relocs


def extract_from_decomp(coff, symbol):
    """Extract function bytes and relocations from a .obj file.

    Handles both multi-function sections and COMDAT sections.
    Skips compiler-internal labels ($M, $T, $LN) when finding boundaries.

    Returns (bytearray, list[dict]) or (None, None) if symbol not found.
    """
    sym = coff.symbol_map.get(symbol)
    if not sym or sym['section'] <= 0:
        return None, None

    start = sym['value']
    end = _find_function_end(coff, sym)

    if end <= start:
        return None, None

    return _extract_with_relocs(coff, sym, start, end)


def extract_from_original(coff, symbol):
    """Extract function bytes and relocations from a .obj file.

    Uses the same boundary-scanning logic as extract_from_decomp,
    handling both multi-function and COMDAT sections correctly.

    Returns (bytearray, list[dict]) or (None, None) if symbol not found.
    """
    sym = coff.symbol_map.get(symbol)
    if not sym or sym['section'] <= 0:
        return None, None

    start = sym['value']
    end = _find_function_end(coff, sym)

    if end <= start:
        return None, None

    return _extract_with_relocs(coff, sym, start, end)


def has_indirect_branch(code_bytes):
    """Detect bctr (0x4E800420) and bctrl (0x4E800421) in function bytes.

    Returns "bctrl", "bctr", or None.
    """
    for i in range(0, len(code_bytes), 4):
        insn = struct.unpack_from(">I", code_bytes, i)[0]
        if insn == 0x4E800421:  # bctrl — indirect call (virtual dispatch)
            return "bctrl"
        if insn == 0x4E800420:  # bctr — indirect branch (switch table or vtable tail call)
            return "bctr"
    return None


def classify_indirect_branch(code_bytes, relocs, coff=None):
    """Classify indirect branch type with richer detail.

    Returns "bctrl", "bctr_switch", "bctr_tailcall", or None.

    bctr_switch: function has bctr + REFHI/REFLO relocs to .rdata sections (jump table)
    bctr_tailcall: function has bctr but no .rdata references (vtable tail call)
    """
    has_bctrl = False
    has_bctr = False

    for i in range(0, len(code_bytes), 4):
        insn = struct.unpack_from(">I", code_bytes, i)[0]
        if insn == 0x4E800421:
            has_bctrl = True
        elif insn == 0x4E800420:
            has_bctr = True

    if has_bctrl and has_bctr:
        # Both present — check for switch table (.rdata relocs)
        if coff is not None:
            for reloc in relocs:
                if reloc["type_name"] in ("REFHI", "REFLO"):
                    sym = coff.symbol_map.get(reloc["symbol_name"])
                    if sym and sym['section'] > 0:
                        sec = coff.sections[sym['section'] - 1]
                        if sec['name'].startswith('.rdata'):
                            return "bctrl_switch"  # vtable + switch table
        return "bctrl"  # vtable only

    if has_bctrl:
        return "bctrl"

    if not has_bctr:
        return None

    # bctr found — classify as switch table or tail call
    if coff is not None:
        for reloc in relocs:
            if reloc["type_name"] in ("REFHI", "REFLO"):
                sym = coff.symbol_map.get(reloc["symbol_name"])
                if sym and sym['section'] > 0:
                    sec = coff.sections[sym['section'] - 1]
                    if sec['name'].startswith('.rdata'):
                        return "bctr_switch"

    return "bctr_tailcall"


def has_ppc64_insns(code_bytes):
    """Detect 64-bit PPC instructions unsupported by Unicorn PPC32 mode.

    The Xbox 360 Xenon CPU is a PPC64 chip running in 32-bit compat mode,
    so MSVC uses ld/std (opcode 58/62) for callee-saved register preservation.
    Unicorn PPC32 mode doesn't support these and raises UC_ERR_EXCEPTION.

    Returns "std/ld" or None.
    """
    for i in range(0, len(code_bytes), 4):
        insn = struct.unpack_from(">I", code_bytes, i)[0]
        opcode = (insn >> 26) & 0x3F
        if opcode == 62:  # std (store doubleword)
            return "std/ld"
        if opcode == 58:  # ld (load doubleword)
            return "std/ld"
    return None
