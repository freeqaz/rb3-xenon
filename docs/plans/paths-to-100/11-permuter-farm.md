# MSVC permuter farm — automated source-permutation search at scale

Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: body-divergence

> **PILOT RESULT — 2026-07-08 — KILL full-band farm.** The mandatory 80-fn
> stratified pilot ran (4 CoW workers, decomp-synth symbol-resolution bug fixed
> first — see `../../../` decomp-synth `d4cfe67`; report.json fallback so the
> permuter no longer silently returns 0 candidates in a fresh worktree).
> **Result: 0 TRUE-100 wins / 66 real attempts** (0/80 target; ~14 were
> fn_/scheduling cases with no permutable proposals). The full 139-pattern set
> was exercised (e.g. `SetupGamma` 48 s) and beam-search was tried (`AddTask`);
> per-pattern win rate **0/139** across the whole pilot. The [90,100) band is
> **codegen-wall-dominated**, exactly as the 2026-06-24 pivot warned (permuter
> 0/all on strcpy/regalloc walls). Kill gate `<3/80 → KILL` is met decisively.
> **Do NOT build the standing farm.** The one durable win is the decomp-synth
> worktree fix (below), which also benefits the grind fleet (RFC-12). Redirect
> body-divergence effort to RFC-13 (idiom library) / RFC-12 (get more fns *into*
> the band), not automated permutation of the existing band.

## Summary

The `decomp_synth` permuter already exists, is wired (skill `permute`, 139
patterns, `decomp-synth.json`), and the grind close-out (3342b30/a1312de) proved
the routing `>=90% draft -> permute -> TRUE 100%`. What does *not* exist is a
**standing farm**: many CoW workers each hill-climbing one function, fed a
persistent queue of the [90,100) near-miss band (1,503 at >=95 + 278 at [90,95)),
with per-fn objdiff scoring and results banked. This RFC designs that farm and,
critically, bounds its expected value honestly — the permuter has a documented
**0/all** convergence on the strcpy/regalloc *walls*, so the farm's yield is
gated entirely by target-selection discipline, not by throughput.

## Motivation

- The [90,100) band is 1,781 functions (verified from `report.json`,
  `tools/fuzzy_progress.py`: `[95,100)=1503`, `[90,95)=278`). Every one already
  compiles and is pinned (a target `.obj` exists) — the precondition the
  permuter needs. This is the single largest pool of "one codegen residue away
  from STRICT" functions in the binary.
- Each closed near-miss is a **+1 STRICT function** and, because these are WIRED,
  moves the primary fuzzy denominator toward 100% too (WIRED fuzzy is 94.602%
  today over 1,392,316 attempted bytes).
- The manual permuter loop is human-in-the-loop and serial. A farm removes the
  human, parallelizes across CoW worktrees, and persists results so the same
  function is never re-climbed from scratch.
- This is the *closer* half of the grind pipeline. Sibling `12-grind-fleet-v2.md`
  designs the *drafter* (LLM near-miss drafting); this RFC is what consumes a
  `>=90%` draft and grinds the last mile of regalloc/decl-order exactness. The
  two compose: fleet lifts 70-90% -> 90%+, farm closes 90%+ -> 100%.

## Current state (verified)

**STRICT** (main @a1312de, `report.json`): 11,240 / 65,619 fns (17.129%);
962,656 / 11,074,108 code bytes (8.693%).

**Near-miss band** (`tools/fuzzy_progress.py` HISTOGRAM, live):

| band | count |
|---|---:|
| ==100 | 11,240 |
| [95,100) | 1,503 |
| [90,95) | 278 |
| [80,90) | 154 |
| [50,80) | 232 |
| (0,50) | 213 |

The [90,100) = **1,781** functions is the primary farm target. (Brief's
"1,503 / 278" numbers verified exactly.)

**What already exists (verified in repo / `../decomp-synth`):**

- **`decomp_synth` package** — installed `pip install -e` into the shared venv
  (`venv` symlinks to `../decomp-synth/decomp_synth`). Real, substantial:
  **139 pattern modules** under `decomp_synth/patterns/` (counted:
  `ls patterns/*.py | grep -v base.py | wc -l` = 139), an AST scanner
  (`ast_queries.py`), hill climber (`hill_climber.py`), chain/beam/evolutionary
  search (`beam_search.py`, `evolutionary.py`, `composer.py`), constraint solver
  (`constraint_solver.py`), and Ghidra-guided patterns (`ghidra_ast.py`).
- **Skill `permute`** (`.claude/skills/permute/SKILL.md`) — wraps
  `decomp_synth.scan_and_permute`, config-driven via `decomp-synth.json` at repo
  root (title `45410914`, MSVC PPC, `.obj`/`/Fo`, `objdiff_map` obj layout,
  `objdiff-cli`). Variants compile to `/tmp`; apply-on-win only.
- **Batch drivers** — `decomp_synth/batch_sweep.py` (feeds a triage report
  through the climber, `--jobs N` parallel *by source file*, `ProcessPoolExecutor`),
  `batch_auto.py` (queries `decomp.db` for workable fns, resume file),
  `batch_triage.py`, `batch_unit_climber.py`, `batch_validate.py`.
- **Harvest** — `decomp_synth/harvest.py` already batch-harvests sweep wins
  **into a worktree** (`--worktree` required) recording verified match%.
- **Queue tool** — `scripts/permuter_targets.py rank` builds a ranked queue
  straight from `report.json` (no build, no db), scoring
  `band_weight * size_factor * icf_factor` and flagging STL-template ICF names
  (down-ranked 0.15x). Output `permuter_targets.json` (verified present, 647
  fns at 80-99.99%, 616 real / 31 likely-ICF) + `permuter_targets.txt`.
- **Skills `permuter-sweep`, `permuter-sweep-fresh`** — agent workflows that
  already run the permuter on verified permuter-class near-misses, one agent per
  unit in a worktree.

**Measured timings (this session, verified):**

- Permuter *scan* phase on one fn (`SharedGroup::TryPoll`): **~2.0s wall**
  (0.8s pattern scan over 139 patterns). No compiles in dry-run.
- objcache warm full rebuild: **3.5s / 778 hits / 5 misses** (CLAUDE.md,
  measured); touched-TU incremental **~0.8s** (CLAUDE.md). This is the
  per-variant compile cost floor: **~0.8s/TU warm**.
- `objdiff-cli` per-fn diff is available (`bin/objdiff-cli` ->
  `../objdiff/target/release/objdiff-cli`); the skill scores each variant with it.

**What does NOT exist / is broken (verified — important honesty):**

- **`scripts/orchestrator/decomp.db` is empty (0 bytes).** The populated db is
  the untracked `./decomp.db` (22 MB) at repo root. `batch_auto.py` queries
  `decomp.db` for its queue — a farm must point at the right db or (better) at
  `report.json`.
- **`./decomp.db` is stale (max `updated_at` = 2026-07-03)** and its per-fn
  pattern flags are **entirely unpopulated**: `detected_patterns` is NULL on all
  69,741 rows, and every `has_register_swap`/`has_offset_swap`/... flag sums to
  **0** on the [90,100) band. Its `best_percent` band ([90,100)=954) does **not**
  match the live `report.json` (1,781). *The db cannot be trusted for triage
  today* — the farm must derive its queue from `report.json` (as
  `permuter_targets.py` already does), not from the db's flag columns.
- **The 2026-06-24 pivot recorded the permuter converging `0/all`** on the
  class-B / strcpy-NUL-terminator near-misses it was pointed at (10rd/100var,
  chain-depth 5) — `docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md`.
  Same doc: fresh worktrees "ha[d] no `functions` table -> permuter
  symbol-resolution returns 0 candidates" (a real setup bug the farm must avoid).
  **This is the central tension**: the permuter is a *closer* for genuine
  regalloc/decl-order residue, and a *waste of compute* on instruction-selection
  walls. Target selection is the whole game (see Risks).

## Proposal

### A. Queue: `report.json`-derived, banded, ICF-filtered, per-fn state

Do **not** depend on `decomp.db`'s stale flag columns. Build the queue from
`report.json` (the live source of truth), which `scripts/permuter_targets.py`
already does. Extend it into a persistent farm queue:

```
scripts/permuter_farm/queue.py build \
    --report build/45410914/report.json \
    --min-pct 90 --max-pct 99.99 \
    --exclude-icf \                 # drop ?$vector@/_Rb_tree/... (ICF_NAME_MARKERS)
    --state permuter_farm_state.sqlite
```

`permuter_farm_state.sqlite` (new, gitignored, regenerable) holds one row per
candidate: `symbol, unit, size, start_pct, best_pct, last_climbed_at,
rounds_spent, winning_patterns, verdict (OPEN|WON|EXHAUSTED|WALL), notes`. On
each rebuild the queue is *reconciled* against `report.json` (a fn that reached
100% via any other lever is retired; new near-misses are enqueued). This makes
the farm **idempotent and resumable** and prevents re-grinding EXHAUSTED fns —
the missing piece today (skills re-scan from scratch each invocation).

Ranking (reuse `permuter_targets.py` scoring, extended):
`score = band_weight(90-95 -> 1.5 ; 95-99 -> 2.0 ; 99-99.99 -> 3.0)
         * size_factor(<=128B: 1.0, decaying to ~0.3 by 1KB)
         * icf_factor(0.15 if STL-template name else 1.0)
         * novelty(0.2 if verdict in {EXHAUSTED,WALL} else 1.0)`.
Small + close + non-ICF + not-yet-exhausted floats to the top. This matches the
permuter's documented sweet spot (`permuter_targets.py` docstring: "wins most on
SMALL functions CLOSE to 100% whose residue is real codegen").

### B. Workers: N CoW worktrees, warm objcache, one fn at a time

Each worker is a `setup_worktree.sh` CoW worktree under `~/tmp` (NOT `/tmp` —
tmpfs has no btrfs reflink; CLAUDE.md). Warm-seeded so the first build is a
0-compile no-op, and every variant compile is an objcache-warm **~0.8s/TU**.

```
scripts/permuter_farm/worker.sh <worktree> <state.sqlite>
  loop:
    fn = claim_next_open(state)          # atomic UPDATE ... WHERE verdict='OPEN'
    if none: exit
    setup functions-table shim           # <-- FIX the 0-candidate bug (see below)
    venv/bin/python -m decomp_synth.scan_and_permute \
        --symbol "$fn" --project-dir <worktree> \
        --max-rounds 8 --max-variants 60 --plateau-limit 3 \
        --chain-depth 5 --no-apply --json 2>&1 | tee ~/tmp/permfarm_$fn.log
    record(state, fn, best_pct, winning_patterns, verdict)
    if best_pct == 100: stage patch to <worktree>/permuter_farm/winners/
```

**Fix the "0 candidates in a worktree" bug first** (from the pivot doc):
`scan_and_permute`'s Phase 2 resolves symbols from `decomp.db`. In a CoW worktree
that db is absent or empty. Two options: (1) reflink `./decomp.db` into each
worktree in `setup_worktree.sh` (cheap CoW), or (2) teach `scan_and_permute` to
fall back to `report.json` symbol resolution when the db has no `functions`
table. Option (2) is more robust and should be a one-time patch to
`../decomp-synth` (rebuild is `pip install -e`, no ninja edge). **This bug must
be fixed before any farm run — it silently returns 0 candidates and looks like a
clean no-op.**

Parallelism: `batch_sweep.py` already parallelizes by *source file* within one
worktree (`ProcessPoolExecutor`, `--jobs N`); variants of the *same* fn compile
serially (they touch the same TU). The farm's parallelism is instead **across
worktrees** (one fn per worker), which is cleaner: no TU contention, each worker
owns its build dir. Recommend **6-8 workers** (matches the objcache/CoW infra;
each worktree is ~cheap CoW but each holds a warm build dir).

### C. Scoring: normalized objdiff, RAW-verdict trap guarded

Score each variant with `objdiff-cli diff -p . '<sym>' --build -f json`
(the skill already does this via `decomp_synth`). **Use normalized
match_percent**, and for the final WON gate re-verify against `report.json`
(whole-binary), because RAW verdict reads ~97.5% on byte-exact fns with anonymous
local-static slots (the grind memo's documented trap:
`project_grind_loop_2026-07-07.md` — "dtk target objs leave local-static slots
anonymous, so RAW verdict reads ~97.5 on byte-exact fns; trust
normalized/report.json"). A farm that gates on RAW will discard true wins.

### D. Landing: harvest -> whole-binary A/B -> policy gate

Winners accumulate in each worker's `permuter_farm/winners/`. The coordinator (not
the workers — workers never touch main; CLAUDE.md worktree discipline) runs
`decomp_synth/harvest.py --worktree <wt>` to collect them, then a **cold-cache
whole-binary A/B** (`setup_worktree.sh --cold-cache`; warm CoW can serve stale
objs and fake a net-zero — HONESTY GATES). Land only net-WIRED-positive, 0
regressions, via the sibling `16-auto-landing-pipeline.md` verification lane.
Per-fn permuter wins are pure codegen edits to already-pinned fns, so they can't
inflate the WIRED denominator — the gate is simply `+STRICT, 0 regr`.

### E. Pattern -> diff-signature catalog (which move fixes which residue)

The permuter's 139 patterns exist but there's no doc mapping *move -> diff
signature*. The brief asks for this; here is the verified-from-`patterns/`
catalog (pattern filenames are real; the signature mapping is the design
hypothesis to validate by measuring per-pattern win rate, see Open questions):

| diff signature (objdiff/skill) | source move | permuter patterns (real files) |
|---|---|---|
| **regswap** (GPR/FPR pair swapped) | declaration reorder; first-use reorder; commutative operand swap | `declaration_reorder`, `declaration_movement`, `first_use_reorder`, `commutative_swap`, `assignment_reorder`, `statement_reorder`, `fma_reorder`, `fpr_declaration_reorder`, `fpr_cascade_operand_hoist` |
| **stack-layout** (SWAPPED/SHIFTED slots) | local decl order; temp intro/elim; scope width | `member_init_reorder`, `temp_elimination`, `variable_extraction`, `variable_inline`, `scope_narrowing`, `scope_widening`, `stack_array_hoist`, `slot_pad` |
| **instruction-selection** (cmp/branch/cast form) | signedness; comparison flip; ternary vs if/else; branch polarity | `signed_unsigned`, `signed_unsigned_cast_polarity`, `sizeof_signed_cast`, `type_width_change`, `comparison_flip`, `comparison_equivalence`, `comparison_operator_fix`, `branch_polarity`, `positive_branch_invert`, `ternary_swap`, `clamp_to_ternary`, `fsel_template`, `switch_if_convert` |
| **local-static vs global Symbol** | fn-local `static Symbol` vs global ref | (no dedicated pattern today — **gap**; see `14-systematic-symbol-sweeps.md`. Candidate new pattern `local_static_symbol`) |
| **loop form** | for/while/do-while, rotation, unroll | `loop_rotation_to_while`, `foreach_to_dowhile`, `bare_label_loop_to_while`, `loop_var_hoist`, `loop_condition_cache`, `pointer_iter_unroll` |
| **control-flow / goto** | goto<->structured, early return merge | `goto_to_continue`, `goto_to_return`, `goto_skip_to_ifelse`, `early_return_merge`, `single_return`, `nested_fallback_to_else_if` |

The farm should **record which pattern won** per fn (`winning_patterns` column)
so this table becomes *measured* rather than hypothesized — closing the loop that
`docs/permuter/pattern_stats` was meant to provide. A pattern that never wins
across 200 fns is a candidate to prune (speeds every scan).

### F. The local-static-Symbol gap (highest-leverage new pattern)

The grind close-out discovered retail band3 uses **function-local `static Symbol`**
where our tree uses global `Symbols.h` refs (`?sym@?N??Fn@...@4V3@A` relocs + `$S`
guards); converting also pairs the `??__E`/guard thunks — a direct attack on the
guard-thunk wall (`project_grind_loop_2026-07-07.md`, commit 3342b30). **No
permuter pattern implements this today.** Adding a `local_static_symbol` pattern
(detect global `Symbol` args in a band3 fn, rewrite to a fn-local
`static Symbol s("name")`) would let the farm mechanize a *one-pattern-many-
functions* fix across the band3 sub-100 pool. This is likely the single
highest-EV addition and should be built first. See `14-systematic-symbol-sweeps.md`
for the systematic (non-permuter) framing of the same lever.

## Alternatives considered

1. **Keep the manual `permute` skill, don't build a farm.** Status quo. Works but
   is serial + human-gated + re-scans from scratch. The farm's marginal value is
   *persistence* (never re-grind an EXHAUSTED fn) and *parallelism* (6-8x). If
   the conversion rate turns out near-zero (see Kill criteria), the farm is
   strictly worse than doing nothing — hence the mandatory pilot.

2. **Fix the stale `decomp.db` and drive `batch_auto.py` off it.** Rejected as
   primary path: the db's `detected_patterns`/`has_*` flags are unpopulated and
   its bands disagree with `report.json`. Re-populating it is a separate project
   (sibling `18-metrics-and-dashboard.md`); the farm should not block on it.
   `report.json` is already the honest denominator source.

3. **Run the full 139-pattern set on every fn (max search).** Rejected as
   default: scan is 0.8s but *climbing* all patterns x 60 variants x 8 rounds is
   the cost. Better to let the AST scanner scope patterns per-fn (it already
   does) and rely on the pattern-win telemetry (E) to prune globally.

4. **decomp-permuter (the classic randomizer) instead of `decomp_synth`.**
   `decomp_synth` is the MSVC/X360-adapted, config-driven, AST-scoped successor
   already wired here. The classic decomp-permuter is GCC/MIPS-oriented and
   randomizes at the C level without MSVC codegen priors. No reason to import it.

## Effort & expected value

**Effort:** the machinery is ~80% built. Net new work:
- Fix the worktree `0-candidates` symbol-resolution bug (patch `../decomp-synth`,
  ~half a day). **Blocking.**
- `scripts/permuter_farm/{queue.py,worker.sh,coordinator.sh}` + state sqlite
  (~1-2 days; queue.py is a thin extension of `permuter_targets.py`).
- `local_static_symbol` pattern (~1 day; the highest-EV addition).
- Pilot + calibration (~1 day).

Total ~1 week to a running, calibrated farm.

**Throughput (anchored):** 6-8 workers, each fn ~8 rounds x ~60 variants =
~480 compiles at ~0.8s warm = **~6-7 min/fn worst case**; small [95,100) fns
plateau far earlier (2-3 rounds). Realistic **~2-3 min/fn effective**. 8 workers
=> **~150-240 fn-attempts/hour**, i.e. the entire 1,781-fn band scanned once in
**~8-12 hours** of wall time.

**Conversion rate (honest, anchored to two real data points):**
- *Positive anchor:* the grind close-out routed `>=90% drafts -> permute -> TRUE
  100%` and landed +22 (3342b30/a1312de) — but those were **drafts hand-selected
  as regalloc-residue**, and the "+22" is 2 fns + 20 paired guard thunks, not 22
  independent fn conversions.
- *Negative anchor:* the 2026-06-24 pivot ran the permuter (10rd/100var,
  chain-depth 5) on the class-B/strcpy near-misses and got **0/all**. Those were
  instruction-selection *walls*, not regalloc residue.

The conversion rate is therefore **entirely a function of how much of the [90,100)
band is genuine regalloc/decl-order residue vs instruction-selection walls**. We
do not know this split for the full 1,781 today (the db flags that would tell us
are unpopulated). **Honest estimate: 3-10% conversion on the first full sweep**
(~50-180 fns), heavily front-loaded on the [99,99.99) sub-band where residue is
smallest. This is a *guess with a wide band* — the pilot exists precisely to
replace it with a measured number before committing the full sweep.

Compared to past veins: class-A span harvest was +403 (now exhausted); grind
close-out +22; CharClipGroup micro-pin +partial; Waypoint flip +7. A farm that
delivers **+50-180 STRICT for ~1 week of setup + ~12h compute** would be the best
unexplored lever by ROI **if** the conversion rate holds — and a clean, cheap
KILL if it doesn't.

## Risks & failure modes

1. **Instruction-selection walls dominate the band (primary risk).** If most of
   [90,100) is the strcpy `extsb.`/`cmplwi` family and FP-scheduling walls
   (documented SOURCE- AND PERMUTER-UNREACHABLE, 2026-06-24 pivot + the /J KILL),
   conversion is near-zero and the farm burns compute for nothing. *Mitigation:*
   the pilot measures this first; the ICF filter + size-ranking already skew away
   from walls; record per-fn WALL verdicts to permanently retire them.
2. **The 0-candidate worktree bug** silently returns "clean no-op" (looks like
   the fn is unimprovable when the permuter never ran). *Mitigation:* fix before
   any run; add a smoke assert that Phase 2 resolved >0 candidates.
3. **RAW-verdict false negatives** discard true wins (local-static anon slots).
   *Mitigation:* gate on normalized + `report.json`, per D/C.
4. **Warm CoW serves stale objs -> false net-zero A/B.** *Mitigation:* cold-cache
   A/B at land time (HONESTY GATES).
5. **ICF-fold inflation** if a "win" is actually the fn folding into an existing
   identical COMDAT. *Mitigation:* `icf_alias_check` at land (HONESTY GATES).
6. **Compute cost / fleet contention** with sibling grind fleet
   (`12-grind-fleet-v2.md`) and bodyport waves sharing the same CoW pool.
   *Mitigation:* cap workers; farm is the *closer* stage — schedule it to consume
   what the fleet produces rather than run wide-open concurrently.

## Kill criteria

- **Pilot (mandatory, do this before the full farm):** run the permuter on a
  stratified sample of **80 fns** from [90,100) (40 from [99,99.99), 25 from
  [95,99), 15 from [90,95)), non-ICF, in worktrees, with the symbol-resolution
  bug fixed. Measure fns reaching TRUE 100% (report.json-verified).
  - **<3 wins / 80 (<3.75%) -> KILL the full-band farm.** The band is
    wall-dominated; the permuter is not the lever. Redirect to
    `13-codegen-idiom-library.md` (systematic idiom translation) and
    `12-grind-fleet-v2.md` (get more fns *into* the band first).
  - **3-8 wins -> PILOT-CONTINUE** on the [99,99.99) sub-band only (best ROI),
    do not sweep the whole band.
  - **>8 wins -> PURSUE** the full farm as designed.
- If the `local_static_symbol` pattern, once built, converts **<5 band3 fns**
  across a 100-fn band3 sweep -> drop that pattern (the systematic sweep in
  `14-systematic-symbol-sweeps.md` may still be worth it out-of-band).
- If per-fn effective time exceeds **10 min** at steady state -> the search is
  too deep; cap `--max-rounds`/`--max-variants` down until it's <4 min or KILL.

## Open questions

1. **What is the actual residue-vs-wall split of [90,100)?** Unknown today (db
   flags unpopulated). The pilot answers this and should *also* back-populate the
   `has_*`/`detected_patterns` columns of `./decomp.db` as a byproduct (feeds
   `18-metrics-and-dashboard.md`).
2. **Which patterns actually win?** The E-catalog is a hypothesis. Telemetry
   (`winning_patterns` column) turns it into measured data and lets us prune the
   139-pattern set to a fast core.
3. **Does chain/beam/evolutionary search (`beam_search.py`, `evolutionary.py`,
   `composer.py`) beat single-pattern hill-climbing** on this band, at what
   compile-cost multiple? Worth an A/B once the pilot proves any conversion.
4. **Should the farm and the grind fleet share one queue?** Both consume the
   near-miss band; a shared `state.sqlite` with a `stage` column (DRAFT / CLOSE)
   would prevent double-work. Coordinate with `12-grind-fleet-v2.md` and
   `16-auto-landing-pipeline.md`.
5. **Ghidra-guided patterns** (`ghidra_ast.py`, on-by-default) need the Ghidra
   MCP (port 8002) reachable from each worker. Do they help on X360, or add
   latency for no gain? Measure in pilot with `--no-ghidra` as control.

## References

- `.claude/skills/permute/SKILL.md` — the wired permuter skill + invocation.
- `decomp-synth.json` (repo root) — permuter project config (title 45410914,
  MSVC PPC, objdiff_map layout).
- `../decomp-synth/decomp_synth/` — the package: `scan_and_permute.py`,
  `patterns/` (139 modules), `hill_climber.py`, `batch_sweep.py`,
  `batch_auto.py`, `harvest.py`, `beam_search.py`, `constraint_solver.py`.
- `scripts/permuter_targets.py` — the report.json-derived ranked queue (reuse as
  farm queue base); outputs `permuter_targets.json` / `.txt`.
- `docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md` — the
  permuter `0/all` on class-B/strcpy walls; the /J KILL; the worktree
  0-candidate bug (the honesty anchor for this RFC).
- `docs/plans/grind-loop-calibration-2026-07-07.md` +
  `~/.claude/projects/-home-free-code-milohax-rb3-xenon/memory/project_grind_loop_2026-07-07.md`
  — the grind close-out (+22, 3342b30/a1312de), draft->permute routing, RAW-verdict
  trap, local-static-Symbol lever.
- `build/45410914/report.json` — live STRICT + per-fn near-miss bands (queue
  source of truth). `tools/fuzzy_progress.py` — the staircase/histogram.
- `scripts/setup_worktree.sh` (+ `--cold-cache`) — CoW workers under `~/tmp`.
- CLAUDE.md — objcache warm-rebuild timings (3.5s all-hit / 0.8s touched-TU),
  worktree/`~/tmp` discipline, HONESTY GATES.
- Siblings: `12-grind-fleet-v2.md` (the drafter that feeds this farm),
  `13-codegen-idiom-library.md` (systematic idiom translation, the fallback if
  KILLed), `14-systematic-symbol-sweeps.md` (local-static-Symbol as a sweep),
  `16-auto-landing-pipeline.md` (the land gate), `18-metrics-and-dashboard.md`
  (db back-population + ROI accounting).
