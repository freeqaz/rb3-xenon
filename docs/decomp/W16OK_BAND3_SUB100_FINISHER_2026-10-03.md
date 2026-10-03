# W16-OK — src/band3 sub-100 rows, largest size-if-it-crosses first (2026-10-03)

**Branch** `w16-ok`, off main `8bba58005`. **Ruler** `name_check` (graded, from `report.json`
`provenance.diff_config`). The permuter was not run. Out of scope by brief: `src/network`, the Quazal
block, and `src/system/{bandobj,char,world,beatmatch}` (lane W16-OL).

## 1. Population

Every row with `fuzzy < 100` in a unit whose `objdiff.json` `source_path` is under `src/band3/`, on the
worktree build of `8bba58005`: **401 rows / 127,500 B** (289 non-zero / 117,792 B, plus 112 at fuzzy 0 /
9,708 B). Each was diffed with `objdiff-cli diff` under the project config and classified with W16-NA's
classifier (`~/tmp/w16na/cls.py`; output `~/tmp/w16ok/cls_base.json`):

| class (what is charged) | rows | bytes |
|---|---:|---:|
| STRUCT_INSDEL (includes all 112 fuzzy-0 rows) | 179 | 65,232 |
| REG_ONLY | 48 | 35,428 |
| NAME_ONLY | 117 | 12,164 |
| NAME+REG | 14 | 8,484 |
| IMMEDIATE | 35 | 4,188 |
| OPCODE | 7 | 1,244 |
| STACK_REG | 1 | 760 |

Of the 112 fuzzy-0 rows, 93 (7,692 B) are anonymous `fn_` rows, mostly Quazal DDL helpers and EH
funclets; they were not worked. The other 19 are named.

The population was split into five disjoint slices (`~/tmp/w16ok/slice_*.json`), four worked by forks in
their own worktrees and one (S5) in the lane worktree:

| slice | rows | bytes |
|---|---:|---:|
| S1 bandtrack | 22 | 26,128 |
| S2 game | 43 | 23,544 |
| S3 meta_band A–M | 46 | 24,600 |
| S4 meta_band N–Z, net_band, tour | 61 | 31,356 |
| S5 NAME_ONLY rows (all dirs) + named fuzzy-0 rows | 136 | 14,180 |

## 2. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ok-ab --patch <git diff main w16-ok -- . ':!docs'> --label w16ok-whole-branch-final`
- **Worktree:** fresh, made with `scripts/setup_worktree.sh` at main `8bba58005`.
- **Patch:** 19 files, `+300/−211`. The kinds are map, source and splits, so both legs re-split, and leg B
  reached a `symbols.txt` fixed point.
- **objdiff-cli:** sha `c1b7d952`, stable across legs.
- **Run dir:** `~/tmp/wt-w16-ok-ab/.ab_measure_runs/20261003-021328-w16ok-whole-branch-final-1524310/`.

```
leg A: matched=53165 masked=25127 honest=28038 code%=57.205140  (recompiles: 0, settled)
leg B: matched=53193 masked=25130 honest=28063 code%=57.270410  (recompiles: 624, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+28  Δmasked_equal=+3  Δhonest=+25  Δcode%=+0.065270pp  Δcode_bytes=+6688
Δfuzzy=+0.012905pp   (legA 63.507835 -> legB 63.520740)
units at 100% [mpn ruler]: legA 510 -> legB 521  (11 reached 100, 0 fell off; pairable units 1734->1733)
units at 100% [all-rows-fuzzy ruler]: legA 451 -> legB 462  (11 reached 100, 0 fell off)
[control none] Δmatched_code=+5636 B -- NOT_APPLICABLE (kinds=map,source,splits)
```

**Two runs.** The first A/B (run dir `…/20261003-020856-w16ok-whole-branch-1453623/`) measured the branch
before the last alias commit, `b1fd7140e`.
- **Prediction, written before it ran:** +25 fns / +6,288 B, 0 rows down. That was the in-tree composition
  read of the merged forks: S5 +13 / +1,864, S1 +1 / +744, S2 +4 / +604, S4 +2 / +580, S3 +5 / +2,496.
  Each was measured against the same base report, and the merged tree reproduced the sum exactly at both
  merge waves.
- **Measured: +25 / +22 honest / +6,288 B.**

`b1fd7140e` then read +3 / +400 in-tree against that run's leg B. The final run above was predicted at
+28 / +6,688 and measured **+28 / +6,688**.

**Δfuzzy did not move between the two runs.** Leg B read 63.520740 both times. `report.json` stores that
aggregate as an f32, and the +400 B change is about +0.000006 pp, which is below one ulp at 63.5
(about 7.6e-6). `matched_code` moved the predicted +400.

**Row diff of the archived final legs** (`~/tmp/w16na/rowdiff.py`; output in `~/tmp/w16ok/ab2_rowdiff.txt`):
- 39 rows up, **0 rows down, 0 rows off 100** on either ruler.
- The row sums reproduce the deltas exactly.
- 10 keys were renamed or re-homed. Every new key is at fuzzy and `mpn` 100, and no old key scored higher
  than its replacement. (`MakeString<float,const char*>` was 100 in VocalTrack and is 100 in DisplayEvents.)
- Pairable units went 1734 → 1733 because the `MoveAsyncDetector.cpp` heading is gone (§3.5).

Of the 11 unit completions, 5 are DENOMINATOR_SHRANK: a wrongly attributed row left BandMachineMgr,
Performer, SongSortBySong, PresenceMgr and GigFilter.

## 3. S5 — relocation-name rows and named fuzzy-0 rows (lane worktree)

S5 measured in-tree, full `./tools/ninja-locked` build plus a whole-report row diff after every commit:
**+16 fns / +2,264 B, 0 rows down** (+13 / +1,864 before the merges, then +3 / +400 for `b1fd7140e`).

### 3.1 Pair census

The 117 NAME_ONLY rows carry **77 distinct (retail, ours) pairs**. All were chased
(`icf_pair_adjudicate.py --chase`, after `--chasetest` passed). Results:

- **22 pairs (9,112 B of row size) chase PROVEN only through a CYCLE-ASSUMED leaf.** Channel 1 of W16-JE's
  two-channel rule holds for most of them (every leaf is self-recursive and its masked shape is unique
  image-wide), but **none has a retail name witnessing our element type** (`cycproof.py`, `cocallee.py`
  rerun: 0 fan-in hits, 0 co-callee hits). That is the class W16-JE parked on purpose: for 4-byte
  element types the code is identical, so "ours is `vector<ShortcutNode*>`" cannot be told apart from
  "retail's was some other pointer vector". **Left parked.** This includes the largest row in S5,
  `VocalPlayer::HandlePhraseEnd` (2,332 B, `sort<VocalPart**>` vs retail `sort<RndPollable**>`), and the
  pointer `_M_fill_insert` family (ShortcutNode, GameMic, Track, RndTex, `_Slist_node_base`).
  **One of the 22 passes both channels:** `_M_fill_insert<unsigned char>` → the `<char>` body at
  `0x823ea1c8`. Its leaf shape is unique, and retail `CachedRead<unsigned char>` and
  `operator>>(vector<unsigned char>)` call the survivor chain. It was admitted (`b1fd7140e`):
  `GemStatus::Resize` plus those two witness rows go to 100, +3 fns / +400 B.
- **Most of the rest are 40–88 B EH funclets** whose callee is a tiny destructor. The chase calls them
  VACUOUS (under 4 words). They were re-examined by normalising both bodies (see §3.2).

### 3.2 A carve artifact that the chase reads as "bodies DIFFER"

`~TrackerDesc` (ours, 36 B) against retail `~BandHeadShaper` at `0x822afd68` (32 B) chased REFUTED,
"masked bodies DIFFER". The extra word is the `blr` MSVC leaves after the tail branch to
`MemOrPoolFreeSTL`. Retail has the same `blr`, at `0x822AFD88`, but dtk carved it into a 4-byte function
of its own. With that word set aside, the two bodies are byte-identical, relocation names included.

Normalising every S5 pair the same way (strip retail's trailing zero padding, or our trailing `blr`)
and requiring a unique retail census for the normalised shape and relocation names gave the admissions
below. Each also has the retail call site as a witness.

### 3.3 What was installed (`scripts/symbol_aliases.json`), each with its evidence record

| ours | retail survivor | proof | rows to 100 |
|---|---|---|---|
| `~TrackerDesc` | `~BandHeadShaper` `0x822afd68` | byte-identical after the carve; 1 retail body of this shape image-wide; the 5 funclets pass `this+0x1c` (Tracker::mDesc), `this+0x20` (TrackerManager::mDesc) or a stack TrackerDesc | 5 Tracker/TrackerManager funclets |
| `TourPerformerImpl::GetCurrentQuestFilter` | `InternalSavedSetlist::GetIdentifyingToken` | `lwz r11,0x30(r4); stw; blr`, no relocations, 1 retail body image-wide; both members `Symbol @0x30` (compiler layout) | `Tour::GetCurrentFilterName`, `TourChallengeResultsPanel::UpdateSetlistLabel` |
| `~FriendsListChangedMsg`, `~ContentInstalledMsg` | `~Message` | identical except the vptr store, whose retail side is the placeholder `~Message` itself installs | 2 funclets (+2 PlatformMgr_Xbox funclets via the same fold) |
| `~TourHideShowFiltersMsg`, `~KickPlayerMsg`, `~SyncLocalMachineMsg` | `~NetMessage` | same | 3 funclets |
| `_Vector_base<pair<float,float>>` ctor | `_Vector_base<Extent>` ctor | chase PROVEN, no cycle; its one name difference is the existing `MemOrPoolAllocSTL` → `MemOrPoolAlloc` fold | (row moved 99.66 → 99.83; its other site is a vacuous `get_allocator` pair) |

### 3.4 `SetlistToStorePanel::StartMetadataLoaders` was never written

The row scored 0 because our source declared it and never defined it, so `LoadSongMetadata` called
nothing. Neither rb3-Wii nor DC3 has it. It was written from the retail asm (`0x826429A0`, 256 B):
resume `mSongs` at index `mLoaders.size()`, push `new DataNetLoader(String(path.c_str()))`, stop after
20 per call. Its anonymous helper `fn_82642918` (124 B, also a 0 row) builds
`MakeString("dlc_store/%s/%s/songs/%i/", region, SystemLanguage(), id)` from a `.data` format pointer.
It was named `GetSongMetadataPath` and mapped. Two aliases were needed, both chase PROVEN with no cycle
leaves and witnessed by these call sites: `MakeString<Symbol,const char*,int>` → `0x8229d1a8`, and
`push_back<DataNetLoader*>` → `0x82b5f808` (ours proves only against the survivor, not its one other
masked twin). Both functions 0 → 100, plus the StartMetadataLoaders EH funclet `fn_82642AF0` 0 → 100.

### 3.5 Map and pin defects behind named fuzzy-0 rows

- **`0x826C32A8` is GemPlayer's `vector<HeldNote>::_M_insert_overflow_aux`.** It was named for the
  `PlayerTrackConfig` twin, which GemPlayer.obj never instantiates, so the GemPlayer row could not pair.
  Retail has one body for both 0x24-byte instantiations, and the copy the linker kept is inside GemPlayer's
  span (`mHeldNotes.resize(5)`). Both spellings chase PROVEN with no cycle leaves. Renamed. The
  PlayerTrackConfig spelling and the two HeldNote callees (`__uninitialized_copy`, `__uninitialized_fill_n`,
  flat T1 PROVEN, one twin each) fold into their survivors. **Tested first and refuted: an alias alone does
  not pair a row.** objdiff pairs rows by name in the unit's base object, so the map name has to change.
- **`0x826C75E0`** (`_M_fill_insert` for the same 0x24-byte vector) was pinned under `MoveAsyncDetector.cpp`,
  a DC3 Kinect source with no RB3 counterpart, named for DC3's `DetectFrame`, and scored 99.79 against code
  retail never had. Re-homed into GemPlayer as `_M_fill_insert<HeldNote>`; the heading is deleted.
- **Four COMDAT islands at block edges** were re-homed to the neighbour that defines them:
  `??_GQuestJournal` (GigFilter → QuestJournal), `??_GMusicLibraryTaskMsg` (PresenceMgr → Game),
  `??1SongSortCmp` (BandMachineMgr → SetlistSortByLocation),
  `NonDestructiveTransitionEvent::OnActivate` (SongSortBySong → UIEvent).

### 3.6 Left, with reasons

- **Campaign's two `map<Symbol,Symbol>` islands** (`insert_unique` `0x822EA818` 472 B, `_M_create_node`
  `0x823D9628` 92 B). No object of ours instantiates them. Their retail callers are FileMergerOrganizer
  (`src/system/char`, W16-OL's) and GemTrackDir, so they are engine islands mis-pinned under Campaign.
  Handed to W16-OL.
- **`0x826C23A0`**, the `_M_fill_insert_aux` of GemPlayer's HeldNote resize chain, is pinned under
  `system/world/Dir` (W16-OL's area). Handed over.
- **`SendDataPoint<const char*,int>`** (`0x82563258`, MetaPerformer, 100 B). Its only caller is an
  unidentified, source-less TU right after MetaPerformer (`"rbn/audition/fail"`, `on_validation_failed`).
- **`??_ETrainerPanel`**, `ObjPtrList<RndGroup>::push_back`, `_M_find<int,SongStatus>`, Gem's two
  anonymous-namespace `__destroy_range_aux`: no object of ours defines them.
- **The 40–88 B funclet pairs** over `map<int,float>` / `_Rb_tree` destructors whose retail target is a
  placeholder (`fn_827690D0`, `fn_826DA438`, `fn_8260EED8`). These would need an anonymous address named
  first, which is a bet under `name_check`.
- **The anonymous RockCentral rows** `fn_8250A5..`. They call into the Quazal block (out of scope).

## 4. Forks S1–S4

Each fork worked its slice in its own worktree, verified every kept fix with a full build and a
whole-report row diff against the shared base report, and committed each fix separately. The forks were
merged into `w16-ok` with `--no-ff`.

| fork | rows up | to 100 | Δfns | Δbytes | rows down |
|---|---:|---:|---:|---:|---:|
| S1 bandtrack | 1 | 1 | +1 | +744 | 0 |
| S2 game | 4 | 4 | +4 | +604 | 0 |
| S3 meta_band A–M | 11 | 9 | +5 | +2,496 | 0 |
| S4 meta_band N–Z, net_band, tour | 3 | 2 | +2 | +580 | 0 |

### 4.1 Behaviour and type defects

- **`VocalTrack::PollLyricAnimations`** (744 B, 84.26 → 100). There is a behaviour bug here:
  `DumpLyricPlates` takes `plate->mSyllables.front()->mLead` (`lwz 0x34; lwz 0; lwz 0x48`), and our
  source, following the oracle, passed `!mSyllables.empty()`. Also: a debug print retail does not have
  was removed, the rollback poll time no longer overwrites `ms`, and `end()` is evaluated once.
  `MakeString<float,const char*>` was re-homed to DisplayEvents, its only retail caller, which removed
  the −92 B cost a source comment had recorded for dropping the print.
- **`LayerProvider::GetMatForData`** (508 B, 86.68 → 88.01). Retail writes `mTexGen`/`mTexWrap` raw and
  does one `mDirty |= 2` (from `SetTexXfm`). Our `SetTexGen`/`SetTexWrap` did two more read-modify-writes.
  This needed `friend class LayerProvider;` in `src/system/rndobj/BaseMaterial.h`, which does not change
  codegen. **This is the lane's only edit outside `src/band3`.**
- **User-declared empty destructors**: `SetlistSubmissionMsg` (104 B, 88.04 → 100), `BandScreen`
  (100 B, 87.56 → 100) and `Instarank` (72 B, 65.83 → 100). A user-declared `virtual ~X() {}` makes MSVC
  emit the derived vptr store; retail's compiler-generated destructor does not. S3 and S4 each found this
  independently. A sweep of the remaining band3 destructor rows found no other instance.
- **`Singer::AddAmbiguousPart`** (152 B, 87.21 → 100): retail builds `{part1, part2}` as one 8-byte object
  and copies it with a single `ld`/`std`.
- **`vector<SingerResultsData>::_M_fill_insert_aux`** (392 B, 96.73 → 100): a TU-local
  `__copy_backward_ptrs` specialisation, written to imitate MWCC's unrolling, was removed; retail
  uses the generic STLport loop.
- **`SongStatusMgr::UpdateSongStats`** (476 B, 98.19 → 100): an `updated = updated != 0`
  re-normalisation that retail does not have.
- **`MusicLibraryStore::ParseOffers`** (292 B, 88.90 → 100): retail merges two `delete; continue;` arms.
- **`MetaPerformer::SaveAndUploadScores`** (684 B, 98.36 → 99.42): signed `size() <= 0` (retail
  `srawi.`), plus a `Server*` local. One dead stack store remains.

### 4.2 Expression-path fixes (no behaviour change)

`AccomplishmentProgress::UpdateScoreTypeSpecificStats` (580 B → 100, via the `Stats` accessors),
`CalibrationPanel::UpdateProgress` (472 B → 100, `Find` through a local `ObjectDir*`),
`CharacterCreatorPanel::SetHair`/`SetFaceHair`/`SetGlasses`/`SetFaceType` (204 ×3 + 288 B → 100, bind the
`Outfit&` / `Head&` the way the matched `SetEyebrows` does), `AssetProvider::ComponentStateOverride`
(80 B → 100).

### 4.3 Map and pin defects

- **`0x82365E58` is `TourGameRules::GetTarget`, not `Stats::GetVocalPartPercentage`.** It reads a vector
  at `this+0x8` (Stats keeps that vector at 0x70), it fills a 16-byte hole in TourGameRules' pin, and its
  caller passes the object it just passed to `GetNumTargets`. Renamed, and re-homed from Performer.
- **`0x826BC9A8` is `GemPlayer::UnkTU5Virtual`** (vtable +0x180). It had been read at 70.45, byte-paired
  with an unrelated funclet.

### 4.4 Examined and left (summary; the fork reports list every row)

- **Register, scheduling or stack-slot only, after 2–5 spellings each:**
  - bandtrack: `UpdateTambourineGems`, `DrawFill`, `SetupGems`, `DrawBeatLine`, `Tail::UpdateVerts`, `Gem::Poll`
  - game: `FocusTracker::Poll_`, `CopyTubes`, `CopyPhrasesImp`, `TambourineSwing`
  - meta_band: `ParseDataResultsIntoSetlists`, `CycleCamera`, `RemoveOldestCachedContent`,
    `DoesOfferMatchFilter` / `DoesSongMatchFilter`, `RecordAccomplishmentData`
  - The many REG_ONLY rows already at `mpn` 100 that earlier census docs file as permuter walls were not
    reopened.
- **An STLport experiment, reverted.** Swapping the term order in `deque::size()` changed nothing in
  `UpdateTambourineGems` and knocked a 232 B `deque` `__copy` row off 100. So the "needs an STLport change"
  note in VocalTrack is wrong about term order.
- **A compiled-away EH state with no recoverable source.** `ModifierMgr::IsHidden`, `IsActive`,
  `IsModifierActive(Symbol)`, `Campaign::GetLaunchUser` and `SongDB::PostLoad` each keep a retail FuncInfo
  with one empty cleanup state. A dead loop around a `std::vector` local reproduces the frame exactly. It
  was **not** written into source, because it would be an invented construct. `Campaign::OnMsg(ProfileSwappedMsg)`
  is blocked behind `GetLaunchUser` not being inlined.
- **Behaviour-checked and identical:** `SaveLoadManager::OnMsg(MCResultMsg)` (60.9%). Case-by-case
  comparison shows the same behaviour; retail splits the switch with a binary test. Regrouping the cases
  was inert.
- **`ProfileMgr::GetJoypadExtraLagInits`:** initialising to the default value fell to 72.5% and took the
  ProfileMgr ctor off 100. Reverted.
- **Not reopened (recorded walls):**
  - `CustomizePanel::Handle`: `clrlwi`, 18+ spellings.
  - `UIStats::MaybePublish`: W16-EI.
  - `OvershellPanel::ResolveSlotStates`: W16-GE.
  - `VocalTrack::UpdateScrolling`: ~10 lanes.
  - `AccomplishmentProvider::Mat`: rdata packing.
  - The TU5 patch rows `IsDemo`, `AddSongData`.
- **Identification left for later:** UploadErrorMgr's funclets belong to an unpinned token-redemption
  handler, `fn_82641670` (1,708 B).

## 5. Rows that went down

**None.** 0 rows down and 0 rows off 100 in the whole-branch A/B, on both the fuzzy and `mpn` rulers.
Every fork and every S5 commit was also row-diffed with 0 down.

## 6. Handed to W16-OL (its directories; not touched here)

- Campaign's `map<Symbol,Symbol>` islands `0x822EA818` / `0x823D9628` (564 B at 0%). Their retail callers
  are FileMergerOrganizer (`system/char`) and GemTrackDir.
- `0x826C23A0` (396 B), GemPlayer's `vector<HeldNote>::_M_fill_insert_aux`, pinned under
  `system/world/Dir`.
- `BandHeadShaper::~BandHeadShaper` reads 87.5% in its own row. Its body is the `~TrackerDesc` fold
  target installed in §3.3.

## 7. Native gate (run last, on `149f492a9`)

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
Log: `~/tmp/w16ok/native_gate.log`.
