# W16-ID — game-layer fuzzy sweep, 90–99.99% band (2026-10-01)

**Branch** `w16-id`, rebased onto main `14d2b1881`. **Ruler** `name_check` (graded, read from
`report.json` `provenance.diff_config`).

**Population:** every *named* row in a `src/band3` / `src/network` unit with `90 ≤ fuzzy_match_percent < 100`,
excluding Quazal. Rows are ranked by `size × (100 − fuzzy)`.

Before this lane, on the A/B's leg A at main `14d2b1881`:

| | rows | bytes | weight |
|---|---:|---:|---:|
| named (the population) | 584 | 231,088 | 461,860 |
| anonymous `fn_` (EH funclets and thunks, not worked directly) | 338 | 14,064 | |

The two prior game-layer sweeps (W16-HE, W16-HX) covered the 1–90 band. This band had not been swept.

**After (A/B leg B):** **491 rows, 198,836 B, weight 226,008**, so the weight fell by 51%. Of the 584 rows in the band, 91 reached
fuzzy 100 (32,280 B) and 135 rose.

## 1. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-id-ab --patch <main..w16-id>` was run once.
- **Worktree:** fresh, at main `14d2b1881`.
- **Diff:** the whole branch, 79 files, with no docs and no `symbols.txt`.
- **objdiff-cli:** pinned to one binary across both legs.
- **Leg A:** settled after 2 iterations; the first iteration forced the re-split.
- **Leg B:** 464 recompiles, one split, renamer patched 1,853 objects, 2 settle iterations.

```
leg A: matched=47269 masked=23816 honest=23453 code%=46.811344  (recompiles: 0, settled)
leg B: matched=47391 masked=23822 honest=23569 code%=47.137640  (recompiles: 464, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+122  Δmasked_equal=+6  Δhonest=+116  Δcode%=+0.326296pp  Δcode_bytes=+33436
Δfuzzy=+0.023296pp   (legA 56.670788 -> legB 56.694084)
units at 100% [mpn ruler]: legA 267 -> legB 270  (Δ+3; 3 reached 100, 0 fell off)
units at 100% [all-rows-fuzzy ruler]: legA 230 -> legB 232  (Δ+2; 2 reached 100, 0 fell off)
[control none] Δmatched_code=+36112 B (default ruler +33436 B) -- NOT_APPLICABLE (source in patch)
```

The units that reached 100 are ChordbookPanel, `band3/bandtrack/Track` and `band3/meta_band/BandStorePanel`.

**Prediction, written before the run:** about +115 to +125 functions and +32 to +34 KB. This was based on the pre-rebase in-tree build (+125 functions, +34,012 B against `add52fdb6`), less the TambourineManager re-home that main had since landed on its own. The measured +122 functions and +33,436 B are inside that range.

**Row-level diff of the archived leg reports.** Run dir:
`~/tmp/wt-w16-id-ab/.ab_measure_runs/20261001-040543-w16-id-branch-1449467/`. Result: **150 rows up, 0 down**, plus 20 GONE/NEW pairs from map renames and re-homes.
- Two leg-A rows at 100 left under their old names, and both reappear at 100 under the corrected names:
  - `GetBestSongStatusFlag`, now `QBA_N…` because it returns `bool`.
  - `SelectSetting`, now `QAA_NH` because it returns `bool`.
- **No row falls off 100.**
- Two renamed rows read lower, both on retail evidence:
  - `clear<SetlistArtRecord>` 99.95 → `fn_827E1708` 0. The old score was a false pairing; see §3.5.
  - `??0TrainerGemTab` reads 8.3. It is a dtk mis-carve: the ctor continues as `fn_826EFC98`.

## 2. Method

1. **Eight forks**, each with its own worktree and branch (`w16-id-1` … `w16-id-8`). Each took a disjoint unit set:

   | fork | units |
   |---|---|
   | 1 | VocalTrack |
   | 2 | VocalPlayer, VocalPart, VocalTrainerPanel, Singer, TambourineManager |
   | 3 | Gem*, Track*, ChordbookPanel, GemTrainerPanel |
   | 4 | meta / accomplishment / campaign / tour |
   | 5 | store / library / save |
   | 6 | game / trackers |
   | 7 | network, song managers, BandProfile |
   | 8 | the 93-unit long tail |

2. **Per-fix check.** Every fix was confirmed with a full `./tools/ninja-locked` build and a whole-report row diff against that fork's base, never with `run_objdiff` alone.
3. **Merging.** Each fork landed on `w16-id` with `git merge --no-ff`. The lane then made three edits of its own (§3).
4. **Rebase.** The lane was rebased onto main with `--rebase-merges`; §5 lists the conflicts and how each was resolved.

Rows whose only residue was register allocation or scheduling were skipped after about three tries. The permuter was not run.

## 3. Defects fixed

### 3.1 Behaviour fixes (retail does something our source did not)

- **`AccomplishmentTourConditional::Configure`** wrote `Node(1).Int()` to `mGameType`. Retail writes it to `mValue` (cond+0x4), so every tour condition had a target of 0.
- **`VocalTrack::PrepareNoteTubes`**
  - Hidden-part dimming is gated by vtable slot 7, which is `Player::IsNet()`, not `GetEnabledStateAt(1.0f)`.
  - The tube point is clamped to `std::max(prevX + 0.01, x)`.
- **`VocalTrack::UpdateTubePlates`**
  - The plate loop stops at the first plate with no verts.
  - Deploy polling runs in static mode. Retail compares the style with 0, and `OnSetDisplayMode` @`0x82BA0C80` stores 0 for static.
- **`SaveLoadManager::SetState`**: cases 0x36, 0x37 and 0x40 set the global-options state to `kMetaProfileError`; the old source set `kMetaProfileLoaded`.
- **`RefreshArchivedBattles`** requests `battles/closed/get` (retail `0x82504A50`); the old source requested the song-lists endpoint.
- **`MusicLibrary::OnMsg(RemoteMachineLeftMsg)`** calls `RebuildRestrictedData`. Retail `0x8253DF28` begins with `SyncSharedSongs`.
- **`StickerProvider::SetStickers`** gives sticker materials `SetTexGen(kTexGenXfm)`; the old source used `SetBlend(kBlendAdd)`.
- **`BandStorePanel::GetIndexFile`** builds `"/dlc_top_%s_%s.dta"` from the platform and the system language.
- **`GemTrainerPanel::Enter`** sends `set_key` for every key, not only minor keys.
- **`SongDB::ClearTrackPhrases`** clears only quarantined phrases. Arpeggio and chord-markup extents move to a separate function, retail `0x82686CB0`, which our source calls `ClearArpeggioPhrases`. That name is descriptive and is not in the map. `DisableOverdrivePhrases` changes behaviour as a result.
- **`Game::AddPlayer`** toggles pause only when the game is not paused.
- **`MultiplayerAnalyzer::AddTrack`** caps the multiplier at 6 only for the three bass track types.
- **`BandSongMgr::ContentDone`** autosaves whenever `TheSaveLoadMgr` exists. Retail `0x82579F50` has no cache-write test.
- **`SongStatusMgr`** had two functions under swapped names:
  - `0x825D20B0` is `CalculateTotalStars(ScoreType)`; both retail callers pass one argument.
  - `0x825D1FC8` is `GetTotalBestStars(ScoreType, Difficulty, Symbol)`. It sums each song's best stars clamped to 5, up to 15,000.
  - The invented 5,000-cap body is gone.
- **`MetaPanel::Init`** registers retail's set of panels, including `AuditionSessionPanel` (RTTI only, so it gets a declaration-only header). It makes no `OvershellPanel::Init` call and registers no cheat toggles.
- **`MetaPanel` and `Campaign` layout:**
  - `MetaMusicManager` sits at +0x4c.
  - The match build constructs no `HAQManager`; it stays under `HX_NATIVE`.
  - `Campaign` is 0xa8 bytes.
- **Return types.** These functions return values our source dropped or mistyped:
  - `TambourineGemPool::NewGem` returns `TambourineGem*`; its epilogue is `mr r3,r30`.
  - `SongStatusMgr::GetBestSongStatusFlag` returns `bool`.
  - `ViewSettingsProvider::SelectSetting` returns `bool`; every retail caller masks the result to a byte.

### 3.2 Codegen-shape fixes. These are the most common classes, and each was confirmed on several rows.

- **Named local versus temporary.** When retail passes a stack-slot address (`addi rX,r31,slot`) where we pass a call or ctor return (`mr rX,r3`), the source used a named local, or an implicit conversion temporary for `FilePath`. This fixed about 12 rows across forks 4 and 8.
- **`find(...) != end()` held as a `bool` local** before branching. This fixed three NetSession rows.
- **Unsigned `size()` loops.** Retail shows `cmplw` plus an empty-container `beq`. This fixed BandMachineMgr and three tracker rows.
- **`static const float` for a constant reloaded every iteration.** Retail keeps the constant's address in a callee-saved GPR and reloads the float inside the loop; a bare literal gets hoisted into an FPR instead. Seen in `Singer::ResolveAmbiguity` and `VocalPart::GetNoteSliceWeight`.
- **Implicit special members:**
  - A literal 0 stored to the vtordisp slot means no user-declared ctor (`UIEventMgr`; confirms W16-HE §4.2).
  - `VocalPhrase` is copied by a 0x38-byte `memcpy`, so it has no user-declared copy ctor.
  - `LinearInterpolator` has no user-declared dtor. This lets `~Tail` and `~Game` reach 100.
- **`(float)std::floor(x)`.** A `frsp` between the floor call and `fctiwz` means the source had a `(float)` cast.
- **`std::max` / `std::min` by const reference** select between two address-taken temporaries. Milo's by-value `Max` emits `fsel` instead.
- **`FOREACH` re-evaluates `end()` on every iteration.** When retail copies both iterators once before the loop, the source had explicit `it`/`end` locals.
- **`_msg->Obj<T>(n)`** reproduces retail's `__RTDynamicCast` argument order; `dynamic_cast<T*>(_msg->GetObj(n))` sets the arguments up in reverse.
- **Base-class declaration order without a layout change.** `ModifierMgr : MsgSource, UIListProvider` puts `UIListProvider` at offset 0 because `MsgSource`'s vfptr lives in its virtual base. The destructor order then gives away the declaration order.
- **Per-TU flags:** `/DRB3_STRIP_CHEAT_HANDLERS` is added for `MetaPanel.cpp` and `Tour.cpp`. Retail also strips the cheat-only data members that went with those handlers.

### 3.3 Map rows corrected

Each correction rests on retail bytes. The evidence was a branch target, the argument count at the call sites, RTTI, or the return-value register. Every insert went through `tools/gated_map_write.py`; renames are textual value edits.

| address | was | now | evidence |
|---|---|---|---|
| `0x825D20B0` | `GetTotalBestStars` | `CalculateTotalStars(ScoreType)` | both callers pass 1 argument |
| `0x825D1FC8` | — | `GetTotalBestStars` | CampaignSongInfoPanel passes 3 arguments |
| `0x82ba4110` | `NewGem` (void) | `NewGem` returning `TambourineGem*` | `mr r3,r30` epilogue |
| `0x827184a8` | `__ucopy_ptrs<float>` | `TalkyMatcher::Reset` | tail-call target plus call site |
| `0x827187f0` | `_Destroy<pair<String,NetAddress>>` | `TalkyMatcher::SetEnableTalkyMatcher` | same |
| `0x826cce60` | `??__Fmsg@…SpeechMgr` | `GameMic::GetMyMic` | same |
| `0x826b98e0` | `push_back<BeatCollisionData>` (a DC3 type) | `push_back<VocalPhrase>` | all 9 callers push a VocalPhrase |
| `0x824575c8` | `TourProgress::ClearPeformanceProperties` | `RndText::ReserveLines` | `addi r3,r3,0xd8; b vector<Line>::reserve` |
| `0x82b93f40`–`0x82b940a0`, `0x82b98d70`–`0x82b98d80` | shifted names: `ExtractBodyPart`, `QueueEnumJob` | GemTrack/GemManager smasher thunks | each thunk's branch target |
| `0x826efc38` | `__destroy_range_aux<reverse_iterator<GameGem*>>` | `??0TrainerGemTab` | body |
| `0x827919c8` | `TaskMgr::ResetBeatTaskTime` | `BeatMatcher::SetDrumKitBank` | compiler class layout |
| `0x82686ca8` | `__destroy_aux<TrackData>` | `SongDB::ClearTrackPhrases` | body |
| `0x82605c38` | null | `BandStorePanel::GetIndexFile` | body and all 3 callers |
| `0x825d5748` | `SelectSetting` (int) | `SelectSetting` (bool) | callers mask to 8 bits |
| `0x827e1708` | `clear<SetlistArtRecord>` | null | frees 0x4c-byte nodes; SetlistArtRecord's clear folds into `0x822724E8` |
| `0x825D2BB8` | `GetBestSongStatusFlag` (int) | `GetBestSongStatusFlag` (bool) | callers mask to a byte |
| NewNetMessage pair | swapped | un-swapped | RTTI |

### 3.4 Five `.text` blocks re-homed (lane edit)

Each corrected name above sat in a block pinned to a unit whose object cannot define it, so the row read 0. Each block moves to the unit whose blocks surround it:

| block | from | to | row before → after |
|---|---|---|---|
| `0x82B93F48` | PlatformMgr | GemTrack | `PartialHit@GemTrack` 0 → 100 |
| `0x827919C8–0x82791A24` | Task | BeatMatcher | `SetDrumKitBank` 0 → 100 |
| `0x824575C8` | TourProgress | Text | `ReserveLines` 0 → 100 |
| `0x827187F0` | WebSvcMgr | VoiceBeat | `SetEnableTalkyMatcher` 0 → 100 |
| `0x826B98E0–0x826B9960` | SongCollision | VocalTrainerPanel | `push_back<VocalPhrase>` 0 → 99.84 |

`WebSvcMgr.cpp`'s split entry held only that one 4-byte block, so the entry is removed. An empty unit would hard-fail `report.json`. The `WebSvcMgr.cpp` source still compiles.

A sixth re-home, DepthBuffer3D → TambourineManager at `0x826FB758`, and the names `Restart`@`0x826fb758` and `PostDynamicAdd`@`0x826fb880` were also done on this lane. Main's W16-IF had landed the same re-home, at finer grain, and the **identical two names** independently. The rebase therefore dropped the lane's versions. Two lanes reached the same identification separately, so I take it as corroborated.

### 3.5 Five empty-body ICF fold memberships (lane edit)

The functions `GemManager::Ignore(int)`, `VocalPart::Start`/`SetPaused` and `Singer::Start`/`SetPaused` are empty `{}` bodies. Each compiles to a 4-byte `blr` with no relocation. Retail calls the `blr` survivor `0x826c3888` at exactly those call sites:
- the `GemTrack::Ignore` thunk at `0x82b93f40`, which is `lwz r3,0x90(r3); b 0x826c3888`;
- both part and singer loops of `VocalPlayer::Start` (`0x826e46a0`) and `VocalPlayer::SetPaused` (`0x826e4828`).

`tools/icf_pair_adjudicate.py --chase` gives **CHASED T1 PROVEN, VACUOUS-BUT-IDENTICAL** for all five. None of the five is placed anywhere else in the map, which would have made it a contradiction. Each membership has an `added[]` record in the group.

Retail has 23 `blr` bodies, so the byte comparison alone could not pick the survivor. The call sites do: each one branches to `0x826c3888` specifically.

This closes the only row that fell in the merged forks. `Ignore@GemTrack`'s old 100 had paired the `SetFretButtonPressed` thunk under the old shifted map names; at its true address it read 97.5 until the membership landed. It also brings `VocalPlayer::SetPaused` to 100.

## 4. Rows left in the band, by blocker

- **Register allocation or scheduling only.** These were skipped by brief, after about three spellings each. This is the largest class:
  - VocalTrack: UpdatePitchArrow, ProcessStaticLyrics, BuildScrollingDeployZones, GetHarmonyScore
  - GemManager: SetupGems, DrawTrackMasks
  - DisplayChord, DrawBeatLines, Tail::UpdateVerts, PerfectSectionTracker::HandleExitExtent
  - SongSortMgr::IntersectFilter, RockCentral::UpdateSetlist (51 sites, all registers)
  - SaveLoadManager::SetState's residue
  - Most CustomizePanel rows
- **ICF fold-name call sites.** These can only be closed by alias entries, each proven like §3.5:
  - About 35 rows in fork 3 alone sit at 99.75–99.98 with exactly one relocation-name charge, such as `GemPlayer::Hit` (`push_back<Vector2>` / `<UpcomingFretRelease>`) and the `DeleteAll` / `_M_fill_insert` / `resize` rows.
  - The `deque<LyricPlate*>` instantiation called from VocalTrack.
  - `hash_map` default ctors in the BandSongMgr, SongMgr and SongStatusMgr ctors.
  - `vector<unsigned>::push_back`.
  - `SongStatusMgr::GetHighScore`, which is folded into `GetScore` at `0x825D1840`.
  - `Singer::Poll_`: `AtLastPhrase` ≡ `PastFinalNote`, and `IgnorePhrase` folds into a return-false survivor. These are not empty bodies, so they need their own adjudication.
  - `Game::PopulatePlayerLists` calls `GetContainerName@MemcardXbox` where we call `GetBandUsers`. There is no alias record; this needs adjudicating.
- **`VocalTrack::UpdateScrolling` (8,948 B, the largest row).** Not attempted. The in-tree record (W16-GB/FS/CY/CW) puts the residue in liveness plus STLport deque `_M_subtract` term order, which every deque user shares. It needs its own whole-binary A/B.
- **dtk mis-carves.** These need `symbols.txt` edits, which this lane does not make:
  - `??0TrainerGemTab` continues as `fn_826EFC98`.
  - `??0PerformerStatsInfo` is 0x84 bytes, but its tail is carved off as `fn_8257C238`.
  - `GemSmasher::CodaHit` / `CodaHitChord` and `ClosetMgr::PlayFinalizedSound` each have a trailing `blr` split off.
  - `BandUser::DeletePlayer`.
- **rdata carve.** `ProfileMgr::GetJoypadExtraLagInits`: `lbl_82091FBC` is one 8-byte `{14.0f, 74.0f}` object. The renamer names the +4 relocation `__real@41600000` (14.0) and drops the addend, so a correct 74.0f load is charged. Our source is right.
- **Wrong name and wrong pin:**
  - `0x822CD8C0`, named `TourDescPanel::Load`, is `BandStarDisplay::SyncObjects`.
  - `0x8264D058`, named `CharMeshHide::Init`, registers `AppMiniLeaderboardDisplay`.
  - `0x826CCE28–0x826CD438` holds GameMic leaf functions but is pinned to `TrainerPanel.cpp`. `GameMic` has no `.cpp` in our tree, so `GetMyMic` reads 0 where it is pinned.
- **`vector<MemStream>` push_back** (440 B): `new(p) MemStream` picks up BinStream's own placement `operator new` from `MEM_OVERLOAD`, which adds a null check. Fixing it means changing BinStream's allocation overloads engine-wide.
- **`MusicLibraryStore` ctor and ClearPreview:** need `TheContentMgr` to be a plain object; that header change is recorded in-tree at −37 functions.
- **Unexplained:**
  - A reference or iterator spilled to the `0x50(r31)` cleanup slot in one build and not the other: TrackerManager::CreateSource, OvershellPanel::EnableAutoVocals, AssetProvider::Update, ClosetPanel::CycleCamera, CharacterCreatorPanel::RandomizeFace.
  - `GetSoloProfile`'s extra element-address temporary.
  - `GetCareerLevel` and `AccomplishmentProvider::Mat` share one 9-float `.rdata` block at `0x820BC0C0`. A non-`const` table matches `GetCareerLevel` but would move the table to `.data`, so it was not taken.
  - `DoesSongMatchFilter` / `DoesOfferMatchFilter` normalise the find result to bool twice.
  - `IsEmptyPhrase`: an identity `clrrwi`.

## 5. Rebase onto main (91 commits) and how each conflict was resolved

| file / commit | resolution |
|---|---|
| `scripts/target_symbol_map.json` (6 commits) | three-way key-level merge (§5.1) |
| `src/system/math/Interp.h` (forks 3 and 6 made the same change) | kept fork 3's version, which carries the comment |
| `config/45410914/splits.txt`, the re-home commit | took main's side for the TambourineManager block, which W16-IF had already re-homed; re-applied the other five moves to main's text |
| `.pdata` fixed-point commit | skipped as empty; the re-split re-derived `.pdata`, and the one moved line is committed |

### 5.1 How the map conflicts were merged

`~/tmp/w16id/resolve_map.py` applies each commit's own key changes, compared with the merge base, as line edits on upstream's text. It **refuses** if a key was changed differently by both sides. It treats `_bijection_arbitrary` as a set and applies the commit's add/remove delta. It asserts that the parsed result equals upstream plus the commit's changes.

It refused nothing on a real key conflict. Main had independently named the same TambourineManager addresses identically.

## 6. Gates (rebased tip, after the A/B)

```
[map-injectivity] OK: 32161 applied rows, 32160 distinct names, injective (+1 enumerated internal-linkage exception(s))
VALIDATE: PASS -- 1496 map-consistent, 252 tolerated (enumerated above), 0 contradicted, 1749 total
[patch-state] OK: tree is a fixed point of 6 post-compile passes
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The native gate ran on the rebased tip, after the A/B. Only this docs-only commit follows it, and it touches no build input.

## 7. Not done

- No `symbols.txt` edits. The mis-carves in §4 are recorded, not fixed.
- No alias entries beyond the five empty-body memberships in §3.5. The fold-name residue in §4 needs adjudication one pair at a time.
- `UpdateScrolling` was not attempted.
- The permuter was not run.
- **Not merged to main**, per the brief.

## 8. Rows raised, before → after

The values are `fuzzy_match_percent` from full builds, comparing base `add52fdb6` with the merged branch before the rebase. The rebase moved none of these rows downward; §1's A/B is the authoritative aggregate. The table is ordered by weight gained.

| row | unit | B | before | after |
|---|---|---:|---:|---:|
| `?Init@MetaPanel@@SAXXZ` | MetaPanel | 1,676 | 91.30 | **100** |
| `?RebuildHUD@VocalTrack@@QAAXXZ` | VocalTrack | 2,188 | 93.93 | 99.96 |
| `?PrepareNoteTubes@VocalTrack@@QAAXMHAAHH@Z` | VocalTrack | 1,160 | 90.42 | 99.93 |
| `?SetState@SaveLoadManager@@IAAXW4State@1@@Z` | band3/meta_band/SaveLoadManager | 4,096 | 97.32 | 99.89 |
| `?Poll@BandStorePanel@@UAAXXZ` | band3/meta_band/BandStorePanel | 980 | 90.26 | **100** |
| `?DisplayChord@ChordbookPanel@@QAAXI@Z` | ChordbookPanel | 3,436 | 97.20 | 99.94 |
| `?SetupDetailLine@NextSongPanel@@QAAXPAVDataArray@@HPBDM@Z` | NextSongPanel | 1,556 | 94.96 | 99.99 |
| `?UpdateTubePlates@VocalTrack@@QAAXAAV?$deque@PAVTubePlate@@V?$StlNodeAlloc@PAVTubePlate...` | VocalTrack | 772 | 90.69 | **100** |
| `?Poll_@FocusTracker@@UAAXM@Z` | FocusTracker | 760 | 92.63 | 99.97 |
| `?Poll@PatchPanel@@UAAXXZ` | band3/meta_band/PatchPanel | 980 | 94.59 | 99.92 |
| `?OnStartTest@CalibrationPanel@@QAA?AVDataNode@@PAVDataArray@@@Z` | CalibrationPanel | 600 | 91.87 | **100** |
| `?Poll@StoreMainPanel@@UAAXXZ` | band3/meta_band/StoreMainPanel | 664 | 92.98 | 99.94 |
| `?Enter@GemTrainerPanel@@UAAXXZ` | band3/game/GemTrainerPanel | 1,472 | 97.02 | **100** |
| `?AddTrack@MultiplayerAnalyzer@@QAAXHW4TrackType@@@Z` | band3/game/MultiplayerAnalyzer | 444 | 90.22 | **100** |
| `?StartSectionImpl@VocalTrainerPanel@@UAAXXZ` | band3/game/VocalTrainerPanel | 1,292 | 96.66 | **100** |
| `?Handle@ViewSettingsProvider@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | band3/meta_band/ViewSetting | 1,036 | 96.21 | **100** |
| `??0MetaPanel@@QAA@XZ` | MetaPanel | 760 | 95.26 | **100** |
| `?InitRGTuning@GemManager@@QAAXPAVBandUser@@@Z` | GemManager | 460 | 92.23 | **100** |
| `?AddPlayer@Game@@QAAXPAVBandUser@@@Z` | band3/game/Game | 396 | 91.32 | **100** |
| `?AssignTracks@GameConfig@@QAAXXZ` | GameConfig | 716 | 95.59 | **100** |
| `?Poll@Track@@UAAXM@Z` | band3/bandtrack/Track | 604 | 95.04 | **100** |
| `?GetHighestDifficultyForPart@MetaPerformer@@QBAHVSymbol@@@Z` | MetaPerformer | 364 | 91.96 | **100** |
| `?PartPlaysInSet@MetaPerformer@@QBA_NVSymbol@@@Z` | MetaPerformer | 316 | 90.82 | **100** |
| `?LoadCampaignIcons@AccomplishmentPanel@@QAAXXZ` | AccomplishmentPanel | 356 | 91.84 | 99.94 |
| `?GetSetlistMaxVocalParts@MetaPerformer@@QBAHXZ` | MetaPerformer | 292 | 90.21 | **100** |
| `?Poll_@PerfectSectionTracker@@UAAXM@Z` | band3/game/PerfectSectionTracker | 636 | 92.11 | 96.60 |
| `?DrawBeatLines@GemTrack@@QAAXHH@Z` | GemTrack | 448 | 93.53 | 99.33 |
| `?UpdateGameCymbalLanes@GemPlayer@@QAAXXZ` | GemPlayer | 480 | 92.92 | 98.00 |
| `?ResolveAmbiguity@Singer@@QAAXXZ` | band3/game/Singer | 300 | 91.60 | 99.47 |
| `?SetStickers@StickerProvider@@QAAXPAV?$vector@PAVPatchSticker@@V?$StlNodeAlloc@PAVPatch...` | band3/meta_band/PatchPanel | 344 | 92.23 | 98.95 |
| `??1Tour@@UAA@XZ` | Tour | 232 | 90.00 | 99.83 |
| `?StartArbitration@NetSession@@QAAXXZ` | network/net/NetSession | 276 | 92.32 | **100** |
| `?HandleExitExtent@PerfectSectionTracker@@QAA_NMH_N@Z` | band3/game/PerfectSectionTracker | 1,256 | 92.09 | 93.77 |
| `?StartSection@TrainerPanel@@QAAXH@Z` | band3/game/TrainerPanel | 708 | 97.09 | **100** |
| `?LocalEndgameEnergy@VocalPlayer@@QAAXH@Z` | VocalPlayer | 284 | 92.75 | **100** |
| `?UpdateCrowdMeter@GemPlayer@@QAAXMH@Z` | GemPlayer | 748 | 97.26 | **100** |
| `?SendMsg@NetSession@@QAAXABV?$vector@PAVRemoteUser@@V?$StlNodeAlloc@PAVRemoteUser@@@stl...` | network/net/NetSession | 296 | 93.38 | 99.93 |
| `?GetNoteSliceWeight@VocalPart@@QBAMMMH@Z` | VocalPart | 484 | 94.93 | 98.76 |
| `?GetCareerLevel@AccomplishmentGroupProvider@@QBA?AVSymbol@@M@Z` | AccomplishmentPanel | 556 | 94.75 | 97.84 |
| `?SetRandomUnplayedSong@AccomplishmentPanel@@QAAXXZ` | AccomplishmentPanel | 200 | 91.36 | 99.80 |
| `?SendToAllClientsExcept@NetSession@@QAAXABVNetMessage@@W4PacketType@@I@Z` | network/net/NetSession | 308 | 94.35 | 99.81 |
| `?LoadIcons@CampaignGoalsLeaderboardChoicePanel@@QAAXXZ` | band3/meta_band/CampaignGoalsLeaderboardChoicePanel | 352 | 95.26 | 99.94 |
| `?LoadIcons@TourDescPanel@@QAAXXZ` | TourDescPanel | 360 | 95.37 | 99.94 |
| `?CountGemsInSection@TrackerSectionManager@@QBAHPBVPlayer@@H@Z` | TrackerUtils | 272 | 93.97 | **100** |
| `?ContentDone@BandSongMgr@@UAAXXZ` | BandSongMgr | 1,220 | 98.67 | **100** |
| `??1ModifierMgr@@UAA@XZ` | band3/meta_band/ModifierMgr | 252 | 93.65 | **100** |
| `?Disconnect@NetSession@@QAAXXZ` | network/net/NetSession | 988 | 98.56 | **100** |
| `?Text@BadReviewViewSetting@@UBAXHHPAVUIListLabel@@PAVUILabel@@@Z` | band3/meta_band/ViewSetting | 212 | 93.58 | **100** |
| `??0MetadataLoadedMsg@@QAA@PAVDataArray@@_NPBD11@Z` | band3/meta_band/BandStorePanel | 264 | 94.85 | **100** |
| `?Poll@TrackPanel@@UAAXXZ` | band3/bandtrack/TrackPanel | 752 | 96.88 | 98.62 |
| `?Poll@FreestylePanel@@UAAXXZ` | FreestylePanel | 372 | 96.56 | **100** |
| `?CopyPhrasesImp@VocalTrainerPanel@@QAAXABV?$vector@VVocalPhrase@@V?$StlNodeAlloc@VVocal...` | band3/game/VocalTrainerPanel | 272 | 91.18 | 95.84 |
| `?PopulatePlayerLists@Game@@QAAXXZ` | band3/game/Game | 164 | 92.29 | 99.88 |
| `?ChangeDifficulty@GemPlayer@@UAAXW4Difficulty@@@Z` | GemPlayer | 352 | 96.53 | **100** |
| `?HasReachedCampaignLevel@Campaign@@QBA_NVSymbol@@@Z` | Campaign | 280 | 95.70 | **100** |
| `??0ModifierMgr@@QAA@XZ` | band3/meta_band/ModifierMgr | 476 | 97.35 | 99.87 |
| `?GetScaledFanValue@AccomplishmentManager@@QAAHH@Z` | band3/meta_band/AccomplishmentManager | 216 | 95.00 | **100** |
| `?LoadPortrait@PrefabChar@@QAAXXZ` | CharData | 164 | 93.41 | **100** |
| `?GetAllChars@BandProfile@@QBAXAAV?$vector@PAVTourCharLocal@@V?$StlNodeAlloc@PAVTourChar...` | band3/meta_band/BandProfile | 104 | 90.00 | **100** |
| `?InitFromDataArray@TrackerMultiplierMap@@QAAXPBVDataArray@@@Z` | TrackerUtils | 268 | 95.21 | 99.03 |
| `?TambourineSwing@TambourineManager@@QAAHH@Z` | band3/game/TambourineManager | 288 | 92.62 | 95.83 |
| `?RemoveClient@NetSession@@QAAXI@Z` | network/net/NetSession | 212 | 95.75 | **100** |
| `?InitializeMusicLibraryTaskForArtist@Tour@@QAAXAAVMusicLibraryTask@MusicLibrary@@HPBDVS...` | Tour | 244 | 96.39 | **100** |
| `?AddNewRewardVignette@AccomplishmentProgress@@QAAXVSymbol@@@Z` | band3/meta_band/AccomplishmentProgress | 140 | 93.71 | **100** |
| `?LengthSym@BandSongMetadata@@QBA?AVSymbol@@XZ` | BandSongMetadata | 280 | 96.89 | **100** |
| `?FakeComponentSelect@MultiSelectListPanel@@QAAXXZ` | band3/meta_band/MultiSelectListPanel | 124 | 93.03 | **100** |
| `?FakeComponentScroll@MultiSelectListPanel@@QAAXXZ` | band3/meta_band/MultiSelectListPanel | 124 | 93.03 | **100** |
| `?IsUserOnValidScoreType@Accomplishment@@QBA_NPAVLocalBandUser@@@Z` | Accomplishment | 256 | 96.72 | 99.92 |
| `?SetEyebrows@CharacterCreatorPanel@@QAAXVSymbol@@@Z` | CharacterCreatorPanel | 212 | 96.19 | **100** |
| `??0BandSongMgr@@QAA@XZ` | BandSongMgr | 320 | 97.38 | 99.88 |
| `??0SongStatusMgr@@QAA@PAVLocalBandUser@@PAVBandSongMgr@@@Z` | SongStatusMgr | 124 | 93.39 | 99.84 |
| `?GetPlayerContributionString@OverdriveTracker@@UBA?AVString@@VSymbol@@@Z` | OverdriveTracker | 240 | 96.58 | 99.92 |
| `?GetNotesHitFraction@GemStatus@@QBAMPA_N@Z` | GemPlayer | 104 | 93.08 | **100** |
| `?ChallengeSuccess@TrainerPanel@@QBA_NXZ` | band3/game/TrainerPanel | 80 | 91.75 | **100** |
| `?PosToNextGroupPos@StoreOfferProvider@@QAAHH@Z` | band3/meta_band/StoreOfferProvider | 92 | 93.17 | **100** |
| `?TranslateRelativeTargets@OverdriveTracker@@UAAXXZ` | OverdriveTracker | 516 | 98.80 | **100** |
| `?CheckHitBRECondition@AccomplishmentSongConditional@@QBA_NPAVSongStatusMgr@@VSymbol@@AB...` | AccomplishmentSongConditional | 92 | 93.70 | **100** |
| `?CheckAllDoubleAwesomesCondition@AccomplishmentSongConditional@@QBA_NPAVSongStatusMgr@@...` | AccomplishmentSongConditional | 92 | 93.70 | **100** |
| `?CheckAllTripleAwesomesCondition@AccomplishmentSongConditional@@QBA_NPAVSongStatusMgr@@...` | AccomplishmentSongConditional | 92 | 93.70 | **100** |
| `?CheckPerfectDrumRollsCondition@AccomplishmentSongConditional@@QBA_NPAVSongStatusMgr@@V...` | AccomplishmentSongConditional | 92 | 93.70 | **100** |
| `?CheckFullComboCondition@AccomplishmentSongConditional@@QBA_NPAVSongStatusMgr@@VSymbol@...` | AccomplishmentSongConditional | 92 | 93.70 | **100** |
| `??1Game@@UAA@XZ` | band3/game/Game | 792 | 99.31 | **100** |
| `??1Tail@@UAA@XZ` | Tail | 276 | 98.01 | **100** |
| `?HandleSongCompleted@TourPerformerImpl@@QAAXPBVBandStatsInfo@@@Z` | TourPerformer | 228 | 97.60 | 99.91 |
| `?FailAllContexts@ContextWrapperPool@@QAAXXZ` | ContextWrapper | 220 | 93.69 | 96.00 |
| `?GetRemoteMachine@BandMachineMgr@@QBAPAVRemoteBandMachine@@I_N@Z` | BandMachineMgr | 148 | 96.62 | **100** |
| `?OnMsg@BandMachineMgr@@QAA?AVDataNode@@ABVNewRemoteUserMsg@@@Z` | BandMachineMgr | 244 | 97.79 | 99.84 |
| `?AddRemoteMachine@BandMachineMgr@@QAAXI@Z` | BandMachineMgr | 296 | 98.31 | **100** |
| `??0UIEventMgr@@QAA@XZ` | UIEventMgr | 176 | 97.39 | **100** |
| `?HasProgress@AccomplishmentPanel@@QBA_NXZ` | AccomplishmentPanel | 168 | 97.36 | **100** |
| `?HasPlayerForInstrument@Tracker@@QBA_NVSymbol@@@Z` | band3/game/Tracker | 72 | 93.89 | **100** |
| `?IsScoring@Track@@UBA_NXZ` | band3/bandtrack/Track | 96 | 95.42 | **100** |
| `?GetCurrentValue@AccomplishmentPanel@@QBAHXZ` | AccomplishmentPanel | 164 | 97.34 | **100** |
| `?GetMaxValue@AccomplishmentPanel@@QBAHXZ` | AccomplishmentPanel | 164 | 97.34 | **100** |
| `?IsWinning@TourPerformerImpl@@UBA_NXZ` | TourPerformer | 148 | 97.14 | **100** |
| `?HasAward@AccomplishmentPanel@@QBA_NXZ` | AccomplishmentPanel | 100 | 95.76 | **100** |
| `??0NextSongPanel@@QAA@XZ` | MetaPanel | 172 | 97.56 | **100** |
| `?GetCurrentStatus@BadReviewViewSetting@@UBAPBDXZ` | band3/meta_band/ViewSetting | 160 | 97.38 | **100** |
| `?IntToSetlistIndex@SetlistMergePanel@@SAHHH@Z` | SetlistMergePanel | 160 | 97.38 | **100** |
| `?UpdateTimeRemainingDisplay@OverdriveTimeTracker@@QAAXXZ` | OverdriveTimeTracker | 112 | 96.25 | **100** |
| `?IsConditionMet@AccomplishmentTourConditional@@QBA_NPAVBandProfile@@ABUAccomplishmentTo...` | band3/meta_band/AccomplishmentTourConditional | 76 | 94.47 | **100** |
| `?GetTypeForGem@GemManager@@QAA?AVSymbol@@H@Z` | GemManager | 872 | 99.54 | **100** |
| `?Init@SessionMgr@@SAXXZ` | band3/meta_band/SessionMgr | 152 | 97.24 | 99.87 |
| `?CountNonEmptySections@TrackerSectionManager@@QBAHPBVTrackerSource@@_N@Z` | TrackerUtils | 288 | 99.10 | **100** |
| `?RecordBattleScore@MetaPerformer@@QAAXABVBandStatsInfo@@_N@Z` | MetaPerformer | 552 | 97.75 | 98.19 |
| `?Handle@StoreOfferProvider@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | band3/meta_band/StoreOfferProvider | 1,556 | 99.85 | **100** |
| `?SetPaused@VocalPlayer@@UAAX_N@Z` | VocalPlayer | 232 | 99.83 | **100** |
| `?GetPlayerFromTrack@Game@@QBAPAVPlayer@@H_N@Z` | band3/game/Game | 84 | 99.52 | **100** |
| `?OverrideBasePoints@MultiplayerAnalyzer@@QAAXHW4TrackType@@ABVUserGuid@@HHH@Z` | band3/game/MultiplayerAnalyzer | 316 | 99.87 | **100** |
| `?Start@VocalPlayer@@UAAXXZ` | VocalPlayer | 248 | 94.92 | 95.08 |
| `?CopyTubes@VocalTrainerPanel@@QAAXH@Z` | band3/game/VocalTrainerPanel | 944 | 99.51 | 99.53 |
| `?PostDynamicAdd@VocalPlayer@@UAAXXZ` | VocalPlayer | 224 | 99.91 | **100** |
| `?OnRefreshTrackButtons@GemPlayer@@QAAXXZ` | GemPlayer | 108 | 99.81 | **100** |
| `?SetMicProcessing@Singer@@QAAX_N0@Z` | band3/game/Singer | 84 | 99.76 | **100** |
| `?ProcessTalkyData@Singer@@QAAXXZ` | band3/game/Singer | 168 | 99.88 | **100** |
| `?LocalShowFillHit@GemPlayer@@QAAXHH_N@Z` | GemPlayer | 140 | 99.86 | **100** |
| `?DisableOverdrivePhrases@Player@@QAAXXZ` | band3/game/Player | 96 | 99.79 | **100** |
| `?FretButtonDown@GemPlayer@@UAAXHM@Z` | GemPlayer | 304 | 99.93 | **100** |
| `?Poll@Singer@@QAAXMABVSongPos@@MM@Z` | band3/game/Singer | 272 | 99.93 | **100** |
| `??0GemTrainerPanel@@QAA@XZ` | band3/game/GemTrainerPanel | 356 | 99.94 | **100** |
| `?ApplyPlayback@GameMicManager@@QBAX_NPAVGameMic@@@Z` | GameMicManager | 80 | 99.75 | **100** |
| `?SetDrumKitBank@GemPlayer@@QAAXPAVObjectDir@@@Z` | GemPlayer | 8 | 97.50 | **100** |
| `?FillHit@GemTrack@@QAAXHH@Z` | GemTrack | 8 | 97.50 | **100** |
| `?ImplicitGem@GemPlayer@@UAAXHMHABVUserGuid@@@Z` | GemPlayer | 112 | 99.82 | **100** |
| `?PlayDrum@GemPlayer@@QAAXHHMH@Z` | GemPlayer | 120 | 99.83 | **100** |
| `?LoadFixed@SongStatusMgr@@UAAXAAVFixedSizeSaveableStream@@H@Z` | SongStatusMgr | 232 | 99.91 | **100** |
| `?OnMsg@MusicLibrary@@QAA?AVDataNode@@ABVRemoteMachineLeftMsg@@@Z` | MusicLibrary | 192 | 99.90 | **100** |
| `?Ignore@GemPlayer@@UAAXHMHABVUserGuid@@@Z` | GemPlayer | 192 | 99.90 | **100** |
| `?RefreshArchivedBattles@MusicLibraryNetSetlists@@QAAXXZ` | band3/meta_band/MusicLibraryNetSetlists | 132 | 99.85 | **100** |
| `??0LyricPlate@@QAA@PAVRndText@@PBV1@1@Z` | band3/bandtrack/Lyric | 284 | 99.93 | **100** |
| `?UpdateSongStats@SongStatusMgr@@QAA_NW4ScoreType@@W4Difficulty@@ABVPerformerStatsInfo@@...` | SongStatusMgr | 476 | 98.15 | 98.19 |
| `?FretButtonUp@GemPlayer@@UAAXHM@Z` | GemPlayer | 600 | 99.97 | **100** |
| `?Hit@GemPlayer@@UAAXHMHIW4GemHitFlags@@@Z` | GemPlayer | 2,724 | 99.90 | 99.90 |
| `?FirstFrame_@PerfectOverdriveTracker@@UAAXM@Z` | band3/game/PerfectOverdriveTracker | 344 | 99.92 | 99.94 |
| `?OnMsg@SaveLoadManager@@QAA?AVDataNode@@ABVNoDeviceChosenMsg@@@Z` | band3/meta_band/SaveLoadManager | 176 | 99.95 | **100** |
| `?SetupRealGuitarFretPos@GemManager@@QAAXXZ` | GemManager | 680 | 99.99 | **100** |
| `?OnMsg@SaveLoadManager@@QAA?AVDataNode@@ABVDeviceChosenMsg@@@Z` | band3/meta_band/SaveLoadManager | 180 | 99.96 | **100** |
| `?Configure@AccomplishmentTourConditional@@QAAXPAVDataArray@@@Z` | band3/meta_band/AccomplishmentTourConditional | 304 | 99.92 | 99.93 |
