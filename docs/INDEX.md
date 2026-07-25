# rb3-xenon docs index

Single entry point for every doc under `docs/`. Find the right reference, skip the stale ones.

**Project:** decompilation of Rock Band 3 for Xbox 360 (PowerPC), compiling C++ to matching
retail machine code. Decomp effort concentrates on the **game layer** (`src/band3/`,
`src/network/`); the Milo engine (`src/system/`) is effectively pre-solved via DC3. Full
framing in `../CLAUDE.md` — **read that first**, it is the authoritative current-state doc.

## How to read this index

- **No tag** = current / evergreen reference that matches repo reality.
- **`[HIST]`** = dated historical record (research log, per-task handoff, executed/superseded
  plan). Kept as a searchable archive; its match-counts and "current state" claims are frozen
  at its date. Do not trust these for today's numbers.
- On **2026-07-06** the inherited dtk-template boilerplate docs and several stale current-state
  docs were audited and amended: each now carries a `> **STATUS (2026-07-06):**` banner right
  under its title telling you whether it's accurate, historical, or superseded (and by what).
  If a doc has no banner, it was judged accurate as-is.

## Known traps (read before you touch anything)

- **No leaked map for RB3.** There is NO `ham_xbox_r.map` for this binary. That map is
  **DC3's** (`../dc3-decomp/orig/373307D9/ham_xbox_r.map`); RB3's functions are anonymous
  `fn_8XXXXXXX`. Any doc implying `orig/45410914/ham_xbox_r.map` exists is wrong — symbols come
  from `tools/fingerprint_match.py`, `decomp.db`/`report.json`, and the Ghidra+BinDiff /
  `apply_symbols.py` pipeline. See `tools/GHIDRA.md`, `tools/REFERENCE.md`.
- **Target is a retail `/O1 /Oi /GR /EHsc` size-optimized release with ICF — NOT a debug build
  and NOT LTCG/LTO.** ICF (identical-COMDAT folding) is a separate linker feature that IS active.
  Verdict evidence: `plans/lto-vs-icf-investigation-2026-06-06.md`.
- **Worktrees and build logs go under `~/tmp` (= `/home/free/tmp`), never `/tmp`.** `/tmp` is
  RAM-backed tmpfs with no btrfs reflink — it fills up and defeats CoW. Use
  `scripts/setup_worktree.sh` + `~/tmp/rb3_build_<task>.log`.
- **Match-counts age fast.** Any doc dated ≤ 2026-06 carries a matched-function count from its
  era (e.g. 394, 3919, 6568, 9793). Current progress lives in the orchestrator DB
  (`decomp.db`) / `build/45410914/report.json` and MEMORY.md, not in these docs.
- **Two different "mapped"s — never compare them.** dtk's own progress box says *mapped* for
  bytes **pinned** to a `splits.txt` unit (the prerequisite to matching). `tools/scope_map.py`'s
  dashboard footer counts bytes **classified into a scope tier** by any of its 8 layers, pinned
  or not — always the larger number. The dashboard therefore labels its own axis
  **"tier-classified"**, not "mapped".
- **The scope-tier percentages depend on a gitignored cache.**
  `config/45410914/scope_map.json` is addr-keyed to ONE target build and is not committed. If it
  is absent (fresh checkout), corrupt, or keyed to an older XEX revision, the ~65k anonymous
  `fn_8XXXXXXX` functions fall into `unknown`, the per-tier **denominators collapse to
  pinned-only**, and every tier % in the dashboard reads **INFLATED** and is not comparable to
  main's. The dashboard now prints a banner in that case; the fix is always
  `python3 tools/scope_map.py build` (~1 s). `scripts/setup_worktree.sh` reflinks the cache into
  new worktrees, but a cache produced before a target re-base (e.g. the TU0→TU5 flip) must be
  rebuilt everywhere, main included. The headline `binary NN% matched` line is read straight
  from `report.json` and is always honest.

---

## 1. Start here / current state

- [../CLAUDE.md](../CLAUDE.md) — project framing, build tracks, decomp priority, worktree/git
  rules, toolchain wiring. Authoritative current state.
- [plans/decomp-state-2026-07-19.md](plans/decomp-state-2026-07-19.md) — **live state & veins
  doc** (strict count, PIVOT POINT: cheap veins exhausted → deep grind), updated as waves land.
- [plans/paths-to-100/README.md](plans/paths-to-100/README.md) — **paths-to-100 RFC set
  (2026-07-08, 20 RFCs + ranked index)**: every remaining vein sized against the two walls
  (identification recall, body-divergence), verify-before-assert, with PURSUE/PILOT/DO-NOT
  verdicts and a settled do-not-re-litigate list. Read the README ranking first.
- [plans/frontier-workstreams-2026-07-02.md](plans/frontier-workstreams-2026-07-02.md) —
  tracking doc for the 7 frontier streams (ws1-ws7); superseded as "live state" by
  decomp-state-2026-07-19 above.
- [decomp/handoff/wave-loop-SOP-2026-06-20.md](decomp/handoff/wave-loop-SOP-2026-06-20.md) —
  wave-loop SOP: discover / execute / audit / reduce + harvest/land protocol.
- [plans/rb3enhanced-same-instrument-patch.md](plans/rb3enhanced-same-instrument-patch.md) —
  runtime-mod plan (not decomp): fork RB3Enhanced to add multiple-players-on-one-instrument
  to retail RB3 TU5. Uses rb3-xenon as address oracle. 3 enforcement layers + gem-list
  clone centerpiece (installed at the `RecalcGemList` re-borrow choke-point, not at
  watcher ctor); build/boot pipeline (Xenia `.patch.toml` mechanism resolved), Phase-0
  spikes, fingerprint-based address cookbook. Derived + prologue-verified:
  `IsActive 0x8264B5F8`, `ResolvePartWaitStates 0x8259D948`.

### Active worklists (open work to pull from)

- [plans/band3-port-worklist-loose.md](plans/band3-port-worklist-loose.md) — loose 301-fn
  game-code worklist (0.85 precision), for ws2 regen.
- [plans/sysnet-port-worklist.md](plans/sysnet-port-worklist.md) — strict 290-fn engine+netcode
  worklist (0.967 precision; 46 safe-first core).
- [plans/sysnet-port-worklist-loose.md](plans/sysnet-port-worklist-loose.md) — loose 474-fn
  engine+netcode worklist (BSim 10-15, 0.85 precision).
- [plans/workstreams-2026-07-02/ws2-worklist-regen.md](plans/workstreams-2026-07-02/ws2-worklist-regen.md) — WS2 worklist regen, 775-candidate worklist, open.
- [plans/workstreams-2026-07-02/ws5-caseb-campaign.md](plans/workstreams-2026-07-02/ws5-caseb-campaign.md) — WS5 case-B campaign, partially executed, remaining ids open.
- [plans/workstreams-2026-07-02/ws7-dead-lever-reaudit.md](plans/workstreams-2026-07-02/ws7-dead-lever-reaudit.md) — dead-lever re-audit: 2 CONFIRMED_DEAD, 5 PARTIAL_REOPEN.
- [decomp/handoff/port-frontier-2026-07-02-plan.md](decomp/handoff/port-frontier-2026-07-02-plan.md) — port-frontier wave: 9 TUs ranked with port/pin specs (owner-WIP cautions).
- [decomp/handoff/w5-plan-2026-07-02.md](decomp/handoff/w5-plan-2026-07-02.md) — Wave-5 plan: 7 Opus port lanes + 1 Sonnet pins lane roster + SOP.

---

## 2. Evergreen references

### Build & config formats

- [config.md](config.md) — dtk config format (banner: real config in `config/45410914/config.json`).
- [objects.md](objects.md) — objects.json format + this repo's NonMatching / splits pinning workflow.
- [splits.md](splits.md) — splits.txt per-source-file section-range format.
- [symbols.md](symbols.md) — symbols.txt format (mangled names, addresses, attributes).
- [dependencies.md](dependencies.md) — toolchain deps (banner: sibling forks at fixed paths, not auto-download).
- [getting_started.md](getting_started.md) — [HIST] bootstrap (superseded; project already bootstrapped).
- [github_actions.md](github_actions.md) — [HIST] template CI (the real workflow ran once and failed; verify via `gh`).
- [reference/DATABASE_SCHEMA.md](reference/DATABASE_SCHEMA.md) — decomp.db SQLite schema.
- [reference/FREE60_XEX_FORMAT.md](reference/FREE60_XEX_FORMAT.md) — [HIST] archived XEX format reference.
- [decomp/OBJECT_MATCHING.md](decomp/OBJECT_MATCHING.md) — object-level match requirements: COFF sections, linking, what must match.

### Compiler / codegen references (MSVC X360, same flags as us)

- [decomp/MSVC_X360_REGALLOC.md](decomp/MSVC_X360_REGALLOC.md) — reverse-engineered register allocator; declaration order controls assignment.
- [decomp/TECHNICAL_NOTES.md](decomp/TECHNICAL_NOTES.md) — compiler patterns & session lessons: regalloc, static init, control flow, merged fns.
- [decomp/XBOX360_FLOATING_POINT_CODEGEN.md](decomp/XBOX360_FLOATING_POINT_CODEGEN.md) — FP codegen: `/fp:` flags, contraction pragmas, FPU patterns.
- [decomp/PRAGMA_INDEX.md](decomp/PRAGMA_INDEX.md) — navigation index for the pragma doc suite.
- [decomp/PRAGMA_CODEGEN_SUMMARY.md](decomp/PRAGMA_CODEGEN_SUMMARY.md) — quick reference for pragmas affecting instruction selection.
- [decomp/PRAGMA_MATCHING_CHECKLIST.md](decomp/PRAGMA_MATCHING_CHECKLIST.md) — step-by-step guide to applying pragmas.
- [decomp/XBOX360_PRAGMA_REFERENCE.md](decomp/XBOX360_PRAGMA_REFERENCE.md) — complete X360 pragma reference: scope rules, flag interactions.

### Matching methodology

- [decomp/patterns/INDEX.md](decomp/patterns/INDEX.md) — **master pattern index** (fixable/unfixable/harmful codegen patterns; start here for a specific mismatch).
- [decomp/playbooks/README.md](decomp/playbooks/README.md) — playbook overview + shared invariants.
- [decomp/playbooks/bodyport-wave.md](decomp/playbooks/bodyport-wave.md) — body-port campaign playbook.
- [decomp/playbooks/hasreal-grind.md](decomp/playbooks/hasreal-grind.md) — HAS_REAL near-miss grind playbook.
- [decomp/playbooks/nearmiss-harvest.md](decomp/playbooks/nearmiss-harvest.md) — evaluation-order-sculpting harvest waves (96–99.99% named fns; local-.cpp-only lanes; technique catalog + wall taxonomy).
- [decomp/playbooks/offset-drift-sweep.md](decomp/playbooks/offset-drift-sweep.md) — mechanical layout/header-drift sweep (85–99.99%; the header-edit complement of nearmiss-harvest; one fix closes many fns; recon-before-edit discipline).
- [decomp/patterns/false-layout-drift.md](decomp/patterns/false-layout-drift.md) — offset diffs that are NOT layout bugs (anchor-bias, vbase mirage, diagonal pairing); rule out before editing a header.
- [decomp/UPSTREAM_PORT_WORKFLOW.md](decomp/UPSTREAM_PORT_WORKFLOW.md) — porting matching impls from DC3 / rb3-Wii when theirs is closer.
- [decomp/identity-transfer.md](decomp/identity-transfer.md) — per-function identity transfer for ICF-scattered TUs (case-A vs case-B).
- [decomp/pin-candidates.md](decomp/pin-candidates.md) — unified oracle→pin ranker: 5 oracle sources → consensus tiers + ranked splits wave.
- [decomp/callgraph-triangulation.md](decomp/callgraph-triangulation.md) — vote rb3 anonymous fns via anchor callsites vs dc3 named fns.
- [decomp/rtti-vtable-transitivity.md](decomp/rtti-vtable-transitivity.md) — transfer dc3 vtable slot names onto rb3 anonymous fns via RTTI+vtable.
- [decomp/handoff/objdiff-caseb-fork-banked.md](decomp/handoff/objdiff-caseb-fork-banked.md) — banked objdiff fork: case-B cross-unit identity transfer + landing gate.
- [decomp/handoff/worktree-build-tooling-findings-2026-07-01.md](decomp/handoff/worktree-build-tooling-findings-2026-07-01.md) — worktree build findings: SIGPIPE fix, scoped prime, PCH reflink (refuted).
- [plans/engine-reuse-and-asset-rendering.md](plans/engine-reuse-and-asset-rendering.md) — proof DC3 engine renders RB3-360 assets; why decomp value is in game layer (CLAUDE.md-referenced).
- [plans/coupled-base-and-body-port-playbook.md](plans/coupled-base-and-body-port-playbook.md) — [HIST] reference playbook: coupled-base (family blast) vs body-port classes.

---

## 3. Tooling

### Hardware / live debugging (RB3Enhanced on the console)

- [tools/LIVE-DEBUG-RUNBOOK.md](tools/LIVE-DEBUG-RUNBOOK.md) — **the live-debugging runbook**:
  console facts + topology (direct `192.168.8.180`; the relay-era
  `tools/oss-xbox-build/xbox.sh` is deprecated), `build-si.sh` edit→run loop,
  observability channels (XBDM notify / RB3E UDP / HTTP), `/execute` live DTA
  introspection (returns evaluation results), `xdbg` crash capture, recovery ladder.
- [../tools/oss-xbox-build/BUILD-AND-DEPLOY.md](../tools/oss-xbox-build/BUILD-AND-DEPLOY.md) —
  build/pack/deploy pipeline internals (XDK-free compile, load-critical `xextool -m d -c c`
  compress step, 8 hard-won gotchas).
- [plans/si-hw-fix/README.md](plans/si-hw-fix/README.md) — SI hardware campaign entry point:
  `DEBUG-WORKFLOW.md` (crash→analyze→hook-fix loop + crash ledger), load-blocker
  root-cause record, worked crash traces.

### Ghidra / decompiler

- [tools/GHIDRA.md](tools/GHIDRA.md) — primary Ghidra MCP integration doc (banner: DC3-map assumptions relabeled; RB3 uses fingerprint/apply_symbols pipeline).
- [tools/GHIDRA_SETUP.md](tools/GHIDRA_SETUP.md) — quick Ghidra setup + RB3 disclaimers + XEX loader integration.
- [tools/GHIDRA_MANUAL_SETUP.md](tools/GHIDRA_MANUAL_SETUP.md) — GUI-only setup (no MCP) for manual import/analysis.
- [tools/XEXLOADERWV.md](tools/XEXLOADERWV.md) — XEXLoaderWV Ghidra extension for X360 binary loading.

### objdiff / analysis / orchestrator

- [tools/INDEX.md](tools/INDEX.md) — **tool-selection index** (MCP orchestrator tools, Ghidra CLI, analysis utilities).
- [tools/REFERENCE.md](tools/REFERENCE.md) — command reference for symbol lookup (banner: no RB3 map; corrected pointers).
- [tools/WORKFLOW.md](tools/WORKFLOW.md) — decomp tool workflow narratives (new fns, near-matches, pattern analysis).
- [tools/UNICORN_FUNCTION_RUNNER.md](tools/UNICORN_FUNCTION_RUNNER.md) — Unicorn differential function execution (PPC32 BE emulation).
- [tools/objdiff/CLI_OPTIONS.md](tools/objdiff/CLI_OPTIONS.md) — objdiff-cli options, output formats, pattern detection.
- [tools/objdiff/USAGE.md](tools/objdiff/USAGE.md) — extended objdiff-cli reference (report queries, analysis, markdown).
- [tools/objdiff/JSON_EXTENSIONS.md](tools/objdiff/JSON_EXTENSIONS.md) — milohax fork extensions: data-symbol diffs + CFG structures.
- [tools/objdiff/LEARNINGS.md](tools/objdiff/LEARNINGS.md) — patterns, diagnostics, fixability decision trees from objdiff work.
- [tools/objdiff/AGENT_WORKFLOW.md](tools/objdiff/AGENT_WORKFLOW.md) — [HIST] DC3-heritage design note (live workflow is the orchestrator MCP tools).
- [tools/orchestrator/INCREMENTAL_BUILDS.md](tools/orchestrator/INCREMENTAL_BUILDS.md) — incremental vs full build strategy + metrics.

### Permuter

- [permuter/INDEX.md](permuter/INDEX.md) — **C++ permuter doc index**: patterns, CLI, architecture, beam/hill-climb search.
- [permuter/guided-permuter.md](permuter/guided-permuter.md) — diagnosis-guided permutation using objdiff mismatches.
- [permuter/bsf-engine.md](permuter/bsf-engine.md) — BSF register-allocation tracing for guided declaration reordering.
- [permuter/evolution/OVERVIEW.md](permuter/evolution/OVERVIEW.md) — permuter architecture upgrade (SourceEditor, ast_queries); phases 1-3.
- [decomp/patterns/PERMUTER_ROI_ANALYSIS.md](decomp/patterns/PERMUTER_ROI_ANALYSIS.md) — permuter coverage vs documented patterns; ROI rankings.

### LLM grind loop / OSS-model eval / training data (2026-07-07..19)

- [decomp/eval-ledger.md](decomp/eval-ledger.md) — **THE standing scoreboard** for the frozen 50-fn eval bench + SIGNAL/NOISE noise threshold; appended by `scripts/grind/bench.sh`.
- [plans/grind-r3-trace-review-2026-07-19.md](plans/grind-r3-trace-review-2026-07-19.md) — trace review verdict: harness bugs (stdout-swallow, 8 unspliceable fns) dominate compile-fails; ranked prompt levers + ceiling estimate. Full findings JSON in `decomp/research/`.
- [plans/grind-tooling-effectiveness-review-2026-07-19.md](plans/grind-tooling-effectiveness-review-2026-07-19.md) — Fable Q1-Q7 tooling audit: sync-drift, tool ROI, metric honesty; do-now vs re-baseline action list.
- [plans/grind-data-leverage-execution-2026-07-18.md](plans/grind-data-leverage-execution-2026-07-18.md) — **current state of record**: 7-thread options catalog, dispatch results, verdicts (token fix, gold-clone finding, replay verdict).
- [plans/grind-oreval-data-leverage-review-2026-07-16.md](plans/grind-oreval-data-leverage-review-2026-07-16.md) — the underlying review: two-piles diagnosis, token-length constraint, ranked opportunities.
- [decomp/training-corpus-annotations.md](decomp/training-corpus-annotations.md) — per-run corpus verdicts (sft+/partial/hard-neg/junk lanes) + normalizer provenance rules.
- [plans/grind-loop-calibration-2026-07-07.md](plans/grind-loop-calibration-2026-07-07.md) — the grind loop itself (decomp-synth bootstrap_loop port + live calibration).
- [plans/grind-agentic-tools.md](plans/grind-agentic-tools.md) — `--agent-tools` mode (model requests read/struct/asm/ghidra mid-attempt); landed 2026-07-10.
- [plans/grind-training-data-capture.md](plans/grind-training-data-capture.md) — lossless attempt capture (RFC-21 T4) → B2 → corpus.py sync design.
- [plans/grind-openrouter-tiered-eval.md](plans/grind-openrouter-tiered-eval.md) — OSS-model tiered eval campaign design (metrics defs used by eval_report.py §5).
- [plans/grind-teacher-critique-rlhf.md](plans/grind-teacher-critique-rlhf.md) — teacher-critique / reasoning-rewrite design (v0 critique-only shipped 0bc326bc).
- [plans/grind-followups-batch2-2026-07-18.md](plans/grind-followups-batch2-2026-07-18.md) — execution design for replay-refine + roster sweep + standing bench follow-ups.

### VMX128 (Ghidra SLEIGH support)

- [vmx128/README.md](vmx128/README.md) — VMX128 Ghidra support overview; phases 1-4 (13,836 instructions validated).
- [vmx128/ISA_REFERENCE.md](vmx128/ISA_REFERENCE.md) — VMX128 instruction set reference.
- [vmx128/REGISTER_ENCODING.md](vmx128/REGISTER_ENCODING.md) — VMX128 7-bit register field encoding.
- [vmx128/VCMPBFP128_SEMANTICS.md](vmx128/VCMPBFP128_SEMANTICS.md) — vcmpbfp128 semantics (2-bit result codes per lane).
- [vmx128/GHIDRA_IMPLEMENTATION.md](vmx128/GHIDRA_IMPLEMENTATION.md) — all 77 instructions with full pcode semantics.
- [vmx128/DC3_VMX128_USAGE.md](vmx128/DC3_VMX128_USAGE.md) — DC3 binary VMX128 usage analysis (37,020 instructions).
- [vmx128/COMPARISON_REPORT.md](vmx128/COMPARISON_REPORT.md) — stock vs modified Ghidra validation on DC3.
- [vmx128/REFERENCE_SOURCES.md](vmx128/REFERENCE_SOURCES.md) — authoritative VMX128 doc sources + local clone paths.
- [vmx128/TESTING.md](vmx128/TESTING.md) — VMX128 Ghidra headless testing/validation guide.
- [vmx128/PLAN.md](vmx128/PLAN.md) · [vmx128/PHASE4_TODO.md](vmx128/PHASE4_TODO.md) · [vmx128/GESTURE_TARGETS.md](vmx128/GESTURE_TARGETS.md) · [vmx128/SESSION_CONTEXT.md](vmx128/SESSION_CONTEXT.md) · [vmx128/SESSION_HANDOFF.md](vmx128/SESSION_HANDOFF.md) — [HIST] plan/session snapshots.

---

## 4. Archives

Dated, append-only records. Descriptions preserved so the archive stays greppable; treat all
numbers and "current state" as frozen at the doc's date.

### 4a. Research — dated investigation records (all [HIST])

- [decomp/fuzzy-reconstruction-frontier-2026-06-21.md](decomp/fuzzy-reconstruction-frontier-2026-06-21.md) — state snapshot (9793 matched): body-divergence wall, oracle inventory, tooling.
- [decomp/near-miss-classification-2026-06-06.md](decomp/near-miss-classification-2026-06-06.md) — near-miss root-cause classification + ranked lever list.
- [decomp/partial-match-porting-strategy.md](decomp/partial-match-porting-strategy.md) — partial→100% conversion model; baseline 3919 matched.
- [decomp/matng-deferral.md](decomp/matng-deferral.md) — Mat_NG deferred: member reorder too risky for shared Mat.h.
- [decomp/string-layout-gap.md](decomp/string-layout-gap.md) — RESOLVED: String/FilePath layouts byte-identical to dc3; do NOT touch.
- [decomp/plans/fuzzy-locator-reconstruction-design.md](decomp/plans/fuzzy-locator-reconstruction-design.md) — locator-first reconstruction (pilot FALSIFIED per its own update).
- [decomp/plans/codegen-matcher-investment-prompt.md](decomp/plans/codegen-matcher-investment-prompt.md) — option-B codegen-matcher prompt (executed → KILL verdict).
- [decomp/plans/post-codegen-kill-streams-2026-06-30.md](decomp/plans/post-codegen-kill-streams-2026-06-30.md) — post-codegen-KILL streams; executed, +3..+9.
- [decomp/identity-transfer/PIPELINE-DESIGN.md](decomp/identity-transfer/PIPELINE-DESIGN.md) — identity-transfer pipeline design; bottleneck = source-port byte-exactness.
- [decomp/identity-transfer/B2-FINDINGS-oracle-wall.md](decomp/identity-transfer/B2-FINDINGS-oracle-wall.md) — B2: oracle VA misattribution dominant; vein thin.
- [decomp/identity-transfer/research/01-tooling-audit.md](decomp/identity-transfer/research/01-tooling-audit.md) — tooling audit; binding constraint = body divergence.
- [decomp/identity-transfer/research/02-objdiff-caseb-fork.md](decomp/identity-transfer/research/02-objdiff-caseb-fork.md) — objdiff case-B fork audit: do-no-harm passes; banked.
- [decomp/identity-transfer/research/03-backlog-inventory.md](decomp/identity-transfer/research/03-backlog-inventory.md) — backlog: 375 eligible game TUs, 975 realA case-A methods.
- [decomp/identity-transfer/research/04-sourceport-bottleneck.md](decomp/identity-transfer/research/04-sourceport-bottleneck.md) — source-port bottleneck: BandProfile 0/64 dissected.

The `decomp/research/` folder is a dense, dated investigation log (~100 files, 2026-06-10 →
2026-07-02) covering the body-port waves (w3-w13), hash_map cluster hunts, Handle-macro reveal
cascades, pin audits, and the structural-levers-exhausted capstone. Notable capstones/entry points:

- [decomp/research/2026-06-21-structural-levers-exhausted-capstone.md](decomp/research/2026-06-21-structural-levers-exhausted-capstone.md) — CAPSTONE: cheap + structural matching levers exhausted (June).
- [decomp/research/2026-06-22-classA-tupure-harvest-results.md](decomp/research/2026-06-22-classA-tupure-harvest-results.md) — Class-A TU-pure span harvest: +126 composed w3-w13; vein thinning.
- [decomp/research/2026-06-22-dc3-oracle-built-engine-naming-dead.md](decomp/research/2026-06-22-dc3-oracle-built-engine-naming-dead.md) — DC3↔RB3 BinDiff oracle built; engine naming dead for strict, alive for fuzzy.
- [decomp/research/2026-06-23-dc3-drain-and-sonnet-opus-pipeline.md](decomp/research/2026-06-23-dc3-drain-and-sonnet-opus-pipeline.md) — DC3-oracle drain exhausted at +46 strict; Sonnet/Opus pipeline.
- [decomp/research/2026-06-30-nearmiss-codegen-inventory.md](decomp/research/2026-06-30-nearmiss-codegen-inventory.md) — near-miss codegen inventory (feeds post-codegen kill streams).
- [decomp/research/2026-06-21-dc3-engine-oracle-feasibility.md](decomp/research/2026-06-21-dc3-engine-oracle-feasibility.md) — DC3 engine body-oracle feasibility GO; game-layer Wii wall.
- [decomp/research/2026-07-18-anthropic-adaptive-thinking-capture.md](decomp/research/2026-07-18-anthropic-adaptive-thinking-capture.md) — REFERENCE: Sonnet-5/Opus-4.8 thinking capture — legacy `budget_tokens` shape returns EMPTY thinking text (display defaults "omitted"); must use `{"type":"adaptive","display":"summarized"}` + `output_config.effort`.

The remaining `decomp/research/*` files are per-lever / per-TU scout logs — grep the folder by
TU name (SongMgr, SongStatusMgr, BandSongMgr, UIComponent, Waypoint, Campaign, SavedSetlist,
Handle-macro families, hash_map clusters) when reviving a specific investigation. Their
one-line descriptions are catalogued in the 2026-07-06 audit
(`~/tmp/docs-audit-2026-07-06.md`, "decomp/research" section).

### 4b. Handoff — per-task agent records (all [HIST] unless noted above)

Per-TU / per-wave landing records from the port campaigns (mostly 2026-07-01/02). Grep by TU:

- CharClipGroup: [handoff/charclipgroup-flip-RESULT-2026-07-02.md](decomp/handoff/charclipgroup-flip-RESULT-2026-07-02.md) · [handoff/charclipgroup-objvector-flip-READY.md](decomp/handoff/charclipgroup-objvector-flip-READY.md) (banner: superseded by RESULT).
- Member-delta / MetaPanel / span waves: [handoff/exec-r1-member-delta-run-2026-07-02.md](decomp/handoff/exec-r1-member-delta-run-2026-07-02.md) · [handoff/exec-r2-metapanel-run-2026-07-02.md](decomp/handoff/exec-r2-metapanel-run-2026-07-02.md) · [handoff/exec-r3-span-confirm-run-2026-07-02.md](decomp/handoff/exec-r3-span-confirm-run-2026-07-02.md).
- ws1/ws3/ws4 exec: [handoff/exec-ws1-waveA-run-2026-07-02.md](decomp/handoff/exec-ws1-waveA-run-2026-07-02.md) · [handoff/exec-ws1-waveA-p1-verdicts.md](decomp/handoff/exec-ws1-waveA-p1-verdicts.md) · [handoff/exec-ws3-optionc-run-2026-07-02.md](decomp/handoff/exec-ws3-optionc-run-2026-07-02.md) · [handoff/exec-ws4-round3-run-2026-07-02.md](decomp/handoff/exec-ws4-round3-run-2026-07-02.md) · [handoff/round3-shared-header-followups-2026-07-02.md](decomp/handoff/round3-shared-header-followups-2026-07-02.md).
- ws3 p2-p4: [handoff/ws3-p2-motionblur-softparticles-2026-07-02.md](decomp/handoff/ws3-p2-motionblur-softparticles-2026-07-02.md) · [handoff/ws3-p3-moggclip-2026-07-02.md](decomp/handoff/ws3-p3-moggclip-2026-07-02.md) · [handoff/ws3-p4-navlist-scantool-2026-07-02.md](decomp/handoff/ws3-p4-navlist-scantool-2026-07-02.md).
- BeatMatcher/BeatMatchController: [handoff/w3-port-beatmatcher-handoff.md](decomp/handoff/w3-port-beatmatcher-handoff.md) (banner: verified & pinned, 1 revert) · [handoff/port-beatmatchcontroller-handoff.md](decomp/handoff/port-beatmatchcontroller-handoff.md).
- w3 ports: [handoff/w3-joypadcontroller-handoff.md](decomp/handoff/w3-joypadcontroller-handoff.md) · [handoff/w3-pins-handoff.md](decomp/handoff/w3-pins-handoff.md) · [handoff/w3-port-gemtrackdir-handoff.md](decomp/handoff/w3-port-gemtrackdir-handoff.md) · [handoff/w3-sliptrack-handoff.md](decomp/handoff/w3-sliptrack-handoff.md).
- w5 wave: [handoff/w5-closure-2026-07-02.md](decomp/handoff/w5-closure-2026-07-02.md) · [handoff/w5-bandlist-handoff.md](decomp/handoff/w5-bandlist-handoff.md) · [handoff/w5-baseguitartrackwatcherimpl-handoff.md](decomp/handoff/w5-baseguitartrackwatcherimpl-handoff.md) · [handoff/w5-endingbonus-handoff.md](decomp/handoff/w5-endingbonus-handoff.md) · [handoff/w5-notetube-handoff.md](decomp/handoff/w5-notetube-handoff.md) · [handoff/w5-sfx-handoff.md](decomp/handoff/w5-sfx-handoff.md) · [handoff/w5-tambourinemanager-handoff.md](decomp/handoff/w5-tambourinemanager-handoff.md) · [handoff/w5-trackwatcher-handoff.md](decomp/handoff/w5-trackwatcher-handoff.md).
- Port-frontier TUs: [handoff/port-ADSR-port-frontier-2026-07-02.md](decomp/handoff/port-ADSR-port-frontier-2026-07-02.md) · [handoff/port-bandpatchmesh-handoff.md](decomp/handoff/port-bandpatchmesh-handoff.md) · [handoff/port-MemMgr-port-frontier-2026-07-02.md](decomp/handoff/port-MemMgr-port-frontier-2026-07-02.md) · [handoff/port-songdata-handoff.md](decomp/handoff/port-songdata-handoff.md) · [handoff/port-songparser-handoff.md](decomp/handoff/port-songparser-handoff.md) · [handoff/port-trackwatcherimpl-handoff.md](decomp/handoff/port-trackwatcherimpl-handoff.md) · [handoff/port-TrackWatcherImpl-port-frontier-2026-07-02.md](decomp/handoff/port-TrackWatcherImpl-port-frontier-2026-07-02.md).
- Landing / reconcile: [handoff/land-bt-land-plan-2026-07-02.md](decomp/handoff/land-bt-land-plan-2026-07-02.md) · [handoff/land-vtd-land-plan-2026-07-02.md](decomp/handoff/land-vtd-land-plan-2026-07-02.md) · [handoff/tambourine-reconcile-2026-07-02.md](decomp/handoff/tambourine-reconcile-2026-07-02.md) · [handoff/verify-ab-reliability-2026-07-01.md](decomp/handoff/verify-ab-reliability-2026-07-01.md).

### 4c. Historical plans (executed / superseded)

- [plans/decomp-state-and-roadmap-2026-06-09.md](plans/decomp-state-and-roadmap-2026-06-09.md) — [HIST] state/roadmap (6568 matched; banner → frontier-workstreams).
- [plans/path-to-100.md](plans/path-to-100.md) — [HIST] original roadmap (394 matched; honest ceiling estimate).
- [plans/execution-schedule.md](plans/execution-schedule.md) — [HIST] dependency-aware roadmap superseding path-to-100.
- [plans/band3-port-worklist.md](plans/band3-port-worklist.md) — [HIST] strict 232-fn game-code worklist — drained.
- [plans/exploratory-techniques.md](plans/exploratory-techniques.md) — [HIST] identification POCs: callgraph, RTTI, vtable transitivity (+2,735 union).
- [plans/lto-vs-icf-investigation-2026-06-06.md](plans/lto-vs-icf-investigation-2026-06-06.md) — [HIST] VERDICT: retail XEX is NOT LTO/LTCG, only ICF (trap-reference).
- [plans/engine-baseclass-layout-bugs.md](plans/engine-baseclass-layout-bugs.md) — [HIST] foundational layout bugs: ObjRef/ObjPtr, ObjectDir, vbptr.
- [plans/objptr-family-relayout-migration.md](plans/objptr-family-relayout-migration.md) — [HIST] ObjRef/ObjPtr re-layout migration (233-file blast).
- [plans/objptr-regression-analysis-2026-05-30.md](plans/objptr-regression-analysis-2026-05-30.md) — [HIST] post-landing ObjPtr: +54 net, 4 regressed units.
- [plans/hmx-object-layout.md](plans/hmx-object-layout.md) — [HIST] Hmx::Object 0x2c→0x28 correction (landed).
- [plans/ui-base-layout-reconstruction.md](plans/ui-base-layout-reconstruction.md) — [HIST] UIComponent retail layout (0x140) reconstructed.
- [plans/structural-readiness-2026-06-03.md](plans/structural-readiness-2026-06-03.md) — [HIST] struct-layout readiness audit (4094-matched era).
- [plans/struct-offset-sweep.md](plans/struct-offset-sweep.md) — [HIST] engine near-misses 82% struct-offset bugs.
- [plans/recon-structural-levers-2026-05-29.md](plans/recon-structural-levers-2026-05-29.md) — [HIST] 5-lever survey: StlNodeAlloc, LightPreset, NgStats, SAVE_REVS, EH funclets.
- [plans/next-levers-2026-05-29.md](plans/next-levers-2026-05-29.md) — [HIST] post-strict-oracle lever ranking.
- [plans/next-wave-onediff-clusters.md](plans/next-wave-onediff-clusters.md) — [HIST] 99%+ one-diff clusters ranked by cause.
- [plans/permuter-readiness.md](plans/permuter-readiness.md) — [HIST] permuter queue generator wired; 240 fns in 80-99.99% band.
- [plans/permuter-sweep-struct-cascades-2026-05-29.md](plans/permuter-sweep-struct-cascades-2026-05-29.md) — [HIST] 151-fn sweep: 1 permuter win.
- Identification / oracle plans: [plans/bindiff-integration.md](plans/bindiff-integration.md) · [plans/bindiff-vs-rb3wii.md](plans/bindiff-vs-rb3wii.md) · [plans/game-code-anchoring.md](plans/game-code-anchoring.md) · [plans/game-code-pairing.md](plans/game-code-pairing.md) · [plans/game-oracle-triage.md](plans/game-oracle-triage.md) · [plans/jeff-vtable-detector.md](plans/jeff-vtable-detector.md) · [plans/jeff-residual-overlaps.md](plans/jeff-residual-overlaps.md) — [HIST].
- Pin/port waves: [plans/pin-tier2-clusters.md](plans/pin-tier2-clusters.md) · [plans/pin-wave-2.md](plans/pin-wave-2.md) · [plans/porting-backlog-ranked.md](plans/porting-backlog-ranked.md) · [plans/porting-wave-1.md](plans/porting-wave-1.md) · [plans/wave5-session-2026-05-28.md](plans/wave5-session-2026-05-28.md) · [plans/wire-missing-config-units.md](plans/wire-missing-config-units.md) · [plans/match-first-fn.md](plans/match-first-fn.md) — [HIST].
- Band3/port-era: [plans/bandobj-port.md](plans/bandobj-port.md) · [plans/meta_band-port-breaking-changes.md](plans/meta_band-port-breaking-changes.md) · [plans/codegen-iteration-targets.md](plans/codegen-iteration-targets.md) · [plans/remaining-matching-work-handoff.md](plans/remaining-matching-work-handoff.md) · [plans/session-handoff-2026-05-27.md](plans/session-handoff-2026-05-27.md) · [plans/instrumentation-patcher-experiment.md](plans/instrumentation-patcher-experiment.md) — [HIST].
- Data artifact: [plans/fingerprint-transfer-backlog-2026-06-06.json](plans/fingerprint-transfer-backlog-2026-06-06.json) — [HIST] fingerprint-transfer backlog snapshot.

### 4d. Buildspeed round-2 campaign ([HIST], landed 2026-07-02)

The wibo-fork + 9-dir PCH + Rust objcache work — all live on main. See CLAUDE.md's build
sections for current mechanics; these are the design/execution records.

- [plans/buildspeed/00-overview.md](plans/buildspeed/00-overview.md) — campaign overview (FULLY LANDED).
- [plans/buildspeed/01-wiring-window-1.md](plans/buildspeed/01-wiring-window-1.md) · [02-objcache-crate.md](plans/buildspeed/02-objcache-crate.md) · [03-pch-verify.md](plans/buildspeed/03-pch-verify.md) · [04-wibo-residual.md](plans/buildspeed/04-wibo-residual.md) · [05-pch-land.md](plans/buildspeed/05-pch-land.md) · [06-objcache-integration.md](plans/buildspeed/06-objcache-integration.md) · [07-wibo-merge-stage.md](plans/buildspeed/07-wibo-merge-stage.md) · [08-objcache-wire-and-wibo-deploy.md](plans/buildspeed/08-objcache-wire-and-wibo-deploy.md).
- [plans/buildspeed/09-worktree-seeding.md](plans/buildspeed/09-worktree-seeding.md) — warm-state `.ninja_log`/`.ninja_deps` seeding (banner: EXECUTED/LANDED).

### 4e. Frontier workstream plans (2026-07-02)

Execution docs behind the master `frontier-workstreams-2026-07-02.md`. ws2/ws5/ws7 have open
work (see §1); the rest are [HIST] executed.

- [plans/workstreams-2026-07-02/ws1-sysnet-drain.md](plans/workstreams-2026-07-02/ws1-sysnet-drain.md) — [HIST] Wave A executed (+46).
- [plans/workstreams-2026-07-02/ws3-optionc-port-then-pin.md](plans/workstreams-2026-07-02/ws3-optionc-port-then-pin.md) — [HIST] option-C harvest (+85).
- [plans/workstreams-2026-07-02/ws4-round3-banked-repair.md](plans/workstreams-2026-07-02/ws4-round3-banked-repair.md) — [HIST] banked repairs (+25).
- [plans/workstreams-2026-07-02/ws6-reconstruction-prep.md](plans/workstreams-2026-07-02/ws6-reconstruction-prep.md) — [HIST] reconstruction prep, awaiting downstream.

### 4f. Non-md data artifacts

Indexed as data (not audited): `decomp/dc3-residual/ranked.json`,
`decomp/gameid/{crossval_agree,VERDICT}.json`, `decomp/matng-abandoned.jsonl`,
`decomp/research/2026-06-11-pin-audit-worklist.json`,
`decomp/research/2026-06-21-{bsim-seedprop-densification,songsortnode-va-confirmation}.json`,
`plans/fingerprint-transfer-backlog-2026-06-06.json`, `images/*.png` (dtk-template screenshots).

---

## Maintenance

- **New docs must be linked here** in the right section — an unlinked doc is invisible to the
  next agent.
- **`decomp/research/` and `decomp/handoff/` are append-only archives.** Add new dated records;
  do not rewrite old ones. When a record is superseded, add a banner pointing forward rather
  than deleting.
- Keep each entry to one line: relative link + ≤120-char description.
- When a plan is executed or superseded, move it into §4 and, if it makes current-state claims,
  give it a `> **STATUS (YYYY-MM-DD):**` banner under its title.
