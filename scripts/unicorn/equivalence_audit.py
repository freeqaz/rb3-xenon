#!/usr/bin/env python3
"""Audit the EVIDENCE QUALITY behind every unicorn verdict.

Motivation
----------
`batch_to_db.py` records a verdict per function but not *why*. That matters
because `comparator.py` returns EQUIVALENT when BOTH sides hit the same
error at the same PC (comparator.py:186). Such a row is not evidence of
behavioural equivalence — it is a shared *emulation failure* laundered into
a positive verdict. A function whose emulation dies at instruction 12 of 35
never executes its stores, so nothing it computes was ever compared.

Counting verdicts cannot detect this: the EQUIVALENT bucket looks the same
either way. This script re-runs the same schedule batch_to_db uses and
records, per function, which kind of EQUIVALENT it earned:

  equiv_real          both sides ran to completion and agreed  -> real evidence
  equiv_matching_err  both sides hit the same error at same PC -> NO evidence
  divergent           behaviour differs (with class)
  skipped / error

Output: a CSV (one row per function) plus a summary. The CSV is the durable
artifact; decomp.db is gitignored, so a DB-only result is invisible.

Usage:
    venv/bin/python scripts/unicorn/equivalence_audit.py -j 4 -o ~/tmp/audit.csv
    venv/bin/python scripts/unicorn/equivalence_audit.py --unit default/CharBones
"""

import argparse
import csv
import os
import sys
import time
import multiprocessing as mp

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, PROJECT_ROOT)

from scripts.unicorn_runner.coff import COFFParser
from scripts.unicorn_runner.comparator import classify_divergence
from scripts.unicorn_runner.memory_map import FILL_BYTE
from scripts.unicorn_runner.run import (
    get_all_units, resolve_unit, _find_common_text_symbols,
    _run_comparison_core,
    EXIT_EQUIVALENT, EXIT_DIVERGENT, EXIT_ERROR, EXIT_SKIPPED,
)

FIELDS = ["unit", "symbol", "verdict", "evidence", "div_class", "confidence",
          "matching_error", "final_pc", "decomp_terminated", "orig_terminated",
          "cap_exhausted"]


def audit_unit(name, decomp_path, orig_path, timeout=5_000_000):
    rows = []
    if not os.path.exists(decomp_path) or not os.path.exists(orig_path):
        return rows
    try:
        dc = COFFParser(decomp_path)
        oc = COFFParser(orig_path)
    except Exception:
        return rows

    for sym in _find_common_text_symbols(dc, oc):
        row = {k: "" for k in FIELDS}
        row["unit"] = name
        row["symbol"] = sym
        try:
            code, bundle, _v, _err = _run_comparison_core(
                sym, dc, oc, timeout=timeout)
        except Exception as e:
            row["verdict"] = "ERROR"
            row["evidence"] = "exception"
            row["div_class"] = type(e).__name__
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
                # was never executed, so this is NOT equivalence evidence.
                row["evidence"] = "equiv_matching_err"
                row["matching_error"] = str(details.get("matching_error"))[:80]
                pc = details.get("matching_error_pc")
                row["final_pc"] = f"0x{pc:08X}" if isinstance(pc, int) else ""
            else:
                row["evidence"] = "equiv_real"
        elif code == EXIT_DIVERGENT:
            row["verdict"] = "DIVERGENT"
            row["evidence"] = "divergent"
            if bundle is not None:
                try:
                    row["div_class"] = classify_divergence(
                        bundle.result, bundle.decomp_result, bundle.orig_result,
                        bundle.decomp_relocs, bundle.orig_relocs)
                except Exception:
                    row["div_class"] = "?"
        elif code == EXIT_SKIPPED:
            row["verdict"] = "SKIPPED"
            row["evidence"] = "skipped"
        else:
            row["verdict"] = "ERROR"
            row["evidence"] = "error"

        # Second fixture (0xCD) -> confidence, exactly as batch_to_db does.
        if code in (EXIT_EQUIVALENT, EXIT_DIVERGENT):
            try:
                code2, _b2, _v2, _e2 = _run_comparison_core(
                    sym, dc, oc, timeout=timeout, fill_pattern=FILL_BYTE)
                if code2 == code:
                    row["confidence"] = ("high" if code == EXIT_EQUIVALENT
                                         else "stable_divergent")
                else:
                    row["confidence"] = "input_sensitive"
            except Exception:
                row["confidence"] = ""
        rows.append(row)
    return rows


def _worker(args):
    name, d, o, timeout = args
    try:
        return (name, audit_unit(name, d, o, timeout), None)
    except Exception as e:
        return (name, [], str(e))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--unit", default=None)
    ap.add_argument("-j", "--jobs", type=int, default=4,
                    help="workers (CAP AT 4 -- user constraint, more can "
                         "sink the box)")
    ap.add_argument("-o", "--out", default=os.path.expanduser(
        "~/tmp/unicorn_equivalence_audit.csv"))
    ap.add_argument("--timeout", type=int, default=5_000_000)
    args = ap.parse_args()

    if args.unit:
        d, o = resolve_unit(args.unit, PROJECT_ROOT)
        units = [(args.unit, d, o)]
    else:
        units = get_all_units(PROJECT_ROOT)
    jobs = min(args.jobs, 4)
    print(f"auditing {len(units)} units with {jobs} workers -> {args.out}")

    tasks = [(n, d, o, args.timeout) for (n, d, o) in units]
    all_rows = []
    t0 = time.time()
    done = 0
    # maxtasksperchild is load-bearing, not tuning: each comparison builds
    # Unicorn engine instances, and a long-lived worker eventually dies with
    # "Could not allocate dynamic translator buffer", taking the whole run
    # with it (BrokenProcessPool). Recycling workers bounds that. Python
    # 3.10's ProcessPoolExecutor has no max_tasks_per_child, hence mp.Pool.
    with mp.Pool(processes=jobs, maxtasksperchild=8) as pool:
        for name, rows, err in pool.imap_unordered(_worker, tasks):
            done += 1
            if err:
                print(f"  ERROR {name}: {err}", file=sys.stderr)
            all_rows.extend(rows)
            if done % 100 == 0:
                print(f"  {done}/{len(units)} units, {len(all_rows)} fns, "
                      f"{time.time() - t0:.0f}s")

    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    with open(args.out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=FIELDS)
        w.writeheader()
        w.writerows(all_rows)

    ev = {}
    for r in all_rows:
        ev[r["evidence"]] = ev.get(r["evidence"], 0) + 1
    total = len(all_rows)
    print(f"\nfunctions audited: {total}   ({time.time() - t0:.0f}s)")
    for k, v in sorted(ev.items(), key=lambda kv: -kv[1]):
        print(f"  {k:20s} {v:6d}  {100.0 * v / total:5.1f}%")

    eq_real = ev.get("equiv_real", 0)
    eq_err = ev.get("equiv_matching_err", 0)
    if eq_real + eq_err:
        print(f"\nOf {eq_real + eq_err} EQUIVALENT verdicts, "
              f"{eq_err} ({100.0 * eq_err / (eq_real + eq_err):.1f}%) are a "
              f"SHARED EMULATION ERROR, not evidence of equivalence.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
