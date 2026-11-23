# FRAMERATE: rb3-app runs at retail's 60 Hz (2026-11-23)

Lane framerate, branch `framerate` (from main `7d3ca4f0b`), worktree `~/tmp/wt-framerate`.
Scratch, logs and binaries: `~/tmp/framerate/`. Scripts: `tools/rb3_app_walks/framerate/`
(copy them into `~/tmp/framerate/`, which they `cd` into). Every native run went through
`coord/no610m.sh` under the shared `gpu1.lock` (runb.sh).

## Question

Lane codawalk found the guitar tilt filter counts polls (`any_sign_filter (num_frames 12)`),
so at rb3-app's 30 polls/s a tilt has to be held 0.4 s, where a 60 Hz retail loop needs 0.2 s.
What rate does retail run its loop and joypad at, what rate does rb3-app run at, what else
counts frames, and can rb3-app run at retail's rate?

## 1. Retail: one frame per 60 Hz refresh, one joypad poll per frame

- **Rate source.** System `config/gen/default.dtb` (TU5, decoded with
  `tools/rb3_app_walks/trainwalk/dtb.py`): `(rnd ... (sync 1) ...)`; the band `config/gen/rnd.dtb`
  does not override it, and no TU5 dtb (all 272 non-song dtbs decoded, `~/tmp/framerate/dta/`)
  sends `set_sync` (the only rate cheat is `frame_rate_screen`, a test screen). `Rnd::PreInit`
  reads `sync` (`rndcfg->FindData("sync", mSync)`), `DxRnd::Init` calls `SetSync(mSync)`.
- **`DxRnd::SetSync` 0x8273B108** (`va_disasm 8273b108 120`): `stw r4,0xf0(r3)`, then 0 ->
  `lis r4,-0x8000` (IMMEDIATE), 1 -> `li r4,1`, 2 -> `li r4,2`, into the
  SetRenderState_PresentInterval call (`bl 0x82851b28`). So sync 1 = present interval ONE.
- **`DxRnd::Present` 0x82739830**: with `mAsyncSwapCurrent` (+0x301) clear it calls
  `D3DDevice_SynchronizeToPresentationInterval` (`bl 0x82865c50`) before the swap, i.e. it
  blocks to the next vblank. No `bl` to `DxRnd::SetSync` exists in `.text` (it is virtual, set
  once at init); nothing in the data changes it.
- **Loop and poll.** `App::RunWithoutDebugging` 0x82270080 calls `SystemPoll` at 0x822700D8
  (its only caller in `.text`), and `SystemPoll` 0x82510270 calls `JoypadPoll` 0x82529AE0 at
  0x825102C8. The only other `bl JoypadPoll` is inside `JoypadPollForButton` (0x825275BC, a
  debug blocking read). So the pads are read once per frame, and every per-poll count is a
  count of frames. `BeatMatcher::CheckMercurySwitch` 0x82790B40 has one caller,
  `BeatMatcher::Poll` (0x827918F8), once per frame.
- The timer budgets in the same default.dtb are `(cpu (budget 16.7)) (gs (budget 16.7))`, a
  60 Hz frame. Retail therefore runs at most 60 frames a second; whether it holds 60 in a full
  band song is a performance question this lane did not measure (no console run).

The same chain is what DC3's native `FramePacer_Native.h` documents for DC3 (sync 1 on Xbox).

## 2. rb3-app before this lane

| mode | frame rate | clocks |
|---|---|---|
| `RB3_APP_DETERMINISTIC=1` | virtual 1/30 s frame (`kFrameNs` 33,333,333) | UI and task seconds: `MILO_HEADLESS` fixed step +1/30 s per poll (UI.cpp `UIManager::Poll`, Task.cpp `TaskMgr::Poll`); song clock: virtual steady_clock |
| normal launch | **unpaced**: run n0 (base binary, walk.txt, 6000 frames) took 33.1 s; menus ran ~650 frames/s (frames 217-263 in 0.07 s), the song ~210 frames/s (frames 2428-6000 in 16.8 s) | UI/task seconds still +1/30 per poll, so UI time ran 6-20x wall time while the song clock ran at wall time |

rb3-app forces `MILO_HEADLESS=1` (main_app.cpp), which is what selects the fixed step. So the
deterministic walk was a consistent 30 Hz machine, and a normal launch was not a consistent
anything: a tilt there needed 12 polls at ~200 polls/s, about 60 ms.

## 3. Game logic that counts frames, not time

Sweep of `src/band3` and the RB3-relevant `src/system` dirs (an Explore agent's list, the top
items spot-checked in source). At 30 Hz each of these runs at half retail's rate; unpaced, at
3-10x.

| what | where | count | effect at 30 Hz |
|---|---|---|---|
| tilt (mercury switch) filter | `beatmatch/MercurySwitchFilter.h` AnySign | `num_frames 12` (TU5 beatmatch.dtb) | hold 0.4 s, not 0.2 s (gameplay) |
| poll granularity of every hit | `SystemPoll` -> `JoypadPoll`; `BeatMatcher::Poll` stamps swings with the frame's `mNow` | 1 poll | swings snap to 33 ms, not 16.7 ms; the 25 / 52 ms windows in `BeatMatcher::Swing` span fewer polls; `JoypadController::NoSlotButtonsThisFrame` pairs more pro-drum cymbal/pad presses |
| vibrato detector | `dsp/VibratoDetector.cpp`, `Singer::Poll_` (Singer.cpp:568-580) | peaks 3-8 polls apart; bonus summed over the returned poll count, capped 20/poll | normal vibrato not recognised; max payout rate halves (vocals scoring) |
| mic energy smoothing | `Singer::Poll_` Singer.cpp:512, `0.9` per poll | 1 poll | reacts ~2x faster (vocals scoring) |
| octave-jump window | `Singer::UpdatePitchHistory`/`SuddenOctaveShift` | 5 polls | 167 ms window, not 83 ms |
| calibration lag | `CalibrationPanel::TriggerCalibration` CalibrationPanel.cpp:619 | `-16.666666f` = one 60 Hz frame; sensor read per poll | measured lag ~17 ms off |
| rollback (rewind) animation | `Game::Rollback`/`HandleRollbackAnimation` Game.cpp:639-657 | interpolator 0..60, `unk11c += 1` per poll | 2 s, not 1 s |
| auto-wah idle | `GuitarFx::Poll` GuitarFx.cpp:85-91, `KeysFx::Poll` | 120 polls | 4 s, not 2 s (audio) |
| menu-music fx crossfade | `synth/MetaMusic.cpp` | 90 polls | 3 s, not 1.5 s (audio) |
| UI select delay | `UIComponent::Poll` | `select_frames 15` (band ui_objects.dtb; system 30) | `select_done` 0.5 s after the press, not 0.25 s (UI) |
| pitch arrow / tail alpha smoothing | `VocalTrack.cpp:2209` (0.85), `Tail.cpp:191` | 1 poll | visual only |
| fixed 60 Hz lead | `TrackPanelDirBase::GetPulseAnimStartDelay` | `+16.7` ms | visual only |
| interstitial exit, talking poll | `InterstitialPanel::Exiting` (3 draws), `Player::PollTalking` (every 5th) | | negligible / no-op natively |
| debug | `Automator::Poll` (30 frames), `Timer::Reset` (60), `FlowManager` 60-frame average | | debug only |

Time-based and fine: joypad `hold_ms 500`/`repeat_ms 50` (JoypadClient, Timer ms), the
low-pass mercury filter (17 ms steps), `UIListState` scrolling (UI seconds), CameraShot
shake, spotlight damping, `NextSongPanel` (`DeltaUISeconds*30`), the watcher slop windows (ms).

Native-only frame counts (`HX_NATIVE`; not retail): UI.cpp `sStuckFrames`/fast-boot delays and
`sEnterWaitFrames > 90`, BandCharacter.cpp quiet-poll latches (90/600/120). They are native
scaffolding, now counting 60 Hz frames; untouched (UI.cpp and BandCharacter.cpp are outside
this lane's brief).

## 4. The change

Rather than rescale each count, rb3-app now runs at retail's rate, so all of the above behave
as retail at once.

`RB3_APP_HZ=N` (default **60**; `RB3AppHz()` in `native/src/rb3_app_determinism.cpp`):

- **Deterministic walk**: the virtual frame is 1/N s (16,666,666 ns at 60). The frame
  schedule became relative: frame N starts one frame after frame N-1, or at the next 1/N
  boundary after the clock reads frame N-1 consumed (each main-thread read is 1 us), as a
  missed vblank presents at the next refresh. The legacy absolute grid (`kBaseNs + N*kFrameNs`)
  was kept only for `legacy`: under it, the load on the calibration screen (frames ~120-150)
  pushed the clock ~10 s ahead, and every frame after advanced it by its reads alone (44 us a
  frame) for ~570 frames until the grid caught up, which the 1/30 fixed-step UI clock had hidden
  (first 60 Hz run d1: the splash -> cal_welcome transition took frames 125-872; after the fix,
  run d2, 125-257, i.e. 2.2 s at 60 Hz, as legacy's 104-171 is 2.2 s at 30 Hz).
- **Normal launch**: `PaceFrame()` (main_app.cpp, at the SystemPoll wrapper) sleeps each frame
  start to the next 1/N s boundary since the first frame (`clock_nanosleep` absolute); a frame
  that overran waits for the boundary after it (no catch-up burst). Not in a deterministic walk.
- **Clocks**: after `App::App`, main_app.cpp `unsetenv("MILO_HEADLESS")`, so `UIManager::Poll`
  and `TaskMgr::Poll` read their Timers (retail's path; natively steady_clock, which a
  deterministic walk makes its virtual clock) instead of adding 1/30 s per poll. The engine
  reads `MILO_HEADLESS` only inside the constructor (window, audio). UI.cpp caches it in a
  function-local static at its first poll, which is after the constructor, so it takes the
  Timer path without an edit there. **`TaskMgr::Poll` already polled inside the constructor**
  (`Splash::Draw`), so its static had cached `true`: in run d1 the task seconds ran +1/30 per
  60 Hz frame over frames 1-30. `src/system/obj/Task.cpp` (HX_NATIVE arm only) now reads the
  variable every poll.
- **`RB3_APP_HZ=legacy`** (or 0): virtual 1/30 s frame on the old absolute grid, normal launch
  unpaced, fixed-step clocks left on. Run d2L (bin-f2, walk.txt, 6000 frames) is
  **IDENTICAL over 6000 frames** to run d0 (base binary `8ba6acd11bde`, same walk), by
  `tools/rb3_app_det_cmp.py`, so the old behaviour is one variable away.
- chartplay `RB3_APP_CHARTPLAY_TILT_MS=N`: hold each tilt N ms of song time (one tilt per
  1000 ms at most), so a tilt is the same input at any frame rate. Without it the old 15-poll
  hold is unchanged.

## 5. Before / after

Binaries: `bin-base` `8ba6acd11bde` (main `7d3ca4f0b`), `bin-f2` `9debaf06cc5a` (this branch's
first commit). Every leg rc 0 at its frame cap.

**Clocks (walk.txt, seed 0, 6000 frames).** d0 (base) and d2L (`legacy`) identical over 6000
frames. d2 (60 Hz): menu task/UI seconds advance 2.5 s per 150 frames (1/60 a frame; legacy
1/30 a frame), the song's 5.0 s per 300 frames; game_screen settled at frame 1281 (legacy d0: frame 1112,
which at 1/30 s is about 1.7x the virtual time). Normal launch n2 (bin-f2, 4000 frames): 88.2 s,
the song at ~58 frames/s with four other rb3-app runs on the box; n0 (base) ran 6000 frames
in 33.1 s.

**Tilt, same input in song time** (`tilt.sh`, expert guitar by chartplay; prediction: a hold
of H ms deploys iff it covers 12 polls, i.e. H >= 400 at 30 Hz, H >= 200 at 60 Hz):

Run t2, beastandtheharlot both rates (legacy seed 2, 60 Hz seed 10), 200 s of song:

| leg | rate | tilt hold | tilts | deploys | first deploy | score / hits / misses at song 140 s |
|---|---|---|---|---|---|---|
| LN0 | legacy 30 | none | 0 | 0 | - | 87,344 / 721 / 0 |
| L300 | legacy 30 | 300 ms | 133 | **0** | - | 87,344 / 721 / 0 (= LN0) |
| L500 | legacy 30 | 500 ms | 6 | 6 | tilt at song 17,470 ms (poll 1966), deploy frame 1979: **13 polls, ~400 ms** | 113,942 / 721 / 0 |
| FN0 | 60 | none | 0 | 0 | - | 87,345 / 721 / 0 |
| F150 | 60 | 150 ms | 154 | **0** | - | = FN0 at every frame read (141,957 / 973 / 0 at frame 12000) |
| F300 | 60 | 300 ms | 8 | 6 by 140 s | tilt at song 17,472 ms (poll 2715), deploy frame 2728: **13 polls, ~200 ms** | 114,295 / 721 / 0 |

Run t1 (seed 18 both, so ohmygod at 30 Hz and smokeonthewater at 60 Hz) agrees: L300 85 tilts
0 deploys, L500 deploys at frame 2234 13 polls after the tilt at 2221 (codawalk's c3GT was
2222 -> 2234), F150 88 tilts 0 deploys, F300 deploys 12 frames after the tilt (4250 -> 4262).

So the 300 ms tilt that retail's filter accepts (18 frames >= 12) now deploys, a 150 ms one
(9 frames) still does not, and a deploy lands 200 ms after the tilt starts instead of 400 ms.
F300 out-scores L500 by 353 at 140 s with the same hits and deploys: each deploy starts 200 ms
earlier, so more notes fall inside it.

**Hits at the doubled poll rate.** LN0 and FN0 hit the same gems (55 / 250 / 513 / 721 at
song 30 / 60 / 100 / 140 s, 0 misses); scores differ by at most 1 point (29,212 vs 29,213 at
60 s; sustain points read at a different poll).

**X360 match build.** The only `src/` edit is Task.cpp's `HX_NATIVE` arm, which the match build
never compiles. Predicted Δ0. `tools/ab_measure.py --patch` (the revert of that hunk) measured
Δmatched +0, Δhonest +0, Δcode% +0.000000 pp, Δfuzzy +0 (legA = legB: matched 55,268,
masked_equal 25,360, code% 59.665310 on `name_check`; `none` control +0 B). Run dir
`~/tmp/wt-framerate/.ab_measure_runs/` (label `framerate-task-headless-revert`). A first run
refused at apply because I edited `tilt.sh` in the worktree while it ran (my mistake; its
restore put the file back, nothing was lost).

## Not done

- **Walk scripts are written in frames** (`MILO_INPUT_SCRIPT` `+N` offsets and `wait_screen`'s
  1,800-frame timeout count frames, in the engine's input script). At 60 Hz a `+30` is 0.5 s,
  and seven chained `wait_screen` lines cover 210 s instead of 420 s, so a long song can
  outlast a chain written for 30 Hz, and `RB3_APP_FRAMES` caps need doubling for the same
  song time. walk.txt and walk_x.txt ran unchanged here (every step waits on a screen). A lane
  that wants its old frame numbers, or an old baseline, runs `RB3_APP_HZ=legacy`; that is the
  base binary frame for frame (d0 vs d2L). The coordinator decides the default at landing:
  the one-line flip is `if (!e || !*e) hz = 60;` in `RB3AppHz()`.
- **Baselines and seed -> song maps move.** The menus draw from the game RNG per frame, so a
  seed picks a different song at 60 Hz (legacy 18 = ohmygod, 60 Hz 18 = smokeonthewater;
  60 Hz seeds 0-11 in `seeds_a.out`: 0 bohemianrhapsody, 1 thebeautifulpeople, 2 walkoflife,
  3 getfree, 4 outerspacemuffs, 5 beencaughtstealing, 6 saturdaynightsalright, 7 lowrider,
  8 combatbaby, 9 none by frame 2300, 10 beastandtheharlot, 11 whipit). Lanes that name a song
  by seed must re-find it (`tools/rb3_app_walks/framerate/seeds.sh`).
- **A normal launch is slower in wall time**: paced at 60 frames/s instead of ~200-650, so a
  6000-frame normal walk takes ~100 s, not ~33 s (n2: 4000 frames in 88.2 s with four other
  rb3-app runs on the box; the song ran at ~58 frames/s). `RB3_APP_SECONDS` caps may need
  raising for long normal walks.
- **Engine**: nothing to change for correctness. Its `NativeCompatFlags` text for
  `MILO_HEADLESS` says "UI.cpp fakes a fixed 1/30s UI clock", which is no longer true for
  rb3-app at a real frame rate (rb3-app clears the variable after the constructor); worth a
  word there, not edited (engine is outside this lane).
- UI.cpp's native frame counters (`sStuckFrames`, fast-boot delays, `sEnterWaitFrames > 90`) and
  BandCharacter.cpp's quiet-poll latches now count 60 Hz frames, so their wall-time thresholds
  halve. They are native scaffolding, not retail; not touched (outside the brief).
- No console measurement of retail's achieved frame rate in a full band song (the code caps it
  at 60; a 360 that drops frames would count fewer polls, as rb3-app does on a loaded box).
- Per-item before/after walks only for the tilt filter. The other frame counts (vibrato,
  auto-wah, rollback, UI select delay) follow from the same frame rate and were not walked one
  by one; the UI select delay has no line in the logs to read it from.
- `tools/native_health.sh --selftest` left to the coordinator (brief). Its rb3-app target is
  linked but not run, so the gate's runtime half is not affected.
