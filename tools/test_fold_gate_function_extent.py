#!/usr/bin/env python3
"""The fold/alias gates must read the FUNCTION-ONLY COMDAT extent.

`comdat_bytes.comdats()` returns two views of every COMDAT.  `raw`/`relocs`
run from the definition to the END OF THE SECTION, so an EH-bearing MSVC /Gy
COMDAT bills its trailing `__unwind$` funclet into the body -- and in a
non-/Gy monolithic section (keygen_xbox.obj) it bills the rest of the section
too (2,456 B for a 144 B function).  `fn_raw`/`fn_relocs` carry the function
extent, which is what a retail `.pdata` extent is.

Reading `raw` against a retail extent is a ONE-SIDED reader artifact -- the
same family as STLPORT-1's phantom "+8 B STLport source bug" -- and it
refuses before any byte is compared.  Measured 2026-09-15 on the 911-pair
comdat_fold_gate worklist: 146 pairs were refused as "body size N vs M" and
moved to a real byte/relocation reason once the function extent was used, one
of them flipping REFUSE -> ADMIT (`_Copy_Construct<EyeDesc>`, retail 60 B vs
our funclet-billed 104 B).

Set FOLDGATE_DIR to scan another copy of the tree -- against a copy with any
site reverted this test MUST fail.
"""
import os
import sys
from pathlib import Path

ROOT = Path(os.environ.get("FOLDGATE_DIR") or Path(__file__).resolve().parent.parent)

# Each of these compares our COMDAT against a retail .pdata extent, or groups
# COMDATs across DIFFERENT objects (whose section tails differ, so the artifact
# does NOT cancel the way it does within one object).
GUARDED = [
    "tools/fold_thunk_gate.py",
    "tools/comdat_fold_gate.py",
    "tools/alloc_fold_gate.py",
    "tools/alias_uniqueness_audit.py",
    "tools/ourside_fold_sweep.py",
]
# Deliberately NOT guarded: tools/w16s_alias_census.py keeps BOTH views on
# purpose (it reports the funclet-billed and function-only reads side by side).

BANNED = ('["raw"]', '["relocs"]')
REQUIRED = '["fn_raw"]'

failures = []
for rel in GUARDED:
    p = ROOT / rel
    if not p.exists():
        failures.append("%s: missing" % rel)
        continue
    src = p.read_text()
    for b in BANNED:
        n = src.count(b)
        if n:
            failures.append("%s: reads %s %d time(s) -- funclet-billed extent against a "
                            "retail .pdata extent; use the fn_* keys" % (rel, b, n))
    if REQUIRED not in src:
        failures.append("%s: never reads %s -- does it still consult comdat_bytes?" % (rel, REQUIRED))

if failures:
    print("FAIL test_fold_gate_function_extent (%s):" % ROOT)
    for f in failures:
        print("  - " + f)
    sys.exit(1)
print("PASS test_fold_gate_function_extent: %d gate(s) read the function-only extent (%s)"
      % (len(GUARDED), ROOT))
