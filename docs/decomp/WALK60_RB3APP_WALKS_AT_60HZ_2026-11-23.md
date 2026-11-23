# WALK60: rb3-app walk suite migrated to retail's 60 Hz (2026-11-23)

Lane walk60, branch `walk60` (from main `82ff40429`), worktree `~/tmp/wt-walk60`. Scratch, logs
and binaries: `~/tmp/walk60/`. Every native run went through `coord/no610m.sh` under the shared
`gpu1.lock`. Background: `FRAMERATE_RB3APP_60HZ_2026-11-23.md` (harness runs default to legacy
30 Hz until both walk suites migrate; `RB3_APP_HZ=60` runs retail's rate).

Binaries: `bin-w0` `e1a413d728f8` (main `82ff40429`, built in the worktree), `bin-w1`
`c4e9ff7195be` (+ the chartplay driver change below). Every run rc 0 at its frame cap.

## 1. What changed

- **`tools/rb3_app_walks/hz60.py`** converts a walk or pad script from legacy 30 Hz frames to
  60 Hz frames: every `N`/`+N` (and a `highlight`'s start) doubles; a run of one button on
  consecutive frames (a hold, songselwalk's RB hold) stays a hold, `t..t+n-1 -> 2t..2(t+n)-1`;
  every `wait_screen` line is written twice. The last rule keeps the timeouts: a wait gives up
  after 1800 frames (the engine's `kWaitTimeoutFrames`; `rb3_app_joypad.cpp`'s 1800 polls for
  the extra pads), 60 s at 30 Hz but 30 s at 60 Hz, and a satisfied wait lets the next line run
  in the same frame (`GetScriptedButtons` `continue`s, as does `ExtraButtons`), so the copy is
  inert whenever the first wait is satisfied and the pair times out at the old 60 s when it is
  not. Comments are not rewritten (a comment's frame numbers are legacy). A converted file starts
  with a `# hz60:` mark and the tool refuses to convert it twice.
- **All 143 walk scripts** under `tools/rb3_app_walks/` except `framerate/` (whose scripts only
  wait on screens and are run at both rates on purpose) are converted in place. The legacy
  originals are main `82ff40429`'s.
- **Every `runb.sh`** (19, plus `movie/run.sh`) passes `RB3_APP_HZ=60` and doubles the frame cap
  and the `SECS`/`TMO` wall caps it is given, so the `runs*.sh`/`disc.sh` callers and the dated
  records keep their legacy numbers (a note in each runb.sh says so). An `RB3_APP_HZ=legacy` in
  the extra env still wins (it comes later on the env line), for a legacy leg, but the scripts are
  60 Hz now; a legacy leg uses main `82ff40429`'s scripts.
- The three generators (`optwalk/mk.py`, `songselwalk/mk.py`, `trainwalk2/gen.py`) still emit
  legacy frames; a note at the top of each says to regenerate from main `82ff40429`'s inputs and
  pipe the output through `hz60.py`.
- **`tools/rb3_app_walks/gate60/`**: the band gate walk, rb3-scout `a38/walk2.txt` and its
  `p1_guitar_2.txt`, `p2_drums_2.txt`, `p3_bass_2.txt` (first commit: verbatim copies; then
  converted), `gate.sh` (4 pads, det + autoplay, `RB3_APP_HZ=60`, seed 12, 44,000-frame cap),
  `runb.sh` (takes 60 Hz frames directly), `gsum.py` (screen sequence + per-song band/player
  scores), `atbeat.py` (scores at a song beat, a rate-independent position).
- **`native/src/rb3_app_chartplay.cpp`** (a native harness file; not in the X360 match build):
  the chart driver's own poll counts, deploy and tilt "at most once a second" (`>= 30` polls),
  the tilt hold (15 polls, 0.5 s) and the first poll's `dt` fallback (`1000/30`), are now
  counted in polls of the run's rate (`RB3AppHz()`, 30 under legacy, so a legacy run is
  unchanged). At 60 Hz they had halved in time: two deploy presses a second.

The harness default rate is not changed (the coordinator flips it with the peer).

## 2. Seeds: the song a seed picks moves with the rate and with the walk

Prediction: the converted scripts reach song select after the same virtual time but twice the
frames, and the menus draw from the game RNG per frame, so a seed picks a different song at
60 Hz. Measured (`probe.sh`, `lprobe.sh`, `cprobe.sh`, `lcprobe.sh` in `~/tmp/walk60/`; song =
first `songs/*.milo` loaded):

- The pick also depends on the walk and the pad kind: legacy seed 0 is `oyemiamor` in the band
  gate walk, `lastdance` in walk.txt with the default pad (framerate d0), `rehab` in chartplay
  walk_x with a guitar and `plush` with drums.
- **Band gate walk**: 60 Hz seeds 0-27 and legacy seeds 0-27 share six songs: beforeiforget
  (legacy 17 = 60 Hz 12, 13), meenamora (27 = 11), needyoutonight (13 = 14, 21), nooneknows
  (3 = 15), somethingbigger (2, 4 = 3, 9), thelook (19 = 23). Legacy's seed-0 song `oyemiamor` is
  not among 60 Hz seeds 0-27, so **gate60 uses seed 12 (beforeiforget; legacy 17)**. The
  dropwalk/modwalk/deploywalk band walks share the gate walk's boot (seed 0 = oyemiamor legacy,
  spaceoddity at 60 Hz in dropwalk's w_pause, as in the gate probe), so the same table applies
  to them as far as song 1 goes.
- **chartplay walk_x, guitar**: legacy 1-11 vs 60 Hz 0-11 share llama (legacy 1 = 60 Hz 6) and
  beastandtheharlot (legacy 2 = 60 Hz 8; with the framerate lane's unconverted walk_x it was
  60 Hz seed 10, so the conversion moved it).
- Not remapped (time): drums kind, micwalk2's vocal songs, codawalk's seed 18, practicewalk's
  and optwalk's seed 0. Their 60 Hz runs play a different song than their records.

## 3. Legacy vs 60 Hz (bin-w1 both legs unless noted)

### Band gate walk (gate60; GL17 = legacy a38 scripts, seed 17; G60 = converted, seed 12)

| | legacy GL17 | 60 Hz G60 |
|---|---|---|
| song 1 | beforeiforget | beforeiforget |
| band score / stars at song end | 156,463 / 4.1416 | **156,462** / 4.1416 |
| vocals / guitar / drums / bass score | 42,000 / 28,962 / 48,475 / 37,025 | **identical** |
| hits (misses) | 54 / 292 / 499 / 271 (0) | **identical** |
| player stars | 5.0 / 5.0 / 4.9984 / 5.0 | **identical** |
| song 1 length | game 1116 -> popups 9678 = 285.4 s | game 2008 -> popups 19125 = 285.3 s |
| flow after song 1 | popups, endgame, meta_loading_continue, **campaign_rewardvignette_firstgig**, accomplishments_newaward, song_select, part_difficulty, game | **same sequence**; vignette 25.7 s (legacy 26.0 s) |
| song 2 | walkoflife, played to coop_endgame (19,885) | oyemiamor (the song after song 1 differs: song 1's frame count changes the RNG); at frame 40,000 it is 279 s in: band 170,122 = legacy GL's (seed 0) full oyemiamor song 1, every player identical |
| FAIL lines | 3 (`NetSession.cpp Line: 763 IsUserSignedIn`, one per guest join) | the same 3 |

The only message kind in one log and not the other, apart from song-specific loads, is
`Achievement awarded: acc_percussionperfect`: legacy awards it at the end of song 2 (walkoflife),
in both legacy runs (seed 0 GL and seed 17 GL17). It is not a rate effect: in legacy GL the same
award is absent after oyemiamor (song 1), and G60's song 2 is oyemiamor; G60b (below) plays it to
the end.

The 1-point band difference: retail `BandPerformer::ComputePoints` 0x826ED8E0 sums each active,
unquarantined, non-null user's `GetScore()` as an int (`add r29,r3,r29` at +0x78) and stores it
as a float (`extsw`/`fcfid`/`frsp`/`stfs 0x218`), as our source does. Each player's `GetScore()`
truncates `mScore + 0.01` (Performer.cpp), and a sustain's points accrue per poll, so a player's
fraction at the last poll moves with the poll rate; the per-player integers printed are equal
here, and the sum differs by one through a fraction the print does not show. Same size and cause
as the framerate lane's ±1 (LN0 vs FN0). Not a native defect.

TIMEOUT lines rise from 23 (legacy, 20,000 frames) to 51 (60 Hz, 40,000): every timed-out wait in
a chain is now two, as predicted.

**G60b** (bin-w1, default 44,000-frame cap): repeat of G60, IDENTICAL to it over their common
frames: **IDENTICAL over 40,000 common frames** (`tools/rb3_app_det_cmp.py`).

### chartplay expert guitar, tilt (beastandtheharlot; cLGT legacy seed 2, c6GT 60 Hz seed 8)

`RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_TILT=1`, p0 at song beats (`gate60/atbeat.py`):

| beat | legacy hits / score / energy | 60 Hz hits / score / energy |
|---|---|---|
| 50 | 38 / 3,678 / 0.3782 | 38 / 3,697 / 0.3648 |
| 100 | 100 / 13,471 / 0.2510 | 100 / 13,473 / 0.2510 |
| 200 | 378 / 56,303 / 0.3460 | 377 / 56,225 / 0.3311 (sampled 0.05 beat earlier) |
| 300 | 597 / 90,002 / 0.2510 | 597 / 90,145 / 0.2510 |

0 misses in all. Same hits; the scores and energy differ because a deploy starts 13 polls after
the tilt in both, which is 400 ms at 30 Hz and 200 ms at 60 Hz (the retail filter,
`num_frames 12`, framerate section 5): first tilt at song 17,470 ms (legacy) / 17,472 ms (60 Hz).
That is the retail rate's behaviour, not a defect. c6GT0 (bin-w0, before the driver change) is
identical to c6GT at every beat read: on this song the energy, not the driver's 30-poll spacing,
limits how often it deploys, so the change is inert here (it matters when a deploy could follow
within the second).

### Menu and mode walks (pair.sh: legacy script + `RB3_APP_HZ=legacy` cap F vs converted script at 60 Hz cap 2F)

| walk | env | settled screens, legacy = 60 Hz | last screen | results |
|---|---|---|---|---|
| practicewalk w_full | drums, chartplay, focus log | 17 = 17, same order | practice_game (legacy frame 1895, 60 Hz 3525) | different songs (563 vs 815 gems; seed not remapped); 0 FAIL both |
| trainwalk w_trg | proguitar | 17 = 17 | rg_trainer (1883 / 3559) | 3 vs 4 misses at the cap: the 60 Hz leg has 3.5 s more lesson time before its cap (cap counted from boot, the entry 1.7 s earlier) |
| optwalk w_opt | options log | 15 = 15 | game (2886 / 5568) | different songs (seed 0) |
| songselwalk w_sorts | songsel log | 13 = 13 | song_select (678 / 1188) | - |
| tourwalk w_tour | focus log | 19 = 19 | coop_endgame (7930 / 15580) | same song (iloverocknroll), 50 misses both (pad 0 not autoplayed) |
| dropwalk w_pause | 4 pads, autoplay skip 3 | 16 = 16 | lose (3568 / 9001) | different songs (oyemiamor / spaceoddity, seed 0); the unplayed bass fails the band after 27 misses in both |

All rc 0, 0 FAIL (dropwalk: the 3 NetSession lines in both legs), no wait TIMEOUT in the menu
walks except tourwalk (3 legacy, 6 at 60 Hz: the doubled chain).

## 4. Native bugs found at 60 Hz

None in the game: every walk reached the same screens at 60 Hz, and where the song was the same
the hits, misses and per-player scores were identical (band ±1, explained above). One harness
defect, fixed: the chartplay driver's per-poll spacing and hold (section 1).

Checked and not firing at either rate: UI.cpp's native `sEnterWaitFrames > 90` enter-animation
safety net (3 s at 30 Hz, 1.5 s at 60 Hz) printed no `Enter animation timeout` in any run (the
framerate runs d0, d2, c2S1, c2N0 nor this lane's). It remains a native frame count that halves
at 60 Hz (not edited: no run needs it). The sung-mic emulation is time-based (samples from song
ms, `rb3_app_mic_native.cpp`), so it is rate-independent.

**X360 match build**: no `src/` file changed, so nothing to price with `tools/ab_measure.py`
(`native/src/rb3_app_chartplay.cpp` is native-only).

## 5. Gate song 2 to its end (G60b, 44,000 frames, 250.8 s, rc 0)

G60b plays song 2 (oyemiamor) to the end: band 170,122, every player's score, hits and stars
equal to legacy GL's oyemiamor (seed 0, song 1), 0 misses. Then coop_endgame 40,660,
meta_loading_continue 40,963, **song_select 41,192**: the walk's end state, as legacy reaches
song_select at 19,258 (GL) after its song 2. Legacy goes through coop_endgame_popups and
accomplishments_newaward after song 2 because walkoflife earns `acc_percussionperfect`; G60b's
oyemiamor earns none (as in legacy GL after oyemiamor), so meta_loading_continue goes straight to
song_select, the path rb3-scout a38's handoff describes for a walk with no new award. 3 FAIL
(the NetSession lines), 51 TIMEOUT.

Helper scripts for every run in this record: `tools/rb3_app_walks/walk60/` (copy into
`~/tmp/walk60/`; they `cd` there and expect `bin-w1` and `legacy/<dir>/` = main `82ff40429`'s
scripts).

## Not done

- Runs for codawalk, deploywalk, micwalk2, chartplay2/3, harmwalk, menuwalk, modwalk, movie,
  tourwalk2/3, trainwalk2: their scripts are converted and their runb.sh migrated but they were
  not walked at 60 Hz in this lane (time); the six walks above and the gate are the evidence for
  the converter.
- Seed remaps beyond the gate (seed 12) and chartplay expert guitar (seed 8 = legacy 2): the
  runs*.sh files still pass their legacy seeds, which pick other songs at 60 Hz.
- Song 2 of the gate walk is a different song at 60 Hz; no pair of seeds was searched that keeps
  both songs.
- `tools/native_health.sh --selftest` left to the coordinator (brief).
