"""Intra-TU callee co-loading for the Unicorn Function Runner.

When comparing a function, also extracts its intra-TU callees and loads them
sequentially in the CODE region. Patches the caller's bl instructions to jump
to the real callee code instead of a trampoline. External calls still hit
trampolines as before.
"""

import struct
from collections import deque
from dataclasses import dataclass, field

from .memory_map import REGION_SIZE


@dataclass
class ColoadResult:
    """Result of building a co-load layout."""
    symbol_offsets: dict  # symbol_name -> byte offset within combined buffer
    coloaded_symbols: list  # ordered list of callee symbols (not including root)
    total_size: int  # total size of combined buffer


def _is_section_symbol(coff, name):
    """Check if a symbol name matches a section name (section symbol).

    In COFF, section symbols like '.text' appear in the symbol table
    with value 0 and are used as relocation targets for intra-section refs.
    """
    return name in coff._section_names


def _find_function_at_offset(coff, sec_num, offset):
    """Find a function symbol at the given offset in the given section.

    Skips compiler-internal labels ($M, $T, $LN) and section symbols.
    Returns the symbol name, or None if not found.
    """
    name = coff._symbols_by_section_offset.get((sec_num, offset))
    if name is not None and name not in coff._section_names:
        return name
    return None


def resolve_section_rel24_targets(coff, symbol_name, code_bytes, relocs):
    """Resolve REL24 relocations targeting section symbols to actual functions.

    In original .obj files, all functions share a single .text section.
    When one function calls another in the same section, the COFF relocation
    targets the .text section symbol (value 0) rather than the callee's
    function symbol. The actual callee offset is encoded in the bl
    instruction's displacement field.

    This reads those displacements and resolves them to the actual function
    symbols, enabling proper callee discovery and co-loading.

    Returns a new list of relocs with section-symbol REL24 targets resolved.
    Unresolvable targets are left unchanged.
    """
    sym = coff.symbol_map.get(symbol_name)
    if sym is None or sym['section'] <= 0:
        return relocs

    func_start = sym['value']
    sec_num = sym['section']

    resolved = []
    for r in relocs:
        if (r['type_name'] == 'REL24'
                and _is_section_symbol(coff, r['symbol_name'])):
            # Read bl instruction displacement
            if r['offset'] + 4 <= len(code_bytes):
                insn = struct.unpack_from(">I", code_bytes, r['offset'])[0]
                disp = insn & 0x03FFFFFC
                if disp & 0x02000000:  # sign extend 26-bit
                    disp -= 0x04000000

                # Compute section-relative target offset
                section_offset = func_start + r['offset']
                target_offset = section_offset + disp

                # Find function symbol at that offset
                target_name = _find_function_at_offset(
                    coff, sec_num, target_offset)
                if target_name and target_name != symbol_name:
                    adj = dict(r)
                    adj['symbol_name'] = target_name
                    resolved.append(adj)
                    continue

        resolved.append(r)

    return resolved


def is_intra_tu_callee(coff, symbol_name):
    """Check if a REL24 target is defined in this .obj's .text section.

    Returns True if the symbol exists, has a positive section number,
    and its section name starts with '.text'.
    """
    sym = coff.symbol_map.get(symbol_name)
    if sym is None or sym['section'] <= 0:
        return False
    sec = coff.sections[sym['section'] - 1]
    return sec['name'].startswith('.text')


def _get_rel24_targets(relocs):
    """Extract unique REL24 target symbol names from relocations."""
    return {r['symbol_name'] for r in relocs if r['type_name'] == 'REL24'}


def collect_intra_tu_callees(coff, root_symbol, extract_fn, max_depth=None):
    """BFS from root's REL24 relocs, collecting transitive intra-TU callees.

    Handles both COMDAT-style sections (decomp: per-function .text$mn sections
    where REL24 targets name the callee directly) and monolithic sections
    (original: one big .text section where REL24 targets the section symbol
    and the callee offset is encoded in the instruction displacement).

    Args:
        coff: COFFParser instance
        root_symbol: mangled symbol name of the root function
        extract_fn: extract_from_decomp or extract_from_original
        max_depth: caps recursion depth (None = unlimited)

    Returns:
        dict of symbol_name -> (bytes, relocs) for each intra-TU callee
        (does NOT include the root symbol itself)
    """
    callees = {}  # symbol_name -> (bytes, relocs)
    visited = {root_symbol}
    queue = deque()

    # Get root's relocs to find initial targets
    root_bytes, root_relocs = extract_fn(coff, root_symbol)
    if root_bytes is None:
        return callees

    # Resolve section-symbol REL24 targets to actual function symbols
    root_relocs = resolve_section_rel24_targets(
        coff, root_symbol, root_bytes, root_relocs)

    # Seed BFS with root's REL24 targets
    for target in _get_rel24_targets(root_relocs):
        if target not in visited and is_intra_tu_callee(coff, target):
            queue.append((target, 1))
            visited.add(target)

    while queue:
        sym_name, depth = queue.popleft()

        callee_bytes, callee_relocs = extract_fn(coff, sym_name)
        if callee_bytes is None or len(callee_bytes) == 0:
            continue

        # Resolve section-symbol targets in callee relocs
        callee_relocs = resolve_section_rel24_targets(
            coff, sym_name, callee_bytes, callee_relocs)

        callees[sym_name] = (callee_bytes, callee_relocs)

        # Continue BFS if depth allows
        if max_depth is not None and depth >= max_depth:
            continue

        for target in _get_rel24_targets(callee_relocs):
            if target not in visited and is_intra_tu_callee(coff, target):
                queue.append((target, depth + 1))
                visited.add(target)

    return callees


def _has_bctr_switch(code_bytes, relocs, coff):
    """Check if a function uses bctr + .rdata references (switch table)."""
    has_bctr = False
    for i in range(0, len(code_bytes), 4):
        insn = struct.unpack_from(">I", code_bytes, i)[0]
        if insn == 0x4E800420:  # bctr
            has_bctr = True
            break

    if not has_bctr:
        return False

    # Check for .rdata references (indicates switch table)
    for reloc in relocs:
        if reloc['type_name'] in ('REFHI', 'REFLO'):
            sym = coff.symbol_map.get(reloc['symbol_name'])
            if sym and sym['section'] > 0:
                sec = coff.sections[sym['section'] - 1]
                if sec['name'].startswith('.rdata'):
                    return True

    return False


def build_coload_layout(root_symbol, root_bytes,
                        common_callees, decomp_callees, orig_callees,
                        decomp_coff, orig_coff):
    """Build sequential layout for root + co-loaded callees.

    Args:
        root_symbol: mangled symbol name of the root function
        root_bytes: bytes of the root function (used for sizing the root slot)
        common_callees: set of symbol names present in both decomp and orig
        decomp_callees: dict of symbol_name -> (bytes, relocs) from decomp
        orig_callees: dict of symbol_name -> (bytes, relocs) from orig
        decomp_coff: COFFParser for decomp
        orig_coff: COFFParser for orig

    Returns:
        ColoadResult or None if combined size exceeds 64KB
    """
    # Filter out problematic callees
    eligible = []
    for sym_name in sorted(common_callees):
        d_bytes, d_relocs = decomp_callees[sym_name]
        o_bytes, o_relocs = orig_callees[sym_name]

        # Skip callees with bctr_switch (switch table rdata handling is complex)
        if _has_bctr_switch(d_bytes, d_relocs, decomp_coff):
            continue
        if _has_bctr_switch(o_bytes, o_relocs, orig_coff):
            continue

        eligible.append(sym_name)

    if not eligible:
        return None

    # Build layout: root at offset 0, callees after (4-byte aligned)
    root_size = len(root_bytes)
    symbol_offsets = {root_symbol: 0}

    offset = (root_size + 3) & ~3  # 4-byte align after root

    coloaded_symbols = []
    for sym_name in eligible:
        d_bytes, _ = decomp_callees[sym_name]
        o_bytes, _ = orig_callees[sym_name]
        # Use max size for consistent offsets between decomp and orig
        callee_size = max(len(d_bytes), len(o_bytes))

        symbol_offsets[sym_name] = offset
        coloaded_symbols.append(sym_name)
        offset = (offset + callee_size + 3) & ~3  # 4-byte align

    total_size = offset

    # Check 64KB limit (CODE region size)
    if total_size > REGION_SIZE:
        return None

    return ColoadResult(
        symbol_offsets=symbol_offsets,
        coloaded_symbols=coloaded_symbols,
        total_size=total_size,
    )


def partition_relocs(relocs, intra_tu_symbols):
    """Filter out REL24 relocs targeting intra-TU symbols.

    Non-REL24 relocs (REFHI/REFLO/ADDR32/PAIR) pass through unchanged.
    Result is passed to assign_addresses so intra-TU symbols don't get
    trampoline stubs.

    Args:
        relocs: list of relocation dicts
        intra_tu_symbols: set of symbol names that are co-loaded

    Returns:
        list of relocs with intra-TU REL24 entries removed
    """
    return [r for r in relocs
            if not (r['type_name'] == 'REL24' and r['symbol_name'] in intra_tu_symbols)]


def adjust_relocs_to_layout(relocs, base_offset):
    """Adjust relocation offsets by adding base_offset.

    Used to shift callee relocs to their position in the combined buffer.

    Returns new list of adjusted reloc dicts (original list is not modified).
    """
    adjusted = []
    for r in relocs:
        adj = dict(r)
        adj['offset'] = r['offset'] + base_offset
        adjusted.append(adj)
    return adjusted
