#!/usr/bin/env python3
"""Split `call_arg` divergences into artifact-suspect vs candidate-real.

Why this exists
---------------
`call_arg` is the second-largest class in the matched-but-wrong worklist, and
taken at face value each row says "we pass a different argument than retail
does". But the emulator mocks each relocation target by mapping it into a
region chosen by the target's SECTION (memory_map.py): GLOBAL_BASE for data,
RDATA_BASE for read-only data, OBJECT_BASE, VTABLE_BASE, and so on.

So if the same conceptual symbol lives in `.rdata` in our object and `.data`
in retail's -- a build/layout difference, not a semantic one -- the mocked
POINTER VALUES differ and the comparator reports `call_arg`. Measured
example, `?Handle@UIScreen@@`: identical call COUNT (2633 on both sides),
diverging at call #2 where we pass 0x800200B0 (RDATA region) and retail
passes 0x30000008 (GLOBALS region).

A pointer that differs only in WHICH MOCK REGION it lands in is weak
evidence of a behavioural bug. A pointer that differs WITHIN a region, or a
non-pointer scalar that differs, is much stronger.

This does not prove the cross-region rows are all artifacts -- passing a
genuinely wrong object could also cross regions. It bounds how much of the
class is explained by a mechanism that is not a bug.
"""

import argparse
import csv
import os
import sys
from collections import Counter

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, PROJECT_ROOT)

from scripts.unicorn_runner.coff import COFFParser
from scripts.unicorn_runner.run import resolve_unit, _run_comparison_core
from scripts.unicorn_runner.memory_map import (
    STACK_BASE, OBJECT_BASE, GLOBAL_BASE, VTABLE_BASE, CODE_BASE,
    TRAMPOLINE_BASE, RDATA_BASE, SENTINEL_ADDR, REGION_SIZE,
)

REGIONS = [
    ("stack", STACK_BASE), ("object", OBJECT_BASE), ("globals", GLOBAL_BASE),
    ("vtable", VTABLE_BASE), ("code", CODE_BASE),
    ("trampoline", TRAMPOLINE_BASE), ("rdata", RDATA_BASE),
    ("sentinel", SENTINEL_ADDR),
]


def region_of(val):
    best = None
    for name, base in REGIONS:
        if base <= val < base + REGION_SIZE:
            # trampoline/rdata overlap the code range; prefer the tighter one.
            if best is None or base > best[1]:
                best = (name, base)
    return best[0] if best else ("scalar" if val < 0x1000000 else "other")


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", required=True)
    ap.add_argument("--limit", type=int, default=0)
    ap.add_argument("--klass", default="call_arg",
                    choices=["call_arg", "object_memory"])
    ap.add_argument("--out", default=None)
    args = ap.parse_args()

    want = args.klass
    rows = [r for r in csv.DictReader(open(args.csv))
            if r["verdict"] == "DIVERGENT" and r["div_class"] == want]
    if args.limit:
        rows = rows[:args.limit]
    print(f"{want} rows to characterize: {len(rows)}")

    by_unit = {}
    for r in rows:
        by_unit.setdefault(r["unit"], []).append(r["symbol"])

    tally = Counter()
    detail = []
    for unit, syms in by_unit.items():
        try:
            d, o = resolve_unit(unit, PROJECT_ROOT)
            dc, oc = COFFParser(d), COFFParser(o)
        except Exception as e:
            tally["resolve_error"] += len(syms)
            continue
        for sym in syms:
            try:
                code, b, _v, _e = _run_comparison_core(sym, dc, oc)
            except Exception:
                tally["rerun_error"] += 1
                continue
            if b is None or not b.result.details:
                tally["no_detail"] += 1
                continue
            det = b.result.details
            if want == "object_memory":
                if det.get("reason") != "memory_mismatch":
                    tally[f"reclassified:{det.get('reason')}"] += 1
                    continue
                diffs = det.get("object_diffs") or []
                if not diffs:
                    tally["no_object_diffs"] += 1
                    continue
                # Same question as call_arg: is the differing WORD a pointer
                # that merely lands in a different mock region on each side?
                kinds = set()
                for (_addr, dw, ow) in diffs:
                    dr2, or2 = region_of(dw), region_of(ow)
                    kinds.add("cross" if dr2 != or2 else "same")
                if kinds == {"cross"}:
                    kind = "CROSS_REGION(artifact-suspect)"
                elif "same" in kinds and "cross" in kinds:
                    kind = "MIXED"
                else:
                    kind = "SAME_REGION(candidate-real)"
                tally[kind] += 1
                a0, d0, o0 = diffs[0]
                detail.append({
                    "unit": unit, "symbol": sym, "kind": kind,
                    "register": f"{len(diffs)} word(s)",
                    "call_index": "",
                    "decomp_val": f"0x{d0:08X}", "orig_val": f"0x{o0:08X}",
                    "decomp_region": region_of(d0), "orig_region": region_of(o0),
                })
                continue
            if det.get("reason") != "call_arg_mismatch":
                tally[f"reclassified:{det.get('reason')}"] += 1
                continue
            dv, ov = det.get("decomp_val"), det.get("orig_val")
            dr, orr = region_of(dv), region_of(ov)
            if dr != orr:
                kind = "CROSS_REGION(artifact-suspect)"
            elif dr in ("scalar", "other"):
                kind = "SCALAR_DIFF(candidate-real)"
            else:
                kind = "SAME_REGION(candidate-real)"
            tally[kind] += 1
            detail.append({
                "unit": unit, "symbol": sym, "kind": kind,
                "register": det.get("register"),
                "call_index": det.get("call_index"),
                "decomp_val": f"0x{dv:08X}" if isinstance(dv, int) else dv,
                "orig_val": f"0x{ov:08X}" if isinstance(ov, int) else ov,
                "decomp_region": dr, "orig_region": orr,
            })

    total = sum(v for k, v in tally.items())
    print(f"\n--- {want} breakdown ({total}) ---")
    for k, v in tally.most_common():
        print(f"  {k:34s} {v:5d}  {100.0*v/max(total,1):5.1f}%")

    if args.out and detail:
        with open(args.out, "w", newline="") as f:
            w = csv.DictWriter(f, fieldnames=list(detail[0].keys()))
            w.writeheader()
            w.writerows(detail)
        print(f"detail -> {args.out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
