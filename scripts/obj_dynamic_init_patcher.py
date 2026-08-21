#!/usr/bin/env python3
"""Post-build patcher to promote ??__E dynamic initializer symbols from STATIC to EXTERNAL.

MSVC emits ??__E symbols (C++ dynamic initializers for global/static objects) with
STATIC storage class, making them invisible to the linker. The CRT init table in
auto_08_82F05C00_data.obj has EXTERNAL references to these symbols, requiring
ALTERNATENAME stubs in link_glue.cpp to resolve them.

This patcher promotes ??__E from STATIC to EXTERNAL in decomp .obj files,
allowing the linker to resolve auto_08's references directly and eliminating
the need for those ALTERNATENAME stubs.

Patches are LOST on rebuild (same as regswap/anon_ns patchers) — this is a post-build step.

Usage:
    python3 scripts/obj_dynamic_init_patcher.py --batch [--apply] [--verbose]
    python3 scripts/obj_dynamic_init_patcher.py [--apply] <rel/path.obj> ...

Without --apply, performs a dry run showing what would be changed.

The per-file form takes paths relative to --src-dir, matching obj_guard_patcher
and obj_bool_mangle_patcher. It is exactly the batch pass restricted to those
files: this patcher reads only the object in front of it (no target obj, no
cross-object index), so per-file and batch are byte-identical by construction.
"""

import argparse
import glob
import struct
import sys
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
SRC_DIR = PROJECT_ROOT / "build" / "45410914" / "src"

IMAGE_SYM_CLASS_EXTERNAL = 2
IMAGE_SYM_CLASS_STATIC = 3


def patch_obj(path, apply=False, verbose=False):
    """Find ??__E STATIC symbols in a COFF .obj and promote to EXTERNAL.

    Returns list of patched symbol names.
    """
    with open(path, 'rb') as f:
        data = bytearray(f.read())

    # COFF header
    sym_offset = struct.unpack_from('<I', data, 8)[0]
    num_syms = struct.unpack_from('<I', data, 12)[0]
    if num_syms == 0 or sym_offset == 0:
        return []

    str_table_offset = sym_offset + num_syms * 18

    patched_names = []
    i = 0
    while i < num_syms:
        entry_off = sym_offset + i * 18
        name_bytes = bytes(data[entry_off:entry_off + 8])
        storage_class = data[entry_off + 16]
        num_aux = data[entry_off + 17]

        # Resolve symbol name
        if name_bytes[:4] == b'\x00\x00\x00\x00':
            str_off = struct.unpack_from('<I', name_bytes, 4)[0]
            abs_off = str_table_offset + str_off
            end = data.index(b'\x00', abs_off)
            name = data[abs_off:end].decode('ascii', errors='replace')
        else:
            name = name_bytes.split(b'\x00')[0].decode('ascii', errors='replace')

        if name.startswith('??__E') and storage_class == IMAGE_SYM_CLASS_STATIC:
            data[entry_off + 16] = IMAGE_SYM_CLASS_EXTERNAL
            patched_names.append(name)

        i += 1 + num_aux

    if patched_names and apply:
        _write_preserving_mtime(path, data)

    return patched_names


def process_batch(args):
    """Process all decomp .obj files in batch mode."""
    src_dir = Path(args.src_dir) if args.src_dir else SRC_DIR

    if not src_dir.exists():
        print(f"ERROR: Decomp .obj directory not found: {src_dir}", file=sys.stderr)
        sys.exit(1)

    if args.batch:
        obj_paths = sorted(glob.glob(str(src_dir / '**' / '*.obj'), recursive=True))
    else:
        obj_paths = [str(src_dir / f) for f in args.files]

    total_patched = 0
    files_patched = 0
    all_symbols = []

    for obj_path in obj_paths:
        names = patch_obj(obj_path, apply=args.apply, verbose=args.verbose)
        if names:
            files_patched += 1
            total_patched += len(names)
            all_symbols.extend(names)
            if args.verbose:
                relpath = Path(obj_path).relative_to(src_dir)
                for n in names:
                    print(f"  {relpath}: {n}")

    action = "Patched" if args.apply else "Would patch"
    print(f"\n{action} {total_patched} ??__E symbols across {files_patched} files")
    print(f"  (STATIC -> EXTERNAL promotion for linker visibility)")

    if not args.apply and total_patched > 0:
        print(f"\nRun with --apply to actually patch the files.")

    return files_patched


def _check_exit(pending, tag):
    """Shared `--check` contract across all six post-compile patchers.

    Exit 2 -- matching obj_anon_ns_patcher.py, which has had `--check` since
    lane CN-1 -- when the build tree is NOT a fixed point of this pass.  The
    non-zero exit is the whole point: it is what lets an edge in build.ninja,
    or scripts/verify_objs_patched.py, refuse instead of reporting a number
    derived from raw compiler output.
    """
    if pending:
        print('FAIL[{tag}]: {n} pending patch(es) -- this build tree carries '
              'objects that were compiled but never post-processed.'.format(
                  tag=tag, n=pending), file=sys.stderr)
        sys.exit(2)


def main():
    parser = argparse.ArgumentParser(
        description='Promote ??__E dynamic initializer symbols from STATIC to EXTERNAL in decomp .obj files')
    parser.add_argument('--apply', action='store_true',
                        help='Actually apply patches (default: dry run)')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Show per-file details')
    parser.add_argument('--batch', action='store_true',
                        help='Process all decomp .obj files')
    parser.add_argument('--src-dir',
                        help='Decomp .obj directory (default: build/45410914/src)')
    parser.add_argument('--check', action='store_true',
                        help='Dry-run and EXIT 2 if any object in the build tree '
                             'still needs this pass')
    parser.add_argument('files', nargs='*',
                        help='Specific .obj files to patch (paths relative to --src-dir)')
    args = parser.parse_args()

    if args.check:
        # --check never writes.  A checker that could mutate the tree it is
        # auditing is not a checker.
        args.apply = False

    if not args.batch and not args.files:
        parser.error('Specify --batch or provide specific files')

    pending = process_batch(args)
    if args.check:
        _check_exit(pending, 'dynamic_init')



def _write_preserving_mtime(path, data):
    """Write `data` to `path`, then restore the file's original mtime.

    ★ LOAD-BEARING, NOT COSMETIC (lane CN-1, measured). ninja's `deps = msvc`
    stores each obj's mtime beside its dep record in .ninja_deps. If the obj on
    disk is later NEWER than that stored mtime, ninja reports
        ninja explain: stored deps info out of date for '<obj>'
    and RECOMPILES the obj -- silently discarding every symbol-table patch this
    script just applied. Because the patcher stamps take `all_source` as an
    implicit input, ninja then re-runs the patchers, which bump the mtime again:
    without this mtime restore the build OSCILLATES forever. See
    scripts/obj_eh_boundary_patcher.py, which has done this since lane CM-3
    (its comment misnames the file as .ninja_log -- the real one is .ninja_deps).
    """
    import os as _os
    p = str(path)
    st = _os.stat(p)
    with open(p, 'wb') as f:
        f.write(data)
    _os.utime(p, ns=(st.st_atime_ns, st.st_mtime_ns))

if __name__ == '__main__':
    main()

