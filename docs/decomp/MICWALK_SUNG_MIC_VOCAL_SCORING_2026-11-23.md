# MICWALK: rb3-app scores vocals through the real mic path (2026-11-23)

Lane micwalk, branch `micwalk`, worktree `~/tmp/wt-micwalk`, base main `62111353f`.
Scratch, logs and binaries: `~/tmp/micwalk/` (`runb.sh` is a copy of `a25/runb.sh` with
`A=~/tmp/micwalk`, `X=~/tmp/wt-micwalk`; every run goes through `coord/no610m.sh` under the
shared `gpu1.lock`; walk `rb3-scout/a25/walk.txt`, 8,400 frames, `SECS=200 TMO=240`,
`RB3_APP_DETERMINISTIC=1` on every run). Summaries: `~/tmp/micwalk/scripts/summ.py`
(p0's score at each song's end, plus sung-vs-detected pitch stats) and `scripts/micstats.py`.

## Goal

SCOREWALK and VOCALOD made the vocalist score through `RB3_APP_AUTOPLAY`, which replaces the
mic pitch in `Singer::Poll_` with the chart's (`VocalNoteList::PitchAt`). The mic path
(device -> GameMic -> PitchDetector with the VMX `ShiftedDotProduct` from STUBRING ->
Singer -> VocalPart) never ran: rb3-app's Synth had no mic. This lane feeds it a synthesised
singer and looks for native bugs in pitch detection and vocal scoring.

## What changed

| commit | what |
|---|---|
| `959437816` | `RB3_APP_SUNG_MIC=1`: a synthetic mic that sings the chart (native only) |
| `98f5fdbbc` | **fix**: `TambourineManager::HandleButtonDown` adds the profile sync offset in rb3-app (retail 0x826FB650) |

- **The knob.** `native/src/rb3_app_mic_native.cpp` (new; classified in
  `tools/native_stub_census.py` `NON_STUB_RE`). `Rb3AppNativeSynth`
  (`rb3_app_native_glue.cpp`) overrides the base Synth's mic virtuals (`GetMic`,
  `GetNumConnectedMics`, `GetNextAvailableMicID`, `IsMicConnected`, `Capture/ReleaseMic`,
  `DidMicsChange`/`ResetMicsChanged`) to report one connected mic, mic 0, while the knob is
  set; unset, they return the base answers (no mic), so default walks are unchanged.
  `MicClientMapper::HandleMicsChanged` (first `Synth::Poll`, and `GameMicManager::Init`) maps
  it to player 0 and `GameMicManager::CreateMic(0)` builds the GameMic; Singer 0 finds it
  through `MicClientID(0, -1)`. Nothing else in the game is touched.
- **The device** is modelled on retail `MicXbox` (`synth_xbox/Mic.cpp`): 16 kHz,
  `GetRecentBuf` returns the latest 0x600 samples, sensitivity and gain 1.0 (its ctor values),
  `GetType` 2 while connected (retail `MicXbox::GetType`). The signal is a harmonic tone
  (partials 1, 1/2, 1/3, 1/4; fundamental amplitude 4000) at
  `VocalNoteList::PitchAt(vp->GetCompensatedTime(songMs)) + detune` of the local VocalPlayer's
  part 0, silence between notes. Pitch and timing are the song MIDI's notes **as the game
  parsed them**; an independent MIDI parse was not written, so a note-parse bug would be sung
  faithfully and not caught here. The signal is a pure function of song time, so det walks
  stay reproducible (measured below).
- `RB3_APP_SUNG_MIC_DETUNE=<semitones>` sings every note off; `RB3_APP_SUNG_MIC_LOG=1` prints
  one `rb3-app-mic: ms … sung … det … energy …` line per analysed window (the GameMic's
  `mPitch` after the previous window, against the pitch sung at that window's end).

## Results (first song of the walk, vocals on Easy, 39 phrases)

Predictions before the runs: on-pitch sung = autoplay or slightly under (detector onset lag);
+3 semitones clearly lower. Difficulty: perfect 30,000 = 200 x (1+2+3+36x4), i.e.
`phrase_value` 200 = Easy, `pitch_margin` 3.8 st, so `VocalPart::ScoreNote`'s per-frame
credit `exp(-d^2 ln10 / 3.8^2)` is 0.85 / 0.53 / 0.24 at 1 / 2 / 3 semitones.

| run | binary | env | score | hit / miss | stars | crowd |
|---|---|---|---|---|---|---|
| A | bin-s1 `2259d8cdcf04` | AUTOPLAY | 30,000 | 39 / 0 | 5.00 | 1.00 |
| S0 | bin-s1 | SUNG_MIC | **30,000** | 39 / 0 | 5.00 | 1.00 |
| D1 | bin-s2 (branch tip) | SUNG_MIC, DETUNE=1 | 30,000 | 39 / 0 | 5.00 | 1.00 |
| D2 | bin-s2 | SUNG_MIC, DETUNE=2 | 23,783 | 33 / 6 | 5.00 | 1.00 |
| S3 | bin-s1 | SUNG_MIC, DETUNE=3 | **6,315** | 5 / 34 | 3.08 | 0.38 |
| S0b | bin-s2 | SUNG_MIC | = S0 | | | |

- **Sung on pitch scores exactly like autoplay**, and the phrase hits land on the same frames
  with the same running scores (`A.hits` vs `S0.hits`, 40 score changes each, `diff` empty).
  The first prediction (slightly under) was wrong: Easy's 180 ms slop and 3.8 st margin
  absorb the detector's onset lag.
- **Off pitch scores lower, monotonically**: 0 / 1 st both perfect (Easy's margin), 2 st
  loses 6 phrases, 3 st loses 34. S3's 3.08 stars for 21% of the perfect score is RB3's vocal
  cutoffs (`star_thresholds.dta` vocals 0.21 0.46 0.77 1.85 3.08 x base, missed phrases still
  earn partial credit), not a bug I can show; no retail run was made to confirm it.
- **Determinism**: S0b (branch tip) vs S0 (`tools/rb3_app_det_cmp.py`): IDENTICAL over 8,400
  common frames, so the sung signal adds no run-to-run variation (and the tambourine fix does
  not reach this walk).
- **The detector tracks the synthetic singer.** S0: 2,662 analysed windows, 2,540 with a note
  sung; 2,305 detected. |det - sung| median 0.016 st, 92.7% within 0.5 st, **no octave
  errors** (octave-folding the error changes nothing). Every error > 0.5 st above MIDI 66
  (77 windows) sits at a note change (the 32 ms analysis frame straddles two notes); of 235
  sung-but-silent windows, 185 are at an onset or pitch change and 50 mid-note (where the
  window may straddle a gap between two same-pitch notes; not split further). S3 tracks the
  detuned signal to the same accuracy (mean signed error +0.012 st).

## Retail comparison (tools/va_disasm.py on clean TU5)

Checked by hand against retail bytes, all **matching** our source:

- `PitchDetector::AnalyzeBlock` 0x82B80810: overlap shift of the decimation and cumulative
  energy buffers, the one-sample filter prime, the 0.3 one-pole smoother and decimation test,
  `mIdx` carry, `mAveEnergy = sqrt(corr[N-1]) / N`, the call order and arguments of
  ShiftedDotProduct / FindCCPeak / RefinePeriod2, the adaptive floor (`unk14 > 60`, ratio,
  gate, IIR towards the top), the clamps, the 30 cap, `pitchHz = (rate/decim)/period`, the
  `log10 * 39.863136 - 36.376316` MIDI conversion, and the outputs (confidence =
  `fixedGain/floor * gain * energy` to the 8th argument). The Xbox ABI puts the two float
  arguments in GPR slots too, so `pitchOut`/`confidenceOut` are r9/r10 and `gateOut` is on the
  stack; our signature agrees.
- `GameMic::ThreadProcessOneFrame` 0x826CCE80: the Mic vtable slots it calls (0x70
  GetRecentBuf, 0x74 GetContinuousBuf, 0x78 GetDroppedSamples, 0x40 GetSensitivity, 0x8c
  GetName), the 0x2000 caps, AnalyzeBlock's arguments (sensitivity as gain, `Mic::unk8` as the
  hint), the `energy / (clamp * 500)` clamp and the asymmetric smoothing.
- `VocalPart::ScoreNote` 0x826F2D00: octave folding below 2.5 st (constants 2.5, 1/12, 0.5,
  12.0 read from .rdata 0x8202A570, 0x820DE684, 0x82075090, 0x82029108/0x82029348), the
  margin test, `exp(-d^2/sigma)`, the 0.01 floor (0x82017E70) and the slice-weight zero.
- `IIR4PoleFilter::FilterSlow` is a transposed direct form II over retail's coefficients; the
  VMX fast path was verified by STUBRING (2.38e-6 against the scalar arm).

## Finding: the tambourine button swung uncompensated in rb3-app (fixed)

A sweep of the bare `#ifdef HX_NATIVE` / `#ifndef HX_NATIVE` arms on the vocal path (VocalPlayer,
Singer, VocalPart, GameMic, GameMicManager, PitchDetector, VoiceBeat, TambourineManager) for
ones rb3-app still takes found them to be overlay/spew, null-user fallbacks rb3-app never hits,
or retail-uninitialised fields zeroed, except one: `TambourineManager::HandleButtonDown`
used `float offset = 0.0f` ("headless: no BandUser pad / TheProfileMgr") in every native
build. Retail 0x826FB650 takes the local user's pad (via `GetUser()` / `GetLocalBandUser()`
vcalls) and calls `ProfileMgr::GetSyncOffset` (bl 0x825464F0) before
`TaskMgr::Seconds(kRealTime)` (bl 0x82747920, r4 = 1). rb3-app links ProfileMgr and has the
BandUser, and `VocalPlayer::GetCompensatedTime` already takes retail's arm there, so a pad-X
tambourine hit was timed against a different clock than the mic. The gate is now
`!HX_NATIVE || RB3_NATIVE_APP` (the TambourineManager pattern); the headless vocal harnesses
keep the zero offset. **Not measured in a walk**: the a25 walk presses no tambourine button,
and I did not log what `GetSyncOffset` returns for the walk's profile, so the size of the
timing shift is unknown (it is zero if the profile has no calibration).

## A/B

`tools/ab_measure.py --worktree ~/tmp/wt-micwalk --revert 98f5fdbbc` (log
`~/tmp/micwalk/ab_tamb.log`, result
`.ab_measure_runs/20261123-124531-98f5fdbbc-403008/result.json`; predicted Δ0, since only a
native arm changed): Δmatched +0, Δmasked +0, Δhonest +0, Δcode% +0.000000pp, Δfuzzy +0,
units at 100% 635 -> 635, rc 0 (ruler `name_check`, objdiff sha256 `ec3badc0f33323e9`).
The mic commit touches no `src/` file and was not priced.

## No bug found in the mic pitch path

Against the brief's expectation, I found no native bug in pitch detection or pitch scoring:
the retail comparisons above match, and the end-to-end numbers agree with autoplay exactly.
This lane's walk cannot see the following, so treat them as open:

- Medium/Hard/Expert (smaller slop and margin) would show detector lag or tracking error
  that Easy's tolerance absorbs. A walk script that picks Expert vocals is the next run.
- Unpitched (talky) notes go through `TalkyMatcher`/`VoiceBeat` on the continuous buffer, which
  a sine voice exercises only weakly; harmonies (Singers 1-2 have no mic); tambourine via mic
  energy (`TambourineDetector::CheckForSwing`), which the synthetic singer never taps;
  freestyle/overdrive deploy by mic energy.
- Timing: the singer sings at the compensated time, so it cannot show a latency-compensation
  error that a real (late) mic signal would.

## Not done

- `tools/native_health.sh --selftest` not run, as briefed. rb3-app builds and links; no other
  target was built or run (the headless vocal harnesses compile TambourineManager's old arm by
  construction).
- No stub dropped (`tools/rb3_app_link_stubs_gen.py` not needed: no new link reference).
- No Xenia/retail run of the same song; the comparison is retail bytes plus autoplay.
- Not landed; nothing pushed.
