# MESSY: no-mic vocal phrase ratings in rb3-app vs retail (2026-11-23)

**Verdict: no divergence. Negative result, no `src/` change.** Retail TU5 also rates every
scored phrase of a mic-less vocalist and shows "MESSY" (`vox_rating_0`) under the now-bubble.
rb3-app's behaviour matches retail in where the ratings start, where they stop and how often
they show. The claim being tested (peer audit A54, gap 3/8: "native shows MESSY in at least
7 of 100 frames, retail in 0 of about 25") was a sampling miss on the retail side.

## Claim under test

- Source: `~/tmp/claude-1000/rb3-scout/a54/HANDOFF.md` item 3. Retail run is A48's r6
  (`a48/png/r6`, Xenia, one vocals pad, no mic, "Connect a Mic" shown). Native run is A54's S4a
  (`RB3_APP_SEED=4`, `RB3_APP_DETERMINISTIC=1`, China Grove, `a54/walk_cg.txt`, rb3-app from
  main 17275b46f).
- Prediction going in: some gate on mic presence exists in retail, and native bypasses it.

## What retail's code does (retail bytes, `tools/va_disasm.py 826f3068 400`)

`VocalPart::HandlePhraseEnd` (0x826F3068) writes rating -1, then gates the real rating on
exactly five things and nothing else:

```
826f313c  bl    0x826e3d70          ; VocalPlayer::ScoringEnabled
826f3144  beq   skip
826f3158  beq   cr6, skip           ; mThisPhrase == mPhrases.end()
826f3160  lwz   r10, 0x280(r3)      ; Player::mEnabledState
826f3168  bne   cr6, skip           ; != kPlayerEnabled
826f3170  lfs   f13, 0xbc(r31)      ; mFirstPhraseMsToScore
826f3178  blt   cr6, skip
826f317c  lfs   f0, 0x38(r31)       ; mPhraseScoreMax
826f3184  beq   cr6, skip           ; == 0
826f318c  fdivs f1, f13, f0         ; mPhraseScore / mPhraseScoreMax
826f3190  bl    0x826e55f8          ; VocalPlayer::CalculatePhraseRating
```

- None of the five involves the mic. `TheGameMicManager` is read only by the mic *display*
  (`UpdateMicDisplay`), pitch correction and `AllowWarningState`.
- With no mic, `mPhraseScore` stays 0, so `CalculatePhraseRating(0)` returns 0, and
  `LocalScorePhrase` -> `VocalTrackDir::ShowPhraseFeedback(0)` -> `GetRating(0)` gives
  `vox_rating_0`, which is MESSY.
- Our source for this chain scores 100 on `report.json` (main build, `name_check`):
  `VocalPart::HandlePhraseEnd` 99.96 fuzzy (the known `mullw` operand-order residual),
  `VocalPlayer::HandlePhraseEnd`, `LocalScorePhrase`, `CalculatePhraseRating`,
  `ScoringEnabled`, `Poll`, `UpdateMicDisplay`, `VocalTrackDir::ShowPhraseFeedback`,
  `GetRating` and `StreakMeter::ShowPhraseFeedback` all 100.
- rb3-app (`RB3_NATIVE_APP`) takes the retail arm at every `HX_NATIVE` split on this path.
  The `!defined(RB3_NATIVE_APP)` arms only cover the headless harnesses.

## What retail's frames show (the refutation)

- **Instrument:** a 300x130 crop under the now-bubble (x 110..410, y 190..320 at 1280x720) of
  every captured frame, as contact sheets (`~/tmp/messy/retail_crops.png`,
  `~/tmp/messy/native_crops.png`). A white-pixel counter (`~/tmp/messy/count.py`) flagged
  candidate frames, and I confirmed each by eye. The counter alone gives false positives on
  bright backgrounds (retail 4320, 9960 and 10980; native 1920, a white shoe).
- **Control:** the same crop and counter find all of A54's native MESSY frames
  (2640, 2940, 3600, 3960, 4080, 5220 and 5340), so the instrument can return a positive.
- **Retail r6, swaps 2760..12000 (155 frames, one per 60 swaps):** MESSY in 18 frames: 4680,
  4800, 5040, 5220 (fading), 5400, 5580, 5940, 6120, 6600, 6720, 6960, 7440, 7800, 7980, 8640,
  8880, 9540 and 9780. Full-resolution checks of 4800 and 6120: `~/tmp/messy/retail_messy_pair.png`.
- **Native S4a, frames 1560..6600 (85 frames, one per 60 frames at 30 fps):** MESSY in 10
  frames: 2640, 2760 (fading), 2940, 3240 (fading), 3600, 3960, 4080, 4260, 5220 and 5340.
- **Song-time agreement**, using A54's mapping `native ≈ 3084 + 0.5587 × (retail − 5700)`:

  | | retail swap | maps to native frame | native observed |
  |---|---|---|---|
  | first MESSY | 4680 | ≈2514 | 2640 (sampled every 60 frames, about 2 s) |
  | last MESSY | 9780 | ≈5364 | 5340 |
  | none after | to 12000 | to ≈6613 | none, 5400..6600 |

- **Rate inside the rated span:** retail 18 of 86 frames (21%); native 10 of 46 frames (22%).
  The on-screen feedback lasts a fixed time, so the fraction of frames is the comparable
  number. The two agree.
- **Why A54 saw 0 of 25:** the retail frames in its sheet (5340, 7500, 8700 and 9300) all fall
  between feedback flashes. At about 21% per frame, a hand-picked sample can easily miss them.
  A54's "7 of 100" native count is a full-sequence count, so the two numbers were never
  measured the same way.

## Native re-run on current main (worktree build)

- **Binary:** worktree `messy` at main 3fdecfd70 (after micwalk, whose synthetic mic is
  opt-in), built with `cmake --build build --target rb3-app`, rc 0.
- **Run:** through `coord/no610m.sh` with A54's env (seed 4, deterministic, `walk_cg.txt`, no
  mic), 5400 frames, shots at A54's MESSY frames. Script: `~/tmp/messy/run.sh`, log
  `~/tmp/messy/run/run.log`.
- **Result:** rc 0, 5400 frames, `songs/chinagrove/`. MESSY appears in all seven of A54's
  frames (`~/tmp/messy/run/crops.png`), so this behaviour is unchanged since 17275b46f.
- **Per-phrase timing from the det-line vocal `miss` counter:** the first rated phrase is at
  native frame 2535. Retail's first MESSY frame, 4680, maps to about 2514. That is 0.7 s
  apart, inside the mapping's ±0.5 s plus the feedback's on-screen time.
  - The counter reaches 23 by frame 5317. Retail's last MESSY, 9780, maps to about 5364.
  - A54's full S4a run ends at 25 misses (`a54/r_S4a/run.log`, last det line).
- **Enabled state:** rb3-app reports `en 0` (kPlayerEnabled) throughout, which matches the
  retail gate above. Retail must also be enabled here, or it could not show MESSY.

## What I deliberately did not do

- No `src/` change, so there was nothing to price with `tools/ab_measure.py`.
  Adding a mic gate would make rb3-app diverge from retail.
- No new Xenia run. r6's 155 in-song frames were enough to refute the claim.
- I did not touch `VocalPlayer.cpp`, which the parallel micwalk2 lane may edit.
