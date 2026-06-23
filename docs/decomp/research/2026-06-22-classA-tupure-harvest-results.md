# Class-A TU-pure span harvest — results + the proven method (2026-06-22/23)

The one repeatable STRICT-match lever found at the structural-lever floor. Records the
winners, the method, and the (thinning) yield trend so future sessions can resume it.

## The method (proven, tooled = workflow `classa-tupure-harvest` / `classa-broaden-harvest`)
An UNPINNED game/engine TU whose methods form a CONTIGUOUS TU-PURE span in retail (preserved
by /O1 spatial grouping, no LTCG) is span-pinnable even though the rb3-Wii BinDiff oracle
MIS-LOCATES it (near-random, names ~0-2 of N). The win does NOT come from oracle naming — it
comes from the span being TU-PURE: once the source compiles, the TU's OWN STL/destructor/
funclet bodies **byte-reproduce even while anonymous** (dtk splits them into the unit obj by
VA; objdiff pairs them within-unit by bytes). Named methods are a minority of the +N.

### The gate (decisive — this is what makes it honest)
Two-stage: (1) VALIDATE ownership = span TU-PURITY via **distinctive-string content +
intra-cluster call-topology** (NOT the near-random oracle), require `max_foreign_zero_run < 8`
(the GemPlayer kill-test: a ≥8 contiguous foreign run = MIXED, reject). (2) PORT honesty =
whole-binary composed A/B + `icf_alias_check.py` (real-bodied >44B, not ≤44B stub-folds).
⚠ The validate stage OVER-CLAIMS OWN as the pool thins (string-content over-attributes):
ClosetMgr (0.9) and MainHubPanel (0.9) both validated OWN but the PORT stage's oracle-cross-
check refuted them as mixed/foreign (+0, no false land). The port-stage gate is the real guard.

## Winners (all composed-verified, ICF-clean, landed)
| TU | + | span |
|---|---:|---|
| GemManager.cpp | +35 | 0x82B67448–0x82B6D688 |
| AppLabel.cpp | +52 | 0x825ACF88–0x825AF938 |
| Defines.cpp | +7 | 0x82670CB0–0x82671230 |
| BandCrowdMeter.cpp | +8 | 0x822AE238–0x822AF160 |
| GameMicManager.cpp | +18 | 0x82663A1C–0x82664890 |
| PatchDir.cpp | +16 | 0x822677C8–0x82269450 |
| BandScoreboard.cpp | +15 | (+ BandStarDisplay.h) |
| VocalTrainerPanel.cpp | +11 | Handle + UpdateScore |
| TrackPanel.cpp | +22 | 0x82B5E0B8–0x82B61E30 |
| **total** | **+184** | 9 TUs |

(Plus the separate RndEnviron +9 DC3-drift base-class keystone — a different lever class.)

## Rejected (the gate working — no false lands)
GemPlayer (96-fn foreign run), Matchmaker (31-fn foreign), PitchArrow (15), OvershellSlot,
TrackPanelDirBase, GemTrackDir (gap-mixed w/ ChordShapeGenerator+Symbols3), ChordShapeGenerator,
OverdriveMeter, TrainerProgressMeter, ClosetMgr (port-refuted), MainHubPanel (port-refuted,
the known ICF-scattered class).

## Yield trend (THINNING — the vein is drying)
- batch-1 (5 cand): AppLabel +52 (1 winner)
- broaden-1 (10 cand): +49 (4 winners: Defines/BandCrowdMeter/GameMicManager/PatchDir)
- broaden-2 (10 cand): +26 (2 winners + 1 false-OWN: BandScoreboard/VocalTrainerPanel; ClosetMgr refuted)
- broaden-3 (4 cand): +22 (1 winner + 1 false-OWN: TrackPanel; MainHubPanel refuted)
Trend +49→+26→+22, scan pool shrinking (10→4 fresh candidates), false-OWN rate rising. Cost
~1.7-1.9M agent tokens/wave (~40-65k/match). DIMINISHING — consolidated here.

## How to resume (for a future session that wants the marginal tail)
Edit the `DONE_OR_MIXED` exclusion in the `classa-broaden-harvest` workflow script to add the
TUs above, re-launch fresh (no resumeFromRunId). Land honest winners via `scripts/harvest/land.sh`
+ composed verify. Expect ~+10-25/wave, dropping. Each winner is a real TU-pure span; the pool
of string-rich unpinned contiguous game/bandobj TUs is finite and now mostly mined.
