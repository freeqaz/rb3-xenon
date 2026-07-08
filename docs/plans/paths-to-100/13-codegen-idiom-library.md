# MWCC-to-MSVC codegen idiom library — systematic source-idiom translation

> Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: body-divergence

## Summary

Body-divergence in near-miss functions often reduces to a small set of *repeated*
codegen idiom classes. This RFC evaluates whether we can mine those classes at scale,
solve each *once* at the source level, and apply the fix as a codemod across every
afflicted TU — turning per-function grinding into per-idiom engineering. Verdict:
**PILOT-FIRST, narrow scope.** The mining pipeline already largely exists
(`tools/classify_nearmiss_codegen.py`), and when you actually measure the band the
clusterable-and-source-fixable population is small: the big idiom classes are either
already covered by the permuter, are header-levers owned by sibling RFCs, or are
compiler-internal walls (the strcpy `extsb`/`cmplwi` wall is *closed*). Real EV is
**+10 to +40 strict fns**, not the hundreds the framing implies.

## Motivation

The two confirmed walls (per the shared-facts brief) are IDENTIFICATION and
BODY-DIVERGENCE. This RFC targets the second: functions that are correctly identified,
correctly pinned, and ported from the oracle, yet stall below 100% on MWCC(Wii source)
→ MSVC(X360) codegen differences.

The appealing hypothesis: these residual diffs are not random. If N functions across
M TUs all miss by the *same* idiom (e.g. the same NUL-terminator test shape, the same
bool-materialization, the same ternary-vs-branch selection), then discovering the
source rewrite that fixes idiom X *once* and applying it everywhere is O(idioms) work
instead of O(functions) work. With ~2,167 functions in the `[50,100)` band, even a
10× amortization would be transformational.

The counter-hypothesis this RFC must test honestly: **most residuals are compiler-internal
and source-unreachable, and the ones that aren't are already automated.** This is the
project's documented lived experience (the strcpy wall, the `/J` KILL). An idiom library
is only worth building if the mine surfaces a *new* clusterable, source-fixable population
that the permuter and header-lever RFCs don't already own.

## Current state (verified)

All numbers verified 2026-07-08 against `build/45410914/report.json`,
`tools/fuzzy_progress.py`, and the cached classifier inventory
`/tmp/claude/nearmiss_inventory.jsonl` (1,936 records, `report_pct` range 90.12–99.99).

### The near-miss band (measured, not guessed)

`tools/fuzzy_progress.py` histogram, whole binary:

| band | fn count |
|---|---:|
| `[95,100)` | 1,503 |
| `[90,95)` | 278 |
| `[80,90)` | 154 |
| `[50,80)` | 232 |
| **`[50,100)` total** | **2,167** |

Confirmed independently by walking `report.json` `match_percent_normalized`:
`[50,100) = 2,167`. Of the RB3-specific (game) slice: **band3 = 306**, network = 5
in `[50,100)`. So the game-code near-miss frontier this RFC could touch is ~311 fns;
the rest is engine (DC3 oracle, byte-exact still expected) plus vendor.

### The mining pipeline already exists — and it already clusters

`tools/classify_nearmiss_codegen.py` (verified present, runs read-only against
already-built objs via `objdiff.json` paths — no ninja, safe on the shared tree) does
*exactly* the mine-and-cluster the brief proposes. It runs the freeqaz **objdiff fork's**
pattern detector (`bin/objdiff-cli diff --batch --analyze --verdict`), strips the
source-immune NOISE classes (`LINKER_MERGED` = ICF, `ADDRESS_RELOCATION_NOISE` = .text
layout), and assigns each function a PRIMARY codegen class with a **reachability label**
(`permuter_decl` / `scheduling` / `header_lever` / `patcher` / `instr_select` / `unknown`).

Live output for the `[90,99.99)` band, **real-bodied** (size ≠ 40, i.e. not funclet
stubs), 687 functions:

| CLASS | count | named | reachability |
|---|---:|---:|---|
| UNATTRIBUTED | 505 | 219 | unknown |
| REGALLOC_GPR_CALLEE | 62 | 54 | permuter_decl |
| CONTROL_FLOW | 26 | 25 | scheduling |
| REGALLOC_GPR_VOLATILE | 23 | 21 | scheduling |
| REGALLOC_FPR_VOLATILE | 16 | 16 | scheduling |
| STRUCT_OFFSET | 9 | 9 | header_lever |
| REGALLOC_MIXED_CALLEE | 8 | 8 | permuter_decl |
| BOOL_MASK | 8 | 7 | permuter_bool |
| REGALLOC_FPR_CALLEE | 5 | 5 | permuter_decl |
| COMMUTATIVE | 3 | 3 | permuter_commute |
| REGALLOC_MIXED_VOLATILE | 3 | 3 | scheduling |
| INSTR_SELECT_CMP | 2 | 0 | instr_select |

Reachability rollup (real-bodied): `unknown 505 · permuter_decl 75 · scheduling 68 ·
header_lever 9 · patcher 8 · permuter_bool 8 · permuter_commute 3 · instr_select 2`.

### What this measurement means for the RFC (the load-bearing finding)

The classes are **not** a fresh vein of source-codemod idioms:

- **REGALLOC (permuter_decl 75 + scheduling 68 = 143)** — the single largest actionable
  block, but it is the **permuter's** domain (declaration-order and instruction-scheduling
  regalloc). Covered by `11-permuter-farm.md` and `12-grind-fleet-v2.md`. A codemod that
  reorders declarations is *worse* than the permuter, which searches the reorder space
  automatically. Not idiom-library territory.
- **STRUCT_OFFSET (9, header_lever)** — single-header layout fixes that flip many fns at
  once. These are the **struct-layout / coupled-base levers** owned by
  `14-systematic-symbol-sweeps.md` and the `grind-execute*` skills, not source idioms.
- **BUILD_ENV / patcher (8)** — anonymous-namespace hashes, guard/atexit scope counters,
  prologue mismatch. Already handled by the **wired COFF obj patchers**
  (`scripts/obj_atexit_scope_patcher.py` et al., which rewrite the COFF *symbol table*,
  not code — matching the brief's guard-thunk note). No source change needed.
- **INSTR_SELECT_CMP (2)** — this *is* the strcpy `extsb`/`cmplwi` wall class, and it is
  **CLOSED**. See below.
- **UNATTRIBUTED (505 real-bodied; 280 named)** — looks like the big opportunity, but
  when you inspect it (verified from the cache): median residual is **1 mismatched
  instruction per fn** (max 54), and 253 of the 280 named ones co-present a
  `LINKER_MERGED` or `ADDRESS_RELOCATION_NOISE` pattern that the report already discounts —
  i.e. the "unattributed" residual is a 1–2 instruction tail sitting next to ICF/reloc
  noise, not a repeated clusterable shape. Only **27** of 280 have *no* detected pattern
  at all. That is the true patternless-residual pool, and it is tiny.

### The strcpy wall is closed — the idiom library's flagship candidate is dead

`docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md` documents 4 near-misses
(BandCharacter::OnChangeFaceGroup 98.72%, FirstSortChar 98.85%, CharUtlFindBone 98.67%,
+ the strcpy family) all missing by the *same single instruction*: the NUL-terminator
test inside an inlined `strcpy` intrinsic — retail emits `cmplwi rN,0x0`, our
`cl.exe 16.00.11886.00` emits `extsb. rM,rN` or `mr. rM,rN`. This is precisely the
"solve one idiom → fix many" shape the brief invokes. It was chased hard:

- ~12 source forms per agent (plain/`unsigned char`/`unsigned int`/two-pointer/
  offset-addressed/manual loops/casts) — none reached `cmplwi` cleanly.
- The `decomp_synth` permuter (10 rounds / 100 vars, chain-depth 5) converged 0/all.
- The compiler-flag `/J` (default-unsigned-char) test (2026-06-30) = **KILL, −18 net**:
  `/J` does not even reach the strcpy intrinsic's internal NUL-test codegen, and it broke
  18 signed-char-dependent fns elsewhere.

Conclusion recorded in that doc: the wall is **INTERNAL to the X360 strcpy intrinsic**,
source-, permuter-, and flag-unreachable. The one idiom class that was *most* obviously
"repeated and clusterable" is the one that proved *most* resistant. This is the strongest
evidence against a broad idiom-library bet.

### Pattern docs: catalogued, but NOT actionable as codemods

`docs/decomp/patterns/` has 18 files. `INDEX.md` lists ~72 fixable techniques across
`fixable-{casting,comparison,control-flow,declarations,operators,fsel-fma,bool-mask,...}.md`.
**But that catalog is a DC3-era snapshot** (its own STATUS banner, 2026-07-06: the counts
are from DC3's 50,981-function corpus, not rb3-xenon's). Crucially,
`docs/decomp/patterns/PERMUTER_ROI_ANALYSIS.md` shows **22 of these ~72 patterns are
already implemented as permuter transforms** (bitwise_accumulator, max_to_conditional,
sizeof_signed_cast, fsel_template, pragma_fp_contract, bool_return_expr, bit_test_bool,
single_return, …). So the pattern library is *already partly a codemod library* — it just
runs inside the permuter's AST-mutation loop, per-function, not as a repo-wide sed. There
is **no** standalone `codemod` tool in the repo (verified: `tools/*codemod*` = no matches).

## Proposal

Given the measurement, the proposal is deliberately narrow: **not** "build a general idiom
library," but "run the existing mine, isolate the *one* pool it surfaces that is genuinely
source-fixable-once-many, and pilot a single codemod against it." Three phases, each with
a kill gate.

### Phase 0 — Widen the mine to `[50,100)` and dedupe against sibling RFCs (0.5 day)

The current classifier cache only covers `[90,99.99)`. Extend the band and re-cluster:

```
tools/classify_nearmiss_codegen.py --refresh --lo 50 --hi 99.99 --jobs 8
```

Then post-process the JSONL to drop every function whose reachability label is owned by a
sibling: `permuter_decl` + `scheduling` + `permuter_bool` + `permuter_commute` → owned by
`11-permuter-farm.md`; `header_lever` → owned by `14-systematic-symbol-sweeps.md`;
`patcher` → already wired; `source_immune` → dead. What remains is the **candidate idiom
pool** for *this* RFC. Prediction from the `[90,99.99)` data: after dedup, the residual is
`UNATTRIBUTED-with-no-detected-pattern` (~27 in that band) plus `CONTROL_FLOW` scheduling
tails that the permuter's control-flow transforms *miss* — order tens, not hundreds.

### Phase 1 — Idiom fingerprinting on the residual pool (1 day)

For each residual function, extract a normalized **diff signature** so identical idioms
cluster. The tooling to produce the raw material already exists:

```
scripts/analysis/diff_inspect.py --symbol <sym> --project-dir <wt> --mismatches
scripts/analysis/diff_inspect.py --symbol <sym> --project-dir <wt> --diagnose
```

`diff_inspect.py`'s `diagnose` mode (verified, `cmd_diagnose` at line 363) already buckets
each mismatch into reg-swap / offset-shift / symbol-reloc / branch-noise / **actionable**,
and `mismatches` mode lists every mismatched instruction with target/base opcodes and
typed args. Build a signature per fn = the multiset of `(target_opcode → base_opcode,
arg-shape)` tuples over just the *actionable* mismatches. Hash → cluster. A cluster of
size ≥ 3 that shares a signature is an **idiom candidate**; a singleton is per-fn grind
(hand off to `12-grind-fleet-v2.md`).

Output: `docs/decomp/research/2026-07-08-idiom-clusters.md` (or a JSONL) ranked by
`cluster_size × named_fraction` (named = has an oracle body to rewrite against).

### Phase 2 — Solve-once-and-codemod, per top cluster (2–4 days, gated per cluster)

For each idiom cluster surviving Phase 1, in an isolated CoW worktree
(`scripts/setup_worktree.sh ~/tmp/wt-idiom-N <branch>` — **`~/tmp`, never `/tmp`**):

1. Pick the smallest/cleanest exemplar. Find the source rewrite that takes it to TRUE
   100% (oracle diff via `dc3-pair` / `rb3wii-pair` skills, then hand-permute the shape).
2. Generalize the rewrite to a mechanical transform. If (and only if) it is expressible as
   a **new permuter AST transform**, add it there — that is the project's existing codemod
   substrate and it composes with best-of-N. If it's a header/type change, it belongs to a
   sibling RFC; hand it off. A raw text sed across TUs is the *last* resort and must be
   reviewed per hunk (source idioms rarely have safe syntactic boundaries).
3. Apply to every cluster member. **Cold-cache A/B** the whole binary
   (`scripts/setup_worktree.sh --cold-cache`, then `tools/fuzzy_progress.py` before/after) —
   warm CoW can serve stale objs and fake a net-zero (HONESTY GATE from the brief).
   Composed verify: run1 == run2, and `icf_alias_check` to rule out ICF-stub-fold inflation.
4. Land only if **net-WIRED-positive with 0 regressions**. Never commit to main from the
   worktree; hand the verified patch to the landing pipeline (`16-auto-landing-pipeline.md`).

### Data flow summary

```
report.json ──► classify_nearmiss_codegen.py --lo 50 ──► inventory.jsonl (per-fn class+reach)
        │
        ├─ drop reachability ∈ {permuter_*, header_lever, patcher, source_immune}   [dedup vs siblings]
        ▼
   residual pool ──► diff_inspect.py --mismatches (per fn) ──► actionable-diff signature ──► cluster
        │
        ├─ cluster_size≥3 & named  ──►  Phase-2 solve-once ──► permuter transform OR handoff ──► cold A/B ──► land
        └─ singleton               ──►  hand off to grind-fleet-v2 (11/12)
```

## Alternatives considered

1. **Do nothing; feed the whole band to the permuter farm (11) + grind fleet (12).** This
   is the honest default and the current practice. The idiom library only beats it if
   Phase 1 surfaces a ≥3-member cluster the permuter *cannot* already mutate. If Phase 1
   returns only singletons and permuter-owned classes, this alternative wins outright.
2. **Broaden the pattern docs into a general "idiom cookbook" and grind by hand.** Rejected:
   the docs are already the most complete artifact here, 22 of the techniques are already
   permuter transforms, and the DC3-era counts don't reflect rb3-xenon — more prose doesn't
   move strict fns.
3. **Compiler-side: patch `c2.dll` / build a peephole post-pass to emit `cmplwi` for the
   strcpy NUL-test.** This is the *only* path that could unlock the closed INSTR_SELECT_CMP
   wall, but it is out of scope here (toolchain modification, not source translation) and
   the shared-facts brief lists it nowhere as a live lever. Note it and move on.
4. **Chase UNATTRIBUTED as if it were an idiom pool.** Rejected by measurement: median 1
   mismatched instruction, ~90% co-present with ICF/reloc noise, only 27/280 truly
   patternless in the `[90,99.99)` band. Not clusterable.

## Effort & expected value

Anchored to comparable past results in this repo:

- **Body-port tail sessions** (`bodyport-batch*` skills, the 2026-06-24 pivot): +2 to +3
  landed per multi-agent session on the near-miss band; the *majority* of near-misses
  refuted as walls.
- **Grind loop best-of-N + merge** (3342b30/a1312de): +22 landed across a full campaign.
- **Selective micro-pinning / surgical flips** (CharClipGroup d696b52, Waypoint d3c6e4f +7):
  single-digit wins each.

The idiom-library bet, if Phase 1 finds any real cluster, is in the same regime:

| outcome | probability (est) | strict-fn EV |
|---|---|---|
| Phase 1 finds 0 clusters ≥3 → all singletons/permuter-owned | ~40% | **+0** (kill; redirect to 11/12) |
| Phase 1 finds 1–2 small clusters, one codemods cleanly | ~45% | **+5 to +20** |
| Phase 1 finds a fat control-flow cluster the permuter misses | ~15% | **+20 to +40** |

Blended EV ≈ **+10 to +25 strict fns** for ~4–6 agent-days, most of it front-loaded into
Phase 0/1 (cheap, read-only). This is a *modest* vein, not a frontier-mover. **[UNVERIFIED]**
probabilities are estimates; the Phase-0 mine is the cheap way to replace them with fact.

## Risks & failure modes

- **Double-counting sibling work.** The biggest risk is re-discovering permuter-class
  regalloc (143 fns) and claiming it as idiom-library EV. The Phase-0 reachability dedup is
  mandatory *before* any EV is asserted.
- **Codemod overreach.** A text-level source idiom (e.g. "always write `x != 0` not `x`")
  can *regress* other functions where the naive form already matched (see
  `docs/decomp/patterns/harmful-avoid.md`: several patterns are −2% to −6.5%). Every codemod
  must be whole-binary cold-A/B'd, never applied on faith.
- **Warm-cache false net-zero.** CoW worktrees serve stale objs; a codegen edit A/B'd warm
  can read +0 when it actually moved. Cold-cache A/B is non-negotiable for this RFC's edits.
- **The cluster is real but the fix is compiler-internal.** Exactly what happened to the
  strcpy wall: a perfect ≥4-member cluster whose fix lives in `c2.dll`, not source. Phase 2
  must time-box the solve-once step (≤1 day per cluster) and KILL to a wall doc if the
  permuter converges 0/all, rather than grinding.

## Kill criteria

- **Phase 0 kill:** after dedup against sibling-owned reachability classes, the residual
  candidate pool is < ~30 functions **and** contains no signature-cluster of size ≥ 3 →
  there is no idiom vein; kill this RFC and route the pool to `11`/`12`. (The `[90,99.99)`
  data already predicts this outcome for that sub-band.)
- **Phase 2 per-cluster kill:** the exemplar cannot be taken to TRUE 100% by any source
  form in ≤1 day and the permuter converges 0/all → declare it a compiler-internal wall,
  append to a wall ledger, do not codemod.
- **Whole-RFC kill:** if the first two attempted codemods each land ≤ +2 net after cold
  A/B, the amortization thesis is refuted (per-idiom ≈ per-fn cost) — stop and fold the
  remaining pool into the grind fleet.

## Open questions

1. Does widening the classifier to `[50,80)` surface *structurally different* idioms than
   the `[90,99.99)` band (bigger control-flow divergences with more clusterable shape), or
   just lower-percentage versions of the same tiny residuals? Phase 0 answers this.
2. Is the objdiff fork's `UNATTRIBUTED` bucket hiding a real clusterable idiom that the
   detector simply doesn't have a pattern for (the 27 patternless-residual fns)? Worth a
   manual `diff_inspect --mismatches` pass on all 27 before writing off UNATTRIBUTED.
3. Can any surviving idiom be expressed as a permuter transform (composes with best-of-N)
   rather than a fragile text codemod? Strongly preferred if so.
4. **[UNVERIFIED]** How many of the band3 306 near-miss game fns overlap the residual pool
   vs the permuter/header classes? The game slice is where matching value concentrates
   (CLAUDE.md decomp-priority), so a game-weighted re-rank in Phase 1 may change ordering.

## References

- `tools/classify_nearmiss_codegen.py` — the mine (per-fn codegen class + reachability;
  runs `bin/objdiff-cli diff --batch --analyze --verdict`; read-only, safe on shared tree).
- `tools/classify_nearmiss.py` — sibling classifier (NAME_RELOC / WRONG_PAIR / OFFSET /
  REG / OPCODE / OTHER bucketing).
- `/tmp/claude/nearmiss_inventory.jsonl` — cached classifier output (1,936 records,
  `[90,99.99)`), the source of every count in "Current state."
- `scripts/analysis/diff_inspect.py` — 1,969 LOC; modes `diagnose` (l.363), `mismatches`,
  `clusters` (l.651), `regswaps` (l.707), `offsets` (l.762), `replaces` (l.831). Phase-1
  signature source.
- `tools/fuzzy_progress.py` — the north-star metric; `[50,100)` histogram = 2,167.
- `docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md` — the strcpy
  `extsb`/`cmplwi` wall characterization + the `/J` KILL (−18 net). The flagship
  idiom-cluster that proved compiler-internal.
- `docs/decomp/patterns/INDEX.md` + `PERMUTER_ROI_ANALYSIS.md` — the ~72-technique catalog
  (DC3-era snapshot, STATUS-bannered) and the 22-already-implemented permuter transforms.
- `docs/decomp/patterns/harmful-avoid.md` — codemod-regression evidence (−2% to −6.5%).
- `scripts/obj_atexit_scope_patcher.py` (+ siblings in `scripts/`) — the wired COFF
  symbol-table patchers that handle guard/atexit *naming* (not code).
- `docs/decomp/near-miss-classification-2026-06-06.md` — the struct_offset vs reveal
  primitive split (header-lever territory, owned by sibling 14).
- `scripts/setup_worktree.sh` — CoW worktree (use `~/tmp`, `--cold-cache` for honest A/B).

### Sibling RFCs (cross-references)

- `11-permuter-farm.md` — **owns the REGALLOC classes** (143 fns) this RFC dedups out.
- `12-grind-fleet-v2.md` — **owns the singletons** Phase 1 hands off.
- `14-systematic-symbol-sweeps.md` — **owns STRUCT_OFFSET/header-levers and local-static-
  Symbol/guard-thunk sweeps**; the one-pattern-many-functions fixes that are *not* source
  idioms belong there, not here.
- `15-ghidra-guided-synthesis.md` — for oracle-poor residuals with no rewrite target.
- `16-auto-landing-pipeline.md` — the verification/regression-lock lane that lands codemods.
- `18-metrics-and-dashboard.md` — vein-ROI accounting to record whether this vein pays out.
