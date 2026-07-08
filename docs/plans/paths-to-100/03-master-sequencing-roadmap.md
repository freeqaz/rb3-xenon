# Master sequencing — dependency-ordered roadmap to maximum match

> Status: DRAFT-RFC | Date: 2026-07-08 | Author: Claude Opus (paths-to-100 wave) | Theme: strategy

## Summary

This RFC sequences every candidate vein (the 20 sibling RFCs) into a
dependency-ordered roadmap, states each phase's honest expected yield anchored to
*measured* past results, and gives a diminishing-returns analysis: where STRICT%
realistically saturates without a breakthrough, and which 2-3 breakthrough bets
change the curve. The core thesis: the cheap levers (class-A span harvest,
body-port tails, structural base-class flips) are **empirically exhausted**, so
incremental work now yields tens of matches, not hundreds; the curve only bends
again if **identification** (05/07/08/09) or a **denominator redefinition** (01/10)
lands. Everything else is grind that harvests a shrinking tail.

## Motivation

The project has run dozens of waves and killed dozens of veins with recorded
reasons. Without a single sequencing document, agents re-litigate dead veins and
mis-order dependent work (e.g. attempting a case-B objdiff fork before body
divergence is bounded, or pinning before identification). This RFC is the
dependency graph plus the ROI curve, so a cold agent can pick the next phase
without re-deriving the whole history. It is deliberately a **strategy** doc: it
owns the *ordering* and *saturation* questions; the *how* of each vein lives in
its sibling RFC.

## Current state (verified)

All numbers verified against the live repo at main @`a1312de`, 2026-07-08.

- **STRICT: 11,240 / 65,619 functions matched (17.13%); 962,656 / 11,074,108 code
  bytes (8.69%).** Source: `python3 tools/fuzzy_progress.py` (reads
  `build/45410914/report.json`). The 8.69% byte figure is the honest,
  whole-binary, dc3-comparable metric — no denominator gaming.
- **FUZZY (WIRED set): 94.602%** over 1,392,316 attempted bytes, n=13,584 fns.
  Whole-binary fuzzy is 11.895%. Staircase: ≥100: 11,240 | ≥95: 12,743 | ≥90:
  13,021 | ≥80: 13,175 | ≥50: 13,407. Source: same tool.
- **Histogram (verified):** ==100: 11,240 | [95,100): 1,503 | [90,95): 278 |
  [80,90): 154 | [50,80): 232 | (0,50): 213 | ==0: **51,999**. The mass is the
  ==0 band — 51,999 functions with no fuzzy credit at all, dominated by
  **unidentified / unwired** functions (see 02-gap-composition-atlas.md).
- **Sub-goals (verified):** RB3-specific (band3/network) 5,962 fns, 3,364 wired,
  2,798 at ≥100, whole-fuzzy 40.12%. Engine (src/system) 17,531 fns, 10,220
  wired, 8,442 at ≥100, whole-fuzzy 42.67%. Other (thirdparty/vendor/xdk) 42,126
  fns, **0 wired, 0 at ≥100** — this is the untouched denominator (see
  10-middleware-and-denominator.md).
- **Pinning state:** `config/45410914/splits.txt` pins **773 TUs** (grep
  `^\S+\.(cpp|c):`); `build/45410914/report.json` carries **2,456 units**. So the
  large majority of units are unmeasured/unpinned — the WIRED set (n=13,584 fns)
  is a *fraction* of the binary. Pinning grows the WIRED denominator, which is why
  micro-pinning has a net-WIRED-positive gate (see below and
  04-pinning-at-scale.md).

## Proposal: the dependency graph

### The four fundamental resources, and what gates what

Every match requires four things in order. A vein is blocked at whichever it
lacks:

1. **A denominator entry** — the function must be a *countable* target. The
   42,126 "other" fns (Bink/Quazal/XDK/CRT) currently have 0 wired; whether they
   *should* count is the denominator question (01, 10). This gates nothing
   downstream but reframes the whole percentage.
2. **Identification** — a `.text` span or per-fn VA must be attributable to a
   source TU. This is the **primary wall** for the scattered game layer (class-B
   ICF-scattered methods). Gated veins: 05 (data-xref anchoring), 07 (ICF
   constraint solver), 08 (ML embedding triage), 09 (sibling-title oracles), 15
   (Ghidra-guided synthesis for oracle-poor fns).
3. **Pinning** — an identified span must be entered in `splits.txt` so objdiff
   measures it. This *grows the WIRED denominator*, so it interacts with the
   fuzzy metric (04, and the CharClipGroup micro-pin precedent d696b52). Pinning
   is cheap once identification exists; it is *blocked by* identification for the
   scattered majority.
4. **Body match** — the ported source must compile to byte-identical machine
   code. This is the **secondary wall** (MWCC→MSVC codegen divergence). Gated
   veins: 11 (permuter farm), 12 (grind fleet), 13 (codegen idiom library), 14
   (systematic symbol sweeps), 16 (auto-landing), 17 (unicorn equivalence lane —
   a *relaxation* of this gate to behavioral equivalence).

**The critical edges:**

- **Identification → Pinning → (grows WIRED denominator).** You cannot pin what
  you cannot locate; every pin you add dilutes the WIRED fuzzy % unless the pinned
  fns are already high-fuzzy. This is why CharClipGroup (d696b52) pinned only its
  2 *high-fuzzy own* methods, not the whole scattered set — the gate is
  net-WIRED-positive.
- **Struct-layout fixes → body-match ceilings.** A wrong base-class layout puts
  *every method* of a class in the 40-70% band with a constant member-offset
  delta (the RndEnviron 0xB0 signature). Fix the layout and the whole class's
  ceiling lifts at once. This is why 05/data-anchoring and struct work precede
  per-fn body-port on any layout-diverged class.
- **Body-divergence bound → case-B objdiff fork (19).** A reloc-normalized,
  shiftable-relink equivalence metric (19) only pays off once body divergence is
  *characterized and bounded* — otherwise it relaxes the wrong axis. It sits
  downstream of the codegen-idiom work (13) and unicorn lane (17).
- **Oracle refresh (06) is a multiplier on identification+body, not a source.**
  As matches accumulate, re-diffing reveals newly-anchorable neighbors (the
  reveal cascade: e.g. Waypoint's ObjVector flip revealed +5 own-TU operator
  COMDATs; the grind +22 was 2 real fns + **20 revealed static-init/atexit
  thunks**). It amplifies whatever the other veins produce; it is not itself a
  primary vein.

### Proven-lever inventory (measured yields)

| Lever | Measured yield | Status | Ref |
|---|---:|---|---|
| Class-A TU-pure span harvest | **+403 one session** (cumulative +640 all-sessions) | **EXHAUSTED** (wave-8 +0) | `docs/decomp/research/2026-06-22-classA-tupure-harvest-results.md` |
| LLM grind loop (best-of-N + merge + permuter) | **+22** (2 real fns + 20 revealed thunks) | active, low-yield tail | `docs/plans/grind-loop-calibration-2026-07-07.md`; 3342b30/a1312de |
| Surgical ObjPtrVec→ObjVector<ObjOwnerPtr> flip | **+7** (Waypoint mConnections) | scattered rare cases | d3c6e4f |
| Structural base-class flip (dual-base drift) | **+9** (RndEnviron 0xB0) | **1 keystone, not a family** | `2026-06-21-structural-levers-exhausted-capstone.md` |
| Selective high-fuzzy micro-pinning | net-WIRED-positive (2 methods) | active, tiny per-hit | d696b52 (CharClipGroup) |
| Body-port near-miss tails | **+2** / session (post-class-A pivot) | thin; strcpy wall dominates residue | `2026-06-24-pivot-bodyport-classb-results.md` |
| Class-B identity-transfer micro-pin | **+1** / ~9 candidates | thin ceiling confirmed | same doc (OvershellSlotState) |
| Local-static-Symbol lever | (noted, unquantified here) | fresh, band3-specific | a1312de |

Note on the grind **+22**: only **2** were genuine near-miss closures
(`SetlistTypeToSym`, `CampaignLevel::Configure`); the other **20** were
static-init guard / atexit thunks that *revealed* byte-exact once the parent fns
matched. This thunk-reveal multiplier is real but bounded — it is the same
mechanism the obj symbol patchers (`scripts/`, wired in `configure.py`) exist to
capture, and it does not scale independently of the primary closures.

### Killed-vein list (do NOT re-litigate)

| Vein | Kill metric | Ref |
|---|---|---|
| **topo_locate** (callee-set topological locator) | held-out precision@1 = **3/23 = 0.13** at build (< 0.55 kill bar); design pilot's 0.61 did not reproduce | `2026-06-30-topo-locator-design.md` |
| **BSim seed-propagation** densification | precision **0.24** at sim≥0.90 (degrades vs plain query); no scattered game TU reached the ≥8-fn GO bar | `2026-06-21-bsim-seedprop-densification.md` |
| **/J compiler flag** (default-unsigned-char) | **−18 net** (10664→10646, deterministic 2×); strcpy NUL-test still emits `extsb.`, wall is intrinsic-internal | a8dc075; `2026-06-24-pivot-bodyport-classb-results.md` |
| **Grind body-port enrichment A/B** | REFUTED — variance dominates; enrichment packet did not lift closure rate | 5048142; `grind-loop-calibration-2026-07-07.md` §Round 3 |
| **DC3 "8599 unmatched engine" vein** | INFLATED — LightPreset 132 = 93 identification-wall + 22 reloc-noise + 10 permuter + 5 mis-paired + 0 cheap | `2026-06-21-dc3-engine-vein-yield-pilot.md` |
| **DC3-engine oracle *naming* build** | dead — named bodies still diverge (permuter noise) | `2026-06-22-dc3-oracle-built-engine-naming-dead.md` |
| **strcpy NUL-terminator family** | compiler-internal instruction selection (`extsb.`/`mr.` vs retail `cmplwi`); source- AND permuter-unreachable | `2026-06-24-pivot-bodyport-classb-results.md` |
| **rndobj base re-basing** (BaseMaterial/RndFontBase/RndHighlightable) | RB3-360 == DC3 by 3-way Evidence-1 cross-check; re-basing REGRESSES | `docs/plans/engine-reuse-and-asset-rendering.md`; capstone |

**Refuted scout claim:** the brief said topo_locate was "killed at precision
0.13" — this is **correct** (the *build* result was 3/23=0.13), but note the
*design pilot* claimed 0.61 on an N≥2 pool; the kill is the failure to reproduce
the pilot at build time, not the pilot itself. Both facts should travel together
so nobody re-runs the pilot expecting 0.61.

### The phased plan

**Phase 0 — Instrumentation (prerequisite, cheap).**
Stand up the metrics-of-record dashboard (18) and the honest-denominator decision
(01, 10) BEFORE any grind, so every subsequent phase is measured net-WIRED and
net-STRICT with regression locks (16, honesty gates: `icf_alias_check`,
cold-cache A/B, composed `run1==run2`). Yield: +0 matches, but it is the gate that
makes every later yield *trustworthy*. Without it, warm-CoW stale objs produce
false net-zero (a documented failure mode).
Expected STRICT yield: **0** (enabling).

**Phase 1 — Harvest the identified-but-unpinned + near-miss tail (weeks).**
Everything already located, not yet at 100%:
- Selective high-fuzzy micro-pinning of wired-but-unpinned high-fuzzy fns (04),
  gated net-WIRED-positive (CharClipGroup precedent).
- Grind fleet v2 (12) + permuter farm (11) + codegen idiom library (13) on the
  [95,100) band (**1,503 fns**) and [90,95) (**278 fns**).
- Systematic symbol sweeps (14): local-static-Symbol, guard thunks — one-pattern-
  many-functions fixes.
- Oracle refresh loop (06) after each landing to catch reveal cascades.
Anchored EV: grind measured +22/session; body-port +2/session; the [95,100) band
is 1,503 fns but a large fraction is the strcpy/FP-regalloc/scheduling wall
(source- and permuter-unreachable). Realistic Phase-1 total: **+100 to +300
STRICT** over many waves, then asymptotic. This is the *productive tail*, not a
step change.

**Phase 2 — New identification signals (the actual frontier).**
The ==0 band (51,999 fns) is dominated by unidentified functions. This phase is
the only one that can add *hundreds*. Order by measured precision-so-far:
- Data-xref anchoring (05): vtables (`??_7*@@6B@`), RTTI (`??_R4`), `.rdata`/`.data`
  pins as a *structural* identification signal — orthogonal to the string/oracle
  signals that saturated. Highest-confidence untried direction.
- Sibling-title oracles (09): RB1/RB2/TBRB/GDRB/devkit/TU builds — a *fresh*
  named-source oracle for the game layer that rb3-Wii (near-random on game TUs)
  cannot provide.
- ICF constraint solver (07): global assignment as constraint satisfaction —
  attacks the class-B scatter that killed topo_locate and BSim.
- ML embedding triage (08): learned similarity as a *triage amplifier* (feed
  05/07/09 candidates, not a standalone locator).
- Ghidra-guided synthesis (15): for oracle-poor fns, synthesize source from the
  decompile.
EV is **unbounded above but unproven** — every prior identification attempt for
the scattered layer was killed (topo 0.13, BSim 0.24). If *any one* of 05/07/09
clears a calibrated ≥90%-precision, ≥8-fn-per-TU bar (the BSim GO bar), it
reopens hundreds of fns. Honest per-vein EV: **0 (another kill) to +200-500 (a
breakthrough)**. This is where breakthrough bets live.

**Phase 3 — Denominator + equivalence redefinition (changes the *bar*, not the
count).**
- Middleware/denominator honesty (10): decide whether Bink/Quazal/XDK/CRT count.
  This can move the *reported %* by a large amount with **0 new matched fns** — it
  is an accounting decision, not a decomp win. Must be made honestly and once (01).
- Unicorn behavioral-equivalence lane (17): secondary credit metric for fns that
  are behaviorally equivalent but not byte-identical (relaxes the body-match gate
  for the codegen-wall residue).
- Shiftable relink milestone (19): reloc-normalized equivalence + bootable XEX —
  the *capstone* proof, gated on Phase-2 identification (you cannot relink what
  you cannot place) and Phase-1 body-match bounding.
EV in STRICT: **0** (these redefine the target). EV in *project value*: high
(01/17/19 define what "done" means).

**Phase 4 — Native/playable track (parallel, non-competing).**
Native port + DC3 engine reuse (20) runs orthogonally — it does not add STRICT
matches but delivers a playable artifact and the milo-native-engine extraction.
Run in parallel from day 1; it never blocks and is never blocked by the matching
phases.

### Diminishing-returns curve — where does STRICT saturate?

Anchoring to measured history: the big session gains are behind us. The capstone
records a **+2,946** session (6932→9878) when structural levers were live; the
post-exhaustion sessions record **+403** (class-A, then wave-8 +0), then **+413**
(pivot session, mostly the same class-A), then **+22** (grind), then **+7/+2/+1**
per-lever hits. The derivative is clearly collapsing.

**Saturation estimate WITHOUT a Phase-2 breakthrough:**
Phase-1 harvest of the identified tail realistically adds **+100 to +300** STRICT
(the [90,100) band minus the ~40-60% of it that is source/permuter-unreachable
codegen wall). That takes STRICT from 11,240 (17.13%) to roughly **11,400-11,600
fns (~17.4-17.7%)** and then flattens. **Byte-% barely moves** because the tail
fns are small. This is the honest ceiling of grind-only work: **~17.5-18% STRICT,
low double-digit byte-%**. The 51,999 ==0 fns stay untouched — they are the
saturation floor set by the identification wall.

**Bets that change the curve (in expected-impact order):**
1. **A working scattered-layer identifier** (05 data-xref anchoring is the
   best-odds untried signal; 09 sibling-title oracles is the best fresh source;
   07 ICF constraint solver is the most principled attack). Any one clearing the
   BSim GO bar reopens the game-layer class-B belt — potentially **+200 to +1000+**
   fns, the only bet that moves byte-% materially.
2. **A denominator/equivalence redefinition** (01 + 10 + 17). Changes the reported
   number by re-scoping what counts (middleware in/out) and crediting behavioral
   equivalence. Zero new *byte-identical* matches but potentially a large reported-%
   swing — must be done honestly, not as gaming.
3. **A codegen-idiom breakthrough** (13) that cracks the strcpy/FP-regalloc/
   instruction-selection wall. This is *lower* odds (the /J kill and permuter
   0/all convergence show the wall is compiler-internal), but if a source idiom or
   a compiler-flag combination reproduces `cmplwi`, it unlocks the entire
   already-identified [95,100) codegen-wall residue at once — a one-time **+100-200**.

Bets 1 and 3 are decomp breakthroughs (move the real number); bet 2 is an
accounting/definition decision (moves the reported number). All three should be
*attempted* in Phase 2/3 precisely because Phase-1 grind saturates so low.

## Alternatives considered

- **Pure grind-to-saturation (no Phase 2).** Rejected: the curve above shows this
  caps at ~17.5-18% STRICT. It is the *default* if no breakthrough lands, but
  choosing it *deliberately* forecloses the only bets that matter.
- **Denominator redefinition first (lead with 10).** Rejected as the *lead* move:
  re-scoping the denominator before instrumentation (Phase 0) risks a
  number-that-looks-good-but-means-less. It belongs in Phase 3 after the honest
  baseline dashboard (18) exists.
- **Case-B objdiff fork / shiftable relink (19) early.** Rejected: it relaxes the
  body-match axis before body divergence is bounded, so it would credit
  differences that are actually bugs. Correctly gated behind 13/17.
- **Re-run topo_locate / BSim with more tuning.** Rejected: both are killed with
  recorded precision (0.13, 0.24) below the GO bar. 07/08 are *different* signals
  (constraint-solving, learned embeddings), not tuned reruns of the killed ones.

## Effort & expected value

- **Phase 0** (instrumentation): ~1-2 agent-days; EV +0 STRICT, high enabling
  value.
- **Phase 1** (harvest tail): ongoing; EV **+100 to +300 STRICT** total,
  asymptotic; per-wave +2 to +22 (measured).
- **Phase 2** (new identification): high-variance; EV per vein **0 to +200-500**;
  this is the only phase with a real ceiling-raising bet.
- **Phase 3** (denominator/equivalence): EV +0 STRICT, large reported-% and
  definitional value.
- **Phase 4** (native): parallel; EV +0 STRICT, delivers a playable artifact.

Honest aggregate: **without a Phase-2 breakthrough, ~17.5-18% STRICT is the
ceiling.** With one Phase-2 identification win, plausibly **20-30%+**. The whole
strategic question is whether to fund the Phase-2 bets or accept the Phase-1
asymptote.

## Risks & failure modes

- **False net-zero from warm-CoW stale objs.** Any codegen edit must use
  cold-cache A/B (documented in the honesty gates); warm CoW serves stale objs.
- **ICF-stub-fold inflation.** Pinning + thunk-reveal can inflate counts via
  ICF-folded stubs; guard with `icf_alias_check` (grind +22 was audited: 2 real +
  20 thunks, disclosed).
- **WIRED-denominator dilution.** Pinning low-fuzzy scattered fns *lowers* the
  WIRED fuzzy % while adding countable targets; the net-WIRED-positive gate must
  hold (04, d696b52).
- **Re-litigating killed veins.** The killed-vein table exists precisely to
  prevent this; any agent proposing topo/BSim/`/J`/enrichment must cite a *new
  signal*, not a tuned rerun.
- **Phase-2 all-kill.** The realistic worst case: every identification bet is
  killed like its predecessors, and the project saturates at the Phase-1
  asymptote. This is survivable — Phase 3 (definition) and Phase 4 (native) still
  deliver value.

## Kill criteria

- **Kill Phase 1 as a *primary* focus** when three consecutive harvest waves each
  net < +5 STRICT (the tail is mined out; the class-A wave-8 +0 is the precedent).
- **Kill a Phase-2 identification bet** when its calibrated held-out precision@1 <
  0.55 OR it fails the ≥8-fn-per-TU ≥90%-precision GO bar (the topo_locate and
  BSim kill thresholds — reuse them verbatim).
- **Kill the whole "raise STRICT%" thesis** (accept the asymptote, pivot to
  Phase 3/4) if *all* of 05/07/08/09 are killed at their GO bars. At that point
  the identification wall is proven insurmountable with current oracles+signals,
  and the honest move is to redefine the target (01) and ship the native track
  (20).

## Open questions

- Does data-xref anchoring (05) actually clear the GO bar the string/oracle
  signals could not? It is untested; it is the pivotal unknown for the entire
  curve.
- Are sibling-title oracles (09) *available and legally usable*, and do they
  cover the RB3 game layer that rb3-Wii does not? (rb3-Wii is near-random on game
  TUs.)
- What is the true size of the "other" 42,126-fn denominator that is genuinely
  RB3-authored vs vendored (Bink/Quazal/XDK/CRT)? (Feeds 01/10; changes the
  meaning of every %.)
- Does the thunk-reveal multiplier (grind +22 = 2+20) have a stable ratio, or was
  20 a one-off? If stable, it re-weights the ROI of near-miss closures.

## References

- `tools/fuzzy_progress.py` — STRICT/FUZZY/staircase/histogram (metric source).
- `build/45410914/report.json` — objdiff measures (2,456 units).
- `config/45410914/splits.txt` — 773 pinned TUs.
- `docs/decomp/research/2026-06-22-classA-tupure-harvest-results.md` — class-A
  +403/session, +640 cumulative, EXHAUSTED.
- `docs/decomp/research/2026-06-24-pivot-bodyport-classb-results.md` — body-port
  +2, class-B IDT +1, strcpy wall, `/J` = −18 KILL.
- `docs/decomp/research/2026-06-30-topo-locator-design.md` — topo_locate build =
  3/23 = 0.13 (killed; design pilot 0.61 did not reproduce).
- `docs/decomp/research/2026-06-21-bsim-seedprop-densification.md` — BSim
  seed-prop precision 0.24, NO-GO.
- `docs/decomp/research/2026-06-21-structural-levers-exhausted-capstone.md` —
  +2946 session, RndEnviron +9 keystone, structural levers exhausted.
- `docs/decomp/research/2026-06-21-dc3-engine-vein-yield-pilot.md` — DC3 engine
  vein INFLATED.
- `docs/plans/grind-loop-calibration-2026-07-07.md` — grind +22 (2 real + 20
  thunks), enrichment A/B REFUTED (variance dominates).
- Commits: d3c6e4f (Waypoint ObjVector +7), d696b52 (CharClipGroup micro-pin),
  a8dc075 (/J KILL), 3342b30 / a1312de (grind close-out +22), 5048142
  (enrichment refuted).
- `docs/plans/engine-reuse-and-asset-rendering.md` — Evidence-1 3-way cross-check
  (rndobj base re-basing = REGRESS).
- `docs/INDEX.md` — audited master doc index (flags stale docs).
- Sibling RFCs: 01-endgame-definitions, 02-gap-composition-atlas,
  04-pinning-at-scale, 05-data-xref-anchoring, 06-oracle-refresh-loops,
  07-icf-constraint-solver, 08-ml-embedding-triage, 09-sibling-title-oracles,
  10-middleware-and-denominator, 11-permuter-farm, 12-grind-fleet-v2,
  13-codegen-idiom-library, 14-systematic-symbol-sweeps, 15-ghidra-guided-synthesis,
  16-auto-landing-pipeline, 17-unicorn-equivalence-lane, 18-metrics-and-dashboard,
  19-shiftable-relink-milestone, 20-native-port-and-engine-reuse.
