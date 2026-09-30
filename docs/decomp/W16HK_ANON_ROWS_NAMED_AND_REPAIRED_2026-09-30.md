# W16-HK — naming and repairing anonymous game rows our source already defines (2026-09-30)

Branch `w16-hk` off main `caa195408`. Continues W16-HG
(`docs/decomp/W16HG_ANON_ROWS_WE_ALREADY_DEFINED_2026-09-30.md`), whose finding was that the
anonymous `fn_` rows in the game layer are mostly functions we already define, with bodies
that have drifted from retail. Scope: `src/band3`, `src/network` (not Quazal). Nothing under
`src/system/` was edited.

## 1. What was done

1. **Identity first, with W16-HF's adjudicator.** W16-HG's matrix
   (`~/tmp/w16hg/matrix40.json`, top 40 units) was turned into proposals: best candidate
   ≥ 70, margin ≥ 1 over the runner-up, bijective, generic helpers excluded (STL,
   `Ease*`, `BinStream` operators, `$4`/`$R4` thunks, `NewObject`). That gave 125
   proposals. `tools/anon_proposal_adjudicate.py` checks every implied relocation
   against retail bytes, and its control catches a wrong sibling in 1 of 168 cases. Its
   verdicts:
   - 72 mechanical accepts: no content negatives, at least one positive, and the
     runner-up contradicted.
   - 52 needing a manual read.
   - 1 refused.
2. **Manual reads.** Most negatives on a drifted row are the drift itself: Wii-only
   paths, `fmod` where retail calls `Mod`, TU5 calls absent from the oracle, and symbol
   literals. A row was refused when **matched callers call a different name at that
   address**. That signal found two swapped identities:
   - `0x82575628` is `BandSongMgr::IsRestricted`, with 5 caller witnesses.
   - `0x825d9640` is `OvershellSlot::DeleteCharacter`.

   It also found one wrong accept. `0x82b90670` = `??0TrackPanelInterface` was withdrawn
   because `icf_alias_finder --validate` failed on it: a proven alias group already
   places that ctor at `0x827ba020`.
3. **The matrix was extended to units 41+.** It covered 668 targets / 84,684 B. That run
   gave 86 more proposals: 48 mechanical accepts and 19 manual.
4. **Body repair, biggest first.** Five forks worked disjoint source files (A–E), each
   in its own worktree. Rules for each fork:
   - full `ninja-locked` builds only;
   - every row in the touched unit is compared after each edit;
   - an edit is kept only if nothing drops;
   - about three ideas per row, then stop.

   The coordinator did the unowned rows: `BandUser::SetChar`, `PresenceMgr` and
   `FilterViewSetting` dtors, `GetCurrentMakeup`, `SelectTour`, and `Player`
   `Deploy`/`BroadcastScore`/`Poll`/`DeterminePerformanceAwards`.
5. **`*_msg` → local `static Message` conversions.**
   - CampaignGoalsLeaderboardPanel's `ResultFailure`/`EnumerationStarted` (W16-HG left
     them ambiguous). Retail RTTI settles which row is which: `0x825F0D80` is slot 3
     and `0x825F0E40` slot 1 of `CampaignGoalsLeaderboardPanel`'s callback vtable
     `0x820ba4e4`, and the other pair belongs to `CampaignCareerLeaderboardPanel`.
   - Player's `send_update_energy`, `send_update_score`, `deploy`, `finished_coda`.
   - `TambourineManager`'s `tambourine_miss`.

   After conversion, `SetFinishedCoda`, `BroadcastScore` and `Deploy` became
   identifiable: each is ≥ 44 points ahead of the next candidate, and all were named.

## 2. Recurring retail-vs-port differences (the fixes, by pattern)

| pattern | examples |
|---|---|
| Wii global `Symbol`/`Message` → function-local `static` (one guard word per function) | ~40 rows: `GetCurrentMakeup` (`eyes`/`lips`), `Player::Poll` (`intro` sharing the guard with `send_update_energy`), `DeterminePerformanceAwards`, `AddPhrase` (an **unused** `vocals`), `OverdriveTracker::FirstFrame_` (unused `overdrive_chain`) |
| User-declared empty `virtual ~X() {}` where retail's dtor has no own-vtable store → implicit dtor (`patterns/fixable-declarations.md`) | `PresenceMgr`, `FilterViewSetting`, `BandSongMgr`, `CharCache`, `Track` (partial: the rest is `TrackInterface.h`, in `src/system/`) |
| Dev-build code absent in TU5 → `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` or removed | `SetChar` `send_fake_patches`, `GetFeatureIndex` null check, `AssetProvider::UpdateExtendedText` `gShowAssetName`, `RockCentral::Poll` login test, Wii `JoypadWiiOnUserLeft` |
| TU5 code absent from the oracle, rebuilt from retail bytes | `GemPlayer::JumpReset` tail + new `GemTrack::JumpReset`, `AddAccomplishment` gamerpic/avatar rewards, `UpdateMetaMusic` XMP toggling, `ProfileMgr` ctor lag table and 5/22/22/50 ms defaults, `SetChar`'s discarded `vector<BandCharDesc*>` |
| Loop/expression shape | vector reference hoisting `begin` (`GetCurrentMakeup`, `IsAssetPatchable`), signed range test instead of the Wii unsigned trick (`BroadcastScore`), double inline `GetBandTrack()` (`Deploy`) |

New functions added for retail calls with no oracle. Their **names are ours**:
- `ClosetMgr::IsPurchaseUIActive` (`0x82566988`, unmapped)
- `GetKickPercent(const Stats&)` (`0x8258ef20`, mapped)
- `GemTrack::JumpReset` (`0x82b951d8`, mapped)
- `Accomplishment::HasGamerpicReward` / `HasAvatarAssetReward` (`0x82594058`/`0x82594070`)

The last two are **not mapped**. Those retail rows sit inside the `UIEventMgr` pin,
whose base obj cannot define them, so a name would pair with nothing. This is a pin
issue.

**Two name corrections.**
- `0x82b90670` = `??0TrackPanelInterface` was withdrawn (validate FAILED on it; see §1).
- The four Campaign "points for next major level" rows had **Current and Total crossed**:
  `0x825a6f08`/`0x825a6ff8` are `GetTotal…`, `0x825a7070`/`0x825a7158` are `GetCurrent…`.
  Three of the four were pre-existing map rows; the fourth, `0x825a7070`, was this lane's.
  Four pieces of evidence, none of them a score:
  1. Retail layout is [X·MetaScore, X·ForUser, Y·MetaScore, Y·ForUser], in our source's
     Total-then-Current order.
  2. `0x825a6f08` returns next − current, which is a total. `0x825a7070` returns
     metaScore − current, which is a current.
  3. The rb3-Wii oracle calls Current then Total in both `UpdateProgressMeter` and
     `NotifyPlayerOfAccomplishment`.
  4. Lane MPNGAP-1 (`ebec38819`) had transposed `UpdateProgressMeter`'s calls to fit the
     crossed names. With that reverted to the oracle order, the inlining
     `Campaign::Handle` stays at 100.

  Result: all four rows are at 100, and nothing dropped.

## 3. Measurement

`tools/ab_measure.py --worktree ~/tmp/wt-w16-hk-ab --patch <caa195408..w16-hk>` was run
**once**, on the final branch.

The patch excludes `symbols.txt`. The base worktree instead carries the two dtk
over-carve merges, committed as `f806edfbd`, so both legs have them. The reason:
naming `0x8264bf10` and `0x8268f148` lets dtk merge a branch-reached tail into each
(0x7C+0x10 → 0x8C, 0x58+0x8 → 0x60), and that rewrites `symbols.txt`. The split guard
fails the build on that rewrite, which is what refused the first attempt. The merges
are correct: with the merge, `TrackTypeToControllerType` is 96 B on both sides and
scores 100. In leg A these rows are anonymous and unpaired, so carrying the merges
there moves no matched figure.

| | leg A (base + merges) | leg B (branch) | Δ |
|---|---:|---:|---:|
| matched_functions | 44,604 | 44,767 | **+163** |
| masked_equal | 23,341 | 23,382 | +41 |
| honest (matched − masked_equal) | 21,263 | 21,385 | **+122** |
| matched_code | 4,336,164 | 4,371,596 | **+35,432 B** |
| matched_code_percent | 42.316160 | 42.661938 | +0.345778 pp |
| fuzzy_match_percent | 50.955180 | 51.489906 | +0.534726 pp |
| total_functions / total_code | 69,236 / 10,247,064 | same | 0 |

- Both legs were settled and read at a split fixed point. The ruler was `name_check`,
  the shipped one.
- **Units:** 60 improved, **0 regressed**. RGTutor reached 100%, which takes units at
  100 from 212 to 213 on both rulers.
- `none`-ruler control: +39,900 B. The tool reports `NOT_APPLICABLE` because the patch
  includes source.
- **Predicted vs measured:** I predicted +163 fns / +35,432 B, from the first A/B
  (+161 / +35,184) plus the two Campaign MetaScore rows reaching 100 (120 B + 128 B).
  The measurement matched exactly.
- Where the +163 comes from: 117 of the 178 newly named rows reached 100, 33,448 B.
  One pre-existing row did too (Campaign `0x825a6f08`, 128 B). The rest are EH funclets
  that newly pair by bytes (Δmasked_equal +41), plus neighbours that source edits
  fixed on the way (for example `Campaign::GetTotalPoints…` siblings,
  `BandSongMgr::GetRankedSongs`).
- The first A/B, on the branch before the Campaign fix, read +161 / +35,184 B, with 59
  units improved and 0 regressed.

## 4. Rows, before and after

On main (`caa195408`) **every row below is an anonymous `fn_` at fuzzy 0, unpaired**; the
"main" column is omitted for that reason. "at naming" is the row's `fuzzy_match_percent`
right after the map entry went in, before any body repair. "after" is `report.json`
after the final full build.

| after | rows | bytes |
|---|---:|---:|
| 100 | 117 | 33,448 |
| 99–100 | 15 | 6,292 |
| 95–99 | 24 | 7,252 |
| 90–95 | 11 | 5,096 |
| < 90 | 11 | 3,000 |
| **all** | **178** | **55,088** |

| retail row | size | name | unit | at naming | after |
|---|---:|---|---|---:|---:|
| `0x82b95570` | 1652 | `?DrawFill@GemTrack@@QAAXPAVFillInfo@@HH@Z` | GemTrack | 71.71 | 87.56 |
| `0x8264ab98` | 1556 | `?SetupDetailLine@NextSongPanel@@QAAXPAVDataArray@@HPBDM@Z` | NextSongPanel | 73.77 | 94.96 |
| `0x822a5a38` | 1164 | `?PropSync@@YA_NAAVMatSwap@OutfitConfig@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | Gem | 94.78 | 94.78 |
| `0x825927b0` | 1084 | `?AddAccomplishment@AccomplishmentProgress@@QAA_NVSymbol@@@Z` | AccomplishmentProgress | 82.94 | 99.98 |
| `0x826a12a8` | 1024 | `?ComputeStarThresholds@Scoring@@QBAX_N@Z` | Scoring | 96.03 | **100** |
| `0x824f9620` | 1020 | `?RecordDataPoint@RockCentral@@SAXAAVDataPoint@@HAAVDataResultList@@PAVObject@Hmx@@@Z` | RockCentral | 78.42 | **100** |
| `0x8269c118` | 1012 | `??0Band@@QAA@_NHPAVBandUser@@PAVBeatMaster@@@Z` | Band | 96.34 | **100** |
| `0x82b949f8` | 988 | `?CheckShifts@GemTrack@@QAAXMH@Z` | GemTrack | 89.90 | **100** |
| `0x82b973d0` | 892 | `?UpdateShifts@GemTrack@@UAAXXZ` | GemTrack | 92.77 | 99.93 |
| `0x8262a4e0` | 868 | `?OnMsg@PatchPanel@@QAA?AVDataNode@@ABVButtonDownMsg@@@Z` | PatchPanel | 92.12 | **100** |
| `0x82b7e3a8` | 836 | `?UpdateExtendedText@TourDescProvider@@UBAXHHPAVUILabel@@@Z` | TourDescPanel | 71.09 | 99.98 |
| `0x8250df50` | 804 | `?BuildProfileUploadOps@EntityUploader@@QAAHAAPAPAVEntityData@@PAVBandProfile@@@Z` | EntityUploader | 98.00 | **100** |
| `0x82573ee0` | 760 | `??0MetaPanel@@QAA@XZ` | MetaPanel | 95.26 | 95.26 |
| `0x826abbb8` | 760 | `?CopyGems@GemTrainerPanel@@QAAXH@Z` | GemTrainerPanel | 95.00 | **100** |
| `0x826c39f8` | 704 | `?PostLoad@GemPlayer@@UAAX_N@Z` | GemPlayer | 94.99 | **100** |
| `0x826717c8` | 696 | `?Update@AssetProvider@@QAAXW4AssetType@@W4AssetBoutique@@@Z` | AssetProvider | 77.80 | 98.42 |
| `0x8258f280` | 664 | `?UpdateStats@AccomplishmentProgress@@QAAXW4ScoreType@@W4Difficulty@@HABVStats@@PAVPerformer@@PAVBand@@@Z` | AccomplishmentProgress | 74.95 | **100** |
| `0x825d99f0` | 640 | `?SelectPartImpl@OvershellSlot@@QAAXW4TrackType@@_N1@Z` | OvershellSlot | 70.46 | **100** |
| `0x8257a538` | 624 | `?RemoveOldestCachedContent@BandSongMgr@@QAA_NXZ` | BandSongMgr | 70.30 | 96.44 |
| `0x826faae8` | 620 | `?TambourineFail@TambourineManager@@QAAXH_N@Z` | TambourineManager | 88.41 | **100** |
| `0x826a72b8` | 608 | `?Poll@Player@@UAAXMABVSongPos@@@Z` | Player | 73.38 | **100** |
| `0x826f2330` | 592 | `?Poll@VocalPart@@QAAXMABVSongPos@@@Z` | VocalPart | 82.47 | **100** |
| `0x822ab3e0` | 588 | `??0OutfitConfig@@QAA@XZ` | Gem | 90.40 | 90.40 |
| `0x826b0180` | 588 | `?Enter@ProTrainerPanel@@UAAXXZ` | RGTrainerPanel | 92.94 | **100** |
| `0x826a4d80` | 584 | `?Deploy@Player@@QAAXXZ` | Player | 79.16 | **100** |
| `0x82686cf0` | 584 | `?SetupCommonPhrasesForTrack@SongDB@@QAAXH@Z` | SongDB | 93.68 | **100** |
| `0x8258f038` | 580 | `?UpdateScoreTypeSpecificStats@AccomplishmentProgress@@QAAXW4ScoreType@@W4Difficulty@@ABVStats@@PAVPerformer@@PAVBand@@@Z` | AccomplishmentProgress | 93.48 | 93.48 |
| `0x82548058` | 564 | `??0ProfileMgr@@QAA@XZ` | ProfileMgr | 88.76 | **100** |
| `0x8260fda0` | 564 | `?SetOutfit@CharacterCreatorPanel@@QAAXVSymbol@@@Z` | CharacterCreatorPanel | 89.11 | **100** |
| `0x82bab960` | 564 | `?AddStrumInstance@Gem@@QAAXVSymbol@@0@Z` | Gem | 70.80 | **100** |
| `0x826d72b8` | 540 | `?CheckCondition@StreakFocusTracker@@UAAXM_NAA_N1@Z` | FocusTracker | 76.69 | 99.24 |
| `0x825796c0` | 536 | `?Init@BandSongMgr@@UAAXXZ` | BandSongMgr | 74.87 | **100** |
| `0x826dbef0` | 532 | `?FirstFrame_@PerfectSectionTracker@@UAAXM@Z` | PerfectSectionTracker | 88.66 | 99.96 |
| `0x8260ef38` | 516 | `?Load@CharacterCreatorPanel@@UAAXXZ` | CharacterCreatorPanel | 83.27 | **100** |
| `0x8260c070` | 496 | `?TriggerCalibration@CalibrationPanel@@QAAXH@Z` | CalibrationPanel | 74.76 | **100** |
| `0x826a35f0` | 488 | `?SetEnergyAutomatically@Player@@QAAXM@Z` | Player | 93.24 | **100** |
| `0x82670230` | 484 | `??0MakeupProvider@@QAA@VSymbol@@@Z` | MakeupProvider | 71.66 | **100** |
| `0x826184f8` | 476 | `?GetCurrentMakeup@CustomizePanel@@QAA?AVSymbol@@V2@@Z` | CustomizePanel | 73.17 | **100** |
| `0x826d2510` | 468 | `?Restart@Tracker@@QAAXXZ` | Tracker | 89.41 | 92.16 |
| `0x82618328` | 452 | `?OnMsg@CustomizePanel@@QAA?AVDataNode@@ABVButtonDownMsg@@@Z` | CustomizePanel | 92.58 | **100** |
| `0x826710d8` | 452 | `?UpdateExtendedText@AssetProvider@@UBAXHHPAVUILabel@@@Z` | AssetProvider | 73.54 | **100** |
| `0x8268ca38` | 448 | `?SetChar@BandUser@@QAAXPAVCharData@@@Z` | BandUser | 74.15 | 98.79 |
| `0x826afb88` | 432 | `?HandleLegendLefty@RGTrainerPanel@@QAAX_N@Z` | RGTrainerPanel | 83.41 | **100** |
| `0x826de428` | 428 | `?FirstFrame_@OverdriveTracker@@UAAXM@Z` | OverdriveTracker | 71.97 | **100** |
| `0x82baeee8` | 428 | `?Poll@LyricPlate@@QAAXM@Z` | Lyric | 92.43 | **100** |
| `0x82672280` | 428 | `?UpdateExtendedText@NewAssetProvider@@UBAXHHPAVUILabel@@@Z` | NewAssetProvider | 72.36 | **100** |
| `0x826aa3c8` | 408 | `?HandleTrackShifting@GemTrainerPanel@@QAAXXZ` | GemTrainerPanel | 85.79 | **100** |
| `0x826a9fc8` | 380 | `?SetLoopPoints@GemTrainerPanel@@QAAXXZ` | GemTrainerPanel | 76.72 | **100** |
| `0x823e32f0` | 372 | `??1NetSession@@UAA@XZ` | NetSession | 91.08 | 99.95 |
| `0x82b99a58` | 372 | `?CheckRemoveChordBracket@GemManager@@QAAXH@Z` | GemManager | 91.67 | 98.60 |
| `0x82ba5d90` | 368 | `?BuildScrollingDeployZones@VocalTrack@@QAAXM@Z` | VocalTrack | 94.21 | 98.12 |
| `0x8255cf50` | 364 | `?UpdateAssetHintLabel@AccomplishmentManager@@QAAXVSymbol@@PAVUILabel@@@Z` | AccomplishmentManager | 70.37 | **100** |
| `0x8260c508` | 364 | `?OnMsg@CalibrationPanel@@QAA?AVDataNode@@ABVButtonDownMsg@@@Z` | CalibrationPanel | 80.03 | **100** |
| `0x826a8458` | 356 | `?DeterminePerformanceAwards@Player@@QAAXXZ` | Player | 85.04 | 97.63 |
| `0x8262a848` | 344 | `?OnMsg@PatchPanel@@QAA?AVDataNode@@ABVButtonUpMsg@@@Z` | PatchPanel | 83.16 | **100** |
| `0x826259e0` | 332 | `?RefreshVignettes@VignetteViewerProvider@@QAAXPAVBandProfile@@PAVDataArray@@@Z` | ManageBandPanel | 81.90 | 99.88 |
| `0x8255f1f0` | 320 | `?ConfigurePrecachedFilterData@AccomplishmentManager@@QAAXPAVDataArray@@@Z` | AccomplishmentManager | 97.81 | 98.00 |
| `0x825da840` | 308 | `?SelectChar@OvershellSlot@@QAAXH@Z` | OvershellSlot | 78.77 | **100** |
| `0x824f7110` | 308 | `?Poll@RockCentral@@QAAXXZ` | RockCentral | 82.73 | **100** |
| `0x825a48e0` | 304 | `?Configure@AccomplishmentTourConditional@@QAAXPAVDataArray@@@Z` | AccomplishmentTourConditional | 78.87 | 99.92 |
| `0x82629fe8` | 304 | `?CalcMotion@PatchPanel@@QAAMMH@Z` | PatchPanel | 96.45 | **100** |
| `0x825c2838` | 296 | `?AddRemoteMachine@BandMachineMgr@@QAAXI@Z` | BandMachineMgr | 98.31 | 98.31 |
| `0x826f6680` | 288 | `?CountNonEmptySections@TrackerSectionManager@@QBAHPBVTrackerSource@@_N@Z` | TrackerUtils | 99.10 | 99.10 |
| `0x8260e210` | 288 | `?SetFaceType@CharacterCreatorPanel@@QAAXVSymbol@@@Z` | CharacterCreatorPanel | 97.85 | 97.85 |
| `0x826f27a8` | 288 | `?UpdateMinMaxPitch@VocalPart@@QAAXABQBVVocalPhrase@@@Z` | VocalPart | 75.24 | 98.61 |
| `0x8266ad88` | 284 | `?Configure@AccomplishmentConditional@@QAAXPAVDataArray@@@Z` | AccomplishmentConditional | 77.72 | **100** |
| `0x82bb0890` | 276 | `??1Tail@@UAA@XZ` | Tail | 98.01 | 98.01 |
| `0x82687360` | 276 | `?AddPhrase@SongDB@@UAAXW4BeatmatchPhraseType@@HABUPhrase@@@Z` | SongDB | 75.23 | **100** |
| `0x8235c100` | 276 | `?GetFilterName@Tour@@QBA?AVString@@VSymbol@@@Z` | Tour | 78.41 | **100** |
| `0x82bab460` | 272 | `?Poll@Gem@@QAAXMMMMM@Z` | Gem | 92.25 | 96.25 |
| `0x826a4170` | 268 | `?BroadcastScore@Player@@QAAXXZ` | Player | 89.76 | **100** |
| `0x8266b1f8` | 268 | `?Configure@AccomplishmentTrainerConditional@@QAAXPAVDataArray@@@Z` | AccomplishmentConditional | 76.96 | 99.93 |
| `0x826f26a0` | 264 | `?CalculateScore@VocalPart@@QBAXMHMAAVVocalScoreCache@@@Z` | VocalPart | 97.35 | **100** |
| `0x82639370` | 260 | `?FetchRecommendations@StoreInfoPanel@@QAAXXZ` | StoreInfoPanel | 72.66 | **100** |
| `0x82594ad0` | 256 | `?IsUserOnValidScoreType@Accomplishment@@QBA_NPAVLocalBandUser@@@Z` | Accomplishment | 86.69 | 96.72 |
| `0x82644290` | 256 | `?HideAllDetailComponents@NextSongPanel@@QAAXH@Z` | NextSongPanel | 75.91 | **100** |
| `0x825c31d0` | 244 | `?OnMsg@BandMachineMgr@@QAA?AVDataNode@@ABVNewRemoteUserMsg@@@Z` | BandMachineMgr | 97.79 | 97.79 |
| `0x826c38f8` | 244 | `?JumpReset@GemPlayer@@QAAXM@Z` | GemPlayer | 70.49 | **100** |
| `0x825da3d0` | 244 | `?OnMsg@OvershellSlot@@QAA?AVDataNode@@ABVLocalUserLeftMsg@@@Z` | OvershellSlot | 75.23 | **100** |
| `0x82632448` | 240 | `?PollForLoading@SelectDifficultyPanel@@UAAXXZ` | SelectDifficultyPanel | 71.85 | **100** |
| `0x82682e00` | 236 | `?RemoveNullUsers@BandUserMgr@@QAAXXZ` | BandUserMgr | 100.00 | **100** |
| `0x8235ebb8` | 232 | `?ConfigureTourPropertyData@Tour@@QAAXPAVDataArray@@@Z` | Tour | 99.91 | 99.91 |
| `0x825e05c0` | 228 | `?AttemptSwapUserProfile@OvershellSlot@@QAAXH@Z` | OvershellSlot | 73.84 | 73.84 |
| `0x82b99ff0` | 220 | `?InMissedPhrase@GemManager@@QAA_NH@Z` | GemManager | 91.55 | **100** |
| `0x8260e738` | 220 | `?GetFeatureIndex@CharacterCreatorPanel@@QAAHVSymbol@@@Z` | CharacterCreatorPanel | 73.00 | **100** |
| `0x82b915c8` | 220 | `?HandleRemoveUser@TrackPanel@@QAAXPAVBandUser@@@Z` | TrackPanel | 80.71 | **100** |
| `0x826dd490` | 216 | `?SavePlayerStats@OverdriveTracker@@UBAXXZ` | OverdriveTracker | 98.85 | **100** |
| `0x82681da8` | 208 | `?HookUpFxForMicId@GameMicManager@@QAAXPAVGameMic@@@Z` | GameMicManager | 78.85 | 78.85 |
| `0x826f25d0` | 204 | `?AddPhrasePoints@VocalPart@@QAAXM@Z` | VocalPart | 99.61 | **100** |
| `0x82ba4048` | 200 | `?FreeOldGems@TambourineGemPool@@QAAXM@Z` | VocalTrack | 71.92 | 99.90 |
| `0x826a3410` | 200 | `?GetMultiplier@Player@@UBAH_NAAH11@Z` | Player | 95.60 | **100** |
| `0x825a6048` | 196 | `?OnMsg@Campaign@@QAA?AVDataNode@@ABVProfileSwappedMsg@@@Z` | Campaign | 86.33 | 86.33 |
| `0x826b4910` | 196 | `?ProfileCheckComplete@ChordbookPanel@@QAA_NVSymbol@@PAVGemPlayer@@@Z` | PracticePanel | 83.10 | **100** |
| `0x8258ae28` | 196 | `?HasSomethingToUpload@BandProfile@@UAA_NXZ` | BandProfile | 93.88 | **100** |
| `0x826d5688` | 188 | `?Enable@TrackerPlayerDisplay@@QBAXXZ` | TrackerDisplay | 76.60 | **100** |
| `0x826725f0` | 188 | `??0NewAssetProvider@@QAA@PAVBandProfile@@W4AssetGender@@@Z` | NewAssetProvider | 74.04 | **100** |
| `0x825d9640` | 188 | `?DeleteCharacter@OvershellSlot@@QAAXXZ` | OvershellSlot | 63.55 | **100** |
| `0x8257d920` | 188 | `?ExportUpdateMetaPerformer@MetaPerformer@@QAAXXZ` | MetaPerformer | 74.79 | **100** |
| `0x82671660` | 188 | `??0AssetProvider@@QAA@PAVBandProfile@@W4AssetGender@@@Z` | AssetProvider | 73.83 | **100** |
| `0x8257a7e0` | 184 | `??1BandSongMgr@@UAA@XZ` | BandSongMgr | 71.50 | **100** |
| `0x823e3970` | 184 | `?SetDoneArbitrating@NetSession@@QAAXH@Z` | NetSession | 95.07 | 99.89 |
| `0x82593c80` | 176 | `??0UIEventMgr@@QAA@XZ` | UIEventMgr | 97.39 | 97.39 |
| `0x82693fb8` | 176 | `?CreateSource@TrackerManager@@QBAPAVTrackerSource@@ABVTrackerDesc@@@Z` | TrackerManager | 97.73 | 97.73 |
| `0x8235eb00` | 176 | `?Cleanup@Tour@@QAAXXZ` | Tour | 100.00 | **100** |
| `0x82571ff8` | 176 | `?UpdateMetaMusic@MetaPanel@@QAAXVSymbol@@@Z` | MetaPanel | 76.82 | **100** |
| `0x826a5190` | 172 | `?SetFinishedCoda@Player@@QAAXXZ` | Player | 100.00 | **100** |
| `0x8256be10` | 172 | `?RecomposeCharsWithPatchIx@CharCache@@QAAXH@Z` | CharCache | 91.63 | **100** |
| `0x826af980` | 164 | `??0ProTrainerPanel@@QAA@XZ` | RGTrainerPanel | 97.44 | 97.44 |
| `0x8269c760` | 160 | `?AddPlayerDynamically@Band@@QAAPAVPlayer@@PAVBeatMaster@@PAVBandUser@@@Z` | Band | 72.50 | **100** |
| `0x82633ed0` | 160 | `?IntToSetlistIndex@SetlistMergePanel@@SAHHH@Z` | SetlistMergePanel | 97.38 | 97.38 |
| `0x8257cd58` | 160 | `?SetHasMissingVocalHarmony@MetaPerformer@@QBA_NXZ` | MetaPerformer | 71.33 | **100** |
| `0x82b7ead8` | 160 | `?UpdateList@TourDescProvider@@QAAXXZ` | TourDescPanel | 94.72 | **100** |
| `0x82545d28` | 160 | `?ForceMicOutputGain@ProfileMgr@@QAAXHM@Z` | ProfileMgr | 91.85 | **100** |
| `0x8257c938` | 156 | `?VocalHarmonyInSong@MetaPerformer@@QBA_NXZ` | MetaPerformer | 70.28 | **100** |
| `0x826f67a0` | 152 | `?GatherSections@TrackerSectionManager@@QAAXXZ` | TrackerUtils | 91.32 | 91.32 |
| `0x826d48d0` | 152 | `?Disable@TrackerPlayerDisplay@@QBAXXZ` | TrackerDisplay | 71.32 | **100** |
| `0x825877a8` | 152 | `?Init@SessionMgr@@SAXXZ` | SessionMgr | 97.24 | 97.24 |
| `0x82b7c018` | 152 | `?SelectTour@TourDescPanel@@QAAXVSymbol@@@Z` | TourDescPanel | 90.53 | 98.82 |
| `0x82b92458` | 152 | `?PostHandleRemoveUser@TrackPanel@@QAAXPAVBandUser@@@Z` | TrackPanel | 93.82 | 99.87 |
| `0x825f0d80` | 152 | `?ResultFailure@CampaignGoalsLeaderboardPanel@@UAAXXZ` | CampaignGoalsLeaderboardPanel | 100.00 | **100** |
| `0x825f0e40` | 152 | `?EnumerationStarted@CampaignGoalsLeaderboardPanel@@UAAXXZ` | CampaignGoalsLeaderboardPanel | 100.00 | **100** |
| `0x825c25a8` | 148 | `?GetRemoteMachine@BandMachineMgr@@QBAPAVRemoteBandMachine@@I_N@Z` | BandMachineMgr | 96.62 | 96.62 |
| `0x82360ca0` | 148 | `?IsWinning@TourPerformerImpl@@UBA_NXZ` | TourPerformer | 97.14 | 97.14 |
| `0x8268e520` | 148 | `??1NullLocalBandUser@@UAA@XZ` | BandUser | 100.00 | **100** |
| `0x825f7b48` | 140 | `?CanLaunchSelectedEntry@AccomplishmentPanel@@QBA_NXZ` | AccomplishmentPanel | 91.43 | **100** |
| `0x825a5310` | 140 | `?GetNextCampaignLevel@Campaign@@QBA?AVSymbol@@V2@@Z` | Campaign | 86.11 | 86.11 |
| `0x8264bf10` | 140 | `?SetLeaderboardStatus@AppMiniLeaderboardDisplay@@QAAXW4LeaderboardStatus@@@Z` | AppMiniLeaderboardDisplay | 91.43 | 91.43 |
| `0x82616938` | 140 | `?IsAssetPatchable@CustomizePanel@@QAA_NXZ` | CustomizePanel | 88.43 | **100** |
| `0x825907f8` | 140 | `?AddNewRewardVignette@AccomplishmentProgress@@QAAXVSymbol@@@Z` | AccomplishmentProgress | 93.71 | 93.71 |
| `0x82556c98` | 136 | `?GetNameForFirstNewRewardVignette@AccomplishmentManager@@QBA?AVSymbol@@XZ` | AccomplishmentManager | 78.82 | **100** |
| `0x826bd238` | 128 | `?GetPlayerState@GemPlayer@@QBAXAAVPlayerState@@@Z` | GemPlayer | 73.81 | **100** |
| `0x82361148` | 124 | `??1TourPerformerImpl@@UAA@XZ` | TourPerformer | 90.32 | **100** |
| `0x8257c1b8` | 124 | `??0PerformerStatsInfo@@QAA@ABV0@@Z` | MetaPerformer | 90.32 | 90.32 |
| `0x826a54a0` | 124 | `?CompleteCommonPhrase@Player@@UAAX_N0@Z` | Player | 92.87 | **100** |
| `0x825a7070` | 120 | `?GetCurrentPointsForNextMajorCampaignLevelForMetaScore@Campaign@@QAAHH@Z` | Campaign | 84.57 | **100** |
| `0x825762a8` | 116 | `?GetPartDifficulty@BandSongMgr@@QBAHVSymbol@@0@Z` | BandSongMgr | 71.17 | **100** |
| `0x8235c080` | 116 | `?UpdateProgressWithCareerData@Tour@@QAAXXZ` | Tour | 100.00 | **100** |
| `0x826d6bf8` | 116 | `?PlayerWantsFocus@AccuracyFocusTracker@@UBA_NABUTrackerPlayerID@@M@Z` | FocusTracker | 78.38 | **100** |
| `0x82681418` | 112 | `?SetEnable@@YA?AVDataNode@@PAVDataArray@@@Z` | GameMicManager | 88.21 | 88.21 |
| `0x826d0fe8` | 112 | `?CalcProgressPercentage@Tracker@@QBAMXZ` | Tracker | 87.86 | **100** |
| `0x826e1e60` | 112 | `?UpdateTimeRemainingDisplay@OverdriveTimeTracker@@QAAXXZ` | OverdriveTimeTracker | 96.25 | 96.25 |
| `0x8235c4c8` | 112 | `?OnMsg@Tour@@QAA?AVDataNode@@ABVRemoteLeaderLeftMsg@@@Z` | Tour | 89.11 | **100** |
| `0x825f7a70` | 108 | `?GetCurrentUnits@AccomplishmentPanel@@QBA?AVSymbol@@H@Z` | AccomplishmentPanel | 96.07 | **100** |
| `0x82634260` | 104 | `??1SetlistSubmissionMsg@@UAA@XZ` | SetlistMergePanel | 88.04 | 88.04 |
| `0x825f7ae0` | 104 | `?IsSecret@AccomplishmentPanel@@QBA_NXZ` | AccomplishmentPanel | 95.92 | **100** |
| `0x8250d4c8` | 104 | `?HasServerTimedOut@EntityUploader@@QAA_NXZ` | EntityUploader | 78.46 | **100** |
| `0x82b96e58` | 104 | `??1Track@@UAA@XZ` | GemTrack | 76.50 | 88.46 |
| `0x825d6288` | 104 | `??1FilterViewSetting@@UAA@XZ` | ViewSetting | 88.04 | **100** |
| `0x826b1e78` | 100 | `?IsVocals@PracticePanel@@QBA_NXZ` | PracticePanel | 87.40 | **100** |
| `0x82680ce0` | 100 | `??1PresenceMgr@@UAA@XZ` | PresenceMgr | 87.56 | **100** |
| `0x82b9ad38` | 100 | `?DisableSlot@GemManager@@QAAXH@Z` | GemManager | 93.60 | 93.60 |
| `0x825f73a0` | 96 | `?GetNumCompleted@AccomplishmentProvider@@QAAHXZ` | AccomplishmentPanel | 92.88 | **100** |
| `0x826b1e18` | 96 | `?IsDrums@PracticePanel@@QBA_NXZ` | PracticePanel | 86.88 | **100** |
| `0x8268f148` | 96 | `?TrackTypeToControllerType@@YA?AW4ControllerType@@W4TrackType@@@Z` | Defines | 100.00 | **100** |
| `0x82b951d8` | 92 | `?JumpReset@GemTrack@@QAAXXZ` | GemTrack | n/a | **100** |
| `0x8258ef20` | 92 | `?GetKickPercent@@YAHABVStats@@@Z` | BandProfile | 100.00 | **100** |
| `0x825678c8` | 92 | `?SetCurrentCharacterPatch@ClosetMgr@@QAAXW4Category@Patch@BandCharDesc@@PBD@Z` | ClosetMgr | 73.70 | 73.70 |
| `0x826815f0` | 84 | `?DeleteMic@GameMicManager@@QAAXH@Z` | GameMicManager | 83.10 | **100** |
| `0x82b9ace0` | 84 | `?EnableSlot@GemManager@@QAAXH@Z` | GemManager | 92.38 | 92.38 |
| `0x826f2b38` | 84 | `?ResetScoring@VocalPart@@QAAXXZ` | VocalPart | 89.52 | 89.52 |
| `0x82360bf8` | 80 | `?GetPlayerContributionString@TourPerformerImpl@@QAA?AVString@@PAVBandUser@@@Z` | TourPerformer | 93.25 | **100** |
| `0x826f2580` | 80 | `?AfterPoll@VocalPart@@QAAXM@Z` | VocalPart | 89.00 | **100** |
| `0x8268b7e8` | 80 | `?Reset@RemoteBandUser@@UAAXXZ` | BandUser | 100.00 | **100** |
| `0x82671030` | 80 | `?ComponentStateOverride@AssetProvider@@UBA?AW4State@UIComponent@@HHW423@@Z` | AssetProvider | 89.00 | 89.00 |
| `0x825c5d70` | 76 | `?SetLinkingCode@AppLabel@@QAAXPBD@Z` | AppLabel | 84.21 | **100** |
| `0x825f7968` | 76 | `?HasLeaderboard@AccomplishmentPanel@@QBA_NXZ` | AccomplishmentPanel | 94.42 | **100** |
| `0x825f7a20` | 76 | `?GetCurrentShouldShowDenominator@AccomplishmentPanel@@QBA_NXZ` | AccomplishmentPanel | 94.16 | 99.74 |
| `0x8256bbe0` | 72 | `??1CharCache@@UAA@XZ` | CharCache | 82.72 | **100** |
| `0x8257d480` | 72 | `?TotalStars@MetaPerformer@@QBAH_N@Z` | MetaPerformer | 98.78 | **100** |
| `0x82572b80` | 72 | `?OnMsg@MetaPanel@@QAA?AVDataNode@@ABVCurrentScreenChangedMsg@@@Z` | MetaPanel | 93.56 | **100** |
| `0x823624a8` | 64 | `?GetNumSongsForCurrentGig@TourProgress@@QBAHXZ` | TourProgress | 87.50 | **100** |
| `0x82575628` | 64 | `?IsRestricted@BandSongMgr@@QBA_NH@Z` | BandSongMgr | 71.25 | **100** |
| `0x824f6e68` | 60 | `?ForceLogout@RockCentral@@QAAXXZ` | RockCentral | 100.00 | **100** |
| `0x82686460` | 60 | `?GetPitchOffsetForTick@SongDB@@QBAMH@Z` | SongDB | 75.33 | **100** |
| `0x826f0f08` | 16 | `?Loop@RGTutor@@QAAXXZ` | RGTutor | 100.00 | **100** |
| `0x8269cd18` | 12 | `?GetSongNumVocalParts@Performer@@QBAHXZ` | Performer | 100.00 | **100** |
| `0x82b98098` | 8 | `?GetTrackNum@Track@@QBAHXZ` | Track | 100.00 | **100** |
| `0x82b98dd8` | 8 | `?ResetSmashers@GemManager@@QAAX_N@Z` | GemManager | 100.00 | **100** |

## 5. Left, and where the rest is

- **Blocked only by an ICF fold-survivor callee name.** Closing these needs a proven
  alias, not source work:
  - `UpdateShifts`: `_M_erase`/`push_back` survivors
  - `AddAccomplishment`: `CampaignLevel::GetAward` vs `TourDesc::GetTourSilverGoal`
  - `~NetSession`: `StillDeleting` vs `CheatsInitialized`
  - `ConfigureTourPropertyData`: `_M_find`
  - `PostHandleRemoveUser`: `remove<Track**>` vs `remove<BandUser**>`
  - `SetDoneArbitrating`: `__find<const int*>`
  - `TourDescProvider::UpdateExtendedText`
- **Needs `src/system/` (out of scope).**
  - `PropSync(MatSwap)` 94.8 and `??0OutfitConfig` 90.4: the source is
    `src/system/bandobj/OutfitConfig.cpp`.
  - `~Track` 88.5: the remaining own-vtable store is `TrackInterface`'s empty dtor in
    `src/system/bandobj/TrackInterface.h`. Track is its only subclass.
- **Layout.**
  - `??0MetaPanel` 95.3: dev-only `HAQManager`; retail allocates 0xa8, we allocate 0xac.
  - `AttemptSwapUserProfile` 73.8: needs a user vector at +0x30 of
    `OvershellProfileProvider` and the real X360 swap body at `0x825DF9C8`.
  - `??0ProTrainerPanel` 97.4: the vtordisp slot value.
- **Register or scheduling only**, after about three ideas each:
  - `DrawFill` 87.6
  - `SetupDetailLine` 95.0
  - `SetChar` 98.8: `lastdata`'s register; hoisting it scored 96.65
  - `DeterminePerformanceAwards` 97.6: an extra Symbol stack copy
  - `SelectTour` 98.8: an r9/r10 swap; declaration order is inert
  - `Enable`/`DisableSlot`
  - `UpdateScoreTypeSpecificStats`
  - `AddNewRewardVignette`
  - `RemoveOldestCachedContent` 96.4
- **Pin and split problems found, not fixed.**
  - `0x82594058`/`0x82594070`: Accomplishment helpers sitting in the `UIEventMgr` pin.
  - A `Str.cpp` `.text` pin covers `0x82566978–0x82566998`, which are two
    ClosetMgr-range functions.
  - The `PerformerStatsInfo` copy ctor row is mis-carved. The real function runs to
    `0x8257C240`, and `fn_8257C238` holds its tail.
  - `0x824f6f80` `GetActiveContextHighWatermark` was **not named**. Naming it makes dtk
    merge the following 8-byte function (`fn_824F6FA0`, which scores 100 as a
    `IsDirPtr` stub) into it, and that merge was not verified.
- **Not attempted.** The matrix candidates that failed adjudication: 51 contradicted in
  round 1, 34 in round 2. Their callers name another function, or the name is bound
  elsewhere. Neither run of the matrix scored units whose rows have no candidate in our
  base obj at all.
- **Fork E stopped at its turn limit.** At that point it was investigating the
  Campaign pair that §2 resolves. One uncommitted experiment remains in
  `~/tmp/wt-w16-hk-e` and was not merged.
- **Main moved during the lane.** Main is now `84d332885`, 52 commits ahead. Its map
  changes conflict with this branch only **textually** (adjacent insertions). Checked:
  none of main's 144 new rows shares an address or a name with this branch's 178.
  `src/band3/meta_band/BandSongMgr.cpp` auto-merges.

## 6. Reproduce

```
python3 ~/tmp/w16hk/matrix2.py 400 <out.json> 12                     # matrix, units ranked 41+
python3 tools/anon_proposal_adjudicate.py <props.json> --json-out <out>   # identity check
python3 tools/gated_map_write.py --target scripts/target_symbol_map.json --rows-json <rows>
python3 tools/icf_alias_finder.py --validate && python3 tools/map_name_injectivity.py
python3 tools/ab_measure.py --worktree <clean wt at base + f806edfbd> --patch <branch diff minus symbols.txt>
```

Scratch inputs (not committed): `~/tmp/w16hk/` (proposals, adjudications, buckets,
`matrix_rest.json`, per-round score lists, `table.json`).
