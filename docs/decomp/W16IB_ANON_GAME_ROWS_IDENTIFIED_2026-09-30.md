# W16-IB — anonymous game-layer rows identified by retail vtables, callers and missing TUs (2026-09-30)

Branch `w16-ib` off main `74ee485ef`. Scope: W16-HO's class A ("no identity found in own unit or
neighbours", `docs/decomp/W16HO_ANON_ROWS_CLASSIFIED_AND_REPAIRED_2026-09-30.md` §2), widened to
every anonymous fuzzy-0 `fn_` row in a `src/band3` / `src/network` (non-Quazal) unit, since the same
instruments apply. Nothing under `src/system/` was touched.

**Result.** 154 rows (28,496 B) now carry names; 74 of them (7,832 B) read fuzzy 100 in-tree. Of the
139 class-A rows still anonymous at `74ee485ef` (56,684 B), 45 (18,040 B) are among them.
Whole-binary A/B over the branch diff: see §5.

## 1. Why class A had "no identity"

W16-HO's candidate pool was the unclaimed functions of the row's own base obj, scored by bytes and
by shared strings/callees. Three things put identifiable rows outside that pool:

1. **Whole retail TUs we never compiled.** Quest.cpp and TourReward.cpp exist in the rb3-Wii oracle
   but not in our tree. Their retail bodies were pinned under Tour.cpp, TourCondition.cpp and
   QuestManager.cpp, where no base obj defines a Quest or TourReward name.
2. **Rows whose name exists but the byte score is low.** A body drifted by TU5 function-local
   statics or other changes scores under the matrix threshold, and a virtual or a call-site-only
   helper shares few strings. The vtable slot and the calling instruction identify it anyway.
3. **Map names on the wrong body.** `??0/??1/??_GTourCondition` were mapped to TourReward's
   bodies. TourCondition's real ctor/dtor/`Init`/`??_G` were anonymous.

## 2. Instruments

- **Retail RTTI vtables** (`~/tmp/w16ib/pe.py`, `vtsweep.py`). All 2,220 vtables whose `[-1]` word is
  a Complete Object Locator were read from `orig/45410914/band.exe`. For each class we compile, our
  `??_7` variant with the most agreeing named slots was taken. Every fuzzy-0 anonymous game-layer
  slot then gets our slot's name (our dump carries the COL at index 0, so our `[k+1]` = retail `k`).
  Alignment was checked by eye on the multi-inheritance classes; e.g. StreakFocusTracker agrees on
  every named slot, and its disagreements are all ICF-folded empty bodies.
- **Caller alignment** (`calleralign.py`). For each anonymous row, every named retail caller was
  diffed with `objdiff-cli`. At each `bl` whose retail target is the row, the base-side callee is a
  proposed name. Kept only when the alignment is `equal`/`diff_arg`, exactly one name is proposed,
  and that name is defined in the row's own unit base obj.
- **Adjudication on retail bytes.** `tools/anon_proposal_adjudicate.py` was run on both batches. Its
  mechanical `CONTRADICTED` fires on `RETAIL_ONLY`/`OURS_ONLY` relocations, which for a drifted body
  is the drift itself (W16-HO §3 reading). A name was refused on a caller contradiction, on a
  name bound elsewhere, or on too little independent support (below).
- **Collateral check after every map write.** Same-tree `report.json` before/after, every row.

## 3. What was done

1. **Quest.cpp and TourReward.cpp ported and wired** (oracle bodies; `objects.json` +2). Retail TU
   boundaries come from RTTI and body contents:
   - TourCondition.cpp starts at `0x8235A408`. Vtable `.?AVTourCondition@@` `0x8203CC6C` has
     slot 0 = `0x8235A438`. `0x8235A408–0x8235A480` moved from QuestManager.
   - Quest.cpp is `0x8235AE68–0x8235B978`, minus `0x8235AFA0` (GetTier, ICF-folded into an
     AsyncFile pin). `~Quest` stores vtable `.?AVQuest@@` `0x8203CDC4`. It moved from TourCondition
     and Tour.
   - TourReward.cpp is `0x82364428–0x82364AA0`. Vtable `.?AVTourReward@@` `0x8203F394` has
     slot 0 = `0x823644B0`. It moved from TourCondition.
   - `Quest::Configure` needed retail's per-use static `Symbol`s and `FindArray` before the virtual
     `mGameRules.Init`: 97.89 → 99.96, then 100 with the fold below.
   - Result: every named row in Quest, TourReward and TourCondition is at 100.
2. **ICF fold declared.** `0x8235A430` is both `TourCondition::Init` and `TourReward::Init`. Retail
   and both of our bodies are `90830004 4E800020`, relocation-free.
3. **Deliberately left anonymous: `0x8235B008` and `0x8235B018`.** They are 8-byte fold survivors
   (`addi r3,r3,0x40` / `lbz r3,0x34`) shared with `TourProgress::GetTourProperties`, SetlistRecord,
   SongMgr, Game::LoadSong and SampleInst360 getters. Naming them charged about 10 foreign callers
   for 16 B of rows (measured, then reverted).
4. **50 rows named by vtable slot.** Refused four:
   - `0x826758E8`: caller contradiction; it is a 12 B fold with `MetaPerformer::GetVenue`.
   - `0x8268B888` and `0x8268E2B0`: retail thunks jump to a different function than ours
     (retail `RemoteBandUser` does not override `Handle`).
   - `0x8258A2B8`.
   Slot-0 `??_E` was renamed to `??_G` only where our obj defines it.
5. **76 rows named by caller alignment.** Refused nine:
   - Five whose scratch diff scored 0, so no byte adjudication was possible: `GetUnitsToken`,
     `HandleEventResponse`, `SongFilePath`, `Prune`, `CalculateRemainingTambourineTicks`.
   - Four with fewer than 3 AGREE and no matched-caller support: `0x825667C8`, which W16-HO also
     held; `Update@CurrentOutfitProvider`; `Success@TrainerChallenge`;
     `SetProGuitarOrBassSongs`.
   `0x82699830` is a deliberately vacated (`null`) key and was left alone. On the forced re-split,
   dtk merged branch-reached over-carve tails into seven newly named rows (`symbols.txt`).
6. **Two bodies repaired** (both rows were anonymous before this lane):
   - `GemManager::GetTypeForGem` has nine statics up front (`bonus` and `miss` are constructed but
     unused) and tests `fillLogic == 1 || == 2`: 47.1 → 99.54. The residue is a register rotation.
   - `CymbalSelectionProvider::ReloadData` builds six statics before `clear()`: 24.7 → **100**.
7. **One alias membership withdrawn**, with a record:
   - The removed spelling is `?SwapUserProfile@OvershellSlot` from the `Splash::ThreadStart` group
     at `0x82742968`.
   - Both retail `bl` callers of SwapUserProfile call `0x825DF9C8`. `0x82742968` has zero `bl`
     callers.
   - The T1 byte identity came from our Wii stub `{ ShowWiiProfileFail(); return false; }`, which has
     ThreadStart's shape.
   - Before the withdrawal, `icf_alias_finder --validate` failed on the new name (1 contradicted).
     After it: PASS, and no row moved.

No invented names: every name above is one our source already declares. The Quest/TourReward
bodies come from the oracle.

## 4. Rows changed

"fuzzy now" is this branch's in-tree `report.json` (`name_check`). Before, every row was an
anonymous `fn_` at fuzzy 0, except the three TourReward renames, which paired with our TourCondition
bodies.

| retail row | size | name | unit | evidence | fuzzy now |
|---|---:|---|---|---|---:|
| `0x8235b078` | 1132 | `?Configure@Quest@@UAAXPAVDataArray@@@Z` | band3/tour/Quest | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x8255de20` | 1076 | `?InitializeTourSafeDiscSongs@AccomplishmentManager@@QAAXXZ` | band3/meta_band/AccomplishmentManager | caller alignment: ContentDone@AccomplishmentManager | 57.83 |
| `0x82b9a3d0` | 872 | `?GetTypeForGem@GemManager@@QAA?AVSymbol@@H@Z` | GemManager | caller alignment: UpdateGemStates@GemManager, AdvanceEnd@GemManager, AdvanceEnd@GemManager | 99.54 |
| `0x826aabc0` | 740 | `?Poll@GemTrainerPanel@@UAAXXZ` | band3/game/GemTrainerPanel | vtable GemTrainerPanel slot 7 | 48.63 |
| `0x82680430` | 628 | `?UpdatePresence@PresenceMgr@@QAAXXZ` | band3/game/PresenceMgr | caller alignment: OnPresenceChange@PresenceMgr, SetNotInGame@PresenceMgr, SetSongID@PresenceMgr | 41.69 |
| `0x82559880` | 588 | `?GetHintStringForSource@AccomplishmentManager@@QBA?AVString@@VSymbol@@@Z` | band3/meta_band/AccomplishmentManager | caller alignment: UpdateAssetHintLabel@AccomplishmentManager | 56.02 |
| `0x826e64a0` | 576 | `?OnMsg@VocalPlayer@@QAA_NABVButtonDownMsg@@@Z` | VocalPlayer | caller alignment: Handle@VocalPlayer | 30.66 |
| `0x826db9f8` | 544 | `?TranslateRelativeTargets@PerfectSectionTracker@@UAAXXZ` | band3/game/PerfectSectionTracker | vtable PerfectSectionTracker slot 1 | 49.82 |
| `0x825a0038` | 528 | `?_M_rehash@?$hashtable@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@@2@@stlpmtx_std@@AAAXI@Z` | BandSongMetadata | caller alignment: resize@?$hashtable | 97.99 |
| `0x8262a198` | 508 | `?GetMatForData@LayerProvider@@QBAPAVRndMat@@H@Z` | band3/meta_band/PatchPanel | caller alignment: Handle@LayerProvider, Mat@LayerProvider | 37.42 |
| `0x82609130` | 472 | `?UpdateProgress@CalibrationPanel@@QAAX_N@Z` | CalibrationPanel | caller alignment: UpdateLabel@CalibrationPanel, UpdateLabel@CalibrationPanel | 46.28 |
| `0x82551b50` | 456 | `?OnMsg@SaveLoadManager@@QAA?AVDataNode@@ABVMCResultMsg@@@Z` | band3/meta_band/SaveLoadManager | caller alignment: Handle@SaveLoadManager | 46.84 |
| `0x82558e38` | 448 | `?UpdateReasonLabelForAward@AccomplishmentManager@@QAAXVSymbol@@PAVUILabel@@@Z` | band3/meta_band/AccomplishmentManager | caller alignment: Handle@AccomplishmentManager | 48.28 |
| `0x8255e710` | 448 | `?ConfigureAccomplishmentData@AccomplishmentManager@@QAAXPAVDataArray@@@Z` | band3/meta_band/AccomplishmentManager | caller alignment: Init@AccomplishmentManager | 21.78 |
| `0x82669398` | 448 | `?ReloadData@CymbalSelectionProvider@@QAAXXZ` | CymbalSelectionProvider | caller alignment: Handle@OvershellSlot, 0CymbalSelectionProvider@ | **100** |
| `0x826f1f90` | 436 | `?GetSloppyPitch@VocalPart@@QBAMMHMAAM@Z` | VocalPart | caller alignment: CouldScoreAgainstPart@VocalPart, ScoreNote@VocalPart | 41.48 |
| `0x82b93cf8` | 428 | `?UpdateFills@GemTrack@@QAAXXZ` | GemTrack | caller alignment: UpdateGems@GemTrack | 54.95 |
| `0x826818d0` | 424 | `?SetPitchCorrectionTarget@GameMicManager@@QAAX_N0MMMMM@Z` | GameMicManager | caller alignment: Poll@VocalPlayer, HookUpFxForMicId@GameMicManager | **100** |
| `0x825872a8` | 412 | `?OnMsg@SessionMgr@@QAA?AVDataNode@@ABVSigninChangedMsg@@@Z` | band3/meta_band/SessionMgr | caller alignment: Handle@SessionMgr | 41.0 |
| `0x826f6ea8` | 392 | `?UpdatePitchHistory@Singer@@QAAXM@Z` | band3/game/Singer | caller alignment: Poll@VocalPlayer | 56.03 |
| `0x826f7598` | 388 | `?AllScoresAreIn@Singer@@QAAXABV?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@@Z` | band3/game/Singer | caller alignment: Poll@VocalPlayer | 84.91 |
| `0x823646e0` | 380 | `?ApplyRewardEntry@TourReward@@QBAXPAVTourProgress@@AAVTourPropertyCollection@@PAVDataArray@@@Z` | band3/tour/TourReward | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x82babd28` | 368 | `?AddHopoTails@Gem@@QAAXVSymbol@@@Z` | band3/bandtrack/Gem | caller alignment: CreateWidgetInstances@Gem | 54.27 |
| `0x8266aac0` | 344 | `?UpdateConditionOptionalData@AccomplishmentConditional@@QAAXAAUAccomplishmentCondition@@PAVDataArray@@@Z` | AccomplishmentConditional | caller alignment: Configure@AccomplishmentConditional | 55.73 |
| `0x826f7030` | 332 | `?SetAssignedPart@Singer@@QAAXHM@Z` | band3/game/Singer | caller alignment: Poll@VocalPlayer | 77.23 |
| `0x826be898` | 328 | `?ShouldPenalizeGem@GemPlayer@@QBA_NH@Z` | GemPlayer | caller alignment: Pass@GemPlayer | 38.77 |
| `0x82569e88` | 324 | `??0InstrumentOutfit@BandCharDesc@@QAA@ABV01@@Z` | band3/meta_band/AssetMgr | caller alignment: EquipAsset@AssetMgr | **100** |
| `0x823f2470` | 324 | `?_M_insert_overflow_aux@?$vector@VMemStream@@V?$StlNodeAlloc@VMemStream@@@stlpmtx_std@@@stlpmtx_std@@IAAXPAVMemStream@@ABV3@ABU__false_type@2@I_N@Z` | SessionMessages | caller alignment: push_back@?$vector | 96.11 |
| `0x825ef4f8` | 320 | `?IsInstrumentAssetType@@YA_NVSymbol@@@Z` | AssetTypes | caller alignment: IsAlreadyLoaded@ClosetMgr, FinalizeChanges@ClosetMgr, SetCurrentOutfitPiece@ClosetMgr | 17.8 |
| `0x825f5970` | 316 | `?SetType@CampaignSongInfoPanel@@UAAXVSymbol@@@Z` | CampaignSongInfoPanel | caller alignment: SetType@CampaignSongInfoPanel | **100** |
| `0x825df9c8` | 300 | `?SwapUserProfile@OvershellSlot@@QAA_NPAVLocalBandUser@@@Z` | OvershellSlot | caller alignment: AttemptSwapUserProfile@OvershellSlot, ConfirmSwapUserProfile@OvershellSlot | 8.44 |
| `0x825a05d8` | 300 | `?_M_initialize_buckets@?$hashtable@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@M@stlpmtx_std@@@2@@stlpmtx_std@@AAAXI@Z` | BandSongMetadata | caller alignment: 0?$hashtable@U?$pair | 20.27 |
| `0x82b90e70` | 296 | `?CrowdRatingDefaultVal@TrackPanel@@UBAMVSymbol@@@Z` | band3/bandtrack/TrackPanel | vtable TrackPanel slot 24 | 43.01 |
| `0x825400e8` | 288 | `?RebuildProfileData@MusicLibrary@@QAAXXZ` | MusicLibrary | caller alignment: OnMsg@MusicLibrary | 59.35 |
| `0x8263bfe0` | 288 | `?OnMsg@StoreMainPanel@@QAA?AVDataNode@@ABVMetadataLoadedMsg@@@Z` | band3/meta_band/StoreMainPanel | caller alignment: Handle@StoreMainPanel | 47.08 |
| `0x825d32a8` | 272 | `??$LoadStdPtr@VSongStatus@@@FixedSizeSaveable@@SAXAAVFixedSizeSaveableStream@@AAV?$hash_map@HPAVSongStatus@@U?$hash@H@stlpmtx_std@@U?$equal_to@H@3@V?$StlNodeAlloc@U?$pair@$$CBHPAVSongStatus@@@stlpmtx_std@@@3@@stlpmtx_std@@HH@Z` | SongStatusMgr | caller alignment: LoadFixed@SongStatusMgr | **100** |
| `0x826bc508` | 260 | `?SetPaused@GemPlayer@@UAAX_N@Z` | GemPlayer | vtable GemPlayer slot 46 | 7.05 |
| `0x82540fc0` | 256 | `?GetStoreOffers@MusicLibrary@@QBAXAAV?$vector@PAVStoreOffer@@V?$StlNodeAlloc@PAVStoreOffer@@@stlpmtx_std@@@stlpmtx_std@@@Z` | MusicLibrary | caller alignment: BuildFilteredSongList@SongSortMgr | 7.09 |
| `0x825d8be0` | 256 | `?IsCymbalSelected@OvershellSlot@@QAA_NVSymbol@@@Z` | OvershellSlot | caller alignment: Handle@OvershellSlot | 32.58 |
| `0x8256a860` | 256 | `?insert_unique_noresize@?$hashtable@U?$pair@$$CBVSymbol@@PAVAsset@@@stlpmtx_std@@VSymbol@@U?$hash@VSymbol@@@2@U?$_HashMapTraitsT@U?$pair@$$CBVSymbol@@PAVAsset@@@stlpmtx_std@@@priv@2@U?$_Select1st@U?$pair@$$CBVSymbol@@PAVAsset@@@stlpmtx_std@@@2@U?$equal_to@VSymbol@@@2@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@PAVAsset@@@stlpmtx_std@@@2@@stlpmtx_std@@QAA?AU?$pair@U?$_Ht_iterator@V?$_Slist_iterator@U?$pair@$$CBVSymbol@@PAVAsset@@@stlpmtx_std@@U?$_Nonconst_traits@U?$pair@$$CBVSymbol@@PAVAsset@@@stlpmtx_std@@@2@@priv@stlpmtx_std@@U?$_NonLocalHashMapTraitsT@U?$pair@$$CBVSymbol@@PAVAsset@@@stlpmtx_std@@@23@@priv@stlpmtx_std@@_N@2@ABU?$pair@$$CBVSymbol@@PAVAsset@@@2@@Z` | band3/meta_band/AssetMgr | caller alignment: _M_insert@?$hashtable | 98.91 |
| `0x823648e8` | 252 | `?ApplyRewardEntry@TourReward@@QBAXPAVTourProgress@@PAVDataArray@@@Z` | band3/tour/TourReward | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x8235b7c0` | 252 | `??0Quest@@QAA@PAVDataArray@@@Z` | band3/tour/Quest | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x82b9c430` | 252 | `?ClearTrackMasks@GemManager@@QAAXXZ` | GemManager | caller alignment: Jump@GemManager, SetupGems@GemManager | 51.0 |
| `0x826d76c0` | 244 | `?BroadcastFocusSuccess@StreakFocusTracker@@UBAXXZ` | FocusTracker | vtable StreakFocusTracker slot 26 | 40.77 |
| `0x82ba0f88` | 236 | `?_M_initialize_map@?$_Deque_base@PAVTubePlate@@V?$StlNodeAlloc@PAVTubePlate@@@stlpmtx_std@@@stlpmtx_std@@IAAXI@Z` | VocalTrack | caller alignment: 0?$_Deque_base@PAVTubePlate | 75.47 |
| `0x825799b0` | 220 | `?ContentMounted@BandSongMgr@@UAAXPBD0@Z` | BandSongMgr | vtable BandSongMgr slot 5 | 1.73 |
| `0x82ba0d38` | 220 | `?_M_initialize_map@?$_Deque_base@PAVLyricPlate@@V?$StlNodeAlloc@PAVLyricPlate@@@stlpmtx_std@@@stlpmtx_std@@IAAXI@Z` | VocalTrack | caller alignment: 0?$_Deque_base@PAVLyricPlate | 99.91 |
| `0x8260a3f8` | 216 | `?OnMsg@CalibrationWelcomePanel@@QAA?AVDataNode@@ABVInputStatusChangedMsg@@@Z` | CalibrationPanel | caller alignment: Handle@CalibrationWelcomePanel, Enter@CalibrationWelcomePanel | 34.39 |
| `0x825d4c90` | 212 | `?Text@BadReviewViewSetting@@UBAXHHPAVUIListLabel@@PAVUILabel@@@Z` | band3/meta_band/ViewSetting | vtable BadReviewViewSetting slot 1 | 93.58 |
| `0x825d70c8` | 208 | `?OnMsg@CriticalUserListener@@QAA?AVDataNode@@ABVLocalUserLeftMsg@@@Z` | CriticalUserListener | caller alignment: Handle@CriticalUserListener | 26.67 |
| `0x825894a8` | 204 | `?ToggleModifierEnabled@ModifierMgr@@QAAXVSymbol@@@Z` | band3/meta_band/ModifierMgr | caller alignment: SetBandNoFail@MetaPerformer, Handle@ModifierMgr, EnableAutoVocals@OvershellPanel | 27.61 |
| `0x825da2e0` | 204 | `?IsValidControllerType@OvershellSlot@@QAA_NW4ControllerType@@@Z` | OvershellSlot | caller alignment: FindSlotForRemoteUser@OvershellPanel, RefreshJoinableUsers@OvershellPanel | 45.37 |
| `0x8260d578` | 204 | `?SetGlasses@CharacterCreatorPanel@@QAAXVSymbol@@@Z` | CharacterCreatorPanel | caller alignment: Handle@CharacterCreatorPanel | 40.04 |
| `0x8260d740` | 204 | `?SetHair@CharacterCreatorPanel@@QAAXVSymbol@@@Z` | CharacterCreatorPanel | caller alignment: Handle@CharacterCreatorPanel | 40.04 |
| `0x826aa968` | 176 | `?GetLastGameGemInSection@GemTrainerPanel@@QBAABVGameGem@@AAH@Z` | band3/game/GemTrainerPanel | caller alignment: ShouldMissCauseFail@GemTrainerPanel | 87.23 |
| `0x825d4ba8` | 160 | `?GetCurrentStatus@BadReviewViewSetting@@UBAPBDXZ` | band3/meta_band/ViewSetting | vtable BadReviewViewSetting slot 22 | 97.38 |
| `0x82617bd0` | 160 | `?FinishLoad@CustomizePanel@@UAAXXZ` | CustomizePanel | vtable CustomizePanel slot 14 | 31.82 |
| `0x82697810` | 156 | `??$__find_if@PAU?$pair@HM@stlpmtx_std@@UPartMatches@?A0x0a60b722@@@stlpmtx_std@@YAPAU?$pair@HM@0@PAU10@0UPartMatches@?A0x0a60b722@@ABUrandom_access_iterator_tag@0@@Z` | band3/game/Stats | caller alignment: SetPartPercentage@SingerStats | **100** |
| `0x82652d68` | 148 | `?UpdateMatchmakingSettings@BandMatchmaker@@UAAXXZ` | Matchmaker | vtable BandMatchmaker slot 2 | **100** |
| `0x826d7878` | 148 | `?GetBroadcastDescription@AccuracyFocusTracker@@UBA?AVDataArrayPtr@@XZ` | FocusTracker | vtable AccuracyFocusTracker slot 15 | 45.27 |
| `0x826d8ac0` | 148 | `?GetContributionToken@StreakFocusTracker@@UBA?AVSymbol@@H@Z` | FocusTracker | vtable StreakFocusTracker slot 28 | 0.41 |
| `0x826dd8d8` | 148 | `?GetBroadcastDescription@OverdriveTracker@@UBA?AVDataArrayPtr@@XZ` | OverdriveTracker | vtable OverdriveTracker slot 15 | 45.27 |
| `0x826984a0` | 148 | `??$__linear_insert@PAU?$pair@HM@stlpmtx_std@@U12@UPartPercentageSorter@SingerStats@@@stlpmtx_std@@YAXPAU?$pair@HM@0@0U10@UPartPercentageSorter@SingerStats@@@Z` | band3/game/Stats | caller alignment: $__insertion_sort@PAU?$pair | **100** |
| `0x825d3148` | 136 | `?ClearLeastImportantSongStatusEntry@SongStatusMgr@@QAAXXZ` | SongStatusMgr | caller alignment: CreateOrAccessSongStatus@SongStatusMgr | 32.32 |
| `0x826f6e20` | 136 | `?SuddenOctaveShift@Singer@@QBAHM@Z` | band3/game/Singer | caller alignment: Poll@VocalPlayer | 30.88 |
| `0x82588e80` | 132 | `?CustomLocation@Modifier@@QBA_NXZ` | band3/meta_band/ModifierMgr | caller alignment: 0ModifierMgr@ | 39.0 |
| `0x82589108` | 132 | `?DefaultEnabled@Modifier@@QBA_NXZ` | band3/meta_band/ModifierMgr | caller alignment: 0ModifierMgr@ | 39.0 |
| `0x8264f6a0` | 128 | `?ContentMounted@LicenseMgr@@UAAXPBD0@Z` | band3/meta_band/LicenseMgr | vtable LicenseMgr slot 5 | 1.25 |
| `0x825d4268` | 124 | `??0SongStatusMgr@@QAA@PAVLocalBandUser@@PAVBandSongMgr@@@Z` | SongStatusMgr | caller alignment: 0BandProfile@ | 93.39 |
| `0x823644f8` | 120 | `?ApplyAddReward@TourReward@@QBAXPAVTourProgress@@AAVTourPropertyCollection@@PAVDataArray@@@Z` | band3/tour/TourReward | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x82364570` | 120 | `?ApplySubtractReward@TourReward@@QBAXPAVTourProgress@@AAVTourPropertyCollection@@PAVDataArray@@@Z` | band3/tour/TourReward | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x823645e8` | 120 | `?ApplyMultiplyReward@TourReward@@QBAXPAVTourProgress@@AAVTourPropertyCollection@@PAVDataArray@@@Z` | band3/tour/TourReward | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x82364660` | 120 | `?ApplyDivideReward@TourReward@@QBAXPAVTourProgress@@AAVTourPropertyCollection@@PAVDataArray@@@Z` | band3/tour/TourReward | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x82364a28` | 120 | `?Apply@TourReward@@QBAXPAVTourProgress@@@Z` | band3/tour/TourReward | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x82614508` | 116 | `?ContentDiscovered@ContentLoadingPanel@@UAA_NVSymbol@@@Z` | band3/meta_band/ContentLoadingPanel | vtable ContentLoadingPanel slot 2 | 31.45 |
| `0x826da320` | 112 | `?UpdateGoalValueLabel@PerfectSectionTracker@@UBAXAAVUILabel@@@Z` | band3/game/PerfectSectionTracker | vtable PerfectSectionTracker slot 2 | 29.93 |
| `0x826dd380` | 108 | `?ConfigureTrackerSpecificData@OverdriveTracker@@UAAXPBVDataArray@@@Z` | OverdriveTracker | vtable OverdriveTracker slot 6 | 34.19 |
| `0x826f6bb8` | 108 | `?DisableAmbiguousPart@Singer@@QAAXHH@Z` | band3/game/Singer | caller alignment: Poll@VocalPlayer | 72.67 |
| `0x82bb0660` | 108 | `?WriteTailVerts@@YAXAAPAVVert@RndMesh@@PBV12@1MMMM@Z` | Tail | caller alignment: UpdateVerts@Tail, UpdateVerts@Tail, UpdateVerts@Tail | **100** |
| `0x8235ae68` | 100 | `??1Quest@@UAA@XZ` | band3/tour/Quest | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x8235b6e8` | 100 | `?GetLongDescription@Quest@@QBA?AVSymbol@@XZ` | band3/tour/Quest | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x8235b750` | 100 | `?GetDescription@Quest@@QBA?AVSymbol@@XZ` | band3/tour/Quest | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x822a6670` | 100 | `?insert@?$list@VOldMatOption@@V?$StlNodeAlloc@VOldMatOption@@@stlpmtx_std@@@stlpmtx_std@@QAA?AU?$_List_iterator@VOldMatOption@@U?$_Nonconst_traits@VOldMatOption@@@stlpmtx_std@@@2@U32@ABVOldMatOption@@@Z` | band3/bandtrack/Gem | caller alignment: $?0U?$_List_iterator@VOldMatOption, 0?$list@VOldMatOption, resize@?$list | **100** |
| `0x82614df0` | 100 | `?InPreviewState@CustomizePanel@@QBA_NXZ` | CustomizePanel | caller alignment: PreviewAsset@CustomizePanel | **100** |
| `0x82573828` | 100 | `??1BandScreen@@UAA@XZ` | MetaPanel | caller alignment: _GSigninScreen@ | 87.56 |
| `0x8260dbe8` | 100 | `?SetFaceOption@CharacterCreatorPanel@@QAAXH@Z` | CharacterCreatorPanel | caller alignment: Handle@CharacterCreatorPanel | **100** |
| `0x826da3b8` | 96 | `?UpdateCurrentValueLabel@PerfectSectionTracker@@UBAXAAVUILabel@@@Z` | band3/game/PerfectSectionTracker | vtable PerfectSectionTracker slot 3 | 15.42 |
| `0x82b7dc18` | 96 | `?FindMat@TourDescProvider@@QBAPAVRndMat@@ABVString@@@Z` | TourDescPanel | caller alignment: Mat@TourDescProvider, Mat@TourDescProvider, Mat@TourDescProvider | **100** |
| `0x82364450` | 92 | `?ApplyRewardValue@TourReward@@QBAXPAVTourProgress@@AAVTourPropertyCollection@@VSymbol@@M@Z` | band3/tour/TourReward | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x82666d18` | 92 | `?IsActive@CharProvider@@UBA_NH@Z` | band3/meta_band/CharProvider | vtable CharProvider slot 11 | **100** |
| `0x825d1238` | 92 | `??0SongStatus@@QAA@H@Z` | SongStatusMgr | caller alignment: CreateOrAccessSongStatus@SongStatusMgr | **100** |
| `0x825d4498` | 92 | `??0MusicLibraryUpsellViewSetting@@QAA@XZ` | band3/meta_band/ViewSetting | caller alignment: 0ViewSettingsProvider@ | **100** |
| `0x825d44f8` | 92 | `??0BadReviewViewSetting@@QAA@XZ` | band3/meta_band/ViewSetting | caller alignment: 0ViewSettingsProvider@ | **100** |
| `0x826bdee0` | 92 | `?FindHeldNoteFromSlot@GemPlayer@@QAAPAVHeldNote@@H@Z` | GemPlayer | caller alignment: FretButtonUp@GemPlayer | 65.22 |
| `0x82669c18` | 92 | `?CheckHitBRECondition@AccomplishmentSongConditional@@QBA_NPAVSongStatusMgr@@VSymbol@@ABUAccomplishmentCondition@@@Z` | AccomplishmentSongConditional | caller alignment: CheckConditionsForSong@AccomplishmentSongConditional | 93.7 |
| `0x82669c78` | 92 | `?CheckAllDoubleAwesomesCondition@AccomplishmentSongConditional@@QBA_NPAVSongStatusMgr@@VSymbol@@ABUAccomplishmentCondition@@@Z` | AccomplishmentSongConditional | caller alignment: CheckConditionsForSong@AccomplishmentSongConditional | 93.7 |
| `0x82669cd8` | 92 | `?CheckAllTripleAwesomesCondition@AccomplishmentSongConditional@@QBA_NPAVSongStatusMgr@@VSymbol@@ABUAccomplishmentCondition@@@Z` | AccomplishmentSongConditional | caller alignment: CheckConditionsForSong@AccomplishmentSongConditional | 93.7 |
| `0x82669d38` | 92 | `?CheckPerfectDrumRollsCondition@AccomplishmentSongConditional@@QBA_NPAVSongStatusMgr@@VSymbol@@ABUAccomplishmentCondition@@@Z` | AccomplishmentSongConditional | caller alignment: CheckConditionsForSong@AccomplishmentSongConditional | 93.7 |
| `0x82669d98` | 92 | `?CheckFullComboCondition@AccomplishmentSongConditional@@QBA_NPAVSongStatusMgr@@VSymbol@@ABUAccomplishmentCondition@@@Z` | AccomplishmentSongConditional | caller alignment: CheckConditionsForSong@AccomplishmentSongConditional | 93.7 |
| `0x82593fa8` | 88 | `?GetSecretDescription@Accomplishment@@QBA?AVSymbol@@XZ` | Accomplishment | caller alignment: GetAccomplishmentDescription@AccomplishmentPanel | 15.0 |
| `0x826c2348` | 88 | `?IteratorAt@?$TickedInfoCollection@VString@@@@QBAPBV?$TickedInfo@VString@@@@H_N@Z` | GemPlayer | caller alignment: UpdateCrowdMeter@GemPlayer | **100** |
| `0x826e3aa8` | 88 | `?IsNetOrSpoofed@VocalPlayer@@QBA_NXZ` | VocalPlayer | caller alignment: SetTrack@VocalPlayer | **100** |
| `0x8268ef78` | 80 | `?GetLocalBandUser@RemoteBandUser@@UBAPAVLocalBandUser@@XZ` | BandUser | caller alignment: GetLocalBandUser@RemoteBandUser | 7.0 |
| `0x82362bd0` | 80 | `?GetToursPlayed@TourProgress@@QBAHVSymbol@@@Z` | TourProgress | caller alignment: Mat@TourDescProvider, UpdateExtendedCustom@TourDescProvider, UpdateExtendedText@TourDescProvider | **100** |
| `0x8235af50` | 76 | `?GetDisplayName@Quest@@QBA?AVSymbol@@XZ` | band3/tour/Quest | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x8235b020` | 76 | `??_GQuest@@UAAPAXI@Z` | band3/tour/Quest | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x823f3980` | 76 | `??_GMatchmakingSettings@@UAAPAXI@Z` | Matchmaker | vtable MatchmakingSettings slot 0 | **100** |
| `0x825ab098` | 76 | `??_GLockResponseMsg@@UAAPAXI@Z` | band3/meta_band/LockStepMgr | vtable LockResponseMsg slot 0 | **100** |
| `0x825addb8` | 76 | `??_GNonDestructiveTransitionEvent@@UAAPAXI@Z` | WaitingUserGate | vtable NonDestructiveTransitionEvent slot 0 | **100** |
| `0x825e9ac0` | 76 | `??_GAccomplishmentSongListConditional@@UAAPAXI@Z` | band3/meta_band/AccomplishmentSongListConditional | vtable AccomplishmentSongListConditional slot 0 | **100** |
| `0x82680d70` | 76 | `??_GPresenceMgr@@UAAPAXI@Z` | band3/game/PresenceMgr | vtable PresenceMgr slot 0 | **100** |
| `0x824f6ec8` | 76 | `?GetIsDiskSong@RockCentral@@QAA_NH@Z` | RockCentral | caller alignment: SyncAvailableSongs@RockCentral, SyncAvailableSongs@RockCentral | 85.26 |
| `0x823e2f68` | 76 | `??1UpdateUserDataMsg@@UAA@XZ` | network/net/NetSession | caller alignment: _GUpdateUserDataMsg@, UpdateUserData@NetSession | 83.63 |
| `0x82609bf0` | 72 | `?DataIndex@CalibrationModesProvider@@UBAHVSymbol@@@Z` | CalibrationPanel | vtable CalibrationModesProvider slot 9 | **100** |
| `0x826bc290` | 72 | `?GetNumStars@GemPlayer@@UBAHXZ` | GemPlayer | vtable GemPlayer slot 9 | 80.0 |
| `0x82b940c0` | 72 | `?SetGemsEnabledByPlayer@GemTrack@@UAAXXZ` | GemTrack | vtable GemTrack slot 55 | 59.72 |
| `0x823ebc90` | 72 | `?GetInstanceType1Delegator@?A0x50e10b5e@@YAPAXXZ` | NetworkEmulator | caller alignment: 0NetworkEmulator@, 0NetworkEmulator@ | **100** |
| `0x826e50c0` | 72 | `?HandleDeactivateVolume@VocalPlayer@@QAAXW4JoypadButton@@@Z` | VocalPlayer | caller alignment: OnMsg@VocalPlayer | **100** |
| `0x8235a438` | 68 | `??_GTourCondition@@UAAPAXI@Z` | band3/tour/TourCondition | TourCondition RTTI vtable 0x8203CC6C slot 0 = 0x8235A438; ctor/dtor store that vtable | **100** |
| `0x823644b0` | 68 | `??_GTourReward@@UAAPAXI@Z` | band3/tour/TourReward | RENAMED from ...TourCondition: TourReward RTTI vtable 0x8203F394 slot 0 = 0x823644B0; bodies store that vtable | **100** |
| `0x826a26e0` | 64 | `?GetNumStars@Player@@UBAHXZ` | band3/game/Player | vtable Player slot 9 | 48.75 |
| `0x826a2720` | 64 | `?GetNumStarsFloat@Player@@UBAMXZ` | band3/game/Player | vtable Player slot 10 | 48.75 |
| `0x825bc008` | 60 | `?ByteCode@VerifyBuildVersionMsg@@UBAEXZ` | band3/meta_band/MetaNetMsgs | vtable VerifyBuildVersionMsg slot 6 | **100** |
| `0x825d4da8` | 60 | `?SelectOption@BadReviewViewSetting@@UAAXH@Z` | band3/meta_band/ViewSetting | vtable BadReviewViewSetting slot 28 | **100** |
| `0x8235afb8` | 56 | `?HasCustomIntro@Quest@@QBA_NXZ` | band3/tour/Quest | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x826d6318` | 48 | `?Restart_@FocusTracker@@UAAXXZ` | FocusTracker | vtable FocusTracker slot 11 | **100** |
| `0x8268b8b8` | 32 | `?IsNullUser@BandUser@@$R4BI@7PPPPPPPM@JI@BA_NXZ` | BandUser | vtable RemoteBandUser slot 28 | 99.38 |
| `0x8235a408` | 24 | `??0TourCondition@@QAA@XZ` | band3/tour/TourCondition | TourCondition RTTI vtable 0x8203CC6C slot 0 = 0x8235A438; ctor/dtor store that vtable | **100** |
| `0x82364428` | 24 | `??0TourReward@@QAA@XZ` | band3/tour/TourReward | RENAMED from ...TourCondition: TourReward RTTI vtable 0x8203F394 slot 0 = 0x823644B0; bodies store that vtable | **100** |
| `0x825d4de8` | 24 | `?StartingOption@BadReviewViewSetting@@UBAHXZ` | band3/meta_band/ViewSetting | vtable BadReviewViewSetting slot 29 | **100** |
| `0x82364ac0` | 20 | `?LoadFixed@QuestJournal@@UAAXAAVFixedSizeSaveableStream@@H@Z` | QuestJournal | vtable QuestJournal slot 2 | **100** |
| `0x82617e10` | 20 | `?ContentDone@CustomizePanel@@UAAXXZ` | CustomizePanel | vtable CustomizePanel slot 9 | 34.0 |
| `0x826cfa88` | 20 | `?DataSymbol@PracticeSectionProvider@@UBA?AVSymbol@@H@Z` | band3/game/PracticeSectionProvider | vtable PracticeSectionProvider slot 8 | 0.0 |
| `0x8235a420` | 16 | `??1TourCondition@@UAA@XZ` | band3/tour/TourCondition | TourCondition RTTI vtable 0x8203CC6C slot 0 = 0x8235A438; ctor/dtor store that vtable | **100** |
| `0x82364440` | 16 | `??1TourReward@@UAA@XZ` | band3/tour/TourReward | RENAMED from ...TourCondition: TourReward RTTI vtable 0x8203F394 slot 0 = 0x823644B0; bodies store that vtable | **100** |
| `0x823ec848` | 16 | `?Export@MsgSource@@$4PPPPPPPM@FA@AAXPAVDataArray@@_N@Z` | Server | vtable Server slot 14 | **100** |
| `0x823ec868` | 16 | `?SyncProperty@MsgSource@@$4PPPPPPPM@FA@AA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | Server | vtable Server slot 7 | **100** |
| `0x823ec888` | 16 | `?Replace@MsgSource@@$4PPPPPPPM@FA@AAXPAVObjRef@@PAVObject@Hmx@@@Z` | Server | vtable Server slot 2 | **100** |
| `0x824cee00` | 16 | `?Export@RndDir@@$4PPPPPPPM@BME@AAXPAVDataArray@@_N@Z` | band3/meta_band/ViewSetting | vtable WorldDir slot 14 | **100** |
| `0x825bc048` | 16 | `?Save@VerifyBuildVersionMsg@@UBAXAAVBinStream@@@Z` | band3/meta_band/MetaNetMsgs | vtable VerifyBuildVersionMsg slot 1 | **100** |
| `0x825bc058` | 16 | `?Load@VerifyBuildVersionMsg@@UAAXAAVBinStream@@@Z` | band3/meta_band/MetaNetMsgs | vtable VerifyBuildVersionMsg slot 2 | **100** |
| `0x826b2518` | 16 | `?Load@UIPanel@@$4PPPPPPPM@CM@AAXAAVBinStream@@@Z` | PracticePanel | vtable PracticePanel slot 10 | **100** |
| `0x8235aff0` | 12 | `?GetSuccessSymbol@Quest@@QBA?AVSymbol@@XZ` | band3/tour/Quest | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x823ec8c8` | 12 | `?Handle@Server@@$4PPPPPPPM@A@AA?AVDataNode@@PAVDataArray@@_N@Z` | Server | vtable Server slot 6 | **100** |
| `0x82b7cc48` | 12 | `?SetType@TourDescPanel@@$4PPPPPPPM@A@AAXVSymbol@@@Z` | TourDescPanel | vtable TourDescPanel slot 5 | **100** |
| `0x82b7ef78` | 12 | `?Handle@TourDescPanel@@$4PPPPPPPM@A@AA?AVDataNode@@PAVDataArray@@_N@Z` | TourDescPanel | vtable TourDescPanel slot 6 | **100** |
| `0x82b90760` | 12 | `?GetNumPlayers@TrackPanel@@UBAHXZ` | band3/bandtrack/TrackPanel | vtable TrackPanel slot 17 | **100** |
| `0x8235a430` | 8 | `?Init@TourCondition@@QAAXPBVDataArray@@@Z` | band3/tour/TourCondition | TourCondition RTTI vtable 0x8203CC6C slot 0 = 0x8235A438; ctor/dtor store that vtable | **100** |
| `0x8235afa8` | 8 | `?GetWeight@Quest@@QBAMXZ` | band3/tour/Quest | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x8235afb0` | 8 | `?GetPrereqs@Quest@@QBAPBVTourCondition@@XZ` | band3/tour/Quest | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x8235b000` | 8 | `?GetGameRules@Quest@@QBAPBVTourQuestGameRules@@XZ` | band3/tour/Quest | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x8235b010` | 8 | `?GetFailureReward@Quest@@QBAPBVTourReward@@XZ` | band3/tour/Quest | retail TU re-homed + ported (Quest/TourReward RTTI vtable, body offsets) | **100** |
| `0x8257ad10` | 8 | `?ContentAltDirs@BandSongMgr@@UAAPAV?$vector@VString@@V?$StlNodeAlloc@VString@@@stlpmtx_std@@@stlpmtx_std@@XZ` | BandSongMgr | vtable BandSongMgr slot 12 | **100** |
| `0x82594020` | 8 | `?ShowBestAfterEarn@Accomplishment@@UBAEXZ` | Accomplishment | vtable Accomplishment slot 2 | **100** |

## 5. Measurement

AB_RESULT

## 6. Left, with identities where known

- **TU5 classes whose functions are identified but have no body in our tree**, so no name can pair:
  - `MusicLibraryStore::Poll` = `0x825BDCB8` (1,448 B) and `Finish` = `0x825BCA38`, from caller
    alignment against `MusicLibrary::Poll`. Our `MusicLibraryUnkOp` is a deliberate stub (see
    `MusicLibrary.h`).
  - `XboxEntityUploader` (retail RTTI, vtable `0x82085054`, 29 slots) owns `0x82508850–0x8250AF08`
    in the RockCentral pin, including `0x82509938` (1,736 B, slot 27) and slots 23–25 and 28. The
    class is in neither oracle.
  - `AppInlineHelp` `0x82603DE0` (1,696 B) is a `get_slot_*`/`add_autoplayer` handler with local
    statics.
- **Re-home candidates**: the name is defined, but in a different unit's base obj than the pin
  (50 rows / 4,104 B from the vtable sweep, `~/tmp/w16ib/vtrehome.json`; 76 / 15,268 B from caller
  alignment). Strongest:
  - PerfectSectionTracker `ConfigureTrackerSpecificData`, `GetPlayerContributionString` and
    `GetCurrentValue` (`0x826D95D0`/`0x826D9790`/`0x826D9780`) sit in the FocusTracker pin, and
    `GetBroadcastDescription` (`0x826D9F58`) in RockCentral's.
  - `SaveLoadStatusPanel::Draw` and `FinishLoad` (`0x826322C0`/`0x82631E00`) sit in the
    SelectDifficultyPanel pin.
  - `PerfectSectionTracker::CheckForCompletedSections` (`0x826D9938`, 964 B) sits in the RockCentral
    pin.
  Re-homing is not metric-neutral, so it needs its own A/B.
- **Biggest named rows still below 100**, in size × gap order:
  - `InitializeTourSafeDiscSongs` 57.8. Retail builds literal + storage pairs per `HasPart`; a
    different pattern from the statics fix.
  - `OnMsg(ButtonDownMsg)@VocalPlayer` 30.7.
  - `GemTrainerPanel::Poll` 48.6.
  - `UpdatePresence` 41.7.
  - `ConfigureAccomplishmentData` 21.8.

## 7. Side findings (tooling)

- **A bare `python3 configure.py` in a `~/tmp` worktree resolves dtk and objdiff to stale sibling
  copies under `~/tmp/jeff` and `~/tmp/objdiff`.** It does not use the live fleet binaries.
  `setup_worktree.sh` passes `--dtk/--objdiff/--wrapper` explicitly; re-run configure with those
  same arguments.
- **objcache served an object that a real compile does not produce.** `mtx.cpp` with identical
  source, headers, dependency list and command compiles to `FastInvert` = 248 B with
  `OBJCACHE=off`, in two trees. One tree had been served a cached 232 B object, and that object
  scores 99.5 against retail. The per-row comparisons in this lane therefore used a same-tree
  baseline only. `ab_measure` is unaffected: both legs run in one tree.
  Not investigated further; filed for the objcache owner.

## 8. Reproduce

```
python3 ~/tmp/w16ib/xref.py; python3 ~/tmp/w16ib/survey.py            # xrefs + class-A dossier
python3 ~/tmp/w16ib/vtsweep.py                                         # RTTI vtable sweep
python3 ~/tmp/w16ib/calleralign.py 64                                  # caller alignment
python3 tools/anon_proposal_adjudicate.py ~/tmp/w16ib/props_vt.json --json-out ~/tmp/w16ib/adj_vt.json
python3 tools/anon_proposal_adjudicate.py ~/tmp/w16ib/props_ca.json --json-out ~/tmp/w16ib/adj_ca.json
python3 tools/gated_map_write.py --target scripts/target_symbol_map.json --rows-json <rows>
python3 tools/map_name_injectivity.py && python3 tools/icf_alias_finder.py --validate
python3 tools/ab_measure.py --worktree <clean wt at 74ee485ef> --patch <74ee485ef..w16-ib minus symbols.txt>
```
