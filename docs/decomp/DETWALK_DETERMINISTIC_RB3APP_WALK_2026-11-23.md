# DETWALK: a deterministic rb3-app walk (2026-11-23)

Lane detwalk, branch `detwalk`, worktree `~/tmp/wt-detwalk`, base main `6c9c5077d`.

## Goal

Two runs of one rb3-app binary on the a25 walk
(`~/tmp/claude-1000/rb3-scout/a25/walk.txt`: returning boot, calibration, main_hub,
song_select, a played song, endgame presses) should agree frame by frame, so a walk
can A/B two builds and a one-byte behaviour change shows up as a divergence. Opt-in:
`RB3_APP_DETERMINISTIC=1`. Unset, rb3-app behaves as before.

## What changed

- `native/src/rb3_app_determinism.cpp` (new; classified in
  `tools/native_stub_census.py` `NON_STUB_RE`). Three hooks, all pass-through unless
  `RB3_APP_DETERMINISTIC` is set:
  - `__wrap__Z8SeedRandi`: SystemInit's `SeedRand(time.wMilliseconds)`
    (os/System.cpp, native arm) seeds with `RB3_APP_SEED` (default 0). Every later
    draw is the game's own `Rand::sRand`, so song_select, the shell-music loop pick
    and FileMergerOrganizer run their retail paths unchanged.
  - `__wrap__Z10ThreadCall*` / `__wrap__Z14ThreadCallPollv`: ThreadCall_Native.cpp runs
    each job (DTA loads etc.) on a pthread, so the frame its done callback lands on
    depends on scheduling. With the flag, the wrappers keep their own 12-slot queue and
    run the job on the main thread: a poll delivers the done callback of the job it
    started at the previous poll, then starts the next one (the threaded path's
    shortest latency, made fixed). The real worker is created and never released.
  - `__wrap__ZNSt6chrono3_V212steady_clock3nowEv`: on the main thread, a virtual
    clock. Every native `Timer` (`__mftb`, os/Timer.h) reads steady_clock, and so do
    LoadMgr's per-frame load budget (`CheckSplit`), UIManager's timer and Game's song
    clock in realtime mode. Frame N starts at 1000 s + N/30 s; each read adds 1 us,
    so a timer-bounded loop still ends; the clock never goes backwards. Other threads
    keep the real clock (their condition-variable deadlines must stay real).
  - `rb3_app_determinism_frame(N)`, called from main_app.cpp's SystemPoll wrapper
    at each frame start: prints
    `rb3-app-det: frame N state H chain C rand i1/i2 seconds S beat B ui U screen X transition Y`.
    `state` hashes the RNG object's bytes, the three TaskMgr clocks and the screen
    names; `chain` folds every earlier frame in.
- `native/src/main_app.cpp`: two declarations and two call lines
  (`rb3_app_determinism_init()` first in main, `rb3_app_determinism_frame()` after the
  frame counter increments).
- `native/CMakeLists.txt`: the new source and five `--wrap` options on rb3-app.
- `tools/rb3_app_det_cmp.py`: compares the `rb3-app-det` lines of two run logs over
  their common frames; exit 0 identical, 1 with the first differing frame.

## Acceptance test

Recipe: `~/tmp/detwalk/runb.sh` is a copy of `a25/runb.sh` with `A=~/tmp/detwalk` and
`X=~/tmp/wt-detwalk`. Every run goes through `coord/no610m.sh` under the shared
`gpu1.lock`, with a fresh reflink copy of `memcard/content_a`. Long runs:
`BIN=<copy> SECS=540 TMO=580 ./runb.sh <tag> a25/walk.txt 90000 RB3_APP_DETERMINISTIC=1`.
Binaries were copied out of `native/build` before each run, so a rebuild could not
change a running one:

| binary | source | sha256 (first 12) |
|---|---|---|
| `bin-base` | branch tip `6788ba690` (main `6c9c5077d` + this lane) | `fc28ee6444c3` |
| `bin-sab` | + FileMerger.h copy ctor `mForceReload = false` -> `true` (one byte) | `2c0337571d0a` |
| `bin-rev` | + the literal revert of `83298d958`'s two HX_NATIVE inits | (not hashed) |

A rebuild of the tip after both experiments was byte-identical to `bin-base` (`cmp`).

Prediction before the runs: A and B identical over their common frames; the sabotage
diverges where FileMergerOrganizer::StartLoad first draws, before App::Run's frame 1
(`83298d958`'s message: four extra RandomInt draws move the shell-music pick); the
literal revert probably does NOT diverge, because that commit measured the byte as 0
on fresh glibc memory without ASan.

Results (A, B, S started together at 03:57-03:58 UTC, R a minute later; four
concurrent runs plus a rebuild, load average 55-90):

| run | binary | frames in 540 s | song (`songs/<x>/`) | game_screen / coop_endgame_screen at frame | vs A |
|---|---|---|---|---|---|
| A | bin-base | 39,886 | thecon | 1119 / 8009 | - |
| B | bin-base | 40,448 | thecon | 1119 / 8009 | **IDENTICAL over 39,886 common frames** |
| S | bin-sab | 37,860 | imagine | 1241 / 7319 | **first divergence at frame 1** (all 37,860 frames differ) |
| R | bin-rev | 41,160 | thecon | 1119 / 8009 | IDENTICAL over 39,886 common frames |

- A vs B: every `rb3-app-det` line through frame 39,886 is equal (last:
  `frame 39886 state 0db4b43a chain 6e818e2499378051 rand 131/234 seconds 1028.0509 beat
  2528.1362 ui 1034.7689 screen coop_endgame_screen`), and so is the screen-change
  timeline once its wall-clock `(%.2f s)` suffix is dropped. The runs end at different
  frames only because the 540 s wall-clock cap is real time; compare the common prefix.
  The walk covers the boot, calibration, main_hub, song_select, a whole played song and
  the endgame screen.
- A vs S: frame 1 already differs, `rand 106/209` vs `rand 110/213`. That is four extra
  draws before the first frame, as predicted. The game then picks another song (thecon
  -> imagine) and every later frame differs. So the instrument detects this one-byte
  change at once.
- A vs R: identical. The plain revert changes no behaviour in this build: the
  uninitialised byte reads 0, which is what the init writes. This is the predicted
  negative, and it is not a miss: the walk cannot see a source change that changes no
  behaviour. To show the field matters, force its value (S); do not just delete the init.
- Short smoke pair first (s1/s2, 3000 frames, SECS=150): identical over 3000 frames.

### Default runs (flag unset)

- Every wrapper's first test is `gDet`, which is false, and it then makes the real call.
  `rb3_app_determinism_frame` returns before it reads anything. A default run of
  `bin-base` (d1, 3000 frames) prints no `rb3-app-det` line, reaches game_screen at
  frame 1267 and ends rc 0, which matches the a25 runs.
- Default runs are not deterministic, and that is the reason for the flag. The a25 runs
  w7/w8/w9 (one binary) already disagree on screen-change frames from the splash on
  (cal_welcome_screen at 176 / 175 / 175, cal_manual_video_screen at 269 / 267 / 268).

## Findings and limits

- The seed hook pins nothing new today: the native `GetLocalTime` shim
  (`native/src/xdk_shims.cpp`) sets `wMilliseconds = 0`, so the log reads
  `SeedRand(0) -> SeedRand(0)`. The real run-to-run noise came from the clock (load
  budget, song clock) and from ThreadCall scheduling. The hook stays in, so the seed
  remains fixed if the shim ever returns real milliseconds, and `RB3_APP_SEED=<n>`
  picks another seed (another song) through the retail path.
- I did not run an ablation (each hook disabled alone), so I have not measured how much
  divergence each hook removes on its own. The 39,886-frame identity holds with all
  three hooks on.
- The state hash covers the RNG object, TaskMgr seconds/beat/ui-seconds and the screen
  and transition names. A change that alters neither the RNG draw count nor any clock
  nor the screen flow (for example, a render-only difference) is invisible to it. Extend
  `rb3_app_determinism_frame` when you need that.
- Threads other than main keep the real clock. Anything they compute that main later
  reads (audio, xdk_shims threads) could still diverge; none did in this walk.
- Not run, as the brief asked: `tools/native_health.sh --selftest`.
- Not landed. Main moved to `58580d6e2` during the lane (it touches main_app.cpp);
  `git merge-tree` of detwalk onto it is clean.

Logs: `~/tmp/detwalk/r_{A,B,S,R,s1,s2,d1}/run.log`; comparisons from
`python3 tools/rb3_app_det_cmp.py ~/tmp/detwalk/r_A/run.log ~/tmp/detwalk/r_B/run.log` (and `r_S`, `r_R`).
