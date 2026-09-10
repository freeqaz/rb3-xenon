# Roadmap — gap to target (2026-09-01)

> **STATUS (2026-09-01): CURRENT.** Written by the coordinator from four
> concurrent Opus survey lanes run 2026-09-01 (GAP-A fresh measurement, GAP-B
> identification, GAP-C structural-vs-grind, GAP-D tooling audit), all against
> HEAD `dc605388` on the shipped `name_check` ruler. Supersedes the workstream
> section (W0–W5) of `docs/decomp/CAMPAIGN_STATE_2026-08-17.md` as the plan of
> record; that doc's history/rounds record stands. Every figure below carries
> its derivation; per the standing rule, RE-MEASURE these before building on
> them — the denominator has now taken three distinct values in four weeks
> (10,688,688 → 10,320,664 → 10,245,956).

## 0. Incident found and repaired during the survey

On 2026-09-01 the main tree held **244 of 1,205 decomp objects rebuilt outside
the full build graph** (mtime burst 04:32:12–:13, objcache-speed, patchers never
ran; `verify_objs_patched.py --verify-manifest` rc=1). The ordering exonerated
the Aug-31 `report.json` (drift strictly after it was written). GAP-A repaired
it with a full `./tools/ninja-locked` — **zero compile edges, patch chain
only** — and the regenerated report is **identical on all 69,219 rows**
(`--verify-manifest` now rc=0, `tree_sha256=14b7c8dc9f4dbbc9`). Whoever ran the
targeted build: this is the `⛔ NEVER ninja <one>.obj` hazard firing live; any
whole-binary number read from the tree between 08-31 18:12 and the repair is
suspect. GAP-C separately proved the drift did **not** contaminate per-row
partial-stratum reads (0 disagreements on 8,131 rows, clean and drifted units
alike) — only whole-binary aggregates were exposed.

## 1. Where we are — fresh measurement

Provenance: `build/45410914/report.json` regenerated 2026-09-01 04:40 on the
repaired tree, objdiff **4.2.8** (`358c715835cc`), ruler
`functionRelocDiffs=name_check` read from `provenance.diff_config`;
`verify_ruler_agreement.py --check` PASS.

| measure | value |
|---|---:|
| `total_code` | **10,245,956 B** / 69,219 fns |
| `matched_code` | **3,772,844 B = 36.823%** |
| `matched_functions` | 42,276 (`mpn==100`) |
| `masked_equal_functions` | 22,911 → **honest 19,365** |
| reachable ceiling (scaffold-corrected) | **6,296,688 B = 61.455%** |
| position vs ceiling | **59.92% of reachable** |
| **gap to ceiling** | **2,523,844 B** |

Two structural identities GAP-A proved exactly (sharper than the recorded
approximations): `masked_equal_functions` **is** the set of placeholder-named
rows at `mpn==100` (22,911, bit-exact), and **honest ≡ real-named rows at
`mpn==100`** — zero real-named rows receive masked-equal credit.

★★★ **`matched_functions` fell 44,514 → 42,276 since 08-20 and it is a TOOL
ARTIFACT, not a regression.** objdiff 4.2.3→4.2.8 largely collapsed `mpn`'s
arg-blindness: 2,214 rows lost `mpn==100` with **2,196 of them bit-identical
`fuzzy`** across 548 units (source work cannot do that); the
`mpn==100 ∧ fuzzy<100` class shrank 5,699 rows/763,460 B → 3,392/213,524 B.
Real progress over the same window: **+13,964 B, reconciled to the byte**
(103 rows crossed, 2 fell, +1,440 B rename churn). ⚠ Corollary: the recorded
"28.38% of the gap was the arg-only class, DRAINED by lanes" line is partly
wrong — a large share of that stratum was **collapsed by the tool**, not
drained by work. And function-count absolutes across the 4.2.3→4.2.8 boundary
are incomparable, exactly like byte absolutes across the 08-12 ruler flip.

Alias forgiveness: last **measured** (by ablation, the only valid method)
2026-08-16 at 818,416 B / 7.93 pp. The alias file has since changed shape
dramatically (memberships 15,196 → 5,338, groups 1,528 → 1,591; `97771c75`
retired a 9,395-membership fabrication class for only −4,128 B) — the figure
is **stale and must be re-ablated before anyone quotes an exposure number**.

## 2. What "100%" means — three targets, three answers

**(a) 100% of `total_code` — NOT A REAL TARGET.** Requires 6,473,112 B, of
which **2,086,328 B (20.4%) is XDK vendor code with no source**, out of scope
by standing user directive (and already 100% mapped, which satisfies the
mapping goal there). In-scope maximum: **79.64% of total_code**.

**(b) 100% of the reachable ceiling — THE HONEST MATCHING TARGET.**
2,523,844 B remain. Decomposes exactly (GAP-A):

```
credited residual (0<fuzzy<100)           1,333,032 B
named rows at 0%                             48,276 B
placeholder rows INSIDE pairable units    1,322,732 B
less map-scaffold shells                   −180,196 B
                                        = 2,523,844 B  ✓
```

**(c) 100% identification — MOSTLY ALREADY MOOT.** 28,299/69,219 rows (40.9%)
carry real names, holding 64.0% of bytes. Of the 40,920 placeholders, 19,675
(745,288 B) are **already fully credited** via byte-signature pairing — no
name needed. The real backlog is 21,245 rows / 2,942,048 B, of which only
1,322,732 B sits where identification could ever convert to bytes — and see
§3: it mostly can't.

⚠ Raising the ceiling remains **structurally self-cancelling** (Δgap exactly 0,
measured, lane W5-CEILING) — pin+wire raises the target and collects it in the
same step. It is not a route to closing anything.

## 3. The load-bearing finding: the "identification wall" is a BODY problem

GAP-B proved, **from objdiff's source, not from the metric**: anonymous target
rows already participate in reloc-masked byte-signature pairing
(`is_funclet_like()` accepts any `fn_<8hex>` symbol). So **if our object
produced byte-identical code for an anonymous row, it would already be
matched without a name.** An anonymous row still at zero is one whose bytes we
do NOT reproduce — naming it cannot cross it, and `matched_code` is
all-or-nothing per row. **Identification is structurally incapable of being a
byte lever.**

Decomposing the 1.32 MB of anonymous zero-rows inside pairable units:
**~71% is missing bodies** (retail's TU owns more code symbols than our obj —
the extreme is Quazal scaffolds: `PRUDPEndPoint` retail owns 67 symbols, our
7-line `namespace Quazal {}` obj owns 0), **~13% is fold-ambiguous by
construction** (body byte-duplicated inside its own target obj), and the
genuinely-nameable bijective residual is **~21 kB ≈ 0.2% of total_code**.

Every bulk identification channel is measured dead (GAP-B channel table:
BinDiff decoy-null p95=1.000 ⇒ no threshold exists; BSim precision 0.16–0.36;
proximity flat 26–28% at every distance; span transfer precision@1 = 0.115;
autoid has **never had a precision measurement** — do not run it until it
does). Live instruments are per-function and pay in **bug exposure + honest
pairing**, not bytes: callee-read from a 100%-matched caller (best), `??_R0`
TypeDescriptor reads (88/88 clean, nearly exhausted), gated body-identity
(FP 0.33%), adjustor-thunk vtable-slot identity (untested, 392 B).

⇒ **Do not fund identification as a wave.** Use the live per-function
instruments opportunistically when a specific row blocks other work.

## 4. Structural vs grind — the work-kind partition

GAP-C charge-classified 100% of both partial strata (8,131 rows, 0 failures)
and decomposed `reachability_census`'s priority cascade into a real partition.
Two corrections to the recorded framing first:

- ⛔ **"WALLED_REG = 557,612 B" is a cascade artifact, not a permuter prize.**
  The verdict tests `reg > 0` first, so it absorbs every mixed row. Truly
  permuter-shaped (pure register ± immediates): **84,548 B = 1.3% of the gap**.
  The other ~476 kB also carries hard structural charges — and register swaps
  have repeatedly **dissolved** when the real body defect was fixed.
- The name-charge class (E, 650,480 B) is per-pair dispersed (1,735 distinct
  pairs, top pair 1.5%) but **family-concentrated**: top 10 template method
  families = 178,764 B (34.8%); 58% of instances are same-method template
  siblings (`push_back`/`insert`/`~`/`_M_fill_insert`…). Proof is per-pair;
  tooling is per-family.
  > ⛔⛔ **REFUTED SAME DAY — see §4a. The class is ~99% irreducible, and the
  > figure above is a COMPOSITE OF TWO POPULATIONS that no artifact contains.**

### §4a — the fold class is NOT an investment item (lane S1-FOLDTOOL, merge `0facddf1`)

The roadmap's original #3 item is **refuted by the very instrument it asked
for**. `tools/s1_fold_family.py` adjudicated the top 20 families (427,320 B =
65.6% of class E, 1,182 pairs) on retail bytes:
**REFUTED 795 (67.3%) · UNDECIDED 376 (31.8%) · PROVEN_FOLD 11 (0.9%)**.

Priced by ROW (because `matched_code` is all-or-nothing per row):

| | bytes | % of covered |
|---|---:|---:|
| **FLOOR** — rows whose every pair is proven today | **2,480** | 0.64% |
| CEILING — if *every* UNDECIDED pair were also a fold | 123,860 | 31.78% |
| **HARD-REFUTED** — ≥1 pair proven NOT a fold ⇒ can never cross | 236,448 | 60.67% |

Extrapolated to the full class: **floor ≈ 4.1 kB, ceiling ≈ 207 kB** — and the
ceiling is the measurement's own uncertainty, not a prize (collecting it needs
the anonymous-operand resolver W33 asked for, which nobody has built). 93.7% of
refutations rest on direct body evidence, not on map correctness.

⛔ **My briefed figure was a COMPOSITE.** "2,183 rows / 1,735 pairs /
650,480 B" takes its **bytes** from the 8,131-row placeholder-inclusive
population and its **row/pair counts** from the 3,427-row named-only one.
Separated, each leg reproduces to ~0.1%: named-only **2,181 rows / 1,732 pairs
/ 514,984 B**; placeholder-inclusive **651,116 B**. A coordinator brief fused
two populations and the fused number survived into a roadmap — the standing
`READ THE IN-TREE RECORD FIRST` failure, in a plan document this time.

★★★★★ **The tool was wrong first, in the direction that MANUFACTURES work.**
Its first revision refuted 222 pairs as "identical bodies, different callee
name" — but retail's `push_back<pair<VocalPhrase*,VocalPart*>>` calls a helper
the map names `_M_insert_overflow_aux<vector<MicClientID>>`, and **one retail
body naming two unrelated `T` is impossible without folding**. It was charging
the linker's arbitrary survivor-name coin-flip as a source defect; the evidence
for the fold sat inside the very string used to refute it. The fix cost real
refuting power (negative control REFUTED 11→4), stated rather than hidden.

**Accuracy payoff, which is the class's real value:** of the hard-refuted 60.67%,
**245 pairs are thunk-vs-real-function** — retail calls a 188 B `MemFree` where
we call a 4-byte forwarder. That is inline/thunk POLICY, not folding; the whole
`MemFree` family is **159/159 refuted**. Any future work here is a
correctness/inline-policy lane, not a fold lane.

Whole-gap partition (sums to 6,473,112 B exactly; % of gap-below-100%):

| kind | bytes | % | closes via |
|---|---:|---:|---|
| irreducible/out-of-scope — XDK vendor | 2,090,904 | 32.3% | nothing in scope |
| largely unreachable — `auto_*` unattributed | 1,677,528 | 25.9% | identification (adjudicated ~dead; ~0.2% residual) |
| grind — missing bodies in pairable units (anon 0%) | 1,322,732 | 20.4% | **porting real bodies** from oracles (mostly Quazal-flavored; triage needed) |
| ~~structural — ICF fold adjudication (E class)~~ **⛔ REFUTED §4a: ~99% irreducible, floor ≈4.1 kB** | 650,480 | 10.0% | ~~family-mechanized retail-byte proof~~ → reclassify as (d) irreducible; residue is an inline/thunk-policy correctness lane |
| grind — body divergence entangled with regalloc (C/D) | 475,832 | 7.4% | per-function, hardest class; fund only where oracle > our source |
| permuter-shaped (pure register), DEFERRED | 84,548 | 1.3% | permuter — measured 0/66 conversion so far |
| mixed name+other (F) | 66,192 | 1.0% | case-by-case |
| grind — clean source levers (G, no reg/name charges) | 55,980 | 0.9% | per-function with readable controls |
| grind — named 0% pairable rows | 48,276 | 0.7% | per-function / stubs→bodies |
| other unpairable | 640 | 0.0% | — |

**Direct answer to "structural or grind?": both, in a ~1:3 byte ratio inside
the addressable slice — but the structural work is where the tooling leverage
and the accuracy/native payoff live, and the grind is dominated by ORACLE
PORTING (missing bodies), not by instruction-tweaking.** The classic
"per-function source grind with a readable control" class is genuinely tiny
(~56 kB; the named SOURCE_LEVER charge class is 37,696 B = 0.58% of the gap —
re-derived within 3% of W44's figure).

Within the structural slice, one **new, previously-unsized lever**: 442 rows /
87,496 B where the map **refutes** the fold hypothesis and the divergence is
same-method **container/element type** — retail uses the concrete type
(`list<CharClip*>::insert`, `ObjPtrList<CharInterest>`, `ObjDirItr<CharBone>`)
where our headers use the generic `Object` form. One header declaration fix
pays every call site in the TU; 262 units, top 20 = 41.2%. This is squarely
inside the "vtable + struct work is especially valuable" directive and is
behavior-relevant for native. ⚠ The class provably also contains **map**
errors (the `Handle@GemPlayer` archetype) — adjudicate per-pair on retail
bytes; and note GAP-C's instrument is one-directional (address-keyed map can
refute a fold, never confirm one).

## 5. Permuter: preconditions before un-deferral

Deferred-not-refused per user directive. Facts to price it honestly:
realistic ceiling **76,508 B / 0.75 pp** (159 rows, two independent
derivations); measured conversion **0/66** on this residual class (plus 0/121
on dc3); the 08-31 revert (`843c7e98`, `PERMUTER_REVERT_2026-08-31.md`)
showed the objective is **crossing-blind** (+3,810 fractional claimed, −4 B
graded delivered) and generated **23 confirmed behavior defects, 21 compiling
clean**. The defect-generating hoist/sink transform
(`decomp_synth/patterns/statement_reorder.py`/`assignment_reorder.py`) is
**untouched since the audit** — turning the permuter on today re-arms the
identical hazard. The prize is concentrated: top row 12,220 B behind 2
register charges (`?CountOrCreateExpandedDetails@NextSongPanel@@`) — the top
handful may be hand-tractable via `fixable-liveness.md` without the permuter.

**Preconditions:** (1) crossing-aware objective (score = graded
`matched_code` delta, not fractional fuzzy); (2) disable/repair the
hoist-across-call transform; (3) behavioral gating (unicorn verdicts) on every
candidate win. Until all three: stays deferred.

## 6. Tooling: what exists, what to build

The measurement stack is sound where it matters — `ab_measure`,
`verify_ruler_agreement`, `icf_alias_finder --validate` (empty-file vacuity
fixed, `9b961bca`), `verify_objs_patched`/`patch_guard` (fired live and
correctly this week), MCP `run_objdiff` patcher-skip fixed (`4ba12257`). All
carry demonstrated-can-fail selftests. `validate_symbols` and `map_lint` were
repaired during the survey (TU0 phantom ranges; crash-on-valid-input).

**Build these (ranked, all cheap):**

1. ✅ **DONE (lane T1-FRESH)** — **Freshness precondition on
   measurement-consuming tools.** `scripts/analysis/freshness.py` reuses
   `patch_guard.ensure_patched_tree(build=False)` and adds the four things the
   manifest cannot answer: a NON-VACUITY floor; "report.json is non-vacuous and
   not older than the manifest"; **TOOL IDENTITY** (`provenance.tool_binary_hash`
   vs the binary a live diff will run); and the **ICF ALIAS MAP** not being
   newer than the report. Every stale subject is named
   (`STALE OBJECTS`/`STALE TOOL`/`STALE ALIAS MAP`/…), collected rather than
   short-circuited — "stale" without a subject sends the next lane to rebuild
   the wrong thing. Wired into
   `reachability_census` and `verify_ruler_agreement --verify-scores/--selftest`
   (**not** `--check` — that is a ninja edge gating REPORT and would deadlock).
   Raises rather than returning a number; `--allow-stale` overrides behind a
   banner; `scripts/test_freshness.py` proves each guard can fail by mutation.
   ⚠ Two corrections to this item as written: the floor is **load-bearing, not
   belt-and-braces** — `verify_objs_patched --verify-manifest` prints
   `OK: 0 decomp, 0 target objects match` and exits **0** on an empty manifest —
   and the gate must be **content-keyed, not mtime-keyed** (380 objects in a
   fresh worktree were newer than report.json and byte-identical to it).
   ⛔ **An objects-only gate would have passed this tree while it was
   unmeasurable**: the shared `bin/objdiff-cli` (a symlink into ../objdiff,
   shared with ../rb3 and ../dc3-decomp) was rebuilt at 08:59 under running
   lanes while the report dated from 08:25 — which is why item 3's provenance
   record and this gate agree that the tool and alias-map hashes are the
   discriminators. ⚠ Both are **xxh3_64**, so the tool is asked for its own
   identity (`--version`) and the map is checked by mtime; a `sha256[:16]`
   guess reads `ee78f52f…` where the truth is `faf33906…` and would refuse
   every tree forever.
   ⚠ **The commissioning diagnosis was NOT confirmed.** `--selftest`'s red
   state did not reproduce: it measured GREEN (3,323 examined / 0 disagree /
   31 control, byte-identical) on a settled worktree, on main, with the new
   binary against an OLD report, and with the new binary against a fresh one.
   What IS measured is that an **unbuilt** worktree makes it exit 5 VACUOUS
   while blaming its own `WITNESS_UNITS`, and that a tree with 2 genuinely
   unpatched objects was passed by `--verify-scores` at **rc=0** — so the
   score comparison cannot detect an unpatched tree; only the manifest can.
2. **Fix `crossing_worklist.py`'s ruler** (~5 lines): line ~396 hardcodes
   `functionRelocDiffs=none`, so the tree's only size-if-it-crosses ranker
   systematically over-reports rows as source-reachable — the exact
   `CustomizePanel` failure that misled three lanes — while *looking* like
   the trustworthy tool because it has a selftest. Import
   `scripts/analysis/ruler.py`, re-run its selftest.
3. ✅ **DONE — longitudinal gap ledger** (`tools/progress_ledger.py`, merge
   `ceb9eaf2`; first production snapshot `412e3f85`). Records headline
   measures + gap strata + **tool provenance** (objdiff version/commit/binary
   hash, resolved `diff_config`, AND the ICF alias-map hash — the last two
   were the actual discriminators) keyed by merge commit. Verdicts:
   SOURCE_PROGRESS / RULER_OR_TOOL_CHANGE / DENOMINATOR_CHANGE / REGRESSION /
   INDETERMINATE, each naming the evidence used *and the evidence missing*.
   Known-answer test passes in production. Refuses without an explicit key
   (rc=2, fail-closed). **It corrected two of this doc's own claims**: the
   `mpn` collapse is objdiff **4.2.3→4.2.5** in a 99-minute window (not
   4.2.8), and `total_code` has taken **~24** distinct values, not 3 — from
   `build/45410914/progress_history.jsonl`, a **249-row series
   `scope_map.py` has been writing since 07-29 while gitignored**, i.e.
   invisible institutional memory of exactly the kind that has twice caused
   duplicate lane funding.
4. ✅ **DONE (lane T1-FRESH)** — **`reachability_census.py`'s variable
   clobber.** `verdict()` now reads `name_chg`, the key the same `update(...)`
   already wrote, so the counts and the row identity never share a key. The
   `reg` half of that assignment was always a no-op. Measurement-neutral by
   construction (verdict read `(g, n)` before and reads `(g, n)` after) and
   verified by measuring the CHARGE CLASSES table immediately before and after
   the edit on one settled tree. Coverage now genuinely passes **and** can
   still fail; the SOURCE_LEVER printer runs for the first time.
5. **Fold-adjudication family tool** (the enabler for §4's structural slice):
   per-family retail-byte COMDAT proof for the top template families, with
   the W33/W34 rule baked in (**never mask a relocation; unresolvable ⇒
   UNDECIDED**) and `ALIAS_SUSPECT` integrity gates. This is the one
   *new* instrument the roadmap needs.
6. **Native runtime measurement on a schedule** (see §7): the link gate is
   solid but proves LINK, not RUN; real runtime oracles exist in
   `main_render.cpp`/`main_milo.cpp` (~25 gates, forced-failure controls) but
   are manual and uncollected. Cheapest win: run them + `scatter_audit.py` on
   a cadence and record results (scatter drift 42→47 went unnoticed).
7. **Re-run the alias ablation** (not a build — a measurement): the 7.93 pp
   forgiveness figure predates a 65% membership collapse.
8. **Refresh unicorn behavioral coverage**: 7,960 verdicts, all from one
   72-minute window on 2026-07-16 (11.5% of rows); six weeks of landings
   carry no verdict. This is the instrument class that catches "matched but
   wrong" — the defect class that breaks native.

**Explicitly not worth building** (GAP-D, with reasons in its record): a
native golden-image comparator (no ground truth exists; invariant oracles are
the deliberate design), a new per-row pricer (`w25_charge_census.py` exists —
wrap it), an alias group-count-monotonic gate (refuted: fires on 13.5% of
legitimate commits).

## 7. Investment plan

Ordered by expected value per effort, honoring the standing directives
(native is the real goal; accuracy > headline; vtable/struct high-value;
XDK out of scope except pinning + mem-mgmt subset):

| # | workstream | expected payoff | notes |
|---|---|---|---|
| 1 | **Tooling items 1–4** (freshness, crossing ruler, ledger, census fix) | trust in every future number | days, not weeks; do first |
| 2 | **Container-type divergence sweep** (87.5 kB, 442 rows, 262 units) | bytes + accuracy + native correctness | per-pair retail-byte adjudication; expect some map-error outcomes |
| ~~3~~ | ⛔ **ICF fold family adjudication — DO NOT FUND** | ~~the largest addressable slice~~ **floor ≈4.1 kB** | **Refuted by its own instrument the day it was proposed** (§4a, merge `0facddf1`): 67.3% of pairs REFUTED, 0.9% proven, 60.67% of covered bytes can NEVER cross. The tool is built and shipped (`tools/s1_fold_family.py`) — use it to REFUSE fold claims, not to hunt bytes. The ~91%-irreducible prior was right and the pre-registration was too generous |
| ~~4~~ | ⛔ **Missing-body porting triage — DO NOT FUND** (1.32 MB anon-zero in pairable units) | ~~the only big vein left~~ **oracle-backed slice = 1 unit / 52 B** | **Triaged and refuted** (lane P1-BODYTRIAGE, `docs/decomp/p1-bodytriage-anon-zero-vein-2026-09-10.md`). The briefed decomposition inverted: **17.7% unwritten, not ~71%** (GRIND-1 measured the same 17% on 08-14 and the record was in-tree the whole time). **band3's rb3-Wii oracle adds 52 B across 168 units** (97.7% is `WE_ALREADY_HOLD`); engine `ORACLE_SURPLUS` collapses **115×** to 988 B against the function-count deficit; **Quazal is 96.8% inside the measured `/Od` band** ⇒ unmatchable at `/O1` at any source quality. Genuine write surface **≤3.4 kB**; historical conversion **0/10 TUs, 0/55 methods**. Residual value is **accuracy, not bytes** (8 MIS-PIN SUSPECT units / 43 kB) |
| 5 | **Clean source levers + close-to-crossing grind** (~56 kB + the ≤3-charge heads) | steady small wins | the existence of a pre-registration control is the screen — human judgement, not a sweep |
| 6 | **Native runtime instrumentation + unicorn refresh** (tooling 6, 8) | serves the actual goal directly | "matched but wrong" is what native surfaces |
| 7 | **Permuter un-deferral** — only after §5's three preconditions | ≤0.75 pp ceiling, historically 0 conversion | cheapest path may be hand-work on its top-5 rows instead |
| — | Identification waves, ceiling-raising, autoid re-runs, vtable order sweeps, `SOURCE_INSDEL` re-sweeps, dual-heading splits merge | **do not fund** | each has a dated refutation; see GAP-B/C ledgers and the drained table in CAMPAIGN_STATE |

**Honest bottom line on the matching metric:** from 59.9% of reachable, the
fully-addressable in-scope remainder (structural + controlled grind +
permuter ceiling, taking measured irreducibility rates at face value) is
plausibly **+6–12 pp of total_code over a long campaign**, dominated by how
much of the fold class proves and how much of the missing-body vein is
oracle-backed. 100% of the reachable ceiling is not attainable —
GAP-C/MPNGAP-1-class measurements show a substantial fold/ICF-destroyed
residue inside it (129,360 pair-bytes proven information-destroyed, plus the
~91%-irreducible name stratum) — and that is fine, because the metric is the
means: **the native port is the goal, and the highest-value work above
(container types, fold adjudication as bug-finder, unicorn coverage, runtime
gates) is exactly the work that makes native correct.**

## 7a. EXECUTION LOG — what the first tooling/structural wave actually measured

Eight lanes ran 2026-09-01 (four survey + six execution). Landed results, and
**every one of them corrected something in this document or in my brief**:

| lane | outcome |
|---|---|
| **T3-LEDGER** | ✅ `tools/progress_ledger.py`. Corrected §1: the `mpn` collapse is objdiff **4.2.3→4.2.5**, not 4.2.8, and `total_code` has taken **~24** values, not 3. Recovered a **gitignored 249-row series** `scope_map.py` has written since 07-29 |
| **S1-FOLDTOOL** | ✅ `tools/s1_fold_family.py` — and **REFUTED item 3** (§4a): fold class ~99% irreducible, floor ≈4.1 kB. Also caught my briefed figure as a **two-population composite** |
| **T2-RULER** | ✅ `crossing_worklist.py` priced on `none` while classifying on graded: **79.08% of rows mispriced**, 33.2% of advertised bytes were phantoms. Root cause: the file was **absent from `ruler.py`'s `_CONSUMERS`**, so the regression guard passed cleanly for 18 days. Also found **3 of 10 human-ratified "known positive" pins were never pure** — they were ratified *on the wrong ruler* |
| **S2-CONTAINER** | ✅ **+10 fns / +1,936 B**, all MAP repairs on retail-byte RTTI evidence. Refuted 2 of 4 briefed shapes as fold noise, then **corrected its own headline**: high unit-spread does NOT distinguish folding from a wrong name on a popular callee (that misread hid its largest win, +1,136 B) |
| **S3-ABLATE** | ✅ alias exposure **811,492 B / 7.92 pp** — memberships fell 65%, bytes moved 0.85%. **My Δ`matched_functions`==0 validity check EXPIRED** (objdiff `b14ba45`, 08-20). Found the **CI gate RED on main** |
| **S4-UNICORN** | ✅ matched-but-wrong **1,017 → ~6 (99.2% artifact)**. **~31% of EQUIVALENT verdicts are not evidence** (both sides erroring at the same PC is laundered into a positive). Killed the two most attractive "bugs" by byte-identity. My brief wrongly listed `cap_exhausted` as a real-bug class: **+542 phantom rows** |
| **S5-NATIVE** | ✅ native **PASS 18/18, 0 SKIPs** on a cold worktree. My briefed forced-failure controls **do not exist** (env vars, not flags) and misspelling them passes **silently green**. `main_score2.cpp` returns 0 unconditionally while printing DIVERGENT |

**Coordinator-level lesson, three instances in one session:** my briefs were wrong
three times (composite figure, expired validity check, non-existent controls,
`cap_exhausted` class), and **every error was caught by the lane, not by me** —
because each brief demanded a control that could fail. A brief that says "verify
before building on this" is worth more than a brief that is correct.

⚠ **Two integrity events worth propagating:**
1. **The fleet-shared `objdiff-cli` was rebuilt at 08:59 mid-session**
   (`358c715835cc` → `210aab60ca30`). It is a symlink shared by three repos.
   Detected because T2's `--self-break` returned **rc=4 VOID** naming the moved
   binary rather than a verdict. S2 and S3 each re-measured on one pinned
   binary; deltas measured across the boundary **must not be summed**.
2. **A map repair and the alias file are COUPLED, and the coupling is INVISIBLE
   UNTIL THE SPLIT RE-RUNS.** The alias CI gate went red **three times** off one
   lane's map renames: once pre-existing (`SetJump`), once on merging S2, and
   once **only after a full rebuild** — because `obj_target_symbol_renamer` is a
   *pre-compile* step, so a map edit does not reach the **target objs** until the
   split re-runs, and my first two "green" verifications were reading
   **pre-rename** objects. The gate was right every time; my verification window
   was too early twice.
   ⇒ **Re-run `icf_alias_finder --validate` after landing any map change AND
   after a full `ninja-locked` — never before.** All three repairs measured
   **Δ0** (binary-pinned `ab_measure`, both legs settled), so this is free
   accuracy; a red CI gate over the ~7.9 pp forgiveness mechanism is not.
   ★ Adjudication rule: **retail-byte evidence outranks an automated alias
   tier.** The `SetJump` group is the self-confirming class in its purest form —
   its `address` field was one symbol's while its `survivor` named the other, so
   its recorded T1 test **compared a symbol against itself** and was true by
   construction. Two spellings at two distinct addresses **did not fold**; that
   is the one direction an address-keyed map can settle.

## 8. Lane records

Full survey transcripts are session artifacts (not committed); their key
figures are reproduced above with method. Derivation tools:
`tools/reachability_census.py`, `tools/reachable_ceiling.py`,
`scripts/verify_objs_patched.py`, read-only `objdiff-cli diff` at the graded
ruler. GAP-C flagged two stale in-tree claims not fixed by the survey
(CLAUDE.md dual-heading paragraph superseded by `b341d7ab`'s −40 B
measurement; `reachability_census.py:~250` clobber) — tooling item 4 and a
doc pass own these.
