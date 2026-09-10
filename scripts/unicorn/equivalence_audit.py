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
    ap.add_argument("--resume", action="store_true",
                    help="skip units already present in the output CSV")
    ap.add_argument("--sample-every", type=int, default=None, metavar="N",
                    help="audit every Nth unit of the NAME-SORTED unit list. "
                         "A full 1,045-unit run is too expensive to repeat, "
                         "and an A/B needs the SAME units on both legs -- "
                         "name-sorted striding is deterministic and spreads "
                         "across directories, unlike a prefix (which would "
                         "sample one subsystem) or a random draw (which would "
                         "not reproduce).")
    ap.add_argument("--units-file", default=None,
                    help="file of unit names, one per line; overrides sampling")
    args = ap.parse_args()

    if args.unit:
        d, o = resolve_unit(args.unit, PROJECT_ROOT)
        units = [(args.unit, d, o)]
    else:
        units = get_all_units(PROJECT_ROOT)

    if args.units_file:
        wanted = set()
        with open(args.units_file) as f:
            for line in f:
                line = line.strip()
                if line and not line.startswith("#"):
                    wanted.add(line)
        units = [u for u in units if u[0] in wanted]
        missing = wanted - {u[0] for u in units}
        if missing:
            print(f"WARNING: {len(missing)} named units are not probeable "
                  f"(no target/base obj pair): {sorted(missing)[:5]}",
                  file=sys.stderr)
    elif args.sample_every and args.sample_every > 1:
        units = sorted(units, key=lambda u: u[0])[::args.sample_every]
        print(f"sampling every {args.sample_every}th unit (name-sorted): "
              f"{len(units)} units")

    jobs = min(args.jobs, 4)

    # Resume support. Rows are flushed per unit (see below), so a run killed
    # by the box -- or by Unicorn failing to mmap its translator buffer under
    # memory pressure -- leaves usable partial results instead of nothing.
    done_units = set()
    all_rows = []
    if args.resume and os.path.exists(args.out):
        with open(args.out) as f:
            for r in csv.DictReader(f):
                done_units.add(r["unit"])
                all_rows.append(r)
        print(f"resume: {len(done_units)} units already done "
              f"({len(all_rows)} rows)")
        units = [u for u in units if u[0] not in done_units]

    print(f"auditing {len(units)} units with {jobs} workers -> {args.out}")
    tasks = [(n, d, o, args.timeout) for (n, d, o) in units]
    t0 = time.time()
    done = 0
    os.makedirs(os.path.dirname(os.path.abspath(args.out)), exist_ok=True)
    append = args.resume and os.path.exists(args.out)
    out_fh = open(args.out, "a" if append else "w", newline="")
    writer = csv.DictWriter(out_fh, fieldnames=FIELDS)
    if not append:
        writer.writeheader()
        out_fh.flush()
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
            writer.writerows(rows)
            out_fh.flush()   # checkpoint: never lose a completed unit
            if done % 100 == 0:
                print(f"  {done}/{len(units)} units, {len(all_rows)} fns, "
                      f"{time.time() - t0:.0f}s")

    out_fh.close()

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
