# Worktree build-tooling findings (2026-07-01)

Coordinator session under an extreme owner build-storm (load ~292, **35 concurrent
`all_source` full builds**). Verify+land of the staged struct candidates was blocked
by build-infra friction; the durable output is two tooling findings + one fix.

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
+ the ninja prime**. Load-dependent race: fine at low load / few worktrees (git's
output fits the 64 KB pipe buffer and it finishes before awk exits), trips at
load-292 with ~38 worktrees. Same class as the `grep -c` abort fixed in f1f7d0d.

**Fix (8d8d257):** `awk '/^worktree /{if(!seen++) print $2}'` — prints the first
match but reads to EOF, so `git` never gets SIGPIPE. Verified: setup now reaches
configure + prime and emits `build.ninja`.

⚠ This likely also bit the OWNER's wave — any worktree created under load got a
silent no-`build.ninja` and a false-0 stale report. Worth a broad re-check.

## FINDING 2 — warm-cache "first build is a no-op" is NOT holding (real setup full-rebuilds all 733)
`setup_worktree.sh` reflinks main's `build/45410914` and (line 254)
`find "$WT_BUILD" -type f -exec touch {} +` to mark outputs current, claiming
"prime + agent's first build are no-ops." **Empirically every fresh full-setup
worktree does a full 733-step MSVC rebuild** — under this load that is 30–60+ min
and starves everything. This is the single biggest tax on wave throughput.

### The no-op IS achievable (proven)
build.ninja compile commands use **relative** include paths (`/I src`,
`/I src/system`) — there are **zero** absolute worktree paths in build.ninja, so the
command TEXT is identical across worktrees and the reflinked cache is genuinely
valid. In a hand-built worktree (reflink `build/45410914` + `configure.py` with the
prebuilt `--dtk/--objdiff/--wrapper` paths + `find -exec touch`), a dry-run
`ninja -n build/45410914/report.json` and even bare `ninja -n` (the prime's default
target) both want **0 MSVC recompiles** — only `SPLIT config.yml` + `RUN
configure.py`. `.ninja_log` presence made no difference (mtime governs these edges).

So the reflinked cache + touch is sufficient in principle. The real
`setup_worktree.sh` path nonetheless triggers 733 recompiles → something in the
setup/prime sequence re-dirties a shared input AFTER the touch.

### Leading suspect: the PCH (`build/45410914/pch/system.pch`)
Every MSVC compile depends on `system.pch`. If the prime rebuilds it (its header
inputs' mtimes vs the reflinked pch), all 733 compiles cascade. The warm path
reflinks the pch (main's mtime) and line 254 touches it to NOW, but a later
`configure.py`/`SPLIT`/prime step can re-dirty it (the `RUN configure.py` edge
re-fires and regenerates build.ninja; SPLIT regenerates config.json + target objs).
Unverified — needs `ninja -d explain` on a FRESH real-setup worktree BEFORE its
first build to read the exact dirty reason (couldn't run cleanly mid-storm without
lock contention).

### Proposed fix direction (for the owner — this is their actively-iterated script)
- Capture `ninja -d explain -n` on a just-setup worktree to confirm the pch (or
  which edge) is the dirty root.
- If pch: rebuild/settle the pch ONCE during setup, then re-touch all outputs
  current AFTER configure + the prime's SPLIT, so nothing downstream is newer.
- Payoff is large: 35 full rebuilds → 35 no-ops. Every wave gets ~10× cheaper.

## FINDING 3 — full builds don't compose; verify lanes must serialize
4 concurrent full `ninja-locked` builds (each 733 steps) + the owner's 35 +
wt-lyric ×2 = lock waits and starvation; none of my lanes finished. Until Finding 2
is fixed, a verify wave that spins a fresh worktree per candidate is self-defeating.
**Do all candidates serially in ONE warm worktree** (one full baseline build, then
each patch is a fast incremental: 1 TU recompile + report). The
`verify-stage-fuzzy` workflow should cap full-build concurrency at 1.

## PROCESS LESSONS (re-confirmed)
- Never `pgrep -f <str>` where `<str>` appears in the kill command → self-kill
  (exit 144). Kill by explicit PID or match `/proc/<pid>/cwd` inode.
- Don't kill a build that's 637/733 to chase an optimization — restart re-runs the
  full build (reconfigure cascade loses `.ninja_log` progress). Let baselines finish.

## STATE
- main @8d8d257 (SIGPIPE fix). ~10682 matched.
- Staged struct candidates (CreditsPanel/GamePanel/Character/CharEyes) exported as
  patch files in `~/tmp/verify_patches/`; serial A/B in `~/tmp/vf3` in progress.
- Land gate (owner policy): fuzzy-positive AND 0 strict-100 regressions.
