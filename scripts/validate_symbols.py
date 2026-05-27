#!/usr/bin/env python3
"""Validate symbols.txt addresses against known section ranges.

Checks that function symbols in .text fall within the valid .text virtual
address range for the target binary. Reports any out-of-range entries.

Usage:
    python3 scripts/validate_symbols.py [--limit N] [--json] [config/45410914/symbols.txt]
"""

import argparse
import json
import re
import sys

# Section ranges for 45410914 (vanilla retail XEX)
SECTION_RANGES = {
    ".text":   (0x82260000, 0x82C148D4),
    ".rdata":  (0x82000400, 0x821E99AC),
    ".pdata":  (0x821E9A00, 0x82258D70),
    ".data":   (0x82C34400, 0x82E279EC),
}

ADDR_PAT = re.compile(
    r"^(.+?)\s*=\s*(\.[a-z]+):0x([0-9A-Fa-f]+)\s*;"
    r".*?type:(\w+)"
)


def validate(path, limit=0):
    errors = []
    counts = {"total": 0, "checked": 0, "valid": 0, "invalid": 0}
    section_counts = {}

    with open(path, errors="replace") as f:
        for lineno, line in enumerate(f, 1):
            counts["total"] += 1
            m = ADDR_PAT.match(line)
            if not m:
                continue

            name, section, addr_hex, sym_type = m.groups()
            addr = int(addr_hex, 16)
            section_counts[section] = section_counts.get(section, 0) + 1

            if section not in SECTION_RANGES:
                continue

            # Only validate function symbols in .text
            if section == ".text" and sym_type == "function":
                counts["checked"] += 1
                lo, hi = SECTION_RANGES[section]
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
    args = parser.parse_args()

    counts, errors, section_counts = validate(args.path, args.limit)

    if args.json:
        print(json.dumps({
            "counts": counts,
            "section_counts": section_counts,
            "invalid_count": len(errors),
            "sample_errors": errors[:args.limit] if args.limit else errors,
        }, indent=2))
    else:
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
