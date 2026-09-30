# W16-HD — anonymous fn_ identity scorer: controls, threshold, proposals (2026-09-30)

Tool: `tools/anon_candidate_scorer.py` (v2 subcommands `census`, `control2`,
`evaluate`, `propose`, `apply`). Tree: branch `w16-hd` off main `8809d58b`,
built worktree (`total_functions` 69,240 = main's). Ruler: report.json's
`name_check` + the three other pinned keys + `icf_aliases.map`. **Nothing in
`scripts/target_symbol_map.json` was edited. No whole-binary A/B was run**, because
that would require a map edit. So this doc makes **no Δmatched claim**. It
measures identification quality only.

Proposals: `docs/decomp/W16HD_ANON_PROPOSALS_T75_2026-09-30.json`.

## 1. Population (measured, `census`)

Scope: 279 units with a compiled base obj, source under `src/band3/` or
`src/network/` minus `src/network/quazal/`. `TrackWidget*`/`MidiParser` are
excluded because W16-HB owns them.

| quantity | value |
|---|---|
| target rows (`fn_`/`lbl_`, fuzzy 0) | **1,779 rows / 331,292 B** |
| defined function symbols in the 279 base objs | 162,201 EXTERNAL + **675 STATIC** (+19,639 `__unwind$` funclets dropped) |
| distinct names | 31,549 external / 578 static |
| candidate pool after excluding every map name and every paired row name (29,688) | 131,062 EXTERNAL + **604 STATIC** instances = **22,819 distinct names** |
| in-size-band (0.5–2×) candidates per target | median 118, max 2,046; 47 targets have 0 |
| targets actually scored | 1,723 (47 no in-band candidate, 9 deliberately `null` in map) |

## 2. Defects fixed in v1 before measuring

- **Statics were excluded.** v1 took storage class 2 only; v2 takes 2 and 3.
- **Sizes were wrong.** v1 used COMDAT section size, but 29,700 function
  symbols share their section with their own `__unwind$` funclets. v2 computes
  the extent to the next function symbol.
- **v1's control could collide.** It renamed a wrong candidate *to the true
  name* while the true name still existed in the base obj, giving two symbols
  with one name. v2 first renames the true name to junk.
- **Fidelity check for the fix:** the true candidate's score equals
  report.json `fuzzy_match_percent` on **1,962 / 1,962** control rows. 0 of
  26,630 objdiff jobs failed.

## 3. Control design (`control2`)

- **Sample:** 2,001 named rows with fuzzy > 0, stratified to the targets' size
  distribution (seed 20260930).
- **Prefilter:** size band, then top-6 by instruction-shape similarity. The
  true name is in the top 6 on 1,962 / 2,001 rows (98.1%).
- **Parallelism:** 16 worker processes, about 16 jobs/s under load average
  ~64. Control: 26,630 objdiff jobs, ~50 min. Real targets: 9,931 jobs.
- **Leg IN** (true name in pool): precision is P(top-1 is the true name | the
  rule fires).
- **Leg OUT** (true name removed from pool): FP rate is P(the rule fires).
- **Bracket NAMED:** the target obj as built. Callees are named, so wrong
  callees are charged. This is the strict case.
- **Bracket ANON:** every map name in a target copy is reverted to
  `fn_<addr>`. name_check forgives placeholder callees, so this is the lenient
  case and the worst case for false positives. Real fn_ targets sit between
  the two brackets.

**Pre-registered predictions and outcomes:**

| # | prediction | outcome |
|---|---|---|
| 1 | margin ≥ 1 needed for ≥ 98% precision | **true** |
| 2 | FP at T=100 is ties | **false** — ties are already killed by the margin. The residue is byte-equivalent *non-tied* twins: template instances, `$4PPPPPPPM` thunks, same-offset accessors, and source siblings such as `Enable`/`Disable@NetSync` |
| 3 | anon FP is 1.5–2× named | **true (2.4×)**, score-only |
| 4 | < 20% of targets fire | **true** |

## 4. Score alone is not enough (score-only rule, named / anon)

| T | M | prec IN | recall IN | FP OUT |
|---|---|---|---|---|
| 100 | 0 | 97.15 / 94.78 | 90.4 / 89.0 | 3.30 / 6.40 % |
| 100 | 1 | 100.00 / 100.00 | 82.3 / 81.3 | 0.90 / 2.15 % |
| 90 | 1 | 99.88 / 99.88 | 86.2 / 85.1 | 6.05 / 6.05 % |

In-pool precision is excellent. **The base rate is the problem:** the mixture
fit puts only **~1–15%** of real targets as having their identity in our pool
(p̂ rises as T falls, because real in-pool bodies are near-misses, not 100s).
At T=100/M=1 the estimated real precision is only:

- **~70% (named) / ~27% (anon)** with score alone (50 real fires);
- **78% / 67%** with the sandwich gate but without the occupancy gate
  (23 real fires).

## 5. Added evidence: layout (`--spatial`)

The gate is a **sandwich + occupancy** test:

- **Sandwich:** the candidate's (section, value) position in our obj lies
  strictly between the positions of the target's nearest *named* retail
  neighbours.
- **Occupancy:** our obj has no more unclaimed-pool functions in that gap than
  retail has rows in it.

It uses only retail neighbours and our obj, never the hidden name. The hidden
row is dropped from the neighbour list but still counted as an occupied retail
slot, exactly like a real fn_ target. So neither control leg can leak through
it.

**Final table (score + spatial, M = 1). Each cell is named / anon.**

| T | fires IN | prec IN | recall IN | fires OUT | FP OUT | real proposals |
|---|---|---|---|---|---|---|
| 100 | 513 / 508 | 100.00 / 100.00 | 25.6 / 25.4 | 1 / 1 | 0.05 / 0.05 % | 6 (824 B) |
| 95 | 523 / 518 | 100.00 / 100.00 | 26.1 / 25.9 | 3 / 3 | 0.15 / 0.15 % | 23 (6,320 B) |
| 90 | 528 / 523 | 99.81 / 99.81 | 26.3 / 26.1 | 5 / 5 | 0.25 / 0.25 % | 37 (10,292 B) |
| 80 | 534 / 529 | 99.81 / 99.81 | 26.6 / 26.4 | 5 / 5 | 0.25 / 0.25 % | 63 (20,296 B) |
| **75** | **534 / 529** | **99.81 / 99.81** | 26.6 / 26.4 | **5 / 5** | **0.25 / 0.25 %** | **72 (23,176 B)** |
| 70 | 534 / 529 | 99.81 / 99.81 | 26.6 / 26.4 | 8 / 8 | 0.40 / 0.40 % | 82 (27,680 B) |

## 6. Chosen rule and proposals

**Rule: fuzzy ≥ 75, margin over runner-up ≥ 1, spatial gate on, globally
bijective.** T=75 is the knee of the control curve: FP OUT is flat at 5 / 2,001
from T=90 down to T=75 and jumps at T=70. The threshold was chosen from the
control, not from the real run.

**Control legs at this rule (both brackets):**

- **In-pool precision: 533/534 = 99.81%** (one-sided 95% lower bound ≈ 98.96%).
  The single miss is `Campaign::GetCurrentPoints…`. Our body scores 85.5
  against its own retail row, but its sibling `GetTotalPoints…` scores 92.7.
- **True-removed FP: 5/2,001 = 0.25%** (Poisson 95% upper bound 0.53%). All 5
  events are sibling or twin functions in the same gap:
  - an anonymous-namespace twin, `CheckContextSong` vs `CheckContextLastSong`
  - a template twin, `__uninitialized_move`
  - `Enable`/`Disable@NetSync`
  - two `Campaign` siblings

**Proposals:** `apply ~/tmp/w16hd/propose_raw.json --threshold 75 --margin 1
--spatial` gives **72 proposals / 23,176 B** in 40 units.

- 0 were dropped as non-bijective.
- 0 names are already in the map, and 0 addresses are mapped, `null`, or
  denylisted.
- Proposal fuzzy bands: 6 at 100, 17 at ≥95, 14 at ≥90, 26 at ≥80, 9 at 75–80.

**⚠ What the 98% does and does not cover.** 99.81% is the precision *when the
true name is in the pool*.

- **Prior-free bound on the real run:** wrong ≤ FP × 1,723 = 4.3 (9.1 at the
  FP upper bound). So precision on the 72 is **≥ 94.0%** (≥ 87.4% at the
  bound).
- **Mixture estimate:** p̂ = 0.149, giving ≈ 94.7%.

So expect **roughly 1–9 wrong names among the 72, most likely source-sibling
swaps**. Before this set goes into the map it needs retail-byte adjudication
and a whole-binary A/B. Under name_check a wrong name is a *bet* (see
CLAUDE.md). The runner-up of every proposal is recorded in the JSON for that
review.

## 7. Reproduce

```
python3 tools/anon_candidate_scorer.py census
python3 tools/anon_candidate_scorer.py control2 --rows 2000 --k 6 --workers 16 --json-out ~/tmp/w16hd/ctrl_2000.json
python3 tools/anon_candidate_scorer.py evaluate ~/tmp/w16hd/ctrl_2000.json --spatial --thresholds 100,95,90,80,75,70 --margins 1
python3 tools/anon_candidate_scorer.py propose --threshold 100 --margin 100 --k 6 --workers 12 --json-out ~/tmp/w16hd/propose_raw.json
python3 tools/anon_candidate_scorer.py apply ~/tmp/w16hd/propose_raw.json --threshold 75 --margin 1 --spatial --json-out <out>
```

The raw score files (`ctrl_2000.json` 26,630 scores, `propose_raw.json` 9,931
scores) are in `~/tmp/w16hd/`. They are not committed.
