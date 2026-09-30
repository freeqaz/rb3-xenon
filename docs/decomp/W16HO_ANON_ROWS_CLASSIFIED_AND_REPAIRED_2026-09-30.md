# W16-HO — why the remaining anonymous game rows don't pair, and the biggest class repaired (2026-09-30)

Branch `w16-ho` off main `85f06cbc4` (after W16-HD/HF/HG/HK). Scope: `src/band3`, `src/network`
minus Quazal. One header under `src/network/net/` was touched; nothing under `src/system/`.

**Result.** 129 anonymous rows (51,804 B) now carry names; 83 of them (30,912 B) read fuzzy 100.
One whole-binary A/B over the branch diff: **+152 fns / +89 honest / +33,744 B / fuzzy
+0.498887 pp**. That is exactly what the final in-tree build predicted. 54 units improved, 1
regressed (RockCentral, EH-funclet disclosure only, §4), and 2 units reached 100%.

## 1. Population

The census uses anonymous `fn_` rows at fuzzy 0 in units with a compiled base obj whose source is
under `src/band3/` or `src/network/` (not `quazal/`). It was taken on the settled tree at
`85f06cbc4`.

**1,466 rows / 242,752 B in 159 units** (the brief's "about 1,500 / 237 KB").
- 375 rows ≥ 200 B carry 158,668 B (65%). These were classified row by row.
- 1,091 rows < 200 B carry 84,084 B. These got a cheaper, coarse classification.

## 2. Classification (rows ≥ 200 B)

Every row got several independent signals:

- **Candidate matrix.** Scratch-rename scoring of every unclaimed function in the unit's base obj
  within a 0.33–3× size band, on `report.json`'s ruler (`tools/anon_candidate_scorer.py`
  machinery). 5,705 jobs.
- **Template re-score.** The inherited matrix pool (W16-HG/HK) drops every candidate whose mangled
  name contains `?$`. That silently removes **game methods that merely take a template parameter**
  (`BandSongMgr::GetValidSongs(const vector<int>&)`, `SetlistSort::BuildSetlistTree(map&)`,
  `StickerProvider::SetStickers(vector*)`) and the STL COMDATs a unit compiles. The rows with no
  other candidate were re-scored against that set.
- **Semantic overlap, independent of the byte score.** This is the overlap between the retail row's
  named callees and strings and each candidate's relocation targets, string literals and the global
  `Symbol`/`Message` objects it references. It catches the case the byte score cannot: a true
  identity whose retail body grew by dozens of function-local static-init blocks.
  `CustomizePanel::PreviewAsset` scores **29.7** by bytes but shares **31 of 35** features. Retail
  builds 31 `static Symbol`s (`bl ??0Symbol` ×31 under one guard word) where our port references
  Wii-era globals.
- **Neighbour-unit semantic overlap.** The same test against the pools of units whose pins lie
  within 32 KB, to find rows pinned into the wrong unit.
- **Pin locality.** Pin blocks with no named row, far (> 64 KB) from any named row of the unit.
- **Quazal `/Od` band** `0x82A6D168–0x82B54190` (DuplicatedObject, Scheduler, …), which has no
  oracle.
- **Mis-carve test.** The predecessor falls through (not `blr`/`b`/`bctr`, and not EH-prefix bytes
  or padding decoded as code) **and** the row has no prologue.

| class | rows | bytes | share |
|---|---:|---:|---:|
| **D — identity held in own unit, body drifted** (D1 matrix ≥ 70: 30 / 10,532 B · D2 45–70: 65 / 29,952 B · D3 < 45, found by semantic overlap: 25 / 10,480 B) | **120** | **50,964** | **32.1%** |
| A — no identity found in own unit or neighbours (A0 no strings: 74 / 26,620 · A3 strings present in our tree: 42 / 21,148 · A1 strings only in the rb3-Wii oracle: 18 / 6,884 · A2 strings in neither, TU5/Xbox-only: 8 / 2,824) | 142 | 57,476 | 36.2% |
| Q — Quazal `/Od` band, no oracle | 55 | 27,040 | 17.0% |
| R — wrong unit pin: identity is a neighbouring unit's function (strong: ≥ 5 shared features, or ≥ 4 at ≥ 75%) | 25 | 12,404 | 7.8% |
| S — template-signature method or STL instance we compile | 20 | 6,620 | 4.2% |
| P — foreign pin block, identity unresolved | 12 | 3,840 | 2.4% |
| M — mis-carve | 1 | 324 | 0.2% |
| **all ≥ 200 B** | **375** | **158,668** | |

A is the largest *bucket*, but it is a residual ("no identity found"), not one cause. D is the
largest class with a single cause and a single remedy, so D was worked.

**Rows < 200 B (coarse: band + mis-carve + own-unit semantic overlap, no byte matrix):**

| class | rows | bytes |
|---|---:|---:|
| no own-unit identity by overlap | 497 | 43,148 |
| featureless (no string, no named callee: leaf/accessor/thunk) | 480 | 28,788 |
| Quazal `/Od` band | 62 | 6,572 |
| identity likely held (overlap) | 32 | 4,908 |
| mis-carve candidate | 20 | 668 |

Whole population, mis-carves: **21 rows / 992 B**. The first version of the mis-carve detector
flagged 67 rows, almost all false. It read the 8-byte EH prefix (`lwz r16,…`) and `.4byte 0`
padding as fall-through code, while the "tail" rows began with `mflr r12`. It was fixed before
the count was used. A second defect gave a vacuous "not found" for 479 rows: path-qualified units
keep their `.s` in subdirectories, and the glob was not recursive.

## 3. What was done (class D, then two small side classes)

1. **Identity by retail-byte adjudication** (`tools/anon_proposal_adjudicate.py`) for all 120 D
   rows. Runner-up = next matrix or overlap candidate. The tool's mechanical `CONTRADICTED` fires
   on any `RETAIL_ONLY`/`OURS_ONLY` relocation, which for a drifted body is the drift itself. The
   accept rule used HK's decisive-negative reading: ≥ 3 AGREE, no `CALLER_CONTRA`, no
   `NAME_BOUND_ELSEWHERE`, and more AGREE than the runner-up.
   - That rule accepted **104**.
   - Four more were settled by matched-caller evidence: `UpdateSongStats`, `CleanUpTracks`,
     `SetSuccessState@TrackerPlayerDisplay`, `SetType@TrackerBroadcastDisplay`.
   - A second round named three twins by their caller-supported sibling instead: `0x82616230` =
     `MovePatch`, and `0x826D45A0`/`0x826D4290` = `TrackerBandDisplay::SetSuccessState`/`SetType`.
   - Held: 9 D rows, listed in §6.
2. **Body repair**, biggest size × (100 − fuzzy) first. The work was split across four forks on
   disjoint source files, each in its own worktree. Rules: full `ninja-locked` builds only; every
   row of the touched unit compared after each edit; an edit kept only if nothing dropped; about
   three ideas per row.

| pattern | examples |
|---|---|
| Wii global `Symbol`/`Message` → function-local `static` (one guard per function, sometimes unused, per switch case, or inside the loop) | almost every row: `PreviewAsset` 29.7 → 99.6, `RGStringToken` 23.9 → 100, `CheckCoda` 18.9 → 100, `GetPresenceMode`, `SetVenue`, `TokenRedemptionPanel::OnMsg` |
| per-TU `/D` gates already in the tree | `/DRB3_SYNCPROP_LOCAL_STATIC` for BandUser.cpp; `/DRB3_HANDLE_LOCAL_STATIC` + `/DRB3_STRIP_CHEAT_HANDLERS` for TourDescPanel.cpp (`config/45410914/objects.json`) |
| dev-only code → `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` | `CustomizePanel::Load` prefab branch, `ConnectedControllerType` `fake_controllers`, `SetEyebrows`/`SetFaceHair` null guard |
| TU5 code absent from the oracle, rebuilt from retail bytes | `Load`'s `PremiumAssetProvider` + ClosetMgr/AssetStore offer refresh; `SourceSym` `Locale::Localize`; `SetVenue` audition gate; TokenRedemption flows; `RockCentral` version gate removed; `UtlInit` registration order |
| user-declared empty virtual dtor → implicit (`patterns/fixable-declarations.md`) | `SearchSettings` (`src/network/net/MatchmakingSettings.h`): `StartSearch` 93.8 → 100 |
| **real behavioural bugs** | `BandSongMgr::IsSongUnplayable`'s pro-guitar branch was inverted. `BandMachine::HasProGuitarOrBass` was an inline `return false` stub; retail `fn_825C17D8` is the oracle's set lookup, which lifts `IsSongAllowedToHavePart` 85.7 → 100 |

3. **New functions for TU5 retail rows with no oracle (the names are ours)**, each adjudicated
   SUPPORTED before mapping:
   - `BandMatchmaker::IsRanked` (`0x82651b28`)
   - `MusicLibrary::RefreshSongLists` (`0x82540208`)
   - `TrainerPanel::{Begin,End,Challenge,StartEarly,StartNorm}Token` (`0x826C9818`–`0x826C99D8`).
     Each row was assigned by its unique retail string: every getter scores 100 against every
     row, because scratch scoring forgives string relocations.
   - `Performer::GetSongFraction` (`0x8269D068`)
   - `TrackPanel::ResetEndingBonus` (`0x82B907F8`)
   - `BandMachine::HasProGuitarOrBass` (`0x825C17D8`; the oracle has this one)

   Not mapped: `ClosetMgr`/`AssetStore` offer-refresh helpers (`0x82566978`, inside a `Str.cpp`
   pin, as W16-HK found) and `Band::PlayerDoneWithCoda`/`PlayerDoneOrBlewCoda` (`0x8269A5A8`/
   `0x8269A610`, inside UITransitionNetMsgs' pin). A name cannot pair until those pins move.
4. **Class S (side class): 8 of 20 named.** Accepted only rows with no caller or bound-elsewhere
   contradiction and with either caller agreement or a margin ≥ 55. Refused: `??4vector<StreakInfo>`
   (16 caller contradictions) and the fold soup where callees name another instantiation.

Gates after the final build: `tools/map_name_injectivity.py` **OK** (29,993 applied rows,
injective); `tools/icf_alias_finder.py --validate` **PASS** (1,478 map-consistent, 254 tolerated,
**0 contradicted**, 1,733 total). `symbols.txt` did not change.

## 4. Measurement

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ho-ab --patch <85f06cbc4..w16-ho>`, **one
run**, map + source + configgen. Both legs were force-re-split and read at a split fixed point on
the `name_check` ruler. Result:
`~/tmp/wt-w16-ho-ab/.ab_measure_runs/20260930-162413-branch-3881740/result.json`.

| | leg A (base) | leg B (branch) | Δ |
|---|---:|---:|---:|
| matched_functions | 44,847 | 44,999 | **+152** |
| masked_equal | 23,399 | 23,462 | +63 |
| honest (matched − masked_equal) | 21,448 | 21,537 | **+89** |
| matched_code_percent | 42.803635 | 43.132940 | +0.329305 pp (**+33,744 B**) |
| fuzzy_match_percent | 51.776733 | 52.275620 | **+0.498887 pp** |
| units at 100 (mpn) | 215 | 217 | +2 (AccomplishmentSongFilterConditional, ManageBandPanel) |

- **Predicted vs measured.** Predicted from the lane-start build (44,847 / 4,386,116 B / 51.776733)
  and the final branch build (44,999 / 4,419,860 B / 52.275620): +152 / +33,744 B / +0.498887 pp.
  The measurement matched exactly.
- **Progression**, read from in-tree builds (these are not A/Bs):
  - naming the 108 alone: +3 fns / +808 B;
  - + twins and class S: +5 fns / +1,472 B;
  - fork A: 19 of 27 rows at 100; fork B: 20 of 26; fork C: 20 of 32 at ≥ 99.9; fork D: 15 of 23.
- **The one regressing unit, RockCentral −5, is disclosure noise.** Its only real row
  (`RockCentral::OnMsg(RockCentralOpCompleteMsg)`) went 0 → 100. Every other change is a 40 B EH
  funclet paired by byte signature (`masked_equal`) re-pairing within its twin pool, e.g.
  `fn_825093E8` 99.5 → 40.8 and `fn_824F9FA8` 93.5 → 99.3. Those rows carry no honest credit.
  Likewise two funclets elsewhere, GameMicManager `fn_82681730` and GemManager `fn_82B79BA4`,
  moved to 99.5.
- **The `none`-ruler control** reads +35,580 B. The tool marks it NOT_APPLICABLE because the patch
  contains source.

## 5. Rows changed

On `85f06cbc4` **every row below is an anonymous `fn_` at fuzzy 0, unpaired**.
- "at naming" is the scratch-rename or report score under the name, before any body repair.
- "after" is leg B's `report.json` (the branch build).

| after | rows | bytes |
|---|---:|---:|
| 100 | 83 | 30,912 |
| 99–100 | 17 | 9,080 |
| 95–99 | 14 | 6,104 |
| 90–95 | 4 | 1,536 |
| < 90 | 11 | 4,172 |
| **all** | **129** | **51,804** |

| retail row | size | name | unit | how identified | at naming | after |
|---|---:|---|---|---|---:|---:|
| `0x82615440` | 1956 | `?PreviewAsset@CustomizePanel@@QAAXVSymbol@@@Z` | CustomizePanel | drift class | 29.68 | 99.59 |
| `0x8264b328` | 1640 | `?FillExpandedDetails@NextSongPanel@@QAAXH@Z` | NextSongPanel | drift class | 61.26 | 99.76 |
| `0x82640288` | 1640 | `?OnMsg@TokenRedemptionPanel@@QAA?AVDataNode@@ABVRockCentralOpCompleteMsg@@@Z` | TokenRedemptionPanel | drift class | 61.34 | **100** |
| `0x82576450` | 1484 | `?IsSongUnplayable@BandSongMgr@@QBA_NHAAVBandUserMgr@@_N@Z` | BandSongMgr | drift class | 67.02 | **100** |
| `0x8267ff00` | 996 | `?GetPlayModeContextFromUser@PresenceMgr@@QAAHPBVLocalBandUser@@_N@Z` | PresenceMgr | drift class | 51.31 | **100** |
| `0x826cb660` | 964 | `?InternalInitSections@TrainerPanel@@QAAXPBVDataEventList@@@Z` | TrainerPanel | drift class | 64.03 | 95.54 |
| `0x8267fa48` | 960 | `?GetPresenceMode@PresenceMgr@@QAA?AVSymbol@@XZ` | PresenceMgr | drift class | 69.85 | **100** |
| `0x82699260` | 904 | `??0Stats@@QAA@XZ` | Stats | drift class | 95.08 | **100** |
| `0x82617e78` | 844 | `?SelectAsset@CustomizePanel@@QAAXVSymbol@@@Z` | CustomizePanel | drift class | 37.50 | 98.67 |
| `0x825c0268` | 836 | `?BuildSetlistTree@SetlistSort@@QAAXAAV?$map@VSymbol@@VSetlistRecord@@U?$less@VSymbol@@@stlpmtx_std@@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@VSetlistRecord@@@stlpmtx_std@@@4@@stlpmtx_std@@@Z` | SongSort | drift class | 76.02 | 99.35 |
| `0x826f7260` | 804 | `?Poll_@Singer@@QAAXMABVSongPos@@MMMM@Z` | Singer | drift class | 92.42 | 97.22 |
| `0x8260f690` | 748 | `?SetCharCreatorState@CharacterCreatorPanel@@QAAXW4CharCreatorState@1@@Z` | CharacterCreatorPanel | drift class | 57.76 | **100** |
| `0x826b4b88` | 740 | `?RGStringToken@ChordbookPanel@@QAA?AVSymbol@@H_N@Z` | PracticePanel | drift class | 23.86 | **100** |
| `0x825b4aa8` | 716 | `??0OvershellPanel@@QAA@PAVSessionMgr@@PAVBandUserMgr@@@Z` | OvershellPanel | drift class | 68.66 | **100** |
| `0x8263feb0` | 676 | `?EnumerateOffers@TokenRedemptionPanel@@QAAXPAVLocalBandUser@@@Z` | TokenRedemptionPanel | drift class | 54.17 | **100** |
| `0x82577ab8` | 668 | `?SyncSharedSongs@BandSongMgr@@QAAXXZ` | BandSongMgr | drift class | 68.59 | 99.89 |
| `0x82bae840` | 664 | `?Init@GemRepTemplate@@QAAXPAVObjectDir@@@Z` | GemRepTemplate | drift class | 53.75 | **100** |
| `0x825d6320` | 620 | `??0ViewSettingsProvider@@QAA@XZ` | ViewSetting | drift class | 68.19 | 83.26 |
| `0x82653d80` | 556 | `??0BandMatchmaker@@QAA@XZ` | Matchmaker | drift class | 65.29 | **100** |
| `0x825dfdb8` | 536 | `?OnMsg@OvershellSlot@@QAA?AVDataNode@@ABVRockCentralOpCompleteMsg@@@Z` | OvershellSlot | drift class | 68.49 | **100** |
| `0x82689048` | 512 | `?GetFxSwitchPosition@GameConfig@@QAAHPAVLocalBandUser@@@Z` | GameConfig | drift class | 57.85 | 97.88 |
| `0x82577610` | 508 | `?GetValidSongs@BandSongMgr@@QBAHABV?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@AAVBandUserMgr@@AAV23@MM_N3@Z` | BandSongMgr | template/STL | 85.16 | 85.16 |
| `0x826104b8` | 496 | `?RandomizeFace@CharacterCreatorPanel@@QAAXXZ` | CharacterCreatorPanel | drift class | 44.98 | 98.34 |
| `0x825b6188` | 492 | `??1OvershellPanel@@UAA@XZ` | OvershellPanel | drift class | 61.05 | **100** |
| `0x826148c8` | 484 | `?Load@CustomizePanel@@UAAXXZ` | CustomizePanel | drift class | 55.46 | **100** |
| `0x826bd2c0` | 480 | `?UpdateGameCymbalLanes@GemPlayer@@QAAXXZ` | GemPlayer | drift class | 59.79 | 92.92 |
| `0x82589c48` | 476 | `??0ModifierMgr@@QAA@XZ` | ModifierMgr | drift class | 87.69 | 97.35 |
| `0x825d2ef8` | 476 | `?UpdateSongStats@SongStatusMgr@@QAA_NW4ScoreType@@W4Difficulty@@ABVPerformerStatsInfo@@PAVSongStatus@@@Z` | SongStatusMgr | drift class | 98.15 | 98.15 |
| `0x826c2780` | 468 | `?SetPitchShiftRatio@GemPlayer@@QAAXM@Z` | GemPlayer | drift class | 62.20 | 76.95 |
| `0x825dfb00` | 468 | `?FetchLinkingCode@OvershellSlot@@QAAXXZ` | OvershellSlot | drift class | 56.40 | **100** |
| `0x82b9b600` | 460 | `?InitRGTuning@GemManager@@QAAXPAVBandUser@@@Z` | GemManager | drift class | 65.64 | 92.23 |
| `0x825ae160` | 456 | `?OnMsg@WaitingUserGate@@QAA?AVDataNode@@ABVLockStepStartMsg@@@Z` | WaitingUserGate | drift class | 69.32 | **100** |
| `0x82b7ed28` | 452 | `?Handle@TourDescPanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | TourDescPanel | drift class | 67.50 | **100** |
| `0x8262c690` | 432 | `?Custom@LayerProvider@@UBAXHHPAVUIListCustom@@PAVObject@Hmx@@@Z` | PatchPanel | drift class | 61.21 | **100** |
| `0x8259dd70` | 432 | `?SourceSym@BandSongMetadata@@QBA?AVSymbol@@XZ` | BandSongMetadata | drift class | 30.71 | **100** |
| `0x82651cd8` | 424 | `??1BandMatchmaker@@UAA@XZ` | Matchmaker | drift class | 66.14 | **100** |
| `0x826e78c0` | 420 | `?GetNumPhrases@VocalPlayer@@QAAMHHH@Z` | VocalPlayer | drift class | 85.66 | 86.80 |
| `0x826bd6c8` | 416 | `?ConfigureBehavior@GemPlayer@@UAAXXZ` | GemPlayer | drift class | 67.32 | 99.95 |
| `0x8269bef8` | 408 | `?CheckCoda@Band@@QAAXAAVSongPos@@@Z` | Band | drift class | 18.89 | **100** |
| `0x826be048` | 396 | `?AddHeadPoints@GemPlayer@@QAAXMHHW4GemHitFlags@@@Z` | GemPlayer | drift class | 43.92 | **100** |
| `0x8260e4e0` | 396 | `?AddGridThumbnails@CharacterCreatorPanel@@QAAXVSymbol@@@Z` | CharacterCreatorPanel | drift class | 27.56 | **100** |
| `0x82686f38` | 396 | `?SetupPhrasesForTrack@SongDB@@QAAXHAAV?$vector@VExtent@@V?$StlNodeAlloc@VExtent@@@stlpmtx_std@@@stlpmtx_std@@AAV?$vector@EV?$StlNodeAlloc@E@stlpmtx_std@@@3@@Z` | SongDB | template/STL | 80.00 | 80.00 |
| `0x82615fb0` | 392 | `?PreviewFinish@CustomizePanel@@QAAXVSymbol@@@Z` | CustomizePanel | drift class | 52.74 | 99.64 |
| `0x825c3c60` | 392 | `?IsSongAllowedToHavePart@BandMachineMgr@@QBA_NHVSymbol@@@Z` | BandMachineMgr | drift class | 58.08 | **100** |
| `0x822aaa70` | 392 | `??$PropSync@VPiercing@OutfitConfig@@@@YA_NAAV?$ObjVector@VPiercing@OutfitConfig@@@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | Gem | drift class | 86.17 | 86.17 |
| `0x825e8d88` | 384 | `?Configure@AccomplishmentSongFilterConditional@@QAAXPAVDataArray@@@Z` | AccomplishmentSongFilterConditional | drift class | 61.41 | 99.79 |
| `0x8250c280` | 376 | `?Poll@ContextWrapper@@QAAHXZ` | ContextWrapper | drift class | 53.12 | **100** |
| `0x8235d308` | 372 | `?UpdateNextMedalLabel@Tour@@QAAXPAVUILabel@@@Z` | Tour | drift class | 41.11 | **100** |
| `0x8235d108` | 372 | `?UpdateFinishedMedalLabel@Tour@@QAAXPAVUILabel@@@Z` | Tour | drift class | 41.11 | **100** |
| `0x8263a300` | 368 | `?ParseRecommendations@StoreInfoPanel@@QAA_NPAVDataArray@@@Z` | StoreInfoPanel | drift class | 61.60 | 99.95 |
| `0x824fa138` | 364 | `?OnMsg@RockCentral@@QAA?AVDataNode@@ABVRockCentralOpCompleteMsg@@@Z` | RockCentral | drift class | 59.92 | **100** |
| `0x8260c340` | 360 | `?InitData@CalibrationModesProvider@@UAAXPAVRndDir@@@Z` | CalibrationPanel | drift class | 48.48 | **100** |
| `0x82551d80` | 360 | `?OnMsg@SaveLoadManager@@QAA?AVDataNode@@ABVSigninChangedMsg@@@Z` | SaveLoadManager | drift class | 59.44 | **100** |
| `0x825d9140` | 356 | `?AttemptToggleAutoVocals@OvershellSlot@@QAAXXZ` | OvershellSlot | drift class | 69.74 | **100** |
| `0x8257d1c0` | 356 | `?SetVenue@MetaPerformer@@QAAXVSymbol@@@Z` | MetaPerformer | drift class | 43.11 | **100** |
| `0x826e1740` | 344 | `?FirstFrame_@PerfectOverdriveTracker@@UAAXM@Z` | PerfectOverdriveTracker | drift class | 58.28 | 99.92 |
| `0x8262c128` | 344 | `?SetStickers@StickerProvider@@QAAXPAV?$vector@PAVPatchSticker@@V?$StlNodeAlloc@PAVPatchSticker@@@stlpmtx_std@@@stlpmtx_std@@VSymbol@@@Z` | PatchPanel | drift class | 91.86 | 92.23 |
| `0x825b50f8` | 340 | `?CanGuitarPlayKeys@OvershellPanel@@QBA_NXZ` | OvershellPanel | drift class | 66.96 | **100** |
| `0x826d1fc0` | 336 | `?_M_insert_overflow_aux@?$vector@VTrackerPlayerDisplay@@V?$StlNodeAlloc@VTrackerPlayerDisplay@@@stlpmtx_std@@@stlpmtx_std@@IAAXPAVTrackerPlayerDisplay@@ABV3@ABU__false_type@2@I_N@Z` | Tracker | template/STL | 100.00 | **100** |
| `0x826cee50` | 328 | `?_M_insert_overflow_aux@?$vector@VData@MultiplayerAnalyzer@@V?$StlNodeAlloc@VData@MultiplayerAnalyzer@@@stlpmtx_std@@@stlpmtx_std@@IAAXPAVData@MultiplayerAnalyzer@@ABV34@ABU__false_type@2@I_N@Z` | MultiplayerAnalyzer | template/STL | 100.00 | **100** |
| `0x8257f5b0` | 324 | `?_M_insert_overflow_aux@?$vector@VPlayerScore@@V?$StlNodeAlloc@VPlayerScore@@@stlpmtx_std@@@stlpmtx_std@@IAAXPAVPlayerScore@@ABV3@ABU__false_type@2@I_N@Z` | MetaPerformer | drift class | 100.00 | **100** |
| `0x8266d7a8` | 324 | `?_M_insert_overflow_aux@?$vector@VLeaderboardRow@@V?$StlNodeAlloc@VLeaderboardRow@@@stlpmtx_std@@@stlpmtx_std@@IAAXPAVLeaderboardRow@@ABV3@ABU__false_type@2@I_N@Z` | Leaderboard | template/STL | 100.00 | **100** |
| `0x82634e50` | 324 | `?_M_insert_overflow_aux@?$vector@U?$pair@V?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@H@stlpmtx_std@@V?$StlNodeAlloc@U?$pair@V?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@H@stlpmtx_std@@@2@@stlpmtx_std@@IAAXPAU?$pair@V?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@H@2@ABU32@ABU__false_type@2@I_N@Z` | SetlistMergePanel | template/STL | 99.94 | 99.94 |
| `0x82566618` | 320 | `?SetDefaultColors@ClosetMgr@@QAAXXZ` | ClosetMgr | drift class | 53.54 | **100** |
| `0x8268c750` | 316 | `?SyncProperty@BandUser@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | BandUser | drift class | 59.66 | 99.94 |
| `0x82614fc8` | 316 | `?SetupCurrentOutfit@CustomizePanel@@QAAXVSymbol@@@Z` | CustomizePanel | drift class | 41.81 | **100** |
| `0x825bf0c0` | 316 | `?UtlInit@@YAXXZ` | MusicLibraryStore | drift class | 89.24 | **100** |
| `0x8268cc78` | 304 | `?OnSetPrefabChar@BandUser@@QAA?AVDataNode@@PAVDataArray@@@Z` | BandUser | drift class | 62.96 | **100** |
| `0x8260d280` | 304 | `?GetDefaultVKName@CharacterCreatorPanel@@QAAPBDXZ` | CharacterCreatorPanel | drift class | 30.76 | **100** |
| `0x825e4f90` | 304 | `?Configure@AccomplishmentDiscSongConditional@@QAAXPAVDataArray@@@Z` | AccomplishmentDiscSongConditional | drift class | 64.28 | **100** |
| `0x82566d20` | 304 | `?UpdateCurrentOutfitConfig@ClosetMgr@@QAAXXZ` | ClosetMgr | drift class | 50.96 | **100** |
| `0x826854a0` | 296 | `?ParseEvents@SongDB@@QAAXPAVDataEventList@@@Z` | SongDB | drift class | 65.92 | **100** |
| `0x8257cb50` | 292 | `?SetlistHasVocalHarmony@MetaPerformer@@QBA_NXZ` | MetaPerformer | drift class | 54.36 | **100** |
| `0x82548ba8` | 292 | `?CheckProfileWebSetlistStatus@ProfileMgr@@QAAXXZ` | ProfileMgr | drift class | 82.70 | 99.59 |
| `0x82548a30` | 292 | `?CheckProfileWebLinkStatus@ProfileMgr@@QAAXXZ` | ProfileMgr | drift class | 82.70 | 99.59 |
| `0x826fb4e0` | 288 | `?TambourineSwing@TambourineManager@@QAAHH@Z` | TambourineManager | drift class | 65.28 | 73.75 |
| `0x826b49e0` | 288 | `?RGFingerStep@ChordbookPanel@@QAA?AVSymbol@@H@Z` | PracticePanel | drift class | 18.40 | **100** |
| `0x825720e8` | 288 | `??0AppScoreDisplay@@QAA@XZ` | MetaPanel | drift class | 86.86 | 86.86 |
| `0x82baf278` | 284 | `??0LyricPlate@@QAA@PAVRndText@@PBV1@1@Z` | Lyric | drift class | 60.55 | 60.55 |
| `0x82572a40` | 284 | `?UpdatePostProc@MetaPanel@@QAAXXZ` | MetaPanel | drift class | 60.08 | **100** |
| `0x826720b8` | 280 | `?Text@NewAssetProvider@@UBAXHHPAVUIListLabel@@PAVUILabel@@@Z` | NewAssetProvider | drift class | 65.74 | **100** |
| `0x8259ea28` | 280 | `?LengthSym@BandSongMetadata@@QBA?AVSymbol@@XZ` | BandSongMetadata | drift class | 63.69 | 96.89 |
| `0x8255ccf0` | 280 | `?AddAwardSource@AccomplishmentManager@@QAAXVSymbol@@0@Z` | AccomplishmentManager | drift class | 69.26 | **100** |
| `0x82624f88` | 272 | `?CheckForKickoutCondition@ManageBandPanel@@QAAXXZ` | ManageBandPanel | drift class | 65.46 | **100** |
| `0x826f6430` | 268 | `?InitFromDataArray@TrackerMultiplierMap@@QAAXPBVDataArray@@@Z` | TrackerUtils | drift class | 69.76 | 95.21 |
| `0x826163d8` | 264 | `?ScalePatch@CustomizePanel@@QAAXMM@Z` | CustomizePanel | drift class | 82.98 | **100** |
| `0x82616230` | 264 | `?MovePatch@CustomizePanel@@QAAXMM@Z` | CustomizePanel | twin runner-up | 82.98 | 82.98 |
| `0x82681760` | 260 | `?Poll@GameMicManager@@QAAXM@Z` | GameMicManager | drift class | 52.08 | **100** |
| `0x82616810` | 260 | `?SetupAssetPatchData@CustomizePanel@@QAAXVSymbol@@@Z` | CustomizePanel | drift class | 69.34 | **100** |
| `0x826dda00` | 256 | `?UpdateTimeRemainingDisplay@OverdriveTracker@@QAAXXZ` | OverdriveTracker | drift class | 66.91 | **100** |
| `0x82591fd0` | 256 | `?InqGoalLeaderboardData@AccomplishmentProgress@@QBA_NAAV?$hash_map@VSymbol@@HU?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@H@stlpmtx_std@@@3@@stlpmtx_std@@@Z` | AccomplishmentProgress | drift class | 74.11 | **100** |
| `0x8268bf78` | 252 | `?ConnectedControllerType@LocalBandUser@@UBA?AW4ControllerType@@XZ` | BandUser | drift class | 30.83 | **100** |
| `0x825b72e8` | 252 | `?EnableAutoVocals@OvershellPanel@@QAAXXZ` | OvershellPanel | drift class | 3.92 | 95.84 |
| `0x82589aa0` | 252 | `??1ModifierMgr@@UAA@XZ` | ModifierMgr | drift class | 93.65 | 93.65 |
| `0x8255ce30` | 248 | `?InqAssetSourceList@AccomplishmentManager@@QBA_NVSymbol@@AAV?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@@Z` | AccomplishmentManager | drift class | 73.26 | **100** |
| `0x826e5e98` | 244 | `?OnMsg@VocalPlayer@@QAA_NABVButtonUpMsg@@@Z` | VocalPlayer | drift class | 71.75 | 71.75 |
| `0x825895a0` | 244 | `?Text@ModifierMgr@@UBAXHHPAVUIListLabel@@PAVUILabel@@@Z` | ModifierMgr | drift class | 69.48 | **100** |
| `0x826d49b0` | 244 | `?SetSuccessState@TrackerPlayerDisplay@@QBAX_N@Z` | TrackerDisplay | drift class | 100.00 | **100** |
| `0x826d45a0` | 244 | `?SetSuccessState@TrackerBandDisplay@@QBAX_N@Z` | TrackerDisplay | twin runner-up | 100.00 | **100** |
| `0x826dd5b8` | 240 | `?GetPlayerContributionString@OverdriveTracker@@UBA?AVString@@VSymbol@@@Z` | OverdriveTracker | drift class | 67.63 | 96.58 |
| `0x825d8a88` | 240 | `?ToggleCymbal@OvershellSlot@@QAAXVSymbol@@@Z` | OvershellSlot | drift class | 27.17 | **100** |
| `0x826d5048` | 240 | `?SetType@TrackerBroadcastDisplay@@QBAXW4BroadcastDisplayType@1@@Z` | TrackerDisplay | drift class | 100.00 | **100** |
| `0x826d4290` | 240 | `?SetType@TrackerBandDisplay@@QBAXW4TrackerBandDisplayType@@@Z` | TrackerDisplay | twin runner-up | 100.00 | **100** |
| `0x82b92d68` | 236 | `?CleanUpTracks@TrackPanel@@QAAXXZ` | TrackPanel | drift class | 61.00 | 99.92 |
| `0x826145a8` | 232 | `?ContentMountBegun@ContentLoadingPanel@@UAAXH@Z` | ContentLoadingPanel | drift class | 53.10 | **100** |
| `0x8253dbd8` | 228 | `?PushMissingSetlistSongsToScreen@MusicLibrary@@QAAXH@Z` | MusicLibrary | drift class | 75.54 | **100** |
| `0x826d7a40` | 220 | `?BroadcastFocusSuccess@AccuracyFocusTracker@@UBAXXZ` | FocusTracker | drift class | 61.31 | **100** |
| `0x8269d5c0` | 216 | `?Poll@Performer@@UAAXMABVSongPos@@@Z` | Performer | drift class | 62.15 | 99.91 |
| `0x8260dc58` | 212 | `?SetEyebrows@CharacterCreatorPanel@@QAAXVSymbol@@@Z` | CharacterCreatorPanel | drift class | 40.30 | 96.19 |
| `0x826143f0` | 208 | `?ContentStarted@ContentLoadingPanel@@UAAXXZ` | ContentLoadingPanel | drift class | 43.60 | **100** |
| `0x825d2180` | 208 | `?GetCompletedSongs@SongStatusMgr@@QBAHW4ScoreType@@W4Difficulty@@VSymbol@@@Z` | SongStatusMgr | drift class | 71.67 | **100** |
| `0x825be558` | 208 | `?IsVignette@@YA_NPAVUIPanel@@@Z` | MusicLibraryStore | drift class | 69.77 | **100** |
| `0x8260d908` | 204 | `?SetFaceHair@CharacterCreatorPanel@@QAAXVSymbol@@@Z` | CharacterCreatorPanel | drift class | 39.94 | 98.82 |
| `0x82540de0` | 204 | `?OnMsg@MusicLibrary@@QAA?AVDataNode@@ABVNewRemoteUserMsg@@@Z` | MusicLibrary | drift class | 67.41 | **100** |
| `0x8267ae58` | 200 | `?Jump@Game@@QAAXM_N@Z` | Game | drift class | 96.00 | **100** |
| `0x82652e08` | 200 | `?StartSearch@BandMatchmaker@@QAAX_N@Z` | Matchmaker | drift class | 66.40 | **100** |
| `0x825b7ea8` | 200 | `?OnMsg@OvershellPanel@@QAA?AVDataNode@@ABVSessionReadyMsg@@@Z` | OvershellPanel | drift class | 88.60 | **100** |
| `0x82b9a920` | 200 | `?_M_copy@?$_Rb_tree@PAVTrackWidget@@U?$less@PAVTrackWidget@@@stlpmtx_std@@PAV1@U?$_Identity@PAVTrackWidget@@@3@U?$_SetTraitsT@PAVTrackWidget@@@priv@3@V?$StlNodeAlloc@PAVTrackWidget@@@3@@stlpmtx_std@@AAAPAU_Rb_tree_node_base@2@PAU32@0@Z` | GemManager | template/STL | 99.80 | 99.80 |
| `0x825970f8` | 200 | `?_M_copy@?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@U?$pair@$$CBVSymbol@@H@3@U?$_Select1st@U?$pair@$$CBVSymbol@@H@stlpmtx_std@@@3@U?$_MapTraitsT@U?$pair@$$CBVSymbol@@H@stlpmtx_std@@@priv@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@H@stlpmtx_std@@@3@@stlpmtx_std@@AAAPAU_Rb_tree_node_base@2@PAU32@0@Z` | SongSortMgr | template/STL | 99.80 | 99.80 |
| `0x82651b28` | 148 | `?IsRanked@BandMatchmaker@@QBA_NXZ` | Matchmaker | new TU5 fn (name ours) | 100.00 | **100** |
| `0x82540208` | 140 | `?RefreshSongLists@MusicLibrary@@QAAXXZ` | MusicLibrary | new TU5 fn (name ours) | 100.00 | **100** |
| `0x826c9818` | 112 | `?BeginToken@TrainerPanel@@QBA?AVSymbol@@XZ` | TrainerPanel | new TU5 fn (name ours) | 100.00 | **100** |
| `0x826c9888` | 112 | `?EndToken@TrainerPanel@@QBA?AVSymbol@@XZ` | TrainerPanel | new TU5 fn (name ours) | 100.00 | **100** |
| `0x826c98f8` | 112 | `?ChallengeToken@TrainerPanel@@QBA?AVSymbol@@XZ` | TrainerPanel | new TU5 fn (name ours) | 100.00 | **100** |
| `0x826c9968` | 112 | `?StartEarlyToken@TrainerPanel@@QBA?AVSymbol@@XZ` | TrainerPanel | new TU5 fn (name ours) | 100.00 | **100** |
| `0x826c99d8` | 112 | `?StartNormToken@TrainerPanel@@QBA?AVSymbol@@XZ` | TrainerPanel | new TU5 fn (name ours) | 100.00 | **100** |
| `0x8269d068` | 76 | `?GetSongFraction@Performer@@QBAMXZ` | Performer | new TU5 fn (name ours) | 98.95 | 98.95 |
| `0x825c17d8` | 68 | `?HasProGuitarOrBass@BandMachine@@QBA_NH@Z` | BandMachine | new TU5 fn (name ours) | 100.00 | **100** |
| `0x82b907f8` | 60 | `?ResetEndingBonus@TrackPanel@@QAAXXZ` | TrackPanel | new TU5 fn (name ours) | 100.00 | **100** |

## 6. Left, and where the rest is

- **Held D rows (9), not named:**
  - `AllScoresAreIn@Singer`, `SetAssignedPart@Singer`: 1 AGREE only.
  - `HitTambourineGem@VocalTrack`, `SourceSym` at `0x8259E5B0`, `OnMsg(RockCentralOpCompleteMsg)`
    at `0x824F7890`, `UpdateCurrentOutfitConfig` at `0x825667C8`: name bound elsewhere. Each
    candidate's name now sits at its true address.
  - `??0AppScoreDisplay` at `0x82319DB0`: 2 caller contradictions.
  - `ToggleCymbal` at `0x825D8BE0`: contradicted; its twin `0x825D8A88` holds the name.
  - `GetFontCharForHarmonyMics` at `0x825BE760`: 1 caller contradiction.
- **Below 100, needing `src/system/` (out of scope):**
  - `GemPlayer::SetPitchShiftRatio` 76.9: an out-of-line `FxSendPitchShift` setter `0x827122A0`.
  - `VocalPlayer::OnMsg(ButtonUpMsg)` 71.8: `VocalTrackDir` method `0x822F6168`.
  - `LyricPlate` ctor 60.5: the `Color` default ctor.
  - `PropSync<Piercing>` 86.2: the `OutfitConfig.h` inline ctor.
  - `ModifierMgr` ctor/dtor 97.4/93.7: the `UIListProvider` base.
  - `TrainerPanel::InternalInitSections` 95.5: the `DataArray::Node` inline.
- **Below 100 for other reasons:**
  - `ViewSettingsProvider` ctor 83.3: two TU5 classes are unported, `SongUpsellViewSetting` and
    `BadReviewViewSetting` (from retail RTTI).
  - `AppScoreDisplay` ctor 86.9: vtordisp slot values, the same class of problem as HK's
    `ProTrainerPanel`.
  - Register or scheduling residue after about three ideas: `TambourineSwing` 73.8,
    `GetNumPhrases` 86.8, `InitRGTuning`, `UpdateGameCymbalLanes`, `LengthSym`, `UpdateSongStats`.
  - ICF fold-survivor callee names only: `ParseRecommendations`, `BuildSetlistTree`,
    `SyncSharedSongs`, `BandUser::SyncProperty`.
- **Class R, the next vein (25 rows / 12.4 KB, not worked).** Re-homing an already-pinned block is
  *not* metric-neutral, so it needs its own A/B. Strongest:
  - FreestylePanel's block `0x826B9BAC–0x826BA880` is VocalTrainerPanel code:
    `fn_826BA0A8` = `VocalTrainerPanel::Poll` (it calls `Loop`/`AddBeatMask`/`UpdateScore`),
    `fn_826BA2F8` = `StartSectionImpl`, `fn_826B9CC8` = `CopyTubes`, and
    `??_EVocalTrainerPanel` sits there at 0.
  - NewAwardPanel `fn_826275F0` = `MultiSelectListPanel::Handle` (10/10 features).
  - UIEventMgr `fn_82594090` = `Accomplishment::CanBeLaunched` (13/15); this is the same pin HK
    found holding `HasGamerpicReward`/`HasAvatarAssetReward`.
  - Stats `fn_826DBC68` = `PerfectSectionTracker::Poll_`.
  - BandMachine `fn_825C0618`/`fn_825C0BE0` = SongSort `BuildSongList`/`BuildSetlistList`.
  - RockCentral `fn_822EADE0` = `GemTrackDir::SetupSmasherPlate` (21 features).
  - The full list with overlap scores is in `~/tmp/w16ho/rehome_strong.json`.
- **Class A (142 / 57 KB)** has no identity in our base objs.
  - The A1 subset has oracle strings only, which makes it a port candidate: `NewAwardPanel` fake
    component scroll/select, `AccomplishmentManager` hint rows.
  - A2 is TU5/Xbox-only: `RetryAudioPanel` `finding_presence_*`, and AppInlineHelp's
    `get_slot_*` handler block, which sits right after `??_ECampaignSongInfoPanel`.
- **Class Q (Quazal, 55 / 27 KB):** no oracle. Out of scope for the same reason as HG/HK.

## 7. Reproduce

```
python3 ~/tmp/w16ho/census.py                           # population
python3 ~/tmp/w16ho/matrix.py 200 <out.json> 24          # byte matrix, rows >= 200 B
python3 ~/tmp/w16ho/matrix_tpl.py                        # template/STL re-score
python3 ~/tmp/w16ho/semantic.py; python3 ~/tmp/w16ho/classify3.py
python3 ~/tmp/w16ho/rehome.py; python3 ~/tmp/w16ho/miscarve.py; python3 ~/tmp/w16ho/small.py
python3 tools/anon_proposal_adjudicate.py <props.json> --json-out <adj.json>
python3 ~/tmp/w16ho/decide.py <adj.json> <props.json> <accept.json>
python3 tools/gated_map_write.py --target scripts/target_symbol_map.json --rows-json <rows.json>
python3 tools/icf_alias_finder.py --validate && python3 tools/map_name_injectivity.py
python3 tools/ab_measure.py --worktree <clean wt at 85f06cbc4> --patch <85f06cbc4..w16-ho>
```

Scratch inputs are not committed: `~/tmp/w16ho/` holds the census, matrices, adjudications,
accept lists, per-fork row lists, `final_rows.json` and `rehome_strong.json`. The fork branches
`w16-ho-{A,B,C,D}` are merged with `--no-ff`, so their per-row commits are in the history.
