# MENUWALK: rb3-app driven into the menus no walk had reached (2026-11-23)

Lane menuwalk, branch `menuwalk` (from main `24024b599`), worktree `~/tmp/wt-menuwalk`.
Scratch, logs and binaries: `~/tmp/menuwalk/` (`r_<tag>/run.log`, `r_<tag>/screens.txt`).
Walk scripts and wrappers are committed in `tools/rb3_app_walks/menuwalk/` (`runb.sh` is a copy
of `rb3-scout/a25/runb.sh` with `A=~/tmp/menuwalk`; `disc.sh <tag> <bin> <walk> [frames] [env…]`
runs one walk with `RB3_APP_DETERMINISTIC=1 RB3_APP_FOCUS_LOG=1`). Every run went through
`coord/no610m.sh` under the shared `gpu1.lock`, on a fresh reflink copy of `memcard/content_a`,
default (vocals) pad unless noted. The full native selftest was not run (coordinator's job).

## Commits

| commit | what |
|---|---|
| `aae695051` | `RB3_APP_FOCUS_LOG=1` (opt-in, read-only): `rb3-app: frame N focus=<panel>/<component> [sel=pos:sym]` and `rb3-app: frame N overshellK state=<id> view=<view> focus=<list> sel=…` lines on change; the walk scripts |
| `88e82767f` | **fix**: rb3-app's SystemInit names `virtual_keyboard` (`TheVirtualKeyboard.Init`, retail 0x825113C4) |

No change is under the match build's compile set except `src/system/os/System.cpp`'s
`RB3_NATIVE_SYSTEM_NET_MGRS` arm, which the match build does not compile; `ab_measure` not run.
`rb3_app_link_stubs.s` untouched (no stubbed function was defined).

## Binaries and engine state (coordinator note: engine moved to `2dcbb17` mid-lane)

| binary | sha256[:12] | tree | engine |
|---|---|---|---|
| bin-m1 | 64c5b64fb668 | focus log v1 | `6b34583` |
| bin-asan-m1 | 708de3be1596 | focus log v1, `-DRB3X_SANITIZE=address,undefined` | `6b34583` |
| bin-m4 | d7a3d2e18c35 | `aae695051` | `2dcbb17` (its build recompiled the two changed engine files) |
| bin-m5 | 6885463b2975 | `88e82767f` | `2dcbb17` |
| bin-asan-m5 | f27769ade707 | `88e82767f`, ASan+UBSan | `2dcbb17` (built, not run) |

The only before/after pair (VirtualKeyboard fix) is bin-m4 vs bin-m5, both on `2dcbb17`.
`2dcbb17` changes only rect drawing into texture targets, which the det hash does not cover.

## Method

Prediction at the start: the screens past the quickplay path would show link-stub aborts. **Wrong:
there were none.** Over 30-odd walks, the result was 0 `UNREACHED` stubs and 0 `^FAIL` lines. The
work went into making the menus observable. The det hash and frame line record only screen
names, and the overshell is an overlay rather than a screen, so the first options walks
(`d_opt*`, bin-m1) showed no change at all. The focus log showed that the vocals pad's overshell
is **slot 2**, not slot 0, and gave every list position by name. Each list order below was read
from it.

## Screens reached and verdicts

| area | walk | screens reached (new ones in bold) | verdict |
|---|---|---|---|
| Practice (vocals) | `d_practice`, `w_practice` (+AUTOPLAY) | main_hub (mb_trainers → training_band_practice) → song_select → part_difficulty → **practice_preloading → practice_pre_sel_section → practice_sel_section** (sel 2 `full_song`) → preloading → **practice_game_screen** | **works**. Autoplay scores (5.0 stars by frame 9000). In-game overshell `options_in_game`: return / restart / practice_new_section / change_diff. **restart** goes through **restart_sync_audio_net_screen → sync_audio_net_screen** and back to practice_game. At the section end (152.3 s) the song loops to 0 and the score resets (frames 9777 and 14348). That is retail: practice mode is `end_with_song FALSE` (`config/modes.dta`) and practice.dta's `loop` handler does `jump_to_start`. `practice_vocal_end_screen` was not reached, because the loop never ends |
| Practice (guitar) | `d_practice` KIND=guitar | as above, plus **practice_sel_speed_screen** (`speeds.lst`) | works so far. The walk ran out of presses on the speed list |
| Overshell options | `d_opt0-2`, `d_avlist`, `d_mods` | Start on main_hub: slot 2 `options` (return, register_online, change_character, game_options, leave) → `game_options` (mod_auto_vocals, mod_no_fail_band, vocal_options, av_settings, extras, modifiers) → `options_av_settings` (dolby, bass_boost, overscan, calibration, autosave_enabled, audio_options) and `modifiers` (drum_surface_navigation, drum_fills, doublespeed, independent_track_speeds, enable_popup_help, chord_numbers, nohud, miss_sounds, awesome, …) | navigation works and each toggle press is taken. Toggle effects were not checked |
| Calibration (from options) | `w_calib` | av_settings sel 3 → confirm_action yes → **meta_loading_calibration_screen** → cal_welcome → cal_manual_audio (right/left) → cal_manual_video → cal_manual_complete → main_hub | **works**. When options are reopened, focus is back on the last entry (`sel=3:overshell_game_options`) |
| Extras / credits | `d_extras`, `w_credits` | extras → credits → confirm yes → **meta_loading_main_credits_screen → credits_movie_screen → credits_screen** | **stuck screen (open)**: see finding 2 |
| Music library: setlist browser | `d_setlist`, `w_setlist` | pn_quickplay → qp_setlist → song_select_screen (setlist browser). Confirm on entry 4 → part_difficulty → game_screen → **lose_screen** (`lose_resume.btn`; the song was played with no autoplay and failed) | works: lose_screen is reached and has focus. The browser rows have empty `DataSymbol` (`sel=N:`), so the node names are unread |
| Song select extras | `d_songsel` | y/x/select/rb/lb on song_select: no screen change. b goes back to main_hub | inconclusive. The details and sort sub-panels are not screens, and the log cannot see them |
| Career | `d_hub1` | mb_career → career_progress → **tv6_a_screen → hint_career_screen → accomplishment_screen** (groups.lst → categories.lst) | works |
| Manage band | `d_hub3` | mb_shop → customize_band → **manage_band_screen** (edit_band_name.btn) | **bug, fixed**: finding 1 |
| Music store | `d_hub4`, `w_hubtour` | mb_musicstore → dlc_store → **main_store_signin_screen**. opt1 (sign in) stays on the dialog, opt2 (cancel) pops to main_hub | as on retail with no Live account: opt1 calls `platform_mgr signin`, and the dialog stays until a Live sign-in arrives |

Counts over all runs: `^FAIL` 0, `UNREACHED` stubs 0. ASan reports: 0 in five ASan+UBSan walks
(bin-asan-m1: S_calib, S_setlist, S_credits, S_prac with autoplay, S_mods; 3,000-7,000 frames each).

## Findings

### 1. `virtual_keyboard` did not exist in rb3-app (fixed, `88e82767f`)

Pressing `edit_band_name.btn` on manage_band_screen logged `virtual_keyboard not function or
object (file ui/band/manage_band.dta, line 95)` 4 times. rb3-app's SystemInit arm
(`RB3_NATIVE_SYSTEM_NET_MGRS`) skipped `TheVirtualKeyboard.Init` ("stay skipped here"), even
though rb3-app compiles `os/VirtualKeyboard.cpp` with the HX_NATIVE no-keyboard bodies. Retail
check (`tools/va_disasm.py 825112e8 400`): SystemInit calls `bl 0x82527CD8`
(`VirtualKeyboard::Init`) at 0x825113C4, after `0x8251D378` (the platform manager init), before
the content manager. Five UI files address the object: manage_band, character_creator, the
overshell slot_states, song_select_extras (setlist naming) and token_redemption. After the fix
(bin-m5 vs bin-m4, same walk `d_hub3`, both on engine `2dcbb17`), the count is 0. The det walks
are identical through frame 1104, the press itself, and differ from 1106 (rand 113/216 → +2
draws). The press now evaluates the script's arguments (`get_default_vk_name`). Natively no
keyboard UI opens: `ShowKeyboardUI` returns 0, so nothing calls back. That is the remaining
native gap, and it is not a regression.

### 2. credits_screen never ends or exits (open; `src/system/meta/CreditsPanel.cpp` + `ui/UIList`, listed, not edited)

credits_screen is reached at frame 1117 with `credits.lst sel=29`. Over 40,000 frames (`credlong`,
about 1,300 s of virtual UI time) the selection never changes and `credits_done` never fires, so
the screen never pops. CreditsPanel::Poll fires `credits_done` once
`!mList->IsScrolling()` after `AutoScroll()`, so the list either never scrolls or always reports
that it is scrolling. While it auto-scrolls, `b` and `a` are swallowed by design (retail
credits.dta shows a hint). Start opens the pause menu with skip.btn, but no walk pressed Start
there. Next step: log `UIList::IsScrolling` and the first-showing index in credits, and
compare the auto-scroll speed source (`UIListState` auto-scroll) against retail. This lives in
`src/system/ui`, which this lane does not edit.

### 3. New UBSan sites versus gemwalk S and scorewalk S2 (24 file:line sites in all, 21 already known)

- `src/band3/bandtrack/VocalTrack.h:97`: `IsCurrentVocalParam` loads `mCharOptParam`
  (+0x220) uninitialised (0xBEBEBEBE, ASan's malloc fill). The stack is ButtonUp →
  `VocalPlayer::HandleDeactivateVolume` → `VocalTrackDir::DeactivateVolume`, seen in S_prac and
  S_setlist: a real pad press during a vocal song, which autoplay walks never make.
  **Retail-faithful, not fixed:** the retail ctor `??0VocalTrack` (0x82BA9A28) stores
  +0x224 (`mCharOptMicID = -1`, at 0x82BA9BE4) and nothing to +0x220. Our ctor matches.
- `src/system/bandobj/StreakMeter.cpp:222` (`NumActiveParts`): a bool loads 190 (0xBE,
  uninitialised). Reached from `VocalTrackDir::ShowPhraseFeedback` ← `VocalPlayer::LocalScorePhrase`
  in practice (S_prac only). Listed, not checked against retail (bandobj; out of time).
- `milo-native-engine/src/platform/Keygen_Stub.cpp:63`: signed overflow in `random(int)` under
  `KeyChain::getMasher` ← `VorbisReader::setupCypher`. Engine-side, and benign as long as the
  arithmetic is meant to wrap (it should use unsigned). Listed for the engine.

### 4. Listed, out of this lane's scope

- `rock_central not function or object` (song_select.dta lines 53 and 103) on every song-select
  entry, also 8 times per gemwalk quickplay walk. RockCentral is GATED (ring online-rockcentral,
  net, out of scope).
- Song-select sub-panels (details, sort) and the setlist-browser node names cannot be seen: the
  setlist rows' `DataSymbol` is empty. A `music_library` highlighted-node line in the focus log
  would show them.

## What was not done

No toggle (dolby, overscan, modifiers) was checked for effect, the practice speed list and the
practice_vocal_end path were not walked, and drum freestyle, trainers and pro song lessons were
not walked (they need a drums, pro or keys pad). Credits' Start → skip was not pressed. No walk
ran on an ASan build of the fixed tree: bin-asan-m5 is built but was not run, and finding 1's
before/after used the plain bin-m4 and bin-m5. The
native health selftest was left to the coordinator.
