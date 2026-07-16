# Paths to 100% — ranked index

> **STATUS UPDATE — 2026-07-16 (post-TU5-flip).** The decomp target is now
> **TU5** (`orig/45410914/default.xex` = TU5 bytes since merge d9c44305);
> the "Framing" numbers below are TU0-era and stale. Current:
> **strict 15,100/71,123 fns, 13.34% of code bytes, fuzzy 18.02%**. The TU5
> P5 loss manifest (`../tu5-p5-manifest.json`) drove 4 struct-rebase waves
> (+253, 0 regressions, 2026-07-16); of its 1,407 losses, 192 are recovered,
> **A_TOOLING (514 left) is DEAD** (ICF-fold mirage — do not re-hunt),
> B_STRUCT_OFFSET has 324 left (clean keystones drained; remainder is
> recon-class), C_DIVERGED has 357 genuine body rewrites
> (`../tu5-rewritten-functions-analysis.md` §4). New selection axis: the
> unicorn behavioral-probe DB columns are live (~186 actionable sub-99
> divergents) — RFC-17's precondition now exists. Unmatched pool split:
> named/pinned TUs 12,908 fns / 2.21 MB (incl. a 1,437-fn ≥99% near-miss
> band); `auto_*` unpinned scatter 43,196 fns / 6.79 MB (Wall 1 unchanged).
> Wall-1/Wall-2 framing below still holds; per-RFC verdicts unchanged.

> **EXECUTION UPDATE — 2026-07-08 (round 1).** The top-4 moves were executed as
> an Opus workflow. **Shipped to main:** RFC-02 `tools/gap_atlas.py` + snapshot
> (`8f06bc4`, reproduces every bucket), RFC-14 `tools/symbol_sweep_scan.py`
> (`74270ab`), RFC-16 Phase-A regression lock (`ec46311`, schema v17 +
> snapshot/check, backfilled 20 commits, 0 historical strict-100 drops), and a
> durable decomp-synth fix (`d4cfe67`) making the permuter usable in fresh
> worktrees. **Both match-producing pilots came back NEGATIVE and are now
> settled:** RFC-11 permuter farm **KILLED** (0 TRUE-100 / 66 attempts,
> full 139-pattern set exercised — band is codegen-wall-dominated); RFC-14
> sweep **empty at the measurable band** (all 415 scanned near-misses classified
> non-signature — the grind win was revealed thunks, not a sweepable population).
> Net strict-match delta this round: **+0** (as predicted for the tooling; the
> two lever bets were falsified cheaply, which was their purpose). See the
> per-RFC PILOT RESULT banners in `11-` and `14-`, and "Round-1 outcomes" below.


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
| +  | 21-crack-farm-cpu-training-capture | LLM-free massive-CPU whole-TU crack farm on decomp-synth's B2 fleet + training-data capture | DESIGN done; build = future Opus run | unproven (Wall-2 bet; <5% band conversion = kill) | E1 smoke ~1-2 CPU-hr | supersedes 11+12 (distributed-CPU + corpus flywheel) |

> **Doc 21 (2026-07-09)** is the design/problem-overview for the CPU-crack-farm the
> owner requested — it operationalizes 11 (permuter-farm) + 12 (grind-fleet) as a
> distributed, LLM-free, B2-scheduled sweep with per-pass training-data capture.
> Key finding: **not greenfield** — decomp-synth already has a working B2 CPU farm
> (`farm_worker.sh`/`runmeta.py`/`b2_sync.sh`); the only real TO-BUILDs are an
> exhaustive whole-TU search engine + a TU-value scheduler. Identical copy in
> `decomp-synth/docs/rb3-crackfarm-and-training-capture.md`.

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

## Round-1 outcomes (2026-07-08) — what the pilots settled

The original "next 3 moves" (gap atlas / RFC-14 sweep pilot / RFC-16 + RFC-11
permuter pilot) are **all executed**. Result:

- **RFC-02 gap atlas — DONE, landed** (`8f06bc4`). `tools/gap_atlas.py`
  reproduces all five buckets. The denominator-of-record now confirms the
  highest-EV *untapped* pool is **bucket-3 named-but-unpinned: 7,086 fns /
  2.63 MB, already oracle-identified** (vs the 35,039-fn / 5.31 MB anonymous
  frontier behind Wall 1).
- **RFC-11 permuter farm — KILLED** (0/66). The band is codegen-wall-dominated;
  automated permutation of the *existing* [90,100) band is not the lever.
- **RFC-14 systematic sweep — scanner landed, vein empty at this band** (0/415).
  The local-static-Symbol population isn't sitting in the measured near-miss band.
- **RFC-16 Phase-A regression lock — DONE, landed** (`ec46311`); 0 historical
  strict-100 drops in the last 20 commits (the manual gate has been clean).

**Consequence:** both body-divergence levers that targeted the *current* band are
spent. The surviving positive-EV moves are (a) getting *more* functions into the
grind-closable band, and (b) attacking the pinning/identification frontier the
atlas just quantified.

## Round-2 outcome (2026-07-08) — RFC-04 pinning probe

Executed move #2 (the pinning probe). **Result: +1 strict, vein exhausted.** The
atlas's "7,086 named-but-unpinned / 2.63 MB" pool collapses under source-present +
not-already-pinned to **18 NEW-pinnable TUs / 78 fns** — the rest are scattered in
already-pinned units (class-B). Of the 18, only 1 converted (`WebSvcMgrCurl`
`make_pair<String,String>`, +1, landed); the other 13 were span-overlaps with
existing pins (8), dilutive scatter (3), body-divergence (1), or false ICF
attribution (1). **RFC-04 Track-1 automation is not worth building.** Two durable
side-findings: (i) `pin_candidates.py` provisional spans have a false-positive
rate (min-max hulls overlapping existing pins) worth fixing if reused; (ii) a
**project-wide baseline correction** — the stale `report.json` everything cited
read **11,240**, but a forced full rebuild shows the true strict count is
**11,278 / 17.19%** (WIRED fuzzy 94.67%). Subtract ~37 from every "11,240" claim
in this doc set.

**Roadmap effect:** with the permuter (round 1) and pinning (round 2) veins both
confirmed near-dry, the *only* live positive-EV lever left is **RFC-12 grind-fleet
drafting** (closes fns directly, permuter-independent) — now clearly the #1 next
move — followed by the low-probability Wall-1 identification probes.

## Recommended next 3 moves (post round-1)

1. **RFC-12 autonomous grind fleet — the surviving body-divergence lever.** The
   +22 proof (`3342b30`) closed functions via LLM best-of-N *drafts reaching 100
   directly*, NOT via the (now-killed) permuter. So grind drafting is still live,
   and the decomp-synth worktree fix (`d4cfe67`) de-risks running it in CoW
   worktrees. Wire the cron/orchestrator drafting loop over the [75,95) band
   (489 fns), policy-gated landing behind the new RFC-16 regression lock.
2. **RFC-04 pinning probe on the 7,086 named-but-unpinned fns.** The atlas
   confirms this is the largest identified-but-uncredited pool (2.63 MB). Run the
   "how much pins cleanly?" 20-TU shadow probe (RFC-04 Track-3) — if a meaningful
   fraction ports+pins to real matches, this reorders the whole roadmap and is a
   far bigger vein than the [90,100) tail the pilots just exhausted.
3. **One cheap Wall-1 identification probe (RFC-06 BSim seed-prop A/B @ 11,240
   anchors, or RFC-05 data-xref P1 measurement).** Low probability, high payoff:
   without a Wall-1 recall breakthrough strict saturates ~17.5–18% (RFC-03). Run
   exactly one decisive 1-day probe, gated at its stated recall bar; do not build
   the machinery unless the probe passes.

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
