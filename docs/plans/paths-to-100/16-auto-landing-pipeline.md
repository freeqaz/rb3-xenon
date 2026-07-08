# Auto-landing pipeline — verification lanes, regression locks, and policy-gated merges

Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: infra

## Summary

Landing wave results onto `main` is today a coordinator-manual sequence
(`scripts/harvest/land.sh` rebase → composed cold verify → hand-run honesty
gates → `git merge --ff-only`) with several documented data-integrity hazards
and no machine-enforced regression lock. This RFC proposes a single hardened
**land-lane**: a serialized queue that rebases, cold-verifies, runs every
honesty gate as a hard exit-code gate, snapshots a **per-function match%
regression lock** into `decomp.db` on every merge, and hard-fails any landing
that drops a previously-100% function. It is infra, not a match lever — it
protects the +N the other 19 RFCs produce.

## Motivation

Every match landed by the LLM grind fleet (`12-grind-fleet-v2.md`), the
permuter farm (`11-permuter-farm.md`), the symbol sweeps
(`14-systematic-symbol-sweeps.md`) and the codegen library
(`13-codegen-idiom-library.md`) has to pass through landing. As those veins run
concurrently and unattended, the landing step becomes the fleet's single
serialization point and its single point of silent corruption. The failure
modes are already documented and have already cost real matches:

- **Warm-cache false-net-zero** (`docs/decomp/handoff/verify-ab-reliability-2026-07-01.md`
  finding #1): a CoW worktree reflinks main's warm `build/`; ninja mtime tracking
  can reuse a stale `.obj` whose source changed, so the "after" report never sees
  the edit and A/B shows exactly 0 delta — a real win *or* a real regression both
  vanish. Proven on `CharEyes`: edited `CharEyes.obj` md5 differed yet the
  whole-binary report showed 0 fuzzy change across all units.
- **git-stash race under concurrency** (finding #3): `git stash push/pop` uses
  the object store shared across sibling worktrees; a concurrent agent's stash
  push made another's pop grab the wrong stash. `fresh_report.sh` also writes a
  **shared** log (`/tmp/rb3_build_fresh_report.log`) → garbled under concurrency.
- **objects.json / target_symbol_map cross-agent replace-not-merge drops**
  (SOP `wave-loop-SOP-2026-06-20.md` line ~52): two lanes touch the same union
  files; a keep-theirs resolve at module level silently drops the last-landed
  lane's wiring. This is the "2026-07-01 zeroed wave" the resolver's swallowed
  CONFLICT warning let slip by.
- **objdiff-cli `--batch` global-resolve false STUBs** (finding #4): batch mode
  resolves symbols against whatever unit the target map points to, ignoring the
  explicit `-1/-2` pair → `base_size=0` → false STUB verdicts.
- **Stamp path confusion** (finding #2): the renamer stamp is at
  `build/45410914/target_symbol_renames.stamp` (top level), but multiple
  workflows glob `build/45410914/*/target_symbol_renames.stamp` (no match → stamp
  not removed → stale renames).

None of these is caught by the current `measure_delta.py` alone. And crucially:
**there is no regression lock.** `decomp.db.functions` carries
`current_percent`/`best_percent` (verified: `PRAGMA table_info(functions)`), but
nothing snapshots the strict-100 set at each merge and hard-fails a landing that
drops one. The only guard is the coordinator re-reading `measures.matched_functions`
by eye and trusting `measure_delta.py`'s fuzzy-regression scan was run twice.

## Current state (verified)

Numbers verified 2026-07-08 against the live repo:

- STRICT: **11,240 / 65,619** functions matched (17.129%); **962,656 /
  11,074,108** code bytes (8.693%) — `build/45410914/report.json`
  `measures.matched_functions` and `tools/fuzzy_progress.py`.
- FUZZY WIRED set: **94.602%** over 1,392,316 attempted bytes (n=13,584).
  Staircase ≥95: 12,743 | ≥90: 13,021 | ≥80: 13,175 | ≥50: 13,407
  (`tools/fuzzy_progress.py`).

Existing landing machinery (verified present):

- `scripts/harvest/land.sh` (72 lines) — rebases one lane branch onto `main`,
  auto-resolves the three union files, prints `READY:<branch>` or
  `DEFER:<branch> <reason>`. It already has real guards: discards *only* a
  lone `tools/download_tool.py` dirt (else DEFERs), routes resolver CONFLICT
  warnings to stderr (not `/dev/null`), and enforces a `merge-base
  --is-ancestor main <branch>` READY invariant that catches silent-rebase-failure
  false-READYs. **It does not merge** — the coordinator runs `git merge --ff-only`
  by hand afterward.
- `scripts/harvest/resolve_json_union.py` / `resolve_splits_union.py` — dict-union
  and line-union resolvers for `scripts/target_symbol_map.json`,
  `config/45410914/objects.json`, `config/45410914/splits.txt`.
- `scripts/harvest/overlap_check.py` — splits `.text`/`.pdata` range-overlap gate
  (hard-fail exit 1); the line-union resolvers do **not** detect a range overlap.
- `scripts/harvest/measure_delta.py` — strict net (gains − regressions) over the
  99.999% set + per-function fuzzy-regression scan (`--fuzzy-eps` default 1.0);
  disambiguates duplicate function names by occurrence index. Docstring already
  states the gate: *net>0 AND zero unexplained strict regressions AND zero real
  fuzzy regressions, run twice, NET identical.*
- `scripts/harvest/ab_supervise.sh` — supervised composed whole-binary A/B in a
  worktree (rm stamp + touch config.yml + retry `fresh_report.sh` up to 6×),
  monitored by marker not PID.
- `tools/fresh_report.sh` — forces a fresh full `report.json`; already snapshots
  pre-build `matched_functions` and warns on a spurious delta >10 with no source
  change (but **cannot** catch a false-net-*zero* — finding #1).
- `tools/icf_alias_check.py` — honesty audit for ICF-stub-fold inflation in
  pinned spans; exit 0 = HONEST, 1 = inflation (gateable).
- `scripts/setup_worktree.sh` — CoW worktree; `--cold-cache` disables warm
  seeding for a guaranteed-clean A/B (verified flag at lines 14/20/78).
- `scripts/sync_match_percent.py` — syncs `report.json` → `decomp.db`
  `current_percent`/`size`/`demangled`; `--promote` flips 100% → COMPLETE.
- `decomp.db` (repo root, 22 MB, `schema_version=16`; the
  `scripts/orchestrator/decomp.db` in git-status is a stale 0-byte artifact).
  `functions` has `current_percent`, `best_percent`, `verdict`, `locked_by`,
  `locked_at`. **No landing-snapshot / regression-lock table exists** — this is
  the gap.

**Scout-claim corrections:** the brief says "post-land whole-binary regression
lock snapshot into decomp.db" as if a place exists — it does not; the RFC has to
*add* the table (see §Proposal). The brief's "git-stash races under concurrency"
is real and documented, but note `land.sh` itself does **not** use stash (it
rebases); the stash race was in the older `verify-stage-wave.js` flow, now
advised against. Verified correct.

## Proposal

A single hardened **land-lane** — one script, one queue, one lock — that every
producer (grind fleet, permuter farm, sweeps, manual ports) hands a
worktree/branch to, and that is the *only* writer to `main`.

### 1. The land queue (serialize the merge point)

Landing must be serial: two concurrent `git merge --ff-only` into the same
`main` is the current pain point (`land.sh` rebases against `main`, so a second
lane racing the first is either rebased against a stale main or conflicts).
Serialize with a filesystem lock, not stash:

```
scripts/harvest/land_queue.py enqueue <worktree|branch>   # producers call this
scripts/harvest/land_queue.py drain                       # coordinator/cron: one lane at a time
```

- Queue = an append-only JSONL at `.claude/land_queue.jsonl` (branch, worktree,
  producer id, enqueue ts). Producers never touch `main`.
- `drain` takes an exclusive `flock` on `.claude/land.lock` (advisory, blocks a
  second drainer), pops the head, and runs the land-lane below. On success it
  advances `main`; on gate failure it moves the entry to
  `.claude/land_deferred.jsonl` with the reason and continues. This replaces the
  coordinator eyeballing `land.sh` output and hand-running `git merge --ff-only`.

### 2. The land-lane (per entry, inside `drain`, under the lock)

```
 a. land.sh <branch>            -> READY | DEFER(reason)     [existing]
 b. overlap_check.py            -> exit 1 on .text/.pdata overlap   [existing]
 c. objects.json wiring check   -> every landed TU's <TU>.cpp still present  [SOP grep, promote to gate]
 d. COLD verify in a throwaway worktree:
      setup_worktree.sh ~/tmp/land-verify <branch> --cold-cache
      ab_supervise.sh ~/tmp/land-verify ~/tmp/land-verify.log    [existing, cold]
 e. measure_delta.py BASE.json AFTER.json  (run TWICE, NET must match)  [existing]
 f. icf_alias_check.py          -> exit 1 = ICF-alias inflation    [existing]
 g. regression-lock check       -> NEW (see §3): hard-fail on any 100->x drop
 h. only if a..g all pass:  git merge --ff-only <branch>  into main
 i. post-land snapshot          -> NEW (see §3): write the new 100-set lock row
 j. sync_match_percent.py       -> decomp.db current_percent refresh   [existing]
```

The verify at (d) is **cold** by construction — this is the single most
important design decision, because the warm-cache false-net-zero (finding #1)
is precisely the failure that makes a landing look safe when it silently
regressed. `setup_worktree.sh --cold-cache` disables warm seeding (verified);
`fresh_report.sh` in that worktree then builds every obj from source. Cost is
bounded: objcache serves a cold full rebuild in ~3.5 s all-hits (CLAUDE.md), so
the cold worktree is near-free for a lane that touches a handful of TUs.

Per-worktree logs (never the shared `/tmp/rb3_build_fresh_report.log`) — the
land-lane always passes an explicit per-entry log path under `~/tmp` to fix
finding #3's garbled-log half.

### 3. Per-function match% regression lock (the new machinery)

Add one table to `decomp.db` (`schema_version` bump 16 → 17). Keyed by the same
`(unit, function-name, occurrence)` tuple `measure_delta.py::pct_map` already
uses to disambiguate the ~11 binary-wide duplicate names:

```sql
CREATE TABLE landing_snapshot (
    merge_commit TEXT NOT NULL,      -- main SHA after the ff-merge
    landed_at    INTEGER NOT NULL,   -- unix ts
    unit         TEXT NOT NULL,
    fn_name      TEXT NOT NULL,
    occurrence   INTEGER NOT NULL,
    match_pct    REAL NOT NULL,       -- match_percent_normalized at merge time
    PRIMARY KEY (merge_commit, unit, fn_name, occurrence)
);
CREATE INDEX ix_snapshot_strict ON landing_snapshot(unit, fn_name, occurrence)
    WHERE match_pct >= 99.999;
```

- **Snapshot (step i):** after each ff-merge, write the full strict-100 set (and
  optionally the ≥50 near-miss band, for fuzzy-regression forensics) tagged with
  the new `main` SHA. This is the authoritative "what was 100% as of commit X".
- **Regression-lock check (step g), the hard gate:** load the strict-100 set from
  the *latest* `landing_snapshot`; from the candidate's cold `report.json`, any
  `(unit, fn, occ)` that was ≥99.999% then and is <99.999% now is a **hard fail**
  — the lane DEFERs, `main` is untouched, and the entry is written to
  `land_deferred.jsonl` naming the exact regressed functions.

This is stronger than `measure_delta.py`'s in-worktree A/B: A/B compares the
lane against *its own* rebase base, but the lock compares against the
*authoritative last-landed main*, so it catches a regression introduced by a
bad union-resolve during the rebase itself (the objects.json replace-not-merge
drop, finding / SOP line ~52) — a class the per-lane A/B is blind to because the
drop happens *during* landing, not in the lane's commit.

An explicit escape hatch: a landing may *intentionally* drop a strict-100 (e.g.
a wider correct span replacing a fake ICF-stub-fold that `icf_alias_check`
flagged). The lane records an allowlist entry
`land_queue.py enqueue <branch> --allow-drop <unit>:<fn>[:<occ>] --reason "..."`;
the gate then treats *only* those tuples as expected and still hard-fails any
unlisted drop. Every allowed drop is logged to `land_deferred.jsonl`/the
snapshot audit trail so it is never silent.

### 4. Auto-land policy (what is safe unattended)

Not every producer output should merge without a human. Tier by blast radius,
measured from the diff + the gate results:

| Tier | Criteria | Policy |
|---|---|---|
| **AUTO** | Touches only its own TU's source + its own splits/objects/tsm lines; all gates green; regression-lock clean; strict net ≥ 0 AND fuzzy net > 0; diff ⊆ `src/<one TU>` + the 3 union files | `drain` merges unattended |
| **REVIEW** | Touches a shared header (`src/system/**/*.h` used by >1 TU), a base-class layout, or a vtable; OR strict net > 0 but with an allowed-drop; OR fuzzy-only win (0 strict, fuzzy>0) on an engine TU | Gates run, result parked in `land_ready.jsonl`; human runs `land_queue.py approve <branch>` |
| **BLOCK** | Any gate red; regression-lock dirty w/o allowlist; overlap; ICF-inflation; objects.json wiring dropped | DEFER, never merge |

The AUTO tier is deliberately narrow. The reason: the highest-EV veins this
protects (grind fleet, permuter, single-TU ports) are *exactly* the ones that
touch one TU and are provably local — they qualify for AUTO and drain without a
human. The dangerous class (shared-header / base-layout / vtable edits, which
ripple across many units and are where the reveal-cascade and the false-net-zero
traps bite) is forced to REVIEW. This lets the fleet run unattended on its bread
and butter while keeping a human in the loop exactly where the documented
disasters happened.

### 5. Data flow

```
producer (grind/permuter/port) --commits one-lane branch in its worktree-->
  land_queue.py enqueue  --appends .claude/land_queue.jsonl-->
    land_queue.py drain (flock .claude/land.lock, one at a time)
      land.sh rebase --union-resolve--> overlap/wiring gates
      --> cold worktree A/B (ab_supervise) --> measure_delta x2
      --> icf_alias_check --> regression-lock vs latest landing_snapshot
      --> tier decision: AUTO=ff-merge | REVIEW=park | BLOCK=defer
      --> on merge: write landing_snapshot(new SHA) + sync_match_percent.py
```

## Alternatives considered

- **Keep it fully manual.** Works today at low wave cadence, but does not scale
  to an unattended fleet and has already lost matches (the zeroed wave). The
  regression lock is valuable even at manual cadence — it is the cheapest single
  addition and can ship independent of the queue.
- **GitHub PR + CI merge queue.** The obvious industry answer, but the whole
  build/toolchain is gitignored (`build/`, `orig/*`, `build.ninja`, per
  CLAUDE.md), objcache/wibo/jeff/objdiff are local forks, and a cold verify needs
  the CoW worktree infra — none of which lives in a stock CI runner. Re-hosting
  that is a project in itself for zero match gain. Rejected; the local
  `flock`-serialized drain is the right-sized version of a merge queue.
- **Optimistic concurrent merges + post-hoc detection.** Let lanes merge freely
  and run the regression-lock as a nightly audit that reverts bad commits.
  Rejected: a bad merge poisons every subsequent lane's rebase base (the
  cascade), and reverting a merge that later lanes built on is far more expensive
  than serializing at the merge point. Serialize; don't reconcile.
- **Trust `measure_delta.py`'s in-worktree A/B as the whole gate.** Insufficient:
  it compares the lane to its rebase base, not to authoritative main, so it
  cannot see a regression introduced by the union-resolve during landing (§3).

## Effort & expected value

Effort (one infra-focused agent, phased):

- **Phase A — regression lock (highest value, smallest):** `landing_snapshot`
  table + schema bump + snapshot/check functions, wired as gate (g)/(i) around
  the *existing* manual land sequence. ~1 day. Ships value immediately without
  the queue.
- **Phase B — land-lane script:** compose land.sh + overlap + wiring + cold A/B
  + measure_delta×2 + icf + lock into one `land_lane.py` with a single
  pass/defer verdict and per-entry `~/tmp` log. ~1–2 days.
- **Phase C — queue + flock + tiers:** `land_queue.py` enqueue/drain/approve,
  policy tiers, deferred/ready ledgers. ~1–2 days.

Expected value: this vein does **not** directly add matched functions — it is
insurance and throughput, and the honest EV is *matches protected + coordinator
time reclaimed*, not +N. Anchored to comparable past events in this repo: the
2026-07-01 zeroed wave (union-resolve keep-theirs drop) and the recurring
warm-cache false-net-zero each silently cost or masked wave-sized deltas
(single waves in this repo have landed +50 to +85; e.g. round-1 ws3 +85, wave-13
+50 per MEMORY.md). Preventing even one such silent loss per month pays for the
whole build. The larger, non-numeric payoff is unblocking the *unattended*
operation the fleet RFCs (11/12/14) assume: without a machine-enforced land gate,
those veins cannot safely run without a coordinator babysitting every merge.

## Risks & failure modes

- **Cold verify is not free at scale.** If dozens of lanes queue, serial cold
  A/Bs bottleneck throughput. Mitigation: objcache makes a cold full rebuild
  ~3.5 s all-hits; batch several *provably-independent* AUTO-tier lanes (disjoint
  TU sets, verified by `overlap_check` + objects.json/tsm key-disjointness) into
  one cold verify, then ff-merge them in sequence under the same lock — verify
  once, land N. Fall back to per-lane on any key overlap.
- **The lock becomes the bottleneck.** A single `flock` serializes all landing.
  Acceptable — landing is *already* serial in practice (rebase onto growing
  main). The queue makes the serialization explicit and non-racy rather than
  slower.
- **Regression-lock false positives from reloc-name noise.** A function can wobble
  <1% across builds from relocation-name normalization. Mitigation: the strict
  gate is at 99.999% (matching `measure_delta.py::STRICT`), and the fuzzy-drop
  half uses `--fuzzy-eps` (default 1.0) exactly as `measure_delta.py` does —
  reuse its thresholds verbatim, don't invent new ones.
- **Snapshot drift if a landing bypasses the lane.** If someone merges by hand
  the snapshot goes stale and the next lane's lock check compares against an old
  main. Mitigation: a pre-drain assertion that `main`'s HEAD equals the SHA of
  the newest `landing_snapshot`; mismatch → refuse to drain and tell the human to
  re-snapshot (`land_queue.py resnapshot`). This also makes the hand-merge path
  detectable rather than silently corrupting the lock.
- **ICF-alias inflation slips the strict count up, not down.** The regression
  lock catches *drops*, not fake *gains*; `icf_alias_check.py` (gate f) is the
  complementary guard against inflation. Both must be in the lane; neither alone
  is sufficient. Cross-ref `18-metrics-and-dashboard.md` for the accounting.

## Kill criteria

- If Phase A's regression lock, backfilled against the last ~20 landed commits,
  fires on **zero** real historical regressions AND the coordinator reports the
  manual gate has never actually let a strict-100 drop through, the lock is
  belt-and-suspenders and only Phase A ships (skip B/C); the queue is deferred
  until wave cadence actually exceeds one-at-a-time.
- If cold verify per lane cannot be kept under ~30 s median even with objcache +
  independent-lane batching, the cold-verify-every-lane design is too slow for an
  unattended fleet — fall back to warm verify **plus** a mandatory nightly cold
  re-verify of the day's merges, accepting a detection lag instead of prevention.
- If, after Phase C, `land_deferred.jsonl` is dominated by false DEFERs (gates
  red on landings a human confirms were fine), the gate tuning is wrong and the
  fleet routes around it — kill the AUTO tier, keep the lock + manual drain.

## Open questions

- Should the snapshot store the full ≥50 near-miss band (enables fuzzy-regression
  forensics + feeds `18-metrics-and-dashboard.md`) or only strict-100 (smaller,
  cheaper)? Leaning full-band, gated behind the strict-100 index for the hot path.
- Where does the queue live so *all* producers see it — repo-relative
  `.claude/land_queue.jsonl` (simple, but agents in CoW worktrees see their own
  copy)? Likely must be an **absolute** path in the main repo
  (`/home/free/code/milohax/rb3-xenon/.claude/land_queue.jsonl`) that worktree
  producers write to explicitly, since a worktree's `.claude/` is its own.
- Should AUTO-tier draining be cron-driven (`12-grind-fleet-v2.md` already
  proposes cron) or event-driven off enqueue? Cron is simpler and matches the
  fleet's existing cadence; event-driven risks two drainers without the flock.
- Does the `allow-drop` escape hatch need coordinator co-sign, or is the logged
  audit trail enough? Start with audit-trail-only for AUTO, require
  `approve` for any REVIEW-tier drop.

## References

- `docs/decomp/handoff/verify-ab-reliability-2026-07-01.md` — the four tooling
  findings this RFC engineers away (warm-cache false-zero, stamp glob, stash
  race, batch global-resolve), plus the partials-landable policy change.
- `docs/decomp/handoff/wave-loop-SOP-2026-06-20.md` — authoritative coordinator
  harvest/land SOP; the manual sequence this RFC hardens (esp. lines ~37–71:
  objects.json wiring survival, per-unit pin attribution).
- `scripts/harvest/land.sh` — existing rebase + union-resolve; keep as land-lane
  step (a).
- `scripts/harvest/README.md` — land sequence, splits overlap self-check snippet.
- `scripts/harvest/measure_delta.py` — strict + fuzzy-regression A/B; reuse its
  `(unit,fn,occ)` keying and thresholds verbatim in the regression lock.
- `scripts/harvest/overlap_check.py` — splits range-overlap hard gate.
- `scripts/harvest/ab_supervise.sh` — supervised composed cold A/B in a worktree.
- `scripts/harvest/resolve_json_union.py`, `resolve_splits_union.py` — union
  resolvers (the replace-not-merge drop lives here).
- `tools/fresh_report.sh` — fresh full report; already snapshots pre-build
  matched_functions (extend for the false-net-zero check per finding #1 TODO).
- `tools/icf_alias_check.py` — ICF-stub-fold inflation gate (exit 0/1).
- `tools/fuzzy_progress.py` — the STRICT/FUZZY/staircase numbers of record.
- `scripts/setup_worktree.sh` — CoW worktree; `--cold-cache` for trusted A/B.
- `scripts/sync_match_percent.py` — report.json → decomp.db sync (post-land).
- `scripts/ingest_report.py` — seed/rebuild decomp.db from report.json.
- `decomp.db` (repo root, `schema_version=16`) — `functions` table
  (`current_percent`/`best_percent`/`locked_by`/`locked_at`); the
  `landing_snapshot` table (§3) is the new addition. Note: the
  `scripts/orchestrator/decomp.db` in git-status is a stale 0-byte artifact —
  the live DB is at repo root.
- Sibling RFCs: `11-permuter-farm.md`, `12-grind-fleet-v2.md`,
  `13-codegen-idiom-library.md`, `14-systematic-symbol-sweeps.md` (the producers
  this lane serves); `18-metrics-and-dashboard.md` (consumes the snapshot for
  vein-ROI accounting); `01-endgame-definitions.md` (defines the strict-100 set
  the lock protects).
