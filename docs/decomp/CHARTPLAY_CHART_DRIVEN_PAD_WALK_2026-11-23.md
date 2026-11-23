# CHARTPLAY: rb3-app plays guitar and drums from the chart through real pad input (2026-11-23)

Lane chartplay, branch `chartplay` (from main `62111353f`), worktree `~/tmp/wt-chartplay`.
Scratch, logs and binaries: `~/tmp/chartplay/`. The run scripts are committed in
`tools/rb3_app_walks/chartplay/` (`runb.sh` is deploywalk's with `A=`/`X=` changed; `walk.txt`
is gemwalk's copy of `a25/walk.txt`; `walk_x.txt` adds three DOWNs before the difficulty A;
`summ.py` prints p0's per-song hit/miss/streak/score/crowd from the det lines). Every run went
through `coord/no610m.sh` under the shared `gpu1.lock`, with `RB3_APP_DETERMINISTIC=1`.

## Goal

Every gem walk so far scored through `RB3_APP_AUTOPLAY` (BandUser::mAutoplay), which never
calls `Swing`: the hit windows, fret matching, strum logic and the watcher's miss paths were
never exercised by input. This lane adds a chart-driven pad and then looks for native bugs in
hit windows, sustains, HOPOs, chords, misses, streaks and the crowd meter.

## What changed

| commit | what |
|---|---|
| `9d2c79591` | `RB3_APP_CHARTPLAY` (new `native/src/rb3_app_chartplay.cpp`, hook in `rb3_app_joypad.cpp`) |
| `4c3f85896` | frets go up after the last gem (harness bug, below); `_LOG` diagnostics |
| `c89c2e75c` | **fix**: rb3-app takes the retail `NewTrackWatcherImpl` dispatch (`src/system/beatmatch/TrackWatcher.cpp`) |

- **`RB3_APP_CHARTPLAY=1`** (pad 0). Each poll, before `JoypadPollCommon`, the driver finds the
  local GemPlayer on pad 0, reads the gem list its watcher scores against
  (`TrackWatcherImpl::mGemList`, the picked difficulty) and the BeatMatcher clock
  (`BeatMatcher::GetNow()`, the ms every Swing is judged at), and returns button *levels*:
  guitar: the gem's frets one poll before its strum, the strum (the controller's own
  `mStrumBarButtons`, alternating so two strums on consecutive polls are two presses) on the
  poll whose clock is nearest the gem; frets held through a sustain. Drums: each slot pressed on
  its poll and released on the next. Buttons come from the controller's `SlotToButton`, so
  joypad.dta's mapping is the game's. The levels are ORed into pad 0's state; everything from
  `ReadSingleXinputJoypad` on is retail code. Nothing in the game is told to hit anything.
- Knobs: `_OFFSET_MS=f` (press f ms late; negative early), `_MISS=p` (skip p% of gems, picked by
  an FNV hash of the gem index and `_SEED`; the game's RNG is not touched), `_MISS_FROM=n`,
  `_HOPO=tap` (play a HOPO gem by the fret change alone, no strum), `_SUSTAIN=f` (hold a sustain
  for fraction f), `_LOG=1` (one line per gem plus what JoypadData / the controller made of each
  pressing poll). Keys and pro guitar are not driven (the driver returns 0 for their controllers).
- Per song: `rb3-app-chartplay: song done: gems N acted A skipped S late L (max …) same-pad
  collisions C`.
- `MILO_INPUT_LOG` still logs only the scripted presses (the chart's levels would be one line
  per poll).

## Findings

### 1. No drum hit could ever count in rb3-app (fixed: retail TrackWatcher dispatch)

Prediction: perfect drum timing matches drum autoplay. **Wrong.** k1DC (bin-c1): 0 hits,
133 misses by frame 2850, crowd 0.20, while k1DA (autoplay) had 109/109. The `_LOG` lines
(k4DC, bin-c4) showed the press arrived: JoypadData type 8 buttons 0x40 (green, slot 4),
`JoypadController::mFretMask` 0x10, so `OnMsg(ButtonDownMsg)` ran its body and called
`BeatMatcher::Swing` — but `BeatMatcher::mLastSwing` stayed 0, so the watcher's Swing returned
false. Cause: `NewTrackWatcherImpl` (`TrackWatcher.cpp`) had an `#ifdef HX_NATIVE` arm that
built a `GuitarTrackWatcherImpl` for every part ("the other impls are not compiled natively").
`BaseGuitarTrackWatcherImpl::Swing` returns false for a non-strum swing outside a solo, so a
drum part was unplayable from input. Retail 0x8279D990 (`tools/va_disasm.py 8279d990 840`,
a 100% row) constructs GuitarTrackWatcherImpl 0x827A0FC8, DrumFillTrackWatcherImpl 0x827A0C70,
JoypadTrackWatcherImpl 0x827A0668, KeyboardTrackWatcherImpl 0x827A04F8 and
RealGuitarTrackWatcherImpl 0x8279F0E0 by controller and track type. rb3-app compiles all five
(`RB3_APP_SOURCES`; they were dropped by `--gc-sections`), so the arm is now
`HX_NATIVE && !RB3_NATIVE_APP`, the pattern of Player.cpp's rb3-app arms. Autoplay never showed
this because autoplay captures gems without calling Swing. The same arm also gave keys, pro
keys and pro guitar the guitar impl; those parts were not walked here.

After the fix (bin-c5), drums from input score: easy `livinginamerica` k5DC 412 hits, 0 misses,
5.0 stars, all 498 gems pressed (0 late, 0 same-pad collisions); expert k6XDC 1,145 hits,
0 misses, all 1,424 gems pressed. **Chartplay counts more hits than autoplay** (k5DA 387,
k6XDA 1,073) and scores more (48,175 vs 37,275; 139,475 vs 105,875). This is not a scoring bug,
as far as measured: chartplay deploys overdrive through the drum fills (970 frames with
`deploy 1` in each DC run, the first at frame 3673; 0 in either DA run, since retail autoplay
cannot deploy, deploywalk finding 1). Gems under a fill section are not scored, and a deploy
removes the next fills. Check: every press poll whose hit count did not rise (k6XDC 181,
k5DC 71) came at energy >= 0.5 and not deploying, which is the state in which fills show; no
uncounted press came while deploying or below 0.5 energy. Not checked gem by gem against the
fill spans.

### 2. Guitar: perfect timing matches autoplay exactly

Easy `thelook` (140 gems, no HOPOs, no chords): k5GC = k5GA exactly: 140 hits, 0 misses,
longest streak 140, score 13,295, 5.0 stars, crowd 1.0. Expert (`walk_x.txt`, 814 gems: 75
HOPO, 497 chords, the closest pair 139.8 ms apart): k6XGC = k6XGA exactly: 814/0, score
146,907, 5.59 stars. Every strum landed within +/-16.6 ms of its gem (frame quantisation at
30 polls/s), 0 late. Both match the prediction.

### 3. Hit window

Offsets on easy guitar (strum and frets shifted together): -80 ms (k5GN) hits all 140
(score 13,289, 6 below perfect: the timing part of the score); +150 ms (k5GW) hits none
(0/140, crowd at 0.20). +80 ms (k5GP) hits 109, with 58 misses (late strums also count as
overstrums once their gem has passed). Per strum (outcome read off the det line two frames
later): k5GP hits at +63.4..+89.9 ms and misses at +90.3..+96.6 ms; k5GN hits at -96.5..-63.4
ms. So the window is asymmetric, closing at +90 ms late and open past -96.5 ms early. That is
`TrackWatcherImpl::InSlopWindow` (`|now + mSyncOffset - gem| <= mSlop`, a 100% row) with
mSlop 100 and mSyncOffset +10 ms; the walk's manual calibration presses `right` once on each
cal screen, so a +10 ms offset from that is expected. I did not log mSyncOffset itself (the
A/B held the worktree when I would have rebuilt), so the +10 is inferred from the two edges,
not read.

### 4. Misses, streaks and the crowd

`_MISS=10` (no press at all for about 10% of gems). Guitar k5GM: 14 gems skipped, 14
misses exactly, 126 hits; longest streak 17; score 3,980 (perfect: 13,295, because the
multiplier resets); crowd minimum 0.8458 (perfect run: 0.8666 at song start, then 1.0) and
0.9827 at song end. Drums k5DM: 53 skipped, 47 misses, 400 hits, longest streak 33, score
15,675, crowd minimum 0.8625. The 6 skipped drum gems with no miss are consistent with gems
under fill sections (see 1), which are not scored either way. Streaks break and the crowd
drops on misses, as predicted; no native-only code is on this path (GemPlayer's miss,
Penalize and UpdateCrowdMeter are matched rows).

### 5. HOPOs, chords and sustains

- HOPOs: expert guitar played with `_HOPO=tap` (k6XGH, 75 HOPOs played by the fret change
  alone, no strum) scores exactly the strummed run's 814/0 and 146,907. Strumming a HOPO one
  poll after its fret change (k6XGC) is also forgiven.
- Chords: 497 expert chords, all hit (frets pressed together one poll before the strum).
- Sustains: `_SUSTAIN=0.5` (k5GS) keeps 140/140 hits but scores 12,937 (358 below a full hold).
  The crowd drops from 1.0 to 0.9916 at frame 6909, when the last gem's 8.7 s sustain is
  released halfway. Source: a sustained hit defers `UpdateCrowdMeter` to the sustain's end with
  the held fraction as its note score (GemPlayer.cpp ~545), so a 0.5 score below the crowd's
  raw value lowers it. This is game design, on matched rows; not a native bug.
- No native bug found in hit windows, HOPOs, chords, sustains, misses, streaks or the crowd
  beyond finding 1. Guitar, drums and keys watchers are 100% rows in `report.json` except
  `ClosestUnplayedGem` (99.14), `GetNextRoll` (99.23) and `DrumTrackWatcherImpl::CheckCymbal`
  (99.55). The two I checked against retail (`va_disasm.py 82796588 140`, `82780240 88`) are
  the same logic: ClosestUnplayedGem calls `GameGem::PlayableBy(mPlayerSlot)` 0x8278E960,
  which our `Playable(idx)` wraps; CheckCymbal's residual is one AND's operand order.

### 6. Harness bug found on the way (fixed in the driver)

k1GC finished song 1 and then sat on `coop_endgame_screen` to the frame cap: the driver held
the last gem's fret (green = kPad_X = Confirm) after the song, so the walk's scripted A presses
never produced a new ButtonDown. Frets now go up after the last gem's end.

## Runs

All `RB3_APP_DETERMINISTIC=1`, `walk.txt` (easy) or `walk_x.txt` (expert), through no610m.sh.
Binaries in `~/tmp/chartplay/`: bin-c1 `86da84df5751` (k1, before the fix), bin-c4 (k4,
diagnostic), bin-c5 `d622f359ffed` (k5, k6: the fix and the driver as committed except the
docs). Song 1 only (9,000 / 7,600-frame caps, both past song 1's end at frame 7,322).

| run | pad | mode | hits | misses | longest | score | stars | crowd end (min) |
|---|---|---|---|---|---|---|---|---|
| k1GA / k1GC | guitar easy | autoplay / chartplay | 140 / 140 | 0 / 0 | 140 / 140 | 13,295 / 13,295 | 5.0 | 1.0 |
| k1DC | drums easy | chartplay, bin-c1 | 0 (by fr 2850) | 133 | 0 | 0 | 0 | 0.20 |
| k5GA | guitar easy | autoplay | 140 | 0 | 140 | 13,295 | 5.0 | 1.0 (0.8666) |
| k5GC | guitar easy | chartplay | 140 | 0 | 140 | 13,295 | 5.0 | 1.0 (0.8666) |
| k5GH | guitar easy | chartplay, HOPO tap | 140 | 0 | 140 | 13,295 | 5.0 | 1.0 |
| k5GS | guitar easy | chartplay, sustain 0.5 | 140 | 0 | 140 | 12,937 | 5.0 | 0.9916 |
| k5GN | guitar easy | offset -80 | 140 | 0 | 140 | 13,289 | 5.0 | 1.0 |
| k5GP | guitar easy | offset +80 | 109 | 58 | 11 | 3,005 | 3.10 | 0.5738 (0.4792) |
| k5GW | guitar easy | offset +150 | 0 | 140 | 0 | 0 | 0 | 0.20 |
| k5GM | guitar easy | miss 10% (14 skipped) | 126 | 14 | 17 | 3,980 | 3.37 | 0.9827 (0.8458) |
| k5DA | drums easy | autoplay | 387 | 0 | 387 | 37,275 | 5.0 | 1.0 |
| k5DC | drums easy | chartplay | 412 | 0 | 412 | 48,175 | 5.0 | 1.0 |
| k5DM | drums easy | miss 10% (53 skipped) | 400 | 47 | 33 | 15,675 | 3.50 | 1.0 (0.8625) |
| k6XGA | guitar expert | autoplay | 814 | 0 | 814 | 146,907 | 5.59 | 1.0 |
| k6XGC | guitar expert | chartplay | 814 | 0 | 814 | 146,907 | 5.59 | 1.0 |
| k6XGH | guitar expert | chartplay, HOPO tap | 814 | 0 | 814 | 146,907 | 5.59 | 1.0 |
| k6XDA | drums expert | autoplay | 1,073 | 0 | 1,073 | 105,875 | 5.00 | 1.0 |
| k6XDC | drums expert | chartplay | 1,145 | 0 | 1,145 | 139,475 | 5.70 | 1.0 |

All 16 k5/k6 runs: 0 `FAIL` lines, 0 `UNREACHED` stubs, rc 0 at the frame cap. k5 runs went on into
song 2 (the endgame fix works); song 2 differs between legs (different scores change the RNG
draws, so the next song differs) and is not compared.

Price of the src/ change: `tools/ab_measure.py --worktree ~/tmp/wt-chartplay --revert c89c2e75c`
(objdiff-cli sha256 `ec3badc0f333`): Δmatched +0, Δmasked_equal +0, Δhonest +0, Δmatched_code
+0 B on the shipped `name_check` ruler and on the `none` control (matched 55,322 both legs),
units at 100% 635 -> 635. Expected: the guard is HX_NATIVE-only, so the match build compiles
the same `#else`. Note: the run's restore step deleted this lane's untracked
`tools/rb3_app_walks/chartplay/` scripts (backed up under `.ab_measure_runs/`, copied back).

## Not done

- `tools/native_health.sh --selftest` not run (brief: left to the coordinator).
- Keys / pro keys / pro guitar are not driven by chartplay; the dispatch fix gives them their
  retail watcher impls, unwalked.
- `_LOG=2` switches on `TheBeatMatchOutput`, but its LogFile wrote no `beatmatch-*.rec` natively
  (k3DC); not chased.
- Not landed; nothing pushed.
