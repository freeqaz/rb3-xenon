# SCOREWALK: a deterministic rb3-app walk that scores (2026-11-23)

Lane scorewalk, branch `scorewalk`, worktree `~/tmp/wt-scorewalk`, base main `d1e64a303`.
Scratch, logs and binaries: `~/tmp/scorewalk/` (`runb.sh` is a copy of `a25/runb.sh` with
`A=~/tmp/scorewalk`, `X=~/tmp/wt-scorewalk`; every run goes through `coord/no610m.sh` under
the shared `gpu1.lock`, with a fresh reflink copy of `memcard/content_a`).

## Goal

The detwalk record (`DETWALK_DETERMINISTIC_RB3APP_WALK_2026-11-23.md`) hashes the RNG, the
clocks and the screen names, and the a25 walk plays a song with no instrument input, so
scoring was barely exercised and no score reached the hash (perturb2's "what this instrument
cannot see"). This lane makes the walk score, through the game's own autoplay, and adds band
and per-player scoring state to the per-frame record.

## What changed

| commit | what |
|---|---|
| `f13c7cd8d` | `RB3_APP_AUTOPLAY=1` + band / per-player scoring fields in the det record; `rb3_app_det_cmp.py --base-fields` |
| `6be2bb044` | record: player enabled state and VocalPlayer track / singer / part counts; stars are asked for only once there is a score |
| `9ca2fa69c` | **fix**: `VocalPlayer::HookupTrack` takes retail's arm in rb3-app (0x826E63D8) |

- **Autoplay is retail's.** `RB3_APP_AUTOPLAY=1` sets `BandUser::mAutoplay` (the `autoplay`
  sync property, `BandUser.h` 0x7c) on every local band user at each frame start, and nothing
  else. Retail `GemPlayer::Start` (0x826BC460) reads that byte (`lwz r11,0x260(r31)`;
  `lbz r4,0x7c(r11)`) and passes it to `SetAutoplay` for a local player; `VocalPlayer::Start`
  does the same (`SetAutoplay(mUser->mAutoplay)`), and the singer then sings the chart pitch
  in `Singer::Poll_`. The flag is independent of `RB3_APP_DETERMINISTIC`.
- **Record.** With `RB3_APP_DETERMINISTIC=1`, a frame inside a Game appends
  `band acc A stars T mult M` + the band performer's `score hit miss streak stars crowd`,
  then per active player `p<i> track T en E auto 0|1 energy E deploy D long L`
  (+ `vtrack singers parts scoring` for a VocalPlayer) + the same performer fields. All of it
  is folded into `state` (and so into `chain`). Outside a Game the line is unchanged.
- The code lives in `native/src/rb3_app_determinism.cpp` (already classified in
  `tools/native_stub_census.py` `NON_STUB_RE`); `main_app.cpp` and `CMakeLists.txt` are
  untouched. Both flags unset: `ArmAutoplay` and the record return at their first test.
- `rb3_app_det_cmp.py --base-fields` compares only detwalk's fields (drops `state`/`chain`
  and the game suffix).

## Findings

### 1. The walk's player is a vocalist, and the vocalist never scored (fixed)

The native pad is a standard gamepad (`rb3_app_joypad.cpp`: XInput type 1 / subtype 1,
kJoypadAnalog), which RB3 treats as a vocals controller, so the a25 walk's two `a` presses on
part_difficulty_screen pick vocals (`track 3`). Prediction: autoplay alone gives a score.
**Wrong.** The first autoplay walk (bin-base `47c1e11519c9`) read `auto 1`, `en 0` (enabled)
and score 0, 0 phrases, crowd pinned at 0.8666 over the whole of thecon. The no-autoplay
control read the same: 0 misses, crowd 0.8666, i.e. no vocal scoring at all.

The VocalPlayer probe read `vtrack 0 singers 3 parts 1 scoring 1`: `mTrack` was null, and
`VocalPlayer::Poll` (`if (!mTrack || paused) { Player::Poll(); return; }`) took that return
every frame. Cause: `VocalPlayer::HookupTrack` was `#ifdef HX_NATIVE return;` ("the driver
installs a sentinel mTrack ... never called headless"), which is right for the headless vocal
harnesses but not for rb3-app, which has the BandUser track and a VocalTrack. Retail
0x826E63D8 is the `#else` arm: `lwz r3,0x88(mUser)` (mTrack) -> dynamic_cast (bl 0x8282A0C8)
-> `stw r3,0x30c(r31)`, then `mTrack->Restart(...)` (bl 0x82BA8910). The guard is now
`HX_NATIVE && !RB3_NATIVE_APP` (the TambourineManager pattern); the X360 arm is unchanged.

After the fix (bin-fix2 `66097bfe1b7a`): a 2500-frame walk reads `vtrack 1`, score 4400,
7 phrases, streak 7, 2.96 stars, crowd 1.0. Full walks: thecon 23,600 (31/31 phrases,
5.0 stars, crowd 1.0), then iwannabesedated 32,400 (40/40, 5.0). The no-input control now
scores as a no-input vocalist should: thecon 0 points, **31 misses**, crowd 0.20.

A/B (`tools/ab_measure.py --revert 9ca2fa69c`, log `~/tmp/scorewalk/ab_vp.log`): Δmatched +0,
Δmasked +0, Δhonest +0, Δcode% +0.000000pp, Δfuzzy +0, units at 100% 635 -> 635, rc 0
(predicted Δ0: only an HX_NATIVE arm changed).

### 2. The record's own star query tripped an assert (fixed in the instrument)

The first record called `GetNumStarsFloat` every game frame; `Scoring::GetNumStarsFloat`
asserts `!thresholds.empty()` (0x16C) before its zero-score return, and during the song load
the thresholds are still empty: 88 `FAIL ... Scoring.cpp Line: 364` per walk, 0 in detwalk's
runs, which make no such query. Now the record asks only for a non-zero score:
0 FAILs in every run below. Not a game bug: the game itself does not ask before the load ends.

### 3. Overdrive energy stays 0 (open, not fixed)

Over both autoplayed songs (71 phrases, all hit) `energy` reads 0.0000 and `deploy` 0 on every
frame. Either the charts' vocal parts carry no overdrive phrases on this path or vocal phrase
energy is not awarded natively. I did not chase it (budget). Next step: check whether thecon's
vocal chart has overdrive phrases (MIDI note 116 on PART VOCALS) in the native song data, then
follow the energy award from VocalPlayer's phrase scoring to `Player::AddEnergy`.

## Runs (90000-frame cap, SECS=540 TMO=580, a25/walk.txt, all `RB3_APP_DETERMINISTIC=1`)

Predictions: A2 = B2; P2 = A2 (perturb2 found the walk insensitive to malloc fill); S2 = A2
over its shorter prefix; N2 diverges once the song starts.

| run | binary | extra env | frames | songs (game_screen / endgame frame) | vs A2 |
|---|---|---|---|---|---|
| A2 | bin-fix2 `66097bfe1b7a` | AUTOPLAY | 60,325 | thecon 1119/8003, iwannabesedated 9156/14071 | - |
| B2 | bin-fix2 | AUTOPLAY | 61,052 | same | **IDENTICAL over 60,325 common frames** |
| P2 | bin-fix2 | AUTOPLAY, `GLIBC_TUNABLES=glibc.malloc.perturb=190` | 62,287 | same | **IDENTICAL over 60,325** |
| S2 | bin-asan2 `007a26bc72bc` (`-DRB3X_SANITIZE=address,undefined`) | AUTOPLAY, asanwalk's ASAN/UBSAN_OPTIONS | 21,267 | same | **IDENTICAL over 21,267** |
| N2 | bin-fix2 | (no autoplay) | 62,841 | thecon 1119/8003, yoshimibattles 8843/18069 | first divergence frame 1339 |

- The runs end at different frames only because the 540 s cap is wall-clock; compare the
  common prefix. Last common A2/B2/P2 line: `frame 60325 state 2164b5a2 chain d57fa9f226dcb90b`.
- A2 vs N2: frame 1339 is the first frame after `VocalPlayer::Start`, where `auto` reads 1
  against 0. On the detwalk fields alone (`--base-fields`) they first differ at frame 1680
  (`rand 145/248` vs `143/246`): with autoplay hitting phrases, something in the song draws
  the RNG twice more, so the second song also differs (iwannabesedated vs yoshimibattles).
  So the old fields would have caught autoplay only through the RNG, and a score change that
  draws no RNG would have been invisible to them; it now changes `state` the frame it lands.
- FAIL lines: 0 in A2, N2, P2, S2. ASan: 0 reports in S2.
- UBSan in S2: 36 unique sites (`~/tmp/scorewalk/S2.sites`), **none new in kind**. 29 match
  a site by file:line in the sanitize/asanwalk/asanwalk2 records. The other 7 are
  `BandFaceDeform.cpp:186-189` and `BandFaceDeform.h:15-16`, next to the recorded
  `BandFaceDeform.cpp:185` and in the same packed-`Delta` misalignment class
  (benign on x86, sanitize §3.4). The scoring-path sites (`Stats.h:330` and
  `AccomplishmentProgress.cpp:494` NaN-to-int, asanwalk2 M4: same result as retail) are the
  same as before, though vocal scoring now runs for real.

## Excluded areas (not fixed, as briefed)

Reached and seen, outside this lane's fix scope: `char/CharClip.cpp:187` (char, sanitize
row 13); `net/MatchmakingSettings.cpp:45,98` (net); bandobj render-side sites
(`BandHeadShaper`, `BandFaceDeform`, `StreakMeter.cpp:208`, `GemTrackDir.cpp:487`,
`BandTrack`) are the earlier records' verdicts and were not reopened. No Rnd/Draw/Text/
Spotlight or ui site was touched.

## Not done

- `tools/native_health.sh --selftest` not run, as the brief said. rb3-app and its ASan build
  link; no other target was built.
- No gem-instrument walk: the pad is a vocals controller. A guitar walk would need a pad
  subtype knob in `rb3_app_joypad.cpp` (XInput subtype 6) and a walk script for the guitar's
  calibration screens. `GemPlayer::HookupTrack` has no native skip, so that path should work.
- No ablation of the record's fields, and no sabotage run that changes only a score. The
  discriminating control is N2 against A2 and the pre-fix walks, whose vocal fields were
  pinned at zero.
- Not landed; nothing pushed.
