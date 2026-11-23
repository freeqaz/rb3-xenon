# VOCALOD: vocal overdrive energy in the rb3-app walk (2026-11-23)

Lane vocalod, branch `vocalod` (from `scorewalk` `602f2bb07`), worktree `~/tmp/wt-vocalod`.
Scratch, logs and binaries: `~/tmp/vocalod/`. `runb.sh` is a copy of `a25/runb.sh` with
`A=~/tmp/vocalod`, `X=~/tmp/wt-vocalod`; every run goes through `coord/no610m.sh` under the
shared `gpu1.lock`, walk `/home/free/tmp/claude-1000/rb3-scout/a25/walk.txt`, 90000 frames,
`SECS=540 TMO=580`, `RB3_APP_AUTOPLAY=1 RB3_APP_DETERMINISTIC=1`.

## Question

The scorewalk record (`SCOREWALK_DETERMINISTIC_SCORING_WALK_2026-11-23.md` §3) left open why
`energy` stayed 0.0000 over 71 autoplayed vocal phrases, all hit: either the charts carry no
vocal overdrive phrases on this path, or native never awards them.

## 1. The charts carry vocal overdrive phrases

Read from the retail ark (`rb3-ark <xbox-zip> /dev/null songs/<s>/<s>.mid --dump`; the rc=1 is
the `/dev/null` reference mismatch, the dump is the ark's bytes), counted with
`~/tmp/vocalod/scripts/midnotes.py` (note-on, velocity > 0, per named track):

| song | PART VOCALS phrases (105) | overdrive (116) | 116 ticks |
|---|---|---|---|
| thecon | 31 | **7** | 23040 38520 70200 81600 97080 128160 151320 |
| iwannabesedated | 41 | **8** | 27960 47640 66960 105480 120600 140160 158790 170760 |

(thecon's HARM1 repeats PART VOCALS' 7; HARM2 has none. The walk plays solo vocals.)
The 31 phrases match scorewalk's 31/31 on thecon, so this is the chart the walk played.
Prediction before reading: some OD. So the chart is not the cause: 15 OD phrases over the walk.

## 2. Native skipped both halves of retail's vocal overdrive award (fixed)

The award path, read from source and checked against retail at each step:

1. `VocalPart::HandlePhraseEnd` (0x826F3068) sets `mSpotlightPhraseID` for the phrase that
   just ended: retail 0x826F3370 `bl GetUserGuid` (0x826A2600) -> 0x826F3380
   `bl GameConfig::GetTrackNum` (0x82688780) -> 0x826F3394 `bl SongDB::GetCommonPhraseID`
   (0x826853E0). **Native forced it to -1** under `#ifdef HX_NATIVE` ("headless: no BandUser
   ... no TheGameConfig track table").
2. `VocalPlayer::HandlePhraseEnd` (0x826E97F8) reads it through `GetSpotlightPhraseID`
   (0x826E98A0 `bl 0x826E4D50`), and at the end retail runs `mTrack->OnPhraseComplete`
   (0x826E9CA0), `LocalScorePhrase` (0x826E9CD0), the `send_score_phrase` message, and, for a
   spotlight phrase rated >= 4 with scoring enabled and `mTrack` (0x30c) non-null,
   `CommonPhraseCapturer::HandleVocalPhrase(this, mTrack->mTrackConfig.TrackNum(), id, hit)`
   (0x826EA044..0x826EA06C; `0x822740C8` is the ICF-folded `TrackNum` getter on `+0x28`).
   **Native skipped that whole tail** under `#ifdef HX_NATIVE` (sentinel mTrack, no
   VocalTrackDir, no net session).
3. `HandleVocalPhrase` -> `HitLastGem` -> `LocalHitLastGem` -> `OneTrackCompletedPhrase` ->
   `Player::CompleteCommonPhrase(false,false)`: retail 0x826A54A0 reads `mPhraseBonus`
   (`+0x2b0`), takes `mParams->mSpotlightPhrase` (`+0x14`) else 0.0, `AddEnergy`
   (0x826A53D8), then `mOverdrivePhrasesCompleted++`. Our source matches; no native guard here.
   `AddEnergy` -> `SetEnergy` needs `IsLocal() && unk2b0` (`mEnableOverdrive`).

So both skips were on the path, and either alone keeps energy at 0. Both are right for the
headless vocal harnesses (`main_vocal*`, which have no BandUser and install a sentinel
mTrack) and wrong for rb3-app, which has the BandUser, `TheGameConfig` and a real VocalTrack
since scorewalk's HookupTrack fix. Same shape as that fix: both guards are now
`HX_NATIVE && !RB3_NATIVE_APP`, so rb3-app takes retail's `#else` arm and the harnesses keep
theirs. The X360 arm is unchanged. Commit `82222a16d`.

Not on the path, checked and unchanged: `SongDB::GetCommonPhraseID` (native-guarded only in
the gem-index bound, which serves GetPhraseID), `CommonPhraseCapturer` (native guards are
`GetTrackPanel()` null checks around unison render leaves), `Player::CompleteCommonPhrase`.

## Runs

Binaries: baseline is scorewalk's bin-fix2 `66097bfe1b7a` (scorewalk HEAD's source minus its
doc commit), new is `~/tmp/vocalod/bin-od1` `766bcd3ded45` (this branch at `82222a16d`).
Summaries: `~/tmp/vocalod/scripts/energy.py` (frames where p0 energy/deploy change) and
`scripts/fielddiff.py` (first differing frame per detwalk field).

Predictions before O1: energy steps by the configured award at each hit OD phrase; scores
unchanged (no deploy, so no multiplier change); no new FAIL.

| run | binary | frames (540 s cap) | energy steps (frame: value) | score thecon / iwannabesedated |
|---|---|---|---|---|
| A2 (scorewalk) | bin-fix2 | 60,325 | none: 0.0000 throughout | 23,600 (31/31) / 32,400 (40/40) |
| O1 | bin-od1 | 36,570 | 2116: .251, 2613: .502, 3477: .753, 3802: 1.0; reset 8941; 10039: .251, 10490: .502, 10917: .753, 11806: 1.0 | 23,600 (31/31) / 32,400 (40/40) |
| O2 | bin-od1 | 51,171 | same 10 change frames and values as O1 | 23,600 (31/31) / 32,400 (40/40) |

**O2 = O1: IDENTICAL over all 36,570 common frames** (full record, `state`/`chain` included;
last common line `frame 36570 state eb844210 chain f8ac5d1f96f8f495`). Against A2, O2 diverges on
the same frames as O1 (full record 2116; `beat` only at 14229; `rand` from 17628), 0 FAIL lines.
A2 = B2 (scorewalk) and O1 = O2, so every A2/O1 difference comes from the binary change.

- **The step is the configured award.** 0.2510 each, which is `scoring.dta`
  `(spotlight_phrase 0.251)` (read in the Deluxe ark copy's `config/scoring.dta:983`),
  `Player::mSpotlightPhrase`. Energy reaches 1.0 after four OD phrases in each song and
  stays there (`AddEnergy` clamps with `std::min(1.0f, ...)`), so the other 3 and 4 OD
  phrases add nothing. It resets to 0 when the second song starts (frame 8941).
- **Deploy stays 0.** Not a bug I can show: a vocalist deploys only from
  `VocalPlayer::Poll` when a freestyle section's mic energy passes
  `GetFreestyleDeploymentRequiredMs`, or from Select in a freestyle section
  (`VocalPlayer.cpp` near 930 and 1700). Autoplay sings chart pitch and presses nothing. I did not
  check retail's autoplay deploy on Xenia.
- O1 runs fewer frames than A2 because ab_measure's leg-B build shared the CPU during it; the
  cap is wall-clock, so compare the common prefix.
- O1 vs A2 on the full record (`tools/rb3_app_det_cmp.py`): **first divergence at frame
  2116**, the frame of the first award, and only `energy` (and so `state`/`chain`) differs
  there. On the detwalk fields alone (`--base-fields`, `fielddiff.py`): `seconds`, `ui`,
  `screen`, `transition` identical over all 36,570 common frames; `rand` identical up to
  frame 17,627 and different from 17,628 on, which is ~3,500 frames into idling on the second
  song's `coop_endgame_screen`; `beat` differs on one frame only, 14,229 (the
  `coop_endgame_screen` arrival: 445.2703 vs 445.2704, `%.4f` print). So the award changes
  nothing the game draws from the RNG during either song. I did not find which endgame
  consumer draws differently at 17,628 (likely candidates are the endgame screen's use of
  the player's stats, whose `mOverdrivePhrasesCompleted` now counts every hit OD phrase
  (incremented even when energy is already clamped at 1.0), but that is not established).
- FAIL lines: 0 in O1 (A2: 0). The `WARN` line multiset is identical between A2 and O1
  (64 each, `sort | uniq -c` diff empty).

## A/B

`tools/ab_measure.py --worktree ~/tmp/wt-vocalod --revert 82222a16d`
(log `~/tmp/vocalod/ab_od.log`, result under `.ab_measure_runs/20261123-061522-82222a16d-*`):
Δmatched +0, Δmasked_equal +0, Δhonest +0, Δcode% +0.000000pp, Δcode_bytes +0, Δfuzzy +0,
units at 100% 635 -> 635 (mpn) and 548 -> 548 (all-rows-fuzzy); leg B recompiled 1,275 objs
(the revert touches two sources) and settled in 2 iterations; rc 0. Predicted Δ0: only
`HX_NATIVE` arms changed, and the match build never defines it.

## Excluded areas

The fix touches only `src/band3/game/VocalPart.cpp` and `VocalPlayer.cpp`. The newly taken
rb3-app arm now calls into areas outside this lane's fix scope, unchanged and not inspected
beyond "no FAIL, no new WARN": `VocalTrack::OnPhraseComplete` -> `BuildPhrase` and
`LocalScorePhrase` -> `VocalTrackDir` phrase feedback / vocal meters (bandtrack render side,
which reaches Rnd/Text objects), the `send_score_phrase` / `send_vocal_phrase_over` /
`send_hit_last_unison_gem` messages (net sync), and `TrackPanel` unison leaves in
CommonPhraseCapturer (ui). No Rnd/Draw/Text/Spotlight, ui, char or net sync/game-start code
was edited.

## Not done

- `tools/native_health.sh --selftest` not run, as briefed. Only rb3-app was built; the
  headless vocal harnesses (`main_vocal*`) keep the old arm by construction (they do not
  define `RB3_NATIVE_APP`) but were not rebuilt or run.
- No ASan/UBSan walk of the new arm (scorewalk's S2 covered the old one).
- No retail comparison of autoplay deploy, and the endgame RNG consumer at frame 17,628 is
  not identified.
- No harmony walk: the walk plays solo vocals; HARM2 carries no OD.
- Not landed; nothing pushed.
