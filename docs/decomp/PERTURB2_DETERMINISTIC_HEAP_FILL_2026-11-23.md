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

(seed table)

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
- No ablation of the BONE DIAG filter under perturb alone. Its hunks were not inspected
  per arm beyond the d-vs-d2 baseline.
