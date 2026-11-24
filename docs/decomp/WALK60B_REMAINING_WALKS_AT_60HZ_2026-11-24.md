# WALK60B: the remaining rb3-app walk suites at 60 Hz, seed remaps (2026-11-24)

Lane walk60b, branch `walk60b` (from main `f25eee490`, the walk60 merge), worktree
`~/tmp/wt-walk60b`. Follows `WALK60_RB3APP_WALKS_AT_60HZ_2026-11-23.md` (its "Not done": codawalk,
deploywalk, chartplay2/3, harmwalk, menuwalk, modwalk, tourwalk2/3, trainwalk2 not walked at
60 Hz, seeds not remapped). Scratch and logs: `~/tmp/walk60b/<suite>/r_<tag>/run.log`; helper
scripts and the seed tables are committed in `tools/rb3_app_walks/walk60b/`.

Binary: `bin-m0` = walk60's `bin-w1` (sha256 `c4e9ff7195be`): main's `native/` is identical to
walk60's tip `ea4211f6a` (`git diff --stat walk60 main -- native` is empty), so it is main's
native code. Every run went through `coord/no610m.sh` under the shared `gpu1.lock`, at
`RB3_APP_HZ=60`, deterministic, with the converted (walk60) scripts. All rc 0.

**Host load bounded this lane.** The box ran at load ~150 on 32 cores (other lanes building
and rendering), so a run advanced about 11 frames/s: an 18,000-frame coda leg took about 25 min
and the 128,000-frame tour leg would have taken about 3 h. What was not run, or was stopped, is
listed under "Not done" with how to run it.

## 1. Harness change: `A` is overridable in every suite runb.sh

The ten suites' `runb.sh` hard-coded `A=/home/free/tmp/<suite>` (the original lane's scratch dir)
and `rm -rf $A/r_<tag>` per run, so a re-run with a record tag would overwrite the record's logs.
Each now reads `A=${A:-/home/free/tmp/<suite>}` (default unchanged). The suites' own `runs*.sh`
`cd` into the old lanes' scratch copies (legacy scripts), so this lane drove the converted scripts
with `walk60b/r.sh <suite> <tag> <walk> <legacy cap> [env...]` (`@W@` = the converted script dir,
for `RB3_APP_EXTRA_PADS`).

## 2. Seeds: the record songs and where they are at 60 Hz

The song each record run played, read from the original lanes' surviving run logs (first
`songs/<name>/` loaded; all legacy runs loaded it at legacy frame ~1010, pro guitar ~1180,
menuwalk setlist 1431, modwalk 2540):

| suite / legs | pad kind | record song (legacy seed) | 60 Hz seed |
|---|---|---|---|
| codawalk c1/c2G*/c3 | guitar, walk_x | ohmygod (18) | **41** |
| codawalk survey k1s1/2/3/5/6 | guitar, walk_x | llama, beastandtheharlot, crazytrain, thelook, midlifecrisis (1, 2, 3, 5, 6) | **6, 8, 55, 13, 26** |
| codawalk c1DM | drums, walk_x | ohmygod (18) | not found (drums seeds 0-6 checked) |
| codawalk c2U*, deploywalk F*/b*/s*, harmwalk b0 | band, 4 pads | oyemiamor (0) | not in 28-39 (walk60: not in 0-27 of the gate walk) |
| deploywalk g* | guitar, walk_od | thisbastardslife (0) | **not in 0-63** (guitar table) |
| deploywalk d* | drums | lowrider (0) | not in 0-6 |
| deploywalk v*, harmwalk v0 / v1 | vocals | lastdance (0) / oyemiamor (1) | not in 0-6 |
| chartplay2 keys/prokeys | keys | yoshimibattles (0) | not in 0-6 |
| chartplay3 bXP* | keys | goodvibrationslive (0) | not in 0-6 |
| chartplay2 pro guitar | proguitar | stopmeifyouthink (0) | not in 0-5 |
| chartplay3 pro guitar/bass, set 3 | proguitar | inthemeantime (0), coldasice (3) | not in 0-5 |
| modwalk base/mods/indep | band | somethingbigger, falsealarm, crosstowntraffic (0) | not remapped (see 3.4) |
| menuwalk w_setlist | vocals | 20thcenturyboy (0) | not remapped (lose at 6 misses either way) |

Prediction (from walk60 section 2): the pick moves with the walk and the pad kind. **Refuted in
part, measured:** it moves with the **pad kind**, not the walk. The 60 Hz songs for seeds 0-6 are
equal between codawalk `walk_x.txt` (guitar) and deploywalk `walk_od.txt` (guitar), and for
seeds 1-5 between deploywalk `walk_od.txt` (vocals) and harmwalk `walk_harm.txt` (vocals)
(`walk60b/seeds60.txt`). So one table per pad kind serves every suite, and the guitar table
(seeds 0-63, complete) is the one this lane could finish. The tables are not uniform
(lowrider 4 times and rocklobster 3 times in 64 guitar seeds; thisbastardslife never appears),
so a song can need well over 64 seeds. The seed only moves the boot clock that seeds the game
RNG (`rb3_app_determinism.cpp`), so there is no shortcut short of modelling the RNG draws up to
song select.

**Committed remaps:** codawalk `coda.sh` (default seed 18 -> 41), `legs2.sh`, `legs3.sh`
(`RB3_APP_SEED=41`), and `survey.sh` now carries a note mapping the record's seeds to 6/8/55/13/26.
coda.sh's DM (drums) leg at seed 41 plays the drums table's seed-41 song, not ohmygod.

## 3. 60 Hz vs record

### 3.1 codawalk (seed 41 = ohmygod; record seed 18; `cf.py`, `gsum.py`)

| leg | record (legacy) | 60 Hz |
|---|---|---|
| c1GC chartplay | coda pts 2,749, `coda 1/1/0 bcoda 1/1`, 211,092 -> 213,841 (f7217) | 2,752, `1/1/0` `1/1`, **211,093 -> 213,845** (f14132, beat 552.045 vs 552.077) |
| c1GB blow | 0 pts, `coda 1/0/1 bcoda 1/2`, 211,092 | 0 pts, `1/0/1` `1/2`, 211,093 |
| c1GA autoplay | 921 pts, won, 212,013 | 926, won, 212,018 |
| c1GM mash | 922 pts, won, 212,014 | **884**, won, 211,977 |

The same song plays through the same coda; the outcome flags (`coda` hit/won/blown, `bcoda`) are
identical in all four. The base-score +1 (211,093 vs 211,092) is walk60's ±1: a sustain's
points accrue per poll, so its fraction at the last poll moves with the rate. The coda point
totals move by 3 (GC) and 5 (GA). Their mechanism was not checked against retail bytes; a
per-poll accrual like the sustain's would fit.

**c1GM -38 is a harness defect, fixed:** `rb3_app_chartplay.cpp`'s `CODA=mash` strums every third
*poll* (`ph % 3 == 2`) and on drums hits one pad per poll. At 60 Hz that is 20 strums/s instead
of 10, so the 60 Hz leg fed the coda different input than the record's. This is the same class of
defect walk60 fixed for deploy/tilt spacing. Fix: the mash phase now counts 30ths of a second
(`ph = polls / (PollsPerSecond()/30)`, a press only on the first poll of each phase), so a legacy
run is unchanged (step 1) and 60 Hz strums 10 times a second. **Not yet built or re-run** (see
"Not done"); the prediction for the re-run is c1GM ≈ 922 ± a few points.

### 3.2 menuwalk, trainwalk2, tourwalk2 losequit (song-independent; legacy run = the record's log)

Every leg walks the same settled screens in the same order as its record run. Times compare as
virtual time (60 Hz frames / 2):

- menuwalk calib (8,239 of 10,000): ... main_hub -> meta_loading_calibration -> cal_welcome ->
  cal_manual_audio -> cal_manual_video -> cal_manual_complete -> main_hub, as recorded.
- practice (9,490): song_select -> part_difficulty -> practice_sel_section -> practice_game.
- setlist (8,362): song_select -> part_difficulty -> game -> lose, p0 `0/0/6` in both (the
  vocal pad is not played; 60 Hz song not remapped, the lose comes at 6 misses either way).
- hub1: hint_career -> accomplishment; hub3: manage_band. FAIL 0, TIMEOUT 0 in all five.
- trainwalk2 WR (5,200 = cap): training -> basic_confirm_dialog_small x3, as recorded.
- CP1 (9,560 of 14,000): lesson_finished x3 so far. First lesson drum_trainer -> lesson_finished
  2044 frames = 34.07 s (legacy 1021 = 34.03 s). Later lessons play 1703 frames = 28.38 s
  (legacy 852 = 28.40 s), but lesson_finished -> gem_preview is 75 frames = 1.25 s (legacy 45 =
  1.50 s), so lesson to lesson is 32.67 s vs 32.97 s. That gap is a menu dwell between a
  `wait_screen` and the script's next press, so it is likely the converted script's own offset
  (not investigated). The screens and lesson count are the record's.
- SP (9,223 of 18,000): the speed lesson lasts 4950 frames = 82.50 s (legacy 2474 = 82.47 s).
- tourwalk2 losequit (7,993 of 18,000): game -> lose -> lose_confirm_quit ->
  meta_loading_continue -> tour_welcome, p0 `0/0/27` both; game -> lose 2369 frames = 39.5 s
  (legacy 1186 = 39.5 s). Same song (iloverocknroll, the fixed tour row).

CP1's 4th lesson and losequit's return to the gig guide were past the frame read; the legs were
still running at write-up.

### 3.3 First-boot timing

Every 60 Hz leg reaches `first_time_calibration` at frame 110 (legacy 80) and `main_hub` at 855
(legacy 505): 1.83 s vs 2.67 s and 14.25 s vs 16.8 s of virtual time, so the 60 Hz boot reaches
the hub 2.6 s sooner. Not investigated: both sides pass the same screens, and every later
comparison here is taken as an interval from a screen, not from boot.

### 3.4 modwalk (seed 0, songs differ)

At 60 Hz seed 0 the three walks reach song_select at 4247 (legacy 2208 = 4416 at 60) and game
at ~5067. The songs are not the record's: the toggle presses draw from the RNG, and modwalk was
not remapped. All three were mid-song at write-up (base f6604: p3 bass unplayed 0/0/23, crowd
falling as in the record's lose path; mods f7479, indep f7156, both bass unplayed), each with
the 3 NetSession FAILs walk60 attributes to guest joins. The record's song-independent outcomes
(`nofail`/`fills`/hud flags, view times, base -> lose_screen vs mods/indep -> coop_endgame) are
in each log's `RB3_APP_MOD_LOG` lines once the songs end.

## 4. Native bugs

None in the game. One harness defect, fixed and not yet verified: the chartplay mash driver's
poll-counted strum spacing (section 3.1). Every same-song comparison (codawalk x4, losequit) shows
the record's flags, hits and misses; the scores move by the ±1-5 per-poll sustain fraction
walk60 explained from retail `BandPerformer::ComputePoints` 0x826ED8E0. No new retail-byte
question came up: the mash difference is input, not game logic.

X360 match build: no `src/` file changed (`native/src/rb3_app_chartplay.cpp` is native-only), so
there is nothing to price with `tools/ab_measure.py`.

## Not done (handoff)

- **Build + verify the mash fix.** `~/tmp/walk60b/build.log` is the worktree's rb3-app build,
  started at write-up (`native/build`, nice 15, -j 8). When `build rc=0` appears, re-run c1GM:
  `BIN=/home/free/tmp/wt-walk60b/native/build/rb3-app ~/tmp/walk60b/r.sh codawalk c1GMf walk_x.txt 9000 RB3_APP_SEED=41 RB3_APP_DET_SCORE=1 RB3_APP_DET_OD=1 RB3_APP_JOYPAD_KIND=guitar RB3_APP_CHARTPLAY=1 RB3_APP_CHARTPLAY_LOG=1 RB3_APP_CHARTPLAY_CODA=mash`
  and read `python3 tools/rb3_app_walks/walk60b/cf.py ~/tmp/walk60b/codawalk/r_c1GMf/run.log | tail -3`
  (predicted ≈ 922 pts, won). Also c1GC should be unchanged (2,752).
- **Seed tables beyond the probed ranges** for drums, vocals, keys, proguitar and the band walk
  (`walk60b/p1.sh <class> <seed>` appends to `~/tmp/walk60b/probe.txt`; classes in `classes.txt`;
  the pool was still running seeds 7-15 / band 40-59 at write-up, so read `probe.txt`, which ends
  `PROBE_DONE`). Until a song is found, deploywalk, chartplay2/3 and harmwalk's song-dependent
  numbers (deploy counts, harmony part counts, pro scores) cannot be compared at 60 Hz.
- **Long legs stopped for load:** tourwalk2 full (64,000 legacy -> 128,000 frames; tourwalk3's
  full leg is the same walk) and loseres, trainwalk2 DC, RGPC and RKMB. Their commands are
  in `walk60b/legs.sh`. tourwalk3 custom* and the ASan/gdb legs were not started.
- modwalk's three legs, CP1, SP and losequit were left running to their caps; their logs keep
  growing in `~/tmp/walk60b/`.
- `tools/native_health.sh --selftest` left to the coordinator (brief: do not land).
