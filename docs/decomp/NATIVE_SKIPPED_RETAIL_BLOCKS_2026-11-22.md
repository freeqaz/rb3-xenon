# Native-skipped retail blocks (2026-11-22, lane `skipped`)

Goal: list every block that compiles only when `HX_NATIVE` is undefined and has **no native
arm** (`#ifndef HX_NATIVE` / `#if !defined(HX_NATIVE)…` with no `#else`/`#elif`), i.e. retail
code the native build skips; classify each; fix the player-visible gaps by running the retail
code natively. The two earlier sweeps
(`NATIVE_ARM_DIVERGENCE_SWEEP_2026-11-22.md`, `NATIVE_ARM_DIVERGENCE_SWEEP_2_2026-11-22.md`)
read only two-sided arms. `Draw.cpp`, `Text.cpp` and `Spotlight*.cpp` belong to another
session and were classified but not edited.

## Method

1. **What native compiles.** A private configure of this worktree's `native/` (build dir
   `~/tmp/wt-skipped-nb`, `CMAKE_EXPORT_COMPILE_COMMANDS=ON`): 5,049 compile commands, 1,048
   unique TUs. Each TU was re-run with `clang -M` (its own flags) to get its header closure:
   1,048/1,048 succeeded. Files in scope = every closure member under `src/band3/`,
   `src/system/` and `src/network/net/` (xdk and the rest of `src/network/` skipped):
   **2,074 files**.
2. **Enumeration** (`~/tmp/skipped/scan.py`): a preprocessor-conditional stack per file; a
   block is kept when its opening condition is `#ifndef HX_NATIVE`, `#if !defined(HX_NATIVE)`
   or a conjunction containing `!defined(HX_NATIVE)`, and it closes with no `#else`/`#elif`.
   Result: **191 blocks in 122 files** (187 `#ifndef`, 2 `&&` compounds, 2 `||` per-target
   forms, kept for completeness).
3. **Classification** by reading every block plus its preceding comment. Where the block
   is a definition (a function body, a static), "defined natively elsewhere" was accepted
   only because the native link succeeds without it. Retail behaviour was checked with
   `tools/va_disasm.py` where a fix depended on it (VocalPlayer::OnGameOver 0x826E5108:
   `MetaPerformer::Current`, `SongEndsWithEndgameSequence`, then `lwz r3,0x2394(0x82E0…)`
   (TheGameMicManager) vcall slot 2 on the not-taken path). "Is RndRibbon retail?" was
   answered by a string search of `band.exe`: `Ribbon` occurs 0 times; the control strings
   `Spotlight\0`, `MeshAnim\0`, `p0_hit`, `play_tambourine\0` and `percussion.cue` occur
   1, 1, 1, 1 and 1 times.
4. **Which targets compile what** came from the compile DB's object paths (per-target
   `CMakeFiles/<target>.dir`). That decided the fix shape: a TU compiled only into `rb3-app`
   lost its gate; a TU the headless harnesses also compile got
   `#if !defined(HX_NATIVE) || defined(RB3_NATIVE_APP)`, with the new define set only on
   `rb3-app` (precedent: `RB3_NATIVE_AUDITION`).

## Headline

| class | blocks |
|---|---|
| harmless (XDK/platform, endian, STLport internals, COMDAT placement, debug macros, codegen devices, native object-model design, defined natively elsewhere, not retail RB3) | 158 |
| handled per target already / two-sided in effect | 3 |
| **player-visible** | 12 (9 fixed, 3 deferred: SpotlightDrawer_NG, owned elsewhere) |
| **matters later** (online, saves, loading, renderer) | 18 (8 fixed, 10 deferred) |

The main finding: **the comments on the game-layer gates were stale.** They said
`TheNetSession`, `TheNet`'s searcher, `TheGameMicManager` and `TheSaveLoadMgr` were null or
excluded natively. Since lanes A3/A5/postcal/rbx-net, `rb3-app` runs the real `App::Init`
(`TheNet.Init`, `GameMicManager::Init`, `SaveLoadManager::Init`) and compiles
`NetSession.cpp`, `SessionSearcher.cpp`, `SyncStore.cpp`, `Server.cpp`,
`GameMicManager.cpp`, `SaveLoadManager.cpp`, `GamePanel.cpp` and `WaitingUserGate.cpp`.
So rb3-app skipped retail subscriptions and calls to objects it actually has.

## Fixed

| block | file | what natively now runs | targets |
|---|---|---|---|
| 12 | `band3/game/Player.cpp` `Player::Hit` | `p<slot>_hit` to TheGamePanel → venue `p0_hit..p3_hit` EventTriggers (retail 0x826A49A0) | rb3-app |
| 16, 19 | `TambourineManager::PostLoad`, `VocalPlayer::PostLoad` | `tambourine` / `play_tambourine` MidiParser sinks (`config/midi_parsers.dta`) | rb3-app |
| 17 | `TambourineManager::OnPlayTambourine` | percussion cue playback from the bank | rb3-app |
| 20 | `VocalPlayer::OnGameOver` | `TheGameMicManager->SetPlayback(false)` (retail 0x826E5108) | rb3-app |
| 25–29 | `meta_band/BandUI.cpp` Init/Terminate | net-session, searcher, mic, save/load sinks; WaitingUserGate | rb3-app (only TU user; gates removed) |
| 42, 43 | `meta_band/ProfileMgr.cpp` | TheServer, mic, save/load sinks | rb3-app |
| 44 | `meta_band/SaveLoadStatusPanel.cpp` | TheSaveLoadMgr sink (save write icon) | rb3-app (only TU user; gate removed) |
| 34, 35 | `meta_band/Matchmaker.cpp` | searcher `search_finished` sink | rb3-app |
| 79 | `meta/FixedSizeSaveable.cpp` | over-long symbol saved as a blank slot (retail 0x827A2B60) | all |
| 171 | `utl/Loader.cpp` `PollUntilLoaded` | re-entrance stamp + check (retail fn_827BF608) | all |

## Deferred

- **187–189 `world/SpotlightDrawer_NG.cpp`** (~600 lines: CheckCam, BlurRT, CheckRTs and the
  rest of the NG spotlight pass). Player-visible (venue spotlights) and owned by another
  session.
- **10, 11 `Player` `send_update_energy`**: the DTA handler is `send_net_gameplay_msg` →
  `TheNetSession->SendMsgToAll`. That is online-only, and natively its send paths are abort stubs.
- **32, 33 `MainHubPanel` TheServer sink**: compiled only into rb3-milo, which has no TheServer.
- **49, 50 `Synchronizable::Publish/Unpublish`**: lane postcal keeps TheSyncStore empty on
  purpose; online-only.
- **55–57 `PatchDir` mip LOD bias −1.0**: a renderer knob with no native equivalent yet.
- **138 `NgRnd::CreateDefaults`**: renderer; `Rnd_NG.cpp` is not a native TU.
- **135–137 Rnd compress thread**: platform (DXT). Natively `CompressTexture` requests are
  queued and never drained (a small leak), noted for later.

Not changed although noticed: block 122 (`RndFont::Replace`) is two-sided in effect. Its
native arm forwards non-owner refs to `Hmx::Object::Replace`, which retail does not. That is
outside this lane's one-sided scope and is needed by the native ObjRef design. Separately,
`Rnd::DrawPreClear`'s two-sided `drawList` arm selects the opposite list from retail when
`mReleaseImmediate` is set. That arm is not one-sided either, so it was left for an arms lane.

## Measurements

**Prediction:** every edit either removes an `#ifndef HX_NATIVE`/`#endif` pair around code
X360 already compiles, or turns it into `#if !defined(HX_NATIVE) || defined(RB3_NATIVE_APP)`
(true on X360). The Loader statements keep their order. So the X360 token stream is unchanged
apart from comments, and the A/B should read Δ0 on every ruler.

- **X360 A/B** (`tools/ab_measure.py --patch` of the two `src/` commits on main `7699130a4`,
  ruler `name_check`, objdiff `sha256:ec3badc0f33323e9`). Leg A: matched 55,268,
  masked_equal 25,360, honest 29,908, code% 59.665310. Leg B: the same. **Δmatched +0,
  Δhonest +0, Δcode +0 B, Δfuzzy +0.000000 pp**. Units at 100%: 635 → 635. The `none` control
  also reads +0. Leg B recompiled 10 TUs and settled in 2 iterations. Prediction held.
- **Native link**: a full cold build of `~/tmp/wt-skipped-nb` (5,073 edges, every target
  including rb3-app) gave rc=0, and so did the incremental rebuild after rebasing onto
  `7699130a4` (74 edges). The compile DB shows `RB3_NATIVE_APP` on rb3-app's `Player.cpp`
  and on none of the eight other targets that compile it.
- **Native runs** (built tree, through `coord/no610m.sh`, the same arguments
  `tools/native_health.sh` passes): `rb3-save` `=== ALL ROUND-TRIPS OK (0 check failures)`
  (exercises FixedSizeSaveable and the Loader change); `rb3-vocal2`, `rb3-harmony`,
  `rb3-score4` `Done.`; `rb3-score2` `RESULT: OK`; `rb3-crowd` `=== M12 complete`;
  `rb3-milo` and `rb3-song` `RESULT: ALL GATES PASSED`. All rc=0. `rb3-render` reads
  `RESULT: NO GPU` (rc=2), expected under no610m. **rb3-app was not run**: it is a GPU
  target here. Its new code paths are mostly reached only during gameplay (Player::Hit,
  tambourine, OnGameOver), which the boot run does not reach. The exceptions are the
  BandUI/ProfileMgr/SaveLoadStatusPanel subscriptions, which run on the boot path, so the
  coordinator's rb3-app gate is the check for those.
- No new file under `native/src/`, so `tools/native_stub_census.py` needed no entry.
  `tools/native_health.sh --selftest` was not run in this lane (GPU targets cannot run here).

Not done on purpose: no change to any two-sided arm. No edit to Draw.cpp, Text.cpp or
Spotlight*.cpp. No online-only send path enabled (send_update_energy, Synchronizable).

## Full table

Line numbers are pre-edit (worktree base `fc2f5bf68`). Paths are relative to `src/`. `lines` = non-blank,
non-comment lines in the block.

| # | block | lines | class | note |
|---|---|---|---|---|
| 0 | `band3/bandtrack/Gem.cpp:25` | 2 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 1 | `band3/bandtrack/Gem.cpp:636` | 9 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 2 | `band3/bandtrack/GemManager.cpp:41` | 2 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 3 | `band3/bandtrack/GemManager.cpp:1655` | 7 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 4 | `band3/bandtrack/TrackPanel.cpp:57` | 2 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 5 | `band3/bandtrack/TrackPanel.cpp:69` | 2 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 6 | `band3/bandtrack/VocalTrack.cpp:2900` | 7 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 7 | `band3/game/Game.cpp:379` | 6 | harmless: XDK / platform (D3D, threads, Bink, Wii/Xbox API) |  |
| 8 | `band3/game/Game.cpp:407` | 1 | harmless: XDK / platform (D3D, threads, Bink, Wii/Xbox API) | TheXOutputMixer is the X360 Synth360 output mixer; Game.cpp is compiled only into rb3-milo |
| 9 | `band3/game/GameMic.cpp:122` | 4 | harmless: platform (byte swap for the big-endian host) |  |
| 10 | `band3/game/Player.cpp:195` | 5 | later: online -- DEFERRED | send_update_energy -> send_net_gameplay_msg -> TheNetSession->SendMsgToAll; NetSession send paths are online-only abort stubs natively |
| 11 | `band3/game/Player.cpp:783` | 2 | later: online -- DEFERRED | send_update_energy -> send_net_gameplay_msg -> TheNetSession->SendMsgToAll; NetSession send paths are online-only abort stubs natively |
| 12 | `band3/game/Player.cpp:961` | 5 | player -- FIXED (rb3-app) | Player::Hit sends p<slot>_hit to TheGamePanel (venue EventTriggers); retail 0x826A49A0 |
| 13 | `band3/game/Singer.cpp:36` | 47 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 14 | `band3/game/Stats.cpp:9` | 36 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 15 | `band3/game/Stats.cpp:583` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 16 | `band3/game/TambourineManager.cpp:84` | 2 | player -- FIXED (rb3-app) | tambourine MidiParser subscriptions (tambourine sections in vocals) |
| 17 | `band3/game/TambourineManager.cpp:396` | 9 | player -- FIXED (rb3-app) | tambourine percussion cue playback |
| 18 | `band3/game/TrainerGemTab.cpp:16` | 5 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 19 | `band3/game/VocalPlayer.cpp:230` | 2 | player -- FIXED (rb3-app) | tambourine MidiParser subscriptions (tambourine sections in vocals) |
| 20 | `band3/game/VocalPlayer.cpp:1860` | 3 | player -- FIXED (rb3-app) | mic playback off at game over; retail 0x826E5108 vcalls TheGameMicManager |
| 21 | `band3/meta_band/AccomplishmentDiscSongConditional.cpp:16` | 20 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 22 | `band3/meta_band/AccomplishmentManager.cpp:60` | 33 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 23 | `band3/meta_band/AccomplishmentPanel.cpp:54` | 42 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 24 | `band3/meta_band/AssetMgr.cpp:20` | 4 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 25 | `band3/meta_band/BandUI.cpp:69` | 3 | later: online -- FIXED (gate removed; rb3-app only TU) | join-request / local-user-left / invite subscriptions |
| 26 | `band3/meta_band/BandUI.cpp:86` | 2 | player -- FIXED (gate removed; rb3-app only TU) | BandUI hears GameMicsChanged / SaveLoadMgr updates |
| 27 | `band3/meta_band/BandUI.cpp:108` | 2 | later: online -- FIXED (gate removed; rb3-app only TU) | WaitingUserGate (lock-step); WaitingUserGate.cpp is now compiled into rb3-app |
| 28 | `band3/meta_band/BandUI.cpp:132` | 2 | later: online -- FIXED (gate removed; rb3-app only TU) | join-request / local-user-left / invite subscriptions |
| 29 | `band3/meta_band/BandUI.cpp:143` | 2 | player -- FIXED (gate removed; rb3-app only TU) | BandUI hears GameMicsChanged / SaveLoadMgr updates |
| 30 | `band3/meta_band/CampaignGoalsLeaderboardChoicePanel.cpp:27` | 42 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 31 | `band3/meta_band/LessonProvider.cpp:20` | 5 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 32 | `band3/meta_band/MainHubPanel.cpp:99` | 1 | later: online -- DEFERRED | MainHubPanel TheServer sink; MainHubPanel.cpp is compiled only into rb3-milo, which has no TheServer |
| 33 | `band3/meta_band/MainHubPanel.cpp:157` | 1 | later: online -- DEFERRED | MainHubPanel TheServer sink; MainHubPanel.cpp is compiled only into rb3-milo, which has no TheServer |
| 34 | `band3/meta_band/Matchmaker.cpp:136` | 3 | later: online -- FIXED (rb3-app) | matchmaker search_finished subscription |
| 35 | `band3/meta_band/Matchmaker.cpp:153` | 2 | later: online -- FIXED (rb3-app) | matchmaker search_finished subscription |
| 36 | `band3/meta_band/MetaPanel.cpp:62` | 1 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 37 | `band3/meta_band/MetaPanel.cpp:66` | 1 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 38 | `band3/meta_band/MetaPanel.cpp:154` | 1 | handled per target already (RB3_NATIVE_AUDITION / RB3_NATIVE_PROFILEPICTURE in rb3-app) |  |
| 39 | `band3/meta_band/PerformanceData.cpp:8` | 1 | harmless: defined natively by another TU (the link proves it) |  |
| 40 | `band3/meta_band/PrefabMgr.cpp:382` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 41 | `band3/meta_band/PrefabMgr.cpp:400` | 9 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 42 | `band3/meta_band/ProfileMgr.cpp:114` | 1 | later: online -- FIXED (rb3-app) | TheServer UserLoginMsg subscription |
| 43 | `band3/meta_band/ProfileMgr.cpp:122` | 2 | player -- FIXED (rb3-app) | ProfileMgr hears GameMicsChanged / SaveLoadMgrStatusUpdate |
| 44 | `band3/meta_band/SaveLoadStatusPanel.cpp:24` | 1 | player -- FIXED (gate removed; rb3-app only TU) | save/load status (write icon) panel subscribes to TheSaveLoadMgr |
| 45 | `band3/meta_band/SongUpgradeMgr.cpp:328` | 4 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 46 | `band3/meta_band/StoreInfoPanel.cpp:221` | 6 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 47 | `band3/net_band/RockCentral.cpp:36` | 1 | harmless: XDK / platform (D3D, threads, Bink, Wii/Xbox API) | Wii NWC24 extern |
| 48 | `band3/tour/TourDescPanel.cpp:41` | 20 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 49 | `network/net/Synchronize.cpp:28` | 1 | later: online -- DEFERRED | TheSyncStore insert/remove; lane postcal kept the store empty natively on purpose |
| 50 | `network/net/Synchronize.cpp:38` | 1 | later: online -- DEFERRED | TheSyncStore insert/remove; lane postcal kept the store empty natively on purpose |
| 51 | `system/bandobj/BandCamShot.cpp:929` | 3 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 52 | `system/bandobj/BandCharacter.cpp:1817` | 10 | harmless: X360 codegen device (forceinline/noinline/declared-only spec, .data rev int, static_assert) |  |
| 53 | `system/bandobj/BandIKEffector.cpp:9` | 1 | harmless: X360 codegen device (forceinline/noinline/declared-only spec, .data rev int, static_assert) |  |
| 54 | `system/bandobj/PatchDir.cpp:13` | 2 | harmless: XDK / platform (D3D, threads, Bink, Wii/Xbox API) |  |
| 55 | `system/bandobj/PatchDir.cpp:689` | 1 | later: renderer | PatchDir draws patch layers with sampler-0 mip LOD bias -1.0 (retail 0x82C6B648); the native renderer has no equivalent knob yet |
| 56 | `system/bandobj/PatchDir.cpp:696` | 4 | later: renderer | PatchDir draws patch layers with sampler-0 mip LOD bias -1.0 (retail 0x82C6B648); the native renderer has no equivalent knob yet |
| 57 | `system/bandobj/PatchDir.cpp:706` | 1 | later: renderer | PatchDir draws patch layers with sampler-0 mip LOD bias -1.0 (retail 0x82C6B648); the native renderer has no equivalent knob yet |
| 58 | `system/bandobj/TrackPanelDirBase.cpp:66` | 7 | harmless: X360 codegen device (forceinline/noinline/declared-only spec, .data rev int, static_assert) |  |
| 59 | `system/beatmatch/GameGemList.cpp:13` | 80 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 60 | `system/beatmatch/SongData.cpp:311` | 2 | harmless: retail loop with an empty body |  |
| 61 | `system/beatmatch/TrackWatcherImpl.cpp:875` | 10 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 62 | `system/char/CharBlendBone.cpp:172` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 63 | `system/char/CharBoneDir.cpp:297` | 1 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 64 | `system/char/CharBoneDir.cpp:427` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 65 | `system/char/CharBones.cpp:204` | 3 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 66 | `system/char/CharBones.cpp:214` | 1 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 67 | `system/char/CharClip.cpp:36` | 4 | harmless: defined natively by another TU (the link proves it) |  |
| 68 | `system/char/CharClip.cpp:216` | 1 | harmless: native object-model design (ObjRef ring / layout) supplies its own | native arm follows (HoldClip), a two-sided pair in effect |
| 69 | `system/char/CharClip.cpp:334` | 1 | harmless: native object-model design (ObjRef ring / layout) supplies its own | native arm follows (HoldClip), a two-sided pair in effect |
| 70 | `system/char/CharClip.h:179` | 1 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 71 | `system/char/CharEyes.cpp:1590` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 72 | `system/char/CharIKFingers.cpp:676` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 73 | `system/char/CharIKRod.cpp:140` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 74 | `system/char/CharSignalApplier.cpp:193` | 25 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 75 | `system/char/Character.cpp:827` | 1 | harmless: X360 codegen device (forceinline/noinline/declared-only spec, .data rev int, static_assert) |  |
| 76 | `system/char/Character.cpp:1101` | 98 | harmless: defined natively by another TU (the link proves it) |  |
| 77 | `system/char/Character.cpp:1231` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 78 | `system/math/Geo.cpp:622` | 50 | harmless: STLport-internal specialization (host STL supplies the algorithm) | native sorts with a lambda giving the same descending-area order (Geo.cpp:1028) |
| 79 | `system/meta/FixedSizeSaveable.cpp:59` | 4 | later: saves -- FIXED (all targets) | over-long symbol written as a blank slot (retail 0x827A2B60) instead of asserting/overrunning |
| 80 | `system/meta/SongMgr.cpp:576` | 12 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 81 | `system/midi/MidiParser.cpp:66` | 2 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 82 | `system/midi/MidiParser.cpp:100` | 2 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 83 | `system/movie/Movie.h:10` | 1 | harmless: XDK / platform (D3D, threads, Bink, Wii/Xbox API) |  |
| 84 | `system/movie/Splash.cpp:259` | 5 | harmless: XDK / platform (D3D, threads, Bink, Wii/Xbox API) |  |
| 85 | `system/obj/DirLoader.cpp:115` | 1 | harmless: native object-model design (ObjRef ring / layout) supplies its own |  |
| 86 | `system/obj/DirLoader.cpp:129` | 2 | harmless: native object-model design (ObjRef ring / layout) supplies its own |  |
| 87 | `system/obj/DirLoader.cpp:1501` | 243 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 88 | `system/obj/ObjPtr_p.h:727` | 2 | harmless: native object-model design (ObjRef ring / layout) supplies its own |  |
| 89 | `system/obj/ObjPtr_p.h:1075` | 10 | harmless: defined natively by another TU (the link proves it) |  |
| 90 | `system/obj/ObjPtr_p.h:1368` | 108 | harmless: native object-model design (ObjRef ring / layout) supplies its own |  |
| 91 | `system/obj/Object.cpp:105` | 16 | harmless: native object-model design (ObjRef ring / layout) supplies its own |  |
| 92 | `system/obj/Object.h:43` | 1 | harmless: native object-model design (ObjRef ring / layout) supplies its own |  |
| 93 | `system/obj/Object.h:52` | 1 | harmless: native object-model design (ObjRef ring / layout) supplies its own |  |
| 94 | `system/obj/Object.h:864` | 2 | harmless: native object-model design (ObjRef ring / layout) supplies its own |  |
| 95 | `system/obj/Object.h:2224` | 2 | harmless: native object-model design (ObjRef ring / layout) supplies its own |  |
| 96 | `system/oggvorbis/codec.h:32` | 1 | harmless: XDK / platform (D3D, threads, Bink, Wii/Xbox API) |  |
| 97 | `system/os/AppChild.cpp:29` | 1 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 98 | `system/os/Archive.cpp:283` | 2 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 99 | `system/os/Archive.cpp:414` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 100 | `system/os/AsyncFile.cpp:66` | 26 | harmless: defined natively by another TU (the link proves it) | native AsyncFile::New lives with the native AsyncFile |
| 101 | `system/os/AsyncFile.cpp:306` | 1 | harmless: platform (byte swap for the big-endian host) |  |
| 102 | `system/os/Debug.cpp:462` | 68 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 103 | `system/os/Debug.h:89` | 38 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 104 | `system/os/File.cpp:979` | 57 | harmless: defined natively by another TU (the link proves it) |  |
| 105 | `system/os/Memcard_Xbox.cpp:17` | 2 | harmless: XDK / platform (D3D, threads, Bink, Wii/Xbox API) |  |
| 106 | `system/os/ProfilePicture.cpp:9` | 45 | handled per target already (RB3_NATIVE_AUDITION / RB3_NATIVE_PROFILEPICTURE in rb3-app) |  |
| 107 | `system/os/System.cpp:487` | 46 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 108 | `system/os/System.cpp:913` | 1 | harmless: shutdown ordering (FileTerminate / empty MemTerminate at process exit) |  |
| 109 | `system/os/System.cpp:921` | 2 | harmless: shutdown ordering (FileTerminate / empty MemTerminate at process exit) |  |
| 110 | `system/os/Timer.cpp:6` | 2 | harmless: XDK / platform (D3D, threads, Bink, Wii/Xbox API) |  |
| 111 | `system/rndobj/AmbientOcclusion.cpp:1701` | 19 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 112 | `system/rndobj/Bitmap.cpp:527` | 1 | harmless: platform (byte swap for the big-endian host) |  |
| 113 | `system/rndobj/Cam.cpp:22` | 1 | harmless: X360 codegen device (forceinline/noinline/declared-only spec, .data rev int, static_assert) |  |
| 114 | `system/rndobj/Cam.cpp:94` | 1 | harmless: X360 codegen device (forceinline/noinline/declared-only spec, .data rev int, static_assert) |  |
| 115 | `system/rndobj/CamAnim.cpp:80` | 1 | harmless: X360 codegen device (forceinline/noinline/declared-only spec, .data rev int, static_assert) |  |
| 116 | `system/rndobj/Console.cpp:8` | 1 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 117 | `system/rndobj/CubeTex.cpp:10` | 4 | harmless: defined natively by another TU (the link proves it) |  |
| 118 | `system/rndobj/CubeTex.cpp:185` | 2 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 119 | `system/rndobj/CubeTex.cpp:219` | 2 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 120 | `system/rndobj/CubeTex.cpp:300` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 121 | `system/rndobj/Draw.cpp:164` | 5 | harmless: defined natively by another TU (the link proves it) |  |
| 122 | `system/rndobj/Font.cpp:182` | 8 | harmless here; native arm follows (two-sided in effect) | native also forwards non-owner refs to Hmx::Object::Replace (needed by native ObjRef); not changed |
| 123 | `system/rndobj/Group.cpp:558` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 124 | `system/rndobj/LitAnim.cpp:75` | 1 | harmless: X360 codegen device (forceinline/noinline/declared-only spec, .data rev int, static_assert) |  |
| 125 | `system/rndobj/MatAnim.cpp:326` | 7 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 126 | `system/rndobj/Mesh.cpp:1275` | 74 | harmless: renderer (native GPU mesh cache defines its own OnSync) |  |
| 127 | `system/rndobj/MeshAnim.cpp:329` | 16 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 128 | `system/rndobj/MeshDeform.cpp:421` | 10 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 129 | `system/rndobj/PropAnim.cpp:1054` | 33 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 130 | `system/rndobj/PropKeys.cpp:913` | 21 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 131 | `system/rndobj/PropKeys.cpp:1064` | 8 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 132 | `system/rndobj/Ribbon.cpp:132` | 61 | harmless: not retail RB3 | RndRibbon is DC3-only: band.exe holds no "Ribbon" string (control: "Spotlight", "MeshAnim" found) |
| 133 | `system/rndobj/Ribbon.cpp:203` | 63 | harmless: not retail RB3 | RndRibbon is DC3-only: band.exe holds no "Ribbon" string (control: "Spotlight", "MeshAnim" found) |
| 134 | `system/rndobj/Ribbon.cpp:272` | 173 | harmless: not retail RB3 | RndRibbon is DC3-only: band.exe holds no "Ribbon" string (control: "Spotlight", "MeshAnim" found) |
| 135 | `system/rndobj/Rnd.cpp:486` | 7 | harmless: platform (runtime DXT compression thread) | native keeps textures uncompressed; Rnd::CompressTexture requests are never drained (small leak) -- later |
| 136 | `system/rndobj/Rnd.cpp:508` | 4 | harmless: platform (runtime DXT compression thread) | native keeps textures uncompressed; Rnd::CompressTexture requests are never drained (small leak) -- later |
| 137 | `system/rndobj/Rnd.cpp:1368` | 40 | harmless: platform (runtime DXT compression thread) | native keeps textures uncompressed; Rnd::CompressTexture requests are never drained (small leak) -- later |
| 138 | `system/rndobj/Rnd_NG.cpp:71` | 1 | later: renderer | NgRnd::CreateDefaults re-creates defaults as Ng types; Rnd_NG.cpp is not a native TU (header closure only) |
| 139 | `system/rndobj/Rnd_NG.cpp:130` | 1 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 140 | `system/rndobj/Rnd_NG.cpp:138` | 1 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 141 | `system/rndobj/Rnd_NG.cpp:144` | 1 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 142 | `system/rndobj/ShaderOptions.cpp:280` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 143 | `system/rndobj/TexRenderer.cpp:582` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 144 | `system/rndobj/Text.cpp:66` | 3 | harmless: X360 codegen device (forceinline/noinline/declared-only spec, .data rev int, static_assert) |  |
| 145 | `system/rndobj/Utl.cpp:2120` | 17 | harmless: defined natively by another TU (the link proves it) |  |
| 146 | `system/rndobj/Utl.cpp:2804` | 4 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 147 | `system/synth/Emitter.cpp:45` | 1 | harmless: X360 codegen device (forceinline/noinline/declared-only spec, .data rev int, static_assert) |  |
| 148 | `system/synth/MidiInstrumentMgr.cpp:27` | 1 | harmless: defined natively by another TU (the link proves it) | native polls MidiInstrument through SynthPollable::PollAll (W16-TM) |
| 149 | `system/synth/Sequence.cpp:17` | 2 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 150 | `system/synth/Sfx.cpp:309` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 151 | `system/synth/Sfx.cpp:324` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 152 | `system/synth/StandardStream.cpp:647` | 1 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 153 | `system/synth/StreamReceiver.cpp:171` | 4 | harmless: defined natively by another TU (the link proves it) |  |
| 154 | `system/synth/Synth.cpp:196` | 10 | harmless: XDK / platform (D3D, threads, Bink, Wii/Xbox API) | DTA masterKey letter functions; the native cypher setup bypasses them |
| 155 | `system/synth/VoiceBeat.cpp:10` | 1 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 156 | `system/synth/VoiceBeat.cpp:31` | 1 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 157 | `system/synth/VorbisReader.cpp:54` | 12 | harmless: XDK / platform (D3D, threads, Bink, Wii/Xbox API) |  |
| 158 | `system/synth/VorbisReader.cpp:72` | 6 | harmless: XDK / platform (D3D, threads, Bink, Wii/Xbox API) |  |
| 159 | `system/synth/VorbisReader.cpp:434` | 201 | harmless: defined natively by another TU (the link proves it) |  |
| 160 | `system/ui/UIFontImporter.cpp:22` | 34 | harmless: X360 codegen device (forceinline/noinline/declared-only spec, .data rev int, static_assert) |  |
| 161 | `system/ui/UILabelDir.cpp:23` | 4 | harmless: X360 codegen device (forceinline/noinline/declared-only spec, .data rev int, static_assert) |  |
| 162 | `system/ui/UIList.cpp:1128` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 163 | `system/ui/UIListMesh.cpp:35` | 2 | harmless: X360 codegen device (forceinline/noinline/declared-only spec, .data rev int, static_assert) |  |
| 164 | `system/utl/BinStream.h:112` | 1 | harmless: platform (byte swap for the big-endian host) |  |
| 165 | `system/utl/BinStream.h:164` | 1 | harmless: platform (byte swap for the big-endian host) |  |
| 166 | `system/utl/BinkIntegration.cpp:238` | 229 | harmless: XDK / platform (D3D, threads, Bink, Wii/Xbox API) |  |
| 167 | `system/utl/ChunkStream.cpp:225` | 2 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 168 | `system/utl/ChunkStream.cpp:244` | 7 | harmless: platform (byte swap for the big-endian host) |  |
| 169 | `system/utl/ChunkStream.cpp:369` | 7 | harmless: platform (byte swap for the big-endian host) |  |
| 170 | `system/utl/ChunkStream.cpp:471` | 1 | harmless: platform (byte swap for the big-endian host) |  |
| 171 | `system/utl/Loader.cpp:445` | 2 | later: loading -- FIXED (all targets) | PollUntilLoaded re-entrance stamp + check (retail fn_827BF608) |
| 172 | `system/utl/MemHeap.cpp:544` | 30 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 173 | `system/utl/MemHeap.h:56` | 1 | harmless: allocator / MemTrack (platform) |  |
| 174 | `system/utl/MemMgr.cpp:417` | 55 | harmless: allocator / MemTrack (platform) |  |
| 175 | `system/utl/MemMgr.cpp:564` | 4 | harmless: allocator / MemTrack (platform) |  |
| 176 | `system/utl/MemMgr.cpp:1287` | 1 | harmless: allocator / MemTrack (platform) |  |
| 177 | `system/utl/MemMgr.h:244` | 12 | harmless: allocator / MemTrack (platform) |  |
| 178 | `system/utl/MemTrack.cpp:122` | 2 | harmless: allocator / MemTrack (platform) |  |
| 179 | `system/utl/MemTrack.cpp:135` | 3 | harmless: allocator / MemTrack (platform) |  |
| 180 | `system/utl/MemTracker.cpp:238` | 2 | harmless: allocator / MemTrack (platform) |  |
| 181 | `system/utl/MemTracker.cpp:798` | 12 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 182 | `system/world/CameraShot.cpp:176` | 5 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 183 | `system/world/DefaultPhysicsManager.cpp:122` | 1 | harmless: debug-only (per-TU stripped-macro shape, MiloStripEval, timer, assert) |  |
| 184 | `system/world/LightPreset.cpp:1682` | 5 | harmless: match-build COMDAT placement (scatter-include / body dup / orphan instantiation); native compiles the canonical TU |  |
| 185 | `system/world/LightPresetManager.h:9` | 6 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 186 | `system/world/SpotlightDrawer.cpp:661` | 25 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
| 187 | `system/world/SpotlightDrawer_NG.cpp:152` | 300 | player: renderer -- DEFERRED | NgSpotlightDrawer CheckCam/BlurRT/CheckRTs etc.; Spotlight*.cpp is owned by another session |
| 188 | `system/world/SpotlightDrawer_NG.cpp:589` | 198 | player: renderer -- DEFERRED | NgSpotlightDrawer CheckCam/BlurRT/CheckRTs etc.; Spotlight*.cpp is owned by another session |
| 189 | `system/world/SpotlightDrawer_NG.cpp:929` | 39 | player: renderer -- DEFERRED | NgSpotlightDrawer CheckCam/BlurRT/CheckRTs etc.; Spotlight*.cpp is owned by another session |
| 190 | `system/world/SpotlightDrawer_NG.cpp:984` | 71 | harmless: STLport-internal specialization (host STL supplies the algorithm) |  |
