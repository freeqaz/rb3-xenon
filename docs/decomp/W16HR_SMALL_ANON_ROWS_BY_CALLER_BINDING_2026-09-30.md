# W16-HR — the small anonymous game rows, named by their callers and repaired (2026-09-30)

Continues W16-HO (`W16HO_ANON_ROWS_CLASSIFIED_AND_REPAIRED_2026-09-30.md`) on the rows HO only
classified coarsely: anonymous `fn_` rows at fuzzy 0, **< 200 B**, in units whose source is under
`src/band3/` or `src/network/` (Quazal and the `/Od` band `0x82A6D168–0x82B54190` excluded).
Branch `w16-hr`, rebased onto main `c0fa121ca`. Source edits only under `src/band3/`;
map, `symbol_aliases.json`, `splits.txt`, `objects.json` and (as its own commits) `symbols.txt`.

**Result.** 206 rows (24,400 B) now carry names; 161 of them (18,324 B) read fuzzy 100. One
whole-binary A/B over the branch diff: **+173 fns / +163 honest / +19,088 B / fuzzy +0.228873 pp**,
exactly the sum of the in-tree steps predicted before the run. 75 units improved; the one unit
"regression" is reattribution from a re-home (§4). 3 units reached 100.

## 1. Population

HO's census (`~/tmp/w16ho/census.py`) re-run on the lane-start tree (`7f011b2bd`, after HO + HQ):
1,279 rows / 175,884 B, of which **1,052 rows / 81,412 B are < 200 B** (HO: 1,091 / 84,084).
After this lane (re-census on the branch): **839 rows / 60,080 B** remain < 200 B.

## 2. The instrument: the caller binding table, and its control

HO's matcher (strings + named callees overlapping each unclaimed candidate) cannot see a tiny body:
480 of HO's small rows had no string and no named callee, and many more are getters whose bytes
match dozens of functions. What does identify them is **who calls them**. The binding table of
`tools/anon_proposal_adjudicate.py` pairs, for every named row objdiff scores mpn 100 with equal
size, each retail placeholder callee at offset *k* with our callee name at offset *k*. Run
backwards it answers: "which of our names do matched callers call at this retail address?"
369 of the 1,052 small rows (33,884 B) had at least one such witness.

**Control (is a caller vote trustworthy?).** The same table, restricted to call sites whose retail
callee is *already map-named* and < 200 B: 9,992 callee addresses, 9,188 with a unanimous vote.
**9,187 of 9,188 unanimous votes name the right function up to fold spelling** (295 are
map-proven folds, 196 are `??_G`/`??_E` pairs of one class, the last is a `??_G`/`??_E` pair
whose anonymous-namespace hash differs). Single-witness votes: 5,172 / 5,552 exact, the rest fold
spellings of the same kinds. ⇒ a unanimous caller vote settles identity; the remaining question
is only *which spelling of a fold*. (The one real miss found in this lane is `0x8268EF78`, §5 —
it was already refused by the size rule below.)

## 3. Decision rules (written before looking at who passed)

**Caller-proposed rows.** Refuse on any *decisive* contradiction: the name is bound or mapped at
another address; a matched caller calls a name here that is not a fold spelling of the proposal;
a vtable-class mismatch in the body; our body size outside [1/3, 3]× the retail row. Otherwise
accept if the vote is unanimous **and** one of: scratch-rename fuzzy ≥ 50, ≥ 1 AGREE/FOLD
relocation, ≥ 2 caller witnesses. Rows meeting none are *held*.

**Rows with no caller witness** (string/callee overlap against unclaimed functions of the row's
own unit): stricter — no caller contradiction, size within [1/2, 2], AGREE ≥ 2 (or ≥ 1 carried
by a string no other retail function references), AGREE beats the runner-up, fuzzy ≥ 50.
Featureless rows with no caller are not named: bytes alone cannot settle a tiny body.

**Carve heads** (one stated exception). Where dtk carved a function at a branch-reached tail, the
retail row is only the head and the size rule compares against a truncated extent. Such a row was
accepted only if the caller vote is unanimous, there is no decisive contradiction, and our full
body compares with **0 differing words** (relocated fields masked) against retail from the head
address — with a control (the nearest-size other function of the same obj, same address) that
must differ. Controls differed by 22–98 words; `Metronome::GetVolume` failed (20 words vs control
21) and was refused.

## 4. What was done

1. **166 rows named by caller binding** (211 proposals → 168 accepted; 2 dropped for an existing
   explicit `null`; 32 refused; 11 held). Naming 19 heads let dtk's Class-4 over-carve merge fold
   their tails back (`symbols.txt`, its own commit), e.g. `ScoreTypeViewSetting::GetBaseScoreType`.
2. **12 rows named by string/callee overlap** (41 proposals): mostly `??_G` deleting dtors bound by
   their own `??1`, `GameMicManager::SetOverdriveEffectEnable`, `GemPlayer` fill/beatmatch rows.
3. **Missing bodies and wrong pins** for caller-proven names our base obj did not define:
   - `SongRecord::UpdateDemo` (`0x825BAE40`) — declared-only, decoded in the header; defined.
     Name inferred (TU5, no oracle).
   - `PresenceMgr::SetSongID` (`0x82680CB8`) — the `SetNotInGame` twin; defined.
   - `AccomplishmentLessonDiscSongConditional`: its TU `0x825E9500–0x825E9668` sat inside
     AccomplishmentDiscSongConditional's pin → re-homed to its own heading; ctor, `??_G` and
     `CheckConditionsForSong` named. Its `CheckLessonCompleteCondition` is ICF-folded into the
     SongList twin at `0x825E9700` (adjudicator: reloc-masked identical). Both
     `CheckConditionsForSong` build a function-local `static Symbol lesson_complete`; the SongList
     one (`0x825E97B8`) was named too.
   - `PlayerBehavior`: header only in our tree; retail has its own TU `0x826EEC34–0x826EECD8`
     split across the VocalPlayer and TrainerGemTab pins. Added `src/band3/game/PlayerBehavior.cpp`
     (rb3-Wii body; retail bytes agree) and a pin. `0x826EECA0` stays with TrainerGemTab — it is a
     fold survivor already named `??0value_compare<map<int,float>>`, and moving it cost −1 fn.
     Named: ctor, `SetFillsDeployBandEnergy`. Refused: `0x826EECC0`/`C8` (also bound by
     `TrackConfig::SetTrackNum`/`SetMaxSlots` callers — ICF fold). Held: `0x826EECA8/B8/D0`
     (8-byte stores, no caller, no string).
4. **Body repair in four forks on disjoint files** (`w16-hr-{A,B,C,D}`, merged `--no-ff`; per-row
   commits are in the history). Full `ninja-locked` builds only; every row of each touched unit
   compared after each edit; an edit was kept only if nothing dropped. Patterns, in order of
   frequency: Wii global `Symbol`/`Message` → function-local `static`; implicit instead of
   user-declared empty virtual dtors (`CampaignGoalsLeaderboardPanel`, `TourDescProvider`,
   `SetlistToStorePanel`, `AppInlineHelp`, `UGCPurchasePanel`, `FaceOptionsProvider`,
   `AccomplishmentCategoryProvider`, `ProTrainerPanel`); dev-only code behind
   `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` (`CheckCharacterAssets`, `VerifyAssets`,
   calibration dumps); out-of-line calls retail makes (`Symbol::operator==(const char*)`,
   `Timer::SplitMs`, `Game::CanUserPause`); stock STLport (`find_if`, `__insertion_sort`) in place
   of hand-inlined loops; TU5 logic rebuilt from retail (`Player::PollTalking` voice engine,
   `TrackPanel::Reload`, autosave, `CanChangeSynapseOption` mic-count gate, 22→17-fret fallback).
   Stub bodies rebuilt: `IsExiting`, `??0AssetMgr`, `HandlePendingGamerRewards`,
   `ViewUserGamercard`, `InviteFriend`, `CensorString`, `MakeupProvider::Update`,
   `IsBandNoFailSet`, `HitTambourineGem` (+ the TU5 helper `TambourineGemPool::SetGemState`,
   name ours). Names introduced for anonymous callees (placeholder-forgiven at their call sites):
   `Stats::SetNumSections`, `SessionUsersProvider::ShowGamercard`, `FriendsProvider::InviteFriend`.
5. **20 more rows named** once the forks made them adjudicable: 11 repaired stubs, the TU5 helper,
   8 carve heads (6 merged to their true extent and read 99–100; `GetConfigNameFromAssetType` and
   `GetAssetTypeFromCurrentState` did not merge and read 0 — identity stands, carve-limited).
6. **Two fold memberships withdrawn** (`scripts/symbol_aliases.json`, recorded, nothing pruned).
   Naming `0x82B90670` and `0x82578D28` made `icf_alias_finder --validate` FATAL:
   - `??0TrackPanelInterface` in the `ConnectionStatusPanel` ctor group: retail `0x82B90670`
     installs vtables whose RTTI COL is `.?AVTrackPanelInterface@@`; the survivor `0x827BA020`
     installs `.?AVConnectionStatusPanel@@`. Different vtables cannot fold. The W16-CU restore
     passed flat T1 only because every vtable relocation was a tolerated placeholder.
   - `_Copy_Construct<SongRanking>` in the `Overlay` group: separate retail body calling a
     different element copy-ctor; `tools/icf_pair_adjudicate.py` now returns REFUTED.
   Measures did not move (no row relied on either forgiveness).

Gates on the final tip: `tools/map_name_injectivity.py` **OK** (30,549 applied rows, injective);
`tools/icf_alias_finder.py --validate` **PASS** (1,475 map-consistent, 264 tolerated,
**0 contradicted**, 1,740 total).

## 5. Measurement

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-hr-ab --patch <branch diff>`, **one run**.
Leg A = main `c0fa121ca` + the lane's two `symbols.txt` over-carve commits (HK's recipe: the split
guard rejects a patch that rewrites `symbols.txt`; in leg A those rows are anonymous and unpaired,
so the merges move no matched figure there). Patch kinds: configgen, map, source, splits. Both legs
read at a split fixed point on `name_check`. Result:
`~/tmp/w16hr/ab_result.json` (copied from the run dir `.ab_measure_runs/20260930-182350-branch-952062/`; the tool reported its tree restore unverified — `tree_restore.ok = false` — so the scratch A/B worktree was discarded after the copy).

| | leg A | leg B | Δ |
|---|---:|---:|---:|
| matched_functions | 45,487 | 45,660 | **+173** |
| masked_equal | 23,563 | 23,573 | +10 |
| honest | 21,924 | 22,087 | **+163** |
| matched_code | 4,505,028 | 4,524,116 | **+19,088 B** (+0.186279 pp) |
| fuzzy_match_percent | 53.372307 | 53.601180 | **+0.228873 pp** |
| units at 100 (mpn) | 219 | 222 | +3 (AccomplishmentLessonSongListConditional, MakeupProvider, UGCPurchasePanel) |

- **Predicted vs measured.** The in-tree steps on the lane-start base summed to 45,217 → 45,390 /
  4,454,124 → 4,473,212 B / 52.662395 → 52.891296: +173 / +19,088 B / +0.228901 pp, predicting leg A
  at 45,487. Measured: exactly +173 / +19,088 B; fuzzy +0.228873.
- Fork deltas composed additively (in-tree, each on `b617b1107`): A +23 / +3,076 B, B +30 /
  +3,548 B, C +21 / +2,588 B, D +30 / +3,188 B; merged together +104 / +12,400 B.
- **The one regressing unit**, AccomplishmentDiscSongConditional −1, is the re-home: the 32 B
  funclet `fn_825E9648` moved to the new Lesson unit, where it pairs at 100.
- **Rows that moved down:** one previously named row, `vector<MeshAO>::_M_fill_insert_aux` (Gem)
  99.80 → 99.69 — naming `0x82567630` `__uninitialized_copy<Patch const*>` (caller-proven) turned
  a forgiven placeholder into a checked fold-spelling site; the row already charged four identical
  `Patch`/`MeshAO` fold sites. One anonymous funclet (40 B) moved down, 15 (600 B) up.
- `none`-ruler control: +20,896 B; NOT_APPLICABLE (patch contains source).

## 6. Rows changed

On main **every row below was an anonymous `fn_` at fuzzy 0, unpaired**. "at naming" is the
scratch-rename score under the name before body repair (blank for carve heads, whose truncated
row reads ~0 by construction); "after" is leg B's `report.json`.

| after | rows | bytes |
|---|---:|---:|
| 100 | 161 | 18,324 |
| 99-100 | 17 | 2,768 |
| 90-99 | 4 | 404 |
| 50-90 | 21 | 2,704 |
| < 50 | 3 | 200 |
| **all** | **206** | **24,400** |

| retail row | size | name | unit | how identified | at naming | after |
|---|---:|---|---|---|---:|---:|
| `0x82545ac8` | 448 | `?GetJoypadExtraLagInits@ProfileMgr@@QBAMW4JoypadType@@W4LagContext@@@Z` | ProfileMgr | callers | 0.00 | 99.91 |
| `0x82566b38` | 408 | `?PlayFinalizedSound@ClosetMgr@@QAAX_N@Z` | ClosetMgr | callers | 0.00 | 99.02 |
| `0x82ba29a0` | 356 | `?SetGemState@TambourineGemPool@@QAAPAVTambourineGem@@HH@Z` | VocalTrack | TU5 helper (name ours) | 100.00 | **100** |
| `0x826a0f10` | 204 | `?GetNumStarsFloat@Scoring@@QBAMHAAV?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@@Z` | Scoring | callers | 0.00 | **100** |
| `0x825b7410` | 196 | `?ExportAll@OvershellPanel@@QAAXABVMessage@@@Z` | OvershellPanel | callers | 67.55 | **100** |
| `0x825c58e0` | 196 | `?SetFromCharacter@AppLabel@@QAAXPBVCharData@@@Z` | AppLabel | callers | 67.00 | **100** |
| `0x8266b0f0` | 196 | `?CheckConditionsForLesson@AccomplishmentTrainerConditional@@QBA_NPAVBandProfile@@VSymbol@@@Z` | AccomplishmentConditional | callers | 68.84 | **100** |
| `0x826a32e8` | 196 | `?PollTalking@Player@@QAAXH@Z` | Player | callers | 42.31 | **100** |
| `0x826f1ec8` | 196 | `?GetNoteRange@VocalPart@@QAAXMAAH0@Z` | VocalPart | callers | 0.00 | 94.49 |
| `0x82540ed8` | 192 | `?OnMsg@MusicLibrary@@QAA?AVDataNode@@ABVRemoteMachineLeftMsg@@@Z` | MusicLibrary | callers | 64.96 | 99.90 |
| `0x825d86a0` | 192 | `?AttemptRegisterOnline@OvershellSlot@@QAAXXZ` | OvershellSlot | callers | 40.60 | **100** |
| `0x82681650` | 192 | `?SetOverdriveEffectEnable@GameMicManager@@QAAX_N@Z` | GameMicManager | string/callee overlap | 53.38 | 53.38 |
| `0x826a2320` | 192 | `?StartIntro@Player@@UAAXXZ` | Player | callers | 28.79 | **100** |
| `0x826f1e08` | 192 | `?CalcPhraseScoreMax@VocalPart@@QBAMABQBVVocalPhrase@@@Z` | VocalPart | callers | 42.44 | 66.85 |
| `0x82365978` | 188 | `??$LoadStd@M@FixedSizeSaveable@@SAXAAVFixedSizeSaveableStream@@AAV?$hash_map@VSymbol@@MU?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@@3@@stlpmtx_std@@HH@Z` | TourPropertyCollection | callers | 100.00 | **100** |
| `0x826a8630` | 188 | `?HandleNewSection@Player@@UAAXABVPracticeSection@@HH@Z` | Player | callers | 33.09 | **100** |
| `0x82547a08` | 180 | `??1ProfileMgr@@UAA@XZ` | ProfileMgr | callers | 68.64 | 68.64 |
| `0x825519e8` | 180 | `?OnMsg@SaveLoadManager@@QAA?AVDataNode@@ABVDeviceChosenMsg@@@Z` | SaveLoadManager | callers | 48.44 | 99.96 |
| `0x825a04a0` | 180 | `??A?$hash_map@VSymbol@@VString@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@VString@@@stlpmtx_std@@@4@@stlpmtx_std@@QAAAAVString@@ABVSymbol@@@Z` | BandSongMetadata | callers | 99.89 | 99.89 |
| `0x825b54b0` | 180 | `?OnMsg@OvershellPanel@@QAA?AVDataNode@@ABVButtonDownMsg@@@Z` | OvershellPanel | callers | 52.09 | **100** |
| `0x825b5568` | 180 | `?OnMsg@OvershellPanel@@QAA?AVDataNode@@ABVButtonUpMsg@@@Z` | OvershellPanel | callers | 52.09 | **100** |
| `0x8266c048` | 180 | `?GetModeSymbol@Leaderboard@@QAA?AVSymbol@@XZ` | Leaderboard | callers | 25.73 | **100** |
| `0x826ca288` | 180 | `?GetRestrictionToken@TrainerChallenge@@QAA?AVSymbol@@XZ` | TrainerPanel | callers | 36.29 | **100** |
| `0x82551aa0` | 176 | `?OnMsg@SaveLoadManager@@QAA?AVDataNode@@ABVNoDeviceChosenMsg@@@Z` | SaveLoadManager | callers | 37.25 | 99.95 |
| `0x8258fe88` | 176 | `?HandlePendingGamerRewards@AccomplishmentProgress@@QAAXXZ` | AccomplishmentProgress | callers | 0.91 | 86.43 |
| `0x825e8890` | 176 | `?Configure@AccomplishmentTrainerListConditional@@QAAXPAVDataArray@@@Z` | AccomplishmentTrainerListConditional | callers | 66.32 | **100** |
| `0x8266c1c0` | 172 | `??0LeaderboardRow@@QAA@ABV0@@Z` | Leaderboard | callers | 58.72 | 58.72 |
| `0x82671e40` | 172 | `??0CurrentOutfitProvider@@QAA@XZ` | CurrentOutfitProvider | callers | 54.81 | **100** |
| `0x8268acc0` | 172 | `?Reset@BandUser@@UAAXXZ` | BandUser | callers | 41.86 | **100** |
| `0x82540888` | 168 | `?SetupTaskForTrainer@MusicLibrary@@QAAXW4ControllerType@@@Z` | MusicLibrary | callers | 42.64 | **100** |
| `0x825e95a0` | 168 | `?CheckConditionsForSong@AccomplishmentLessonDiscSongConditional@@UBA_NPAVSongStatusMgr@@VSymbol@@@Z` | AccomplishmentLessonDiscSongConditional | callers / TU re-home | 62.26 | 99.88 |
| `0x825e97b8` | 168 | `?CheckConditionsForSong@AccomplishmentLessonSongListConditional@@UBA_NPAVSongStatusMgr@@VSymbol@@@Z` | AccomplishmentLessonSongListConditional | twin of re-homed TU | 100.00 | **100** |
| `0x8268f6c8` | 168 | `?CensorString@@YAXAAVString@@@Z` | Defines | callers | 0.95 | **100** |
| `0x825677f8` | 164 | `?PreviewCharacter@ClosetMgr@@QAAX_N0@Z` | ClosetMgr | callers | 47.80 | **100** |
| `0x82613f38` | 164 | `??$Find@VUITrigger@@@ObjectDir@@QAAPAVUITrigger@@PBD_N@Z` | ContentLoadingPanel | callers | 100.00 | **100** |
| `0x82613fe0` | 164 | `??$Find@VMeterDisplay@@@ObjectDir@@QAAPAVMeterDisplay@@PBD_N@Z` | ContentLoadingPanel | callers | 100.00 | **100** |
| `0x826a0ff8` | 164 | `?GetPlayerScoreInfo@Scoring@@QBAPAVPlayerScoreInfo@@W4TrackType@@@Z` | Scoring | callers | 52.24 | **100** |
| `0x826ad0b8` | 164 | `?SetSpeedRatio@GemTrainerPanel@@QAAXM@Z` | GemTrainerPanel | callers | 63.07 | **100** |
| `0x8253f140` | 160 | `?PushMakingSetlistToScreen@MusicLibrary@@QAAXXZ` | MusicLibrary | callers | 24.32 | **100** |
| `0x825758f8` | 160 | `?UpgradeMidiFile@BandSongMgr@@QBAPBDH@Z` | BandSongMgr | callers | 46.83 | 99.75 |
| `0x825adb90` | 160 | `?Save@OpenGateData@?A0xb0de99ba@@QBAXAAVBinStream@@@Z` | WaitingUserGate | callers | 85.00 | 85.00 |
| `0x8268f308` | 160 | `?TrackTypeToScoreType@@YA?AW4ScoreType@@W4TrackType@@_N1@Z` | Defines | callers | 0.00 | **100** |
| `0x82540cf0` | 156 | `?GetNetSetlists@MusicLibrary@@QBAXAAV?$vector@PAVNetSavedSetlist@@V?$StlNodeAlloc@PAVNetSavedSetlist@@@stlpmtx_std@@@stlpmtx_std@@@Z` | MusicLibrary | callers | 43.23 | **100** |
| `0x8257cf40` | 156 | `?IsNoFailActive@MetaPerformer@@QBA_NXZ` | MetaPerformer | callers | 39.31 | **100** |
| `0x825be8b8` | 156 | `?GetFontCharForProDrums@@YAPBDH@Z` | SongSort | callers | 27.00 | **100** |
| `0x8262ac50` | 156 | `?SetLabelForData@LayerProvider@@QBAXPAVUILabel@@H@Z` | PatchPanel | callers | 64.95 | **100** |
| `0x826521e0` | 156 | `?OnMsg@BandMatchmaker@@QAA?AVDataNode@@ABVModeChangedMsg@@@Z` | Matchmaker | callers | 100.00 | **100** |
| `0x8268c0b8` | 156 | `??_DRemoteBandUser@@QAAXXZ` | BandUser | callers | 100.00 | **100** |
| `0x826bdd50` | 156 | `?EnableDrumFills@GemPlayer@@UAAX_N@Z` | GemPlayer | string/callee overlap | 64.36 | 64.36 |
| `0x826ca0b8` | 156 | `?Exit@TrainerChallenge@@QAAXXZ` | TrainerPanel | callers | 30.90 | **100** |
| `0x82365560` | 152 | `??$SaveStd@M@FixedSizeSaveable@@SAXAAVFixedSizeSaveableStream@@ABV?$hash_map@VSymbol@@MU?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@3@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@@3@@stlpmtx_std@@HH@Z` | TourPropertyCollection | callers | 100.00 | **100** |
| `0x825d16b8` | 152 | `??$SaveStdPtr@VSongStatus@@@FixedSizeSaveable@@SAXAAVFixedSizeSaveableStream@@ABV?$hash_map@HPAVSongStatus@@U?$hash@H@stlpmtx_std@@U?$equal_to@H@3@V?$StlNodeAlloc@U?$pair@$$CBHPAVSongStatus@@@stlpmtx_std@@@3@@stlpmtx_std@@HH@Z` | SongStatusMgr | callers | 86.13 | 86.13 |
| `0x825d9750` | 152 | `?RenameCharacter@OvershellSlot@@QAAXPBD@Z` | OvershellSlot | callers | 75.66 | **100** |
| `0x825efe18` | 152 | `?GetGoalUnits@CampaignGoalsLeaderboardPanel@@QBA?AVSymbol@@XZ` | CampaignGoalsLeaderboardPanel | callers | 64.29 | **100** |
| `0x8257b4b8` | 148 | `?CurrentImpl@MetaPerformer@@QBAPAVMetaPerformerImpl@@XZ` | MetaPerformer | callers | 41.84 | **100** |
| `0x8257de70` | 148 | `?UpdateBattleTypeLabel@MetaPerformer@@QAAXPAVUILabel@@@Z` | MetaPerformer | callers | 34.78 | **100** |
| `0x8260dd78` | 148 | `?FinalizeCharacter@CharacterCreatorPanel@@QAAXXZ` | CharacterCreatorPanel | callers | 44.24 | **100** |
| `0x82678aa8` | 148 | `?GetSectionAtMs@Game@@QBA?AVSymbol@@M@Z` | Game | callers | 83.78 | **100** |
| `0x826e5a50` | 148 | `?PackFloats@VocalPlayer@@QBAIABV?$vector@MV?$StlNodeAlloc@M@stlpmtx_std@@@stlpmtx_std@@MM@Z` | VocalPlayer | callers | 38.91 | **100** |
| `0x8254c020` | 144 | `?DisableAutosave@SaveLoadManager@@QAAXPAVLocalBandUser@@@Z` | SaveLoadManager | callers | 85.25 | **100** |
| `0x82573d40` | 144 | `??1SetlistToStorePanel@@UAA@XZ` | MetaPanel | callers | 60.81 | **100** |
| `0x825c5a28` | 144 | `?SetEditSetlistName@AppLabel@@QAAXPBVUIPanel@@@Z` | AppLabel | callers | 41.00 | **100** |
| `0x825c5ae0` | 144 | `?SetEditSetlistDesc@AppLabel@@QAAXPBVUIPanel@@@Z` | AppLabel | callers | 41.00 | **100** |
| `0x825d1360` | 144 | `?LoadFromStream@SongStatusData@@QAAXAAVBinStream@@W4ScoreType@@@Z` | SongStatusMgr | callers | 60.28 | **100** |
| `0x8266fdc8` | 144 | `?Update@MakeupProvider@@QAAXVSymbol@@@Z` | MakeupProvider | callers | 19.86 | **100** |
| `0x826ad188` | 144 | `?EnableMetronome@GemTrainerPanel@@QAAX_N@Z` | GemTrainerPanel | callers | 38.00 | **100** |
| `0x82bafde8` | 144 | `?SetGlowing@GemSmasher@@QAAX_N@Z` | GemSmasher | callers | 0.00 | **100** |
| `0x8257df30` | 140 | `?SetBandNoFail@MetaPerformer@@QAAX_N@Z` | MetaPerformer | callers | 47.57 | **100** |
| `0x825d97e8` | 140 | `?CanChangeSynapseOption@OvershellSlot@@QAA_NXZ` | OvershellSlot | callers | 40.37 | **100** |
| `0x826851d8` | 140 | `?GetBandFailCue@SongDB@@QBAXAAVString@@@Z` | SongDB | callers | 26.77 | 63.94 |
| `0x82b90670` | 140 | `??0TrackPanelInterface@@QAA@XZ` | TrackPanel | callers | 76.03 | 76.03 |
| `0x8260d690` | 136 | `?GetGlasses@CharacterCreatorPanel@@QAA?AVSymbol@@XZ` | CharacterCreatorPanel | callers | 29.47 | **100** |
| `0x8260d858` | 136 | `?GetHair@CharacterCreatorPanel@@QAA?AVSymbol@@XZ` | CharacterCreatorPanel | callers | 39.76 | 98.24 |
| `0x8260da20` | 136 | `?GetFaceHair@CharacterCreatorPanel@@QAA?AVSymbol@@XZ` | CharacterCreatorPanel | callers | 39.47 | **100** |
| `0x826c3470` | 136 | `?DynamicAddBeatmatch@GemPlayer@@UAAXXZ` | GemPlayer | string/callee overlap | 52.18 | 52.18 |
| `0x82588f58` | 132 | `?SaveValue@Modifier@@QBA_NXZ` | ModifierMgr | callers | 39.00 | **100** |
| `0x82589030` | 132 | `?UseSaveValue@Modifier@@QBA_NXZ` | ModifierMgr | callers | 39.00 | **100** |
| `0x825891e0` | 132 | `?DelayedEffect@Modifier@@QBA_NXZ` | ModifierMgr | callers | 39.00 | **100** |
| `0x8267b3d8` | 132 | `?RemovePlayer@Game@@QAAXPAVPlayer@@@Z` | Game | callers | 67.27 | 99.85 |
| `0x8269acc8` | 132 | `?EveryoneFinishedCoda@Band@@QAA_NXZ` | Band | callers | 100.00 | **100** |
| `0x826a3d70` | 132 | `?UpdateSectionStats@Player@@QAAXMM@Z` | Player | callers | 59.64 | **100** |
| `0x826bc758` | 132 | `?FillsEnabled@GemPlayer@@UAA_NH@Z` | GemPlayer | string/callee overlap | 52.79 | 52.79 |
| `0x8253ac40` | 128 | `?IsExiting@MusicLibrary@@QAA_NXZ` | MusicLibrary | callers | 6.06 | **100** |
| `0x8257b620` | 128 | `?IsBandNoFailSet@MetaPerformer@@QBA_NXZ` | MetaPerformer | callers | 21.56 | **100** |
| `0x825d6728` | 128 | `??1ViewSettingsProvider@@UAA@XZ` | ViewSetting | callers | 40.31 | 99.84 |
| `0x82669b98` | 128 | `?CheckTripleAwesomesCondition@AccomplishmentSongConditional@@QBA_NPAVSongStatusMgr@@VSymbol@@ABUAccomplishmentCondition@@@Z` | AccomplishmentSongConditional | string/callee overlap | 58.75 | 58.75 |
| `0x8268f648` | 128 | `?ControllerHasRepresentativePartPriority@@YA_NW4ControllerType@@W4TrackType@@@Z` | Defines | callers | 59.47 | **100** |
| `0x825ae830` | 124 | `??1OpenGateData@?A0xb0de99ba@@UAA@XZ` | WaitingUserGate | callers | 64.52 | 64.52 |
| `0x825f7718` | 124 | `?SelectedAccomplishment@AccomplishmentPanel@@QBA?AVSymbol@@XZ` | AccomplishmentPanel | callers | 56.77 | **100** |
| `0x8268f3a8` | 120 | `?ScoreTypeToTrackType@@YA?AW4TrackType@@W4ScoreType@@@Z` | Defines | callers | 0.00 | **100** |
| `0x8269d0b8` | 120 | `?GetPercentComplete@Performer@@QBAHXZ` | Performer | callers | 59.33 | **100** |
| `0x826e4628` | 120 | `?SetTrack@VocalPlayer@@UAAXH@Z` | VocalPlayer | string/callee overlap | 63.50 | 63.50 |
| `0x826f1a70` | 120 | `?UpdateSongMinMaxPitch@VocalPart@@QAAXXZ` | VocalPart | callers | 0.00 | **100** |
| `0x82550778` | 116 | `?IsReasonToAutosave@SaveLoadManager@@IAA_NXZ` | SaveLoadManager | callers | 81.21 | **100** |
| `0x825bae40` | 116 | `?UpdateDemo@SongRecord@@QAA_NXZ` | SongRecord | callers + new body | 100.00 | **100** |
| `0x82678c88` | 116 | `?SetMusicSpeed@Game@@QAAXM@Z` | Game | callers | 61.00 | **100** |
| `0x826f1d70` | 116 | `?GetPartHitPercentage@VocalPart@@QBAMABV?$vector@VVocalPhrase@@V?$StlNodeAlloc@VVocalPhrase@@@stlpmtx_std@@@stlpmtx_std@@HH@Z` | VocalPart | callers | 92.93 | **100** |
| `0x825d8fd0` | 112 | `?ViewUserGamercard@OvershellSlot@@QAAXH@Z` | OvershellSlot | callers | 0.00 | **100** |
| `0x825fc9b0` | 112 | `??1AccomplishmentCategoryProvider@@UAA@XZ` | AccomplishmentPanel | callers | 46.64 | **100** |
| `0x82678b40` | 112 | `?SetTimeOffset@Game@@QAAXXZ` | Game | callers | 0.00 | **100** |
| `0x8267b478` | 112 | `?OvershellSetPaused@Game@@QAAX_N@Z` | Game | callers | 14.29 | **100** |
| `0x82b7e738` | 112 | `??1TourDescProvider@@UAA@XZ` | TourDescPanel | callers | 46.64 | **100** |
| `0x82552560` | 108 | `?EnableAutosave@SaveLoadManager@@QAAXPAVLocalBandUser@@@Z` | SaveLoadManager | callers | 44.81 | **100** |
| `0x8256b740` | 108 | `??0AssetMgr@@QAA@XZ` | AssetMgr | callers | 0.00 | 99.63 |
| `0x825a6f88` | 108 | `?GetTotalPointsForNextMajorCampaignLevelForPrimary@Campaign@@QAAHXZ` | Campaign | callers | 100.00 | **100** |
| `0x825a70e8` | 108 | `?GetCurrentPointsForNextMajorCampaignLevelForPrimary@Campaign@@QAAHXZ` | Campaign | callers | 100.00 | **100** |
| `0x825d4860` | 108 | `?GetBaseScoreType@ScoreTypeViewSetting@@QBA?AW4ScoreType@@XZ` | ViewSetting | callers | 0.00 | **100** |
| `0x825d48d0` | 108 | `?GetAlternateScoreType@ScoreTypeViewSetting@@QBA?AW4ScoreType@@XZ` | ViewSetting | callers | 0.00 | **100** |
| `0x825d9040` | 108 | `?InviteFriend@OvershellSlot@@QAAXH@Z` | OvershellSlot | callers | 0.00 | **100** |
| `0x8253b8d0` | 104 | `?SetlistIsFull@MusicLibrary@@QAA_NXZ` | MusicLibrary | callers | 91.46 | **100** |
| `0x8256aa38` | 104 | `?clear@?$hashtable@U?$pair@$$CBHVString@@@stlpmtx_std@@HU?$hash@H@2@U?$_HashMapTraitsT@U?$pair@$$CBHVString@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBHVString@@@stlpmtx_std@@@2@U?$equal_to@H@2@V?$StlNodeAlloc@U?$pair@$$CBHVString@@@stlpmtx_std@@@2@@stlpmtx_std@@QAAXXZ` | AssetMgr | callers | 100.00 | **100** |
| `0x826a7558` | 104 | `?LocalDeployBandEnergy@Player@@UAAHXZ` | Player | callers | 45.96 | **100** |
| `0x822a6ed0` | 100 | `??$_M_allocate_and_copy@PBVMeshAO@OutfitConfig@@@?$vector@VMeshAO@OutfitConfig@@V?$StlNodeAlloc@VMeshAO@OutfitConfig@@@stlpmtx_std@@@stlpmtx_std@@IAAPAVMeshAO@OutfitConfig@@IPBV23@0@Z` | Gem | callers | 100.00 | **100** |
| `0x82567758` | 100 | `??$_M_allocate_and_copy@PBVPatch@BandCharDesc@@@?$vector@VPatch@BandCharDesc@@V?$StlNodeAlloc@VPatch@BandCharDesc@@@stlpmtx_std@@@stlpmtx_std@@IAAPAVPatch@BandCharDesc@@IPBV23@0@Z` | ClosetMgr | callers | 100.00 | **100** |
| `0x82588df0` | 100 | `?DisableAutoVocals@ModifierMgr@@QBAXXZ` | ModifierMgr | callers | 22.16 | **100** |
| `0x82594858` | 100 | `?GetIconArt@Accomplishment@@QBAPBDXZ` | Accomplishment | callers | 0.00 | 84.00 |
| `0x8260c008` | 100 | `?EndTest@CalibrationPanel@@QAAXXZ` | CalibrationPanel | callers | 13.96 | **100** |
| `0x8263b310` | 100 | `??$_M_allocate_and_copy@PAVNewReleaseEntry@StoreMainPanel@@@?$vector@VNewReleaseEntry@StoreMainPanel@@V?$StlNodeAlloc@VNewReleaseEntry@StoreMainPanel@@@stlpmtx_std@@@stlpmtx_std@@IAAPAVNewReleaseEntry@StoreMainPanel@@IPAV23@0@Z` | StoreMainPanel | callers | 100.00 | **100** |
| `0x82698ca8` | 100 | `?SetPartPercentage@SingerStats@@QAAXHM@Z` | Stats | callers | 0.00 | **100** |
| `0x826afaf0` | 100 | `??1ProTrainerPanel@@UAA@XZ` | RGTrainerPanel | callers | 43.56 | **100** |
| `0x826cfb40` | 100 | `??$_M_allocate_and_copy@PAVPracticeSection@@@?$vector@VPracticeSection@@V?$StlNodeAlloc@VPracticeSection@@@stlpmtx_std@@@stlpmtx_std@@IAAPAVPracticeSection@@IPAV2@0@Z` | PracticeSectionProvider | callers | 99.80 | 99.80 |
| `0x826eec38` | 100 | `??0PlayerBehavior@@QAA@XZ` | PlayerBehavior | callers + new TU | 100.00 | **100** |
| `0x82ba2e60` | 100 | `?HitTambourineGem@VocalTrack@@QAAXH@Z` | VocalTrack | callers | 0.00 | **100** |
| `0x82567630` | 96 | `??$__uninitialized_copy@PBVPatch@BandCharDesc@@PAV12@@stlpmtx_std@@YAPAVPatch@BandCharDesc@@PBV12@0PAV12@ABU__false_type@0@@Z` | ClosetMgr | callers | 100.00 | **100** |
| `0x825d2c90` | 96 | `?UpdateCachedTotalDiscScore@SongStatusMgr@@QAAHW4ScoreType@@@Z` | SongStatusMgr | callers | 33.88 | **100** |
| `0x826029d8` | 96 | `??_DAppInlineHelp@@QAAXXZ` | AppInlineHelp | callers | 75.46 | **100** |
| `0x8260dac8` | 96 | `?SetHeight@CharacterCreatorPanel@@QAAXH@Z` | CharacterCreatorPanel | callers | 72.29 | **100** |
| `0x8260db28` | 96 | `?SetWeight@CharacterCreatorPanel@@QAAXH@Z` | CharacterCreatorPanel | callers | 72.29 | **100** |
| `0x8260db88` | 96 | `?SetBuild@CharacterCreatorPanel@@QAAXH@Z` | CharacterCreatorPanel | callers | 72.29 | **100** |
| `0x82639678` | 96 | `??$__uninitialized_fill_n@PAVRecommendedEntry@StoreInfoPanel@@IV12@@stlpmtx_std@@YAPAVRecommendedEntry@StoreInfoPanel@@PAV12@IABV12@ABU__false_type@0@@Z` | StoreInfoPanel | callers | 100.00 | **100** |
| `0x8266d3d0` | 96 | `??$__uninitialized_copy@PAVLeaderboardRow@@PAV1@@stlpmtx_std@@YAPAVLeaderboardRow@@PAV1@00ABU__false_type@0@@Z` | Leaderboard | callers | 99.79 | 99.79 |
| `0x826727b0` | 96 | `?AddTickerData@MainHubMessageProvider@@QAAXW4TickerDataType@@HH_N1@Z` | MainHubMessageProvider | callers | 30.67 | **100** |
| `0x82698698` | 96 | `??$__insertion_sort@PAU?$pair@HM@stlpmtx_std@@U12@UPartPercentageSorter@SingerStats@@@stlpmtx_std@@YAXPAU?$pair@HM@0@00UPartPercentageSorter@SingerStats@@@Z` | Stats | callers | 2.38 | **100** |
| `0x826ceae8` | 96 | `??$__uninitialized_fill_n@PAVData@MultiplayerAnalyzer@@IV12@@stlpmtx_std@@YAPAVData@MultiplayerAnalyzer@@PAV12@IABV12@ABU__false_type@0@@Z` | MultiplayerAnalyzer | callers | 100.00 | **100** |
| `0x826ceb88` | 96 | `??$__uninitialized_copy@PAVData@MultiplayerAnalyzer@@PAV12@@stlpmtx_std@@YAPAVData@MultiplayerAnalyzer@@PAV12@00ABU__false_type@0@@Z` | MultiplayerAnalyzer | callers | 100.00 | **100** |
| `0x82b92e58` | 96 | `?Reload@TrackPanel@@QAAXXZ` | TrackPanel | callers | 0.00 | **100** |
| `0x82b9b8f8` | 96 | `??$__uninitialized_fill_n@PAVGem@@IV1@@stlpmtx_std@@YAPAVGem@@PAV1@IABV1@ABU__false_type@0@@Z` | GemManager | callers | 100.00 | **100** |
| `0x82551d18` | 92 | `?OnMsg@SaveLoadManager@@QAA?AVDataNode@@ABVRockCentralOpCompleteMsg@@@Z` | SaveLoadManager | callers | 84.35 | **100** |
| `0x825d94e0` | 92 | `?ShowCharEdit@OvershellSlot@@QAAXH@Z` | OvershellSlot | callers | 50.91 | **100** |
| `0x82666948` | 92 | `?IsIndexNewChar@CharProvider@@QAA_NH@Z` | CharProvider | callers | 0.00 | **100** |
| `0x82666a00` | 92 | `?IsIndexCustomChar@CharProvider@@QAA_NH@Z` | CharProvider | callers | 0.00 | **100** |
| `0x82666a60` | 92 | `?IsIndexPrefab@CharProvider@@QAA_NH@Z` | CharProvider | callers | 0.00 | **100** |
| `0x82360a50` | 88 | `?GetCurrentQuestDescription@TourPerformerImpl@@QBA?AVSymbol@@XZ` | TourPerformer | callers | 71.64 | **100** |
| `0x82360aa8` | 88 | `?GetCurrentQuestLongDescription@TourPerformerImpl@@QBA?AVSymbol@@XZ` | TourPerformer | callers | 71.64 | **100** |
| `0x82362408` | 88 | `?GetFilterForCurrentGig@TourProgress@@QBA?AVSymbol@@XZ` | TourProgress | callers | 67.95 | **100** |
| `0x823624e8` | 88 | `?GetVenueForCurrentGig@TourProgress@@QBA?AVSymbol@@XZ` | TourProgress | callers | 67.95 | **100** |
| `0x82592da0` | 88 | `?HasActiveEvent@UIEventMgr@@QBA_NXZ` | UIEventMgr | callers | 53.00 | **100** |
| `0x825a0708` | 88 | `??$_Move_Construct_Aux@V?$hash_map@VSymbol@@VString@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@VString@@@stlpmtx_std@@@4@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$hash_map@VSymbol@@VString@@U?$hash@VSymbol@@@stlpmtx_std@@U?$equal_to@VSymbol@@@4@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@VString@@@stlpmtx_std@@@4@@0@AAV10@ABU__false_type@0@@Z` | BandSongMetadata | callers | 100.00 | **100** |
| `0x825e9540` | 88 | `??_GAccomplishmentLessonDiscSongConditional@@UAAPAXI@Z` | AccomplishmentLessonDiscSongConditional | callers / TU re-home | 100.00 | **100** |
| `0x826669a8` | 88 | `?IsIndexNone@CharProvider@@QAA_NH@Z` | CharProvider | callers | 0.00 | **100** |
| `0x82686100` | 88 | `?IteratorAt@?$TickedInfoCollection@M@@QBAPBV?$TickedInfo@M@@H_N@Z` | SongDB | callers | 17.86 | 17.86 |
| `0x82ba0968` | 88 | `?_M_create_nodes@?$_Deque_base@U?$pair@W4KeyframeCmd@LightPreset@@M@stlpmtx_std@@V?$StlNodeAlloc@U?$pair@W4KeyframeCmd@LightPreset@@M@stlpmtx_std@@@2@@stlpmtx_std@@IAAXPAPAU?$pair@W4KeyframeCmd@LightPreset@@M@2@0@Z` | VocalTrack | callers | 100.00 | **100** |
| `0x82ba0a00` | 88 | `?_M_create_nodes@?$_Deque_base@VLyricShift@VocalTrack@@V?$StlNodeAlloc@VLyricShift@VocalTrack@@@stlpmtx_std@@@stlpmtx_std@@IAAXPAPAVLyricShift@VocalTrack@@0@Z` | VocalTrack | callers | 100.00 | **100** |
| `0x8235be50` | 84 | `?GetGigSpecificIntro@Tour@@QBA?AVSymbol@@XZ` | Tour | callers | 85.19 | **100** |
| `0x8235bea8` | 84 | `?GetGigSpecificOutro@Tour@@QBA?AVSymbol@@XZ` | Tour | callers | 85.19 | **100** |
| `0x8260d0c0` | 84 | `??1FaceOptionsProvider@@UAA@XZ` | CharacterCreatorPanel | callers | 28.19 | **100** |
| `0x826a2540` | 84 | `?Saveable@Player@@QBA_NXZ` | Player | callers | 54.00 | **100** |
| `0x8235bdb0` | 80 | `?HasGigSpecificIntro@Tour@@QBA_NXZ` | Tour | callers | 84.45 | **100** |
| `0x8235be00` | 80 | `?HasGigSpecificOutro@Tour@@QBA_NXZ` | Tour | callers | 84.20 | 99.75 |
| `0x82362c20` | 80 | `?GetTourMostStars@TourProgress@@QBAHVSymbol@@@Z` | TourProgress | callers | 100.00 | **100** |
| `0x82578d28` | 80 | `??$_Copy_Construct@VSongRanking@BandSongMgr@@@stlpmtx_std@@YAXPAVSongRanking@BandSongMgr@@ABV12@@Z` | BandSongMgr | callers | 99.75 | 99.75 |
| `0x8258c6d8` | 80 | `?GrantCampaignKey@BandProfile@@QAAXVSymbol@@@Z` | BandProfile | callers | 73.90 | **100** |
| `0x8258c728` | 80 | `?UnlockModifier@BandProfile@@QAAXVSymbol@@@Z` | BandProfile | callers | 73.90 | **100** |
| `0x82594808` | 80 | `?IsDynamic@Accomplishment@@QBA_NXZ` | Accomplishment | callers | 0.00 | **100** |
| `0x823e30f0` | 76 | `??1NewUserMsg@@UAA@XZ` | NetSession | callers | 83.63 | 83.63 |
| `0x823e3270` | 76 | `??1DataArrayMsg@@UAA@XZ` | NetSession | callers | 83.63 | 83.63 |
| `0x824f6f18` | 76 | `?GetBattleEndTimeStr@RockCentral@@QAAPBDW4BattleTimeUnits@@@Z` | RockCentral | callers | 43.75 | 68.42 |
| `0x825f0068` | 76 | `??1CampaignGoalsLeaderboardPanel@@UAA@XZ` | CampaignGoalsLeaderboardPanel | callers | 9.95 | **100** |
| `0x82688600` | 76 | `??_GSongDB@@UAAPAXI@Z` | SongDB | string/callee overlap | 100.00 | **100** |
| `0x826d23c8` | 76 | `??_GTracker@@UAAPAXI@Z` | Tracker | string/callee overlap | 100.00 | **100** |
| `0x82b97380` | 76 | `??_GGemTrack@@UAAPAXI@Z` | GemTrack | string/callee overlap | 100.00 | **100** |
| `0x82baf9a0` | 76 | `??_GLyricPlate@@UAAPAXI@Z` | Lyric | string/callee overlap | 100.00 | **100** |
| `0x8235cc38` | 72 | `?GetTourProperty@Tour@@QBAPAVTourProperty@@VSymbol@@@Z` | Tour | callers | 99.72 | 99.72 |
| `0x82362460` | 72 | `?GetSetlistTypeForCurrentGig@TourProgress@@QBA?AVSymbol@@H@Z` | TourProgress | callers | 68.06 | **100** |
| `0x825692a8` | 72 | `?GetCategoriesFromTrainer@LessonMgr@@QBAPAV?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@VSymbol@@@Z` | LessonMgr | callers | 100.00 | **100** |
| `0x825692f0` | 72 | `?GetLessonsFromCategory@LessonMgr@@QBAPAV?$vector@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmtx_std@@VSymbol@@@Z` | LessonMgr | callers | 100.00 | **100** |
| `0x8263e978` | 72 | `??1UGCPurchasePanel@@UAA@XZ` | UGCPurchasePanel | callers | 21.61 | **100** |
| `0x826872a8` | 72 | `?RebuildPhrases@SongDB@@QAAXH@Z` | SongDB | callers | 19.72 | **100** |
| `0x82b7c2f8` | 68 | `?IsTourAvailable@TourDescPanel@@QAA_NXZ` | TourDescPanel | callers | 64.71 | **100** |
| `0x825c0f20` | 64 | `?IsWaitingNetUIState@@YA_NW4NetUIState@@@Z` | BandMachine | callers | 10.88 | **100** |
| `0x826d19a8` | 64 | `??$__uninitialized_copy@PAVTrackerPlayerDisplay@@PAV1@@stlpmtx_std@@YAPAVTrackerPlayerDisplay@@PAV1@00ABU__false_type@0@@Z` | Tracker | callers | 85.71 | **100** |
| `0x82bafb58` | 64 | `?CodaHit@GemSmasher@@QAAXXZ` | GemSmasher | callers | 0.00 | 93.75 |
| `0x82276fd8` | 60 | `??0length_error@stlpmtx_std@@QAA@ABV01@@Z` | Accomplishment | string/callee overlap | 100.00 | **100** |
| `0x8235cbf8` | 60 | `?HasTourProperty@Tour@@QBA_NVSymbol@@@Z` | Tour | callers | 99.67 | 99.67 |
| `0x823e1320` | 60 | `?ByteCode@NetPushScreenMsg@@UBAEXZ` | BandUI | string/callee overlap | 100.00 | **100** |
| `0x82589710` | 60 | `?IsModifierActive@ModifierMgr@@QBA_NVSymbol@@@Z` | ModifierMgr | callers | 79.67 | 79.67 |
| `0x825e9500` | 60 | `??0AccomplishmentLessonDiscSongConditional@@QAA@PAVDataArray@@H@Z` | AccomplishmentLessonDiscSongConditional | callers / TU re-home | 100.00 | **100** |
| `0x825eeb70` | 60 | `?GetConfigNameFromAssetType@@YAPBDW4AssetType@@@Z` | AssetTypes | callers | 0.00 | 0.00 |
| `0x82575f30` | 52 | `?GetPosInRecentList@BandSongMgr@@QAAHH@Z` | BandSongMgr | callers | 10.77 | **100** |
| `0x82615188` | 52 | `?GetAssetTypeFromCurrentState@CustomizePanel@@QAA?AW4AssetType@@XZ` | CustomizePanel | callers | 0.00 | 0.00 |
| `0x826d1970` | 52 | `??$__uninitialized_fill_n@PAVTrackerPlayerDisplay@@IV1@@stlpmtx_std@@YAPAVTrackerPlayerDisplay@@PAV1@IABV1@ABU__false_type@0@@Z` | Tracker | callers | 70.00 | **100** |
| `0x826f1c58` | 52 | `?GetFreestyleSectionDurationMs@VocalPart@@QBAMXZ` | VocalPart | callers | 52.69 | **100** |
| `0x8253b888` | 48 | `?SongAtSetlistIndex@MusicLibrary@@QAAHH@Z` | MusicLibrary | callers | 50.00 | **100** |
| `0x8268ae60` | 48 | `?IsFullyInGame@BandUser@@QBA_NXZ` | BandUser | callers | 38.33 | **100** |
| `0x826a10f8` | 48 | `?GetBandNumStars@Scoring@@QBAHH@Z` | Scoring | callers | 41.67 | **100** |
| `0x824f6f80` | 40 | `?GetActiveContextHighWatermark@RockCentral@@QAAHXZ` | RockCentral | callers | 75.00 | **100** |
| `0x82592df8` | 40 | `?HasActiveDialogEvent@UIEventMgr@@QBA_NXZ` | UIEventMgr | callers | 32.50 | **100** |
| `0x82592e20` | 40 | `?HasActiveTransitionEvent@UIEventMgr@@QBA_NXZ` | UIEventMgr | callers | 32.50 | **100** |
| `0x82680cb8` | 32 | `?SetSongID@PresenceMgr@@QAAXH@Z` | PresenceMgr | callers + new body | 100.00 | **100** |
| `0x8253b8b8` | 16 | `?SetlistSize@MusicLibrary@@QAAHXZ` | MusicLibrary | callers | 75.00 | 75.00 |
| `0x82b98c38` | 12 | `?GetBeardThreshold@@YAHXZ` | GemManager | callers | 100.00 | **100** |
| `0x8258a178` | 8 | `?GetSongHighScore@BandProfile@@QBAHHW4ScoreType@@@Z` | BandProfile | callers | 97.50 | 97.50 |
| `0x82594610` | 8 | `?IsTrackedInLeaderboard@Accomplishment@@QBA_NXZ` | Accomplishment | callers | 100.00 | **100** |
| `0x8268b2a0` | 8 | `?GetLocalBandUser@LocalBandUser@@UAAPAV1@XZ` | BandUser | callers | 100.00 | **100** |
| `0x826eecb0` | 8 | `?SetFillsDeployBandEnergy@PlayerBehavior@@QAAX_N@Z` | PlayerBehavior | callers + new TU | 100.00 | **100** |
| `0x826f6838` | 4 | `?Init@TrackerSectionManager@@QAAXXZ` | TrackerUtils | callers | 100.00 | **100** |

**Previously named rows whose score changed (leg A → leg B):**

| row | size | unit | leg A | leg B |
|---|---:|---|---:|---:|
| `??0ProTrainerPanel@@QAA@XZ` | 164 | RGTrainerPanel | 97.44 | **100** |
| `?_M_fill_insert_aux@?$vector@VMeshAO@OutfitConfig@@V?$StlNodeAlloc@VMeshAO@OutfitConfig@@@stlpmtx_std@@@stlpmtx_std@@AAAXPAVMeshAO@OutfitConfig@@IABV34@ABU__false_type@2@@Z` | 392 | Gem | 99.80 | 99.69 |

## 7. Refused, held, and left

- **Refused (caller set, 32):** fold soup where callers name other instantiations
  (`clear@_Rb_tree<…>` ×2, `clear@hashtable<…>`, `__ucopy_trivial`, `GetFontCharFromTrackType`/
  `FromScoreType`, `GetLesson@LessonMgr`), message/panel dtors with a second caller name
  (`AccomplishmentEarnedMsg`, `SetUserTrackTypeMsg`, `Instarank`, `BandScreen`), and size-ratio
  refusals — most later repaired and named (§4.5). Still refused: `0x8268EF78` (the caller name
  `RemoteBandUser::GetLocalBandUser` is **wrong** — retail is a `this-0x2c` deleting-dtor thunk;
  the only caller-oracle miss found), `Metronome::GetVolume`, `VocalPart::Rollback` and
  `_M_advance<TambourineGem*>` (retail tail-merged into other functions).
- **Held:** `GetTestQuality@CalibrationPanel`, `CodaHitChord@GemSmasher`,
  `GetTrackerTypeFromGameType`, `SetFaceOption`, `Ignore@GemManager`, `IsPhraseMarkerAtEnd`,
  `CalculateRemainingTambourineTicks`, `GetStreamSettingsForContext`, `IsPurchasing@MusicLibrary`
  (single witness, fuzzy < 50, no relocation evidence); PlayerBehavior setters `0x826EECA8/B8/D0`.
- **Caller-proven but foreign-pinned (79 rows / 6.3 KB, not worked):** the name is defined in
  another unit's base obj (e.g. the `ObjPtrList<T>` dtors in RockCentral's pin at `0x82286D88+`,
  `SessionSettings`/`MatchmakingSettings` in Matchmaker's, `TourCharLocal` in GemManager's). These
  need re-homing, one A/B each.
- **Caller-proven, name unrecoverable honestly:** `MusicLibraryUnkOp::IsDownloading`
  (`MusicLibraryUnkOp` is our stand-in for `MusicLibraryStore`; the fix is that refactor).
- **Supported identity, body drift < 50 (string set, not named):** `Modifier::CustomLocation`/
  `DefaultEnabled`, `ContentLoadingPanel::ContentDiscovered`, `CustomizePanel::FinishLoad`,
  `OverdriveTracker::ConfigureTrackerSpecificData`/`GetBroadcastDescription`,
  `PerfectSectionTracker::UpdateGoalValueLabel`.
- **Below 100 after repair:** `CalcPhraseScoreMax` 66.9 (loop shape), `GetBandFailCue` 63.9
  (`BandFailCue()` returns `const char*` in retail — `BandSongMetadata.h`), `GetNoteRange` 94.5,
  `HandlePendingGamerRewards` 86.4, `GetIconArt` 84.0, `~NewUserMsg`/`~DataArrayMsg` and
  `~ProfileMgr` (vptr re-store; shared headers), `LeaderboardRow` copy ctor (`os/OnlineID.h`),
  `TrackPanelInterface` ctor 76.0 (our body is 168 B, retail 140 B — the same drift that made it
  look like the ConnectionStatusPanel ctor), `SetlistSize` (its `blr` is a separate 4 B row).
- **Remaining < 200 B population:** 839 rows / 60,080 B — mostly featureless with no caller
  witness, which the rules above deliberately do not name.

## 8. Reproduce

```
python3 ~/tmp/w16hr/census.py                         # population
python3 ~/tmp/w16hr/bind.py                           # binding table -> bind.pkl
python3 ~/tmp/w16hr/bindctl.py                        # the caller-vote control (§2)
python3 ~/tmp/w16hr/callers.py                        # caller classes
python3 tools/anon_proposal_adjudicate.py <props.json> --json-out <adj.json> --independent
python3 ~/tmp/w16hr/decide_call.py <adj> <props> <accept> <oursize>   # caller rule
python3 ~/tmp/w16hr/strmatch.py; python3 ~/tmp/w16hr/decide_str.py <adj> <props> <accept>
python3 ~/tmp/w16hr/bodycmp.py <unit> <name> <addr>   # full-extent compare for carve heads
python3 tools/gated_map_write.py --target scripts/target_symbol_map.json --rows-json <rows.json>
python3 tools/icf_alias_finder.py --validate && python3 tools/map_name_injectivity.py
python3 ~/tmp/w16hr/rows_table.py c0fa121ca           # §6 table
```

Scratch inputs (proposals, adjudications, accept lists, logs) live in `~/tmp/w16hr/`; they are not
committed.
