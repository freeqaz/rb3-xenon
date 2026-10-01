# W16-LD — second finisher pass, game layer 90–99.99 (2026-10-01)

**Branch** `w16-ld`, rebased onto main `d183547ca` (W16-LC). **Not merged to main.**
**Ruler** `name_check` (graded, from `report.json` `provenance.diff_config`).

**Population:** every row with `90 ≤ fuzzy < 100` in a unit whose `objdiff.json` `source_path` is under
`src/band3/{meta_band,game,bandtrack,net_band}/`. W16-ID, W16-JC and W16-KD had worked this band.

| measured on | rows | bytes | named rows | named bytes |
|---|---:|---:|---:|---:|
| main `9d627e235` (start), and A/B leg A at `d183547ca` (identical) | 433 | 152,080 | 270 | 145,192 |
| A/B leg B (this branch) | 360 | 131,652 | 206 | 125,144 |

At the start, the charge classifier (W16-JC's `classify.py`, run on `objdiff-cli diff` output) put 85 named rows in the
instruction-level class, 64 in register-only, 96 in relocation-name-only (16,852 B), and 14 in register plus relocation.

## 1. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ld-ab --patch <git diff d183547ca w16-ld -- . ':!docs'>` was run once.
- **Worktree:** fresh, made with `scripts/setup_worktree.sh` at main `d183547ca`.
- **Patch:** 33 files, `+655/−239`.
- **Re-split:** the patch includes map and splits edits, so both legs re-split.
- **Run dir:** `~/tmp/wt-w16-ld-ab/.ab_measure_runs/20261001-162559-ab_final-3404581/`.

```
leg A: matched=49579 masked=24330 honest=25249 code%=50.989967  (recompiles: 0, settled)
leg B: matched=49656 masked=24337 honest=25319 code%=51.203180  (recompiles: 111, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+77  Δmasked_equal=+7  Δhonest=+70  Δcode%=+0.213213pp  Δcode_bytes=+21848
Δfuzzy=+0.004673pp   (legA 58.779762 -> legB 58.784435)
unit net (ALL units) = +77   vs whole-binary Δmatched = +77
units at 100% [mpn ruler]: legA 379 -> legB 391  (Δ+12; 12 reached 100, 0 fell off; pairable units 1759->1759)
units at 100% [all-rows-fuzzy ruler]: legA 311 -> legB 317  (Δ+6; 6 reached 100, 0 fell off; pairable units 1759->1759)
[control none] Δmatched_code=+15608 B -- NOT_APPLICABLE (kinds=map,source,splits)
[tree] restored to the pre-run state (33 path(s), verified by re-reading the diff AND the untracked set)
```

**Prediction, written before the run:** +77 functions and +21,848 B. That was the pre-rebase in-tree figure on
`9d627e235` (§5), and W16-LC's diff touched none of this band's source. The measurement matched it exactly. Leg B's
49,656 also equals an in-tree full build of the rebased tip.

**Row-level diff of the archived legs** (`~/tmp/w16ld/rowdiff.py`):
- 75 rows up, 1 down, 21 gone/new pairs.
- **0 rows fell off 100 on either the fuzzy or the `mpn` ruler.**
- The row sums reproduce the deltas exactly: Δ(fuzzy==100 bytes) = +21,848 and Δ(mpn==100 rows) = +77.
- Every renamed row that was at 100 in leg A is at 100 under its corrected name in leg B.
- The down row is `VocalPlayer::Poll`, 95.025 → 94.985 (§3.4). Both edited sites now carry retail's instructions; the
  diff alignment scores an insert/delete pair where it used to score a replace.

## 2. Method

1. **Seven forks**, each with its own worktree and branch:

   | fork | rows |
   |---|---|
   | 1 | `VocalTrack::UpdateScrolling` and its TU |
   | 2 | meta UI panels |
   | 3 | meta data and managers, net_band |
   | 4 | vocal and tracker rows |
   | 5 | gem and track rows |
   | 6 | relocation-name-only rows: fold, wrong callee, or wrong map name, decided on retail bytes |
   | 7 | a second wave that applied fork 3's finding (§4.1) to three rows fork 2 had left |

2. **Per-fix check.** Every fix was verified with a full `./tools/ninja-locked` build and a whole-report row diff
   against the lane base, never with `run_objdiff` alone.
3. **Merging.** Forks landed on `w16-ld` with `git merge --no-ff`.
4. **Lane edit.** The lane added one commit of its own, the renamer fix (§3.6).
5. **Composition check.** After each merge wave, an in-tree build confirmed that the fork deltas compose (§5).
6. **Rebase** onto `d183547ca` with `--rebase-merges`.

Rows whose only residue was register allocation or scheduling were skipped after about three spellings each. The
permuter was not run.

## 3. Defects fixed

### 3.1 Behaviour fixes (retail does something our source did not)

- **`VocalTrack::UpdateScrolling`:** the static-lyrics early break compares `(phStartMs - mMinPhraseHighlightMs)` against `ms`.
  - Retail copies `ms` into f17 on entry and compares against f17; our source compared against `0.0f`.
  - objdiff scored that instruction "equal", because both sides read `f17`. The defect only surfaced through the
    register assignment around it.
  - Fixing it restored retail's floating-point register assignment across the whole function. The f16/f17 swap that
    W16-GB described as pervasive is gone.
- **`VocalPlayer::HandlePhraseEnd`:** the third argument of `send_score_phrase` is `bSpotlightPhraseHit`, which retail
  passes in r26, masked into the DataNode. Our source sent `iPrevActivePartCount`. The receiver,
  `RemoteScorePhrase(int, int, bool)`, already treats that argument as the spotlight flag.
- **`PracticePanel::Poll`:** only the metronome poll is guarded by `mMetronome`. Retail's `mMetronome == 0` branch
  lands on `GetSectionBounds`. Our source had also nested the section bounds, the controller reset and
  end_play/loop under the guard.
- **`StoreMainPanel::ParseConfigData`:** `mCurrentEntry` and `mTimeNextEvent` are reset only when a content list was
  parsed. Retail's null-array and short-array branches land on `Release`.
- **`PlayerLeaderboard::OnSelectRow`:** when `ShowGamercard` returns NotSignedIn (−3), retail returns `pad_error`, not
  `gamertag_error`. The stale comment that claimed otherwise is rewritten.
- **`GameplayOptions::SaveSize`:** it used `TheDebug << MakeString`. It now uses `MILO_LOG`, like every other
  `SaveSize`, and compiles to retail's `li r3,9; blr`.

### 3.2 `VocalTrack::UpdateScrolling` (8,948 B): 95.274 → 96.971 fuzzy, 95.936 → 97.355 mpn

This lane opened it as the brief asked. The brief called it never opened, but W16-FS, W16-CW, W16-CY and W16-GB had
all worked it; W16-ID, W16-JC and W16-KD had left it alone.

**Fixes.** Besides the behaviour fix in §3.1, these code-shape fixes were each confirmed on retail bytes (fork 1, 14 commits):
- the part-2 select picks the `mLyricPhrases` reference, not the note list;
- the lyric-phrase cursor is compared unsigned (`*curPhPtr == size()`);
- the next-zone bound is written `size() > idx + 1`;
- the coda split builds both halves in its own arm, and the function tail now matches retail instruction for instruction;
- `plate->mSyllables` is bound before the `mBaked` store, which restored size identity;
- the freestyle sweeps go through a `mFreestyleSections` reference;
- the same TU's `BuildScrollingDeployZones` rose from 98.120 to 99.935.

**Own measurement.** `ab_measure` on fork 1's diff alone, against `w16-ld` at `9d627e235`:

```
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.001273pp   (legA 58.721867 -> legB 58.723140)
```

The result is +0 functions and +0 bytes because the row is still below 100; `matched_code` is all-or-nothing per row.
A row diff of the archived legs shows 2 rows up, 0 down and 0 rows off 100. The run dir is
`~/tmp/wt-w16-ld-1ab/.ab_measure_runs/20261001-153146-f1_ab-2957593/`.

**What is left, and why fork 1 stopped.** 32 insert/delete pairs and 166 register charges remain:
- **Deque `size()` scheduling, about 60% of the residue:**
  - the order of the `_M_start` copy stores;
  - the order of the `/24` terms;
  - the loop-bottom tail merge.

  The same STLport expansion matches 100% in `ClearLyrics` and `UpdateTubePlates` in this TU, so the STLport spelling
  is not the defect. Editing `_deque.h` would change every deque user.
- Loop-increment placement.
- The copy and store order of a `Vector3` and of the plate colours.
- The `SetPointPos` argument stores.
- A swap between r29 and r30 in the trailing deploy loop. Of three spellings tried, one was inert and one cost about 13 pp.

**Reverted negatives:**
- `GetVocalNoteList` as a member function cost about 0.8 pp, and it took RebuildHUD and PrepareNoteTubes off mpn 100.
- Indexing `mNextDeployZone` directly cost about 13 pp.

### 3.3 Meta rows (forks 2, 3 and 7)

| row | B | fuzzy before → after | source construct |
|---|---:|---|---|
| `CustomizePanel::PreviewAsset` | 1,956 | 99.59 → **100** | `mProfileAssets` bound to a reference before `SetOld` |
| `CustomizePanel::SelectAsset` | 844 | 98.67 → **100** | the same, at `HasAsset` |
| `PracticePanel::Poll` | 1,656 | 99.87 → **100** | §3.1 |
| `MetaPerformer::TriggerSongCompletion` | 1,052 | 98.33 → **100** | `GetSlot()` held in a named local |
| `SetlistMergePanel::OnMsg(ReleasingLockStepMsg)` | 708 | 93.67 → 99.77, mpn **100** | `mSetlists[i].first` indexed at each use |
| `AssetProvider::Update` | 696 | 98.45 → **100** | named reference to the asset map (§4.1) |
| `MetaPerformer::RecordBattleScore` / `PopulatePlayerScores` / `PopulatePlayerBandScores` | 552 / 264 / 248 | 98.19 / 96.36 / 96.21 → **100** | `GetSoloProfile` reads `.first` through a named reference (one inline, three callers) |
| `StoreMainPanel::ParseConfigData` | 476 | 98.24 → **100** | §3.1 |
| `MetaPerformer::SyncSave` | 380 | 98.74 → **100** | gNullStr written through an implicit String temporary |
| `AccomplishmentManager::ConfigurePrecachedFilterData` | 320 | 98.00 → **100** | `GetPrecachedFilter(Symbol(key))`: retail copies `key` to its own argument slot |
| `OvershellPanel::EnableAutoVocals` | 252 | 95.84 → **100** | named `mSlots` reference (§4.1) |
| `MusicLibraryUpsellViewSetting::Text` / `GetCurrentStatus` | 212 / 160 | 93.59 / 97.38 → **100** | one `SetTextToken` / `Localize` call per arm |
| `ClosetPanel::CycleCamera` and its five 40 B funclets | 480 | 97.10 → 98.97 (funclets 99.9 → **100**) | `String &last = substrs.back()` (§4.1) |
| `BandSongMgr::RemoveOldestCachedContent` | 624 | 96.51 → 98.72 | per-iteration `age` local; inner vector by reference |
| `PlayerLeaderboard::OnSelectRow` | 436 | 99.17 → 99.82 | §3.1 |

Forks 2 and 3 independently made the identical `ConfigurePrecachedFilterData` change. The merge kept one copy, with the comment.

### 3.4 Game rows (forks 4 and 5)

| row | B | fuzzy before → after | source construct |
|---|---:|---|---|
| `VocalPlayer::HandlePhraseEnd` | 2,332 | 98.51 → 99.991 | §3.1 plus a `const bool` local. The last charge is a CYCLE-family fold. |
| `PerfectOverdriveTracker::Poll_` | 1,248 | 99.60 → 99.94, mpn **100** | per-player flags are `const bool` |
| `TrainerPanel::InternalInitSections` | 964 | 95.56 → **100** | tokens read as `ev.Msg()->Sym(n)`, which stops a per-call spill of the array pointer |
| `Singer::Poll_` | 804 | 97.32 → 99.95, mpn **100** | energy assigned in every arm; magnitude loaded before `sin` |
| `DeployCountTracker::Poll_` | 780 | 96.72 → **100** | `const bool` local. W16-JC had recorded this row as register residue. |
| `PerfectSectionTracker::Poll_` | 636 | 96.60 → **100** | named `static const float kZero` (§4.3) |
| `StreakFocusTracker::CheckCondition` (and funclet `fn_826D7564`) | 540 | 99.24 → **100** | `const bool` |
| `GemPlayer::UpdateGameCymbalLanes` | 480 | 98.00 → 99.58, mpn **100** | the GH-drums result keeps its own flag, copied after `GetUsingRealDrums()` |
| `TambourineDetector::CheckForSwing` | 264 | 94.17 → **100** | `const bool` |
| `VocalPart::GetNoteRange` | 196 | 94.49 → **100** | the note vector through one reference |
| `TrackerManager::CreateSource` | 176 | 97.73 → **100** | `pUser` declared after the first `return new` (§4.1) |
| `VocalNote::PitchAt` | 144 | 96.25 → **100** | `Max(0, Min(end, ms) - mMs)` (in `beatmatch/VocalNote.h`, inline only) |
| `GemManager::DisableSlot` / `EnableSlot` | 100 / 84 | 93.60 / 92.38 → **100** | `std::find` run in place; retail enters the loop at its bottom test |
| `VocalPlayer::Poll` | 3,388 | 95.025 → **94.985** | `bSolo` is `size()==1`; the pitch-correction flag starts false. Kept because both sites now carry retail's instructions. |

### 3.5 Relocation-name rows (fork 6): folds, map repairs, re-homes

**21 alias memberships added.** On the rebased tip, `tools/icf_pair_adjudicate.py --chase --pairs` over every pair the
branch adds (extracted against main's `symbol_aliases.json`) gives **21 / 21 CHASED T1 PROVEN, none with CYCLE text in
its block** (`~/tmp/w16ld/chase_final.log`). Highlights:
- `list<T*>::insert` twins → 0x823d14c0, 0x8266ad18, 0x823d6fa8;
- `hash_map` default ctors → 0x8255d480;
- `_M_initialize_buckets` → 0x8256ad18;
- `__find<int*>` / `__find<TrackType*>` → 0x823e1060. The leaf is RETAIL-TAIL-PAD; the placeholder-slot audit reads
  NOT-ALIGNED because of the extra zero word, and the body has no relocations.
- `GameplayOptions::SaveSize` → 0x82654ab8, VACUOUS-BUT-IDENTICAL, with a `bl` witness and a `lis`/`addi` witness.

**11 memberships removed or re-keyed**, each with a record:
- **Re-keyed (6).** The former folded spelling is now the map name at that address, and the group's evidence says why:
  NetLoaderRef `insert`, the `vector<SingerStats>` helpers, two hash_map ctors, and two PrefabChar heap helpers.
- **Withdrawn (2).** Two sized-ctor memberships came out of 0x825a07e0, which retail shows is a copy ctor.
  - This fixed a validator contradiction introduced by fork 6's own commit `faa3b73fa`. That commit went in with the
    validator failing; `cf5da5491` is the fix, and both are kept as history.
  - 24 more sized-ctor memberships in the same group have the same defect but forgive nothing measurable today, so
    they were left with a note.
- **Moved to the address retail's callers use (3).** MeshAO `_M_fill_insert`, the StreakList `_Param_Construct`, and
  MultiplayerAnalyzer `Data`.

**Map repairs and re-homes:**
- **PrefabMgr sort helpers.** Six rows renamed from `EventEntry`/`ObjEntry` to `PrefabChar`, decided by the retail
  callees and the call from `LoadPortraits`.
- **Stats `vector` helpers.** Renamed from `vector<RhythmDetector::Frame>` to `vector<SingerStats>`, and 0x82657660
  re-homed from RhythmDetector to Stats.cpp.
- **`hash_map` ctors.** 0x825af9e8 / 0x825af728 are the outer `hash_map<Symbol,hash_map<…>>` ctor and its hashtable
  ctor; 0x8256b6e8 / 0x8256b298 are `<int,String>`.
- **`_Param_Construct<MultiplayerAnalyzer::Data>`.** 0x826ce268, re-homed from Leaderboard to MultiplayerAnalyzer.
- **`list<NetLoaderRef>::insert`** at 0x827cdd28.
- **`??_GLeafSortNode`** at 0x82675900, proved through the retail vtable's RTTI and re-homed from Game to StoreSongSortNode.cpp.

Forks 3 and 6 each re-admitted `list<CharData*>::insert` into the 0x823d14c0 group: fork 3 under `restored[]`, fork 6
under `admitted[]`. The membership appears once; both records stay, since both are accurate.

### 3.6 Lane edit: the target-symbol renamer gave stale labels a function's name

Forks 2 and 6 independently found that `scripts/obj_target_symbol_renamer.py` renames by the hex in a symbol's **name**,
not by its address.
- **The mechanism.** `symbols.txt` carries 18,335 `.text` `lbl_<X>` labels that sit at an address other than X.
  - Example: `lbl_825F8AA8 = .text:0x82615368` is `CustomizePanel::GetWearing`'s switch base. It is materialised by
    `lis`/`addi` to 0x82615368 and `bctr`'d off.
  - 414 of those X's are map keys, so the label was renamed to an unrelated function, here
    `?RefreshHeader@AccomplishmentPanel@@QAAXXZ`.
  - `name_check` then charged the reference. An unrenamed `lbl_` placeholder would have been forgiven.
- **The fix** (`e9f5af448`). The renamer reads `symbols.txt` (`--symbols`) and drops the rename for any `lbl_` whose
  name hex disagrees with its address. `fn_` names, and `lbl_` names at their own address (dtk's frameless entry
  points), are unaffected.
- **Check.** The dry run reports `18335 … not at their named address; 414 … left as placeholders`, and
  `scripts/test_obj_target_symbol_renamer_refusal.py` passes.
- **Measured** in-tree, after a forced re-split (93,084 renames applied) and a full build, against the pre-change merged
  state: **1 row up (GetWearing 99.870 → 100, +308 B), 0 down, 0 off 100.**

**The "414" overstated the reach.** Only one of those labels was a charged relocation target in a row whose score
could move. The fix is landed on its merit, not its size.

## 4. Levers found (reusable)

### 4.1 The "dead store to the 0x50 cleanup slot" is a named reference local

W16-ID §4 listed this as unexplained. It is the stack home of a **named reference** local, in one of three shapes:
- a container or member reference declared before a local with a destructor;
- a reference inside an inlined helper (`GetSoloProfile`);
- a pointer local declared after the first `return new …` branch, which shares the `new`-cleanup slot (`CreateSource`).

It closed `AssetProvider::Update`, three `MetaPerformer` callers, `EnableAutoVocals`, `CreateSource` and `CycleCamera`'s funclets.

It did **not** close `CharacterCreatorPanel::RandomizeFace`:
- Retail homes both `&outfit` (desc+0x70) and `&head` (desc+0x18) to 0x50; ours homes only `&head`.
- A single-use `outfit` reference never gets a home, whatever the declaration order and even when unused.
- A diagnostic build confirmed that the multi-use `head` reference is what produces the store we do have.

### 4.2 `const bool` locals

**The tell:** retail writes `li rN,1` or `li rN,0` straight into a callee-saved register and tests it unmasked, or has no
`stb` home store. Ours has `clrlwi rN,rM,24` and/or an extra `stb`.

**Rows it closed:** `CheckCondition`, `CheckForSwing`, `DeployCountTracker::Poll_`, `PerfectOverdriveTracker::Poll_` (to mpn 100), and the `HandlePhraseEnd` site.

**Limits:**
- It is not blanket. In `PerfectOverdriveTracker`, `endStreak` must stay a plain `bool`, because retail re-masks it.
- It was inert on `VocalPlayer::Poll`, `VocalPart::GetBestHit` and `Singer::Poll_`.

### 4.3 A named `static const float kZero` outside loops

Retail keeps the pool address in a GPR and reloads the constant at each compare, saving no FPR. A bare `0.0f` is
hoisted into f31 with a `stfd`/`lfd` pair. This is W16-ID §3.2's loop case, and it also applies to straight-line code:
`PerfectSectionTracker::Poll_` 96.60 → 100.

## 5. How the deltas compose (in-tree full builds against base `9d627e235`)

| state | Δ functions | Δ bytes |
|---|---:|---:|
| forks 1+5+2+6 merged | +53 | +14,708 (= 4+6+43 fns, 1,148+5,252+8,308 B) |
| + fork 3 | +61 | +18,312 (overlap: `ConfigurePrecachedFilterData` 1 fn / 320 B, and the CharSync membership) |
| + renamer fix | +61 | +18,620 |
| + forks 7 and 4 (pre-rebase) | **+77** | **+21,848** (overlap: `HandleMicsChanged` 1 fn / 300 B, closed by both fork 4 and fork 6) |

Each merge wave reproduced the sum of its forks, less exactly the rows two forks fixed, so no fork's gain was lost to
another fork's edits.

## 6. Rebase onto `d183547ca` (W16-LC)

There were 84 commits. The only conflicts were in fork 3's merge, the same two hunks resolved at the original merge:
`symbol_aliases.json` (the duplicate CharData membership) and `AccomplishmentManager.cpp` (the duplicate
`ConfigurePrecachedFilterData` fix). Both were resolved to the first-parent side.

`git diff <pre-rebase tip> <rebased tip>` is exactly main's 20 files, `+635/−295`, the same stat as
`9d627e235..d183547ca`, so no lane change was lost. A full build of the rebased tip leaves `symbols.txt` unchanged.

## 7. Gates (rebased tip, after the A/B)

```
[map-injectivity] OK: 32907 applied rows, 32906 distinct names, injective (+1 enumerated internal-linkage exception(s))
VALIDATE: PASS -- 1650 map-consistent, 271 tolerated (enumerated above), 0 contradicted, 1922 total
[patch-state] OK: tree is a fixed point of 6 post-compile passes
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

- The native gate ran last, on the final code.
- Only this docs-only commit follows it, and it touches no build input.
- No added source comment or commit message cites rb3-Wii or "the oracle", and none carries a Co-Authored-By line.
  This was grepped over `d183547ca..w16-ld`.

## 8. Rows left, by blocker

- **Register allocation or scheduling only, about three spellings each:**
  - VocalTrack: UpdateScrolling's residue (§3.2), ProcessStaticLyrics, UpdatePitchArrow.
  - Big rows: GemManager::SetupGems, RockCentral::RecordAccomplishmentData, ChordbookPanel::DisplayChord / SetFret,
    GemPlayer::Hit, Game::OnMsg(ButtonDownMsg). The last four differ only in the operand order of `fadds` / `and` / `or`,
    and swapping the source operands is inert.
  - StoreOfferProvider::BuildList, UIStats::MaybePublish (W16-EI), SaveLoadManager::SetState,
    MusicLibraryNetSetlists::ParseDataResultsIntoSetlists, Tail::UpdateVerts.
  - The CharacterCreatorPanel `Set*` family, and most of the 64-row register-only class.
- **Unresolved code shape:**
  - `CustomizePanel::Handle`: one `clrlwi`, with more than 18 spellings recorded in the source.
  - `NextSongPanel::FillExpandedDetails`: a dead `stfs f0,0x88`, which looks like an inlined by-value float home.
  - `MusicLibrary::Text`: retail merges the two "song" arms and keeps the kNodeSong copy.
  - `DoesOfferMatchFilter` / `DoesSongMatchFilter`: the bool is normalised twice.
  - `SaveAndUploadScores`: a signed `srawi.` size test.
  - `TrackPanel::Poll`: `&TheGame->mProperties` is spilled.
  - `DrawTrackMasks`: an identity `clrrwi`.
  - `IsEmptyPhrase`: an identity `clrrwi`.
  - `Gem::Poll`, `DrawBeatLine`, `GetLane@TrainerGemTab`, `IsEndOfFill`.
  - `RandomizeFace` (§4.1).
- **Fold names with a CYCLE-ASSUMED leaf. These were refused; chase is PROVEN but not admissible:**
  - the `_M_fill_insert<Object*>` family, `_Rb_tree::clear` (Color, Symbol and TrackType), `sort` / `__introsort_loop`,
    `_M_copy` and `_Rb_tree::operator=`, which together cover 29 rows / 6,820 B;
  - `sort<VocalPart*>`, the last charge on `HandlePhraseEnd`.
- **Chase REFUTED (16 rows / 2,428 B):**
  - MusicLibraryStore `push_back`, as W16-JC also found;
  - `DeltaArray` called from `vector<TrackerPlayerDisplay>::resize`;
  - `__uninitialized_copy<pair<float,float>>`;
  - InterstitialMgr `resize` / `insert_after`, AssetMgr `_M_insert`, LicenseMgr `insert_after`;
  - the `??_ETrackPanelInterface` `$4` thunk.
- **Wrong map names found but not renamed.** Naming has no byte upside, and it would put forgiven call sites at risk.
  - 0x82b90740 is mapped as `__ucopy_aux<Track*>`. Its body is `b ~UIPanel`, and its callers are TrackPanel's ctor and
    dtor. It looks like `~TrackPanelInterface`.
  - 0x82b9ada0 is mapped as `__ucopy_ptrs<Tail>`. Its body is `b _List_base<HitGem>::clear`.
  - 0x8235fa58 / 0x8235fba8 have their `vector<set<Symbol>>` / `vector<map<int,float>>` names swapped (192 B). Fixing
    it needs CharClip, which another lane owns.
  - `GetVocalPartPercentage` (16 B): the retail body indexes a vector at `this+0x8`, so the name belongs to another
    class's accessor.
- **Layout:**
  - `TrackerSectionManager::GatherSections` needs `Section`'s ticks to be an 8-byte sub-object assigned as a unit.
    Scratch compiles confirmed the `ld`/`std` shape.
  - `FocusTracker::Poll_` has a 16-byte dead frame slot.
- **Structurally unmatchable against this image:** `BandSongMgr::AddSongData`, `IsDemo` (the RB3DX patch set).
- **Possibly dead.** Two scatter-includes existed only to pair the wrong names fixed in §3.5: `MessageTimer.cpp` in
  `PrefabMgr.cpp`, and `MusicLibraryNetSetlists.cpp` in `DataPointMgr.cpp`. They were left in place.

## 9. Not done

- **Not merged to main**, per the brief.
- No `symbols.txt` edits and no PCH-input edits. The only shared engine header touched is `beatmatch/VocalNote.h`, an
  inline-only change; the native gate covers it.
- The permuter was not run.
- The 24 remaining sized-ctor memberships in group 0x825a07e0 (§3.5) are recorded, not withdrawn.
