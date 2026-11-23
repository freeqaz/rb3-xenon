# CODAWALK: solos, big rock endings, unisons, fills, whammy, tilt and the multiplier in rb3-app (2026-11-23)

Lane codawalk, branch `codawalk` (from main `068db0471`), worktree `~/tmp/wt-codawalk`. Scratch,
logs and binaries: `~/tmp/codawalk/`. Scripts are committed in `tools/rb3_app_walks/codawalk/`
(copy them into `~/tmp/codawalk/`, which they `cd` into; `runb.sh`, `build.sh`, `walk*.txt` are
chartplay's with the paths changed, `walk_od.txt`/`p*_od.txt` are deploywalk's band walk).
Every run: `RB3_APP_DETERMINISTIC=1`, through `coord/no610m.sh` under the shared `gpu1.lock`.
`ana.py run.log [pN]` prints each frame where a player's bonus / multiplier / energy state changes.

## Goal

Walk the in-song scoring features no lane had checked (solos and their bonus, big rock endings
hit and missed, unison bonuses, drum fills as the OD activation point, whammy and tilt OD gain,
the multiplier with and without OD) through real pad input, judge each against retail's code and
the TU5 config, and fix native bugs.

## What changed

| commit | what |
|---|---|
| `c0d8736a5` | `RB3_APP_DET_SCORE`; chartplay `_WHAMMY`, `_TILT`, `_CODA`; **fix**: a stickless native guitar pad rests its whammy at full negative `sThumbRX` |
| (this commit) | tilt held 15 polls and on the right axis sign; record |

- **`RB3_APP_DET_SCORE=1`** adds to the det line, per gem player: `solo a/b/c/d` (GemPlayer
  solo start tick `unk310`, in-solo `unk315`, solo-data-read `unk316`, disabled-in-solo `unk314`),
  `coda in/finished/blown`, `pts` (GemPlayer::mCodaPoints), `ssolo`/`spct`/`scoda` (Stats solo
  bonus, solo %, coda points), `whm w/v` (whammy-active `unk348`, `GetWhammyBar()*-100`); per band
  `bcoda a/b` (Band `unk40` coda checked, `unk44` 1 won / 2 blown); and a
  `rb3-app-det: song coda tick N` line when the song's coda start changes. Off, the line is
  unchanged.
- **Chartplay (guitar/bass)**: `RB3_APP_CHARTPLAY_WHAMMY=od|all` moves the whammy full travel
  every other poll while a held sustain is in an OD phrase (`od`) or any sustain;
  `RB3_APP_CHARTPLAY_TILT=1` tilts (sThumbRY full positive) for 15 polls when energy >= 0.5 and not
  deploying, at most once a second; `RB3_APP_CHARTPLAY_CODA=mash|blow` (`mash`: skip the chart's
  gems inside the coda freestyle and hold all frets, strumming every third poll; drums one pad a
  poll in rotation; `blow`: skip every gem after the freestyle). The axes reach the game through
  `GetXinputSinceLastFrame` (new `RB3AppChartPlayAxes`); from `ReadSingleXinputJoypad` on it is
  retail code.

## TU5 data used as the judge

Decoded with `tools/rb3_app_walks/trainwalk/dtb.py` from
`rb3/orig-assets/extracted-xbox-full`: `config/gen/scoring.dtb` (`solo` awards default
`(60 5) (70 10) (80 20) (90 30) (95 50) (100 100)` points per solo note, `coda (point_rate 100)
(max_mash_ms 1500)`, `unison_phrase (reward 2) (penalty 2) (point_bonus 1000)`,
`band_energy spotlight_phrase 0.251`, `overdrive whammy_rate 0.034 whammy_speed 0.05`,
`bonuses multiplier (1 2 4 6 8)`, `streaks singleplayer (0 1) (10 2) (20 3) (30 4)`),
`config/gen/beatmatch.dtb` (`mercury_switch_filter (any_sign_filter (window 0.5)
(num_frames 12) (threshold 0.93))`), system `joypad.dtb` (`strat_xbox`:
`TRADITIONAL_WHAMMY_VAL`, `NEEDS_XBOX_MERCURY_SWITCH_ADJUSTMENT`) and `beatmatch_controller.dtb`
(`ro_guitar_xbox`: `controller_style hx_xbox`). Only 4 of the 83 disc songs carry a `[coda]`
text event (freebird, ifeelgoodalt, kinggeorge, ohmygod); `RB3_APP_SEED=18` loads ohmygod.

## Findings

### 1. A native guitar pad held its whammy half down all song (fixed, native joypad)

Prediction: with no whammy input `GetWhammyBar()` reads 0. **Wrong.** Every expert guitar leg on
bin-k1 read `whm 0/50`, i.e. -0.5, from song start (c1GC: 6,801 game frames at -0.5). The
`strat_xbox` entry reads the whammy as `traditional_whammy_val`, `-(RX + 1) / 2`
(GuitarController::GetWhammyBar), so it rests at sThumbRX = -32768; `GetXinputSinceLastFrame`
gave a stickless native pad RX 0, which is half travel. Effect: the sustain pitch bend sat at
half and `send_whammy -0.5` went out; no OD (`FilteredWhammyBar` gains only on whammy *speed*).
Fix (`rb3_app_joypad.cpp`): a stickless pad whose subtype is guitar (6) reports RX -32768.
After (c2GC, bin-k3): `whm 0/0` on every game frame, and the gameplay is identical (score
213,841, 1,019 hits, 0 misses, coda 2,749 both legs; the det hash differs only because it now
carries the whammy value).

### 2. Solos: the bonus is retail's (clean)

crazytrain (seed 3), expert guitar by chartplay (k1s3): one 229-gem solo; `solo` goes
`184320/1/0/0` -> `/1/1/0` at its last gem -> `-1/0/0/0` with `ssolo 22900 spct 100` and the score
jumps 118,613 -> 141,597 (22,900 bonus + 84 note points). 22,900 = 229 notes x 100, the default
`perfect_solo` award per note (`LocalSoloEnd` 100% row: `GetSoloAward` points x numGems,
`AddBonusPoints`, unmultiplied). Not walked: a partial solo (lower award rows), keys and drums
solos.

### 3. Big rock endings: hit, blown, autoplayed and mashed (clean)

ohmygod (seed 18), expert. Coda tick 253,440; freestyle 186,377..194,545 ms; 36 chart gems inside
(marked 0x80 by `SongDB::DisableCodaGems`, not counted as hits), 1 after.

| leg | input | coda pts | result | score after |
|---|---|---|---|---|
| c1GC | chart gems through the freestyle | 2,749 | `coda 1/1/0`, `bcoda 1/1` (won) | 211,092 -> 213,841 |
| c1GM | `_CODA=mash` (all frets held, strum every 3rd poll) | 922 | won | 212,014 |
| c1GA | autoplay (TrackWatcherImpl cheat coda swings) | 921 | won | 212,013 |
| c1GB | `_CODA=blow` (last gem skipped) | 0 | `coda 1/0/1`, `bcoda 1/2`, miss 1 | 211,092 |
| c1DM | drums, `_CODA=mash` (one pad a poll) | 4,654 | won | 155,875 -> 160,529 |

Against retail: points come from `FillInProgress` (0x826C2988, 100%): the first swing on a
lane scores `point_rate x min(max_mash_ms, ms - lastSwing) / 1000` = 100 x 1500 / 1000 = 150
(seen); after that 9 per lane per 100 ms swing (100 x 99.99 / 1000 truncated, the virtual clock's
3-frame step). Retail computes it as `fmuls` by a 0.001 constant (`va_disasm 826c2988 400`), our
native build divides by 1000; the two can only differ when the product is within one ulp of an
integer, not seen here. Guitar keeps a timer per lane (`RequireAllCodaLanes`), so changing frets
(c1GC) scores three times holding all five (c1GM, which equals autoplay). Winning adds the points
unmultiplied (`AddCodaPoints` -> `AddPoints(pts, false, false)`); blowing resets them. All on
100% rows.

### 4. Unison phrases (clean)

Band walk (deploywalk's `walk_od.txt` + `p1..p3_od.txt`: vocals, guitar, drums, guitar;
autoplay + autodeploy). c2UA, p1: every unison (`unison in 1`) ends with `uni` +1 and energy
+0.502 = 2 x `spotlight_phrase` 0.251 (`unison_phrase reward 2`). c2US
(`RB3_APP_AUTOPLAY_SKIP=3`, pad 3 misses everything): p1 passes 7 unisons, `uni` stays 0/0 and
no unison energy is awarded to anyone, as `CommonPhraseCapturer` requires every track. Open:
`unison_phrase point_bonus 1000` is read into `PlayerParams::mPointBonus` (+0x24) but nothing in
our source reads that field, and no 1,000-point step shows at a unison; whether retail reads it
was not checked.

### 5. Drum fills as the OD activation point (clean)

ohmygod drums by chartplay (c1DM): 3 deploys, each at a fill's end with energy >= 0.5
(0.5020 -> deploying at 0.4991; 0.9970; 0.5020), never below 0.5; while deploying the player's
multiplier is 8 (4 x 2) and the band's `bmult 2/2 bonus 1/4`. Deploy 1 drained 0.5 energy in 169
frames (5.6 s = 16 beats at ~170 bpm; `deploy_beats 32` per full bar). The fills only appear at
>= 0.5 energy and not deploying (chartplay finding 1); same here.

### 6. Whammy OD gain (clean)

`_WHAMMY=od` (c2GW): the whammy turns `whm 1/...` (GemPlayer `unk348`) only while an OD-phrase
sustain is held, and energy rises during it (0.2542 -> 0.2732 over 6 frames: `whammy_rate`
0.034 per beat at ~170 bpm). `_WHAMMY=all` (c3GA): active on 23 frames, all inside OD
phrases; outside them `FilteredWhammyBar` does nothing (`GetPhraseID == -1`). Score and hits at
frame 4,300 equal the no-whammy leg (97,589, 519/0). During a deploy, whammy extends it (c3GW
deploy 1 ran to frame 2,495 against c3GT's 2,488, energy 0.7723 vs 0.7501 at the deploy).

### 7. Tilt deploys (clean; two harness errors on the way)

Prediction: sThumbRY full negative tilts. **Wrong twice**, both harness: (a) `ReadSingleXinputJoypad`
negates RY (`TranslateStick(..., true, ...)`) and `GuitarController::Poll`'s
`xbox_mercury_switch` passes `-GetRY()`, so tilt is RY *positive*; (b) the TU5
`any_sign_filter` needs 12 consecutive polls >= 0.93, and my first hold was 6 (c2GT: 167 tilts,
0 deploys). Held 15 polls on +RY (c3GT): the first tilt starts at poll 2,222 and the deploy lands
at frame 2,234, 12 frames later, exactly the filter's count; multiplier 8 while deploying.
Note: the filter counts frames, so natively at 30 polls/s a tilt must be held 0.4 s where a
60 Hz loop needs 0.2 s. That is the frame rate, not this code.

### 8. The multiplier with and without OD (clean)

Single player (every guitar leg): 1 -> 2 -> 3 -> 4 at streaks 10/20/30 (`singleplayer`), reset to
1 on a miss, 8 while deploying (`mult 8 (4,1,2)`, `bmult 2/2`). Band (c2UA): the band multiplier
follows `bonuses multiplier (1 2 4 6 8)` by the number deploying (`bmult 6/6 bonus 3/4` with 3),
and a player's multiplier is individual x band/own x own, `Player::GetMultiplier` (100% row):
p1 deploying with two others 24 = 4 x 3 x 2; p1 not deploying while two others do 16 = 4 x 4 x 1.

## Open

- crazytrain expert guitar by chartplay hits all 1,376 gems but counts 4 misses (k1s3; the first
  at frame 2,058: gem 16, a pull-off HOPO, frets changed one poll before the strum). thelook's 75
  HOPOs had none (chartplay record). Not chased: the watcher rows are 100% and I had no per-swing
  trace (`_LOG=2` writes nothing natively, chartplay record). Could be the harness's
  one-poll-early fret change.
- c1DM: 1 miss when the drum mash starts half a poll before the freestyle (harness timing).
- `point_bonus` (finding 4).

## Not done

- No `src/` change, so no `ab_measure` run (all edits are `native/src/rb3_app_*.cpp`, outside the
  match build). No native/src file added, so `native_stub_census.py` is untouched.
- `tools/native_health.sh --selftest` left to the coordinator (brief).
- Keys solos, partial solos, a blown coda on drums, coda with a full band, bass 6x were not walked.

## Runs

Binaries in `~/tmp/codawalk/`: bin-k1 (knobs, before the whammy fix), bin-k3 `15fc0b4f5f02`
(whammy fix, tilt sign), bin-k4 `363880f5bc0c` (tilt hold 15, as committed). All legs rc 0 at the
frame cap; 0 `UNREACHED`/`FAIL` lines in the k1 survey. Legs: `survey.sh` (k1, seeds 1 2 3 5 6:
llama, beastandtheharlot, crazytrain, thelook, midlifecrisis; none has a coda), `seeds.sh`
(seeds 7-18 -> song names), `coda.sh` (c1), `legs2.sh` (c2), `legs3.sh` (c3).
