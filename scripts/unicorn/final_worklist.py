#!/usr/bin/env python3
"""Build the SCREENED matched-but-wrong worklist.

A DIVERGENT verdict on a row the metric scores 100% is the headline
"matched but wrong" case. Taken raw, that set is ~650 rows. It should not be
briefed as ~650 bugs, because two mechanisms inflate it and both were
measured, not guessed:

1. cap_exhausted* -- both sides hit the emulator's instruction cap. That is
   an INDETERMINATE result, not a divergence. (Excluded upstream, in
   analyze_refresh.py.)

2. The MOCK-REGION artifact. The emulator maps each relocation target into a
   region chosen by the target's SECTION. When the same conceptual symbol is
   `.rdata` in our object and `.data` in the dtk-split target object, the
   mocked POINTER VALUES differ, and the comparator reports call_arg or
   object_memory. Measured: 367 of 375 call_arg rows and 491 of 493
   object_memory rows differ ONLY by landing in a different mock region --
   overwhelmingly the single pair (rdata -> globals).

So this script re-runs each candidate and tiers it:

  TIER1_STRONG    -- the region artifact CANNOT explain it:
                     call_count (a differing NUMBER of calls), decomp_error
                     (our side faults where retail does not), and
                     return_value differing as a scalar.
  TIER2_CANDIDATE -- call_arg / object_memory differing WITHIN a region.
  TIER3_SUSPECT   -- cross-region: artifact-suspect, do not fund as bugs.

⚠ TIER1 is "strong", not "proven". call_count in particular is also produced
by an INLINE-POLICY difference -- if retail inlined a helper and we call it
out of line, behaviour is identical and the call count still differs. That is
a known pattern in this project. Adjudicate on retail bytes before fixing.
"""

import argparse
import csv
import json
import os
import sys
from collections import Counter

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
sys.path.insert(0, PROJECT_ROOT)
sys.path.insert(0, os.path.join(PROJECT_ROOT, "scripts", "unicorn"))

from scripts.unicorn_runner.coff import COFFParser
from scripts.unicorn_runner.run import resolve_unit, _run_comparison_core
from scripts.unicorn_runner.extractor import (extract_from_decomp,
                                              extract_from_original)
from characterize_callarg import region_of
from analyze_refresh import load_report_100, ARTIFACT_CLASSES


def tier_of(det, div_class):
    reason = det.get("reason", "")
    if reason == "call_count_mismatch":
        return "TIER1_STRONG", "differing NUMBER of calls"
    if reason == "decomp_error":
        return "TIER1_STRONG", f"our side errors: {det.get('decomp_error','')}"[:70]
    if reason == "error_mismatch":
        return "TIER1_STRONG", "one side errors, the other does not"
    if reason == "return_value_mismatch" or div_class == "return_value":
        dv, ov = det.get("decomp_val"), det.get("orig_val")
        if isinstance(dv, int) and isinstance(ov, int):
            if region_of(dv) != region_of(ov):
                return "TIER3_SUSPECT", f"return crosses mock regions " \
                                        f"({region_of(dv)}->{region_of(ov)})"
            return "TIER1_STRONG", f"return value {hex(dv)} vs {hex(ov)}"
        return "TIER2_CANDIDATE", "return value differs"
    if reason == "call_arg_mismatch":
        dv, ov = det.get("decomp_val"), det.get("orig_val")
        if isinstance(dv, int) and isinstance(ov, int):
            dr, orr = region_of(dv), region_of(ov)
            if dr != orr:
                return "TIER3_SUSPECT", f"arg {det.get('register')} crosses " \
                                        f"mock regions ({dr}->{orr})"
            return "TIER2_CANDIDATE", f"arg {det.get('register')} differs " \
                                      f"within {dr}: {hex(dv)} vs {hex(ov)}"
        return "TIER2_CANDIDATE", "call arg differs"
    if reason == "memory_mismatch":
        diffs = det.get("object_diffs") or []
        if diffs and all(region_of(d) != region_of(o) for (_a, d, o) in diffs):
            return "TIER3_SUSPECT", f"{len(diffs)} object word(s), all cross-region"
        return "TIER2_CANDIDATE", f"{len(diffs)} object word(s) differ within region"
    return "TIER2_CANDIDATE", reason or "?"


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--csv", required=True)
    ap.add_argument("--report", required=True)
    ap.add_argument("--out", required=True)
    args = ap.parse_args()

    by_sym, info = load_report_100(args.report)

    cands = []
    for r in csv.DictReader(open(args.csv)):
        if r["verdict"] != "DIVERGENT":
            continue
        if (r["div_class"] or "") in ARTIFACT_CLASSES:
            continue
        ent = by_sym.get(r["symbol"])
        if not ent:
            continue
        fz, mpn, size, unit = ent
        if fz < 100.0 and mpn < 100.0:
            continue
        cands.append((r, fz, mpn, size, unit))
    print(f"candidates (DIVERGENT, 100%, non-artifact class): {len(cands)}")

    by_unit = {}
    for c in cands:
        by_unit.setdefault(c[0]["unit"], []).append(c)

    out_rows = []
    tally = Counter()
    for unit, cs in by_unit.items():
        try:
            d, o = resolve_unit(unit, PROJECT_ROOT)
            dc, oc = COFFParser(d), COFFParser(o)
        except Exception:
            tally["resolve_error"] += len(cs)
            continue
        for (r, fz, mpn, size, runit) in cs:
            try:
                code, b, _v, _e = _run_comparison_core(r["symbol"], dc, oc)
            except Exception:
                tally["rerun_error"] += 1
                continue
            if b is None or not b.result.details:
                tally["no_detail"] += 1
                continue
            tier, why = tier_of(b.result.details, r["div_class"])

            # Hard screen. If our function body and retail's are BYTE
            # IDENTICAL, emulation is deterministic over identical code, so
            # the divergence cannot come from our instructions -- it is
            # entirely attributable to how the RELOCATION TARGETS were
            # mocked. Combined with the measured (rdata -> globals) region
            # pattern that is the artifact, not a bug in our source.
            # (Caveat kept honest: a genuinely WRONG CALLEE also lives here,
            # since a `bl` encodes identically and only the reloc target
            # differs -- such a case shows up as call_count or as differing
            # trampoline targets, not as a data-pointer region flip.)
            try:
                db, _dr = extract_from_decomp(dc, r["symbol"])
                ob, _or = extract_from_original(oc, r["symbol"])
                identical = (db is not None and ob is not None
                             and bytes(db) == bytes(ob))
            except Exception:
                identical = False
            if identical:
                tier = "TIER3_SUSPECT"
                why = "BYTE-IDENTICAL body; divergence is reloc-target " \
                      "mocking only -- " + why
            tally[tier] += 1
            out_rows.append({
                "tier": tier, "symbol": r["symbol"], "unit": runit,
                "size": size, "fuzzy": f"{fz:.2f}", "mpn": f"{mpn:.2f}",
                "div_class": r["div_class"], "confidence": r["confidence"],
                "bytes_identical": int(identical), "why": why,
            })

    order = {"TIER1_STRONG": 0, "TIER2_CANDIDATE": 1, "TIER3_SUSPECT": 2}
    out_rows.sort(key=lambda w: (order.get(w["tier"], 9),
                                 0 if w["confidence"] == "stable_divergent" else 1,
                                 -int(w["size"])))

    print("\n--- TIERS ---")
    for k, v in tally.most_common():
        print(f"  {k:18s} {v}")

    with open(args.out, "w") as f:
        f.write("# Unicorn matched-but-wrong worklist (SCREENED)\n\n")
        f.write("Rows whose emulated behaviour differs from retail while the "
                "match metric scores them **100%**.\n\n")
        f.write(f"- report.json objdiff-cli **{info['tool_version']}** "
                f"commit `{info['tool_commit']}` binary "
                f"`{info['tool_binary_hash']}` "
                f"(NOT the binary deployed after the 2026-09-01 08:59 "
                f"rebuild -- a row's 100% status is a tool-dependent claim)\n")
        f.write(f"- ruler: `{info['diff_config'][0] if info['diff_config'] else '?'}`\n")
        f.write("- `cap_exhausted*` excluded as INDETERMINATE (emulator "
                "instruction cap, not a divergence)\n\n")
        f.write("## Tiers\n\n")
        f.write("| tier | meaning | rows |\n|---|---|---:|\n")
        f.write(f"| TIER1_STRONG | the mock-region artifact cannot explain it "
                f"(call_count / decomp_error / scalar return) | "
                f"{tally.get('TIER1_STRONG',0)} |\n")
        f.write(f"| TIER2_CANDIDATE | arg/memory differs WITHIN a mock region | "
                f"{tally.get('TIER2_CANDIDATE',0)} |\n")
        f.write(f"| TIER3_SUSPECT | cross-region pointer -- artifact-suspect, "
                f"do NOT fund as bugs | {tally.get('TIER3_SUSPECT',0)} |\n\n")
        f.write("> ⚠ TIER1 is *strong*, not *proven*. `call_count` is also "
                "produced by an INLINE-POLICY difference (retail inlined a "
                "helper, we call it out of line) -- identical behaviour, "
                "different call count. Adjudicate on retail bytes before "
                "fixing.\n\n")
        for tier in ("TIER1_STRONG", "TIER2_CANDIDATE", "TIER3_SUSPECT"):
            sel = [w for w in out_rows if w["tier"] == tier]
            if not sel:
                continue
            f.write(f"\n## {tier} ({len(sel)})\n\n")
            f.write("| # | symbol | unit | size | fuzzy | mpn | class | why |\n")
            f.write("|---|--------|------|-----:|------:|----:|-------|-----|\n")
            cap = len(sel) if tier != "TIER3_SUSPECT" else 40
            for i, w in enumerate(sel[:cap], 1):
                f.write(f"| {i} | `{w['symbol']}` | {w['unit']} | {w['size']} "
                        f"| {w['fuzzy']} | {w['mpn']} | {w['div_class']} "
                        f"| {w['why']} |\n")
            if cap < len(sel):
                f.write(f"\n_({len(sel) - cap} further rows omitted; full set "
                        f"in the CSV.)_\n")
    print(f"\nworklist -> {args.out}")

    csv_out = os.path.splitext(args.out)[0] + ".csv"
    with open(csv_out, "w", newline="") as f:
        w = csv.DictWriter(f, fieldnames=list(out_rows[0].keys()))
        w.writeheader()
        w.writerows(out_rows)
    print(f"csv      -> {csv_out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
