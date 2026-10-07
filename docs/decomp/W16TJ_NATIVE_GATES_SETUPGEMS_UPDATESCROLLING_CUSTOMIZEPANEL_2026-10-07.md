# W16-TJ: SetupGems, UpdateScrolling and CustomizePanel::Handle run natively, on real data (2026-10-07)

Branch `w16-tj`, rebased on main `d5cbd3cbc` (W16-SY, dtk 1.15.0) before any X360
measurement. Not merged or pushed.

## 0. Headline

W16-TF (`W16TF_NATIVE_RUNTIME_COVERAGE_2026-10-07.md` §6) left the three largest
in-scope gap rows unexecuted by any native target, because each needs a populated track
or panel. This lane runs all three in `rb3-render` on shipped data and checks each one
against a reference computed without the code under test:

| row | B | fuzzy | entered (instrumented run) | gates |
|---|---:|---:|---:|---|
| `VocalTrack::UpdateScrolling` | 8,948 | 97.35 | 8,355 calls | 5 `us-*` |
| `CustomizePanel::Handle` | 5,036 | 99.92 | 562 calls | 9 `cp-*` |
| `GemManager::SetupGems` | 3,404 | 98.68 | 3 calls | 6 `sg-*` |

Four `tj-*` fixture gates come first. Every gate is written so that a wrong fixture
fails it before a gate on the function under test can read it.

- **Behaviour bug fixed (native-only):** `UpdateScrolling` aborted natively with
  `vector<VocalNote>::operator[]: Assertion '__n < this->size()'`. Its note cursors
  legitimately sit one past the last note, and the code formed `&v[cursor]` to get an
  end pointer. Under `HX_NATIVE` the same pointers now come from `data() + cursor`
  (`src/band3/bandtrack/VocalTrack.cpp`). The X360 code is unchanged. Reverting the fix
  reproduces the abort (§4, S2).
- **No gate found a behaviour difference** in the three rows on these inputs. This
  matches W16-PK and W16-TF: executed sub-100 rows have so far behaved like retail.
- **X360 A/B: Δ0 on every key**, as predicted (§5).
- `native_health.sh`: `gates_pass` 125 → **149** (+24, exactly this lane's gates), 0
  failures, 18/18 linked and run.

## 1. The reference

All expected values come from the shipped data, read by code in
`native/src/w16tj_phase.cpp` that shares nothing with the code under test:

- **Song fixture** (`antibodies`). `MiniSmf` is this file's own SMF reader: chunk walk,
  running status, tempo map from `FF 51`, and note on/off pairing. It reads the song's
  `.mid` straight from the ark. `songs/songs.dta` is read directly. The engine's
  `SongParser` → `SongDB` path builds the fixture the functions consume. The `sg-fixture`
  and `us-phrase-table` gates check that path against `MiniSmf` before anything else
  reads it.
- **Customize panel.** The shipped `ui/customize/customize.dta` and the English locale
  `ui/locale/eng/locale_keep.dta` are read as data with `DataReadFile`. The state
  numbers come from the `kCustomizeState_*` macros in `config/macros.dta`.

## 2. Gates

The phase runs after W16-TF's in `rb3-render`'s default mode, which `native_health.sh`
uses. `--no-w16tj` turns it off.

| gate | reference | what it checks |
|---|---|---|
| tj-config | shipped beatmatcher/scoring/player config spliced in | fixture |
| tj-songmgr | `songs.dta` song id 1080 | fixture: real `BandSongMgr` + `SongUpgradeMgr` |
| tj-smf | `MiniSmf` parse: 16 tracks, ppq 480, 87 tempo events | fixture |
| tj-parse | `SongParser` track numbers; hopo threshold 170 from `beatmatcher.dta` | fixture |
| sg-fixture | `SongDB` guitar gems vs `MiniSmf` expert gems (1,373, 0 differ) | fixture |
| sg-count | one `Gem` per `GameGem`, each bound to its own | SetupGems |
| sg-times | start/end seconds from `MiniSmf`'s tempo map (worst 1.00 ms) | SetupGems |
| sg-lanes | lane masks from the MIDI pitches | SetupGems |
| sg-hopo | the MIDI HOPO rule: force-on/off markers at 101/102 over `[start,end)`, else single lane, no shared lane with the previous gem, within the threshold; the first gem is never a HOPO. 99 HOPOs | SetupGems |
| sg-start-tick | `SetupGems(165120)`: gems before the cut emptied, the rest kept | SetupGems |
| us-phrase-table | lead-in, every charted phrase end, rest-break phrases hold no sung notes, tambourine flags vs pitch 96 (106/106) | fixture (VocalNoteList) |
| us-scroll-cursor | `MiniSmf` sung notes 36–84 plus `+` glides, counted to the look-ahead horizon, cursor carried through abutting same-pitch tubes | UpdateScrolling, 140 checkpoints at 30 fps |
| us-beat-cursor | BEAT-track beats (pitches 12/13) to the horizon, inside the BEAT grid | UpdateScrolling |
| us-markers | live phrase/beat/downbeat marker meshes, classified by owner template, vs the validated phrase table and the beats inside charted tambourine phrases | UpdateScrolling (1,196 beat, 104 phrase sightings) |
| us-tambourine | tambourine gems shown in `[ms−1000, look)` vs pitch 96, plus the gem cursor | UpdateScrolling (403 sightings) |
| cp-fixture | `customize.milo` loads as a PanelDir; the script's 36 `set_focus_component` rows all resolve by walking the dir (not `Find`) | fixture |
| cp-focus | for all 37 states, `set_state` then `get_focus_component` = the script's table (state 0 = none); `get_state` round trip | Handle |
| cp-focus-store | for each state, `set_focus X` (UIPanel arm), `store_focus_component`, then `get_focus_component` = X | Handle |
| cp-clothing | `in_clothing_state` over 37 states = the `update_state` cases that fire `browse_clothing.trg`, among states the script enters | Handle |
| cp-boutique | `set_current_boutique`/`get_current_boutique` over the `BOUTIQUES` macro; after `clear_current_boutique`, no boutique | Handle |
| cp-patch-return | the script's patch-entry sequence (`set_patch_menu_return_state R`, `set_state PatchMenu`), then `leave_state` → R, for 25 return states | Handle |
| cp-waiting | `is_waiting_to_leave` follows its setter; a cancel `ButtonDownMsg` while waiting is handled and leaves the state alone | Handle |
| cp-makeup | a real `MakeupProvider` per gender: `update_makeup_provider` + `makeup_provider` return the panel's provider, which lists `none_makeup` and then exactly the locale's `<gender>_makeup_<eyes\|lips>_N` keys (29/17 female, 16/10 male) | Handle |
| cp-super | `loaded_dir` (UIPanel arm) = the loaded dir; `set`/`get pending_state` through the Object property path | Handle |

### 2.1 Predictions against measurement (reference corrections)

Each correction below came from a first-run disagreement. Each was traced to the chart
or script semantics, not fitted to the code's output:

- **Phrase 0.** The first reference skipped MIDI phrase 0. The code skips the phrase
  table's lead-in phrase, so the reference now does too.
- **Glides.** `+` lyrics add bend notes, so they count as scrolled-in segments.
- **Tubes.** `PrepareNoteTubes(…, int &endNote, …)` carries the cursor through abutting
  same-pitch segments.
- **Rest breaks.** The parser inserts "rest break" phrases inside long gaps.
  `us-phrase-table` checks them (they hold no sung note), and marker expectations come
  from the checked table.
- **Beats before a tambourine phrase** get no line (`WantBeatLines` covers only charted
  tambourine phrases). The beat cursor is checked only while the BEAT grid covers the
  horizon, because the BeatMap extrapolates past the last BEAT note.
- **HOPO.** The first `sg-hopo` passed the parser's own flag through, so it was not
  independent. It was replaced by the MIDI rule before any result was recorded: 53 HOPOs
  and 33 force markers on `bohemianrhapsody` while the fixture used it, then 99 HOPOs on
  `antibodies`, 0 disagreeing both times.
- **Clothing.** The first reference was "every `update_state` case that fires
  `browse_clothing.trg`". That gave 4 states, including `BrowseTshirts`. No shipped
  script ever sets `BrowseTshirts`: `boutique_tshirts.btn` enters `BrowseTorso`. The
  case is dead, so the reference now keeps only the states the script enters. The gate
  prints the dead case count (1). This is a choice about the reference, not evidence
  about the code: retail's `IsClothingState` is byte-matched (RESIDUAL-2) to the range
  `BrowseTorso..BrowseFeet`, which agrees.

## 3. Link and load closure

`rb3-render` links with `--gc-sections`. Constructing these objects pulled in vtable
closures that had never been resolved.

- **Real TUs added** (rb3-render only): `W16TJ_LINK_SOURCES` (26 TUs: ClosetMgr, the
  asset providers, CharCache, UIEventMgr, MetaPanel, PrefabMgr, SongMgr,
  DataArraySongInfo, Jukebox, LicenseMgr, …) and `W16TJ_VOCAL_SOURCES` (FxSendSynapse,
  FxSendDelay, SlipTrack). They use the LOCAL_STATIC defs, as do VocalPlayer, Player,
  Performer, Band and UIList. New `src/band3/bandtrack/GraphicsUtl.cpp` holds
  `UnhookGroupParents`/`UnhookAllParents`. It is not in `objects.json`, and no retail
  address has been identified for it.
- **Loud `UNREACHED()` stubs** in `native/src/bandtrack_link_stubs.cpp` cover:
  - 12 VocalOverlay methods and 11 TrackerManager methods. `TrackerManager.cpp` would
    pull the whole Tracker family.
  - NetSession `IsLocal`/`IsInGame`/`EndGame`/`IsBusy`.
  - RGTrainerPanel `GetLegendMode` and RealGuitarGemPlayer `GetRGState`.
  - Synth `Play`, RockCentral `SyncAvailableSongs`/`FailAllOutstandingCalls`,
    XBackgroundDownloadSetMode, XMarketplaceGetDownloadStatus and the AssetOffer ctor.
  - `GestureMgr::SetInVoiceMode`, reached from UIScreen's native Exit.
  - Storage: `TheContentMgr` bound to a static base `ContentMgr` (it was a null
    reference, and `RefreshInProgress` faulted), `TheTour`, `TheRGTrainerPanel`,
    `TheGestureMgr`, `frame_rate` and `gShowAssetName`.
- **Native-only source changes** (all `#ifdef HX_NATIVE` / `#ifndef HX_NATIVE`; X360
  code unchanged, confirmed by §5):
  - `VocalTrack.cpp`: the bug fix above.
  - `UIList::Update`: a missing type resource (`mResource` null, because no
    `UIManager::InitResources` runs) reads as a missing list dir. The native path
    already tolerated a missing list dir. MEASURED: without this, `BandList::PostLoad`
    faults in `UIResource::Dir`.
  - `UIListMesh.cpp`: the declared-only `ObjPtr<RndMat>` ctor specialization (which
    forces retail's out-of-line call) is now X360-only. Natively nothing defines it.
  - `GemRepTemplate.cpp`/`CharCache.cpp`: native-only debug hooks were removed. They
    named `RB3StompLabelVV` and `gNativeStartLoadTag`, which nothing in the tree or the
    engine defines; they only linked because no target called them.
  - `PrefabMgr.cpp`: the MessageTimer/Text scatter-includes, which duplicated
    standalone TUs natively.
  - `PerformanceData.cpp`: `MetaPanel::sIsPlaytest` (MetaPanel.cpp now links).
  - `BandSongMgr.cpp`: local statics.
- **Driver-side fixture:**
  - The phase builds a real `SongUpgradeMgr` and a calloc'd `BandUserMgr` slot map.
  - It loads `ui/track/gen/trackpanel.milo` and runs `SetupSmasherPlate` after
    `SetInstrument`. MEASURED: without this, NowBar asserts `smasherPlateDir`.
  - It builds a real `VocalPlayer`, Band and Scoring. A `CrowdRating` was dropped: its
    ctor needs `TheScoring` before one exists.
  - It registers the PanelDir/UITrigger/BandList factories. InlineHelp stays
    unregistered: MEASURED, its PostLoad faults reading a type resource this driver never
    preloads, so the loader skips the 7 help bars.
  - It installs a zeroed, never-`Init()`ed `UIManager` as `TheUI` while focus is set,
    because `BandButton::SetState` asks `TheUI->InTransition()`.

## 4. Sabotage controls (predictions written before each run, all reverted)

- **S1**, three defects in one build:
  - `SetupGems` inverts `isHopo`.
  - `UpdateScrolling` swaps the downbeat and beat meshes.
  - `IsClothingState` uses `<` instead of `<=` for `BrowseFeet`.

  Predicted: exactly `sg-hopo`, `us-markers` and `cp-clothing` fail; `cp-clothing` has
  1 wrong state; every other gate passes. **Measured exactly that:**
  `RESULT: FAILED (3 gate failure(s))`. `sg-hopo` had 1,372 disagreeing (every gem
  except the first), `us-markers` 54 wrong checkpoints, `cp-clothing` 1 wrong.
- **S2**, the VocalTrack fix reverted. Predicted: a libstdc++ assertion abort during the
  `us-*` sweep, after `us-phrase-table`. **Measured:** rc=134, `stl_vector.h:1253 …
  Assertion '__n < this->size()' failed`, right after `[PASS] us-phrase-table`.

## 5. X360 A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16tj --patch <reverse src diff>
--jobs 8`. Leg A is this branch; leg B is main's `src/` (all 9 `src/` paths). The
prediction was Δ0, because every X360-visible change is inside a native-only guard, and
`GraphicsUtl.cpp` is not compiled by the X360 build. Leg B recompiled 11 TUs.

```
leg A: matched=54914 masked=25214 honest=29700 code%=59.292706  (recompiles: 0, settled)
leg B: matched=54914 masked=25214 honest=29700 code%=59.292706  (recompiles: 11, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   (legA 64.299890 -> legB 64.299890)
units at 100% [mpn ruler]: legA 621 -> legB 621  (Δ+0)
```

The `none` control was also +0 B. The tree was restored by the tool, verified.

## 6. Coverage after the gates (`native_runtime_rank.py`, this worktree)

| ring | gap rows | gap B | executed (this lane) | W16-TF "after gates" |
|---|---:|---:|---:|---:|
| **IN-SCOPE** | 550 | 201,792 | **87 / 68,168** | 74 / 43,716 |
| IN-CORE | 133 | 43,888 | 39 / 22,044 | 39 / 22,044 |
| IN-SOON | 273 | 99,808 | 33 / 31,888 | 22 / 11,012 |
| IN-RB3ENG | 144 | 58,096 | 15 / 14,236 | 13 / 10,660 |
| VIA-DC3 | 459 | 193,648 | 32 / 22,284 | 30 / 21,524 |

⚠ The two columns are measured against different `report.json`s: W16-TF's was at main
`e0cca0a7f`, this one at `d5cbd3cbc`, and the gap denominators moved between them (551 →
550 rows in-scope). Read the change, **+13 rows / +24,452 B in-scope**, as approximate.

Beyond the three target rows, `rb3-render` now also runs these rows, all inside
W16-TF's "linked but never entered" VocalTrack.cpp (11,532 B) and GemManager.cpp
(5,404 B) totals:
- `VocalTrack`: `PrepareNoteTubes` 1,160, `BuildScrollingDeployZones` 368,
  `ProcessStaticLyrics` 312.
- `GemManager`: ctor 896, `SetupRealGuitarAreaStrumSections` 344,
  `UpdateSlotPositions` 232.
- `VocalTrackDir::SetRange` 700.

None of these has its own gate. They run only as callees of the gated rows.

## 7. Result lines

```
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=149 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=15 scatter_dirb=0 scatter_multihost=14 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
```

`scatter_unlinked` went 16 → 15 and `scatter_multihost` 15 → 14 against W16-TF's line.
That fits the PrefabMgr scatter-includes being turned off natively, but it was not
attributed file by file.

The native build gate line is in the lane's final report. The gate was run as the last
action, after this doc was committed.

## 8. Not done

- **Six `Too many tube plates … dumping plates in deploy_mask_lead.mat`** warnings
  appear during the vocal sweep. They come from the native-only MILO_DEBUG warning path;
  not investigated.
- **SetupGems force-marker path.** The final fixture song (`antibodies`) has 0 HOPO force
  markers. That path was checked only while the fixture used `bohemianrhapsody` (33
  markers, 0 disagree). The phase does not gate both songs.
- **CustomizePanel arms that need a ClosetMgr, user, profile or preview desc are not
  sent.** This covers `LeaveState`'s closet arms, `ButtonDownMsg` past the waiting check,
  previews, patches, `get_current_makeup`, `has_license` and the other providers' update
  arms. The panel has no typedef, so `set_state` does not run the script's `update_state`
  handler (camera shots and triggers of the character screen).
- **UpdateScrolling's RangeShift lerp and lyric plates** run (ProcessStaticLyrics above)
  but have no gate.
- Only `bandtrack_link_stubs.cpp` holds the new stubs. A census with
  `native_stub_census.py --authored` was not rerun.

## Reproduce

```
cmake --build native/build --target rb3-render
native/build/rb3-render ~/code/milohax/rb3/orig-assets/xbox-zip <outdir>   # W16-TJ phase runs by default
tools/native_health.sh <worktree>
CMAKE_BUILD_PARALLEL_LEVEL=6 python3 tools/native_runtime_rank.py <worktree> --out rank.tsv --json rank.json
```
