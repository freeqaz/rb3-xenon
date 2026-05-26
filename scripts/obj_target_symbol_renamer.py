#!/usr/bin/env python3
"""Post-SPLIT patcher: rename anonymous `fn_<addr>` symbols in dtk-split
target .obj files to their MSVC-mangled equivalents.

Background
----------
dtk's `xex split` step emits one section per function and names every code
symbol `fn_<addr>` (e.g. `fn_8275A2C0`). Our compiled .obj files (produced
by the MSVC X360 toolchain via wibo) carry full MSVC-mangled symbols
(e.g. `?SetupTrackChannel_@MasterAudio@@QAAXHAAVExtraTrackInfo@1@@Z`).

objdiff's auto-pairing relies on symbol name equality, so the gap blocks
pairing even when the instruction bytes match exactly. The bool-mangle
patcher rewrites compiled-side names toward the target side, but only
fires when the target side has a name to anchor onto — useless here.

This patcher takes the opposite approach: rewrite **target-side**
`fn_<addr>` symbols to the MSVC-mangled name we expect, based on an
explicit address->name map. After patching, objdiff auto-pairs both the
top-level symbol and the relocations that reference the renamed callees.

Map file
--------
`scripts/target_symbol_map.json` (project-relative). Shape:

    {
        "0x8275A2C0": "?SetupTrackChannel_@MasterAudio@@QAAXHAAVExtraTrackInfo@1@@Z",
        "0x82759A78": "?SetupTrackChannel@MasterAudio@@QAAXH_NM00@Z",
        ...
    }

Idempotent: re-running on an already-patched obj is a no-op (the
`fn_<addr>` symbol is gone, so the rename rule finds nothing to do).

Usage
-----
    python3 scripts/obj_target_symbol_renamer.py --batch --apply

Or for a single file:
    python3 scripts/obj_target_symbol_renamer.py --apply \\
        build/45410914/obj/MasterAudio.obj
"""

import argparse
import glob
import json
import os
import struct
import sys
from pathlib import Path
from typing import Dict, List, Optional, Tuple

PROJECT_ROOT = Path(__file__).resolve().parent.parent
DEFAULT_MAP = PROJECT_ROOT / "scripts" / "target_symbol_map.json"


def parse_coff_symbols(data: bytes) -> List[Tuple[str, int, int, int]]:
    """Return list of (name, entry_off, str_abs_off_or_-1, str_len)."""
    if len(data) < 20:
        return []
    sym_offset = struct.unpack_from("<I", data, 8)[0]
    num_syms = struct.unpack_from("<I", data, 12)[0]
    if sym_offset == 0 or num_syms == 0:
        return []
    str_table_offset = sym_offset + num_syms * 18
    symbols: List[Tuple[str, int, int, int]] = []
    i = 0
    while i < num_syms:
        entry_off = sym_offset + i * 18
        if entry_off + 18 > len(data):
            break
        name_bytes = data[entry_off:entry_off + 8]
        is_long = name_bytes[:4] == b"\x00\x00\x00\x00"
        if is_long:
            str_off = struct.unpack_from("<I", name_bytes, 4)[0]
            abs_off = str_table_offset + str_off
            if abs_off < len(data):
                end = data.index(b"\x00", abs_off)
                name = data[abs_off:end].decode("ascii", errors="replace")
                str_abs = abs_off
                str_len = end - abs_off
            else:
                name, str_abs, str_len = "", -1, 0
        else:
            name = name_bytes.split(b"\x00")[0].decode("ascii", errors="replace")
            str_abs = -1
            str_len = len(name)
        aux_count = data[entry_off + 17]
        symbols.append((name, entry_off, str_abs, str_len))
        i += 1 + aux_count
    return symbols


def load_address_map(path: Path) -> Dict[str, str]:
    """Load address->name map. Keys may be `"0xABCDEF12"`, `"0xabcdef12"`,
    or bare hex. Normalize to uppercase `fn_ABCDEF12`."""
    raw = json.loads(path.read_text())
    out: Dict[str, str] = {}
    for k, v in raw.items():
        # Skip metadata / comment entries (keys not starting with 0x).
        if not k.lower().startswith("0x"):
            continue
        k_clean = k.lower().removeprefix("0x")
        try:
            addr = int(k_clean, 16)
        except ValueError:
            print(f"WARN: invalid address key {k!r} in {path}", file=sys.stderr)
            continue
        out[f"fn_{addr:08X}"] = v
    return out


def rename_symbols(
    data: bytearray, renames: Dict[str, str]
) -> Tuple[int, List[str]]:
    """Apply renames in-place to a COFF .obj buffer.

    Returns (num_patches, details).
    """
    sym_offset = struct.unpack_from("<I", data, 8)[0]
    num_syms = struct.unpack_from("<I", data, 12)[0]
    if sym_offset == 0 or num_syms == 0:
        return 0, []

    str_table_start = sym_offset + num_syms * 18
    str_table_size = struct.unpack_from("<I", data, str_table_start)[0]

    symbols = parse_coff_symbols(data)
    num_applied = 0
    details: List[str] = []

    for sym_name, entry_off, str_abs, str_len in symbols:
        if sym_name not in renames:
            continue
        new_name = renames[sym_name]
        new_bytes = new_name.encode("ascii")

        # Always append to the string table and convert to long-name form.
        # This keeps the logic simple and avoids overlap/truncation edge cases.
        new_str_off = str_table_size
        data.extend(new_bytes + b"\x00")
        str_table_size += len(new_bytes) + 1
        struct.pack_into("<I", data, str_table_start, str_table_size)
        # Mark the symbol's name field as a string-table reference (long form).
        data[entry_off:entry_off + 4] = b"\x00\x00\x00\x00"
        struct.pack_into("<I", data, entry_off + 4, new_str_off)
        num_applied += 1
        details.append(f"  {sym_name} -> {new_name}")

    return num_applied, details


def patch_file(
    path: Path, renames: Dict[str, str], apply: bool, verbose: bool
) -> Tuple[int, List[str]]:
    data = bytearray(path.read_bytes())
    n, details = rename_symbols(data, renames)
    if n > 0 and apply:
        path.write_bytes(data)
    return n, details


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Rename fn_<addr> symbols in target .obj files to MSVC mangled names"
    )
    parser.add_argument(
        "--batch", action="store_true", help="Process every .obj in --obj-dir"
    )
    parser.add_argument(
        "--apply", action="store_true", help="Write changes (default: dry-run)"
    )
    parser.add_argument(
        "--verbose", "-v", action="store_true", help="Print per-file details"
    )
    parser.add_argument(
        "--obj-dir",
        default="build/45410914/obj",
        help="Target .obj directory (default: %(default)s)",
    )
    parser.add_argument(
        "--map",
        default=str(DEFAULT_MAP),
        help="JSON address->name map (default: %(default)s)",
    )
    parser.add_argument(
        "files", nargs="*", help="Specific .obj files to patch (overrides --batch)"
    )
    args = parser.parse_args()

    if not args.batch and not args.files:
        parser.error("Specify --batch or provide files")

    map_path = Path(args.map)
    if not map_path.is_file():
        # An empty/missing map is not an error — pipeline can run before any
        # mappings are declared. Treat as zero-op.
        if args.verbose:
            print(f"[skip] no map file at {map_path}")
        renames: Dict[str, str] = {}
    else:
        try:
            renames = load_address_map(map_path)
        except json.JSONDecodeError as exc:
            print(f"ERROR: cannot parse {map_path}: {exc}", file=sys.stderr)
            return 1

    obj_dir = Path(args.obj_dir)
    if args.batch:
        targets = sorted(
            Path(p) for p in glob.glob(str(obj_dir / "**/*.obj"), recursive=True)
        )
    else:
        targets = [Path(f) for f in args.files]

    total_files = 0
    patched_files = 0
    total_patches = 0
    for target in targets:
        if not target.exists():
            continue
        total_files += 1
        n, details = patch_file(target, renames, args.apply, args.verbose)
        if n > 0:
            patched_files += 1
            total_patches += n
            if args.verbose:
                print(f"{target}: {n} renames")
                for d in details:
                    print(d)

    mode = "APPLIED" if args.apply else "DRY RUN"
    print(
        f"[{mode}] {total_files} files checked, "
        f"{patched_files} files patched, "
        f"{total_patches} total symbol renames"
    )
    if not args.apply and total_patches > 0:
        print("Run with --apply to modify files.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
