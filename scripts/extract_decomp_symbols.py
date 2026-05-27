#!/usr/bin/env python3
"""Extract data symbol names from decomp .obj files and map to original VAs.

For Matching units, the decomp .obj exports data symbols with real C++ names.
The split .obj uses lbl_* names for the same data. This script maps between them
so we can update symbols.txt with real names, enabling Config B linking to resolve.

Usage:
    python3 scripts/extract_decomp_symbols.py [--unit UNIT] [--apply] [--verbose]
"""

import argparse
import json
import os
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
BUILD_DIR = PROJECT_ROOT / "build" / "45410914"
SRC_OBJ_DIR = BUILD_DIR / "src"
SPLIT_OBJ_DIR = BUILD_DIR / "obj"
CONFIG_DIR = PROJECT_ROOT / "config" / "45410914"
SPLITS_PATH = CONFIG_DIR / "splits.txt"
SYMBOLS_PATH = CONFIG_DIR / "symbols.txt"
OBJECTS_PATH = CONFIG_DIR / "objects.json"

# COFF constants
IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3
IMAGE_SYM_CLASS_LABEL = 6
IMAGE_SYM_UNDEFINED = 0


def read_coff_header(data):
    """Read COFF file header (20 bytes).
    Format: Machine(2) NumSections(2) TimeDateStamp(4) SymbolTableOffset(4) NumSymbols(4) OptHdrSize(2) Characteristics(2)
    """
    if len(data) < 20:
        return None
    machine, num_sections, timestamp, sym_offset, num_symbols, opt_size, characteristics = struct.unpack_from(
        '<HHlIIHH', data, 0
    )
    return {
        'machine': machine,
        'num_sections': num_sections,
        'symbol_table_offset': sym_offset,
        'num_symbols': num_symbols,
        'optional_header_size': opt_size,
    }


def read_section_headers(data, num_sections, offset=20):
    """Read COFF section headers. Returns list of section dicts."""
    sections = []
    for i in range(num_sections):
        pos = offset + i * 40
        name_bytes = data[pos:pos+8]
        virtual_size, virtual_addr, raw_size, raw_offset, reloc_offset, _, num_relocs, _, characteristics = struct.unpack_from(
            '<IIIIIIHHI', data, pos + 8
        )
        # Decode section name
        if name_bytes[0:1] == b'/':
            # Long name: offset into string table
            try:
                str_offset = int(name_bytes[1:].rstrip(b'\x00').decode('ascii'))
                name = None  # Will resolve from string table
            except ValueError:
                name = name_bytes.rstrip(b'\x00').decode('ascii', errors='replace')
                str_offset = None
        else:
            name = name_bytes.rstrip(b'\x00').decode('ascii', errors='replace')
            str_offset = None

        sections.append({
            'name': name,
            'name_str_offset': str_offset if name is None else None,
            'virtual_size': virtual_size,
            'virtual_addr': virtual_addr,
            'raw_size': raw_size,
            'raw_offset': raw_offset,
            'reloc_offset': reloc_offset,
            'num_relocs': num_relocs,
            'characteristics': characteristics,
        })
    return sections


def read_string_table(data, sym_offset, num_symbols):
    """Read COFF string table (follows symbol table)."""
    str_table_offset = sym_offset + num_symbols * 18
    if str_table_offset + 4 > len(data):
        return {}

    str_table_size = struct.unpack_from('<I', data, str_table_offset)[0]
    str_table_data = data[str_table_offset:str_table_offset + str_table_size]

    # Build offset -> string map
    strings = {}
    pos = 4  # Skip the size field
    while pos < len(str_table_data):
        end = str_table_data.find(b'\x00', pos)
        if end == -1:
            break
        strings[pos] = str_table_data[pos:end].decode('ascii', errors='replace')
        pos = end + 1

    return strings


def read_symbols(data, header, sections, string_table):
    """Read COFF symbol table. Returns list of symbol dicts."""
    symbols = []
    sym_offset = header['symbol_table_offset']
    num_symbols = header['num_symbols']

    i = 0
    while i < num_symbols:
        pos = sym_offset + i * 18
        if pos + 18 > len(data):
            break

        name_bytes = data[pos:pos+8]
        value, section_num, sym_type, storage_class, num_aux = struct.unpack_from(
            '<IhHBB', data, pos + 8
        )

        # Decode symbol name
        if name_bytes[:4] == b'\x00\x00\x00\x00':
            # Long name: offset into string table
            str_offset = struct.unpack_from('<I', name_bytes, 4)[0]
            name = string_table.get(str_offset, f'<str#{str_offset}>')
        else:
            name = name_bytes.rstrip(b'\x00').decode('ascii', errors='replace')

        symbols.append({
            'name': name,
            'value': value,  # Section-relative offset
            'section_number': section_num,  # 1-based, 0=UNDEF, -1=ABS, -2=DEBUG
            'type': sym_type,
            'storage_class': storage_class,
            'num_aux': num_aux,
            'index': i,
        })

        i += 1 + num_aux  # Skip aux symbols

    return symbols


def parse_coff(filepath):
    """Parse a COFF .obj file. Returns (sections, symbols) or None on error."""
    try:
        with open(filepath, 'rb') as f:
            data = f.read()
    except (FileNotFoundError, PermissionError):
        return None

    header = read_coff_header(data)
    if header is None or header['num_symbols'] == 0:
        return None

    opt_size = header['optional_header_size']
    sections = read_section_headers(data, header['num_sections'], 20 + opt_size)
    string_table = read_string_table(data, header['symbol_table_offset'], header['num_symbols'])

    # Resolve long section names
    for s in sections:
        if s['name'] is None and s['name_str_offset'] is not None:
            s['name'] = string_table.get(s['name_str_offset'], '<unknown>')

    symbols = read_symbols(data, header, sections, string_table)

    return sections, symbols


def parse_splits():
    """Parse splits.txt to get unit -> section ranges mapping."""
    units = {}  # unit_name -> {section_name: [(start, end)]}

    current_unit = None
    with open(SPLITS_PATH) as f:
        for line in f:
            line = line.rstrip()
            if not line or line.startswith('Sections:'):
                continue
            if not line.startswith('\t') and line.endswith(':'):
                current_unit = line[:-1]
                if current_unit not in units:
                    units[current_unit] = defaultdict(list)
            elif line.startswith('\t') and current_unit:
                m = re.match(r'\t(\S+)\s+start:0x([0-9A-Fa-f]+)\s+end:0x([0-9A-Fa-f]+)', line)
                if m:
                    section = m.group(1)
                    start = int(m.group(2), 16)
                    end = int(m.group(3), 16)
                    # Strip rename suffix for section matching
                    base_section = section
                    if 'rename:' in line:
                        rename_m = re.search(r'rename:(\S+)', line)
                        if rename_m:
                            base_section = rename_m.group(1)
                    units[current_unit][section].append((start, end, base_section))

    return units


def get_matching_units():
    """Get set of unit paths that are Matching in objects.json."""
    with open(OBJECTS_PATH) as f:
        objects = json.load(f)

    matching = set()
    for lib, lib_config in objects.items():
        for path, obj_config in lib_config.get('objects', {}).items():
            status = obj_config if isinstance(obj_config, str) else obj_config.get('status', 'MISSING')
            if status == 'Matching':
                matching.add(path)

    return matching


def find_decomp_obj(unit_name):
    """Find the decomp .obj file for a unit."""
    # Unit name like "system/rndobj/Fur.cpp" -> build/45410914/src/system/rndobj/Fur.obj
    base = unit_name
    if base.endswith('.cpp') or base.endswith('.c'):
        base = base.rsplit('.', 1)[0]
    obj_path = SRC_OBJ_DIR / (base + '.obj')
    if obj_path.exists():
        return obj_path
    return None


def map_decomp_symbols_to_va(unit_name, splits_data):
    """For a Matching unit, extract decomp .obj data symbols and map to original VAs.

    Returns: list of (original_va, symbol_name, section_name, size) tuples
    """
    obj_path = find_decomp_obj(unit_name)
    if obj_path is None:
        return []

    result = parse_coff(obj_path)
    if result is None:
        return []

    sections, symbols = result
    unit_ranges = splits_data.get(unit_name, {})

    mappings = []

    for sym in symbols:
        # Only care about defined symbols (not UNDEF, not ABS, not DEBUG)
        if sym['section_number'] <= 0:
            continue

        # Only care about EXTERNAL or STATIC symbols (data definitions)
        if sym['storage_class'] not in (IMAGE_SYM_CLASS_EXTERNAL, IMAGE_SYM_CLASS_STATIC):
            continue

        section_idx = sym['section_number'] - 1  # 0-based
        if section_idx >= len(sections):
            continue

        section = sections[section_idx]
        section_name = section['name']

        # Skip code sections - we only care about data
        chars = section['characteristics']
        is_code = bool(chars & 0x00000020)  # IMAGE_SCN_CNT_CODE
        if is_code:
            continue

        # Skip COMDAT sections - these have their own resolution mechanism
        is_comdat = bool(chars & 0x00001000)  # IMAGE_SCN_LNK_COMDAT
        if is_comdat:
            continue

        # Skip .pdata sections (exception handling data)
        if '.pdata' in section_name:
            continue

        # Skip symbols that are section symbols (name == section name)
        if sym['name'] == section_name:
            continue

        # Skip auto-generated names (these aren't useful for mapping)
        if sym['name'].startswith('lbl_') or sym['name'].startswith('fn_'):
            continue

        # Skip compiler temp symbols
        if sym['name'].startswith('$T') or sym['name'].startswith('$S'):
            continue

        # Map section-relative offset to original VA
        # Match decomp section to original section using splits.txt
        offset_in_section = sym['value']

        # Find matching original section range
        # The section name in the decomp .obj should match one in splits.txt
        # But MSVC uses section suffixes like .rdata$r, .data, .bss etc.
        base_section = section_name.split('$')[0]  # .rdata$r -> .rdata

        if base_section in unit_ranges:
            ranges = unit_ranges[base_section]
            # Simple case: one range for this section in this unit
            if len(ranges) == 1:
                start_va, end_va, _ = ranges[0]
                original_va = start_va + offset_in_section
                if original_va < end_va:
                    mappings.append((original_va, sym['name'], base_section, sym))
            else:
                # Multiple ranges: need to figure out which one this offset maps to
                # Concatenate ranges in order, map offset into combined space
                cumulative = 0
                for start_va, end_va, _ in ranges:
                    range_size = end_va - start_va
                    if offset_in_section < cumulative + range_size:
                        original_va = start_va + (offset_in_section - cumulative)
                        mappings.append((original_va, sym['name'], base_section, sym))
                        break
                    cumulative += range_size

    return mappings


def load_current_symbols():
    """Load current symbols.txt as a dict of address -> (name, full_line)."""
    symbols = {}  # address -> (name, full_line)
    with open(SYMBOLS_PATH) as f:
        for line in f:
            line = line.rstrip()
            m = re.match(r'(\S+)\s*=\s*(\.\w+):0x([0-9A-Fa-f]+)', line)
            if m:
                name = m.group(1)
                section = m.group(2)
                addr = int(m.group(3), 16)
                symbols[addr] = (name, line)
    return symbols


def main():
    parser = argparse.ArgumentParser(description='Extract decomp symbols and map to original VAs')
    parser.add_argument('--unit', type=str, help='Process single unit (e.g., system/rndobj/Fur.cpp)')
    parser.add_argument('--apply', action='store_true', help='Update symbols.txt with real names')
    parser.add_argument('--verbose', action='store_true', help='Show detailed output')
    parser.add_argument('--stats-only', action='store_true', help='Only show statistics')
    args = parser.parse_args()

    print("Loading splits.txt...")
    splits_data = parse_splits()

    print("Loading objects.json...")
    matching_units = get_matching_units()

    if args.unit:
        units_to_process = [args.unit]
    else:
        units_to_process = sorted(matching_units)

    print(f"Processing {len(units_to_process)} units...")

    all_mappings = []  # (va, real_name, section, unit)
    units_with_mappings = 0
    units_no_obj = 0

    for unit in units_to_process:
        if unit not in splits_data:
            continue

        mappings = map_decomp_symbols_to_va(unit, splits_data)
        if mappings:
            units_with_mappings += 1
            for va, name, section, sym_info in mappings:
                all_mappings.append((va, name, section, unit))
                if args.verbose:
                    print("  0x%08X -> %s (%s in %s)" % (va, name, section, unit))
        elif find_decomp_obj(unit) is None:
            units_no_obj += 1

    print(f"\nResults:")
    print(f"  Units with decomp .obj: {len(units_to_process) - units_no_obj}")
    print(f"  Units with mappings: {units_with_mappings}")
    print(f"  Total data symbol mappings: {len(all_mappings)}")

    if args.stats_only:
        return

    # Check which mappings replace lbl_* symbols
    print("\nLoading symbols.txt...")
    current_symbols = load_current_symbols()

    replacements = []  # (va, old_name, new_name, section, unit)
    for va, new_name, section, unit in all_mappings:
        if va in current_symbols:
            old_name, old_line = current_symbols[va]
            if old_name.startswith('lbl_'):
                replacements.append((va, old_name, new_name, section, unit))

    print(f"  lbl_* symbols that can be renamed: {len(replacements)}")

    # Show sample
    if replacements and not args.stats_only:
        print("\nSample replacements:")
        for va, old_name, new_name, section, unit in replacements[:20]:
            print("  %s -> %s (%s)" % (old_name, new_name, unit))

    if args.apply and replacements:
        print(f"\nApplying {len(replacements)} replacements to symbols.txt...")
        # Build replacement map
        va_to_new_name = {va: new_name for va, _, new_name, _, _ in replacements}

        lines_out = []
        replaced = 0
        with open(SYMBOLS_PATH) as f:
            for line in f:
                m = re.match(r'(\S+)\s*(=\s*\.\w+:0x([0-9A-Fa-f]+).*)', line.rstrip())
                if m:
                    old_name = m.group(1)
                    rest = m.group(2)
                    addr = int(m.group(3), 16)
                    if addr in va_to_new_name and old_name.startswith('lbl_'):
                        new_name = va_to_new_name[addr]
                        line = new_name + ' ' + rest + '\n'
                        replaced += 1
                lines_out.append(line)

        with open(SYMBOLS_PATH, 'w') as f:
            f.writelines(lines_out)

        print(f"  Replaced {replaced} symbols in symbols.txt")
        print("  Run 'dtk xex split' to regenerate split .obj files with real names")


if __name__ == '__main__':
    main()
