# W16-UF — lever 2 finished: the 40 rows below W16-TW's #22 and TW's 5 deferred rows (2026-10-07)

Branch `w16-uf`, worktree `~/tmp/wt-w16uf`, based on main `a0d283292`.
Continues `docs/decomp/W16TW_UNENTERED_ROWS_SHIPPED_DATA_GATES_2026-10-07.md` (lever 2 of
`docs/decomp/CAMPAIGN_STATE_2026-10-07c.md` §6).

## 0. Headline

- **45 rows taken: all 45 have a gate or a recorded reason.**
  - 40 are TW's #23–#62, renumbered here as #1–#40.
  - 5 are TW's deferred rows, numbered #41–#45.
  - **14 are gated** on shipped data in rb3-render's default mode, through **24 gates**:
    10 fixture gates and 14 row gates.
  - **31 have a recorded reason** (§6).
- **Every row gate fails on broken code.** Three sabotage rounds were run, with predictions
  written to a file before each build:

  | round | defects | gates predicted to fail | gates that failed |
  |---|---:|---:|---:|
  | S1 | 7 | 8 | the same 8 |
  | S2 | 7 | 7 | the same 7 |
  | S3a | 1 | 1 | the same 1 |
  | S3b | 1 | 1 | the same 1 |

  No gate outside the prediction failed.
- **Two native bugs were found and fixed, both in `HX_NATIVE` arms.**
  1. `MsgSource` kept a deleted sink subscribed, and the next export called into freed
     memory (rc=139).
  2. `DeltaArray::CompressDelta` stored `0x82` for a NaN delta where retail stores `0`.
- **X360 A/B: Δ0 on every measure, as predicted.** Leg B recompiled 2 TUs.
- **Native health: PASS, `gates_pass=308 gates_fail=0`.** The native build gate was the
  lane's last action (§9).

## 1. Rows

`#` is this lane's number. `TW#` is the number in W16-TW's list, from W16-TS's ranking.
B and fuzzy are from `report.json` at the base commit.

| # | TW# | row | B | fuzzy | outcome |
|---:|---:|---|---:|---:|---|
| 1 | 23 | `LayerDir::GetBitmapList` | 692 | 99.65 | reason |
| 2 | 24 | `MetaPerformer::SaveAndUploadScores` | 684 | 99.42 | reason |
| 3 | 25 | `GemTrack::DrawBeatLine` | 684 | 95.85 | reason |
| 4 | 26 | `BandSongMgr::SyncSharedSongs` | 668 | 99.95 | reason |
| 5 | 27 | `SongData::UnflipGems` | 656 | 95.70 | reason |
| 6 | 28 | `BandCrowdMeter::Poll` | 652 | 97.70 | **gated** `uf-crowd-poll` |
| 7 | 29 | `MemHeap::Alloc` | 652 | 91.37 | reason |
| 8 | 30 | `PreInitSystem` | 652 | 99.98 | reason |
| 9 | 31 | `MemHeap::Print` | 624 | 98.01 | reason |
| 10 | 32 | `BandSongMgr::RemoveOldestCachedContent` | 624 | 98.72 | reason |
| 11 | 33 | `SongData::Load` | 612 | 98.82 | reason |
| 12 | 34 | `ImmediateWidgetImp::DrawInstances` | 600 | 95.65 | reason |
| 13 | 35 | `StoreMainPanel::FinishLoad` | 592 | 99.32 | **gated** `uf-storemain` |
| 14 | 36 | `DeltaArray::AppendDeltas` | 584 | 95.07 | **gated** `uf-deltas` |
| 15 | 37 | `BandTrack::SetCrowdRating` | 528 | 98.46 | **gated** `uf-crowd-rating` |
| 16 | 38 | `ClosetPanel::CycleCamera` | 480 | 99.98 | reason |
| 17 | 39 | `BandCamShot::SetPreFrame` | 476 | 98.32 | reason |
| 18 | 40 | `BandSongMgr::ReadCachedMetadataFromStream` | 400 | 99.78 | reason |
| 19 | 41 | `CharWidgetImp::RemoveInstances` | 396 | 95.84 | reason |
| 20 | 42 | `GemManager::CheckRemoveChordBracket` | 372 | 98.60 | reason |
| 21 | 43 | `AccomplishmentProvider::Mat` | 360 | 97.11 | reason |
| 22 | 44 | `BandWardrobe::OnEnterCloset` | 356 | 99.47 | **gated** `uf-enter-closet` |
| 23 | 45 | `TrackPanelDir::ConfigureCrowdMeter` | 336 | 98.01 | **gated** `uf-crowd-configure` |
| 24 | 46 | `BandTrack::SetupCrowdMeter` | 320 | 98.75 | **gated** `uf-crowd-setup` |
| 25 | 47 | `MemTracker::MemTracker` | 312 | 74.81 | reason |
| 26 | 48 | `KeyboardTrackWatcherImpl::CheckForFatFinger` | 300 | 95.73 | reason |
| 27 | 49 | `BandCharacter::StartLoad` | 292 | 97.81 | reason |
| 28 | 50 | `JoypadController::IsCymbal` | 264 | 99.85 | **gated** `uf-cymbal` |
| 29 | 51 | `BandCamShot::Target::operator=` | 248 | 99.60 | **gated** `uf-target-copy` |
| 30 | 52 | `InitSystem` | 228 | 99.89 | reason |
| 31 | 53 | `CharKeyHandMidi::FindPreferredFinger` | 228 | 92.37 | **gated** `uf-keyhand-finger` |
| 32 | 54 | `DataNode` map `operator[]` (STL instantiation) | 220 | 63.73 | reason |
| 33 | 55 | `MasterAudio::FillSwing` | 140 | 76.86 | reason |
| 34 | 56 | `JoypadClient::Poll` | 116 | 93.59 | **gated** `uf-repeat` |
| 35 | 57 | `BandCharacter::AddOverlays` | 112 | 92.46 | reason |
| 36 | 58 | `BandCharacter::OnClosetTeleport` | 112 | 99.21 | **gated** `uf-closet-teleport` |
| 37 | 59 | `MetaPerformer.cpp` template row (`…int>`, unidentified) | 100 | 0 | reason |
| 38 | 60 | `StopLog` (utl/MemTrack.cpp) | 92 | 99.78 | **gated** `uf-stoplog` |
| 39 | 61 | `SongDB::PostLoad` | 88 | 84.36 | reason |
| 40 | 62 | `Campaign::GetLaunchUser` | 68 | 82.06 | reason |
| 41 | 3 | `CharKeyHandMidi::Poll` | 2,236 | 92.84 | **gated** `uf-keyhand-poll-layout`, `uf-keyhand-poll-fingers` |
| 42 | 5 | `GemTrack::DrawFill` | 1,652 | 88.80 | reason |
| 43 | 11 | `PerfectSectionTracker::HandleExitExtent` | 1,256 | 97.23 | reason |
| 44 | 14 | `OutfitConfig::SetSkinTextures` | 1,192 | 95.07 | reason |
| 45 | 22 | `NetCacheMgr::AddLoaderRef` | 696 | 97.84 | reason |

## 2. Gates and references

All gates are in `native/src/w16uf_phase.cpp`. The phase runs by default after W16-UB
(`--no-w16uf` skips it). It is compiled with `-fno-access-control`, as TW's was.

**Where the expected values come from:**
- Shipped files, read directly.
- Retail bodies read off the retail asm. Each rule is written in a comment above the
  reference function that implements it.

No expected value comes from the code under test. A fixture gate checks each fixture before
the row gates that use it, so a row-gate failure points at the row.

**Factory registration.** Retail boot registers factories that rb3-render does not:
- BandInit registers `CharKeyHandMidi`, `BandCamShot` and `BandFaceDeform`.
- The meta init registers `AppLabel` under its base name `BandLabel`.

The phase saves `Hmx::Object::sFactories`, registers these four, and restores the table at
the end.

| gate | row | shipped data | reference |
|---|---|---|---|
| `uf-keyhand-finger` | #31 | `char/main/rigging/gen/keyboard.milo_xbox`: both `.keyhand`s | retail `fn_822CF888`. 8,112 cases: 2 hands × 26 × 26 keys × 6 fingers |
| `uf-cymbal` | #28 | `config/beatmatch_controller.dta`, joypad config: drum type 8 `hx_drums_xbox` (cymbal shift 3, pad shift 10), guitar type 6 `strat_xbox_rb2` (cymbal shift 24) | retail `fn_8279B390`. 768 cases: 2 controller types × 64 button sets × slots 0–5. Also checks that the ctor read the config's shift buttons, and that a pad with no local user reads false |
| `uf-repeat` | #34 | `config/joypad.dta` (hold 1000 ms, repeat 80 ms) | retail `fn_82529270`. Two held buttons on pads 0 and 2: no repeat at once, a repeat after hold, again after repeat. Guide up suppresses repeats; guide down resets the timers |
| `uf-target-copy` | #29 | `world/meta/closet/gen/portrait_space.milo_xbox` `portrait.shot`, 1 target | retail `fn_822B1140`: every field copied, both into a fresh target and over an all-different one |
| `uf-storemain` | #13 | `ui/store/store.dta` (`display_rate 4`, `crossfade_duration 1`), `store_main.milo`: `cover_art_none.tex`, 6 cover mats, labels, `album_scroll.anim` | retail `fn_8263AB70`. Labels 1 and 2 hold a token before `FinishLoad`, so skipping the reset is caught |
| `uf-keyhand-poll-layout` | #41 | `keyboard.milo`'s spot meshes | retail `fn_822D1980`'s key/tip construction (1/14, 1/28, −0.4, 0.5, the black-key table) from the spots' world transforms. Limit 1e-5 of keyboard width; measured worst 7.41e-08 |
| `uf-keyhand-poll-fingers` | #41 | same rig | retail `fn_822D1980`'s assignment for 1–5 keys and the free-finger count. 3,000 LCG-scripted polls per hand, including 0–7 keys, lifted fingers and the clock going back |
| `uf-crowd-poll` | #6 | `ui/track/gen/trackpanel.milo_xbox`: crowd meter, 5 icons | retail `fn_8227DF58`: peak list order and flags, group frames easing to index+2, arrow and peak-arrow triggers. 307 polls |
| `uf-crowd-configure` | #23 | same panel | retail `fn_8228E6B8`. 4 game-mode cases (practice × `show_crowd_meter`), instruments guitar/none/drum/pending/vocals; want used `1 0 1 0 0` when not practice |
| `uf-crowd-setup` | #24 | the panel's `GemTrackDir` as track 2 | retail `fn_822D7E08`: show flag from the game mode, icon 2's label icon (`"G"` with no parent) and panel, other icons untouched |
| `uf-crowd-rating` | #15 | same, plus `warning_anims.grp` | retail `fn_822D9C50`. 8 ratings: normal, normal again, warning, failed, normal, invalid state, meter disabled, no track index |
| `uf-deltas` | #14 | `char/main/shared/gen/head_female.milo_xbox`: `base.msnm` (2,999 points), 6 `BandFaceDeform`s, 350 frames | retail `fn_822C7298` and its quantizer `fn_822C7040`. Each frame is decoded and re-appended, once and twice into one array (734,923 B). 12 edge points cover the clamp, half steps and NaN |
| `uf-closet-teleport` | #36 | `world/shared/gen/chars.milo_xbox`: player0–3 and their closet waypoints | retail `fn_82281D50`: the waypoint takes the character's transform, the character is teleported to the waypoint, flag `0x5fe` is cleared, returns 0 |
| `uf-enter-closet` | #22 | the shipped `BandWardrobe` (4 targets with drivers) and `world/meta/closet/gen/portrait_clips_shared.milo_xbox` `clips` | retail `fn_8232F1C0`: closet names, closet/venue weights, `"closet_character"` on the target only, driver clips, the dir set, only the target shown |
| `uf-stoplog` | #38 | none; the gate drives the `mem_log` script function | retail `fn_827C46A0` → `fn_827D4508`, with `lbl_8204BD5C = ")"`. Runs in a forked child, because `MemTrackInit` hooks every allocation |

**Gate notes:**
- **`uf-deltas`: 8 of 350 re-encoded frames are byte-identical to the shipped frame, as
  expected.** Retail's quantizer is `(x·63.5 + 0.5)` truncated toward zero, so a shipped
  negative byte `b` decodes and re-encodes as `b+1`. The gate compares against the
  reference quantizer, not against the shipped bytes.
- **`uf-keyhand-poll-fingers` stands in for missing finger bones.** The shipped rig has no
  finger bones, so `CharIKFingers::SetFinger` would dereference null. For the 10 unbound
  finger-bone sets the gate binds the rig's hand bone and restores the bindings afterwards.
  Fingering is decided before any bone is touched, so this does not change what the gate
  checks.
- **`uf-crowd-rating` installs a temporary tempo map and beat map.**
  `SetCrowdRating` reaches `TheTempoMap`/`TheBeatMap`, which are null in rb3-render. The
  gate installs a `SimpleTempoMap(500)` and a `BeatMap` only while they are null, and
  removes them afterwards.
- **The crowd gates answer three game-mode properties from a lane object.** `CrowdGameMode`
  answers `is_practice`, `show_crowd_meter` and `update_crowd_meter`. These are inputs
  enumerated by the gate, not expected values.

**Tolerances.** Only `uf-keyhand-poll-layout` (1e-5 of keyboard width) and the crowd-meter
group frames (1e-5) have float tolerances. The cause is `/fp:fast`: retail multiplies by a
reciprocal and fuses into `fmadd`, while native does separate operations. Everything else is
compared exactly.

## 3. Link and load closure (`native/CMakeLists.txt`)

- **`w16uf_phase.cpp`** is added, with `-fno-access-control`.
- **`W16UF_LINK_SOURCES`**, all compiled with
  `RB3_SYNCPROP_LOCAL_STATIC;RB3_HANDLE_LOCAL_STATIC`:
  - `BeatMatchController.cpp`
  - `StoreArtLoaderPanel.cpp`
  - `BandStorePanel.cpp`
  - `StoreMenuProvider.cpp`, for `StoreMenuProvider::GetTitle`
  - `network/net/Synchronize.cpp`, for `typeinfo for Synchronizable`
- **`AppLabel.cpp`, `BandCharDesc.cpp` and `BandFaceDeform.cpp`** get the same two defines
  through a separate `set_source_files_properties`. Their Handle or property tables become
  live once the phase reaches them, and without the defines they reference undefined
  `extern Symbol`s (`set_user_name`, `target`, …).

## 4. Sabotage controls (predictions in `~/tmp/w16uf/sabotage_predictions.txt`, written before each build; all reverted)

| # | round | defect | predicted | result |
|---:|---|---|---|---|
| 1 | S1 | `FindPreferredFinger`: right hand moving up, `d <= 5` → `<= 4` | `uf-keyhand-finger`, `uf-keyhand-poll-fingers` | both FAIL |
| 2 | S1 | `IsCymbal` case 3: `kPad_DDown` → `kPad_DUp` | `uf-cymbal` | FAIL |
| 3 | S1 | `JoypadClient::Poll`: guide-up arm does not reset the repeat timer | `uf-repeat` | FAIL |
| 4 | S1 | `BandCamShot::Target`: a user `operator=` that drops `mHide` | `uf-target-copy` | FAIL |
| 5 | S1 | `StoreMainPanel::FinishLoad`: label 2's token not cleared | `uf-storemain` | FAIL |
| 6 | S1 | `AppendDeltas`: a run no longer ends at a zero delta | `uf-deltas` | FAIL |
| 7 | S1 | `BandCrowdMeter::Poll`: a new peak is linked at the back | `uf-crowd-poll` | FAIL |
| | **S1 total** | | **exactly 8** | **8 failures** |
| 8 | S2 | `CharKeyHandMidi::Poll` case 4: 4th key on the ring finger | `uf-keyhand-poll-fingers` | FAIL |
| 9 | S2 | `Poll` layout: black-key raise `up*0.5` → `up*0.4` | `uf-keyhand-poll-layout` | FAIL, worst 8.88e-03 |
| 10 | S2 | `ConfigureCrowdMeter`: practice arm leaves the meter shown | `uf-crowd-configure` | FAIL, 2 cases |
| 11 | S2 | `SetupCrowdMeter`: default icon `"G"` → `"H"` | `uf-crowd-setup` | FAIL `icon2='H'` |
| 12 | S2 | `SetCrowdRating`: the "not failed" condition dropped | `uf-crowd-rating` | FAIL, warning flag |
| 13 | S2 | `OnClosetTeleport`: flag `0x5fe` not cleared | `uf-closet-teleport` | FAIL, 4 of 4 |
| 14 | S2 | `OnEnterCloset`: every closet name `"closet_character"` | `uf-enter-closet` | FAIL, 4 of 4 |
| | **S2 total** | | **exactly 7** | **7 failures** |
| 15 | S3a | `MemTracker::StopLog` (native arm): `")"` not written | `uf-stoplog` | FAIL: file ends `(data\r\n` |
| 16 | S3b | `StopLog`: `gLog` not released | `uf-stoplog` | FAIL: file empty (never flushed) |

The diffs are in `~/tmp/w16uf/sabotage_S{1,2,3a,3b}.diff`. After each revert,
`git diff --stat` showed no `src/` change, and a clean rebuild ran
`RESULT: ALL GATES PASSED`.

## 5. Bugs found

### 5.1 `MsgSource`: a deleted sink stayed subscribed natively (`src/system/obj/Msg.cpp`)

- **Before the fix.** The first run with `uf-cymbal` exited **rc=139**. The crash was in
  `MsgSource::Sink::Export`, on a `JoypadController` deleted by the previous gate.
- **Cause.** Native defined `MSGSRC_ADDREF`/`MSGSRC_RELEASE` as `((void)0)`.
- **What retail does.** Retail's `AddRef(owner)` puts the source on the sink's ref list.
  When the sink dies, `~Hmx::Object` walks that list and calls `MsgSource::Replace`, which
  calls `RemoveSink(dead, Symbol())` (`fn_82766EE0`). `JoypadController`'s dtor
  (`fn_8279BC30`) is the bare `~Hmx::Object`, so this walk is the only thing that removes it.
- **Fix (`HX_NATIVE` only).** For each (source, sink) pair the source holds one counted
  ring node, `MsgSourceSinkRef`, kept in a never-freed map.
  - The node's `Replace`/`NullifyObj` remove the dead sink once per reference.
  - `~MsgSource` frees any nodes left over.
  - The X360 arm still expands to the original `(obj)->AddRef(owner)` / `Release(owner)`.

### 5.2 `DeltaArray::CompressDelta`: NaN stored `0x82` natively (`src/system/bandobj/BandFaceDeform.cpp`)

- **Before the fix.** `uf-deltas`' NaN edge point failed with `82/01`.
- **What retail does** (`fn_822C7040`):
  1. Clamps with two `fsel`, which pass a NaN through.
  2. Converts toward zero to 64 bits with `fctidz`.
  3. Stores the low byte, so a NaN delta stores 0.
- **What native did.** Native `Clamp` maps NaN to the lower bound, −2 (`0x82`). A double
  converted straight to `unsigned char` is also undefined when it is negative.
- **A first fix attempt was not enough.** It tested `q != q` after the clamp, by which point
  the NaN had already become −2.
- **The fix.** The `HX_NATIVE` arm tests `d[i]` before clamping, and converts through
  `long long`.

### 5.3 Divergences recorded, not fixed

- **Native `MemHeap::Print`** prints each alloc's stack trace (`ts << *info`), where retail
  TU5 prints one ` (type "%s")` field (`MemHeap.cpp:47`).
- **Native `MemTracker::MemTracker`** registers `spit_alloc_info`/`sai`
  (`MemTracker.cpp:178`), which retail's ctor does not. This is the DC3-era body; the
  X360 arm is unaffected.
- **Native float `Clamp`** maps NaN to the lower bound; retail's `fsel` passes it through.
  Only `CompressDelta` was fixed (§5.2), because it is the only site a gate reaches with
  a NaN.
- **`/fp:fast` rounding.** The tolerances in §2 absorb it; it is not a defect.
- **The phase adds `Locale.cpp:472 mSymTable` FAIL notices to stderr: 303 without it (run0),
  411 with it.** They predate the phase, which only reaches more labels. They are notices,
  not gate failures, and were not investigated.
- **A standalone load of `big_club_07.milo` spins natively** after `Stream error: Can't
  read`. A probe hung for about 10 minutes and was killed, and the probe was removed. This
  was not investigated; it bears on #17 below.

## 6. Recorded reasons (rows taken, not gated)

- **#1 `LayerDir::GetBitmapList`.** As TW found for #7 `RefreshLayer`, no shipped milo
  contains a `LayerDir`.
- **#2 `MetaPerformer::SaveAndUploadScores`.** Its work is a network score upload. No
  shipped data drives it, and native has no server.
- **#3 `GemTrack::DrawBeatLine`, #12 `ImmediateWidgetImp::DrawInstances`,
  #19 `CharWidgetImp::RemoveInstances`, #42 `GemTrack::DrawFill`.**
  - These are track-widget drawing rows. They need W16-TJ's track fixture running
    gameplay: beat lines from a playing song's tempo map, widget instances from gems.
  - `DrawFill` also needs a `GemPlayer` whose `BeatMatcher` answers `FillsEnabled`, as
    TW recorded.
  - A lane-made matcher would not be shipped data.
- **#4 `SyncSharedSongs`, #10 `RemoveOldestCachedContent`,
  #18 `ReadCachedMetadataFromStream`** (`BandSongMgr`). Each needs a song cache or a save
  data stream, and nothing on the disc holds one.
- **#5 `SongData::UnflipGems`, #11 `SongData::Load`, #39 `SongDB::PostLoad`.** These are
  in the song-load pipeline (`SongDB` → `SongData` → track parsers with `MasterAudio`),
  which rb3-render's default mode does not run. `rb3-score*` loads MIDI by another path.
- **#7 `MemHeap::Alloc`.**
  - Retail's heap is ILP32 word layout: 4-byte block headers and free-list links counted in
    words (`GetSizeWords`).
  - Native's LP64 heap has different block sizes, so retail's free-list and size rules do
    not transfer as a byte-level reference.
  - #9 and #25 are listed in §5.3 as divergences instead.
- **#8 `PreInitSystem`, #30 `InitSystem`.** These are the boot path. rb3-render already ran
  them at boot, and running them again in a booted process initialises everything twice.
  The `boot-*` gates cover the parts rb3-render checks.
- **#9 `MemHeap::Print`.** Native prints a stack where retail prints ` (type "%s")` (§5.3),
  so a gate against retail would fail on a known native-only arm.
- **#16 `ClosetPanel::CycleCamera`.** It cycles closet camera shots named `<base>_N.shot`.
  No shipped closet milo has them; `portrait_space.milo` has only `portrait.shot`.
- **#17 `BandCamShot::SetPreFrame`.** No shot with a pre-frame chain loads in default mode.
  The venue that would supply one spins on load (§5.3).
- **#20 `GemManager::CheckRemoveChordBracket`, #26 `CheckForFatFinger`,
  #33 `MasterAudio::FillSwing`.** These need live gameplay state: `GemManager`, a keyboard
  track watcher, `MasterAudio` with a playing song.
- **#21 `AccomplishmentProvider::Mat`.** It needs the accomplishment icon mats and a
  profile's progress, i.e. save data.
- **#25 `MemTracker::MemTracker`.** Native's ctor registers two script functions retail's
  does not (§5.3). `ub-memtrack-stacks` already constructs it in a child.
- **#27 `BandCharacter::StartLoad`.** It goes through the `FileMerger` load pipeline of a
  full band load (`RB3_BAND_PLACE=1`), not default mode. This is TW's reason for
  `LoadMainCharacters`.
- **#32 `DataNode` map `operator[]`.** This is a host STL instantiation. The native build
  uses the host's STL, not the STLport body this row scores.
- **#35 `BandCharacter::AddOverlays`.** Its overlays draw into a patch render target that
  default mode does not create.
- **#37 the unidentified `MetaPerformer.cpp` `…int>` row.** Its only caller is
  unidentified TU5 RBN code (lever 4 of `CAMPAIGN_STATE_2026-10-07c.md`). It has no
  identified body to gate.
- **#40 `Campaign::GetLaunchUser`.** It needs a `BandProfile` associated with a pad, i.e. a
  signed-in profile.
- **#43 `PerfectSectionTracker::HandleExitExtent`.** TW's reason stands.
  - `quests.dtb` supplies the accuracy and multipliers.
  - It also needs a `TrackerSource` with local `Player`s and `Stats`, and a
    `TrackerSectionManager`; no default-mode fixture provides them.
- **#44 `OutfitConfig::SetSkinTextures`.** It runs only on the `RB3_BAND_PLACE=1` full band
  load. Memory was short, and this lane added no band load to default mode.
- **#45 `NetCacheMgr::AddLoaderRef`.** TW's reason stands.
  - It needs `NetCacheMgr` forced ready, which needs a cache mount.
  - The `local` server it would use exists natively only because native lacks `_SHIP`.

## 7. X360 A/B

- **Prediction: Δ0.** Both `src/` changes sit inside `#ifdef HX_NATIVE`, so the X360 TUs
  expand to the same tokens.
- **Method.** The branch's `src/` diff was reverted in a temporary commit, which was
  dropped afterwards (`git reset --hard HEAD~1` on this branch). The diff was then measured
  as a patch:
  `python3 tools/ab_measure.py --worktree ~/tmp/wt-w16uf --patch <src diff> --label w16uf-hxnative-src`.
- **Result.** Leg B recompiled 2 TUs (`Msg.cpp`, `BandFaceDeform.cpp`).

  ```
  leg A: matched=54947 masked=25223 honest=29724 code%=59.314644  (recompiles: 0, settled)
  leg B: matched=54947 masked=25223 honest=29724 code%=59.314644  (recompiles: 2, split=0, patch_steps=6, settle iterations: 2)
  Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
  Δfuzzy=+0.000000pp   (legA 64.315315 -> legB 64.315315)
  units at 100% [mpn ruler]: legA 607 -> legB 607
  ```

  The `none` ruler was flat as well.

## 8. Not done

- **31 of 45 rows are not gated** (§6). Most need gameplay, a song load, save data or a
  full band load. This lane added none of these to rb3-render's default mode, because
  memory was short.
- **`native_runtime_rank.py` was not rerun**, as in TW. Entry of each gated row is shown by
  sabotage instead: each row's gate fails when that body is broken (§4).
- **The divergences in §5.3 were recorded, not fixed.**
- **The `big_club_07` spin was not investigated.**
- **`native_health` was not run on main**, so the +24 gates below are the phase's own gate
  count, not a measured main-to-branch delta.

## 9. Result lines

```
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=308 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=13 scatter_dirb=0 scatter_multihost=10 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
```

The native build gate's line is in the lane's final report. The build gate was run as the
last action, after this doc was committed.

## Reproduce

```
cmake --build native/build --target rb3-render
native/build/rb3-render ~/code/milohax/rb3/orig-assets/xbox-zip <outdir>   # W16-UF phase runs by default; --no-w16uf skips it
tools/native_health.sh <worktree>
tools/native_build_gate.sh
```
