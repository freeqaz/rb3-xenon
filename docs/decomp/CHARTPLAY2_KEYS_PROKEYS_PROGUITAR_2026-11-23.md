# CHARTPLAY2: rb3-app plays keys, pro keys and pro guitar through the pad report (2026-11-23)

Lane chartplay2, branch `chartplay2` (from main `3fdecfd70`), worktree `~/tmp/wt-chartplay2`.
Scratch, logs and binaries: `~/tmp/chartplay2/`. The run scripts are committed in
`tools/rb3_app_walks/chartplay2/` (`runb.sh` and `build.sh` are chartplay's with the paths
changed; `runs.sh`, `runs2.sh` and `runs3.sh` are the three sets below). Every run went
through `coord/no610m.sh` under the shared `gpu1.lock`, with `RB3_APP_DETERMINISTIC=1`.
Follows `CHARTPLAY_CHART_DRIVEN_PAD_WALK_2026-11-23.md` (guitar and drums) and
`KEYSWALK_KEYS_PROGUITAR_WALK_2026-11-23.md` (the `keys` and `proguitar` pad kinds).

## Goal

Chartplay's TrackWatcher dispatch fix (retail 0x8279D990) gave keys, pro keys and pro guitar
their retail watcher impls (KeyboardTrackWatcherImpl, RealGuitarTrackWatcherImpl), but no
input had ever driven them. This lane extends `RB3_APP_CHARTPLAY` to those three parts and
then looks for native bugs in their watchers and scoring.

## What changed

| commit | what |
|---|---|
| `d44dfeb02` | chartplay drives JoypadMidiController, KeyboardController and RealGuitarController through the raw pad report; the native `ReadSingleJoypad` copies it |
| (this commit) | pro guitar strums only the gem's `mSlots` strings (ghost notes are fretted, not strummed); run scripts; this record |

- **The input path.** These three controllers take no pad buttons. Their input is the 16-byte
  peripheral report that retail `ReadSingleJoypad` (0x82529AF8) copies from the HID report and
  `JoypadPollCommon` stores in `JoypadData::mProGuitarData`. `UsbMidiKeyboard::Poll` (0x8251AC10)
  turns key bits into `KeyboardKeyPressed/ReleasedMsg`. `UsbMidiGuitar::Poll` (0x8251A340) turns
  string fret/velocity changes into `StringStrummedMsg` and one `RGSwingMsg`. Natively
  `ReadSingleJoypad` zeroed the report. Now, when chartplay owns pad 0 on one of these
  controllers, it copies the driver's bytes (`RB3AppChartPlayRaw`). From there on the code is
  retail's: both Poll bodies, the controllers, the watchers and scoring.
- **Keys** (`JoypadMidiController`, kTrackKeys): slot s is pressed as the white key in the octave
  from note 0x30 whose `MidiNoteToButton` (note % 12: 0 L2, 2 R2, 4 L1, 5 R1, 7 Tri) is the
  controller's `SlotToButton(s)`. **Pro keys** (`KeyboardController`, kTrackRealKeys): key
  `0x30 + s` (`MidiNoteToSlot`). Key i sets bit `7 - i % 8` of byte `i / 8`. Velocity 100 goes in
  the low 7 bits of bytes 3..7 (`GetSlottedKeyVelocityFromExtended`). A key is held to the gem's
  sustain end (times `_SUSTAIN`), else released on the next poll. A key that was down on the
  last poll goes up for one poll before it is pressed again; that counts as a collision.
- **Pro guitar** (`RealGuitarController`, kTrackRealGuitar on the 22-fret pad): report string j
  is controller string `5 - j` (`j` when lefty). The gem's frets (`GameGem::GetFret`; -1 means
  open) go down one poll before its time, and the strummed strings' velocities toggle
  100 <-> 101 on its poll. `_HOPO=tap` sends the fret change alone, at the gem's time. The
  fields are written through `ProGuitarData` itself, so the bitfield layout is the reader's own.
- Pro keys is picked by one `down` before the part `a` (`walk_k1.txt`). Measured: `walk_k1` gives
  `p0 track 5` in its det lines. A `down` before the first song-select `a` (`walk_k2`) never
  reached the game.
- No `src/` file changed and no `native/src` file was added: nothing for `tools/ab_measure.py`
  to price, and no census or stub change.

## Findings

### 1. Perfect timing matches autoplay on all three parts (prediction: right)

On every leg, chartplay's hit count, miss count and score equal autoplay's (table below):
easy and expert keys, easy and expert pro keys, and easy pro guitar. Expert pro guitar matched
only after finding 2. The whole song-1 input path is the game's own code: key bits ->
UsbMidiKeyboard -> KeyboardController / JoypadMidiController -> KeyboardTrackWatcherImpl
(chords, fat-finger windows, sustains), and fret/velocity -> UsbMidiGuitar -> RealGuitarController
-> RGGemMatcher -> RealGuitarTrackWatcherImpl. So far this found **no native bug** on it.

### 2. Expert pro guitar lost 10 gems to the harness, not the game (fixed in the driver)

Prediction: the expert pro guitar leg (XGC) equals autoplay. **Wrong at first.** With bin-p1,
q1XGC scored 635 hit / 10 miss (score 253,993) against q1XGA's 645 / 0 (285,734). Every missed gem
has `mSlots` smaller than its fretted strings. Example, gem 173: frets `1,3,0,0,-1,-1`,
slots 0x8. The driver had strummed all four strings. Cause: `GameGem(const RGGemInfo &)` puts a
string in `mSlots` only if it is fretted **and not `kRGGhost`**. `RGGemMatcher::FretMatchImpl`
checks one string here (`numChecked == 1`) but counts every string swung within 68 ms
(`numCloseSwings`), and fails a non-strum gem with more than 2. Strumming a ghosted chord on a
single-note gem is therefore a miss in retail too: RGGemMatcher is 9/9 rows at fuzzy 100 in
`report.json`. The driver now strums the `mSlots` strings only. After the fix (bin-p2,
r1XGC): 645 / 0, score 285,734, the same as autoplay.

### 3. Timing knobs behave as the watchers' windows say

- Pro guitar `+80 ms` (q1GP): 156 / 0, the same as on time. RealGuitarTrackWatcherImpl sets
  `mSlop = 125`, so 80 ms late is still inside the window. `-80 ms` (r1GN): 150 / 6. The frets go
  down 113 ms before the gem, outside the fret window.
- Keys `+80 ms`: keys 155 hit / 10 miss and pro keys 129 / 10. The miss counts include spurious
  misses (hit + miss is more than the gem count), as on 5-lane guitar in chartplay.
- 10% skipped: keys 144 / 16, pro keys 120 / 14, easy pro guitar 141 / 15 and expert 576 / 69.
  Each miss count equals the number of skipped gems, so no gem the driver played was lost.
- Pro keys `_SUSTAIN=0.5` (q1PS): 134 / 0 with the score 4,305 lower (33,352). The crowd ends at
  0.9916, the same mechanism chartplay measured on guitar (GemPlayer defers the crowd update to
  the sustain's end).
- Pro guitar `_HOPO=tap`: easy 156 / 0 and expert 645 / 0 (r1XGH), the same as strummed.

### 4. Open lead: p0's solo stars read 0 for a whole second song (also on autoplay)

In some runs' second song, p0's `Player::GetNumStarsFloat` stays 0.0000 all song while the
score climbs. The band performer's stars are normal: q1GP frame 12000 has band stars 3.6373 and
p0 stars 0 at the same score of 20,100. Runs: q1GP, q1XGC and **r1GA, an autoplay run**, so this
is not caused by chartplay; it follows which song the RNG picks next. That path is
`Scoring::GetSoloNumStarsFloat(GetIndividualScore(), mTrackType)`, which returns 0 when
`GetPlayerScoreInfo` finds no base-score entry or the individual score is 0. I checked
`GetPlayerScoreInfo` against retail 0x826A0FF8: same loop and the same 22-fret -> 17-fret
fallback. I did not establish which song it is (the log does not name songs), whether its pro
guitar base-score entry is missing natively, or whether `GetBandContribution` equals the score.
This is the next thing to chase.

### Rows checked against retail (no divergence found)

`KeyboardTrackWatcherImpl::CheckForFatFinger` (95.73 fuzzy; `va_disasm 827a01d8 400`: the
same calls and stores, with `TrackForgivesFatFingering` inlined as `track type == 5`),
`GameGem::PackRealGuitarData` (95.19; `8278ed38 200`: the same loop), and
`UsbMidiGuitar::Poll`'s connected-accessories read (retail also reads byte 0xd for it and for
pitch bend). Everything else on these paths is fuzzy 100: RealGuitarTrackWatcherImpl 21/21,
KeyboardController 25/25, JoypadMidiController 31/31, RealGuitarController 26/26 and
RGGemMatcher 9/9.

## Runs (song 1, det)

bin-p1 `3c5f85df8cda` (`d44dfeb02`), bin-p2 `60d38f59a5f5` (the ghost-string fix). Sets:
p1 (`runs.sh`, 9,000 frames), q1 (`runs2.sh`, 16,000), r1 (`runs3.sh`, bin-p2, 16,000). All 33
runs ended rc 0 at the frame cap with 0 `FAIL` and 0 `UNREACHED` lines. Second songs differ
between legs (the RNG follows the scores) and are not compared, except for finding 4.

| run | part | mode | hit | miss | score | stars |
|---|---|---|---|---|---|---|
| q1KA / q1KC | keys easy | autoplay / chartplay | 160 / 160 | 0 / 0 | 18,028 / 18,028 | 5.0 |
| q1KM | keys easy | miss 10% (16 skipped) | 144 | 16 | 5,500 | 3.39 |
| q1KP | keys easy | +80 ms | 155 | 10 | 11,686 | 4.58 |
| q1XKA / q1XKC | keys expert | autoplay / chartplay | 250 / 250 | 0 / 0 | 39,448 / 39,448 | 5.59 |
| q1PA / q1PC | pro keys easy | autoplay / chartplay | 134 / 134 | 0 / 0 | 37,657 / 37,657 | 5.0 |
| q1PM | pro keys easy | miss 10% (14) | 120 | 14 | 11,335 | 3.38 |
| q1PP | pro keys easy | +80 ms | 129 | 10 | 22,181 | 4.39 |
| q1PS | pro keys easy | sustain 0.5 | 134 | 0 | 33,352 | 5.0 |
| q1XPA / q1XPC | pro keys expert | autoplay / chartplay | 250 / 250 | 0 / 0 | 89,061 / 89,061 | 5.60 |
| r1GA / r1GC | pro guitar easy | autoplay / chartplay | 156 / 156 | 0 / 0 | 36,751 / 36,751 | 5.0 |
| q1GM | pro guitar easy | miss 10% (15) | 141 | 15 | 12,487 | 3.53 |
| q1GP / r1GN | pro guitar easy | +80 / -80 ms | 156 / 150 | 0 / 6 | 36,751 / 27,308 | 5.0 / 4.93 |
| q1GH | pro guitar easy | HOPO tap | 156 | 0 | 36,751 | 5.0 |
| q1XGA | pro guitar expert | autoplay | 645 | 0 | 285,734 | 5.61 |
| q1XGC | pro guitar expert | chartplay, bin-p1 (ghosts strummed) | 635 | 10 | 253,993 | 5.32 |
| r1XGC | pro guitar expert | chartplay, bin-p2 | 645 | 0 | 285,734 | 5.61 |
| r1XGH | pro guitar expert | HOPO tap | 645 | 0 | 285,734 | 5.61 |
| r1XGM | pro guitar expert | miss 10% (69) | 576 | 69 | 88,661 | 3.42 |

The p1 set (9,000 frames) agreed with q1/r1 over its prefix: keys 154/154 and pro keys 128/128,
equal on both legs at frame 9,000.

## Not done

- `tools/native_health.sh --selftest` was not run (left to the coordinator, as briefed). Only
  rb3-app was built.
- Finding 4 (solo stars 0 on some second songs) is not chased.
- Pro bass, 17-fret pro guitar tracks and the button (Mustang) pad were not walked. Pro guitar
  slides, bends and muted strings are played as plain frets. The pro keys hand-placement,
  sustain-pedal and expression bytes stay zero.
- BandCharacter.cpp, src/system/ui, rndobj and char are untouched. Not landed; nothing pushed.
