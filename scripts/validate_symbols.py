#!/usr/bin/env python3
"""Validate symbols.txt addresses against the LIVE binary's section ranges.

Checks that function symbols in .text fall within the valid .text virtual
address range for the target binary. Reports any out-of-range entries.

Ranges are read from the PE section table of the extracted image
(``orig/45410914/band.exe``, which the build re-extracts from the XEX), so the
check follows a target flip automatically.

⛔ This script used to carry a HARDCODED ``SECTION_RANGES`` table from the TU0
era.  main has targeted TU5 since 2026-07-15, and **all four ranges were
wrong**.  Measured 2026-08-17 (lane W44-REQUEUE) over 69,060 checked symbols:

    agree valid     66,776
    FALSE ALARMS     2,284   <- every single "invalid" it reported was phantom
    FALSE PASSES         0
    agree invalid        0

i.e. the checker was 100% noise, and noise that trains people to ignore a
checker is a real defect.  The stale table was also *too permissive at the
bottom* (.text lo 0x82260000 vs the live 0x82270000), so it was capable of
false passes as well -- it happened to have none only because no symbol lands
in that 64 KB.

Refuses (exit 2) if it cannot read the section table.  It must never silently
fall back to constants: that is precisely the failure this replaced.

Usage:
    python3 scripts/validate_symbols.py [--limit N] [--json] [config/45410914/symbols.txt]
    python3 scripts/validate_symbols.py --self-test   # prove the check CAN fail
"""

import argparse
import json
import re
import struct
import sys
from pathlib import Path

# Sections whose symbols we range-check.  Populated from the live image.
CHECKED_SECTIONS = (".text", ".rdata", ".pdata", ".data")

DEFAULT_IMAGE = "orig/45410914/band.exe"


def read_section_ranges(image_path):
    """Return {section: (lo, hi)} from a PE's section table, or raise."""
    data = Path(image_path).read_bytes()
    if len(data) < 0x40 or data[:2] != b"MZ":
        raise ValueError(f"{image_path}: not a PE image (no MZ header)")
    pe = struct.unpack_from("<I", data, 0x3C)[0]
    if data[pe:pe + 4] != b"PE\0\0":
        raise ValueError(f"{image_path}: bad PE signature at 0x{pe:X}")
    nsec = struct.unpack_from("<H", data, pe + 6)[0]
    optsz = struct.unpack_from("<H", data, pe + 20)[0]
    magic = struct.unpack_from("<H", data, pe + 24)[0]
    if magic == 0x10B:      # PE32
        base = struct.unpack_from("<I", data, pe + 24 + 28)[0]
    elif magic == 0x20B:    # PE32+
        base = struct.unpack_from("<Q", data, pe + 24 + 24)[0]
    else:
        raise ValueError(f"{image_path}: unknown optional-header magic {magic:#x}")

    ranges = {}
    off = pe + 24 + optsz
    for _ in range(nsec):
        name = data[off:off + 8].rstrip(b"\0").decode("ascii", "replace")
        vsize, va = struct.unpack_from("<II", data, off + 8)
        if name in CHECKED_SECTIONS:
            ranges[name] = (base + va, base + va + vsize)
        off += 40
    if ".text" not in ranges:
        raise ValueError(f"{image_path}: no .text section found")
    return ranges

ADDR_PAT = re.compile(
    r"^(.+?)\s*=\s*(\.[a-z]+):0x([0-9A-Fa-f]+)\s*;"
    r".*?type:(\w+)"
)


def validate(path, ranges, limit=0, extra_lines=()):
    errors = []
    counts = {"total": 0, "checked": 0, "valid": 0, "invalid": 0}
    section_counts = {}

    with open(path, errors="replace") as f:
        lines = list(enumerate(f, 1))
    lines += [(-1, t) for t in extra_lines]

    if True:
        for lineno, line in lines:
            counts["total"] += 1
            m = ADDR_PAT.match(line)
            if not m:
                continue

            name, section, addr_hex, sym_type = m.groups()
            addr = int(addr_hex, 16)
            section_counts[section] = section_counts.get(section, 0) + 1

            if section not in ranges:
                continue

            # Only validate function symbols in .text
            if section == ".text" and sym_type == "function":
                counts["checked"] += 1
                lo, hi = ranges[section]
                if lo <= addr < hi:
                    counts["valid"] += 1
                else:
                    counts["invalid"] += 1
                    errors.append({
                        "line": lineno,
                        "name": name.strip(),
                        "section": section,
                        "address": f"0x{addr:08X}",
                        "range": f"0x{lo:08X}..0x{hi:08X}",
                    })

    return counts, errors, section_counts


def main():
    parser = argparse.ArgumentParser(description="Validate symbols.txt addresses")
    parser.add_argument("path", nargs="?", default="config/45410914/symbols.txt")
    parser.add_argument("--limit", type=int, default=10,
                        help="Max invalid entries to print (0=all)")
    parser.add_argument("--json", action="store_true",
                        help="Output machine-readable JSON summary")
    parser.add_argument("--image", default=DEFAULT_IMAGE,
                        help=f"PE image to read section ranges from (default {DEFAULT_IMAGE})")
    parser.add_argument("--self-test", action="store_true",
                        help="Inject an out-of-range symbol and assert the check FAILS. "
                             "A checker that cannot fail is worse than no checker.")
    args = parser.parse_args()

    # Ranges come from the live image.  Refuse rather than fall back to
    # constants -- a stale hardcoded table is exactly what this replaced.
    try:
        ranges = read_section_ranges(args.image)
    except (OSError, ValueError) as exc:
        print(f"CANNOT VALIDATE: {exc}", file=sys.stderr)
        print("  (build the target first so the image is extracted, or pass --image)",
              file=sys.stderr)
        return 2

    if args.self_test:
        # A poisoned line one byte past the end of .text must be caught.
        bad = ranges[".text"][1] + 0x10
        poison = f"fn_W44SELFTEST = .text:0x{bad:08X}; // type:function size:0x4\n"
        clean_counts, clean_errors, _ = validate(args.path, ranges, args.limit)
        _, dirty_errors, _ = validate(args.path, ranges, args.limit,
                                      extra_lines=[poison])
        caught = len(dirty_errors) - len(clean_errors)
        print("== validate_symbols self-test ==")
        for sec in sorted(ranges):
            lo, hi = ranges[sec]
            print(f"  live {sec:<7} 0x{lo:08X}..0x{hi:08X}   (from {args.image})")
        print(f"  clean run          : {len(clean_errors)} invalid")
        print(f"  with poisoned addr : {len(dirty_errors)} invalid  (delta {caught:+d})")
        if caught == 1:
            print("  SELF-TEST PASS -- the check discriminates.")
            return 0
        print("  SELF-TEST FAIL -- the check did NOT catch an out-of-range symbol.",
              file=sys.stderr)
        return 1

    counts, errors, section_counts = validate(args.path, ranges, args.limit)

    if args.json:
        print(json.dumps({
            "image": args.image,
            "ranges": {k: [f"0x{v[0]:08X}", f"0x{v[1]:08X}"] for k, v in ranges.items()},
            "counts": counts,
            "section_counts": section_counts,
            "invalid_count": len(errors),
            "sample_errors": errors[:args.limit] if args.limit else errors,
        }, indent=2))
    else:
        lo, hi = ranges[".text"]
        print(f"Section ranges from: {args.image}")
        print(f"  .text 0x{lo:08X}..0x{hi:08X}")
        print(f"Total lines: {counts['total']}")
        print(f"Checked .text functions: {counts['checked']}")
        print(f"  Valid: {counts['valid']}")
        print(f"  Invalid: {counts['invalid']}")
        if errors:
            show = errors[:args.limit] if args.limit else errors
            print(f"\nInvalid entries (showing {len(show)}/{len(errors)}):")
            for e in show:
                print(f"  L{e['line']}: {e['name']}")
                print(f"    {e['section']}:{e['address']} not in {e['range']}")

    return 1 if errors else 0


if __name__ == "__main__":
    sys.exit(main())
