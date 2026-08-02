# band3 porting worklist — net-new Wii→Xenon identities (DC3-cannot-provide)

**Generated:** `tools/gen_band3_port_worklist.py` (regenerable). **Source:** `ghidriff_identities.json` (ACCEPT tier) minus `scripts/target_symbol_map.json`.
**Data feed:** `band3_port_worklist.json` (machine-readable, one row per fn; gitignored/regenerable).

> **Scope note, 2026-08-02 — this ranking is for the X360 MATCH, and that framing is
> unchanged and correct.** Every TU here is under `src/band3/`. A second, **disjoint**
> ranking now exists for the *native* port —
> [`band3-native-unblock-priority-2026-08-02.md`](band3-native-unblock-priority-2026-08-02.md)
> — which ranks by *native-unblock value* (object-factory misses retired ÷ wiring cost).
> **The two share zero TUs.** The 14 classes blocking every RB3 venue root live in
> `src/system/{bandobj,synth,world,ui}`, **none in `src/band3/`** (X4a's contrary claim
> is retracted; see `x4b-animation-2026-08-02.md` §4). So:
> - Picking a TU **to match**? Use this file. Nothing below is superseded.
> - Picking a TU **to unblock the native venue/render milestone**? This file will not
>   help — use the native-unblock doc.
>
> Stated because the failure mode is silent: this ranking scores `BandCamShot`
> invisible (not `src/band3/`, and already 251/295 fns matched) while it is ~90% of the
> native blocker; conversely `MusicLibrary.cpp` at rank 1 contributes nothing to it.

## What this is

232 RB3 **game-code** functions across **93 TUs**, each pinned to a specific Wii (Bank-8, CodeWarrior-mangled) function by the forked-ghidriff/BSim Wii→Xenon identity pipeline. These are **net-new**: their Xenon address is NOT yet in the production pairing set (`target_symbol_map.json`), and they live in band3 TUs the active class-A port has **not yet reached**. band3 is RB3-specific gameplay/scoring/song/tracker code — **DC3 (Dance Central 3, no Rock Band gameplay) fundamentally cannot identify these.** This is the irreplaceable core of the Wii→Xenon lever.

**This is a targeting/porting worklist + per-fn identity oracle, NOT a `target_symbol_map.json` injection.** The TUs aren't compiled yet, so there is no MSVC symbol to pair against; and our `wii_symbol` is CW/MWCC-mangled, not MSVC-mangled — injecting it as a map key would mis-pair objdiff at our ~0.90 precision (actively harmful). Use this to pick which TU to port next and, when porting, to name each function from the Wii body. Both outputs are additive + reversible.

## How to consume

- **Pick the next TU** from the ranking below (highest yield × certainty first). The `wf_classa_harvest.js` Scan stage / coordinator picks the next band3 TU from this ranking instead of a blind string-anchor guess.
- **Cross-check `OWN` attribution.** A function this worklist pins to a TU at `high`/`bsim≥30` confidence is strong independent evidence for the Validate stage's `OWN` verdict (better than the near-random `unified_id_rb3wii.json` oracle).
- **Name from the Wii body.** In the rb3 repo: `bin/analyze-function <wii_symbol>` shows the Bank-8-accurate body + real arg shape; `wii_addr_bank8` is the Bank-8 address.
- **fn_resolver T4b** (`ghidriff_wii_b8`) already serves all 978 identities for per-address resolution; this worklist adds the missing **TU-priority + per-TU member roster**.

## Confidence strata (the measured prior)

Band3 human-judged precision (round 2, n=30) = **0.900**. Totals here: **20 high** · **27 bsim≥30** · **92 bsim20-30** · **93 bsim15-20**.

- **high** — `ExactInstructions`/`SwitchSig`/`Implied`/`SymbolsHash`, or BSim simconf ≥ 30. The safest-first targets.
- **bsim≥30 / bsim20-30 / bsim15-20** — BSim similarity×confidence bands; lower = vet harder.

**Dominant failure mode (~10%): same-TU sibling aliasing.** Near-identical template/sibling bodies differing only in a type-tag immediate (e.g. `kDataFloat` vs `kDataInt`) or an STL node-size literal, or a hash-shape match a string later refutes. When confirming a name, diff the small immediates / node-size literals and referenced strings against the Wii body.

## HIGH-confidence subset (safest first — verify these names with most trust)

| Xenon addr | TU | src | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827a9768` | AccomplishmentProgress.o | `src/band3/meta_band/AccomplishmentProgress.cpp` | high | ExactInstructionsFunctionHasher | AccomplishmentProgress::SendHardCoreStatusUpdateToRockCentral(...) | `SendHardCoreStatusUpdateToRockCentral__22AccomplishmentProgressFv` |
| `0x8267c830` | Band.o | `src/band3/game/Band.cpp` | high | ExactInstructionsFunctionHasher | Band const::EnergyCrowdBoost(...) | `EnergyCrowdBoost__4BandCFv` |
| `0x825a8520` | BandMachine.o | `src/band3/meta_band/BandMachine.cpp` | high | ExactInstructionsFunctionHasher | LocalBandMachine::SetPrimaryMetaScore(...) | `SetPrimaryMetaScore__16LocalBandMachineFi` |
| `0x826d8b80` | BandMachineMgr.o | `src/band3/meta_band/BandMachineMgr.cpp` | high | ExactInstructionsFunctionHasher | @unnamed@BandMachineMgr_cpp@::SyncLocalMachineMsg::Dispatch(...) | `Dispatch__Q228@unnamed@BandMachineMgr_cpp@19SyncLocalMachineMsgFv` |
| `0x825508c8` | ClosetMgr.o | `src/band3/meta_band/ClosetMgr.cpp` | high | ExactInstructionsFunctionHasher | ClosetMgr::ShowClothes(...) | `ShowClothes__9ClosetMgrFv` |
| `0x82550d58` | ClosetMgr.o | `src/band3/meta_band/ClosetMgr.cpp` | high | ExactInstructionsFunctionHasher | ClosetMgr::ResetPatches(...) | `ResetPatches__9ClosetMgrFv` |
| `0x82b7caa8` | Lyric.o | `src/band3/bandtrack/Lyric.cpp` | high | ExactInstructionsFunctionHasher | Lyric const::EndPos(...) | `EndPos__5LyricCFv` |
| `0x82655768` | MainHubMessageProvider.o | `src/band3/meta_band/MainHubMessageProvider.cpp` | high | ExactInstructionsFunctionHasher | MainHubMessageProvider::ClearData(...) | `ClearData__22MainHubMessageProviderFv` |
| `0x8252c728` | MusicLibrary.o | `src/band3/meta_band/MusicLibrary.cpp` | high | SwitchSigHasher | MusicLibrary const::DifficultySortPart(...) | `DifficultySortPart__12MusicLibraryCFv` |
| `0x82672b08` | NetGameMsgs.o | `src/band3/game/NetGameMsgs.cpp` | high | ExactInstructionsFunctionHasher | TourHideShowFiltersMsg::TourHideShowFiltersMsg(...) | `__ct__22TourHideShowFiltersMsgFb` |
| `0x825bfb80` | OvershellSlot.o | `src/band3/meta_band/OvershellSlot.cpp` | high | ExactInstructionsFunctionHasher | OvershellSlot const::InOverrideFlow(...) | `InOverrideFlow__13OvershellSlotCF21OvershellOverrideFlow` |
| `0x82353b40` | Performer.o | `src/band3/game/Performer.cpp` | high | ExactInstructionsFunctionHasher | Stats const::GetVocalPartPercentage(...) | `GetVocalPartPercentage__5StatsCFi` |
| `0x82532198` | ProfileMgr.o | `src/band3/meta_band/ProfileMgr.cpp` | high | ExactInstructionsFunctionHasher | ProfileMgr::GlobalOptionsNeedsSave(...) | `GlobalOptionsNeedsSave__10ProfileMgrFv` |
| `0x823527e0` | QuestJournal.o | `src/band3/tour/QuestJournal.cpp` | high | ExactInstructionsFunctionHasher | QuestJournal::HandleDataChange(...) | `HandleDataChange__12QuestJournalFv` |
| `0x825a66e8` | SongSort.o | `src/band3/meta_band/SongSort.cpp` | high | ExactInstructionsFunctionHasher | NodeSort const::GetShortcutIx(...) | `GetShortcutIx__8NodeSortCFP8SortNode` |
| `0x826798b0` | Stats.o | `src/band3/game/Stats.cpp` | high | Implied Match | SingerStats const::GetRankData(...) | `GetRankData__11SingerStatsCFi` |
| `0x826dbaa8` | TambourineManager.o | `src/band3/game/TambourineManager.cpp` | high | ExactInstructionsFunctionHasher | TambourineManager const::TambourineGems(...) | `TambourineGems__17TambourineManagerCFv` |
| `0x82357450` | TourSavable.o | `src/band3/tour/TourSavable.cpp` | high | ExactInstructionsFunctionHasher | TourSavable::SetDirty(...) | `SetDirty__11TourSavableFbi` |
| `0x82357490` | TourSavable.o | `src/band3/tour/TourSavable.cpp` | high | ExactInstructionsFunctionHasher | TourSavable::SaveLoadComplete(...) | `SaveLoadComplete__11TourSavableF16ProfileSaveState` |
| `0x826c6318` | VocalPlayer.o | `src/band3/game/VocalPlayer.cpp` | high | ExactInstructionsFunctionHasher | VocalPlayer const::CurrentPhrase(...) | `CurrentPhrase__11VocalPlayerCFv` |
| `0x825dbd78` | AccomplishmentPanel.o | `src/band3/meta_band/AccomplishmentPanel.cpp` | bsim 45 | BSIM | AccomplishmentPanel::Unload(...) | `Unload__19AccomplishmentPanelFv` |
| `0x82572980` | BandProfile.o | `src/band3/meta_band/BandProfile.cpp` | bsim 38 | BSIM | BandProfile::SaveSize(...) | `SaveSize__11BandProfileFi` |
| `0x825f14a8` | CharacterCreatorPanel.o | `src/band3/meta_band/CharacterCreatorPanel.cpp` | bsim 39 | BSIM | CharacterCreatorPanel::SetFaceOption(...) | `SetFaceOption__21CharacterCreatorPanelFi` |
| `0x824f8f98` | ContextWrapper.o | `src/band3/net_band/ContextWrapper.cpp` | bsim 32 | BSIM | ContextWrapper::Reset(...) | `Reset__14ContextWrapperFv` |
| `0x82659e60` | Game.o | `src/band3/game/Game.cpp` | bsim 31 | BSIM | Game const::CanUserPause(...) | `CanUserPause__4GameCFv` |
| `0x82b67448` | GemManager.o | `src/band3/bandtrack/GemManager.cpp` | bsim 67 | BSIM | GemManager::SetupRealGuitarImportantStrings(...) | `SetupRealGuitarImportantStrings__10GemManagerFv` |
| `0x8269eb78` | GemPlayer.o | `src/band3/game/GemPlayer.cpp` | bsim 33 | BSIM | GemPlayer::LocalSoloStart(...) | `LocalSoloStart__9GemPlayerFv` |
| `0x8268d410` | GemTrainerPanel.o | `src/band3/game/GemTrainerPanel.cpp` | bsim 31 | BSIM | GemTrainerPanel::Poll(...) | `Poll__15GemTrainerPanelFv` |
| `0x8268db88` | GemTrainerPanel.o | `src/band3/game/GemTrainerPanel.cpp` | bsim 34 | BSIM | GemTrainerPanel::StartSectionImpl(...) | `StartSectionImpl__15GemTrainerPanelFv` |
| `0x8252a8f8` | MusicLibrary.o | `src/band3/meta_band/MusicLibrary.cpp` | bsim 37 | BSIM | MusicLibrary::RebuildRestrictedData(...) | `RebuildRestrictedData__12MusicLibraryFv` |
| `0x8252a9f0` | MusicLibrary.o | `src/band3/meta_band/MusicLibrary.cpp` | bsim 36 | BSIM | MusicLibrary::RebuildSharedSongData(...) | `RebuildSharedSongData__12MusicLibraryFv` |
| `0x8252c130` | MusicLibrary.o | `src/band3/meta_band/MusicLibrary.cpp` | bsim 32 | BSIM | MusicLibrary::RebuildProfileData(...) | `RebuildProfileData__12MusicLibraryFv` |
| `0x825b6500` | MusicLibraryNetSetlists.o | `src/band3/meta_band/MusicLibraryNetSetlists.cpp` | bsim 32 | BSIM | MusicLibraryNetSetlists::CleanUpArt(...) | `CleanUpArt__23MusicLibraryNetSetlistsFv` |
| `0x8260d330` | PatchPanel.o | `src/band3/meta_band/PatchPanel.cpp` | bsim 33 | BSIM | PatchPanel::StoreUndo(...) | `StoreUndo__10PatchPanelFv` |
| `0x824e57a0` | RockCentral.o | `src/band3/net_band/RockCentral.cpp` | bsim 35 | BSIM | RockCentral::DataPointToQString(...) | `DataPointToQString__11RockCentralFRC9DataPointRQ26Quazal6String` |
| `0x82667a50` | SongDB.o | `src/band3/game/SongDB.cpp` | bsim 30 | BSIM | SongDB::GetCommonPhraseExtent(...) | `GetCommonPhraseExtent__6SongDBFiiR6Extent` |
| `0x825a1ba8` | SongRecord.o | `src/band3/meta_band/SongRecord.cpp` | bsim 84 | BSIM | SongRecord::UpdatePerformanceData(...) | `UpdatePerformanceData__10SongRecordFv` |
| `0x82b65fd0` | Track.o | `src/band3/bandtrack/Track.cpp` | bsim 30 | BSIM | Track::FailedAtStart(...) | `FailedAtStart__5TrackFv` |
| `0x82b66238` | Track.o | `src/band3/bandtrack/Track.cpp` | bsim 79 | BSIM | Track::Poll(...) | `Poll__5TrackFf` |
| `0x826b1f50` | Tracker.o | `src/band3/game/Tracker.cpp` | bsim 40 | BSIM | Tracker::Poll(...) | `Poll__7TrackerFf` |
| `0x826b3178` | Tracker.o | `src/band3/game/Tracker.cpp` | bsim 54 | BSIM | Tracker::HandleRemovePlayer(...) | `HandleRemovePlayer__7TrackerFP6Player` |
| `0x82674f68` | TrackerManager.o | `src/band3/game/TrackerManager.cpp` | bsim 64 | BSIM | TrackerManager::Poll(...) | `Poll__14TrackerManagerFf` |
| `0x826d8478` | TrackerUtils.o | `src/band3/game/TrackerUtils.cpp` | bsim 34 | BSIM | TrackerMultiplierMap const::FindEntry(...) | `FindEntry__20TrackerMultiplierMapCFf` |
| `0x826ab3b0` | TrainerPanel.o | `src/band3/game/TrainerPanel.cpp` | bsim 31 | BSIM | TrainerPanel::ResetChallenge(...) | `ResetChallenge__12TrainerPanelFv` |
| `0x826d4058` | VocalPart.o | `src/band3/game/VocalPart.cpp` | bsim 58 | BSIM | VocalPart const::CalcPhraseScoreMax(...) | `CalcPhraseScoreMax__9VocalPartCFRCPC11VocalPhrase` |
| `0x826d52b8` | VocalPart.o | `src/band3/game/VocalPart.cpp` | bsim 35 | BSIM | VocalPart::HandlePhraseEnd(...) | `HandlePhraseEnd__9VocalPartFRiRfRfRif` |
| `0x826c57b0` | VocalPlayer.o | `src/band3/game/VocalPlayer.cpp` | bsim 36 | BSIM | VocalPlayer::UpdateVocalStyle(...) | `UpdateVocalStyle__11VocalPlayerFv` |

## TU ranking (port these first — by #high+#bsim≥30 desc, then total desc)

| Rank | TU | src | #ids | high | ≥30 | 20-30 | 15-20 | DC3? |
|---|---|---|---|---|---|---|---|---|
| 1 | MusicLibrary.o | `src/band3/meta_band/MusicLibrary.cpp` | 7 | 1 | 3 | 2 | 1 | cannot-provide |
| 2 | VocalPart.o | `src/band3/game/VocalPart.cpp` | 7 | 0 | 2 | 3 | 2 | cannot-provide |
| 3 | Track.o | `src/band3/bandtrack/Track.cpp` | 6 | 0 | 2 | 3 | 1 | cannot-provide |
| 4 | ClosetMgr.o | `src/band3/meta_band/ClosetMgr.cpp` | 4 | 2 | 0 | 1 | 1 | cannot-provide |
| 5 | VocalPlayer.o | `src/band3/game/VocalPlayer.cpp` | 4 | 1 | 1 | 1 | 1 | cannot-provide |
| 6 | GemTrainerPanel.o | `src/band3/game/GemTrainerPanel.cpp` | 3 | 0 | 2 | 0 | 1 | cannot-provide |
| 7 | Tracker.o | `src/band3/game/Tracker.cpp` | 3 | 0 | 2 | 0 | 1 | cannot-provide |
| 8 | TourSavable.o | `src/band3/tour/TourSavable.cpp` | 2 | 2 | 0 | 0 | 0 | cannot-provide |
| 9 | GemPlayer.o | `src/band3/game/GemPlayer.cpp` | 19 | 0 | 1 | 9 | 9 | cannot-provide |
| 10 | TrackerManager.o | `src/band3/game/TrackerManager.cpp` | 10 | 0 | 1 | 2 | 7 | cannot-provide |
| 11 | Stats.o | `src/band3/game/Stats.cpp` | 8 | 1 | 0 | 1 | 6 | cannot-provide |
| 12 | Game.o | `src/band3/game/Game.cpp` | 7 | 0 | 1 | 5 | 1 | cannot-provide |
| 13 | SongRecord.o | `src/band3/meta_band/SongRecord.cpp` | 5 | 0 | 1 | 1 | 3 | cannot-provide |
| 14 | BandProfile.o | `src/band3/meta_band/BandProfile.cpp` | 4 | 0 | 1 | 3 | 0 | cannot-provide |
| 15 | GemManager.o | `src/band3/bandtrack/GemManager.cpp` | 4 | 0 | 1 | 1 | 2 | cannot-provide |
| 16 | OvershellSlot.o | `src/band3/meta_band/OvershellSlot.cpp` | 4 | 1 | 0 | 2 | 1 | cannot-provide |
| 17 | SongDB.o | `src/band3/game/SongDB.cpp` | 4 | 0 | 1 | 1 | 2 | cannot-provide |
| 18 | MusicLibraryNetSetlists.o | `src/band3/meta_band/MusicLibraryNetSetlists.cpp` | 3 | 0 | 1 | 2 | 0 | cannot-provide |
| 19 | Performer.o | `src/band3/game/Performer.cpp` | 3 | 1 | 0 | 1 | 1 | cannot-provide |
| 20 | ProfileMgr.o | `src/band3/meta_band/ProfileMgr.cpp` | 3 | 1 | 0 | 0 | 2 | cannot-provide |
| 21 | TambourineManager.o | `src/band3/game/TambourineManager.cpp` | 3 | 1 | 0 | 2 | 0 | cannot-provide |
| 22 | TrackerUtils.o | `src/band3/game/TrackerUtils.cpp` | 3 | 0 | 1 | 1 | 1 | cannot-provide |
| 23 | TrainerPanel.o | `src/band3/game/TrainerPanel.cpp` | 3 | 0 | 1 | 2 | 0 | cannot-provide |
| 24 | Band.o | `src/band3/game/Band.cpp` | 2 | 1 | 0 | 1 | 0 | cannot-provide |
| 25 | BandMachine.o | `src/band3/meta_band/BandMachine.cpp` | 2 | 1 | 0 | 1 | 0 | cannot-provide |
| 26 | ContextWrapper.o | `src/band3/net_band/ContextWrapper.cpp` | 2 | 0 | 1 | 0 | 1 | cannot-provide |
| 27 | NetGameMsgs.o | `src/band3/game/NetGameMsgs.cpp` | 2 | 1 | 0 | 0 | 1 | cannot-provide |
| 28 | PatchPanel.o | `src/band3/meta_band/PatchPanel.cpp` | 2 | 0 | 1 | 1 | 0 | cannot-provide |
| 29 | RockCentral.o | `src/band3/net_band/RockCentral.cpp` | 2 | 0 | 1 | 0 | 1 | cannot-provide |
| 30 | AccomplishmentPanel.o | `src/band3/meta_band/AccomplishmentPanel.cpp` | 1 | 0 | 1 | 0 | 0 | cannot-provide |
| 31 | AccomplishmentProgress.o | `src/band3/meta_band/AccomplishmentProgress.cpp` | 1 | 1 | 0 | 0 | 0 | cannot-provide |
| 32 | BandMachineMgr.o | `src/band3/meta_band/BandMachineMgr.cpp` | 1 | 1 | 0 | 0 | 0 | cannot-provide |
| 33 | CharacterCreatorPanel.o | `src/band3/meta_band/CharacterCreatorPanel.cpp` | 1 | 0 | 1 | 0 | 0 | cannot-provide |
| 34 | Lyric.o | `src/band3/bandtrack/Lyric.cpp` | 1 | 1 | 0 | 0 | 0 | cannot-provide |
| 35 | MainHubMessageProvider.o | `src/band3/meta_band/MainHubMessageProvider.cpp` | 1 | 1 | 0 | 0 | 0 | cannot-provide |
| 36 | QuestJournal.o | `src/band3/tour/QuestJournal.cpp` | 1 | 1 | 0 | 0 | 0 | cannot-provide |
| 37 | SongSort.o | `src/band3/meta_band/SongSort.cpp` | 1 | 1 | 0 | 0 | 0 | cannot-provide |
| 38 | Player.o | `src/band3/game/Player.cpp` | 7 | 0 | 0 | 3 | 4 | cannot-provide |
| 39 | VocalTrack.o | `src/band3/bandtrack/VocalTrack.cpp` | 7 | 0 | 0 | 3 | 4 | cannot-provide |
| 40 | MetaPerformer.o | `src/band3/meta_band/MetaPerformer.cpp` | 4 | 0 | 0 | 1 | 3 | cannot-provide |
| 41 | Gem.o | `src/band3/bandtrack/Gem.cpp` | 3 | 0 | 0 | 2 | 1 | cannot-provide |
| 42 | GemSmasher.o | `src/band3/bandtrack/GemSmasher.cpp` | 3 | 0 | 0 | 1 | 2 | cannot-provide |
| 43 | PracticePanel.o | `src/band3/game/PracticePanel.cpp` | 3 | 0 | 0 | 3 | 0 | cannot-provide |
| 44 | VocalGuidePitch.o | `src/band3/game/VocalGuidePitch.cpp` | 3 | 0 | 0 | 2 | 1 | cannot-provide |
| 45 | BandPerformer.o | `src/band3/game/BandPerformer.cpp` | 2 | 0 | 0 | 2 | 0 | cannot-provide |
| 46 | BandSongMetadata.o | `src/band3/meta_band/BandSongMetadata.cpp` | 2 | 0 | 0 | 1 | 1 | cannot-provide |
| 47 | BandUser.o | `src/band3/game/BandUser.cpp` | 2 | 0 | 0 | 1 | 1 | cannot-provide |
| 48 | CustomizePanel.o | `src/band3/meta_band/CustomizePanel.cpp` | 2 | 0 | 0 | 2 | 0 | cannot-provide |
| 49 | EntityUploader.o | `src/band3/net_band/EntityUploader.cpp` | 2 | 0 | 0 | 0 | 2 | cannot-provide |
| 50 | FadePanel.o | `src/band3/game/FadePanel.cpp` | 2 | 0 | 0 | 1 | 1 | cannot-provide |
| 51 | GemTrack.o | `src/band3/bandtrack/GemTrack.cpp` | 2 | 0 | 0 | 0 | 2 | cannot-provide |
| 52 | Leaderboard.o | `src/band3/meta_band/Leaderboard.cpp` | 2 | 0 | 0 | 2 | 0 | cannot-provide |
| 53 | MainHubPanel.o | `src/band3/meta_band/MainHubPanel.cpp` | 2 | 0 | 0 | 1 | 1 | cannot-provide |
| 54 | SetlistMergePanel.o | `src/band3/meta_band/SetlistMergePanel.cpp` | 2 | 0 | 0 | 2 | 0 | cannot-provide |
| 55 | Singer.o | `src/band3/game/Singer.cpp` | 2 | 0 | 0 | 1 | 1 | cannot-provide |
| 56 | Tail.o | `src/band3/bandtrack/Tail.cpp` | 2 | 0 | 0 | 1 | 1 | cannot-provide |
| 57 | TourProgress.o | `src/band3/tour/TourProgress.cpp` | 2 | 0 | 0 | 0 | 2 | cannot-provide |
| 58 | TrackConfig.o | `src/band3/bandtrack/TrackConfig.cpp` | 2 | 0 | 0 | 2 | 0 | cannot-provide |
| 59 | BandStorePanel.o | `src/band3/meta_band/BandStorePanel.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 60 | CalibrationPanel.o | `src/band3/meta_band/CalibrationPanel.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 61 | CampaignSongInfoPanel.o | `src/band3/meta_band/CampaignSongInfoPanel.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 62 | ChordbookPanel.o | `src/band3/game/ChordbookPanel.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 63 | CrowdRating.o | `src/band3/game/CrowdRating.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 64 | DirectInstrument.o | `src/band3/game/DirectInstrument.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 65 | EditSetlistPanel.o | `src/band3/meta_band/EditSetlistPanel.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 66 | FocusTracker.o | `src/band3/game/FocusTracker.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 67 | GameConfig.o | `src/band3/game/GameConfig.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 68 | GameTimePanel.o | `src/band3/meta_band/GameTimePanel.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 69 | GameplayOptions.o | `src/band3/meta_band/GameplayOptions.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 70 | InterstitialPanel.o | `src/band3/meta_band/InterstitialPanel.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 71 | MetaPanel.o | `src/band3/meta_band/MetaPanel.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 72 | Metronome.o | `src/band3/game/Metronome.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 73 | NextSongPanel.o | `src/band3/meta_band/NextSongPanel.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 74 | OverdriveTracker.o | `src/band3/game/OverdriveTracker.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 75 | OvershellPanel.o | `src/band3/meta_band/OvershellPanel.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 76 | PerformanceData.o | `src/band3/meta_band/PerformanceData.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 77 | PlayerBehavior.o | `src/band3/game/PlayerBehavior.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 78 | PlayerLeaderboards.o | `src/band3/meta_band/PlayerLeaderboards.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 79 | PrefabMgr.o | `src/band3/meta_band/PrefabMgr.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 80 | QuestFilterPanel.o | `src/band3/tour/QuestFilterPanel.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 81 | QuestManager.o | `src/band3/tour/QuestManager.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 82 | SongSortMgr.o | `src/band3/meta_band/SongSortMgr.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 83 | SongSortNode.o | `src/band3/meta_band/SongSortNode.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 84 | StandIn.o | `src/band3/meta_band/StandIn.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 85 | StatCollector.o | `src/band3/game/StatCollector.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 86 | TambourineDetector.o | `src/band3/game/TambourineDetector.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 87 | Tour.o | `src/band3/tour/Tour.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 88 | TourBand.o | `src/band3/tour/TourBand.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 89 | TourPerformer.o | `src/band3/tour/TourPerformer.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 90 | TrainingPanel.o | `src/band3/meta_band/TrainingPanel.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 91 | UIStats.o | `src/band3/meta_band/UIStats.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |
| 92 | VocalScoreHistory.o | `src/band3/game/VocalScoreHistory.cpp` | 1 | 0 | 0 | 1 | 0 | cannot-provide |
| 93 | WaitingUserGate.o | `src/band3/meta_band/WaitingUserGate.cpp` | 1 | 0 | 0 | 0 | 1 | cannot-provide |

## Per-TU function rosters

Each TU's identities, confidence-ranked. `wii_symbol` is the CW/MWCC ground-truth name (`bin/analyze-function <wii_symbol>` in the rb3 repo for the real body).

### MusicLibrary.o — 7 ids (high 1, ≥30 3, 20-30 2, 15-20 1)  ·  `src/band3/meta_band/MusicLibrary.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8252c728` | `0x80300e10` | high | SwitchSigHasher | MusicLibrary const::DifficultySortPart(...) | `DifficultySortPart__12MusicLibraryCFv` |
| `0x8252a8f8` | `0x80303460` | bsim 37 | BSIM | MusicLibrary::RebuildRestrictedData(...) | `RebuildRestrictedData__12MusicLibraryFv` |
| `0x8252a9f0` | `0x80303300` | bsim 36 | BSIM | MusicLibrary::RebuildSharedSongData(...) | `RebuildSharedSongData__12MusicLibraryFv` |
| `0x8252c130` | `0x80303170` | bsim 32 | BSIM | MusicLibrary::RebuildProfileData(...) | `RebuildProfileData__12MusicLibraryFv` |
| `0x8252bf10` | `0x80302b30` | bsim 24 | BSIM | MusicLibrary::UpdateHeaderData(...) | `UpdateHeaderData__12MusicLibraryFv` |
| `0x8252cdf8` | `0x802f9d30` | bsim 23 | BSIM | MusicLibrary::Poll(...) | `Poll__12MusicLibraryFv` |
| `0x82527ac8` | `0x80300cd0` | bsim 17 | BSIM | MusicLibrary const::ComponentStateOverride(...) | `ComponentStateOverride__12MusicLibraryCFiiQ211UIComponent5State` |

### VocalPart.o — 7 ids (high 0, ≥30 2, 20-30 3, 15-20 2)  ·  `src/band3/game/VocalPart.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826d4058` | `0x80209260` | bsim 58 | BSIM | VocalPart const::CalcPhraseScoreMax(...) | `CalcPhraseScoreMax__9VocalPartCFRCPC11VocalPhrase` |
| `0x826d52b8` | `0x8020ac60` | bsim 35 | BSIM | VocalPart::HandlePhraseEnd(...) | `HandlePhraseEnd__9VocalPartFRiRfRfRif` |
| `0x826d3cc0` | `0x80208c70` | bsim 29 | BSIM | VocalPart::UpdateSongMinMaxPitch(...) | `UpdateSongMinMaxPitch__9VocalPartFv` |
| `0x826d50a8` | `0x8020a780` | bsim 25 | BSIM | VocalPart::GetBestHit(...) | `GetBestHit__9VocalPartFfiiP12TalkyMatcherRffRiRiRfRfRb` |
| `0x826d5be0` | `0x80208be0` | bsim 26 | BSIM | VocalPart::PostLoad(...) | `PostLoad__9VocalPartFv` |
| `0x826d4b18` | `0x8020b4e0` | bsim 16 | BSIM | VocalPart const::IsEmptyPhrase(...) | `IsEmptyPhrase__9VocalPartCFRCPC11VocalPhrase` |
| `0x826d5a80` | `0x80208ff0` | bsim 19 | BSIM | VocalPart::CalcNoteWeights(...) | `CalcNoteWeights__9VocalPartFv` |

### Track.o — 6 ids (high 0, ≥30 2, 20-30 3, 15-20 1)  ·  `src/band3/bandtrack/Track.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82b65fd0` | `0x801473a0` | bsim 30 | BSIM | Track::FailedAtStart(...) | `FailedAtStart__5TrackFv` |
| `0x82b66238` | `0x80146710` | bsim 79 | BSIM | Track::Poll(...) | `Poll__5TrackFf` |
| `0x82b65b70` | `0x80146c10` | bsim 23 | BSIM | Track const::GetTrackIcon(...) | `GetTrackIcon__5TrackCFv` |
| `0x82b65ca0` | `0x80146d50` | bsim 25 | BSIM | Track const::PlayerDisconnectedAtStart(...) | `PlayerDisconnectedAtStart__5TrackCFv` |
| `0x82b65e98` | `0x80147110` | bsim 23 | BSIM | Track::PushGameplayOptions(...) | `PushGameplayOptions__5TrackF10VocalParami` |
| `0x82b65a88` | `0x80146b00` | bsim 15 | BSIM | Track const::GetPlayer(...) | `GetPlayer__5TrackCFv` |

### ClosetMgr.o — 4 ids (high 2, ≥30 0, 20-30 1, 15-20 1)  ·  `src/band3/meta_band/ClosetMgr.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x825508c8` | `0x802ac920` | high | ExactInstructionsFunctionHasher | ClosetMgr::ShowClothes(...) | `ShowClothes__9ClosetMgrFv` |
| `0x82550d58` | `0x802ac5e0` | high | ExactInstructionsFunctionHasher | ClosetMgr::ResetPatches(...) | `ResetPatches__9ClosetMgrFv` |
| `0x8254f298` | `0x802ac470` | bsim 21 | BSIM | ClosetMgr::UpdateCharacterPatch(...) | `UpdateCharacterPatch__9ClosetMgrFQ312BandCharDesc5Patch8CategoryPCc` |
| `0x8254f240` | `0x802ac380` | bsim 18 | BSIM | ClosetMgr::ForceClosetPoll(...) | `ForceClosetPoll__9ClosetMgrFv` |

### VocalPlayer.o — 4 ids (high 1, ≥30 1, 20-30 1, 15-20 1)  ·  `src/band3/game/VocalPlayer.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826c6318` | `0x8020d8d0` | high | ExactInstructionsFunctionHasher | VocalPlayer const::CurrentPhrase(...) | `CurrentPhrase__11VocalPlayerCFv` |
| `0x826c57b0` | `0x80214a90` | bsim 36 | BSIM | VocalPlayer::UpdateVocalStyle(...) | `UpdateVocalStyle__11VocalPlayerFv` |
| `0x826c5f10` | `0x8020ce50` | bsim 25 | BSIM | VocalPlayer::StartIntro(...) | `StartIntro__11VocalPlayerFv` |
| `0x826c5680` | `0x80214340` | bsim 16 | BSIM | VocalPlayer::LocalHitCoda(...) | `LocalHitCoda__11VocalPlayerFv` |

### GemTrainerPanel.o — 3 ids (high 0, ≥30 2, 20-30 0, 15-20 1)  ·  `src/band3/game/GemTrainerPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8268d410` | `0x801a2ab0` | bsim 31 | BSIM | GemTrainerPanel::Poll(...) | `Poll__15GemTrainerPanelFv` |
| `0x8268db88` | `0x801a35e0` | bsim 34 | BSIM | GemTrainerPanel::StartSectionImpl(...) | `StartSectionImpl__15GemTrainerPanelFv` |
| `0x8268c138` | `0x801a3230` | bsim 16 | BSIM | GemTrainerPanel const::IsGemInFutureLoop(...) | `IsGemInFutureLoop__15GemTrainerPanelCFi` |

### Tracker.o — 3 ids (high 0, ≥30 2, 20-30 0, 15-20 1)  ·  `src/band3/game/Tracker.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826b1f50` | `0x801eeae0` | bsim 40 | BSIM | Tracker::Poll(...) | `Poll__7TrackerFf` |
| `0x826b3178` | `0x801ee670` | bsim 54 | BSIM | Tracker::HandleRemovePlayer(...) | `HandleRemovePlayer__7TrackerFP6Player` |
| `0x826b1538` | `0x801ee1d0` | bsim 16 | BSIM | Tracker::UpdateSource(...) | `UpdateSource__7TrackerFP13TrackerSource` |

### TourSavable.o — 2 ids (high 2, ≥30 0, 20-30 0, 15-20 0)  ·  `src/band3/tour/TourSavable.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82357450` | `0x803dad70` | high | ExactInstructionsFunctionHasher | TourSavable::SetDirty(...) | `SetDirty__11TourSavableFbi` |
| `0x82357490` | `0x803dadb0` | high | ExactInstructionsFunctionHasher | TourSavable::SaveLoadComplete(...) | `SaveLoadComplete__11TourSavableF16ProfileSaveState` |

### GemPlayer.o — 19 ids (high 0, ≥30 1, 20-30 9, 15-20 9)  ·  `src/band3/game/GemPlayer.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8269eb78` | `0x8019d8e0` | bsim 33 | BSIM | GemPlayer::LocalSoloStart(...) | `LocalSoloStart__9GemPlayerFv` |
| `0x8269d670` | `0x801947f0` | bsim 21 | BSIM | GemPlayer::CanFlail(...) | `CanFlail__9GemPlayerFf` |
| `0x8269db38` | `0x80198530` | bsim 26 | BSIM | GemPlayer::GetPitchShift(...) | `GetPitchShift__9GemPlayerFv` |
| `0x8269e0a0` | `0x80198f80` | bsim 22 | BSIM | GemPlayer::PlayDrum(...) | `PlayDrum__9GemPlayerFiifi` |
| `0x8269e3d8` | `0x8019a700` | bsim 22 | BSIM | GemPlayer::HandleFirstGemAfterRollback(...) | `HandleFirstGemAfterRollback__9GemPlayerFi` |
| `0x8269ea00` | `0x8019c600` | bsim 27 | BSIM | GemPlayer::UpdateGameCymbalLanes(...) | `UpdateGameCymbalLanes__9GemPlayerFv` |
| `0x8269f028` | `0x8019f0c0` | bsim 24 | BSIM | GemPlayer const::InRGTrill(...) | `InRGTrill__9GemPlayerCFi` |
| `0x8269f5c8` | `0x8019c920` | bsim 23 | BSIM | GemPlayer::IsCodaMiss(...) | `IsCodaMiss__9GemPlayerFf` |
| `0x8269f860` | `0x8019f050` | bsim 24 | BSIM | GemPlayer const::InTrill(...) | `InTrill__9GemPlayerCFi` |
| `0x826a51c0` | `0x801988c0` | bsim 23 | BSIM | GemPlayer::Jump(...) | `Jump__9GemPlayerFfb` |
| `0x8269d780` | `0x80195f10` | bsim 19 | BSIM | GemPlayer::PlayMissSound(...) | `PlayMissSound__9GemPlayerFi` |
| `0x8269e120` | `0x80199610` | bsim 16 | BSIM | GemPlayer::SetAutoplayAccuracy(...) | `SetAutoplayAccuracy__9GemPlayerFf` |
| `0x8269e138` | `0x80199660` | bsim 15 | BSIM | GemPlayer const::GetMaxSlots(...) | `GetMaxSlots__9GemPlayerCFv` |
| `0x8269eb60` | `0x8019d8b0` | bsim 15 | BSIM | GemPlayer const::GetRGFret(...) | `GetRGFret__9GemPlayerCFi` |
| `0x8269ecf0` | `0x8019e050` | bsim 20 | BSIM | GemPlayer::LocalSetGuitarFx(...) | `LocalSetGuitarFx__9GemPlayerFi` |
| `0x8269f098` | `0x8019f130` | bsim 15 | BSIM | GemPlayer const::InRoll(...) | `InRoll__9GemPlayerCFi` |
| `0x8269fd60` | `0x80199e80` | bsim 17 | BSIM | GemPlayer const::OnGetPercentHitGemsPractice(...) | `OnGetPercentHitGemsPractice__9GemPlayerCFiff` |
| `0x826a0498` | `0x8019e420` | bsim 19 | BSIM | GemPlayer const::AllCodaGemsHit(...) | `AllCodaGemsHit__9GemPlayerCFv` |
| `0x826a1988` | `0x80197090` | bsim 16 | BSIM | GemPlayer::IgnoreGem(...) | `IgnoreGem__9GemPlayerFi` |

### TrackerManager.o — 10 ids (high 0, ≥30 1, 20-30 2, 15-20 7)  ·  `src/band3/game/TrackerManager.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82674f68` | `0x801f4000` | bsim 64 | BSIM | TrackerManager::Poll(...) | `Poll__14TrackerManagerFf` |
| `0x82675220` | `0x801f48e0` | bsim 28 | BSIM | TrackerManager::OnPlayerAddEnergy(...) | `OnPlayerAddEnergy__14TrackerManagerFP6Playerf` |
| `0x82675ee8` | `0x801f3ea0` | bsim 23 | BSIM | TrackerManager::Restart(...) | `Restart__14TrackerManagerFv` |
| `0x82674ec8` | `0x801f3f20` | bsim 16 | BSIM | TrackerManager::HandleAddPlayer(...) | `HandleAddPlayer__14TrackerManagerFP6Player` |
| `0x82674ee0` | `0x801f3f80` | bsim 16 | BSIM | TrackerManager::StartIntro(...) | `StartIntro__14TrackerManagerFv` |
| `0x826752a0` | `0x801f4960` | bsim 16 | BSIM | TrackerManager::OnPlayerSaved(...) | `OnPlayerSaved__14TrackerManagerFP6Player` |
| `0x826752b8` | `0x801f3f40` | bsim 16 | BSIM | TrackerManager::HandleRemovePlayer(...) | `HandleRemovePlayer__14TrackerManagerFP6Player` |
| `0x82675330` | `0x801f4a50` | bsim 16 | BSIM | TrackerManager::OnRemoteTrackerPlayerProgress(...) | `OnRemoteTrackerPlayerProgress__14TrackerManagerFP6Playerf` |
| `0x826753a8` | `0x801f4b30` | bsim 16 | BSIM | TrackerManager::OnRemoteTrackerPlayerDisplay(...) | `OnRemoteTrackerPlayerDisplay__14TrackerManagerFP6Playeriii` |
| `0x82b62248` | `0x801f3f60` | bsim 16 | BSIM | TrackerManager::HandleGameOver(...) | `HandleGameOver__14TrackerManagerFf` |

### Stats.o — 8 ids (high 1, ≥30 0, 20-30 1, 15-20 6)  ·  `src/band3/game/Stats.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826798b0` | `0x801e68a0` | high | Implied Match | SingerStats const::GetRankData(...) | `GetRankData__11SingerStatsCFi` |
| `0x82679998` | `0x801e39b0` | bsim 21 | BSIM | Stats const::GetSingerRankedPercentage(...) | `GetSingerRankedPercentage__5StatsCFii` |
| `0x82679030` | `0x801e4c70` | bsim 17 | BSIM | Stats::AddSustain(...) | `AddSustain__5StatsFf` |
| `0x82679070` | `0x801e4cb0` | bsim 17 | BSIM | Stats::AddTambourine(...) | `AddTambourine__5StatsFf` |
| `0x82679160` | `0x801e5a10` | bsim 17 | BSIM | Stats::IncrementHighFretGemsHit(...) | `IncrementHighFretGemsHit__5StatsFb` |
| `0x82679188` | `0x801e5aa0` | bsim 17 | BSIM | Stats::IncrementTrillsHit(...) | `IncrementTrillsHit__5StatsFb` |
| `0x826791b0` | `0x801e5a70` | bsim 15 | BSIM | Stats::AddRoll(...) | `AddRoll__5StatsFb` |
| `0x826795a0` | `0x801e35e0` | bsim 19 | BSIM | Stats::SetSoloButtonedSoloPercentage(...) | `SetSoloButtonedSoloPercentage__5StatsFi` |

### Game.o — 7 ids (high 0, ≥30 1, 20-30 5, 15-20 1)  ·  `src/band3/game/Game.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82659e60` | `0x8017f3e0` | bsim 31 | BSIM | Game const::CanUserPause(...) | `CanUserPause__4GameCFv` |
| `0x8265a168` | `0x80181720` | bsim 29 | BSIM | Game::SetGameOver(...) | `SetGameOver__4GameFb` |
| `0x8265a278` | `0x80182c10` | bsim 20 | BSIM | Game::E3CheatAutoplayAccuracy(...) | `E3CheatAutoplayAccuracy__4GameFv` |
| `0x8265baf0` | `0x8017fd50` | bsim 28 | BSIM | Game const::GetScoringTracks(...) | `GetScoringTracks__4GameCFv` |
| `0x8265c250` | `0x80181310` | bsim 25 | BSIM | Game::HandleAudioLoad(...) | `HandleAudioLoad__4GameFv` |
| `0x8265ea78` | `0x8017e250` | bsim 28 | BSIM | Game::IsLoaded(...) | `IsLoaded__4GameFv` |
| `0x8265c3b0` | `0x80181580` | bsim 16 | BSIM | Game const::AdjustForVocalPhrases(...) | `AdjustForVocalPhrases__4GameCFRfRf` |

### SongRecord.o — 5 ids (high 0, ≥30 1, 20-30 1, 15-20 3)  ·  `src/band3/meta_band/SongRecord.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x825a1ba8` | `0x8036ad50` | bsim 84 | BSIM | SongRecord::UpdatePerformanceData(...) | `UpdatePerformanceData__10SongRecordFv` |
| `0x825a1e00` | `0x8036afd0` | bsim 22 | BSIM | SongRecord::UpdateScoreType(...) | `UpdateScoreType__10SongRecordFv` |
| `0x825a1d90` | `0x8036af40` | bsim 16 | BSIM | SongRecord::UpdateReview(...) | `UpdateReview__10SongRecordFv` |
| `0x825a2038` | `0x8036bbd0` | bsim 18 | BSIM | SetlistRecord const::IsLocal(...) | `IsLocal__13SetlistRecordCFv` |
| `0x825a20e0` | `0x8036bc60` | bsim 17 | BSIM | SetlistRecord const::IsProfileOwner(...) | `IsProfileOwner__13SetlistRecordCFPC11BandProfile` |

### BandProfile.o — 4 ids (high 0, ≥30 1, 20-30 3, 15-20 0)  ·  `src/band3/meta_band/BandProfile.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82572980` | `0x80264ef0` | bsim 38 | BSIM | BandProfile::SaveSize(...) | `SaveSize__11BandProfileFi` |
| `0x824e4580` | `0x80268b10` | bsim 21 | BSIM | RockCentralOpCompleteMsg::RockCentralOpCompleteMsg(...) | `__ct__24RockCentralOpCompleteMsgFP9DataArray` |
| `0x82572e08` | `0x80267980` | bsim 28 | BSIM | BandProfile::SetLastCharUsed(...) | `SetLastCharUsed__11BandProfileFP8CharData` |
| `0x825739f0` | `0x80266b90` | bsim 29 | BSIM | BandProfile::GetBandLogoTex(...) | `GetBandLogoTex__11BandProfileFv` |

### GemManager.o — 4 ids (high 0, ≥30 1, 20-30 1, 15-20 2)  ·  `src/band3/bandtrack/GemManager.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82b67448` | `0x80130950` | bsim 67 | BSIM | GemManager::SetupRealGuitarImportantStrings(...) | `SetupRealGuitarImportantStrings__10GemManagerFv` |
| `0x82b6aac8` | `0x80133d80` | bsim 25 | BSIM | GemManager::AddChordBracket(...) | `AddChordBracket__10GemManagerF6SymbolUif` |
| `0x82b67108` | `0x80135b90` | bsim 16 | BSIM | GemManager::IsInFill(...) | `IsInFill__10GemManagerFi` |
| `0x82b684e8` | `0x80135340` | bsim 17 | BSIM | GemManager const::SlotEnabled(...) | `SlotEnabled__10GemManagerCFi` |

### OvershellSlot.o — 4 ids (high 1, ≥30 0, 20-30 2, 15-20 1)  ·  `src/band3/meta_band/OvershellSlot.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x825bfb80` | `0x80328f00` | high | ExactInstructionsFunctionHasher | OvershellSlot const::InOverrideFlow(...) | `InOverrideFlow__13OvershellSlotCF21OvershellOverrideFlow` |
| `0x825c0008` | `0x803245e0` | bsim 24 | BSIM | OvershellSlot::LookupUserInJoinList(...) | `LookupUserInJoinList__13OvershellSlotFPC13LocalBandUserP9JoinState` |
| `0x825c0050` | `0x80324770` | bsim 24 | BSIM | OvershellSlot::GenerateCurrentState(...) | `GenerateCurrentState__13OvershellSlotFv` |
| `0x825c04e0` | `0x80328dd0` | bsim 17 | BSIM | OvershellSlot const::IsValidUser(...) | `IsValidUser__13OvershellSlotCFP8BandUser` |

### SongDB.o — 4 ids (high 0, ≥30 1, 20-30 1, 15-20 2)  ·  `src/band3/game/SongDB.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82667a50` | `0x801dc300` | bsim 30 | BSIM | SongDB::GetCommonPhraseExtent(...) | `GetCommonPhraseExtent__6SongDBFiiR6Extent` |
| `0x82667670` | `0x801ded90` | bsim 21 | BSIM | SongDB const::GetSustainGemCount(...) | `GetSustainGemCount__6SongDBCFi` |
| `0x82667448` | `0x801dc800` | bsim 19 | BSIM | SongDB const::GetCommonPhraseID(...) | `GetCommonPhraseID__6SongDBCFii` |
| `0x826676d0` | `0x801df1c0` | bsim 17 | BSIM | SongDB::NextPhraseIndexAfter(...) | `NextPhraseIndexAfter__6SongDBFii` |

### MusicLibraryNetSetlists.o — 3 ids (high 0, ≥30 1, 20-30 2, 15-20 0)  ·  `src/band3/meta_band/MusicLibraryNetSetlists.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x825b6500` | `0x80308220` | bsim 32 | BSIM | MusicLibraryNetSetlists::CleanUpArt(...) | `CleanUpArt__23MusicLibraryNetSetlistsFv` |
| `0x825b6590` | `0x80307c20` | bsim 27 | BSIM | MusicLibraryNetSetlists::RefreshSetlistArt(...) | `RefreshSetlistArt__23MusicLibraryNetSetlistsFv` |
| `0x827bc190` | `0x80308710` | bsim 21 | BSIM | stlpmtx_std::_List_base<Q223MusicLibraryNetSetlists16SetlistArtRecord,Q211stlpmtx_std59StlNodeAlloc<Q223MusicLibraryNetSetlists16SetlistArtRecord>>::clear(...) | `clear__Q211stlpmtx_std134_List_base<Q223MusicLibraryNetSetlists16SetlistArtRecord,Q211stlpmtx_std59StlNodeAlloc<Q223MusicLibraryNetSetlists16SetlistArtRecord>>Fv` |

### Performer.o — 3 ids (high 1, ≥30 0, 20-30 1, 15-20 1)  ·  `src/band3/game/Performer.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82353b40` | `0x801c0a40` | high | ExactInstructionsFunctionHasher | Stats const::GetVocalPartPercentage(...) | `GetVocalPartPercentage__5StatsCFi` |
| `0x8267e578` | `0x801be100` | bsim 22 | BSIM | Performer const::GetMultiplier(...) | `GetMultiplier__9PerformerCFbRiRiRi` |
| `0x8267ec30` | `0x801bfa90` | bsim 17 | BSIM | Performer::LoseGame(...) | `LoseGame__9PerformerFv` |

### ProfileMgr.o — 3 ids (high 1, ≥30 0, 20-30 0, 15-20 2)  ·  `src/band3/meta_band/ProfileMgr.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82532198` | `0x8034aa10` | high | ExactInstructionsFunctionHasher | ProfileMgr::GlobalOptionsNeedsSave(...) | `GlobalOptionsNeedsSave__10ProfileMgrFv` |
| `0x82532280` | `0x8034c2a0` | bsim 17 | BSIM | ProfileMgr const::GetFxVolume(...) | `GetFxVolume__10ProfileMgrCFv` |
| `0x82532b48` | `0x8034aa60` | bsim 17 | BSIM | ProfileMgr::SaveGlobalOptions(...) | `SaveGlobalOptions__10ProfileMgrFR23FixedSizeSaveableStream` |

### TambourineManager.o — 3 ids (high 1, ≥30 0, 20-30 2, 15-20 0)  ·  `src/band3/game/TambourineManager.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826dbaa8` | `0x801ebda0` | high | ExactInstructionsFunctionHasher | TambourineManager const::TambourineGems(...) | `TambourineGems__17TambourineManagerCFv` |
| `0x826dd580` | `0x801ecac0` | bsim 22 | BSIM | TambourineManager::TambourineSwing(...) | `TambourineSwing__17TambourineManagerFi` |
| `0x826dd6f0` | `0x801ec9d0` | bsim 23 | BSIM | TambourineManager::HandleButtonDown(...) | `HandleButtonDown__17TambourineManagerFv` |

### TrackerUtils.o — 3 ids (high 0, ≥30 1, 20-30 1, 15-20 1)  ·  `src/band3/game/TrackerUtils.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826d8478` | `0x801f70d0` | bsim 34 | BSIM | TrackerMultiplierMap const::FindEntry(...) | `FindEntry__20TrackerMultiplierMapCFf` |
| `0x826d7be0` | `0x801f7d40` | bsim 22 | BSIM | TrackerUtils::CountVocalPhrasesInSong(...) | `CountVocalPhrasesInSong__12TrackerUtilsFi` |
| `0x826d7e00` | `0x801f7390` | bsim 15 | BSIM | TrackerSectionManager const::GetSectionEndTick(...) | `GetSectionEndTick__21TrackerSectionManagerCFi` |

### TrainerPanel.o — 3 ids (high 0, ≥30 1, 20-30 2, 15-20 0)  ·  `src/band3/game/TrainerPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826ab3b0` | `0x802001d0` | bsim 31 | BSIM | TrainerPanel::ResetChallenge(...) | `ResetChallenge__12TrainerPanelFv` |
| `0x826aa578` | `0x801ff5f0` | bsim 22 | BSIM | TrainerPanel::Draw(...) | `Draw__12TrainerPanelFv` |
| `0x826aa958` | `0x801ffb80` | bsim 23 | BSIM | TrainerPanel const::GetCurrentStartTick(...) | `GetCurrentStartTick__12TrainerPanelCFv` |

### Band.o — 2 ids (high 1, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/game/Band.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8267c830` | `0x8015f610` | high | ExactInstructionsFunctionHasher | Band const::EnergyCrowdBoost(...) | `EnergyCrowdBoost__4BandCFv` |
| `0x8267c088` | `0x8015f5b0` | bsim 21 | BSIM | Band::LocalFinishedCoda(...) | `LocalFinishedCoda__4BandFP6Player` |

### BandMachine.o — 2 ids (high 1, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/meta_band/BandMachine.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x825a8520` | `0x802d6050` | high | ExactInstructionsFunctionHasher | LocalBandMachine::SetPrimaryMetaScore(...) | `SetPrimaryMetaScore__16LocalBandMachineFi` |
| `0x825a83e0` | `0x802d5f70` | bsim 21 | BSIM | LocalBandMachine::SetNetUIStateParam(...) | `SetNetUIStateParam__16LocalBandMachineFi` |

### ContextWrapper.o — 2 ids (high 0, ≥30 1, 20-30 0, 15-20 1)  ·  `src/band3/net_band/ContextWrapper.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x824f8f98` | `0x803db5f0` | bsim 32 | BSIM | ContextWrapper::Reset(...) | `Reset__14ContextWrapperFv` |
| `0x824f95d8` | `0x803dbec0` | bsim 15 | BSIM | ContextWrapperPool::Poll(...) | `Poll__18ContextWrapperPoolFv` |

### NetGameMsgs.o — 2 ids (high 1, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/game/NetGameMsgs.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82672b08` | `0x801b0050` | high | ExactInstructionsFunctionHasher | TourHideShowFiltersMsg::TourHideShowFiltersMsg(...) | `__ct__22TourHideShowFiltersMsgFb` |
| `0x82b79678` | `0x801aef40` | bsim 15 | BSIM | PlayerStatsMsg::PlayerStatsMsg(...) | `__ct__14PlayerStatsMsgFP4UseriRC5Stats` |

### PatchPanel.o — 2 ids (high 0, ≥30 1, 20-30 1, 15-20 0)  ·  `src/band3/meta_band/PatchPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8260d330` | `0x8033bcc0` | bsim 33 | BSIM | PatchPanel::StoreUndo(...) | `StoreUndo__10PatchPanelFv` |
| `0x8260d260` | `0x8033bc10` | bsim 23 | BSIM | PatchPanel::RestoreUndo(...) | `RestoreUndo__10PatchPanelFv` |

### RockCentral.o — 2 ids (high 0, ≥30 1, 20-30 0, 15-20 1)  ·  `src/band3/net_band/RockCentral.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x824e57a0` | `0x803ecdf0` | bsim 35 | BSIM | RockCentral::DataPointToQString(...) | `DataPointToQString__11RockCentralFRC9DataPointRQ26Quazal6String` |
| `0x824e5190` | `0x803e67a0` | bsim 17 | BSIM | __ct<PCc,6Symbol>__Q211stlpmtx_std24pair<C6Symbol,8DataNode>FRCQ211stlpmtx_std17pair<PCc,6Symbol>_Pv | `__ct<PCc,6Symbol>__Q211stlpmtx_std24pair<C6Symbol,8DataNode>FRCQ211stlpmtx_std17pair<PCc,6Symbol>_Pv` |

### AccomplishmentPanel.o — 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0)  ·  `src/band3/meta_band/AccomplishmentPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x825dbd78` | `0x80232280` | bsim 45 | BSIM | AccomplishmentPanel::Unload(...) | `Unload__19AccomplishmentPanelFv` |

### AccomplishmentProgress.o — 1 ids (high 1, ≥30 0, 20-30 0, 15-20 0)  ·  `src/band3/meta_band/AccomplishmentProgress.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x827a9768` | `0x80243c00` | high | ExactInstructionsFunctionHasher | AccomplishmentProgress::SendHardCoreStatusUpdateToRockCentral(...) | `SendHardCoreStatusUpdateToRockCentral__22AccomplishmentProgressFv` |

### BandMachineMgr.o — 1 ids (high 1, ≥30 0, 20-30 0, 15-20 0)  ·  `src/band3/meta_band/BandMachineMgr.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826d8b80` | `0x802d9c30` | high | ExactInstructionsFunctionHasher | @unnamed@BandMachineMgr_cpp@::SyncLocalMachineMsg::Dispatch(...) | `Dispatch__Q228@unnamed@BandMachineMgr_cpp@19SyncLocalMachineMsgFv` |

### CharacterCreatorPanel.o — 1 ids (high 0, ≥30 1, 20-30 0, 15-20 0)  ·  `src/band3/meta_band/CharacterCreatorPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x825f14a8` | `0x8029eab0` | bsim 39 | BSIM | CharacterCreatorPanel::SetFaceOption(...) | `SetFaceOption__21CharacterCreatorPanelFi` |

### Lyric.o — 1 ids (high 1, ≥30 0, 20-30 0, 15-20 0)  ·  `src/band3/bandtrack/Lyric.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82b7caa8` | `0x80142ce0` | high | ExactInstructionsFunctionHasher | Lyric const::EndPos(...) | `EndPos__5LyricCFv` |

### MainHubMessageProvider.o — 1 ids (high 1, ≥30 0, 20-30 0, 15-20 0)  ·  `src/band3/meta_band/MainHubMessageProvider.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82655768` | `0x802db640` | high | ExactInstructionsFunctionHasher | MainHubMessageProvider::ClearData(...) | `ClearData__22MainHubMessageProviderFv` |

### QuestJournal.o — 1 ids (high 1, ≥30 0, 20-30 0, 15-20 0)  ·  `src/band3/tour/QuestJournal.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x823527e0` | `0x803b82c0` | high | ExactInstructionsFunctionHasher | QuestJournal::HandleDataChange(...) | `HandleDataChange__12QuestJournalFv` |

### SongSort.o — 1 ids (high 1, ≥30 0, 20-30 0, 15-20 0)  ·  `src/band3/meta_band/SongSort.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x825a66e8` | `0x8036f950` | high | ExactInstructionsFunctionHasher | NodeSort const::GetShortcutIx(...) | `GetShortcutIx__8NodeSortCFP8SortNode` |

### Player.o — 7 ids (high 0, ≥30 0, 20-30 3, 15-20 4)  ·  `src/band3/game/Player.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82683f78` | `0x801c3ae0` | bsim 21 | BSIM | Player::DisableOverdrivePhrases(...) | `DisableOverdrivePhrases__6PlayerFv` |
| `0x82684130` | `0x801c4ba0` | bsim 23 | BSIM | Player::CheckCrowdFailure(...) | `CheckCrowdFailure__6PlayerFv` |
| `0x82684418` | `0x801c4ec0` | bsim 22 | BSIM | Player const::UnisonMiss(...) | `UnisonMiss__6PlayerCFi` |
| `0x82683b98` | `0x801c13c0` | bsim 17 | BSIM | Player::RebuildPhrases(...) | `RebuildPhrases__6PlayerFv` |
| `0x82683de0` | `0x801c1f80` | bsim 16 | BSIM | Player::AddBonusPoints(...) | `AddBonusPoints__6PlayerFi` |
| `0x82684078` | `0x801c3d10` | bsim 19 | BSIM | Player const::SavePersistentData(...) | `SavePersistentData__6PlayerCFR20PersistentPlayerData` |
| `0x82686e78` | `0x801c4620` | bsim 18 | BSIM | Player::UpdateEnergy(...) | `UpdateEnergy__6PlayerFRC7SongPos` |

### VocalTrack.o — 7 ids (high 0, ≥30 0, 20-30 3, 15-20 4)  ·  `src/band3/bandtrack/VocalTrack.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82b6e140` | `0x80157d00` | bsim 22 | BSIM | VocalTrack::ProcessStaticLyrics(...) | `ProcessStaticLyrics__10VocalTrackFbP5LyricRfRfRP5LyricRP5LyricRfbP10LyricPlate` |
| `0x82b71050` | `0x8015bb10` | bsim 24 | BSIM | stlpmtx_std::_Deque_impl<Pv,Q211stlpmtx_std16StlNodeAlloc<Pv>>::_M_push_back_aux_v(...) | `_M_push_back_aux_v__Q211stlpmtx_std49_Deque_impl<Pv,Q211stlpmtx_std16StlNodeAlloc<Pv>>FRCPv` |
| `0x82b743e8` | `0x801529a0` | bsim 22 | BSIM | VocalTrack::UpdateVocalStyle(...) | `UpdateVocalStyle__10VocalTrackFv` |
| `0x827ca508` | `0x80153f90` | bsim 18 | BSIM | MakeString<f,PCc>(...)   [free function] | `MakeString<f,PCc>__FPCcfPCc_PCc` |
| `0x82b6e7f0` | `0x8015bbc0` | bsim 18 | BSIM | stlpmtx_std::_Deque_impl<Pv,Q211stlpmtx_std16StlNodeAlloc<Pv>>::_M_pop_front_aux(...) | `_M_pop_front_aux__Q211stlpmtx_std49_Deque_impl<Pv,Q211stlpmtx_std16StlNodeAlloc<Pv>>Fv` |
| `0x82b710f8` | `0x8015b510` | bsim 15 | BSIM | stlpmtx_std::_Deque_impl<Q210VocalTrack10RangeShift,Q211stlpmtx_std40StlNodeAlloc<Q210VocalTrack10RangeShift>>::_M_push_back_aux_v(...) | `_M_push_back_aux_v__Q211stlpmtx_std97_Deque_impl<Q210VocalTrack10RangeShift,Q211stlpmtx_std40StlNodeAlloc<Q210VocalTrack10RangeShift>>FRCQ210VocalTrack10RangeShift` |
| `0x82b73370` | `0x80157970` | bsim 19 | BSIM | VocalTrack::BuildStaticDeployZone(...) | `BuildStaticDeployZone__10VocalTrackFiRCQ211stlpmtx_std9pair<f,f>fRfRQ211stlpmtx_std91deque<Q210VocalTrack10LyricShift,Q211stlpmtx_std40StlNodeAlloc<Q210VocalTrack10LyricShift>>` |

### MetaPerformer.o — 4 ids (high 0, ≥30 0, 20-30 1, 15-20 3)  ·  `src/band3/meta_band/MetaPerformer.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x825691d0` | `0x802ed970` | bsim 21 | BSIM | MetaPerformer::SetBattle(...) | `SetBattle__13MetaPerformerFPC18BattleSavedSetlist` |
| `0x82564468` | `0x802f2670` | bsim 18 | BSIM | MetaPerformer const::IsRandomSetList(...) | `IsRandomSetList__13MetaPerformerCFv` |
| `0x82564498` | `0x802f2d30` | bsim 18 | BSIM | MetaPerformer const::HasSyncPermission(...) | `HasSyncPermission__13MetaPerformerCFv` |
| `0x82566a78` | `0x802f27d0` | bsim 19 | BSIM | MetaPerformer::OnSynchronized(...) | `OnSynchronized__13MetaPerformerFUi` |

### Gem.o — 3 ids (high 0, ≥30 0, 20-30 2, 15-20 1)  ·  `src/band3/bandtrack/Gem.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8229f730` | `0x8012b3f0` | bsim 24 | BSIM | __as__3GemFRC3Gem | `__as__3GemFRC3Gem` |
| `0x82b79d18` | `0x8012d1e0` | bsim 22 | BSIM | Gem::RemoveAllInstances(...) | `RemoveAllInstances__3GemFv` |
| `0x82b79348` | `0x8012da90` | bsim 16 | BSIM | Gem::ReleaseSlot(...) | `ReleaseSlot__3GemFi` |

### GemSmasher.o — 3 ids (high 0, ≥30 0, 20-30 1, 15-20 2)  ·  `src/band3/bandtrack/GemSmasher.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82b7dc08` | `0x8013bd90` | bsim 28 | BSIM | GemSmasher::SetGlowing(...) | `SetGlowing__10GemSmasherFb` |
| `0x82b7dbb0` | `0x8013bca0` | bsim 18 | BSIM | GemSmasher::StopBurn(...) | `StopBurn__10GemSmasherFv` |
| `0x82b7dd48` | `0x8013bd00` | bsim 17 | BSIM | GemSmasher::FillHit(...) | `FillHit__10GemSmasherFi` |

### PracticePanel.o — 3 ids (high 0, ≥30 0, 20-30 3, 15-20 0)  ·  `src/band3/game/PracticePanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82693818` | `0x801c88f0` | bsim 26 | BSIM | PracticePanel::SetPitchShiftRatio(...) | `SetPitchShiftRatio__13PracticePanelFf` |
| `0x82693ff0` | `0x801c90b0` | bsim 26 | BSIM | PracticePanel::ToggleGuidePart(...) | `ToggleGuidePart__13PracticePanelFv` |
| `0x826943d0` | `0x801c9470` | bsim 24 | BSIM | PracticePanel::MarkGemsAsProcessed(...) | `MarkGemsAsProcessed__13PracticePanelFv` |

### VocalGuidePitch.o — 3 ids (high 0, ≥30 0, 20-30 2, 15-20 1)  ·  `src/band3/game/VocalGuidePitch.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826d3088` | `0x80207460` | bsim 27 | BSIM | VocalGuidePitch::EnableGuideTrack(...) | `EnableGuideTrack__15VocalGuidePitchFi` |
| `0x826d3168` | `0x80207190` | bsim 28 | BSIM | VocalGuidePitch::Poll(...) | `Poll__15VocalGuidePitchFf` |
| `0x826d34d0` | `0x80206670` | bsim 16 | BSIM | VocalGuidePitch::VocalGuidePitch(...) | `__ct__15VocalGuidePitchFv` |

### BandPerformer.o — 2 ids (high 0, ≥30 0, 20-30 2, 15-20 0)  ·  `src/band3/game/BandPerformer.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826cef70` | `0x80160fa0` | bsim 23 | BSIM | BandPerformer::ComputeScoreData(...) | `ComputeScoreData__13BandPerformerFv` |
| `0x826cf150` | `0x80161570` | bsim 28 | BSIM | BandPerformer const::NoOneContributingToCrowd(...) | `NoOneContributingToCrowd__13BandPerformerCFv` |

### BandSongMetadata.o — 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1)  ·  `src/band3/meta_band/BandSongMetadata.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x827f2ee8` | `0x8026c6f0` | bsim 21 | BSIM | BandSongMetadata::Save(...) | `Save__16BandSongMetadataFR9BinStream` |
| `0x825864c8` | `0x8026b950` | bsim 18 | BSIM | BandSongMetadata const::SongKey(...) | `SongKey__16BandSongMetadataCFv` |

### BandUser.o — 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1)  ·  `src/band3/game/BandUser.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8266cf60` | `0x801624b0` | bsim 27 | BSIM | BandUser const::GetTrackIcon(...) | `GetTrackIcon__8BandUserCFv` |
| `0x8266ecb0` | `0x80162c30` | bsim 16 | BSIM | BandUser::SetLoadedPrefabChar(...) | `SetLoadedPrefabChar__8BandUserFi` |

### CustomizePanel.o — 2 ids (high 0, ≥30 0, 20-30 2, 15-20 0)  ·  `src/band3/meta_band/CustomizePanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x825fb3d0` | `0x802b5bb0` | bsim 24 | BSIM | CustomizePanel::Unload(...) | `Unload__14CustomizePanelFv` |
| `0x825fb548` | `0x802b6430` | bsim 22 | BSIM | CustomizePanel::UpdateAssetProvider(...) | `UpdateAssetProvider__14CustomizePanelFv` |

### EntityUploader.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 2)  ·  `src/band3/net_band/EntityUploader.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x824fa108` | `0x803f08d0` | bsim 15 | BSIM | EntityUploader::EntityUploader(...) | `__ct__14EntityUploaderFv` |
| `0x824facf8` | `0x803f19e0` | bsim 18 | BSIM | EntityUploader::GetNumUpdates(...) | `GetNumUpdates__14EntityUploaderFP11BandProfile` |

### FadePanel.o — 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1)  ·  `src/band3/game/FadePanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8268a058` | `0x801754f0` | bsim 21 | BSIM | FadePanel::Unload(...) | `Unload__9FadePanelFv` |
| `0x8268a150` | `0x80175610` | bsim 16 | BSIM | FadePanel::Enter(...) | `Enter__9FadePanelFv` |

### GemTrack.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 2)  ·  `src/band3/bandtrack/GemTrack.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82b63148` | `0x8013f9d0` | bsim 19 | BSIM | GemTrack::HandleNewSong(...) | `HandleNewSong__8GemTrackFv` |
| `0x82b63410` | `0x801405f0` | bsim 15 | BSIM | GemTrack::OverrideRangeShift(...) | `OverrideRangeShift__8GemTrackFff` |

### Leaderboard.o — 2 ids (high 0, ≥30 0, 20-30 2, 15-20 0)  ·  `src/band3/meta_band/Leaderboard.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8264ee00` | `0x802cc5c0` | bsim 26 | BSIM | LeaderboardRow::LeaderboardRow(...) | `__ct__14LeaderboardRowFv` |
| `0x8264f170` | `0x802ce4a0` | bsim 24 | BSIM | Leaderboard::SetMode(...) | `SetMode__11LeaderboardFQ211Leaderboard4Modeb` |

### MainHubPanel.o — 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1)  ·  `src/band3/meta_band/MainHubPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826051b0` | `0x802dbef0` | bsim 21 | BSIM | MainHubPanel::CycleNextMessage(...) | `CycleNextMessage__12MainHubPanelFv` |
| `0x82603958` | `0x802e0320` | bsim 17 | BSIM | @unnamed@MainHubPanel_cpp@::MainHubAdvanceMsg::Load(...) | `Load__Q226@unnamed@MainHubPanel_cpp@17MainHubAdvanceMsgFR9BinStream` |

### SetlistMergePanel.o — 2 ids (high 0, ≥30 0, 20-30 2, 15-20 0)  ·  `src/band3/meta_band/SetlistMergePanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82617730` | `0x80364640` | bsim 20 | BSIM | SetlistMergePanel::SetLocalsMerging(...) | `SetLocalsMerging__17SetlistMergePanelFb` |
| `0x82617830` | `0x80365020` | bsim 24 | BSIM | SetlistMergePanel::SendSongsToMetaPerformer(...) | `SendSongsToMetaPerformer__17SetlistMergePanelFRCQ211stlpmtx_std45vector<i,Us,Q211stlpmtx_std15StlNodeAlloc<i>>` |

### Singer.o — 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1)  ·  `src/band3/game/Singer.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826d8d68` | `0x801d9750` | bsim 23 | BSIM | Singer::AddToFreestyleDeployment(...) | `AddToFreestyleDeployment__6SingerFf` |
| `0x826d8a80` | `0x801d7260` | bsim 16 | BSIM | Singer::CreateMicClientID(...) | `CreateMicClientID__6SingerFv` |

### Tail.o — 2 ids (high 0, ≥30 0, 20-30 1, 15-20 1)  ·  `src/band3/bandtrack/Tail.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82b7e618` | `0x80144ea0` | bsim 24 | BSIM | Tail::Hit(...) | `Hit__4TailFv` |
| `0x82b7e3d0` | `0x80144db0` | bsim 17 | BSIM | Tail::ReleaseMeshes(...) | `ReleaseMeshes__4TailFv` |

### TourProgress.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 2)  ·  `src/band3/tour/TourProgress.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8234ff50` | `0x803d8100` | bsim 16 | BSIM | TourProgress::RemoveStars(...) | `RemoveStars__12TourProgressFi` |
| `0x8234ff68` | `0x803d8120` | bsim 15 | BSIM | TourProgress::EarnStars(...) | `EarnStars__12TourProgressFi` |

### TrackConfig.o — 2 ids (high 0, ≥30 0, 20-30 2, 15-20 0)  ·  `src/band3/bandtrack/TrackConfig.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82b78200` | `0x80148400` | bsim 28 | BSIM | TrackConfig const::AllowsOverlappingGems(...) | `AllowsOverlappingGems__11TrackConfigCFv` |
| `0x82b78288` | `0x80148490` | bsim 24 | BSIM | TrackConfig const::IsRealGuitarTrack(...) | `IsRealGuitarTrack__11TrackConfigCFv` |

### BandStorePanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/meta_band/BandStorePanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x822e3f40` | `0x80279de0` | bsim 15 | BSIM | MakeString<Us>(...)   [free function] | `MakeString<Us>__FPCcUs_PCc` |

### CalibrationPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/meta_band/CalibrationPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x825ec650` | `0x8028eb70` | bsim 17 | BSIM | CalibrationPanel::UpdateStream(...) | `UpdateStream__16CalibrationPanelFv` |

### CampaignSongInfoPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/meta_band/CampaignSongInfoPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826035a8` | `0x8028b9c0` | bsim 18 | BSIM | CampaignSongInfoPanel::Unload(...) | `Unload__21CampaignSongInfoPanelFv` |

### ChordbookPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/game/ChordbookPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826966f0` | `0x8016c4c0` | bsim 17 | BSIM | ChordbookPanel::CreateController(...) | `CreateController__14ChordbookPanelFv` |

### CrowdRating.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/game/CrowdRating.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826cf9b0` | `0x801717e0` | bsim 26 | BSIM | CrowdRating const::GetThreshold(...) | `GetThreshold__11CrowdRatingCF15ExcitementLevel` |

### DirectInstrument.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/game/DirectInstrument.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826c4890` | `0x80173e40` | bsim 22 | BSIM | DirectInstrument::Disable(...) | `Disable__16DirectInstrumentFv` |

### EditSetlistPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/meta_band/EditSetlistPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x825fe3e0` | `0x802bbcc0` | bsim 16 | BSIM | EditSetlistPanel::CleanupStringVerify(...) | `CleanupStringVerify__16EditSetlistPanelFv` |

### FocusTracker.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/game/FocusTracker.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826b7b08` | `0x80177940` | bsim 19 | BSIM | FocusTracker::ActivateFocus(...) | `ActivateFocus__12FocusTrackerFf` |

### GameConfig.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/game/GameConfig.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8266aca8` | `0x80186af0` | bsim 22 | BSIM | GameConfig const::GetSectionBoundsTick(...) | `GetSectionBoundsTick__10GameConfigCFiRiRi` |

### GameTimePanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/meta_band/GameTimePanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826022a0` | `0x802c0fd0` | bsim 16 | BSIM | GameTimePanel::Enter(...) | `Enter__13GameTimePanelFv` |

### GameplayOptions.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/meta_band/GameplayOptions.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82637f48` | `0x802c0550` | bsim 23 | BSIM | GameplayOptions::SetVocalVolume(...) | `SetVocalVolume__15GameplayOptionsFii` |

### InterstitialPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/meta_band/InterstitialPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826027f8` | `0x802c9ba0` | bsim 16 | BSIM | InterstitialPanel const::Exiting(...) | `Exiting__17InterstitialPanelCFv` |

### MetaPanel.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/meta_band/MetaPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8255a890` | `0x802ea6f0` | bsim 21 | BSIM | MetaPanel::Unload(...) | `Unload__9MetaPanelFv` |

### Metronome.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/game/Metronome.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826d1e98` | `0x801ab2d0` | bsim 16 | BSIM | Metronome::Exit(...) | `Exit__9MetronomeFv` |

### NextSongPanel.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/meta_band/NextSongPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8262f3a8` | `0x8030eb60` | bsim 21 | BSIM | NextSongPanel::Enter(...) | `Enter__13NextSongPanelFv` |

### OverdriveTracker.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/game/OverdriveTracker.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826be0e0` | `0x801b56f0` | bsim 16 | BSIM | OverdriveTracker::LocalEndDeployStreak(...) | `LocalEndDeployStreak__16OverdriveTrackerFf` |

### OvershellPanel.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/meta_band/OvershellPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8259df90` | `0x8031c470` | bsim 24 | BSIM | OvershellPanel::ResolveSlotStates(...) | `ResolveSlotStates__14OvershellPanelFv` |

### PerformanceData.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/meta_band/PerformanceData.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8263b038` | `0x80340940` | bsim 28 | BSIM | PerformanceData::PerformanceData(...) | `__ct__15PerformanceDataFv` |

### PlayerBehavior.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/game/PlayerBehavior.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826d0108` | `0x801c7380` | bsim 16 | BSIM | PlayerBehavior::PlayerBehavior(...) | `__ct__14PlayerBehaviorFv` |

### PlayerLeaderboards.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/meta_band/PlayerLeaderboards.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82656348` | `0x80344980` | bsim 16 | BSIM | PlayerBattleLeaderboard::PlayerBattleLeaderboard(...) | `__ct__23PlayerBattleLeaderboardFP7ProfilePQ211Leaderboard8Callbacki` |

### PrefabMgr.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/meta_band/PrefabMgr.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82541120` | `0x80344f20` | bsim 17 | BSIM | PrefabMgr::Poll(...) | `Poll__9PrefabMgrFv` |

### QuestFilterPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/tour/QuestFilterPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82b4b768` | `0x803bb810` | bsim 18 | BSIM | QuestFilterPanel::HandleLeaderToggledFilters(...) | `HandleLeaderToggledFilters__16QuestFilterPanelFb` |

### QuestManager.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/tour/QuestManager.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82346e18` | `0x803b85b0` | bsim 24 | BSIM | QuestManager::Cleanup(...) | `Cleanup__12QuestManagerFv` |

### SongSortMgr.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/meta_band/SongSortMgr.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8257e968` | `0x8037b060` | bsim 18 | BSIM | __ls__FR9BinStreamRCQ211SongSortMgr10SongFilter | `__ls__FR9BinStreamRCQ211SongSortMgr10SongFilter` |

### SongSortNode.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/meta_band/SongSortNode.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82643ae8` | `0x80380530` | bsim 16 | BSIM | OwnedSongSortNode const::IsEnabled(...) | `IsEnabled__17OwnedSongSortNodeCFv` |

### StandIn.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/meta_band/StandIn.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x825d13e8` | `0x80388c90` | bsim 16 | BSIM | StandIn::StandIn(...) | `__ct__7StandInFv` |

### StatCollector.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/game/StatCollector.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826d7970` | `0x801e1cf0` | bsim 20 | BSIM | StatCollector::Poll(...) | `Poll__13StatCollectorFf` |

### TambourineDetector.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/game/TambourineDetector.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826ddca0` | `0x801eb330` | bsim 25 | BSIM | TambourineDetector::CheckForSwing(...) | `CheckForSwing__18TambourineDetectorFff` |

### Tour.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/tour/Tour.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82348d08` | `0x803beef0` | bsim 28 | BSIM | Tour::ClearPerformer(...) | `ClearPerformer__4TourFv` |

### TourBand.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/tour/TourBand.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82b4a6a0` | `0x803c4bc0` | bsim 28 | BSIM | TourBand::ChooseBandLogo(...) | `ChooseBandLogo__8TourBandFii` |

### TourPerformer.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/tour/TourPerformer.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8234e678` | `0x803d1ae0` | bsim 30 | BSIM | TourPerformerImpl const::GetQuestSuccessfulSongCount(...) | `GetQuestSuccessfulSongCount__17TourPerformerImplCFv` |

### TrainingPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/meta_band/TrainingPanel.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x82621988` | `0x8039aae0` | bsim 17 | BSIM | TrainingPanel::LeaveState(...) | `LeaveState__13TrainingPanelFv` |

### UIStats.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/meta_band/UIStats.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x8254bd10` | `0x8039fa80` | bsim 19 | BSIM | UIStats::Init(...) | `Init__7UIStatsFv` |

### VocalScoreHistory.o — 1 ids (high 0, ≥30 0, 20-30 1, 15-20 0)  ·  `src/band3/game/VocalScoreHistory.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x826dd928` | `0x802184d0` | bsim 22 | BSIM | VocalScoreHistory::AddScore(...) | `AddScore__17VocalScoreHistoryFff` |

### WaitingUserGate.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 1)  ·  `src/band3/meta_band/WaitingUserGate.cpp`

| Xenon addr | Bank-8 | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|
| `0x825951a0` | `0x803a8080` | bsim 17 | BSIM | EnterFlowMsg::Load(...) | `Load__12EnterFlowMsgFR9BinStream` |

---

## For the next agent

- **Regenerate:** `python3 tools/gen_band3_port_worklist.py` (cwd-independent; reads `ghidriff_identities.json` + `scripts/target_symbol_map.json` + the rb3 CW map; the script VERIFIES every `wii_symbol` resolves to its claimed Bank-8 addr and that 0 entries are already in the production map, exiting non-zero on any failure).
- **Data feed:** `band3_port_worklist.json` — per-fn rows + `tu_summary` + `ranked_tus` for machine ingestion by the `wf_classa_harvest.js` Scan/Validate stages.
- **Do NOT inject these into `target_symbol_map.json`** — CW≠MSVC mangling, TUs uncompiled; wrong key mis-pairs objdiff at ~0.90 precision. Confirm each name when the TU is actually ported.
- **Validation gap still open:** the ~530 net-new system/network identities are unjudged at human grade (round-2 judging was band3-only).
