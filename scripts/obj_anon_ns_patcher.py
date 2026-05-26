#!/usr/bin/env python3
"""Post-build patcher for MSVC anonymous namespace hashes in .obj files.

MSVC generates anonymous namespace hashes (e.g., ?A0x12345678@@) based on
the computer name and source file path. Our decomp build produces different
hashes than the original because the build environment differs.

This script patches the anonymous namespace hashes in decomp .obj files
to match the original .obj files, enabling proper symbol matching for
linking and reducing relocation argument noise in objdiff.

Patches are LOST on rebuild (same as regswap patcher) - this is a post-build step.

Usage:
    python3 scripts/obj_anon_ns_patcher.py --batch [--apply] [--verbose]

Without --apply, performs a dry run showing what would be changed.
"""

import argparse
import os
import re
import sys
from collections import defaultdict
from pathlib import Path

PROJECT_ROOT = Path(__file__).resolve().parent.parent
OBJ_DIR = PROJECT_ROOT / "build" / "373307D9" / "obj"
SRC_DIR = PROJECT_ROOT / "build" / "373307D9" / "src"

ANON_NS_PATTERN = re.compile(rb'\?A0x([0-9a-fA-F]{8})@@')


def find_anon_ns_hashes(data: bytes) -> set:
    """Find all unique anonymous namespace hashes in a COFF .obj file.

    Returns set of 8-byte ASCII hash strings (e.g., {b'c9fefd64'}).
    """
    return set(ANON_NS_PATTERN.findall(data))


def build_obj_mappings(obj_dir: Path, src_dir: Path):
    """Build mappings between original and decomp .obj files.

    Returns:
        orig_by_relpath: {relative_path: absolute_path} for original .obj files
        decomp_by_relpath: {relative_path: absolute_path} for decomp .obj files
    """
    orig_by_relpath = {}
    for root, dirs, files in os.walk(obj_dir):
        for f in files:
            if not f.endswith('.obj') or f.startswith('auto_'):
                continue
            abspath = os.path.join(root, f)
            relpath = os.path.relpath(abspath, obj_dir)
            orig_by_relpath[relpath] = abspath

    decomp_by_relpath = {}
    for root, dirs, files in os.walk(src_dir):
        for f in files:
            if not f.endswith('.obj'):
                continue
            abspath = os.path.join(root, f)
            relpath = os.path.relpath(abspath, src_dir)
            decomp_by_relpath[relpath] = abspath

    return orig_by_relpath, decomp_by_relpath


def build_common_hashes(orig_by_relpath: dict, threshold: int = 5) -> set:
    """Identify hashes that appear in many original .obj files.

    These are 'common header hashes' from shared headers like ObjPtr_p.h.
    Any hash appearing in more than `threshold` original files is considered
    common and excluded from single-hash heuristics.

    Returns set of common hash bytes (e.g., {b'c9fefd64', b'b39b74bf'}).
    """
    hash_file_counts = defaultdict(int)
    for relpath, abspath in orig_by_relpath.items():
        with open(abspath, 'rb') as fh:
            data = fh.read()
        for h in find_anon_ns_hashes(data):
            hash_file_counts[h] += 1

    common = set()
    for h, count in hash_file_counts.items():
        if count > threshold:
            common.add(h)

    return common


def patch_obj_file(obj_path: str, old_hash: bytes, new_hash: bytes,
                   apply: bool = False) -> int:
    """Replace all occurrences of old_hash with new_hash in the obj file.

    The replacement is a simple byte-level find-replace of the 8-digit
    ASCII hash string. This works because the hash only appears in
    symbol name contexts within the COFF symbol table and string table.

    Returns the number of replacements made.
    """
    old_pattern = b'?A0x' + old_hash + b'@@'
    new_pattern = b'?A0x' + new_hash + b'@@'

    with open(obj_path, 'rb') as f:
        data = f.read()

    count = data.count(old_pattern)
    if count == 0:
        return 0

    if apply:
        new_data = data.replace(old_pattern, new_pattern)
        with open(obj_path, 'wb') as f:
            f.write(new_data)

    return count


def process_batch(args):
    """Process all decomp .obj files in batch mode."""
    obj_dir = Path(args.obj_dir) if args.obj_dir else OBJ_DIR
    src_dir = Path(args.src_dir) if args.src_dir else SRC_DIR

    if not obj_dir.exists():
        print(f"ERROR: Original .obj directory not found: {obj_dir}", file=sys.stderr)
        sys.exit(1)
    if not src_dir.exists():
        print(f"ERROR: Decomp .obj directory not found: {src_dir}", file=sys.stderr)
        sys.exit(1)

    # Build file mappings by relative path (handles duplicate filenames correctly)
    orig_by_relpath, decomp_by_relpath = build_obj_mappings(obj_dir, src_dir)

    if args.verbose:
        print(f"Found {len(orig_by_relpath)} original .obj files")
        print(f"Found {len(decomp_by_relpath)} decomp .obj files")

    # Build common hash set dynamically from original .obj files
    if args.verbose:
        print("Scanning original .obj files for common header hashes...")
    common_hashes = build_common_hashes(orig_by_relpath, threshold=5)
    if args.verbose:
        if common_hashes:
            ch_str = ', '.join(sorted(h.decode() for h in common_hashes))
            print(f"Common header hashes (>5 files): {ch_str}")
        else:
            print("No common header hashes found")

    # Cache original file hashes (keyed by relpath)
    orig_hash_cache = {}
    for relpath, abspath in orig_by_relpath.items():
        with open(abspath, 'rb') as fh:
            data = fh.read()
        hashes = find_anon_ns_hashes(data)
        if hashes:
            orig_hash_cache[relpath] = hashes

    # Process decomp .obj files
    patched_files = 0
    total_replacements = 0
    already_ok = 0
    skipped_multi = 0
    skipped_no_orig = 0
    skipped_no_hash_orig = 0

    for relpath, decomp_path in sorted(decomp_by_relpath.items()):
        with open(decomp_path, 'rb') as fh:
            data = fh.read()

        decomp_hashes = find_anon_ns_hashes(data)
        if not decomp_hashes:
            continue

        # Find matching original by relative path
        if relpath not in orig_by_relpath:
            if args.verbose:
                print(f"  SKIP {relpath}: no matching original .obj")
            skipped_no_orig += 1
            continue

        # Get original hashes
        if relpath not in orig_hash_cache:
            if args.verbose:
                print(f"  SKIP {relpath}: original has no anonymous namespace hashes")
            skipped_no_hash_orig += 1
            continue

        orig_hash_set = orig_hash_cache[relpath]

        # Case 1: Both have exactly 1 unique hash -> simple replacement
        if len(decomp_hashes) == 1 and len(orig_hash_set) == 1:
            decomp_hash = next(iter(decomp_hashes))
            orig_hash = next(iter(orig_hash_set))

            if decomp_hash == orig_hash:
                if args.verbose:
                    print(f"  OK   {relpath}: hashes already match ({decomp_hash.decode()})")
                already_ok += 1
                continue

            count = patch_obj_file(decomp_path, decomp_hash, orig_hash, apply=args.apply)
            if count > 0:
                patched_files += 1
                total_replacements += count
                action = "PATCH" if args.apply else "WOULD PATCH"
                if args.verbose:
                    print(f"  {action} {relpath}: {decomp_hash.decode()} -> {orig_hash.decode()} ({count} occurrences)")

        # Case 2: Decomp has 1 hash, original has multiple
        elif len(decomp_hashes) == 1 and len(orig_hash_set) > 1:
            decomp_hash = next(iter(decomp_hashes))

            # Already matches one of the original hashes
            if decomp_hash in orig_hash_set:
                if args.verbose:
                    print(f"  OK   {relpath}: decomp hash matches one of {len(orig_hash_set)} original hashes")
                already_ok += 1
                continue

            # Heuristic: filter out common header hashes, patch to the unique remainder
            unique_hashes = orig_hash_set - common_hashes
            if len(unique_hashes) == 1:
                orig_hash = next(iter(unique_hashes))
                count = patch_obj_file(decomp_path, decomp_hash, orig_hash, apply=args.apply)
                if count > 0:
                    patched_files += 1
                    total_replacements += count
                    action = "PATCH" if args.apply else "WOULD PATCH"
                    if args.verbose:
                        print(f"  {action} {relpath}: {decomp_hash.decode()} -> {orig_hash.decode()} "
                              f"({count} occ, heuristic: 1 unique among {len(orig_hash_set)})")
            elif len(unique_hashes) == 0:
                # All original hashes are common - can't determine which is the file's own
                if args.verbose:
                    orig_str = ', '.join(sorted(h.decode() for h in orig_hash_set))
                    print(f"  SKIP {relpath}: all {len(orig_hash_set)} original hashes are common ({orig_str})")
                skipped_multi += 1
            else:
                if args.verbose:
                    orig_str = ', '.join(sorted(h.decode() for h in orig_hash_set))
                    unique_str = ', '.join(sorted(h.decode() for h in unique_hashes))
                    print(f"  SKIP {relpath}: {len(unique_hashes)} non-common hashes remain ({unique_str}) "
                          f"from {len(orig_hash_set)} total ({orig_str})")
                skipped_multi += 1

        # Case 3: Decomp has multiple hashes (shouldn't happen with current build, but handle it)
        elif len(decomp_hashes) > 1:
            if args.verbose:
                d_str = ', '.join(sorted(h.decode() for h in decomp_hashes))
                o_str = ', '.join(sorted(h.decode() for h in orig_hash_set))
                print(f"  SKIP {relpath}: multiple decomp hashes ({d_str}) vs original ({o_str})")
            skipped_multi += 1

    # Summary
    action_word = "Applied" if args.apply else "Would apply"
    print(f"\n{action_word} patches to {patched_files} files ({total_replacements} total replacements)")
    print(f"Already matching: {already_ok}")
    print(f"Skipped (ambiguous/multiple hashes): {skipped_multi}")
    print(f"Skipped (no matching original): {skipped_no_orig}")
    print(f"Skipped (original has no anon ns): {skipped_no_hash_orig}")

    if not args.apply and patched_files > 0:
        print(f"\nRun with --apply to actually patch the files.")


def main():
    parser = argparse.ArgumentParser(
        description='Patch anonymous namespace hashes in decomp .obj files to match originals')
    parser.add_argument('--apply', action='store_true',
                        help='Actually apply patches (default: dry run)')
    parser.add_argument('--verbose', '-v', action='store_true',
                        help='Show per-file details')
    parser.add_argument('--batch', action='store_true',
                        help='Process all decomp .obj files')
    parser.add_argument('--obj-dir',
                        help='Original .obj directory (default: build/373307D9/obj)')
    parser.add_argument('--src-dir',
                        help='Decomp .obj directory (default: build/373307D9/src)')
    args = parser.parse_args()

    if not args.batch:
        print("ERROR: Currently only --batch mode is supported.", file=sys.stderr)
        print("Usage: python3 scripts/obj_anon_ns_patcher.py --batch [--apply] [--verbose]",
              file=sys.stderr)
        sys.exit(1)

    process_batch(args)


if __name__ == '__main__':
    main()
