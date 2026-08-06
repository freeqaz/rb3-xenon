#!/usr/bin/env python3
"""Calibration harness for tools/icf_alias_check.py's verdict thresholds.

Re-derives, from the CURRENT ``build/45410914/report.json``, the per-TU
features the gate's verdict rules consume (stub fraction, longest contiguous
stub run, masked_equal pairing route), evaluates them against the LABELLED
evaluation set below, prints the full threshold trade-off, and **exits 1 if
the committed thresholds stop separating the labelled classes** -- so a tree
change that invalidates the calibration fails loudly instead of silently.

THE LABELLED SET (2026-08-06) -- how it was built
-------------------------------------------------
KNOWN-GOOD = TUs whose current 100%-matched population comes from landings
independently A/B-verified at +N/-0 (strict, double full builds, report.cache
cleared), so their matches are real by measurement, not by assumption:

* laneBD ports (+69/-0 combined; docs/plans/wii-oracle-tu-location-2026-07-29.md
  §7): UIProxy (+21/-0, a8e8860c), TrainingMgr (+29/-0, c3af9ef7),
  CharSync (+19/-0, 7be91272).
* ws3 Option-C (+85 net, 5 up / 0 down, and the only landings with a recorded
  icf_alias_check --worktree HONEST pass; docs/plans/workstreams-2026-07-02/
  ws3-optionc-port-then-pin.md): MoggClip, SongSortNode, SoftParticles,
  MotionBlur, CharClipGroup.
* laneBL tu-pin-wave ledger (+599/-38 net +561 overall; the rows used here are
  the 12 pure ADDs into unclaimed space -- structurally zero-loss -- plus the
  zero-loss carves; docs/plans/tu-pin-wave-2026-07-29.md §1): LockStepMgr
  (+65/0), UGCPurchasePanel, SlotChannelMapping, ChordShapeGenerator, HitTracker,
  TourGameRules, TourGameModifier, LogFile, Asset, DrumTrackWatcherImpl,
  SndAnalysis, SongSetlistProvider, BandUserMgr (+38/0), InputMgr (+34/0),
  RealGuitarGemPlayer, BandPerformer, PracticeSectionProvider, FaceHairProvider,
  DialogDisplay, KeysFx, CrowdRating.

KNOWN-BAD = TUs the campaign record documents as ICF-alias inflation
(docs/plans/decomp-state-and-roadmap-2026-06-09.md WAVE-14/15/16 CLOSE blocks;
docs/plans/branch-audit-2026-07-29.md §4.2):

* OvershellSlot  (wave-14 head extension: "+57 of ICF-ALIAS INFLATION" caught
  by the hand honesty audit; the tool's own founding counter-example).
* MusicLibrary   (wave-15 dual-range +57, "56/57 are <=44B stub folds";
  re-measured wave-16: 113/125 range-2 fns are <=44B stubs).
* RockCentral    (whole-TU mode: 766 matched / 687 stubs inherited from the
  span.  ⚠ CONTESTED per-mode: its wave-15 +17 DELTA was composed-verified
  real, so in --worktree newly-matched mode RockCentral is known-GOOD; the
  BAD label here applies to --tu whole-TU mode only).
* MainHubPanel   (branch cA2-MainHubPanel 04ecccbf: span-pin REFUTED,
  "92.9% ICF stub-folds").
* CharacterCreatorPanel (branch cas-CharacterCreatorPanel f34c1bb7: pin
  0x825F1FCC-0x825F5A38, "0 honest matches (all 35 ICF stub-folds)").
  ⚠ For these two the refuting branches were never landed; their CURRENT
  matched populations (163 and 165 rows, runs 46 and 55) come from donor
  spans over the same regions and carry the same stub-farm shape the refuting
  audits measured.  Excluding them widens the gap ([30,76] instead of
  [30,46]) -- they BOUND the threshold, they do not rescue it.

CAVEATS, stated so nobody over-trusts the fit:
* n is small (29 good / 5 bad TUs) and whole-TU mode; the gate's blocking use
  is --worktree (newly-matched sets).  The historical newly-matched bad shapes
  (+18 all-stub, 0/35 all-stub, +39 all-stub, 56-of-57) are all caught by
  rule 1 or the >=0.95 backstop; the verified-good newly-matched sets (ws3's
  36 real / 32 stub run<38; laneBD's +69 incl. TrainingMgr's 20-funclet run)
  all pass.
* The wave-era labels were made with the (then-live) rb3-Wii oracle's foreign
  attribution; that axis is dead (doc 56 §3) and the fitted features are
  size/run/masked_equal only.  A small all-stub honest ADD (SndAnalysis) is
  structurally indistinguishable from a small all-stub fake ADD here -- the
  1/29 FP is that shape, it fires rule 1, and it is the conservative
  direction: the reviewer adjudicates ownership by other evidence.
"""

from __future__ import annotations

import json
import os
import re
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
sys.path.insert(0, HERE)
from icf_alias_check import (  # noqa: E402
    STUB_MAX, STUB_DOMINANCE, FOREIGN_RUN_FLAG,
    load_symbol_sizes, load_name_to_va, fn_va,
)

GOOD = [
    "UIProxy.cpp", "CharSync.cpp", "TrainingMgr.cpp",
    "MoggClip.cpp", "SongSortNode.cpp", "SoftParticles.cpp", "MotionBlur.cpp",
    "CharClipGroup.cpp",
    "LockStepMgr.cpp", "UGCPurchasePanel.cpp", "SlotChannelMapping.cpp",
    "ChordShapeGenerator.cpp", "HitTracker.cpp", "TourGameRules.cpp",
    "TourGameModifier.cpp", "LogFile.cpp", "Asset.cpp",
    "DrumTrackWatcherImpl.cpp", "SndAnalysis.cpp", "SongSetlistProvider.cpp",
    "BandUserMgr.cpp", "InputMgr.cpp", "RealGuitarGemPlayer.cpp",
    "BandPerformer.cpp", "PracticeSectionProvider.cpp", "FaceHairProvider.cpp",
    "DialogDisplay.cpp", "KeysFx.cpp", "CrowdRating.cpp",
]
BAD = [
    "OvershellSlot.cpp", "MusicLibrary.cpp", "RockCentral.cpp",
    "MainHubPanel.cpp", "CharacterCreatorPanel.cpp",
]
# The one known-good rule-1 false positive (see module docstring).
EXPECTED_FP = {"SndAnalysis.cpp"}


def tu_features(report, sym_sizes, name_to_va):
    """Per-unit features over the 100%-matched set (mirrors the gate)."""
    out = []
    for u in report.get("units", []):
        tu = os.path.basename((u.get("metadata") or {}).get("source_path")
                              or u.get("name") or "")
        rows = []
        for fn in u.get("functions", []):
            if float(fn.get("match_percent_normalized", 0) or 0) < 100.0:
                continue
            va = fn_va(fn, name_to_va)
            size = sym_sizes.get(va)
            if size is None:
                try:
                    size = int(fn.get("size"))
                except (TypeError, ValueError):
                    size = None
            rows.append({
                "va": va,
                "stub": size is None or size <= STUB_MAX,
                "folded": bool(fn.get("masked_equal")),
            })
        if not rows:
            continue
        ordered = sorted((r for r in rows if r["va"] is not None),
                         key=lambda r: r["va"])
        best = cur = 0
        for r in ordered:
            cur = cur + 1 if r["stub"] else 0
            best = max(best, cur)
        n = len(rows)
        n_stub = sum(1 for r in rows if r["stub"])
        out.append({
            "unit": u["name"], "tu": tu, "total": n,
            "n_real": n - n_stub, "n_stub": n_stub,
            "stub_frac": n_stub / n, "run_stub": best,
            "n_folded_stub": sum(1 for r in rows if r["stub"] and r["folded"]),
        })
    return out


def fires(u, run_flag=None, dominance=None):
    run_flag = FOREIGN_RUN_FLAG if run_flag is None else run_flag
    dominance = STUB_DOMINANCE if dominance is None else dominance
    return ((u["n_real"] == 0 and u["n_stub"] > 0)
            or u["stub_frac"] >= dominance
            or u["run_stub"] >= run_flag)


def main(argv=None):
    import argparse
    p = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    p.add_argument("--report",
                   default=os.path.join(ROOT, "build", "45410914", "report.json"))
    args = p.parse_args(argv)

    report = json.load(open(args.report))
    sym_sizes = load_symbol_sizes()
    name_to_va = load_name_to_va()
    units = tu_features(report, sym_sizes, name_to_va)
    by_tu = {}
    for u in units:
        by_tu.setdefault(u["tu"], []).append(u)

    missing = [t for t in GOOD + BAD if t not in by_tu]
    if missing:
        print(f"⚠ labelled TUs with no matched functions in this report "
              f"(labels are state-dependent): {missing}")

    rows = ([(t, "GOOD", u) for t in GOOD for u in by_tu.get(t, [])]
            + [(t, "BAD", u) for t in BAD for u in by_tu.get(t, [])])

    # --- the trade-off, printed in full -----------------------------------
    print("== run-rule sweep (run_stub >= R alone) ==")
    print(f"{'R':>4} {'FP':>3} {'FN':>3}")
    for R in (8, 12, 16, 20, 24, 29, 30, 34, 38, 42, 46, 47, 56, 76):
        fp = sum(1 for t, l, u in rows if l == "GOOD" and u["run_stub"] >= R)
        fn = sum(1 for t, l, u in rows if l == "BAD" and u["run_stub"] < R)
        print(f"{R:>4} {fp:>3} {fn:>3}")
    print("\n== dominance sweep (stub_frac >= D alone) -- the refuted feature ==")
    print(f"{'D':>6} {'FP':>3} {'FN':>3}")
    for D in (0.5, 0.6, 0.685, 0.7, 0.75, 0.8, 0.9, 0.95, 1.0):
        fp = sum(1 for t, l, u in rows if l == "GOOD" and u["stub_frac"] >= D)
        fn = sum(1 for t, l, u in rows if l == "BAD" and u["stub_frac"] < D)
        print(f"{D:>6} {fp:>3} {fn:>3}")

    good_max = max((u["run_stub"] for t, l, u in rows if l == "GOOD"), default=0)
    bad_min = min((u["run_stub"] for t, l, u in rows if l == "BAD"), default=1 << 30)
    fp = sorted({t for t, l, u in rows if l == "GOOD" and fires(u)})
    fn = sorted({t for t, l, u in rows if l == "BAD" and not fires(u)})
    n_fire = sum(1 for u in units if fires(u))
    print(f"\nlabelled run gap: good max {good_max} < bad min {bad_min}"
          f" (committed FOREIGN_RUN_FLAG={FOREIGN_RUN_FLAG})")
    print(f"operating point (rule1 | frac>={STUB_DOMINANCE} | run>="
          f"{FOREIGN_RUN_FLAG}): FP {fp} ({len(fp)}/{len(GOOD)}), FN {fn} "
          f"({len(fn)}/{len(BAD)}); tree-wide firing {n_fire}/{len(units)} "
          f"= {100 * n_fire / len(units):.1f}%")

    ok = (not fn
          and set(fp) <= EXPECTED_FP
          and good_max < FOREIGN_RUN_FLAG <= bad_min)
    if not ok:
        print("\nCALIBRATION BROKEN: the committed thresholds no longer "
              "separate the labelled classes on this report. Re-run the "
              "sweep above, re-fit, and update icf_alias_check.py -- do NOT "
              "keep gating on stale thresholds.")
        return 1
    print("\ncalibration HOLDS on this report.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
