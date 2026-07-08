# Unicorn behavioral equivalence — triage lane and secondary credit metric

Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: infra

## Summary

decomp.db's schema, the MCP query filters, `scripts/recon.py`, and the
`unicorn-query` skill all *reference* a unicorn behavioral-equivalence pipeline —
but in rb3-xenon that pipeline is a **hollow interface**: the DB is 0 bytes, the
5,594-LOC `scripts/unicorn_runner/` package does not exist here (only in
dc3-decomp), and the skill points at a `scripts/unicorn/query.py` that isn't
present. This RFC proposes (1) porting the runner from dc3-decomp and populating
verdicts for our pinned units, then using **DIVERGENT+logic** as a fix-agent
triage feed and **EQUIVALENT-at-<100%** as a de-prioritize signal; and (2) a
*secondary* behavioral-equivalence% reported alongside strict/fuzzy — explicitly
never the north star. Honest verdict: the triage lane is worth a bounded pilot;
the secondary metric is low-value and easily becomes metric-creep, so gate it.

## Motivation

The project has two confirmed walls (CLAUDE.md, brief): identification (can't
locate class-B ICF-scattered methods) and body-divergence (correctly-ported fns
stall <100% on MWCC→MSVC codegen). Unicorn equivalence attacks a slice of the
second: for a fn we've already **located and pinned** but that sits at, say,
97%, the question "is the residual a *real behavioral bug* or just
register-allocation/scheduling noise?" decides whether an LLM grind agent should
keep hammering it or whether it's done-in-spirit. Today agents answer that by
eyeballing objdiff, which is slow and inconsistent. A behavioral oracle that
emulates the original and decompiled object code and compares observable output
converts that judgement into a cached DB column.

Two concrete uses:

- **Route work.** `DIVERGENT + class=logic` = a near-miss where the *behavior*
  is wrong, not just the codegen. Those are the highest-value fixes: a real bug
  in an almost-matching function. Feed them to fix agents (grind-fleet, siblings
  12/16).
- **Stop wasted work.** `EQUIVALENT` at <100% = "behavior is correct, only
  codegen differs" — for these the residual is regalloc/scheduling/build-env,
  i.e. the body-divergence wall. Mark them `AT_LIMIT`-adjacent and stop grinding.
  This is pure de-prioritization value: it prevents the grind fleet from burning
  best-of-N budget on functions that cannot reach 100% from source changes.

## Current state (verified)

Verified 2026-07-08 against the live repo (main @a1312de era).

**Strict / fuzzy baselines** (for cross-reference; unchanged by this RFC):

- STRICT: 11,240 / 65,619 fns matched (17.13%); 962,656 / 11,074,108 code bytes
  (8.69%). Verified via `python3` on `build/45410914/report.json`
  (`matched_functions`, `total_functions`, `matched_code`, `total_code`).
- FUZZY: `tools/fuzzy_progress.py` exists and is the fuzzy metric of record
  (see sibling `18-metrics-and-dashboard.md`). Brief's 94.602% WIRED-set figure
  not re-run here — **[UNVERIFIED in this doc]**, deferred to sibling 18.

**Unicorn interface present, engine + data absent — the core finding:**

- `scripts/orchestrator/database.py` fully supports the columns. Migrations add
  them: v8 (`unicorn_verdict`, `unicorn_class`, `unicorn_confidence`,
  `unicorn_tested_at`; line ~351), v9 (`unicorn_reason`; ~371), v15
  (`unicorn_signal_version`, `unicorn_probe_schedule_hash`,
  `unicorn_unmapped_pages_fingerprint`; ~472). `query_functions` filters on
  `unicorn_verdict/_class/_confidence` (~820–893). `get_divergent_logic_functions`
  exists (`WHERE unicorn_verdict='DIVERGENT' AND unicorn_class='logic'`, ~1800).
- `scripts/orchestrator/mcp_server.py` exposes the three filters on
  `query_functions` (~389–419, ~770–805) — the tool schema in this session's
  system prompt lists them.
- `scripts/recon.py` already codes the exact triage logic the brief proposes.
  `_assess()` (~330–348): `EQUIVALENT` at <100% → "likely register allocation /
  scheduling. Consider AT_LIMIT"; `class=build_env` → "unfixable from source";
  `class=regalloc` → "may be fixable with variable reordering"; `class=logic` →
  "real logic difference — needs source fix". It imports the runner via
  `from scripts.unicorn_runner.run import ...` (~63, ~99).
- **BUT:** `scripts/orchestrator/decomp.db` is **0 bytes** (empty placeholder;
  appears untracked in `git status`). `scripts/unicorn_runner/` **does not exist
  in rb3-xenon** — `find` locates it only under `../dc3-decomp` (and its
  worktrees). recon.py's unicorn imports are wrapped in `try/except` returning
  `None`, so recon runs but silently emits **zero** unicorn data
  (verified: `python3 scripts/recon.py --symbol nonexistent --no-unicorn`
  → "Low match or no data").
- The `unicorn-query` skill (`.claude/skills/unicorn-query/SKILL.md`) invokes
  `python3 scripts/unicorn/query.py $ARGUMENTS` — **that file does not exist**
  (`scripts/unicorn/` is absent). The skill is dead in rb3-xenon.

**Scout-claim corrections (per rule 2):**

- The brief says "decomp.db already carries unicorn emulation verdicts." **This
  is wrong for rb3-xenon.** The *schema* carries the columns; the *data* is
  empty (0-byte db) and there is **no runner to produce verdicts here.** The
  verdicts exist in **dc3-decomp's** db, not ours.
- The brief lists `unicorn_class` values `logic/regalloc/build_env/fpr_precision`.
  Verified against dc3's live data the real class taxonomy is **much finer** than
  the brief implies. dc3 `decomp.db` (52,504 fns) distribution:
  - Verdicts: `EQUIVALENT` 25,288 | `DIVERGENT` 2,372 | untested (NULL) 24,844.
  - DIVERGENT classes (top): `build_env` 762, `cap_exhausted` 452,
    `call_count` 382, `orig_error` 174, `cap_exhausted_decomp` 155,
    `stack_layout` 115, `call_arg` 64, `wild_jump_match` 59, `merged_call` 59,
    `cap_exhausted_orig` 48, `regalloc` 24, `object_memory` 24, `error` 17,
    `fpr_precision` 14, `return_value` 12, `merged_arg` 9,
    `unmapped_access_mismatch` 2. (The MCP `unicorn_class` enum in this session's
    tool schema lists all of these — brief under-listed.)
  - Confidence: `high` 24,946, `stable_divergent` 2,082, `input_sensitive` 631,
    `fixture_sensitive` 1.
  - Note the classifier (`comparator.py:299–453`) has **retired the broad
    `logic` class into fine sub-classes** (`call_count`, `call_arg`,
    `return_value`, `object_memory`, ...). Pure `logic` is now "remaining
    fallthrough (should be minimal)" — in dc3 it doesn't even appear in the top
    list. **So a naive "DIVERGENT+logic" triage query would return almost
    nothing;** the real actionable buckets are `call_count`/`call_arg`/
    `return_value`/`object_memory`. Any port must update the triage query
    accordingly.

**The runner pipeline (as it exists in dc3-decomp, the port source):**

`../dc3-decomp/scripts/unicorn_runner/` — 18 modules, 5,594 LOC. Key pieces:
- `run.py` — CLI + `_run_comparison_core`, `resolve_unit` (maps unit name →
  `(decomp_obj, orig_obj)` via **`objdiff.json`** `base_path`/`target_path`).
- `coff.py` — COFF parser; `coloader.py` — co-loads intra-TU callees + resolves
  `rel24` targets; `builder.py`/`memory_map.py`/`engine.py` — set up and run
  **Unicorn PPC32 big-endian** emulation of both sides; `comparator.py` —
  `compare` + `classify_divergence`; `prober.py` — struct field-access probing;
  `signal_version.py`, `cache.py`, `research.py`, `diagnose.py`, `bench.py`.
- Design docs: `../dc3-decomp/docs/unicorn_runner/PHASE1_DESIGN.md`.

**Load-bearing constraint:** `resolve_unit` needs a unit with **both**
`target_path` (dtk-split original obj) **and** `base_path` (our compiled obj) in
`objdiff.json`. That means unicorn coverage is bounded by **pinned units**, not
the whole 65k binary. rb3-xenon currently has ~3,091 target objs under
`build/45410914/obj/` and ~801 compiled objs under `build/45410914/*src*`
(verified via `ls`/`find`). So the addressable universe for unicorn here is at
most the pinned/compiled overlap — order-of-thousands of functions, concentrated
in units we've already located.

## Proposal

Two-phase, gated. Phase A is the real value; Phase B is optional and fenced.

### Phase A — Port the runner, populate verdicts, wire the triage lane

**A1. Port `scripts/unicorn_runner/` from dc3-decomp.** Same MSVC-X360
toolchain, same COFF/PPC32-BE emulation target, so the port is mostly
mechanical. Steps:
  - Copy `../dc3-decomp/scripts/unicorn_runner/` → `scripts/unicorn_runner/`.
  - Reconcile `resolve_unit` against rb3-xenon's `objdiff.json` schema (verify
    `base_path`/`target_path` field names match — dc3 and rb3-xenon share the
    objdiff fork, so likely identical).
  - Verify the Unicorn Python dep is in the shared venv (`venv` →
    `../dc3-decomp/venv`; dc3 runs the same runner, so it should already be
    present — confirm `python3 -c "import unicorn"`).
  - Add `scripts/unicorn/query.py` (the skill's expected entrypoint) OR fix the
    `unicorn-query` skill to call the real query path. Cheapest: a thin
    `scripts/unicorn/query.py` that shells `query_functions` on the DB with the
    unicorn filters and prints a table.

**A2. Regenerate the DB and run the probe over pinned units.**
  - `venv/bin/python scripts/ingest_report.py build/45410914/report.json` to
    seed `decomp.db` from scratch (per CLAUDE.md; the current 0-byte db is just
    an uninitialized placeholder).
  - Batch-run the runner over every unit with both `base_path` and `target_path`
    in `objdiff.json`. dc3's `run.py` already supports `ProcessPoolExecutor`
    fan-out. Write verdict/class/confidence/reason back to `decomp.db` (the
    columns and `unicorn_tested_at`/`_signal_version` provenance already exist).
  - Expected wall-time: dc3 has verdicts for ~27,660 fns; rb3-xenon's pinned
    universe is smaller (thousands), so a first full sweep is hours, not days.

**A3. Wire the triage feed.** Add two `query_functions` presets (MCP already
supports the filters):
  - **FIX feed:** `unicorn_verdict=DIVERGENT` AND `unicorn_class IN
    (call_count, call_arg, return_value, object_memory, logic)` AND
    `status=workable` AND `current_percent >= 85`. These are near-miss functions
    with a *real behavioral difference* — the highest-value grind targets. Route
    to sibling `12-grind-fleet-v2.md` / `16-auto-landing-pipeline.md`.
  - **STOP feed:** `unicorn_verdict=EQUIVALENT` AND `current_percent < 100`.
    These are behaviorally correct but codegen-divergent — the body-divergence
    wall. Tag them so the grind fleet *skips* them (or caps attempts at 1). This
    is the de-prioritization lever the brief calls out; recon.py's `_assess`
    already prints the recommendation, so agents get it for free once data
    exists.
  - **UNFIXABLE feed (informational):** `unicorn_class IN (build_env,
    merged_call, merged_arg, stack_layout, fpr_precision, orig_error,
    cap_exhausted*, wild_jump_match)` — never route these to source fixers.

**A4. Integrate into recon.** recon.py already consumes the DB columns; once the
data exists, `/recon <symbol>` and the `recon` skill surface the verdict with no
further work. Confirm the `unicorn-query` skill returns rows (A1 fix).

### Phase B — Secondary behavioral-equivalence metric (FENCED)

**Only if Phase A ships and proves useful.** Add a *reported-but-not-tracked*
number to the dashboard (sibling `18-metrics-and-dashboard.md`):

> behavioral-equivalence over pinned units: (EQUIVALENT count) / (units probed),
> with the denominator **explicitly the probed pinned set**, not the whole binary.

Hard rules to prevent metric-creep (the project explicitly guards against this —
CLAUDE.md "no denominator gaming"):
  - **Strict matched% remains the sole north star.** The equiv-% is annotated
    "secondary / diagnostic only" everywhere it appears.
  - The denominator is stated inline every time (probed pinned units), so it can
    never be mistaken for a whole-binary figure.
  - It is **never** used as a landing gate or a progress claim in commit
    messages. It exists to answer "how much of our located-but-unmatched work is
    behaviorally done vs actually buggy," nothing more.

## Alternatives considered

- **Do nothing / rely on objdiff eyeballing.** Status quo. Works but is slow and
  agent-inconsistent; the STOP signal (don't grind an EQUIVALENT fn) is the part
  humans/agents get wrong most, wasting best-of-N budget.
- **Import dc3's verdicts directly instead of re-probing.** Rejected: dc3 is a
  *different binary* (Dance Central 3). Shared-engine functions that ICF-fold
  identically might transfer, but game code (`band3/`, `network/`) doesn't exist
  in dc3, and even engine fns can diverge. Cross-binary verdict transfer is
  unsound; re-probe against rb3-xenon's own objs.
- **Skip the runner; use unicorn only for field-access probing.** recon.py also
  uses `prober.probe_field_access` for struct offset maps. That's valuable but
  orthogonal — this RFC is about the equivalence verdict. Porting the package
  gets both.
- **Build a fresh runner instead of porting.** Wasteful — the dc3 runner is
  5,594 LOC of matured PPC32-BE emulation with a signal-version/provenance model
  already validated. Port it.

## Effort & expected value

**Effort:**
- A1 port: ~0.5–1 day (mechanical copy + `objdiff.json`/venv reconciliation +
  one query entrypoint). Risk: subtle path/schema drift between the two repos'
  objdiff configs.
- A2 populate: ~0.5 day of wall-clock probe time + a batch-driver script.
- A3/A4 wiring: ~0.5 day (query presets + skill fix + grind-fleet hookup).
- Phase B: ~0.5 day, only if greenlit.
- Total Phase A: ~1.5–2.5 engineer-days.

**Expected value (honest, anchored to this repo's past results):**
- This RFC produces **no direct matches.** It is a **triage amplifier** — its
  value is routing existing grind capacity better. Anchor: the grind loop landed
  **+22** strict (3342b30/a1312de) via best-of-N + merge; the levers that moved
  the needle were class-A span harvest (+403, now exhausted) and surgical
  struct/idiom fixes. Unicorn's plausible contribution is a **modest multiplier**
  on grind throughput: fewer wasted best-of-N runs on EQUIVALENT-but-<100%
  functions, and a cleaner ranked FIX feed.
- **Realistic bound:** if the pinned-unit DIVERGENT+actionable pool is a few
  hundred functions and even 10–20% are true source-fixable near-misses that the
  grind fleet closes, that's a **+20 to +60 strict** ceiling *over multiple
  grind waves* — comparable to one good grind session, not transformative. The
  STOP-signal savings are real but unquantifiable (budget not burned ≠ matches).
- **This does not touch the identification wall at all** (sibling `07`, `08`,
  `09`) — it only helps functions we've *already located and pinned*.

## Risks & failure modes

- **`logic`-class is nearly empty** (verified in dc3: retired into sub-classes).
  A triage query keyed on `logic` returns almost nothing. Mitigation: key on the
  fine actionable classes (`call_count/call_arg/return_value/object_memory`).
  This is the single most likely way a naive implementation delivers "0 targets"
  and looks like a dud.
- **False EQUIVALENT → premature STOP.** If the probe's fixture inputs don't
  exercise a divergent path, it reports EQUIVALENT and we wrongly de-prioritize a
  fixable fn. dc3 mitigates with `confidence` (`stable_divergent` vs
  `input_sensitive`) and multi-probe schedules. Only trust `confidence=high`
  EQUIVALENT for the STOP signal; treat `input_sensitive` as "keep grinding."
- **Coverage is small.** Bounded by pinned units (~thousands), and the *most
  valuable* unmatched functions (class-B scattered) are exactly the ones **not
  pinned** — so unicorn can't see them. The lane helps the already-located
  middle band, not the frontier.
- **Emulation cost / flakiness.** PPC32-BE emulation of arbitrary functions hits
  unmapped pages, indirect branches, syscalls → `cap_exhausted`/`error`/`SKIPPED`
  (dc3: ~700+ such). A large fraction of probes yield no usable verdict.
- **Metric-creep (Phase B).** Any secondary % risks being quoted as progress.
  Fenced by the hard rules above; if it can't stay fenced, drop Phase B.
- **Maintenance drift.** Two copies of a 5.6k-LOC runner (dc3 + rb3-xenon) will
  diverge. Consider a shared-tooling extraction later (mirrors the
  milo-native-engine extraction ambition), but not in scope here.

## Kill criteria

- **Kill Phase A** if, after A2, the actionable FIX pool
  (DIVERGENT + fine-actionable-class + workable + ≥85%) over rb3-xenon's pinned
  units is **< ~50 functions** — too small to justify a triage lane; agents can
  hand-pick instead.
- **Kill Phase A** if probe yield is dominated by `error`/`SKIPPED`/`cap_exhausted`
  (say **> 70%** of pinned units produce no confident verdict) — the emulator
  can't see enough of our code to be a useful oracle.
- **Kill Phase B** the moment the equiv-% appears in a commit message, a landing
  gate, or a progress claim as if it were the north-star metric. It is
  diagnostic-only or it's gone.
- **Kill the whole thing** if a 20-function spot-check shows the verdicts
  disagree with hand objdiff analysis more than ~2/20 times at
  `confidence=high` — an unreliable oracle is worse than none (it mis-routes
  grind budget).

## Open questions

- Does rb3-xenon's `objdiff.json` use the same `base_path`/`target_path` field
  names dc3's `resolve_unit` expects? (Shared objdiff fork suggests yes — verify
  on port.)
- Is `import unicorn` satisfied by the shared `venv` → `../dc3-decomp/venv`, or
  does it need adding? (dc3 runs the runner, so likely yes.)
- Should the FIX feed merge with sibling `08-ml-embedding-triage.md`'s ranking
  (behavioral-divergence as a feature in the ML triage), rather than a separate
  query? Likely yes — unicorn class is a strong triage feature.
- Cross-binary transfer: are there ICF-identical engine fns where dc3's verdict
  *is* sound for rb3-xenon (same folded machine code)? Could pre-seed a subset
  cheaply. Needs `icf_alias_check`-style validation first (honesty gate).
- Whether to extend the probe to run against **rb3-Wii** oracle bodies as a
  third comparison point — but rb3-Wii is MWCC, so bodies diverge by
  construction (brief); probably out of scope.

## References

Verified paths in this repo unless marked otherwise.

- `scripts/orchestrator/database.py` — unicorn column migrations (v8 ~351,
  v9 ~371, v15 ~472), `query_functions` filters (~820–893),
  `get_divergent_logic_functions` (~1775–1804).
- `scripts/orchestrator/mcp_server.py` — MCP `query_functions` unicorn filters
  (~389–419, ~770–805).
- `scripts/orchestrator/decomp.db` — **0 bytes** (empty placeholder; untracked).
- `scripts/recon.py` — triage `_assess()` logic (~330–360); unicorn runner
  imports in try/except (~63, ~99, ~147–192).
- `.claude/skills/unicorn-query/SKILL.md` — points at nonexistent
  `scripts/unicorn/query.py`.
- `.claude/skills/recon/` — the `recon` skill (consumes DB unicorn columns).
- `scripts/ingest_report.py` — seeds decomp.db from `build/45410914/report.json`.
- `build/45410914/report.json` — strict measures (verified figures above).
- `tools/fuzzy_progress.py` — fuzzy metric of record.
- `objdiff.json` — per-unit `base_path`/`target_path` (runner's `resolve_unit`
  input).
- **Port source (external):** `../dc3-decomp/scripts/unicorn_runner/` — 18
  modules, 5,594 LOC (`run.py`, `comparator.py` classifier ~299–453, `coff.py`,
  `coloader.py`, `engine.py`, `prober.py`, `signal_version.py`, ...).
- `../dc3-decomp/docs/unicorn_runner/PHASE1_DESIGN.md` — design doc.
- `../dc3-decomp/decomp.db` — reference verdict distribution (52,504 fns;
  25,288 EQUIVALENT / 2,372 DIVERGENT / 24,844 untested; class + confidence
  breakdowns above).
- `docs/INDEX.md` — audited docs index; `CLAUDE.md` — project framing, metric
  honesty norms, worktree rules.

**Sibling RFCs:**
- `12-grind-fleet-v2.md` — consumer of the FIX feed (routes near-miss grinding).
- `16-auto-landing-pipeline.md` — landing gates for grind output.
- `08-ml-embedding-triage.md` — unicorn class as a triage feature; potential merge.
- `18-metrics-and-dashboard.md` — where the (fenced) secondary equiv-% would live.
- `07-icf-constraint-solver.md`, `09-sibling-title-oracles.md` — the
  identification wall this RFC does **not** address (unicorn needs a pinned unit).
