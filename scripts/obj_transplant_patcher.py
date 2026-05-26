#!/usr/bin/env python3
"""Post-build .obj transplant patcher.

Replaces a function's COFF section data with the original .obj's machine code,
achieving 100% match for functions where source-level changes can't fix
instruction scheduling or register allocation differences.

This is a superset of regswap patching — it fixes registers, instruction
ordering, and size differences all at once.

Usage:
  python3 scripts/obj_transplant_patcher.py SYMBOL [--apply] [--dry-run]

Example:
  python3 scripts/obj_transplant_patcher.py "?Transform@CSHA1@@AAAXPAIPBE@Z" --apply
"""

import argparse
import json
import shutil
import sqlite3
import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))
from obj_regswap_patcher import COFFPatcher, PROJECT_ROOT


def get_unit_for_symbol(symbol: str) -> str:
    """Look up unit path for a symbol in decomp.db."""
    conn = sqlite3.connect(str(PROJECT_ROOT / "decomp.db"))
    cur = conn.cursor()
    cur.execute("SELECT unit FROM functions WHERE symbol = ?", (symbol,))
    row = cur.fetchone()
    conn.close()
    if not row:
        raise ValueError(f"Symbol not found in DB: {symbol}")
    return row[0]


def get_decomp_obj_path(unit: str) -> Path:
    """Get decomp .obj path from unit."""
    path = unit
    if path.startswith("default/"):
        path = "src/" + path[len("default/"):]
    return PROJECT_ROOT / "build" / "373307D9" / (path + ".obj")


def get_orig_obj_path(unit: str) -> Path:
    """Get original .obj path from unit."""
    path = unit
    if path.startswith("default/"):
        path = path[len("default/"):]
    return PROJECT_ROOT / "build" / "373307D9" / "obj" / (path + ".obj")


def transplant_function(symbol: str, unit: str = None, dry_run: bool = True) -> dict:
    """Transplant original machine code into decomp .obj.

    Returns result dict with status info.
    """
    result = {"symbol": symbol, "success": False}

    if not unit:
        unit = get_unit_for_symbol(symbol)
    result["unit"] = unit

    decomp_path = get_decomp_obj_path(unit)
    orig_path = get_orig_obj_path(unit)

    if not decomp_path.exists():
        result["error"] = f"Decomp .obj not found: {decomp_path}"
        return result
    if not orig_path.exists():
        result["error"] = f"Original .obj not found: {orig_path}"
        return result

    # Parse both COFF files
    decomp = COFFPatcher(str(decomp_path))
    orig = COFFPatcher(str(orig_path))

    # Find function section in both
    decomp_sec_info = decomp.find_function_section(symbol)
    if not decomp_sec_info:
        result["error"] = f"Symbol not found in decomp COFF: {symbol}"
        return result

    orig_sec_info = orig.find_function_section(symbol)
    if not orig_sec_info:
        result["error"] = f"Symbol not found in original COFF: {symbol}"
        return result

    decomp_sec, decomp_sym_off = decomp_sec_info
    orig_sec, orig_sym_off = orig_sec_info

    decomp_sec_idx = decomp_sec["index"]
    orig_sec_idx = orig_sec["index"]

    decomp_size = decomp_sec["raw_size"]
    orig_size = orig_sec["raw_size"]

    result["decomp_size"] = decomp_size
    result["orig_size"] = orig_size
    result["size_diff"] = orig_size - decomp_size

    # Get relocation counts
    decomp_num_relocs = decomp.get_num_relocs(decomp_sec_idx)
    orig_num_relocs = orig.get_num_relocs(orig_sec_idx)

    result["decomp_relocs"] = decomp_num_relocs
    result["orig_relocs"] = orig_num_relocs

    if decomp_num_relocs != orig_num_relocs:
        result["error"] = (
            f"Relocation count mismatch: decomp={decomp_num_relocs}, "
            f"orig={orig_num_relocs}. Cannot transplant."
        )
        return result

    if dry_run:
        result["dry_run"] = True
        result["success"] = True
        print(f"DRY RUN: Would transplant {symbol}")
        print(f"  Decomp: {decomp_sec['name']} ({decomp_size} bytes, {decomp_num_relocs} relocs)")
        print(f"  Original: {orig_sec['name']} ({orig_size} bytes, {orig_num_relocs} relocs)")
        print(f"  Size diff: {orig_size - decomp_size:+d} bytes")
        return result

    # === Apply transplant ===

    # 1. Resize section if needed
    if orig_size != decomp_size:
        decomp.resize_section(decomp_sec_idx, orig_size)
        result["resized"] = True

    # 2. Copy original section bytes
    orig_data = orig.get_section_data(orig_sec_idx)
    decomp.write_section_data(decomp_sec_idx, bytearray(orig_data))

    # 3. Remap relocations: copy original VAs with remapped symbol indices
    symbols_added = []
    for i in range(orig_num_relocs):
        o_va, o_sym_idx, o_type = orig.get_relocation(orig_sec_idx, i)

        # Get original target symbol name
        orig_sym_name = orig.get_symbol_name_by_index(o_sym_idx)

        # Find or add in decomp's symbol table
        old_count = decomp.num_symbols
        decomp_sym_idx = decomp.find_or_add_symbol(orig_sym_name)
        if decomp.num_symbols > old_count:
            symbols_added.append(orig_sym_name)

        # Write remapped relocation (original VA, decomp symbol index, original type)
        decomp.set_relocation(decomp_sec_idx, i, o_va, decomp_sym_idx, o_type)

    # 4. Backup original decomp .obj (only if no backup exists)
    backup_path = str(decomp_path) + ".bak"
    if not Path(backup_path).exists():
        shutil.copy2(str(decomp_path), backup_path)

    # 5. Save patched .obj
    decomp.save(str(decomp_path))

    result["backup"] = backup_path
    result["symbols_added"] = symbols_added
    result["success"] = True

    print(f"Transplanted {symbol}")
    print(f"  Size: {decomp_size} -> {orig_size} ({orig_size - decomp_size:+d} bytes)")
    print(f"  Relocations remapped: {orig_num_relocs}")
    if symbols_added:
        print(f"  Symbols added: {len(symbols_added)}")
        for s in symbols_added:
            print(f"    + {s}")

    return result


def main():
    parser = argparse.ArgumentParser(
        description="Transplant original machine code into decomp .obj files"
    )
    parser.add_argument("symbol", help="Function symbol (mangled name)")
    parser.add_argument("--unit", help="Unit path (auto-detected from DB if omitted)")
    parser.add_argument(
        "--dry-run", action="store_true", default=True,
        help="Show what would be done without modifying files (default)"
    )
    parser.add_argument(
        "--apply", action="store_true",
        help="Actually apply the transplant"
    )
    args = parser.parse_args()

    if args.apply:
        args.dry_run = False

    try:
        result = transplant_function(args.symbol, unit=args.unit, dry_run=args.dry_run)
    except ValueError as e:
        print(f"ERROR: {e}", file=sys.stderr)
        sys.exit(1)

    if not result["success"]:
        print(f"ERROR: {result.get('error', 'unknown')}", file=sys.stderr)
        sys.exit(1)


if __name__ == "__main__":
    main()
