# W16-PE — the native runtime check now runs every target (2026-10-06)

Branch `w16-pe`, worktree `~/tmp/wt-w16-pe`, off main `6f1d1d794`.

**Brief.** The native gate only links its targets, so `rb3-vocal2` and
`rb3-harmony` crashed on their first frame for two months unnoticed
([W16-PD §2.1](W16PD_NATIVE_STUB_RECONCILIATION_2026-10-03.md), follow-up §6.1).
Make the native runtime check run every native target that can run headless on
real data, and fail on a crash or a nonzero exit.

**Changed files:** `tools/native_health.sh` (the runtime check),
`docs/decomp/NATIVE_HEALTH.md` (committed baseline), this doc. No `src/` change,
no `native/` change, no build-graph change ⇒ no X360 A/B is owed (nothing the
match build compiles was touched).

## 0. Headline

| | before (main `6f1d1d794`) | after |
|---|---:|---:|
| native targets the runtime check runs | **4 / 18** (milo, ark, render, score2) | **18 / 18** |
| crash before the first `[PASS]` line | `UNRUNNABLE novgates` ⇒ **rc=3** (INCOMPLETE) | **FAIL, rc=1** |
| a crash/exit in a target without the `[PASS]/[FAIL]` contract | invisible (target not run) | **FAIL, rc=1** |
| W16-PD's bug reproduced (null-`BandUser` guard removed) | `runtime=PASS runtime_ran=4` | **`runtime=FAIL runtime_failed=rb3-vocal2:crash,rb3-harmony:crash`** |
| runtime gates counted | 40 | 56 |
| wall, full `--selftest` run, warm tree | — | **14 s** (runtime half ~3 s) |

## 1. Per-target run

Each target runs under `timeout -k 10 $NATIVE_HEALTH_TIMEOUT` (default 120 s).
Its inputs are declared in the script, so an absent file reads as
`UNRUNNABLE nodata` instead of looking like a driver abort.
"Completion marker" is the line each driver prints only after its last stage.
Times were measured on clean main in this worktree, one run each.

| target | input (real data unless noted) | completion marker (ERE) | contract | wall |
|---|---|---|---|---:|
| rb3-dta | retail `songs.dta` (`rb3/orig-assets/extracted/songs/`) | `^Done\. Showed [0-9]+ song` | — | 0.04 s |
| rb3-song | the ark | `^RESULT: ALL GATES PASSED` | 7 gates | 0.07 s |
| rb3-midi | the ark (`20thcenturyboy.mid`) | `^RESULT: ALL GATES PASSED` | 9 gates | 0.21 s |
| rb3-gem / rb3-hit / rb3-score | `hurt/pills` chart | `^Done\.$` | — | 0.01 s each |
| rb3-score2 | none (arithmetic vs M5) | `^RESULT: OK` | 3 gates | 0.01 s |
| rb3-score3 / rb3-score4 | `tool/vicarious` chart | `^Done\.$` | — | 0.05 / 0.06 s |
| rb3-vocal | `tool/vicarious` | `^  all-off \(\+6\) ` (its last row) | — | 0.13 s |
| rb3-vocal2 | `tool/vicarious` | `^Done\.$` | — | 0.15 s |
| rb3-harmony | RB3DX `centerfold.mid` (HARM1–3) | `^Done\.$` | — | 0.14 s |
| rb3-crowd | `tool/vicarious` | `^=== M12 complete` | — | 0.07 s |
| rb3-save | none (serialization round-trip) | `^=== ALL ROUND-TRIPS OK` | — | 0.00 s |
| rb3-ark | the ark + independent extraction | `^RESULT: ALL GATES PASSED` | 9 gates | 0.07 s |
| rb3-frame | none (GPU clear + readback) | `^rb3-frame: OK ` | — | 0.57 s |
| rb3-milo | the ark (`tracksystem_meshes`) | `^RESULT: ALL GATES PASSED` | 8 gates | 0.16 s |
| rb3-render | the ark (2 default cells) | `^RESULT: ALL GATES PASSED` | 20 gates | 1.31 s |

The inputs are the drivers' own built-in defaults, now passed explicitly. They
can be overridden with `RB3_MILOHAX`, `RB3_MID_{VICARIOUS,PILLS,CENTERFOLD}`
and `RB3_SONGS_DTA`. rb3-score2, rb3-save and rb3-frame take no data but run
headless, so they are included; leaving out a runnable target was the defect.

## 2. Classifier (`classify_run`, one function, also used by the selftest)

These checks run in order, and the first one that hits decides the result:
`hang` (rc 124) → `crash` (rc > 128; coreutils `timeout` re-raises the child's
signal, so SIGSEGV is rc 139 through the wrapper too, as measured) → rb3-render
rc 2 / rb3-frame adapter-init message ⇒ `UNRUNNABLE nogpu` → `exit` (any other
rc ≠ 0) → `gatefail` (a `  [FAIL] ` line) → `nocomplete` (rc 0 without the
marker) → `nogates` (a contract target with 0 `[PASS]` lines) → `ok`.

⛔ **Semantics change.** The old `run_target` tested "zero gate lines" before it
looked at the rc. So a contract target that segfaulted before its first `[PASS]`
line came out `UNRUNNABLE novgates`, and the whole run read **INCOMPLETE, rc=3:
"nothing is known to be broken"**. UNRUNNABLE now means only a verified
environmental reason checked before the run: no executable, absent input, or no
GPU.

## 3. Acceptance: injected crashes go red, the clean tree passes

All legs ran in this worktree with `--skip-link`, after a
`cmake --build native/build`. rc=3 on the clean leg comes only from
`--skip-link`; the full run is in §4.

| leg | prediction | measured `NATIVE_HEALTH_RESULT` (runtime fields) | rc |
|---|---|---|---:|
| clean main | 18/18 OK | `runtime=PASS runtime_ran=18 runtime_total=18 … runtime_crashed=0 runtime_failed=none` | 3 (skip-link) |
| **(a)** `*(volatile int*)0 = 1` inserted in `main_crowd.cpp` after Stage 1's gem loop, i.e. mid-run after output | exactly `rb3-crowd:crash` | `runtime=FAIL runtime_ran=18 … runtime_crashed=1 runtime_failed=rb3-crowd:crash` | **1** |
| **(b)** W16-PD's `#ifdef HX_NATIVE if (!u) return false;` removed from `VocalPlayer::PressingToTalk` (the real f3ec9592d bug) | exactly vocal2 + harmony | `runtime=FAIL … runtime_crashed=2 runtime_failed=rb3-vocal2:crash,rb3-harmony:crash` | **1** |
| (b) through **main's** `native_health.sh` | PASS (never runs them) | `runtime=PASS runtime_ran=4 runtime_total=4` | 3 (skip-link) |
| (b) through `native_build_gate.sh` | PASS (links only) | `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0` | 0 |

All five predictions held. In (b) both drivers died after printing their
first table header (`last output:   songMs   phrase …`), which is the
first-frame crash W16-PD describes. Both injections were reverted
(`git status` clean apart from the script) before §4.

### Permanent selftest controls (`--selftest`)

Each control runs a real, unmodified binary through the same `classify_run`, and
must land in the **specific** class it names; "red somehow" does not count. It
also obeys the existing pair rule: the target's positive run must have been
green first.

| control | how | required class | measured |
|---|---|---|---|
| `crash-segv` | `rb3-harmony` under `ulimit -s 16` | `crash` | `CRASH -- died on SIGSEGV (rc=139)`; 5/5 deterministic in probing |
| `exit-nonzero` | `rb3-crowd <chart> "PART NOSUCH"` (its real `track not found; abort.`) | `exit` | `EXIT rc=1` |
| `nocomplete` | `rb3-midi <assets> --list` (a real `return 0` that runs no gate) | `nocomplete` | `rc=0 but never printed its completion line` |

⚠ `crash-segv` dies **before `main` prints anything** (where exactly was not
investigated; the log holds only `timeout`'s core-dump line). So it proves that signal death is classified as a crash whatever was printed,
but not a crash in the middle of a run. Injection (a) above covers that case. It
was done by hand and is not a permanent control, because a permanent mid-run
crash hook would need an edit to every driver.
Other candidate injectors were measured and rejected: `ulimit -t 0` gives
SIGKILL (137), which is death by the harness rather than by a fault, and
`ulimit -s 64` is not enough to crash (rc 0).

## 4. Final full run (clean tree, link gate included)

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
selftest: PASS (9 red, 0 stuck-green, 0 skipped)
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=56 gates_fail=0 unrunnable=none selftest=PASS scatter_unlinked=17 scatter_dirb=0 scatter_multihost=20 rc=0 handpose_controls=3/3 handpose_baseline_fail=1 runtime_crashed=0 runtime_failed=none
```

"9 red" = 8 controls that went red + `render-perturb`, which is inert by design
and is counted as a success by the pre-existing script.

## 5. Deliberately NOT done

- **Not wired into `native_build_gate.sh` or CI.** The gate's contract is
  "compiles + links". `native_health.sh` and lane write-ups parse its
  `NATIVE_GATE_RESULT` line and rc vocabulary, and CI has neither the ark nor the charts, so a runtime phase
  there would always come out INCOMPLETE. The cost argument against it is gone,
  though: the runtime half takes about 3 s warm. A gate-side
  `--run` (or having the gate call `native_health.sh --skip-link`) is the obvious
  next step if the coordinator wants this to run with no human remembering. The
  pre-landing cadence in `NATIVE_HEALTH.md` already says to run
  `native_health.sh --selftest` for shared-`src/` changes.
- **No completion markers added to drivers.** Every marker is a line the driver
  already prints. `rb3-vocal`'s marker is its last table row; that is the most
  fragile one, and it fails loudly (`nocomplete`) if someone reformats it.
- **Scatter drift not investigated**: 14 → 17 unlinked guests and 21 → 20
  multi-host since W3-G's 09-11 line. This change does not touch scatter.
- **CLAUDE.md's native-gate section not edited** (coordinator-owned).

## Reproduce

```bash
tools/native_health.sh <tree> --selftest                 # full
tools/native_health.sh <tree> --skip-link                # runtime only (no build!)
# --skip-link does not build: run tools/native_build_gate.sh or
# `cmake --build native/build` first, or it runs whatever binaries are on disk.
```
