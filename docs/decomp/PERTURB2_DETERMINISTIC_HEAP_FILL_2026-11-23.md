# PERTURB2: heap-fill walks on the deterministic rb3-app (2026-11-23)

Lane perturb2, branch `perturb2`, worktree `~/tmp/wt-perturb2`, base main `d1e64a303`.
Scratch, scripts and logs: `~/tmp/perturb2/`.

## Goal

Lane perturb (`PERTURB_UNINIT_HEAP_READS_2026-11-23.md`) could not use the rb3-app walk as a
diff instrument because the walk was not reproducible (§3.1 there). Lane detwalk
(`DETWALK_DETERMINISTIC_RB3APP_WALK_2026-11-23.md`) made it frame-exact under
`RB3_APP_DETERMINISTIC=1`. This lane runs the a25 walk in deterministic mode with default malloc
and with `GLIBC_TUNABLES=glibc.malloc.perturb=190` (malloc fill 0x41, free fill 0xBE) and `=65`
(malloc fill 0xBE, free fill 0x41), for several `RB3_APP_SEED` values. The plan was to find, for
each first divergent frame, the uninitialised member behind it and check retail's constructor.

**Result: no divergence.** Five seeds were run under three malloc arms each, and every perturb
run agrees with its seed's default run at every common frame. That holds on the per-frame state
hash and on the whole main-thread log. A positive control (one forced uninit byte) diverges at
frame 1 under both fills, so the instrument can see such a byte. No source member was changed,
so there is nothing to price with `ab_measure`.

## Method

- **Binaries.** Built in this worktree (`native/build`, clang, target `rb3-app` only) and
  copied out before any run, so a rebuild could not change a running one.
  - `bin-tip`: main `d1e64a303`, sha256 `8b670c00b381…`.
  - `bin-ctl`: the same tree with FileMerger.h's two `HX_NATIVE` `mForceReload` inits turned
    to `#if 0` (the ctor's `mForceReload(0)` and the copy ctor's `mForceReload = false`),
    sha256 `dd9663b2cc3b…`. The header was restored right after the build, and
    `git status` is clean apart from this lane's files.
- **Walk.** `~/tmp/perturb2/runb.sh` is a copy of `a25/runb.sh` with `A=~/tmp/perturb2` and
  `X=~/tmp/wt-perturb2`. It gives each run a fresh reflink copy of `memcard/content_a`, the
  3090 ICD under the shared `gpu1.lock`, and `coord/no610m.sh`.
  `~/tmp/perturb2/wave.sh <bin> <tag> <seed> <d|p190|p65>` runs
  `BIN=<bin> SECS=540 TMO=580 runb.sh <tag> a25/walk.txt 90000 RB3_APP_DETERMINISTIC=1
  RB3_APP_SEED=<seed> [GLIBC_TUNABLES=…]`. Every run ended rc 4, which is rb3-app's
  wall-clock-cap exit (`RESULT: APP RAN N FRAMES … (wall-clock cap) (540.0 s)`), the same as
  detwalk's run A. Up to 19 walks ran at once at load average 55-90, so a run covers about
  29,000-40,000 frames in 540 s. Determinism does not depend on speed, so the comparison is
  over the common frames.
- **That the fill reaches the binary through this wrapper** was shown by lane perturb's §1 control
  (`ctl/m`: `00 00` default, `41 41` p190, `be be` p65, all through `no610m.sh timeout`). This
  lane's positive control below shows it again, end to end, on a game object.
- **Two comparators:**
  1. `tools/rb3_app_det_cmp.py` (detwalk) compares the per-frame
     `rb3-app-det: frame N state H chain C rand … seconds … beat … ui … screen … transition …`
     lines, which cover the RNG, the three TaskMgr clocks and the screen names.
  2. `tools/rb3_app_log_cmp.py` (new, this lane) is an ordered diff of the **whole** run log
     up to the last common frame. Hex pointers and wall-clock suffixes are normalised. It drops
     two line classes that differ between two default-malloc runs of one binary (t0_d vs t0_d2):
     `=== BONE DIAG` blocks (render-side pose dumps: which mesh is sampled at "frame 1000", and
     third-decimal pose values) and StandardStream's `timer error is large` (it reads
     `GetRawTime()`, the audio thread's real sample clock). The d-vs-d2 baseline gave 17 hunks
     unfiltered, all of those two classes, and 0 after the filter. Exit 0 means no hunks.

Prediction before the runs: lane perturb found two retail-faithful heap reads with memcheck,
already zeroed, and no perturb-visible behaviour, so most likely no divergence on the tip. Any
divergence would most likely come from a never-stored bool or flag read during the
FileMergerOrganizer or ThreadCall load paths.

## Positive control

| pair | det_cmp | log_cmp |
|---|---|---|
| t0_d (tip, default) vs c0_d (ctl, default) | IDENTICAL over 38,311 frames | 0 hunks |
| t0_d vs c0_p190 (ctl, p190) | **FIRST DIVERGENCE at frame 1** | 266 hunks |
| t0_d vs c0_p65 (ctl, p65) | **FIRST DIVERGENCE at frame 1** | 266 hunks |
| c0_p190 vs c0_p65 | IDENTICAL over 36,880 frames | 0 hunks |

- **Frame 1.** It reads `rand 106/209` on the tip and `rand 110/213` on ctl under either fill.
  Those are the four extra `FileMergerOrganizer::StartLoad` RandomInt draws detwalk's sabotage
  run S showed. The song then changes (thecon → imagine) and game_screen moves from frame 1,119
  to frame 1,241.
- **Default malloc.** Without the init the byte reads 0, which is what the init writes, so ctl
  matches the tip. That is the expected negative.
- **Why p190 and p65 agree.** 0x41 and 0xBE are both a true `bool`, so both fills set the
  reload.

So a single never-stored heap byte on a hot path is visible at once to both comparators, under
both fills, and a run that does not read a fill is not mistaken for a divergence.

## Runs and verdicts

All runs use `bin-tip`. The frame counts are each run's own count at the 540 s cap. The
comparisons cover the common frames (the shorter run of the pair).

| seed | song | game_screen / 1st coop_endgame / exit / 2nd coop_endgame (frame) | frames d / p190 / p65 | d vs p190 | d vs p65 |
|---|---|---|---|---|---|
| 0 | thecon | 1,119 / 8,009 / 8,159 / 15,793 | 39,759 / 39,771 / 39,536 | IDENTICAL, 0 hunks | IDENTICAL, 0 hunks |
| 1 | combatbaby | 1,242 / 8,082 / 8,232 / 15,606 | 40,066 / 39,917 / 39,855 | IDENTICAL, 0 hunks | IDENTICAL, 0 hunks |
| 2 | everybodywantstorule | 1,242 / 9,340 / 9,491 / 18,949 | 36,761 / 36,870 / 36,712 | IDENTICAL, 0 hunks | IDENTICAL, 0 hunks |
| 3 | combatbaby | 1,242 / 8,062 / 8,212 / 14,637 | 59,371 / 59,116 / 58,923 | IDENTICAL, 0 hunks | IDENTICAL, 0 hunks |
| 4 | smokeonthewater | 1,138 / 12,205 / 12,356 / 21,435 | 59,321 / 59,380 / 59,375 | IDENTICAL, 0 hunks | IDENTICAL, 0 hunks |
| 0, second default run (t0_d2) | thecon | same frames | 37,802 | — | d vs d2: IDENTICAL, 0 hunks |

- **Milestones.** Within a seed they are identical across the three arms. The "IDENTICAL"
  column is `rb3_app_det_cmp.py` and the "hunks" column is `rb3_app_log_cmp.py`.
- **Songs.** Seeds 1 and 3 both pick combatbaby, but the walks differ (endgame at frame 8,082
  against 8,062). So across the seeds the RNG takes five distinct paths through song_select and
  through the second song's pick.
- **Unfiltered (`RAW=1`).** The perturb pairs have 1-4 hunks each, against 17 for d vs d2. Every
  line in them is a BONE DIAG row (mesh header, bone list, `localRot/localPos/worldRot/worldPos/offset/skin`)
  or a `timer error is large` value. Diffs: `~/tmp/perturb2/raw_<a>_<b>.txt`.
- **Exit status.** All 19 runs ended rc 4 (the wall-clock cap). There were no SIGSEGV, ASSERT,
  `RING CORRUPTION` or `FAIL:` lines (`~/tmp/perturb2/tools/summ.py`).

| # | divergence | first frame | member | retail ctor | verdict |
|---|---|---|---|---|---|
| — | none in 10 tip perturb pairs (5 seeds × 2 fills), 36,700-59,400 common frames each | — | — | — | no uninit-heap read on the walk path changes RNG, clocks, screen flow or a main-thread log line |
| ctl | `FileMerger::Merger::mForceReload` with its two HX_NATIVE inits removed | 1 | `Merger` +0x29 | `0x8232DCF8` stores +0x28 and +0x2A, not +0x29 (FileMerger.h comment, FORCERELOAD record) | positive control; the init is already on main

## Limits (what this instrument cannot see)

- **What the fill reaches.** glibc perturb fills `malloc` returns and freed blocks. It does not
  fill `calloc`, the stack, statics, or memory reused inside a native pool without going back
  through `malloc`/`free`. Stack reads are the memcheck lanes' ground (perturb §4 row 10,
  stackuninit).
- **What the comparators see.** A read is caught only if it changes the RNG draw count, a
  TaskMgr clock, the screen flow, or a main-thread log line. A member that changes only what is
  drawn (Rnd, Draw, Text, Spotlight, char poses), or an unlogged value such as a score, is
  invisible. The walk plays with no instrument input, so score paths are barely exercised.
- **BONE DIAG filter.** It also hides any fill dependence in the char pose dumps. That area is
  outside this lane's fix scope, and the dumps already differ between two default runs.
- **Coverage.** The walk covers: boot, calibration, main_hub, song_select, one song,
  coop_endgame, the exit to meta_loading_continue, song_select, part_difficulty, a second song
  and the second coop_endgame (the walk's presses end there).

## Out-of-scope areas (Rnd/Draw/Text/Spotlight, ui, char, net sync/game-start)

No divergence was found in any of them, so there is nothing to list with a proposed fix.

Lane perturb's endgame-exit SIGSEGV (§3.4 there: `CharClipGroup::Replace` under `~CharClip`
`ReplaceRefs`, 2/2 under p190 and 1/3 with default malloc in non-deterministic walks) **did not
reproduce here.** Every tip run went through the same exit (coop_endgame →
meta_loading_continue, frames 8,159 / 8,232 / 9,491 for seeds 0 / 1 / 2) under p190 and p65
with no crash. In deterministic mode ThreadCall jobs run serially on the main thread. So the
crash more likely depends on load-completion timing than on heap contents, but I have not
measured that. It stays in the char class, open.

## Not done

- No source member was changed, so there was no `ab_measure` run and no HX_NATIVE init.
- `tools/native_health.sh --selftest` was not run, per the brief.
- The control's retail ctor was not re-disassembled here. Its `0x8232DCF8` reading comes from
  the FORCERELOAD record and the in-header comment. No new divergence needed a retail check.
- Two lines of follow-up would buy more reach than more seeds would:
  - Widen `rb3_app_determinism_frame`'s state hash with game state (band score, per-player
    streaks and track state), since the walk's song paths are otherwise checked only through
    the RNG and clocks.
  - Run an instrument-input walk, so the scoring paths read their members.

Logs: `~/tmp/perturb2/r_<tag>/run.log`, with tags `t<seed>_{d,p190,p65}`, `t0_d2` and
`c0_{d,p190,p65}`. Binaries are `~/tmp/perturb2/bin-{tip,ctl}`.
