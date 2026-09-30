# W16-HX — game-layer fuzzy sweep, 1–90% band (2026-09-30)

**Branch** `w16-hx`, based on main `b72a90b0e`. **Ruler** `name_check` (graded, read from
`report.json` `provenance.diff_config`).
**Population:** every *named* row in a `src/band3` / `src/network` unit with
`1 ≤ fuzzy_match_percent < 90`, excluding Quazal (source path or symbol name).
Ranked by `size × (100 − fuzzy)` off the branch base's own full build.
**172 rows, weight 1,326,540** at the start → **41 rows, weight 173,588** at the tip.

The work was split into seven sub-branches, `w16-hx-1` … `w16-hx-7`, each in its own worktree
with a disjoint set of units. Each fix was committed separately and verified with a full
`./tools/ninja-locked` build plus `report.json`, never with `run_objdiff` alone. Each sub-branch
landed on `w16-hx` with `git merge --no-ff`. There were no merge conflicts.

## 1. Whole-binary A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-hx-ab --patch <b72a90b0e..w16-hx, docs excluded>`
was run **once**, in a fresh worktree at main `b72a90b0e`, over the whole branch diff (97 files).
- objdiff-cli was pinned to one binary across both legs.
- Leg A settled at 0 recompiles.
- Leg B had 567 recompiles and 2 settle iterations.

```
leg A: matched=45960 masked=23607 honest=22353 code%=44.522045
leg B: matched=46102 masked=23648 honest=22454 code%=44.788890
Δmatched=+142  Δmasked_equal=+41  Δhonest=+101  Δcode%=+0.266845pp  Δcode_bytes=+27344
Δfuzzy=+0.108020pp   (legA 54.067350 -> legB 54.175370)
units at 100% [mpn]: 225 -> 230 (5 in, 0 out)   [all-rows-fuzzy]: 194 -> 197
```

**Prediction, written before the run:** Δfuzzy ≈ +0.108 pp, Δmatched_code ≈ +27,300 B,
Δmatched ≈ +142, and exactly two funclet rows down. The basis was an in-tree full-build diff of
the merged branch against an archived baseline `report.json`. **All four held:** the A/B read
+0.108020 pp, +27,344 B, +142, and the same two rows down.

**Row-level diff of the two archived leg reports:** 189 rows up, **2 down**, 0 gone, 0 new.

### 1.1 The two rows that went down (both 40-byte EH funclets paired by byte signature)

| row | before | after | why it is not a code regression |
|---|---:|---:|---|
| `default/MessageTimer fn_8242C690` | 99.5 | 93.5 | `MessageTimer.cpp` scatter-includes `GemPlayer.cpp`, so this unit contains GemPlayer's funclets. Retail's funclet is `addi r3,r31,0x50; bl __destroy_aux<Key<ObjectStage>>`. The old 99.5 was a **false pairing**: our wrong `SetProperty(pitch_ratio, …)` path in `GemPlayer::SetPitchShiftRatio` produced a DataNode-cleanup funclet with the same byte signature. Retail calls an out-of-line `FxSendPitchShift::SetRatio`. The fix takes `SetPitchShiftRatio` from 76.95 to 99.96 and removes that funclet. Getting the 99.5 back would mean putting the wrong callee back. |
| `default/network/net/NetSession fn_823E6D60` | 100 | 99.5 | A **zero-sum swap**: `fn_823E5298` went 99.5 → 100 in the same build. Both are `masked_equal` byte-signature pairings at canonical 100, both call retail's folded `??1Message`, and only one of them can take that name. The swap was triggered by `ProcessUserLeftMsg` (76.56 → 100). |

The done-condition asks for "no regressed rows". These two are the only exceptions, and neither is a
code defect. I kept both rather than reintroduce a wrong callee. **Accuracy over headline.**

## 2. Rows, before → after

Before is the branch-base full build, which is byte-identical in measures to A/B leg A. After is the merged branch tip, which is byte-identical in measures to leg B. The values are `fuzzy_match_percent` from `report.json`.

### 2a. The in-band population (172 rows, ranked by size x (100 - fuzzy) at the baseline)

| row | unit | B | before | after |
|---|---|---:|---:|---:|
| `?CanBeLaunched@Accomplishment@@UBA_NXZ` | Accomplishment | 776 | 19.90 | **100** |
| `?BuildSongList@SongSort@@QAAXXZ` | band3/meta_band/SongSort | 1,020 | 51.67 | 99.96 |
| `?SetupGems@GemManager@@QAAXH@Z` | GemManager | 3,404 | 87.77 | 97.56 |
| `?UpdateVerts@Tail@@QAAXM_N@Z` | Tail | 1,064 | 65.67 | 96.80 |
| `?DrawTrackMasks@GemManager@@QAAXHH@Z` | GemManager | 1,600 | 80.29 | 98.77 |
| `??0NetworkEmulator@@QAA@XZ` | NetworkEmulator | 544 | 48.17 | 98.40 |
| `?GetNextLyricPlate@VocalTrack@@QAAPAVLyricPlate@@AAV?$deque@PAVLyricPlate@@V?$StlNodeAl...` | VocalTrack | 516 | 54.56 | **100** |
| `??0Entry@LocalePanel@@QAA@ABU01@@Z` | GemManager | 308 | 24.21 | 24.21 |
| `?BuildSetlistList@SetlistSort@@QAAXXZ` | band3/meta_band/SongSort | 512 | 54.85 | 99.92 |
| `?UpdateExtendedCustom@TourDescProvider@@UBAXHHPAVObject@Hmx@@@Z` | TourDescPanel | 1,944 | 88.67 | **100** |
| `?Text@MusicLibrary@@UBAXHHPAVUIListLabel@@PAVUILabel@@@Z` | MusicLibrary | 1,292 | 83.64 | 94.21 |
| `?Mat@TourDescProvider@@UBAPAVRndMat@@HHPAVUIListMesh@@@Z` | TourDescPanel | 616 | 65.99 | **100** |
| `?DrawFill@GemTrack@@QAAXPAVFillInfo@@HH@Z` | GemTrack | 1,652 | 87.56 | 87.56 |
| `?InqConditionProgress@AccomplishmentTourConditional@@QBA_NPAVBandProfile@@ABUAccomplish...` | band3/meta_band/AccomplishmentTourConditional | 500 | 61.02 | **100** |
| `?SyncAvailableSongs@RockCentral@@QAAXABV?$vector@PAVBandProfile@@V?$StlNodeAlloc@PAVBan...` | RockCentral | 944 | 80.19 | 99.96 |
| `?SongAudioData@BandSongMgr@@UBAPAVSongInfo@@H@Z` | BandSongMgr | 444 | 58.77 | 99.95 |
| `?Enter@PracticePanel@@UAAXXZ` | PracticePanel | 432 | 57.73 | **100** |
| `?LoadFixed@SongStatusMgr@@UAAXAAVFixedSizeSaveableStream@@H@Z` | SongStatusMgr | 232 | 29.59 | 99.91 |
| `?DoesOfferMatchFilter@SongSortMgr@@QBA_NPAVStoreOffer@@PBVSongFilter@1@VSymbol@@@Z` | SongSortMgr | 1,416 | 88.65 | 99.41 |
| `?LoadGlobalOptions@ProfileMgr@@QAAXAAVFixedSizeSaveableStream@@@Z` | ProfileMgr | 480 | 67.25 | **100** |
| `?Configure@AccomplishmentGroup@@UAAXPAVDataArray@@@Z` | band3/meta_band/AccomplishmentGroup | 420 | 64.83 | **100** |
| `?AllowContentToBeAdded@BandSongMgr@@UAA_NPAVDataArray@@W4ContentLocT@@@Z` | BandSongMgr | 332 | 55.63 | **100** |
| `?UnpackFloats@VocalPlayer@@QBAXHMMAAV?$vector@MV?$StlNodeAlloc@M@stlpmtx_std@@@stlpmtx_...` | VocalPlayer | 228 | 36.75 | **100** |
| `?ShowData@Leaderboard@@QAAXXZ` | Leaderboard | 1,348 | 89.31 | 99.96 |
| `?Handle@MultiSelectListPanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | band3/meta_band/MultiSelectListPanel | 776 | 81.87 | **100** |
| `??0MetaPerformer@@QAA@ABVBandSongMgr@@PBD@Z` | MetaPerformer | 820 | 83.69 | **100** |
| `??0VocalTrack@@QAA@PAVBandUser@@@Z` | VocalTrack | 868 | 84.70 | 99.82 |
| `?SaveAndUploadScores@MetaPerformer@@QAAXAAV?$vector@PAVLocalBandUser@@V?$StlNodeAlloc@P...` | MetaPerformer | 684 | 80.69 | 98.36 |
| `?OnMsg@MultiSelectListPanel@@QAA?AVDataNode@@ABVUIComponentSelectMsg@@@Z` | band3/meta_band/MultiSelectListPanel | 256 | 49.58 | **100** |
| `?ConfigureTrackerSpecificData@FocusTracker@@UAAXPBVDataArray@@@Z` | FocusTracker | 172 | 28.09 | **100** |
| `?OnMsg@MultiSelectListPanel@@QAA?AVDataNode@@ABVUIComponentScrollMsg@@@Z` | band3/meta_band/MultiSelectListPanel | 308 | 60.08 | **100** |
| `?Exiting@NextSongPanel@@UBA_NXZ` | NextSongPanel | 152 | 20.63 | **100** |
| `?PollLyricAnimations@VocalTrack@@QAAXAAV?$deque@PAVLyricPlate@@V?$StlNodeAlloc@PAVLyric...` | VocalTrack | 744 | 84.26 | 84.26 |
| `??0LyricPlate@@QAA@PAVRndText@@PBV1@1@Z` | band3/bandtrack/Lyric | 284 | 60.55 | 99.93 |
| `?SetupRealGuitarFretPos@GemManager@@QAAXXZ` | GemManager | 680 | 83.54 | 99.99 |
| `?SetPitchShiftRatio@GemPlayer@@QAAXM@Z` | GemPlayer | 468 | 76.95 | 99.96 |
| `?OnMsg@BandProfile@@QAA?AVDataNode@@ABVRockCentralOpCompleteMsg@@@Z` | band3/meta_band/BandProfile | 924 | 88.39 | **100** |
| `?GetSetlistType@QuestFilterProvider@@QBA?AW4TourSetlistType@@H@Z` | QuestFilterPanel | 240 | 55.95 | **100** |
| `?ConfigureCampaignLevelData@Campaign@@QAAXPAVDataArray@@@Z` | Campaign | 420 | 75.00 | 99.86 |
| `?UpdateLabel@CalibrationPanel@@QAAXXZ` | CalibrationPanel | 804 | 86.97 | **100** |
| `?UpdateConditionOptionalData@AccomplishmentTourConditional@@QAAXAAUAccomplishmentTourCo...` | band3/meta_band/AccomplishmentTourConditional | 288 | 63.85 | **100** |
| `??0ViewSettingsProvider@@QAA@XZ` | band3/meta_band/ViewSetting | 620 | 83.26 | 99.77 |
| `?SyncLoad@BandMachine@@QAAXAAVBinStream@@E@Z` | BandMachine | 340 | 69.61 | 99.88 |
| `?FakeProfileFill@ProfileMgr@@QAAXXZ` | ProfileMgr | 136 | 27.88 | 27.88 |
| `?UpdateTambourineGems@VocalTrack@@QAAXXZ` | VocalTrack | 744 | 87.22 | 87.22 |
| `?HasPart@BandSongMetadata@@QBA_NVSymbol@@@Z` | BandSongMetadata | 340 | 72.47 | 72.47 |
| `?SetOverdriveEffectEnable@GameMicManager@@QAAX_N@Z` | GameMicManager | 192 | 53.38 | **100** |
| `?Poll@VocalTrainerPanel@@UAAXXZ` | band3/game/VocalTrainerPanel | 548 | 83.88 | **100** |
| `?Initialize@PerformanceData@@QAAXABVStats@@HW4ScoreType@@W4Difficulty@@VSymbol@@HH_N@Z` | PerformanceData | 208 | 58.29 | **100** |
| `?GetSongToTaskMgrMs@ProfileMgr@@QBAMW4LagContext@@@Z` | ProfileMgr | 92 | 9.57 | 9.57 |
| `?SymToControllerType@@YA?AW4ControllerType@@VSymbol@@@Z` | Defines | 108 | 23.48 | 23.48 |
| `?CheckJoinable@NetSession@@QAA_NAAW4JoinResponseError@@AAHV?$vector@VUserGuid@@V?$StlNo...` | network/net/NetSession | 488 | 83.11 | 99.96 |
| `?SetupPhrasesForTrack@SongDB@@QAAXHAAV?$vector@VExtent@@V?$StlNodeAlloc@VExtent@@@stlpm...` | SongDB | 396 | 80.00 | **100** |
| `?ReadCachedMetadataFromStream@BandSongMgr@@UAAXAAVBinStream@@H@Z` | BandSongMgr | 400 | 80.30 | 99.78 |
| `?SetFromSongSelectNode@AppLabel@@QAAXPBVNode@@@Z` | AppLabel | 236 | 66.75 | **100** |
| `?SetLeaderboardRankAndName@AppLabel@@QAAXABVLeaderboardRow@@@Z` | AppLabel | 184 | 57.83 | 90.11 |
| `?ProcessUserLeftMsg@NetSession@@QAAXABVUserLeftMsg@@@Z` | network/net/NetSession | 328 | 76.56 | **100** |
| `?TambourineSwing@TambourineManager@@QAAHH@Z` | band3/game/TambourineManager | 288 | 73.75 | 92.62 |
| `?GetValidSongs@BandSongMgr@@QBAHABV?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_st...` | BandSongMgr | 508 | 85.16 | 96.06 |
| `?AllowedToAccessContent@@YA_NH@Z` | band3/meta_band/SongSort | 88 | 16.14 | 16.14 |
| `?Dispatch@TourMostStarsMsg@@UAAXXZ` | NetGameMsgs | 192 | 61.85 | **100** |
| `?Init@Tour@@QAAXPAVDataArray@@@Z` | Tour | 308 | 76.34 | **100** |
| `?InitializeDiscSongs@AccomplishmentManager@@QAAXXZ` | band3/meta_band/AccomplishmentManager | 412 | 82.40 | **100** |
| `?IteratorAt@?$TickedInfoCollection@M@@QBAPBV?$TickedInfo@M@@H_N@Z` | SongDB | 88 | 17.86 | **100** |
| `??0LeaderboardRow@@QAA@ABV0@@Z` | Leaderboard | 172 | 58.72 | **100** |
| `?OnMsg@VocalPlayer@@QAA_NABVButtonUpMsg@@@Z` | VocalPlayer | 244 | 71.75 | **100** |
| `?UpdateSongStatusFlagsForPerformer@AccomplishmentManager@@QAAXPAVPerformer@@PAVSongStat...` | band3/meta_band/AccomplishmentManager | 340 | 79.94 | **100** |
| `?GetAccomplishmentName@AccomplishmentPanel@@QAA?AVSymbol@@XZ` | AccomplishmentPanel | 128 | 47.09 | 90.75 |
| `?BeginRockCentralOps@EntityUploader@@QAAHH@Z` | EntityUploader | 412 | 84.00 | **100** |
| `?DynamicAddBeatmatch@GemPlayer@@UAAXXZ` | GemPlayer | 136 | 52.18 | **100** |
| `??1TexLoadPanel@@UAA@XZ` | TexLoadPanel | 112 | 42.25 | **100** |
| `?HandleInExtent@PerfectSectionTracker@@QAAXMH@Z` | band3/game/PerfectSectionTracker | 500 | 87.13 | **100** |
| `?CombinePartSymbols@Tour@@QAA?AVSymbol@@V2@0@Z` | Tour | 172 | 62.67 | **100** |
| `?CalcPhraseScoreMax@VocalPart@@QBAMABQBVVocalPhrase@@@Z` | VocalPart | 192 | 66.85 | 76.75 |
| `?FillsEnabled@GemPlayer@@UAA_NH@Z` | GemPlayer | 132 | 52.79 | **100** |
| `?Exit@PracticePanel@@UAAXXZ` | PracticePanel | 192 | 67.67 | **100** |
| `??0BandUser@@QAA@XZ` | BandUser | 304 | 79.87 | **100** |
| `?Configure@AccomplishmentSongListConditional@@QAAXPAVDataArray@@@Z` | band3/meta_band/AccomplishmentSongListConditional | 196 | 69.24 | **100** |
| `?PushSortToScreen@MusicLibrary@@QAAXXZ` | MusicLibrary | 244 | 75.34 | **100** |
| `?_M_insert_overflow_aux@?$vector@VUpcomingFretRelease@GemPlayer@@V?$StlNodeAlloc@VUpcom...` | GemPlayer | 404 | 85.21 | 85.21 |
| `?AttemptSwapUserProfile@OvershellSlot@@QAAXH@Z` | OvershellSlot | 228 | 73.84 | **100** |
| `?SetupPracticeSections@SongDB@@QAAXXZ` | SongDB | 576 | 89.78 | **100** |
| `?HandleSetlistCompletedForUser@AccomplishmentManager@@QAAXVSymbol@@_NPAVLocalBandUser@@...` | band3/meta_band/AccomplishmentManager | 456 | 87.31 | **100** |
| `?Text@AssetProvider@@UBAXHHPAVUIListLabel@@PAVUILabel@@@Z` | band3/meta_band/AssetProvider | 392 | 85.39 | **100** |
| `??1ProfileMgr@@UAA@XZ` | ProfileMgr | 180 | 68.64 | **100** |
| `?EnableDrumFills@GemPlayer@@UAAX_N@Z` | GemPlayer | 156 | 64.36 | **100** |
| `?GetNumPhrases@VocalPlayer@@QAAMHHH@Z` | VocalPlayer | 420 | 86.80 | **100** |
| `??$PropSync@VPiercing@OutfitConfig@@@@YA_NAAV?$ObjVector@VPiercing@OutfitConfig@@@@AAVD...` | band3/bandtrack/Gem | 392 | 86.17 | 86.17 |
| `?CheckAwesomesCondition@AccomplishmentSongConditional@@QBA_NPAVSongStatusMgr@@VSymbol@@...` | AccomplishmentSongConditional | 128 | 58.75 | **100** |
| `?CheckDoubleAwesomesCondition@AccomplishmentSongConditional@@QBA_NPAVSongStatusMgr@@VSy...` | AccomplishmentSongConditional | 128 | 58.75 | **100** |
| `?CheckTripleAwesomesCondition@AccomplishmentSongConditional@@QBA_NPAVSongStatusMgr@@VSy...` | AccomplishmentSongConditional | 128 | 58.75 | **100** |
| `?AddSavedSetlist@BandProfile@@QAAPAVLocalSavedSetlist@@PBD0_NABVPatchDescriptor@@ABV?$v...` | band3/meta_band/BandProfile | 268 | 80.55 | 99.93 |
| `?AddSongData@BandSongMgr@@UAAXPAVDataArray@@PAVDataLoader@@W4ContentLocT@@@Z` | BandSongMgr | 152 | 66.08 | 66.08 |
| `?GetRankedSongs@BandSongMgr@@QBAXAAV?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_s...` | BandSongMgr | 320 | 84.10 | **100** |
| `?GetBandFailCue@SongDB@@QBAXAAVString@@@Z` | SongDB | 140 | 63.94 | 99.86 |
| `??0VerifyBuildVersionMsg@@QAA@XZ` | band3/meta_band/MetaNetMsgs | 136 | 65.44 | **100** |
| `?ChangeDifficulty@GemPlayer@@UAAXW4Difficulty@@@Z` | GemPlayer | 352 | 86.70 | 96.53 |
| `??0SongMgr@@QAA@XZ` | BandSongMgr | 260 | 82.20 | 99.62 |
| `?UpdateUserData@NetSession@@QAAXPAVUser@@I@Z` | network/net/NetSession | 192 | 76.15 | **100** |
| `?MovePatch@CustomizePanel@@QAAXMM@Z` | CustomizePanel | 264 | 82.98 | 89.70 |
| `?HookUpFxForMicId@GameMicManager@@QAAXPAVGameMic@@@Z` | GameMicManager | 208 | 78.85 | 99.90 |
| `?StartArbitration@NetSession@@QAAXXZ` | network/net/NetSession | 276 | 84.06 | 92.32 |
| `??1OpenGateData@?A0xb0de99ba@@UAA@XZ` | WaitingUserGate | 124 | 64.52 | **100** |
| `?SetTrack@VocalPlayer@@UAAXH@Z` | VocalPlayer | 120 | 63.50 | **100** |
| `?GetNumStarsForGig@TourProgress@@QBAHH@Z` | TourProgress | 48 | 10.67 | 10.67 |
| `?LocalSetEnabledState@VocalPlayer@@UAAXW4EnabledState@@HPAVBandUser@@_N@Z` | VocalPlayer | 292 | 85.32 | **100** |
| `?OnPhraseComplete@VocalTrack@@QAAXMMH@Z` | VocalTrack | 120 | 65.87 | **100** |
| `?PushSetlistToScreen@MusicLibrary@@QAAXXZ` | MusicLibrary | 216 | 81.76 | **100** |
| `??0AppScoreDisplay@@QAA@XZ` | MetaPanel | 288 | 86.86 | **100** |
| `?OfferType@StoreOffer@@QBA?AVSymbol@@XZ` | RockCentral | 100 | 63.40 | **100** |
| `?OnMsg@SongStatusMgr@@QAA?AVDataNode@@ABVRockCentralOpCompleteMsg@@@Z` | SongStatusMgr | 152 | 77.34 | **100** |
| `?GetShouldAutosaveProfiles@ProfileMgr@@QAA?AV?$vector@PAVBandProfile@@V?$StlNodeAlloc@P...` | ProfileMgr | 208 | 83.58 | **100** |
| `??0TrackPanelInterface@@QAA@XZ` | band3/bandtrack/TrackPanel | 140 | 76.03 | **100** |
| `?HandleLegendLefty@ChordbookPanel@@QAAX_N@Z` | ChordbookPanel | 316 | 89.49 | **100** |
| `??0RGTutor@@QAA@XZ` | band3/game/RGTutor | 64 | 56.25 | 56.25 |
| `?OnMsg@Campaign@@QAA?AVDataNode@@ABVProfileSwappedMsg@@@Z` | Campaign | 196 | 86.33 | 86.33 |
| `??_GSetUserDifficultyMsg@@UAAPAXI@Z` | MusicLibrary | 76 | 65.53 | 65.53 |
| `?CanEndGame@GameConfig@@QBA_NXZ` | GameConfig | 152 | 82.76 | **100** |
| `?CreateOrAccessSongStatus@SongStatusMgr@@QBAPAVSongStatus@@H@Z` | SongStatusMgr | 176 | 85.30 | **100** |
| `??0?$pair@$$CBVSymbol@@VSongRecord@@@stlpmtx_std@@QAA@ABU01@@Z` | UploadErrorMgr | 64 | 61.56 | 61.56 |
| `?SetCurrentCharacterPatch@ClosetMgr@@QAAXW4Category@Patch@BandCharDesc@@PBD@Z` | band3/meta_band/ClosetMgr | 92 | 73.70 | **100** |
| `?GetBattleEndTimeStr@RockCentral@@QAAPBDW4BattleTimeUnits@@@Z` | RockCentral | 76 | 68.42 | 68.42 |
| `?Save@OpenGateData@?A0xb0de99ba@@QBAXAAVBinStream@@@Z` | WaitingUserGate | 160 | 85.00 | 86.50 |
| `?SetPaused@VocalPlayer@@UAAX_N@Z` | VocalPlayer | 232 | 89.71 | 99.83 |
| `?HandlePendingGamerRewards@AccomplishmentProgress@@QAAXXZ` | band3/meta_band/AccomplishmentProgress | 176 | 86.43 | 86.43 |
| `?CountGemsInSong@TrackerUtils@@SAHHW4TrackType@@@Z` | TrackerUtils | 208 | 88.79 | **100** |
| `??$SaveStdPtr@VSongStatus@@@FixedSizeSaveable@@SAXAAVFixedSizeSaveableStream@@ABV?$hash...` | SongStatusMgr | 152 | 86.13 | **100** |
| `?CurrentDialogEvent@UIEventMgr@@QBA?AVSymbol@@XZ` | UIEventMgr | 128 | 84.16 | **100** |
| `?IsHidden@ModifierMgr@@UBA_NH@Z` | band3/meta_band/ModifierMgr | 28 | 28.57 | 28.57 |
| `?IsActive@ModifierMgr@@UBA_NH@Z` | band3/meta_band/ModifierMgr | 28 | 28.57 | 28.57 |
| `?GetNextCampaignLevel@Campaign@@QBA?AVSymbol@@V2@@Z` | Campaign | 140 | 86.11 | **100** |
| `?HasRows@AppMiniLeaderboardDisplay@@QAA_NXZ` | band3/meta_band/AppMiniLeaderboardDisplay | 80 | 76.45 | **100** |
| `?_M_Start@?$_String_base@DV?$allocator@D@stlpmtx_std@@@stlpmtx_std@@IBAPBDXZ` | Accomplishment | 24 | 22.17 | 22.17 |
| `?SaveSize@PerformanceData@@SAHH@Z` | PerformanceData | 52 | 64.62 | 64.62 |
| `?SetProfileName@AppLabel@@QAAXPBVLocalBandUser@@@Z` | AppLabel | 96 | 82.71 | **100** |
| `?HandleProfileLoadComplete@ProfileMgr@@QAAXXZ` | ProfileMgr | 68 | 75.88 | **100** |
| `?GetIconArt@Accomplishment@@QBAPBDXZ` | Accomplishment | 100 | 84.00 | **100** |
| `?GetLongestStreak@Band@@QBAHXZ` | band3/game/Band | 84 | 82.57 | **100** |
| `?PostLoad@SongDB@@QAAXPAVDataEventList@@@Z` | SongDB | 88 | 84.36 | 84.36 |
| `?Terminate@UIEventMgr@@SAXXZ` | UIEventMgr | 80 | 83.45 | 83.45 |
| `?GetNumVocalParts@BandSongMgr@@QBAHVSymbol@@@Z` | BandSongMgr | 88 | 85.00 | **100** |
| `?SetEnable@@YA?AVDataNode@@PAVDataArray@@@Z` | GameMicManager | 112 | 88.21 | 88.21 |
| `?GetQuest@Tour@@QAAPAVQuest@@XZ` | Tour | 68 | 81.71 | **100** |
| `??1SetlistSubmissionMsg@@UAA@XZ` | SetlistMergePanel | 104 | 88.04 | 88.04 |
| `??1AddUserRequestMsg@@UAA@XZ` | network/net/NetSession | 84 | 85.19 | **100** |
| `??1MusicLibraryTaskMsg@@UAA@XZ` | Tour | 80 | 84.45 | **100** |
| `??1NewUserMsg@@UAA@XZ` | network/net/NetSession | 76 | 83.63 | **100** |
| `??1DataArrayMsg@@UAA@XZ` | network/net/NetSession | 76 | 83.63 | **100** |
| `??1SyncMachineMsg@?A0x5fd33732@@UAA@XZ` | BandMachineMgr | 76 | 83.63 | **100** |
| `??1LockResponseMsg@@UAA@XZ` | band3/meta_band/LockStepMgr | 76 | 83.63 | **100** |
| `??1EndLockMsg@@UAA@XZ` | band3/meta_band/LockStepMgr | 76 | 83.63 | **100** |
| `??1LocationCmp@@UAA@XZ` | band3/meta_band/SetlistSortByLocation | 76 | 83.63 | **100** |
| `??1UIStats@@UAA@XZ` | band3/meta_band/UIStats | 88 | 85.86 | **100** |
| `?GetLaunchUser@Campaign@@QBAPAVLocalBandUser@@XZ` | Campaign | 68 | 82.06 | 82.06 |
| `?IsModifierActive@ModifierMgr@@QBA_NVSymbol@@@Z` | band3/meta_band/ModifierMgr | 60 | 79.67 | 79.67 |
| `?ClearGems@GemTrainerPanel@@QAAXXZ` | band3/game/GemTrainerPanel | 108 | 88.89 | **100** |
| `?PostSave@Object@Hmx@@UAAXAAVBinStream@@@Z` | TexLoadPanel | 16 | 25.00 | 25.00 |
| `??1Track@@UAA@XZ` | GemTrack | 104 | 88.46 | **100** |
| `?AssetProviderHasAsset@CustomizePanel@@QAA_NVSymbol@@@Z` | CustomizePanel | 116 | 89.66 | **100** |
| `?VertLess@@YA_NABVVert@RndMesh@@0@Z` | band3/bandtrack/Gem | 64 | 85.94 | **100** |
| `?ComponentStateOverride@AssetProvider@@UBA?AW4State@UIComponent@@HHW423@@Z` | band3/meta_band/AssetProvider | 80 | 89.00 | 99.50 |
| `?HasContentAltDirs@BandSongMgr@@UAA_NXZ` | BandSongMgr | 32 | 72.50 | **100** |
| `?ResetScoring@VocalPart@@QAAXXZ` | VocalPart | 84 | 89.52 | **100** |
| `?GetNumPlayers@TrackPanel@@UBAHXZ` | band3/bandtrack/TrackPanel | 12 | 40.00 | 40.00 |
| `?GetGlobalOptionsSize@ProfileMgr@@QAAHXZ` | ProfileMgr | 48 | 86.17 | **100** |
| `??0?$reverse_iterator@PAPAVLocalSavedSetlist@@@stlpmtx_std@@QAA@ABV01@@Z` | RockCentral | 12 | 60.00 | 60.00 |
| `??0?$reverse_iterator@PAD@stlpmtx_std@@QAA@ABV01@@Z` | RockCentral | 12 | 60.00 | 60.00 |
| `?SetlistSize@MusicLibrary@@QAAHXZ` | MusicLibrary | 16 | 75.00 | 75.00 |
| `?SetInCoda@GemTrack@@QAAX_N@Z` | GemTrack | 16 | 75.00 | 75.00 |
| `?GapSize@UIListProvider@@UBAMHHHH@Z` | MusicLibrary | 12 | 80.00 | 80.00 |
| `?Main@ObjectDir@@SAPAV1@XZ` | Matchmaker | 12 | 80.00 | 80.00 |
| `?StartFrame@RndAnimatable@@UAAMXZ` | Accomplishment | 12 | 80.00 | 80.00 |

In-band: 134 of 172 raised, 97 to 100, 38 unchanged.

### 2b. Rows outside the band that moved (57)

| row | unit | B | before | after |
|---|---|---:|---:|---:|
| `fn_8242C690` | MessageTimer | 40 | 99.50 | 93.50 |
| `fn_823E6D60` | network/net/NetSession | 40 | 100.00 | 99.50 |
| `fn_82BA45EC` | VocalTrack | 40 | 99.90 | **100** |
| `fn_825D33B8` | SongStatusMgr | 40 | 99.90 | **100** |
| `fn_825D3530` | SongStatusMgr | 40 | 99.90 | **100** |
| `fn_8255DDCC` | band3/meta_band/AccomplishmentManager | 40 | 99.90 | **100** |
| `fn_825C0A54` | band3/meta_band/SongSort | 40 | 99.90 | **100** |
| `fn_825C0ABC` | band3/meta_band/SongSort | 40 | 99.90 | **100** |
| `fn_825C0B44` | band3/meta_band/SongSort | 40 | 99.90 | **100** |
| `fn_825C0BAC` | band3/meta_band/SongSort | 40 | 99.90 | **100** |
| `fn_825C0E20` | band3/meta_band/SongSort | 40 | 99.90 | **100** |
| `fn_825C0E88` | band3/meta_band/SongSort | 40 | 99.90 | **100** |
| `fn_825EAB1C` | band3/meta_band/AccomplishmentGroup | 40 | 99.90 | **100** |
| `fn_825EAB64` | band3/meta_band/AccomplishmentGroup | 40 | 99.90 | **100** |
| `fn_8258BEA4` | band3/meta_band/BandProfile | 40 | 99.90 | **100** |
| `fn_8258BECC` | band3/meta_band/BandProfile | 40 | 99.90 | **100** |
| `fn_8258BF14` | band3/meta_band/BandProfile | 40 | 99.90 | **100** |
| `fn_8258C40C` | band3/meta_band/BandProfile | 40 | 99.90 | **100** |
| `fn_826C2954` | GemPlayer | 40 | 99.90 | **100** |
| `fn_8266E484` | Leaderboard | 40 | 99.90 | **100** |
| `fn_82B9C3C0` | GemManager | 40 | 99.90 | **100** |
| `fn_82627958` | band3/meta_band/MultiSelectListPanel | 40 | 99.90 | **100** |
| `fn_82627980` | band3/meta_band/MultiSelectListPanel | 40 | 99.90 | **100** |
| `fn_826279A8` | band3/meta_band/MultiSelectListPanel | 40 | 99.90 | **100** |
| `fn_826279D0` | band3/meta_band/MultiSelectListPanel | 40 | 99.90 | **100** |
| `fn_826279F8` | band3/meta_band/MultiSelectListPanel | 40 | 99.90 | **100** |
| `fn_82627A20` | band3/meta_band/MultiSelectListPanel | 40 | 99.90 | **100** |
| `fn_8266E4AC` | Leaderboard | 40 | 99.30 | 99.50 |
| `fn_8253DCFC` | MusicLibrary | 40 | 99.50 | **100** |
| `fn_82B7DEE8` | TourDescPanel | 40 | 99.50 | **100** |
| `fn_82B7DF10` | TourDescPanel | 40 | 99.50 | **100** |
| `fn_82B7DF38` | TourDescPanel | 40 | 99.50 | **100** |
| `fn_82B7DF60` | TourDescPanel | 40 | 99.50 | **100** |
| `fn_822AEBB8` | BandSwatch | 40 | 99.50 | **100** |
| `fn_825D66C4` | band3/meta_band/ViewSetting | 40 | 99.50 | **100** |
| `fn_82681730` | GameMicManager | 40 | 99.50 | **100** |
| `fn_823E5298` | network/net/NetSession | 40 | 99.50 | **100** |
| `fn_826E7A64` | VocalPlayer | 40 | 99.50 | **100** |
| `?DoesSongMatchFilter@SongSortMgr@@QBA_NHPBVSongFilter@1@VSymbol@@@Z` | SongSortMgr | 568 | 96.99 | 98.56 |
| `?RebuildHUD@VocalTrack@@QAAXXZ` | VocalTrack | 2,188 | 90.48 | 93.91 |
| `?OnSelectRow@PlayerLeaderboard@@UAA?AVSymbol@@HPAVBandUser@@@Z` | PlayerLeaderboards | 436 | 92.70 | 99.17 |
| `fn_8268ABB0` | BandUser | 44 | 93.45 | **100** |
| `fn_825C0AE4` | band3/meta_band/SongSort | 32 | 92.50 | **100** |
| `fn_825C0B04` | band3/meta_band/SongSort | 32 | 92.50 | **100** |
| `fn_825C0B24` | band3/meta_band/SongSort | 32 | 92.50 | **100** |
| `fn_825C0B6C` | band3/meta_band/SongSort | 32 | 92.50 | **100** |
| `fn_825C0B8C` | band3/meta_band/SongSort | 32 | 92.50 | **100** |
| `fn_82627918` | band3/meta_band/MultiSelectListPanel | 32 | 92.50 | **100** |
| `fn_82627938` | band3/meta_band/MultiSelectListPanel | 32 | 92.50 | **100** |
| `fn_825A4488` | band3/meta_band/AccomplishmentTourConditional | 32 | 0.00 | **100** |
| `fn_825A46C4` | band3/meta_band/AccomplishmentTourConditional | 32 | 0.00 | **100** |
| `fn_825A46E4` | band3/meta_band/AccomplishmentTourConditional | 32 | 0.00 | **100** |
| `fn_825A4704` | band3/meta_band/AccomplishmentTourConditional | 32 | 0.00 | **100** |
| `fn_825E9BDC` | band3/meta_band/AccomplishmentSongListConditional | 32 | 0.00 | **100** |
| `fn_825EAADC` | band3/meta_band/AccomplishmentGroup | 32 | 0.00 | **100** |
| `fn_825EAAFC` | band3/meta_band/AccomplishmentGroup | 32 | 0.00 | **100** |
| `fn_825EAB44` | band3/meta_band/AccomplishmentGroup | 32 | 0.00 | **100** |

## 3. Patterns that paid

Each pattern below is listed with the rows it fixed. Most are the W16HE/HK patterns again.

- **Function-local `static Symbol`/`Message` at first use, in retail's guard-bit order.** This was
  still the single most common fix. Rows: `CanBeLaunched` (13 statics), `AccomplishmentGroup::Configure`,
  `FocusTracker::ConfigureTrackerSpecificData`, `SetOverdriveEffectEnable`, `VocalTrainerPanel::Poll`,
  `RockCentral::SyncAvailableSongs` (statics after the DataPoint), `BandProfile::OnMsg`,
  `TourMostStarsMsg::Dispatch`, `VerifyBuildVersionMsg`, `Tour::Init`/`CombinePartSymbols`,
  `QuestFilterProvider::GetSetlistType`, `MetaPerformer` ctor, and others.
  - Retail also builds statics it never reads. `InitializeDiscSongs` has `rb3`.
- **Implicit special members** (`patterns/fixable-declarations.md`). Implicit dtors: `~ProfileMgr`,
  `~TexLoadPanel`, `~MusicLibraryTaskMsg`, `~UIStats`, `~LocationCmp`, `~Track`, `OpenGateData`, and six
  net message dtors. Implicit ctors: `TrackPanelInterface`, `SongMgr`.
  - One counter-case, reverted: making `SetlistSubmissionMsg`'s dtor implicit takes it to 100, but the
    unit then stops emitting the inline `ByteCode` function, whose row falls from 100 to 0.
- **Out-of-line helpers that retail calls and the oracle inlines.** Each was rebuilt from retail bytes
  and given a name of our own:
  - `TourDescProvider::FindMat` (4 calls in `Mat`)
  - `Tail::WriteTailVerts` (`0x82BB0660`)
  - `FxSendPitchShift::SetRatio`
  - `VocalPlayer::IsNetOrSpoofed` / `GetVolumeParam`
  - `VocalTrackDir::DeactivateVolume`
  - `VocalTrack::BuildPhrase`
  - `Player::CountPause`
  - `GameMicManager::SetPitchCorrectionTarget`
  - the NetworkEmulator device-lookup helper (`fn_823EBC90`)

  Their retail rows are mostly **unmapped**, so the gain shows up only in the callers.
- **Compiled-out asserts whose side-effecting argument survives.** `MILO_ASSERT(cond)` is
  `((void)(cond))`, so a call inside it is still made. `VocalTrack::OnPhraseComplete` (65.87 → 100)
  needed `BuildPhrase`'s dead `GetVocalNoteList(0)`. A pure-inline assert argument goes the other way:
  it still perturbs register allocation, and `Campaign::GetNextCampaignLevel` reached 100 only once it
  was guarded native-only.
- **Loop and block shape.**
  - Early `continue` fixed the block order in `ConfigureCampaignLevelData`.
  - Retail re-reads the element instead of hoisting it (`EntityUploader::BeginRockCentralOps`).
  - Hoisting a vector reference fixed `Band::GetLongestStreak` and `CountGemsInSong`.
  - Materialised bool locals, one per flag test, fixed `UpdateSongStatusFlagsForPerformer`.
- **The oracle is wrong and retail bytes decide.** These are real behaviour fixes, listed in §4.

## 4. Behavioural fixes (the source did something retail does not)

- `SongSortMgr::DoesOfferMatchFilter`/`DoesSongMatchFilter`: filter types 4/7/8 now match vocal
  parts/length/rating, as retail's jump table does. They were matching the wrong attributes.
- `BandSongMgr::GetRankedSongs`: the demos-allowed flag was read and discarded. Demo songs are now
  skipped unless demos are allowed.
- `BandSongMgr::AllowContentToBeAdded`: now evicts before the root-location early return.
- `BandSongMgr::SongAudioData`: adds the TU5 content re-root, which is absent from the Wii oracle.
- `GemManager::DrawTrackMasks`: the arpeggio skip test was inverted.
- `GemManager::SetupGems`: a left-hand slide writes Gem `+0x24` (`mEnd`), not `mTailStart`. The Wii
  oracle has the same bug.
- `GemPlayer::EnableDrumFills`: also checks the TU5 Game `+0x47` bool.
- `NetworkEmulator` ctor: the device offsets are `0x4b4`/`0x498`. Ours were `0x4ac`/`0x490`, which are
  the wrong devices.
- `BandMachine::SyncLoad`: now reads the mask-8 block that `SyncSave` writes. Before this, the stream
  desynced.
- `ProfileMgr::GetGlobalOptionsSize`: `0x35`, which is the 53 bytes the save actually writes. Ours was
  `0x52`.
- `ProfileMgr::LoadGlobalOptions`: follows retail's rev handling.
- `OvershellProfileProvider`: the members follow retail's ctor layout (`BandUserMgr*` at `0x2c`,
  vector at `0x30`).
- `AccomplishmentSongConditional::Check{,Double,Triple}AwesomesCondition`: retail has no
  vocals/harmony score-type guard.
- `TourDescProvider::UpdateExtendedCustom`: retail has no `TheTour` guard.
- `MetaPerformer`: `mCheatInFinale` is left uninitialised, as in retail. The TU5 tour always
  insta-ranks.
- `ClosetMgr::SetCurrentCharacterPatch`: stores the index directly, with no texture lookup.
- `Leaderboard::ShowData`: compares pguid with `gNullStr`, not `"0"`.
- `PracticePanel::Enter`: adds the TU5 tail that seeds `unk64` from the first GemPlayer's controller.
- `ViewSettingsProvider`: adds the two TU5 settings `music_library_upsell` and `bad_review_showing`,
  plus the new `ProfileMgr` setters at retail `0x82545988`/`0x825459A0`.
- `RndText::Style()`: the default ctor no longer sets a colour, matching retail and taking `LyricPlate`
  from 60.55 to 99.93.
- `OnlineID`: the copy ctor is now implicit. Retail has no `??0OnlineID@@QAA@ABV0@@Z`.
- `TickedInfoCollection::IteratorAt`: no longer clamps. Retail's callers do the clamping.

## 5. Out-of-unit and shared edits (all checked with a whole-report diff; nothing else moved)

- `src/system/os/OnlineID.h`, `src/system/net/DingoJob.cpp`
- `src/system/synth/FxSendPitchShift.{h,cpp}`, `src/system/synth/FxSendSynapse.{h,cpp}`
- `src/system/track/TrackDir.h`, `src/system/bandobj/GemTrackDir.{h,cpp}` (`PrepareChordMesh` returns bool)
- `src/system/bandobj/TrackPanelInterface.h`, `src/system/bandobj/TrackInterface.h`, `src/system/bandobj/VocalTrackDir.{h,cpp}`
- `src/system/rndobj/Text.h`, `src/system/meta/FixedSizeSaveable.h`, `src/system/utl/TickedInfo.h`,
  `src/system/meta/SongMgr.h`, `src/system/meta/StoreOffer.h`
- `config/45410914/objects.json`: `/DRB3_HANDLE_LOCAL_STATIC` added for `MultiSelectListPanel.cpp`. This
  is a cflag change, not a map or splits change.

⚠ **An address that two workers named differently.** W1 gave `FxSendPitchShift::SetRatio` as retail
`0x827122A0`. W4 gave `FxSendSynapse::SetAmount` as the same address. Both functions are
float-member setters, so an ICF fold of the two is plausible. Neither name was mapped, so nothing
depends on which is right. This is unverified.

## 6. Left in the band (41 rows), by blocker

- **Wrong map name.** The retail body is a different function. A map rename would pay; none was made
  here, per scope.
  - `TourProgress::GetNumStarsForGig` @`0x82362128` is `GetTotalStarsForTour`.
  - `BandSongMetadata::HasPart` @`0x8259f980` is the two-argument virtual `_NVSymbol@@_N`. Our body
    scores 98.41 against it.
  - `BandSongMgr::GetValidSongs`: retail returns void.
  - `ProfileMgr::FakeProfileFill` @`0x827B7CA0` is a virtual-base dtor.
  - `ProfileMgr::GetSongToTaskMgrMs` @`0x82B8FC88`.
  - `??0Entry@LocalePanel` in GemManager is a 0x8c-byte copy ctor.
  - `_M_insert_overflow_aux<UpcomingFretRelease>` is really `vector<PlayerTrackConfig>`.
  - `_M_Start@_String_base` and `StartFrame@RndAnimatable` in Accomplishment.
  - `??_GSetUserDifficultyMsg`, `UIListProvider::GapSize` (MusicLibrary), `pair<Symbol,SongRecord>`
    copy ctor, `ObjectDir::Main` (Matchmaker).
  - Plus W16-HE's recorded four.
- **dtk mis-carve:**
  - `TrackPanel::GetNumPlayers` (HE §3)
  - `MusicLibrary::SetlistSize`
  - `Hmx::Object::PostSave` (TexLoadPanel)
  - `RGTutor::RGTutor`
  - `GemTrack::SetInCoda`
  - the `RockCentral::GetBattleEndTimeStr` tails
- **Unexplained retail EH frame with no cleanup:**
  - `Campaign::GetLaunchUser` / `OnMsg(ProfileSwappedMsg)`
  - `ModifierMgr::IsHidden`/`IsActive`/`IsModifierActive`
  - `SongDB::PostLoad`

  These share one shape. Their `.pdata` carries the EH flag and one state with no action. The source
  construct behind it is unknown, and this is now **three units** with it.
- **Loop rotation / register residue:**
  - `SymToControllerType` (5 spellings tried)
  - `AccomplishmentProgress::HandlePendingGamerRewards`
  - `VocalPart::CalcPhraseScoreMax`
  - `PerformanceData::SaveSize`
  - `GemTrack::DrawFill`
  - `AccomplishmentPanel::GetAccomplishmentName`
- **Would regress another row:** `VocalTrack::PollLyricAnimations`, where the in-tree note records −92 B.
- **Out of scope:**
  - `AllowedToAccessContent` (HE §3)
  - `AddSongData`, which is in the RB3DX byte-patch set
  - `PropSync<Piercing>`, which is engine code

## 7. What was not done

- No map, splits, alias or `symbols.txt` edits. Every wrong-name row above is recorded, not renamed.
- Permuter not run.
- `DataPointToQString` and `RBDataClient` were excluded by the Quazal filter.

## 8. Native gate

`tools/native_build_gate.sh` was run on the branch tip after the A/B. The first run FAILED 16/18: `rb3-vocal2` and `rb3-harmony` link `VocalPlayer.cpp` but not
`VocalTrackDir.cpp`, so the new `VocalTrackDir::DeactivateVolume` call was undefined. Commit `309945acd` adds a leaf stub in
`native/src/m10_leaf_stubs.cpp`. That file is native-only and not a match-build input, so the A/B above still stands. The re-run, which was the last build action:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

Only this docs-only commit follows it. It touches no build input.
