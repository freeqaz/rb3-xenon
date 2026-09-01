#!/usr/bin/env python3
"""Anti-vacuity control for the unicorn behavioural harness.

An equivalence instrument that cannot emit DIVERGENT is worse than no
instrument: it returns a clean, confident "no divergences" that agrees with
the comfortable prior. Verdict counts alone cannot establish sensitivity —
they are the instrument restating its own input.

This script measures sensitivity DIRECTLY. For a function the harness
currently calls EQUIVALENT, it corrupts our compiled object's bytes one
instruction at a time (perturbing the low 16 bits of the encoding) and
requires the verdict to flip to DIVERGENT.

Reports:
  * NULL leg      -- unmutated run must be EQUIVALENT (guards against a
                     harness that calls everything DIVERGENT)
  * DETECTED/N    -- how many single-instruction corruptions were caught

A detection rate is not expected to be 100%: some instruction slots are
genuinely dead with respect to observable state under a zero-fill schedule
(padding, dead stores, unreached branches). The load-bearing claim is that
the rate is substantially above zero AND the null leg passes.

Usage:
    venv/bin/python scripts/unicorn/mutation_sensitivity.py \
        --unit default/CharBones --symbol '?ToQuat@ByteQuat@@QBAXAAVQuat@Hmx@@@Z'
"""

import argparse
import os
import struct
import sys
import tempfile

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, PROJECT_ROOT)

from scripts.unicorn_runner.coff import COFFParser
from scripts.unicorn_runner.run import (
    resolve_unit, _run_comparison_core,
    EXIT_EQUIVALENT, EXIT_DIVERGENT, EXIT_ERROR, EXIT_SKIPPED,
)
from scripts.unicorn_runner.memory_map import FILL_BYTE, OBJECT_BASE

NAMES = {EXIT_EQUIVALENT: "EQUIVALENT", EXIT_DIVERGENT: "DIVERGENT",
         EXIT_ERROR: "ERROR", EXIT_SKIPPED: "SKIPPED"}


def find_symbol_file_offset(coff, symbol):
    """Return (file_offset, size_bytes) of a .text symbol's body in the file."""
    sym = coff.symbol_map.get(symbol)
    if sym is None or sym['section'] <= 0:
        return None, None
    sec = coff.sections[sym['section'] - 1]
    if not sec['name'].startswith('.text'):
        return None, None
    # Size: distance to next symbol in the same section, else section end.
    starts = sorted(
        s['value'] for s in coff.symbols
        if s['section'] == sym['section'] and s['value'] > sym['value']
    )
    end = starts[0] if starts else sec['raw_size']
    return sec['raw_offset'] + sym['value'], end - sym['value']


ARG_REGS = None  # optional {uc_reg_id: value} applied to BOTH sides


def run_once(decomp_path, orig_path, symbol):
    """Run BOTH fixtures the batch driver uses and combine.

    batch_to_db decides the verdict from the zero-fill run alone and uses
    the 0xCD run only for a confidence label. For a sensitivity measurement
    that understates nothing, treat a divergence under EITHER fixture as a
    detection, and report which fixture caught it.
    """
    decomp = COFFParser(decomp_path)
    orig = COFFParser(orig_path)
    code_z, _b, _v, err = _run_comparison_core(
        symbol, decomp, orig, arg_registers=ARG_REGS)
    try:
        code_c, _b2, _v2, _e2 = _run_comparison_core(
            symbol, decomp, orig, fill_pattern=FILL_BYTE,
            arg_registers=ARG_REGS)
    except Exception:
        code_c = None
    return code_z, code_c, err


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--unit", required=True)
    ap.add_argument("--symbol", required=True)
    ap.add_argument("--max-mutations", type=int, default=24)
    ap.add_argument("--point-args-at-memory", action="store_true",
                    help="Point r4/r5/r6 at MAPPED object memory instead of "
                         "leaving them 0 (NULL). The batch schedule leaves "
                         "them 0, which makes every store through an "
                         "out-parameter land on an unmapped page identically "
                         "on both sides -- i.e. unobservable.")
    args = ap.parse_args()

    global ARG_REGS
    if args.point_args_at_memory:
        from unicorn.ppc_const import (UC_PPC_REG_4, UC_PPC_REG_5,
                                       UC_PPC_REG_6)
        ARG_REGS = {UC_PPC_REG_4: OBJECT_BASE + 0x2000,
                    UC_PPC_REG_5: OBJECT_BASE + 0x3000,
                    UC_PPC_REG_6: OBJECT_BASE + 0x4000}
        print("arg regs: r4/r5/r6 -> mapped object memory")

    decomp_path, orig_path = resolve_unit(args.unit, PROJECT_ROOT)
    print(f"unit   : {args.unit}")
    print(f"decomp : {decomp_path}")
    print(f"orig   : {orig_path}")
    print(f"symbol : {args.symbol}")

    # --- NULL LEG: unmutated must be EQUIVALENT -------------------------
    code, code_c, err = run_once(decomp_path, orig_path, args.symbol)
    print(f"\nNULL leg (unmutated): zero-fill={NAMES.get(code, code)} "
          f"0xCD-fill={NAMES.get(code_c, code_c)}"
          + (f"  [{err}]" if err else ""))
    if code != EXIT_EQUIVALENT:
        print("REFUSED: null leg is not EQUIVALENT -- this function is not a "
              "valid sensitivity fixture (pick one the harness currently "
              "calls EQUIVALENT).")
        return 2

    coff = COFFParser(decomp_path)
    off, size = find_symbol_file_offset(coff, args.symbol)
    if off is None:
        print("REFUSED: symbol not found in a .text section of the decomp obj")
        return 2
    print(f"body   : file offset 0x{off:X}, {size} bytes "
          f"({size // 4} instructions)")

    with open(decomp_path, "rb") as f:
        original_blob = f.read()

    n_insns = size // 4
    slots = list(range(n_insns))[:args.max_mutations]

    detected = 0
    tried = 0
    outcomes = {}
    tmpdir = tempfile.mkdtemp(prefix="unicorn_mut_")
    mutant_path = os.path.join(tmpdir, os.path.basename(decomp_path))

    for i in slots:
        ins_off = off + i * 4
        word = struct.unpack_from(">I", original_blob, ins_off)[0]
        if word == 0:
            continue  # padding: mutating it is not a meaningful probe
        # Mutation = DELETE the instruction (replace with nop).
        #
        # An earlier version XOR'd the low 16 bits. That was a defective
        # mutator and it produced a false VACUOUS verdict: on X/A-form
        # instructions bit 31 is the Rc bit, so the "corruption" only set
        # CR0 (semantically near-identical), and on D-form loads it shifted
        # a memory DISPLACEMENT -- invisible under a uniform fill, where
        # every offset holds the same byte. Deleting the instruction is
        # opcode-agnostic and removes a real computation.
        mutated = 0x60000000  # nop (ori r0,r0,0)
        if mutated == word:
            continue
        blob = bytearray(original_blob)
        struct.pack_into(">I", blob, ins_off, mutated)
        with open(mutant_path, "wb") as f:
            f.write(bytes(blob))

        tried += 1
        try:
            cz, cc, _err = run_once(mutant_path, orig_path, args.symbol)
            nz, nc = NAMES.get(cz, str(cz)), NAMES.get(cc, str(cc))
            name = f"{nz}/{nc}"
            hit = (cz == EXIT_DIVERGENT) or (cc == EXIT_DIVERGENT)
        except Exception as e:  # a crash is a detection too, but label it
            name = f"EXCEPTION({type(e).__name__})"
            hit = True
        outcomes[name] = outcomes.get(name, 0) + 1
        if hit:
            detected += 1
        print(f"  insn {i:3d} @0x{ins_off:X}: 0x{word:08X} -> nop"
              f"  =>  {name}   {'DETECTED' if hit else 'missed'}")

    os.remove(mutant_path)
    os.rmdir(tmpdir)

    # Confirm we did not corrupt the real object on disk.
    with open(decomp_path, "rb") as f:
        assert f.read() == original_blob, "decomp obj was modified on disk!"

    print(f"\nSENSITIVITY: {detected}/{tried} single-instruction corruptions "
          f"detected as DIVERGENT")
    print(f"outcomes: {outcomes}")
    print("decomp obj on disk verified UNCHANGED")
    if tried and detected == 0:
        print("VERDICT: VACUOUS -- instrument detected nothing. Do not trust "
              "any EQUIVALENT verdict it emits.")
        return 1
    print("VERDICT: instrument DISCRIMINATES (null passes, corruptions caught)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
