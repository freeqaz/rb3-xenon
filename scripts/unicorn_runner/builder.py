"""Code preparation pipeline for the Unicorn Function Runner.

Owns the "prepare code for execution" phase — everything between extraction
and execute_function(). Two entry points:

- prepare_side(): non-coload path (single function)
- prepare_coloaded_side(): coload path (root + intra-TU callees in one buffer)
"""

from dataclasses import dataclass

from .coloader import adjust_relocs_to_layout, partition_relocs
from .memory_map import CODE_BASE
from .patcher import assign_addresses, patch_function, rewrite_ppc64_insns, prepare_switch_tables, prepare_data_sections


@dataclass
class PreparedSide:
    """Execution-ready code buffer with metadata."""
    code: bytearray        # Patched, execution-ready code buffer
    trampolines: dict      # symbol -> trampoline addr (external only)
    func_size: int          # Execution size (combined for coload, code len otherwise)
    rdata_bytes: bytes | None  # Switch table data for RDATA_BASE, if any


def prepare_side(code_bytes, relocs, coff, symbol, side_class):
    """Prepare a single function for execution (no co-loading).

    Steps: copy bytes -> rewrite PPC64 -> prepare switch tables -> assign
    addresses -> patch function.

    Returns PreparedSide.
    """
    code = bytearray(code_bytes)
    rewrite_ppc64_insns(code)

    rdata_bytes = None
    rdata_override = {}
    if side_class in ("bctr_switch", "bctrl_switch"):
        rdata_bytes, rdata_override = prepare_switch_tables(
            coff, symbol, relocs, CODE_BASE)
        if rdata_bytes is None:
            rdata_override = {}

    # Collect initialized data sections (.data*, .rdata*) referenced by relocs
    rdata_bytes, rdata_override = prepare_data_sections(
        coff, relocs, rdata_bytes, rdata_override)

    trampolines, globals_map = assign_addresses(relocs)
    globals_map.update(rdata_override)

    patch_function(code, relocs, trampolines, globals_map, CODE_BASE)

    return PreparedSide(
        code=code,
        trampolines=trampolines,
        func_size=len(code),
        rdata_bytes=rdata_bytes,
    )


def prepare_coloaded_side(root_bytes, root_relocs, coff, symbol, side_class,
                          callees, layout, intra_tu_addrs):
    """Prepare root + co-loaded callees in a single combined buffer.

    Steps: build combined buffer -> rewrite PPC64 -> collect adjusted relocs ->
    partition relocs -> assign addresses -> prepare switch tables -> merge
    targets -> patch function.

    Returns PreparedSide.
    """
    # Build combined buffer
    combined = bytearray(layout.total_size)
    combined[:len(root_bytes)] = root_bytes

    for csym in layout.coloaded_symbols:
        c_bytes, _ = callees[csym]
        off = layout.symbol_offsets[csym]
        combined[off:off + len(c_bytes)] = c_bytes

    # Rewrite PPC64 insns on entire combined buffer
    rewrite_ppc64_insns(combined)

    # Collect ALL relocs adjusted to combined-buffer offsets
    all_relocs = list(root_relocs)  # root relocs already at offset 0
    for csym in layout.coloaded_symbols:
        _, c_relocs = callees[csym]
        callee_offset = layout.symbol_offsets[csym]
        all_relocs.extend(adjust_relocs_to_layout(c_relocs, callee_offset))

    # Partition: external-only relocs for assign_addresses
    intra_tu_set = set(layout.coloaded_symbols) | {symbol}
    ext_relocs = partition_relocs(all_relocs, intra_tu_set)

    ext_trampolines, ext_globals = assign_addresses(ext_relocs)

    # Prepare switch table for root if needed
    rdata_bytes = None
    rdata_override = {}
    if side_class in ("bctr_switch", "bctrl_switch"):
        rdata_bytes, rdata_override = prepare_switch_tables(
            coff, symbol, root_relocs, CODE_BASE)
        if rdata_bytes is None:
            rdata_override = {}

    # Collect initialized data sections (.data*, .rdata*) referenced by all relocs
    rdata_bytes, rdata_override = prepare_data_sections(
        coff, all_relocs, rdata_bytes, rdata_override)

    ext_globals.update(rdata_override)

    # Merge: external trampolines + intra-TU real addresses
    merged_targets = dict(ext_trampolines)
    merged_targets.update(intra_tu_addrs)

    # Patch all relocs using merged targets
    patch_function(combined, all_relocs, merged_targets, ext_globals, CODE_BASE)

    return PreparedSide(
        code=combined,
        trampolines=ext_trampolines,
        func_size=layout.total_size,
        rdata_bytes=rdata_bytes,
    )
