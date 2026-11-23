# GEMWALK: an autoplayed guitar scoring walk in rb3-app (2026-11-23)

Lane gemwalk, branch `gemwalk` (from `scorewalk` `602f2bb07`), worktree `~/tmp/wt-gemwalk`.
Scratch, logs and binaries: `~/tmp/gemwalk/` (`runb.sh` is a copy of `rb3-scout/a25/runb.sh`
with `A=~/tmp/gemwalk`, `X=~/tmp/wt-gemwalk`; every run goes through `coord/no610m.sh` under the
shared `gpu1.lock`, with a fresh reflink copy of `memcard/content_a`; `runs.sh` is the run set,
`disc.sh` the short discovery walk).

## Goal

The scorewalk walk (`SCOREWALK_DETERMINISTIC_SCORING_WALK_2026-11-23.md`) scores, but only on
vocals: the native pad is a standard gamepad, which RB3 maps to vocals. This lane adds an
opt-in pad kind so the same deterministic, autoplayed walk plays a gem instrument (guitar),
then runs it for identity under plain, malloc-perturb and ASan+UBSan builds.

## What changed

| commit | what |
|---|---|
| `38330416f` | `RB3_APP_JOYPAD_KIND=guitar\|drums`; native `SetupHXGuitar` / `SetupHXDrums` (retail 0x82529E20 / 0x82529EB8); their two aborting stubs dropped |
| `19eadd1e4` | **fix**: rb3-app defines `RB3_NATIVE_HAS_BEATMATCHER` (the target links `BeatMatcher.cpp`) |

- **`RB3_APP_JOYPAD_KIND`** (`native/src/rb3_app_joypad.cpp`): unset or `gamepad` keeps
  today's pad (XInput subtype 1, `kJoypadAnalog`, joypad config `analog` -> vocals). `guitar`
  reports subtype 6 with Flags 0, which retail `SetupHXGuitar` reads as `kJoypadXboxHxGuitar`
  -> controller `strat_xbox` (the HX_XBOX `controllers` block of `system/run/config/joypad.dta`;
  GUITAR_WITHOUT_CALBERT, so calibration stays manual). `drums` reports subtype 8, Flags 0 ->
  `kJoypadXboxDrums` -> `hx_drums_xbox`. Any other value exits 2 at the first caps query.
  Only the caps change; button bits, binding and messages are the same retail path.
- **`SetupHXGuitar` / `SetupHXDrums`** were `peripheral-only` aborting stubs in
  `rb3_app_link_stubs.s` (their TU, `os/Joypad_Xbox.cpp`, is not in rb3-app: it defines the
  XInput2 thread and the `JoypadInit`/`Poll`/`GetXinputSinceLastFrame` this file replaces).
  They are now defined in `rb3_app_joypad.cpp`, checked against retail bytes
  (`tools/va_disasm.py`): 0x82529E20 stores Flags bit 1 to JoypadData +0x4b (`mIsWireless`) and
  bit 0 to +0x4a (`mHasCapFlag1`), returns 0x1d on `sThumbLX == 0x1BAD` (after
  `GetBreedData`), else `bit1 && (bit0 || sThumbRX >= 0x100) ? 6 : 5`; 0x82529EB8 does the
  same stores, 0x21 on 0x1BAD, 9 on `bit0 || sThumbRX >= 0x100`, else `subfic/subfe` ->
  10 (Rock of Ages: bit1 && !bit0) or 8. One deliberate difference: the 0x1BAD arm skips
  `GetBreedData` (0x82529D18), which reads the breed table the absent XInput2 thread fills;
  natively the caps are zeroed, so that arm cannot be taken. Keytar / RealGuitar stay stubbed.
  `tools/rb3_app_link_stubs_gen.py`'s comment says so (the `.s` was edited by hand: two blocks
  and their two name strings removed).
- No `src/` file changed, so there is nothing for `tools/ab_measure.py` to price (the match
  build compiles none of `native/`); not run.

## Findings

### 1. The guitar pad aborted on its first poll (fixed: native SetupHXGuitar)

Prediction: subtype 6 alone gives a guitar. **Wrong.** The first guitar walk (bin-g1) died at
frame 1: `UNREACHED stub [ring peripheral-only] SetupHXGuitar`, rc 134, because
`ReadSingleXinputJoypad` (compiled, retail) calls it for subtype 6. The stub generator's own
comment had predicted exactly this ("a guitar/drums/keytar/pro-guitar pad would").

### 2. The first gem player failed in SongData (fixed: RB3_NATIVE_HAS_BEATMATCHER)

With the Setup bodies (bin-g2), the guitar walk went through the same screens as the vocal walk
and failed at game start: `FAIL: SongData: built without RB3_NATIVE_HAS_BEATMATCHER, cannot
drive a BeatMatcher`, rc 134. `ae973d006` compiled SongData's three BeatMatcher loops out
because, measured 2026-10-06, no native program linked `beatmatch/BeatMatcher.cpp`; rb3-app
has since added it (`RB3_APP_SOURCES`), GemPlayer hands SongData its BeatMatcher
(`AddBeatMatcher`), and nothing defined the macro. A vocal player never constructs a
BeatMatcher, which is why the vocal walks never reached this. Fix: rb3-app defines
`RB3_NATIVE_HAS_BEATMATCHER` (`native/CMakeLists.txt`); `SongData.cpp`'s comment already says a
target that links BeatMatcher.cpp must. Control: V below (default pad, bin-g3) is identical to
scorewalk's A2 (bin-fix2, without the define) over all 45,818 common frames, so the define
does not move the vocal walk.

### 3. The guitar walk needs no new screens

Prediction: the guitar needs extra calibration or part screens. **Wrong.** With a non-Calbert
guitar, `cal_welcome_panel` (`ui/calibration/calibration.dta`) defaults to `cal_manual` and the
a25 presses (`right`, `a`) land on the same `cal_manual_audio/video/complete` screens;
`a` (kPad_X, bit 6) is `kAction_Confirm` for `strat_xbox` as for the gamepad. `walk.txt` in
`~/tmp/gemwalk/` is a verbatim copy of `a25/walk.txt`. The screen sequence through
`part_difficulty_screen` -> `game_screen` matches the vocal walk's; the song differs
(the guitar walk plays `thelook` twice; the vocal walk `thecon` then `iwannabesedated`).
A and V share every frame up to 62 and split at 63, right after the scripted Start
(poll 60): from there on the guitar walk has drawn 2 fewer RNG values (`rand 125/228` vs
`127/230`).

### 4. Guitar scoring, overdrive energy awarded

Song 1 (`thelook`, guitar, `track 1`, autoplay): 184/184 gems hit, 0 misses, streak 184,
player score 17,774 (band 17,775), 5.0 stars, crowd 1.0; song 2 (`thelook` again) scores the
same after the stats reset to 0 at its start (frame 10,380). `energy` rises (0.502 by frame
4000) and reaches 1.0: overdrive energy is awarded on guitar, so scorewalk's open item 3
(vocal energy stays 0) is vocal-specific. `deploy` stays 0: autoplay never deploys. I did not
check whether retail autoplay deploys either (open). The no-autoplay control N scores 0 with
151 misses by frame 8000 and crowd 0.20. The record does not include difficulty.

## Runs (90000-frame cap, SECS=540 TMO=580, `~/tmp/gemwalk/walk.txt`, all `RB3_APP_DETERMINISTIC=1`)

bin-g3 `7ec7ba48a281` (tip `19eadd1e4`), bin-asan-g3 `4cc8ecd1f3d3`
(`-DRB3X_SANITIZE=address,undefined`). Predictions: A = B = P; S = A over its prefix;
N diverges at the first GemPlayer frame; V = scorewalk A2.

| run | binary | extra env | frames | vs A |
|---|---|---|---|---|
| A | bin-g3 | KIND=guitar AUTOPLAY | 46,471 | - |
| B | bin-g3 | KIND=guitar AUTOPLAY | 46,584 | **IDENTICAL over 46,471** |
| P | bin-g3 | KIND=guitar AUTOPLAY `GLIBC_TUNABLES=glibc.malloc.perturb=190` | 46,586 | **IDENTICAL over 46,471** |
| S | bin-asan-g3 | KIND=guitar AUTOPLAY, scorewalk's ASAN/UBSAN_OPTIONS | 22,604 | **IDENTICAL over 22,604** (both songs) |
| N | bin-g3 | KIND=guitar (no autoplay) | 49,358 | first difference frame 1352 (`auto 1` vs 0) |
| V | bin-g3 | (default pad) AUTOPLAY, `a25/walk.txt` | 45,818 | vs scorewalk A2: **IDENTICAL over 45,818** |
| DA | bin-g3 | KIND=drums AUTOPLAY (`druns.sh`) | 39,042 | drums leg: - |
| DB | bin-g3 | KIND=drums AUTOPLAY | 38,983 | vs DA: **IDENTICAL over 38,983** |
| DP | bin-g3 | KIND=drums AUTOPLAY, perturb 190 | 38,938 | vs DA: **IDENTICAL over 38,938** |
| DS | bin-asan-g3 | KIND=drums AUTOPLAY, ASan+UBSan | 12,929 | vs DA: **IDENTICAL over 12,929** (into song 2) |

- Drums (`track 0`, `hx_drums_xbox`, same `walk.txt`): `livinginamerica` 466/466 hit, score
  45,175, 5.0 stars, then `freebird` 899/899, 96,858; energy reaches 1.0, deploy 0. DS: 29
  UBSan file:line sites, all in S and in scorewalk's S2; 0 ASan, 0 FAIL, 0 UNREACHED.
- Every run ends `rc=4`, the 540 s wall-clock cap (`RESULT: APP RAN N FRAMES`), on
  `coop_endgame_screen` after song 2, where the script ends. Compare common prefixes only.
  Last common A/B/P line: `frame 46471 state 853c348d chain 56d0d36eaaa8de0a`.
- FAIL lines: 0 in every run; UNREACHED stubs: 0; ASan reports: 0 in S.
- UBSan in S: 29 unique file:line sites (`~/tmp/gemwalk/S.sites`), **all 29 also in
  scorewalk's S2** (`comm` against `~/tmp/scorewalk/S2.sites`, which is checked against the
  sanitize/asanwalk/asanwalk2 records there); the one S2 site S did not hit is
  `StreakMeter.cpp:208`. The gem path (BeatMatcher, TrackWatcher, GuitarTrackWatcherImpl,
  GemPlayer, the BeatMatcher loops SongData now compiles) adds **no** UBSan site.
  So there was no new report or divergence to triage against retail in this lane.
- Gates: `RB3_APP_JOYPAD_GATE=1` PASSED (rc 0) with the default pad (bin-g1, bin-g2), and
  with `KIND=guitar` and `KIND=drums` (bin-g2).

## Excluded areas (not fixed, as briefed: Rnd/Draw/Text/Spotlight, ui, char, net sync/game-start)

Reached and seen, same sites as scorewalk S2: `char/CharClip.cpp:187` (char);
`net/MatchmakingSettings.cpp:45,98` (net); bandobj render-side sites (`BandHeadShaper`,
`BandFaceDeform`, `GemTrackDir.cpp:487`, `BandTrack`) are earlier records' verdicts, not
reopened. None touched.

## Not done

- `tools/native_health.sh --selftest` not run (brief). Only rb3-app (plain + ASan) was built;
  the CMake change touches only rb3-app's definitions.
- Keytar / pro guitar (`SetupHXKeytar`, `SetupHXRealGuitar`) stay aborting stubs; no kind for
  them. No no-autoplay drums control (N covers the guitar).
- Not checked: whether retail autoplay deploys overdrive; why the guitar walk picks
  `thelook` twice (the song list's cursor or the RNG); difficulty is not in the record.
- Not landed; nothing pushed.
