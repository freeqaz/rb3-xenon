# Worktree build-tooling findings (2026-07-01)

Coordinator session under an extreme owner build-storm (load ~290–350, **35+
concurrent `all_source` full builds**). What began as verify+land friction turned
into a full root-cause of the worktree "full-rebuild tax." Two bugs found and
fixed; the second one's mechanism is now proven by experiment (the earlier PCH
guess in this doc was WRONG — corrected below).

## FINDING 1 — setup_worktree SIGPIPE abort (FIXED, committed 8d8d257)
The 4-lane parallel `verify-stage-fuzzy` workflow "failed" (all agents completed
without StructuredOutput). Root cause was NOT quota: every lane's worktree had **no
`build.ninja`**, so every build died with `ninja: error: loading 'build.ninja'` and
the lanes spun on the lock until they gave up.

Why no build.ninja: `setup_worktree.sh` did
`_primary="$(git worktree list --porcelain | awk '/^worktree /{print $2; exit}')"`
under `set -euo pipefail`. `awk ... exit` closes the pipe after the first line; under
load (scheduling) `git` is still writing when the pipe closes → **SIGPIPE (141)** →
pipefail propagates → `set -e` aborts the whole script **right before `configure.py`
+ the ninja prime**. Load-dependent race: fine at low load / few worktrees, trips at
load-290 with ~38 worktrees. Same class as the `grep -c` abort fixed in f1f7d0d.

**Fix (8d8d257):** `awk '/^worktree /{if(!seen++) print $2}'` — prints the first
match but reads to EOF, so `git` never gets SIGPIPE. Verified: setup now reaches
configure + prime and emits `build.ninja`. A post-configure `build.ninja` existence
assertion was added as a permanent backstop so this failure mode can never again be
silent (it would previously yield an unbuildable worktree + false-0 stale reports).

⚠ This likely also bit the OWNER's wave — any worktree created under load got a
silent no-`build.ninja` and a false-0 stale report. Worth a broad re-check.

## FINDING 2 — the "full 727-obj rebuild" tax: ROOT CAUSE = absolute tool paths (FIXED, scoped prime)
Every fresh full-setup worktree's **prime recompiled all ~727 objs** — 30–60+ min
under storm load, on *every* orchestrator-pool worktree, not just verify lanes. This
is the single biggest wave-throughput tax. (An earlier draft of this doc guessed the
PCH; that was wrong. The real mechanism is below, proven by dry-run experiments in a
warm worktree — no builds needed.)

### The experiments (all via `ninja -n -d explain`, no compiles)
1. **Fresh worktree, both `.ninja_log` + `.ninja_deps` absent, outputs touched
   current → `ninja -n` reports 0 dirty compiles.** So the reflinked cache + touch is
   mtime-valid. (This is what the earlier draft mistook for "the no-op works".)
2. **But the real bare-`ninja` prime compiled 727 anyway.** The `-n` dry-run lies
   because it never executes the intermediate edges. The real sequence:
   `ninja` (default target = `report.json`, depends on ALL objs) → runs SPLIT →
   regenerates `config.json` → the `configure.py` generator edge fires → `build.ninja`
   is regenerated → **ninja reloads and recomputes the plan** → now `.ninja_log`
   exists (SPLIT wrote it) but the compile edges have no `.ninja_deps` entry →
   `ninja explain: deps for '…obj' are missing` → **all 727 rebuild.**
3. **Copying main's `.ninja_log` + `.ninja_deps` into the worktree did NOT help** —
   still 727 dirty, but now the reason flips to `"is dirty"` (command-hash), not
   "deps missing". The diff of the two `build.ninja` files shows why:
   - worktree compile command: `--wrapper /home/free/code/milohax/wibo/build/release/wibo` (**absolute**)
   - main compile command:      `build/tools/wibo` (**relative**)

### Root cause (confirmed)
`setup_worktree.sh` runs `configure.py --dtk/--objdiff/--wrapper <ABSOLUTE sibling
paths>` on purpose — to dodge the cargo (`dtk`, `objdiff-cli`) and download (`wibo`)
edges that main's relative `build/tools/*` references would drag in (the documented
manifest-dirty loop). The unavoidable side effect: **every compile command's text —
hence its ninja command-hash — differs from main's relative-path `.ninja_log`.** So
the reflinked objs, though byte-valid, can never be command-hash-validated against
main's log; the first full build recompiles all of them. It is a genuine design
tension (absolute paths buy "no cargo/download edges" at the cost of "cannot reuse
main's command hashes"), not a simple bug.

### Fix shipped — scope the prime to `config.json`
Bare `ninja` builds `report.json` (all 727 objs) and triggers the SPLIT→reload→
deps-missing cascade. Building **`build/<VERSION>/config.json`** alone performs the
SPLIT + graph-settle (the actual determinism goal the prime exists for) with **ZERO
obj compiles.** Measured on the fixed script:
- prime MSVC compiles: **727 → 0**
- single-obj objdiff build afterward (the common agent case): **1** recompile
- a later full `ninja` (e.g. verify A/B needing report.json): still 727 **once**,
  amortized in the serial-one-worktree pattern (Finding 3).

`WT_SKIP_PRIME=1` env gate was also added (skip the prime entirely for pure-config
diagnostics / fast worktree creation).

### Option NOT taken (documented for the owner to weigh)
A *comprehensive* warm — making even the first full build a no-op — would require the
worktree's compile commands to be byte-identical to main's: reflink main's
`build/tools/{wibo,dtk,objdiff-cli}` into the worktree as private copies, configure
with main's **relative** paths, copy main's `.ninja_log`+`.ninja_deps`, and re-touch.
That re-introduces exactly the cargo/download edges the absolute-path design was
built to avoid, and must be validated carefully against the manifest-dirty loop. Not
done here (too risky mid-storm, on the owner's actively-iterated script). The scoped
prime captures ~all the value for the single-obj agent fleet without that risk.

## FINDING 3 — full builds don't compose; verify lanes must serialize
4 concurrent full `ninja-locked` builds (each 727 steps) + the owner's 35 = lock
waits and starvation. A verify wave that spins a fresh worktree per candidate is
self-defeating for anything needing a full report.json. **Do all candidates serially
in ONE warm worktree** (one full baseline build, then each patch is a fast
incremental: 1 TU recompile + report). With the scoped prime, single-obj objdiff
lanes no longer pay the 727 at all.

## PROCESS LESSONS (re-confirmed)
- Never `pgrep -f <str>` where `<str>` appears in the kill command → self-kill
  (exit 144). Kill by explicit PID or match `/proc/<pid>/cwd` inode.
- Don't kill a build that's 600+/727 to chase an optimization — restart re-runs the
  full build (reconfigure cascade loses `.ninja_log` progress). Let baselines finish.
- `ninja -n` (dry-run) is NOT a faithful predictor of a real build when SPLIT/
  configure generator edges are in play — it skips the reload that flips deps state.
  Confirm rebuild counts with a real (scoped) run.

## STATE
- Script fixes committed on main: SIGPIPE (8d8d257) + scoped prime + build.ninja
  assertion + `WT_SKIP_PRIME` gate + honest comments (this commit).
- ~10682 matched. Staged struct candidates (CreditsPanel/GamePanel/Character/
  CharEyes) remain as patch files in `~/tmp/verify_patches/`; serial A/B pending.
- Land gate (owner policy): fuzzy-positive AND 0 strict-100 regressions.
