#!/usr/bin/env python3
"""Post-build patcher: fix bool parameter back-reference mangling.

Our MSVC compiler caches `bool` (_N) in the parameter back-reference table,
producing back-references like `0`, `1`, etc. for repeated bool params.
The target binary's compiler did NOT cache `_N` types, so each bool param
is independently mangled as `_N`.

Example: Load(float, bool, bool)
  Our compiler:  ?Load@MetaMusic@@QAAXM_N0@Z   (_N then 0 = back-ref)
  Target:        ?Load@MetaMusic@@QAAXM_N_N@Z   (_N then _N = no back-ref)

Machine code is identical between the two manglings, so only the COFF symbol
table needs patching.

Usage:
    python3 scripts/obj_bool_mangle_patcher.py --batch [--apply] [--verbose]
"""

import argparse
import glob
import os
import re
import struct
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent


def parse_coff_symbols(data):
    """Parse COFF symbol table, return all symbols with their names and locations.

    Returns list of (name, entry_offset, str_abs_offset, str_len)
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
    symbols = []

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

        aux_count = data[entry_off + 17]
        symbols.append((name, entry_off, str_abs, str_len))
        i += 1 + aux_count

    return symbols


def find_bool_backref_mismatches(orig_symbols, decomp_symbols):
    """Find symbols that differ only in bool back-reference mangling.

    Returns list of (decomp_name, orig_name, decomp_entry_off, decomp_str_abs, decomp_str_len)
    """
    orig_set = {name for name, _, _, _ in orig_symbols}
    decomp_by_name = {}
    for name, entry_off, str_abs, str_len in decomp_symbols:
        decomp_by_name[name] = (entry_off, str_abs, str_len)

    patches = []

    for decomp_name, (entry_off, str_abs, str_len) in decomp_by_name.items():
        if decomp_name in orig_set:
            continue  # Already matches

        # Check if this symbol contains _N followed by a digit (bool back-ref)
        # Pattern: _N<digit> where <digit> is a back-reference
        # We need to try expanding back-references to _N and see if the result
        # matches an original symbol.
        #
        # Strategy: find all occurrences of _N followed by digits in the
        # parameter section, try replacing each digit after _N with _N
        expanded = try_expand_bool_backrefs(decomp_name)
        for candidate in expanded:
            if candidate in orig_set and candidate != decomp_name:
                patches.append((decomp_name, candidate, entry_off, str_abs, str_len))
                break

    return patches


def try_expand_bool_backrefs(name):
    """Try expanding bool back-references in a mangled name.

    Given a name like ?Load@MetaMusic@@QAAXM_N0@Z, try replacing
    digit back-references that follow _N with _N.

    Also handles symbols where _N is referenced by a digit elsewhere
    (e.g., the digit could appear after other type encodings).

    Returns list of candidate expanded names.
    """
    candidates = []

    # Find the parameter section (after @@QAA, @@IAA, @@UAA, etc.)
    # The @@ marks transition to calling convention + params
    param_match = re.search(r'@@[QIUA][A-Z]{2}', name)
    if not param_match:
        # Also handle non-member functions (@@YA)
        param_match = re.search(r'@@Y[A-Z]', name)
    if not param_match:
        return candidates

    param_start = param_match.end()
    prefix = name[:param_start]
    suffix = name[param_start:]

    # Find all positions of _N in the suffix (bool type)
    # After finding _N, check if the next characters include digits
    # that could be back-references to bool

    # Simple approach: find _N<digit> pattern and try _N_N
    # This handles the common case of (type, bool, bool)
    expanded = expand_backrefs_in_params(suffix)
    for exp in expanded:
        candidates.append(prefix + exp)

    return candidates


def expand_backrefs_in_params(params):
    """Expand bool back-references in the parameter portion of a mangled name.

    Returns list of expanded candidates.
    """
    results = set()

    # Strategy: scan for _N followed by a single digit, try replacing
    # the digit with _N.
    #
    # Also need to handle cases where _N appears and later a digit
    # back-references it. The digit might not be immediately after _N.
    #
    # For safety, try ALL single-digit replacements with _N and check
    # if the original has a match.

    # Find positions of all digits that could be back-references
    # Back-references are single digits 0-9 that appear as type encodings
    # They can appear anywhere in the parameter list

    # Simple case: _N followed immediately by digit(s)
    # e.g., _N0 -> _N_N, _N01 -> _N_N_N, etc.
    pattern = re.compile(r'_N(\d)')
    for m in pattern.finditer(params):
        # Replace this specific digit with _N
        new_params = params[:m.start(1)] + '_N' + params[m.end(1):]
        results.add(new_params)

    # Also try replacing standalone digits after @Z-terminated sections
    # or after other type encodings
    # Find all single digits in the params
    i = 0
    while i < len(params):
        if params[i].isdigit() and '_N' in params[:i]:
            # This digit COULD be a back-reference to _N
            new_params = params[:i] + '_N' + params[i+1:]
            results.add(new_params)
        i += 1

    # Also handle all symbols that reference the function scope
    # (like guard vars, static locals, etc.)
    # These embed the mangled function name

    return list(results)


def patch_obj_file(decomp_path, orig_path, apply=False, verbose=False):
    """Patch bool mangling in a single decomp .obj file.

    Returns (num_patches, details_list).
    """
    with open(orig_path, 'rb') as f:
        orig_data = f.read()
    with open(decomp_path, 'rb') as f:
        decomp_data = bytearray(f.read())

    orig_symbols = parse_coff_symbols(orig_data)
    decomp_symbols = parse_coff_symbols(decomp_data)

    if not orig_symbols or not decomp_symbols:
        return 0, []

    # Build set of original symbol names
    orig_names = {name for name, _, _, _ in orig_symbols}

    # Find all symbols in decomp that need bool backref expansion
    patches = find_bool_backref_mismatches(orig_symbols, decomp_symbols)

    if not patches:
        return 0, []

    # Now we also need to patch ALL references to the old name throughout
    # the obj file. This includes: the symbol table entry itself, plus
    # any other symbols that embed this name (like guard vars, static locals,
    # unwind tables, etc.)

    # First, collect ALL name replacements needed (including dependent symbols)
    all_replacements = {}  # old_name -> new_name
    for decomp_name, orig_name, _, _, _ in patches:
        all_replacements[decomp_name] = orig_name

    # Find dependent symbols (those that contain the old mangled name as a substring)
    additional = {}
    for decomp_name, orig_name in list(all_replacements.items()):
        for sym_name, entry_off, str_abs, str_len in decomp_symbols:
            if sym_name == decomp_name:
                continue
            if decomp_name in sym_name and sym_name not in all_replacements:
                # This symbol contains the old name - replace it
                new_sym = sym_name.replace(decomp_name, orig_name)
                if new_sym in orig_names or True:  # Always replace dependent symbols
                    additional[sym_name] = new_sym

    all_replacements.update(additional)

    # Apply all replacements
    details = []
    num_applied = 0

    # Get string table info
    sym_offset = struct.unpack_from('<I', decomp_data, 8)[0]
    num_syms = struct.unpack_from('<I', decomp_data, 12)[0]
    str_table_start = sym_offset + num_syms * 18
    str_table_size = struct.unpack_from('<I', decomp_data, str_table_start)[0]

    for sym_name, entry_off, str_abs, str_len in decomp_symbols:
        if sym_name not in all_replacements:
            continue

        new_name = all_replacements[sym_name]
        new_bytes = new_name.encode('ascii')

        if str_abs >= 0:
            # Long name in string table
            if len(new_bytes) <= str_len:
                # Fits in existing space
                decomp_data[str_abs:str_abs + len(new_bytes)] = new_bytes
                decomp_data[str_abs + len(new_bytes)] = 0
                for j in range(str_abs + len(new_bytes) + 1,
                               str_abs + str_len + 1):
                    if j < len(decomp_data):
                        decomp_data[j] = 0
            else:
                # Need to append to string table
                new_str_off = str_table_size
                decomp_data.extend(new_bytes + b'\x00')
                str_table_size += len(new_bytes) + 1
                struct.pack_into('<I', decomp_data, str_table_start,
                                str_table_size)
                struct.pack_into('<I', decomp_data, entry_off + 4, new_str_off)
        else:
            # Short name
            if len(new_bytes) <= 8:
                padded = new_bytes.ljust(8, b'\x00')
                decomp_data[entry_off:entry_off + 8] = padded
            else:
                new_str_off = str_table_size
                decomp_data.extend(new_bytes + b'\x00')
                str_table_size += len(new_bytes) + 1
                struct.pack_into('<I', decomp_data, str_table_start,
                                str_table_size)
                decomp_data[entry_off:entry_off + 4] = b'\x00\x00\x00\x00'
                struct.pack_into('<I', decomp_data, entry_off + 4, new_str_off)

        num_applied += 1
        details.append(f'  {sym_name} -> {new_name}')

    if apply and num_applied > 0:
        with open(decomp_path, 'wb') as f:
            f.write(decomp_data)

    return num_applied, details


def main():
    parser = argparse.ArgumentParser(
        description='Patch bool parameter back-reference mangling differences')
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
        decomp_objs = sorted(glob.glob(str(src_dir / '**/*.obj'), recursive=True))
    else:
        decomp_objs = [str(src_dir / f) for f in args.files]

    total_files = 0
    total_patches = 0
    patched_files = 0

    for decomp_path in decomp_objs:
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
