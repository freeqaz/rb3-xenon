#!/usr/bin/env python3
"""Post-build patcher: convert $S guard variables to ??_B format.

Compares decomp .obj files against original .obj files and renames
$S (STATIC, uint) guard variables to match the original's ??_B
(EXTERNAL, char) naming where applicable. Also renumbers remaining
$S counters to match the original.

Machine code is byte-identical between the two guard types, so only
the COFF symbol table needs patching.

Usage:
    python3 scripts/obj_guard_patcher.py --batch [--apply] [--verbose]
"""

import argparse
import glob
import os
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3


def extract_guard_scope(name):
    """Extract function scope from a guard variable symbol name.

    $S format:  ?$S<N>@<scope>@4IA  -> returns <scope>
    ??_B format: ??_B<scope>@5<enc> -> returns <scope>

    The scope uniquely identifies the static local's enclosing function.
    """
    # $S guard: ?$S<digits>@<scope>@4IA
    m = re.match(r'^\?\$S(\d+)@(.+)@4IA$', name)
    if m:
        return m.group(2)

    # ??_B guard: ??_B<scope>@5<encoded_scope_num>
    if name.startswith('??_B'):
        inner = name[4:]  # strip ??_B prefix
        # The scope starts with ?<num>?? where num is MSVC-encoded
        m2 = re.match(r'^(\?[^?]+\?\?.+)', inner)
        if m2:
            scope_and_suffix = m2.group(1)
            # Extract the scope number from ?<num>??
            m3 = re.match(r'^\?([^?]+)\?\?', scope_and_suffix)
            if m3:
                encoded_num = m3.group(1)
                suffix = '@5' + encoded_num
                if scope_and_suffix.endswith(suffix):
                    return scope_and_suffix[:-len(suffix)]
    return None


def parse_coff_guard_symbols(data):
    """Parse COFF symbol table, return guard-related symbols.

    Returns list of (name, entry_offset, storage_class, str_abs_offset, str_len)
    where str_abs_offset is the position in the file where the name string starts
    (for long names in string table) or -1 for short names.
    """
    if len(data) < 20:
        return []

    sym_offset = struct.unpack_from('<I', data, 8)[0]
    num_syms = struct.unpack_from('<I', data, 12)[0]
    if sym_offset == 0 or num_syms == 0:
        return []

    str_table_offset = sym_offset + num_syms * 18
    guards = []

    i = 0
    while i < num_syms:
        entry_off = sym_offset + i * 18
        if entry_off + 18 > len(data):
            break

        name_bytes = data[entry_off:entry_off + 8]
        is_long = (name_bytes[:4] == b'\x00\x00\x00\x00')

        if is_long:
            str_off = struct.unpack_from('<I', name_bytes, 4)[0]
            abs_off = str_table_offset + str_off
            if abs_off < len(data):
                end = data.index(b'\x00', abs_off)
                name = data[abs_off:end].decode('ascii', errors='replace')
                str_abs = abs_off
                str_len = end - abs_off
            else:
                name = ''
                str_abs = -1
                str_len = 0
        else:
            name = name_bytes.split(b'\x00')[0].decode('ascii', errors='replace')
            str_abs = -1
            str_len = len(name)

        storage_class = data[entry_off + 16]
        aux_count = data[entry_off + 17]

        # Check if guard variable
        if name.startswith('?$S') or name.startswith('??_B'):
            guards.append((name, entry_off, storage_class, str_abs, str_len))

        i += 1 + aux_count

    return guards


def patch_obj_file(decomp_path, orig_path, apply=False, verbose=False):
    """Patch guard variables in a single decomp .obj file.

    Returns (num_patches, details_list).
    """
    with open(orig_path, 'rb') as f:
        orig_data = f.read()
    with open(decomp_path, 'rb') as f:
        decomp_data = bytearray(f.read())

    orig_guards = parse_coff_guard_symbols(orig_data)
    decomp_guards = parse_coff_guard_symbols(decomp_data)

    if not orig_guards and not decomp_guards:
        return 0, []

    # Build scope -> ordered list of (name, storage_class) from original.
    # A single function scope can have multiple guard variables (one per
    # static local), so we must match them positionally, not by scope alone.
    orig_by_scope = defaultdict(list)
    for name, _, cls, _, _ in orig_guards:
        scope = extract_guard_scope(name)
        if scope:
            orig_by_scope[scope].append((name, cls))

    decomp_by_scope = defaultdict(list)
    for name, entry_off, cls, str_abs, str_len in decomp_guards:
        scope = extract_guard_scope(name)
        if scope:
            decomp_by_scope[scope].append(
                (name, entry_off, cls, str_abs, str_len))

    # Match guards positionally within each scope
    patches = []
    for scope in decomp_by_scope:
        if scope not in orig_by_scope:
            continue
        orig_list = orig_by_scope[scope]
        decomp_list = decomp_by_scope[scope]
        for i, (name, entry_off, cls, str_abs, str_len) in enumerate(decomp_list):
            if i >= len(orig_list):
                break
            target_name, target_cls = orig_list[i]
            if name != target_name or cls != target_cls:
                patches.append((name, target_name, entry_off, cls, target_cls,
                                str_abs, str_len))

    if not patches:
        return 0, []

    details = []

    # Get string table info for appending
    sym_offset = struct.unpack_from('<I', decomp_data, 8)[0]
    num_syms = struct.unpack_from('<I', decomp_data, 12)[0]
    str_table_start = sym_offset + num_syms * 18
    str_table_size = struct.unpack_from('<I', decomp_data, str_table_start)[0]

    for old_name, new_name, entry_off, old_cls, new_cls, str_abs, str_len in patches:
        new_bytes = new_name.encode('ascii')

        if str_abs >= 0:
            # Long name in string table
            if len(new_bytes) <= str_len:
                # Fits in existing space - overwrite in place
                decomp_data[str_abs:str_abs + len(new_bytes)] = new_bytes
                decomp_data[str_abs + len(new_bytes)] = 0  # null terminator
                # Zero remaining bytes
                for j in range(str_abs + len(new_bytes) + 1,
                               str_abs + str_len + 1):
                    if j < len(decomp_data):
                        decomp_data[j] = 0
            else:
                # Need to append to string table
                new_str_off = str_table_size
                decomp_data.extend(new_bytes + b'\x00')
                str_table_size += len(new_bytes) + 1
                # Update string table size
                struct.pack_into('<I', decomp_data, str_table_start,
                                str_table_size)
                # Update symbol's string offset
                struct.pack_into('<I', decomp_data, entry_off + 4, new_str_off)
        else:
            # Short name (<=8 chars) - shouldn't happen for guard vars
            if len(new_bytes) <= 8:
                padded = new_bytes.ljust(8, b'\x00')
                decomp_data[entry_off:entry_off + 8] = padded
            else:
                # Need to convert to long name - append to string table
                new_str_off = str_table_size
                decomp_data.extend(new_bytes + b'\x00')
                str_table_size += len(new_bytes) + 1
                struct.pack_into('<I', decomp_data, str_table_start,
                                str_table_size)
                decomp_data[entry_off:entry_off + 4] = b'\x00\x00\x00\x00'
                struct.pack_into('<I', decomp_data, entry_off + 4, new_str_off)

        # Update storage class
        decomp_data[entry_off + 16] = new_cls

        change_type = '??_B' if new_name.startswith('??_B') else 'renum'
        details.append(f'  [{change_type}] {old_name} -> {new_name}'
                       f' (class {old_cls}->{new_cls})')

    if apply:
        with open(decomp_path, 'wb') as f:
            f.write(decomp_data)

    return len(patches), details


def main():
    parser = argparse.ArgumentParser(
        description='Patch $S guard variables to match original ??_B naming')
    parser.add_argument('--batch', action='store_true',
                        help='Process all decomp .obj files')
    parser.add_argument('--apply', action='store_true',
                        help='Actually modify files (default: dry-run)')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Show per-file details')
    parser.add_argument('--obj-dir', default='build/373307D9/obj',
                        help='Original .obj directory')
    parser.add_argument('--src-dir', default='build/373307D9/src',
                        help='Decomp .obj directory')
    parser.add_argument('files', nargs='*',
                        help='Specific .obj files to patch (relative paths)')
    args = parser.parse_args()

    if not args.batch and not args.files:
        parser.error('Specify --batch or provide specific files')

    obj_dir = Path(args.obj_dir)
    src_dir = Path(args.src_dir)

    if args.batch:
        # Find all decomp .obj files that have matching originals
        decomp_objs = sorted(glob.glob(str(src_dir / '**/*.obj'), recursive=True))
    else:
        decomp_objs = [str(src_dir / f) for f in args.files]

    total_files = 0
    total_patches = 0
    patched_files = 0

    for decomp_path in decomp_objs:
        # Find corresponding original
        rel = os.path.relpath(decomp_path, src_dir)
        orig_path = obj_dir / rel
        if not orig_path.exists():
            continue

        total_files += 1
        num_patches, details = patch_obj_file(
            decomp_path, str(orig_path), apply=args.apply, verbose=args.verbose)

        if num_patches > 0:
            patched_files += 1
            total_patches += num_patches
            if args.verbose:
                print(f'{rel}: {num_patches} patches')
                for d in details:
                    print(d)

    mode = 'APPLIED' if args.apply else 'DRY RUN'
    print(f'\n[{mode}] {total_files} files checked, '
          f'{patched_files} files patched, '
          f'{total_patches} total symbol patches')

    if not args.apply and total_patches > 0:
        print('Run with --apply to modify files.')


if __name__ == '__main__':
    main()
