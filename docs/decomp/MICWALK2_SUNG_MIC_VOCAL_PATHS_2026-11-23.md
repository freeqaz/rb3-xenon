# MICWALK2: the sung mic on Hard/Expert, tambourine, freestyle deploy, harmonies and spoken notes (2026-11-23)

Lane micwalk2, branch `micwalk2` (from main `3fdecfd70`), worktree `~/tmp/wt-micwalk2`. Scratch,
logs and binaries: `~/tmp/micwalk2/`. Scripts are committed in `tools/rb3_app_walks/micwalk2/`
(`runb.sh` is harmwalk's with `A=`/`X=` changed; `runsN.sh` are the run sets; `summ.py` is
micwalk's summary, `hits.py` p0's (score, hit, miss) at every change, `tcmp.py` per-phrase
increments side by side, `rd.py` reads retail .rdata by VA). Every run went through
`rb3-scout/coord/no610m.sh` under the shared `gpu1.lock`, with `RB3_APP_DETERMINISTIC=1` and a
fresh reflink copy of `memcard/content_a`, walk `rb3-scout/a25/walk.txt` (or the variants below),
8,400 frames.

## Goal

MICWALK showed the synthetic sung mic scoring exactly like autoplay on Easy. This lane pushes it
into the vocal paths that run did not reach (Hard/Expert, unpitched notes, tambourine by mic,
overdrive deploy by mic energy, harmonies by mic) and looks for native bugs there.

## What changed

| commit | what |
|---|---|
| `615c6febb` | sung mic: `_TAMB`, `_FREESTYLE`, `_COUNT`, `_UNPITCHED`, the per-song summary line |
| `2839f8560` | `vb` talky flags in the per-window log; walk scripts |
| this one | this record |

No `src/` file changed, so nothing was priced with `ab_measure.py` (no X360 TU is touched; the
two native files are rb3-app-only). No native/src file was added
(`rb3_app_mic_native.cpp` and `rb3_app_native_glue.cpp` were already classified), no stub
dropped.

- `RB3_APP_SUNG_MIC_TAMB=1`: mic 0 sings 60 ms of tone (MIDI 72) from each tambourine gem's
  time (`VocalNoteList::mTambourineGems`, `TickToMs`), at the same compensated clock as the notes.
  `TambourineDetector::CheckForSwing` sees an energy rise and drop.
- `RB3_APP_SUNG_MIC_FREESTYLE=1`: mic 0 sings MIDI 60 through every one of part 0's
  `mFreestyleSections`, i.e. a sustained loud voice (energy above `scream_energy_threshold`)
  that `Singer::AddToFreestyleDeployment` accumulates while the section is live.
- `RB3_APP_SUNG_MIC_COUNT=n` (1..3): n connected mics, mic k singing part k.
  `Rb3AppNativeSynth` reports n connected mics; `MicClientMapper::RefreshMics` captures them in
  order, so client k (Singer k's `MicClientID(k, -1)`) gets mic k.
- `RB3_APP_SUNG_MIC_UNPITCHED=0`: mic 0 is silent on unpitched notes (a control for the talky
  path; default 1 sings them at their nominal pitch).
- Once per note list: `rb3-app-mic: song notes N unpitched U phrases P tamb_gems G freestyle F`
  plus the `scoring vocals` thresholds as rb3-app read them: `nonpitch_energy_threshold` 0.000,
  `tambourine_energy_rise_threshold` 0.040, `_drop_` 0.020, `tambourine_ms_offset` 24,
  `tambourine_window_ticks` 120, `scream_energy_threshold` 0.300.
- The per-window log line gains `vb <unk1><unk0> <unk4>` (Singer k's `VoiceBeat`: voiced,
  spam, syllable level), the three inputs to `VocalPart::GetBestHit`'s talky test.

Unset, every knob leaves rb3-app as it was (default walks have no mic; `RB3_APP_SUNG_MIC=1`
alone is micwalk's single mic). The song-summary line is a printf and reads config only.

## Results

All runs seed 0 unless noted. The a25 song changed since MICWALK (baselines move with
landings): seed 0 is now 477 notes, 2 unpitched, 37 phrases, 83 tambourine gems, 41 freestyle
sections; at 8,400 frames it is cut after 31 phrases (31 hit 0 miss in every run below), so
compare runs with each other, not with MICWALK's 30,000.

### Hard and Expert (run set 1, `walk_d2.txt` / `walk_d3.txt`: two / three DOWNs before the difficulty A)

Prediction: the tight Hard/Expert slop and pitch margin would expose the detector's onset lag
(MICWALK measured 0.016 st median error but 235 sung-but-silent windows at onsets), so sung
would lose some phrase points to autoplay.

| run | difficulty | input | score at 8,400 | hit/miss | stars |
|---|---|---|---|---|---|
| HA | Hard | autoplay | 102,700 | 31 / 0 | 5.00 |
| HS | Hard | sung mic | 94,400 | 31 / 0 | 5.00 |
| XA | Expert | autoplay | 126,300 | 31 / 0 | 5.46 |
| XS | Expert | sung mic | 118,000 | 31 / 0 | 5.26 |

**The prediction was wrong.** Every phrase lands on the same frame with the same increment
(`HA.hits` vs `HS.hits`: 3,200 per phrase on Hard, 4,000 on Expert). The whole 8,300 gap is one
event at frame 6371 with no hit/miss change: the tambourine section bonus, which autoplay
collects and a mic that never taps cannot. Detector: 2,045 sung windows, 1,818 detected,
|det − sung| median 0.014 st, 94.0% within 0.5 st, no octave errors.

### Tambourine by mic (run set 2, Easy)

| run | input | score at 8,400 | stars |
|---|---|---|---|
| E0 | autoplay | 31,900 | 5.00 |
| ES | sung mic | 23,600 | 4.96 |
| ET | sung mic + `_TAMB` | **31,900** | 5.00 |

ET's hit timeline is **identical** to autoplay's (`diff E0.hits ET.hits` empty, including the
+8,300 at frame 6371), so the 83 taps went through `TambourineDetector::CheckForSwing` ->
`TambourineManager::TambourineSwing` and scored every gem inside the 120-tick window. ES is
the control: without taps the tambourine bonus is absent.

### Overdrive deploy by mic energy (run set 2, `RB3_APP_DET_OD=1`)

EF (sung mic + `_FREESTYLE`) has the same hit frames as ES and **three deploys**, ES and E0
none (autoplay vocals do not deploy without Select). Each deploy fires once energy is 0.5 and a
freestyle section is live: energy reached 0.502 at frames 3160, 4433 and 7630 and the deploy
followed at 3315 (the next freestyle section), 4449 and 7646, i.e. 16 frames (533 ms of sung
voice) inside a live section; each lasted 247 frames and doubled the multiplier (`mult 8`).
Score 28,340 against ES's 23,600 (+4,740 from overdrive phrases).

### Harmony by three mics (run set 3, `walk_harm.txt`: harmwalk's DOWN on part_difficulty_screen)

HMA (autoplay) vs HM3 (`_COUNT=3`, mic k sings part k, plus `_TAMB`), `RB3_APP_DET_HARM=1`:
the song is now 2-part (477 / 119 notes). Final state **identical**: score 24,240, 31 hit 0
miss, `hstat 160 dbl 8/8 vpc 1.0000/1.0000`, and the hit timelines are identical
(`HMA.hits` = `HM3.hits`). Of 7,371 common frames 1,423 differ, all inside phrases: the running
phrase score under the mic trails autoplay by about one frame (`sc 13.4` vs `15.9` at the first
scored frame of phrase 1) and reaches the same cap. Singer 1's mic tracked part 1 (178 sung
windows, 153 detected, 146 within 0.5 st); mic 2 stayed silent, as it should on a 2-part song.

### Unpitched (spoken) notes (run set 6, seed 2: 629 notes, **138 unpitched**, 59 phrases, 14 tambourine gems)

Run set 5 (the seed-0 song's 2 unpitched notes, sung vs silent) was stopped early once the
sweep found a better song; its logs are partial and unused. A seed sweep (run set 4, 2,200
frames each) found the song: seeds 1, 3, 4, 5 have no unpitched
notes, seed 6 has 3, seed 2 has 138. TA autoplay, T1 sung mic (sings unpitched notes at their
nominal pitch), T0 the control (silent on unpitched notes); no `_TAMB`, so autoplay's tambourine
bonuses are extra. Per-phrase increments (`tcmp.py`):

| frame | TA autoplay | T1 sung | T0 silent on unpitched |
|---|---|---|---|
| 1820 | bonus +700 | - | - |
| 1851 | hit +200 | hit +200 | miss +0 |
| 2074 | bonus +700 | - | - |
| 2216 | hit +400 | hit +400 | hit +200 |
| 2342 | hit +600 | hit +600 | hit +400 |
| 2468 | hit +800 | hit +800 | miss +384 |
| 2594 | hit +800 | hit +800 | hit +200 |
| 2720 | hit +800 | hit +800 | miss +0 |
| 2845 | hit +800 | hit +800 | miss +0 |
| 2910 | hit +800 | hit +800 | hit +200 |
| … | (46 more phrases, full table `~/tmp/micwalk2/tcmp_T.txt`) | | |

| run | input | song end (frame 7597) | hit / miss | stars |
|---|---|---|---|---|
| TA | autoplay | 45,000 | 56 / 0 | 5.00 |
| T1 | sung mic | 43,600 | 56 / 0 | 5.00 |
| T0 | sung mic, silent on unpitched notes | 18,729 | 33 / 23 | 3.85 |

**T1 matches autoplay on all 56 phrases** (same frame, same increment, 0 differences); the
1,400 gap is autoplay's two +700 tambourine bonuses (frames 1820, 2074), which T1 did not tap.
T0 misses 23 phrases.

Prediction: a steady harmonic tone is not speech, so the talky matcher might refuse it. It did
not: the `vb` flags read `10` (voiced, not spam) on unpitched notes with a positive syllable
level, which is all `GetBestHit` asks for (`nonpitch_energy_threshold` is 0): of 887 analysed windows
on unpitched notes (T1 sung where T0 is silent at the same song ms) 879 read `10` and 871 also
have level > 0; the 2,040 pitched windows all read `10` with level > 0. The control
discriminates (T0 misses the phrases whose notes are mostly unpitched), and T1 matches autoplay
phrase for phrase. Detector on T1: 2,927 sung windows, 2,419 detected, |det − sung| median 0.016 st.

## Retail comparison (tools/va_disasm.py on clean TU5)

- `TambourineDetector::CheckForSwing` 0x826FBC00 and its ctor 0x826FBAC8 (the unit is not in
  `objects.json` as matching, so it was the likeliest place for a native-only divergence):
  **match**. `tick = MsToTick(ms - mLagMsOffset)`; armed: `unkc = max(unkc, e)`, disarm when
  `e < unkc - drop` (0x14) storing `unk10 = e`; disarmed: `unk10 = min(unk10, e)`, arm when
  `e > unk10 + rise` (0x18) storing `unkc = e`; on the rising edge `TambourineSwing(tick)`
  (bl 0x826FB4E0) and, if non-zero, `Singer::NoteTambourineSwing(sub)` (bl 0x826F71F0). The
  ctor's config keys at 0x820F4C3C/0x820F4C18/0x820F4C00 are drop, rise, ms_offset in that
  order into 0x14/0x18/0x1C; initial `unkc` 0.0 (0x82000D78), `unk10` 1.0 (0x820009FC).
- `TambourineManager::TambourineSwing` 0x826FB4E0 (95.83% fuzzy): the gem lookup through
  `mPlayerRef.mVocalParts[0]->mVocalNoteList->mTambourineGems`, the popup at `unk4c >= 8`, the
  `unk4c = 0` reset, early `TambourineFail(-1, true)` returning 1 without advancing, succeed 2 /
  fail 3 with the index advance: **match**; the residual is scheduling.
- `VocalPart::GetBestHit` (96.97%, the talky branch) and `VocalPart::IsEmptyPhrase` (96.55%):
  the only objdiff differences are two swapped instructions and one `clrrwi`; **no logic diff**.
  `VocalPart::CalcPhraseScoreMax` 0x826F1E08 (76.96%): retail reloads the phrase start/end each
  iteration and clamps each note to the phrase, as our loop does; register allocation only.
- `VoiceBeat::Analyze` 0x82717D38 (86.5%, the talky detector) — **partly checked**: all 27
  constant-pool loads have the same set of values as retail (read from band.exe; the
  positional pairing differs because retail spills the filter coefficients to different stack
  slots), and the anti-alias filter's `-0.9459779362 * ya[0] + 1.9444776578 * ya[1]` was
  traced through retail's slots (0x90/0x98) to the same products. The syllable and spam filter
  blocks were not traced instruction by instruction. `tools/const_value_audit.py --unit` died
  with `KeyError: 'res'` on every unit tried; not debugged.

## No native bug found

Against the brief's expectation, none of the five paths shows a native bug: Hard/Expert,
tambourine by mic and harmony by mic reproduce autoplay's phrase results exactly, freestyle
deploys fire by mic energy, and the talky path scores spoken notes from the mic. The retail
functions on those paths that are not byte-matched were compared by hand and agree.

Open, not covered by this lane:

- Timing: the synthetic singer sings at the compensated time, so a latency-compensation error
  (a real mic is late) stays invisible. Tambourine taps are on the gem; a deliberately early or
  late tap (inside and outside the 120-tick window) was not tried.
- The rest of `VoiceBeat::Analyze` (syllable/spam filters) by instruction.
- 3-part harmony by mic: the seed-0 song is 2-part now; mic 2 was connected but silent.
- No Xenia/retail run of the same inputs; the comparison is retail bytes plus autoplay.
- `tools/native_health.sh --selftest` not run, as briefed. rb3-app builds and links.
