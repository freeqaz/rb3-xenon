# CHARTPLAY3: rb3-app chartplay plays pro bass, 17- and 22-fret tracks, mutes, slides, bends and the pro keys pedals (2026-11-23)

Lane chartplay3, branch `chartplay3` (from main `8001d870d`), worktree `~/tmp/wt-chartplay3`.
Scratch, logs and binaries: `~/tmp/chartplay3/`. The run scripts are committed in
`tools/rb3_app_walks/chartplay3/`: `runb.sh` and `build.sh` are chartplay2's with the paths
changed; `runs1.sh` (part discovery), `runs2.sh` (sets 1-3 below), `seeds.sh` (which song a
seed picks); `summ.py` is chartplay2's; `scorediff.py` prints the first frames where p0's
score, hit or miss differ between two runs. Every run went through `coord/no610m.sh` under
the shared `gpu1.lock`, with `RB3_APP_DETERMINISTIC=1`. One binary for every run:
bin-c1, sha256 `d0ad4bbbaa9b…` (commit `2cc92162f`; the later commit only adds comments).
Follows `CHARTPLAY2_KEYS_PROKEYS_PROGUITAR_2026-11-23.md`.

## Goal

Drive the pro string and pro keys paths chartplay2 left out: pro bass, 17-fret pro guitar
tracks, muted strings, slides and bends played as the chart marks them, and the pro keys
pedal and hand-placement bytes. On time, chartplay should score what autoplay scores. Judge
any difference against retail.

## What changed (`native/src/rb3_app_chartplay.cpp` only)

No `src/` file changed, so there was nothing for `tools/ab_measure.py` to price. No
`native/src` file was added, so the stub census is unchanged.

- The gem-list line now prints the song track's **type** (`SongData::TrackTypeAt`; 6/7 =
  pro guitar 17/22-fret, 8/9 = pro bass 17/22-fret) and how many gems are muted, left-hand
  slides (sustained or reverse) or bends. The summary line counts muted gems played, slides,
  bends and deploys. The det line's `p0 track` is `Player::mTrackType`, the user's type, and
  is not the song track's type (see STARZERO).
- `_MUTE=open|keep|openall`: a `kRGMuted` string goes down open, or keeps its last fret.
  `openall` opens every string of a gem that has a muted one.
- `_SLIDE=on|all`: halfway through the sustain of a `LeftHandSlide` gem, each fretted
  string moves 2 frets (down for `ReverseSlide`). This is a fret change alone, so
  UsbMidiGuitar sends a velocity-0 StringStrummedMsg. `all` is the control: it slides every
  sustained gem.
- `_BEND=1`: the pitch-bend byte (0xd) goes to 100 on a gem with a `kRGBend` string.
- `_DEPLOY=pedal|stomp|button` (pro keys): for one poll, press the sustain pedal (byte 8
  bit 7), the stomp pedal (byte 9 bit 7), or the controller's `mForceMercuryBut`. This
  happens when the player has at least 0.5 energy and is not deploying, at most once every
  30 polls.
- `_HANDS=1` (pro keys): the low hand-placement field (byte 0xe, low 5 bits) gets the held
  keys' lowest index. The high one gets their highest index, capped at 15 (bit 7 of 0xb,
  0xc and 0xd, plus bits 5-6 of 0xe).
- The pedal and hand fields are written through `ProKeysData`, the reader's own bitfields.
  Retail's byte and bit positions come from `va_disasm 8251AC10` (UsbMidiKeyboard::Poll).
  Each native field read matches retail's: sustain `lbz 8; srwi 7`, stomp `lbz 9; srwi 7`,
  mod `lbz 0xa & 0x7f`, expression `lbz 9 & 0x7f`, accessories `lbz 0xf`, low hand
  `lbz 0xe & 0x1f`. For the high hand, retail's sum `(d>>5&4)+(c>>6&2)+(e>>2&0x18)+(b>>7)` is
  `unkdbool<<2 + unkcbool<<1 + unkemiddle<<3 + unkbbool`.

## Which songs and parts

- The seed-0 song moved since chartplay2 (main's landings changed RNG draws). Song 1 is now
  **inthemeantime** on the pro guitar pad. Its MIDI has `PART REAL_GUITAR` (17-fret only),
  `PART REAL_BASS` and `PART REAL_BASS_22`. On the keytar, song 1 is goodvibrationslive.
- Pro bass is one `down` before the part `a` (`walk_pb1.txt`). Two or three downs
  (`walk_pb2/3`) also gave `p0 track 8`.
- Measured song track types on the 22-fret pad: pro guitar on inthemeantime is **type 6**,
  the 17-fret track (TrackNumOfType(7) falls back to it). Pro bass is **type 9**, the 22-fret
  bass track. So chartplay2's song-1 pro guitar runs were also 17-fret tracks.
- Seed scan (`seeds.sh`, 1,500 frames): seed 1 heartofglass, **2 beencaughtstealing (G22)**,
  **3 coldasice (G22)**, 4 not run (it crashes in calibration; see MESSY), 5 dontburyme,
  6 antibodies, 7 ifeelgoodalt, 8 thisbastardslife, **9 inabigcountry (G22)**. Seed 3 is set 3
  below, the 22-fret pro guitar track.
- Expert chart contents (gem-list lines): pro guitar type 6 has 833 gems, 126 muted, 0
  slides and 66 bends. Pro bass type 9 has 898 gems, 26 muted, 89 slides (80 sustained, 44
  reverse) and 0 bends. The guitar slide runs therefore play no slides, and the slide test
  is on bass.

## Findings

### 1. Pro bass on time equals autoplay (prediction: right)

Easy: chartplay BC and autoplay aB1A both have 82 hits, 0 misses and 20,820 points at frame
9,000 (aB1A stops there). BC finishes the song with 92/92 and no misses. Expert: XBA and XBC
both have 898 hits, 0 misses and **326,971** points at frame 10,000. p0's stars read 0.0000
on both legs. That is STARZERO's mechanism on bass: the user's type is 8, the song track's is
9, and `GetPlayerScoreInfo` falls back only 9→8, never 8→9. Retail does the same; I checked
`GemPlayer::ConfigureBehavior` against `va_disasm 826bd6c8`, including bass's ×6 for user
types 2 and 8. Not a native bug.

### 2. Muted strings: every way of playing them hits (prediction: right; the control could not fail)

`RGGemMatcher::FretMatchImpl` counts a `kRGMuted` string as matched at any fret. Expert
guitar has 126 muted gems:

- XGC (chart frets): 833/0, 405,202.
- XGMo (muted strings open): 833/0, 405,201.
- XGMa (`openall`): 833/0, 405,201.

Bass XBMo (26 muted gems) scores 326,971, the same as XBC. **The `openall` control did not
discriminate**: on this chart every fretted string of a muted gem is itself muted, so
`openall` is the same input as `open`. The 1-point differences are finding 5.

### 3. Slides keep their sustain; sliding a non-slide sustain kills it (prediction: right, with a working control)

- XBSo (bass, 79 slides played): 898/0, 326,971, the same as XBC and XBA.
  `TrackWatcherImpl::KillSustain` spares a `LeftHandSlide` gem when
  `RealGuitarTrackWatcherImpl::PollHook`'s current-state FretMatch fails.
- Control XBSa (113 sustains slid): 898/0 but **325,466** points, and the crowd ends at
  0.9969.
- Control XGSa (guitar, 109 sustains slid): 833/0 but **381,813** points against 405,202.

So the instrument discriminates: a mid-sustain fret change costs sustain points unless the
gem is a slide.

### 4. Bends and hand placement change no score; pedal, stomp and button deploy the same (prediction: right)

- XGB (66 bend gems with the pitch-bend byte raised): 833/0, 405,201. Nothing in the game
  handles `RGPitchBendMsg` or `RGConnectedAccessoriesMsg` (the grep finds only their
  senders). Retail fills both messages from byte 0xd.
- XPH (hand placement on): song 1 scores 379/0 and 212,055, the same as XPC. Nothing in the
  game handles `Keyboard{Low,High}HandPlacementMsg`.
- Deploy runs: XPDp (sustain pedal) and XPDb (OD button) deploy 6 times each. Both end song 1
  with 379/0 and **271,957** points (stars 6.0), against 212,055 with no deploys (XPC). A
  pedal press and release sends two `KeyboardSustainMsg`, and each one calls
  `ForceMercurySwitch(true/false)`. The second call comes while the band is already
  deploying, so it changes nothing. `GemPlayer::MercurySwitch` is retail's
  (`va_disasm 826bc130`: the `mTiltDeployBand` byte at behavior+1, then `vtbl+0x190`), and
  `ConfigureBehavior` turns tilt deploy on for keys and pro keys.
- Stomp pedal (XPDs): 6 deploys, 379/0 and 271,957, the same as the pedal and the button.

### 5. Det runs are not input-exact: extra message traffic shifts the virtual clock

Score differences of ±1 point flicker between legs whose hits and misses are equal: XGC
against XGMo or XGB, and XPDp against XPDb. In XGC against XGB, the det state hash differs
from frame 2851 while the RNG counters stay equal (frame 2851: `rand 124/227` on both legs).
In XPC against XPH, the beat clock differs in the fourth decimal at frame 1441 (12.7160
against 12.7161). XPC's second song then differs too (thisbastardslife against
inthemeantime).

Mechanism, by inference and not proven: `rb3_app_determinism.cpp` advances the virtual clock
1 µs on every `steady_clock` call. Extra messages (pitch bend, hand placement, a pedal's two
messages) run more code per poll, which moves the song clock by microseconds. That moves the
fractional sustain points by less than one point, which shows up as rounding flips. So under
`RB3_APP_DETERMINISTIC`, `rb3_app_det_cmp.py` cannot certify "no behaviour change" for an
input change that adds messages. Compare hits, misses and end scores instead, and allow a
1-point sustain rounding. Not chased further.

### Native bugs found

**None.** Every scored difference has a retail-identical cause: finding 1, and the slide
and control results in finding 3. The 1-point flickers are a harness effect (finding 5).

## Runs (det, bin-c1, 10,000 frames unless noted)

| run | part | mode | hit | miss | score | note |
|---|---|---|---|---|---|---|
| aB1A / bBC | pro bass easy (type 9) | autoplay / chartplay | 82 / 82 | 0 / 0 | 20,820 / 20,820 | at frame 9,000 |
| bXBA / bXBC | pro bass expert | autoplay / chartplay | 898 / 898 | 0 / 0 | 326,971 / 326,971 | stars 0 (STARZERO) |
| bXBMo | pro bass expert | mute open | 898 | 0 | 326,971 | |
| bXBSo | pro bass expert | slides (79) | 898 | 0 | 326,971 | |
| bXBSa | pro bass expert | control: slide all (113) | 898 | 0 | 325,466 | crowd 0.9969 |
| aGC | pro guitar easy (type 6) | chartplay | 134 | 0 | 28,740 | at frame 9,000 |
| bXGA / bXGC | pro guitar expert (type 6) | autoplay / chartplay | 833 / 833 | 0 / 0 | 405,201 / 405,202 | finding 5 |
| bXGMo / bXGMa | pro guitar expert | mute open / openall | 833 | 0 | 405,201 | 126 muted |
| bXGSo | pro guitar expert | slides (0 on this chart) | 833 | 0 | 405,202 | det-identical to bXGC |
| bXGSa | pro guitar expert | control: slide all (109) | 833 | 0 | 381,813 | crowd 0.9934 |
| bXGB | pro guitar expert | bends (66) | 833 | 0 | 405,201 | |
| bXPC | pro keys expert | chartplay | 379 | 0 | 212,055 | song 1 |
| bXPDp / bXPDb | pro keys expert | deploy pedal / button (6 each) | 379 | 0 | 271,957 | stars 6.0 |
| bXPH | pro keys expert | hand placement | 379 | 0 | 212,055 | |

All runs ended rc 0 with no `FAIL` or `UNREACHED` lines.

Set 3, also on bin-c1. The b3 runs use seed 3, song 1 **coldasice**, which has a 22-fret
pro guitar track. bXGMk and bXPDs use seed 0.

| run | part | mode | hit | miss | score | note |
|---|---|---|---|---|---|---|
| b3XGA / b3XGC | pro guitar expert, **type 7 (22-fret)** | autoplay / chartplay | 249 / 249 | 0 / 0 | 115,251 / 115,251 | 6 muted, 27 slides (5 sustained), 10 bends; stars 0 (STARZERO) |
| b3XGMo | same | mute open | 249 | 0 | 115,251 | |
| b3XGSo | same | slides (5 played) | 249 | 0 | 115,251 | |
| bXGMk | pro guitar expert (type 6) | mute keep | 833 | 0 | 405,201 | |
| bXPDs | pro keys expert | deploy stomp (6) | 379 | 0 | 271,957 | equals pedal and button |

The 22-fret track on time equals autoplay, and so do its mutes and slides. The 22-fret
`RealGuitarTrackWatcherImpl` (`mIs22Fret`) scores like the 17-fret one. Second songs differ
between legs because the RNG follows the scores, so they are not compared.

## Not done

- The 17-fret **button guitar** (Mustang, `kJoypadXboxButtonGuitar`) was not walked. Its
  calibration reads a Calbert sensor that the zeroed raw report cannot supply (KEYSWALK §2).
  A 17-fret track was played instead through the 22-fret pad's type 6 fallback.
- The `openall` mute control cannot fail on these charts (finding 2). A chart with mixed
  muted and fretted strings would be needed.
- Not touched: the mod wheel (whammy), the expression pedal, the pro guitar muting byte and
  the stomp box on pro guitar.
- The 1-point clock-shift mechanism (finding 5) is inferred, not proven: I did not count
  `steady_clock` calls.
- `tools/native_health.sh --selftest` was not run; that is left to the coordinator, as
  briefed. Only rb3-app was built. BandCharacter.cpp, src/system/ui, rndobj and char are
  untouched. Not landed; nothing pushed.
