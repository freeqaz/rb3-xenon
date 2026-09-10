#!/usr/bin/env python3
"""Run ONE named fixture schedule over a unit list and emit audit-shaped rows.

WHY A SEPARATE RUNNER
---------------------
`equivalence_audit.py` hard-codes the batch schedule (zero-fill, then 0xCD for
a confidence label, r4/r5/r6 left NULL). Lane S4 named the untested directions
-- typed mocks, out-parameter coverage, non-uniform fills -- and this runner
exists to measure them one at a time, against the SAME units, so the legs are
comparable. It emits the same columns as equivalence_audit so
`deep_schedule_report.py` can diff any two legs.

It is a separate FILE rather than a flag on the audit for an operational
reason: editing a module while a long multiprocessing run imports it swaps the
tool underneath a measurement in progress (mp.Pool recycles workers, and each
recycled worker re-imports). A new file cannot do that.

THE SCHEDULES
-------------
zero        the batch default. Uniform 0x00 fill, r4/r5/r6 = 0 (NULL).
cd          uniform 0xCD fill (MSVC's uninitialised-heap byte).
outparam    zero fill, but r4/r5/r6 point at MAPPED object memory.

            S4's observation: with the arg registers NULL, any function whose
            observable effect is a write through a pointer argument faults
            identically on both sides, so it is untested. ⚠ Measured negative
            on ?ToQuat@ByteQuat@@ (1/12 either way) -- but that function dies
            at instruction 12 on an illegal instruction with ZERO calls
            logged, i.e. it never reaches its store. The knob itself is proven
            live by a `mr r3,r4; blr` control.

sentinel    NON-UNIFORM object memory: every word holds its own address
            (OBJECT_BASE + offset), vtable pointer preserved at offset 0.

            This is the direct answer to S4's "the uniform 0-fill/0xCD is what
            makes displacement changes invisible". Under a uniform fill every
            offset holds the SAME byte, so reading the wrong struct member is
            unobservable -- which is precisely how S4's first mutator scored a
            false VACUOUS 0/24 by shifting D-form displacements. Under a
            sentinel fill, reading the wrong offset yields a DIFFERENT value,
            so a wrong member access becomes visible.

            ⚠ Expected cost, stated in advance: every word is non-zero, so a
            pointer-typed member now holds a garbage pointer instead of NULL.
            Functions that null-check their members will take the other branch
            and some will crash. Expect equiv_matching_err to RISE on this leg;
            that is the schedule's price, not a defect, and it is why this is a
            separate leg rather than a replacement for the default.

typed       type-aware object memory from struct_db: floats get valid floats,
            pointers get NULL, containers get zeroed. Ported long ago and
            never run in a batch. Falls back to `zero` when the unit's class
            is not in struct_db, and SAYS SO in the coverage line -- a typed
            leg that silently degrades to zero-fill would be a vacuity.

Usage:
    venv/bin/python scripts/unicorn/schedule_legs.py --schedule sentinel \
        --units-file ~/tmp/u1_units.txt -o ~/tmp/u1_sentinel.csv -j 4
"""

import argparse
import csv
import os
import sys
import time
import multiprocessing as mp

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(
    os.path.abspath(__file__))))
sys.path.insert(0, PROJECT_ROOT)

from scripts.unicorn_runner.coff import COFFParser
from scripts.unicorn_runner.comparator import classify_divergence
from scripts.unicorn_runner.memory_map import FILL_BYTE, OBJECT_BASE
from scripts.unicorn_runner.run import (
    get_all_units, resolve_unit, _find_common_text_symbols,
    _run_comparison_core,
    EXIT_EQUIVALENT, EXIT_DIVERGENT, EXIT_ERROR, EXIT_SKIPPED,
)

FIELDS = ["unit", "symbol", "verdict", "evidence", "div_class", "confidence",
          "matching_error", "final_pc", "decomp_terminated", "orig_terminated",
          "cap_exhausted", "schedule", "fixture_note"]

SCHEDULES = ("zero", "cd", "outparam", "sentinel", "typed")


def _arg_registers(schedule):
    if schedule != "outparam":
        return None
    from unicorn.ppc_const import UC_PPC_REG_4, UC_PPC_REG_5, UC_PPC_REG_6
    return {UC_PPC_REG_4: OBJECT_BASE + 0x2000,
            UC_PPC_REG_5: OBJECT_BASE + 0x3000,
            UC_PPC_REG_6: OBJECT_BASE + 0x4000}


def _object_memory(schedule, unit_name):
    """(bytes_or_None, note). The note records whether the schedule ACTUALLY
    applied, so a silent fallback to zero-fill can never be read as a result."""
    if schedule == "sentinel":
        from scripts.unicorn_runner.typed_fixture import generate_sentinel_object
        return generate_sentinel_object(), "sentinel"
    if schedule == "typed":
        import random
        from scripts.unicorn_runner.typed_fixture import (
            extract_class_from_unit, generate_typed_object)
        try:
            from tools.struct_db import StructDB
        except ImportError:
            return None, "typed_UNAVAILABLE_struct_db_import"
        db_path = os.path.join(PROJECT_ROOT, "struct_db.sqlite")
        if not os.path.exists(db_path):
            return None, "typed_UNAVAILABLE_no_struct_db"
        cls = extract_class_from_unit(unit_name)
        if not cls:
            return None, "typed_FELL_BACK_no_class"
        db = StructDB(db_path)
        db.connect()
        try:
            mem = generate_typed_object(cls, db, random.Random(42), fill_byte=0x00)
        finally:
            db.close()
        if mem is None:
            return None, f"typed_FELL_BACK_class_absent:{cls}"
        return mem, f"typed:{cls}"
    return None, schedule


def audit_unit(name, decomp_path, orig_path, schedule, timeout=5_000_000):
    rows = []
    if not (os.path.exists(decomp_path) and os.path.exists(orig_path)):
        return rows
    try:
        dc, oc = COFFParser(decomp_path), COFFParser(orig_path)
    except Exception:
        return rows

    fill = FILL_BYTE if schedule == "cd" else None
    args = _arg_registers(schedule)
    objmem, note = _object_memory(schedule, name)

    for sym in _find_common_text_symbols(dc, oc):
        row = {k: "" for k in FIELDS}
        row.update(unit=name, symbol=sym, schedule=schedule, fixture_note=note)
        try:
            code, bundle, _v, _err = _run_comparison_core(
                sym, dc, oc, timeout=timeout, fill_pattern=fill,
                object_memory=objmem, arg_registers=args)
        except Exception as e:
            row.update(verdict="ERROR", evidence="exception",
                       div_class=type(e).__name__)
            rows.append(row)
            continue

        if bundle is not None:
            dr, orr = bundle.decomp_result, bundle.orig_result
            row["decomp_terminated"] = int(bool(
                getattr(dr, "terminated_normally", False)))
            row["orig_terminated"] = int(bool(
                getattr(orr, "terminated_normally", False)))
            row["cap_exhausted"] = int(bool(
                getattr(dr, "cap_exhausted", False)
                or getattr(orr, "cap_exhausted", False)))

        if code == EXIT_EQUIVALENT:
            row["verdict"] = "EQUIVALENT"
            details = bundle.result.details if bundle else {}
            if details.get("matching_error"):
                # Both sides crashed identically -- the function's real work
                # never ran, so this is NOT equivalence evidence.
                row["evidence"] = "equiv_matching_err"
                row["matching_error"] = str(details.get("matching_error"))[:80]
                pc = details.get("matching_error_pc")
                row["final_pc"] = f"0x{pc:08X}" if isinstance(pc, int) else ""
            else:
                row["evidence"] = "equiv_real"
        elif code == EXIT_DIVERGENT:
            row.update(verdict="DIVERGENT", evidence="divergent")
            if bundle is not None:
                try:
                    row["div_class"] = classify_divergence(
                        bundle.result, bundle.decomp_result, bundle.orig_result,
                        bundle.decomp_relocs, bundle.orig_relocs, symbol=sym)
                except Exception:
                    row["div_class"] = "?"
        elif code == EXIT_SKIPPED:
            row.update(verdict="SKIPPED", evidence="skipped")
        else:
            row.update(verdict="ERROR", evidence="error")
        rows.append(row)
    return rows


def _worker(a):
    name, d, o, sched, timeout = a
    try:
        return (name, audit_unit(name, d, o, sched, timeout), None)
    except Exception as e:
        return (name, [], str(e))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--schedule", required=True, choices=SCHEDULES)
    ap.add_argument("--units-file", default=None)
    ap.add_argument("--unit", default=None)
    ap.add_argument("-j", "--jobs", type=int, default=4,
                    help="workers (CAPPED AT 4 -- user constraint)")
    ap.add_argument("-o", "--out", required=True)
    ap.add_argument("--timeout", type=int, default=5_000_000)
    args = ap.parse_args()

    if args.unit:
        d, o = resolve_unit(args.unit, PROJECT_ROOT)
        units = [(args.unit, d, o)]
    else:
        units = get_all_units(PROJECT_ROOT)
        if args.units_file:
            with open(args.units_file) as f:
                wanted = {ln.strip() for ln in f
                          if ln.strip() and not ln.startswith("#")}
            units = [u for u in units if u[0] in wanted]

    jobs = min(args.jobs, 4)
    print(f"schedule={args.schedule}  units={len(units)}  workers={jobs} "
          f"-> {args.out}")

    t0 = time.time()
    all_rows = []
    os.makedirs(os.path.dirname(os.path.abspath(args.out)) or ".", exist_ok=True)
    with open(args.out, "w", newline="") as fh:
        w = csv.DictWriter(fh, fieldnames=FIELDS)
        w.writeheader()
        tasks = [(n, d, o, args.schedule, args.timeout) for n, d, o in units]
        # maxtasksperchild bounds the emulator-per-worker growth that turns a
        # long run into a BrokenProcessPool (see equivalence_audit).
        with mp.Pool(processes=jobs, maxtasksperchild=8) as pool:
            for name, rows, err in pool.imap_unordered(_worker, tasks):
                if err:
                    print(f"  ERROR {name}: {err}", file=sys.stderr)
                all_rows.extend(rows)
                w.writerows(rows)
                fh.flush()

    import collections
    ev = collections.Counter(r["evidence"] for r in all_rows)
    total = len(all_rows) or 1
    print(f"\nfunctions: {len(all_rows)}   ({time.time() - t0:.0f}s)")
    for k, v in ev.most_common():
        print(f"  {k:20s} {v:6d}  {100.0 * v / total:5.1f}%")

    # Coverage of the schedule itself: a leg that silently degraded to
    # zero-fill everywhere would otherwise look like a measured null.
    notes = collections.Counter(r["fixture_note"] for r in all_rows)
    print("\nfixture actually applied:")
    for k, v in notes.most_common(8):
        flag = "  <== DEGRADED" if ("FELL_BACK" in k or "UNAVAILABLE" in k) else ""
        print(f"  {k:40s} {v:6d}{flag}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
