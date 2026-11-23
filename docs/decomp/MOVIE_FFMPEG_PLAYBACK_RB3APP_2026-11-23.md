# rb3-app plays movies: retail's Movie::Impl over the engine's FFmpegMovieImpl (2026-11-23)

Lane movie, branch `movie` (from main `4123d5744`), worktree `~/tmp/wt-movie`. Scratch, logs
and screenshots: `~/tmp/movie/` (`r_<tag>/run.log`). Walk scripts: `tools/rb3_app_walks/movie/`.
Every run went through `coord/no610m.sh` (in `run.sh`). The full native selftest was left to
the coordinator.

## Problem

rb3-app had no movie player. Its `Rb3AppMovieImpl` (`native/src/rb3_app_native_glue.cpp`)
modelled Bink's `BinkOpen` as failing, so `Movie::Poll` returned false on the first poll and
every `MoviePanel` sent `movie_done` at once. The boot intro went straight to `splash_screen`,
and a guitar tutorial (`trainers_guitar_tutorial_01_screen`) awarded `acc_guitartutorial01`
and went back to `accomplishment_screen` one frame after it was entered. TRAINWALK finding 5
and KEYSWALK finding 4 recorded that bounce.

## How dc3-decomp selects the backend, and why rb3-xenon could not

dc3: `BinkMovieSys::CreateMovieImpl` returns `new FFmpegMovieImpl()` under `HX_FFMPEG`, and
otherwise `WebMovieImpl` (`src/system/moviebink/BinkMovieSys.cpp:85`). The engine's copy
links (dc3 `native/CMakeLists.txt:409`, H-5).

rb3-xenon listed `FFmpegMovieImpl.cpp` in `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE`, because
`movie/MovieImpl.h` declared `virtual bool SetPaused(bool)` and the engine overrides it with
`void`. The engine TU also reads `TheBinkMovieSys.Track()`, which xenon's `BinkMovieSys` did
not have. The engine was not edited and no engine branch was needed.

## Retail evidence (tools/va_disasm.py on band.exe; map names from scripts/target_symbol_map.json)

- `?SetPaused@Impl@Movie@@QAAX_N@Z` (0x82744348): returns **void** (`X`). DC3's
  `MovieImpl_p.h` says void too. No caller reads a result.
- `Movie::Impl::Poll` (0x82745628), read instruction by instruction:
  1. `CheckOpen(1)` true means `return 1`.
  2. `mBink` (+0x14) null means `return 0`.
  3. The time callback (+0xA8) unpauses the movie when it returns non-zero.
  4. `BinkWait`/`DoFrame`/`BinkShouldSkip` loop.
  5. `return 0` when `bink->ReadError` (+0x1C) is set, or when `!mLoop` (+0x24) and
     `bink->FrameNum` (+0xC) == `bink->Frames` (+0x8). Otherwise `return 1`.

  FinishFrame calls `NextFrame` only while `FrameNum != Frames`, so FrameNum is one past the
  frame shown. Retail therefore ends when frame Frames-1 has been decoded.
- `MoviePanel::IsLoaded` (0x827AF968) returns `Movie::Ready()`, then `!mSubtitlesLoader ||
  loader->IsLoaded()`, then `UIPanel::IsLoaded()`. The native arm skipped `Ready()`.
- `MoviePanel::Exit` (0x827AF9D8): `UIPanel::Exit`; `if (!mPreload) mMovie.End()`;
  `mShowMenu = 0`. The source is unchanged.
- Data: `system/run/config/movie.dta` `is_timed_movie` = `{&& {exists meta_performer} {! {meta_performer
  is_set_complete}}}`. `ui/splash/splash.dta` `intro_movie_panel` has preload FALSE, audio TRUE
  and loop FALSE, and BUTTON_DOWN start/confirm goes to `splash_screen`.
  `ui/meta.dta` `MOVIE_PANEL_STUFF`: start shows or hides the pause menu (`set_paused`), and
  `skip.btn` sends `movie_done`.

## Changes

src (commit `cb4812986`). `ab_measure --from-dirty`: Δmatched +0, Δmasked_equal +0,
Δhonest +0, Δcode +0 B, Δfuzzy +0.000000 pp. Ruler name_check, objdiff `ec3badc0`. The none
control was also +0 B. Leg B recompiled 124 objs. Prediction was Δ0, because the change is
native arms plus unpinned MovieImpl/BinkMovieImpl:

- `movie/MovieImpl.h` and `moviebink/BinkMovieImpl.{h,cpp}`: `SetPaused` now returns `void`.
- `moviebink/BinkMovieSys.h` (`HX_NATIVE`): a static `Track()` and
  `extern BinkMovieSys &TheBinkMovieSys`. No member was added, because RB3 keeps the forced
  track in the global `gForceTrack`.
- `movie/Movie.cpp`, native arm: `gForceTrack`, `OnMovieSetTrack`, `BinkMovieSys::Track()`
  and `TaskMgrDeltaSeconds`. `Movie::Init` registers `set_bink_track`, as
  `Movie::Impl::Init` does.
- `movie/MovieSys.cpp`, native arm: `TheBinkMovieSys = gBinkMovieSys`.
- `meta/MoviePanel.cpp`: `IsLoaded` keeps retail's `Ready()` test on desktop. Only
  `__EMSCRIPTEN__` still skips it.

native (commit `cdf456889` plus the follow-up commit):

- `FFmpegMovieImpl.cpp` was removed from `MILO_ENGINE_DECOMP_PLATFORM_EXCLUDE`, so
  libmilo-engine now compiles it against xenon's headers.
- `Rb3AppMovieImpl` now derives from `FFmpegMovieImpl` and keeps retail's front end. Each
  method below is described by what it does in retail:
  - **Begin, Ready, IsLoading:** the FileLoader for a preloaded movie, reading from a cached
    stream when it has one.
  - **CheckOpen:** keeps the loader buffer, runs `stream_begin`, then does MovieOpen:
    `FFmpegMovieImpl::BeginFromFile` on the movie's file. FinishOpen pauses the movie and
    SharedFinishOpen unpauses it when asked. Poll asks; Save does not.
  - **Poll:**
    - A timed movie is paused while `TaskMgrDeltaSeconds() == 0`.
    - Retail's end test: no loop and decoded + 1 >= NumFrames.
  - **SetPaused:** a timed movie with a stopped clock stays paused, and nothing happens
    before the movie is open.
  - **Draw:** with an open movie, draws SetRect's rectangle (0x82743040) with
    MovieInternalBuffers::New's material (pre-lit, kBlendSrc, no alpha write, z off, clamp).
    The texture is a `kRendered` RndTex filled by `UploadRGBAToRndTex`.
  - **GetFrame, MsPerFrame, NumFrames:** as retail (FrameNum-1, or Frames while FrameNum is
    1; 0 with no movie).
  - **End:** runs `stream_end` when there is no preload buffer, then closes the movie and
    frees the loader, buffer, texture and material.
- Clock. With a sound track on the audio device, Bink-like audio pacing is the engine's. With
  no audio clock (headless has no device) or under `RB3_APP_DETERMINISTIC`, the picture
  follows `TheTaskMgr.UISeconds()`. The main-thread `Timer` was tried first and refuted:
  under the deterministic wrap it advanced about 0.017 ms per frame, so the intro was still
  on frame 1 after 2300 frames.
- Env:
  - `RB3_APP_MOVIE_LOG=1` prints one line per pause change and one per end.
  - `RB3_APP_MOVIES=0` restores the old no-decoder behaviour (see Walk impact).
- `TexMovie` keeps the retail `DrawToTexture`, so its movie draws through `Movie::Draw`
  into the target. `HX_FFMPEG` is still undefined for rb3-app. No TexMovie opened in any
  run below.

## Measurements (rb3-app, headless, RB3_APP_DETERMINISTIC=1 unless noted)

| run | prediction | result |
|---|---|---|
| boot, `w_boot.txt`, 2300 frames | the intro (2054 frames at 29.97 fps, 68.5 s) hands off to splash at about frame 2055 | `rb3_intro_cinematic.bik` opened at frame 1. Transition to `splash_screen` at **frame 2057** (ui 68.50 s). Screenshots: movie frames at 1000 and 1500, the RB3 logo at 2050 |
| the same, run twice | identical det chains | `diff` of every `rb3-app-det` line (frame, state, chain): **empty**, 2302 lines |
| intro skipped (`120 start`) | BUTTON_DOWN start goes to splash | transition at frame 122 |
| pro guitar campaign route (keyswalk `walk.txt` plus `120 start`), 9000 frames | the tutorial plays its length, then movie_done earns the goal and goes to accomplishment | `guitartutorial01.bik` (5667 frames at 59.94 fps) opened at the first panel poll (frame 4262, after the screen transition). Transition to `accomplishment_screen` at **frame 7098** (≈ 94.5 s of movie, retail's Frames-1 end, plus the transition) |
| `w_pause.txt`: start at +300, `a` (resume.btn) at +600, start at +900, down, `a` (skip.btn) | pause freezes the frame, resume continues, skip ends the movie | `pause at frame 716` (game frame 4620), `play at frame 716` (no advance across 300 paused game frames), `pause at frame 1287`. The 571 frames played = 9.53 s of UI clock: the resume lands at SELECT_DONE, about 14 frames after the press. Skip: transition to accomplishment at frame 5296 |
| `RB3_APP_MOVIES=0` boot, 200 frames | the old flow | no movie opened, transition to splash at frame 3 |
| plain headless boot (no det) | the movie runs; no sound device | opens with `sound off` (AudioDevice skipped under MILO_HEADLESS). The picture follows the UI clock |

A sound track on a real audio device was **not** exercised (no headless device), so the
engine's audio-clock path is untested here.

## Walk impact (read before landing)

The boot intro now plays 2055 frames, longer than `wait_screen`'s 1800-frame timeout. An
unmodified walk (trainwalk `w_trk.txt`, run `trk_noskip`) logged
`TIMEOUT waiting for 'splash_screen'`. Its `+30 start` then landed on the intro, which skipped
it, so splash never got its own start and the next wait timed out. Two fixes:

- Prepend `120 start` to a walk. This is retail's skip, and the rest of the walk is unchanged
  (the keys and pro guitar walks reached the same screens).
- Or run with `RB3_APP_MOVIES=0`, which gives the pre-lane flow and keeps old baselines
  comparable.

Tutorial walks now stay on the tutorial for its length (≈ 2835 game frames for
guitartutorial01) unless they skip it with start, down, a.

## Not done / open

- **A movie that exists only inside a cached .milo** (TexMovie's `splash.tmov` →
  `startup.bik`) still does not open. Retail opens a preloaded movie from the memory buffer,
  and the engine's FFmpegMovieImpl opens only by path. Fixing it needs an engine change (an
  AVIO over the buffer). It was not made, and no engine branch was opened.
- Not modelled:
  - several movies opened together sharing frame buffers (SharedFinishOpen's refcount,
    which also mutes all but one)
  - the async decode thread
  - the BIKi endian swap
  - disc-contention reporting

  Retail draws the YUV planes with `kMovieShader`; this draws an RGBA frame from FFmpeg's
  conversion, and colour was not compared against retail.
- The tutorial screenshot at frame 4700 (`~/tmp/movie/shots_pause`, earlier run) showed the
  `movie_pause.milo` overlay with RESUME/SKIP and "PRESS START TO PAUSE" together before any
  press. This was not checked against retail. The panel's milo state was never visible before
  this lane, so it may be a pre-existing UI-anim issue.
- Every native target was built in the worktree (`cmake --build native/build`, rc 0, no
  errors or undefined symbols), so moving FFmpegMovieImpl into libmilo-engine broke no link.
  `tools/native_health.sh --selftest` was **not** run (left to the coordinator).
- `native_stub_census.py --authored` ran clean (rc 0, 19 targets, 683 rows). No native/src
  file was added; the movie symbols in the census are the pre-existing `gBinkMovieSys` and
  `Rb3AppMoviePreload`.
