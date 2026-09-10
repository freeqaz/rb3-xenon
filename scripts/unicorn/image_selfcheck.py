#!/usr/bin/env python3
"""Assert the shipped-image seeding source is LIVE, and describe it.

WHY THIS IS A SEPARATE, LOUD CHECK
----------------------------------
`image.get_global_image()` degrades gracefully: a missing or unparseable image
returns a `_MissingImage` that seeds nothing. That is the right runtime
behaviour -- the harness keeps working -- but it is a VACUITY HAZARD for
anyone measuring the effect of seeding, because "the image was never loaded"
and "seeding changed nothing" produce the identical null result, and the null
agrees with the comfortable prior that our globals were fine all along.

So before any before/after number about seeding is believed, run this. It
exits non-zero when the image is unavailable, and prints the counts that make
a null result interpretable: if the pools below are large and a sweep still
moves nothing, that is evidence; if this script cannot even open band.exe, the
sweep measured nothing at all.

Usage:
    venv/bin/python scripts/unicorn/image_selfcheck.py
"""

import collections
import os
import sys

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
sys.path.insert(0, PROJECT_ROOT)

from scripts.unicorn_runner.image import get_global_image


def main():
    img = get_global_image(PROJECT_ROOT)
    print(f"project root : {PROJECT_ROOT}")
    print(f"available    : {img.available}")
    if not img.available:
        print(f"reason       : {img.reason}")
        print()
        print("FAIL: the image is NOT loaded. Any 'seeding changed nothing' "
              "result measured in this state is VACUOUS, not a negative.")
        return 1

    syms = img._symbols
    by_section = collections.Counter(s.section for s in syms.values())
    print(f"sections     : {len(img._sections)}")
    for name, start, vsize, raw_off, raw_size in img._sections:
        tail = max(0, vsize - raw_size)
        note = f"  (.bss tail {tail:#x} reads zero)" if tail else ""
        print(f"    {name:<10} VA {start:#010x} vsize {vsize:#08x} "
              f"raw {raw_size:#08x}{note}")
    print(f"data symbols : {len(syms)}")
    for sec, n in by_section.most_common(8):
        print(f"    {sec:<10} {n}")

    # The seedable pool: scalars with real content that are not pointers.
    scalars = zeros = pointers = big = 0
    for s in syms.values():
        if not (0 < s.size <= 4096):
            big += 1
            continue
        content = img.read(s.address, s.size)
        if not content:
            continue
        if not any(content):
            zeros += 1
        elif img.contains_image_pointer(content):
            pointers += 1
        else:
            scalars += 1
    print()
    print("seeding pool (what seed_image_globals could ever act on):")
    print(f"    nonzero, pointer-free : {scalars}   <- SEEDABLE")
    print(f"    all-zero in the image : {zeros}      (skipped: slot already right)")
    print(f"    contains a pointer    : {pointers}   (skipped: unmappable)")
    print(f"    size 0 or > 4096      : {big}        (skipped: tables)")

    if scalars == 0:
        print()
        print("FAIL: the image loaded but nothing is seedable -- treat as vacuous.")
        return 1
    print()
    print("OK: image is live and the seedable pool is non-empty.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
