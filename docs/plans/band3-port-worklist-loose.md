# band3 porting worklist — net-new Wii→Xenon identities (DC3-cannot-provide)

**Generated:** `tools/gen_band3_port_worklist.py` (regenerable). **Source:** `ghidriff_identities_loose.json` (ACCEPT tier) minus `scripts/target_symbol_map.json`.
**Data feed:** `band3_port_worklist_loose.json` (machine-readable, one row per fn; gitignored/regenerable).

> **LOOSE BAND (ws2, BSim simconf 10–15).** These are CANDIDATES from the run-3 archive re-vetted at the looser ≥10 operating point (sibling-check REJECT applied). Measured band precision ≈ **0.85** (ws2 20-pair judging: 18 confirmed / 2 plausible / 0 wrong on non-contradicted rows). **Confirm-on-consume every id** (diff vtable-slot/type-tag/node-size immediates + strings + resolved callees vs the Wii body); **skip `rb3wii=contradicted` rows** unless separately judged. This is a future-round candidate pool — no strict matches are minted from it directly.

> **Scope note, 2026-08-02 — this ranking is for the X360 MATCH.** Same caveat as the
> strict worklist: every TU here is under `src/band3/`, and the *native* venue/render
> milestone is ranked separately and disjointly in
> [`band3-native-unblock-priority-2026-08-02.md`](band3-native-unblock-priority-2026-08-02.md).
> The 14 classes blocking every RB3 venue root are `src/system/{bandobj,synth,world,ui}`
> — **none in `src/band3/`** (X4a's contrary claim retracted; `x4b-animation-2026-08-02.md` §4).
> Nothing below is superseded for matching purposes.

## What this is

301 RB3 **game-code** functions across **105 TUs**, each pinned to a specific Wii (Bank-8, CodeWarrior-mangled) function by the forked-ghidriff/BSim Wii→Xenon identity pipeline. These are **net-new**: their Xenon address is NOT yet in the production pairing set (`target_symbol_map.json`), and they live in band3 TUs the active class-A port has **not yet reached**. band3 is RB3-specific gameplay/scoring/song/tracker code — **DC3 (Dance Central 3, no Rock Band gameplay) fundamentally cannot identify these.** This is the irreplaceable core of the Wii→Xenon lever.

**This is a targeting/porting worklist + per-fn identity oracle, NOT a `target_symbol_map.json` injection.** The TUs aren't compiled yet, so there is no MSVC symbol to pair against; and our `wii_symbol` is CW/MWCC-mangled, not MSVC-mangled — injecting it as a map key would mis-pair objdiff at our ~0.90 precision (actively harmful). Use this to pick which TU to port next and, when porting, to name each function from the Wii body. Both outputs are additive + reversible.

## How to consume

- **Pick the next TU** from the ranking below (highest yield × certainty first). The `wf_classa_harvest.js` Scan stage / coordinator picks the next band3 TU from this ranking instead of a blind string-anchor guess.
- **Cross-check `OWN` attribution.** A function this worklist pins to a TU at `high`/`bsim≥30` confidence is strong independent evidence for the Validate stage's `OWN` verdict (better than the near-random `unified_id_rb3wii.json` oracle).
- **Name from the Wii body.** In the rb3 repo: `bin/analyze-function <wii_symbol>` shows the Bank-8-accurate body + real arg shape; `wii_addr_bank8` is the Bank-8 address.
- **fn_resolver T4b** (`ghidriff_wii_b8`) already serves all 978 identities for per-address resolution; this worklist adds the missing **TU-priority + per-TU member roster**.

## Confidence strata (the measured prior)

Loose-band (ws2) judged precision (n=20 non-contradicted) ≈ **0.85–1.00** (18 confirmed / 2 plausible / 0 wrong). Totals here: **0 high** · **0 bsim≥30** · **0 bsim20-30** · **0 bsim15-20** · **301 bsim10-15**.

- **high** — `ExactInstructions`/`SwitchSig`/`Implied`/`SymbolsHash`, or BSim simconf ≥ 30. The safest-first targets.
- **bsim≥30 / bsim20-30 / bsim15-20** — BSim similarity×confidence bands; lower = vet harder.

**Dominant failure mode (~10%): same-TU sibling aliasing.** Near-identical template/sibling bodies differing only in a type-tag immediate (e.g. `kDataFloat` vs `kDataInt`) or an STL node-size literal, or a hash-shape match a string later refutes. When confirming a name, diff the small immediates / node-size literals and referenced strings against the Wii body.

## HIGH-confidence subset (safest first — verify these names with most trust)

| Xenon addr | TU | src | confidence | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|

## TU ranking (port these first — by #high+#bsim≥30 desc, then total desc)

| Rank | TU | src | #ids | high | ≥30 | 20-30 | 15-20 | 10-15 | contra | DC3? |
|---|---|---|---|---|---|---|---|---|---|---|
| 1 | SongDB.o | `src/band3/game/SongDB.cpp` | 21 | 0 | 0 | 0 | 0 | 21 | 5 | cannot-provide |
| 2 | Game.o | `src/band3/game/Game.cpp` | 12 | 0 | 0 | 0 | 0 | 12 | 0 | cannot-provide |
| 3 | GemManager.o | `src/band3/bandtrack/GemManager.cpp` | 11 | 0 | 0 | 0 | 0 | 11 | 2 | cannot-provide |
| 4 | ProfileMgr.o | `src/band3/meta_band/ProfileMgr.cpp` | 11 | 0 | 0 | 0 | 0 | 11 | 4 | cannot-provide |
| 5 | MusicLibrary.o | `src/band3/meta_band/MusicLibrary.cpp` | 8 | 0 | 0 | 0 | 0 | 8 | 1 | cannot-provide |
| 6 | TrackConfig.o | `src/band3/bandtrack/TrackConfig.cpp` | 8 | 0 | 0 | 0 | 0 | 8 | 1 | cannot-provide |
| 7 | VocalPlayer.o | `src/band3/game/VocalPlayer.cpp` | 7 | 0 | 0 | 0 | 0 | 7 | 0 | cannot-provide |
| 8 | BandProfile.o | `src/band3/meta_band/BandProfile.cpp` | 8 | 0 | 0 | 0 | 0 | 8 | 2 | cannot-provide |
| 9 | BandUserMgr.o | `src/band3/game/BandUserMgr.cpp` | 7 | 0 | 0 | 0 | 0 | 7 | 1 | cannot-provide |
| 10 | GemTrack.o | `src/band3/bandtrack/GemTrack.cpp` | 7 | 0 | 0 | 0 | 0 | 7 | 1 | cannot-provide |
| 11 | TourProgress.o | `src/band3/tour/TourProgress.cpp` | 7 | 0 | 0 | 0 | 0 | 7 | 1 | cannot-provide |
| 12 | Stats.o | `src/band3/game/Stats.cpp` | 6 | 0 | 0 | 0 | 0 | 6 | 0 | cannot-provide |
| 13 | Performer.o | `src/band3/game/Performer.cpp` | 10 | 0 | 0 | 0 | 0 | 10 | 5 | cannot-provide |
| 14 | GemSmasher.o | `src/band3/bandtrack/GemSmasher.cpp` | 6 | 0 | 0 | 0 | 0 | 6 | 1 | cannot-provide |
| 15 | BandPerformer.o | `src/band3/game/BandPerformer.cpp` | 5 | 0 | 0 | 0 | 0 | 5 | 0 | cannot-provide |
| 16 | VocalPart.o | `src/band3/game/VocalPart.cpp` | 5 | 0 | 0 | 0 | 0 | 5 | 0 | cannot-provide |
| 17 | TrainerPanel.o | `src/band3/game/TrainerPanel.cpp` | 6 | 0 | 0 | 0 | 0 | 6 | 2 | cannot-provide |
| 18 | GemPlayer.o | `src/band3/game/GemPlayer.cpp` | 5 | 0 | 0 | 0 | 0 | 5 | 1 | cannot-provide |
| 19 | Lyric.o | `src/band3/bandtrack/Lyric.cpp` | 5 | 0 | 0 | 0 | 0 | 5 | 1 | cannot-provide |
| 20 | TrackerUtils.o | `src/band3/game/TrackerUtils.cpp` | 5 | 0 | 0 | 0 | 0 | 5 | 1 | cannot-provide |
| 21 | CrowdRating.o | `src/band3/game/CrowdRating.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 0 | cannot-provide |
| 22 | TourSavable.o | `src/band3/tour/TourSavable.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 0 | cannot-provide |
| 23 | PracticePanel.o | `src/band3/game/PracticePanel.cpp` | 5 | 0 | 0 | 0 | 0 | 5 | 2 | cannot-provide |
| 24 | BandUser.o | `src/band3/game/BandUser.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 1 | cannot-provide |
| 25 | InterstitialPanel.o | `src/band3/meta_band/InterstitialPanel.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 1 | cannot-provide |
| 26 | RockCentral.o | `src/band3/net_band/RockCentral.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 1 | cannot-provide |
| 27 | Band.o | `src/band3/game/Band.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | cannot-provide |
| 28 | NetGameMsgs.o | `src/band3/game/NetGameMsgs.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | cannot-provide |
| 29 | Player.o | `src/band3/game/Player.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | cannot-provide |
| 30 | Scoring.o | `src/band3/game/Scoring.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | cannot-provide |
| 31 | SessionMgr.o | `src/band3/meta_band/SessionMgr.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | cannot-provide |
| 32 | TrackPanel.o | `src/band3/bandtrack/TrackPanel.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | cannot-provide |
| 33 | VocalTrack.o | `src/band3/bandtrack/VocalTrack.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 0 | cannot-provide |
| 34 | OvershellSlot.o | `src/band3/meta_band/OvershellSlot.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 2 | cannot-provide |
| 35 | TrackerManager.o | `src/band3/game/TrackerManager.cpp` | 4 | 0 | 0 | 0 | 0 | 4 | 2 | cannot-provide |
| 36 | Tour.o | `src/band3/tour/Tour.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 1 | cannot-provide |
| 37 | AppLabel.o | `src/band3/meta_band/AppLabel.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 38 | CharSync.o | `src/band3/meta_band/CharSync.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 39 | CommonPhraseCapturer.o | `src/band3/game/CommonPhraseCapturer.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 40 | DirectInstrument.o | `src/band3/game/DirectInstrument.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 41 | FadePanel.o | `src/band3/game/FadePanel.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 42 | GemTrainerPanel.o | `src/band3/game/GemTrainerPanel.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 43 | MetaPerformer.o | `src/band3/meta_band/MetaPerformer.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 44 | RealGuitarGemPlayer.o | `src/band3/game/RealGuitarGemPlayer.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 45 | Singer.o | `src/band3/game/Singer.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 46 | TambourineManager.o | `src/band3/game/TambourineManager.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 0 | cannot-provide |
| 47 | Tracker.o | `src/band3/game/Tracker.cpp` | 3 | 0 | 0 | 0 | 0 | 3 | 2 | cannot-provide |
| 48 | MultiplayerAnalyzer.o | `src/band3/game/MultiplayerAnalyzer.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | cannot-provide |
| 49 | OvershellPanel.o | `src/band3/meta_band/OvershellPanel.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 1 | cannot-provide |
| 50 | AccomplishmentSongConditional.o | `src/band3/meta_band/AccomplishmentSongConditional.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 51 | BandMachine.o | `src/band3/meta_band/BandMachine.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 52 | BandSongMgr.o | `src/band3/meta_band/BandSongMgr.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 53 | CampaignLevel.o | `src/band3/meta_band/CampaignLevel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 54 | CharCache.o | `src/band3/meta_band/CharCache.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 55 | CharacterCreatorPanel.o | `src/band3/meta_band/CharacterCreatorPanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 56 | ContentDeletePanel.o | `src/band3/meta_band/ContentDeletePanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 57 | CustomizePanel.o | `src/band3/meta_band/CustomizePanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 58 | Defines.o | `src/band3/game/Defines.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 59 | FixedSetlist.o | `src/band3/tour/FixedSetlist.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 60 | FocusTracker.o | `src/band3/game/FocusTracker.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 61 | FreestylePanel.o | `src/band3/game/FreestylePanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 62 | GameConfig.o | `src/band3/game/GameConfig.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 63 | GamePanel.o | `src/band3/game/GamePanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 64 | Gem.o | `src/band3/bandtrack/Gem.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 65 | HeaderPerformanceProvider.o | `src/band3/meta_band/HeaderPerformanceProvider.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 66 | HeldNote.o | `src/band3/game/HeldNote.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 67 | LockStepMgr.o | `src/band3/meta_band/LockStepMgr.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 68 | MultiSelectListPanel.o | `src/band3/meta_band/MultiSelectListPanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 69 | PatchSelectPanel.o | `src/band3/meta_band/PatchSelectPanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 70 | PlayerBehavior.o | `src/band3/game/PlayerBehavior.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 71 | RGTrainerPanel.o | `src/band3/game/RGTrainerPanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 72 | SaveLoadManager.o | `src/band3/meta_band/SaveLoadManager.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 73 | SavedSetlist.o | `src/band3/meta_band/SavedSetlist.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 74 | ScoreTracker.o | `src/band3/game/ScoreTracker.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 75 | ScoreUtl.o | `src/band3/game/ScoreUtl.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 76 | SelectDifficultyPanel.o | `src/band3/meta_band/SelectDifficultyPanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 77 | SongSortNode.o | `src/band3/meta_band/SongSortNode.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 78 | SongStatusMgr.o | `src/band3/meta_band/SongStatusMgr.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 79 | StatMemberTracker.o | `src/band3/game/StatMemberTracker.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 80 | StoreOfferProvider.o | `src/band3/meta_band/StoreOfferProvider.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 81 | Tail.o | `src/band3/bandtrack/Tail.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 82 | TourCharLocal.o | `src/band3/tour/TourCharLocal.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 83 | TourDesc.o | `src/band3/tour/TourDesc.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 84 | Track.o | `src/band3/bandtrack/Track.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 85 | TrainingMgr.o | `src/band3/meta_band/TrainingMgr.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 86 | TrainingPanel.o | `src/band3/meta_band/TrainingPanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 87 | UIStats.o | `src/band3/meta_band/UIStats.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 88 | UploadErrorMgr.o | `src/band3/meta_band/UploadErrorMgr.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 89 | VocalGuidePitch.o | `src/band3/game/VocalGuidePitch.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 0 | cannot-provide |
| 90 | GraphicsUtl.o | `src/band3/bandtrack/GraphicsUtl.cpp` | 2 | 0 | 0 | 0 | 0 | 2 | 2 | cannot-provide |
| 91 | AccomplishmentProgress.o | `src/band3/meta_band/AccomplishmentProgress.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 92 | BandPreloadPanel.o | `src/band3/meta_band/BandPreloadPanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 93 | BandSongMetadata.o | `src/band3/meta_band/BandSongMetadata.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 94 | CalibrationPanel.o | `src/band3/meta_band/CalibrationPanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 95 | ClosetMgr.o | `src/band3/meta_band/ClosetMgr.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 96 | DataResults.o | `src/band3/net_band/DataResults.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 97 | MainHubPanel.o | `src/band3/meta_band/MainHubPanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 98 | OverdriveTimeTracker.o | `src/band3/game/OverdriveTimeTracker.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 99 | PatchPanel.o | `src/band3/meta_band/PatchPanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 100 | SetlistToStorePanel.o | `src/band3/meta_band/SetlistToStorePanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 101 | SongSetlistProvider.o | `src/band3/meta_band/SongSetlistProvider.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 102 | SyncGameStartPanel.o | `src/band3/game/SyncGameStartPanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 103 | TexLoadPanel.o | `src/band3/meta_band/TexLoadPanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 104 | VocalScoreHistory.o | `src/band3/game/VocalScoreHistory.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |
| 105 | VocalTrainerPanel.o | `src/band3/game/VocalTrainerPanel.cpp` | 1 | 0 | 0 | 0 | 0 | 1 | 1 | cannot-provide |

## Per-TU function rosters

Each TU's identities, confidence-ranked. `wii_symbol` is the CW/MWCC ground-truth name (`bin/analyze-function <wii_symbol>` in the rb3 repo for the real body).

### SongDB.o — 21 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 21, contra 5)  ·  `src/band3/game/SongDB.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82356ca8` | `0x801dbe90` | bsim 11.6 | absent | BSIM | SongDB const::GetSongDurationMs(...) | `GetSongDurationMs__6SongDBCFv` |
| `0x82666ef0` | `0x801dbe60` | bsim 11.6 | absent | BSIM | SongDB::OverrideBasePoints(...) | `OverrideBasePoints__6SongDBFi9TrackTypeRC8UserGuidiii` |
| `0x82666f10` | `0x801dbeb0` | bsim 10.5 | contradicted | BSIM | SongDB const::IsInCoda(...) | `IsInCoda__6SongDBCFi` |
| `0x82666f30` | `0x801dbed0` | bsim 11.8 | absent | BSIM | SongDB const::GetNumTracks(...) | `GetNumTracks__6SongDBCFv` |
| `0x82666f40` | `0x801dbf50` | bsim 14.3 | absent | BSIM | SongDB const::GetBaseBonusPoints(...) | `GetBaseBonusPoints__6SongDBCFRC8UserGuid` |
| `0x82666f70` | `0x801dbf20` | bsim 14.3 | absent | BSIM | SongDB const::GetBaseMaxStreakPoints(...) | `GetBaseMaxStreakPoints__6SongDBCFRC8UserGuid` |
| `0x82666fa0` | `0x801dbef0` | bsim 14.3 | contradicted | BSIM | SongDB const::GetBaseMaxPoints(...) | `GetBaseMaxPoints__6SongDBCFRC8UserGuid` |
| `0x82666fd0` | `0x801dbf80` | bsim 11.6 | absent | BSIM | SongDB const::GetGemList(...) | `GetGemList__6SongDBCFi` |
| `0x82666fd8` | `0x801dbf90` | bsim 11.6 | contradicted | BSIM | SongDB const::GetGemListByDiff(...) | `GetGemListByDiff__6SongDBCFii` |
| `0x82667008` | `0x801dc040` | bsim 11.6 | absent | BSIM | SongDB const::GetDrumFillInfo(...) | `GetDrumFillInfo__6SongDBCFi` |
| `0x82667010` | `0x801dc050` | bsim 11.6 | absent | BSIM | SongDB const::GetVocalNoteList(...) | `GetVocalNoteList__6SongDBCFi` |
| `0x82667018` | `0x801dc060` | bsim 11.6 | absent | BSIM | SongDB const::GetVocalNoteListCount(...) | `GetVocalNoteListCount__6SongDBCFv` |
| `0x82667020` | `0x801dc3e0` | bsim 13.3 | absent | BSIM | SongDB const::IsUnisonPhrase(...) | `IsUnisonPhrase__6SongDBCFi` |
| `0x82667030` | `0x801dc3f0` | bsim 13.3 | absent | BSIM | SongDB const::GetNumOverdrivePhrases(...) | `GetNumOverdrivePhrases__6SongDBCFi` |
| `0x826670c8` | `0x801dc790` | bsim 13.3 | absent | BSIM | SongDB const::NumCommonPhrases(...) | `NumCommonPhrases__6SongDBCFv` |
| `0x82667218` | `0x801df7b0` | bsim 11.6 | contradicted | BSIM | SongDB::RecalculateGemTimes(...) | `RecalculateGemTimes__6SongDBFi` |
| `0x82667220` | `0x801df910` | bsim 11.6 | absent | BSIM | SongDB::EnableGems(...) | `EnableGems__6SongDBFiff` |
| `0x82667238` | `0x801df930` | bsim 11.6 | absent | BSIM | SongDB::ChangeDifficulty(...) | `ChangeDifficulty__6SongDBFi10Difficulty` |
| `0x826672d0` | `0x801df9d0` | bsim 13.0 | absent | BSIM | SongDB::SetTrainerGems(...) | `SetTrainerGems__6SongDBFii` |
| `0x826678a0` | `0x801dbc30` | bsim 14.0 | contradicted | BSIM | SongDB::RunMultiplayerAnalyzer(...) | `RunMultiplayerAnalyzer__6SongDBFv` |
| `0x82667e08` | `0x801dc070` | bsim 10.2 | absent | BSIM | SongDB::GetPhraseExtents(...) | `GetPhraseExtents__6SongDBF19BeatmatchPhraseTypeiiRiRi` |

### Game.o — 12 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 12, contra 0)  ·  `src/band3/game/Game.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82659f10` | `0x80180b60` | bsim 13.3 | absent | BSIM | Game::SetForegroundVolume(...) | `SetForegroundVolume__4GameFf` |
| `0x8265a380` | `0x80183200` | bsim 11.6 | absent | BSIM | Game::ForceTrackerStars(...) | `ForceTrackerStars__4GameFi` |
| `0x8265a388` | `0x80183210` | bsim 11.6 | absent | BSIM | Game::OnPlayerAddEnergy(...) | `OnPlayerAddEnergy__4GameFP6Playerf` |
| `0x8265a390` | `0x80183220` | bsim 11.6 | absent | BSIM | Game::OnPlayerSaved(...) | `OnPlayerSaved__4GameFP6Player` |
| `0x8265a398` | `0x80183240` | bsim 11.6 | absent | BSIM | Game::OnPlayerQuarantined(...) | `OnPlayerQuarantined__4GameFP6Player` |
| `0x8265a3a0` | `0x80183250` | bsim 11.6 | absent | BSIM | Game::OnRemoteTrackerFocus(...) | `OnRemoteTrackerFocus__4GameFP6Playeriii` |
| `0x8265a3a8` | `0x80183260` | bsim 11.6 | absent | BSIM | Game::OnRemoteTrackerPlayerProgress(...) | `OnRemoteTrackerPlayerProgress__4GameFP6Playerf` |
| `0x8265a3b8` | `0x80183280` | bsim 11.6 | absent | BSIM | Game::OnRemoteTrackerPlayerDisplay(...) | `OnRemoteTrackerPlayerDisplay__4GameFP6Playeriii` |
| `0x8265a3c0` | `0x80183290` | bsim 11.6 | absent | BSIM | Game::OnRemoteTrackerDeploy(...) | `OnRemoteTrackerDeploy__4GameFP6Player` |
| `0x8265a3c8` | `0x801832a0` | bsim 11.6 | absent | BSIM | Game::OnRemoteTrackerEndDeployStreak(...) | `OnRemoteTrackerEndDeployStreak__4GameFP6Playeri` |
| `0x8265cb90` | `0x801803d0` | bsim 12.6 | absent | BSIM | Game::CheckRollbackEnd(...) | `CheckRollbackEnd__4GameFf` |
| `0x8265df50` | `0x8017f3d0` | bsim 12.4 | absent | BSIM | Game::SetPaused(...) | `SetPaused__4GameFbbb` |

### GemManager.o — 11 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 11, contra 2)  ·  `src/band3/bandtrack/GemManager.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b66f08` | `0x801349f0` | bsim 11.6 | absent | BSIM | GemManager::FillHit(...) | `FillHit__10GemManagerFii` |
| `0x82b66f10` | `0x80134ad0` | bsim 11.6 | absent | BSIM | GemManager::SetSmasherGlowing(...) | `SetSmasherGlowing__10GemManagerFib` |
| `0x82b66f80` | `0x80135970` | bsim 10.9 | absent | BSIM | GemManager::IsSpotlightGem(...) | `IsSpotlightGem__10GemManagerFiRb` |
| `0x82b67068` | `0x80135ac0` | bsim 10.8 | contradicted | BSIM | GemManager::GetFill(...) | `GetFill__10GemManagerFiR10FillExtent` |
| `0x82b671f0` | `0x80136190` | bsim 11.6 | absent | BSIM | GemManager const::GetMaxSlots(...) | `GetMaxSlots__10GemManagerCFv` |
| `0x82b67d18` | `0x80134570` | bsim 10.1 | contradicted | BSIM | GemManager::ReleaseHitGems(...) | `ReleaseHitGems__10GemManagerFv` |
| `0x82b67db0` | `0x80134a00` | bsim 13.2 | absent | BSIM | GemManager::Released(...) | `Released__10GemManagerFfi` |
| `0x82b689b0` | `0x801355f0` | bsim 11.3 | absent | BSIM | GemManager::UpdateGemStates(...) | `UpdateGemStates__10GemManagerFv` |
| `0x82b68ad0` | `0x80132be0` | bsim 13.3 | absent | BSIM | GemManager::SetGemsEnabled(...) | `SetGemsEnabled__10GemManagerFf` |
| `0x82b68d30` | `0x80134e50` | bsim 11.8 | absent | BSIM | GemManager::SetBonusGems(...) | `SetBonusGems__10GemManagerFbRC11PlayerState` |
| `0x82b6b230` | `0x80133ae0` | bsim 14.3 | absent | BSIM | GemManager::AdvanceEnd(...) | `AdvanceEnd__10GemManagerFv` |

### ProfileMgr.o — 11 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 11, contra 4)  ·  `src/band3/meta_band/ProfileMgr.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82532340` | `0x8034c9d0` | bsim 10.2 | absent | BSIM | ProfileMgr const::GetSecondPedalHiHat(...) | `GetSecondPedalHiHat__10ProfileMgrCFv` |
| `0x82532348` | `0x8034c9e0` | bsim 13.1 | contradicted | BSIM | ProfileMgr::SetSecondPedalHiHat(...) | `SetSecondPedalHiHat__10ProfileMgrFb` |
| `0x82532798` | `0x80349170` | bsim 11.9 | absent | BSIM | ProfileMgr::Poll(...) | `Poll__10ProfileMgrFv` |
| `0x825327e8` | `0x80349780` | bsim 11.1 | contradicted | BSIM | ProfileMgr::GetProfileFromPad(...) | `GetProfileFromPad__10ProfileMgrFi` |
| `0x82532840` | `0x803498b0` | bsim 11.9 | contradicted | BSIM | ProfileMgr::GetProfileForUser(...) | `GetProfileForUser__10ProfileMgrFPC9LocalUser` |
| `0x82533390` | `0x8034d8b0` | bsim 12.8 | absent | BSIM | ProfileMgr::UpdateMultiMicDeviceSliders(...) | `UpdateMultiMicDeviceSliders__10ProfileMgrFP3Mici` |
| `0x82533ef8` | `0x8034c350` | bsim 14.0 | absent | BSIM | ProfileMgr::SetDolby(...) | `SetDolby__10ProfileMgrFb` |
| `0x82535a08` | `0x8034e4e0` | bsim 11.3 | contradicted | BSIM | ProfileMgr::HandleProfileSaveComplete(...) | `HandleProfileSaveComplete__10ProfileMgrFv` |
| `0x826d8d30` | `0x8034cd00` | bsim 12.4 | absent | BSIM | ProfileMgr::SetInGameSyncOffsetAdjustment(...) | `SetInGameSyncOffsetAdjustment__10ProfileMgrFf` |
| `0x82792d58` | `0x8034e4a0` | bsim 11.3 | absent | BSIM | ProfileMgr::HandleProfileLoadComplete(...) | `HandleProfileLoadComplete__10ProfileMgrFv` |
| `0x827d2c98` | `0x8034dd00` | bsim 10.0 | absent | BSIM | ProfileMgr const::GetCymbalConfiguration(...) | `GetCymbalConfiguration__10ProfileMgrCFv` |

### MusicLibrary.o — 8 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 8, contra 1)  ·  `src/band3/meta_band/MusicLibrary.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823d9cd0` | `0x802fc120` | bsim 10.0 | absent | BSIM | MusicLibrary::GetMaxSetlistSize(...) | `GetMaxSetlistSize__12MusicLibraryFv` |
| `0x825277d0` | `0x802fc0f0` | bsim 10.2 | absent | BSIM | MusicLibrary::GetDuplicatesAllowed(...) | `GetDuplicatesAllowed__12MusicLibraryFv` |
| `0x825277d8` | `0x802fc100` | bsim 10.9 | absent | BSIM | MusicLibrary::GetForcedSetlist(...) | `GetForcedSetlist__12MusicLibraryFv` |
| `0x82527c90` | `0x803023c0` | bsim 11.9 | absent | BSIM | MusicLibrary::NetSetlistsFailed(...) | `NetSetlistsFailed__12MusicLibraryFv` |
| `0x82527ca0` | `0x803023d0` | bsim 11.9 | absent | BSIM | MusicLibrary::NetSetlistsSucceeded(...) | `NetSetlistsSucceeded__12MusicLibraryFv` |
| `0x8252b4c0` | `0x802ff030` | bsim 10.8 | contradicted | BSIM | MusicLibrary::SetHighlightIx(...) | `SetHighlightIx__12MusicLibraryFib` |
| `0x8252c278` | `0x802fc900` | bsim 11.8 | absent | BSIM | MusicLibrary::SetTaskScoreType(...) | `SetTaskScoreType__12MusicLibraryF9ScoreType` |
| `0x82772398` | `0x802fc130` | bsim 13.0 | absent | BSIM | MusicLibrary::SetTask(...) | `SetTask__12MusicLibraryFRQ212MusicLibrary16MusicLibraryTask` |

### TrackConfig.o — 8 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 8, contra 1)  ·  `src/band3/bandtrack/TrackConfig.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8226fd70` | `0x80148350` | bsim 11.0 | absent | BSIM | TrackConfig const::Type(...) | `Type__11TrackConfigCFv` |
| `0x827aa970` | `0x80148610` | bsim 10.8 | absent | BSIM | TrackConfig::SetGameCymbalLanes(...) | `SetGameCymbalLanes__11TrackConfigFUi` |
| `0x82b781a0` | `0x801483a0` | bsim 13.9 | confirmed | BSIM | TrackConfig const::IsDrumTrack(...) | `IsDrumTrack__11TrackConfigCFv` |
| `0x82b781d0` | `0x801483d0` | bsim 13.9 | absent | BSIM | TrackConfig const::IsKeyboardTrack(...) | `IsKeyboardTrack__11TrackConfigCFv` |
| `0x82b78258` | `0x80148460` | bsim 13.9 | absent | BSIM | TrackConfig const::AllowsPartialHits(...) | `AllowsPartialHits__11TrackConfigCFv` |
| `0x82b782c8` | `0x801485f0` | bsim 10.8 | absent | BSIM | TrackConfig::SetMaxSlots(...) | `SetMaxSlots__11TrackConfigFi` |
| `0x82b782d8` | `0x80148620` | bsim 10.8 | contradicted | BSIM | TrackConfig::SetDisableHopos(...) | `SetDisableHopos__11TrackConfigFb` |
| `0x82b782e0` | `0x80148630` | bsim 10.8 | absent | BSIM | TrackConfig::SetTrackNum(...) | `SetTrackNum__11TrackConfigFi` |

### VocalPlayer.o — 7 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 7, contra 0)  ·  `src/band3/game/VocalPlayer.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82573000` | `0x80213270` | bsim 10.8 | absent | BSIM | VocalPlayer const::HadMic(...) | `HadMic__11VocalPlayerCFRC11MicClientID` |
| `0x826c5548` | `0x80214200` | bsim 11.4 | absent | BSIM | VocalPlayer::PressingToTalk(...) | `PressingToTalk__11VocalPlayerFv` |
| `0x826c58a8` | `0x80214b90` | bsim 12.3 | absent | BSIM | VocalPlayer const::CanDeployCoda(...) | `CanDeployCoda__11VocalPlayerCFv` |
| `0x826c5a68` | `0x80215c70` | bsim 10.6 | absent | BSIM | VocalPlayer::AddAccuracyStat(...) | `AddAccuracyStat__11VocalPlayerFi` |
| `0x826c5a88` | `0x80215cb0` | bsim 10.6 | absent | BSIM | VocalPlayer::AddTambourinePointsStat(...) | `AddTambourinePointsStat__11VocalPlayerFf` |
| `0x826c5a90` | `0x80215cc0` | bsim 10.6 | absent | BSIM | VocalPlayer::AddHarmonyStat(...) | `AddHarmonyStat__11VocalPlayerFi` |
| `0x826cf458` | `0x80213ce0` | bsim 11.6 | absent | BSIM | VocalPlayer::RebuildPhrases(...) | `RebuildPhrases__11VocalPlayerFv` |

### BandProfile.o — 8 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 8, contra 2)  ·  `src/band3/meta_band/BandProfile.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825728c8` | `0x802634f0` | bsim 11.6 | contradicted | BSIM | BandProfile::Poll(...) | `Poll__11BandProfileFv` |
| `0x825728d0` | `0x80264170` | bsim 11.5 | absent | BSIM | BandProfile::OwnsTourProgress(...) | `OwnsTourProgress__11BandProfileFPC12TourProgress` |
| `0x82572930` | `0x80264530` | bsim 13.1 | absent | BSIM | BandProfile::GetSongReview(...) | `GetSongReview__11BandProfileFi` |
| `0x82572958` | `0x80264570` | bsim 11.6 | absent | BSIM | BandProfile const::GetSongHighScore(...) | `GetSongHighScore__11BandProfileCFi9ScoreType` |
| `0x82572b78` | `0x80266a60` | bsim 10.0 | absent | BSIM | BandProfile::GetTourBand(...) | `GetTourBand__11BandProfileFv` |
| `0x82572cd8` | `0x80266490` | bsim 12.9 | contradicted | BSIM | BandProfile const::GetAssociatedLocalBandUser(...) | `GetAssociatedLocalBandUser__11BandProfileCFv` |
| `0x825becf0` | `0x80266610` | bsim 13.8 | absent | BSIM | BandProfile::SetProGuitarSongLessonSectionComplete(...) | `SetProGuitarSongLessonSectionComplete__11BandProfileFi10Difficultyi` |
| `0x827a8410` | `0x80264560` | bsim 10.0 | absent | BSIM | BandProfile const::GetSongStatusMgr(...) | `GetSongStatusMgr__11BandProfileCFv` |

### BandUserMgr.o — 7 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 7, contra 1)  ·  `src/band3/game/BandUserMgr.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82664870` | `0x80167930` | bsim 10.1 | absent | BSIM | BandUserMgr::GetBandUser(...) | `GetBandUser__11BandUserMgrFP4User` |
| `0x82665460` | `0x80168d60` | bsim 10.1 | absent | BSIM | BandUserMgr const::GetLocalBandUsersInSession(...) | `GetLocalBandUsersInSession__11BandUserMgrCFRQ211stlpmtx_std75vector<P13LocalBandUser,Us,Q211stlpmtx_std30StlNodeAlloc<P13LocalBandUser>>` |
| `0x82665478` | `0x80168d80` | bsim 10.1 | absent | BSIM | BandUserMgr const::GetLocalUsersNotInSessionWithAnyController(...) | `GetLocalUsersNotInSessionWithAnyController__11BandUserMgrCFRQ211stlpmtx_std75vector<P13LocalBandUser,Us,Q211stlpmtx_std30StlNodeAlloc<P13LocalBandUser>>` |
| `0x82665480` | `0x80168d70` | bsim 10.1 | absent | BSIM | BandUserMgr const::GetLocalUsersWithAnyController(...) | `GetLocalUsersWithAnyController__11BandUserMgrCFRQ211stlpmtx_std75vector<P13LocalBandUser,Us,Q211stlpmtx_std30StlNodeAlloc<P13LocalBandUser>>` |
| `0x82665d78` | `0x80168d10` | bsim 10.1 | contradicted | BSIM | BandUserMgr const::GetParticipatingBandUsers(...) | `GetParticipatingBandUsers__11BandUserMgrCFRQ211stlpmtx_std63vector<P8BandUser,Us,Q211stlpmtx_std24StlNodeAlloc<P8BandUser>>` |
| `0x82665d80` | `0x80168d20` | bsim 10.1 | absent | BSIM | BandUserMgr const::GetParticipatingBandUsersInSession(...) | `GetParticipatingBandUsersInSession__11BandUserMgrCFRQ211stlpmtx_std63vector<P8BandUser,Us,Q211stlpmtx_std24StlNodeAlloc<P8BandUser>>` |
| `0x82665d88` | `0x80168d30` | bsim 10.1 | absent | BSIM | BandUserMgr const::GetBandUsersInSession(...) | `GetBandUsersInSession__11BandUserMgrCFRQ211stlpmtx_std63vector<P8BandUser,Us,Q211stlpmtx_std24StlNodeAlloc<P8BandUser>>` |

### GemTrack.o — 7 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 7, contra 1)  ·  `src/band3/bandtrack/GemTrack.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8276bf28` | `0x8013e120` | bsim 11.9 | absent | BSIM | MakeString<f,f,i>(...)   [free function] | `MakeString<f,f,i>__FPCcffi_PCc` |
| `0x82b620f0` | `0x8013fb10` | bsim 11.6 | absent | BSIM | GemTrack::Miss(...) | `Miss__8GemTrackFfii` |
| `0x82b620f8` | `0x8013fb80` | bsim 11.6 | absent | BSIM | GemTrack::Ignore(...) | `Ignore__8GemTrackFi` |
| `0x82b62100` | `0x8013fb90` | bsim 11.6 | absent | BSIM | GemTrack::PartialHit(...) | `PartialHit__8GemTrackFfiUii` |
| `0x82b62110` | `0x8013fbb0` | bsim 11.6 | absent | BSIM | GemTrack::SetFretButtonPressed(...) | `SetFretButtonPressed__8GemTrackFib` |
| `0x82b62118` | `0x8013fbc0` | bsim 11.6 | contradicted | BSIM | GemTrack::ReleaseGem(...) | `ReleaseGem__8GemTrackFfi` |
| `0x82b623d8` | `0x8013cda0` | bsim 10.4 | absent | BSIM | GemTrack::ApplyShiftImmediately(...) | `ApplyShiftImmediately__8GemTrackFRCQ28GemTrack10RangeShift` |

### TourProgress.o — 7 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 7, contra 1)  ·  `src/band3/tour/TourProgress.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8234faa0` | `0x803d7850` | bsim 10.0 | contradicted | BSIM | TourProgress const::GetTourDesc(...) | `GetTourDesc__12TourProgressCFv` |
| `0x8234fd18` | `0x803d7150` | bsim 10.2 | absent | BSIM | TourProgress::HandleDirty(...) | `HandleDirty__12TourProgressFi` |
| `0x8234ff40` | `0x803d7ee0` | bsim 12.3 | absent | BSIM | TourProgress::SetNumCompletedGigs(...) | `SetNumCompletedGigs__12TourProgressFi` |
| `0x82350010` | `0x803d83a0` | bsim 12.2 | absent | BSIM | TourProgress::ClearNewStars(...) | `ClearNewStars__12TourProgressFv` |
| `0x82350088` | `0x803d8980` | bsim 12.3 | absent | BSIM | TourProgress::SetMetaScore(...) | `SetMetaScore__12TourProgressFi` |
| `0x82350140` | `0x803d7140` | bsim 10.1 | absent | BSIM | TourProgress::HandleTourRewardApplied(...) | `HandleTourRewardApplied__12TourProgressFv` |
| `0x823520f0` | `0x803d8990` | bsim 12.4 | absent | BSIM | TourProgress::SetToursPlayedMap(...) | `SetToursPlayedMap__12TourProgressFRCQ211stlpmtx_std110map<6Symbol,i,Q211stlpmtx_std13less<6Symbol>,Q211stlpmtx_std47StlNodeAlloc<Q211stlpmtx_std16pair<C6Symbol,i>>>` |

### Stats.o — 6 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 6, contra 0)  ·  `src/band3/game/Stats.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82679000` | `0x801e4c40` | bsim 13.8 | absent | BSIM | Stats::AddAccuracy(...) | `AddAccuracy__5StatsFi` |
| `0x82679020` | `0x801e4c60` | bsim 13.8 | absent | BSIM | Stats::AddSolo(...) | `AddSolo__5StatsFi` |
| `0x82679258` | `0x801e68c0` | bsim 14.5 | absent | BSIM | SingerStats const::GetPitchDeviationInfo(...) | `GetPitchDeviationInfo__11SingerStatsCFRfRf` |
| `0x826794f8` | `0x801e3200` | bsim 11.2 | absent | BSIM | Stats::BuildHitStreak(...) | `BuildHitStreak__5StatsFif` |
| `0x82679858` | `0x801e3290` | bsim 11.8 | absent | BSIM | Stats const::GetLongestStreak(...) | `GetLongestStreak__5StatsCFv` |
| `0x8267ab28` | `0x801e3320` | bsim 11.5 | absent | BSIM | Stats::EndMissStreak(...) | `EndMissStreak__5StatsFv` |

### Performer.o — 10 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 10, contra 5)  ·  `src/band3/game/Performer.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8267e5f8` | `0x801be1c0` | bsim 11.6 | contradicted | BSIM | Performer const::PollMs(...) | `PollMs__9PerformerCFv` |
| `0x8267e6a8` | `0x801be8f0` | bsim 10.1 | contradicted | BSIM | Performer const::GetSongNumVocalParts(...) | `GetSongNumVocalParts__9PerformerCFv` |
| `0x8267e6c0` | `0x801be910` | bsim 10.2 | absent | BSIM | Performer const::GetMultiplierActive(...) | `GetMultiplierActive__9PerformerCFv` |
| `0x8267e6c8` | `0x801be920` | bsim 11.6 | contradicted | BSIM | Performer::SetCrowdMeterActive(...) | `SetCrowdMeterActive__9PerformerFb` |
| `0x8267e6d0` | `0x801be930` | bsim 11.9 | absent | BSIM | Performer::GetCrowdMeterActive(...) | `GetCrowdMeterActive__9PerformerFv` |
| `0x8267e860` | `0x801bf8f0` | bsim 12.4 | absent | BSIM | Performer::TrulyWinGame(...) | `TrulyWinGame__9PerformerFv` |
| `0x8267e920` | `0x801bfb30` | bsim 11.6 | absent | BSIM | Performer::RemoteUpdateCrowd(...) | `RemoteUpdateCrowd__9PerformerFf` |
| `0x8267ecc8` | `0x801bf870` | bsim 13.9 | contradicted | BSIM | Performer::CheckGameWon(...) | `CheckGameWon__9PerformerFv` |
| `0x8267ef50` | `0x801be770` | bsim 15.0 | contradicted | BSIM | Performer::Poll(...) | `Poll__9PerformerFfRC7SongPos` |
| `0x82772478` | `0x801bcfb0` | bsim 12.8 | absent | BSIM | Stats::Stats(...) | `__ct__5StatsFRC5Stats` |

### GemSmasher.o — 6 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 6, contra 1)  ·  `src/band3/bandtrack/GemSmasher.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b7d820` | `0x8013b8b0` | bsim 11.8 | absent | BSIM | GemSmasher const::Showing(...) | `Showing__10GemSmasherCFv` |
| `0x82b7da08` | `0x8013bae0` | bsim 12.5 | absent | BSIM | GemSmasher::Burn(...) | `Burn__10GemSmasherFv` |
| `0x82b7dab0` | `0x8013bb90` | bsim 12.5 | absent | BSIM | GemSmasher::BurnChord(...) | `BurnChord__10GemSmasherFv` |
| `0x82b7db58` | `0x8013bc40` | bsim 12.6 | absent | BSIM | GemSmasher::CodaBurnChord(...) | `CodaBurnChord__10GemSmasherFv` |
| `0x82b7dc98` | `0x8013be30` | bsim 10.2 | absent | BSIM | GemSmasher const::Glowing(...) | `Glowing__10GemSmasherCFv` |
| `0x82b7dca0` | `0x8013ba20` | bsim 12.8 | contradicted | BSIM | GemSmasher::CodaHitChord(...) | `CodaHitChord__10GemSmasherFv` |

### BandPerformer.o — 5 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 5, contra 0)  ·  `src/band3/game/BandPerformer.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826cec68` | `0x80160bb0` | bsim 13.1 | absent | BSIM | BandPerformer const::GetNumStarsFloat(...) | `GetNumStarsFloat__13BandPerformerCFv` |
| `0x826ced78` | `0x80160ce0` | bsim 12.7 | absent | BSIM | BandPerformer const::PastFinalNote(...) | `PastFinalNote__13BandPerformerCFv` |
| `0x826cedf0` | `0x80160d70` | bsim 13.0 | absent | BSIM | BandPerformer::ComputePoints(...) | `ComputePoints__13BandPerformerFv` |
| `0x826cf1e8` | `0x80161620` | bsim 14.7 | absent | BSIM | BandPerformer const::GetExcitement(...) | `GetExcitement__13BandPerformerCFv` |
| `0x826cf490` | `0x80160ae0` | bsim 12.8 | absent | BSIM | BandPerformer const::GetScore(...) | `GetScore__13BandPerformerCFv` |

### VocalPart.o — 5 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 5, contra 0)  ·  `src/band3/game/VocalPart.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826d3868` | `0x80209390` | bsim 10.2 | absent | BSIM | VocalPart const::ScoringEnabled(...) | `ScoringEnabled__9VocalPartCFv` |
| `0x826d3928` | `0x8020bb10` | bsim 12.4 | absent | BSIM | VocalPart::SetFirstPhraseMsToScore(...) | `SetFirstPhraseMsToScore__9VocalPartFf` |
| `0x826d3d38` | `0x802091f0` | bsim 11.0 | absent | BSIM | VocalPart const::GetFirstPhraseMarker(...) | `GetFirstPhraseMarker__9VocalPartCFv` |
| `0x826d4398` | `0x8020a070` | bsim 14.3 | absent | BSIM | VocalPart const::GetNoteSliceWeight(...) | `GetNoteSliceWeight__9VocalPartCFffi` |
| `0x826d48f0` | `0x8020aa80` | bsim 10.0 | absent | BSIM | VocalPart const::CalculateScore(...) | `CalculateScore__9VocalPartCFfifR15VocalScoreCache` |

### TrainerPanel.o — 6 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 6, contra 2)  ·  `src/band3/game/TrainerPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823d7ad0` | `0x80202560` | bsim 11.0 | absent | BSIM | TrainerSection::SetName(...) | `SetName__14TrainerSectionFRC6Symbol` |
| `0x824590d8` | `0x801fffe0` | bsim 10.0 | absent | BSIM | TrainerPanel const::GetCurrSection(...) | `GetCurrSection__12TrainerPanelCFv` |
| `0x826aa548` | `0x801ff5c0` | bsim 10.7 | confirmed | BSIM | TrainerPanel::Exit(...) | `Exit__12TrainerPanelFv` |
| `0x826aa890` | `0x80200010` | bsim 10.1 | contradicted | BSIM | TrainerPanel const::GetTick(...) | `GetTick__12TrainerPanelCFv` |
| `0x826ab460` | `0x80200230` | bsim 10.2 | contradicted | BSIM | TrainerPanel::GetChallengeRestriction(...) | `GetChallengeRestriction__12TrainerPanelFi` |
| `0x826aceb8` | `0x801ffb70` | bsim 11.6 | absent | BSIM | TrainerPanel::RestartSection(...) | `RestartSection__12TrainerPanelFv` |

### GemPlayer.o — 5 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 5, contra 1)  ·  `src/band3/game/GemPlayer.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8269da70` | `0x80197330` | bsim 12.9 | contradicted | BSIM | GemPlayer const::GetBaseMaxPoints(...) | `GetBaseMaxPoints__9GemPlayerCFv` |
| `0x8269e2d0` | `0x8019a290` | bsim 12.5 | absent | BSIM | GemPlayer::OnGameOver(...) | `OnGameOver__9GemPlayerFv` |
| `0x8269f4d8` | `0x8019aac0` | bsim 10.9 | absent | BSIM | GemPlayer::InFillNow(...) | `InFillNow__9GemPlayerFv` |
| `0x826a2ea8` | `0x80195930` | bsim 11.6 | absent | BSIM | GemPlayer::ImplicitGem(...) | `ImplicitGem__9GemPlayerFifiRC8UserGuid` |
| `0x826a3d80` | `0x80196d60` | bsim 13.8 | absent | BSIM | GemPlayer::FillComplete(...) | `FillComplete__9GemPlayerFii` |

### Lyric.o — 5 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 5, contra 1)  ·  `src/band3/bandtrack/Lyric.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b7cab8` | `0x80142d10` | bsim 10.8 | absent | BSIM | Lyric::SetChunkEnd(...) | `SetChunkEnd__5LyricFb` |
| `0x82b7cac0` | `0x80142d20` | bsim 10.8 | absent | BSIM | Lyric::SetAfterDeploy(...) | `SetAfterDeploy__5LyricFi` |
| `0x82b7cac8` | `0x80142d30` | bsim 10.8 | contradicted | BSIM | Lyric::SetAfterMidPhraseLyricShift(...) | `SetAfterMidPhraseLyricShift__5LyricFb` |
| `0x82b7cad0` | `0x80142650` | bsim 12.5 | absent | BSIM | LyricPlate::EstimateLyricWidth(...) | `EstimateLyricWidth__10LyricPlateFPC5Lyric` |
| `0x82b7d068` | `0x80142cc0` | bsim 13.2 | absent | BSIM | Lyric const::StartTick(...) | `StartTick__5LyricCFv` |

### TrackerUtils.o — 5 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 5, contra 1)  ·  `src/band3/game/TrackerUtils.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826d7ad8` | `0x801f77c0` | bsim 10.6 | absent | BSIM | TrackerSectionManager const::GetGemIDsForRange(...) | `GetGemIDsForRange__21TrackerSectionManagerCFPC6PlayeriiRiRi` |
| `0x826d7df0` | `0x801f7380` | bsim 13.8 | contradicted | BSIM | TrackerSectionManager const::GetSectionStartTick(...) | `GetSectionStartTick__21TrackerSectionManagerCFi` |
| `0x826d7e68` | `0x801f7410` | bsim 13.4 | absent | BSIM | TrackerSectionManager const::TickInSection(...) | `TickInSection__21TrackerSectionManagerCFii` |
| `0x826d8558` | `0x801f7030` | bsim 14.1 | absent | BSIM | TrackerMultiplierMap const::GetMultiplier(...) | `GetMultiplier__20TrackerMultiplierMapCFf` |
| `0x826d8580` | `0x801f7060` | bsim 13.8 | absent | BSIM | TrackerMultiplierMap const::GetMultiplierIndex(...) | `GetMultiplierIndex__20TrackerMultiplierMapCFf` |

### CrowdRating.o — 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 0)  ·  `src/band3/game/CrowdRating.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826cfa18` | `0x801719c0` | bsim 13.0 | absent | BSIM | CrowdRating const::IsBelowLoseLevel(...) | `IsBelowLoseLevel__11CrowdRatingCFv` |
| `0x826cfac8` | `0x80171a60` | bsim 13.0 | absent | BSIM | CrowdRating const::CantFailYet(...) | `CantFailYet__11CrowdRatingCFv` |
| `0x826cfd88` | `0x801719e0` | bsim 10.7 | absent | BSIM | CrowdRating::SetValue(...) | `SetValue__11CrowdRatingFf` |
| `0x827c0af8` | `0x80171590` | bsim 10.8 | absent | BSIM | CrowdRating::SetActive(...) | `SetActive__11CrowdRatingFb` |

### TourSavable.o — 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 0)  ·  `src/band3/tour/TourSavable.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823573c0` | `0x803dacb0` | bsim 10.2 | absent | BSIM | TourSavable const::IsDirtyUpload(...) | `IsDirtyUpload__11TourSavableCFv` |
| `0x823573c8` | `0x803dacc0` | bsim 10.2 | absent | BSIM | TourSavable const::IsNameUnchecked(...) | `IsNameUnchecked__11TourSavableCFv` |
| `0x823574a8` | `0x803dadd0` | bsim 10.6 | absent | BSIM | TourSavable::UploadAttempted(...) | `UploadAttempted__11TourSavableFv` |
| `0x823574b0` | `0x803dade0` | bsim 14.8 | absent | BSIM | TourSavable::UploadComplete(...) | `UploadComplete__11TourSavableFv` |

### PracticePanel.o — 5 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 5, contra 2)  ·  `src/band3/game/PracticePanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825d2170` | `0x801c7f70` | bsim 13.0 | contradicted | BSIM | PracticePanel::Unload(...) | `Unload__13PracticePanelFv` |
| `0x82693740` | `0x801c7f30` | bsim 13.0 | absent | BSIM | PracticePanel::FinishLoad(...) | `FinishLoad__13PracticePanelFv` |
| `0x826937b0` | `0x801c8840` | bsim 10.7 | absent | BSIM | PracticePanel const::GetSectionBounds(...) | `GetSectionBounds__13PracticePanelCFRfRf` |
| `0x82693b40` | `0x801c9220` | bsim 14.4 | contradicted | BSIM | PracticePanel::TrackIn(...) | `TrackIn__13PracticePanelFv` |
| `0x82693bd8` | `0x801c92e0` | bsim 13.7 | absent | BSIM | PracticePanel::TrackOut(...) | `TrackOut__13PracticePanelFv` |

### BandUser.o — 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 1)  ·  `src/band3/game/BandUser.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8266d1e0` | `0x80162d40` | bsim 12.8 | absent | BSIM | BandUser const::ProfileName(...) | `ProfileName__8BandUserCFv` |
| `0x8266db58` | `0x801625c0` | bsim 12.3 | absent | BSIM | BandUser::SetOvershellSlotState(...) | `SetOvershellSlotState__8BandUserF20OvershellSlotStateID` |
| `0x82793650` | `0x80164080` | bsim 12.4 | absent | BSIM | DataNode::DataNode(...) | `__ct__8DataNodeF8DataTypei` |
| `0x827a9b78` | `0x80162760` | bsim 10.0 | contradicted | BSIM | BandUser const::GetControllerType(...) | `GetControllerType__8BandUserCFv` |

### InterstitialPanel.o — 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 1)  ·  `src/band3/meta_band/InterstitialPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826027c0` | `0x802c9b60` | bsim 14.0 | absent | BSIM | InterstitialPanel::Enter(...) | `Enter__17InterstitialPanelFv` |
| `0x82602850` | `0x802c9c00` | bsim 13.2 | absent | BSIM | InterstitialPanel::Unload(...) | `Unload__17InterstitialPanelFv` |
| `0x82602a38` | `0x802ca220` | bsim 12.1 | contradicted | BSIM | BackdropPanel::Enter(...) | `Enter__13BackdropPanelFv` |
| `0x826edad0` | `0x802c9d40` | bsim 10.8 | absent | BSIM | InterstitialPanel::SetCamshotDone(...) | `SetCamshotDone__17InterstitialPanelFv` |

### RockCentral.o — 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 1)  ·  `src/band3/net_band/RockCentral.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824e3ee8` | `0x803e0790` | bsim 10.1 | absent | BSIM | RockCentral::CancelOutstandingCalls(...) | `CancelOutstandingCalls__11RockCentralFPQ23Hmx6Object` |
| `0x824e3ef8` | `0x803e07a0` | bsim 10.1 | absent | BSIM | RockCentral::FailAllOutstandingCalls(...) | `FailAllOutstandingCalls__11RockCentralFv` |
| `0x824e54c0` | `0x803eb790` | bsim 14.3 | absent | BSIM | RockCentral::GetArtFile(...) | `GetArtFile__11RockCentralF6StringP6RndTexPUiPQ23Hmx6Objecti` |
| `0x8257cd58` | `0x803e7dc0` | bsim 11.6 | contradicted | BSIM | stlpmtx_std::map<6Symbol,i,Q211stlpmtx_std13less<6Symbol>,Q211stlpmtx_std47StlNodeAlloc<Q211stlpmtx_std16pair<C6Symbol,i>>>::begin(...) | `begin__Q211stlpmtx_std110map<6Symbol,i,Q211stlpmtx_std13less<6Symbol>,Q211stlpmtx_std47StlNodeAlloc<Q211stlpmtx_std16pair<C6Symbol,i>>>Fv` |

### Band.o — 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/band3/game/Band.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x822695b0` | `0x8015f600` | bsim 10.0 | absent | BSIM | Band const::EnergyMultiplier(...) | `EnergyMultiplier__4BandCFv` |
| `0x8267c0d0` | `0x8015f6c0` | bsim 12.9 | absent | BSIM | Band::AddUserDynamically(...) | `AddUserDynamically__4BandFP8BandUser` |
| `0x8267d208` | `0x8015f090` | bsim 11.0 | absent | BSIM | Band::DealWithCodaGem(...) | `DealWithCodaGem__4BandFP6Playeribb` |

### NetGameMsgs.o — 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/band3/game/NetGameMsgs.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823795c8` | `0x801b0910` | bsim 12.4 | absent | BSIM | MusicLibraryTaskMsg const::Save(...) | `Save__19MusicLibraryTaskMsgCFR9BinStream` |
| `0x82583d18` | `0x801b1430` | bsim 13.1 | absent | BSIM | TourPlayedMsg const::Name(...) | `Name__13TourPlayedMsgCFv` |
| `0x82592a88` | `0x801af520` | bsim 13.3 | absent | BSIM | SetUserTrackTypeMsg::Load(...) | `Load__19SetUserTrackTypeMsgFR9BinStream` |

### Player.o — 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/band3/game/Player.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82683f70` | `0x801c3910` | bsim 10.2 | absent | BSIM | Player const::IsDeployingBandEnergy(...) | `IsDeployingBandEnergy__6PlayerCFv` |
| `0x82684ab0` | `0x801c1ad0` | bsim 13.5 | absent | BSIM | Player::PollEnabledState(...) | `PollEnabledState__6PlayerFf` |
| `0x82686400` | `0x801c3e40` | bsim 11.4 | absent | BSIM | Player::DeployBandEnergyIfPossible(...) | `DeployBandEnergyIfPossible__6PlayerFb` |

### Scoring.o — 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/band3/game/Scoring.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82682938` | `0x801d53d0` | bsim 10.8 | absent | BSIM | Scoring const::GetBandNumStarsFloat(...) | `GetBandNumStarsFloat__7ScoringCFi` |
| `0x82682a58` | `0x801d5490` | bsim 11.6 | absent | BSIM | Scoring const::GetSoloNumStarsFloat(...) | `GetSoloNumStarsFloat__7ScoringCFi9TrackType` |
| `0x82682b20` | `0x801d5460` | bsim 12.7 | absent | BSIM | Scoring const::GetSoloNumStars(...) | `GetSoloNumStars__7ScoringCFi9TrackType` |

### SessionMgr.o — 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/band3/meta_band/SessionMgr.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8256e0a8` | `0x8035fc80` | bsim 12.7 | absent | BSIM | SessionMgr const::GetLocalHost(...) | `GetLocalHost__10SessionMgrCFv` |
| `0x8256e0f0` | `0x8035ff20` | bsim 11.6 | absent | BSIM | SessionMgr::SendMsgToAll(...) | `SendMsgToAll__10SessionMgrFR10NetMessage10PacketType` |
| `0x82570110` | `0x8035ecc0` | bsim 10.5 | absent | BSIM | SessionMgr::Init(...) | `Init__10SessionMgrFv` |

### TrackPanel.o — 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/band3/bandtrack/TrackPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b5e888` | `0x80148650` | bsim 10.9 | absent | BSIM | GetTrackPanelDir(...)   [free function] | `GetTrackPanelDir__Fv` |
| `0x82b5ff90` | `0x8014a030` | bsim 11.6 | absent | BSIM | TrackPanel::PostHandleAddPlayer(...) | `PostHandleAddPlayer__10TrackPanelFP6Player` |
| `0x82b606a8` | `0x80149100` | bsim 11.4 | absent | BSIM | TrackPanel::CreateTracks(...) | `CreateTracks__10TrackPanelFv` |

### VocalTrack.o — 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 0)  ·  `src/band3/bandtrack/VocalTrack.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b702e8` | `0x8014fa90` | bsim 12.6 | absent | BSIM | stlpmtx_std::deque<P13TambourineGem,Q211stlpmtx_std30StlNodeAlloc<P13TambourineGem>>::deque<P13TambourineGem,Q211stlpmtx_std30StlNodeAlloc<P13TambourineGem>>(...) | `__ct__Q211stlpmtx_std71deque<P13TambourineGem,Q211stlpmtx_std30StlNodeAlloc<P13TambourineGem>>FRCQ211stlpmtx_std30StlNodeAlloc<P13TambourineGem>` |
| `0x82b70348` | `0x8014ffb0` | bsim 12.6 | absent | BSIM | stlpmtx_std::deque<P9TubePlate,Q211stlpmtx_std25StlNodeAlloc<P9TubePlate>>::deque<P9TubePlate,Q211stlpmtx_std25StlNodeAlloc<P9TubePlate>>(...) | `__ct__Q211stlpmtx_std61deque<P9TubePlate,Q211stlpmtx_std25StlNodeAlloc<P9TubePlate>>FRCQ211stlpmtx_std25StlNodeAlloc<P9TubePlate>` |
| `0x82b727b8` | `0x80152c50` | bsim 14.4 | absent | BSIM | VocalTrack::RebuildHUD(...) | `RebuildHUD__10VocalTrackFv` |

### OvershellSlot.o — 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 2)  ·  `src/band3/meta_band/OvershellSlot.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825bed30` | `0x803246f0` | bsim 13.0 | contradicted | BSIM | OvershellSlot const::GetUser(...) | `GetUser__13OvershellSlotCFv` |
| `0x825beda0` | `0x80324e70` | bsim 12.4 | absent | BSIM | OvershellSlot::LeaveOptions(...) | `LeaveOptions__13OvershellSlotFv` |
| `0x825c4658` | `0x8032c3e0` | bsim 11.3 | absent | BSIM | OvershellSlot::Update(...) | `Update__13OvershellSlotFv` |
| `0x825c6a58` | `0x80326360` | bsim 14.4 | contradicted | BSIM | OvershellSlot::CancelSongSettings(...) | `CancelSongSettings__13OvershellSlotFv` |

### TrackerManager.o — 4 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 4, contra 2)  ·  `src/band3/game/TrackerManager.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826750d8` | `0x801f4630` | bsim 12.9 | absent | BSIM | TrackerManager const::GetQuestEarnedStars(...) | `GetQuestEarnedStars__14TrackerManagerCFv` |
| `0x82675ca0` | `0x801f5350` | bsim 14.9 | absent | BSIM | TrackerManager const::GetTrackerTypeFromGameType(...) | `GetTrackerTypeFromGameType__14TrackerManagerCF12TourGameType` |
| `0x82675d40` | `0x801f4cc0` | bsim 10.5 | contradicted | BSIM | TrackerManager const::CreateSource(...) | `CreateSource__14TrackerManagerCFRC11TrackerDesc` |
| `0x826761e8` | `0x801f4e00` | bsim 10.6 | contradicted | BSIM | TrackerManager::SetTracker(...) | `SetTracker__14TrackerManagerFRC11TrackerDesc` |

### Tour.o — 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 1)  ·  `src/band3/tour/Tour.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823491d0` | `0x803c18e0` | bsim 14.3 | absent | BSIM | Tour::ClearCurrentQuest(...) | `ClearCurrentQuest__4TourFv` |
| `0x82349230` | `0x803c2890` | bsim 10.4 | contradicted | BSIM | Tour::UpdateProgressWithCareerData(...) | `UpdateProgressWithCareerData__4TourFv` |
| `0x82663428` | `0x803bee00` | bsim 10.0 | absent | BSIM | Tour const::GetTourProgress(...) | `GetTourProgress__4TourCFv` |

### AppLabel.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/band3/meta_band/AppLabel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824ff190` | `0x802557d0` | bsim 10.6 | absent | BSIM | MakeString<PCc,i,i,i>(...)   [free function] | `MakeString<PCc,i,i,i>__FPCcPCciii_PCc` |
| `0x825ac480` | `0x80253690` | bsim 11.0 | absent | BSIM | AppLabel::SetUserName(...) | `SetUserName__8AppLabelFPC4User` |

### CharSync.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/band3/meta_band/CharSync.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82355b88` | `0x802a5f20` | bsim 11.6 | absent | BSIM | stlpmtx_std::set<P6WiiTex,Q211stlpmtx_std14less<P6WiiTex>,Q211stlpmtx_std22StlNodeAlloc<P6WiiTex>>::begin(...) | `begin__Q211stlpmtx_std85set<P6WiiTex,Q211stlpmtx_std14less<P6WiiTex>,Q211stlpmtx_std22StlNodeAlloc<P6WiiTex>>Fv` |
| `0x82b0d4a0` | `0x802a60b0` | bsim 11.0 | absent | BSIM | stlpmtx_std::list<P8CharData,Q211stlpmtx_std24StlNodeAlloc<P8CharData>> const::empty(...) | `empty__Q211stlpmtx_std58list<P8CharData,Q211stlpmtx_std24StlNodeAlloc<P8CharData>>CFv` |

### CommonPhraseCapturer.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/band3/game/CommonPhraseCapturer.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826cddd0` | `0x8016f5e0` | bsim 14.2 | absent | BSIM | CommonPhraseCapturer::Reset(...) | `Reset__20CommonPhraseCapturerFv` |
| `0x826cdee8` | `0x8016fb60` | bsim 11.4 | absent | BSIM | CommonPhraseCapturer const::DidTrackFail(...) | `DidTrackFail__20CommonPhraseCapturerCFii` |

### DirectInstrument.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/band3/game/DirectInstrument.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826c4910` | `0x80173ee0` | bsim 13.9 | absent | BSIM | DirectInstrument::NoteOn(...) | `NoteOn__16DirectInstrumentFi` |
| `0x826c4940` | `0x80173f10` | bsim 13.9 | absent | BSIM | DirectInstrument::PlayNote(...) | `PlayNote__16DirectInstrumentFii` |

### FadePanel.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/band3/game/FadePanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8268a0a8` | `0x80175540` | bsim 11.5 | absent | BSIM | FadePanel::StartFade(...) | `StartFade__9FadePanelFfRCQ23Hmx5Colorbb` |
| `0x8268a5e0` | `0x801757a0` | bsim 14.9 | absent | BSIM | FadePanel::Draw(...) | `Draw__9FadePanelFv` |

### GemTrainerPanel.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/band3/game/GemTrainerPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8268c258` | `0x801a4cc0` | bsim 14.6 | absent | BSIM | GemTrainerPanel const::ShouldMissCauseFail(...) | `ShouldMissCauseFail__15GemTrainerPanelCFv` |
| `0x8268e000` | `0x801a4b00` | bsim 14.1 | absent | BSIM | GemTrainerPanel::NewDifficulty(...) | `NewDifficulty__15GemTrainerPanelFii` |

### MetaPerformer.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/band3/meta_band/MetaPerformer.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824533e8` | `0x802ec320` | bsim 11.4 | absent | BSIM | QuickplayPerformerImpl::QuickplayPerformerImpl(...) | `__ct__22QuickplayPerformerImplFv` |
| `0x82564200` | `0x802eed30` | bsim 10.5 | absent | BSIM | MetaPerformer::UnlockBandOrSolo(...) | `UnlockBandOrSolo__13MetaPerformerFv` |

### RealGuitarGemPlayer.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/band3/game/RealGuitarGemPlayer.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826cd3f8` | `0x801cc2d0` | bsim 11.5 | confirmed | BSIM | RealGuitarGemPlayer const::GetTrackSlot(...) | `GetTrackSlot__19RealGuitarGemPlayerCFi` |
| `0x826cd440` | `0x801cc590` | bsim 10.1 | absent | BSIM | RealGuitarGemPlayer const::GetTrillSlots(...) | `GetTrillSlots__19RealGuitarGemPlayerCFiRQ211stlpmtx_std9pair<i,i>` |

### Singer.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/band3/game/Singer.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826d8d08` | `0x801d8bb0` | bsim 14.5 | absent | BSIM | Singer const::GetPitchDeviation(...) | `GetPitchDeviation__6SingerCFRfRf` |
| `0x826d94a8` | `0x801d8260` | bsim 11.0 | absent | BSIM | Poll___6SingerFfRC7SongPosffff | `Poll___6SingerFfRC7SongPosffff` |

### TambourineManager.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 0)  ·  `src/band3/game/TambourineManager.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826dbbf8` | `0x801ec370` | bsim 10.5 | absent | BSIM | TambourineManager const::GemHit(...) | `GemHit__17TambourineManagerCFi` |
| `0x826dbc30` | `0x801ec3a0` | bsim 11.9 | absent | BSIM | TambourineManager const::GemProcessed(...) | `GemProcessed__17TambourineManagerCFi` |

### Tracker.o — 3 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 3, contra 2)  ·  `src/band3/game/Tracker.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826b1590` | `0x801ee740` | bsim 11.1 | contradicted | BSIM | Tracker::HandlePlayerSaved(...) | `HandlePlayerSaved__7TrackerFP6Player` |
| `0x826b1ac0` | `0x801eeeb0` | bsim 11.2 | contradicted | BSIM | Tracker const::GetTargetSuccessLevel(...) | `GetTargetSuccessLevel__7TrackerCFv` |
| `0x826b2f78` | `0x801ee230` | bsim 13.7 | absent | BSIM | Tracker::Restart(...) | `Restart__7TrackerFv` |

### MultiplayerAnalyzer.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/band3/game/MultiplayerAnalyzer.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826ae400` | `0x801ac8d0` | bsim 12.2 | absent | BSIM | MultiplayerAnalyzer::GetCodaExtents(...) | `GetCodaExtents__19MultiplayerAnalyzerFRC8UserGuidRiRi` |
| `0x826ae928` | `0x801acd10` | bsim 11.5 | contradicted | BSIM | MultiplayerAnalyzer::AddCodas(...) | `AddCodas__19MultiplayerAnalyzerFv` |

### OvershellPanel.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 1)  ·  `src/band3/meta_band/OvershellPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8259a210` | `0x8031be90` | bsim 14.9 | absent | BSIM | OvershellPanel const::IsNonVocalistInVocalsSlot(...) | `IsNonVocalistInVocalsSlot__14OvershellPanelCFv` |
| `0x8259ba08` | `0x8031fb30` | bsim 12.7 | contradicted | BSIM | OvershellPanel::CheckForControllerDisconnects(...) | `CheckForControllerDisconnects__14OvershellPanelFv` |

### AccomplishmentSongConditional.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/AccomplishmentSongConditional.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8264c940` | `0x8024ca20` | bsim 11.4 | absent | BSIM | AccomplishmentSongConditional::AccomplishmentSongConditional(...) | `__ct__29AccomplishmentSongConditionalFP9DataArrayi` |

### BandMachine.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/BandMachine.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825a8ee0` | `0x802d5d00` | bsim 13.6 | absent | BSIM | LocalBandMachine::LocalBandMachine(...) | `__ct__16LocalBandMachineFP14BandMachineMgr` |

### BandSongMgr.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/BandSongMgr.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82ab67b0` | `0x80273e50` | bsim 10.6 | absent | BSIM | __rs<i,Q211stlpmtx_std15StlNodeAlloc<i>>__FR9BinStreamRQ211stlpmtx_std40list<i,Q211stlpmtx_std15StlNodeAlloc<i>>_R9BinStream | `__rs<i,Q211stlpmtx_std15StlNodeAlloc<i>>__FR9BinStreamRQ211stlpmtx_std40list<i,Q211stlpmtx_std15StlNodeAlloc<i>>_R9BinStream` |

### CampaignLevel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/CampaignLevel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825d43b0` | `0x80296190` | bsim 10.4 | absent | BSIM | CampaignLevel const::GetIconArt(...) | `GetIconArt__13CampaignLevelCFv` |

### CharCache.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/CharCache.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82554468` | `0x802a2e40` | bsim 10.4 | absent | BSIM | CharCache::Lock(...) | `Lock__9CharCacheFbb` |

### CharacterCreatorPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/CharacterCreatorPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825f0a78` | `0x8029d6c0` | bsim 10.2 | absent | BSIM | CharacterCreatorPanel::CreateNewCharacter(...) | `CreateNewCharacter__21CharacterCreatorPanelFv` |

### ContentDeletePanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/ContentDeletePanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824589c0` | `0x802af6e0` | bsim 13.1 | absent | BSIM | ContentDeletePanel::SetupDeletion(...) | `SetupDeletion__18ContentDeletePanelFib` |

### CustomizePanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/CustomizePanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825f9f48` | `0x802b8fa0` | bsim 12.2 | absent | BSIM | CustomizePanel::LeaveCustomizePanel(...) | `LeaveCustomizePanel__14CustomizePanelFv` |

### Defines.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/game/Defines.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82a27198` | `0x80171c00` | bsim 11.4 | absent | BSIM | TrackTypeToControllerType(...)   [free function] | `TrackTypeToControllerType__F9TrackType` |

### FixedSetlist.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/tour/FixedSetlist.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82352f78` | `0x803b6630` | bsim 10.1 | absent | BSIM | FixedSetlist::FixedSetlist(...) | `__ct__12FixedSetlistFv` |

### FocusTracker.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/game/FocusTracker.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826b7050` | `0x80178010` | bsim 11.1 | absent | BSIM | StreakFocusTracker const::PlayerWantsFocus(...) | `PlayerWantsFocus__18StreakFocusTrackerCFRC15TrackerPlayerIDf` |

### FreestylePanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/game/FreestylePanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8269c0b8` | `0x80179cc0` | bsim 14.8 | absent | BSIM | FreestylePanel::Exit(...) | `Exit__14FreestylePanelFv` |

### GameConfig.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/game/GameConfig.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8266ad90` | `0x80186fd0` | bsim 14.5 | absent | BSIM | GameConfig const::GetPracticeSections(...) | `GetPracticeSections__10GameConfigCFRiRi` |

### GamePanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/game/GamePanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82676cc0` | `0x8018d7e0` | bsim 14.3 | absent | BSIM | GamePanel::PollForLoading(...) | `PollForLoading__9GamePanelFv` |

### Gem.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/bandtrack/Gem.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b79f60` | `0x8012b140` | bsim 11.1 | absent | BSIM | Gem::Gem(...) | `__ct__3GemFRC7GameGemUiffbiib` |

### HeaderPerformanceProvider.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/HeaderPerformanceProvider.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825b1630` | `0x802c1c50` | bsim 14.6 | absent | BSIM | SetlistScoresProvider::RefreshScores(...) | `RefreshScores__21SetlistScoresProviderFv` |

### HeldNote.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/game/HeldNote.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826d5ef8` | `0x801a9470` | bsim 13.1 | absent | BSIM | HeldNote::ReleaseSlot(...) | `ReleaseSlot__8HeldNoteFi` |

### LockStepMgr.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/LockStepMgr.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82592910` | `0x802d4180` | bsim 13.3 | absent | BSIM | StartLockMsg::Load(...) | `Load__12StartLockMsgFR9BinStream` |

### MultiSelectListPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/MultiSelectListPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8260a688` | `0x802f8f40` | bsim 13.2 | absent | BSIM | MultiSelectListPanel::UnChoose(...) | `UnChoose__20MultiSelectListPanelFv` |

### PatchSelectPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/PatchSelectPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82561458` | `0x8033ecb0` | bsim 11.6 | absent | BSIM | PatchSelectPanel::Unload(...) | `Unload__16PatchSelectPanelFv` |

### PlayerBehavior.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/game/PlayerBehavior.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826d0170` | `0x801c73f0` | bsim 10.1 | absent | BSIM | PlayerBehavior::SetCanDeployOverdrive(...) | `SetCanDeployOverdrive__14PlayerBehaviorFb` |

### RGTrainerPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/game/RGTrainerPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8268f758` | `0x801d0b60` | bsim 10.2 | absent | BSIM | RGTrainerPanel const::GetLegendMode(...) | `GetLegendMode__14RGTrainerPanelCFv` |

### SaveLoadManager.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/SaveLoadManager.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8253eee8` | `0x80354bd0` | bsim 10.9 | absent | BSIM | SaveLoadManager::Start(...) | `Start__15SaveLoadManagerFv` |

### SavedSetlist.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/SavedSetlist.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827b6d30` | `0x80352b10` | bsim 10.0 | absent | BSIM | NetSavedSetlist const::GetArtUrl(...) | `GetArtUrl__15NetSavedSetlistCFv` |

### ScoreTracker.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/game/ScoreTracker.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826c1038` | `0x801d3250` | bsim 11.1 | absent | BSIM | FirstFrame___12ScoreTrackerFf | `FirstFrame___12ScoreTrackerFf` |

### ScoreUtl.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/game/ScoreUtl.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826d5d18` | `0x801d3590` | bsim 14.6 | absent | BSIM | GetStarsForScore(...)   [free function] | `GetStarsForScore__FiRC8UserGuid` |

### SelectDifficultyPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/SelectDifficultyPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826168d8` | `0x8035e550` | bsim 10.7 | absent | BSIM | SelectDifficultyPanel::ContentMounted(...) | `ContentMounted__21SelectDifficultyPanelFPCcPCc` |

### SongSortNode.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/SongSortNode.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82643968` | `0x8037f000` | bsim 12.5 | absent | BSIM | HeaderSortNode const::IsActive(...) | `IsActive__14HeaderSortNodeCFv` |

### SongStatusMgr.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/SongStatusMgr.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82682030` | `0x80384390` | bsim 14.2 | absent | BSIM | SongStatusMgr const::GetCachedTotalStars(...) | `GetCachedTotalStars__13SongStatusMgrCF9ScoreType` |

### StatMemberTracker.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/game/StatMemberTracker.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826b1138` | `0x801e28e0` | bsim 14.3 | absent | BSIM | FirstFrame___23UnisonStatMemberTrackerFf | `FirstFrame___23UnisonStatMemberTrackerFf` |

### StoreOfferProvider.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/StoreOfferProvider.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82646b28` | `0x80390c70` | bsim 10.8 | absent | BSIM | StoreOfferProvider::PosToNextGroupPos(...) | `PosToNextGroupPos__18StoreOfferProviderFi` |

### Tail.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/bandtrack/Tail.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b7e4f8` | `0x80144c80` | bsim 10.1 | absent | BSIM | Tail::ConfigureMeshes(...) | `ConfigureMeshes__4TailFP4Tail` |

### TourCharLocal.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/tour/TourCharLocal.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b4af08` | `0x803c78b0` | bsim 10.6 | absent | BSIM | TourCharLocal::GenerateGUID(...) | `GenerateGUID__13TourCharLocalFv` |

### TourDesc.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/tour/TourDesc.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82355cf0` | `0x803cab10` | bsim 11.6 | absent | BSIM | TourDesc const::GetArt(...) | `GetArt__8TourDescCFv` |

### Track.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/bandtrack/Track.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b658c0` | `0x80146610` | bsim 10.2 | absent | BSIM | NewTrack(...)   [free function] | `NewTrack__FP8BandUser` |

### TrainingMgr.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/TrainingMgr.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8228b5b8` | `0x80398cc0` | bsim 10.5 | absent | BSIM | TrainingMgr::ClearCurrentLesson(...) | `ClearCurrentLesson__11TrainingMgrFv` |

### TrainingPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/TrainingPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82621a00` | `0x8039a510` | bsim 11.5 | absent | BSIM | TrainingPanel::Exit(...) | `Exit__13TrainingPanelFv` |

### UIStats.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/UIStats.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b88290` | `0x8039fae0` | bsim 10.7 | absent | BSIM | UIStats::Terminate(...) | `Terminate__7UIStatsFv` |

### UploadErrorMgr.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/meta_band/UploadErrorMgr.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x823c7308` | `0x803a1a40` | bsim 13.3 | absent | BSIM | UploadErrorMgr::UploadErrorMgr(...) | `__ct__14UploadErrorMgrFv` |

### VocalGuidePitch.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 0)  ·  `src/band3/game/VocalGuidePitch.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826d30f0` | `0x802075c0` | bsim 14.6 | absent | BSIM | VocalGuidePitch::Terminate(...) | `Terminate__15VocalGuidePitchFv` |

### GraphicsUtl.o — 2 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 2, contra 2)  ·  `src/band3/bandtrack/GraphicsUtl.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82b7f368` | `0x80141a10` | bsim 10.6 | contradicted | BSIM | UnhookGroupParents(...)   [free function] | `UnhookGroupParents__FPQ23Hmx6Object` |
| `0x82b7f3b0` | `0x80141a60` | bsim 11.1 | contradicted | BSIM | UnhookAllParents(...)   [free function] | `UnhookAllParents__FPQ23Hmx6Object` |

### AccomplishmentProgress.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/meta_band/AccomplishmentProgress.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82643960` | `0x802438b0` | bsim 10.2 | contradicted | BSIM | AccomplishmentProgress::IsHardCoreStatusUpdatePending(...) | `IsHardCoreStatusUpdatePending__22AccomplishmentProgressFv` |

### BandPreloadPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/meta_band/BandPreloadPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825e81e8` | `0x80262120` | bsim 10.2 | contradicted | BSIM | BandPreloadPanel::Load(...) | `Load__16BandPreloadPanelFv` |

### BandSongMetadata.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/meta_band/BandSongMetadata.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825864e8` | `0x8026b980` | bsim 11.6 | contradicted | BSIM | BandSongMetadata const::ScrollSpeed(...) | `ScrollSpeed__16BandSongMetadataCFv` |

### CalibrationPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/meta_band/CalibrationPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825ed500` | `0x8028e0d0` | bsim 13.6 | contradicted | BSIM | CalibrationPanel::UpdateLabel(...) | `UpdateLabel__16CalibrationPanelFv` |

### ClosetMgr.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/meta_band/ClosetMgr.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82550078` | `0x802abd10` | bsim 14.7 | contradicted | BSIM | ClosetMgr::FinalizeCharCreatorChanges(...) | `FinalizeCharCreatorChanges__9ClosetMgrFv` |

### DataResults.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/net_band/DataResults.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x824f7ec0` | `0x803dcbe0` | bsim 13.8 | contradicted | BSIM | DataResultList::Clear(...) | `Clear__14DataResultListFv` |

### MainHubPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/meta_band/MainHubPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82605de0` | `0x802dccc0` | bsim 11.2 | contradicted | BSIM | MainHubPanel::SetMainHubOverride(...) | `SetMainHubOverride__12MainHubPanelFQ212MainHubPanel15MainHubOverride` |

### OverdriveTimeTracker.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/game/OverdriveTimeTracker.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826c3260` | `0x801b4850` | bsim 10.8 | contradicted | BSIM | OverdriveTimeTracker::UpdateTimeRemainingDisplay(...) | `UpdateTimeRemainingDisplay__20OverdriveTimeTrackerFv` |

### PatchPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/meta_band/PatchPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82c18ed8` | `0x8033b590` | bsim 13.0 | contradicted | BSIM | PatchPanel::EditLayer(...) | `EditLayer__10PatchPanelFv` |

### SetlistToStorePanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/meta_band/SetlistToStorePanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x82626138` | `0x80367750` | bsim 12.1 | contradicted | BSIM | SetlistToStorePanel::Enter(...) | `Enter__19SetlistToStorePanelFv` |

### SongSetlistProvider.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/meta_band/SongSetlistProvider.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825a3cf0` | `0x8036efa0` | bsim 13.0 | contradicted | BSIM | SetlistProvider const::NumData(...) | `NumData__15SetlistProviderCFv` |

### SyncGameStartPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/game/SyncGameStartPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x8268aed8` | `0x801e9da0` | bsim 10.6 | contradicted | BSIM | SyncGameStartPanel::PollIsSynced(...) | `PollIsSynced__18SyncGameStartPanelFv` |

### TexLoadPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/meta_band/TexLoadPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x825d7500` | `0x80394320` | bsim 10.2 | contradicted | BSIM | TexLoadPanel::Poll(...) | `Poll__12TexLoadPanelFv` |

### VocalScoreHistory.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/game/VocalScoreHistory.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x827aa988` | `0x80218620` | bsim 10.8 | contradicted | BSIM | VocalScoreHistory::SetOctaveOffset(...) | `SetOctaveOffset__17VocalScoreHistoryFi` |

### VocalTrainerPanel.o — 1 ids (high 0, ≥30 0, 20-30 0, 15-20 0, 10-15 1, contra 1)  ·  `src/band3/game/VocalTrainerPanel.cpp`

| Xenon addr | Bank-8 | confidence | rb3wii | match | Wii signature | wii_symbol |
|---|---|---|---|---|---|---|
| `0x826672e0` | `0x80219ff0` | bsim 14.2 | contradicted | BSIM | VocalPhrase::VocalPhrase(...) | `__ct__11VocalPhraseFRC11VocalPhrase` |

---

## For the next agent

- **Regenerate:** `python3 tools/gen_band3_port_worklist.py` (cwd-independent; reads `ghidriff_identities.json` + `scripts/target_symbol_map.json` + the rb3 CW map; the script VERIFIES every `wii_symbol` resolves to its claimed Bank-8 addr and that 0 entries are already in the production map, exiting non-zero on any failure).
- **Data feed:** `band3_port_worklist.json` — per-fn rows + `tu_summary` + `ranked_tus` for machine ingestion by the `wf_classa_harvest.js` Scan/Validate stages.
- **Do NOT inject these into `target_symbol_map.json`** — CW≠MSVC mangling, TUs uncompiled; wrong key mis-pairs objdiff at ~0.90 precision. Confirm each name when the TU is actually ported.
- **Validation gap still open:** the ~530 net-new system/network identities are unjudged at human grade (round-2 judging was band3-only).
