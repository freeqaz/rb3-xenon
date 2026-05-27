#!/usr/bin/env python3
"""Dump vtable layout from original COFF .obj files.

Reads COFF symbol and relocation tables to reconstruct vtable entries,
mapping each slot to the actual function symbol. ICF-merged symbols are
noted so you can identify which virtual function each slot corresponds to.

Usage:
    python3 scripts/dump_vtable.py <class_name> [--obj <path>]
    python3 scripts/dump_vtable.py RndFontBase
    python3 scripts/dump_vtable.py RndFont3d --obj build/45410914/obj/system/rndobj/Font3d.obj

If --obj is not given, searches build/45410914/obj/ for a matching .obj file.
"""

import argparse
import glob
import os
import re
import struct
import subprocess
import sys


def read_coff_symbols(data):
    """Parse COFF symbol table and string table."""
    machine, num_sections, timestamp, symtab_offset, num_symbols, opt_hdr_size, flags = \
        struct.unpack_from('<HHIIIHH', data, 0)

    # String table immediately after symbol table
    strtab_offset = symtab_offset + num_symbols * 18
    strtab_size = struct.unpack_from('<I', data, strtab_offset)[0]
    strtab = data[strtab_offset:strtab_offset + strtab_size]

    def get_name(offset):
        if data[offset:offset + 4] == b'\x00\x00\x00\x00':
            str_offset = struct.unpack_from('<I', data, offset + 4)[0]
            end = strtab.index(b'\x00', str_offset)
            return strtab[str_offset:end].decode('ascii', errors='replace')
        else:
            return data[offset:offset + 8].rstrip(b'\x00').decode('ascii', errors='replace')

    # Read all symbols
    symbols = []
    i = 0
    while i < num_symbols:
        sym_offset = symtab_offset + i * 18
        name = get_name(sym_offset)
        value, section, type_val, storage, aux_count = \
            struct.unpack_from('<IhHBB', data, sym_offset + 8)
        symbols.append({
            'index': i,
            'name': name,
            'value': value,
            'section': section,
            'type': type_val,
            'storage': storage,
            'aux_count': aux_count,
        })
        i += 1 + aux_count

    # Read section headers
    section_hdr_offset = 20 + opt_hdr_size
    sections = []
    for s in range(num_sections):
        hdr_off = section_hdr_offset + s * 40
        sec_name_raw = data[hdr_off:hdr_off + 8].rstrip(b'\x00')
        if sec_name_raw.startswith(b'/'):
            # Long section name - offset into string table
            str_off = int(sec_name_raw[1:].decode('ascii'))
            end = strtab.index(b'\x00', str_off)
            sec_name = strtab[str_off:end].decode('ascii', errors='replace')
        else:
            sec_name = sec_name_raw.decode('ascii', errors='replace')
        vsize, vaddr, raw_size, raw_offset, reloc_offset, linenum_offset, \
            num_relocs, num_linenums, characteristics = \
            struct.unpack_from('<IIIIIIHHI', data, hdr_off + 8)
        sections.append({
            'name': sec_name,
            'vsize': vsize,
            'raw_size': raw_size,
            'raw_offset': raw_offset,
            'reloc_offset': reloc_offset,
            'num_relocs': num_relocs,
            'characteristics': characteristics,
        })

    return symbols, sections


def find_vtable(data, symbols, sections, class_name):
    """Find vtable symbol and read its relocation entries."""
    vtable_sym_name = f'??_7{class_name}@@6B@'

    # Find the vtable symbol
    vtable_sym = None
    for sym in symbols:
        if sym['name'] == vtable_sym_name:
            vtable_sym = sym
            break

    if vtable_sym is None:
        # Try partial match
        for sym in symbols:
            if f'??_7{class_name}' in sym['name'] and '6B' in sym['name']:
                vtable_sym = sym
                break

    if vtable_sym is None:
        return None, None

    # Find the section containing the vtable
    sec_idx = vtable_sym['section'] - 1  # 1-based
    if sec_idx < 0 or sec_idx >= len(sections):
        return vtable_sym, []

    section = sections[sec_idx]

    # Build symbol index lookup
    sym_by_idx = {}
    for sym in symbols:
        sym_by_idx[sym['index']] = sym

    # Read relocations for this section
    entries = []
    for r in range(section['num_relocs']):
        rel_off = section['reloc_offset'] + r * 10
        rva, sym_idx, rel_type = struct.unpack_from('<IIH', data, rel_off)
        target_sym = sym_by_idx.get(sym_idx, {'name': f'<unknown_{sym_idx}>'})
        entries.append({
            'offset': rva,
            'type': rel_type,
            'symbol': target_sym['name'],
        })

    return vtable_sym, entries


def demangle_symbol(mangled):
    """Try to demangle a MSVC mangled name."""
    try:
        result = subprocess.run(
            ['c++filt', '-n', mangled],
            capture_output=True, text=True, timeout=5
        )
        demangled = result.stdout.strip()
        if demangled != mangled:
            return demangled
    except (FileNotFoundError, subprocess.TimeoutExpired):
        pass

    # Basic manual demangling for common patterns
    if mangled.startswith('??_G'):
        # Scalar deleting destructor
        cls = mangled[4:].split('@@')[0]
        return f'{cls}::~{cls}() [scalar deleting]'
    if mangled.startswith('??1'):
        cls = mangled[3:].split('@@')[0]
        return f'{cls}::~{cls}()'
    if mangled.startswith('?'):
        parts = mangled[1:].split('@')
        if len(parts) >= 2:
            method = parts[0]
            cls = parts[1]
            return f'{cls}::{method}'

    return mangled


# Known ICF merge patterns - functions with identical machine code
ICF_HINTS = {
    'OnlyReturns': 'returns void/this (empty function or return this)',
}


def classify_icf(symbol, offset, all_entries):
    """Try to classify ICF-merged symbols based on context."""
    if symbol == 'OnlyReturns':
        return 'empty/returns'
    # If the symbol doesn't match the class, it's likely ICF-merged
    return None


def find_obj_file(class_name):
    """Search for the .obj file containing a class's vtable."""
    # Common name mappings
    search_names = [class_name]

    # Strip common prefixes
    if class_name.startswith('Rnd'):
        search_names.append(class_name[3:])  # RndFontBase -> FontBase
    if class_name.startswith('Ham'):
        search_names.append(class_name[3:])

    obj_dir = 'build/45410914/obj'
    for name in search_names:
        pattern = os.path.join(obj_dir, '**', f'{name}.obj')
        matches = glob.glob(pattern, recursive=True)
        if matches:
            # Prefer the one NOT under obj/obj/ (avoid duplicate)
            for m in matches:
                if '/obj/obj/' not in m:
                    return m
            return matches[0]

    return None


def get_vtable_layout(class_name, obj_path=None, project_root=None):
    """Get vtable layout as a list of dicts with offset, slot, symbol, demangled.

    Args:
        class_name: Class name (e.g., 'RndFontBase')
        obj_path: Path to .obj file (auto-detected if None)
        project_root: Project root for auto-detection (defaults to cwd)

    Returns:
        List of dicts: [{'slot': 0, 'offset': 0, 'symbol': '...', 'demangled': '...'}, ...]
        Empty list if vtable not found.
    """
    if project_root:
        old_cwd = os.getcwd()
        os.chdir(project_root)

    try:
        if not obj_path:
            obj_path = find_obj_file(class_name)
            if not obj_path:
                return []

        with open(obj_path, 'rb') as f:
            data = f.read()

        symbols, sections = read_coff_symbols(data)
        vtable_sym, entries = find_vtable(data, symbols, sections, class_name)

        if vtable_sym is None or not entries:
            return []

        result = []
        for i, entry in enumerate(entries):
            result.append({
                'slot': i,
                'offset': entry['offset'],
                'symbol': entry['symbol'],
                'demangled': demangle_symbol(entry['symbol']),
            })
        return result
    finally:
        if project_root:
            os.chdir(old_cwd)


def lookup_vtable_offset(class_name, offset, obj_path=None, project_root=None):
    """Look up which virtual function is at a given vtable offset.

    Args:
        class_name: Class name (e.g., 'RndFontBase')
        offset: Byte offset into vtable (e.g., 0x7c)
        obj_path: Path to .obj file (auto-detected if None)
        project_root: Project root for auto-detection

    Returns:
        Dict with slot info, or None if not found.
    """
    layout = get_vtable_layout(class_name, obj_path, project_root)
    for entry in layout:
        if entry['offset'] == offset:
            return entry
    return None


def enumerate_all_vtables(data, symbols, sections):
    """Find all vtable symbols (??_7) and their RTTI sub-object offsets.

    Returns list of dicts:
    [{'symbol': name, 'base_name': str, 'sub_object_offset': int|None,
      'section_idx': int, 'entries': [...]}]
    """
    # Build symbol index lookup
    sym_by_idx = {}
    for sym in symbols:
        sym_by_idx[sym['index']] = sym

    vtables = []
    for sym in symbols:
        name = sym['name']
        if not name.startswith('??_7') or '6B' not in name:
            continue

        # Extract base class name from mangled vtable symbol
        # ??_7Class@@6BBase@@@ -> Base
        # ??_7Class@@6B@ -> (primary, no base name)
        base_name = ""
        m = re.match(r'\?\?_7\w+@@6B(.+?)@@@?$', name)
        if m:
            # Handle nested names like Object@Hmx
            base_name = m.group(1).replace('@', '::')

        sec_idx = sym['section'] - 1
        if sec_idx < 0 or sec_idx >= len(sections):
            continue

        section = sections[sec_idx]

        # Read relocations for this section
        entries = []
        for r in range(section['num_relocs']):
            rel_off = section['reloc_offset'] + r * 10
            rva, sym_idx, rel_type = struct.unpack_from('<IIH', data, rel_off)
            target_sym = sym_by_idx.get(sym_idx, {'name': f'<unknown_{sym_idx}>'})
            entries.append({
                'offset': rva,
                'type': rel_type,
                'symbol': target_sym['name'],
            })

        # Try to find ??_R4 (RTTI Complete Object Locator) at end of vtable
        # It's the last relocation entry pointing to a ??_R4 symbol
        sub_object_offset = None
        for entry in entries:
            if entry['symbol'].startswith('??_R4'):
                # Found RTTI COL — read the sub-object offset from section data
                # The ??_R4 symbol itself is in another section; we need to find it
                r4_sym = None
                for s in symbols:
                    if s['name'] == entry['symbol']:
                        r4_sym = s
                        break
                if r4_sym and r4_sym['section'] > 0:
                    r4_sec_idx = r4_sym['section'] - 1
                    if r4_sec_idx < len(sections):
                        r4_section = sections[r4_sec_idx]
                        r4_data_off = r4_section['raw_offset'] + r4_sym['value']
                        # COL layout: signature(4), offset(4), cdOffset(4), ...
                        # offset at +4 is the sub-object offset (big-endian PPC)
                        if r4_data_off + 8 <= len(data):
                            sub_object_offset = struct.unpack_from('>I', data, r4_data_off + 4)[0]

        vtables.append({
            'symbol': name,
            'base_name': base_name,
            'sub_object_offset': sub_object_offset,
            'section_idx': sec_idx,
            'entries': entries,
        })

    return vtables


def resolve_vcall(class_name, sub_object_offset, vtable_slot, obj_path=None, project_root=None):
    """Resolve a virtual function call through a sub-object vtable.

    Args:
        class_name: Most-derived class name
        sub_object_offset: Byte offset from this-ptr to vtable load
        vtable_slot: Slot index (if < 100) or byte offset (if >= 100)
        obj_path: Path to .obj file (auto-detected if None)
        project_root: Project root for auto-detection

    Returns:
        Dict with resolution info, or error dict.
    """
    if project_root:
        old_cwd = os.getcwd()
        os.chdir(project_root)

    try:
        if not obj_path:
            obj_path = find_obj_file(class_name)
            if not obj_path:
                return {'error': f'Could not find .obj file for {class_name}'}

        with open(obj_path, 'rb') as f:
            data = f.read()

        symbols, sections = read_coff_symbols(data)
        vtables = enumerate_all_vtables(data, symbols, sections)

        if not vtables:
            return {'error': f'No vtable symbols found for {class_name} in {obj_path}'}

        # Auto-detect byte offset vs slot index
        if vtable_slot >= 100:
            vtable_slot = vtable_slot // 4

        # Find vtable matching the sub-object offset
        matched = None
        for vt in vtables:
            if vt['sub_object_offset'] == sub_object_offset:
                matched = vt
                break

        if matched is None:
            # List available offsets for diagnostics
            available = []
            for vt in vtables:
                available.append({
                    'symbol': vt['symbol'],
                    'base_name': vt['base_name'],
                    'sub_object_offset': vt['sub_object_offset'],
                })
            return {
                'error': f'No vtable at sub-object offset {sub_object_offset} for {class_name}',
                'available_vtables': available,
            }

        entries = matched['entries']

        # Check slot bounds (exclude ??_R4 at end)
        func_entries = [e for e in entries if not e['symbol'].startswith('??_R4')]
        if vtable_slot >= len(func_entries):
            return {
                'error': f'Slot {vtable_slot} out of range (vtable has {len(func_entries)} function slots)',
                'vtable_symbol': matched['symbol'],
                'base_name': matched['base_name'],
            }

        target_entry = func_entries[vtable_slot]
        target_sym = target_entry['symbol']
        demangled = demangle_symbol(target_sym)

        # Build all slots for context
        all_slots = []
        for i, e in enumerate(func_entries):
            slot_info = {
                'slot': i,
                'offset': f"0x{i * 4:02x}",
                'symbol': e['symbol'],
                'demangled': demangle_symbol(e['symbol']),
            }
            icf = classify_icf(e['symbol'], e['offset'], entries)
            if icf:
                slot_info['note'] = f'ICF: {icf}'
            all_slots.append(slot_info)

        # Determine confidence
        confidence = 'high'
        icf = classify_icf(target_sym, target_entry['offset'], entries)
        if icf:
            confidence = 'medium'

        result = {
            'resolved_function': demangled,
            'raw_symbol': target_sym,
            'vtable_symbol': matched['symbol'],
            'base_name': matched['base_name'],
            'sub_object_offset': sub_object_offset,
            'slot': vtable_slot,
            'slot_offset_hex': f"0x{vtable_slot * 4:02x}",
            'confidence': confidence,
            'all_slots': all_slots,
            'obj_file': obj_path,
        }
        if icf:
            result['icf_note'] = icf

        return result

    finally:
        if project_root:
            os.chdir(old_cwd)


def _run_resolve(argv):
    """Handle 'resolve' subcommand."""
    parser = argparse.ArgumentParser(prog='dump_vtable.py resolve',
                                     description='Resolve a virtual call')
    parser.add_argument('resolve_class', help='Most-derived class name')
    parser.add_argument('offset', type=lambda x: int(x, 0), help='Sub-object offset')
    parser.add_argument('slot', type=int, help='Vtable slot index')
    parser.add_argument('--obj', dest='resolve_obj', help='Path to .obj file')
    args = parser.parse_args(argv)

    result = resolve_vcall(args.resolve_class, args.offset, args.slot, obj_path=args.resolve_obj)
    if 'error' in result:
        print(f"Error: {result['error']}")
        if 'available_vtables' in result:
            print("\nAvailable vtables:")
            for vt in result['available_vtables']:
                print(f"  offset={vt['sub_object_offset']}  base={vt['base_name']:<30s}  {vt['symbol']}")
        sys.exit(1)

    print(f"Resolved: {result['resolved_function']}")
    print(f"Vtable:   {result['vtable_symbol']}")
    print(f"Base:     {result['base_name']}")
    print(f"Slot:     [{result['slot']}] at {result['slot_offset_hex']}")
    print(f"Confidence: {result['confidence']}")
    if 'icf_note' in result:
        print(f"ICF Note: {result['icf_note']}")
    print(f"\nAll slots in this vtable:")
    for s in result['all_slots']:
        marker = " <<" if s['slot'] == result['slot'] else ""
        note = f"  ({s['note']})" if 'note' in s else ""
        print(f"  [{s['slot']:3d}] {s['offset']}  {s['demangled']}{note}{marker}")


def main():
    # Handle subcommand routing before argparse
    if len(sys.argv) > 1 and sys.argv[1] == 'resolve':
        _run_resolve(sys.argv[2:])
        return

    parser = argparse.ArgumentParser(description='Dump vtable layout from original COFF .obj files')
    parser.add_argument('class_name', help='Class name (e.g., RndFontBase, RndFont3d)')
    parser.add_argument('--obj', help='Path to .obj file (auto-detected if not given)')
    parser.add_argument('--demangle', '-d', action='store_true', help='Attempt to demangle symbol names')
    parser.add_argument('--raw', action='store_true', help='Show raw mangled symbol names only')
    args = parser.parse_args()

    obj_path = args.obj
    if not obj_path:
        obj_path = find_obj_file(args.class_name)
        if not obj_path:
            print(f"Error: Could not find .obj file for {args.class_name}")
            print(f"Try: python3 {sys.argv[0]} {args.class_name} --obj <path_to_obj>")
            sys.exit(1)

    print(f"Reading: {obj_path}")

    with open(obj_path, 'rb') as f:
        data = f.read()

    symbols, sections = read_coff_symbols(data)
    vtable_sym, entries = find_vtable(data, symbols, sections, args.class_name)

    if vtable_sym is None:
        print(f"Error: No vtable symbol found for {args.class_name}")
        print(f"Available ??_7 symbols:")
        for sym in symbols:
            if '??_7' in sym['name']:
                print(f"  {sym['name']}")
        sys.exit(1)

    print(f"Vtable: {vtable_sym['name']} (section {vtable_sym['section']}, {len(entries)} entries)")
    print()

    # Known Object virtual function order for annotation
    OBJECT_VIRTUALS = [
        'dtor', 'RefOwner', 'Replace', 'ClassName', 'SetType',
        'Handle', 'SyncProperty', 'InitObject', 'Save', 'Copy',
        'Load', 'PreSave', 'PostSave', 'Print', 'Export',
        'SetTypeDef', 'ObjectDef', 'SetName', 'DataDir', 'PreLoad',
        'PostLoad', 'FindPathName',
    ]

    print(f'{"Slot":>4}  {"Offset":>6}  {"Symbol":<60}  {"Annotation"}')
    print('-' * 120)

    for i, entry in enumerate(entries):
        sym = entry['symbol']
        offset_hex = f"0x{entry['offset']:04x}"

        # Annotation
        annotation = ''
        if i < len(OBJECT_VIRTUALS):
            annotation = f'[Object] {OBJECT_VIRTUALS[i]}'

        # Show demangled or raw
        if args.raw:
            display = sym
        elif args.demangle:
            display = demangle_symbol(sym)
        else:
            display = sym

        # ICF detection
        icf = classify_icf(sym, entry['offset'], entries)
        if icf:
            annotation += f' ({icf})'

        print(f'[{i:3d}]  {offset_hex}  {display:<60}  {annotation}')


if __name__ == '__main__':
    main()
