#!/usr/bin/env python3
"""Post-build patcher: rename ??__F atexit destructor symbols to match target
scope counters.

MSVC assigns a sequential scope counter to every `{}` block opened in a
function (`?<counter>?` in the mangled name). The counter only affects
destructor/guard/static symbol names -- the machine code is identical.

When our decomp source has slightly different brace structure than the
original, all the `??__F<var>@?<counter>?<containing>@YAXXZ` symbols end up
with mismatched counters, so objdiff can't find the base counterpart and
reports 0% match even though the bodies are byte-for-byte identical.

This patcher:
  1. Parses ??__F atexit destructor symbols in both target and base .obj
  2. Computes a canonical key by stripping the scope counter
  3. For each canonical key present in both:
     - 1:1 case: rename the base symbol to match target
     - m:n positional: pair by sorted scope counter order
     - n:0 or 0:n: skip (missing/extra static declaration)
  4. Only renames if the target name's byte length fits in the same string
     table slot; otherwise appends to the string table.

Machine code is byte-identical, so only the COFF symbol table is changed.
Relocations reference symbols by index, so cross-refs (e.g., from dynamic
initializers) automatically track the renamed symbol.

Storage class stays STATIC (atexit destructors are always file-local), so
the linker never sees these names -- this is safe for link integrity.

Usage:
    python3 scripts/obj_atexit_scope_patcher.py --batch [--apply] [--verbose]
    python3 scripts/obj_atexit_scope_patcher.py --batch --apply --stats-json out.json

Without --apply, performs a dry run showing what would be changed.
"""

import argparse
import glob
import json
import os
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_TARGET_DIR = PROJECT_ROOT / "build" / "373307D9" / "obj"
DEFAULT_BASE_DIR = PROJECT_ROOT / "build" / "373307D9" / "src"

IMAGE_FILE_MACHINE_POWERPCBE_LE = 0x01F2  # COFF header is little-endian


# --- Canonical key computation ---

# Single-digit counter: ??__F<var>@?<d>?<nested>@YAXXZ  (d in 0-9)
_RE_SINGLE = re.compile(r'^(\?\?__F[^@]+)@\?([0-9])(\?.*)$')
# Multi-char counter: ??__F<var>@?<LETTERS>@?<nested>@YAXXZ
_RE_MULTI = re.compile(r'^(\?\?__F[^@]+)@\?([A-Z]+)@(\?.*)$')


def canonical_atexit(name):
    """Return canonical key for an atexit destructor symbol, or None.

    Returns None for:
      - Non-atexit symbols
      - File-scope atexit dtors (no scope counter, e.g. ??__FTheCharDebug@@YAXXZ)
      - Exotic forms we don't recognize
    """
    if not name.startswith('??__F'):
        return None
    m = _RE_SINGLE.match(name)
    if m:
        return f"{m.group(1)}@{m.group(3)}"
    m = _RE_MULTI.match(name)
    if m:
        return f"{m.group(1)}@{m.group(3)}"
    return None


def extract_scope_counter(name):
    """Return the scope counter token (e.g. '4', 'BJ'), or None."""
    m = _RE_SINGLE.match(name)
    if m:
        return m.group(2)
    m = _RE_MULTI.match(name)
    if m:
        return m.group(2)
    return None


def counter_sort_key(counter):
    """Sort scope counters by MSVC base-16 encoding (A=0..P=15, digit=digit).

    Single digit 0-9 represents value 0-9 directly.
    Multi-char [A-P]+ is base-16 with A=0..P=15.
    """
    if not counter:
        return (0, 0)
    if counter.isdigit():
        return (0, int(counter))
    # Multi-char: treat as base-16
    val = 0
    for c in counter:
        if 'A' <= c <= 'P':
            val = val * 16 + (ord(c) - ord('A'))
        else:
            # Unknown char -- use ord() fallback
            return (2, counter)
    return (1, val)


# --- COFF parsing ---

def parse_coff_atexit_symbols(data):
    """Parse COFF symbol table, return atexit-related info.

    Returns:
        (symbols, sym_offset, num_syms, str_table_start)

    where `symbols` is a list of dicts with keys:
        name, entry_offset, storage_class, section, value, num_aux,
        str_abs_offset (-1 if short name), str_len
    Only ??__F symbols are included.
    """
    if len(data) < 20:
        return [], 0, 0, 0

    sym_offset = struct.unpack_from('<I', data, 8)[0]
    num_syms = struct.unpack_from('<I', data, 12)[0]
    if sym_offset == 0 or num_syms == 0:
        return [], 0, 0, 0

    str_table_start = sym_offset + num_syms * 18
    syms = []

    i = 0
    while i < num_syms:
        entry_off = sym_offset + i * 18
        if entry_off + 18 > len(data):
            break

        name_bytes = data[entry_off:entry_off + 8]
        is_long = (name_bytes[:4] == b'\x00\x00\x00\x00')

        if is_long:
            str_off = struct.unpack_from('<I', name_bytes, 4)[0]
            abs_off = str_table_start + str_off
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

        value, section = struct.unpack_from('<Ih', data, entry_off + 8)
        storage_class = data[entry_off + 16]
        num_aux = data[entry_off + 17]

        if name.startswith('??__F'):
            syms.append({
                'name': name,
                'entry_offset': entry_off,
                'storage_class': storage_class,
                'section': section,
                'value': value,
                'num_aux': num_aux,
                'str_abs_offset': str_abs,
                'str_len': str_len,
            })

        i += 1 + num_aux

    return syms, sym_offset, num_syms, str_table_start


def get_section_raw_bytes(data, sec_idx_1based):
    """Get the raw bytes of a section given its 1-based index."""
    if sec_idx_1based <= 0:
        return b''
    num_sections = struct.unpack_from('<H', data, 2)[0]
    opt_hdr_size = struct.unpack_from('<H', data, 16)[0]
    sec_hdr_off = 20 + opt_hdr_size + (sec_idx_1based - 1) * 40
    if sec_hdr_off + 40 > len(data):
        return b''
    raw_size = struct.unpack_from('<I', data, sec_hdr_off + 16)[0]
    raw_off = struct.unpack_from('<I', data, sec_hdr_off + 20)[0]
    return bytes(data[raw_off:raw_off + raw_size])


# --- Rename logic ---

def build_rename_plan(target_syms, base_syms):
    """Compute rename plan: list of (old_base_name, new_target_name).

    Matching strategy:
      1. Group both sides by canonical key.
      2. For each canonical key present in both:
         - 1:1: rename base to target (single match).
         - m:n with m==n: sort by scope counter, pair positionally.
         - m!=n: pair min(m,n) positionally (partial match).
      3. Skip canonical keys present only on one side.

    Returns:
        List of (old_name, new_name, strategy) where strategy is
        'unique' | 'positional' | 'skip_collision'.
        Also returns list of skipped canonical keys for reporting.
    """
    t_by_canon = defaultdict(list)
    for s in target_syms:
        c = canonical_atexit(s['name'])
        if c:
            t_by_canon[c].append(s['name'])

    b_by_canon = defaultdict(list)
    for s in base_syms:
        c = canonical_atexit(s['name'])
        if c:
            b_by_canon[c].append(s)

    renames = []  # (old_name, new_name, strategy)
    skipped = []

    for canon, b_list in b_by_canon.items():
        t_list = t_by_canon.get(canon)
        if not t_list:
            skipped.append((canon, 'target_missing', len(b_list), 0))
            continue

        # Sort both sides by existing scope counter for positional pairing
        t_sorted = sorted(t_list, key=lambda n: counter_sort_key(extract_scope_counter(n)))
        b_sorted = sorted(b_list, key=lambda b: counter_sort_key(extract_scope_counter(b['name'])))

        if len(t_list) == 1 and len(b_list) == 1:
            old = b_sorted[0]['name']
            new = t_sorted[0]
            if old != new:
                renames.append((old, new, 'unique'))
        else:
            # Positional pairing for collision case
            pair_count = min(len(t_sorted), len(b_sorted))
            for i in range(pair_count):
                old = b_sorted[i]['name']
                new = t_sorted[i]
                if old != new:
                    renames.append((old, new, 'positional'))
            if len(t_sorted) != len(b_sorted):
                skipped.append((
                    canon,
                    'count_mismatch',
                    len(b_sorted),
                    len(t_sorted),
                ))

    return renames, skipped


def verify_byte_equality(target_data, base_data, target_sym, base_sym):
    """Verify the function bodies of two atexit destructors are byte-identical.

    Returns:
        (equal, reason) where reason describes any mismatch.
    """
    t_bytes = get_section_raw_bytes(target_data, target_sym['section'])
    b_bytes = get_section_raw_bytes(base_data, base_sym['section'])

    if len(t_bytes) != len(b_bytes):
        return False, f'size mismatch: target={len(t_bytes)} base={len(b_bytes)}'

    # For atexit destructors, value is usually 0 (function at start of section)
    # but a safety check: ensure offset is 0 in both
    if target_sym['value'] != 0 or base_sym['value'] != 0:
        return False, f'non-zero value: target={target_sym["value"]} base={base_sym["value"]}'

    if t_bytes != b_bytes:
        return False, f'bytes differ (size={len(t_bytes)})'

    return True, 'ok'


def apply_renames_to_obj(data, rename_plan, base_syms):
    """Rename symbols in a COFF .obj file in-place.

    `data` is a mutable bytearray. `rename_plan` is list of (old, new, strategy).
    `base_syms` is the symbol list from parse_coff_atexit_symbols (provides
    entry_offset + str_abs_offset for each).

    Short-name case (<=8 chars) should never happen for atexit symbols.
    """
    sym_offset = struct.unpack_from('<I', data, 8)[0]
    num_syms = struct.unpack_from('<I', data, 12)[0]
    str_table_start = sym_offset + num_syms * 18
    str_table_size = struct.unpack_from('<I', data, str_table_start)[0]

    # Index base syms by old name for O(1) lookup
    by_name = {s['name']: s for s in base_syms}

    applied = []
    for old_name, new_name, strategy in rename_plan:
        sym = by_name.get(old_name)
        if sym is None:
            continue

        new_bytes = new_name.encode('ascii')
        entry_off = sym['entry_offset']
        str_abs = sym['str_abs_offset']
        str_len = sym['str_len']

        if str_abs >= 0:
            # Long name in string table
            if len(new_bytes) <= str_len:
                # Fits in existing space
                data[str_abs:str_abs + len(new_bytes)] = new_bytes
                # Zero remaining bytes in old slot (keep null terminator area clean)
                for j in range(len(new_bytes), str_len):
                    data[str_abs + j] = 0
                data[str_abs + str_len] = 0  # ensure final null
            else:
                # Append to string table
                new_str_off = str_table_size
                data.extend(new_bytes + b'\x00')
                str_table_size += len(new_bytes) + 1
                struct.pack_into('<I', data, str_table_start, str_table_size)
                struct.pack_into('<I', data, entry_off + 4, new_str_off)
        else:
            # Short name (unlikely for atexit): convert to long name
            if len(new_bytes) <= 8:
                padded = new_bytes.ljust(8, b'\x00')
                data[entry_off:entry_off + 8] = padded
            else:
                new_str_off = str_table_size
                data.extend(new_bytes + b'\x00')
                str_table_size += len(new_bytes) + 1
                struct.pack_into('<I', data, str_table_start, str_table_size)
                data[entry_off:entry_off + 4] = b'\x00\x00\x00\x00'
                struct.pack_into('<I', data, entry_off + 4, new_str_off)

        applied.append((old_name, new_name, strategy))

    return applied


# --- Orchestration ---

def patch_obj_pair(target_path, base_path, apply=False, verbose=False):
    """Process a single target/base .obj pair.

    Returns a dict with:
        num_renamed, applied [(old, new, strategy)], skipped [...], verify_fails [...]
    """
    with open(target_path, 'rb') as f:
        target_data = f.read()
    with open(base_path, 'rb') as f:
        base_data = bytearray(f.read())

    target_syms, _, _, _ = parse_coff_atexit_symbols(target_data)
    base_syms, _, _, _ = parse_coff_atexit_symbols(base_data)

    if not target_syms or not base_syms:
        return {
            'num_renamed': 0,
            'applied': [],
            'skipped': [],
            'verify_fails': [],
        }

    plan, skipped = build_rename_plan(target_syms, base_syms)

    if not plan:
        return {
            'num_renamed': 0,
            'applied': [],
            'skipped': skipped,
            'verify_fails': [],
        }

    # Verify byte equality before renaming
    verify_fails = []
    safe_plan = []
    t_by_name = {s['name']: s for s in target_syms}
    b_by_name = {s['name']: s for s in base_syms}

    for old, new, strategy in plan:
        t_sym = t_by_name.get(new)
        b_sym = b_by_name.get(old)
        if not t_sym or not b_sym:
            continue
        ok, reason = verify_byte_equality(target_data, base_data, t_sym, b_sym)
        if ok:
            safe_plan.append((old, new, strategy))
        else:
            verify_fails.append((old, new, reason))

    if not safe_plan:
        return {
            'num_renamed': 0,
            'applied': [],
            'skipped': skipped,
            'verify_fails': verify_fails,
        }

    applied = apply_renames_to_obj(base_data, safe_plan, base_syms)

    if apply and applied:
        with open(base_path, 'wb') as f:
            f.write(base_data)

    return {
        'num_renamed': len(applied),
        'applied': applied,
        'skipped': skipped,
        'verify_fails': verify_fails,
    }


def process_batch(args):
    target_dir = Path(args.target_dir)
    base_dir = Path(args.base_dir)

    if not target_dir.exists():
        print(f"ERROR: target .obj directory not found: {target_dir}", file=sys.stderr)
        sys.exit(1)
    if not base_dir.exists():
        print(f"ERROR: base .obj directory not found: {base_dir}", file=sys.stderr)
        sys.exit(1)

    if args.files:
        obj_rel_paths = args.files
    else:
        obj_rel_paths = []
        for base_path in sorted(glob.glob(str(base_dir / '**/*.obj'), recursive=True)):
            rel = os.path.relpath(base_path, base_dir)
            obj_rel_paths.append(rel)

    total_files = 0
    files_renamed = 0
    total_renames = 0
    total_verify_fails = 0
    total_skipped = 0
    by_strategy = defaultdict(int)
    all_applied = []  # for JSON stats
    per_file_summary = []

    for rel in obj_rel_paths:
        base_path = base_dir / rel
        target_path = target_dir / rel
        if not base_path.exists() or not target_path.exists():
            continue

        total_files += 1
        try:
            result = patch_obj_pair(
                str(target_path),
                str(base_path),
                apply=args.apply,
                verbose=args.verbose,
            )
        except Exception as e:
            if args.verbose:
                print(f'  {rel}: ERROR {e}')
            continue

        if result['num_renamed'] > 0:
            files_renamed += 1
            total_renames += result['num_renamed']
            for old, new, strategy in result['applied']:
                by_strategy[strategy] += 1
                all_applied.append({
                    'unit': rel,
                    'old': old,
                    'new': new,
                    'strategy': strategy,
                })
            per_file_summary.append({
                'unit': rel,
                'renamed': result['num_renamed'],
            })
            if args.verbose:
                print(f'{rel}: {result["num_renamed"]} renamed')
                for old, new, strategy in result['applied']:
                    print(f'  [{strategy}] {old}')
                    print(f'       -> {new}')

        total_verify_fails += len(result['verify_fails'])
        total_skipped += len(result['skipped'])
        if args.verbose and result['verify_fails']:
            for old, new, reason in result['verify_fails']:
                print(f'  {rel}: verify FAIL {old} -> {new}: {reason}')

    mode = 'APPLIED' if args.apply else 'DRY RUN'
    print(f'\n[{mode}] Processed {total_files} .obj pairs, '
          f'renamed {total_renames} atexit symbols '
          f'across {files_renamed} files')
    print(f'  unique 1:1 matches: {by_strategy["unique"]}')
    print(f'  positional (collision) matches: {by_strategy["positional"]}')
    print(f'  verify failures (bytes differ): {total_verify_fails}')
    print(f'  canonical keys skipped (missing side/count mismatch): {total_skipped}')

    if not args.apply and total_renames > 0:
        print('\nRun with --apply to actually rewrite the .obj files.')

    if args.stats_json:
        stats = {
            'mode': mode,
            'total_files': total_files,
            'files_renamed': files_renamed,
            'total_renames': total_renames,
            'by_strategy': dict(by_strategy),
            'total_verify_fails': total_verify_fails,
            'total_skipped': total_skipped,
            'applied': all_applied,
            'per_file': per_file_summary,
        }
        Path(args.stats_json).write_text(json.dumps(stats, indent=2))
        print(f'\nStats written to: {args.stats_json}')


def run_selftest():
    """Inline sanity tests for canonical_atexit and counter_sort_key."""
    fails = 0

    def eq(a, b, label):
        nonlocal fails
        if a != b:
            fails += 1
            print(f"  FAIL {label}: expected {b!r}, got {a!r}")
        else:
            print(f"  OK   {label}")

    # Canonical key: single-digit counter
    eq(
        canonical_atexit('??__F_dw@?4??FindClip@CharDriver@@QAAPAVCharClip@@ABVDataNode@@_N@Z@YAXXZ'),
        '??__F_dw@??FindClip@CharDriver@@QAAPAVCharClip@@ABVDataNode@@_N@Z@YAXXZ',
        'single-digit ?4',
    )
    # Canonical key: multi-char counter with @ terminator
    eq(
        canonical_atexit('??__Fmsg@?BJ@??Display@CharDriver@@IAAMM@Z@YAXXZ'),
        '??__Fmsg@??Display@CharDriver@@IAAMM@Z@YAXXZ',
        'multi-char ?BJ@',
    )
    # Canonical key: different counter, same containing function → same canonical
    eq(
        canonical_atexit('??__Fmsg@?CJ@??Display@CharDriver@@IAAMM@Z@YAXXZ'),
        canonical_atexit('??__Fmsg@?BJ@??Display@CharDriver@@IAAMM@Z@YAXXZ'),
        'two counters → same canonical',
    )
    # Canonical key: file-scope (no counter) → None
    eq(
        canonical_atexit('??__FTheCharDebug@@YAXXZ'),
        None,
        'file-scope → None',
    )
    # Canonical key: non-atexit → None
    eq(
        canonical_atexit('?FindClip@CharDriver@@QAAPAVCharClip@@ABVDataNode@@_N@Z'),
        None,
        'non-atexit → None',
    )
    # Distinct containing functions → different canonical
    a = canonical_atexit('??__F_dw@?4??FindClip@CharDriver@@QAAPAVCharClip@@ABVDataNode@@_N@Z@YAXXZ')
    b = canonical_atexit('??__F_dw@?4??Play@CharDriver@@QAAPAVCharClipDriver@@PAVCharClip@@HMMM@Z@YAXXZ')
    eq(a != b, True, 'FindClip ≠ Play canonical')

    # Counter sort
    eq(counter_sort_key('4'), (0, 4), 'sort digit 4')
    eq(counter_sort_key('BJ'), (1, 0x19), 'sort BJ = 0x19')
    eq(counter_sort_key('BB'), (1, 0x11), 'sort BB = 0x11')
    # BB < BJ
    eq(counter_sort_key('BB') < counter_sort_key('BJ'), True, 'BB < BJ')
    # 4 < BB
    eq(counter_sort_key('4') < counter_sort_key('BB'), True, 'digit < letters')

    # Extract counter
    eq(extract_scope_counter('??__F_dw@?4??Play@CharDriver@@QAAPAVCharClipDriver@@PAVCharClip@@HMMM@Z@YAXXZ'), '4', 'extract 4')
    eq(extract_scope_counter('??__Fmsg@?BJ@??Display@CharDriver@@IAAMM@Z@YAXXZ'), 'BJ', 'extract BJ')

    if fails:
        print(f"\n{fails} tests FAILED")
        sys.exit(1)
    print("\nAll selftests passed")


def main():
    parser = argparse.ArgumentParser(
        description='Rename ??__F atexit destructors in base .obj files to '
                    'match target scope counters (fuzzy match by canonical key).',
    )
    parser.add_argument('--batch', action='store_true',
                        help='Process all .obj file pairs')
    parser.add_argument('--apply', action='store_true',
                        help='Actually modify files (default: dry-run)')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Show per-file details')
    parser.add_argument('--target-dir', default=str(DEFAULT_TARGET_DIR),
                        help='Target (original) .obj directory')
    parser.add_argument('--base-dir', default=str(DEFAULT_BASE_DIR),
                        help='Base (decomp) .obj directory')
    parser.add_argument('--stats-json',
                        help='Write detailed stats to this JSON file')
    parser.add_argument('--selftest', action='store_true',
                        help='Run inline unit tests and exit')
    parser.add_argument('files', nargs='*',
                        help='Specific .obj files to patch (paths relative to base-dir)')
    args = parser.parse_args()

    if args.selftest:
        run_selftest()
        return

    if not args.batch and not args.files:
        parser.error('Specify --batch or provide specific files')

    process_batch(args)


if __name__ == '__main__':
    main()
