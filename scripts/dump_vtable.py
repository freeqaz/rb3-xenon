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


def _legacy_select(symbols, class_name):
    """The PRE-2026-09-14 selection rule. Kept ONLY as the must-fail control of
    --selftest; never call it for real work.  It took the first symbol merely
    CONTAINING f'??_7{class}' and '6B', i.e. whatever COFF symbol-table order
    happened to put first."""
    want = f'??_7{class_name}@@6B@'
    for sym in symbols:
        if sym['name'] == want:
            return sym
    for sym in symbols:
        if f'??_7{class_name}' in sym['name'] and '6B' in sym['name']:
            return sym
    return None


def select_vtable_symbol(symbols, class_name, which=None):
    """Pick the PRIMARY vtable for `class_name`, deterministically.

    Returns (chosen_symbol_or_None, ordered_candidate_names).

    Two defects in the old rule this replaces (lane W16-P, 2026-09-14):

    1. It never selected the virtual-base primary.  A class with a virtual base
       has no `??_7C@@6B@` at all; its primary is `??_7C@@6B0@@` and its
       secondary tables are `??_7C@@6B<Base>@@@`.  The old fallback returned
       whichever came first in the symbol table, so `Server` yielded
       `??_7Server@@6BMsgSource@@@` -- a secondary table -- from our COMPILED
       obj.  (From the dtk TARGET obj the same rule happens to land on the
       primary, which is why the bug stayed latent: the default `--obj`
       resolution hides it.  See --selftest.)
    2. `f'??_7{class}' in name` is an UNANCHORED substring test, so class `Set`
       also matches `??_7Setlist...`.  Now anchored on the full
       f'??_7{class}@@6B' prefix.

    Preference: exact single-inheritance primary, then virtual-base primary,
    then remaining tables sorted by NAME (never symbol-table order, which is
    not a property of the class).
    """
    prefix = f'??_7{class_name}@@6B'
    cands = [s for s in symbols if s['name'].startswith(prefix)]
    # de-dup by name, keep first occurrence of each
    seen, uniq = set(), []
    for s in cands:
        if s['name'] not in seen:
            seen.add(s['name'])
            uniq.append(s)

    def rank(sym):
        n = sym['name']
        if n == f'??_7{class_name}@@6B@':
            return (0, n)          # single-inheritance primary
        if n == f'??_7{class_name}@@6B0@@':
            return (1, n)          # virtual-base primary
        return (2, n)              # secondary / base sub-object tables

    uniq.sort(key=rank)
    names = [s['name'] for s in uniq]

    if which:
        for s in uniq:
            if s['name'] == which or s['name'].endswith(which):
                return s, names
        return None, names

    return (uniq[0] if uniq else None), names


def find_vtable(data, symbols, sections, class_name, which=None):
    """Find vtable symbol and read its relocation entries."""
    vtable_sym, _cands = select_vtable_symbol(symbols, class_name, which)

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



def _selftest():
    """Prove the selection fix with a control that MUST fail under the old rule.

    Fixture: `Server`, in OUR COMPILED obj (not the dtk target obj).  Server has a
    virtual base, so it has NO `??_7Server@@6B@`; its primary is
    `??_7Server@@6B0@@` and its secondary is `??_7Server@@6BMsgSource@@@`.  In the
    compiled obj the secondary is listed FIRST, so the old rule returns it.

    A selftest that cannot fail is worthless, so this asserts BOTH directions:
      (1) the legacy rule picks the WRONG table on this fixture.  If it ever picks
          the right one the fixture has stopped being a trap and the test exits
          non-zero as VACUOUS rather than passing.
      (2) the new rule picks the primary.
    Exit 0 = pass, 1 = fail, 2 = could not run, 3 = vacuous fixture.
    """
    import glob as _glob
    root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))  # scripts/ -> repo root
    cands = _glob.glob(os.path.join(root, 'build/45410914/src/**/Server.obj'), recursive=True)
    cands = [c for c in cands if os.path.basename(c) == 'Server.obj']
    if not cands:
        print('SELFTEST: UNRUNNABLE -- no compiled build/45410914/src/**/Server.obj '
              '(build the tree first)')
        return 2
    obj = cands[0]
    data = open(obj, 'rb').read()
    symbols, _sections = read_coff_symbols(data)

    PRIMARY   = '??_7Server@@6B0@@'
    SECONDARY = '??_7Server@@6BMsgSource@@@'
    present = {s['name'] for s in symbols if s['name'].startswith('??_7Server@@6B')}
    print(f'SELFTEST fixture: {obj}')
    print(f'  Server vtable symbols present: {sorted(present)}')
    if not {PRIMARY, SECONDARY} <= present:
        print(f'SELFTEST: UNRUNNABLE -- fixture needs both {PRIMARY} and {SECONDARY}')
        return 2

    legacy = _legacy_select(symbols, 'Server')
    legacy_name = legacy['name'] if legacy else None
    print(f'  [control] legacy rule -> {legacy_name}')
    if legacy_name != SECONDARY:
        print(f'SELFTEST: VACUOUS -- the legacy rule was expected to pick the SECONDARY '
              f'table {SECONDARY} on this fixture but picked {legacy_name}. The control '
              f'no longer fails, so a pass would prove nothing.')
        return 3

    chosen, cand_names = select_vtable_symbol(symbols, 'Server')
    chosen_name = chosen['name'] if chosen else None
    print(f'  [fixed]   new rule    -> {chosen_name}')
    if chosen_name != PRIMARY:
        print(f'SELFTEST: FAIL -- new rule picked {chosen_name}, expected {PRIMARY}')
        return 1

    ov, _ = select_vtable_symbol(symbols, 'Server', which='6BMsgSource@@@')
    if not ov or ov['name'] != SECONDARY:
        print(f'SELFTEST: FAIL -- --which suffix override did not reach {SECONDARY}')
        return 1
    print(f'  [--which] suffix override -> {ov["name"]}')

    # anchoring: an unanchored substring test would let a longer class name in
    bogus, _ = select_vtable_symbol(symbols, 'Serv')
    if bogus is not None:
        print(f'SELFTEST: FAIL -- prefix "Serv" matched {bogus["name"]}; selection is '
              f'not anchored on f"??_7{{class}}@@6B"')
        return 1
    print('  [anchor]  class "Serv" correctly matches nothing')

    print('SELFTEST: PASS (control failed as required, fix selects the virtual-base primary)')
    return 0


def main():
    # Handle subcommand routing before argparse
    if len(sys.argv) > 1 and sys.argv[1] == 'resolve':
        _run_resolve(sys.argv[2:])
        return
    if len(sys.argv) > 1 and sys.argv[1] in ('--selftest', 'selftest'):
        sys.exit(_selftest())

    parser = argparse.ArgumentParser(description='Dump vtable layout from original COFF .obj files')
    parser.add_argument('class_name', help='Class name (e.g., RndFontBase, RndFont3d)')
    parser.add_argument('--obj', help='Path to .obj file (auto-detected if not given)')
    parser.add_argument('--demangle', '-d', action='store_true', help='Attempt to demangle symbol names')
    parser.add_argument('--raw', action='store_true', help='Show raw mangled symbol names only')
    parser.add_argument('--which', help='Select a specific vtable symbol (full mangled name, or a '
                                        'suffix of it such as "6BMsgSource@@@"). Default: the PRIMARY.')
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
    _chosen, _cands = select_vtable_symbol(symbols, args.class_name, args.which)
    vtable_sym, entries = find_vtable(data, symbols, sections, args.class_name, args.which)

    if vtable_sym is None:
        print(f"Error: No vtable symbol found for {args.class_name}")
        print(f"Available ??_7 symbols:")
        for sym in symbols:
            if '??_7' in sym['name']:
                print(f"  {sym['name']}")
        sys.exit(1)

    # ALWAYS say which symbol was chosen and what else was on offer -- the old
    # rule silently returned a secondary table and nothing in the output said so.
    kind = ('primary (single inheritance)' if vtable_sym['name'] == f'??_7{args.class_name}@@6B@'
            else 'PRIMARY (virtual base)' if vtable_sym['name'] == f'??_7{args.class_name}@@6B0@@'
            else 'secondary / base sub-object table')
    print(f"Vtable: {vtable_sym['name']}")
    print(f"  selected: {kind}{' [--which override]' if args.which else ''}")
    print(f"  section {vtable_sym['section']}, {len(entries)} entries")
    if len(_cands) > 1:
        others = [n for n in _cands if n != vtable_sym['name']]
        print(f"  other vtables on {args.class_name} ({len(others)}): {', '.join(others)}")
        print(f"  (use --which <name> to dump one of those)")
    if vtable_sym['section'] == 0:
        print("  WARNING: symbol has section 0 (no data in this obj) -- 0 slots is an "
              "artifact of the obj, not a property of the class. Try the other obj.")
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
