# STUBRING: rb3-app's in-scope link stubs (rings a1-absent-body, a6-failing-tu, a1-platformmgr, a1-xbox-vmx), 2026-11-23

Branch `stubring`, worktree `~/tmp/wt-stubring`, base main `092112ba6`.

**Goal.** Every aborting stub in `native/src/rb3_app_link_stubs.s` tagged with one of the four
in-scope rings: find the game path that reaches it, then give rb3-app a real body (link the TU or
port it, checked against retail with `tools/va_disasm.py`), and walk each path closed. Network, Xbox
Live and store rings were out of scope.

**Result.** 13 of the 40 in-scope stubs now have bodies, and no new stub came in. The regenerated
stub file goes from 153 to 140 functions: a1-absent-body 23 -> 19, a6-failing-tu 5 -> 1,
a1-platformmgr 7 -> 5, a1-xbox-vmx 3 -> 0. The stubs were dropped only by regeneration: the
stub file was emptied, rb3-app was linked with `--error-limit=0`, and
`tools/rb3_app_link_stubs_gen.py build <log> src/rb3_app_link_stubs.s` was rerun. Before that, a
link with the new bodies next to the old stubs failed with exactly 14 duplicate symbols (the 13
below plus `BeatClock::BeatClock`) and nothing else. The 27 stubs left are reached only online, in
audition mode, from native-only code or from Xbox disc and socket I/O; each has a verdict below.

**How I found the callers.** I ran `nm -u` over every rb3-app object in the worktree build and then
`objdump -dr` to get the function holding each relocation. ⚠ A substring match on
`GetOverlappedResult` also matches `XGetOverlappedResult` and first gave nine false callers
(ContentMgr_Xbox, AccomplishmentProgress and others). An exact match on the symbol leaves one
caller, `CDReader.cpp`'s `CDReadDone`.

## Closed (13)

| Stub | Ring | Who reaches it (path) | Body | Retail check |
|---|---|---|---|---|
| `HeldNote::GetGem`, `GetAwardedPercent`, `GetAwardedPoints` | a6 | Only `GemPlayer::PrintFinishHeldNote`, the held-note line of the gem-player debug overlay. Its call is `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` and it runs only while `mOverlay->Showing()`. | `#ifdef HX_NATIVE` bodies in `src/band3/game/HeldNote.cpp`: they assert the gem pointer, return 0 when there are no points, and read the members. | Retail emits none of the three (see the head of HeldNote.cpp), and the format string `held%s (%.2f) %.2f pts` is absent from retail `band.exe` (0 hits; the `pts` count is 0 too), so retail has no caller either. There is no retail body to match. |
| `Label3d::Label3d` | a6 | `BandInit` (Band.cpp, compiled inside BandCharacter.cpp) calls `Label3d::Init()`, which registers the factory. It is reached whenever a .milo or DTA `new` creates a Label3d. | Links `src/system/bandobj/Label3d.cpp` with X360's `/DRB3_SYNCPROP_LOCAL_STATIC`. Without that flag the TU fails on the SYNC_PROP local-static Symbols. | Retail registers Label3d in BandInit (StaticClassName 0x8227ACC8, NewObject 0x8227BD60, per the Band.cpp comment). The TU is the match-build source. |
| `DisplayEvents(DataEventList*, float, float)` | a1-absent | `MidiParser::OnDebugDraw` (`debug_draw` handler) | Links `src/system/midi/DisplayEvents.cpp`, which compiles as-is. | Match-build TU |
| `DmCaptureStackBackTrace` | a1-absent | `AllocInfo::FillStackTrace` (HX_NATIVE): the per-allocation record MemTracker keeps | `rb3_app_stubring_native.cpp`: zero-fills the frames, so the empty trace ends the caller's copy loop. A 64-bit return address does not fit the caller's `int` slots. | xbdm debug API, not in band.exe |
| `JoypadSetRumble` | a1-absent | `JoypadSetXinputCalbertMode` (the Calbert fallback) and `JoypadStageKitSetRaw` | `rb3_app_joypad.cpp`: a tail call to `JoypadSetXinputActuators` (Joypad_Xinput.cpp, compiled), plus a native `XInputSetState` that logs each change of the motor pair and returns 0. | 0x82529AE8 = `b 0x82532378` = `JoypadSetXinputActuators` (map), which is `sth`/`sth` and then XInputSetState. Matches. |
| `JoypadSetCalbertMode` | a1-absent | `CalibrationPanel::PrepareHwCalibrationState` / `TerminateHwCalibrationState`, hardware (auto) calibration only (`mHardwareMode`) | `rb3_app_joypad.cpp`: when `mType` is 0x1e..0x2e it takes the raw-report arm, which sends nothing because no native pad has an XInput2 channel. Otherwise it calls `JoypadSetXinputCalbertMode`. | 0x82529C98: `bl JoypadGetPadData`, `lwz 0x6c`, `cmpwi 0x1e`/`0x2e`, then 7 zeroed args to SendRawData. Matches. |
| `PlatformMgr::SwapUserPads` | platformmgr | `OvershellSlot::SwapUserProfile` <- `ConfirmSwapUserProfile`: the overshell "swap profile" confirm | `rb3_app_stubring_native.cpp`: PlatformMgr_Xbox.cpp's body, plus a native `XRemapUserDevices` that logs the mapping and succeeds | 0x8251D6C8: pad permutation, `bl 0x82B54288` (XRemapUserDevices), `cmplwi r3,0`, `bl 0x82525DE0` (JoypadSwapPads), static-msg guard, `bl 0x8251CD08` (msg ctor). Matches. |
| `PlatformMgr::IsGuestOnlineID` | platformmgr | `SessionUsersProviders` (overshell user list, guest filter `unk2a`) and SongSelectPanel's object | Same file: XUID & `3<<54` | 0x8251C208: `bl GetXUID`, `li r12,3; sldi r12,r12,0x36; and`. Matches. |
| `__vmaddfp`, `__vperm`, `__vspltw` | vmx | `ShiftedDotProduct`'s fast path (dsp/SndAnalysis.cpp, retail 0x82B816F0) <- `PitchDetector::Process` <- GameMic (live vocal mic, pitch detection on) | Same file: a single-rounding `fmaf`; splat of word `imm&3`; a 32-byte permute indexed in big-endian byte order (byte 0 = most significant byte of word 0, which is how SndAnalysis's `sShiftPerm` table is written) | Checked by behaviour, not bytes: the native fast path matches the scalar `else` arm to max abs diff **2.38e-6** over vlen 128 (random input in [-1,1]; harness `~/tmp/stubring_vmx/t.cpp`, linked against the rb3-app `SndAnalysis.cpp.o`). A little-endian permute would shift the wrong words and miss by O(1). VMX128's denormal flush is not modelled. |

## Not closed (27)

| Stub(s) | Ring | Path | Verdict |
|---|---|---|---|
| `AuditionSessionBuilder::*` (12) | a1-absent | `enter_audition` -> `AuditionMgr::SetState(1)` -> `EnterStart` -> `new AuditionSessionBuilder`, then `AuditionSessionPanel` (audition_session_screen) | **Deferred.** No source in either oracle. The bodies are about 6 KB at retail 0x825EAC38..0x825EC3A0 (addresses in AuditionMgr.h). Porting them would only move the abort: the next state (2, `ThreadCall(this)`) mounts the utility drive and runs the Validator and UGCNet_Server download (ring audition-only, also stubs). Audition mode cannot run offline until those exist. |
| `BeatClock::BeatClock` | a6 | `WorldInit`'s `REGISTER_OBJ_FACTORY(BeatClock)`, which is HX_NATIVE only | **Left, measured.** Retail has no BeatClock (0 hits for the string in band.exe), so no RB3 asset or script can create one. I made BeatClock.cpp compile (its own `mPhrase`, since RB3's SongPos has no phrase) and linked it, and the regenerated stubs then gained `Sound::ElapsedTime()` and `typeinfo for Sound`. That swaps one unreachable stub for two, so I reverted it. |
| `PropSync<ObjectDir>(ObjDirPtr<ObjectDir>&, …)` | a1-absent | `InlineHelp::SyncProperty`'s `resource` arm, which is `#ifdef HX_NATIVE` and DC3-era | **Left; it is in src/system/ui (not mine to edit).** Retail's InlineHelp::SyncProperty (0x823175D8) has no `resource` arm and no call to an ObjDirPtr PropSync. Our `ObjDirPtr` has no file getter, so it cannot be written without an obj/Dir.h change. |
| `GetOverlappedResult` | a1-absent | `CDReadDone` (os/CDReader.cpp): the Win32 overlapped ark disc read | **Left.** XDK file I/O, and rb3-app reads the ark through its own reader. The other `XGetOverlappedResult` callers are a different, already-shimmed symbol. |
| `CloseHandle`, `WSACreateEvent`, `WaitForSingleObject` | a1-absent | os/NetworkSocket.cpp | Out of scope (network). |
| `NetLoaderXbox::NetLoaderXbox` | a1-absent | utl/NetLoader.cpp (network file loader) | Out of scope (network). |
| `WebSvcRequest::Poll` | a1-absent | net/WebSvcMgr.cpp | Out of scope (online). |
| `PlatformMgr::CanCommunicateWith`, `SetGameModeContext`, `SetRankedContext` | platformmgr | NetSession_Xbox.cpp (online session) | Out of scope (Live). |
| `PlatformMgr::CanSeeUserCreatedContent` | platformmgr | TourCharRemote (remote players' characters), MusicLibraryNetSetlists | Out of scope (Live privilege check, online-only callers). |
| `PlatformMgr::ShowGamercard` | platformmgr | PlayerLeaderboards, SongRecord | Out of scope (Live UI). |

The four `a1-absent-body` data globals in `rb3_app_link_stubs.cpp` (`play_msg`, `TheSongDB`,
`TheTour`, `TheHAQMgr`, plus `BandCamShot::sHideAllCharactersHack`) were not examined.

## Walks (worktree binary, `RB3_APP_DETERMINISTIC=1`, through no610m.sh)

The runner is a copy of `tools/rb3_app_walks/menuwalk/runb.sh` with `A=~/tmp/stubring_walks`. Logs
are in `~/tmp/stubring_walks/r_<tag>/run.log`.

- `calib` (`menuwalk/w_calib.txt`, 5000 frames): rc 0, 5000 frames, calibration screens through
  `cal_manual_complete_screen` to `main_hub_screen`, 0 `UNREACHED` lines. This is the manual
  calibration flow, so `mHardwareMode` stays 0 and `JoypadSetCalbertMode` is not called (no
  `XInputSetState` line). **Prediction confirmed: this walk does not reach the Calbert path.**
- `practice` (`menuwalk/w_practice.txt`, 12000 frames): rc 0, 12000 frames (the cap), from boot through song select,
  part/difficulty and practice section select into `practice_game_screen` (playing, with a
  restart-sync cycle), 0 `UNREACHED` lines. The script's final `wait_screen
  practice_endgame_screen` timed out because the 12000-frame cap ended the run mid-song; it is not
  a failure. GemPlayer runs here with its overlay hidden, so no HeldNote accessor is called.

**No walk reaches any closed path, and that is the honest result.** Each one sits behind something
an input-only walk cannot do today: the gem-player debug overlay (HeldNote), a live mic
(VMX/PitchDetector), MemTracker (DmCapture), MidiParser `debug_draw` (DisplayEvents), a
Calbert-capable guitar in hardware calibration (JoypadSetCalbertMode/Rumble), a second signed-in
profile and the overshell swap confirm (SwapUserPads), and an asset that holds a Label3d. The input
scripts can only press pads (no DTA injection, see JoypadNativeSource.cpp). The walks show the new
bodies cost nothing on paths that already ran. The bodies are checked against retail bytes, plus
the VMX harness above.

## Not done

- The full native selftest (`tools/native_health.sh --selftest`) was left to the coordinator, as
  asked.
- No X360 A/B. The only `src/` edit is an `#ifdef HX_NATIVE` block in HeldNote.cpp, which the match
  build does not compile.
- No walk for the overshell swap. It needs `RB3_APP_EXTRA_PADS` plus two profiles; untried.
- `BandCharacter.cpp`, `src/system/ui`, `rndobj` and `char` are untouched. The one item there is
  InlineHelp's `resource` arm (above).
