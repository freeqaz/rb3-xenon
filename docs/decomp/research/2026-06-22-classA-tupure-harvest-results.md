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

## Wave-4 (2026-06-23, main @a38ef1b) — +53 composed (2 winners / 8 scanned), vein THINNING HARD
Same fresh-scan workflow. 8 candidates → only 3 OWN+feasible → **2 honest winners**:

| TU | + | note |
|---|---:|---|
| PatchPanel.cpp | +50 | band3/meta_band; new 722-line port + small TexRenderer.h/InlineHelp.h adds |
| CampaignLevel.cpp | +3 | band3/meta_band; pinned an already-wired (concurrent-scaffolded) TU |
| **composed** | **+53** | 10364→10417, run1==run2 deterministic, 0 regressions |

REJECTED honestly: **BandHeadShaper.cpp +0** (OWN span but the +10 = ICF-stub-fold inflation — `bandobj/`
TUs have ZERO oracle coverage in unified_id_rb3wii.json [it indexes only `band3/` game code] so
gen_game_target_map names nothing, only positional stub-folds pair; PLUS DC3-engine-header body
divergence: RndTransformable::TransChildren std::list-not-vector, Hmx::Object::Refs ObjRef-ring-not-vector
→ real method bodies stay <100%). Validate-stage rejects: BandMachineMgr (FOREIGN, 79-fn run + phantom
IdentityInfo mis-pin + RockCentral micro-pin interleave), CharacterCreatorPanel (MIXED, multi-TU blob,
28-fn unattributable run), BandStorePanel (MIXED, 125/150 foreign song-score/NetCache), UIProxy (MIXED,
22-fn run), Game.cpp (FOREIGN, 235-fn run = a god-TU belt). ⭐ TWO STRUCTURAL WALLS now dominate the
reject pile: (i) the **meta_band panel belt is ICF-scattered class-B** (BandStorePanel/CharacterCreatorPanel/
BandMachineMgr = interleaved multi-TU, un-span-pinnable — identity-transfer territory, not span harvest);
(ii) **`bandobj/`/`system/` engine TUs have NO rb3-Wii game oracle** → even TU-pure spans yield only
stub-folds unless DC3 names them (and DC3 engine-naming is dead for strict). Yield trend across all waves:
batch1 +52 → broaden1 +49 → broaden2 +26 → broaden3 +22 → **wave-3 +126** (broadened to network/track/game/
meta_band = a fresh vein) → **wave-4 +53** (same broad scan, now 75% reject). Cumulative class-A: **+416**.

## Wave-5 (2026-06-23, main @70d60d5) — +152 composed (5 winners / 10 scanned), THINNING REVERSED
The thinning was about the SCAN POOL, not the method: broadening to fresh game+engine TUs hit a rich seam.

| TU | + | note |
|---|---:|---|
| UIPanel.cpp | +48 | system/ui; already in src, just WIRE+PIN (not yet wired) |
| FocusTracker.cpp | +42 | band3/game; decisive MILO_ASSERT line-const contiguity 0x31b9d..0x31c21 |
| TrackWidget.cpp | +26 | system/track |
| EntityUploader.cpp | +24 | band3/net_band; ~0xC base-size drift body caveat (still won) |
| StarDisplay.cpp | +12 | system/bandobj; +StarDisplay.h layout + UILabel.h |
| **composed** | **+152** | 10430→10582, run1==run2 deterministic, **0 regressions, full additive** |

REJECTED honestly: AccomplishmentOneShot +0 (retail-vs-Wii-DEV body divergence, 2 real bodies <100%),
BandSongMetadata refuted (span not TU-pure, oracle attributes 2/78). FOREIGN: BandLabel (30-fn run),
NewAwardPanel (51-fn run), and ⚠ **MetaPanel = FOREIGN @0.93, frun=124** — this SUPERSEDES the wave-3
"MetaPanel deferred OWN@0.62 retry candidate" note: deeper validation shows it is ICF-scattered, NOT a
clean span. Do NOT retry MetaPanel as a class-A span (it's identity-transfer/class-B territory).

⭐ KEY INSIGHT: the wave-4 "thinning" was the EXCLUSION LIST exhausting the meta_band panel belt, NOT the
method drying up. Each fresh broad scan that reaches into under-mined dirs (band3/game, system/track,
system/ui, band3/net_band, system/bandobj) still finds TU-pure spans. The `max_foreign_zero_run=0` +
distinctive-string/line-const gate keeps it honest (5/5 winners real-bodied, icf-clean). Yield by wave:
+52/+49/+26/+22 / **w3 +126 / w4 +53 / w5 +152**. Cumulative class-A: **+568**. Still productive → keep going.

## How to resume (for a future session that wants the marginal tail)
Edit the `DONE_OR_MIXED` exclusion in `scripts/wf_classa_harvest.js` (now includes TrackDir/
TrackerDisplay/StoreInfoPanel/NetworkEmulator/BandUserMgr/OutfitConfig/AccomplishmentPanel/NetSession;
MetaPanel left OUT = retry it) to add this wave's TUs, re-launch fresh. Better: skip re-scan/validate
and run the port-only `scripts/wf_classa_ports.js` against fresh pre-validated candidates. Land honest
winners via `scripts/harvest/land.sh` + the gotchas above + composed verify. Expect ~+10-25/wave,
dropping. The pool of string-rich unpinned contiguous game/bandobj TUs is finite and now mostly mined.
