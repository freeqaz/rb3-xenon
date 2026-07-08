# Paths to 100% — ranked index

**Framing.** rb3-xenon is blocked by exactly two walls, and every RFC here is a
bet against one of them (or a decision to route around it). **Wall 1 —
identification:** 71.7% of `.text` is anonymous `auto_*` buckets objdiff never
measures; the cross-binary locators that could name them are recall-capped
(topo_locate 3/23 ≈ 0.14, BSim 0.24), triple-confirmed — you cannot pin what you
cannot locate, and a re-ranker cannot rescue candidates that aren't in the set.
**Wall 2 — body-divergence:** even for *named* near-misses, DC3/rb3-Wii bodies
diverge from retail MSVC codegen (regalloc, FP scheduling, funclets), and the
permuter/source can't always close them. Current state: **strict 11,240/65,619
fns = 17.1%**, WIRED fuzzy 94.6%, whole-binary byte 8.69%. This doc set is 20
RFCs sizing the remaining veins against those two walls; this README ranks them
by **EV-per-effort after independent review**, not by self-report alone.

## Ranked table

Rank is my judgment. "±self" flags where I disagree with the RFC's own verdict.

| # | RFC | Theme | Verdict | Honest EV (strict fns) | Effort | Deps |
|---|-----|-------|---------|------------------------|--------|------|
| 1 | 02-gap-composition-atlas | Denominator-of-record; routes all other RFCs | PURSUE | +0 direct (accounting) | ~1d | none; feeds all |
| 2 | 14-systematic-symbol-sweeps | local-static-Symbol + guard-thunk one-pattern-many-fn scanner | PURSUE (gated) | +30 to +90 | ~1.5d + 5-fn pilot | 02 |
| 3 | 11-permuter-farm | Automated permutation over [90,100) at scale | PILOT-FIRST | +50 to +180 (3-10% conv) | ~1wk + 80-fn pilot | 02, 16, 14 |
| 4 | 16-auto-landing-pipeline | Regression lock + land-lane (insurance) | PURSUE (Phase A) | +0 direct; protects waves | ~1d (A) / 4-5d (all) | none; protects 11/12/14 |
| 5 | 12-grind-fleet-v2 | Cron LLM drafting on near-miss band | DEFER-to-background | +50 to +90 / full sweep | ~2-4d, near-free human | 11 (permuter close-rate), 16 |
| 6 | 01-endgame-definitions | Per-scope metrics + endgame-B north star | PURSUE | +0 direct (leverage) | ~1d | feeds 03, 10, 19 |
| 7 | 13-codegen-idiom-library | Mine + codemod repeated idiom classes | PILOT-FIRST (narrow) | +10 to +25 | ~4-6d, front-loaded | 02; overlaps 11 |
| 8 | 17-unicorn-equivalence-lane | DIVERGENT→FIX / EQUIVALENT-at-<100→STOP triage | PILOT-FIRST | +20 to +60 over waves | ~1.5-2.5d | 12/grind to consume |
| 9 | 03-master-sequencing-roadmap | 4-phase dependency spine | PURSUE (as spine) | +0 direct; +100-300 Phase-1 total | ~1-2d | reads 02, all |
| 10 | 15-ghidra-guided-synthesis | Asm-first synthesis for oracle-poor leaves | PILOT-FIRST (kill <2/10) | +0 or +30-150 | ~1-2d pilot | 02, 04, gated by Wall 1 |
| 11 | 04-pinning-at-scale | Gated pin loop + shadow-pin probe | PILOT-FIRST / mostly DEFER | +0 to +15 | 20-TU probe first | 02 |
| 12 | 18-metrics-and-dashboard | metrics.jsonl ledger + Vein:/Cost: trailers | PILOT-FIRST (C1+C2 only) | +0 direct (anti-respend) | ~10 LOC-hrs | schedule in a lull |
| 13 | 05-data-xref-anchoring | MI-tail data-xref IDs + data-pin pilot | PILOT-FIRST P1/P3, DEFER vein | low tens at best | ~1d P1 | consumed by 11-14 |
| 14 | 10-middleware-and-denominator | Formal Bink/RAD exclusion + Quazal routing | PURSUE (metric part) | +0 (moves 8.69→8.75%) | small | feeds 01 |
| 15 | 19-shiftable-relink-milestone | Normalized relink metric / bootable XEX | PILOT-FIRST D1/D2, DEFER D3 | +0 (metric/forcing-frame) | ~0.5d D1 | 04 (D2), 01 |
| 16 | 20-native-port-and-engine-reuse | Native host engine, band3-port synergy | PURSUE (slow parallel) | +0 direct; low-tens byproduct | weeks/owner | parallel; feeds bodyport |
| 17 | 09-sibling-title-oracles | GDRB transitive BinDiff bridge | DEFER + 1d GDRB pilot | 0-40 engine IDs (<20 likely) | ~1d pilot | Wall 1; rest DO-NOT |
| 18 | 06-oracle-refresh-loops | Ghidra re-injection cron + 1 BSim A/B | DEFER (mostly) | ~+0 (kill A/B) | ~0.5-1d | Wall 1 |
| 19 | 08-ml-embedding-triage | Supervised re-rank of oracle candidates | PILOT, likely DEFER | +0 to +5 | pilot | Wall 1 (structural) |
| 20 | 07-icf-constraint-solver | Global ICF re-ranker | DO-NOT (yet); 1d P0 probe | most likely +0 | 1d probe | Wall 1 (upstream of ranking) |

**Disagreements with self-reports:**
- **12-grind-fleet-v2** self-rates DEFER-to-background; I rank it below 16 rather
  than beside 11 because its pivotal EV unknown (permuter close-rate on ≥90%
  drafts) is exactly what the **11 pilot** measures. Sequence 11's pilot first;
  12 is near-free to run continuously *after*, not before.
- **14** self-rates PURSUE(bounded) as "the cheap tail." I rank it **#2** (above
  the permuter farm): it's the cheapest concrete match-producing lever, the
  guard-thunk multiplier is the one verified mechanism behind the only landed
  grind wave (3342b30: +22, of which +20 were paired thunks), and its 5-fn pilot
  is decisive in hours.
- **15** I keep at PILOT but rank low: its EV is entirely downstream of Wall 1
  (no ID → no pin → no verify), so it cannot lead — it's a follower of 02/04.

## Recommended next 3 moves

1. **Build the gap atlas (RFC-02).** ~1 day, +0 fns, but it is the denominator
   every other RFC's EV depends on. Do this before committing any grind budget so
   11/13/14/15 estimate against real bucket sizes, not the framing's guesses.
2. **Run the 5-fn local-static-Symbol / guard-thunk pilot (RFC-14).** Hours of
   work; validates the guard-thunk pairing multiplier that produced the only
   verified grind win. If it holds, build the ~1.5d scanner — this is the highest
   EV-per-effort actionable lever in the set.
3. **Ship Phase A of the land-lane (RFC-16) + run the 80-fn permuter pilot
   (RFC-11).** Phase A (per-fn regression lock, ~1d) is insurance that stops the
   silent wave-loss that has already zeroed deltas; the permuter pilot is the
   decisive gate (KILL if <3/80 convert) on the single largest unexplored
   match vein *and* answers RFC-12's pivotal close-rate unknown.

## Do-not / deferred (settled — do not re-litigate)

These RFCs argue **against themselves** after verification; the kill reasons are
recorded so no future session re-spends on them:
- **07-icf-constraint-solver — DO-NOT (yet).** topo_locate incand_rate ==
  precision@1 == 0.1379 proves the true VA is *absent* from the candidate set 86%
  of the time; a re-ranker is downstream of the recall wall it would need to fix.
  Only escape hatch: a 1-day P0 recall probe gated at union-recall ≥ 0.55.
- **08-ml-embedding-triage — likely DEFER.** Same structural recall wall; ML as a
  *locator* is ~0, as a triage amplifier it's recall-neutral by construction.
- **06-oracle-refresh-loops — DEFER.** Production oracles are frozen-input diffs;
  re-running is a no-op. Anchor-driven path thrice-killed on recall. One cheap
  BSim seed-prop A/B is the only thing worth running, and most-likely +0.
- **09 (except GDRB pilot), and all of devkit/RB1/RB2/TBRB/LRB/Wii-DOL — DO-NOT.**
  Stripped, no map, dominated by DC3/rb3-Wii. Only GDRB-360 as a transitive
  BinDiff bridge earns a single 1-day pilot.
- **04 blind bulk-pinning, 19-D3 bootable repack, 18 dashboard generator (C3/C4),
  17 Phase-B equiv-% metric** — each explicitly fenced by its own RFC as
  premature or metric-gaming; build only the gated/narrow slice noted in the table.

## Dependency sketch

```
02 gap-atlas ──┬─→ 14 sweeps ──┐
               ├─→ 11 permuter-farm ──→ 12 grind-fleet (needs 11's close-rate)
               ├─→ 13 idiom-library (overlaps 11's coverage)
               ├─→ 04 pinning ──→ 15 synthesis (needs pin+verify)  ──┐
               └─→ 05 data-xref (feedstock for 11/13/14)             │ all gated by
16 land-lane (Phase A) ──→ protects every wave from 11/12/13/14/15   │ WALL 1 recall
01 endgame-defs ──→ 03 roadmap (spine) ; 01 ──→ 10 denominator ──→ 19 relink metric
Wall-1 identification bets: 06, 07, 08, 09 (all DEFER/DO-NOT until a recall probe passes)
20 native-port: parallel track, feeds band3 body-ports as a byproduct
18 metrics-ledger: cross-cutting; consumed by all, blocks none
```

**Bottom line:** Phase-1 grind (02→14→11→16, plus 12/17 as background)
realistically harvests **+100 to +300 strict total, asymptotically** and
saturates strict near ~17.5-18%. The curve only bends again if a **Wall-1
identification** probe (06/07/08/09) unexpectedly passes its recall gate, or a
**denominator/equivalence redefinition** (01/10/17) changes what "100%" counts.
Spend the cheap gated pilots first; treat the identification bets as
low-probability, high-payoff options, not the plan.
