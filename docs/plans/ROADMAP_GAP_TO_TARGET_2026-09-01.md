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
>
> ⛔ **CORRECTION to the line above (lane T3-LEDGER): `total_code` has taken
> ~24 distinct values, not three** — it wobbles ~100 B routinely. The source is
> `build/45410914/progress_history.jsonl`, a 249-row series `tools/scope_map.py`
> has been writing since 2026-07-29 **while gitignored**, i.e. invisible. Never
> memorise the denominator; read the key.
>
> **RESUMED 2026-09-10.** Tree unchanged since 09-01 (0 commits in the gap,
> patch-state green, `report.json` `tool_commit 032122696555` == `../objdiff`
> HEAD, so no tool drift this time). Items 1–5, 7 and 8 of §6 are **DONE**;
> item 6 is **UNBLOCKED** — the GPU works again, verified *functionally*
> (`vkCreateInstance` → `VK_SUCCESS`, 2 devices) rather than by version string,
> after the NVIDIA kernel module was reloaded to 610.57.04 on a package update
> with **no reboot**. Three lanes ran against the remaining plan; **two have
> landed and both returned refutations**:
> - ✅ **N1-GPUGATES** (merge `a18a9885`) — §6 item 6 CLOSED. `verdict=PASS
>   18/18, 0 SKIPs, runtime 3/3, 37 gates, 0 failed`. Found that
>   `native_health.sh` was **crediting a control over a PRE-EXISTING RED** (the
>   same-breakage-twice trap as a configuration mismatch). Link gate wired into
>   CI; `ALLOW_INCOMPLETE` proved unable to mask a break.
> - ✅ **P1-BODYTRIAGE** (merge `4483c213`) — §7 item 4 **REFUTED: DO NOT FUND.**
>   The oracle-backed slice is **1 unit / 52 bytes**. See §7 item 4 and §3's
>   correction below.
> - ✅ **U1-DEEPSCHED** (merge `1b45fc5b`) — §6 item 8's residue CLOSED, and it
>   **inverted the framing**: the fixture was **BROKEN, not shallow**. On the
>   repaired fixture, genuine `logic`-class divergences are **0 of 22,718**, and
>   the lane audited its own 40-row headline down to an **upper bound** after
>   all 15 rows of its largest cluster dissolved.
>
> ⇒ **All three lanes returned refutations.** Every workstream this roadmap
> proposed has now been measured, and the ones with the largest advertised
> prizes were the ones that died. That is the document working as intended.
>
> ⛔ **AND §0'S INCIDENT RECURRED DURING THIS WAVE, LARGER: 642 of 1,205 decomp
> objects drifted** in main, "produced OUTSIDE the full build graph" — the
> `NEVER ninja <one>.obj` / `objdiff-cli --build` hazard again, at 2.6× the
> scale of the 244 recorded in §0. It was invisible until asked, and a
> measurement taken on that tree reads LOW one-directionally. ★ **The alias CI
> gate's own class split moved under it** (`STALE_SPELLING` 82 → 88,
> map-consistent 1370 → 1364) with **no map or alias change** — i.e.
> ⚠ *(2026-09-11: the "outside the build graph" reading of this 642-object
> drift is RETRACTED as evidence — see the correction under §7b.)*
> `icf_alias_finder --validate` guards the ~7.9 pp forgiveness mechanism with
> **no freshness precondition**. T1's guard covers 2 tools of ~35; this is the
> next one that needs it.
>
> ✅ **DONE — the gate now refuses a stale tree** (`STALE_TREE`, exit 2, same
> idiom as its existing refusals). `need_report=False` on purpose, since
> `cmd_validate` never reads `report.json`, so the report-mtime and
> tool-identity axes cannot false-refuse in CI; and it sits in `cmd_validate`
> rather than `preconditions()`, which is frozen-fixture-driven and must stay
> hermetic. Verified both directions with real exit codes: healthy main rc=0
> (`PASS 1370/219/0`), a genuinely stale worktree rc=2 naming the tree,
> `--allow-stale` overriding a REAL refusal, and `--selftest` still green with
> all five pre-existing controls firing on their own reasons.
> ★ **It caught real drift on its first production run** — `main` drifted a
> THIRD time at 23:54 (27 objects, `system/synth/*`), corroborated independently
> by `verify_objs_patched --verify-manifest` rc=1.
> ⛔ **And my first version was wrong in the flattering direction**: it printed
> "freshness refusal OVERRIDDEN" on a HEALTHY tree — asserting an override that
> never happened — and I found it only because **my own test of it was vacuous**
> (a concurrent repair had settled the tree, so the override path was never
> exercised and the run returned exactly what I expected for the wrong reason).

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

> ⛔⛔ **THE TWO PERCENTAGES ABOVE ARE REFUTED (lane P1-BODYTRIAGE, merge
> `4483c213`). The vein's SIZE reproduces (1,319,540 B, 0.24% from the figure
> above); its COMPOSITION inverts.** Measured: **17.7% is unwritten, not ~71%**
> — 82.3% is **divergence in code we already hold** — and fold-ambiguous is
> **3.9%** (10.0% at the loosest definition), not ~13%. Quazal is **17.9%** of
> the vein, not its flavour (engine 51.9%, game 26.1%).
> ★ **And the 17.7% was never new**: lane GRIND-1 measured the same ~17% on
> 2026-08-14, and it sat in
> `docs/decomp/bodywrite-surface-repriced-GRIND1-2026-08-14.md` the whole time.
> That is `READ THE IN-TREE RECORD FIRST` failing in a coordinator brief for the
> second time in this document's life — the first put a two-population composite
> figure into §4a.
> **The §3 CONCLUSION is unaffected and in fact strengthened**: identification
> still cannot be a byte lever, and the vein it pointed at is now measured
> unfundable for a *different* reason — we mostly hold the code and it diverges.

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
2. ✅ **DONE (lane T2-RULER, merge `017f46b7`)** — the size-if-it-crosses
   ranker was pricing on `none` while banding on the graded ruler. Measured on
   one settled tree, inputs held still: **2,710 of 3,427 rows (79.08%)
   mispriced before, 0/3,427 after**; 4,531 charges were invisible; the PURE
   SYMBOL class went 3 rows/2,364 B → **2,184 rows/517,348 B**. Of the rows it
   advertised at `mm≤3`, **33.2% of the bytes carried a relocation-name charge
   the ranker could not see.**
   ★ **The dominant failure was INVISIBILITY, not over-advertising**: 2,181
   rows read `mm==0` under `none` — literally "no mismatches" — and the
   `0 < mm` filter silently dropped them.
   ⛔ **Root cause of an 18-day miss**: the file was **absent from
   `ruler.py`'s `_CONSUMERS` list**, so the regression guard passed cleanly
   over an identical defect. Now listed, and proved able to fail on it.
   ⛔ **3 of 10 human-ratified "known positive" pins were never pure** — they
   were ratified by a human reading the diff **on the `none` ruler**; one hides
   a real wrong-callee divergence inside a row advertised as verified-good.
   *A human-ratified control is only as good as the ruler it was ratified on.*
   Verified independently by the coordinator on a stable binary: `--selftest`
   **PASS rc=0** (0/3,414 disagree) and `--selftest --self-break` **FAIL rc=2**
   (8 controls red, 0 void, inputs held still).
   **Follow-up landed 2026-09-10 (`dd4dd0ae`)**: three further tools
   (`w25_charge_detail`, `w25_pair_dump`, `pairing_model`) computed on the
   resolved ruler and never disclosed it — `pairing_model` had the label in
   hand and discarded it into `_lbl`. All three now print the banner.
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
5. ✅ **BUILT — AND IT REFUTED ITS OWN USE CASE** (lane S1-FOLDTOOL, merge
   `0facddf1`, `tools/s1_fold_family.py`). Per-family retail-byte COMDAT proof
   with the W33/W34 rule baked in (**never mask a relocation; unresolvable ⇒
   UNDECIDED**). It was commissioned as "the enabler for §4's structural
   slice"; it measured that slice at **~99% irreducible** (§4a: floor ≈4.1 kB,
   60.67% of covered bytes provably unable to cross).
   ⇒ **Its standing role is a REFUSAL instrument — use it to refuse a fold
   claim, not to hunt bytes.** Validation: body mutation flips 5/5,
   relocation-NAME mutation flips 5/5 to `DIFF_NAME` (proving names are not
   masked), 24 recorded withdrawals → **0 wrongly proven**, empty or absent
   population → rc=3.
   ★ It was **wrong first, in the direction that manufactures work** — 222
   pairs refuted as source defects when retail's single body named two
   unrelated `T`, i.e. the callee was itself a fold survivor wearing the
   linker's arbitrary name. Fixed via the internal-inconsistency route, at a
   stated cost in refuting power (control REFUTED 11→4).
6. ⏳ **UNBLOCKED 2026-09-10, IN FLIGHT (lane N1-GPUGATES)** — **Native runtime
   measurement.** The link gate proves LINK, not RUN; real runtime oracles
   exist in `main_render.cpp`/`main_milo.cpp` (~25 gates, forced-failure
   controls) but were manual and uncollected, and `tools/native_health.sh`
   (lane S5, merge `c07cab85`) could only reach **3 of ~27** render gates
   because the GPU was down (`verdict=INCOMPLETE … unrunnable=rb3-render:nogpu
   rc=3`).
   ★ **The GPU is fixed and it was NOT the reboot I flagged**: the NVIDIA
   kernel module was reloaded to **610.57.04** on a package update, matching
   userspace, with **no reboot** (box up since 2026-08-22). Verified
   **functionally, not by version string** — `vkCreateInstance` returns
   `VK_SUCCESS` and enumerates 2 devices, where on 09-01 that exact call
   returned NULL. *A matching version string is a proxy; the call is the
   thing.*
   **CI decision made (coordinator): wire the LINK GATE ONLY**, with
   `NATIVE_GATE_ALLOW_INCOMPLETE=1`. CI has no ark and no GPU, so runtime gates
   would permanently SKIP and train everyone to ignore the job; the link gate
   catches the ODR/undefined-symbol class the X360 build is structurally blind
   to and has caught main broken **four times**. Runtime sweep stays on-demand.
7. ✅ **DONE (lane S3-ABLATE, merge `e6f16afd`)** — alias ablation re-measured:
   **811,492 B / 7.920118 pp**, 1,591 groups / 5,338 memberships.
   ★ **Memberships fell 65% while the byte exposure moved 0.85%** — the retired
   9,858 were worth 0.7 B each against a surviving average of 152 B, so **a
   membership count was never a proxy for exposure**. Price fold work PER
   GROUP: only ~433 of 1,107 sampled groups forgive anything at all.
8. ✅ **DONE, and deepening IN FLIGHT (lane S4-UNICORN, merge `83eba972`;
   lane U1-DEEPSCHED running)** — behavioural coverage refreshed to **62.2% of
   units**, and the headline was **refuted by its own instrument**: the
   matched-but-wrong worklist is **1,017 → ~6 (99.2% artifact)**, killed by a
   mock-region screen and a byte-identity proof (596 of 942 DIVERGENT-at-100
   rows have byte-identical bodies; identical code cannot emulate differently).
   ⛔ **~31% of EQUIVALENT verdicts are NOT EVIDENCE** — `comparator.py:186`
   returns EQUIVALENT when both sides hit the same error at the same PC, and
   verdict counts structurally cannot see this.
   ⇒ **The `logic` class is still 0 and that is a SCHEDULE limit, not a
   coverage limit** (zero-fill + 0xCD only; out-params `r4/r5/r6` = NULL; DC3's
   typed/hostile mocks unrun). **The outstanding work is a DEEPER schedule, not
   a broader run** — which is exactly what U1 is testing.

   > ⛔⛔ **U1 ANSWERED IT AND INVERTED THE FRAMING (merge `1b45fc5b`): the
   > fixture was BROKEN, not shallow — and one mechanism produced BOTH of S4's
   > headline pathologies.** MSVC on Xenon calls out-of-line register
   > save/restore helpers as external REL24 targets, and the harness stubbed
   > every one with a generic `li r3,0; blr`. So `bl __savegprlr_29` **destroyed
   > the incoming `this` at the SECOND instruction of the function, on both
   > sides**, while the tail `b __restgprlr_29` returned to a stale LR, making
   > functions re-enter their own tail and spin to the cap. **Every verdict S4
   > produced sits on that fixture.** Eight fixes ported and verified in THIS
   > binary — all 72 GPR/FPR helper bodies byte-identical to `band.exe` at RB3's
   > own addresses (29 tests, 0 skipped).
   > ★ **The port did not buy its verdicts by desensitising the instrument**:
   > `ClearBones` catches 3/4 single-instruction deletions before the port and
   > **still 3/4 after**. And the deeper schedule made the *broader* run cheap —
   > the harness that wedged on 51 units now audits **all 1,045 units / 22,718
   > functions in 256 s**, 2.9× S4's coverage.
   > **RESULT: genuine `logic`-class divergences = 0 of 22,718**, on a fixture
   > that no longer destroys `this`. Shared-error laundering 30.9% → **17.7%**
   > by fixing CAUSES, with the verdict deliberately NOT flipped (that would
   > convert 88 harness failures into 88 fake bug reports).
   > ★★★ **And it audited its own headline out of existence**: all 15 rows of
   > its `object_memory` cluster dissolve (6 a constant `0x2C` across six
   > unrelated classes, 4 float-literal asymmetry, 5 scalar-vs-GLOBAL), each
   > escaping the `data_layout` screen **by construction** — so **40 is an UPPER
   > BOUND, not a bug count**, and no fix was attempted because nothing reached
   > the bar.
   > ⚠ **Fleet-reach defect found on the way**: the C hook's pure-Python
   > fallback has a signedness bug that makes every call-target lookup miss, so
   > the **wrong-callee warning is silently lost entirely** — and the `.so` is
   > gitignored and `setup_worktree.sh` does not build it, so **every fresh
   > worktree takes that degraded path by default**.
   > **Highest-value next measurement:** the full-corpus `outparam` run — it is
   > the one addition that WON (+24% divergence, −10% crashes, and one
   > matched-but-wrong row the default schedule never sees) and it has only been
   > run on 51 of 1,045 units.

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

**Honest bottom line on the matching metric — REVISED 2026-09-10, DOWNWARD.**
The original estimate below was **explicitly premised on two veins that have
since BOTH been refuted by their own instruments**: the fold class (§4a, ~99%
irreducible, floor ≈4.1 kB) and the missing-body vein (§7 item 4, oracle-backed
slice = 1 unit / 52 B). With both gone, the *identified* remaining levers are
container-type divergence (~87.5 kB, and lane S2 showed its wins were **map**
repairs rather than source work), clean source levers (~56 kB), and the
permuter ceiling (~76.5 kB / 0.75 pp, at a measured historical conversion of
**0 for 66**). That is **≈220 kB ≈ 2.1 pp of `total_code` at 100% conversion**,
and no conversion rate in this project's record supports anything near 100%.
⇒ **Read the realistic figure as low single-digit tenths of a pp, not +6–12 pp**
— and note this is the third time a headline forecast here has had to be revised
down after the vein behind it was measured rather than assumed.
★ **The conclusion this document already drew is unchanged and now better
supported**: the metric is close to its practical ceiling, and the remaining
value is **accuracy and the native port**, not bytes.

*Original text, kept because the correction is the record:* from 59.9% of
reachable, the fully-addressable in-scope remainder (structural + controlled
grind + permuter ceiling, taking measured irreducibility rates at face value) is
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

## 7b. EXECUTION LOG — second wave (2026-09-10/11, coordinator session 3cdd3c)

Seven lanes dispatched off `5aa1cb7a` from §7's live items, all landed by
`--no-ff`, each measured by `ab_measure` with the prediction pre-registered.
Headline `017f46b7` → `28a540b3`: **42,305 → 42,435 fns, 3,774,924 →
3,819,096 B, 36.843063% → 37.274178% (+130 / +44,172 B / +0.431 pp)**, every
step reconciled to the byte in `progress_ledger.jsonl`. Manifest, alias gate
(0 contradicted) and native gate (18/18, 0 SKIPs) all PASS on the merged tree.

| lane | merge | outcome |
|---|---|---|
| **L7-CONTAINER2** | `0ad1e302` | **+107 fns / +21,968 B**, every win a MAP repair: 30 `~ObjRefConcrete<T>` names were a *scrambled bijection*, settled by reading each dtor's `??_7` reloc → `??_R4` → `??_R0` type string from retail. **Zero header bugs** — §4's "our headers use the generic form" hypothesis found no instance; our source was right and the map wrong. ⛔ `container_type_census`'s `NOFOLD` inherits a measured **43–75% map error rate** on the addresses it consults — read it as "not refuted by the map" |
| **L5-SYMBOLHEADS** | `28a540b3` | **+11 fns / +16,964 B** from 4 map repairs; **0 PROVEN_FOLD** in the top 19 SYMBOL heads, no alias licensed. Found `TrackWatcherImpl` slots 6/18 **transposed in both header and map** (cancelling to a clean 100% while `SetAutoplayError` called `Restart`). ⛔ `s1_fold_family.py` was failing as a clean negative (shared cache path, rc=1 not rc=3) — fixed. Rule: **caller COUNT, not row size, predicts a map row's yield** (one rename crossed five rows) |
| **L6-STRUCTHEADS** | `cdc294fb` | **8 of 17 crossed, +4,944 B / +13 matched (+4 honest)**, two behavioural bugs at `mpn` 100 both sides (UIListState scrolled the wrong direction; BandDirector post-proc nesting). CustomizePanel (5,036 B) probed once more, worse; W37's refusal stands |
| **L1-UNICORNFIX** | `c4dfac8e` | S4's 8 rows adjudicated on bytes (all survive U1's fixture refutation): 1 REAL_BUG (`~AppMiniLeaderboardDisplay` unconditional SetProvider, +284 B), 2 `$4` thunk rows were wrong MAP names not layout bugs (+12 B / −1 fn deliberate), 5 artifacts. 33 of U1's 40 score-100 rows are byte-identical with relocs masked |
| **L4-NATIVESCATTER** | `62b300be` | 31 of 47 unlinked scatter guests wired standalone, **47 → 16**; all 47 had one cause (host in no target). `scatter_audit.py` over-reports by exactly one file. Found an `#ifdef HX_NATIVE` feature that had never been compiled |
| **L2-BODYTRIAGE** | `89670f76` | Redirected when p1 landed; corroborates p1 to the byte. Adjudicated the 8 MIS-PIN SUSPECTS: 3 confirmed, 2 REFUTED (hull test cannot work on a fragmented pin), precision 37.5%. **DuplicatedObject is not an accuracy target** (12/12 blocks in the /Od band). No pin moved |
| **L3-NEXTSONG** | `032217ff` | The 12,220 B head did **not** cross: both charges are integer operand-order commutes (ARITH_COMMUTE, inert), one closed, Δ0. Handoff |

**What this wave says about §7:** item 2 (container-type) paid **+21,968 B** but
as MAP work, not header work — the "one header declaration fix pays every call
site" framing was wrong in every tested instance; the SYMBOL class outside it
paid +16,964 B through misidentification repair, again map. The realistic
figure in §1's revised forecast ("low single-digit tenths of a pp") was met in
one wave (+0.431 pp), dominated by map repair on retail-byte evidence. The
accuracy yield — one vtable transposition, three behavioural bugs, a
scatter-guest feature that had never compiled — is the part that reaches the
native port.

**Two coordinator errors, recorded:** the queued "dispatch #556" task from my
own compaction summary was already landed (`b341d7ab`, −40 B) and under §7's
"do not fund" — re-validate a paused coordinator's pending task against `git
log` and this document before dispatching. And a `git commit --amend` on a
shared main rewrote the peer coordinator's `u1-deepsched` merge (`2f08c770`
is that merge, mislabelled; record recovered at `37dc1ebe`). Never amend on
the shared tree.

**Unattributed:** main's 1,196-object rewrite at 23:31 UTC. All seven lanes
answered the audit "no" (worktree-scoped throughout); `REPO_ROOT` being unset
for plain subagents remains the exposure the peer identified.

> ⚠ **CORRECTION 2026-09-11 01:20 UTC (peer coordinator rb3-xenon-3d, from
> stamp mtimes; accepted here).** The evidence behind BOTH drift incidents in
> this document — the 642-object one in the §0 banner and the 1,196-object one
> above — **does not discriminate corruption from a build in progress.** A full
> `./tools/ninja-locked` rewrites objects progressively and rewrites
> `patch_state.json` only at its terminal verify edge, and
> `verify_objs_patched.py --verify-manifest` never takes `.ninja-build.lock`,
> so **any sample taken while a build runs reads "objects changed, manifest
> stale" and the tool prints "produced OUTSIDE the full build graph" as a
> statement of fact it cannot observe.** A third drift, 37 objects at 01:07 UTC
> on 09-11, was reproduced as exactly this: it was this coordinator's own
> full build, sampled mid-flight. The 23:31 event's signature (1,204 of 1,205
> objects sharing one mtime minute, at objcache speed) is at least as
> consistent with a build in progress as with a targeted-build rewrite. ⇒ The
> MCP main-fallback (`mcp_server.py` project_dir resolution, `REPO_ROOT`
> unset for plain subagents) is **still a real path and is being hardened by
> lane W3-F on its own merits** — but it is no longer *evidenced* by these
> incidents. **Pre-merge rule until GATE-DISC lands its BUILD-IN-PROGRESS
> verdict: a red `--verify-manifest` is read only after confirming no build
> holds `.ninja-build.lock`.** The peer also notes its own first exoneration
> probe (recent mtimes) was vacuous — the patchers preserve mtime by design.

## 7c. EXECUTION LOG — third wave (2026-09-11, coordinator session 3cdd3c)

Goal restated by the user at the start of this wave: *match the bytes AND fix
bugs in the critical paths the native port uses.* Seven lanes, each in its own
`~/tmp/wt-w3-*` worktree, each landed `--no-ff` after a full main build with
manifest, alias and (where `src/` moved) native gates green. Every merge
message carries its own pre-registered-vs-measured table; every merge has a
ledger snapshot. Base for the wave was the third session's `814e3a60`.

| lane | merge | Δ fns | Δ bytes | what it was, in one line |
|---|---|---:|---:|---|
| G native handoffs | `52a89cca` | 0 | 0 | four L4 handoffs closed; a real retail MOTD-wipe bug corrected natively; two records corrected (FlowManager's direct host, the SongUpgradeMgr shim) |
| B map vein | `d02467a6` | +6 | +976 | ObjPtr CTOR family; the dtor family L7 opened is CLEAN (0/71); bare naming is a dud (13× over-prediction); L5's 0x823a4fa8 is not a function |
| F tooling | `a4aed958` | 0 | 0 | MCP refuses the main tree (7/7 sabotage); asm-listing replay defect fixed; whole symbol names; census on coverage — L7's "3.1×" measured 0.95× |
| C re-homes | `136a632d` | −13 | +292 | +1 honest; −14 masked funclet credits surrendered (pins were manufacturing 100% rows for foreign code); two records refuted |
| A wrong-callee | `59d502a6` | +55 | +10,520 | 471 native-linked TUs enumerated; nine defects, eight MAP, one DC3-newer overload; **no runtime-affecting wrong callee in native-linked code** |
| D XAudio2 layout | `cbbd1f8f` | +15 | +1,608 | three XDK-version layout differences proven on retail; a fabricated alias withdrawn; Voice.cpp's pin short by 0x8B4 B |
| E vtable | `fbaef26e` | +3 | +224 | `vtable_inherit_sweep.py` (control fails both ways; 21,132 slots un-excluded); GemPlayer NoteOn/NoteOff MAP transposition; two DC3-only virtuals removed from headers, one a native-visible skipped dispatch |
| **wave** | `814e3a60` → `fbaef26e` | **+66** | **+13,620** | **42,439 / 3,821,092 B / 37.293660% → 42,505 / 3,834,712 B / 37.426590% (+0.132930 pp)** |

Every composition reconciled exactly on the merged build, including A over C
and D over A where both edited `splits.txt` (one trailing-append conflict in
the map, resolved by keeping both sides).

**What reached the native port.** MainHubPanel's MOTD wipe (retail pointer
compare, always true; native corrected with a compiled opt-out); InlineHelp /
LabelShrinkWrapper skipping `UIComponent::SetTypeDef` (DC3-only overrides, now
removed); the one-arg `LocalizeSeparatedInt` in DataFunc; `ObjectDir::SetName`
forwarder removed from 29 vtables. Lane A's headline finding is negative and
load-bearing: on the charged-relocation stratum in the 471 native-linked TUs,
every divergence was the MAP wrong and our source right, except one DC3-newer
overload.

**Instruments this wave leaves behind.** `tools/native_linked_tus.py` (the
native TU set, compiler-truth), `tools/vtable_inherit_sweep.py` (inheritance-
aware fold exclusion), the MCP `resolve_project_dir()` refusal, cache v4 and
whole names in `crossing_worklist.py`, the coverage criterion in
`container_type_census.py`.

**Coordinator errors this wave.** (1) Briefs went stale WITHIN THE HOUR: a
third session (commits as `freeqaz`, indistinguishable from the peer by
authorship) landed two of my briefed items between planning and dispatch, and
I addressed the peer coordinator as their owner. Rule adopted: re-read
`git log` immediately before each dispatch and have lanes assert their base.
(2) I read a red `--verify-manifest` as corruption twice; the peer showed the
verifier cannot distinguish a build in progress, and my ledger commit
`4f0ec650` stated the wrong mechanism (git note + §7b correction). GATE-DISC's
three-verdict verifier (`9468ed0e`) retires the ambiguity.

**Surfaced for the user, not taken.** Lane D recommends deleting
`src/system/synth/Sound.cpp`, `ThreeDSound.cpp`, `ThreeDSound.h` (zero
includers, zero compile edges, zero retail presence, two confirmed text bugs)
and keeping `Sound.h` under a DC3-only banner; the peer's earlier claim that
nothing includes them is wrong for the header. Deletion is the user's call.

**Wave-4 candidates, recorded in memory with their evidence:** `eh_boundary`
WHY-not-THAT (peer handoff); `atexit_fuzzy_verify.py` retirement; Object::
AddRef/Release naming (0x8275bd08 / 0x8275b378) after a caller census; the
67-row `??_G` cross-class dtor census; the `$4PPPPPPPM@A@` adjustor off-by-one
family; InlineHelp Copy/PreLoad re-homes; the RetryAudioPanel lower half;
CharWeightable off `OBJ_MEM_OVERLOAD`; `FxSendReverb360::SyncEffectParams`
pinned as `sslgen.c`; the 0x82749630 certain −100 B; the two ranker importers'
Symbol-arg blindness (fix together).
