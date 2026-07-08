# Oracle refresh loops — iterative re-diffing as matches accumulate

Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: identification

## Summary

The identification oracles (`unified_id.json`, `ghidriff_identities.json`, the
`crossval_agree.json` BinDiff∩BSim set) were computed once, against a nearly-empty
anchor set (`unified_id.json` is from **2026-05-26, when only ~290 fns were matched**;
today 11,240 are). The intuition "every landed match creates new anchors, so refresh
the oracles" is *mostly a mirage*: the two production oracles are **cross-binary
diffs whose inputs are two fixed binaries** — landing RB3 matches does not change either
input, so re-running yields near-identical output. The one genuinely anchor-driven
signal (call-graph triangulation / BSim seed-propagation) was **killed three separate
times on recall** (topo_locate 0.13, BSim seed-prop NO-GO, crossval ~2% recall).
**Verdict: DEFER the general "refresh loop" — but adopt one narrow, cheap, real
sub-mechanism** (Ghidra symbol re-injection to keep BSim's in-session anchors current),
run as a per-wave cron, at ~5 min cost. The oracles are not anchor-limited; they are
already sitting on ~10k un-harvested IDs blocked *downstream* by porting/body-divergence.

## Motivation

The brief's premise: "every landed match creates new anchors, but the oracles were
computed once against an old anchor set; design the refresh loop." This is a natural
hypothesis and worth taking seriously — but it must survive the honesty gate this
project applies to every vein. The decisive question, stated in the brief, is:

> Is identification-recall actually **anchor-limited** (refresh helps) or
> **structurally limited** (topo_locate's finding: callees of scattered methods are
> themselves unmatched — refresh may compound)?

This RFC answers that question with live-repo measurements, then designs only the
refresh mechanism that survives the answer. Getting this right matters because a naive
"re-run BinDiff after every wave" loop would burn Ghidra import time (~4 min/run) and
agent orchestration for **~zero marginal identifications**, and worse, could *compound*
false positives by feeding mis-attributed anchors back into a triangulation step.

## Current state (verified)

### The oracle artifacts that exist (verified `ls *.json` + structure inspection)

| Artifact | Rows | Built | Signal / provenance |
|---|---|---|---|
| `unified_id.json` | 11,582 | **2026-05-26** | DC3↔RB3 **BinDiff** (source: 11,045 `bindiff`, 525 `autoid`, 12 `both`) |
| `unified_id_rb3wii.json` | 9,301 [scout=9,301, verified via oracle_quality header] | 2026-05-29 | rb3-Wii↔RB3 BinDiff (game-code oracle) |
| `unified_id_callgraph.json` | — | 2026-05-28 | call-graph propagation layer |
| `unified_id_rtti.json` / `_vtable.json` | — | 2026-06-20 / 05-28 | RTTI / vtable structural IDs (see sibling `05-data-xref-anchoring.md`) |
| `ghidriff_identities.json` | 978 (all tier `ACCEPT`) | 2026-06-11 | rb3-Wii↔RB3 **ghidriff** (Ghidra ExactInstructions hasher) |
| `ghidriff_identities_loose.json` | 960 | 2026-07-02 | looser ghidriff band |
| `crossval_agree.json` (docs/decomp/gameid/) | 3 [scout claim of "93" is **stale**, see below] | 2026-06-09 | BinDiff∩BSim high-precision game IDs |

Verify commands used: `python3 -c "import json; json.load(...)"` on each; `ls -la` for mtimes.

### The killer fact: the oracles are NOT the bottleneck — porting is

Cross-referencing every oracle row against `build/45410914/report.json` matched@100:

- **`unified_id.json`: 0 / 11,582 rows are matched@100.** Only **1,559 / 11,582** even
  appear as a pinned `fn_8XXXXXXX` symbol in the report at all (the other ~10k point at
  functions with no split range → invisible to the report). Of those 1,559 pinned, **0
  reach 100%.**
- **`ghidriff_identities.json`: 0 / 978 rows are matched@100.**

Interpretation: the identification oracles have **already produced ~11k candidate IDs
that are un-harvested**, blocked not by "we can't find the function" but by (a) it isn't
pinned in `splits.txt` yet (see sibling `04-pinning-at-scale.md`), and (b) once pinned,
the ported body diverges (`docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md`;
BandProfile 0/64 at 100% *despite correct identity*). **Refreshing the oracle to produce
more IDs does not touch either downstream wall.** This is the central verified argument
of this RFC.

### Why re-running the cross-binary oracles yields ~nothing new

`unified_id.json` is `BinDiff(DC3.xex, RB3.xex)`. `ghidriff_identities.json` is
`ghidriff(rb3-Wii, RB3.xex)`. **Both inputs are frozen binaries.** Landing more RB3
*source* matches changes neither the DC3 binary nor the RB3 retail XEX nor the rb3-Wii
binary. BinDiff/ghidriff operate on the target's *own* disassembled structure, which is
fixed. So re-running these two oracles today vs. at their build date produces
**structurally identical output** (modulo tool-version drift). The anchor set they were
"computed against" is a red herring for these two — they don't consume our match set as
seeds at all.

### The one place anchors DO feed back — and it was killed on recall

The only oracle mechanisms that consume our *growing matched set* as seeds are:

1. **Ghidra BSim seed-propagation** (`USE_ACCEPTED_MATCHES_AS_SEEDS` + call-graph
   propagation). **Verdict: NO-GO** — `docs/decomp/research/2026-06-21-bsim-seedprop-densification.md`:
   "VT seed propagation does not densify per-TU clusters and it degrades precision …
   densest game cluster propagation produced is 3–4 functions, low-precision."
2. **Callee-set topological triangulation** (`tools/topo_locate.py`, 693 LOC @ e318789).
   **Verdict: PRIMARY KILL** — `docs/decomp/research/2026-06-30-topo-locator-design.md`:
   held-out precision@1 = **3/23 = 0.13** (threshold 0.55); "18/23 produce NO candidate."
   Root cause is exactly the brief's hypothesis, confirmed: *"class-B methods' callees
   are themselves unmatched scattered methods — the signal cannot bootstrap."*
3. **Cross-validated BinDiff∩BSim** (`docs/decomp/gameid/VERDICT.json`, 2026-06-09):
   per-fn **recall ≤ 2.3%**, precision plateaus at 0.542, intersection caps at **146 fns
   binary-wide**, max contiguous correct run = 3. Root cause (quoted): *"BSim/BinDiff
   correctly identify only the distinctive minority of each TU's functions (~2–13%); the
   trivial accessors and coverage-stub-shaped functions … are either unmatched or collapse
   to degenerate generic matches."*

So the anchor-consuming path is **structurally recall-capped, not anchor-count-capped**.
More anchors do not lift a recall wall whose cause is that the *un-identified* functions
are structurally featureless (trivial accessors, ICF-folded stubs) or their callees drift
across the compiler boundary. This is the brief's "refresh may compound" fear made
concrete: feeding more anchors into triangulation adds noise faster than signal once the
distinctive minority is exhausted.

### Refuted scout claims

- **"crossval_agree.json has 93 unpinned per-fn hints."** The *VERDICT.json* narrative
  says the deliverable *was* 93 entries at build time; the **live file today has 3
  entries** (verified `json.load` → len 3). The 93-fn set was mostly consumed/pruned; do
  not plan against "93 waiting IDs."
- **"Refresh the oracle to get more IDs" (implied by brief).** Refuted for the two
  production cross-binary oracles: their inputs are frozen; re-running is a no-op. Only
  the seed-propagation path is anchor-sensitive, and it is dead on recall.

## Proposal

Given the verified state, the proposal is **deliberately minimal** — a "refresh loop"
that spends effort only where anchors genuinely move the needle, plus one cheap A/B that
would *kill or resurrect* the general loop with hard numbers.

### Part A (DO — cheap, real): Ghidra symbol re-injection cron

The genuinely anchor-driven, low-cost mechanism that already exists is
`tools/ghidra/build_symbol_map.py` + `tools/ghidra/apply_symbols.py`: it recovers each
matched function's absolute XEX VA and renames Ghidra's anonymous `fn_8XXXXXXX` to the
mangled symbol. This keeps the Ghidra project's function names current with our match
set. Its value is **not new IDs** — it is that every future manual Ghidra/BSim session,
and any future BSim seed run, starts from an up-to-date anchored program rather than one
frozen at ~290 anchors.

Wire it as a per-wave step (orchestrator-driven), gated on a match-count delta:

```bash
# refresh_ghidra_anchors.sh (new, ~40 LOC) — run after a landing wave
NEW=$(python3 -c "import json;print(json.load(open('build/45410914/report.json'))['measures']['matched_functions'])")
LAST=$(cat .ghidra_anchor_watermark 2>/dev/null || echo 0)
[ $((NEW - LAST)) -lt 50 ] && { echo "delta <50, skip"; exit 0; }
python3 tools/ghidra/build_symbol_map.py --out ~/tmp/ghidra_symbol_map.json
tools/ghidra/import-xex.sh --apply-symbols ~/tmp/ghidra_symbol_map.json   # or apply_symbols.py via MCP
echo "$NEW" > .ghidra_anchor_watermark
```

Cost: `build_symbol_map.py` is seconds (reads `obj/*.obj` + `asm/*.s` + `report.json`);
the Ghidra re-apply is a rename pass over an already-analyzed project (no re-analysis —
the ~4-min full analysis in `tools/ghidra/import-xex.sh` runs **once**, not per refresh).
Gate at Δ≥50 matches so it fires at most once per substantial wave.

Automation: register in the orchestrator as a post-land hook, or a `CronCreate`
job keyed to "after any commit that changes `config/45410914/splits.txt` or bumps
`report.json` matched count." This is the honest, bounded refresh — it maintains oracle
*freshness for humans and future experiments*, not a claim of new automatic IDs.

### Part B (DO ONCE — the decisive A/B): re-run one oracle at 11,240 anchors

The brief asks for "a cheap A/B: rerun one oracle with today's 11,240 anchors vs the
anchor count when it was built; count new proposals." Run it **exactly once**, as a
kill/resurrect experiment, on the *only* oracle where the answer is genuinely unknown:
**BSim seed-propagation**, which is the sole anchor-consuming path not yet re-measured at
this anchor scale (the last run, 2026-06-21, was at a much smaller anchor set).

Procedure (all commands adapted from `docs/decomp/gameid/VERDICT.json`'s recorded recipe):

1. Run **Part A** first so the RB3Xenon Ghidra program has all 11,240 anchors named.
2. Mark those 11,240 as ACCEPTED `VTAssociation`s (seed set) via the VT correlator the
   fork exposes (`BSimProgramCorrelator`, per the seedprop doc's Stage-0 recon).
3. Run seed-propagation; emit candidate IDs to `~/tmp/oracle_ab/seedprop_11240.json`.
4. **Ground-truth gate (identical to topo_locate's honest gate):** hold out a random
   500 of the 11,240 anchors, hide their names, and measure held-out precision@1 and
   "candidate produced at all." Compare the *new-ID yield* (candidates on un-pinned VAs)
   and precision against the 2026-06-21 run's numbers.

**Decision:** if held-out precision@1 ≥ 0.55 **and** ≥ 8 new un-pinned real-bodied
candidates survive the self-consistency guard → the anchor-scale hypothesis is
*resurrected*; graduate to a recurring seed-propagation refresh. Otherwise → **fourth
independent confirmation of the class-B recall wall**; bank the negative and close the
"refresh generates new IDs" vein permanently.

### Part C (DO NOT): the naive full-refresh loop

Do **not** build "re-run BinDiff(DC3,RB3) + ghidriff(Wii,RB3) after every wave." Verified
no-op (frozen inputs). It would cost Ghidra BinExport + BinDiff runtime per wave for
structurally identical output. If DC3-decomp or rb3-Wii *itself* lands new named
functions (their trees are live), a *one-shot* re-diff is warranted — but that is
triggered by **the oracle binary's source changing**, not by our match count. Track that
with an mtime check on `../dc3-decomp/orig/373307D9/ham_xbox_r.map` and
`../rb3/build/SZBE69_B8/`, not a per-wave loop.

### Data flow (what actually refreshes vs. what is frozen)

```
our match set grows  ──► report.json (matched_functions↑)
        │
        ├─ Part A: build_symbol_map.py ─► Ghidra RB3Xenon fn_ names refreshed   [REAL, cheap]
        │                                        │
        │                                        └─► future BSim seed set current
        │
        ├─ Part B: BSim seed-prop @11240 (ONE A/B) ─► new-ID yield? [UNKNOWN → measure]
        │
        └─ unified_id.json / ghidriff  ◄── frozen (inputs are fixed binaries)   [NO-OP]
```

## Alternatives considered

1. **Full per-wave BinDiff/ghidriff refresh** — rejected (Part C): frozen inputs → no-op.
2. **Graft P2/P3/P4 signals onto topo_locate to rescue triangulation** — explicitly
   forbidden by the topo_locate doc's OVERALL STOP CONDITION: "the recall ceiling caps
   the prize regardless of precision." Do not reopen.
3. **Lower the ghidriff/BinDiff confidence threshold to surface more candidates** — the
   loose variants already exist (`ghidriff_identities_loose.json`,
   `unified_id_rtti_low.json`) and add ~0 net (loose ghidriff has *fewer* rows, 960 vs
   978; the crossval curve shows precision collapses below conf 0.7). More candidates at
   lower precision means more mis-attribution, which `tools/oracle_quality.py` exists
   precisely to filter *out*. Wrong direction.
4. **Data-xref-anchored refresh** (vtable/RTTI IDs, which *do* grow with matched vtables)
   — genuinely anchor-sensitive and orthogonal to call-graph recall. **Deferred to sibling
   `05-data-xref-anchoring.md`**, which owns that signal. This RFC's Part A keeps the
   Ghidra project fresh enough for that work to build on.
5. **ML-embedding triage as the recall lifter** — the structural recall wall (featureless
   accessors, ICF stubs) is exactly what an embedding model might see through where BSim's
   hand-weighted features cannot. **Deferred to sibling `08-ml-embedding-triage.md`**; if
   any refresh loop ever pays off, it will be that one, not call-graph re-triangulation.

## Effort & expected value

Anchored to comparable past results in this repo (all verified):

- **Part A (Ghidra re-injection cron):** ~0.5 day to write + wire. EV = **0 direct
  strict matches**; value is *enabling* — keeps every future Ghidra/BSim session and the
  Part-B A/B honest. Low cost, defensible as infra hygiene.
- **Part B (BSim @11240 A/B):** ~1 day (mostly Ghidra runtime + the held-out harness,
  reusable from topo_locate's `--validate`). EV of the *experiment* is a hard yes/no.
  EV of a *positive* outcome, if it beats the 3 prior kills: bounded at the topo_locate
  ceiling — **+6 to +9 strict** best case (that doc's honest arithmetic), and *most
  likely +0* given three prior independent recall kills. Treat Part B as a **kill
  experiment**, not a harvest.
- **Part C (full refresh loop):** negative EV (Ghidra time for no-op output). Do not build.

Honest bottom line: the refresh-loop *theme* has an expected value near **+0 direct
matches**, because the measured wall is downstream (porting/body-divergence, owned by
`13-codegen-idiom-library.md` / `11-permuter-farm.md`) and the anchor-driven ID path is
thrice-killed on recall. The ~11k already-produced-but-un-harvested IDs
(`unified_id.json` 0/11,582 landed) mean the identification pipeline is **supply-saturated**;
adding supply is worthless until the pinning wall (`04-pinning-at-scale.md`) and
body-divergence wall are addressed. This RFC's real deliverable is the *verified negative*
plus the cheap Part-A hygiene mechanism.

## Risks & failure modes

- **Compounding false positives (brief's explicit fear).** If Part B's seed set includes
  mis-attributed anchors, propagation amplifies them. Mitigation: the held-out
  ground-truth gate + `tools/oracle_quality.py` size-band/foreign-name filter must run
  *before* any propagated ID is treated as real. Never `gen_game_target_map.py --apply`
  on propagated IDs without the byte-equality gate (CLAUDE.md: scattered-TU `--apply` is
  POISON).
- **Ghidra project lock.** RB3Xenon is single-process (Ghidra projects lock). A refresh
  cron colliding with an in-progress import/MCP session corrupts state. Mitigation:
  Part A must acquire the project lock (skip-if-locked), mirroring the `gameid-crossval`
  skill's "Ghidra access serialized into one agent" discipline.
- **Watermark drift under concurrent agents.** `.ghidra_anchor_watermark` in the shared
  main tree could race. Mitigation: keep it under `~/tmp` or gate on `report.json`'s
  matched count read live, not a cached file.
- **Tool-version drift masquerading as "new IDs."** Re-running BinDiff with a newer
  `../bindiff` build could change output for reasons unrelated to anchors, inflating a
  false "refresh worked" signal. Mitigation: any re-diff A/B must pin the exact tool
  commit and attribute deltas to version vs. anchor explicitly.

## Kill criteria

- **Kills Part B (and closes the refresh-for-new-IDs vein permanently):** BSim
  seed-propagation @11,240 anchors yields held-out precision@1 < 0.55 **or** < 8 new
  un-pinned real-bodied confirmed candidates. Given three prior kills (topo_locate 0.13,
  seed-prop NO-GO, crossval 2% recall), this is the *expected* outcome — record it as the
  fourth confirmation and stop.
- **Kills the whole RFC theme (retroactively):** if a future audit shows any oracle row
  set landing at scale purely from a refresh (i.e. the downstream porting/pinning walls
  dissolve), then supply *was* the constraint and this "DEFER" was wrong. Watch
  `unified_id.json` landed-fraction: it is **0/11,582 today**; if it climbs materially
  from pinning+porting work *without* a refresh, that confirms refresh is irrelevant.
- **Kills Part A:** if Ghidra symbol re-injection never gets consumed (no BSim/manual
  Ghidra work happens for a full session cycle), drop the cron — it's pure overhead with
  no downstream reader.

## Open questions

1. Does `../dc3-decomp` or `../rb3` land enough *new named functions* over a session to
   justify a one-shot cross-binary re-diff? (Trigger = oracle-binary source change, not
   our match count — needs an mtime watch, unmeasured here.)
2. Can the vtable/RTTI oracles (`unified_id_vtable.json`, `unified_id_rtti.json`) be
   refreshed *cheaply* as we match more vtables, and do they beat the call-graph recall
   wall? (Owned by `05-data-xref-anchoring.md` — cross-reference, do not duplicate.)
3. Is there a "distinctive-minority" subset per un-pinned TU that a refresh *could* still
   surface, sufficient to bracket a span for `04-pinning-at-scale.md`? crossval says max
   contiguous correct run = 3 (too short to bracket) — but sibling `07-icf-constraint-solver.md`
   may lift this via global assignment rather than per-fn ID. Coordinate.
4. Does the Part-A re-injection measurably improve a *manual* Ghidra decomp session's
   productivity (sibling `15-ghidra-guided-synthesis.md`)? If yes, Part A's EV is real even
   with Part B dead.

## References

Live repo (all verified 2026-07-08, main @a1312de):

- `unified_id.json` (11,582 rows, 2026-05-26), `unified_id_rb3wii.json`,
  `unified_id_callgraph.json`, `unified_id_rtti.json`, `unified_id_vtable.json`,
  `ghidriff_identities.json` (978), `ghidriff_identities_loose.json` (960) — oracle artifacts.
- `docs/decomp/gameid/VERDICT.json` — crossval BinDiff∩BSim NEGATIVE (recall ≤2.3%,
  intersection 146 fns binary-wide, 2026-06-09).
- `docs/decomp/gameid/crossval_agree.json` — **3 entries today** (scout's "93" is stale).
- `docs/decomp/research/2026-06-30-topo-locator-design.md` — topo_locate PRIMARY KILL
  (held-out precision@1 3/23=0.13; chicken-and-egg recall wall; class-B floor confirmed).
- `docs/decomp/research/2026-06-21-bsim-seedprop-densification.md` — BSim seed-prop NO-GO.
- `docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md` — body-divergence wall
  (BandProfile 0/64 at 100% despite correct identity) = the real downstream bottleneck.
- `tools/ghidra/build_symbol_map.py`, `tools/ghidra/apply_symbols.py` — the anchor→Ghidra
  re-injection mechanism (Part A).
- `tools/ghidra/import-xex.sh` — one-time full analysis (~4 min; re-analysis NOT per refresh).
- `tools/oracle_quality.py` — pre-port oracle-VA mis-attribution filter (POISON guard).
- `tools/topo_locate.py` (693 LOC @ e318789) — banked margin≥2 confirmer, not a harvester.
- `tools/band3_worklist_pin.py`, `tools/gen_band3_port_worklist.py` — the ghidriff-worklist
  consumers (commit 153197c: Lyric.cpp +1 strict via the ghidriff identity path).
- `build/45410914/report.json` — 11,240/65,619 matched (17.13%); the landed-fraction ground truth.
- `tools/fuzzy_progress.py` — WIRED fuzzy staircase (verified: ==100 11,240; [95,100) 1,503).
- Siblings: `04-pinning-at-scale.md` (the downstream pin wall), `05-data-xref-anchoring.md`
  (anchor-sensitive vtable/RTTI signal — the one refresh that may pay), `07-icf-constraint-solver.md`
  (global assignment vs per-fn ID), `08-ml-embedding-triage.md` (the recall-lifter if any),
  `13-codegen-idiom-library.md` / `11-permuter-farm.md` (the true bottleneck: body-divergence),
  `15-ghidra-guided-synthesis.md` (a downstream consumer of Part-A freshness).
