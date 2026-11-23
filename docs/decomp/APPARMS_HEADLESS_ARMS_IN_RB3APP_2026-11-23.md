# APPARMS: headless-harness `HX_NATIVE` arms that rb3-app compiled (2026-11-23)

Lane apparms, branch `apparms`, worktree `~/tmp/wt-apparms`, base main `24024b599`.
Scratch, scans, logs and binaries: `~/tmp/apparms/` (`build.sh`/`runb.sh` are copies of
scorewalk's with `A`/`W`/`X` pointed here; every run went through `coord/no610m.sh`).

## Goal

Find the `HX_NATIVE` arms whose divergence from retail is justified by the headless
harnesses (no BandUser, sentinel `mTrack`, no TheGameConfig / TheProfileMgr, no render or
net objects) but which rb3-app (`RB3_NATIVE_APP`, the real `App` boot) also compiles, now
that it has the real objects; fix the ones rb3-app reaches with the scorewalk / vocalod
shape (`HX_NATIVE && !RB3_NATIVE_APP`, retail arm checked with `tools/va_disasm.py`). Arms
in `src/system/{rndobj,ui,char}` and joypad / net-sync code are listed, not edited.

## Method

1. `~/tmp/apparms/scan.py`: every `#if…HX_NATIVE` directive in `src/` that does not
   already mention `RB3_NATIVE_APP`, kept when its directive line plus the 4 lines before
   and 14 after mention a harness justification (`headless`, `harness`, `no BandUser`,
   `sentinel`, `TheGameConfig`, `no render`, `no net`, `no session`, `never called`, …):
   **63 directives**. Many `sentinel` hits are the ObjRef ring's head sentinel, not a
   harness (false positives, kept in the table below as such).
2. `scan2.py` (every other HX_NATIVE directive in `band3/game` and `band3/bandtrack`, one
   line of context each) and `scan3.py` (a second keyword set: `native has no`,
   `offline`, `TheProfileMgr`, `TheWorld`, `TheUI`, `mUser`, `TrackPanel`,
   `VocalTrackDir`, `off-path`, … over `band3`, `network/net` and the system dirs rb3-app
   shares with the harnesses) for what the first wording missed.
3. Each hit read by hand. Two shapes do **not** diverge in rb3-app and were left alone:
   a **null guard** (`if (!mUser) return …`, `if (GetTrackPanel())`, `GetUser() ? … :
   gNative…`) falls through to the retail line whenever the object exists, which it does in
   rb3-app; and a **harness-only definition** (the native scoring-core ctors, frame spew,
   vocal overlay) that rb3-app never calls. What remains is an **unconditional** skip or
   substitute: those are the candidates.
4. For each candidate: is the `#else` arm retail (checked with `va_disasm.py`, and the
   function's match state), and does rb3-app reach it (gdb hit counts on a short det walk,
   base binary against fixed binary).

## Candidates and verdicts

Line numbers are at base `24024b599`. Paths relative to `src/`. **FIXED** = now
`HX_NATIVE && !RB3_NATIVE_APP` (rb3-app runs the `#else` arm, the X360 arm is unchanged).

### Fixed (5, all in `band3/game/VocalPlayer.cpp`)

The common cause: scorewalk's `HookupTrack` fix (`9ca2fa69c`) gave rb3-app a real
`VocalTrack` in `mTrack`, so every VocalPlayer arm whose comment says "mTrack is a
sentinel" / "no VocalTrackDir" / "never called headless" became reachable in rb3-app, and
three of them are on `HookupTrack`'s own retail arm. VocalPlayer::Poll (0x826EB030) and
HookupTrack (0x826E63D8) match retail, so their `#else` arms are what retail runs.

| line | function | native arm did | retail (band.exe) | how rb3-app reaches it |
|---|---|---|---|---|
| 843 | `VocalPlayer::Poll` octave wrap | `fBottom = fTop = 0` (wrap off) | `bl GetBottomDisplayPitch` 0x826EB9E4, `bl GetTopDisplayPitch` 0x826EB9F4, minus/plus `mTrackWrappingMargin` | every Poll frame with a non-zero singer pitch (autoplay sings the chart) |
| 1091 | `VocalPlayer::SendCanChat` | nothing | inlined: HookupTrack 0x826E6464 and Poll 0x826EBBBC `bl VocalTrackDir::CanChat` 0x822F6328 | HookupTrack, Restart, Poll when CanChat flips |
| 1495 | `VocalPlayer::GetCompensatedTime` | `return ms` | 0x826E3B00: pad -1 or the local user's pad (vcalls 0x5c/0x64/0), `bl ProfileMgr::GetSyncOffset` 0x825464F0, `fadds` | first call of every Poll (0x826EB090) |
| 1536 | `VocalPlayer::UpdateMicDisplay` | `return` | 0x826E5B80 (`lwz 0x30c` mTrack, TheGameMicManager 0x82E02394 …), called from HookupTrack 0x826E646C | HookupTrack, GameMicsChangedMsg |
| 2289 | `VocalPlayer::ShowPitchCorrectionNotice` | `return false` | 0x826E5A08: `bl AllowPitchCorrection` 0x826E5060, then `lbz 0x69` of TheProfileMgr (0x82DFD818) | VocalTrackDir reset via `VocalTrack::ShowPitchCorrectionNotice` |

### Listed, not edited (scope: joypad / net-sync / ui / rndobj / char)

| line | arm | verdict |
|---|---|---|
| `band3/game/VocalPlayer.cpp:1683` | `OnMsg(ButtonDownMsg)` returns 0 (tambourine button path: TheUI focus, overshell slot) | **joypad**: rb3-app reaches it with a pad; a vocal tambourine hit by button is skipped. Not edited (brief). Retail fn is the `#else`. |
| `band3/game/TambourineManager.cpp:214` | `HandleButtonDown` uses sync offset 0 instead of `TheProfileMgr.GetSyncOffset(pad)` | **joypad**, and unreached while the OnMsg arm above returns 0 (its only caller). Fix together with it. |
| `band3/game/VocalPlayer.cpp:1060` | `LocalEndgameEnergy` returns (endgame_vocals_* to TheWorld) | **net-sync**: its one caller is the `remote_vocal_energy` handler (a remote player's message). Unreached offline. |
| `band3/game/Player.cpp:195`, `:783` | `send_update_energy` broadcast skipped | **net-sync** (already DEFERRED in `NATIVE_SKIPPED_RETAIL_BLOCKS_2026-11-22.md` #10/#11: NetSession send paths abort natively). |
| `band3/meta_band/Matchmaker.cpp:192` | `IsFinding` drops `TheNetSession->mSettings->mPublic` | **net**: rb3-app's offline NetSession has no settings; the deref would fault. Keep. |
| `band3/meta_band/Matchmaker.cpp:272`, `:288` | `if (!settings) return` | null guards (net); inert once settings exist. |
| `system/ui/UI.cpp:416` | `GotoScreenImpl` skips any `*campaign*` screen | **ui**: rb3-app reaches it (bandwalk: `campaign_rewardvignette_firstgig_screen` after a band song). Not edited. |
| `system/ui/UI.cpp:51/661/666/814` | MILO_HEADLESS fake UI seconds (1/30 s per frame) | **ui**: rb3-app sets `MILO_HEADLESS=1` (`main_app.cpp:156`), so it takes this arm by design; with `RB3_APP_DETERMINISTIC=1` the virtual clock already paces frames at 1/30 s. Not edited. |
| `system/ui/UIList.cpp:1131`, `system/rndobj/Tex.cpp:435` | scatter-include / platform switch | false positives ("sentinel", "never called"); not harness arms. |

### Reviewed, no change needed

| line | arm | verdict |
|---|---|---|
| `band3/game/Player.cpp:562, 820, 1072, 1089, 1105, 622, 902` | `if (!mUser)` / `if (!GetTrackPanel())` guards | null guards: rb3-app has a BandUser and a TrackPanel, so the retail line runs. |
| `band3/game/CommonPhraseCapturer.cpp:60, 74, 117, 136, 217` | `if (GetTrackPanel())` before the unison render calls | null guards; inert in rb3-app. |
| `band3/game/Singer.cpp:123, 169`, `VocalPart.cpp:21`, `TambourineManager.cpp:64` | `GetUser() ? retail : gNativeVocalDifficulty` / `if (!u)` | null guards; rb3-app takes the retail value. (VocalPart's `unk38 = 0` is an uninitialised-read fix, not a harness arm.) |
| `band3/game/VocalPlayer.cpp:2108` | `PressingToTalk` `if (!u) return false` | null guard. |
| `band3/game/VocalPlayer.cpp:1018/1032` | `fTargetPitch` hoisted and zeroed | uninitialised-read fix (retail carries a stack slot), not a harness arm. |
| `band3/game/VocalPlayer.cpp:105`, `Performer.h:13`, `Player.h:66`, `Player.cpp:1063`, `Band.h:15`, `Band.cpp:34`, `VocalPlayer.h:112` | native scoring-core ctors, `NativeInitParams`, `gNativeVocalDifficulty` | harness-only definitions; rb3-app builds players through the retail ctors. |
| `band3/game/Player.cpp:314` | `GetSongMs` reads `gNativeSongMs` when `mBeatMaster` is null | null guard; rb3-app players have a BeatMaster. |
| `band3/game/Player.cpp:278` | `talking = false` (no XHV2 voice engine) | XDK voice; out of scope. |
| `band3/game/GamePanel.cpp:347` and the other `GAME_DBG`/`GEM_DBG`/`K8_DBG` blocks | env-gated diagnostics | off unless the env var is set. |
| `band3/game/GamePanel.cpp:312` | V25 force-shows the HUD at song start | a render workaround for a late `play_intro`, not a harness arm; idempotent with retail's show. Left for a render lane to re-check in rb3-app. |
| `band3/meta_band/ProfileMgr.cpp:614` | `if (TheGameMicManager)` | null guard; rb3-app has the manager (comment is stale). |
| `system/beatmatch/TrackWatcherImpl.cpp:42` | `mIsLocalUser = true` instead of `TheUserMgr->GetUser(guid)->IsLocal()` | rb3-app has TheUserMgr, but offline every user is local, so the value is the same; differs only with a remote user (net). Not edited. |
| `system/obj/Task.cpp:529` | MILO_HEADLESS fake task seconds | rb3-app is headless by design (same as UI.cpp above); `system/obj`, but changing it re-times every walk and the det clock already gives 1/30 s frames. Not edited. |
| `system/synth/StandardStream.{h:178,cpp:478}` | wall-clock fallback when the audio callback lags | runtime detection, not a harness assumption; rb3-app runs with no audio device (MILO_HEADLESS), so it is the intended path. |
| `system/os/System.cpp:172`, `system/math/Trig.cpp:32` | lenient missing config section; TrigTableInit guard | harness bring-up helpers; rb3-app boots through SystemInit. |
| `system/obj/Object.{h,cpp}`, `ObjPtr_p.h:1353`, `utl/Str.cpp:7`, `world/Instance.cpp:536`, `bandobj/BandCharacter.cpp:1363, 2732`, `gesture/GestureMgr.cpp:406`, `hamobj/FreestyleMoveRecorder.cpp:469` | ObjRef ring sentinel, `npos`, LP64 bounds, DC3 Kinect | false positives of the keyword scan (host or object-model arms, not harness arms). |

## Runs

Binaries: bin-base `98378abba9da` (worktree at `24024b599`), bin-fix `ad128d6277d1`
(`2678061ff`). Walk: `rb3-scout/a25/walk.txt` (vocals, thecon then the RNG's next song),
`RB3_APP_DETERMINISTIC=1 RB3_APP_AUTOPLAY=1`, 90000-frame cap, `SECS=540 TMO=580`.
Reach runs: the same walk to 3000 frames under `gdb -batch -x ~/tmp/apparms/reach.gdb`
(breakpoints with ignore counts, so `info breakpoints` gives hit counts at exit).

Predictions, written before the runs:

- Reach. The function entries (`GetCompensatedTime`, `UpdateMicDisplay`,
  `ShowPitchCorrectionNotice`) are hit on both binaries; the retail callees behind the
  fixed arms (`ProfileMgr::GetSyncOffset` from vocals, `VocalTrack::GetBottom/TopDisplayPitch`,
  `VocalTrackDir::ShowMicDisplay`/`SetMicDisplayLabel`, `AllowPitchCorrection`) are 0 on
  base and non-zero on fix. `VocalTrackDir::CanChat` is non-zero on both (VocalTrackDir's own
  reset calls `CanChat(false)`), higher on fix.
- Det walk. Display-only arms (CanChat, mic display, pitch-correction notice) cannot reach
  the per-frame hash. GetCompensatedTime and the octave wrap can: if the profile's sync
  offset is non-zero, A and B diverge at the first scored vocal frame; if it is 0 and the
  autoplayed pitch stays inside the display range, A = B.

**Engine state.** All three binaries (bin-base 08:34:03, bin-fix 08:34:18, bin-noct
08:36:40 UTC) were linked before the live `milo-native-engine` moved to `2dcbb17`
(08:37:19), against the same engine objects: the bin-fix and bin-noct builds were
incremental and recompiled only `VocalPlayer.cpp` plus the link, so no engine file changed
between them. The engine commit underneath is `6b34583`.

### Reach (gdb hit counts, frames 0-3000: boot, song select, thecon from frame 1112)

| breakpoint | bin-base | bin-fix |
|---|---|---|
| `VocalPlayer::GetCompensatedTime` | 1649 | 1649 |
| `ProfileMgr::GetSyncOffset` | 5 (gem/options pushes) | **1654** |
| `VocalTrack::GetBottomDisplayPitch` / `GetTopDisplayPitch` | not linked | **639 / 639** |
| `VocalTrackDir::CanChat` | 2 (VocalTrackDir's own reset) | **3** |
| `VocalPlayer::UpdateMicDisplay` | 1 | 1 |
| `VocalTrackDir::ShowMicDisplay` / `SetMicDisplayLabel` | not linked | **1 / 1** |
| `VocalPlayer::ShowPitchCorrectionNotice` | 1 | 1 |
| `VocalPlayer::AllowPitchCorrection` | not linked | **1** |
| `VocalPlayer::HookupTrack` | 1 | 1 |

Every prediction held. "Not linked": `nm -C bin-base` has no symbol for the retail callee at
all (the linker dropped it, nothing referenced it), so base could not have reached it.
`SetMicDisplayLabel` firing once means the mic panel is shown: rb3-app has no microphone,
and retail shows "connect a mic" / "mics disconnected" on the vocal track in that state.

### Det walk A (bin-base) vs B (bin-fix)

- `rb3_app_det_cmp.py`: **first divergence at frame 1607**, in `rand` only (A `176/30`,
  B `178/32`: B draws the game RNG twice more), 495 frames after game_screen starts and
  before the first hit.
- On every field but `rand`/`state`/`chain` (`~/tmp/apparms/gamecmp.py`: seconds, beat,
  ui, screen, transition and the whole scoring suffix) the runs first differ at frame 2785:
  B scores phrase 8 one frame earlier (2785 vs 2786). Over thecon's 39 phrases, 9 score
  one frame earlier in B (phrases 8, 10, 12, 14, 22, 26, 31, 34, 37) and 30 on the same frame; none later. Final thecon state is the
  same in both: 30000 points, 39/39 phrases, 5.0 stars, crowd 1.0, energy 1.0, endgame
  at frame 8008.
- Attribution, bin-noct (`2678061ff` with only `GetCompensatedTime` put back on the
  harness arm), run C, 3000 frames: **identical to A over all 3000 frames**, and differs from
  B from frame 1607. So both effects are the sync offset: retail's vocal clock runs
  `GetSyncOffset(pad)` ahead (`mSyncOffset + mInGameSyncOffsetAdjustment − pad lag`, the
  same offset ProfileMgr already hands every GemPlayer), and natively the singer was graded
  that far late. The other four arms do not reach this hash over those 3000 frames
  (display-only, and the octave wrap never fires for an autoplayed chart pitch).
- Full walks (both stopped by the 540 s wall-clock cap, rc 4: A 49,524 frames, B 47,479):
  0 FAIL/ASSERT lines in either. After thecon the walks pick different second songs
  (A: 48 phrases, 37,200 points; B: 16 phrases, 22,200 points; both 0 misses), because
  the next song is drawn from the game RNG and B's RNG is 2 draws ahead from frame 1607,
  as scorewalk saw for autoplay against no-autoplay. Both end on `coop_endgame_screen`.
- Not run: a B-against-B repeat (the binary is deterministic by detwalk's measurement and
  run C reproduces A frame for frame over 3000 frames from a different binary), the ASan
  build, perturb, and `tools/native_health.sh --selftest` (left to the coordinator, as
  briefed).

## X360 match build

Not measured with `ab_measure`: the diff changes only five `#ifdef HX_NATIVE` lines to
`#if defined(HX_NATIVE) && !defined(RB3_NATIVE_APP)` plus comments inside those native
arms (`git diff -U0` of `2678061ff`, non-comment lines are the five directives), and the
match build never defines `HX_NATIVE`, so its preprocessed `VocalPlayer.cpp` is unchanged.
The scorewalk and vocalod fixes of the same shape measured Δ0.

## What is left (for other lanes)

- **joypad:** `VocalPlayer::OnMsg(ButtonDownMsg)` and `TambourineManager::HandleButtonDown`
  (vocal tambourine by button, with retail's sync offset). They should move together.
- **ui:** UI.cpp's `*campaign*` screen skip (stops the band walk after one song); the
  MILO_HEADLESS fake UI clock.
- **net:** `send_update_energy`, `remote_vocal_energy` -> `LocalEndgameEnergy`,
  Matchmaker's `mPublic`, TrackWatcherImpl's `mIsLocalUser`.
- `system/bandobj/BandCharacter.cpp` (1363, 2732): host-portability arms, not harness arms;
  not edited (a peer lane owns that file).
- GamePanel.cpp's V25 HUD force-show: a render workaround to re-check now that rb3-app runs
  `play_intro` with real objects.
