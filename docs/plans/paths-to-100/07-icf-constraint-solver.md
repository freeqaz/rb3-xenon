# ICF-aware global assignment — constraint-solving identification

> **Status:** DRAFT-RFC | **Date:** 2026-07-08 | **Author:** Claude Opus (paths-to-100 wave) | **Theme:** identification

## Summary

Model function identification as a single **global constraint-satisfaction /
assignment** problem (MAX-SAT / ILP over candidate sets) instead of the
per-function local scoring that `tools/topo_locate.py` used and that was killed
at precision 0.13. The joint constraints (TU spatial contiguity, call-graph
consistency, ICF equivalence classes, .pdata sizes) are attractive on paper. But
the killed local scorer's failure was **recall, not precision** — and this RFC's
verified live re-measurement shows the true VA is *absent from the candidate set*
86% of the time, so no amount of global joint reasoning over those candidate sets
can rescue it. **Verdict: DO-NOT (yet)** — with one narrow, honest exception.

## Motivation

The class-B identification wall is the project's largest single blocker to
strict-match progress: correctly-*located* scattered methods still body-diverge,
but the majority of scattered methods can't even be located. Three independent
attacks (topo_locate call-graph topology, Ghidra BSim seed-propagation, string-
anchor sparsity) all died at the *locate* step. The recurring critique of each
was that they scored each function **in isolation**. The intuitively-obvious next
move is to stop scoring functions one at a time and instead solve for a whole
consistent assignment at once: exploit that under `/O1` (no LTCG) each TU's
functions occupy a contiguous VA span, that a caller's bl-slots must resolve to
its source callees (modulo inlining), that ICF folds identical bodies into shared
addresses, and that .pdata gives every retail function an exact byte size. A
global solver can propagate a few high-confidence anchors through these
constraints to pin their neighbours — *if the signal survives the propagation*.

This RFC exists to answer one question honestly, before anyone spends ~2 weeks
building an ILP/SAT pipeline: **does global joint consistency recover the recall
that local scoring lacked, or does the missing-signal problem persist unchanged?**

## Current state (verified)

All numbers below were checked against the live repo on 2026-07-08 (main
`@a1312de`, but re-run in this session):

- **STRICT:** `build/45410914/report.json` `measures`: `matched_functions` =
  **11,240** / `total_functions` = **65,619** (17.13%); `matched_code` =
  **962,656** / `total_code` = **11,074,108** (8.69%). (Verified via `python3`.)
- **FUZZY** (`tools/fuzzy_progress.py`, this session): histogram over
  `match_percent_normalized`: `==100` 11,240 | `[95,100)` 1,503 | `[90,95)` 278 |
  `[80,90)` 154 | `[50,80)` 232 | `(0,50)` 213 | `==0` 51,999. RB3-specific
  (band3/network) wired fuzzy-code 91.02%; engine wired 95.72%.
- **topo_locate is REAL and KILLED.** `tools/topo_locate.py` (693 LOC, landed
  `@e318789`; kill verdict `@1e17421`, class-B 3rd-confirmation `@755ad78`). Its
  design + verdict: `docs/decomp/research/2026-06-30-topo-locator-design.md`.
- **KILL NUMBER RE-VERIFIED THIS SESSION.** `python3 tools/topo_locate.py
  --validate` → `precision@1 = 0.1379, held_out_n = 29, incand_rate = 0.1379`.
  The design doc records 3/23 = 0.13; the current pool is 29 (`incand_rate`
  identical to `precision@1` — see the decisive finding below).
- **BSim seed-prop NO-GO.** `docs/decomp/research/2026-06-21-bsim-seedprop-
  densification.md`: cross-tool precision at sim ≥ 0.90 = **0.24** (treatment),
  0.14 (baseline); no scattered game TU reached the GO bar. This is the second
  independent wall confirmation.
- **body-divergence wall.** `docs/decomp/research/2026-06-24-pivot-bodyport-
  classb-results.md`: even correctly-located class-B methods stall (SessionUsers/
  Leaderboard body-diverge; inlined-strcpy `extsb`/`cmplwi` instruction-selection
  wall; `/J` flag test = −18 net, KILL). Locating a method is *necessary but not
  sufficient*.

### The four constraint signals — availability and quality

| Constraint | Data source (verified present) | Usable? |
|---|---|---|
| TU spatial contiguity (`/O1` keeps TU `.text` grouped) | `config/45410914/splits.txt` pins known spans; ordering derivable from anchor VAs | Partial — only *pinned* TUs have known boundaries; the scattered TUs by definition don't |
| Call-graph consistency | `fingerprints.json` (61,618 fns, each with `callees[]` as hex-VA) → reverse edges; oracle callees from `../rb3` Wii asm | **This is exactly what topo_locate used** — and it's the killed signal |
| ICF equivalence classes | `build/45410914/icf_aliases.map` (15 lines, PROVEN folds only); `tools/icf_alias_finder.py --scan`; `mcp__orchestrator__lookup_merged_symbol` | Real but tiny (see below) |
| .pdata function sizes | `fingerprints.json` `size`/`n_insns` per VA; oracle `insns` from Wii asm | Weak discriminator — thousands of same-size fns |
| vtable slot order | `scripts/dump_vtable.py`, `??_7*@@6B` COFF decode, `/vtable` skill | Only for virtual methods with a decoded vtable |

- **z3 IS available** (`/home/free/code/milohax/dc3-decomp/venv/bin/z3`, the
  shared `venv`). **pysat / pulp / ortools are NOT** (`import` fails). So a SAT/
  ILP pilot would ride z3's optimizer (`Optimize`, soft constraints) or need a
  new solver dependency. z3 handles pseudo-boolean / MAX-SAT-style soft
  constraints natively; adequate for a pilot.
- **Oracle inputs:** `unified_id_rb3wii.json` (9,301 rows: `rb3_addr`, `wii_name`
  `Class::Method(args)`, `similarity`, `confidence`, `bindiff_src`);
  `scripts/target_symbol_map.json` (13,843 non-`_` entries, VA→mangled).

### Correcting the scout brief

- Scout framing "does GLOBAL joint consistency rescue recall" is the **right**
  question — this RFC answers it with a measurement, not a hope. **Refuted
  optimism:** the implicit premise that candidate sets are recall-complete and
  only *ranking* is broken. They are not (below).
- The `lookup_merged_symbol` MCP tool exists as a DB-backed lookup
  (`scripts/orchestrator/database.py` `merged_symbols` table, migration v6), not
  a live COFF ICF solver. The committed *proven* fold set is `icf_aliases.map`
  with **15 lines / 3 groups** (PoolAlloc, MemOrPoolAlloc families) — an
  allocator-overload artifact, **not** a general "many scattered methods fold
  together" phenomenon. Any RFC leaning on ICF as a rich global constraint over
  game code is overstating the data. Marked **[UNVERIFIED]** would be generous;
  it's verifiably thin.

## Proposal

### The decisive pre-experiment (already run this session — 30 minutes)

Before designing any solver, I re-ran the killed scorer's held-out validation and
read out the one number that decides everything:

```
precision@1        = 0.1379   (top-ranked candidate is the true VA)
trueVA_in_candset  = 0.1379   (true VA appears ANYWHERE in the candidate set)
held_out_n         = 29
```

**`incand_rate == precision@1`.** The true VA is in the candidate set *only when
it is already ranked #1*. In the other 86% of cases (25/29), the true retail VA
is **not in the candidate set at all** — 18/23 in the design doc's run produced
*zero* candidates. This is the load-bearing fact for the entire constraint-solver
thesis:

> A global assignment solver (MAX-SAT / ILP) can only choose among the candidates
> each variable is given. Its whole value is **re-ranking and enforcing mutual
> consistency across candidate sets**. If the correct answer is absent from the
> candidate set 86% of the time, the solver's search space does not contain the
> solution. Global joint consistency **cannot rescue recall it never had.**

The root cause is structural and was diagnosed in the topo kill: Wii→retail
callee VAs *drift* (ICF-fold / inline / devirtualize to different targets than the
true caller reaches), and only 3,089/10,664 anchors are *ever* a callee, so a
scattered method's anchored-callee set is usually empty or wrong. The candidate
sets are generated by the same drifted call-graph a solver would reason over.
**Feeding a broken candidate generator into a smarter optimizer yields a smarter
wrong answer.**

### If a pilot is nonetheless authorized — the smallest honest one

The only thing worth building is a **recall probe**, not a solver. It measures
whether *any combination of the four constraints* puts the true VA in a
tractable candidate set. It does **not** build the ILP until recall is proven.

**Pilot P0 — "does the answer exist in the joint feasible region?" (~1 day)**

Ground truth = the confirmed anchors that are game methods (matched@100, known
true VA), the same 29-method held-out pool topo uses (`unified_id_rb3wii.json`
∩ report matched@100, N≥2). For each held-out method M with true VA V*:

1. **Union candidate generator** (widen recall deliberately): candidate set =
   ⋃ of (a) call-graph callers of M's anchored callees (topo's set), (b) all
   retail fns whose `size` ∈ [0.8·insns, 1.25·insns] of M's Wii body *within any
   plausible TU span*, (c) fns in the unpinned VA gaps adjacent to M's oracle
   `bindiff_src` sibling anchors (spatial prior), (d) ICF-alias expansions of
   (a)–(c) via `icf_aliases.map`.
2. **Measure the ceiling recall:** fraction of the 29 where V* ∈ candidate set
   *at any size*. This is the hard ceiling on what a solver could ever achieve.
3. **Measure candidate-set size:** median/95th-percentile |candidates|. An ILP
   over 29 variables each with 10² candidates is trivial; each with 10⁴ is a
   real solve.

**Decision gate:** if union recall < **0.55** (topo's kill threshold), STOP — the
signal is not in the data and the solver is dead-on-arrival, exactly as this RFC
predicts. If recall ≥ 0.55 AND candidate sets are bounded (≤ ~10³), *then and only
then* proceed to P1.

**Pilot P1 — the z3 joint solve (~3–4 days, only if P0 passes)**

Variables: one integer/enum var per held-out method = its assigned retail VA
(domain = P0's candidate set). Constraints as z3 `Optimize` soft/hard clauses:

- **Hard — injectivity:** no two methods assigned the same VA (an ICF class is a
  single VA the folded members *share*, handled by pre-collapsing ICF members
  into one variable).
- **Hard — spatial contiguity:** methods of the same source TU
  (`bindiff_src`) must map into one contiguous VA window bounded by that TU's
  anchored siblings (from `splits.txt` where pinned; from anchor min/max
  otherwise). This is the constraint that *could* be a genuine force-multiplier —
  it couples the assignment across a whole TU at once.
- **Soft — call-graph consistency:** for each M and each source callee slot i,
  add a soft clause rewarding assignment[M]'s bl-slot-i resolving to
  assignment[callee_i] (weight ∝ oracle `confidence`).
- **Soft — size/insns match** (weight low), **string overlap** (weight high
  where present), **vtable slot order** for virtuals.

Solve with `z3 Optimize`. Held-out precision@1 = fraction where the joint
assignment picks V*. **Compare against topo's 0.1379 local baseline on the
identical pool** — this is the whole experiment: does coupling lift precision?

**Kill gate:** joint precision@1 must beat the local baseline by ≥ 2× AND clear
0.40 absolute to justify a production build. Below that, the coupling adds nothing
the local scorer didn't have, and the RFC's DO-NOT verdict is confirmed with a
fourth independent measurement.

### Data flow (pilot)

```
report.json (anchors) ─┐
unified_id_rb3wii.json ─┼─► P0 union candidate generator ─► recall/size report
fingerprints.json ──────┤        (reuses topo_locate.py load_all + Locator)
icf_aliases.map ────────┘                 │  gate: recall ≥ 0.55?
splits.txt (TU windows) ──────────────────┴─► P1 z3 Optimize ─► joint precision@1
                                                                 vs 0.1379 baseline
```

Reuse: `tools/topo_locate.py` already loads all inputs (`load_all()`,
`build_retail_graph()`, `parse_wii_method_callees()`). P0 is ~120 LOC on top of
it; P1 is ~200 LOC of z3 model-building. No new heavy dependencies.

## Alternatives considered

- **Full binary ILP over all 51,999 unmatched fns.** Rejected outright: 51k
  variables × large domains is intractable *and* the recall problem is unchanged
  — most fns have no candidate set. Scale doesn't fix missing signal.
- **Belief propagation / loopy-BP over the call graph** instead of ILP. Same
  candidate-set-recall ceiling; BP is just soft-constraint propagation. It would
  reproduce topo's diffusion, which the topo design panel already rejected (P1
  beat the diffusion proposals *because* diffusion added noise, not recall).
- **Constraint-solve only the *pinned-TU-adjacent* gaps** (spatial-first, ignore
  call graph). This is a cleaner idea and overlaps `04-pinning-at-scale.md`: use
  TU contiguity as the *primary* signal to fill VA gaps between known anchors.
  Worth folding into P0 constraint (c). But on its own it's an interpolation
  heuristic, not identification — it tells you a fn is *in* TU X, not *which*
  method. Belongs in `04`/`05`, not here.
- **Data-xref anchoring as candidate generator** (vtables/RTTI/rdata) — see
  `05-data-xref-anchoring.md`. A *better candidate generator* is the only thing
  that could revive this RFC (it attacks recall, not ranking). If `05` produces
  high-recall candidate sets, re-open P1 to consume them. This is the honest
  dependency: **07 is downstream of a recall breakthrough it does not itself
  provide.**

## Effort & expected value

- **P0 recall probe:** ~1 day. **This is the only work I recommend committing to
  now.** Its deliverable is either a green light or a fourth clean negative.
- **P1 z3 solve:** ~3–4 days, *gated on P0*.
- **Production identifier + carve integration** (identity_transfer `--pin-only`
  path, honesty gates): ~1–2 weeks, *gated on P1 clearing 0.40*.

**Expected value, honestly anchored to this repo's past results:**

- The topo kill measured the entire prize as ~+6 to +9 strict *at 0.61
  precision* (which was non-reproducible) and ~0 at the real 0.13. Even a solver
  that *doubled* precision to ~0.28 on the same recall-starved pool nets a
  handful of candidates, most of which then hit the **body-divergence wall**
  (BandProfile 0/64 at 100%; per `2026-06-24-pivot`, only ~40–60% of correctly-
  located game methods port to 100%). Realistic strict yield if P1 somehow
  succeeds: **+3 to +8**, comparable to the class-B IDT stream's +1
  (`2026-06-24`) and well below the class-A span harvest's +403 (now exhausted).
- **Most likely outcome: P0 kills it** (recall < 0.55) at 1-day cost, producing a
  fourth-independent-confirmation negative worth banking in the wall docs — a
  valid, cheap deliverable.
- **Opportunity cost is the real argument.** The same 1–2 weeks spent on the
  grind fleet (`12-grind-fleet-v2.md`, +22 landed `3342b30`/`a1312de`), pinning
  automation (`04`), sibling-title oracles (`09`), or data-xref anchoring (`05`)
  has a *demonstrated* positive track record. This vein's expected value is
  dominated by a wall three prior experiments already hit.

## Risks & failure modes

- **Feeding a smarter optimizer a broken candidate generator** — the central
  risk, and the reason for the DO-NOT verdict. Global consistency over drifted
  call-graph candidates produces *confident wrong* pins that pollute the anchor
  set and *degrade* future oracle passes (see `06-oracle-refresh-loops.md`). A
  wrong pin is worse than no pin: it can carve a corrected VA that mis-attributes
  a real method. Mitigation: never carve below the honesty gate; emit to a
  worklist, require independent confirmation (crossval / ≥95% fuzzy port) exactly
  as topo's `--pin-only` already does.
- **ICF over-collapse.** Pre-collapsing ICF members into one variable is only
  valid for *proven* folds (`icf_aliases.map`, 3 groups). Guessing folds to
  shrink the domain would inject false equalities. Keep ICF collapse to the
  proven map; run `tools/icf_alias_check.py` before trusting any alias.
- **z3 solve blow-up** if P0's candidate sets are large (≥10⁴). P0's size report
  gates this; if sets are huge, the problem is *also* low-signal (many
  indistinguishable candidates) — another kill, not a tuning problem.
- **Overfitting to the 29-method held-out pool.** The pool is tiny; a 2× lift on
  29 methods is ~4 methods and statistically fragile. Require the lift to
  replicate on a disjoint pool (e.g. engine anchors held out) before believing
  it.

## Kill criteria

- **PRIMARY (predicted to fire):** P0 union-candidate recall < **0.55** on the
  N≥2 held-out pool → the true VA is not in the joint feasible region; the solver
  cannot find what isn't there. STOP. Bank as wall-confirmation #4.
- **SECONDARY:** P0 passes but P1 joint precision@1 fails to beat the 0.1379
  local baseline by ≥2× or clear 0.40 absolute → coupling adds no recall; the
  local-scoring critique was wrong-diagnosed and the real limit is signal, not
  method. STOP.
- **TERTIARY:** even if P1 clears the bar, if the carved+ported strict yield after
  body-divergence attrition is < +5 (mirroring topo's secondary kill and the IDT
  stream's +1), bank the tool as a one-shot confirmer and do not maintain it as a
  harvest engine.

## Open questions

- Does **spatial contiguity alone** (P0 constraint c, ignoring the drifted call
  graph) generate higher-recall candidate sets than the call graph? If yes, this
  RFC's value migrates entirely into `04-pinning-at-scale.md` /
  `05-data-xref-anchoring.md` and 07 should be closed in favour of those.
- Could a **future data-xref candidate generator** (`05`) raise recall above 0.55
  and thereby revive P1? 07 should be explicitly re-openable on that trigger.
- Is the 29-method pool representative, or an artifact of the N≥2 gate? A pool
  that only *contains* methods with rich call graphs may over-represent the
  easiest cases — the true class-B bulk (SongSortNode 99 fns / 2 anchored
  callees; BandProfile 109 fns / 0) is *structurally invisible* and would never
  enter the pool at all. This suggests even a "successful" P1 addresses a
  vanishing slice.

## References

- `tools/topo_locate.py` (693 LOC) — the killed local scorer; reuse its loaders.
  Re-run `python3 tools/topo_locate.py --validate` reproduces precision 0.1379.
- `docs/decomp/research/2026-06-30-topo-locator-design.md` — full design, pilot,
  kill verdict, and the "only 3,089/10,664 anchors are ever a callee" recall
  diagnosis (lines 32, 85–114).
- `docs/decomp/research/2026-06-21-bsim-seedprop-densification.md` — BSim
  seed-prop NO-GO (precision 0.24), wall confirmation #2.
- `docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md` — class-B
  IDT (+1) + body-divergence wall + `/J` flag kill (−18 net).
- `build/45410914/report.json` — strict measures (11,240 / 65,619).
- `tools/fuzzy_progress.py` — fuzzy histogram + wired sub-goals.
- `fingerprints.json` (61,618 fns; `callees[]`, `size`, `n_insns`, `imms`,
  `strings`), `unified_id_rb3wii.json` (9,301 oracle rows),
  `scripts/target_symbol_map.json` (13,843 entries).
- `build/45410914/icf_aliases.map` (3 proven fold groups), `tools/
  icf_alias_finder.py`, `tools/icf_alias_check.py`, `scripts/symbol_aliases.json`.
- `scripts/orchestrator/database.py` — `merged_symbols` table (migration v6)
  backing `mcp__orchestrator__lookup_merged_symbol`.
- z3 solver: `/home/free/code/milohax/dc3-decomp/venv/bin/z3` (via shared `venv`;
  pysat/pulp/ortools NOT installed).
- Sibling RFCs: `04-pinning-at-scale.md` (TU-contiguity gap-fill — the spatial
  constraint's proper home), `05-data-xref-anchoring.md` (the recall-fixing
  candidate generator 07 depends on), `06-oracle-refresh-loops.md` (pollution
  risk from wrong pins), `08-ml-embedding-triage.md` (an orthogonal candidate
  generator), `09-sibling-title-oracles.md` (more anchors = denser call graph),
  `12-grind-fleet-v2.md` / `13-codegen-idiom-library.md` (the body-divergence
  wall 07's output would still hit), `02-gap-composition-atlas.md` (what the
  unmatched 91% actually is).
