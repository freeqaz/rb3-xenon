# MODWALK: rb3-app's gameplay modifiers checked against retail (2026-11-23)

Lane modwalk, branch `modwalk` (from main `7a05edbf4`), worktree `~/tmp/wt-modwalk`. Scratch,
logs and binaries: `~/tmp/modwalk/` (`r_<tag>/run.log`, `r_<tag>/screens.txt`, decoded retail
dtbs in `dta/`). Walk scripts and tools: `tools/rb3_app_walks/modwalk/`. Engine
`milo-native-engine` `0090e1a`. All runs went through `coord/no610m.sh` (via `runb.sh`). The
full native selftest was not run (coordinator's job).

**Verdict: no native divergence found.** The modifiers retail offers are 11, not the list in the
brief. Seven were measured in a walk, and each sets exactly the value retail's code computes.
Four were checked statically only. No `src/` file changed, so `ab_measure` was not needed and
was not run. `rb3_app_link_stubs.s` was not touched: no modifier path is stubbed.

## Commits

| commit | what |
|---|---|
| `fe5abc952` | `RB3_APP_MOD_LOG=1`, an opt-in read-only line in `native/src/main_app.cpp`. Walk scripts `m_base/m_mods/m_indep` + pads `p1-p3`, runners `build.sh/runb.sh/disc.sh`. Tools `xref.py` (retail code sites that form a string's address: `lis`+`addi` within 12 instructions, mapped to their function through `.pdata` and the map) and `callers.py` (retail `bl`/`b` sites that target a VA) |
| (this commit) | this record |

The log is `rb3-app: frame N mods active=<modifiers> nofail=<MetaPerformer::IsNoFailActive>
fills=<Game::mDrumFillsMod> p=miss<GemPlayer unk408>/fill<BeatMatcher mFillLogic>,…
hud=perf<mPerformanceMode>/dbl<mDoubleSpeedActive>/indep<mIndependentTrackSpeeds>
view=<instrument>:<TrackDir::ViewTimeSeconds>,…`. It prints one line each time the text
changes. It lives in `native/src`, which the match build does not compile.

## What retail offers (the brief's list is partly Deluxe)

`config/gen/modifiers.dtb` from the clean ark (`extracted-xbox-full`, decoded with trainwalk's
`dtb.py`; the TU5 patch does not replace it):

```
mod_no_fail_band (custom_location save_value use_save_value)   mod_auto_vocals (custom_location)
mod_drum_surface_navigation (default_enabled …)                 mod_drum_fills (default_enabled … delayed_effect …)
mod_doublespeed (save_value)                                    mod_independent_track_speeds (save_value)
enable_popup_help (default_enabled …)                           mod_chord_numbers
mod_nohud (save_value delayed_effect)                           mod_miss_sounds (default_enabled … delayed_effect)
mod_awesome (save_value)
```

"Auto kick" (`mod_auto_kick`) and the other extra toggles exist only in **RB3 Deluxe's**
modifiers.dtb (36 entries, `rock-band-3-deluxe/obj/xbox/ark/config/gen/modifiers.dtb`). Retail has
no double-bass-pedal modifier in either list. The two `custom_location` entries sit in the overshell's
`game_options` list (sel 0 auto vocals, sel 1 no fail). The other nine make up the `modifiers` list,
in file order (focus log: sel 0 `mod_drum_surface_navigation` … sel 8 `mod_awesome`).

## Consumers: retail versus our source

`xref.py` lists every retail code site that addresses each `mod_*` string. Each site's function
has the same lookup in our source, and none is missing:

| modifier | retail consumer functions (TU5 VA) |
|---|---|
| mod_no_fail_band | MetaPerformer::IsBandNoFailSet 0x8257B620, SetBandNoFail 0x8257DF30, RockCentral::RecordOptionData. IsNoFailActive 0x8257CF40 is called from Game::UpdatePausedState, Player::CheckCrowdFailure, BandPerformer::SetCrowdMeter, Track::IsNoFailActive, AccomplishmentManager::EarnAccomplishment, Handle (`callers.py`) |
| mod_auto_vocals | 17 sites: TrackPanel::Reset, GameConfig::AssignTracks/AutoAssignMissingSlots, InputMgr, OvershellPanel ×2, OvershellSlot ×3, MainHubPanel, JoinInvitePanel, SelectDifficultyPanel, MetaPerformer::SelectRandomVenue, TourPerformerLocal::SelectVenue, ModifierMgr ×2, RockCentral |
| mod_drum_surface_navigation | ShellInputInterceptor::FilterAction 0x825ACA70, RockCentral |
| mod_drum_fills | Game::Restart 0x8267D1E8, RockCentral |
| mod_doublespeed / mod_independent_track_speeds | TrackPanelDirBase::UpdateTrackSpeed 0x82357A70 (+ ModifierActive) |
| mod_chord_numbers | Gem::AddChordInstance 0x82BACC48 |
| mod_nohud | TrackPanelDir::Reset 0x82308788, Tracker::StartIntro 0x826D0E30 |
| mod_miss_sounds | GemPlayer::Restart 0x826C7260, RockCentral |
| enable_popup_help | **no code site** (the string is not in band.exe). Only `ui/track/popup_help.dta` line 29 reads it |
| mod_awesome | **no code site** (not in band.exe) and no script reads it. The only reference is its row in modifiers.dta |

Every consumer and every effect function on these paths is fuzzy 100 / mpn 100 in
`build/45410914/report.json` (main, `name_check`): IsBandNoFailSet, IsNoFailActive,
SetBandNoFail, CheckCrowdFailure, SetCrowdMeter, UpdatePausedState, Game::Restart,
GemPlayer::Restart, PlayMissSound, UpdateTrackSpeed, ModifierActive, TrackPanelDir::Reset,
Tracker::StartIntro, AddChordInstance, FilterAction, ToggleModifierEnabled, ModifierMgr::Handle,
AssignTracks, AutoAssignMissingSlots, TrackPanel::Reset. The only sub-100 rows in the area
are `ModifierMgr::IsModifierActive(Symbol)` 79.67 and `IsHidden`/`IsActive` 28.57. In each the
residual is the one-null-state EH frame that the in-source note documents, not logic. rb3-app
compiles all of these TUs from this repo's `src/`
(`native/CMakeLists.txt` lists the bandobj files). The only `HX_NATIVE` arm on these paths is
`Player::CheckCrowdFailure`'s `|| !mUser` (headless users). It takes the branch retail's null
user takes and does not touch the no-fail test.

## Walks

Three deterministic band walks (`disc.sh`, 20,000 frames, `RB3_APP_DETERMINISTIC=1
RB3_APP_AUTOPLAY=1 RB3_APP_AUTOPLAY_SKIP=3`, binary bin-m0 `0070ff006b7b` = `fe5abc952`). Pad 0
is vocals and runs the walk. Pads 1-3 are guitar (easy), drums (easy) and bass (medium,
**not autoplayed**, so it misses everything and can fail the band). Pad 0 opens its overshell
(slot 2) on main_hub, goes to game_options and modifiers, and backs out. The press timeline is
the same in all three walks; only the toggling A presses differ:

- `m_base`: no toggle.
- `m_mods`: no fail; list entries 1 drum_fills (off), 2 doublespeed, 4 popup help (off),
  5 chord_numbers, 6 nohud, 7 miss_sounds (off), 8 awesome.
- `m_indep`: no fail; doublespeed + independent_track_speeds.

Prediction before running: every logged value would follow retail's formula, and m_base
would end on lose_screen (as menuwalk's no-autoplay setlist walk and deploywalk's FX walk did).
Both held.

| tag | toggles taken (focus/mods log) | in-song values (first `game_screen` frames) | outcome |
|---|---|---|---|
| base | active = drum_surface_navigation, drum_fills, enable_popup_help, miss_sounds (the four `default_enabled`) | nofail=0 fills=1 p=miss1/fill0 ×3 hud=perf0/dbl0/indep0 view guitar/drum/bass **2.267** | **lose_screen** at frame 7093. Bass crowd −2.20, bass disabled (`en 1`), band crowd 0.199 at frame 7000 |
| mods | + no_fail_band (frame 866), doublespeed, chord_numbers, nohud, awesome; − drum_fills, popup_help, miss_sounds | nofail=1 **fills=0 p=miss0/fill2** ×3 **hud=perf1/dbl1** view **1.511** ×3 | **coop_endgame** (frame 7755). Bass crowd held at 0.2000 and still enabled (`en 0`), band crowd 0.80 |
| indep | + no_fail_band, doublespeed, independent_track_speeds | nofail=1 fills=1 miss1/fill0 hud=perf0/dbl1/**indep1** view guitar **1.600**, drum **1.600**, bass **1.333** | **coop_endgame** (frame 8062). Bass crowd 0.2000, enabled |

Retail's formula for each value:

- **View times.** `track_paneldir.dta` `view_times` gives easy 2.4 and medium 2.0 for
  guitar/drum/bass. In the shared (non-independent) case, UpdateTrackSpeed averages the local
  tracks: (2.4+2.4+2.0)/3 = **2.267**. Double speed divides by the constant retail loads with
  `lfs f24, 0x2820(r11)` = 0x82022820 = **1.5**; the other leg is 0x820009FC = 1.0
  (`va_disasm.py 82357B28`). 2.267/1.5 = **1.511**. With independent speeds the division is per
  track: 2.4/1.5 = **1.600** and 2.0/1.5 = **1.333**. All match to the printed precision.
- **Drum fills off.** `Game::GetFillLogic` gives kFillsDeployGemAndInvisible (2) on all three
  gem players, and GemManager's `unkb8` copies `mDrumFillsMod`.
- **Miss sounds off.** `unk408` = 0, which gates PlayMissSound.
- **nohud.** `mPerformanceMode` = 1. TrackPanelDir::Reset then calls `SetShowing(!mPerformanceMode)`.
- **No fail.** When the bass player falls below the lose level, `CheckCrowdFailure` sets display
  value 0 and does not disable the player, and `SetCrowdMeter` does not call `Lose()`.

Caveats. The toggle presses draw from the RNG, so the three walks quickplay different songs
(beat 299.8 vs 393.6 at frame 7000). The no-fail contrast therefore rests on the bass crowd and
enabled state, not on identical songs. `FAIL` lines: 3 per walk, all
`NetSession.cpp:763 IsUserSignedIn(newUser)` when the extra pads join (network, out of scope).
`UNREACHED` 0 in every walk.

## Verdicts

| modifier | verdict | how |
|---|---|---|
| mod_no_fail_band (No Fail) | **as retail** | walk: band does not lose; the failing bass player stays enabled at the crowd floor. Without it the band loses |
| mod_doublespeed (Breakneck Speed) | **as retail** | walk: view time ÷1.5 (retail constant 0x82022820) |
| mod_independent_track_speeds | **as retail** | walk: per-track view times, ÷1.5 with doublespeed |
| mod_drum_fills (fills off) | **as retail** | walk: fill logic 2 on every gem player, `mDrumFillsMod` 0 (delayed effect, applied at Game::Restart) |
| mod_miss_sounds (off) | **as retail** (flag) | walk: GemPlayer miss flag 0. The sound itself was not observed (the det log does not hash audio) |
| mod_nohud (Performance Mode) | **as retail** (flag) | walk: `mPerformanceMode` 1. Hiding the HUD group is render-only and was not observed. Tracker::StartIntro is 100% |
| mod_awesome (Awesomeness Detection) | **as retail: no effect** | retail has no consumer (string absent from band.exe, no dta reads it). Natively the toggle registers and no logged value moves |
| enable_popup_help | **static** | only `popup_help.dta` reads it (same script natively). Toggled off in m_mods; popups not observed |
| mod_chord_numbers | **static** | only Gem::AddChordInstance (100%) reads it, pro guitar only. The band walk has no pro guitar |
| mod_drum_surface_navigation | **static** | FilterAction (100%) reads it, and so does the overshell script's warning. The pad-shift/cymbal-shift bits come from the `controllers` config (`JoypadTypePadShiftButton`), and a script cannot press two buttons in one frame, so it was not walked |
| mod_auto_vocals | **static** | all 17 retail sites are present in source at 100%. Not toggled: it is offered next to a real vocalist, and the walk keeps pad 0 on vocals |

## Listed, not done

- Not walked: drum surface navigation, chord numbers, popup help, auto vocals, or any
  in-song toggle (the `delayed_effect` dialog path, `show_modifier_delayed_effect`).
- Render-only and audio effects (HUD group hidden, miss sound cues) were not observed. The walks
  check the gating values.
- Nothing in `BandCharacter.cpp`, `src/system/ui`, `rndobj` or `char` is involved in any modifier
  path found here.
