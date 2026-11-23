# OPTWALK: rb3-app options, calibration and options save/reload walked (2026-11-23)

Lane optwalk, branch `optwalk` (from main `6dd53794b`), worktree `~/tmp/wt-optwalk`. Scratch,
logs and binaries: `~/tmp/optwalk/` (`r_<tag>/run.log`, `r_<tag>/screens.txt`). Walks are in
`tools/rb3_app_walks/optwalk/` (`mk.py` writes every `w_*.txt` except `w_auto`/`w_reload`, which
are literal). `disc.sh <tag> <bin> <walk> [frames] [env…]` always sets
`RB3_APP_DETERMINISTIC=1 RB3_APP_FOCUS_LOG=1`; `CONTENT=<store>` boots on an earlier run's
memcard store (the reboot). Every run went through `coord/no610m.sh` under the shared
`gpu1.lock`, movies off. The full native selftest was not run (coordinator's job).

**Result: no native game bug found in these screens.** Every option, the manual calibration and
the global/profile save and reload behave as retail's code; the instrument and walks are the
deliverable. Two platform-layer differences (overscan, Dolby) are the engine's and were not
touched (out of this lane's edit scope).

## Commits

| commit | what |
|---|---|
| `906b45dc7` (harness) | `RB3_APP_OPTIONS_LOG=1` in `native/src/main_app.cpp` (below) |
| (this commit) | walk scripts, this record |

No `src/` change, so nothing to price with `ab_measure`. No native/src file was added.

## The instrument: `RB3_APP_OPTIONS_LOG=1`

One `rb3-app: frame N opts …` line whenever any field changes:
`alag/vlag` = `ProfileMgr::GetExcessAudioLag/GetExcessVideoLag`, `sync/s2t` = the saved raw
`mSyncOffset`/`mSongToTaskMgrMs`, `vol=` bg/fg/fx/crowd/vocal-cue/voice-chat slider indices,
`bass dolby overscan`, `shrink` = `TheRnd.ShrinkToSafeArea()`, `fade=` the dB held by
`background_music_level/crowd_level/sfx_level/instrument_level.fade`, `bassw` = `bass_boost.send`
wet gain, `gstate/gdirty` = global-options save state and dirty flag, `mic=` the three mic gains,
`pK=lefty/vs/vv/unsaved/st/as` per profile; in game `av=` TaskMgr AV offset (real − delayed
seconds), `gs2t` = `Game::GetSongToTaskMgrMs()`, `p=` per GemPlayer `BeatMatcher::mSyncOffset`,
track lefty and smasher-0 world x. Every 600 frames in game: `opts clock song=… audio=…
song-audio=…`. Read-only.

## Screens reached and verdicts

Retail's code for every function named below is fuzzy-100 in `report.json` (main, `name_check`):
`ProfileMgr::{SaveGlobalOptions, LoadGlobalOptions, PushAllOptions, GetSyncOffset,
SetExcessVideoLag}`, `Game::{Poll, GetSongToTaskMgrMs, SetTimeOffset}`,
`ShellInputInterceptor::FilterAction`, `SaveLoadManager::IsReasonToAutosave`,
`GemTrack/GemManager/GemTrackDir::UpdateLeftyFlip`, `CalibrationPanel::TriggerCalibration`.

| area | walk (tag) | measured | verdict |
|---|---|---|---|
| Manual audio calibration | `w_cal` (cal) | 6 × right on `cal_manual_audio_screen`: alag 0→5→…→30 in 5 ms steps (`increment_to_5mult`), s2t 17→−13 | as calibration.dta + `SetExcessAudioLag` |
| Manual video calibration | cal | 4 × left on `cal_manual_video_screen`: vlag 0→−20, sync −22→−2, s2t −13→−33 (audio lag held at 30, as `SetExcessVideoLag` re-applies it) | as retail; hand computation from the setters agrees to the ms |
| Audio lag reaches the song clock | `w_base` (base) vs cal | in game `av` = `gs2t` = −10 (base) / −55 (cal) = s2t − `mInGameExtraVideoLatency` 22. Song − audio clock: base +4.5, cal −39.0 ms (Δ −43.5 vs predicted −45; each leg sits a constant ~14 ms above `gs2t`, the sampling point of the log, and the ±1.5 ms is the de-jitter) | as `Game::Poll` |
| Video lag reaches hit judgement | `w_v200` + `RB3_APP_CHARTPLAY` (c0, v200, v200o) | 40 × right → vlag 200, BeatMatcher sync −190 (= −222 + 50 − 18 guitar lag). Chart presses on the gem: **c0 169 hit / 0 miss; v200 0 / 168; v200o (presses 190 ms late, `RB3_APP_CHARTPLAY_OFFSET_MS=190`) 168 / 0**. Predicted before the run: v200 all miss, v200o all hit | as `BeatMatcher` (`mNow + mSyncOffset`) — calibration drives native judgement end to end |
| Tap calibration (`cal_audio_screen`, A on the default mode row) | `w_auto` (au0 gamepad, au1 guitar) | nobody taps; at frame +453 → `cal_manual_failed_screen` (retry), retry loops | reachable; a tapped run was not done (below) |
| Dolby / bass boost / overscan toggles | `w_opt` (opt3, opt4) | dolby 0→1; bass 0→1 and `bassw` −96→0 (dry −96); overscan 0→1 and `shrink` 1→0 | data as `PushAllOptions` (see platform notes) |
| Volume sliders | opt3 | A focuses a slider, left steps it, A confirms (slot_states.dta). instruments 11→8 = `instrument_level.fade` −4.5 dB; bg 9 = −3; crowd 6 = −7.5; sfx 10 = −1.5 — the sound.dta `slider` table at ix+1 | as retail. The native stream receiver applies the ratio `StandardStream` passes (`StreamReceiver_Native::SetVolume` → `ImagePan::Mono`) |
| Vocal options | `w_voc` (voc) | static vocals toggled (vs 1→0), vocal track volume 11→8 (cue), mic 1 gain 8→6 | as retail |
| Lefty flip | opt3, `w_lp1`/`w_lp0` | p0 lefty 0→1; in game track lefty 1, smasher-0 x +8.00 (righty −8.00, mirrored). After the flip, strum **up/down (and left/right) are swapped in menus** for the guitar pad (lp1 vs lp0 traces) | as retail: `ShellInputInterceptor::FilterAction` swaps them for a lefty user on a `lefty_flip TRUE` pad type |
| Lefty sync offset | opt3 | guitar GemPlayer sync 30 = −2 + 50 − 18 | as `GetSyncOffset` |
| Global options save + reboot | opt3 → rl1 | after reboot: alag 30, vlag −20, vol 9/8/10/6, bass/dolby/overscan 1, faders re-pushed (−3/−7.5/−1.5/−4.5) at frame 75 | works |
| Profile (lefty) save + reboot | opt4, `w_optsave` (os2) → rl2; opt3 → rl1 | lefty set at 2111 stays `unsaved1` until an autosave point; os2 saves at 2726 (main_hub `TRANSITION_COMPLETE_MSG`, main_hub.dta:721) and rl2 boots lefty1. opt3 went straight into a song (no autosave point before frame cap) and rl1 boots lefty0 | as retail's dta: the game_options state has no autosave on exit (av_settings and modifiers do) |
| Vocal options save + reboot | voc → rlv | vocal style and cue volume reload; mic gains reload as 8/8/8 | as retail: `SaveGlobalOptions` has no `mMicVolumes` (session-only) |
| Autosave disabled | `w_noas` (noas) → rln | `autosave_enabled` → confirm → profile `as0` (state 2); bass boost on; global options still saved on av_settings exit and reload with bass 1; the profile reloads with autosave on | as retail: `IsReasonToAutosave` ORs `GlobalOptionsNeedsSave()` regardless of the profile flag, and `Profile::IsAutosaveEnabled` is the load state, not a saved field |

## ASan + UBSan leg

`bin-asan-s3` (`-DRB3X_SANITIZE=address,undefined`, ASAN/UBSAN_OPTIONS as the asanwalk record):
asn1 = `w_opt` (calibration, every toggle, sliders, lefty, then a chartplay song: 70 hit / 0 miss),
asn2 = `w_voc`. **0 ASan reports.** UBSan: only the known sites (mikktspace, the bandobj `Delta`
group, `BinStream.cpp:33`, `ChunkStream.cpp:501`, `StandardStream.cpp:689`, `GameGem.h:84/85`,
`SongParser.cpp:234`, `BeatMap.cpp:90`); nothing new.

## Platform-layer differences (engine-owned, not changed)

- **Overscan.** Retail `DxRnd::SetShrinkToSafeArea` (Rnd_Xbox.cpp) rescales the video output to
  95 % when the flag is set, i.e. when overscan is OFF (the default). Natively
  `ProfileMgr::PushAllOptions` sets the same flag (`shrink=1` from frame 75), but nothing in the
  engine renderer reads it (`Rnd_Wgpu.cpp:553` only clears it at init), so native always draws
  the full frame. A display-scaler setting; for the engine owner if a native safe-area option is
  wanted.
- **Dolby.** Retail `Synth360::SetDolby` overrides the XAudio speaker config (5.1 Dolby Digital vs
  stereo). The native synth keeps the base no-op `Synth::SetDolby`; native output is stereo only.

## Not done

- Tap calibration with real taps (`cal_audio_screen`, `CalibrationPanel::TriggerCalibration`):
  a frame-quantised walk cannot tap the 755 ms cycle exactly (33 ms frames), so the result would
  need the stream-time phase to judge. The no-tap path (fail → retry) was walked.
- Hardware (Calbert) calibration (`cal_hw_*`): needs the sensor; no native Calbert.
- Drum options (hi-hat pedal, cymbal configuration, drum lefty) and the voice-chat slider
  (PS3-only row in options_audio.lst) were not walked.
- The ~14 ms constant between the logged song clock and the audio clock was not traced (equal in
  every leg, so every calibration delta above is exact to the de-jitter).
- Binaries: bin-s1 `6698f076557e`, bin-s4 `efa077e5e63e` (main `6dd53794b` + options log),
  bin-asan-s3 `f380a329bf13`. Nothing pushed.
