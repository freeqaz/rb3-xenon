# W16-TS: SetState, OnFileLoaded, MaybePublish and the song/modifier filters, gated natively on shipped data (2026-10-07)

Branch `w16-ts`, rebased on main `be41a0308` (W16-TT) before the X360 A/B. Not merged or
pushed.

## 0. Headline

`CAMPAIGN_STATE_2026-10-07c.md` §6 lever 2: the in-scope rows the native build links but
no target enters. This lane drives the four rows the brief named, plus the smaller
behaviour-class rows the same fixtures reach, in `rb3-render`'s default mode
(`native/src/w16ts_phase.cpp`, after W16-TR's phase; `--no-w16ts` opts out). Each check
compares against a reference computed without the code under test: the shipped file
read directly, or a retail transition read off the target asm and written down.

| row | B | fuzzy | in the 69? | outcome |
|---|---:|---:|---|---|
| `SaveLoadManager::SetState` | 4,096 | 99.95 | no (one of §4.3's 166) | **gated**, 3 `sl-*` (21 transitions + 11 exit checks) |
| `BandDirector::OnFileLoaded` | 3,816 | 99.78 | no (one of §4.3's 166) | **gated**, 5 `bd-*` |
| `UIStats::MaybePublish` | 2,604 | 99.92 | yes, #0 | **gated**, 5 `uis-*` |
| `StoreOfferProvider::BuildList` | 2,536 | 99.68 | yes, #1 | **recorded reason** (§6); native LP64 bug fixed (§5) |
| `SongSortMgr::DoesOfferMatchFilter` | 1,416 | 99.42 | yes, #8 | **recorded reason** (§6) |
| `BandDirector::OnGetFaceOverrideClips` | 648 | 99.17 | yes, #31 | **recorded reason** (§6) |
| `SongSortMgr::DoesSongMatchFilter` | 568 | 98.56 | yes, #38 | **gated**, `sf-filters` |
| `BandSongMetadata::HasPart(Symbol, bool)` | 340 | 98.59 | yes, #47 | **gated**, `hp-rank` |
| `ModifierMgr::IsModifierActive(Symbol)` | 60 | 79.67 | yes, #66 | **gated**, `mm-active` |
| `ModifierMgr::IsHidden(int)` | 28 | 28.57 | yes, #67 | **gated**, `mm-list` |
| `ModifierMgr::IsActive(int)` | 28 | 28.57 | yes, #68 | **gated**, `mm-list` |

Of the 69 rows (49,512 B), this lane gated 6 (3,628 B) and recorded a reason for 3
(4,600 B). The other 60 were not taken (§8). Five fixture gates (`ts-songs`,
`mm-fixture`, `bd-fixture`, `uis-fixture`, `sl-fixture`) come first, so a broken fixture
fails before a gate on the function under test reads it.

- **No gate found a behaviour difference from retail.** All 19 W16-TS gates pass:
  `RESULT: ALL GATES PASSED (0 gate failure(s))`, 163 gates in the run.
- **Every non-fixture gate was shown to fail on a broken body** (§4): two sabotage builds
  with 13 defects, predictions written before each run. 17 gates failed, and 16 of those
  failures were predicted. The 17th was a gate I predicted would pass. Its cause was
  real: an empty publish counts as a dropped screen.
- **One native bug fixed (native only):** `StoreOfferProvider`'s two chunk-path accessors
  read `BandStorePanel` at retail byte offsets 0xB8 / 0xC4. Natively the members sit at
  **0x170 / 0x188** (measured by the phase). Under `HX_NATIVE` they now use a member
  pointer. This was found by reading the code; no gate exercises it, because `BuildList`
  does not run natively (§6).
- **A retail behaviour the gates pin down:** `MaybePublish` reads `mLastControllerType`
  but never stores it (retail `fn_8255F9D0`: one `lwz r11,0(r15)`, no store), so a
  participating remote user is re-reported on every publish. `uis-remote-again` checks
  this, and a sabotage that adds the store makes it fail.
- **X360 A/B: Δ0 on every key**, as predicted (§7).

## 1. The reference

- **Songs (`ts-songs`, `hp-rank`, `sf-filters`).** `songs/songs.dta` is read with
  `DataReadFile`, and each entry's `song_id`, `year_released`, `rating`, `artist` and
  `rank` table is copied into a plain struct. `HasPart(part, flag)` must equal `rank > 0`
  for 9 parts, with both flag values. Each of the 18 filters is evaluated in the phase
  from those plain fields and compared with `DoesSongMatchFilter` on the real
  `BandSongMetadata` from `BandSongMgr::AddSongData`.
- **Modifiers (`mm-*`).** The reference is `config/modifiers.dta` as shipped. The listed
  rows are the ones with no `custom_location`, in file order. `IsModifierActive` should
  equal `default_enabled`, flip with `toggle_modifier_enabled`, and agree with the
  `is_modifier_active` handler.
- **BandDirector (`bd-*`).** The shipped song milo
  `songs/20thcenturyboy/gen/20thcenturyboy.milo_xbox` is walked with `ObjDirItr`. That
  walk supplies the expected `song.anim`, `BandSongPref` and the four `CharLipSync`s. Its
  23 director tracks (property, key type, interp handler, clamp) are the reference for
  the anim `OnFileLoaded` synthesizes when the song has no milo.
- **UIStats (`uis-*`).** Screen typedefs come from the shipped `ui/splash/splash.dta`:
  `splash_screen` carries `(gather_uistats FALSE)` and `intro_movie_screen` has no such
  entry. The controller symbol comes from the shipped `CHAR_INSTRUMENT_SYMBOLS` in
  `config/macros.dta`; the live macro is copied before the file is read, because a
  DTB's `#define`s apply at load. The behaviour reference is the retail body
  `fn_8255F9D0`:
  - It has one `bl RecordDataPoint` (`fn_827CD110`), so only `stats/pad_user` is ever
    recorded, and only when it has more than its name.
  - `mLastControllerType` (UIStats+0xE8, walked by r15) is loaded at `0x82560110` and
    never stored.
  - The drop arms: a disconnect clears `mPublishingPad` and a `gather_uistats FALSE`
    screen does not. Both bump `mLastDroppedScreen` and rewind the pad log.
  - `JoypadGetBreedString` (`fn_82526308`) formats `%02x` of the pad type, then the ten
    EEPROM bytes for types 0x1D..0x2E (`cmpwi 0x1d` / `cmpwi 0x2e`). The phase formats
    the expected string itself from the bytes it wrote.
- **SaveLoadManager (`sl-*`).** The transition table was read off retail `fn_82550880`.
  Each entry below gives the address of its immediate:

  | from | to | retail |
  |---|---|---|
  | 0x15, 0x16 | 0x19 | `0x82550C9C` `li r4,0x19` |
  | 0x19, dialog fails with a non-zero result | 0x1A | `0x82550D6C` `li r4,0x1a` |
  | 0x24 / 0x25 | `unk7c` = 1 / 0, `unk78` = 0, `unk68` = 1; then 0x22 if `mCache`, else 0x26 | `0x82550F14` `subfic`/`subfe`/`clrrwi 2`/`addi 0x26` |
  | 0x26 | `mCacheID` = 0, then 0x27 | `0x82550F34` `stw`, `li r4,0x27` |
  | 0x13 | `GetCacheID("globaloptions")` if `mCacheID` is null; then 0x37 if still null, else 0x31 | `0x82550F84` / `0x82550F8C` |
  | 0x53 | same lookup; 0x40 / 0x3D | `0x825515D4` / `0x825515DC` |

  The name is `"globaloptions"`: the `.data` pointer at 0x82C72830 points to 0x82089524,
  which was read from `band.exe`. The row reads mpn 100 / fuzzy 99.95, so its only
  charges are relocation names. The immediates and branch shapes of our body are
  retail's; the gate checks what the native build does with them.

## 2. Gates

| gate | checks | result |
|---|---|---|
| `ts-songs` | fixture: 138 `songs.dta` entries with a `song_id`; the real `BandSongMgr` resolves all 138 to the same id | PASS |
| `hp-rank` | 2,484 queries (138 songs × 9 parts × 2 flags) equal `rank > 0`; 869 parts ranked; an unranked symbol reads false | PASS |
| `sf-filters` | 18 filters × 138 songs: decade(s), rating, artist (filter 9), pro guitar yes/no, keys, the 8 required-part filters, excluded ids; 17 of 18 split the list | PASS |
| `mm-fixture` | fixture: 11 modifiers, 9 listed, 4 `default_enabled` | PASS |
| `mm-list` | 9 rows: `IsActive` true and `IsHidden` false on every row (all unlocked) | PASS |
| `mm-active` | 11 modifiers: `IsModifierActive` = `default_enabled`, flips with toggle, `is_modifier_active` agrees | PASS |
| `bd-fixture` | fixture: 55 objects, `song.anim` with 23 keys all on the director, a `BandSongPref`, 4 `CharLipSync`s | PASS |
| `bd-song-finds` | `on_file_loaded venue` changes nothing; `on_file_loaded song` caches the anim, pref and 4 lipsyncs from the dir, keeps the shipped anim, `mEndOfSongSec` 0 | PASS |
| `bd-song-clamp` | exactly the five `<instrument>_intensity` symbol tracks gain clamp-to-previous-range | PASS |
| `bd-null-made` | with no dir: every find cleared; a `song.anim` of type `song_anim` at `k480_fpb` in the merger's dir, owned by the `song` merger | PASS |
| `bd-null-keys` | the synthesized anim's 23 tracks equal the shipped anim's (property set, key types, interp handlers, clamp only on intensities) | PASS |
| `uis-fixture` | fixture: both screen definitions, `CHAR_INSTRUMENT_SYMBOLS` (6 entries, live = shipped), fake Server built | PASS |
| `uis-drop` | disconnected: `mPublishingPad` 0, dropped 1, pad log rewound, nothing recorded; `splash_screen`: `mPublishingPad` kept, dropped 2 | PASS |
| `uis-publish` | first publish: one point `{dropped_screens=2 name=intro_movie_screen pad_1=1da0a7aeb5bcc3cad1d8df remote_user_0=drum:0009000012345678}`, dropped reset to 0, mode recorded | PASS |
| `uis-remote-again` | nothing changed: `{name, remote_user_0}` only (`mLastControllerType[0]` stays 5) | PASS |
| `uis-reconnect` | a disconnect drops one screen; the reconnect publish resets per-pad memory and reports everything, with `dropped_screens=1` | PASS |
| `sl-fixture` | fixture: `saveload_mgr` is free; the real CacheID store answers `globaloptions` → idGlobal | PASS |
| `sl-cache-arms` | 21 transitions (0x26, 0x24, 0x25, 0x13 ×2, 0x53, 0x16, 0x15 ×2, 0x1D, 0x30, 0x1B, 0x2E, 0x20, 0x1E, 0x2C, 0x22, 0x23, 0x34, 0x35, 0x3F): end state, every CacheMgr/Cache call with the state it was made in, every status export | PASS |
| `sl-exit` | 11 checks: `mData` kept leaving 0x1F for `kS_Finish` and freed leaving `kS_Finish`; freed leaving each of 0x1F/0x21/0x32/0x33/0x3E; same-state no-op; `kS_Idle` exports 0 on exit (after `mState` is set) and 5 on entry; `kS_Start` clears `unk7c` | PASS |

The UIStats gates were first named `us-*` and renamed `uis-*`, because W16-TJ's
`UpdateScrolling` gates already use `us-`.

### 2.1 Fixtures and how they are built

- **SongSortMgr** is `calloc`'d. Its constructor builds the sort objects, which need the
  sort TUs; `DoesSongMatchFilter` reads none of that state.
- **BandDirector** is real (`new BandDirector`, named in the main dir). It gets a real
  `FileMerger` with one `song` merger, and messages go in through `Handle`.
- **UIStats** fixture:
  - It uses a real `UIStats` and a real `LocalBandUser` on pad 1 (`gJoypadData[1].mUser`,
    type `kJoypadXboxCoreGuitar`, EEPROM bytes `a0 a7 … df`). The local user is not
    participating: a participating local user asks `PlatformMgr::GetOnlineID`, which is
    XUser-only.
  - It also uses a real `RemoteBandUser` (drums, participating, XUID
    `0009000012345678`), a `calloc`'d `GameMode` and `BandUserMgr`, and
    `TheDataPointMgr`'s recorder hook.
  - **The Server is the one fake.** `Server::Server` and Server's vtable live in
    `network/net/Server.cpp`, which also defines the global `gXboxServer`, whose dynamic
    initializer rb3-render must not run. `MaybePublish` makes exactly one Server call,
    `IsConnected()`. The fake is zeroed Server-sized storage whose vptr points at a table
    holding Server.h's own inline body (`mLoginState == 2`) in `IsConnected`'s slot. The
    slot number is read off `&Server::IsConnected` (Itanium ABI). Every other slot aborts.
- **SaveLoadManager** is real: its constructor names it `saveload_mgr` and sinks
  `ThePlatformMgr`, and its destructor undoes both. `TheCacheMgr` and `mCache` are
  replaced by subclasses that record each call. The CacheID store is the real
  `CacheMgr`'s, because `GetCacheID`, `AddCacheID` and `RemoveCacheID` are not virtual.
  A sink records each `saveloadmgr_status_update_msg`. `mState` is set directly to a
  neutral start (0x1A has no exit cleanup and no entry body) before each `SetState`.
- Everything installed is saved and restored: `TheModifierMgr`, `TheBandDirector`,
  `TheNet.mServer`, `TheGameMode`, `TheBandUserMgr`, the recorder, pad 1's joypad data
  and `TheCacheMgr`.

## 3. Link and load closure

All of it is native-only. X360 sees only the two guards in §5.

- **Real TUs** (`W16TS_LINK_SOURCES`):
  - `bandobj/CrowdAudio.cpp`: `TheCrowdAudio`, `SetBank`.
  - `tour/TourCharRemote.cpp`: `RemoteBandUser`'s constructor creates one.
  - `meta_band/StandIn.cpp`: `BandProfile::SaveSize`.
  - `meta_band/BandMemcardAction.cpp` and `meta/MemcardAction.cpp`: the two actions
    `SetState` creates.
- **Real answers, not stubs** (`w16ts_link_support.cpp`):
  - `Symbol hidden`, interned at phase start. This is the `m6_symbols.cpp` pattern.
  - `BandCamShot::sHideAllCharactersHack` (zero).
  - `StageKitConnected()` returns false; natively there is no Stage Kit.
- **Loud stubs** (each aborts and names itself):
  - `StageKitSetFog`.
  - `PlatformMgr::CanSeeUserCreatedContent` (XPrivilegeCheck).
  - The eleven `MemcardMgr` calls in `SaveLoadManager.cpp`
    (`MemcardMgr{,_Xbox}.cpp`, XContent).
  - `WiiProfileMgr::SaveSize`.
  - `TourProgress::SaveSize`: its TU drags in `QuestJournal` and
    `TourPropertyCollection`, and only the save-size printout calls it.
  - `RockCentral::UpdateBandLogo` (Quazal).
- **Zero storage:** `TheMemcardMgr`. `TheEntityUploader` and `TheServer` are references,
  so each is a null pointer. Calling through any of the three faults.
- **One scatter-include guard:** `CrowdAudio.cpp`'s `#include "hamobj/HamMove.cpp"` is
  now `#ifndef HX_NATIVE`. Natively, `Morph.cpp` already scatter-includes `HamMove.cpp`
  (through `FileMerger.cpp`), so linking CrowdAudio as a real TU defined the HamMove
  symbols twice.
- **Load noise, not a gate input:**
  - `[toplevel] world/shared/director.milo: Can't make BandDirector`: the song milo's
    `../../world/shared/director.milo` subdir loads with no factory. Its keys resolve to
    the fixture director through the main-dir fallback, which `bd-fixture` checks
    (23/23 on the director).
  - Four `FAIL: … Locale.cpp Line: 472 Error: mSymTable` lines inside the phase. The
    0x19 / 0x2C arms pass `Localize(...)` as a dialog title, and rb3-render has no
    locale table. That is a native-only `MILO_ASSERT` that prints and returns. The fake
    ignores the title.
  - `Resetting macro …`: reading `config/macros.dta` re-applies its `#define`s with the
    same values.

## 4. Sabotage controls (predictions written before each run, all reverted)

Both builds rebuilt only rb3-render, then ran it in default mode. Patches are in
`~/tmp/w16ts_sab1.diff` and `~/tmp/w16ts_sab2.diff`. The tree was restored with
`git checkout -- <files>`, then rebuilt and re-run green (163 PASS). The gate names in
the S1/S2 logs are the pre-rename `us-*`.

**S1** (seven defects in five files):

| defect | predicted to fail | failed |
|---|---|---|
| `DoesSongMatchFilter`: drop `if (!found) break;` | `sf-filters` | `sf-filters` |
| `ModifierMgr::IsHidden`: unlocked → `true` | `mm-list` (9 wrong) | `mm-list` |
| `OnFileLoaded`: swap `part3` / `part4.lipsync` | `bd-song-finds` | `bd-song-finds` |
| synthesized `spot_vocal` track as `kObject` | `bd-null-keys` | `bd-null-keys` |
| `MaybePublish`: store `mLastControllerType[remoteCount]` | `uis-remote-again` | `uis-remote-again`, **and `uis-reconnect`** |
| `SetState` 0x24: `unk7c = 0` | `sl-cache-arms` (the 0x24 case only) | `sl-cache-arms` (0x24 WRONG) |
| `SetState` exit: drop the `newState != kS_Finish` guard | `sl-exit` | `sl-exit` ("keeps mData" WRONG) |

Result: `RESULT: FAILED (8 gate failure(s))`. Every other W16-TS gate passed.
**Prediction miss:** I expected `uis-reconnect` to pass. With the store added, the second
publish finds nothing new, so `screenExit` holds only its name and the screen is
dropped (`mLastDroppedScreen` 1). The reconnect publish then reports
`dropped_screens=2`, not 1. The cascade is real behaviour that my prediction left out,
not a gate defect.

**S2** (six defects in four files):

| defect | predicted to fail | failed |
|---|---|---|
| `HasPart(Symbol, bool)`: `rank >= 0` | `hp-rank`, and `sf-filters` (required-part filters call it) | both |
| `IsModifierActive(Modifier*)`: test inverted | `mm-active` (not `mm-list`, which uses `IsModifierUnlocked`) | `mm-active` |
| `OnFileLoaded`: intensity clamp set `false` | `bd-song-clamp`, `bd-null-keys` | both |
| synthesized anim at `k30_fps` | `bd-null-made` | `bd-null-made` |
| `MaybePublish`: disconnect leaves `mPublishingPad` | `uis-drop`, `uis-reconnect` | both |
| `MaybePublish`: no `dropped_screens` pair | `uis-publish`, `uis-reconnect` | both |

Result: `RESULT: FAILED (9 gate failure(s))`, exactly the predicted set.

No sabotage targets the five fixture gates. They read only the shipped files and the
fixtures, so a defect in the code under test cannot reach them, and in both runs they
passed as predicted.

## 5. Bugs found

- **Fixed (native only): `StoreOfferProvider`'s chunk-path accessors.**
  - X360 reaches `BandStorePanel`'s protected `mPrevChunkPath` / `mNextChunkPath` by
    byte offset 0xB8 / 0xC4. That is right for retail: `BuildList` reads the `char*` at
    panel+0xC0 / +0xCC, and `class_layout_report.py` agrees.
  - Native clang lays the class out with 8-byte pointers. The phase measures the members
    at **0x170 / 0x188**, so the X360 spelling would read unrelated bytes as a `String`.
  - Under `HX_NATIVE`, the accessors now take a `String BandStorePanel::*` through a
    derived class (`ChunkPathAccess`), which may name a protected member. The X360
    spelling is unchanged.
  - Not exercised by a gate, because `BuildList` does not run natively (§6). The phase
    prints the measured offsets on every run.
- **Retail behaviour, kept and gated, not fixed:** `MaybePublish` never writes
  `mLastControllerType`, so a participating remote user is reported on every publish.
  The source already says this, and the gate shows the native build does it too.
- **Retail behaviour seen while reading, not in this lane's rows:**
  `SongFilter::IntersectFilter` pushes `filter`'s excluded songs back onto `filter`
  itself while iterating it. So `this` never receives them, and a non-empty list
  invalidates its own iterator. Recorded only.
- **Checked and not a bug:** `JoypadGetBreedString` passes a `String` by value into
  `MakeString("%02x%s", type, s)`. `MakeString` is a template, not varargs. Retail
  (`fn_82526308`) copy-constructs the `String` (`fn_827BE608`), passes its address to
  `MakeString<int,String>` (`fn_826058E8`), and formats it through
  `FormatString::operator<<(const String&)` (`fn_827C42C0`). Natively the phase saw the
  same output: `1da0a7aeb5bcc3cad1d8df` for a core guitar. An earlier note in this lane
  that called it a varargs bug was wrong.

## 6. Recorded reasons (rows taken, not gated)

- **`StoreOfferProvider::BuildList` (2,536 B).**
  - Its first act is `BandStorePanel::Instance()`, which is
    `ObjectDir::Main()->Find<BandStorePanel>("store_panel", true)`, and it then reads the
    panel's chunk paths.
  - It builds rows from `StoreOffer`s, which come from store metadata the game downloads.
    No store offer data ships on the disc.
  - A `BandStorePanel` is a `StorePanel`, and the brief keeps this lane out of the
    StorePanel files another lane is driving.
  - Left for whoever owns StorePanel. The LP64 accessor bug that would have hit it first
    is fixed (§5).
- **`SongSortMgr::DoesOfferMatchFilter` (1,416 B).** It takes a `StoreOffer*`. With no
  shipped offer data, the only offer would be one this lane made up, which is not
  shipped data. Its song-side twin `DoesSongMatchFilter` is gated (`sf-filters`).
- **`BandDirector::OnGetFaceOverrideClips` (648 B).** It needs a `BandCharacter` whose
  lipsync driver has face-override clips loaded. That means a full character load, which
  rb3-render's director fixture does not do.

## 7. X360 A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16ts --patch <reverse src diff> --jobs 8`.
Leg A is this branch and leg B is main's `src/` (2 paths: `CrowdAudio.cpp`,
`StoreOfferProvider.cpp`). I predicted Δ0, because both changes sit behind `HX_NATIVE`
guards and the worktree's `build.ninja` has no `/DHX_NATIVE`
(`command grep -c ' /DHX_NATIVE' build.ninja` = 0). Leg B recompiled 2 TUs.

```
leg A: matched=54939 masked=25214 honest=29725 code%=59.312576  (recompiles: 0, settled)
leg B: matched=54939 masked=25214 honest=29725 code%=59.312576  (recompiles: 2, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   (legA 64.316605 -> legB 64.316605)
units at 100% [mpn ruler]: legA 626 -> legB 626  (Δ+0; 0 reached 100, 0 fell off; pairable units 1753->1753)
```

The `none` control was +0 B. The tool restored the tree and verified it. That restore
also deleted this doc, which had been written during the run; the doc was then written
again.

## 8. Not done

- **SetState arms that hand off** to `TheMemcardMgr`, `TheProfileMgr`, `TheSongMgr`,
  `TheUIEventMgr`, `TheEntityUploader` or `GetNewSigninProfile` are not sent. That covers
  0x2–0xD, 0x12, 0x14, the dialog-event arms, 0x37–0x38, 0x40–0x44, the save and
  manual-load arms, 0x51–0x59 and `kS_LoadComplete`. All of them are linked against the
  loud stubs in §3, so one sent by mistake aborts instead of passing.
- **`MaybePublish`'s pad-log hex path (≥ 0x1A events, `CompressMem`) and the
  `exit_stats` handler pairs** feed `screenExit` only, which retail never records. Their
  one observable effect, whether the screen counts as dropped, is covered for the empty
  case only.
- **A participating local user** is not sent (`PlatformMgr::GetOnlineID` is XUser-only).
- **`native_runtime_rank.py` was not rerun.** It needs an instrumented build, and memory
  was short. Entry of the gated rows is shown by the gates themselves: every non-fixture
  gate fails when the body under test is broken (§4).
- **The other 60 of the 69 rows** were not taken. The largest are `JoypadPollCommon`,
  `CharKeyHandMidi::Poll`, `BandWardrobe::LoadMainCharacters`, `GemTrack::DrawFill`,
  `NextSongPanel::FillExpandedDetails`, `LayerDir::RefreshLayer` and `Locale::Init`.

## 9. Result lines

```
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=200 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=13 scatter_dirb=0 scatter_multihost=12 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
```

The scatter counts were not attributed file by file. This lane changed one scatter
include, the `HamMove.cpp` include in `CrowdAudio.cpp`, which is now off natively.

The native build gate line is in the lane's final report. The gate was run as the last
action, after this doc was committed.

## Reproduce

```
cmake --build native/build --target rb3-render
native/build/rb3-render ~/code/milohax/rb3/orig-assets/xbox-zip <outdir>   # W16-TS phase runs by default
tools/native_health.sh <worktree>
tools/native_build_gate.sh
```
