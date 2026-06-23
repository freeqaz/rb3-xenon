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

## Wave-3 (2026-06-23, main @25ed686) — +126 composed (4 winners / 6 ported)
Refreshed scan (`scripts/wf_classa_harvest.js`) → 10 candidates → 6 OWN+feasible → ported via a
port-ONLY workflow (`scripts/wf_classa_ports.js`, batched 2-at-a-time; the scan+validate results
were cached/extracted so a persistent server-side rate-limit storm didn't force re-running them).

| TU | + | span | note |
|---|---:|---|---|
| TrackDir.cpp | +54 | 0x827B89B0–0x827BB078 | system/track; ~60 PROPSYNC/HANDLE dispatch funclets reproduced |
| TrackerDisplay.cpp | +50 | 0x826B3268–0x826B4B30 | band3/game; SetPercentageProgress core + own funclets |
| StoreInfoPanel.cpp | +21 | 0x8261C660–0x8261E020 | band3/meta_band; +2 lines BandStoreOffer.h (fwd-decl) |
| NetworkEmulator.cpp | +8 | 0x823D8F00–0x823D97F8 | network/net; Handle + 6 setter bodies |
| **composed** | **+126** | 10238→10364 | run1==run2 deterministic, 0 regressions |

REJECTED honestly (the gate working): **BandUserMgr.cpp +0** = ICF-ALIAS INFLATION (the raw +6 is
ALL ≤44B stub-folds, 0 real-bodied; the 4 real methods stay 0% and own STL/funclet bodies do NOT
byte-reproduce here unlike GemManager/AppLabel → needs full per-method BODY-PORTS, not span-pin).
Validate-stage MIXED rejects: OutfitConfig (unportable retail-vs-Wii-DEV 1192B SetSkinTextures core),
AccomplishmentPanel (Quazal-interleaved, max_foreign_run 58), NetSession (multi-TU ICF blob, run 89).

DEFERRED (env, NOT a port outcome): **MetaPanel.cpp** — /tmp tmpfs quota saturated by ~9 concurrent
worktrees (no btrfs reflink on tmpfs → full 660MB copies) → couldn't create a buildable worktree.
RETRY when disk frees: span [0x825595F8,0x8255DE88), pre-validated OWN @0.62, ~215 fns, GOD-OBJECT
~100-header closure is the real guard (swap the lone missing meta/MemcardMgr_Wii.h →
src/system/meta/MemcardMgr.h Xbox equiv). Lowest priority (heavy/risky).

⚠ LANDING GOTCHAS (this wave): (1) `land.sh` prints a FALSE `READY` if the lane worktree is DIRTY
(`git rebase` refuses on unstaged changes — e.g. the download_tool.py skip-guard + global_fuzzy_pairs.json
the port agents leave — with a non-"conflict" message the script's loop doesn't catch) → clean the
worktree (`git -C <wt> checkout -- tools/download_tool.py; rm global_fuzzy_pairs.json`) before re-landing.
(2) The objects.json union (`resolve_json_union.py` / rebase auto-resolve) can REPLACE-not-merge on the
LAST sequential ff-merge, silently dropping earlier lanes' entries (splits.txt keeps all pins →
`configure.py` "Missing configuration for X.cpp" is the tell) → after landing all lanes, grep each TU's
FULL-PATH objects.json entry + re-add any dropped one to its correct cflags group, then `configure.py`.
(3) `build.ninja` is gitignored → after landing newly-ADDED TUs you MUST run `configure.py` before the
composed verify, else the new objs never compile and the delta reads ~+0.

## How to resume (for a future session that wants the marginal tail)
Edit the `DONE_OR_MIXED` exclusion in `scripts/wf_classa_harvest.js` (now includes TrackDir/
TrackerDisplay/StoreInfoPanel/NetworkEmulator/BandUserMgr/OutfitConfig/AccomplishmentPanel/NetSession;
MetaPanel left OUT = retry it) to add this wave's TUs, re-launch fresh. Better: skip re-scan/validate
and run the port-only `scripts/wf_classa_ports.js` against fresh pre-validated candidates. Land honest
winners via `scripts/harvest/land.sh` + the gotchas above + composed verify. Expect ~+10-25/wave,
dropping. The pool of string-rich unpinned contiguous game/bandobj TUs is finite and now mostly mined.
