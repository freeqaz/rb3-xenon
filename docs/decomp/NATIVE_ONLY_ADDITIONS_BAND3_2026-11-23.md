# Native-only additions in band3: one-sided `HX_NATIVE` blocks judged against retail (2026-11-23, lane `narms4`)

The three `NATIVE_ARM_DIVERGENCE_SWEEP*` lanes judged two-sided `HX_NATIVE` conditionals and
`NATIVE_SKIPPED_RETAIL_BLOCKS_2026-11-22.md` judged X360-only blocks native skips. This lane
judges the remaining class in `src/band3`: native-only additions, a `#ifdef HX_NATIVE` /
`#if defined(HX_NATIVE)` / `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` block with no
`#else`, in a file some native target compiles. The question is the same: where does native
do something retail (`band.exe`, read with `tools/va_disasm.py`) does not?

## Method

- **Native closure.** `~/tmp/narms3/mdeps.json` (lane narms3's `clang -MM -MG` over all 5,054
  native compile commands, built today) filtered to `src/band3/`: **597 files** (meta_band 351,
  game 149, tour 54, bandtrack 28, net_band 15). No band3 `char`/`bandobj` file is in it.
- **Enumeration.** narms3's `arms.py` over `src/band3` (`~/tmp/narms4/arms.py`): 310
  `HX_NATIVE` conditionals, 309 in the closure, **194 one-sided native-true** (156 `HX_NATIVE`,
  31 `MILO_DEBUG && HX_NATIVE`, 7 other spellings). `~/tmp/narms4/dump.txt` holds every block
  with three lines of context; the table below gives each one a verdict.
- **Retail reference.** `tools/va_disasm.py` on `band.exe`, plus `~/tmp/narms4/bls.py <va>`,
  which lists every `bl` in a retail function (size from the next map address) with its map
  name, and a byte search of `band.exe` for strings. Addresses are TU5.
- **What counts.** A block is a divergence when it changes state, calls or control flow on a
  path the app takes with its default settings. Env-gated logs (`GEM_DBG`, `K8_DBG`,
  `GAME_DBG`, `PANEL_DBG`), dev cheats whose flag defaults off, script handlers retail lacks,
  and null guards that only fire in the headless scoring drivers were recorded but not
  changed. Dev-build blocks (`MILO_DEBUG && HX_NATIVE`, the house pattern in
  `docs/decomp/patterns/milo-debug-force-define.md`) were held to the same test: warnings and
  prints stay, behaviour changes go.

## Fixed (commit `42ee99652`; native now runs the retail body)

| site | retail | native before |
|---|---|---|
| `game/TambourineManager.cpp` `Poll` | 0x826FB308 goes from the `unk60`/index guard straight to `MsToTick` (bl 0x827C90B8); no mic access | DC3's mic mute: on the first tambourine window, gain 0 and volume -96 on mics 0-2, never restored (the restore half was a flag reset only). A real singer's mic went silent for the rest of the song. The `unk74` member and its init went with it |
| `game/Band.cpp` `AddPlayerDynamically` | 0x8269C760 returns right after `UpdateBonusLevel` (bl 0x8269AE38, then epilogue) | dev-build tail copied the band's multiplier-active and crowd-meter state into the drop-in player |
| `meta_band/OvershellPanel.cpp` ctor / dtor | 0x825B4AA8: six AddSink (GameMicsChangedMsg + five untyped); 0x825B6188: the mirror six RemoveSink | dev-build typed ThePlatformMgr sinks for InviteReceived/InviteExpired/NetStartUtilityFinished on top of the untyped `ThePlatformMgr.AddSink(this)`, so NetStartUtilityFinishedMsg reached the panel twice; the dtor removed the first two from TheWiiFriendMgr, leaving them dangling on ThePlatformMgr |
| `meta_band/OvershellPanel.cpp` `ResolvePartWaitStates` | runs the part resolver for a lone uncontested user: picks that user, and if local sets kState_ChooseDiff | native-only shortcut set kState_ChooseDiff directly, for a remote user too. The ChoosePartWarn loop it worked around came from a decomp error already fixed in the resolver (`li r4, 0xc`) |
| `meta_band/OvershellPanel.cpp` `EndOverrideFlow` | assert stripped: an inactive flow is still cleared, `OvershellOverrideEndedMsg` exported, `UpdateAll` | returned early. Natively `MILO_ASSERT` aborts, so the native arm now `MILO_WARN`s and runs retail's body (two-sided arm; the X360 arm keeps the assert text) |
| `meta_band/OvershellSlot.cpp` `UpdateView` | 0x825DB930: no `update_restart_allowed` (string absent from band.exe), no bl to `UpdateProfilesList` (0x825DEEC8) | sent `update_restart_allowed`, which the overshell script uses to enable/disable `overshell_restart`, and reloaded the swappable-profiles provider on every view update. A dead native-only `b1` local went too |
| `meta_band/OvershellSlot.cpp` dtor | 0x825E1490: one RemoveSink (LocalUserLeftMsg) | also removed a TheServer UserLoginMsg sink no build adds |
| `game/GemPlayer.cpp` `FilteredWhammyBar` | fn_826C17D8: range assert stripped, `val` used as given | clamped `val` to [-1,0], hiding bad whammy input from the pitch bend and whammy-speed logic. Now warns once on an out-of-range value and runs the unclamped body (two-sided arm) |
| `net_band/EntityUploader.cpp` `HasServerTimedOut` | 0x8250D4C8: no `unk34` load before `GetDateAndTime` (bl 0x825227E8) | dev-build early return counted an aborted upload as a server timeout |
| `meta_band/MusicLibrary.cpp` `OnEnter` / `OnExit` | fn_82542238 / fn_82542A00: ten sinks each, no FriendsListChangedMsg | native-only FriendsListChangedMsg sink on ThePlatformMgr; Handle has no arm for it, so it only cost a dispatch |

Native compile check: every native compile command for the eight touched TUs plus
`VocalPlayer.cpp` (which embeds `TambourineManager`), 21 commands, `-fsyntax-only` against a
fresh configure of the worktree (`~/tmp/narms4/nb`, `~/tmp/narms4/synchk.py`): 0 failures.
No native source names `TambourineManager::unk74`.

## Measurement

`python3 tools/ab_measure.py --worktree ~/tmp/wt-narms4 --revert 42ee99652` (leg A = the fix,
leg B = reverted; run dir `~/tmp/wt-narms4/.ab_measure_runs/20261123-021130-narms4-revert-3152176`):
name_check ruler matched 55,268 / masked_equal 25,360 / honest 29,908 / code 59.665310% on
both legs, `none` ruler 55,322 / 60.032020% on both, units at 100% 635 -> 635. **Δ0 on every
reading**, leg B recompiled 23 objects. Expected: every edit is inside an `HX_NATIVE` arm or
a comment, so the X360 code is unchanged.

Not done: `tools/native_health.sh --selftest` (excluded by the brief), and no native binary
was run. `ninja rb3-vocal2 rb3-harmony rb3-app` in a fresh configure of the worktree (`~/tmp/narms4/nb`, log `~/tmp/narms4/nbuild.log`) compiled 1,409 edges and linked all three, rc 0, 0 `error:` lines. The next lane should run `rb3-vocal2`/`rb3-harmony` on a chart that has tambourine gems (through `no610m.sh`) to see the mic-mute removal, and step the overshell part pick with one local player to see the resolver path.

## Flagged and left (owned elsewhere, or the fix needs a root cause first)

- **Other session (Rnd/Draw):** `bandtrack/TrackPanel.cpp:683` adds
  `TheRnd.ClearDepthForOverlay()` before the HUD draws (retail draws with no depth clear);
  `meta_band/SongSelectPanel.cpp:53/68/150/168/180` force `live_lb.grp` / `live_diffs.grp`
  showing because the native renderer ignores the env-alpha hide fade.
- **Other session (char):** `meta_band/CharCache.cpp:80/126` null guards for the deferred
  chars.milo; `CharCache.cpp:235` prefab patch-tex arm (dev toggle, default off).
  Char-adjacent: `meta_band/CharacterCreatorPanel.cpp` 284-566, nine null-`mPreviewDesc`
  guards retail lacks; they fire only if a getter runs before Enter sets the desc, so they
  could hide a native ordering bug.
- **Net area (offline shims):** `meta_band/Matchmaker.cpp:272/288` (null `SessionSettings`
  offline), `net_band/RockCentral.cpp:374/382` (null `mContextWrapperPool`, because
  `RockCentral::Init` is gated off natively), `meta_band/AppMiniLeaderboardDisplay.cpp:91`
  (offline leaderboard fails at once instead of waiting forever). Each runs the retail path
  online.
- **`game/GamePanel.cpp:312` `StartGame`:** forces `TrackPanelDir::SetShowing(true)` at song
  start because natively `play_intro` fires late or not at all. It is a forced value that hides
  a real native bug in the intro anim/camshot machinery, but removing it would leave the HUD
  hidden until that is fixed, so it stays until the intro path is fixed.
- **`game/GameMicManager.cpp:176` `GetMicCount`:** returns 4 in frame_rate mode (retail
  0x8235BA70 is `lwz r3,0x34(r3); blr`). It is DC3's fake-mic feature, reached only from the
  dev frame-rate screen, and it pairs with the two-sided `GetMic` arm; left as one feature.
- **`meta_band/BandSongMgr.cpp:415` `GetAlbumArtPath`:** falls back to the blank texture when
  `<song>_keep.png` is not on disk (retail returns the path). It diverges only when the extract
  lacks the asset.
- **Headless null guards** (`game/Player.cpp`, `Singer.cpp`, `CommonPhraseCapturer.cpp`,
  `VocalPlayer.cpp:2088`): null-`BandUser` / null-`TrackPanel` early-outs for the headless
  scoring drivers. They never fire when the object exists, but in the app they would hide a
  missing TrackPanel rather than crash.

## Not divergent (recorded)

Everything else either does not run with default settings or does not change behaviour:
env-gated logs, dev cheats whose flag defaults off (`sUnlockAll`,
`sFakeLeaderboardUploadFailure`, `fake_controllers`, `send_fake_patches`,
`vocal_frame_spew`, prefab toggles), script handlers retail lacks (reachable only by name),
frame-spew and overlay writes behind pointers that stay null, harness-only constructors,
include/linkage/layout lines, and two equivalents (`SongSortMgr::BuildSetlistList`'s return sits
at the function end and skips only the stripped assert; `SongDB::SetupCommonPhrasesForTrack`'s
bound changes no entry below `gems.size()`). `OvershellSlot::CancelLinkingCode`'s
`unk80`/`unk81` clears and the native UserLoginMsg arm are inert because `unk80` is never set
true anywhere.

## Every block

Numbering follows `~/tmp/narms4/one.json` / `dump.txt`. Paths are under `src/band3/`; the line
is the opening `#if`.

| # | block | verdict |
|---|---|---|
| 0 | `bandtrack/NowBar.cpp:3` | build: include / declaration / linkage / layout, no behaviour |
| 1 | `bandtrack/NowBar.cpp:27` | diag: `GEM_DBG` env log only |
| 2 | `bandtrack/NowBar.cpp:39` | diag: `GEM_DBG` env log only |
| 3 | `bandtrack/TrackPanel.cpp:10` | build: include / declaration / linkage / layout, no behaviour |
| 4 | `bandtrack/TrackPanel.cpp:361` | diag: `GEM_DBG` env log only |
| 5 | `bandtrack/TrackPanel.cpp:586` | diag: `K8_DBG` env log only |
| 6 | `bandtrack/TrackPanel.cpp:616` | diag: `K8_DBG` env log only |
| 7 | `bandtrack/TrackPanel.cpp:683` | **other session (Rnd/Draw)**: added `TheRnd.ClearDepthForOverlay()` before the HUD draws; listed only |
| 8 | `bandtrack/GemTrack.cpp:9` | build: include / declaration / linkage / layout, no behaviour |
| 9 | `bandtrack/GemTrack.cpp:108` | diag: `GEM_DBG` env log only |
| 10 | `bandtrack/GemTrack.cpp:120` | diag: `GEM_DBG` env log only |
| 11 | `bandtrack/GemTrack.cpp:679` | diag: `K8_DBG` env log only |
| 12 | `bandtrack/GemTrack.cpp:868` | diag: `GEM_DBG` env log (derefs `GetPlayer()` only when the env var is set) |
| 13 | `bandtrack/GemRepTemplate.cpp:3` | build: include / declaration / linkage / layout, no behaviour |
| 14 | `bandtrack/GemRepTemplate.cpp:55` | diag: `GEM_DBG` env log only |
| 15 | `bandtrack/GemRepTemplate.cpp:145` | diag: `GEM_DBG` env log only |
| 16 | `bandtrack/VocalTrack.cpp:74` | build: `static` linkage for a per-TU global, value identical |
| 17 | `bandtrack/VocalTrack.cpp:192` | dev diag: warn/log/dump only (`MILO_DEBUG && HX_NATIVE`) |
| 18 | `bandtrack/VocalTrack.cpp:326` | dev diag: warn/log/dump only (`MILO_DEBUG && HX_NATIVE`) |
| 19 | `bandtrack/VocalTrack.cpp:2560` | dev diag: warn/log/dump only (`MILO_DEBUG && HX_NATIVE`) |
| 20 | `bandtrack/VocalTrack.cpp:2582` | dev diag: warn/log/dump only (`MILO_DEBUG && HX_NATIVE`) |
| 21 | `game/TambourineManager.h:63` | **FIXED**: DC3 mic-mute state member, removed with the mute (see #70-71) |
| 22 | `game/GamePanel.h:17` | build: native-only debug/Wii member set macro (layout only) |
| 23 | `game/Band.h:15` | harness: native scoring-core ctor / helper, called only by the headless drivers |
| 24 | `game/Performer.h:13` | harness: native scoring-core ctor / helper, called only by the headless drivers |
| 25 | `game/Player.h:66` | harness: native scoring-core ctor / helper, called only by the headless drivers |
| 26 | `game/Band.cpp:34` | harness: native scoring-core ctor / helper, called only by the headless drivers |
| 27 | `game/Band.cpp:411` | **FIXED**: retail 0x8269C760 returns straight after `UpdateBonusLevel`; native also copied the band multiplier-active and crowd-meter state into a drop-in player |
| 28 | `game/GameMicManager.h:24` | DC3 fake-mic feature (frame_rate mode only); see #47 |
| 29 | `game/GameMicManager.h:46` | DC3 fake-mic feature (frame_rate mode only); see #47 |
| 30 | `game/SyncGameStartPanel.h:10` | build: native-only debug/Wii member set macro (layout only) |
| 31 | `game/Singer.cpp:169` | headless: null-`BandUser` guard for the headless scoring drivers; never fires when a user exists (the app path) |
| 32 | `game/Singer.cpp:587` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 33 | `game/CommonPhraseCapturer.cpp:60` | headless: null-`TrackPanel` guard; never fires when a TrackPanel exists. Would hide a missing TrackPanel in the app |
| 34 | `game/CommonPhraseCapturer.cpp:74` | headless: null-`TrackPanel` guard; never fires when a TrackPanel exists. Would hide a missing TrackPanel in the app |
| 35 | `game/CommonPhraseCapturer.cpp:117` | headless: null-`TrackPanel` guard; never fires when a TrackPanel exists. Would hide a missing TrackPanel in the app |
| 36 | `game/CommonPhraseCapturer.cpp:136` | headless: null-`TrackPanel` guard; never fires when a TrackPanel exists. Would hide a missing TrackPanel in the app |
| 37 | `game/CommonPhraseCapturer.cpp:217` | headless: null-`TrackPanel` guard; never fires when a TrackPanel exists. Would hide a missing TrackPanel in the app |
| 38 | `game/SongDB.cpp:495` | robustness: bound on a walk retail does unchecked past `gems.end()`; changes no entry below `gems.size()` |
| 39 | `game/SongDB.cpp:504` | robustness: bound on a walk retail does unchecked past `gems.end()`; changes no entry below `gems.size()` |
| 40 | `game/VocalPlayer.h:112` | harness: native scoring-core ctor / helper, called only by the headless drivers |
| 41 | `game/VocalPlayer.h:323` | build: native-only frame-spew members |
| 42 | `game/BandUser.cpp:221` | dev cheat: `send_fake_patches` DataVariable (string absent from band.exe), default 0 |
| 43 | `game/BandUser.cpp:456` | dev cheat: controller-type override / `fake_controllers` DataVariable, default off |
| 44 | `game/BandUser.cpp:467` | dev cheat: controller-type override / `fake_controllers` DataVariable, default off |
| 45 | `game/VocalPart.cpp:633` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 46 | `game/VocalPart.cpp:926` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 47 | `game/GameMicManager.cpp:176` | **flagged, left**: `GetMicCount` returns 4 in frame_rate mode (retail 0x8235BA70 is `lwz r3,0x34(r3); blr`). Dev frame-rate screen only; pairs with the two-sided `GetMic` fake-mic arm, so it was left as one feature |
| 48 | `game/GameMicManager.cpp:285` | DC3 fake-mic feature (frame_rate mode only); see #47 |
| 49 | `game/GemPlayer.cpp:73` | build: include / declaration / linkage / layout, no behaviour |
| 50 | `game/GemPlayer.cpp:229` | dev cheat: `enable_deltas`/`print_deltas` script funcs (absent from band.exe); tracker null until enabled |
| 51 | `game/GemPlayer.cpp:398` | dev cheat: `enable_deltas`/`print_deltas` script funcs (absent from band.exe); tracker null until enabled |
| 52 | `game/GemPlayer.cpp:871` | **FIXED**: retail fn_826C17D8 uses `val` unclamped (assert stripped); native clamped it to [-1,0]. Now warns once and runs retail |
| 53 | `game/GemPlayer.cpp:2216` | dev diag: per-hit timing stats / prints shown only while the GemPlayer overlay is showing; the stats members are read only by the print |
| 54 | `game/GemPlayer.cpp:2303` | dev diag: per-hit timing stats / prints shown only while the GemPlayer overlay is showing; the stats members are read only by the print |
| 55 | `game/GamePanel.cpp:168` | diag: `GAME_DBG` env log only |
| 56 | `game/GamePanel.cpp:187` | diag: `GAME_DBG` env log only |
| 57 | `game/GamePanel.cpp:226` | diag: `GAME_DBG` env log only |
| 58 | `game/GamePanel.cpp:312` | **flagged, left**: forces the HUD (`TrackPanelDir::SetShowing(true)`) at song start because natively `play_intro` fires late or not at all. A forced value hiding a real native bug in the intro anim/camshot machinery; removing it would leave the HUD hidden until that is fixed |
| 59 | `game/GamePanel.cpp:347` | diag: `GAME_DBG` env log only |
| 60 | `game/GamePanel.cpp:746` | build: no-op `PresenceMgr::SetSongID` for targets that do not compile PresenceMgr.cpp (rb3-app compiles the real one) |
| 61 | `game/Player.cpp:562` | headless: null-`BandUser` guard for the headless scoring drivers; never fires when a user exists (the app path) |
| 62 | `game/Player.cpp:622` | headless: null-`TrackPanel` guard; never fires when a TrackPanel exists. Would hide a missing TrackPanel in the app |
| 63 | `game/Player.cpp:820` | headless: null-`BandUser` guard for the headless scoring drivers; never fires when a user exists (the app path) |
| 64 | `game/Player.cpp:902` | headless: null-`TrackPanel` guard; never fires when a TrackPanel exists. Would hide a missing TrackPanel in the app |
| 65 | `game/Player.cpp:1063` | harness: native scoring-core ctor / helper, called only by the headless drivers |
| 66 | `game/Player.cpp:1072` | headless: null-`BandUser` guard for the headless scoring drivers; never fires when a user exists (the app path) |
| 67 | `game/Player.cpp:1089` | headless: null-`BandUser` guard for the headless scoring drivers; never fires when a user exists (the app path) |
| 68 | `game/Player.cpp:1105` | headless: null-`TrackPanel` guard; never fires when a TrackPanel exists. Would hide a missing TrackPanel in the app |
| 69 | `game/TambourineManager.cpp:57` | **FIXED**: init of the removed member |
| 70 | `game/TambourineManager.cpp:127` | **FIXED**: retail 0x826FB308 never mutes the mics; native set gain 0 / volume -96 on every mic for the first tambourine window and never restored them |
| 71 | `game/TambourineManager.cpp:133` | **FIXED**: retail 0x826FB308 never mutes the mics; native set gain 0 / volume -96 on every mic for the first tambourine window and never restored them |
| 72 | `game/VocalPlayer.cpp:58` | build: `static` linkage for a per-TU global, value identical |
| 73 | `game/VocalPlayer.cpp:76` | build: init of native-only members |
| 74 | `game/VocalPlayer.cpp:105` | harness: native scoring-core ctor / helper, called only by the headless drivers |
| 75 | `game/VocalPlayer.cpp:251` | dev cheat: `vocal_frame_spew` DataVariable (absent from band.exe), default 0 |
| 76 | `game/VocalPlayer.cpp:460` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 77 | `game/VocalPlayer.cpp:496` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 78 | `game/VocalPlayer.cpp:597` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 79 | `game/VocalPlayer.cpp:710` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 80 | `game/VocalPlayer.cpp:766` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 81 | `game/VocalPlayer.cpp:796` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 82 | `game/VocalPlayer.cpp:875` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 83 | `game/VocalPlayer.cpp:883` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 84 | `game/VocalPlayer.cpp:902` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 85 | `game/VocalPlayer.cpp:912` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 86 | `game/VocalPlayer.cpp:936` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 87 | `game/VocalPlayer.cpp:997` | dev diag: vocal frame-spew / overlay writes, gated on pointers that stay null unless toggled |
| 88 | `game/VocalPlayer.cpp:2088` | headless: null-`BandUser` guard for the headless scoring drivers; never fires when a user exists (the app path) |
| 89 | `game/VocalPlayer.cpp:2313` | dev diag: overlay callback body, runs only while the vocalplayer overlay shows |
| 90 | `game/VocalPlayer.cpp:2405` | dev handler: script arm retail lacks; reachable only by a script naming it |
| 91 | `meta_band/NetSync.cpp:188` | diag: `GAME_DBG` env log only |
| 92 | `meta_band/SongSelectPanel.cpp:21` | build: include / declaration / linkage / layout, no behaviour |
| 93 | `meta_band/SongSelectPanel.cpp:53` | **other session (Rnd)**: forces `live_lb.grp`/`live_diffs.grp` showing because the native renderer ignores the env-alpha hide fade; listed only |
| 94 | `meta_band/SongSelectPanel.cpp:68` | **other session (Rnd)**: forces `live_lb.grp`/`live_diffs.grp` showing because the native renderer ignores the env-alpha hide fade; listed only |
| 95 | `meta_band/SongSelectPanel.cpp:150` | **other session (Rnd)**: forces `live_lb.grp`/`live_diffs.grp` showing because the native renderer ignores the env-alpha hide fade; listed only |
| 96 | `meta_band/SongSelectPanel.cpp:168` | **other session (Rnd)**: forces `live_lb.grp`/`live_diffs.grp` showing because the native renderer ignores the env-alpha hide fade; listed only |
| 97 | `meta_band/SongSelectPanel.cpp:180` | **other session (Rnd)**: forces `live_lb.grp`/`live_diffs.grp` showing because the native renderer ignores the env-alpha hide fade; listed only |
| 98 | `meta_band/AccomplishmentTourConditional.cpp:118` | dev diag: warn/log only |
| 99 | `meta_band/Campaign.h:138` | build: native-only cheat member |
| 100 | `meta_band/AppMiniLeaderboardDisplay.cpp:91` | offline shim (net area): with RockCentral offline the leaderboard fails at once instead of waiting forever; online takes the retail path. Listed, left |
| 101 | `meta_band/BandUI.h:77` | build: native-only debug-overlay declarations / members / init |
| 102 | `meta_band/BandUI.h:93` | build: native-only debug-overlay declarations / members / init |
| 103 | `meta_band/BandUI.h:160` | build: native-only debug-overlay declarations / members / init |
| 104 | `meta_band/BandUI.h:164` | build: native-only debug-overlay declarations / members / init |
| 105 | `meta_band/InterstitialMgr.cpp:129` | dev diag: vignette/UI debug overlays, work only while an overlay shows |
| 106 | `meta_band/MusicLibraryNetSetlists.cpp:124` | dev diag: warn/log only |
| 107 | `meta_band/SelectDifficultyPanel.cpp:31` | diag: `GAME_DBG` env log only |
| 108 | `meta_band/AssetMgr.cpp:33` | dev diag: asset validation (file-list scan / outfit check), warnings only |
| 109 | `meta_band/Utl.cpp:60` | dev cheat: registers `toggle_fake_leaderboard_upload_failure` |
| 110 | `meta_band/BandProfile.cpp:721` | dev cheat: `MetaPanel::sUnlockAll` (default false, set only by the unlock-all cheat); retail 0x8258BAB8 has no such test |
| 111 | `meta_band/SongSortMgr.cpp:128` | robustness, equivalent: the early return sits at the function end and skips only the stripped assert |
| 112 | `meta_band/AssetProvider.cpp:258` | dev cheat: debug flag default off |
| 113 | `meta_band/CalibrationPanel.cpp:505` | dev diag: warn/log only |
| 114 | `meta_band/CalibrationPanel.cpp:513` | dev diag: warn/log only |
| 115 | `meta_band/CharacterCreatorPanel.cpp:284` | **char-adjacent, listed**: null-`mPreviewDesc` guards retail lacks (retail derefs). Fire only if a getter runs before Enter sets the desc, so they could hide a native ordering bug |
| 116 | `meta_band/CharacterCreatorPanel.cpp:302` | **char-adjacent, listed**: null-`mPreviewDesc` guards retail lacks (retail derefs). Fire only if a getter runs before Enter sets the desc, so they could hide a native ordering bug |
| 117 | `meta_band/CharacterCreatorPanel.cpp:388` | **char-adjacent, listed**: null-`mPreviewDesc` guards retail lacks (retail derefs). Fire only if a getter runs before Enter sets the desc, so they could hide a native ordering bug |
| 118 | `meta_band/CharacterCreatorPanel.cpp:405` | **char-adjacent, listed**: null-`mPreviewDesc` guards retail lacks (retail derefs). Fire only if a getter runs before Enter sets the desc, so they could hide a native ordering bug |
| 119 | `meta_band/CharacterCreatorPanel.cpp:422` | **char-adjacent, listed**: null-`mPreviewDesc` guards retail lacks (retail derefs). Fire only if a getter runs before Enter sets the desc, so they could hide a native ordering bug |
| 120 | `meta_band/CharacterCreatorPanel.cpp:432` | **char-adjacent, listed**: null-`mPreviewDesc` guards retail lacks (retail derefs). Fire only if a getter runs before Enter sets the desc, so they could hide a native ordering bug |
| 121 | `meta_band/CharacterCreatorPanel.cpp:441` | **char-adjacent, listed**: null-`mPreviewDesc` guards retail lacks (retail derefs). Fire only if a getter runs before Enter sets the desc, so they could hide a native ordering bug |
| 122 | `meta_band/CharacterCreatorPanel.cpp:554` | **char-adjacent, listed**: null-`mPreviewDesc` guards retail lacks (retail derefs). Fire only if a getter runs before Enter sets the desc, so they could hide a native ordering bug |
| 123 | `meta_band/CharacterCreatorPanel.cpp:563` | **char-adjacent, listed**: null-`mPreviewDesc` guards retail lacks (retail derefs). Fire only if a getter runs before Enter sets the desc, so they could hide a native ordering bug |
| 124 | `meta_band/CharacterCreatorPanel.cpp:621` | dev diag: asset validation (file-list scan / outfit check), warnings only |
| 125 | `meta_band/SongStatusMgr.cpp:983` | dev cheat: debug flag default off |
| 126 | `meta_band/ManageBandPanel.cpp:106` | dev cheat: `MetaPanel::sUnlockAll` (default false, set only by the unlock-all cheat); retail 0x8258BAB8 has no such test |
| 127 | `meta_band/MetaPerformer.cpp:54` | build: `static` linkage for a per-TU global, value identical |
| 128 | `meta_band/MetaPerformer.cpp:1792` | dev handler: script arm retail lacks; reachable only by a script naming it |
| 129 | `meta_band/MetaPerformer.cpp:1816` | dev handler: script arm retail lacks; reachable only by a script naming it |
| 130 | `meta_band/MetaPerformer.cpp:1827` | dev handler: script arm retail lacks; reachable only by a script naming it |
| 131 | `meta_band/MetaPerformer.cpp:1836` | dev handler: script arm retail lacks; reachable only by a script naming it |
| 132 | `meta_band/MetaPerformer.h:26` | build: native-only debug/Wii member set macro (layout only) |
| 133 | `meta_band/CustomizePanel.cpp:70` | dev cheat: debug flag default off |
| 134 | `meta_band/AppLabel.cpp:549` | dev diag: warn/log only |
| 135 | `meta_band/Campaign.cpp:292` | dev diag: `MILO_ASSERT` retail strips (fatal natively only if a caller passes the last level) |
| 136 | `meta_band/Campaign.cpp:937` | dev cheat: campaign-level cheat methods |
| 137 | `meta_band/CharCache.cpp:5` | build: include / declaration / linkage / layout, no behaviour |
| 138 | `meta_band/CharCache.cpp:80` | **other session (char)**: `CharCache` null guards for the deferred chars.milo; listed only |
| 139 | `meta_band/CharCache.cpp:126` | **other session (char)**: `CharCache` null guards for the deferred chars.milo; listed only |
| 140 | `meta_band/CharCache.cpp:235` | **other session (char)**: prefab patch-tex arm, dev toggle default off; listed only |
| 141 | `meta_band/OvershellPanel.cpp:51` | build: `static` linkage for a per-TU global, value identical |
| 142 | `meta_band/OvershellPanel.cpp:104` | **FIXED**: retail ctor 0x825B4AA8 / dtor 0x825B6188 add/remove no typed invite/utility sinks. Natively they duplicated the untyped `ThePlatformMgr.AddSink(this)` (NetStartUtilityFinishedMsg delivered twice) and the dtor removed two from TheWiiFriendMgr, leaving them dangling on ThePlatformMgr |
| 143 | `meta_band/OvershellPanel.cpp:122` | **FIXED**: retail ctor 0x825B4AA8 / dtor 0x825B6188 add/remove no typed invite/utility sinks. Natively they duplicated the untyped `ThePlatformMgr.AddSink(this)` (NetStartUtilityFinishedMsg delivered twice) and the dtor removed two from TheWiiFriendMgr, leaving them dangling on ThePlatformMgr |
| 144 | `meta_band/OvershellPanel.cpp:268` | **FIXED**: an `EndOverrideFlow` for an inactive flow returned early natively; retail (assert stripped) clears the flow, exports `OvershellOverrideEndedMsg` and calls `UpdateAll`. Now warns and runs that |
| 145 | `meta_band/OvershellPanel.cpp:972` | **FIXED**: native-only lone-user shortcut to kState_ChooseDiff. Retail runs the resolver (same outcome for a local user) and leaves a remote lone user alone; the loop it worked around came from a decomp error already fixed below it |
| 146 | `meta_band/OvershellPanel.cpp:1165` | diag: `PANEL_DBG` env log only |
| 147 | `meta_band/OvershellPanel.cpp:1504` | dev cheat: controller-type override / `fake_controllers` DataVariable, default off |
| 148 | `meta_band/SongSelectPanel.h:33` | build: include / declaration / linkage / layout, no behaviour |
| 149 | `meta_band/BandSongMgr.cpp:34` | build: include / declaration / linkage / layout, no behaviour |
| 150 | `meta_band/BandSongMgr.cpp:415` | **flagged, left**: album-art path falls back to the blank texture when `<song>_keep.png` is missing on disk (retail returns the path). Diverges only when the asset is absent from the extract |
| 151 | `meta_band/BandSongMgr.cpp:891` | dev diag: warn/log only |
| 152 | `meta_band/BandSongMgr.cpp:1169` | build: function-local Symbols for globals undefined natively, same values |
| 153 | `meta_band/BandSongMgr.cpp:1191` | dev handler: script arm retail lacks; reachable only by a script naming it |
| 154 | `meta_band/BandSongMgr.cpp:1234` | dev handler: script arm retail lacks; reachable only by a script naming it |
| 155 | `meta_band/BandSongMgr.cpp:1240` | dev handler: script arm retail lacks; reachable only by a script naming it |
| 156 | `meta_band/MetaPanel.cpp:166` | dev: object factories for classes retail does not register; inert unless data names them |
| 157 | `meta_band/MetaPanel.cpp:208` | dev: object factories for classes retail does not register; inert unless data names them |
| 158 | `meta_band/MetaPanel.cpp:252` | dev: native-only `HAQManager` (QA log manager, disabled by default; band.exe has no `HAQManager`/`haq_` string) |
| 159 | `meta_band/MetaPanel.cpp:273` | dev: native-only `HAQManager` (QA log manager, disabled by default; band.exe has no `HAQManager`/`haq_` string) |
| 160 | `meta_band/OvershellSlot.cpp:59` | build: `static` linkage for a per-TU global, value identical |
| 161 | `meta_band/OvershellSlot.cpp:114` | **FIXED**: retail dtor 0x825E1490 has one RemoveSink (LocalUserLeftMsg); native also removed a never-added TheServer UserLoginMsg sink |
| 162 | `meta_band/OvershellSlot.cpp:801` | inert: clears `unk80`/`unk81`, which retail lacks; `unk80` is never set true anywhere, so the clears and the UserLoginMsg arm (#167) do nothing |
| 163 | `meta_band/OvershellSlot.cpp:1096` | **FIXED**: dead local removed |
| 164 | `meta_band/OvershellSlot.cpp:1162` | **FIXED**: native sent `update_restart_allowed` (string absent from band.exe), which the overshell script uses to enable/disable `overshell_restart` |
| 165 | `meta_band/OvershellSlot.cpp:1183` | **FIXED**: native called `UpdateProfilesList()` on every `UpdateView` (reloads the swappable-profiles provider); retail fn_825DB930 has no bl to 0x825DEEC8 |
| 166 | `meta_band/OvershellSlot.cpp:1880` | dev handler: script arm retail lacks; reachable only by a script naming it |
| 167 | `meta_band/OvershellSlot.cpp:1902` | inert: native-only UserLoginMsg arm, gated on `unk80` (never true) |
| 168 | `meta_band/BandUI.cpp:60` | build: native-only debug-overlay declarations / members / init |
| 169 | `meta_band/BandUI.cpp:210` | dev diag: vignette/UI debug overlays, work only while an overlay shows |
| 170 | `meta_band/BandUI.cpp:602` | dev diag: vignette/UI debug overlays, work only while an overlay shows |
| 171 | `meta_band/BandUI.cpp:704` | dev handler: script arm retail lacks; reachable only by a script naming it |
| 172 | `meta_band/Matchmaker.cpp:272` | offline shim (net area): null `SessionSettings` offline; listed, left |
| 173 | `meta_band/Matchmaker.cpp:288` | offline shim (net area): null `SessionSettings` offline; listed, left |
| 174 | `meta_band/ProfileMgr.cpp:52` | build: `static` linkage for a per-TU global, value identical |
| 175 | `meta_band/EditSetlistPanel.cpp:34` | build: LP64 struct port of the XStringVerify buffers (judged in sweep 1) |
| 176 | `meta_band/MusicLibrary.cpp:180` | empty: comment only |
| 177 | `meta_band/MusicLibrary.cpp:299` | **FIXED**: native-only FriendsListChangedMsg sink on ThePlatformMgr; Handle has no arm for it (retail fn_82542238/fn_82542A00 have ten sinks each) |
| 178 | `meta_band/MusicLibrary.cpp:349` | **FIXED**: native-only FriendsListChangedMsg sink on ThePlatformMgr; Handle has no arm for it (retail fn_82542238/fn_82542A00 have ten sinks each) |
| 179 | `meta_band/MusicLibrary.cpp:376` | empty: comment only |
| 180 | `meta_band/MusicLibrary.cpp:713` | diag: `GAME_DBG` env log only |
| 181 | `meta_band/MusicLibrary.cpp:743` | diag: `GAME_DBG` env log only |
| 182 | `meta_band/MusicLibrary.cpp:751` | diag: `GAME_DBG` env log only |
| 183 | `net_band/EntityUploader.cpp:501` | **FIXED**: retail 0x8250D4C8 has no `unk34` test; native reported an aborted upload as a server timeout |
| 184 | `net_band/RockCentral.h:138` | offline/net: native-only RockCentral invite/messenger flags; their only reader is native-only |
| 185 | `net_band/RockCentral.h:194` | offline/net: native-only RockCentral invite/messenger flags; their only reader is native-only |
| 186 | `net_band/RockCentral.cpp:129` | offline/net: native-only RockCentral invite/messenger flags; their only reader is native-only |
| 187 | `net_band/RockCentral.cpp:305` | offline/net: native-only RockCentral invite/messenger flags; their only reader is native-only |
| 188 | `net_band/RockCentral.cpp:317` | offline/net: native-only RockCentral invite/messenger flags; their only reader is native-only |
| 189 | `net_band/RockCentral.cpp:374` | offline shim (net area): null `mContextWrapperPool` guard because `RockCentral::Init` is gated off natively; listed, left |
| 190 | `net_band/RockCentral.cpp:382` | offline shim (net area): null `mContextWrapperPool` guard because `RockCentral::Init` is gated off natively; listed, left |
| 191 | `tour/TourGameRules.cpp:37` | dev validation: pads `m_vTargets` to 2 with a warning; changes nothing on well-formed data |
| 192 | `tour/TourProgress.cpp:516` | dev handler: script arm retail lacks; reachable only by a script naming it |
| 193 | `tour/Tour.cpp:104` | dev diag: warn/log only |
