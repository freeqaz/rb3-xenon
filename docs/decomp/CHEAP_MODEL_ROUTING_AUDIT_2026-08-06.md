# Cheap-model routing audit — which targets are actually solid

**2026-08-06.** Source: `decomp.db.attempts` (5,380 attempts with a `model`
field) joined to `functions`, plus a fresh candidate build from
`build/45410914/report.json`. Queue emitted as data:
`docs/decomp/cheap-model-queue-2026-08-06.tsv`.

Population: **opus 3,794 · sonnet 1,404 · fable 71 · unknown 110 · hy3 1.**
Cheap tier = sonnet + fable (1,475 attempts) throughout.

---

## ★★★ The headline number is a selection artifact — do not route on it

| model | attempts | reached 100 | rate |
|---|---:|---:|---:|
| sonnet | 1,404 | 535 | **38.1%** |
| fable | 71 | 49 | **69.0%** |
| opus | 3,794 | 500 | **13.2%** |

Read naively this says cheap models are 3× better than opus. **They are not.**
Opus carries **3,153 `at_limit` verdicts (83% of its attempts)** at an average
start of 73.4% — it has been assigned the hard residual by policy. The tiers were
never pointed at the same population.

**The head-to-head inverts it.** On the 408 functions both tiers touched, opus
reached 100 on **214 that cheap models could not**; cheap reached 100 on only
**15** opus missed.

⚠ **But the head-to-head is not clean either — it is an escalation funnel.**
**363 of 408 (89%) were cheap-first.** "Both attempted" therefore means, almost
always, *cheap tried and failed, so it escalated*. `opus_only = 214` is a valid
number for **escalation policy** ("opus rescues ~53% of cheap failures") and is
**not** a capability ratio on a randomly-chosen target.

⇒ **Neither aggregate estimates capability.** Route on the within-tier profile
below, which has no cross-model confound.

## ✅ Self-reporting is HONEST — the main safety objection does not survive

`report_result`'s `percent` is agent-supplied, so a cheap model over-claiming was
the obvious risk. Corroborated against ground-truth `functions.current_percent`
(ingested from `report.json`) for every attempt claiming ≥100:

| model | claimed 100 | corroborated | rate | truth <95 |
|---|---:|---:|---:|---:|
| sonnet | 535 | 533 | **99.6%** | 0 |
| fable | 49 | 47 | 95.9% | 1 |
| opus | 499 | 487 | **97.6%** | 6 |

**Sonnet is the most accurate self-reporter of the three** — opus is the worst.
Over 1,475 cheap claims there is no inflation signal. (Non-corroboration is also
not necessarily a false claim: a lane's work may never have landed.)

**Failure severity favours cheap too.** Cheap models regress *more often* but far
*less deeply*: sonnet 141 regressions averaging **−1.1 pp** (worst −14.8),
against opus's 81 averaging **−4.3 pp with a worst case of −100 pp**. Cheap
failures are shallow and cheap to revert.

## ★★★ SIZE is the routing signal, and it is an INVERTED U — not "small is easy"

| size | cheap hit-rate | opus | verdict |
|---|---:|---:|---|
| **<100 B** | **20.9%** (n=359) | 4.4% (n=2,789) | ⛔ **graveyard** |
| **100–250 B** | **55.5%** (n=355) | 38.1% | ✅ **best** |
| **250–500 B** | **50.8%** (n=376) | 36.4% | ✅ **best** |
| 500 B–1 k | 38.9% (n=229) | **46.3%** | ⚠ crossover — opus ahead |
| 1 k–2.5 k | 19.8% (n=116) | 31.4% | ⛔ opus |
| >2.5 k | 23.1% (n=39) | 21.1% | ⛔ neither |

**Sweet spot is 100–500 B**, where the cheap tier *beats opus's own rate on the
same band*. **Crossover is ~500 B**; above it, route to opus.

⛔⛔ **The sub-100 B collapse is the finding that inverts the intuitive
heuristic.** "Tiny function ⇒ easy ⇒ give it to the cheap model" is exactly
backwards: **90.0% of all sub-100 B attempts end `at_limit`**, versus 26–41% in
every larger band. What survives unmatched at that size is *structural*, not
source-level — EH funclets (the only labelled pattern left there: 166 rows), ICF
fold aliases, vbase thunks, `??_G`/`??__E` boilerplate. Opus scores **4.4%** on
that band across 2,789 attempts. **Nobody should be routed there, at any price.**

⚠ Match-% is a *weak* predictor by comparison — cheap models are flat at 30–41%
across every start-percent band. Do not rank by fuzzy alone.

## ⛔ PORTING (0% rows) is NOT the cheap-model vein it appears to be

3,147 named rows sit at 0% in the sweet-spot size range (690 KB), which looks
like a large ready porting queue. **~2,760 of them are in `default/auto_03_*_text`
units** — auto-split units with no real source file. A name from the symbol map
with no `.cpp` behind it is **identification and pinning work**, not porting, and
that is the one job in this campaign that has consistently needed the strongest
model. Only a few hundred 0% rows sit in genuinely named units.

⇒ **Cheap models belong on PARTIAL rows (source exists, body is close), not on
0% rows.** Corroborated by the successful-attempt notes, which cluster on starts
of 75–99% with real root-causing (EH pointer spills, normalized-vs-raw
discipline, `run_objdiff` full listings).

## The queue — 558 rows / 140,876 B

`docs/decomp/cheap-model-queue-2026-08-06.tsv` (`fuzzy · mpn · size · unit ·
symbol`), built from `report.json` at Aug 4, filtered to: **100–500 B, named,
0 < fuzzy < 100**, excluding `auto_*` units and anonymous `fn_`/`sub_` rows.

- **558 rows / 140,876 B** total.
- **182 rows / 48,572 B at fuzzy ≥ 97** — the cheapest-first head.

⚠ **Two contamination filters were necessary and are already applied:**

1. **STL template COMDATs are excluded** (`_M_fill_insert`, `push_back`,
   `_M_insert_overflow_aux`, `vector`/`set`/`map` instantiations). A naive
   fuzzy-descending sort is *dominated* by them, but ED-2 adjudicated this class
   closed: retail's immediates **contradict themselves** across rows of the same
   type, so they are ICF fold aliases with no `sizeof` to name. EE-2's queue
   excluded them for the same reason (870 of 1,011 rows).
2. **`??_G` / `??__E` / `??__F` / `NewObject` boilerplate excluded** — same
   structural-residual class as the sub-100 B graveyard.

## ⚠ Grading: know which ruler you are on

`functions.current_percent` is **`fuzzy_match_percent`** (verified at
`scripts/orchestrator/database.py:639`), the *byte* ruler — not `mpn`, which is
what `matched_functions` counts.

**109 of the 182 fuzzy≥97 head rows are already `mpn == 100`.** Fixing those adds
their bytes to `matched_code` and **+0 matched functions**. That is a real gain on
the accuracy-favouring ruler the project prefers, but a lane graded on Δmatched
alone will read it as zero. Grade cheap lanes on **Δbytes**, or pick the 73 rows
where `mpn < 100` if function count is what's wanted.

## Recommended routing policy

| target class | model |
|---|---|
| partial rows, **100–500 B**, named unit, source exists | **cheap** ✅ |
| partial rows, 500 B – 1 k | opus (crossover) |
| anything **<100 B** | ⛔ **nobody** — 90% `at_limit` |
| >1 k B bodies | opus |
| 0% rows in `auto_*` units (identification/pinning) | opus |
| STL COMDATs / `??_G` / funclets | ⛔ closed classes, do not fund |

**Escalation is proven and should be kept:** opus rescued 214 of ~400 cheap
failures (~53%). Cheap-first-then-escalate is a sound pipeline, not a fallback —
it is how 89% of the shared population was actually worked.

## Caveats

- `actual_cost_usd` and `iterations` are **NULL for every row** — there is no
  cost or tool-call data in the DB, so no $/match figure can be computed. The
  economic case rests on hit-rate at size, not measured spend.
- `model` is **self-reported** by the reporting agent; a coordinator dispatching
  a sonnet subagent may have recorded `opus`. The 110 `unknown` rows show the
  field is not always set.
- `fable`'s n=71 over a single 8-day window (2026-07-02→07-10) is too small and
  too old to route on; its 69% is not comparable to sonnet's 1,404 attempts
  spanning 07-10→08-03.
- The DB ingest is **2026-08-03** and `report.json` is **2026-08-04**; HEAD has
  moved since (ws4-relocname-align, ICF-alias work). Counts are directional —
  **re-measure before grading a lane**, per the standing rule on `total_code`.
