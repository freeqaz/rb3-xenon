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

## 7d. EXECUTION LOG — fourth wave (2026-09-11, coordinator session 3cdd3c)

Seven lanes off `3ab3f494`, each in its own `~/tmp/wt-w4-*` worktree, each
landed `--no-ff` after a full main build with the manifest, alias and (where
`src/` moved) native gates re-run BY THE COORDINATOR on merged main rather than
relayed from the lane. Every merge carries its own pre-registered-vs-measured
table; every merge has a ledger snapshot.

| lane | merge | Δ fns | Δ bytes | what it was, in one line |
|---|---|---:|---:|---|
| E eh_boundary WHY | `10bf877d` | 0 | 0 | the patcher is SUPERSEDED by objdiff `b76f376`, not broken; retire only behind `tools/check_objdiff_eh_prefix.py`, whose red leg was run |
| G allocator | `e5b814a5` | +74 | +5,624 | the briefed lever was wrong: the operator delete is `noinline` and retail INLINES it; a per-class variant macro, never a flip |
| C re-homes | `ae204835` | −2 | +292 | `0x823a4fa8` IS a function — a stackless leaf; naming it took a row 0 → 98.5%; three wave-3 figures refuted |
| F tooling | `19f06c76` | 0 | 0 | the wrong-callee class OWNED THE TOP of both rankers as artifact; 3 of 5 controls were RED ON ARRIVAL; atexit verifier retired, WITHHELD kept |
| B map | `9d81f415` | +11 | +388 | the `??_G` census is a third its headline (42/67 legitimate); the best-evidenced rename REFUSED on pairability |
| A native logic | `a93d7a93` | +9 | +1,928 | five real native-path bugs, incl. a sub-loader capturing its PARENT's root, and three DTA builtins absent natively |
| — guard fix | `b931e58e` | 0 | 0 | a TIMEOUT in `symbols_fixpoint_guard` was exiting 1 = its DRIFT verdict; lock contention read as drift |
| D Voice bodies | `3e6b698a` | +21 | +2,672 | fourteen bodies from retail; the ctor SIGNATURE was wrong; dc3's polarity was right where W3-D inverted it |
| **wave** | `3ab3f494` → `3e6b698a` | **+113** | **+12,808** | **42,514 / 3,834,792 B / 37.427372% → 42,627 / 3,847,588 B / 37.552258%** |

⚠ The wave total is measured main-to-main and therefore INCLUDES three other
sessions' merges that landed inside the window (memtemp-ab, memtemp-names, and
the build-owed/cleanup verifier work). Per-lane deltas are each lane's own
measurement against its own base; they do not sum to the wave figure and are
not meant to. Lane D's case is the explicit one: it measured +4,548 B, and its
`0x82345030` map row landed independently from another session mid-flight, so
only +2,672 B of it was still on the table at merge time. **The ledger
absolutes are the authority; per-lane deltas are provenance.**

**What reached the native port.** A sub-loader constructed inside a parent's
path tracker captured the PARENT's root instead of the current one, because the
newer sibling engine moved that derivation into a constructor. Three
data-language builtins that retail registers were absent from our native build
(registry diffed name-by-name out of `.rdata`: 154 vs our 151). A song-info
copy set one field where retail sets five, so a defaulted copy carried garbage
thresholds and volumes. An integer parser accepted a string kind retail
rejects. `synth_xbox/` is excluded from the native build, so lane D's fourteen
bodies pay in bytes and accuracy only — stated, not glossed.

**Four instruments this wave leaves behind.**
`tools/check_objdiff_eh_prefix.py` (asserts a redundancy that depends on a
hand-swapped binary; rc=2 on a rolled-back objdiff, rc=5 vacuous),
`tools/test_fixpoint_guard_timeout.py` (four tests, two of them NEGATIVE arms
so the positives mean something), the coverage criterion and whole names in the
ranker chain, and the `5/N` charge screen below.

**★ The reusable result of the wave is a screen, not a byte figure: ONE
relocation-name charge costs exactly `5/N` pp of fuzzy, N = size/4.** So a row
at 99.773 on 88 B has one charge and 99.545 has two — the charge count is
readable off the percentage with no diff at all. It scored 35/35 selecting a
population after a mispredicted row exposed the need for it.

**★★ And the wave's load-bearing negative: FOUR CONSECUTIVE MAP LANES FOUND
ZERO SOURCE BUGS.** L7, W3-B, W4-B and W4-G each adjudicated their rows to MAP
or FOLD, never SOURCE_WRONG. Combined with W3-A's finding that the
charged-relocation stratum in native-linked code is all map error, the
conclusion is that **the identification channel, not the source channel, is
where the remaining metric lives — while the BUG channel is in bodies**, which
is exactly where lane A found its five.

**Coordinator errors this wave.** (1) I attributed two merges to the peer
coordinator by author name; every session commits as `freeqaz` and a third
session had landed them. The durable record was written attribution-free
("another session merged…") and only the peer messages were wrong; the rule now
recorded is NAME THE MERGE SHA, NEVER THE SESSION. (2) I read a red
`--verify-manifest` as corruption; it cannot distinguish a build in progress,
and the tree I sampled was mid-build. (3) I recorded a ledger snapshot off a
tree whose report was stale because another session had merged without
building — the ledger tool REFUSED my contradictory re-record and pointed me at
`--replace`, which is the tool working. (4) I ran `symbols_fixpoint_guard` in
the shared main tree and it refused with rc=2, correctly, because it forces a
split; the guard is worktree-only by design.

**Process failure worth a rule.** Two lanes (G, and G's pattern again in D's
watchers) burned large budget on background waits whose `pgrep -f ninja`
matched OTHER lanes' concurrent builds, so they never fired. Lane G returned
twice with no report at all. **A subagent's background wait cannot wake it
after its turn ends; wait on a PID you launched, in the FOREGROUND
(`tail --pid=<pid> -f /dev/null`).** Lane D was then killed outright by a rate
limit mid-turn, having already committed its work and its record but never
reaching its gate — so the coordinator verified its tree independently instead
of trusting the branch.

**Still open for the user, carried from wave 3 and NOT taken:** deleting
`src/system/synth/Sound.cpp`, `ThreeDSound.cpp`, `ThreeDSound.h` (zero
includers, zero compile edges, zero retail presence, two confirmed text bugs),
keeping `Sound.h` under a DC3-only banner since it has seven includers.

**Wave-5 candidates, each with its evidence in a lane record:** the twelve XAPO
`m_regProps` dynamic initialisers in the `.text$yc` tail block (identified by
lane D, needs splits + `__uuidof` source); `OnFileGetBase`/`OnFileGetPath` as
one coherent source+map+alias wave (lane G's H1); the ~161 unadjudicable
allocator classes; `FileGetBase` at fuzzy 29.917; the three new top ranker
families each one bit from a verdict (lane F); the `?Handle@` dispatch-ORDER
class that no fold explains (PlatformMgr, OvershellPanel, OvershellSlot); jeff's
158 symbol-extent truncations (lane C sized it at +32 B for the first); the pin
channel lane B says is undrained where its map channel is not; and tiers 4/5 of
lane A's stratum, 71% of its bytes, untouched.

## 7e. EXECUTION LOG — fifth wave (2026-09-11, coordinator session 3cdd3c)

Four lanes off `a8fdfd65`, weighted at BODIES on wave 4's evidence that the
identification channel is where the metric lives while the bug channel is in
bodies. Every merge `--no-ff` after a full main build, with the manifest, alias
and native gates **re-run by the coordinator on merged main**, never relayed.

| lane | merge | Δ fns | Δ bytes | what it was, in one line |
|---|---|---:|---:|---|
| B XAPO regProps | `8825b103` | +13 | +1,872 | there are THIRTEEN, not twelve; four briefed figures wrong; three real source bugs, incl. a method whose wrong signature paired with NOTHING |
| D pin channel | `48301ac2` | +9 | +540 | a fabricated alias withdrawn — its own T1 evidence convicts the MAP; wave 4's refutation here was CIRCULAR; jeff's carve rule characterised in three measurements |
| A native bodies | `4b81a2aa` | +3 | +528 | **PARTIAL** — lane killed by an API safeguards error; only its one measured commit landed, WIP preserved unlanded |
| C FileGetBase | `d428c212` | +6 | +660 | a map row reading a FALSE 100%; a tool that had crashed on every run for three weeks; a proven alias deliberately NOT installed |
| **wave** | `a8fdfd65` → `d428c212` | **+31** | **+3,600** | **42,627 / 3,847,588 B / 37.552258% → 42,658 / 3,851,188 B / 37.587395%** |

Every lane composed to the byte on the merged build. ⚠ As in §7d, the
main-to-main figure also spans another session's merges inside the window
(`testci`); per-lane deltas are provenance, not addends.

**★ The wave's theme is instruments convicting themselves.** Lane B found four
of its five briefed figures wrong, including the count in its own title. Lane D
found that wave 4's refutation rested on a circular test — "the callee's pin
matches its name", when the pin had been placed to match the name — and
withdrew an alias whose tier-1 evidence, read properly, convicts the map.
Lane C found a map row reading a **false 100%** (the only differing thing was
an unnamed callee, which objdiff forgives) and a tool that had **crashed on
every run since 2026-08-19**, which is what a previous lane's handoff had
pointed it at. ⇒ **A handoff names a target, not a working instrument. Check
the instrument starts before trusting its silence.**

**★★ Two prediction misses that were worth more than hits.** Lane C's step 0
went the WRONG WAY: deleting a discarded call made the function a leaf
tail-call, so MSVC emitted 12 B against a 192 B target — proving the prior
29.9% was mostly prologue coincidence and that **MSVC will not inline a
40-instruction helper, so retail's source never called one**. Its step 2 missed
because MSVC inlined our tiny wrappers into their callers; the next step was
written off that miss and hit exactly. Lane D's +40 B overshoot was three
`??_E` thunks crossing with the `??_G` bodies — wave 4's collateral family with
the OPPOSITE SIGN.

**★★★ PAIRING IS NOT MATCHING** (lane D): two rows moved off a **false 0.000**
to a genuine 69.68 / 77.34 while billing zero bytes. A 0.000 there never meant
"unmatched", it meant **unscoreable** — and the 252 B is now a visible
body-port target instead of an invisible one.

**Native-path bugs fixed.** Reference counting on X360 object-array properties
did nothing where the native branch uses a container (the match build cannot
surface that alone); a type-properties accessor took its owner's type
definition by argument in retail where our inherited version read it through
the owner; thirteen audio-effect registrations would have registered with a
null identifier, no flags and zero buffer counts, four of them uninitialised
and nine never defined at all.

**Two lanes died to infrastructure, not to their work.** W5-A was terminated by
an API safeguards rejection mid-turn and W4-D by a rate limit. Both had
committed real work first. The rule that saved both: **commit measured work
immediately; a lane that dies between measuring and committing loses the
measurement.** W5-A's ten uncommitted files are preserved at
`~/tmp/w5a-handoff/w5a-uncommitted.patch`, unlanded because unmeasured, and a
future lane should RE-DERIVE rather than apply them blind.

**Wave-6 candidates, each with evidence in a lane record:** `?DataInitFuncs@@`
(8,068 B at fuzzy 71.45, badly misaligned — it gates lane C's proven alias);
`FileGetPath` `0x82516550`, a CARVING fix where our body already matches; the
29 remaining `.text$yc` dynamic initialisers and the unpinned 13.4 kB of
`.data` behind lane B's thirteen; the two remaining `??_G` islands ⚠ **with
alias group 432, which unlike 1013 DOES forgive a site**; the AddRef/Release
252 B body-port lane D made visible; retail's `Object.cpp` TU reunification
(`0x8275A384`–`0x8275D03C`, currently split across DirLoader's pins); jeff's
159-site carve seam (characterised, **pin workaround measured impossible at
2/156**); and tiers 4/5 of the body stratum, still 71% of its bytes.

⚠ **And a tool interaction the next pin lane must know:**
`tools/symbols_fixpoint_guard.py` leaves target objs **un-renamed** (70 mangled
symbols → 0) because it re-splits without the pre-compile renamer. That is the
FOLDPROVE-2 state where every name lookup reads "absent" and any negative is
vacuous. `verify_objs_patched` does **not** catch it — it covers the
post-compile passes, not the pre-compile renamer. Run the guard LAST, or
rebuild after it.

## 7f. EXECUTION LOG — sixth wave (2026-09-11, coordinator session 3cdd3c)

Four lanes off `50fd112d`. Every merge `--no-ff` after a full main build, with
the manifest, alias and native gates **re-run by the coordinator on merged
main**, never relayed.

| lane | merge | Δ fns | Δ bytes | what it was, in one line |
|---|---|---:|---:|---|
| D XAPO data | `eed04bd5` | −1 | −84 | **`.data` IS NOT IN THE DENOMINATOR**; a PHANTOM UNIT was financing two false 100% rows; 11 of 29 initialisers are Quazal. `masked_equal` −5 ⇒ **honest +4** |
| C tier 4/5 | `2d3c46f2` | 0 | 0 | two real native RENDERING bugs; **80.6% of the tier carries a register-only charge ⇒ the body surface is 31 kB, not 265 kB** |
| A DataInitFuncs | `da05c172` | +3 | +8,040 | the largest single row crosses 71.4467 → 100.0; cause was **MSVC's INLINE BUDGET** (69 of 154 sites), not the list |
| B Object.cpp TU | `68ebb835` | +16 | +768 | TU reunified on **EH-funclet** evidence; three instruments vacuous **toward caution** |
| **wave** | `50fd112d` → `68ebb835` | **+18** | **+8,724** | **42,658 / 3,851,188 B / 37.587395% → 42,676 / 3,859,912 B / 37.672540%** |

Every lane composed to the byte. The main-to-main figure also spans another
session's `guardorder` merge; per-lane deltas are provenance, not addends.

**★★ THE WAVE'S RESULT IS A CORRECTION TO THIS DOCUMENT'S OWN FORECAST.** §7e
sent lane C after "tiers 4/5, 71% of the stratum's bytes". Lane C regenerated
the population, corrected the figure to **76.5%**, then classified all **25,763
charged sites individually** and found:

| class | rows | bytes |
|---|---:|---:|
| SOURCE-CLEAN | 67 | 31,224 |
| ALIAS-BLOCKED (ICF folds) | 31 | 24,016 |
| MIXED (≥1 register-only charge) | 369 | **228,928** |

⇒ **80.6% of tier-4/5 bytes are permuter-deferred by standing directive, so the
body-lane surface there is ELEVEN PERCENT of the tier.** A "265 kB of bodies"
line item in a roadmap is a **31 kB** line item once priced. Lane B's item 2 is
the same shape one level down: a *perfect* source port of `AddRef`/`Release`
lands both rows one charged relocation-name site below 100 and bills **0 B**,
because the map names those callees by ICF-survivor spellings — **uncollectable
by source work, and correctly deferred rather than attempted.**

**★★★ VACUITY NOW HAS BOTH DIRECTIONS ON THE RECORD.** Lane B's first three
funclet-pairability instruments each reported "0 partners" — *including in
DirLoader, where the rows demonstrably pair today* — because
`tools/coff_bodies_ext.py` skips `__unwind$*` (13 candidates vs 103 in the COFF
string table). **Had the lane trusted that negative it would have refused a
+648 B move.** A vacuity agreeing with CAUTION is as expensive as one agreeing
with HOPE and far easier to accept. The control that caught it was required to
**reproduce the current state** (TP=85, FN=0) — the same construction that
caught W4-G's census control and W5-B's four wrong figures.

**Native-path bugs fixed.** The rim light was uploaded to shader register **61**
where retail uses **63**, so the rim term sampled whatever c63 held while c61
was clobbered. `NgStats`' 14th field is `mSpotlights` (0x34), not DC3's
`mMultiMeshBatches` (0x2c) — the native overlay showed a counter nothing
increments, and every later field was shifted +4 for all consumers; it also
corrects a factually wrong comment that asserted the opposite.

**Two more instrument findings worth the next wave's time.** `.text$yc` rank
mirrors `.text` rank monotonically over 20 anchors — it turns "which TU owns
this initialiser" into a bounded interval — **with a paired negative that is
why it is trustworthy**: our `.bss` permutation differs and would have
mis-assigned 3 of 5. And withdrawing an alias **costs only if the charge it
forgave survives the same commit** (group 432 withdrew at exactly 0 because the
same commit renamed the site to what our source already spelled).

**Wave-7 candidates, each with evidence in a lane record:** `FileMakePath`
`0x82516B10` (792 B at fuzzy 5.64, the largest priced prize left in
`default/File`); the `_S_sort` alias group (11 rows / 4,712 B on one survivor —
the best-concentrated alias target found); `AddRef`/`Release` (needs the
`mRefs` retype across 15 files **and** a T1 adjudication into groups 1481/11);
`0x823c8908`, a map defect needing a **re-home** not a rename (`CharTransDraw.cpp`
has no `splits.txt` entry, so renaming alone pins it at 0% forever); our
`?Load@FxSendBitCrush@@` **is** retail's `FxSendDistortion::Load`, worth +144 B;
DirLoader's 9 remaining blocks (68 of 203 rows still read 0 — is its home now
complete?); `RndEnviron` layout (1,056 B / 204 sites) and `Spotlight`'s +0x10
shift (904 B); auditing the rest of `ShaderMgr.h` for DC3 renumbering, since one
register was already wrong.

⚠ **Two jeff defect classes are now characterised and BOTH have their pin
workaround measured dead.** W5-D's: truncation at an 8-byte re-seed, 159 sites,
strands one instruction (pin workaround 2/156). W6-A's `FileGetPath`: a
*distinct* class — over-carve, contiguous, **zero** stranded bytes, and cutting
the block at exactly the function bounds gives a byte-identical carve.
`.pdata` cannot arbitrate either: those spans are leaf-stratum with no record.
Both are jeff-side, and jeff's release binary is the **live fleet splitter**
shared with two sibling repos — characterise, never deploy.

## 7g. EXECUTION LOG — seventh wave (2026-09-13, coordinator session 3cdd3c)

Four lanes off `3a6bfe40`. Every merge `--no-ff` after a full main build, with
the manifest, alias and native gates **re-run by the coordinator on merged
main**, never relayed.

| lane | merge | Δ fns | Δ bytes | what it was, in one line |
|---|---|---:|---:|---|
| C shader/layout | `89a3ad8b` | +1 | +316 | a spotlight beam OCCLUDED ITS OWN LENS FLARE; the "+0x10 struct shift" was the FRAME POINTER; RimColor was the only renumbering |
| B inline sweep | `11a38cee` | +36 | +864 | **34 of 36 are `masked_equal` ⇒ honest +2**; detector ships with a MEASURED 50% FP rate; its best result is the leg it REVERTED |
| A FileMakePath | `ed58a3fa` | +9 | +368 | the inline-budget hypothesis REFUTED here (181 of 200 rows `delete`); five retail-proven defects; **`run_objdiff` ROUNDS 99.97758 → "100.0"** |
| D alias vein | `d6dd9320` | +16 | +3,756 | 12 proven folds, both figures exact; **all three briefed targets REFUSED**; a predictive rule for the vein |
| **wave** | `3a6bfe40` → `d6dd9320` | **+62** | **+5,304** | **42,676 / 3,859,912 B / 37.672540% → 42,738 / 3,865,216 B / 37.724308%** |

**★★★★★ THE WAVE'S RESULT IS A PREDICTIVE RULE, AND IT IS THE FIRST ONE THIS
CAMPAIGN HAS PRODUCED FOR THE ALIAS MECHANISM: A FAMILY FOLDS IFF ITS
RELOCATION TARGETS ARE TYPE-INDEPENDENT.** Everything installable relocates
only to symbols that do not vary with `T` (`__savegprlr_29`, `Object::AddRef`,
`MemAlloc`/`MemFree`, `MemOrPoolAllocSTL`). Everything refused carries a per-`T`
callee — and retail keeps **48, 4 and 11** of those at *distinct* addresses.
That is CD-7 re-derived as something you apply **before** building: read the
relocation list, predict aliasability. Corollary that closed a briefed item
outright: **a layout difference forecloses a fold before any byte is read** —
two `SetType` bodies differing only in vbase displacement by exactly 8 cannot
have folded, whatever their bytes say.

**Three briefed alias targets refused, one of them expensive to have believed.**
`AddRef`/`Release` was briefed (by §7f, from lane W6-B) as needing "the `mRefs`
retype across 15 files AND a T1 adjudication". The adjudication says the
retype **bills exactly zero**: five `list<T*>::insert` sit at five distinct
addresses differing in one `bl` whose targets are 64 B vs 72 B, and the `erase`
side is 84 B vs 96 B, which cannot fold at all. ⇒ **Do not fund that retype for
metric reasons.** `_S_sort` was also 8 rows / 3,424 B on this tree, not the
briefed 11 / 4,712 — main had moved under the brief.

**⛔⛔ AN INTEGRITY HAZARD THAT OUTRANKS EVERY BYTE IN THIS WAVE:**
`scripts/icf_alias_build.py` **has zero references to `withdrawn`**. The
withdrawal ledger is not a generator input and `--merge` is additive only, so
**all 563 withdrawal records — including the three fabricated aliases this
campaign has withdrawn in three consecutive waves — are only as durable as
"nobody re-runs the generator."** Only a *map* fix blocks regeneration, and
only where the differing words are unrelocated. **This is wave-8's first item.**

**Two more instrument defects, both of which had already produced a wrong
belief before they were caught.** `run_objdiff` prints `100.0% canonical` for
**99.97758** — it rounds — and a lane recorded a false crossing off that
display, catching it only by re-deriving from archived A/B legs. And the `5/N`
screen **over-predicts ~5× for a differing IMMEDIATE** (predicted +0.0221 pp,
measured +0.004424 pp): it is calibrated for a relocation-name charge, which is
a *full* mismatch. Both are now in the standing notes.

**★★ And the wave's best single result is a REVERT.** Lane B's joint
`BandCharacter`+`BandDirector` run read **+14 matched while bytes went −128**:
one file made its own row worse *and* broke an unrelated perfect row
(`?Add@ObjKeys@@` 100.0 → 69.625). Per-unit attribution caught it; **the
whole-binary headline actively concealed it.** Had the lane stopped at +14 it
would have landed a byte regression while reporting a gain. Same lesson as
lane B's own honest headline: **+36 matched with 34 `masked_equal` is +2.**

**Native-path bugs fixed.** A spotlight's volumetric beam was suppressed in the
wrong pass, so natively it occluded its own lens flare on every beamed
spotlight. `RndEnviron` is a **21-revision schema gap** (retail `li r11, 0x25`
vs our `0x10`; 49 streamed offsets vs ~20), so every RB3 `.milo` environment
desynchronises after the first absent field — diagnosed, not yet ported.

**Wave-8 candidates:** the `icf_alias_build.py` durability fix (H1, above,
first); the tier-B ObjPtr rows at ≥99.7% (`Load@OutfitConfig` 99.88,
`SetupScore@BandScoreboard` 99.89, `RandomGroupSeqInst` ctor 99.91,
`StreakMeter` dtor 99.75) — one or two charges from crossing; `FileRelativePath`
body-port **then** name (904 B; naming costed at 35 sites / 30 callers / 22
files, so body first); `vector<String>::~vector` into existing group
`0x822d8cc0` (892 B + 1 fn, T1 sibling already present); the `RndEnviron` port;
`?transform@MD5@Quazal@@`, the **inverse** inline disease (retail calls a helper
4× where we call once); and `?Load@RndMesh@@` (3,452 B at 88.19) ⚠ in
`rndobj/`, the perturbation-prone directory that lane B just priced at 128 B.

⚠ **Two operational rules earned this wave, both about `ab_measure`:** it
**deletes untracked files created while it runs**, and **editing the worktree
mid-run gets silently wiped by its leg-A checkout**. Do not touch the tree
while a measurement is in flight. Separately, the harness caps any shell call
at **600 s regardless of the timeout requested**, so `tail --pid` on a long
build is always reaped and `pgrep -f ninja` matches *other lanes'* builds —
launch detached and poll an `EXIT=` sentinel.

## 7h. EXECUTION LOG — eighth wave (2026-09-13, coordinator session 3cdd3c)

Four lanes off `2fc2552a`. Gates re-run by the coordinator on merged main after
every landing, never relayed.

| lane | merge | Δ fns | Δ bytes | what it was, in one line |
|---|---|---:|---:|---|
| B near-crossings | `1b6c1a6c` | 0 | 0 | the band below 100 is a **NAMING frontier**: 163,812 B symbol-only vs **~588 B** of live source work |
| D FileRelativePath | `933636ce` | 0 | 0 | ported **BYTE-EXACT** and named; **both** briefed alias installs refused |
| A alias durability | `a93d8a45` | 0 | 0 | withdrawals are now a generator **input**; exposure **110 memberships**, measured |
| C RndEnviron | `0e3cc8b5` | +1 | +500 | **the briefed 21-revision gap DOES NOT EXIST** — a map defect, and the prior witness could not discriminate |
| **wave** | `2fc2552a` → `0e3cc8b5` | **+1** | **+500** | **42,738 / 3,865,216 B / 37.724308% → 42,739 / 3,865,716 B / 37.729187%** |

**★★★ READ THAT +1 CORRECTLY: this was the most valuable wave of the campaign
and it moved the headline by one function.** Three of the four lanes were
byte-neutral *by design or by refusal*, and what they produced instead is the
first honest map of what remains.

**★★★★★ THE BAND BELOW 100 IS A NAMING FRONTIER, NOT A PORTING ONE**
(`tools/near_crossing_census.py`). 5,499 paired rows at `99.5 ≤ fuzzy < 100` =
**734,308 B = 7.167% of `total_code`**. Of the 200 largest: **138 rows /
163,812 B symbol-only (relocation-NAME / fold)**, 31 mixed, 29 register-only,
and **2 rows / 5,624 B insert/delete-only** — one of which is documented
drained by four prior lanes. **The live source-porting surface at the top of
that band is ≈588 B.** ⇒ Do not fund a body-porting lane here. Corollary
already proven twice this wave: a *perfect* byte-exact port (lane D's 904 B)
pays **zero** when its residual is two names whose folds cannot be proven.

**★★★★ THE `5/N` SCREEN, FINAL FORM** (supersedes §7g's version): exact at
**1.000 pp per charged ARGUMENT**, max deviation 0.002 over 33 rows; the unit
is the **argument**, not the instruction; it is **equally exact for `register`
args**, so it prices COUNT and says nothing about KIND; **a full mismatch is
`100/N`, 20× that**; and it is scoped to all-`diff_arg` rows. **And `mpn`
excludes `register` arg diffs but CHARGES `symbol` ones ⇒ `mpn == fuzzy` is the
SIGNATURE OF THE FOLD STRATUM**, not evidence of an instruction-level defect —
which is precisely why §7g briefed four symbol-dominated rows as porting work.

**★★★★★ A NEW FAILURE CLASS: THE NON-DISCRIMINATING WITNESS.** §7g recorded
that lane W7-C "audited identity FIRST and confirmed it" before diagnosing
`RndEnviron`. Its witness was *"calls `RndColorXfm::Save` + `Hmx::Object::Save`"*
— and **`RndPostProc` also owns a colour xfm, so both candidates satisfy it.**
The witness was not mis-read; it could not tell them apart. `0x824302B0` is
`RndPostProc::Save`, the real `RndEnviron::Save` sat unnamed already streaming
our exact 22 fields, and **the 21-revision gap never existed.** ⛔ The defect
was **self-reinforcing**: splits had a hole punched at exactly that range, so
the wrong NAME justified the wrong PIN and the PIN made the NAME score.
★ The cheap general screen is **spatial** — `/O1` without LTCG preserves TU
grouping, so a row outside its class's `.text` cluster but inside another
class's unbroken run is a candidate map defect. It found this in one query, and
**screening the map for more of them is now a funded wave-9 item.**

**Two inherited claims corrected, both mine to relay.** §7g's "every RB3 `.milo`
environment desynchronises after the first absent field" is **false** — our
`Load` implements all 16 revision gates and matches on all 22 offsets, so the
"check every consumer" set was empty. The real defect was **write-side only**
(we emitted revision `0x10`, DC3's newer value, where retail writes `0xF`):
**fidelity, not corruption.** And §7g's `AddRef`/`Release` item, already
downgraded once, is now closed: lane D's adjudication shows the alias that
would pay it is **unprovable**, not merely unproven.

**⛔ THE COORDINATOR'S OWN VACUOUS PROBE, recorded because it is the sixth
instance this session and the first committed by me.** Before dispatching lane
A I "confirmed" the hazard with
`grep -c withdrawn scripts/icf_alias_build.py || echo "0 refs (confirms H1)"`.
**That path does not exist** — the generator is `tools/icf_alias_build.py`.
grep printed nothing, exited 2, my `||` branch fired, and I recorded the
reassuring message as confirmation. The conclusion was right for a reason the
check never established. ★ **A probe whose failure mode prints the answer you
expect is not a probe.**

**Alias integrity, now durable.** Withdrawals are a generator input; exposure
was **110 memberships** (72 laundered by `--merge`, 38 newly re-fabricated),
not the "563" I briefed — withdrawals are per-**membership**, and the 564
groups hold **10,058 records**. The file's own `_comment` warning about 9,395
growing back is **~670× overstated**; only 14 do. Sabotage 4/4 RED, re-run by
the coordinator on main. ⚠ **H2 is an unpriced −74 event**: 74 memberships are
simultaneously live and withdrawn, 10 of them breaking the generator's
one-survivor invariant, and a future regeneration will silently drop them.
**Price it before the next regeneration, not during one.**

**Wave-9 candidates:** the spatial map-defect screen (above, first — it is the
cheapest instrument this campaign has found); H2's −74 pricing; the PlatformMgr
four-name cycle (3,112 B, two of four proven — a partial rename would put one
spelling at two addresses); the ~38 kB fold stratum, blocked on regenerating
`wrong_callee_triage.py`'s worklist against a `none`-ruler report; whether
`0x823d14c0` is mis-named, which is map-integrity not alias work and unlocks
904 B (⚠ a map rename is the wrong instrument: it fixes 2 charges and creates
287); `RndPostProc::Save` at 96.36 with `?Load@RndPostProc@@` also absent from
the map; `NewFile` (port then name — naming is non-negative but valueless
today); and `?transform@MD5@Quazal@@`, the inverse inline disease.

## 7i. EXECUTION LOG — ninth wave (2026-09-13, coordinator session 3cdd3c)

Four lanes off `1d560c2b`. Gates re-run by the coordinator on merged main after
every landing.

| lane | merge | Δ fns | Δ bytes | what it was, in one line |
|---|---|---:|---:|---|
| B map integrity | `e05f124f` | 0 | 0 | both questions were ONE: a forgiven anonymous relocation makes **every permutation of a name family score 100** |
| D alias hygiene | `c6711597` | −6 | −808 | the −74 was **−46,520 B blind** and −808 B adjudicated; **69 of 74 withdrawals were wrong** |
| C PostProc bodies | `457155b2` | 0 | 0 | three Δ0 ports; the **failed** fourth is the result: deleting code retail lacks cost −4 fns |
| A spatial screen | `88c944d4` | +16 | +6,400 | the **map-relative** screen finds what the pin-relative one cannot **by construction** |
| **wave** | `1d560c2b` → `88c944d4` | **+10** | **+5,592** | **42,739 / 3,865,716 B / 37.729187% → 42,749 / 3,871,308 B / 37.783768%** |

**★★★★★ THE ONE INSTRUMENT WORTH TAKING FORWARD.** `map_lint --check
class_mixing` is **pin-relative**, and a self-consistent defect — where the pin
was moved to agree with the wrong name — makes the owner match the unit family,
so it is **invisible there by construction**. `tools/spatial_map_screen.py` is
**map-relative** and never reads `splits.txt`: a row fires when it sits outside
its owner's home cluster and inside another owner's unbroken run. One fire was
worth **+16 fns / +6,400 B**. Its controls run on **every scan**, not only under
`--selftest` (positive: revert the known defect in memory and *require* it to
fire; negative; vacuity floors), failure is exit 3 with **no findings emitted**,
and `--break-control` proves each one fails. **Measured FP 66.7% adjudicated,
81.0% conservative bound** — stratum fixed *before* adjudication, folds counted
as FP, no threshold moved after. ⇒ **The FP rate is high and the SELECTIVITY is
the point: 21 candidates out of 17,482 rows.**

**★★★★ THE SCORING MECHANISM THAT MAKES MAP DEFECTS SELF-FUNDING.** When
byte-identical bodies differ in exactly one relocation whose retail target is an
**anonymous placeholder**, `name_check` forgives that field — so **every
permutation of the map's names scores `fuzzy == 100`. The score does not merely
stay silent; it PAYS FOR THE WRONG ANSWER.** Four `PlatformMgr` wrappers were
all wrong at 100.0; three `list<T*>::insert` rows are provably wrong at 100.0
(300 B of false credit) while the one **genuine** pointer insert scores **0.0**.
⇒ **A 100 on a member of a byte-identical family is not evidence of its name.**

**★★★★★ PRICING SCREEN — COMPLETE, ALL THREE RATES MEASURED** (supersedes §7g
and §7h): per charged **argument**, `N = size/4` —

| charge kind | cost |
|---|---|
| relocation-NAME arg | `5/N` |
| REGISTER arg | `5/N` |
| IMMEDIATE arg | **`1/N`** |
| full mismatch (insert/delete) | `100/N` |

The immediate rate was pinned exactly: 32 charges predicted 99.878788, measured
**99.878784**. ⚠ `mpn` excludes register arg diffs but **charges symbol ones**,
so `mpn == fuzzy` is the **fold-stratum signature**, not an instruction defect.

**★★★★★ "RETAIL'S F DOES NOT CONTAIN X" DOES NOT LICENSE "DELETE X FROM THE
TU."** Deleting a branch retail's `NewFile` provably lacks brought our body to
416 B/27 relocs against retail's 424/27 with the relocation multiset matching
1:1 — and cost **−4 matched functions / −88 B**, because that branch was the
TU's only `NullFile` instantiation and retail's `File.obj` defines all four lost
COMDATs. **Coupled through COMDAT emission, not through F's own code.**
Companion: **naming an address is a claim about OUR symbol table too** —
`?Load@RndPostProc@@` is identified four ways and still **not nameable**,
because retail takes `(BinStream&, ushort)` where ours takes `(BinStreamRev&)`.

**★★★★ PRICE THE BLIND COST BEFORE THE EVENT, NOT DURING IT.** §7h flagged an
unpriced −74. Blind it was **−46,520 B / −160 fns**; adjudicated it is **−808 B**,
because **69 of the 74 withdrawals were wrong** — made by a sweep that resolved
operands through the ICF congruence over **our own build** when the folding
happened in **retail's link**. 65 reversed (protecting 45,712 B), 9 enacted
exactly as predicted. ⚠ And **104 CONTRADICTED memberships remain LIVE
ledger-wide while `--validate` reports 0 contradicted, because it measures
map-consistency, not folding.** Do **not** bulk-withdraw.

**Coordinator error, and the lane named it before I did.** W9-A mispredicted
both its fixes, in opposite directions, because **it never checked the caller
population** — a rule already in CLAUDE.md. `??0UIComponent@@` has 16 subclass
ctors (+6,400 B against a predicted ceiling of +1,200); `??0Player@@` has 2
(measured 0). **Caller COUNT, not row size, predicts a map repair's yield.**
My own brief also said the `PlatformMgr` repair was four rows; it was **five**,
and a four-row fix would have discarded a correct but *displaced* name.

**Wave-10 candidates:** run the spatial screen's remaining 18 unadjudicated
fires; `??0UIComponent@@` (496 B @ 81.07) and `??0RndTransformable@@` (512 B @
73.94), correctly paired for the first time; the **allocator debug-overload
stratum**, which blocks the `char*` alias and everything under the STL container
layer; `0x824e0f68` → `list<OldMMInst>::insert`, proven by its *named*
create_node and charging at 99.8 so unlike its siblings **this one should pay**;
the 104 live CONTRADICTED memberships, individually; locating retail's
`NullFile` instantiation site (it blocks finishing `NewFile`); restructuring
`LoadRev` then naming `0x82430FD0` (1,728 + 192 B); and `?transform@MD5@Quazal@@`,
still unexplored. ⚠ `tools/dc3_map.py` resolves its map via a worktree-relative
sibling — the `60837907` bug class, still live.

## 7j. EXECUTION LOG — tenth wave (2026-09-13, coordinator session 3cdd3c)

Dispatched off `dee126a1` (42,749 fns / 3,871,308 B / 37.783768%). Four lanes,
all four landed. Closed at `b2677de3`, which includes one merge that was not
mine.

| lane | branch | merge | predicted | measured | verdict |
|---|---|---|---|---|---|
| A spatial fires | `w10-spatial-fires` | `654dc785` | +3 / +196 B | **+3 / +196 B** | exact |
| B allocator stratum | `w10-allocator-stratum` | `fbfc6229` | see below | **+5 / +1,864 B** | exact vs composed |
| C base-class bodies | `w10-base-bodies` | `c5297f13` | +9 / +1,152 B | **+11 / +1,584 B** | +2 / +432 B collateral |
| D contradicted memberships | `w10-contradicted` | `8404984b` | −7 / −696 B | **−7 / −696 B** | exact, deliberate |
| — *(peer merge, not this wave)* | `thunk-readjudication-2` | `b2677de3` | (+36 B claimed) | **+5 / +36 B** | +36 B exact |

**Wave total (my four lanes): +12 fns / +2,948 B.** Main moved
`dee126a1` → `fbfc6229` = 42,749 → 42,761 fns, 3,871,308 → 3,874,256 B,
37.783768% → 37.812540%. With the peer merge, main closes at **42,766 /
3,874,292 / 37.812890%**.

Every composition was re-measured by me on merged main and reconciled to the
byte; every gate was re-run rather than relayed.

### The wave's real output is three corrections, not the 2,948 bytes

**1. There was never an allocator blocker (B).** Two waves treated the
`MemOrPoolAlloc` 4-arg-vs-1-arg question as a hard blocker on the `char*` alias
and on W8-D's 904 B. The evidence for it was **a misread recursion trace**:
W9-B's four `SLOT-REFUTED` frames are *one* leaf failure printed once per stack
level as the recursion unwinds. Arity never entered into it, and the leaf
refusal is itself empty — both sides identical in every byte, resolving the same
single relocation on both sides, refused only by a `size < 16` vacuity guard.
⇒ **A vacuity guard is a claim about what masking hides; it does not apply when
nothing is masked.** Same shape as flat T1 understating provability by 27 pp.

I propagated that misreading into the wave-10 brief, so the correction lands
against my own dispatch as much as against W9-B.

The witness I warned might not exist **does** exist, and not where the brief
looked. The *callee* genuinely cannot discriminate — a release-stripped 4-arg
form is byte-identical to a 1-arg form. The **call sites** can, because the
debug arguments are compile-time constants and there is no LTCG:
`MemOrPoolAlloc` is **1 arg** (401 of 403 sites write only `r3`; `r4`/`r5`/`r6`
at **zero**), `PoolAlloc` is **2**, with 5 of 7 map arities reproducing as the
control. **Map defect, not source.** `FileRelativePath` reads 100.0/100.0 on
904 B as a result — W8-D's prize, stranded two waves.

**2. Adjudicating beats sweeping, and now it is priced (D).** Withdrawing all
104 live CONTRADICTED memberships blind costs **−5,384 B**; withdrawing only the
89 that retail bytes foreclose costs **−696 B**. **Adjudication protected
4,688 B = 87.1% of the exposure.** The three classes sum to the blind total
exactly (696 + 552 + 4,136 = 5,384), which was not guaranteed — a row can be
charged by more than one pair, so the partition adding up is a real check.
⇒ the W9-D rule ("69 of 74 withdrawals were wrong") generalises at 35× scale.

**3. A lane's inertness control does not survive composition (C).** W10-C
pre-registered that its `obj/Object.h` change recompiled 956 TUs while exactly
one unit moved, and argued 955 units at Δ0 *proved* the new `#ifdef` inert. On
merged main **fourteen** units moved, not three. The control was correctly run;
it simply answered a different question — one change in isolation, in one
worktree. The mechanism is legible: the repeated **+40 B across six unrelated
units** is one **shared template COMDAT** resolving differently, exactly the
hazard `gate_liveness.py` records. ⇒ **an isolation control licenses nothing
about the composed merge.** Units at 100% went 162 → 163 with **zero** falling
off, and `default/FileStream` crossed — a completion nobody predicted.

### Coordinator adjudication: handoff refuted before dispatch

W10-D's handoff #2 asked a future lane to move `Profile::mDirty` up 4 bytes.
**Refuted, and it was destructive** — `Profile` is a base class with many
subclasses, and `mDirty` is followed by `mPadNum` and `mState`.

Its conditional (*"if these two really are one retail body"*) was already
falsified by the same document's FABRICATED verdict. Independently: retail
`.text` contains **17 bodies of shape `li r11,1; stb r11,N(r3); blr` at 15
distinct offsets, two of them at `0xc`**; the compiler puts `mDirty` at `0xc`;
and — decisively — retail's own **`Profile` vtable at `.rdata 0x821121a4`** has
an RTTI COL pointer before index 0, `??_GProfile@@` at 0, `?IsUnsaved@Profile@@`
at 4, `?SaveLoadComplete@Profile@@` at 5, and **slot 7 =
`0x827a4fb0` = `stb r11,0xc(r3)`**. Slot 7 is `Profile::DeleteAll`, which is
literally `{ mDirty = true; }`. ⇒ **retail's `Profile::mDirty` is at `0xc`.**
Recorded in `docs/decomp/CONTRADICTED_AUDIT_2026-09-13.md` §12.
⇒ **a handoff stated as a conditional must be checked against its own lane's
verdict before it is dispatched.**

### Instrument findings worth carrying

- **The bare-vs-nested `splits.txt` trap took its FIFTH consecutive lane** (A).
  W9-A deferred `0x826B03D8` because "RGTrainerPanel.cpp has no `.text` pin"; it
  has **five**, under a nested heading, so a `^RGTrainerPanel.cpp:` grep returns
  zero. It concealed a byte-exact circular pin hole.
- **A Milo `X::Init()` states its own identity** (A) — it registers
  `X::StaticClassName()`. Screening every `RegisterFactory` body found 3
  mis-named and 17 unnamed rows, including one *inside its owner's own run*,
  which no spatial rule can see. The lane recorded its own false alarm:
  `?Register@BandWardrobe@@` legitimately has that body, so only a **class**
  disagreement is a defect.
- **A mis-named map row presents to the alias builder as a fold candidate** (A).
  `symbol_aliases.json` groups[249]/[1105] carried T1 retail-byte evidence for
  exactly the two rows this lane renamed. Both tools reasoned correctly about
  the right observation with the wrong model.
- **"Retail inlines this" is only half a diagnosis** (C). Three retail shapes
  hide behind one inline decision. `UIListArrow` reached 88.42 on the
  UIComponent/Trans pair and stopped — it wanted `DEFER_OWNER`, not
  `DEFER_OBJECT`. Pick wrong and the row reads like a scheduler wall, which is
  the shape that gets a row wrongly deferred as permuter-bound.
- **Price renames on their CALLERS, not their own rows** (B). C4 predicted
  −200 B and measured +708 B; two `PropSync<T>` caller rows paid +792 B. The
  `none` ruler returned exactly the predicted −200, so **the two rulers
  disagreeing was the signal.**
- **Three lanes recorded self-refutations that a results-only report would have
  hidden**: A's screen FP rate moved in *both* directions on recomputation
  (68.4% adjudicated, 71.4% conservative) after a first draft claimed both
  improved; B's first census returned a confident **0** because it filtered on
  `.pdata extent == 100` when the extent is 104 (alignment padding); C's first
  screen counted `bl` **by name**, so an unnamed retail callee was invisible and
  "retail inlines" read identically to "retail calls an unidentified address".

### My own bookkeeping errors this wave

- A ledger note lost the word `none` to **zsh backtick command substitution**
  inside a double-quoted string, leaving a sentence that reads as coherent and
  means nothing. Notes now go through a quoted heredoc.
- The retry keyed a W10-B note to **another session's commit**, because `HEAD`
  moved under me mid-sequence, and carried measures from a `report.json` built
  at *my* commit — a stale read. ⇒ **in a shared tree, `HEAD` is not a stable
  identifier for the thing you just measured. Key a snapshot to the SHA you
  built.** Both corrected in `49face65` rather than quietly fixed.
- I staged a ledger sidecar with a directory-form `git add` against the standing
  rule; the staged set was verified to be exactly the two intended files and
  then restaged by explicit path.

### Wave 11 candidates, priced

★ **The three `Init` bodies are the best-priced target in the tree right now.**
All three have `mpn == fuzzy`, the fold-stratum signature, so their only charges
are relocation-name arguments. Applying the measured rate (a relocation-name arg
costs `5/N` pp, `N = size/4`) gives an **integer** charge count to four decimal
places on all three — a third independent confirmation of the pricing screen:

| row | size | 100 − fuzzy | charges | prize |
|---|---:|---:|---:|---:|
| `?BandInit@@YAXXZ` | 1,008 B | 0.01984 | **1** | 1,008 B |
| `?PreInit@Rnd@@UAAXXZ` | 1,836 B | 0.03268 | **3** | 1,836 B |
| `?Init@UIManager@@UAAXXZ` | 1,916 B | 0.01044 | **1** | 1,916 B |

**Five relocation-name charges stand between us and 4,760 B.** ⚠ W10-A's
instruction stands: these are the ICF **fold** shape (`list<Object*>` vs
`list<void(*)()>`) — **do NOT change our container types; prove the folds.**
`matched_code` is all-or-nothing per row, so each row needs *all* its charges
cleared.

Other candidates, from the lanes' own handoffs:

- `??0RGTrainerPanel@@` now paired at **98.19643**, 224 B behind ~one
  instruction (A).
- **H4 from B: audit alias groups with NO map-resident member** — C1's had none
  and that alone was worth 116 B.
- A `DEFER_OWNER` branch on the one-arg in-class `ObjPtr` ctor, which unblocks
  Gen's 596 B; shared header, wants a PCH-cascade control (C).
- Re-screen with C's `bl`-count instrument beyond `ui/` and `rndobj/`: **239**
  paired sub-100 `??0` rows binary-wide against the 26 screened.
- The 9 named `Init` bodies now sit six-on-**exactly** 59.090908 — one
  systematic cause across six UI/ham classes, a force-multiplier body fix that
  could not be asked about while those rows were anonymous (A).
- `RecursePatternInternal` (892 B) is `default/File`'s last prize (B).
- Still-open user decision, carried since wave 3 and deliberately untaken:
  whether to delete `src/system/synth/Sound.cpp`, `ThreeDSound.cpp` and
  `ThreeDSound.h` (zero includers, zero compile edges, zero retail presence, two
  confirmed text bugs), keeping `Sound.h` under a DC3-only banner for its seven
  includers.

## 7k. EXECUTION LOG — eleventh wave (2026-09-13, coordinator session 3cdd3c)

Dispatched off `77cac933` (42,766 fns / 3,874,292 B / 37.812890%). **All four
lanes landed.** Main also absorbed **four peer merges** from another session
mid-wave, each of which arrived unbuilt and was built and recorded by me so the
composition chain stays unbroken.

| lane | branch | merge | predicted | measured | verdict |
|---|---|---|---|---|---|
| A prove the Init folds | `w11-init-folds` | `7aac40e2` | +9 / +4,848 B · +2 / +1,908 B | **+11 / +6,756 B** | both exact |
| B the 59.09 cluster | `w11-init-cluster` | `15bd7da8` | 4 legs | **+10 / +1,508 B** | all four exact |
| C ctor inline sweep | `w11-ctor-inline-sweep` | `c0b47e9f` | 0 · +596 B · +5/+324 B | **+10 / +920 B** | all three exact |
| D alias unmapped | `w11-alias-unmapped` | `91370b40` | +712 B/+5 · +1,792 B/+11 | **+16 / +2,504 B** | both exact |
| *(peer)* thunk-readjud-3 | — | `bb51be8f` | — | **Δ0 / Δ0** | expected |
| *(peer)* thunk-readjud-4 | — | `1fd041a1` | (+5 / +724 B) | **+5 / +724 B** | exact |
| *(peer)* thunk3-sividoc | — | `cfe959c8` | — | docs-only | — |

**My four lanes: +47 fns / +11,688 B.** With the peer merges, main moved
`77cac933` → `91370b40` = 42,766 → **42,818 fns**, 3,874,292 → **3,886,704 B**,
37.812890% → **37.934030%**.

**Every prediction in this wave was exact.** ELEVEN pre-registered changes
across four lanes, zero misses — the first wave with that record. Every composition
was re-measured by me on merged main with per-unit attribution; no unit fell off
100% in any lane.

### The wave's most valuable output is that it corrected ME twice

**1. The pricing screen cannot identify a charge KIND.** In §7j I priced three
`Init` rows at "five relocation-name charges guarding 4,760 B" and called the
integer charge counts "a third independent confirmation of the screen". Lane B
refuted the inference: **the decomposition is not unique**, since
`100/N == 20 × (5/N) == 100 × (1/N)`. My "one relocation-name charge" reading of
`?BandInit@@` is numerically **indistinguishable from five immediate charges**,
and on large rows the screen cannot discriminate at all.
⇒ **The screen prices a target; only retail bytes identify the charge.** The
arithmetic happened to be right here — lane A's byte-level proof is what
established that, not the arithmetic itself.

**2. My brief misclassified one of the three rows and undercounted the prize.**
Of the three, only **two** were the insert fold (`BandCharacter` 1,008 B + `UI`
1,916 B = 2,924 B). **`?PreInit@Rnd@@` is a MAP defect**, a different class
entirely — settled on the `??_R4` Complete Object Locator behind the vtable that
ctor stores, which spells `.?AVDOFProc@@`. And the fold's reach is **4,848 B
across NINE rows**, not my three: `TheDebug.AddExitCallback()` sits in every
subsystem's `Init()` aggregator. The seven I missed are `Game`, `Waypoint`,
`Graph`, `GameMicManager`, `BandUserMgr`, `UsbMidiGuitar`, `PatchDir`.

### The constraint that paid: proving beats retyping

Lane A was told to **prove the folds and NOT retype our containers** to make the
names agree. It did, and **`src/` was never touched**: our COMDATs are raw
byte-identical to retail at two levels (`insert` 100 B, `_M_create_node` 64 B),
each carrying one relocation, bottoming out at an already-landed
`MemOrPoolAlloc` fold. Refusing the retype cost nothing and bought 6,756 B.

### Findings worth carrying

- **Six rows on an identical score were one missing call** (B):
  `TheUI->InitResources("<ClassName>")` plus a `Symbol` ctor — nine
  instructions, with the `.rdata` argument literally the class name. Per-unit
  shows it cleanly: **seven units at exactly +88 B each.** Order was read
  per-class off the asm against a 14-row positive control carrying **both**
  orders, not assumed.
- ★ **And the vein is CLOSED, stated as such** (B): exactly **23** retail bodies
  call `InitResources` across all 3,083 `.s` including the 1,810 `auto_*`; 15
  already matched, 7 were the cluster, 1 unnamed. **There is no eighth.**
- ★ **A screen that fires 113 times and means 2** (B): arg 2 of
  `RegisterFactory` can only be `X::NewObject`; of 322 sites 113 disagree, but
  **111 are merely UNNAMED forgiven placeholders** and exactly **two** are
  mis-named. Exactly the shape that gets briefed as a backlog by someone who
  does not read the breakdown.
- **A handoff can be right about the machinery and wrong about the reason** (C),
  and taking it literally would have dropped 596 B. W10-C claimed the one-arg
  `DEFER_OWNER` branch would let `Gen`'s ctor sites diverge from `Load`'s; it
  cannot, since Gen's four sites are spelled *two*-arg. It works because
  **`Load`'s site is a LOCAL, not a MEMBER** — `DEFER_OWNER` moves member stores
  past the *enclosing class's* vptr store, and a local has none. ⇒ **members are
  sensitive to `DEFER_OWNER`; locals are not.**
- **`PartLauncher`'s residual is an EH-FRAME difference, not a store-shape
  wall** (C) — one surplus EH-state store displaces three EH temps `0x50`→`0x54`,
  a 340 B frame against retail's 328. Probably a class, not a row.
- **An isolation control held this time, and that is not the same as being
  sound** (C). The lane flagged its own Δ0 header control as isolation-only and
  told me to re-verify; I did, and only four units moved. It holds because no TU
  defines both macros, so the branch is never textually selected — not because
  isolation controls generalise. W10-C's did not.

### Lanes that recorded their own vacuous instruments

- **A's first population control was vacuous AND agreed with its prior** — the
  hardest kind. It masked the `+0x24` discriminator, counted 48 addresses, and
  read a decisive refutation that also matched a standing one. **Masking the
  discriminator and then counting answers nothing.** The control that works
  *resolves* the destination: 48 inserts → 48 **distinct** `_M_create_node`
  destinations, 0 shared, and exactly **one** masked-identical `create_node`
  image-wide — the same scanner returning 48 and 1, so it discriminates.
- **B's lowercase-hex search of the `.s` tree returned a clean decisive NOTHING**
  for 5 of 7 bodies, because `.fn` labels are **uppercase**.
- **B's own "byte-exact pin hole" framing was near-worthless** — a census found
  **915** such holes tree-wide. The body adjudication justified the change; the
  hole only said where to look.
- **C's inherited screen counted `bl` BY NAME**, so an unnamed retail callee was
  invisible and "retail inlines" read identically to "retail calls an
  unidentified address". Rebuilt on `objdiff-cli diff --format json` and
  validated against a known-answer fixture including a **negative** delta, plus
  `237/237` rows with `instructions × 4 == size` on both sides so a zero is a
  real zero.
- **Two tools crashed outright on the 51 landed `address: null` alias groups**
  (A), and in the merge tool **the naive repair would have merged all 51 into
  one.** Fixed. Recorded but not fixed: `icf_alias_merge.py --strict` is
  unusable, refusing on 82 pre-existing multi-address names, so the merge tool's
  only safety flag cannot be enabled by anybody.

### ⛔ My own instrument error: every `rc=0` I printed this session was vacuous

I ran gates as `python3 <gate> 2>&1 | tail -2; echo "rc=$?"`. **`$?` there is
`tail`'s exit status**, and `tail` essentially always exits 0 — so the printed
`rc=0` **could not have come out any other way**. Same family as the
build-probe vacuities already in CLAUDE.md.

Caught only by contradiction: the manifest printed
`Fix: ./tools/ninja-locked, then re-run this check` while I printed `rc=0`. Run
properly it returns **rc=6 (BUILD OWED)** and `--validate` returns **rc=2
(REFUSED — explicitly NOT a pass and NOT a failure)**.

**No landed conclusion changes**, because I also quoted every tool's own verdict
TEXT, which is the real signal — and that is precisely why the house rule says
to paste `NATIVE_GATE_RESULT ...` verbatim rather than paraphrase. But the rc
figures attached were not measurements.
⇒ **Redirect to a file and test `$?` on the next line. Never read an exit code
through a pipe.**

### Shared-tree bookkeeping, now a standing hazard

Four peer merges landed mid-wave, one of them (`1fd041a1`) editing
`target_symbol_map.json` **after** my W11-B build, which correctly drove the
manifest to rc=6. My W11-B figures came from a build that *was* current at
`15bd7da8` and matched prediction to the digit, so the snapshot is keyed to
`15bd7da8` — **the SHA I actually built** — not to whatever `HEAD` had become.
⇒ **In a shared tree, `HEAD` is not a stable identifier for the thing you just
measured.** Capture the SHA before the build and pass that literal downstream.

### Wave 12 candidates

- **The EH-frame wall** (C): 516 B across two rows, `PartLauncher` and
  `EventTrigger::Anim`, one surplus EH-state store shifting three temps by 4.
  Likely a *class*, not two rows.
- **`?Handle@CustomizePanel@@` re-prices to ONE insert/delete away (5,036 B)**
  (B), not the 3+2 that RESIDUAL-1 recorded on 2026-08-14. ⚠ Verify against its
  charged-site list before briefing it — that row has been mis-briefed to three
  consecutive lanes already.
- **The coupled `0x82466000` change** (A): it is misnamed, `MetaPerformer.cpp`
  mis-pins the 112 B sliver `[0x82466000,0x82466070)` (nearest sibling 1.1 MB
  away, while `DOFProc.cpp` pins the 16 bytes immediately after), and alias
  **group 331 is a `MAP_DEFECT_INVERTED_CONCLUSION`** riding on it, its
  fuzzy-100 financed by a forgiven placeholder vtable relocation. **One change,
  taken whole** — renaming without re-homing is a pure −60 B.
- **`0x82574348` is `?NewObject@MetaPanel@@`, reading a FALSE 100.0** carried by
  placeholder forgiveness (B). Fixing it likely **costs 100 B** to buy accuracy
  — its own measured lane, not bundled behind positives.
- `FileMerger::Merger` (216 B, screened clean, cheapest unclaimed row — needs
  its defining TU located) and the 14 mixed positive-delta `??0` rows (C).
- `RecursePatternInternal` (892 B), `default/File`'s last prize.
- Still-open user decision, carried since wave 3 and deliberately untaken:
  whether to delete `src/system/synth/Sound.cpp`, `ThreeDSound.cpp` and
  `ThreeDSound.h`, keeping `Sound.h` under a DC3-only banner for its seven
  includers.

### 7k addendum — lane D, and a third correction to my own brief

**D landed at `91370b40`: +16 fns / +2,504 B, both changes exact.** I briefed
H4 ("audit alias groups with no map-resident member") as the lane's
highest-value item. That named the right **observation** and the wrong **unit
of work**.

The group-side screen finds 130 rows of which exactly **two** are payable.
Turning it around to the **charge side** — *what charged pairs would such a
repair close?* — found a class instead: **13 `_Param_Construct<T>` /
`_Copy_Construct<T>` same-`T` pairs over 21 sites**, where STLport emits the
identical body `new(__p) T(__val)` with the same relocation, which is
`/OPT:ICF`'s fold condition exactly. **The group screen missed 11 of the 13**,
because it required our spelling to already *be* a member — and the best
candidates were in no group at all.
⇒ **A screen defined over the artifact you happen to hold is not a screen over
the thing you are trying to find.**

13/13 proven on retail bytes with **13/13 cross-`T` decoys refuted** — the decoy
arm is load-bearing, since this family shares its masked body **112 ways out of
270**.

★ **A structural blind spot in the alias gate**, worth more than the bytes:
`classify_group`'s *"not named ⇒ UNWITNESSED"* short-circuit fires **before**
the fatal *"survivor not in tmap"* branch, so **`0 contradicted` says nothing
about this class.** Second time this campaign a green alias verdict has proved
scoped far narrower than it reads — the first being `--validate` measuring
map-consistency rather than folding.

**Four refusals, three of which correct my brief:** H6 is worth **+3,084 B /
+11 fns over 12 rows**, not the 892 B I briefed (the briefed "one relocation
name" was 15 raw charges, 14 placeholder-forgiven) — chase-proven and **still
refused**, because the bl-caller census the file demands as its license
*denies* it (`0x82b74600` has 3 callers, so not the uncallable-junk class). H1
is not the Δ0 one-liner I briefed: a rename alone would **unpair** the row, and
`PoolAlloc.h`'s stated reason for the 5-arg form is **stale** — every 5-arg
call site is `HX_NATIVE`-only today. H5 is probably not a defect at all (both
`LevelData` addresses are vendor-band, where `/Gy`-off makes duplicate
instantiations legitimate). Char3D (+192 B) proved `0x82b9b590` constructs
`LocalePanel::Entry`, but identifying it means reading the callee's name out of
the very map under audit.

★ **VB-1, cheap and cross-cutting:** four of the blockers were **vendor-band**
(≥ `0x82A00000`) map rows naming Milo STLport templates, while only 4 of 118
`_Construct` rows live there. A high-yield screen for a map lane.

### Scorecard for the wave, honestly stated

Eleven pre-registered changes, eleven exact. But **my dispatch brief was
corrected in three of the four lanes** — the charge-kind inference (A and B),
the row classification and prize scope (A), and the unit of work (D). The lanes
were right and the coordinator's priors were wrong each time, which is the
argument for briefs that carry *evidence and constraints* rather than
conclusions: every one of these corrections came from a lane that was told to
prove something rather than to apply something.

### Inbound cross-repo findings from a dc3-decomp lane (2026-09-13) — verified, 1 of 3 transfers

A dc3-decomp session offered three shared-engine findings. **Tested literally
against main `96c3a685` before accepting any of them.** Recorded here because
two do NOT hold on this tree, and a future lane should not re-derive that.

**1. `UtilDrawCigar` — CONFIRMED, queued as a lane.** Our row reads mpn
**82.63761**, the briefed figure to the digit (872 B, `default/system/rndobj/Utl`).
Diffing our `src/system/rndobj/Utl.cpp` against dc3's shows **our source is
dc3's pre-fix version exactly**, so its four changes apply as a clean patch.

★ **The headline is a REAL BUG and it was confirmed independently, not taken on
trust.** Our two rings use opposite y/z phase conventions:
```
v1(h0b, sinLonPi2 * r0, sinLon    * r0)   // y = cos-phase, z = plain
v2(h1,  sinLon    * r1, sinLonPi2 * r1)   // y = plain,     z = cos-phase  <-- swapped
```
so the second ring is rotated 90° in that plane. dc3's fix makes `v2` match
`v1`. **Land that on correctness whatever it measures.** The other three changes
are codegen shaping (`double`→`float` to stop an `frsp` at each use; splitting
`h1` into two statements to block an `fmadds` contraction; a loop-induction
restructure) — ⚠ **adjudicate those against RB3 retail bytes**, because dc3's
stated justification cites *DC3's* target listing and our target is a different
binary. dc3 reports reaching 90.61 on its own copy.

**2. `__frsqrte` — already correct here, no action.**
`src/xdk/LIBCMT/ppcintrinsics.h:12` declares `double __frsqrte(double)`, and so
does dc3's line 12. The briefing lane flagged this as probably already-correct
and it was.

**3. `CharIKFingers::CalculateFingerDest` — DOES NOT DESCRIBE THIS TREE.** The
briefed defects were "curl quaternion angle 2× too large" plus "a spurious
negation after `acos` making the 0.87f clamp fire every frame". **Neither
exists here, or in dc3's current source.** A full diff of the function shows
ours and dc3's are identical *except one operand ordering* in the `acos`
denominator (ours `len02 * 2.0f * lenTip`, dc3 `len02 * lenTip * 2.0f`). Our
curl is `curl03.Set(f1z, (2.0f * PI - 2.0f * angle02) * 0.5f)`, which reduces to
**π − angle02** — a correct half-angle — and our line 367 is character-identical
to dc3's line 344.
⇒ **The reading that fits is that dc3 had a divergence and the fix brought dc3
to where rb3-xenon already was**, i.e. a dc3-specific defect rather than a
shared-engine one. Consistent with the standing caveat that **dc3-decomp
postdates RB3 and its engine code carries subtle behavioural differences — "dc3
is correct for RB3" is never assumed.** Nothing to port but the operand reorder,
worth testing as pure codegen shaping on our row (mpn 90.38909, 1,100 B).

⇒ **The general lesson, and it cuts both ways across the shared engine:** a
sibling repo's fix is evidence about *that* repo until its premise is tested
here. Two of three briefed items were already-correct on this tree, and one of
those was described as a live bug. **Diff the function before porting the
patch.**

## 7l. EXECUTION LOG — twelfth wave (2026-09-13, coordinator session 3cdd3c)

Dispatched off `96c3a685` (42,766 / 3,874,292 B). All four lanes landed, plus a
coordinator fix adjudicating W12-C's handoff. Closed at `afdd72f7`.

| lane | merge | predicted | measured | verdict |
|---|---|---|---|---|
| A EH frame | `18dc042d` | +328 B | **+4 / +328 B** | bytes exact |
| B CustomizePanel + false 100 | `f95665a1` | Δ0 | **Δ0** | exact |
| C DOFProc rehome | `1dbfe2a9` | +1/+48 · +2/+160 | **+4 / +460 B** | stage 1 exact, stage 2 missed |
| D vendor-band screen | `afdd72f7` | 4 registered | **+13 / +4,132 B** | 3 exact, 1 diagnosed |
| — Rnd::Terminate (mine) | `afdd72f7` | Δ0 | **Δ0** | exact, 2nd attempt |

**Wave total: +21 fns / +4,920 B.** Main `96c3a685` → `afdd72f7` = 42,766 →
**42,839 fns**, 3,874,292 → **3,891,624 B**, 37.812890% → **37.982048%**.

### Two screens I briefed as high-yield were refuted with controls

**VB-1 is a bounded negative and the band is BETTER than baseline** (D). 214
vendor-band STLport-template map rows of 3,399, byte-corroborated **163 =
76.2%**, against a **non-vendor control of 71.5%**. Proven defects **2 =
0.93%**. `≥ 0x82A00000` is not a band but **three regions**, and the one holding
all four originating blockers is owned by VocalTrack, Synth, GemManager,
TrackPanel, Mic, GemTrack, PostProc_NG, Lit_NG — game and engine code.
⇒ W11-D saw a defect-rich band because it adjudicated rows **it had already been
blocked by** — a *selection effect*, which I then propagated into a dispatch
brief as a yield estimate. **Second measured instance of "an address band is not
a provenance classifier."** Do not re-screen.

**The EH-frame "class" is a diagnosis, not a lever** (A). 15 of the 16 rows
sharing PartLauncher's shape have retail `maxState == 1`; PartLauncher is the
only one with real retail cleanup states *and* an unprotected body. And A
refuted two premises of my own brief: `EventTrigger::Anim` is **not this defect**
(`target_size == base_size == 188`, and the store W11-C read as ours-only is on
**both** sides), so the briefed "516 B if both cross" was never one fix; and the
"+12 B frame" symptom screen I proposed **under-counts by construction**, since
a surplus EH state only costs mainline bytes when it forces a flag store.

### The wave's best result is a row nobody could see was broken

★ **`?Terminate@Rnd@@UAAXXZ` scored a clean fuzzy 100 while calling the wrong
function.** Retail has exactly ONE `bl` in that slot — `+0x5c` → `0x82466080`,
80 bytes, decoding to `RELEASE(global at 0x82CC6368)` = `DOFProc::Terminate`'s
`RELEASE(TheDOFProc)`, sitting immediately after DOFProc's dtor.
`?Terminate@RndMat@@SAXXZ` is a **different** function at `0x82553fc8`, not
called from here. The `#ifdef HX_NATIVE` guard was **exactly backwards**: it
excluded the call retail makes and kept the one it does not.

**It cost nothing on the metric because `0x82466080` is UNNAMED and `name_check`
forgives placeholder targets.** Our `RndMat::Terminate` is literally `{}` but
EXTRN, so it still emitted a `bl`, which paired against retail's — wrong callee,
zero charge, clean 100.
⇒ **"our `bl` pairs with an unnamed retail target" is a far sharper query for
metric-invisible defects than any source diff**, because the score cannot see
that class by construction.

★★ **Two attempts, and only the measurement separated them.** Adding
`DOFProc::Terminate` while keeping `RndMat::Terminate` measured **−1 fn /
−180 B** — exactly the extra `bl`, two calls where retail has one. **Replacing**
measured **+0 / +0**. Reading alone would not have caught the difference.

⚠ **And dc3 is NOT the oracle for this line.** A dc3 session measured its own
`Rnd::Terminate` at 46/46 under `name_check` with the call *unguarded*, and
dc3's source calls **both** functions — one `bl` more than RB3 retail has.
**Porting dc3's shape verbatim IS the −180 B leg.** Both sides match their own
target and still differ: a genuine engine divergence between the titles, not a
decomp error in either. The standing caveat that dc3-decomp postdates RB3 earned
its keep twice today.

⛔ **Scope correction I owe the record: this is NOT a native-path fix, and I
said it was.** The guard meant `HX_NATIVE` already took the DOFProc branch, so
the native port was correct all along; the defect was **match-build-only**. What
it buys is accuracy, removal of a false 100, and an unblock — W12-C measured
naming `0x82466080` at −180 B *before* the source fix, and it should now pay.

### Rules the lanes established

- ⛔ **A relocation-name "contradiction" is not evidence until you grep
  `symbol_aliases.json`** (D). The lane had a refusal *drafted* before finding
  the address is a T1-proven 19-member fold group containing the very spelling
  it was about to reject. Applying the existing rule converted a false refusal
  into the lane's **entire yield**.
- ⛔ **Before re-homing a map row, grep the alias file for its ADDRESS, not its
  NAME** (D). Run 1 measured +836 against a predicted +3,852; the *gain* was
  right (all 29 crossed) and the gap was an unscreened **loss of 3,080 B** from
  orphaning the alias group anchored there. A name-keyed reverse-risk census
  (1,442 rows, answer 0) **structurally cannot see it**.
- ★ **The `bl` inline screen is two-stage** (D): it says *whether* retail
  inlines, never *which* store order. The plain gate scored **below not inlining
  at all** (76.09 → 67.96) before `DEFER_OWNER` reached 100.0 — W11-C's
  members-vs-locals lesson recurring one lane later.
- ★ **A retirement is only valid on the tree it was measured on** (B). This is
  why `?Handle@CustomizePanel@@` reopened four times: each prior account was
  correct when written and never re-derived. It is now priced definitively —
  **one instruction, +1 fn AND +5,036 B, NOT reachable from source**, closed at
  class level by a transplant of the exact retail construct that emits no mask.
  Both clauses are needed; cheap-looking *and* valuable is what reopens a row.
- ★ **When a misidentification is between two classes with byte-identical
  generated bodies, the correction prices at ZERO, not at the row's size** (B).
- ★ **Retail's compiler knew a ctor could not throw and ours could not, because
  it arrives as `EXTRN`** (A) — `PartOverride() throw()`, which the DC3 oracle
  already spells. Retail `FuncInfo maxState=6` with one IP2State entry past the
  last instruction; ours 7.
- ★ **Coupling demonstrated, not asserted** (C): withdrawing alias group 331
  *alone* priced at −352 B and measured **0** once the rename made it redundant.

### Instrument defects the lanes found in themselves

- C's hand COFF comparator read `SizeOfRawData` as a function extent and
  over-read 40 B into the next function, returning a confident `BODY DIFFERS` —
  the same one-sided over-read STLPORT-1 records. It named the row anyway **for
  accuracy, expecting zero bytes**, and collected 252 B its own instrument had
  written off.
- B's clean decisive nothing was **its own regex bug**, not the documented grep
  shim. Reaching for the known hazard would have mis-attributed it; the symptoms
  are identical.
- `ab_measure`'s end-of-run restore **deleted a lane's untracked doc** written
  into the worktree mid-run, and the next `>>` silently produced a half-file —
  caught only because `wc -l` went *down* after an append. Write deliverables to
  `~/tmp` or commit immediately.

### Cross-repo intake (a dc3-decomp session)

Three findings offered, **tested literally, one transferred**: `UtilDrawCigar`
confirmed (our row reads `82.63761` to the digit, our source is dc3's pre-fix
version exactly, and the y/z sine-phase swap between the two rings is real) —
**queued, not yet run**; `__frsqrte` already correct here; and the
`CharIKFingers` finding **did not describe this tree**, which dc3's own history
then confirmed (`143dab01f`) as a dc3-only defect their fix brought *to* where
rb3-xenon already was. ⇒ **A sibling repo's fix is evidence about that repo until
its premise is tested here. Diff the function before porting the patch.**

### Wave 13 candidates

- **`UtilDrawCigar`** (872 B @ 82.63761): land the y/z phase swap on correctness;
  adjudicate the three codegen-shaping changes against **RB3** retail bytes,
  since dc3's justification cites DC3's listing.
- **Name `0x82466080`** now that `Rnd::Terminate` is fixed — measured at −180 B
  before the source fix, should now pay.
- The 13 other mixed `??0` rows (`BandDirector`, `BandCharacter` 2,180 B), each
  needing its own callee breakdown; ⚠ W11-C's screen is now one TU stale.
- `EventTrigger::Anim` (188 B) — characterised as scheduler-bound, permuter off.
- Char3D / `0x82b9b590` (+192 B), the one open row of the VB-1 residue.
- Re-home the `0x82b74600` pin — `~GranularSynth` compiles into `Synapse_dsp.obj`
  so a 136 B row stays unpairable until moved. A splits lane, **not**
  metric-neutral.
- Still-open user decision, carried since wave 3: whether to delete
  `src/system/synth/Sound.cpp`, `ThreeDSound.cpp`, `ThreeDSound.h`.

## 7m. EXECUTION LOG — thirteenth wave (2026-09-13, coordinator session 3cdd3c)

Dispatched off `49b5a79f` on five cross-repo leads from a dc3-decomp session,
every one treated as a **hypothesis about our binary** and adjudicated against
RB3 retail bytes. All three lanes landed, plus a shared-tool fix. Closed at
`fc5338f1` (+ `4ba4aa64`).

| lane | merge | predicted | measured |
|---|---|---|---|
| A swapped args | `d4564ac5` | Δ0 | **Δ0** |
| B float / arg-order | `6d0244f5` | Δ0 | **Δ0** |
| C cigar + DOFProc name | `fc5338f1` | +1 / +80 B | **+1 / +80 B** |
| — `ab_measure` restore fix | `4ba4aa64` | n/a | selftest ALL PASS |

**Wave total: +1 fn / +80 B.** Main `49b5a79f` → `fc5338f1` = 42,839 →
**42,840**, 3,891,624 → **3,891,704 B**. Every prediction exact.

### ★ The wave's yield is SIX confirmed defects, and the score can see almost none of them

| defect | status | metric |
|---|---|---|
| `std::sort(end, begin)` in AmbientOcclusion (UB as written) | CONFIRMED | Δ0 — row unpaired |
| `NormalizeTo` overwriting its own output (`Key.cpp`) | CONFIRMED | Δ0 — row unpaired |
| `2t² + 3t³` where retail has `3t² − 2t³` | CONFIRMED | **−0.637 pp** |
| messages built `(pad, value)`, retail is `(value, pad)` | CONFIRMED | +1.25 pp, 0 B |
| second cigar ring rotated 90° (y/z phase swap) | CONFIRMED | **exactly 0** |
| `DOFProc::Terminate` carrying a DC3-era block retail lacks | CONFIRMED | +1 / +80 B |

Three independent reasons the metric is blind here, and they must not be
conflated: **rows can be UNPAIRED** (`fuzzy 0 / mpn 0` — no edit inside them can
score at all); a swapped argument pair is a **register** arg diff, which `mpn`
excludes **by construction**; and `matched_code` is **all-or-nothing at
`fuzzy == 100`**, so a large sub-100 gain buys nothing.

⇒ **`UtilDrawCigar` gained 7.97 pp (82.63761 → 90.61009) for ZERO bytes.**
⚠ Its remaining ~9.4 pp is **UNDIAGNOSED, not at-limit** — do not file it.

★★ **And being right can COST.** Correcting the easing curve moved
`GetBlendState` **94.412 → 93.775**: retail keeps `-2.0` in `.rdata` and uses
`fmadds`, MSVC folds the correct negation into `fmsubs` against a positive 2.0,
so the arithmetically **wrong** `2t² + 3t³` merely happened to reproduce
retail's instruction shape. Kept, per accuracy over headline.

### Two refutations, both from lanes refuting their own leads

- **The Kinect float lead is REFUTED FOR RB3 because its own positive control
  FAILED.** Of 66 float literals *shared* by both trees, only **4** exist
  anywhere in the 14 MB image (`0.0/0.5/1.0/2.3`). So the negative on the
  suspects proves nothing; the finding is that **the body is not in RB3 at any
  coefficient** — RB3 is not a Kinect title — corroborated by the pinned span
  performing **zero float loads of any kind**. The `joint[1]` change is labelled
  **DC3-grounded, explicitly not an RB3 adjudication**.
- **`FacePriority`'s member order is CORRECT**, refuted by the lane that raised
  the suspicion (retail int at `+0`, float at `+4`, confirmed independently by
  the comparator's `lfs f13,0x4(r3)` at stride 8).

### Of dc3's three cigar shaping changes, RB3's bytes support TWO

Exactly why the brief said re-derive rather than inherit — **both failures are
instructive**:
- **Claim 1 is the whole payload (+6.60 pp) but its stated REASON is wrong.**
  dc3's "no `frsp`" is not literally true — there are two — and both belong to
  `fcfid` int→float casts, so neither is attributable to double arithmetic.
  **Right change, wrong rationale.**
- **Claim 2 is REJECTED: a CORRECT DIAGNOSIS with an INEFFECTIVE REMEDY.** RB3
  *does* contract twice elsewhere (in the `sqrtf` scale), so dc3's blanket claim
  is wrong; at the `h1` site RB3 genuinely does not contract and **we do** — but
  four spellings all emit `fmadds=3` under a standalone `/FAs` probe. Inert at
  the **codegen** level, not merely below the metric's resolution.
  ⇒ **A true diagnosis does not imply an available lever.**

### ★★ Both prior forecasts on `0x82466080` were wrong, in opposite directions, and one was mine

W12-C said "≈0 B" and was right that the call site is forgiven either way. My
brief said "should now pay" and was right about the sign. **Neither priced the
third ingredient:** our `DOFProc::Terminate` carried a DC3-era `DataVariable`
block retail lacks — **256 B against retail's 80** — so pinning and naming alone
pairs a row **that still cannot match**. Three coupled parts: source + splits
re-home + map row. Result verified here: the row now reads **80 B at 100.0/100.0**.

Two side results: **`lbl_82CC6368` IS `TheDOFProc`** (the open question my own
`Rnd::Terminate` adjudication left), proven twice over; and **dtk corroborated
the re-home unprompted**, merging DOFProc's `.pdata` and stripping `Mat.cpp`'s —
the unwind record followed the function, an independent witness from a tool
nobody asked.

### Measurement and instrument findings

- ★★★ **A whole-binary Δfuzzy of `+0.000000pp` DOES NOT ESTABLISH INERTNESS.** A
  408 B row moving 2 pp is ~`0.0000008 pp` against a 10.3 MB denominator — an
  order of magnitude **below the six-decimal print**. Inertness must come from
  per-row figures. Reading `0.000000` as "nothing happened" is reading the print
  precision, not the tree.
- ⛔ **A scan reported "0 `bl` callees" in a 4,260-byte function.** Vacuous:
  capstone's PPC decoder stops at the first undecodable word and never reached
  the call at `0x52c`, while the relocation table said `nrel=127`.
  ⇒ **DRIVE OBJECT SCANS OFF THE RELOCATION TABLE, NEVER A LINEAR DISASSEMBLY.**
- ⚠ **The native gate FAILED on a fresh worktree's FIRST run** (16/18, two
  targets STALE) with `rc=0` and **zero** error or linker lines. `ninja -d
  explain` named `cmake.verify_globs` / `VerifyGlobs.cmake_force`: CMake's
  `CONFIGURE_DEPENDS` glob verification is still dirty right after the initial
  configure. **Not fully disentangled** — a confounding edit to a file linked by
  exactly those two targets landed between runs, n=1 each way, no clean control.
  ⇒ **A first-run STALE with `rc=0` and no error lines must be RE-RUN before it
  is believed.** This does **not** relax the 0-SKIPs rule, which is the opposite
  failure. My own gate passed 18/18 on the first run in main, consistent with the
  fresh-worktree diagnosis.
- **A fourth source comment refuted by bytes in three waves** (`"retail arg
  order: (pad, value)"` — retail is the reverse in all ten constructions).
- **A pre-registered prediction FAILED and is recorded so nobody re-tries it:**
  dc3's operand spelling measured **byte-identical** on both legs; MSVC
  canonicalises both, so source operand order is inert there.

### `ab_measure` restore fix (`4ba4aa64`)

Two lanes lost their deliverable in one day to a silent untracked-file deletion,
**both while being told `verified: true`**. The verification re-read only the
**tracked** diff, so it could not fail in the direction the damage occurs.
★ The two fixes have **different scope** and conflating them would overclaim:
the **print** (now naming every deleted path) is what prevents the loss; the
**predicate** cannot — after a successful removal the untracked sets match by
construction — but it closes the separate hole where a **failed** removal still
reported verified. Both proved to discriminate before landing; `--selftest` ALL
PASS on merged main.

### Wave 14 candidates

- **Name `0x82491918` and `0x824f5b68`** — that makes W13-A's two rows pairable
  and its two confirmed fixes finally measurable. Highest-value follow-up.
- **`0x8252f3e8` / `0x8252e6b0`** — `SigninChangedMsg` is almost certainly
  misnamed and the two names may be **SWAPPED**, making it a coupled change where
  half is worse than none. ⚠ Grep `symbol_aliases.json` by **ADDRESS** first.
- **The empty-EXTRN sweep: 182 candidates** (game 41, meta_band 22, hamobj 13,
  world 13, …). Every one emits a `bl` that cannot be inlined across a TU
  boundary, which is what let `Rnd::Terminate`'s wrong callee pair and be
  forgiven. ★ Rank by retail's paired destination **size** — ours is 4 bytes of
  `blr`, so any materially larger retail destination is a candidate, and only the
  survivors need decoding. The "retail destination is also trivial" rows are the
  in-pass control.
- `Tessellate`'s remaining 552 B; a tree-wide sweep of other `NormalizeTo` /
  `std::sort` call sites for the same swap.
- Adopt the standalone `/FAs` probe (~20 s) as the loop for codegen questions —
  it cannot perturb the build tree or the six obj patchers.
- Still-open user decision, carried since wave 3: whether to delete
  `src/system/synth/Sound.cpp`, `ThreeDSound.cpp`, `ThreeDSound.h`.

## 7n. EXECUTION LOG — fourteenth wave (2026-09-13/14, coordinator session 3cdd3c)

Dispatched off `90be524c`. All three lanes landed. Closed at `ca95db7f`.

| lane | merge | predicted | measured |
|---|---|---|---|
| A name the unpaired rows | `ca95db7f` | Δ0, Δfuzzy +0.02…+0.05 | **Δ0, +0.031406pp** |
| B empty-EXTRN sweep | `5a05874d` | — (no `src/`) | **Δ0**, 0 of 112 |
| C swapped names + sweep | `31ebd0d6` | +6 / +984 B | **+6 / +984 B** |

**Wave total: +6 fns / +984 B**, `90be524c` → `ca95db7f` = 42,840 → **42,846**,
3,891,704 → **3,892,688 B**, 37.982830% → **37.992430%**. One unit completed
(`UsbMidiGuitarMsgs`, 70/70 rows, 4724/4724 B).

### The wave's real output: 5,320 B made MEASURABLE, and two hypotheses replaced

**A named `0x82491918` and `0x824f5b68`, buying ZERO bytes exactly as
pre-registered** — and making W13-A's two already-confirmed bug fixes visible to
the ruler for the first time. `Tessellate` 0 → **64.15** (4,796 B) and
`QuatSpline` 0 → **65.80** (524 B), both verified by me on merged main.

★ **The naming evidence generalises and is stronger than a mangling match:** each
call site is reached from a caller row **already at fuzzy 100**, with the branch
at the **same section offset in a same-sized section** on both sides. *If
retail's branch went anywhere else, those callers could not be at 100* — the
caller row's own perfection is the witness. The same fact discharged the
downside before any edit (three sites, two at fuzzy 100 worth −392 B if our
spelling differed; our base objects already spelled the identical name at the
identical offset).

★★ **The control worth reusing: a per-row sweep of all 69,219 rows** showing the
only changes are the two renames — **0 common rows better, 0 worse.** That is
how you prove a map edit did nothing else; the whole-binary Δ cannot resolve a
small row at all.

**C refuted the swap hypothesis and found a SIX-CYCLE.** `0x8252e6b0` was
correct and untouched — the anomaly had been read from the wrong end. It never
presented as an off-by-one because the map had two entries transposed *inside*
the window, absorbing a step. Verified by a self-witnessing construct:
`DECLARE_MESSAGE(C,"s")` emits a `Type()` referencing a unique literal, so all
eleven names checked 11/11 against their own strings — and **the callee names
the ctor.** Genuinely coupled: the pins were swapped in mirror image, so a
names-only edit would put two names in units that cannot define them, reading 0%
forever. **Third coupled map change this session where half is worse than none.**

**B returned a bounded negative on the class I sent it after: 0 of 112
examined.** `Rnd::Terminate` is a **singleton, not the head of a vein.**

### Instrument findings — three, and all three are about controls

- ⛔⛔ **A CHECK THAT CANNOT DISTINGUISH "BROKEN" FROM "CORRECT" IS NOT A
  CONTROL, even when it is the obvious one to reach for.** B's first validity
  check ("does retail's destination name match our callee?") returned **0%**,
  which reads exactly like a broken scan — but 0% is **the only answer a working
  instrument can give**, because the ICF fold survivor's name is arbitrary. It
  nearly discarded a sound sweep. The two checks that *do* discriminate both
  passed (caller body lengths 111/112 equal; independent re-decode of all 20
  suspects agreeing with the relocation chain 20/20).
- ★ **Report precision WITH its denominator.** B: 1,843 direct call sites → 112
  **examined** (1,731 unpairable, 1,645 of them because objdiff pairs by NAME and
  retail's counterpart is anonymous) → 20 fired = **17.9% of examined**. Quoting
  "20 of 1,843" would have understated the rate **16×**.
- ★ **An ABSOLUTE name-injectivity assertion fires on a clean tree** (2 licensed
  duplicates) and refused a correct edit. **The working assertion is the DELTA.**
- Also: a `.text` move **will** fail its first build on `.pdata` drift — that is
  dtk re-deriving, not an error, and `ab_measure` correctly **refused (exit 2)**
  rather than measuring it.

### Two veins closed with reasons, not shrugs

- **The declaration-order sweep is DRAINED**: ~2,300 sites, 33 automated flags,
  **33/33 refuted by hand**. ⇒ **Inverting an output-parameter call is usually
  UNCOMPILABLE** (const input slot, or type mismatch), so **the compiler is
  already the detector**; wave 13 found the only two shapes that slip past
  (same-type non-const lvalues) and they are now exhaustively enumerated. **Do
  not re-fund a textual sweep** — the instrument is the asm signature (retail
  setting up `r4` before `r3` at a 2-arg `bl`) over the split `.s` files, no
  build required.
- **12 of B's 20 hits cannot be wrong-callees at all** — adjustor-thunk callers,
  and a vtordisp thunk forwards *by definition* to the method it names. And the
  4 masked non-thunk rows are **not porting mistakes**: all four are `{}` in
  rb3-Wii too, with retail-360 carrying Xbox-only bodies (Gamerpic rewards,
  string censoring) the Wii dev build never had — the "we do not hold the body"
  class by a new route.

### ⛔ A correction to my own brief

I wrote *"expect Δ0, silence is a pass"* for both of C's tasks. Right for the
sweep, **WRONG for the map repair**: a relocation-**name** charge **is** scored
by `name_check`, so Δ0 there would have meant the edit never reached the build.
**I over-generalised the register-arg-blindness rule to a different mechanism.**
The lane pre-registered a positive sign *against* my instruction and was right.

### A block neither oracle has

A's Task 2: the missing 552 B of `Tessellate` is retail testing a
`batcher.batching` data variable per mesh and, unless batching, sending
`"record"` and `"update_objects"` to the global `"milo"` object.
**`batcher.batching` appears NOWHERE in dc3-decomp or rb3-Wii** — that absence
is *why* our source was short, since we inherited dc3's newer trimmed version.
Reconstructed from retail bytes alone.
⚠ **Honest shortfall:** 57.44 → 64.15, **below** the lane's own predicted 65–85
band, and we now emit 4,932 B against retail's 4,796 — **overshooting by 136 B**
having been 552 B short. The logic is present; the residual is
regalloc/stack/funclet shape.

### Wave 15 candidates

- ★ **The CIRCULAR-PIN HAZARD** (C's handoff, highest value): a plausible map
  name justifies a pin, and the pin then corroborates the name — a
  self-consistent wrong pair, very unlikely to be unique. Key the lane on **"does
  this function's callee set agree with the unit it is pinned to"**.
- **`default/MatAnim`'s `_M_allocate_and_copy` row is a wrong map name** (target
  1,160 B calling `QuatSpline`; ours 160 B calling `MemOrPoolAllocSTL`) — 5.59%
  fuzzy explained. Needs its own caller census.
- **B's 6 map defects** making objdiff compare our `Save` thunk against retail's
  `??_E` thunk (needs vtables), and its 6 genuine `UNIMPLEMENTED_BODY` rows.
- `Tessellate`'s 136 B overshoot and `QuatSpline`'s remaining 34% — both
  stack/regalloc class, permuter off by directive.
- A one-line comment at `AmbientOcclusion.cpp:1232`, where the declarations are
  *deliberately* `(end, begin)` with a correct call — that site **will re-flag on
  any future textual scan**.
  ✅ **First instance arrived the same day**: a dc3-decomp cross-repo token
  differ (~47k function pairs) forwarded it as "`std::sort(priEnd, priBegin)`,
  first/last reversed". Its `ARG_SWAP` pass cannot tell a call from a
  declaration. Adjudicated as a false re-flag here; the peer recorded the
  reading rule ("if the flagged arguments are declared on the preceding lines,
  it is declaration order and inert") in dc3-decomp
  `scripts/analysis/xrepo_audit.py` (`638a17085`). Of the five leads in that
  batch, four were fixes already landed here (`135f6a9e`, `2a531e2f`) read from
  a stale checkout — a landed fix is indistinguishable from a live defect to a
  differ over two working copies unless the sweep states the revision it read.
- Still-open user decision, carried since wave 3: whether to delete
  `src/system/synth/Sound.cpp`, `ThreeDSound.cpp`, `ThreeDSound.h`.

## 7o. STATUS AT ONE WEEK, and the wave 15 dispatch (2026-09-14, coordinator session 3cdd3c)

Written in answer to the user's check-in ("what is our roadmap to 100%, what
have we accomplished this week, what remains"). Every number below is read
from `report.json` at `08860838` or from `docs/decomp/progress_ledger.jsonl`,
not recalled. Partition by source class was computed by mapping each report
unit to its `objects.json` path (full path first, basename fallback;
`auto_*` and `xdk/` handled explicitly — a `default/` prefix scan gets this
wrong and was discarded).

### What "100%" means — three layers, read the right one

| target | size | status |
|---|---:|---|
| whole binary (`total_code`) | 10,245,956 B | **37.99%** matched (42,846 / 69,219 fns). 20.41% is XDK vendor code with no source — out of scope by standing directive, already 100% mapped |
| in-scope: band3 + system + network + main | 6,464,768 B | **60.21%** matched; gap **2,572,560 B** |
| identified closable levers (§7 bottom line) | ≈220 kB | ≈2.1 pp of `total_code` **at 100% conversion**, a rate no wave in the record has reached |

**Neither 100% figure is attainable.** The whole-binary one is blocked by the
XDK directive; the in-scope one by ICF-destroyed pair bytes (129,360 B proven),
Quazal's `/Od` band (96.8% of network code, unmatchable at `/O1` at any source
quality), and the ~91%-irreducible relocation-name stratum.

### Where the in-scope bytes are

| class | units | @100 | matched | gap |
|---|---:|---:|---:|---:|
| system (engine) | 667 | 107 | 60.14% | 1,626,316 B |
| band3 (game) | 256 | 57 | 66.67% | 702,848 B |
| network / quazal | 116 | 1 | 11.68% | 238,144 B |
| xdk, no source | 229 | 0 | 0% | 2,090,904 B (out of scope) |
| `auto_*` unattributed | 1,810 | 0 | 0% | 1,686,856 B (~9% attributable-and-portable) |

By work kind, game + engine only:

| stratum | band3 | system | what it is |
|---|---:|---:|---|
| partial, `0 < fuzzy < 100` | 302,660 B | 701,872 B | **divergence in code we already hold** — the only real vein, and where "matched but wrong" bugs live |
| placeholder-named at 0 | 337,176 B | 724,924 B | 82% divergence in held code, 18% unwritten (P1-BODYTRIAGE). **Naming cannot cross a row whose bytes we do not reproduce** |
| `mpn` 100, `fuzzy` < 100 | 54,580 B | 155,684 B | relocation-name / register charges; ~91% irreducible fold noise |
| real-named at 0 (unpaired) | 8,432 B | 43,836 B | wrong map name or missing base symbol — 351 rows, small, **highest bug-exposure yield per byte** (the W14-A mechanism) |

Largest per-unit gaps, for the grind lane: band3 — RockCentral 41,200 B,
VocalTrack 24,568, NextSongPanel 17,368, GemManager 17,260, OvershellSlot
16,444; system — VocalTrackDir 27,064, BandCharacter 22,592, LightPreset
21,384, EventTrigger 21,148, CameraShot 19,620.

### The week (2026-09-10 → 09-14), from the ledger

| measure | 09-10 | 09-14 | Δ |
|---|---:|---:|---:|
| `matched_functions` | 42,305 | 42,846 | **+541** |
| `matched_code` | 3,774,924 B | 3,892,688 B | **+117,764 B** |
| `matched_code_percent` | 36.843% | 37.992% | **+1.149 pp** |

14 waves, 84 lane merges, 460 commits; every wave landed on a full main
build with the native gate re-run on the merged tree by the coordinator.
**The score understates the week.** The higher-value output was correctness
the metric cannot see: quaternion interpolation overwriting its own output
(`135f6a9e`), an easing curve with both coefficients wrong (`2a531e2f`), MIDI
messages built pad/value-swapped (`2a531e2f`), a render ring rotated 90°,
`Rnd::Terminate` calling an empty function instead of releasing a global
(`afdd72f7`), a rim light on the wrong shader register, a spotlight beam
occluding its own flare, a sub-loader capturing its parent's root; a 6-cycle of
misnamed message rows and thunks naming each other's addresses; 69 of 74 alias
withdrawals shown wrong and restored; `ab_measure` now reports what its
restore deleted; the crossing ranker prices on the graded ruler; the patcher
chain proven to have four live passes; and four veins closed *with reasons*
(fold class, missing-body, vendor band, empty-EXTRN) so no lane re-hunts them.
One process failure is on the record (§7n): a `--amend` in shared main,
contained, fixed forward.

### Model routing — user directive 2026-09-14

> "drive opus subagents on specific tasks. if a task is really hard then put
> fable on the job but prefer opus in general. if opus reports something cant
> be fixed, that needs digging into. fable is smarter and can likely sniff out
> the real issues."

Applied as: **Opus by default, one specific task per lane; Fable for the
genuinely hard ones; and an Opus "can't be fixed" / `AT_LIMIT` is an
ESCALATION TRIGGER, not a result** — the item is re-dispatched to Fable with
the Opus report attached. This is the repo's own rule made into a dispatch
policy: a confident "unfixable" is the claim most worth auditing, because it
closes veins nobody re-opens (MPNGAP-1; the `REGISTER_SWAP` labels that
dissolved once the real defect was fixed).

### Wave 15 dispatch (three lanes — API capacity, see §7n)

| lane | model | task | pre-registered expectation |
|---|---|---|---|
| **W15-A circular-pin census** | opus | build the instrument "does this function's callee set agree with the unit it is pinned to", run it over every pinned unit, adjudicate the top hits on retail bytes, repair only proven pins | accuracy lever; Δ may be ±; every repair A/B'd with per-unit attribution. Known priors to reproduce: CameraManager's 13 fns inside the Quazal block, nine engine units claiming one fn each there |
| **W15-B real-named-at-0 drain** | opus | classify all 351 real-named rows at 0 in game+engine (wrong map name / missing base symbol / missing body / phantom carve), fix the provable ones; includes MatAnim `_M_allocate_and_copy` and W14-B's six `Save`-vs-`??_E` map defects | +bytes only where a correct body was merely unpaired; otherwise bug exposure; each name change priced BEFORE edit |
| **W15-C game-unit crossing grind** | opus | `crossing_worklist` (graded ruler) over RockCentral, VocalTrack, GemManager, NextSongPanel, OvershellSlot; only rows whose charges NAME a source construct; pre-register each | small positive; `mpn`-only wins expected on wrong-callee fixes |

Escalation slots held for Fable: any lane item reported unfixable.

## 7p. Wave 15 results, and the wave 16 dispatch (2026-09-14, coordinator session 3cdd3c)

Four of wave 15's six lanes are landed (each rebased in its worktree, merged
`--no-ff`, full main build + the four gates re-run by the coordinator, ledger
row keyed to the merge SHA, pushed). Two are still running.

| lane | merge | Δfns / Δbytes (measured on main) | what it proved |
|---|---|---|---|
| **W15-B** real-named-at-0 drain (opus) | `c90f107c` | **+8 / +1,692 B** | 351 rows classified 199 / 87 / 65 / 0 phantom; **`DataArray::Release` had NO definition in the match build** (body inside `#ifdef HX_NATIVE`); `0x8245ee48` = `RndTransAnim::MakeTransform`, a circular pin caught in the act, payout 100 % caller cascade |
| **W15-C** game-unit crossing grind (opus) | `b9e32547` | **+6 / +4,252 B** (fuzzy −0.000061) | RockCentral sent `SystemLanguage` where retail sends `SystemLocale` — the rb3-Wii DEV oracle is wrong at 7 sites; an undefined MWCC extern on a live path; `Find<UILabel>` vs `BandLabel`; `0x826662e0` = `FriendsProvider::Reload` |
| **W15-A** circular-pin census (opus) | `5b14bc86` | **Δ0** (as pre-registered; tree hash moved, SPLIT ran) | the plurality-vote form of the census is NOISE (1.38× / 1.51×); internal linkage is the proof; refuted its own anon-hash premise; 2 pins repaired |
| **W15-F** PlatformMgr_Xbox wiring + region (opus) | `318a09c6` | **+12 / +1,160 B** | `PlatformMgr_Xbox.cpp` (728 lines) was in-tree and absent from `objects.json` — `tools/project.py` drops the edge silently; **5 of 7 dc3 body-ports were byte-exact on the first try**; 2 blocks re-homed, `.pdata` re-derived by dtk unprompted; 11 retail PlatformMgr rows 0 → pairable, 9 at 100 |
| **W15-D** coupled thunk permutation (fable) | `5c051996` | **+218 / +4,056 B** (fuzzy −0.0028; exactly additive to the lane's settled `ab_measure --revert`) | the six rows were ONE CYCLE — every "occupying" row was itself a misnamed thunk (`0x8234ebc8` is the ICF survivor of every empty vtordisp thunk in the binary), so destination-NAME adjudication (W14-B, W15-B) could not resolve it; the name-free instrument is retail RTTI vtables (2,220 COLs) × `class_layout_report` slot assignment, with 721 vtables agreeing slot-for-slot and a planted sabotage that flips AGREE→WRONG as the control; whole population is **1,806 thunks** (W15-B's census missed all 504 r4 hidden-struct-return thunks): 304 renamed, 116 re-homed, 15 proven folds, 185 undecidable; the dtor-family bucket is NOT ICF — MSVC emits `??_E` as a weak-external alias of `??_G` and the linker resolves the thunk to `??_G`; **`BandLabel::Save` fell from a FALSE 100 to 4 %** once correctly named — DC3's `HamLabel.cpp` is pinned onto retail `BandLabel`; 75 wrong-named thunks had scored fuzzy 100 via placeholder forgiveness |
| **W15-E** RockCentral Handle + Movie map (opus) | `c52f2af6` | **+11 / +4,256 B** | `0x827c9110` is `TickToMs`, not `Movie::Init` — both names compile to the SAME 24 B thunk and the row read a FALSE 100 (ruler structurally blind; settled on retail bytes of the neighbour `BeatToMs`); `TickToMs` was declared and **defined nowhere**; `0x825df840` is `OvershellSlot::RemoveUser`, our body carried Wii-only `TheWiiProfileMgr.RemovePad()` — a behavioural bug on Xbox, 40.41 → 100; `Handle@RockCentral` Δ0 → **escalated to Fable (W16-B)** |
| **W16-A** swallowed definitions + unwired TUs (opus) | `c8dbf582` | **+7 / +428 B** (predicted exactly) | the `#ifdef HX_NATIVE` gate hides exactly ONE payable function tree-wide (`MidiFullPath`, +64 B) — the vein W15-B opened is drained; `rnddx9/Tex.cpp` was in-tree and compiled by NOTHING (scattered into `ShaderMgr.cpp`: +5 / +364 B, 5.7× what the census priced it at); `DxRnd::DrawRect` declared-but-undefined, ported from dc3: fuzzy 0 → 99.64, mpn 100 ⇒ **+1 fn / +0 B**, the arg-only landing (retail `shaderMgr` in r24, ours r28; dropping the local measured WORSE); BeatClock/AudioDucker are DC3-only — retail has neither, and the control `MeasureMap` shows the RTTI/string channel is BLIND to non-polymorphic classes; giving an object a local definition converted forgiven call sites into checked ones (−40 B inside a +404 B change); 16, not 17, non-XDK `.cpp` are unwired |
| **W16-C** NextSongPanel commute escalation (fable) | `a427ff73` | **+0 / +12,220 B** (predicted exactly; +0.1193 pp) | W15-C's "ARITH_COMMUTE proved inert" tested ONE textual lever and inferred canonicalisation; retail refutes the inference INSIDE THE SAME FUNCTION (BASE-first at two strength-reduced `&mNodes[count]` sites, IV-first at three, identical 7-instruction schedule); operand order is per-function optimisation-HISTORY dependent (40 `/FAs` compiles: 0/1/2 unrelated sites ahead keep BASE, 3 or 4 flip, non-monotone; a codegen-free temporary MSVC creates then eliminates shifts two sites together); fix is a byte-identical `Symbol` temp at the goals loop + the oracle spelling restored at the section site; `none` control moves identically ⇒ instruction fix, not alias; no closed-form rule derived — the doc records the screening harness instead. **Third Opus "unfixable" to cross on Fable this session** |
| **W16-B** RockCentral OnMsg escalation (fable) | `76a32373` | **+15 / +4,700 B** (predicted exactly; +0.0459 pp) | W15-E's "uncollectable, 28 scheduling charges" on `Handle@RockCentral` was WRONG: porting the three stubbed `OnMsg` bodies took the row 94.11 / 28 sites → **100.0 / 0** with no map or alias edit — the charges were MSVC scheduling `HANDLE_MESSAGE` blocks around leaf stubs, i.e. a caller-side symptom of missing callee bodies; retail calls ONE 128 B body from both the UserLogin and FriendsListChanged blocks (our two COMDATs byte-identical incl. relocations ⇒ T1 alias); `TimeConversion` had four wrong map names (`0x827c9218` `??__FTheLocale`→`TickToSeconds`, `0x827c9288` `OnBeatToMs`→`OnSecondsToBeat`, `0x827c9328` is the real `OnBeatToMs`, `0x827c91a0` a 36 B `TimeToTick(f*1000)` no header declares — left anonymous) and the 12 B `BeatToTick` pinned inside `MetaPerformer.cpp`; the `if (TheBeatMap && TheTempoMap)` guards are DC3 contamination (absent from retail bytes and rb3-Wii); `AttemptRemoveUser@OvershellSlot` crossed only after dropping the oracle-shaped `bool b1` (cost a callee-saved reg); `GetPlayerID` stays `int` — 8-caller census shows 4 `cmpwi` sites vs this one `cmplwi`, fix is site-level. Flagged, not done: `DrawBeatLine@GemTrack` calls `GetLoopTick` where retail calls `?OnIsSpeechSupportable@SpeechMgr@@` (wrong callee or fold); `RemoveAllInstances@Gem` unaliased `_Rb_tree::clear` residue. **Fourth Opus "unfixable" to cross on Fable this session** |
| **W16-E** PlatformMgr follow-ups (opus) | `62943770` | **+3 / +684 B** (bytes predicted exactly; fns predicted +4, measured +3 — the TokenRedemptionPanel re-home paid +1 not +2) | `GetName` and `ShowGamercard` crossed on retail-refuted DC3 divergences (2-arg `Localize`; validity-first, no NUI/Kinect branch); the region's ONE genuine mis-pin was not a Msg block — two `vector<u64>` COMDAT blocks BandUser→TokenRedemptionPanel (BandUser never `push_back`s); all 11 COMDAT-hosting blocks STAY under the emitter census (`tools/comdat_emitter_census.py`); ★ `0x8251CED0` is `??0XMPStateChangedMsg@@QAA@H@Z`, NOT `??0ServerStatusChangedMsg@@` — the block is not mis-pinned, the NAME is wrong (W15-F §6.1 refuted); Opus called it uncollectable (rename alone = permanent 0) ⇒ escalated to W16-G |
| **W16-D** invisible-source sweep (opus) | `83488617` | **+3 / +348 B** (predicted exactly: 43,126→43,129 fns, 3,926,136→3,926,484 B; FELL OUT 0 on every body) | `DxTex::DoCompress` 284 B, `UserHasController` 44 B, `MemHandle::Unlock` 20 B, each byte-exact; two censuses (unwired sources reconcile EXACTLY with W16-A; 211 declared-but-undefined rows / 56,340 B of which **126 rows / 26,376 B are UNPAIRABLE in `auto_*` units — need a PIN, not source**); five real bugs incl. the native `UserHasController` stub that answered `false` unconditionally (its own gate FAILED 16/18 then PASSED 18/18); finding: defining a body behind an already-correctly-spelled external costs callers NOTHING (3/3), W16-A's forgiven→checked penalty bites on a NAME change only; blocked list with retail-byte reasons (`MemFreeH`/`_MemAllocH` 192 B on an `inline` `MainThread`; `FxSendReverb360` 3,280 B has NO DC3 source; FFT quartet 6,432 B VMX128 undefined in DC3 too; `JoypadPollCommon` 2,468 B behind a 19-block grab-bag heading) → escalated as **W16-J** (fable). `docs/decomp/INVISIBLE_SOURCE_SWEEP_2026-09-14.md`; tools `undefined_externals_census.py`, `rowset_snapshot.py`. Native gate PASS 18/18 skipped=0. |
| **W16-F** W15-D pin handoff (opus) | `3b0a5277` | **+18 / +1,628 B** (predicted exactly: 43,129→43,147 fns, 3,926,484→3,928,112 B) | 15 vtable-proven `.text` re-homes (+6/+1,648 B; five reach 100 outright), the 4 "kept" rows re-pinned AND renamed in one commit (−1/−176 B; `Load@BandLabel` 100→26.1 because DC3's `HamLabel` body had been pinned onto retail's `BandLabel` — a false 100 made honest), 13 thunk rows (4 proven-wrong heads set to JSON `null`, 6 thunks pinned over `auto_*` at 12 B; +13/+156 B vs +12/+144 predicted). **Finding:** the +1 surprise is `?Save@EndingBonus@@$4` crossing 98.333→100 because its target `0x822d3908` lost its wrong name and became a forgiven placeholder — **nulling a proven-wrong map name PAYS at every thunk that targets it** (naming economics in reverse). Three units lost 100% by DENOMINATOR_SHRANK (CharTransDraw, MultiMeshProxy, SpotlightEnder) — accuracy. **Refused with retail-byte reason:** `0x8252a598` is a genuine ICF fold (both candidates 12 B relocating against the empty-string COMDAT). `tools/pdata_extent.py`: the `.pdata` length field is `>>8`, not `>>2`. Leads to W16-I: `0x822d3920` map-named `??2SpotlightDrawer` is a `Copy@EndingBonus` thunk target (misnaming likely); three NODEF heads need source. Rebase conflict under `BandUser.cpp` against W16-E's TokenRedemptionPanel move, both honoured. `docs/decomp/THUNK_HANDOFF_REPINS_2026-09-14.md`. Native gate PASS 18/18 skipped=0 (run on main; lane touched no `src/`). |
| **W16-G** vtable-slot divergences (fable) | `1fbefb24` | **+27 / +1,872 B** (43,147→43,174 fns, 3,928,112→3,929,984 B; main gate reproduced exactly) | 4 of the 6 briefed slot-count divergences were INSTRUMENT ARTEFACTS — `class_layout_report` counts vbtable rows and W15-D's RTTI dump, keyed by class name, was comparing SECONDARY vtables — so `TourSavable`, `PracticePanel` and `DxTexRenderer` have no divergence at all; 2 were real header shape bugs: `Server` gains slot [8] (`GetPlayerIDs(vector&)`, name unattested; 21 vs 19 virtuals still unsettled) and `PlatformMgr::Callback` (14 slots, DC3's) is replaced by the 3-slot `ThreadCallback`; the two 50 % `$4` thunks (`SynthEmitter::Handle`, `UILabelDir::PreLoad`) were dtk 8+4 mis-carves, not signature divergences (+1/+20 B; `UILabelDir` re-home +148 B); ★ the W16-E escalation closed as ONE priced change — map rename to `??0XMPStateChangedMsg@@QAA@H@Z` + emitting TU + `.text` re-home = **+26 / +1,704 B**, and the real `??0ServerStatusChangedMsg@@QAA@_N@Z` body is at `0x823ecd70`; `DrawRect@DxRnd` closed NEGATIVE after five spellings (permuter-class, r24/r28 anchored on the `shaderMgr` load). `docs/decomp/VTABLE_SLOT_DIVERGENCES_2026-09-14.md`. Native gate PASS 18/18 skipped=0. **Fifth Opus "uncollectable" to cross on Fable this session** |
| **W16-H** ContextChecker sliver + Save bodies (opus) | `adee9d93` | **+3 / +1,080 B** (43,174→43,177 fns, 3,929,984→3,931,064 B; main gate reproduced exactly; pre-registered Δ0 — the miss is the finding) | `0x8275B868` is `?RegisteredFactory@Object@Hmx@@SA_NVSymbol@@@Z`, not ContextChecker's `IsContextUsed` — renamed and re-homed for **+1 / +864 B** because the bytes that would have discriminated were exactly the forgiven ones (⛔ `OBJECTCPP_TU` §1.2's "byte identity outranks the geometry" is FALSE here). Of W14-B's six `UNIMPLEMENTED_BODY` rows: **2 written from retail bytes** (`BandScoreboard::Save` +128 B and one more, byte-exact before being named, +2 / +216 B), **2 were never bodies** (`BandSwatch::Save`, `WorldInstance::PreSave` are misnamed `$4` thunks reading a FALSE 100), **2 fully decoded but unwritten** (`EventAnim::Save` 172 B, `BandList::Save` 388 B → W16-K). ⛔ the brief's six row sizes were not those rows (each a 12 B thunk already at 100; 3 of 6 sizes wrong). Proposes a tree-wide `$4`-thunk false-100 census (→ W16-K). `docs/decomp/CONTEXTCHECKER_NAME_AND_SAVE_BODIES_2026-09-14.md`. Rebase conflict in `splits.txt` against W16-G (w16-h carve of `0x822DA010–0x822DB298` kept; `.pdata` re-derived). Native gate PASS 18/18 skipped=0 (worktree and main). |
| **W16-I** Gem-region residue (opus) | `58735f7a` | **+14 / +1,328 B** (43,177→43,191 fns, 3,931,064→3,932,392 B; main gate reproduced exactly; 4 of 5 step predictions exact) | `DrawBeatLine@GemTrack`: the MAP NAME was wrong — retail's callee is `int GetLoopTick(int)` (site feeds `TickToBeat(int)`; rb3-Wii `GemTrack.cpp:522` literal), not `SpeechMgr::OnIsSpeechSupportable`; 2 map rows + HamRibbon/TrainerPanel re-pin = **+6 / +656 B** vs +2 / +216 predicted (⚠ callers not enumerated — the lane's own lesson). `RemoveAllInstances@Gem`: a PROVEN ICF fold — `set<TrackWidget*>::clear` was a missing member of an existing T1 group (+2 / +616 B); `Gem::mWidgets` is `set<TrackWidget*>` in both trees, so the brief's container-type branch was never live. W16-F leads: `0x822d3920` is `Copy@EndingBonus` (+5 / +44 B across 4 thunks), `0x823aedf0` is the `Replace@CharWeightable` `$2` thunk — it needed the right SPELLING, not source (+1 / +12 B); `PatchRenderer` still needs a TU. `0x827C91A0` stays a forgiven placeholder (no `SecondsToTick` helper exists in any oracle); its caller `0x82695178` is the debug-HUD `UpdateNowBar`, ported as a one-liner where retail is 628 B — absent source, not a name. `Handle@OvershellSlot` PRICED and refused: **9,276 B behind ONE reloc-name charge** — retail's `ToggleMuteStatus` compiled to a bare `blr` (already in an `FT-EMPTY` alias group) while ours is 56 B because the tree has no `VoiceChatMgr.cpp`; collecting it would need a behaviour-removing `src/` change on inference plus a 4-byte-`blr` alias. ★ Refutes W16-B's "raw `band.exe` reads fail validation" — `tools/pdata_extent.py` reads it via the PE section table with a passing must-fail selftest. `docs/decomp/GEM_REGION_RESIDUE_2026-09-14.md`. No `src/` touched (native gate run on main for the record: PASS 18/18 skipped=0). |
| **W16-L** PlatformMgr / Server / wrong callees (opus) | `bf9eeec7` | **+9 / +4,244 B** (43,191→43,200 fns, 3,932,392→3,936,636 B; main gate reproduced exactly; every step pre-registered and exact) | `StoreInfoPanel::GetRecommendationIndexPath` had **TWO** bugs: the W16-G wrong callee (retail dispatches Server slot 7 with `StorePanel::Instance()`→`StoreUser`→`GetPadNum` = `GetPlayerID(padnum)`) AND a second — retail calls `SystemLocale()` passed as `const char*`, not `SystemLanguage()` as a `Symbol`. The row was ANONYMOUS, so the source fix alone was invisible; named (0→99.83, Δ0) then closed **+236 B**. ★ The same `GetMasterProfileID` bug recurs in `TokenRedemptionPanel::GetOffersForToken` and `GetPreviousOffersForUser` (both anonymous rows; **+2 / +396 B**), and rb3-Wii's `RedemptionState` enum is WRONG for 360 — retail's is contiguous, witnessed at five call sites. PlatformMgr: `ThreadStart` +124 B, `EnumerateFriends` +356 B, `ThreadDone` mpn 100 / +0 B, and **`Handle` (3,112 B) closed by three MAP defects, not body defects** — `0x8251c170` was misnamed `??1ObjPtr<MoggClip>` (body is `b XMPRestoreBackgroundMusic` = `EnableXMP`, old name had zero references), the other two charges are the bare-`blr` FT-EMPTY survivor `0x826c3888`. UILabelDir: `fn_82812028` is ONE 16-byte `$4` thunk (`addi r3,r3,-692` to the `ObjectDir` subobject), `0x82812078` is the EH prefix of `??1UILabelDir`, not code (+1 / +16 B). ⛔ Server's two surplus virtuals **NOT PROVABLE from the image**: retail slots [9..18] all fold to `li r3,0; blr`, a 40-dispatch census tops out at slot 14, `XboxServer`'s override fixes an index not an identity; zero metric payoff either way, nothing removed. `Poll@PlatformMgr` (1,844 B) STOPPED: six charges are one regalloc-class construct but two are unproven reloc-name folds (`??2Friend@@` vs `??2CriticalSection@@`, `push_back<Friend*>` vs `<ChatReceiver*>`), so uncollectable even if the source change lands (RESIDUAL-1). `docs/decomp/PLATFORMMGR_BODIES_2026-09-14.md`. Native gate PASS 18/18 skipped=0 (worktree and main). |
| **W16-K** `$4`/`$2` thunk FALSE-100 census (opus) | `faf3bf75` | **+5 / +64 B** (43,200→43,205 fns, 3,936,636→3,936,700 B) | The forgiven stratum is SIZED for the first time: **325 named thunk rows / 3,812 B** read fuzzy 100 only because their retail branch target is an unnamed placeholder. After repair: FALSE_100 **3 / 36 B**, TRUE 24 / 268 B, UNDECIDED **298 / 3,508 B** — and UNDECIDED is "per-row evidence cannot decide", NOT "fine": one UNDECIDED row was provably wrong and was repaired. New defect class: a thunk row TRANSPOSED with its neighbour leaves one half CHARGED (visible to `thunk_target_audit.py`) and the other a forgiven false-100 (visible only to this census) — neither tool alone sees it, a both-halves-charged sweep returns 0. Two instances repaired, each predicted exactly: TrackDir `CDA@` transposition (+16 B / +1) and a UIFontImporter 5-cycle rotation (+48 B / +4); `0x82403728` = `RndDir::Export` by 20-fold convergence on retail's branch layout. `0x822AF178` re-homed CharInterest→BandSwatch, Δ0 as pre-registered. The 3 surviving FALSE_100 rows (BandSwatch::Save, WorldInstance::PreSave, PreloadPanel::SetTypeDef) adjudicated wrong with NO correct spelling in any of our objs — deliberately neither nulled nor renamed to a spelling that would read a false 100. Instrument `tools/thunk_false100_census.py` with a `--selftest` whose two sabotage legs flip. NOT done: the two Save bodies — `bs << ObjList<T>` does not exist in our tree (verified with a control), so both need a new `operator<<` template in `obj/Object.h`, a PCH input cascading ~281 TUs; carves and the decoded `EventAnim::Save` body recorded in §9. Not named: `0x82403728` (RndDir::Export) and `0x823F94E8` (RndTransformable::Replace, 24-fold), both proven — naming is a bet paying in bug exposure. 9 charged thunk rows in LocalUser/RemoteUser/NullLocalBandUser/BandUser (§8) left alone to avoid colliding with W16-L — now landed, so they are FREE. `docs/decomp/THUNK_FALSE100_CENSUS_2026-09-14.md`. Native gate PASS 18/18 skipped=0 (worktree and main; no `src/` touched). |
| **W16-N** | opus | **+5 / +716 B** (43,205 → 43,210; 3,936,700 → 3,937,416 B; `total_code` unchanged). Merge `f73ccadb`. Item 1: the briefed "9 charged thunk rows" framing was wrong two ways when tested literally — `ClassName@ReviewDisplay` reads fuzzy 100 (not charged) and only 4 of 9 are INCONSISTENT. Verdicts: `PreLoad@UIComponent` and `SetTypeDef@UIPanel` THUNK ROW WRONG (renamed, Δ0 B as predicted); `Load@RndCam` a THREE-ROW ROTATION (CubeTex 3-cycle, +1 / +16 B predicted exactly); `IsLocal@LocalUser`, `GetRemoteUser@RemoteUser`, `CanSaveData@NullLocalBandUser`, `UserName@NullLocalBandUser`, `SyncProperty@BandUser` IRREDUCIBLE (ICF fold hubs; no thunk-side base spelling has a retail address, a rename ⇒ permanently 0%); the `ForceEmit_*` destination METRIC-FITTING, left in place (repair = pin move into `ReviewDisplay.cpp` at −60 B). Item 2: W16-K's `bs << ObjList<T>` blocker REFUTED — retail's `fn_824C93C0` is the generic `operator<<(BinStream&, const std::list<T,Alloc>&)` we already hold at `BinStream.h:367`; no `Object.h` edit, no PCH cascade. Seven retail functions were missing, not two; source-only leg Δ0 exactly, carve+name leg predicted +7 / +808 B, measured **+4 / +700 B**: crossing set 5 rows / 808 B byte-exact, `EventAnim::Save` 41/43 equal but withheld on two reloc-NAME charges against ICF survivor `fn_824C93C0` (⚠ a `[sym]` reloc arg charges BOTH rulers, not fuzzy only), one BandCamShot row fell out −108 B because naming `fn_824C8F68` turned a forgiven site into a checked one (kept as bug exposure). EventAnim boundary is `0x824C95F0`, not W16-K's `0x824C95F8`. Item 3: naming `0x82403728` = `Export@RndDir` and `0x823F94E8` = `Replace@RndTransformable` measured **Δ0 exactly** across 46 newly-checked sites — the 20-/24-fold convergences confirmed by measurement; `RndTransformable::Replace` now pairs at 16.9% and exposes a `dynamic_cast` divergence. NOT done: the `fn_824C93C0` fold alias (368 B behind a T1 byte check of the HamCamShot half), the ReviewDisplay pin move, the `Replace` source fix, re-homing `0x82403728` out of `Anim.cpp`. Deliverable `docs/decomp/THUNK_CHARGED_ROWS_AND_SAVE_BODIES_2026-09-14.md`. Native gate PASS 18/18 skipped=0 (worktree and main; `src/` touched). |
| **W16-M** | fable | **+2 / +704 B** (43,210 → 43,212; 3,937,416 → 3,938,120 B; merge `e004c07b`). ESCALATION of W16-L's Opus stop verdicts, tested on retail bytes. **1a REFUTED**: `??2Friend@@SAPAXI@Z` vs map's `??2CriticalSection@@SAPAXI@Z` @`0x827bd2f0` is a real fold -- `tools/alloc_fold_gate.py` recomputes ADMIT from our compiled COMDAT bytes (same 8-byte `li r4; b MemAlloc` thunk, same reloc target); installed in `symbol_aliases.json` group 1542 with the gate evidence. **1b REFUTED**: retail Poll's `bl` displacement resolves to `0x82b5f808` exactly and our `vector<Friend*>::push_back` COMDAT is T1 reloc-identical to the `push_back<ChatReceiver*>` survivor (chased through `_M_insert_overflow`); alias installed. **1c SURVIVES IN PART**: the member-offset half of Opus's reading was wrong (XONLINE_FRIEND offsets are right against the xdk header) but the placement half stands -- retail loads `mFriendsBuffer` BEFORE the `mFriendsEnum` guard and keeps it live; every spelling forcing that makes MSVC keep a second IV family (V1 **-14 fns / -440 B**, 11 EH-funclet rows fell out, reverted). Six variants, Poll 99.245 -> 99.544 fuzzy / 99.566 mpn, not crossed (1,844 B). **Server tail SURVIVES**: retail `??_7Server` @`0x820577bc` = 19 slots, [9..18] all the `li r3,0; blr` null hub; ours = 21 slots, [0..8] structurally identical; which two of six are surplus is unprovable (three-deep unnamed `/Od` Quazal holder chain, zero DC3 Quazal names). Nothing removed from `Server.h`. **3A `??0PlatformMgr@@QAA@XZ` CROSSED +364 B, 3C `?UpdateSigninState@PlatformMgr@@QAAXXZ` CROSSED +340 B, both predicted exactly** -- levers were the `/Oi` `memset`/`memcpy` intrinsic spellings where cl 10224 differs from DC3's cl 11886, and cl 10224 emitting adjacent same-value member stores in SOURCE ORDER (DC3's spelling is reversed). Measured on the lane's baseline `073152eb`: 43,200 -> 43,202 / 3,936,636 -> 3,937,340 B (**+2 / +704**, 0 fell out); on main additive to the byte (43,210 -> 43,212 / 3,937,416 -> 3,938,120). NOT done: 3B `??0ProfileSwappedMsg` (needs the unnamed constructing TU `fn_8251D6C8` with two unidentified externs); 3D `fn_8251D378` = `PlatformMgr::Init` deliberately NOT named (three callees unresolved; an unproven name is charged). Side findings for other lanes (doc section 6): `fn_82516320` (`PlatformMgr::SetDiskError`) is a bare `blr` whose 208 B `.pdata` extent absorbed a DataSet/DiskErrorMsg fragment -- the aggregate originates in retail `.pdata`, re-carve at `0x82516324`; `src/xdk/LIBCMT/stddef.h:20` `offsetof` bug; `fn_823EC788`/`fn_823EC980`/`fn_82A87F20` mis-pinned into `CharClipDriver.s`; `scripts/dump_vtable.py` never selects the virtual-base primary vtable. Deliverable `docs/decomp/PLATFORMMGR_ESCALATION_2026-09-14.md`. Native gate PASS 18/18 skipped=0 (worktree and main; `src/` touched). |
| **W16-O** | opus | **+4 / +544 B** (43,212 → 43,216; 3,938,120 → 3,938,664 B; merge `4193d816f45b`). W16-N's four NOT-done items; two briefed diagnoses REFUTED on retail bytes. **(1) `fn_824C93C0` fold alias SPLIT**: the brief priced one alias at +3/+368 B; it is two claims at two levels of a recursive fold. List-level (`list<HamCamShot::Target>` vs `list<EventAnim::EventCall>`) T1-proven (4/4 reloc target names equal), installed in `symbol_aliases.json` group 899: **+2 / +260 B exactly as pre-registered**. Element-level **REFUSED** -- retail's element serializer is 72 B / 2 relocs, ours 316 B / 17: a real source divergence in `HamCamShot.cpp:452`, not a fold; the missing 108 B is that refusal. **(2) `ClassName@ReviewDisplay` RE-HOMED at Δ0** instead of the briefed −60 B surrender: bodies byte-identical, `.text` carve moved `0x8231E550` StarDisplay→ReviewDisplay, map row corrected from the fabricated `ForceEmit_*` spelling to `?ClassName@ReviewDisplay@@UBA?AVSymbol@@XZ`; zero `ForceEmit_*` rows remain. **(3) `RndTransformable::Replace` +1 / +124 B — briefed "`dynamic_cast` retail does not perform" REFUTED** (retail DOES `bl __RTDynamicCast`); real causes: `Hmx::Object` is a virtual base at +0xb8 so retail compares the pointee's Object subobject where `RefIs` compares the raw `Ptr()` (a live MI false-negative), and retail has no else-arm; v1 crossed mpn only, v2 fixed `==` operand order. **(4) `0x82403728` +1 / +160 B — briefed re-home to `rndobj/Dir.cpp` REFUTED**: `Anim.cpp:661` `#include`s `Dir.cpp`, so `Anim.obj` already defines the symbol and the pin is CORRECT (Dir.cpp is not in `objects.json`; the move would have made the row 0% forever). Real defect: `RndDir::Export` called `Hmx::Object::Export` across the virtual base instead of chaining to `MsgSource::Export`; the r28/r29 `REGISTER_SWAP` dissolved with the fix. Rowset: 5 in (592 B) / 1 out (48 B). Validators: ruler-agreement rc=0, objs-patched rc=0 (1043/1043), `icf_alias_finder --validate` PASS 0 CONTRADICTED / 1,630 groups. NOT done: element-level alias (not T1); `RefIs` itself (6 other call sites, separate change); the `HamCamShot::Target::operator<<` divergence; the **~15-site base-chaining vein** (`Hmx::Object::Replace`/`Export` called across a virtual-inheritance edge, `TexMovie.cpp:49` already flags one) -- do NOT sweep blind, item 3 = remove the arm, item 4 = retarget the arm, only the disassembly distinguishes. Tooling defect: `tools/w33_fold_adjudicate.py --self-test` is a NO-OP (declared, never referenced). Deliverable `docs/decomp/W16O_ALIAS_REVIEWDISPLAY_REPLACE_2026-09-14.md`. Native gate PASS 18/18 skipped=0 (worktree and main; `src/` touched). |
| **W16-P** | opus | **−2 / −80 B** (43,216 → 43,214; 3,938,664 → 3,938,584 B; merge `2f4c00c50c0b`). W16-M's five recorded-but-unactioned side findings, each decided on retail bytes; deliberately net-negative (removal of false credit, accuracy-beats-headline). **(1) `fn_82516320` = `SetDiskError` CONFIRMED and sharpened**: the 208 B `.pdata` extent is 1 + 6 + 45 instructions — a bare `blr` (8 callers, all `r3=0x82CC9D1C`, `r4∈{1,3}`), a 24 B out-of-line fragment of `DataSet` (`0x8275D670`, mutual branches), and 180 B of SetDiskError's OWN unreachable body tail bound to the record by its own EH `FuncInfo` (`0x82087B70`). Brief option (a) "empty the body" REFUTED (fits source to a link artifact, drives the row down); option (b) `fn_825162F0` REFUTED (a 40 B EH funclet with its own record). The only `.pdata` record of 57,733 that begins with `blr`. `PlatformMgr.h` offset `0x34` correct, citation re-derived from the ctor's reachable store. **(2) `stddef.h` `offsetof` mis-parenthesised CONFIRMED, fixed**; only call sites are curl `memdebug.c` behind `CURLDEBUG`, absent from `objects.json` and the ninja graph — Δ0 predicted and measured. **(3) `fn_823EC788`/`fn_823EC980` mis-pinned into `CharClipDriver.cpp` CONFIRMED on six lines** (311 KB from the unit's span; bracketed by `Server.cpp` blocks; `fn_823EC788` = `??_7XboxServer` slot 15 via `.rdata 0x82057978`; `fn_823EC840` tail-jumps into Quazal; added un-gated by `783ebf34`; dtk re-derived `.pdata` onto `Server.cpp`). Re-homed `0x823EC788`–`0x823ECD48` to `Server.cpp`: −2 / −80 B; `default/CharClipDriver` 25/46 → 21/23 rows. `fn_82A87F20` "also mis-pinned" REFUTED (a `bl` target, never a definition). **(4) `dump_vtable.py` never selects the virtual-base primary CONFIRMED, fixed**: anchored matching, `--which`, choice reporting, `--selftest` exiting 3 VACUOUS if its control stops failing (sabotage → rc=1 proven). Latent because the default obj is the dtk TARGET, which lists the primary first by accident. **(5) `fn_8251D378` = `PlatformMgr::Init` NOT named**: 1 of 3 callees resolved (`fn_82A6AC18` wraps `XOnlineStartupEx`, corroborated by `WSAStartup(2,&buf)`); brief's `XNetStartup` REFUTED (three int args, no `.text` ref to `__imp_NetDll_XNetStartup`, none of the three an import thunk). Lead: `0x8283Dxxx`–`0x8283Exxx` is a platform-wrapper TU — identify the TU. Process trap (twice): a split-guard rc=1 left `report.json` STALE and a set-diff read a confident Δ0 from it. NOT done: no `Server.h` virtual removed (19 vs 21 reproduced, which two undecidable); `PlatformMgr_Xbox.s` `lbl_8217E3B8` missing-COL uninvestigated; 23 re-homed rows left unnamed. Deliverable `docs/decomp/W16P_SETDISKERROR_OFFSETOF_MISPINS_VTABLE_2026-09-14.md`. Native gate PASS 18/18 skipped=0 (worktree and main; `src/` touched). |
| **W16-Q** | opus | **+5 / +648 B** (43,214 → 43,219; 3,938,584 → 3,939,232 B; merge `1aaf901eb254`). W16-O's NOT-done list adjudicated per site on retail bytes. **(1) Replace family — the blocker was IDENTIFICATION, not disassembly**: only `EventTrigger` had a map-named body; the rest had only vtordisp thunks, whose `b <body>` target IS the body address (control: EventTrigger's thunk target equals its map-named address). 8 sites adjudicated (`MatAnim`, `TransAnim`, `EnvAnim`, `LitAnim`, `Movie`, `CharWeightable`, `CharBonesMeshes`, `EventTrigger`), ALL verdict (a) — retail has no base-`Replace` call at all; 4 vbase/member offsets exact vs `/d1reportSingleClassLayout`. Source Δ0 as predicted; the 4 map names that followed paid **+4 / +540 B, predicted exactly**. 15 sites untouched: no reachable retail row. **(2) `RefIs` CONFIRMED** must compare the `Hmx::Object` subobject (7 retail addresses); fixed in `Object.h`, Δ0 on the arg-blind ruler, real bug. **(3) `0x824c93c0` — BOTH halves of the brief REFUTED**: its one charged site calls the already-named `operator<<(BinStream&, const EventAnim::EventCall&)`, so the body IS `list<EventCall>`; `.text` geometry and dtk's own `.pdata` re-derivation place it in `EventAnim`, not `BandCamShot`. `sizeof(Target)` cannot be derived from a linked-list walk in principle, and RB3's real `Target` serializer (`0x822b1b30`, 344 B) was already at fuzzy 100. Re-homed AND renamed: **+108 B exact / +1 fn (predicted +0, unexplained, stated as unconfirmed)**. **(4)** `--self-test` was a no-op, now wired to a control proven able to fail. **(5)** the 316 B COMDAT still emitted, now correctly unpaired. **W16-O's fold at `0x824c93c0` WITHDRAWN as `IDENTIFICATION_NOT_A_FOLD`**: retail-at-X ≡ our-COMDAT-for-N proves X IS N, not that N folded; the T1 instrument is one-sided and cannot separate the claims. Δ0 verified non-vacuous. NOT done: 15 unpairable sites; `EventTrigger::Replace` (636 B, 37.7) is a full rewrite; 3 scattered bodies need scatter-includes; `HamCamShot.cpp` scatter into `BandCamShot.cpp` not removed; **no other T1 alias group audited for the same confusion — the lane's own top follow-up**. Deliverable `docs/decomp/W16Q_BASECHAIN_REFIS_CAMSHOT_TARGET_2026-09-14.md`. Native gate PASS 18/18 skipped=0 (lane, worktree, main; `src/` + `Object.h` touched). |
| **W16-J** | fable | **+53 / +21,580 B** (43,219 → 43,272; 3,939,232 → 3,960,812 B; merge `ada7a2445d51`). Escalation of W16-D's (Opus) blocked list plus W16-I's item-4 refusal: 5 of 6 stop verdicts REFUTED or RESOLVED on retail bytes, every price a full-build set-diff of the `fuzzy == 100` row set. **(1) `MemFreeH` + `_MemAllocH` — "blocked on inline `MainThread`" REFUTED**: `/O1` emits the COMDAT out-of-line (31 of our objs; retail `0x824A4C10`), no header change needed; +2 / +192 B. Three oracle corrections neither sibling carries (`(MemAlloc)(sz,0x10)` parenthesised past the debug-arity macro; explicit `li r3,0` else-arm from placement-new on a `throw()` `PoolAlloc`; no `mUseHeapAlign` on RB3-360). **(2a) `DataResultList` family — "19-block grab-bag" REFUTED as stated**: the real defect was the DataResults TU sliced across 4 units; carved `band3/net_band/DataResults.cpp`; +35 / +3,132 B. **(2b) `JoypadPollCommon` HALF-RIGHT**: layout ported (2,468 B body 0 → 93.35), residual is regalloc/LICM shape, four probes inert; +1 / +4 B (`JoypadSendKeepAlive`). **(3) no-oracle bodies — TRUE but not a stop**: each read and classified, none collectable under 600 B; 0. **(4) 126 unpairable rows — mechanism RIGHT, size ≈ 0**: 121 rows / 26,728 B are CRT/XDK/XAPO (pins there = metric fitting); one exception collected — zlib `trees.c`/`inffast.c`/`inftrees.c` pinned + 3 fold-survivor rows renamed (+11 / +8,392 B) and a mis-carved `_tr_stored_block` merged and named (+1 / +392 B, `total_functions` −1 = phantom row corrected). **(5a) `Handle@OvershellSlot` (W16-I refusal) DECIDED**: retail compiled `SessionUsersProvider::ToggleMuteStatus` EMPTY (inline witness in retail's own `Handle@SessionUsersProvider`); source leg +1 / +112 B, then the 9,276 B row joins the `0x826c3888` blr survivor group +1 / +9,276 B. **(5b) `0x822dea78` RESOLVED**: it is `set<Symbol>::clear`, re-homed to BandProfile and renamed in one commit; +1 / +80 B. Native gate FAILED twice on the lane's own source (2-arg `MemAlloc` has no `HX_NATIVE` overload; `JoypadPollCommon`'s `extern "C"` back-end callees unstubbed), fixed, then PASS 18/18 skipped=0. Ten traps recorded (aliases vs map `ensure_ascii` differ; a `.pdata`-moving splits edit fails its FIRST build by design and leaves `report.json` STALE; a Python walk over the aliases printed NOTHING where `grep -a` found 3). NOT done: `JoypadPollCommon` not crossed (permuter OFF); `FriendsProvider.h` `vector<Friend*>` fix unmeasurable; `?ReleaseAutoRelease@DxRnd@@` unpinned; `0x827BBA68` deliberately unnamed; `0x82529ae8` naming family unaudited; `SetDiskError`/`0x82516320` left to W16-P as a `.pdata` mis-carve. LANDING REPAIR: the first rebased build read +50 / +17,852 B; a set-diff of fuzzy==100 rows attributed the 3,728 B shortfall EXACTLY to two alias groups that had lost memberships main added after the lane's base `a8f9e92b` (W16-L's two PlatformMgr FT-EMPTY members on `0x826c3888`, 3,112 B; W16-I's `set<TrackWidget*>::clear` on `0x822dea78`, 616 B). The lane never removed them -- the rebase kit's alias resolver replaced a both-sides-changed group with the lane's copy and read the lane's survivor rename as delete+add. Restored in follow-up `dcd8d6fe`; resolver rewritten as a field-wise three-way merge with a replay self-test (v1 fails it, v2 passes). OPEN LEAD exposed by the landing, not fixed: naming `0x8250b510` `??1DataResultList@@UAA@XZ` turned two forgiven thunk sites into checked ones -- MetaPerformer `fn_825800B0` destroys a `Message` at +0x84 and RockCentral `fn_824F9BAC` a `DataArrayPtr` at +0x50 where retail destroys a DataResultList (80 B / 2 funclets): probable wrong local types in our source. Deliverable `docs/decomp/W16D_BLOCKED_LIST_ESCALATION_2026-09-14.md`. |
| **W16-S** | opus | **+0 / +0 B** (43,272 → 43,272; 3,960,812 → 3,960,812 B; merge `93fa4303670f`). T1 alias sweep, measured by ABLATION not census: 5,315 memberships → FOLD_CONFIRMED 3,636 (−798,520 B if ablated) · UNDECIDED_MASKED 643 (a class the brief lacked) · IDENTIFICATION_NOT_A_FOLD 175 (98 groups; 113 could re-pair after a re-home, 95 would go permanently 0% — NOT re-homed, ranked worklist in `docs/decomp/W16S_alias_census_2026-09-14.json`) · CONTRADICTED_ON_RETAIL 16 (9 WEAK: closure fails closed on an undefined callee) · stale 381 = 273 dead + 108 LIVE (−8,936 B; the null FAILED — liveness is reference, not definition). ★ Name-identity is STRICTER than `/OPT:ICF`, which is a fixed point: partition refinement took contradictions 118 → 16 and would have closed 100+ veins if shipped strict. One vtordisp-thunk fold withdrawn (`0x8280cb08`, retail branches to `Export@RndDir`, ours to `Replace@RndTransformable`), predicted Δ0 / measured Δ0; five withdrawals REVERTED when the diff surfaced VT1 vtable-geometry proofs outranking the instrument. Four instrument defects fixed first (funclet billed into COMDAT body, −44 B one-sided). Deliverable `docs/decomp/W16S_T1_ALIAS_IDENTIFICATION_VS_FOLD_2026-09-14.md`. |
| **W16-R** | **fable** (escalation) | **+0 / +0 B** (43,272 → 43,272; 3,960,812 → 3,960,812 B; merge `a28569536fd3`). SetDiskError `0x82516320` cave: **REFUTED as TU5** — our `default.xex` is byte-identical to `RB3DX-Xbox/default.xex` and differs from a genuine clean TU5 by exactly 53 words / 10 patch groups at the SAME PE timestamp `0x4E60C7FE` (no relink; the cave is RB3DX's in-place hook, groups 4+5; TU0 has neither edit); class bounded EXACTLY at 3 cross-extent non-entry branches (clean−ours = 0), label `RB3DX-hook`, both rows recorded structurally unmatchable against this image. `Server.h` 21 vs retail 19 slots: bound firm (max receiver vcall offset 0x38, 204-site census), surplus pair NOT proven (5 candidates, `GetMasterProfileID` favoured) — header deliberately untouched; slot 15 = `GetSecureConnectionClient`. Pinned `xdk/xapilibi/sleep.cpp` + `xutilitydrive.cpp` (source-less) and named 7 rows incl. `?Init@PlatformMgr@@QAAXXZ` (3 callees resolved via XEX import ordinals + DC3 `sleep.obj`), predicted Δ0 rows, measured 0/0; refuted its own prediction by +0.00094 pp fuzzy — `?Init@WinSockSocket@@SAXXZ` newly pairs at 95.0 (we DO define it; 104 vs 100 B residue, not fixed). `lbl_8217E3B8` = a real 2-slot vtable in the Quazal `/Od` region, class unnamed. Withdrew its own draft claim that `config.yml`'s hash is wrong. Deliverable `docs/decomp/W16R_SETDISKERROR_CAVE_SERVER_VTABLE_2026-09-14.md`. |
| **W16-T** | opus | **+8 / +620 B** (43,272 → 43,280; 3,960,812 → 3,961,432 B; merge `aa20b995bcb5`). DataResultList funclet lead: **REFUTED on retail bytes** — both parents (`fn_82580068` dtor, `fn_824F9B68`) construct/destroy the same types at the same frame slots retail does, and our objs carry funclets with retail's exact bytes and callee; the charge is objdiff pairing funclets by BYTE SIGNATURE (many-to-many across relocation-only twins) and picking the wrong one — class sized **3,366 rows / 204,884 B (2.00 pp)**, 40 B subset 1,842 / 73,680 B; fix = relocation-name tiebreaker in `../objdiff` (tooling lane), source-unreachable; retyping / alias / un-naming all refused. FriendsProvider `std::vector<int>` → `std::vector<Friend*>` (DeleteAll instantiation + dtor `srawi 2`/`slwi 2`), ctor/dtor/Reload written, three-carve pinned with the scalar-deleting dtor pulled off `BandLabel`'s head: predicted +3, measured **+9 rows / +520 B** (+6 fns; overshoot = 4 EH funclets + `UIList::fn_827F8DA4` via funclet reassignment), 0 fell out. `0x82529ae8` is a 4 B `b fn_82532378` into an `XINPUT_VIBRATION{WORD,WORD}` rumble setter `(int, WORD, WORD)` — false `ReceiveUpstreamAccelerometerResponse` name WITHDRAWN, no replacement invented, Δ0. `?Release@VertexBufferData@DxMesh@@` (68 B) + `??0Shuttle@@QAA@XZ` (32 B) were unresolved externals, collected byte-exact (+2 fns / +100 B; `Rnd_Xbox.cpp:983` scatter-includes `rnddx9/Mesh.cpp`). `fn_82654440` (76 B) read in full (tail-calls `PlatformMgr::ShowGamercard`) but left anonymous — no oracle spelling. Deliverable `docs/decomp/W16T_DATARESULTLIST_LEAD_FRIENDSPROVIDER_2026-09-14.md`. |
| **W16-U** | opus | **+9 / +1,048 B** (43,280 → 43,289; 3,961,432 → 3,962,480 B; merge `5b9f31dc00bc`). Item 1: the four VT1 addresses are 12-byte vtordisp adjustor thunks whose first 8 bytes are identical across all 1,806 in retail, so the fold is decided by the branch DESTINATION — fixes went into the destination bodies (`CharWeightable::Handle` 268→204 B, `RndLightAnim::SyncProperty` 392→120 B, `CharTransDraw::Save` 128→124 B, `BandDirector::Replace` 112→4 B); measured 0 fns / −40 B, the −40 being `default/Waypoint::fn_823DCC54`, an anonymous EH funclet whose byte-signature pairing shifted (W16-W's objdiff artifact, not a wrong body). Item 2: all 9 WEAK rows share one mechanism — retail's callee is defined, ours is not, so W16-S's closure fails closed; 3 proven folds recorded in evidence (`??_GUIPanel` vs `??_EUIPanel` is MSVC naming the vtordisp adjustor with the vector-deleting spelling while the body reached is the scalar `??_G` we define); 2 REAL divergences flagged NOT withdrawn (`??_GGemTrainerLoopPanel`, `??_GTourChallengeResultsPanel` 80 B vs retail 68 B — ours route through a `??_D` vbase dtor; a size argument, so left pending a two-sided normalization); 4 not actionable. Item 3: 10 map rows re-homed to the spelling whose COMDAT equals the retail body, prediction PRE-REGISTERED at +9 / +1,088 and measured EXACT; `icf_alias_finder --validate` PASS. Item 4 DECLINED with evidence (needs a scatter-include of a whole TU — the `mtx.cpp`/`TexRenderer` native-link breakage shape — for 500 B; W16-Q's `CharBonesMeshes` 216 B entry is stale, now fuzzy 100). Landed +9 fns / +1,048 B (10 in / 1 out). NOT done: alias role edits for item 1 (fixes are in source), `0x827f42a8`, gi=24 at `0x822dea78`, the other 164 IDENTIFICATION_NOT_A_FOLD rows (would un-pair). Deliverable `docs/decomp/W16U_VT1_DIVERGENCES_WEAK_CONTRADICTIONS_REHOME_2026-09-14.md`. |
| **W16-V** | opus | **+1 / +100 B** (43,289 → 43,290; 3,962,480 → 3,962,580 B; merge `93283bb41348`). Item 1: retail `?Init@WinSockSocket@@SAXXZ` (100 B) writes exactly one field after the inlined memset — no store to `0x51(r1)`, so `params.cfgFlags = 1` was a DC3-newer inheritance; one line deleted, row crossed, and a pre-existing r9↔r10 swap DISSOLVED (the `REGISTER_SWAP` label was a symptom). Item 2: seven xapilib pins proven WITHOUT a body twin — the DC3 route FAILED (retail links XAPILIB 2.0.11164.0, DC3 a newer XDK) and dispatch-table isomorphism (`_fileIoHooks`), the PE entry point (`config.yml`'s own header already carried it) and import-ordinal decode carried it; five were already in the map under the exact derived names (5-for-5 control). Item 3: `0x82270E68` re-homed RhythmDetector → App.cpp on 18 string refs + byte geometry, Δ0; nine `masked_equal` siblings at 99–100% are NO evidence of TU membership. Item 4: `ReleaseAutoRelease` + 2 EH funclets pinned to Rnd_Xbox on a DC3 twin; the remaining 8 rows re-carve as `auto_03_8273CEF0_text` and stay unproven. Item 5: landing kit is `tools/rebase_kit/` with the W16-J replay as a test (v2 = 0 memberships lost, frozen v1 = 3 lost, reproducing −3,728 B). Item 6: CLAUDE.md "vanilla retail XEX" → RB3DX-lineage TU5. Flagged, not fixed: `pytest tools` 2 pre-existing failures — `symbol_aliases.json` group 147 (`0x823c3ac8`, 0 folded / 86 withdrawn) carries the same survivor as group 1480 (`0x823d14c0`) and the map has NO name at `0x823c3ac8`: an orphan group forgiving nothing. Landed +1 / +100 B, set-diff crossed 1 / fell 0, same ruler (4.2.8 `14ac591a`). | `docs/decomp/W16V_XAPILIB_GAPS_WINSOCK_INIT_REBASE_KIT_2026-09-14.md` |
| **W16-W** | opus | **RULER CHANGE, +1 / +24,392 B from the ruler alone** (43,290 → 43,291; 3,962,580 → 3,986,972 B; merge `a272c5d8`, measured at `fe5ed8b3` = same code as `93283bb4`, objdiff 4.2.8 `14ac591a` → 4.2.9 `5a51cd51`). NOT additive source work: funclet pairing pass 2/2b now breaks byte-signature ties on relocation-target-NAME agreement (`NameEvidence {agree, conflict}` over un-gated `RelocDesc`/`NamedSig`); MetaPerformer class 1 retail/4 ours, RockCentral 5↔5 are the motivating classes. Set-diff vs the 4.2.8 row set: 623 rows / 25,032 B crossed in, 16 rows / 640 B fell out (all 40 B reshuffles), **zero named rows moved** — every mover is an anonymous `fn_8…` funclet; the lane predicted +1 / +24,352 B at `11fc1234` (622 in / 16 out), main differs by exactly one extra 40 B anonymous funclet crossing on a base that also carries W16-V. objdiff `main` at `21f3c57` (tip `a5f0ea9`) is built and live but NOT pushed; 4.2.8 backups kept byte-verified (`~/tmp/objdiff-backup/objdiff-cli.4.2.8-a5c35b15`). Other sessions' trees (dc3-decomp, rb3 Wii) see 4.2.9 on their next report: dc3 expected +3,876 B, rb3 Wii 0. cargo test 135/0; sabotage control verified. Gates at `fe5ed8b3` under 4.2.9: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0. ⚠ Deltas across this row do NOT compose with 4.2.8 rows. | `docs/decomp/RULER_CHANGE_funclet_name_tiebreak_2026-09-14.md` |
| **W16-X** | opus | **+2 / +412 B** (43,291 → 43,293; 3,986,972 → 3,987,384 B; merge `352ba26c8d09`). Item 1: the 80 B vs 68 B `??_G` divergence is NOT the override set — it is the mere presence of a user-declared destructor: `virtual ~X() {}` forces the vptr-restore prologue into `??1X`, which cascades `??_D` 56 → 112 and `??_G` 68 → 80. Five UIPanel subclasses carried one retail did not (FadePanel, GemTrainerPanel, CampaignSongInfoPanel, MultiSelectListPanel, TourChallengeResultsPanel — removed); ConnectionStatusPanel is the control that fires and was LEFT (retail has the user dtor). `UIButton.h` `OBJ_MEM_OVERLOAD` → `OBJ_MEM_OVERLOAD_INLINE_DEL`. Item 2: `tools/twosided_extent.py` (both-sides size reader, EH funclets excluded on both, selftest that can fail); `0x827f42a8` is a 12 B thunk with NO `.pdata` row, so no size claim is licensed either way — left as-is; W16-S's 68 SIZE rows adjudicate 44 real / 24 unlicensed / 0 reader artifacts. Item 3: fold-sweep re-run admits 652, **0 funclet-bearing** — the brief's premise (coverage grows after the funclet fix) is refuted; 2 alias groups installed with T1 proofs. Item 4 not started. Lane predicted +1 / +324 B at `11fc1234`; main measured +2 / +412 B — the extra row is `??_GMultiSelectListPanel@@UAAPAXI@Z` (+88 B), the lane's OWN pre-registered prediction that did not cross in its worktree and does on the rebased tree (cause not isolated; base differs by W16-V + the 4.2.9 ruler). Set-diff: 2 in (`CharEyes::vector<CharInterestState>::_M_insert_overflow_aux` +324, `??_GMultiSelectListPanel` +88), 0 out. Gates at `352ba26c` under 4.2.9: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0. | `docs/decomp/W16X_VBASE_DTOR_DIVERGENCE_TWO_SIDED_SIZE_2026-09-14.md` |
| **W16-Y** | opus | **+1 / +100 B** (43,293 → 43,294; 3,987,384 → 3,987,484 B; merge `505d2bff934b`). Item 1: orphan alias group 147 was a wrong IDENTIFICATION, not a dead group — its survivor `list<Hmx::Object*>::insert` names a body retail placed at `list<ConstraintSystem>::insert` (CharBlendBone), and the duplicate survivor was silently denying two of group 1480's live CF2 memberships; 147 merged into 1480 (84 `withdrawn` records carried verbatim, 2 → `restored`). Item 2: the 109 null `target_symbol_map.json` keys are TOMBSTONES (deliberate un-namings), not corruption — 4 consumers guarded, none deleted. Item 3: naming `0x823c3ac8` exposed a WRONG name at `0x823c4188` (`list<PresetOverride>` range ctor → `list<ConstraintSystem>` range ctor), financed by its caller — corrected, the mis-named row falls out as predicted. `fn_82654440`: no witness found, left anonymous (deliberate deferral, not can't-fix). W16-T caller correction: `0x825d9034` sits inside anonymous `0x825d8fd0`. Landed exactly as self-reported: 2 in / 216 B (ConstraintSystem `insert` +100, range ctor +116), 1 out / 116 B (the PresetOverride-spelled range ctor). `pytest tools` 326 passed / 0 failed. Gates at `505d2bff` under 4.2.9: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0. | `docs/decomp/W16Y_ALIAS_ORPHAN_147_NULL_MAP_KEYS_FN82654440_2026-09-14.md` |
| **W16-Z** | opus | **+7 / +1,324 B** (43,294 → 43,301; 3,987,484 → 3,988,808 B; merge `c6552cfa8772`). Item 1: the brief's layout premise was WRONG — the compiler's layout report says our `DxRnd` is already retail-shaped at every offset these bodies touch (`0x1c4` `mD3DDevice`, `0x2a4`/`0x2b0` pending vectors, `0x398`/`0x39c`); the `0x1c4`-vs-`0x224` gap is DC3-vs-retail, so no layout edit. `ReleaseAutoRelease` (652 B) ported with one retail-driven correction (1-arg `PhysicalFree` = an ICF fold of the tracked overload) and crossed via 5 relocation-name charges each adjudicated on retail bytes (`icf_pair_adjudicate.py`, controls first); the `PhysicalFree` pair REFUTED on size 84 vs 76 exposed a real source defect — `Memory_Xbox.cpp` was missing `MemTrackFree(address)`. Item 2: `0x82E04FFC` is the MSVC local-static guard for `static Timer *cpuTimer` in `BeginDrawing` (`fn_8273D07C` = its unwind funclet); `BeginDrawing` (396 B) written retail-shaped, not DC3-shaped (two witnessed corrections: `MakeColor` packs alpha where retail loads 3 floats; `SetShaderRegisterAlloc` case 1 emits `li 0x20/0x60`, not members), 89.23 → 100. 4 of 8 `auto_03_8273CEF0` rows pinned; `DoPointTests` paired at 75.10 and NOT closed — layout verified correct, residual is body shape (prologue r16–r31 vs r19–r31, frame −0x20, 201 charged sites) — escalation candidate; 4 rows / 356 B left in `auto_*` (no witness). Landed exactly as self-reported: 7 in / 1,324 B (`ReleaseAutoRelease` 652, `BeginDrawing` 396, `ShaderMgr::AutoRelease` 92 ripple, `DoWorldEnd` 72, two 40 B EH funclets, `fn_8273D07C` 32 masked_equal), 0 out. Gates at `c6552cfa` under 4.2.9: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0. | `docs/decomp/W16Z_RELEASEAUTORELEASE_BODY_AUTO03_REMAINDER_2026-09-14.md` |
| **W16-AA** | opus | **+1 / +72 B** (43,301 → 43,302; 3,988,808 → 3,988,880 B; merge `7e6b2498b2e7`). W16-X Item 4 taken up: the `UNDECIDED_MASKED` alias class (643 memberships / 41,360 B) top-10 by forgiven bytes holds **zero wrong callees** — every row adjudicable on retail bytes confirms its fold. Mechanism: `w16s_alias_census.py` computes its ICF closure on OUR build while the fold happened in RETAIL's, so where retail folded the callees and our `/O1` objs do not, the closure under-merges and the parent is filed UNDECIDED — an instrument reporting gap, not an alias-file exposure (channel census: 636/643 = 40,332 B closable by naming, 6 structurally not). One map row landed: `0x823c3960` named `list<CharBlendBone::ConstraintSystem>::_M_create_node` (W16-Y's node creator), predicted +1 / +72 B / 0 fall-out, measured exactly. Three instruments in `tools/w16aa_*.py`. NOT done: no withdrawals (gi=95 `HamCamShot` fold claim is literally false but forgives nothing live); three `operator<<` addresses identified but not named (our bodies larger than retail, no row could cross); Accomplishment comparator swap refused 3/3 by size. Gates at `7e6b2498` under 4.2.9: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0. | `docs/decomp/W16AA_UNDECIDED_MASKED_TOP10_LISTNODE_0x823c3960_2026-09-14.md` |
| **W16-AB** | opus | **+7 / +744 B** (43,302 → 43,309; 3,988,880 → 3,989,624 B; merge `120d205ea53f`). Brief: write three "stub" bodies whose two-sided sizes disagreed with retail, adjudicate the 44 REAL size rows + 24 NO_PDATA rows. **Premise REFUTED on retail bytes**: in 18/18 addresses our source is correct and the MAP names the wrong retail address, with the alias table forgiving the divergence (rows advertised 74–96% near-misses that could never close). ⇒ **`survivor_vs_retail == SIZE` is a WRONG-MAP-NAME DETECTOR**, not an our-side defect detector — 40/40 memberships have the *folded* spelling's size == retail exactly, only the survivor disagrees, so each group already carries its correct spelling. Item 1: three identifications repaired (+3 / +352 B) incl. `0x82553fc8` = `??1CharCreatorPrefab@PrefabMgr@@QAA@XZ` (unmasked 96 B identity, a 96 B island carved out of PrefabMgr.cpp's range and pinned to Mat.cpp — a circular pin); its W15 `_denylist` entry LIFTED with the rationale kept, and the lane found the refuted `?Terminate@RndMat` binding had **never left the applied map** (denylist governs auto-emission only). Item 2: four exact-size island re-homings in `splits.txt` (+4 / +392 B); `vector<T>::erase` for trivially-copyable T is reloc-free and genuinely folds across 21 instantiations. Item 3: two NO_PDATA memberships CONTRADICTED on bytes, withdrawn with records (Δ0). Predicted exactly at every stage, 0 rows out; **reproduced exactly on main**. NOT done → **W16-AE (fable) escalation**: `0x823eb548` uniquely byte-proven as the 16 B vtordisp thunk `?Replace@MsgSource@@$4PPPPPPPM@BI@AAXPAVObjRef@@PAVObject@Hmx@@@Z` but no pinned unit at that address defines the spelling (rename ⇒ 0%, re-home ⇒ circular pin); 5 arbitrary-bijection + 6 span-carving addresses; `0x827f42a8` undecided (16 vs 12 B = dtk carve padding, STLPORT-1 shape — correctly NOT withdrawn); two tooling gaps (`_denylist` never polices the applied map; `icf_alias_build.py` emitted two T1 claims its own adjudicator refutes). Gates at `120d205e` under 4.2.9: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0. | `docs/decomp/W16AB_SIZE_MISMATCH_44_STUB_BODIES_NOPDATA24_2026-09-14.md` |
| **W16-AD** | opus | **+6 / +680 B** (43,309 → 43,315; 3,989,624 → 3,990,304 B; merge `14ac3de1c355`). W16-AA's successor: the RETAIL-SIDE FOLD WITNESS, mechanised over all **643** UNDECIDED_MASKED alias memberships (`tools/w16ad_fold_witness.py`; rows sum 643 / 41,360 B, W16-AA's population table reproduced exactly). Partition: **NO_WITNESS_FOLDED_SIDE 496 / 30,864 B** (absence ≠ refutation, left in place), **WITNESS_REFUTED 63 / 3,828 B**, INCONCLUSIVE 52, CONFIRMED 23, PARTIAL 4, CHANNEL_B 4, ALREADY_NAMED 1. **20 refuted memberships withdrawn** (batch cap) with `withdrawn` records — predicted Δ0, measured Δ0/Δ0, vacuity checked (74 call sites not in report rows). **4 Accomplishment algorithm-instantiation map rows un-swapped** (`AccomplishmentCmp` ↔ `AccomplishmentCategoryCmp`: `merge` / `__upper_bound` / `__unguarded_linear_insert` / `__merge_without_buffer`): pre-registered +1,088 B from 7 rows / −408 B from 1 (`__merge_adaptive`), net **+680 B / +6**, measured identical row for row. Three own-instrument defects found and fixed: the census `gi` was NOT an index into `aliases['groups']` (503 of 5,315 correct — and `tools/w16s_ablate.py` had the same defect, so it was a silent no-op and past mis-keyed ablations under-report); the simulation held the caller address fixed; the simulation charged placeholder destinations. Guard audit: 63 is a lower bound, 70 / 4,320 B strict ceiling (7 rows guard-blocked, 0 genuine twins). Self-test honest: hand_reproduced 2/6, positive control PASS, negative control FIRED — tool not tuned. NOT done: the 496 NO_WITNESS, 43 refuted beyond cap, the 3-way Accomplishment rotation needing `0x825f32a8` (176 B, unnamed) identified + cross-unit re-homing → map lane. Gates at `14ac3de1` under 4.2.9: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0. | `docs/decomp/W16AD_UNDECIDED_MASKED_RETAIL_FOLD_WITNESS_643_2026-09-14.md` |
| **W16-AC** | fable | **+4 / +356 B** (43,315 → 43,319; 3,990,304 → 3,990,660 B; merge `8ea97724e443`). `?DoPointTests@DxRnd@@` (1,392 B, `default/Rnd_Xbox`) **75.10 → 99.87 fuzzy / mpn 99.98563** over seven kept variants: V1 DC3-tuned body + retail corrections (the "prologue → r16" prediction FAILED); V2+V3 inlined `CreateAndBeginQuery`/`EndQueryFrame`/`BeginQueryFrame`, dropped DC3's `HiResScreen` early-out; **V4 `resize(size(), RndPointTest())` right-to-left + loop store order — the prologue moved to r16–r31 on its own, no explicit hoist was the lever**; V6 `vtx`/`verts` at function scope (frame −0x210 and slot map matched); V7 flare read back through `test.mFlare`. V5 / V8 / V9 regressed and were reverted (recorded, not committed). Residual is source-unreachable: idx 97 `bl _M_fill_insert<MidiParser::Note>` vs our `<RndPointTest>` (the fold-alias W16-AB withdrew on evidence — different `T`) and an r22↔r23 callee-saved swap at 8 sites (two probes both regressed). The row is all-or-nothing, so every variant measured **Δ0** whole-binary. **Item 2 crossed all four**: wired `system/rnddx9/Utl.cpp`, pinned `.text 0x8273D658–0x8273D7C8` (dtk back-filled `.pdata`), named the four rows — **Δ0 reattribution, measured exactly** — then removed DC3's four `DX_ASSERT`s (retail's `Make*` are bare `b D3DDevice_Create*Buffer` tail calls, `Clone*` go straight into the `BufLock` ctors): `CloneIndexBuffer` 160 / `CloneVertexBuffer` 156 / `MakeVertexBuffer` 20 / `MakeIndexBuffer` 20 = **+4 / +356 B, predicted exactly, 0 out**. NOT done: `BufLock<>` ctors at `0x82737548`/`0x827375E8` identified but not map-named (bug-exposure bet; callers `RenderState`/`Mesh` outside the lane → map lane); the 836 B unnamed `fn_8273B818`; the `mtx.cpp` TU-include hypothesis (cascades to every rnddx9 unit). Gates at `8ea97724` under 4.2.9: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0. | `docs/decomp/W16AC_DOPOINTTESTS_BODY_SHAPE_AUTO03_CALLEES_2026-09-14.md` |
| **W16-AG** | opus | **Δ0 / Δ0 B** (43,319 → 43,319; 3,990,660 → 3,990,660 B; merge `2473fb6b`), pre-registered Δ0 and measured Δ0 on all four batches. Withdrew the **51** refuted alias memberships W16-AD proved on retail bytes but did not land: the 43 `WITNESS_REFUTED` rows (2,580 B; the lane re-ran the whole 643-row witness itself, 0 disagreements, selftest positive PASS / negative FIRED) plus the guard-blocked population, which is **8 memberships / 572 B, not 7 / 492 B** (gi=942 carries two folded spellings at one `(survivor, address)` — the same "gi is not a key" defect AD's §4.2 documented, resurfacing in its own §6). The guard was a **false refusal 8/8** under the true criterion (identical *including call targets*): seven differ in an `addis`/`addi` pair materialising a different absolute, one in a real `bl` target; control: the strict twin test returns TWIN on 1,529 genuine groups. `tools/w16ad_predict_withdrawal.py` is structurally unable to predict (indexes `groups[gi]`; 0 of 63 rows have `gi` as a true index) — its Δ0 measurement stands, and `tools/w16ag_predict_withdrawal.py` replaces it keyed on `(survivor, address)`. An all-row snapshot shows batch A moved exactly one already-sub-100 row by a fractional `diff_arg` penalty and B/C/D moved 0 of 69,216 rows ⇒ no withdrawal was hiding a paid-for wrong callee. `symbol_aliases.json`: 1,634 groups unchanged, folded 5,294 → 5,243, 51 `withdrawn` records, 12 groups touched, nothing pruned; `icf_alias_finder --validate` 0 CONTRADICTED. Not done: 496 `NO_WITNESS` rows (absence ≠ refutation — settleable by ONE map identification of any EQ caller of the folded spelling), 44 `WITNESS_INCONCLUSIVE`, Accomplishment rows (W16-AF). Gates at `2473fb6b` under 4.2.9: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0. | `docs/decomp/W16AG_REFUTED_43_WITHDRAWALS_GUARD7_2026-09-14.md` |
| **W16-AF** | opus | **+32 / +6,016 B** (43,319 → 43,351; 3,990,660 → 3,996,676 B; merge `5b70c9ebe389`). Pre-registered +5,948 B / 33 in / 0 out; measured **35 in / 6,196 B, 3 out / 180 B, net +6,016 B** (error +68 B, 1.1%) — the three fall-outs (`__chunk_insertion_sort<GoalAlpaCmp>` 108 B, `fn_825F4FC4` 40 B, `fn_825F3358` 32 B) are **re-homes** that `rowset_snapshot` keys by `unit/name` and so double-counts; each sits at `fuzzy==100` under `CampaignGoalsLeaderboardChoicePanel`, so real content is 32 rows / 6,016 B with zero regressions. Identified **`0x825f32a8` = `??RGoalCmp@@QBA_NVSymbol@@0@Z`** (`GoalCmp::operator()`) on six lines of retail-byte evidence: function-local static `Symbol("campaign_metascore")` behind the guard at `0x82E00398`; fan-in 7, all STL algorithm instantiations; `0x825f45f0` already named `__merge_without_buffer<GoalCmp>`; connected-component closure contains `CampaignGoalsLeaderboardChoiceProvider::ctor`; 15 algorithm `.pdata` extents == our `<GoalCmp>` COMDAT sizes; and the rows read 100 only through placeholder forgiveness. Rejected the three Accomplishment comparators and `GoalAlpaCmp` (40 B). Scope grew from 3 rows to **56 map names** (five comparator families × fifteen STL algorithms across two bands; 13 new addresses, 43 renamed, algorithm half untouched on every row) plus 6 `.text` blocks carved out and 8 added under `CampaignGoalsLeaderboardChoicePanel.cpp`. **W16-AD corrected:** comparator `.pdata` extents do NOT discriminate by size — `AccomplishmentCategoryCmp` and `AccomplishmentGroupCmp` are both 112 B and the map had them **swapped** (`0x825f6f20` → Group, `0x825f6f90` → Category); fixed. Instrument defect caught in-lane: sizing from OUR COMDATs bills the successor's EH funclet (156 B vs retail 116 B — the STLPORT-1 hazard); re-derived from retail `.pdata`, 75/75 agree. No `src/` and no `symbol_aliases.json` edits. Not done: our `GoalCmp::operator()` uses the extern global `campaign_metascore` where retail uses a function-local static (ours 144 B vs retail 176 B) — a `src/` follow-up worth ~+176 B. Gates at `5b70c9eb` under 4.2.9: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0. | `docs/decomp/W16AF_ACCOMPLISHMENT_ROTATION_0x825f32a8_2026-09-14.md` |
| **W16-AI** | opus | **+5 / +424 B** (43,351 → 43,356; 3,996,676 → 3,997,100 B; merge `0667441bf2c9`). Pre-registered +176 B / +1 (item 1) then +248 B / +4 (item 2); measured **5 in / 424 B, 0 out, net +424 B**, every row predicted exactly. **The rb3-Wii oracle is WRONG for `GoalCmp::operator()`** — it uses the extern `campaign_metascore` character-identical to our port; retail materialises a function-local static (guard `0x82E00398`, storage `0x82E00394`, literal at `0x820A32E8`). A lane that "ported from the oracle, confirmed identical" would have called the row unfixable. `fn_825F4258`/`fn_825F4268` are `UIPanel` vtordisp thunks (`DataDir`, `SetTypeDef`). `fn_825F57C0`+`fn_825F5854` were a **misplaced TU boundary**: RTTI on both stored vtables reads `CampaignSongInfoPanel`, vbase `+0x44` not `+0x5c`, our COMDAT 224 B = 148+68+8, and `CampaignSongInfoPanel.cpp`'s first block already began at `0x825F5898` — moved, ctor named. Unit 44/50 → 47/48 rows, 5,284/5,756 → 5,492/5,540 B. Left as escalation input: `fn_825F5244` (40 B, fuzzy 99.5, one reloc-name charge — retail tail-calls `0x826101B8` whose thunk branches to unnamed `0x826100F8`; ours calls `??1?$hashtable@…`; one identification decides map-repair vs real container divergence) and `fn_825F4288` (8 B `WDM@` adjustor, two indistinguishable candidates). No alias edits. Gates at `0667441b` under 4.2.9: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0. | `docs/decomp/W16AI_CAMPAIGNGOALS_GOALCMP_LOCAL_STATIC_AND_UNIT_COMPLETION_2026-09-14.md` |
| **W16-AH** | opus | **+0 / +0 B** (43,356 → 43,356; 3,997,100 → 3,997,100 B; merge `24494d6a41bf`). Pre-registered Δ0 and measured Δ0 on every wave, by design — this lane bought accuracy, not bytes. Took up W16-AD's **`NO_WITNESS_FOLDED_SIDE` 492** memberships (30,140 B after W16-AG's 4) and adjudicated them by **type existence** (`tools/w16ah_type_existence.py`): a membership whose mangled class exists nowhere in our source or in retail RTTI is forgiveness resting on nothing. **294 withdrawn / 17,984 B** in two waves (`w16ah_build_selection.py`, `w16ah_apply_withdrawals.py`), each a `withdrawn` record, no group pruned: groups 1,634 → 1,634, folded 5,243 → 4,949, withdrawn 10,164 → 10,458. Two controls kept the oracle from confirming itself — `Quazal::Job` (descriptor absent, type certainly linked: declined) and Bink (rb3-Wii's zero is a port artifact: declined). **Route B refuted on retail bytes**: the map row `0x823c8cf0 → ??_DCharTransDraw@@QAAXXZ` measured Δ0 and moved one witness row 496 → 495 but turned `--validate` FATAL, because retail `??_GCharTransDraw` branches at +0x20 to `0x823c8cf0` and NOT to the survivor `0x82824a58` — two bodies, not one fold; the map row stays (it names a real function), the alias membership is withdrawn. Validator after: PASS, 1,387 map-consistent / 246 tolerated / 0 contradicted. Not done: 184 residual rows whose TUs cannot pair yet, 29 non-polymorphic rows, 3 Bink/Fx + 1 `Quazal::Job` control rows, **14 HELD `ObjPtrVec`-survivor rows (a map question)**, the `gi`-ablation re-measure. Rebase hit one conflict round on `target_symbol_map.json` (`58d3e712`); the resolution verified by JSON decomposition — all 29,412 main keys incl. W16-AI's three rows survive plus AH's one. Gates at `24494d6a` under 4.2.9, worktree and main line-identical: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0, 0 rows in / 0 out. | `docs/decomp/W16AH_NO_WITNESS_492_TYPE_EXISTENCE_AND_ONE_CALLER_2026-09-14.md` |
| **W16-AJ** | fable (escalation) | **+17 / +2,812 B** (43,356 → 43,373; 3,997,100 → 3,999,912 B; merge `c62ce4e077b4`). Fable escalation of W16-AI's one unresolved identification. **`0x826100F8` identified**: the 104 B ICF survivor of **26** reloc-identical `stlpmtx_std::hashtable<pair<const K,V>>::~hashtable` instantiations (every `hash_map<K, trivially-destructible V>`), with `0x826101B8` the 4 B `~hash_map` thunk survivor of 27 spellings — the old map name `??1?$map@HPAVUIComponent…` was wrong in kind, not spelling. Only the thunk rename landed (`7c80d49d`, **+180 B / +1 fn** measured, brief predicted +40); naming `0x826100F8` alone measured **−2,908 B / −18 fns** (`b1dae30d`, withdrawn) because it must land together with an alias group (proposal in the doc's §6). `fn_825F4288` shown distinguishable (a `ContentMgr::Callback` thunk referenced only from the CampaignGoalsLeaderboardChoicePanel and TourDescPanel vtables) and named: +0 / +0 because the row went 0 → 97.5 on an alias-gated `??_G`/`??_E` fold name. **`default/CampaignSongInfoPanel` completed 50/50 rows, 4,860/4,860 B** (was 37/54, 2,344/5,124): all 11 anonymous rows were oracle methods missing guarded function-local statics at retail's declaration positions, `Refresh` needed only its two `static Message`s moved to their use sites, `SelectDefaultInstrument` one nested call hoisted; the far block `0x8261FEB8–0x8261FFC8` was **MainHubPanel's** (COL `.?AVMainHubPanel@@`; the real `CampaignSongInfoPanel::Unload` is at `0x825F58C8`) and was re-homed in `splits.txt` for +3 rows / +116 B there. Not done: `?CheckProfileForTicker@MainHubPanel` 148 B at 98.38 (retail `cmplwi` after `Server::GetPlayerID` implies an unsigned return, ours is `virtual int` in shared `Server.h`), three rows needing alias groups (seven proposals in §6), three interleaved foreign pins left in place, MainHubPanel's `Poll` and eight anonymous rows, the `__ucopy_ptrs` charge on `fn_82610160`. Rebase hit three conflict rounds on `target_symbol_map.json`, verified by JSON decomposition — 14 rows added, 2 changed, 0 removed on both sides, all 29,413 main keys preserved. Gates at `c62ce4e0` under 4.2.9, worktree and main line-identical: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0, 21 rows in / 0 out, matching the self-report exactly. | `docs/decomp/W16AJ_0x826100F8_IDENTIFICATION_AND_CAMPAIGNSONGINFOPANEL_2026-09-14.md` |
| **W16-AE** | fable (escalation) | **+35 / +3,572 B** (43,373 → 43,408; 3,999,912 → 4,003,484 B; merge `8f434138b4b9`). Fable escalation of W16-AB's four "cannot be fixed" items. **All four were fixable, and in each case the stated cause was not the real cause.** **Item 1** `0x823eb548`: the `vtordisp` thunk `?Replace@MsgSource@@$4PPPPPPPM@BI@…` is **SessionSearcher's** (its vtable's `??_R4` COL names `.?AVSessionSearcher@@`), and the zone `0x823EAD68–0x823EB958` was not Anim.cpp's span but **seven circular single-function pins** (FlowTrigger, Anim, PracticeSection, BandUI, Group ×2, VocalNoteList) each carved to fit a folded template name — ported `network/net/SessionSearcher.cpp` + `NetLog.cpp` from rb3-Wii dev shaped by `.pdata` extents and retail bytes (`OnMsg(InviteAcceptedMsg)` returns **bool**; `LogFile` must not declare a virtual dtor), removed the seven pins and the drained `FlowLabel.cpp` entry, named 22 rows, withdrew the `__insertion_sort<RndPollable**>` claim (NOT_A_FOLD, vacuous T1 evidence), proved five memberships by `icf_pair_adjudicate.py --chase`. **Item 2**: the six "need carving" blocks are ordinary `[function + pad + next EH prefix]` blocks — nothing to carve; each sat in the **wrong unit** with a map name the retail size refutes (e.g. `vector<String>::_M_fill_insert` 112 B at a 108 B `vector<DeltaArray@BandFaceDeform>`); re-homed six `.text` blocks, renamed one row each, withdrew 11 memberships. **Item 3a**: the premise was false — `0x82553fc8` is not in `_denylist` and the renamer has honoured it since `f3fe9ab1`; the real gap was that nothing verified the refusal reached the built objs, so `check_denylist_applied()` (exit 7) + a mutation test were added to `verify_objs_patched.py`. **Item 3b**: both "refuted T1 groups" were already withdrawn by W16-AB itself; the generator defect is that T1 never read the survivor's own COMDAT — `survivor_self_check` added to `icf_alias_build.py`, and `icf_pair_adjudicate.py` no longer reads a depth-0 self-pair PROVEN without comparing a byte. **Item 4** `0x827f42a8` decided: the terminal is `??_GUILabel`, T1-identical to our `??_GUIButton` — new group at `0x827f5348`, the 12 B thunk row crossed. Coordinator add-on: `InputMgr::{Set,Clear}InvalidMessageSink` admitted at FT-EMPTY into group 1537 (`0x826c3888`) after fixing `fold_thunk_gate.py`'s crash on no-map-row names and a partial-install clobber (`OvershellPanel::Enter` 204 B + `Exit` 132 B crossed). Not done: the five arbitrary-bijection classes (Item 5), `0x822b6538` (needs `fn_824CE130` identified), the 26 non-refuted carry-path contradictions, the pre-existing red test arms (`test_patch_state.py` 21, unicorn `test_prober` 1 — byte-identical to main, not diagnosed), the `0x82c16aa0` denylist rationale. Rebase: two conflict rounds on `target_symbol_map.json`, verified by decomposition — 18 rows added / 1 removed / 13 changed and 25 alias groups replaced / 21 removed identically on both sides, `objects.json`/`splits.txt` hunks byte-identical, all 29,427 main keys preserved. `total_code` moved +48 B / +1 row from the re-homing. Gates at `8f434138` under 4.2.9, worktree and main line-identical: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0, 53 rows in / 14 out (every FELL OUT row a byte-neutral reattribution, doc §7), matching the self-report exactly. | `docs/decomp/W16AE_ESCALATION_0x823eb548_THUNK_HOMING_SPAN_CARVES_TOOLING_2026-09-14.md` |
| **W16-AK** | opus | **+3 / +684 B** (43,408 → 43,411; 4,003,484 → 4,004,168 B; merge `976a98a3247e`). `default/OvershellPanel` 250 → 253 of 272 (Poll 0 → 100, two OnMsg 99.8x rows crossed). The brief's ICF-fold premise for the two OnMsg charges was REFUTED on retail bytes: the bodies are 76 B retail vs 132 B ours (different-size COMDATs cannot fold), the surplus a Wii-dev-only `unk4cc == 2` block — removed as a source correction, not forgiven. Two map rows byte-proven (`0x825b2ea0` ResolveAutoSignInStates, `0x825b2ff0` ResolveChooseProfileStates); ResolveSlotStates 93.22 → 98.84; seven alias proposals filed for W16-AL (`docs/decomp/W16AK_alias_proposals_2026-09-14.json`). Gates at `976a98a3` under 4.2.9, worktree and main line-identical: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0, 3 rows in / 0 out, matching the self-report exactly. | `docs/decomp/W16AK_0x826100F8_IDENTIFICATION_AND_CAMPAIGNSONGINFOPANEL_2026-09-14.md` |
| **w16am** | opus | **+5 / +980 B** (43,411 → 43,416; 4,004,168 → 4,005,148 B; merge `80aa49efd036`). **W16-AM** (opus, landed `80aa49ef`) | **+5 / +980 B** (43,411 → 43,416; 4,004,168 → 4,005,148 B; 9 rows in / 4 out) | Three briefed figures measured false before any edit: the five bijection classes are alias groups, not `_bijection_arbitrary` entries; `0x823f0b50` has 4 members, not 2; AE's carry-path "38 / 26" population does not exist as described (measured 331 refusals / 159 landed / 188 records / 168 addresses, 148 reloc-target-only + 20 structural). The paying find: `fn_824CE130` hangs off `0x824cf890`, not `0x822b6538`, and the 23 byte-identical `erase<list<T>>` bodies are distinguished by the relocation TARGET (the element destructor), so the `list<T>` bijection is decidable — 8 rows repaired, confirmed by six of nine crossed-in rows being CALLERS of the corrected symbols; ~10 more `list<T>` rows remain inconsistent (priced-positive vein). `0x823d3918` is a folded `Handle` (16 retail `.rdata` slot-6 pointers + 1 `.pdata`), row nulled at Δ0 after a pre-registered rename measured −1 / −164 B in a different unit via the stale `icf_aliases.map`; the brief's `.xdata` test does not exist on X360 PPC. `0x82c16aa0` denial kept (rationale lived only in `49b18c95`'s message; `git log -S` cannot see a key→`_denylist` move, `-G` can). unicorn `test_prober` fixed (born red in `06022df7`); `test_patch_state.py` is red BECAUSE of AE's `e63056f0` denylist check (25+2, every one on its own control), diagnosed not fixed. Gates at `80aa49ef` under 4.2.9, worktree and main line-identical: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0, 9 rows in / 4 out, matching the self-report exactly. | `docs/decomp/W16AM_BIJECTION_CLASSES_fn_824CE130_DENYLIST_CARRYPATH_2026-09-14.md` |
| **w16an** | opus | **+12 / +2,736 B** (43,416 → 43,428; 4,005,148 → 4,007,884 B; merge `302fbd0ea0e3`). **W16-AN** (opus, landed `302fbd0e`) | **+12 / +2,736 B** (43,416 → 43,428; 4,005,148 → 4,007,884 B; 13 rows in / 1 out, the re-homed `??__F` thunk) | `default/MainHubPanel` **166/176 → 176/176** and `default/CampaignSongInfoPanel` **50/50 → 53/53**, both 100 %. `CheckProfileForTicker` closed with a call-site `(unsigned int)` cast on the W16-B precedent, `Server.h:26` NOT widened; `Poll` at 51.80 was one wrong call spelling — retail calls `Timer::SplitMs()` out-of-line where the rb3-Wii oracle expands `Split()`+`Ms()` inline (neither briefed family applied). All eight anonymous rows identified: the two character-identical `OnMsg` bodies (`NewRemoteMachineMsg` / `RemoteMachineLeftMsg`, 188 B each) were separated by the `HANDLE_MESSAGE` dispatch order in `Handle`, validated on a 10-row control after `.text`/`.data` ordering was ruled out as an instrument. The systematic defect behind all eight: retail materialises function-local `static`s (guard word, instance, `??__F` thunk) where the dev source uses file-scope externs from `Messages4.h` / `Symbols*.h` — census clean both ways. Two briefed premises measured false: `ReloadMessages` was never "at 100", it was UNPAIRED (no row, no map entry), and retail makes no `GetPlayerID` test there at all (slot-0x1c scan + `TheServer` absent from data refs); AJ's Item 4 count was three rows, not two. The re-home (`0x825F5BF4–0x825F5D28` PhysicsManager → CampaignSongInfoPanel, `PhysicsManager.cpp` splits entry deleted, 292 B moved wholesale, `total_code` unchanged) made `SelectedScoreType` PAIRABLE and exposed a divergence invisible while mis-pinned — retail reads the returned `Symbol` from its own stack slot, we dereferenced the sret pointer. Reported, not fixed: `tools/icf_alias_finder.py --validate` rc=1, 5 contradicted groups (four STLport `AccomplishmentCmp`/`GoalCmp` pairs + the `SetTypeDef@UIPanel`/`Highlight@RndDir` thunk pair), proved pre-existing at `8d4fb23a` four ways and handed to W16-AL. Gates at `302fbd0e` under 4.2.9, worktree and main line-identical: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0, 13 rows in / 1 out, matching the self-report exactly. | `docs/decomp/W16AN_MAINHUBPANEL_TICKER_POLL_ANON_ROWS_2026-09-14.md` |
| **w16al** | opus | **+14 / +14,820 B** (43,428 → 43,442; 4,007,884 → 4,022,704 B; merge `0196b0ceb881`). **W16-AL** (opus, landed `0196b0ce`) | **+14 / +14,820 B** (43,428 → 43,442; 4,007,884 → 4,022,704 B; 96 rows in / 2 out, both deliberate) | ALIAS/MAP lane, the first with authority over `scripts/symbol_aliases.json` since W16-AH. 20 proposals from W16-AJ, W16-AH and W16-AK adjudicated on retail bytes: **12 installed, 2 refused, 14 held, 5 inherited contradictions withdrawn**. AJ1 named the anonymous survivor `0x826100f8` (26 hashtable dtors) at a deliberate **net −176 B**: the two rows that fell out, `??1TourProgress` 192 B and `??1NameGenerator` 88 B, are real container-type divergences the anonymous callee had been forgiving — filed for a source lane. AJ2 Group B (24 `hash_map` dtor thunks) paid +81 rows / +3,568 B, with the 7 excluded thunks refuted 7/7 as the control. AJ4–AJ6 repaired the `0x8261feb8` Unload group, re-homed `CampaignSongInfoPanel::Unload` to `0x825f58c8` and installed the `0x825f5920` Load fold; AJ7's `??_E$4` thunk at `0x82603a78` was a 2-wide fold proven on RTTI COL enumeration, Δ0 as predicted. AK1–AK6 paid +11 rows / +10,580 B against 8,412 predicted. **Refused:** AJ3 (`??_GTourDescPanel` at `0x825f4598` destroys a second sub-object at `0x5c`, so it cannot have folded with a one-sub-object twin); AK7 (`0x82682668` is `BandUserMgr::GetBandUser(User*)` with six `bl` sites, not an atexit dtor — its L3_EXACT PASS was `name_check` forgiving the two placeholder relocations that carry the whole identity; converted to a map repair, +3 rows / +848 B). **Held:** AH's 14 memberships — `ObjPtrVec` occurs 0 times in the image vs `ObjPtrList` 45, and the 60 B body has 112 reloc-normalized twins so the asserted fold never happened, but our build emits no `ObjPtrList _Copy_Construct` rows, so the type-correct name would strand two 100 % rows; the discriminators are unnamed, so they forgive 0 bytes today. The 5 CONTRADICTED groups inherited by `icf_alias_finder --validate` withdrawn with records, rc=1 → rc=0 (1396 map-consistent / 247 tolerated / 0 contradicted), Δ0. Recurring finding: shape identity is not identity — relocations carry it. Filed: `0x823d3918` `CharLipSync::Handle` +164 B (alias membership AND a source fix), AM's 20 structural contradictions (all survivor-self map questions), the `0x82682668` splits re-home (+28 B), `ResolvePartWaitStates` idx 161 `beq` (1,356 B). Gates at `0196b0ce` under 4.2.9, worktree and main line-identical: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0, 96 rows in / 2 out, matching the self-report exactly. | `docs/decomp/W16AL_ALIAS_INSTALL_AJ_AH_AK_PROPOSALS_2026-09-14.md` |
| **w16ao** | opus | **+8 / +944 B** (43,442 → 43,450; 4,022,704 → 4,023,648 B; merge `622690b6f4e2`). **W16-AO** (opus, landed `622690b6`) | **+8 / +944 B** (43,442 → 43,450; 4,022,704 → 4,023,648 B; 8 rows in / 0 out) | MAP-ONLY lane on W16-AM's 10 remaining `resize<list<T>>` bijection rows, adjudicated on retail bytes: **9 wrong and repaired, 1 not wrong at all, plus a coupled 11th row**. `0x8246af50` was left alone — it has two retail callers (`operator>>` for `list<Transform>` and for `list<Instance@RndMultiMesh>`, where `Instance` is `{Transform mXfm;}`), so both spellings are simultaneously true and the remaining value is an alias membership, filed as report-only JSON (`docs/decomp/W16AO_ALIAS_PROPOSALS_FOR_W16AL.json`). `0x823286c0` (`Layer@LayerDir` → `FilePath`) was pulled in because the two spellings were an exact transposition and the injectivity check refuses a half-applied swap. **Three corrections to AM's method**, each of which would have produced a confidently wrong name: (a) the erase anchor is degenerate for trivially-destructible `T` — no `~T` call, range erase 0x60 not 0x6C, forwards to a single erase whose only callee is `MemOrPoolFreeSTL`, and ICF folds it across every `T` of equal node size; (b) the `ObjList<T>::resize` wrappers AM used as corroboration are shuffled alongside the rows they wrap, not an independent oracle — `0x822b6798`'s erase destroys `EventCall@EventAnim` while its caller is spelled `ObjList<ProxyCall@EventTrigger>`; (c) `li r3,<node>` before `MemOrPoolFreeSTL` gives `sizeof(T) = node − 8` and settles both from bytes alone. Spellings built from a template proved to reproduce all 20 existing `StlNodeAlloc` resize rows byte-exactly. Three of the eight crossing rows are **callers** of the corrected symbols (the same functions used as proof channels), which is the confirmation, not the bytes; zero un-pairing cost. Filed, not acted on: three further wrong rows outside the brief (`0x822b6ee8`, `0x823c38e8`, `0x822a8bd0`), seven anonymous addresses identified but not named, and a flag that `matched_functions` read 43,436 name_check vs 45,105 `none` on this tree, which does not reproduce RULER-SWEEP's "ruler-invariant" claim. Gates at `622690b6` under 4.2.9, worktree and main line-identical: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0, 8 rows in / 0 out, matching the self-report exactly. | `docs/decomp/W16AO_LIST_T_BIJECTION_ROWS_2026-09-14.md` |
| **W16-AP** | opus | **+5 / +584 B** (43,450 → 43,455; 4,023,648 → 4,024,232 B; merge `440bc9db3dfd`). **W16-AP** (opus, landed `440bc9db`) | **+5 / +584 B** (43,450 → 43,455; 4,023,648 → 4,024,232 B; 6 rows in / 0 out) | SOURCE + MAP lane closing `default/GamePanel` **91/97 → 93/97** on retail bytes. Four anonymous rows named after checking the caller population (`CreateGame`, `SendRestartGameNetMsg`, `UpdateNowBar`, `Handle`; map-only step measured **+0/+0 exactly as predicted** — naming pays in pairing and bug exposure, not bytes). **`RestartGameMsg` carries no payload in this image** — four independent witnesses (Save/Load fold onto one bare `blr`, `Dispatch` never touches `this`, the call site passes nothing, the ctor is trivial); the oracle's `mFromWin` and `SetIsRestarting` are absent and `SendRestartGameNetMsg` takes no `bool`. `CreateGame`'s `disable_pause_ms` is a **function-local static**, W16-AN's pattern reproduced in a second unit. Three `diff_arg` rows adjudicated on retail bytes refusing objdiff's `AT_LIMIT`: `Handle@GamePanel` and `IsLoaded` are genuine folds (`IsLoaded` proved programmatically — 96 B, **zero relocations**, byte-identical); **the third is NOT a fold** — `0x826ccc10` is named `??0Splash` but is 328 B against our ~420 B `Splash` ctor while `default/Splash`'s own row reads 6.89 %, so the lane reports a mis-identified address (likely `??0DirectInstrument`) rather than aliasing it. Both byte predictions **under-counted the same way twice** (+3/+244 → +4/+348; +1/+196 → +2/+236): layout changes cascade into EH funclets and factory functions in other units — price a type's whole row family. Three alias memberships filed as report-only JSON (`docs/decomp/W16AP_ALIAS_PROPOSALS_FOR_W16AL.json`) that would close the unit to 95/97 / +1,936 B. Deferred with evidence: `UpdateNowBar` (628 B @ 5.93, RB3-360-specific, no oracle, needs a 4-parameter `TrackPanelDirBase` vtable widening). Gates at `440bc9db` under 4.2.9, worktree and main line-identical: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0, 6 rows in / 0 out, matching the self-report exactly. | `docs/decomp/W16AP_GAMEPANEL_UNIT_CLOSURE_2026-09-14.md` |
| **W16-AS** | opus | **+1 / +468 B** (43,455 → 43,456; 4,024,232 → 4,024,700 B; merge `2c9103b1a3a5`). **W16-AS** (opus, landed `2c9103b1`) | **+1 / +468 B** (43,455 → 43,456; 4,024,232 → 4,024,700 B; 1 row in / 0 out — `??0GamePanel@@QAA@XZ` 468 B, whose `DirectInstrument` member-ctor call was charged as a wrong callee while the map called that address `??0Splash`) | IDENTIFICATION + VTABLE lane on W16-AP's two filed items, both settled on retail bytes. **`0x826ccc10` is `??0DirectInstrument@@QAA@XZ`, not `??0Splash`** — six channels: no polymorphic vptr at `+0` (mandatory for `Splash`'s virtual dtor), `??_7?$ObjDirPtr@VObjectDir@@@@6B@` stored at `+0x4` (= `DirectInstrument::mDir`), `[this+0]` initialised `0x7f` then overwritten from `SystemConfig("instruments")→"chamberlin"` (= `mVolume`), none of `Splash`'s members constructed, bracketed by `?Enable@DirectInstrument@@`/`??1DirectInstrument@@`, sole caller `GamePanel`; the falsifiable one: re-pointing the row at `0x827421b8` moved `??0Splash` **6.89 → 92.27 fuzzy on unchanged source**. The real `??0Splash` is **`0x827421b8`** (420 B, previously anonymous; stores `??_7Splash@@6B@`, interns `"splash_time"`, sole caller `App`). AP's size figure tested literally and confirmed (our COMDAT 420 B vs retail 328 B — a real COMDAT size, not the STLPORT-1 artifact); the 328 B body is a **splits mis-pin** — `DirectInstrument.cpp` has a base obj but no `splits.txt` heading, six of eight bodies size-identical to retail — filed for the splits owner as text. Naming bet priced first (`App` is unpaired ⇒ zero downside); predicted **+1 / +468 B, 0 out; measured exactly that** (`??0Splash` itself sits at 92.27, not yet across). `default/GamePanel` 93/97 → **94/97**. **Slot `0xd4` of `TrackPanelDirBase` is `(const char*, const char*, const char*, Symbol)`** — vtable at `.rdata 0x8202d464`, `0xdc` reads 0 so `0xd4`/`0xd8` are the last slots; `0x82303bb8` occurs exactly once as an `.rdata` word; of 13 `GetTrackPanelDir()` sites exactly one dispatches `0xd4`; types from `SetDisplayText(const char*,bool)` ×3, `MakeString`'s return, and `SetTextToken(Symbol)`. **Not named** — RB3-360-only, no oracle. Landed on proof at measured **Δ0 with 73 TUs recompiled** (live, not absent-vs-absent); `UpdateNowBar` 5.93 → 8.19 from the call shape. Two traps recorded in the header: a bare `0xd4` offset scan hits 10 unrelated vtable calls; the map's `??$MakeString@VSymbol@@PBDPBD@@` spelling is an ICF fold artifact. Two false comments removed from `GamePanel.cpp` — AP's refuted one, and one AP missed: `fn_82695178` has **no MBT string**, its formats are `"%d.%02d.%02d"`×2 / `"%d.%d.%03d"` beside `TaskMgr::Seconds` + `fmod` (a time display). Two of three port blockers cheapened: `fn_827C91A0` is `f1 *= 1000.0f` into a singleton's slot `0x8`; `lbl_82C71838` is `gNullStr`. Remaining: `TheSongDB`'s 0x24-byte record vector at `+0x20`. NOT done: `UpdateNowBar` body (628 B, not unfixable — a Fable candidate), `PerfectOverdriveTracker::Poll_` (regalloc class, permuter deferred). Gates at `2c9103b1` under 4.2.9, worktree and main line-identical: BUILD rc=0, RULER rc=0, MANIFEST rc=0, NATIVE PASS 18/18 skipped=0, DIRTY 0, 1 row in / 0 out, matching the self-report exactly. | `docs/decomp/W16AS_SPLASH_MISID_UPDATENOWBAR_SLOT0xD4_2026-09-14.md` |
| **W16-AR** | opus | **+7 / +1,060 B** (43,456 → 43,463; 4,024,700 → 4,025,760 B; merge `2fd524f0b4c6`). AO's three filed map rows became 11 rows (10 corrected, 1 new): the ObjList/list wrapper shuffle (6 rows, +3 / +364 B — AO's `0x823c38e8` premise was wrong, `sizeof(MsgSinks::Sink)` == `sizeof(ConstraintSystem)` == 16, only the dtor reloc discriminates; no `list<Sink@MsgSinks>::erase` exists in retail) and the displaced EventTrigger/EventAnim spellings, which were a ROTATION not single wrong rows (5 rows, +4 / +696 B incl. `?Copy@EventTrigger@@` 328 B). ⛔⛔ **Item 3 REFUTED CLAUDE.md: `matched_functions` is NOT ruler-invariant on 4.2.9** — objdiff-core `b14ba45` (08-20) lets a vetted wrong-callee reach `mpn` under `name_check` only; measured 43,453 vs 45,104 (`none`), Δ+1,651 one way, 0 the other ⇒ a `none` leg is NOT a function-count control (CLAUDE.md corrected `59d4c55e`). Coupled map+splits defect recorded: `0x823c38e8` sits in a single-function circular `Msg.cpp` pin while its name lives in `CharBlendBone.obj` ⇒ 0% by construction until re-homed (AR-1 144 B, AR-2 328 B, filed in `W16AR_ALIAS_PROPOSALS_FOR_W16AQ.json`, `alias_proposals` deliberately empty). NOT done: seven anonymous addresses (item 4); the `_M_splice_insert_dispatch` three-row rotation (`0x824a2190`/`0x824a2230`/`0x824c9878`, injectivity check needed). Worktree gate 7 in / 0 out, main gate line-identical. | `docs/decomp/W16AR_AO_FILED_ROWS_RULER_INVARIANCE_2026-09-14.md` |
| **W16-AQ** | opus | **+7 / +2,248 B** (43,463 → 43,470; 4,025,760 → 4,028,008 B; merge `90dfdb79cd02`). AL's follow-ups: NameGenerator + TourProgress containers are `hash_map` not `std::map` (4 map rows corrected, 1 new, the dead `std::map` `operator>>` row falls out); `CharLipSync::Handle` has NO cases — group 478 survivor corrected with a `withdrawn` record; `ResolvePartWaitStates` nested `continue` (+1,356 B source-only); `0x82682668` dynamic_cast island MOVED CharLipSync→BandUserMgr; new T1 alias group `$hashtable_assign_Symbol`. Worktree gate 13 in / 1 out, main gate line-identical. | `docs/decomp/W16AQ_AL_FOLLOWUPS_DTOR_TYPES_LIPSYNC_HANDLE_2026-09-14.md` |
| **W16-AU** | opus | **+2 / +216 B** (43,470 → 43,472; 4,028,008 → 4,028,224 B; merge `bc2cd5d1e3e2`). Splits-only. `DirectInstrument.cpp` own heading (Δ0 by set-diff: 10 rows / 540 B reattributed from TrainerPanel, 328 B ctor fuzzy 0 → 97.56, now pairable); AR-1 re-home `0x823C38E8` Msg → CharBlendBone (+1 / +108 B, row is 108 B not 144); AR-2 re-home `resize<ObjList<OldMatOption>>` → OutfitConfig (+1 / +108 B, row is 68 B); AQ's `__RTDynamicCast` thunk `0x82682688` homed to BandUserMgr (Δ0; `default/JsonMemory` 1/2 → 1/1 — its 97.14 against our `JsonRealloc` was opcode-shape only, a wrong-function near-match). ⚠ The brief's 420 / 120 B ctor / dtor sizes were the STLPORT-1 one-sided artifact; all eight bodies size-identical. Filed: 328 B ctor one-line `FilePath` temporary fix (source lane); `TrainerPanel.cpp:465` obsolete scatter-include; map renames AU-1 `0x826ccb48` → `?IsLoaded@DirectInstrument@@` (+8 B, one caller in GamePanel) and AU-2 `0x82682688` → `?GetLocalBandUser@BandUserMgr@@` (+28 B) as `docs/decomp/W16AU_MAP_PROPOSALS_FOR_W16AV.json`. Worktree gate 13 in / 10 out (10 = reattribution), main gate line-identical. | `docs/decomp/W16AU_DIRECTINSTRUMENT_HEADING_AR_REHOMES_THUNK_0x82682688_2026-09-14.md` |
| **W16-AT** | **fable** | **+1 / +628 B** (43,472 → 43,473; 4,028,224 → 4,028,852 B; merge `ea012adc5104`). Source + one map row. `?UpdateNowBar@GamePanel@@QAAXXZ` ported and crossed (628 B, fuzzy 100 / mpn 100; GamePanel 94/97 → 95/97): levers were `math/Utl.h` float `Max`/`Min` for the `fsel` shapes, `(float)fmod(x, 60000.0)` for the `lfd`+`frsp`, and declaring the vector ref before the `Symbol` ctor + binding `const PracticeSection &`. SongDB record = `PracticeSection` (`unk0` name / `unk4` start tick / `unk8` end tick, predicate `start <= tick < end`, first hit wins) — **no header change needed**; ⚠ the bare class name resolves to the hamobj `PracticeSection` (0x60 B), `--tu --exact` required. `lbl_82C78F5C` named `?TheTempoMap@@3PAVTempoMap@@A`, proved by its WRITER `ResetTheTempoMap` (Δ0 measured). `fn_827C91A0` (= `SecondsToTick`, convention-derived) left anonymous. ⚠ Exposed: `SongInfoCopy` map row `0x827d2500` is WRONG (`_M_throw_length_error`; retail reads `TheTempoMap`) — fuzzy 60 → 58.33 is the naming doing its job; needs a SongInfoCopy lane. Filed: TimeConversion cluster sits inside `StringTable`'s `.text` pin (splits re-home). Worktree gate 1 in / 0 out, main gate line-identical. | `docs/decomp/W16AT_UPDATENOWBAR_PORT_SONGDB_RECORD_2026-09-14.md` |
| **W16-AV** | opus | **+12 / +3,500 B** (43,473 → 43,485; 4,028,852 → 4,032,352 B; merge `df358e106917`). Alias/map only. AO `0x8246af50` fold INSTALLED but as a **transitive** fold (AO's "reloc-identical" claim was false at 2 sites; a second `insert` group at `0x824e0f00` was required; +3 / +664 B vs +2 / +244 B predicted); AP-2 already installed; AP-3 **refuted as an alias** — the two thunks tail-call different callees, so the map row `0x826ccb48` was repaired to `?IsLoaded@DirectInstrument@@` (+2 / +132 B, the same fix W16-AU filed as AU-1); AP-1 `PrintStats@HitTracker` into FT-EMPTY `0x826c3888` (+1 / +1,812 B); AR-3 rotation + its third name (+1 / +176 B); AU-2 `0x82682688` → `?GetLocalBandUser@BandUserMgr@@` (+4 / +688 B, all caller cascade). Refused: `Handle@DxCubeTex`/`NgFur` (vacuous — no such symbol in any of our 1,215 objs; the vtable test is circular under ICF); `ObjPtrList` per-site (the briefed addresses are `ObjPtr<>` ctors, a transposition off `0x82706100`). Six briefed figures failed literal testing (tabled). Filed: `0x824c9878` needs an `EventAnim.cpp` splits re-home. Worktree gate 12 in / 0 out (the 12th = the 28 B `GetLocalBandUser` row pairing with W16-AU's re-home), main gate line-identical. | `docs/decomp/W16AV_ALIAS_INSTALL_AO_AP_AR3_DXCUBETEX_OBJPTRLIST_2026-09-14.md` |
| **W16-AW** | opus | **+1 / +328 B** (43,485 → 43,486; 4,032,352 → 4,032,680 B; merge `db54a3964af9`). Source only. `??0DirectInstrument@@QAA@XZ` (328 B) crosses — retail compiled a `FilePath(".", path)` temporary into `LoadFile(…, 1, true, kLoadFront, false)`, not a named local (+1 / +328 B); `TrainerPanel.cpp`'s scatter-include of `DirectInstrument.cpp` removed at measured Δ0. Brief item 2b **REFUTED**: `RhythmDetector.cpp`'s `#include "band3/game/DirectInstrument.cpp"` is **load-bearing** — DirectInstrument.cpp is the only TU instantiating `ObjDirPtr<ObjectDir>` and retail placed those COMDATs inside RhythmDetector's `.text` `0x82270690`–`0x82270B1C`; removing it measured **−6 / −344 B** (incl. `fn_822709A8`, `??3DirLoader@@SAXPAX@Z`), so it was reverted. Located but not named: `?Enabled@DirectInstrument@@` = `0x826CCAF0` (16 B, the T1 `InTransition` ICF survivor already at fuzzy 100 in PracticePanel); `?SetVolume@` = `0x82A478D0` (proved via `?Handle@GamePanel@@` reloc `+0x59C`; 15 of 17 callers XDK, filed in `W16AW_map_proposals.json`). 11 callers of `0x82682688` audited, zero contradictions. Worktree gate 1 in / 0 out, main gate line-identical. | `docs/decomp/W16AW_DIRECTINSTRUMENT_CTOR_SCATTER_INCLUDES_COMDAT_SEARCH_2026-09-14.md` |
| **W16-AX** | opus | **+2 / +296 B** (43,486 → 43,488; 4,032,680 → 4,032,976 B; merge `73927a9be00c`). `splits.txt` only. Three `.text` re-homes EventTrigger → EventAnim: `0x824c9878` `splice_insert_dispatch<EventCall@EventAnim>` (156 B; row 0 → 99.74 mpn, Δ0 bytes — the re-home W16-AV filed, one charged site left); `0x824c99f8` `list<EventCall@EventAnim>::operator=` (+1 / +188 B); `0x824c9ab8` `ObjList<EventCall@EventAnim>::operator=` (+1 / +108 B). Brief item 2 **REFUTED**: carving TimeConversion out of StringTable is Δ0 by design — `StringTable.cpp` deliberately `#include`s `utl/TimeConversion.cpp` (the owner-TU scatter pattern, 252 directives / 170 files) and all 14 named rows are already fuzzy 100; the brief had dropped W16-AT's own hedge. Item 3 (doubled `UIStats`/`AccomplishmentProgress`/`Game` headings) was already fixed by `b341d7ab` on 08-31; `CLAUDE.md`'s paragraph is stale (flagged, not edited). **Filed** for a joint map+splits lane: map row `0x824c97d8` is **wrong** — retail calls `?insert@?$list@VEventCall@EventAnim@@…`, so it is the `list<EventCall@EventAnim>` ctor misnamed as the ProxyCall twin, not a fold; rename and `EventAnim.cpp` re-home must land **together** (either half alone takes the row 99.83 → 0; +116 B, +2 / +272 B combined). Also flagged `??0GlitchFinder@@QAA@XZ` (340 B, fuzzy 0, StringTable unit). Worktree gate 2 in / 0 out, main gate line-identical. | `docs/decomp/W16AX_EVENTANIM_TIMECONVERSION_REHOMES_DOUBLED_HEADINGS_2026-09-14.md` |
| **W16-AY** | opus | **+1 / +144 B** (43,488 → 43,489; 4,032,976 → 4,033,120 B; merge `0ddb8a0c9cbd`). Map only (3 rows). `0x827d2500` renamed `?_M_throw_length_error@…` → `?SetTheTempoMap@@YAXPAVTempoMap@@@Z` — the retail body is `lis/stw` to `TheTempoMap`, a **store**, not the getter/thunk the brief and W16-AT predicted; payout came through the caller `fn_82788518` = `?OnAcceptMaps@SongParser@@` (144 B, its one charged `diff_arg`) at +1 / +144 B, while the row itself drops 58.33 → 0 (it had been falsely pairing against `SongInfoCopy.obj`'s STLport COMDAT — the whole −0.000067 pp fuzzy move). `gDefaultTempoMap` (`0x82c78f60`) and `TheTaskMgr` (`0x82e051a0`) installed from post-build COFF spellings: 414 sites across 91 units flipped forgiven → checked at **Δ0 on all four measures** (proved non-inert by renamer count 86,188 → 86,285). **Both ObjPtrVec premises REFUTED on retail bytes**: `0x8278b7f0` copies its 8-byte element as four halfwords ⇒ 2-aligned ⇒ cannot hold a pointer ⇒ `PhraseAnalyzer::PhraseData {short,short,short,bool}`, not a HamMove divergence; `0x8278bb98` is the `vector<PhraseData>` insert (align-2 vs align-4 charged sites), not `ChecksumData`; `0x822abd60`'s stride is `0x24` (ours 16, `ClipDistMap::Node` 12) and `ClipDistMap.obj` emits no `_M_fill_insert` at all. `fn_827C91A0` left anonymous on a **controlled** negative (`seconds_to_tick` 0 retail hits; controls `seconds_to_beat`/`ms_to_tick` 1 each). Alias group `0x8278b7f0` is **mis-oriented** (T1 verified the folded spelling, never the survivor) and `--validate` still calls it map-consistent — reverse only together with a `PhraseAnalyzer.cpp` pin (up to +484 B). Filed: `0x827D2500` → `TempoMap.cpp` re-home (+1 / +12 B). Worktree gate 1 in / 0 out, main gate line-identical. | `docs/decomp/W16AY_SONGINFOCOPY_ROW_OBJPTRVEC_ROWS_TEMPOMAP_NAMES_2026-09-14.md` |
| **W16-AZ** | **fable** | **+7 / +884 B** (43,489 → 43,496; 4,033,120 → 4,034,004 B; merge `256a63762fc2`). Splits+map+alias only, **no `src/` edit** (`config/45410914/splits.txt` 18 ±, map 3 rows, alias 2 reoriented + 1 new group). Item 1 (`547fe0ae`): `.text` `0x8278B7F0–B838` (from HamMove) and `0x8278BB98–BD68` (from FileChecksum) re-homed into `system/beatmatch/PhraseAnalyzer.cpp`; rows `0x8278b7f0`/`0x8278bb98` renamed to the `PhraseData` spellings our obj emits (`__uninitialized_copy<PhraseData*>`, `vector<PhraseData>::_M_insert_overflow_aux`); alias group `0x8278b7f0` **reoriented** (the `ObjPtrVec` survivor had 0 definers and 0 referencers on our side — it was never the body T1 was measured against; withdrawn as `SURVIVOR_SIZE_MISMATCH`, nothing pruned). Predicted +2 / +484 B, **measured +3 / +620 B** — the miss is `push_back<PhraseData>` (136 B), whose only charge was the `bl` to the misnamed `0x8278bb98`: **repairing a wrong name collects from its callers** (AY's "not pinned at all" claim for these holes was false; they were pinned to the wrong TUs). Item 2 (`e9b3ad2c`): `0x827d2500` SongInfoCopy → TempoMap, **+1 / +12 B exact**. Item 3 (`3e87a7cd`): `.text` `0x822ABCE0–E70` re-homed ClipDistMap → OutfitConfig (a CJ-1 blind extension), `0x822abd60` renamed `?_M_fill_insert@?$vector@VBandPatchMesh@@…`, alias group reoriented with a withdrawn record, **+2 / +152 B** (112 B body + 40 B funclet `fn_822ABE48`). Item 4 (`acb08c89`): new T1 group at `0x826cb590`, `push_back<SongSection>` / `push_back<RawPhrase>` (32/32 masked words on both spellings), **+1 / +100 B exact** (`AddInfo@PhraseAnalyzer` crosses; SongLayout's caller stays 99.84 behind `0x82787ed0`). Set-diff: 8 in / 1 out (the 44 B funclet `fn_8278BD3C` re-attributed FileChecksum → PhraseAnalyzer). Alias validate PASS 1400 / 247 tolerated / 0 contradicted / 1648. Gates: build rc=0, ruler rc=0, manifest rc=0, `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`. NOT done: `fn_822ABCE0` (two-way T1 tie), `fn_822ABDD8` (size mismatch), HamCamTransform's blocks (proposal only), `get_allocator<RawPhrase>` membership (belongs to W16-BA's `StlNodeAlloc<_List_node<int>>` group), `0x82787ed0`, six anonymous PhraseAnalyzer rows. Record: `docs/decomp/W16AZ_PHRASEANALYZER_HOLES_TEMPOMAP_REHOME_CLIPDISTMAP_2026-09-15.md` |
| **W16-BB** | opus | **+1 / +264 B** (43,496 → 43,497; 4,034,004 → 4,034,268 B; merge `2b11726dcdaf`). Map 2 rows, alias 1 new group, `src/system/utl/Cheats.cpp` 7 ±, `CLAUDE.md` 49 ±, proposals json. Item 1 (`8b14483c`, `c5d6329d`): **the briefed premise was false** — `GlitchFinder` occurs **0 times** in retail; `0x827c2780` is **`??0CheatsManager@@QAA@XZ`** (340 B, `FindData("cheats_buffer")` straight to the epilogue), not GlitchFinder's ctor. Map row renamed → **+1 / +264 B** (`?CheatsInit@@YAXXZ` crosses: its `bl` now names the right callee). Our ctor was 364 B because of a DC3-era `SetName("cheats_mgr")` retail never compiled — guarded `HX_NATIVE`, body now byte-exact; the row itself still reads 0 because it is homed in StringTable's block → **proposal BB-1** (`docs/decomp/W16BB_map_proposals.json`), priced **exactly +1 / +340 B**, barred to this lane. Item 2 (`6ece6371`): AW-2's census was wrong **both ways** — **18** references not 17 (a `b` tail-call from `?ForceTrackerStars@Game@@` that a `bl`-only census cannot see) and only **2 of 18** sites chargeable (15 owners unnamed, 1 XDK). `0x82a478d0` = `?SetVolume@DirectInstrument@@QAAXH@Z` + T1 alias `?ForceStars@TrackerManager@@QAAXH@Z` (retail `908300004E800020`, both our COMDATs the same 8 B, 0 relocs). **Discriminating control**: map row alone **−8 B**, with alias **Δ0**. A retracted `.pdata` decode (wrong shift; correct layout big-endian `PrologLen = w & 0xFF`, `FunctionLen = (w >> 8) & 0x3FFFFF`) recorded. Item 3 (`6605cb4a`): CLAUDE.md "3 mispaired" paragraph dated as **fixed 08-31 `b341d7ab`**, with its negative result (−1 / −40 B) refuting the paragraph's own "moves matched bytes" prediction. Item 4: `fn_827C91A0` (= `SecondsToTick` by convention) **declined** with four reversal criteria; the retail string witness is **vacuous** (`SecondsToBeat`, mapped and declared, also reads 0). Aggregate fuzzy moved −0.000038 pp — the measured cost of the naming bet. Rebased over AZ (1 alias conflict round auto-resolved, deltas identical). Gates: build rc=0, ruler rc=0, manifest rc=0, alias validate PASS 0 contradicted, `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`. NOT done: BB-1's splits re-home (barred), `StringTable.cpp`'s `#include "utl/GlitchFinder.cpp"` (dead only after BB-1), `GlitchFinder.cpp/.h` (nothing in retail to match), 15 unnamed + 1 XDK `SetVolume` sites (unadjudicable). Record: `docs/decomp/W16BB_GLITCHFINDER_CTOR_SETVOLUME_CALLERS_CLAUDEMD_2026-09-15.md` |
| **W16-BA** | opus | **+5 / +1,012 B** (43,497 → 43,502; 4,034,268 → 4,035,280 B; merge `cf8fac201e41`). Map 3 rows (+2 renamed), alias 1 membership on the existing `0x826c3888` group, `splits.txt` one `.text` block EventTrigger → EventAnim. AX-1 (`07dc8e22`): `0x824c97d8` is **EventAnim's `list<EventCall>` range ctor**, not EventTrigger's `ProxyCall` twin — rename + block move in one commit, predicted +116 B, measured **+1 / +116 B exact**; the unit lost **2** rows not 1 (the second is the ctor's EH funclet `fn_824C984C`), and node size cannot discriminate `T` (`addi r6,r29,8` is the `_List_node_base` offset for every `T`). AX-2 (`b10c6120`): the splice row's `blr` fold is a **9th membership on an EXISTING group**, not a new group as briefed — predicted +156 B, measured **+1 / +156 B exact**, three-leg byte proof. Item 3 (`91c3db0e`): **the briefed premise was false** — `ObjDirPtr<ObjectDir>` is used binary-wide, the RhythmDetector include stays; the real defect was a **wrong map name**: `?ObjDirPtr<ObjectDir>::operator=` sat at `0x82817ae8` (that body calls `ObjDirPtr<HamListRibbon>::PostLoad`) but belongs at `0x82270690`; named it + `LoadFile` at `0x82270848`, nulled the disproved copy → predicted −6 / −344 B, measured **+3 / +740 B** (the two RhythmDetector rows + `UIFontImporter::~ObjDirPtr<UILabelDir>` 88 B, freed by the nulling). Item 4: `fn_824C95F8` = `PropSync<ObjList<EventAnim::EventCall>>` (`li r3,0x20` ⇒ `sizeof(EventCall)==24`), **proposed, not moved** — cannot reach 100 until an 8-byte liveness gap closes. Rebased over BB (0 conflict rounds, deltas identical). Gates: build rc=0, ruler rc=0, manifest rc=0, alias validate PASS 0 contradicted, `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`; main gate identical to worktree gate. NOT done: the complete `0x82817ae8` repair (splits re-home `UILabel.cpp` → `HamNavList.cpp` + rename to the `HamListRibbon` spelling, 300 B row — outside ownership); item 4's re-home (blocked on the liveness gap); `fn_822709A8` (a funclet, left unnamed). Refuted traps: "18 objs" is 18 on disk / 17 declared; `fn_824C97A4` is 40 B not 52; the "`ProxyCall` is 4 bytes too large" struct hypothesis is **REFUTED on our own 100% rows** — do not reopen. Record: `docs/decomp/W16BA_EVENTANIM_LIST_CTOR_BLR_ALIAS_RHYTHMDETECTOR_OBJDIRPTR_2026-09-15.md` |
| **W16-BC** | opus | **+4 / +1,000 B** (43,502 → 43,506; 4,035,280 → 4,036,280 B; merge `ce520e819180`). Map 6 rows added + 1 repaired, alias 1 new group, `symbols.txt` one merge, `PhraseAnalyzer.cpp` +20 lines (house guard), `splits.txt` untouched. Item 1 (`c1f4f60c`): `0x82787ed0` is **`RGRollChord`'s 24 B `_M_insert_overflow_aux`**, not `pair<int,int>`'s — refuted on bytes (0x18 stride; `retail_bodytwins` 1 / `our_bodytwins` 2 ⇒ fold by pigeonhole), so the map row was renamed and the fold installed with `SongSection` folded onto the `RGRollChord` survivor. Predicted +1 / +128 B, measured **+2 / +532 B** — the predicted *orientation* was wrong (SongParser's 404 B overflow_aux crossed too). Item 2 (`1388d7d2`): retail's `Analyze@PhraseAnalyzer` (`fn_8278C160`, 408 B) carries **no relocations for `MakeString`/`SongFullPath`/`TickFormat`/`TrackName` and none for `Verify`** — the dev-build diagnostic block and the `Verify()` call are `MILO_DEBUG && HX_NATIVE` guarded; the source fix ALONE measured Δ0 (row anonymous), naming it crossed it at 408 B; naming `fn_8278B638` = `IsUnisonPhrase` made **dtk merge `fn_8278B66C` into it** (one 60 B symbol, `total_functions` 69,217 → 69,216), the unpredicted +60 B. Predicted +408 B, measured **+2 / +468 B**. Two rows (`fn_8278B3D8` = `PhraseData` ctor, `fn_8278B678` = `NumPhrases`) named on **sole-caller identity with body compare failing** (60.1 / 48.4) — a stated deviation, callers held at 100, reversible at zero byte cost. Item 3 (`ad6bb033`): AZ §3.3's two ClipDistMap-block rows settled from the caller side — `fn_822ABCE0` (124 B, `OldColorOption` resize helper) and `fn_822ABDD8` = **`OutfitConfig::NewObject`**, which exposes a real allocation-path bug (ours uses class `operator new`; retail relocates `StaticClassName`+`MemAlloc`+ctor, the path `CharIKFoot`/`RndTransProxy` already take). Predicted Δ0, measured **Δ0 exact**, FELL OUT 0. Rebased over BA (0 conflict rounds, deltas identical; record cites pre-rebase shas). Gates: build rc=0, ruler rc=0, manifest rc=0, alias validate PASS 0 contradicted / 1,650 groups, `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`; main gate identical to worktree gate. NOT done: re-home + rename `0x822ab0b0` (`_M_fill_insert<TransformCrowd>` → `<OldColorOption>`, needs the AZ §3.4 HamCamTransform splits move in the same commit, **+232 B** fully evidenced); name `fn_8278C2F8` = `??_GFillInfo` (owner is `SongData.cpp`, wrong unit reads 0% forever); the `OutfitConfig::NewObject` source fix; 4 inert CHASED-T1-PROVEN overflow_aux memberships (deliberately not installed — no caller charges them); HamRibbon 99.607 (`Key<Transform>` **REFUTED on bytes**, do not brief as covered); `get_allocator<RawPhrase>` (BA's `0x826c3888` group, needs our-side COMDAT identity). Instrument note: a malformed `--pairs` schema makes `icf_pair_adjudicate.py` return a confident "CHASED T1: REFUTED" instead of an error (tell: `survivor : survivor` echoed as a symbol name). Record: `docs/decomp/W16BC_SONGSECTION_OVERFLOW_AUX_CALLEE_PHRASEANALYZER_ANON_ROWS_2026-09-15.md` |
| **W16-BF** | fable | **+6 / +816 B** (43,506 → 43,512; 4,036,280 → 4,037,096 B; merge `61894b46dc11`). `src/system/world/EventAnim.cpp` +2 TU-scoped defines, `splits.txt` two headings (EventTrigger −1 block, EventAnim merged to one line `0x824C9430–0x824C9878`), map 3 rows added, aliases untouched. Item 1 (`5e9aa0d6` + `9e3d634e`): the 8 B gap in `fn_824C95F8` was **intra-TU leaf knowledge** — retail keeps `i+1` in volatile r6 across `T item(owner)` because `EventCall::EventCall(Hmx::Object*)` is a same-TU leaf (both `ObjPtr` member ctors inlined) and MSVC exploits callee register-usage; ours called two out-of-line `ObjPtr` ctors, so r6 was assumed clobbered. T selects the shape via the leaf-ness of `T::T(Hmx::Object*)`, which is why `ProxyCall` already matched; BA's refuted struct-size hypothesis stayed closed. Fixed with `RB3_TU_OBJPTR_FORCEINLINE_CTOR` + `RB3_TU_OBJPTR_DEFER_OWNER` in the TU: PropSync COMDAT 388 B 97/97 words, ctor 19/19 vs `fn_824C8DD0`, overload 68/68 vs `fn_824C9270`, sibling `??$PropSync@` rows FELL OUT 0. Item 2 (`54646f45`, one commit): re-home + 3 COFF-read names (`0x824c95f8`, `0x824c8dd0`, `0x824c9270`); pre-registered **+776 B / +3 fns, measured exactly**. Item 3: a FuncInfo/UnwindMap scan of `band.exe` puts `fn_82271620` and all eight funclets `0x822715B0–0x822716E8` under `fn_82270E68` = **`App::App(int,char**)`** (the single-function `App.cpp:` block), NOT RhythmDetector — the 312 B re-home is **filed** (`docs/decomp/W16BF_map_proposals.json`, predicted −1 fn / −32 B, accuracy landing) for BD. Gate set-diff: CROSSED IN 6 rows / 856 B (the three named rows + 3 × 40 B funclets), FELL OUT `EventTrigger::fn_824C977C` 40 B (unit move). NOT done: `0x824c977c`/`0x824c97a4` unnamed (objdiff hides `__unwind*` symbols — naming would drop both rows from numerator AND denominator; the 21 existing `__unwind$` map rows are a tree-wide denominator hazard, flagged); `0x82270e68` unnamed (no `App.obj` to verify the spelling). Native `PASS 18/18 skipped=0`. Write-up `docs/decomp/W16BF_PROPSYNC_EVENTCALL_LIVENESS_GAP_REHOME_2026-09-15.md`. |
| **W16-BE** | opus | **+9 / +792 B** (43,512 → 43,521; 4,037,096 → 4,037,888 B; merge `01762c70ea0b`). `src/system/bandobj/{OutfitConfig,BandLabel,StarDisplay,MiniLeaderboardDisplay,MicInputArrow,ScrollbarDisplay}.h` NEW_OVERLOAD → OBJ_MEM_OVERLOAD (retail inlines the class's own `operator new` inside `NewObject`; MicInputArrow/ScrollbarDisplay had hand-rolled operator news built on the stale "tree-wide OBJ_MEM_OVERLOAD is noinline" premise), one commit per class with retail-byte evidence: six `?NewObject@` rows 86.929 → 100, 112 B each. `symbol_aliases.json` group `0x826c3888` folded 9 → 10 with `?get_allocator@?$vector@URawPhrase@@…` (FT-EMPTY + call-site role on retail's own surrounding words + exhaustion 26,970 bl targets / 4 bare-blr + blast radius exactly 1 reloc): `??0?$vector@URawPhrase@@` 99.833 → 100 (+120 B). Refuted: `??0OutfitConfig@@` is 564 B not 1016; INSDEL-1's "not source-addressable" verdict only tested NewObject-scope levers. Set-diff: 7 rows crossed / 792 B, 0 fell out; +9 fns includes 2 mpn-only unwind funclets. Native gate caught an LP64 compile error in the first hand-rolled `operator new` (fixed with MemMgr.h's own `HX_NATIVE` split). NOT done: `?NewObject@BandSong@@` (retail targets named `??0HamSong@@` — needs fold proof or map repair), `?NewObject@UIPanel@@` 68.4 (real layout divergence `li 0x68` vs `0x108` + CriticalSection ctor — struct-layout lane filed), `??0OutfitConfig@@` 564 vs 588 B, `??3` funclet residuals. For BD: `??2OutfitConfig@@` 8 → 60 B, `?NewObject@OutfitConfig@@` 100 → 112 B, 59 `__catch$N` renumbered +12. Native PASS 18/18. Write-up `docs/decomp/W16BE_OUTFITCONFIG_NEWOBJECT_SHAPE_RAWPHRASE_GETALLOCATOR_ALIAS_2026-09-15.md`. |
| **W16-BD** | opus | **+4 / +916 B** (43,521 → 43,525; 4,037,888 → 4,038,804 B; merge `3b479875b5c4`). SPLITS lane (sole `splits.txt` editor), five re-homes each priced by full-build `fuzzy==100` rowset set-diff: `??0CheatsManager@@` StringTable → Cheats (+340 B) and the dead GlitchFinder scatter-include dropped at Δ0; `0x822ab0b0` identified as `vector<OldColorOption>::_M_fill_insert` with splits block 3303 SPLIT rather than moved whole — `resize`/`_M_fill_insert<OldColorOption>` now pair under OutfitConfig (+124 / +108 B, the latter through a DistEntry/OldColorOption `overflow_aux` fold alias); `0x82817ae8` identified as `ObjDirPtr<HamListRibbon>::operator=` and re-homed UILabel → HamNavList (+300 B, `??1?$ObjDirPtr@VUILabelDir@@` 88 B back) — the pre-existing `4?$ObjDirPtr` alias group at that address REFUTED on retail bytes (the reloc at +0x48 discriminates `PostLoad<ObjectDir>` from `PostLoad<HamListRibbon>`), withdrawn with record and the group re-addressed to `0x82270690`; `fn_8278C2F8` = `??_GFillInfo@@UAAPAXI@Z` split into SongData (+76 B); App::App's eight EH funclets RhythmDetector → App (−1 / −32 B, the accurate home BF filed). Set-diff 5 in / 948 B, 1 out / 32 B; `total_code` unchanged; native PASS 18/18. NOT done: block 3304 unmoved (no OutfitConfig evidence for any of its 12 bodies); `0x822ab128` NOT named — AZ §3.4's `<BandPatchMesh>` reading refuted on retail bytes (copy-construct slots resolve to `??0TransformArea@@QAA@ABV0@@Z`) and the `<TransformArea>` reading also refuted, filed as an open contradiction against the map's `0x822a84d8` row; `0x822ab3e0` (588 B) unnamed; `<HamScrollSpeedIndicator>` alias declined as proven-but-unwitnessed. Lesson for briefs: dtk's aggregate target `.text` is zero-filled (bytes live in per-function COMDATs), so dumping it to inspect a tail reads like padding. Write-up `docs/decomp/W16BD_CHEATS_OLDCOLOROPTION_OBJDIRPTR_FILLINFO_REHOMES_2026-09-15.md`. |
| **W16-BG** | opus | **+23 / +1,024 B** (43,525 → 43,548; 4,038,804 → 4,039,828 B; merge `adac046e7dfe`). MAP-ONLY lane, the 24 `__unwind$`/`__catch$`-named rows from laneAP `783ebf34`: all 24 adjudicated as TRUE EH funclets against every one of the 8,541 retail FuncInfos (21 unwind / 3 catch, parents via the 8-byte EH prefix, 0 of 24 base objs define the asserted name); objdiff hides those prefixes on the TARGET side at row emission (`report.rs:1225`) so they sat in NEITHER numerator nor denominator while ~26,000 sibling funclets pair by masked bytes. Deleted the 24 names ⇒ `total_functions` 69,216 → 69,240 (+24, predicted exactly), `total_code` 10,246,004 → 10,247,068 (+1,064, exact), 23 of the 24 crossed in at 100 (+1,024 B), 0 fell out; code% went UP +0.0059 pp (the brief predicted down; the lane pre-registered up). Sibling sweep: 0 further Hidden-pattern or placeholder-named values in 29,484 rows. Not done: 9 of 24 pinned away from their parent unit (filed, no splits edit); 8 anonymous parents unnamed. Write-up `docs/decomp/W16BG_FUNCLET_NAMED_MAP_ROWS_DENOMINATOR_2026-09-15.md`. |
| **W16-BI** | opus | **+2 / +392 B** (43,548 → 43,550; 4,039,828 → 4,040,220 B; merge `82f4a63488cf`). `0x8268b9a0` (76 B, `UI.cpp:` one-function island inside BandUser's span, fuzzy 68) is **`?NewRemoteBandUser@BandUser@@SAPAVRemoteBandUser@@XZ`**, not `?NewObject@UIPanel@@` — five retail-byte discriminators agree (`li r3,0x108` = `sizeof(RemoteBandUser)` 264 vs `sizeof(UIPanel)` 104 by `/d1reportSingleClassLayout`; 17/17 non-reloc words; single `bl` caller in `??0BandUserMgr@@`). `0x8268b4e8` (316 B) = `??0RemoteBandUser@@QAA@XZ`, 60/60 words, 19 relocs in order. Island dropped, BandUser's flanking blocks merged, `.pdata` re-derived by dtk. Retail's real `?NewObject@UIPanel@@` found at `0x82802418` (100 B, referenced only by `?Init@UIManager@@`'s `REGISTER_OBJ_FACTORY`) and named; it and `0x828023a0` (= `?NewObject@UIScreen@@`) sit in `UIColor.cpp:` — re-home **filed** (+1 fn / +100 B each expected). Predicted +2 / +392 B, FELL OUT 0, `total_*` unchanged — measured exactly. ⚠ Withdraws W16-BE's "UIPanel lacks an embedded `CriticalSection`" layout note: the 264 was `sizeof(RemoteBandUser)` read off this mis-named row. |
| **W16-BH** | opus | **+2 / +160 B** (43,550 → 43,552; 4,040,220 → 4,040,380 B; merge `e3de73bd9ee0`). `0x8227a828` is `??0BandSong@@QAA@XZ`, not `HamSong`'s ctor — all four vtables the 164 B ctor installs resolve through their `??_R4` COLs to the single TypeDescriptor `.?AVBandSong@@` (`0x82c6b6f4`), and `HamSong` occurs 0 times in either retail image. Because only `BandCharacter.obj` defines that name, the rename alone would have been a permanent 0%, so the lane adjudicated the other two `Ham.cpp:` blocks (the `Song` slot of BandCharacter's `OBJ_CLASSNAME` run; a boundary that split `?StaticClassName@BandSong@@` from its own guard thunk; three 8 B leaf accessors), deleted the `Ham.cpp:` entry and merged all three into `BandCharacter.cpp:` — our `hamobj/Ham.cpp` is DC3's `HamInit`/`HamTerminate`, registering seven classes RB3 does not have. Naming `0x8227a910` (`?ClassName@BandSong@@UBA…`) then exposed a pre-existing defect: `0x8227b050` was named as BandCharacter's ClassName adjustor but branches to BandSong's; repaired against all 452 retail adjustor thunks (one per destination, no fold). Corrected the brief: `0x8227a7a8` was never unpinned. Report-only: 5 `Ham*` tokens across 9 named map rows (`HamMove` ×7 first) have no RTTI or substring witness in retail. Filed: `0x82289748` is BandCharacter's real adjustor, proven, but sits UNPINNED in a 12 B gap (one splits line + one map key). Set-diff 9 in / 564 B, 7 out / 404 B (six moves + the thunk), predicted exactly. Gates green, `NATIVE_GATE_RESULT verdict=PASS … skipped=0`. |
| **W16-BJ** | opus | **+2 / +32 B** (43,552 → 43,554; 4,040,380 → 4,040,412 B; merge `39872374106e`). BG's premise stands and the ICF rival is **refuted**: walking all 8,541 FuncInfos (UnwindMap `action` slots + TryBlockMap handlers), **fan-in is exactly 1 for all 26,321 funclet targets**; the one genuinely folded FuncInfo record (`0x820A1EF0`) owns zero funclets and is the positive control that the scanner can see `.xdata` folding. Geometry: every one of the 24 lies inside its own parent's contiguous funclet run. 24-row verdicts **HOMED 14 / MIS-PINNED 8 / ORPHAN 2**, the in-unit control passing 14/14. Pilot widened from 8 rows to **26 in 9 `.text` edits** because every donor block was composed entirely of the receiver's funclets (moving 1 of 3 would split a block). Predicted +2 fns exact; predicted +72 B, measured +32 B — the −40 B miss is `fn_82703AD0` crossing into `system/rndobj/Utl` at `mpn` 100 / `fuzzy` 99.5, i.e. a relocation-name charge the lane's relocation-masked twin comparator is structurally unable to price (its 8/8 positive control was confounded: every donor row was a `masked_equal` false pairing, which has an exact byte twin by construction). Set-diff 19 in / 676 B, 18 out / 644 B, 14 of them pure `unit::name` renames; zero collateral; aggregate fuzzy fell 0.001275 as intended (three rows lost false-twin credit). Tree-wide census, report only: MIS-PINNED **734 rows / 29,216 B**, of which **419 / 16,548 B read `fuzzy==100` on a false twin**; ORPHAN 1,352 / 56,168 B. New `tools/funclet_homing.py --validate`. Corrected the brief: heading is `Watcher.cpp`, not `TrackWatcher.cpp`. Not done: no tree-wide action, 2 orphans, 8 anonymous parents, 9 rows inside other lanes' bars, the charged site on `fn_82703AD0` (filed). Gates green, `NATIVE_GATE_RESULT verdict=PASS … skipped=0`. |
| **W16-BK** | opus | **+3 / +180 B** (43,554 → 43,557; 4,040,412 → 4,040,592 B; merge `a966b9db2da0`). The re-home BI filed, executed exactly: boundary `0x828024B0 → 0x828023A0`, two `.text` lines, `.pdata` re-derived. Predicted +1 fn / +100 B, measured **+3 / +180 B**, 0 rows fell out — the miss is the two anonymous `__unwind$` funclets (`fn_828023E8`, `fn_8280247C`, 40 B each) that followed their parents into `default/UI` and paired by byte signature within the new pool; **price a factory re-home as parent + Σ its funclets** (the first lane to price the rows BG made visible). The unexpected result: BI's identification of `0x828023A0` as `?NewObject@UIScreen@@` is **correct**, so the map row that carries that name today — `0x823f5c20`, scoring 100 on 72 B in `default/UI` — is **WRONG**: its ctor's vtable RTTI reads `.?AVNetSearchResult@@`, and its 100 is a **false credit** that survives only because the ctor callee is unnamed and `name_check` forgives placeholders. Filed as a coupled two-row map job (net ≈0 B, one false credit retired); the name-injectivity gate is necessary, not sufficient. Also **withdrew W16-BE's recommended `UIPanel` struct-layout lane** (BE doc annotated): both of BE's observations were made against `0x8268b9a0`, which is `NewRemoteBandUser` (`sizeof(RemoteBandUser)` = 264, exactly the `li r3,0x108`); the real `UIPanel::NewObject` at `0x82802418` reads `li r3,0x68`. Follow-ups filed: the `UI.cpp:` block `0x823F4A30–0x823F5C98` reads as a 4,712 B mis-pin (NetSearchResult ctor+factory inside); of 38 factory-shaped 0% rows only 4 (368 B) are re-home-shaped; `UIColor::Load` (`fn_82802240`) left at 0%. Leg A reproduced literally after a settle (the handed-over worktree ran 409 edges on first build). No src/map/alias/symbols.txt change. Gates green, manifest `c6474b1b6f771e4a`, `NATIVE_GATE_RESULT verdict=PASS … skipped=0`. |
| **W16-BL** | opus | **+1 / +12 B** (43,557 → 43,558; 4,040,592 → 4,040,604 B; merge `8c830054a4be`). CY-1's `f592571a` had **carved** the 12 B adjustor out of BandCharacter's own contiguous `0x82289710–0x8228A23C` block and moved the middle to `Line.cpp:` unnamed; its instrument was a relocation-masked byte test that on a single-branch thunk compares 8 fixed bytes, and `default/Line` holds ten such `RndLine::*$4PPPPPPPM@A@` thunks — it matched `Line.cpp:` **by construction**. Refuted three ways: the carve left a hole inside a contiguous range, the branch destination is unique (**504 thunks in `.text`, 504 distinct destinations**, exactly one targeting `0x822896e0`), and both map neighbours are BandCharacter thunks. Fix = merge the two carved blocks back (the inverse of the carve, no new boundary) and name the row; predicted **+1 / +12 B, measured exactly**, 1 row in (pairs by **name**), 0 out, 8/8 per-unit figures hit, no `.pdata` churn. One arithmetic miss recorded: code% delta pre-registered +0.000012 pp, true +0.000117 pp. The 19 `Ham*` rows: **0 renames, 0 withdrawals** — decisive retail-byte identifications for four (`BandLabelCountDoneMsg` via a `"count_done"` Symbol + RTTI; `ObjPtr<BandIKEffector>`, `ObjDirPtr<UILabelDir>` via `__RTDynamicCast` TypeDescriptors), yet **every one is un-renameable by a map edit alone**: 16 of 19 sit in units whose base obj is `system/hamobj/*.obj`, which defines only the DC3 spelling, so a rename turns a 100 into a 0 (−120/−128/−88 B, proven by COFF scan). **The binding constraint is pairing topology, not evidence** — a map rename and a splits re-home are one atomic transaction for a cross-lineage mislabel; the coupled patch (incl. deleting the drained `HamLabel.cpp:` entry) is filed. `HamMove` is 4 literal / 3 strict, not BH's 7; BH's §8.1 "UNPINNED" was wrong (pinned at `splits.txt:5162`). Dropped `0x8227b050` from `_bijection_arbitrary` (1,013 → 1,012, Δ0 as predicted). Vein sized: 18 `Ham*.cpp:` pins hold 33,708 B, ~10,288 B matched. Hit the binary-blind `grep` shim once (false "no `RndPointTest` symbols"; Python found 99) and re-derived the refusal. Not done: no `src/`, no alias edits (`0x82817a68`'s 100 may rest on 97 `ObjDirPtr@VUILabelDir` alias mentions — flagged), no headings outside `Line.cpp:`/`BandCharacter.cpp:`. Gates green, manifest `67018c24fa136922`, `NATIVE_GATE_RESULT verdict=PASS … skipped=0`. |
| **W16-BM** | opus | **-35 / -1,096 B** (43,558 → 43,523; 4,040,604 → 4,039,508 B; merge `4f7d819eceb5`). SPLITS-ONLY tree-wide re-home of the **MIS-PINNED** EH funclets to their parents' units, driven by `tools/funclet_homing.py` (+516 lines; parent fan-in `{1: 26321}`, no ambiguous parents). Census **706 rows / 28,176 B** vs the briefed 708 / 28,248 B — the 2-row gap is `0x8227A800` (32 B) + `0x8268B9EC` (40 B), already re-homed by BH/BI. **MIS-PINNED 706 → 6**; the 6 survivors are the rows barred to the then-live BK/BL (`UI.cpp`, `Line.cpp`, `BandCharacter.cpp`, `BandCamShot.cpp`). The delta is **negative on purpose**: batch 1 (funclets into units where a same-byte twin was already pairing) **+145 / +5,756 B** vs predicted +146 / +5,764; batch 2 (funclets that had been earning credit by byte-signature pairing against **alien** twins in the wrong unit) **−180 / −6,852 B**, `masked_equal` −180 — i.e. that credit was never the parent's, and removing it is a truer attribution (accuracy over headline). One miss recorded: `<100→100` predicted 0, measured 7 (+352 B). Two mechanisms named that the pin-neutrality doc lacked: **receiver-side sibling pairing** (a pre-move twin probe is a LOWER BOUND on post-move credit) and **donor-side liberation** (an alien funclet's presence denies the rightful occupant its pairing). Gate: 393 rows / 14,648 B in, 396 rows / 15,744 B out; `total_code` unmoved. ORPHAN sweep sized: **1,352 rows / 56,168 B, 72.2 % above `0x82A00000`**. Recommends pinning `0x822FC508` (3,428 B, `auto_03_822FC4F8_text`) to `VocalTrackDir.cpp` — 125 rows / 5,348 B, predicted neutral, **not content-verified**. Not done: `~/tmp/bm/mutate_assertions.py` is not yet registered in `scripts/test_tools.py`; no `src/`, map or alias edits. Manifest `tree_sha256=8608ea7b3d18d56f`, `NATIVE_GATE_RESULT verdict=PASS … skipped=0`. |
| **W16-BN** | opus | **+10 / +1,244 B** (43,523 → 43,533; 4,039,508 → 4,040,752 B; merge `8bdc70ba1185`). MAP + SPLITS + one ported TU: the 72 B factory at `0x823f5c20` settled as `?New@NetSearchResult@@SAPAV1@XZ` (RTTI: vptr `0x820599D4` → COL → `.?AVNetSearchResult@@`; control `0x827f1e90` → `.?AVUIScreen@@`; `New` vs `NewObject` settled by address-taken — zero `lis+addi` formation sites for `0x823f5c20` in `.text`, and `REGISTER_OBJ_FACTORY` must form the address). Ported `network/net/NetSearchResult.cpp` from the rb3-Wii oracle (52 → 71 lines; measured Δ0 as the no-heading control). Coupled transaction: **8 new map names**, `?NewObject@UIScreen@@` moved to `0x828023A0`, `UI.cpp:` `.text` cut at `0x823F56F8`, new `network/net/NetSearchResult.cpp:` heading — new unit **15/15 rows, 1,396 B, 100 %**. CROSSED IN 16 rows / 1,444 B, FELL OUT 5 rows / 200 B (the same funclets leaving `default/UI`); `total_code` unmoved. **Four of eight proposed spellings were wrong** (virtual `U` not `Q`; `Handle` takes non-const `PAVDataArray`) — caught by reading the BUILT COFF before writing the map, else four permanently unpairable 0 % rows. Under-predicted by **+1 fn / +48 B**: `?AllocateNetSearchResults@SessionSearcher@@` crossed because its call-site relocation name stopped being charged ⇒ rule: **census the `bl` callers of an address before pricing its rename**. Not done: `0x823f5270` (`??_GAutomator@@`, 76 B) left as known-false credit — MessageBroker's slot-0 dtor, but no object emits `??_GMessageBroker@@`, so renaming installs nothing at a certain −76 B; HEAD span `0x823F4A30–0x823F56F8` (28 rows / 3,148 B) needs source, filed; two vendor-band blocks under `UI.cpp:` (`0x82B801D8–0x82B80320`, `0x82B8032C–0x82B80FC4`) filed as wrong-unit-pin shape; `?ReadStats@MatchmakerPoolStats@@` and `?HasCompatibleInstruments@BandMatchmaker@@` refuted (already 100, correctly homed). Manifest `tree_sha256=c797628ddedee41c`, `NATIVE_GATE_RESULT verdict=PASS … skipped=0`. |
| **W16-BO** | opus | **+10 / +1,008 B** (43,533 → 43,543; 4,040,752 → 4,041,760 B; merge `d68ec0fedc43`). SOURCE + MAP lane over the "unemitted factory" stratum (fuzzy-0 rows whose base obj never emits the symbol): ten rows adjudicated on retail bytes, eight closed by source + 8 map names (`??0UnkTU5GuidePitchOwner@@QAA@VSymbol@@@Z` 144 B in `Game.cpp`, `?Load@UIColor@@` 120, `?Init@ExternalMic@@` 120, `?Init@PrefabMgr@@` 120, `?NewArray@JsonConverter@@` 108, `?OnLoad@MusicLibrary@@` 96, `FxSendPitchShift360::SyncEffectParams` 76 + `CreateFx` 72, `?NewNetMessage@NewUserMsg@@` 72) and two FILED as unit re-homes rather than edited (`0x823e3e90` NetSession/MidiInstrument boundary; `0x825aaff0` LockStepMgr block inside `SetlistMergePanel.cpp`, exact lines in doc §5). Two new memberships in the existing `vector<T*>::push_back` fold group (`ExternalMic*`, `JsonObject*`), chased T1 with a recorded self-correction. No `splits.txt` edits. CROSSED IN 11 rows / 1,008 B, FELL OUT 0. Native PASS 18/18 skipped=0 on both worktree and main gates; wt-vs-main gate diff 0 lines. Follow-up filed in doc §10: ~16 rows / 1,544 B of the same shape (`return new X()` bodies, richest in `default/JsonUtils`). Doc `docs/decomp/W16BO_UNEMITTED_FACTORIES_SOURCE_MAP_2026-09-15.md`. |
| **W16-BP** | opus | **+7 / +220 B** (43,543 → 43,550; 4,041,760 → 4,041,980 B; merge `24fee53a3a0e`). SPLITS re-home lane over the Ham vein (BL §10/§10.1): retail has ZERO `.?AVHam*@@` RTTI, so all 18 `Ham*.cpp:` headings were DC3 labels on RB3 code; every edit is a re-home adjudicated on retail bytes with BOTH legs of the instrument (GEOMETRY: block interior to one destination heading; PAIRING: destination base obj DEFines every named row). Headings 18 → 14: `HamLabel.cpp` (known-answer fixture, Δ0), `HamSongData.cpp` → `SyncStore.cpp`, `HamScrollSpeedIndicator.cpp` → `ReviewDisplay.cpp`, `HamCamShot.cpp` drained (15 true-hole blocks + 8 blocks → `CharEyes`/`BandList`/`CrowdAudio`); 6 headings partially re-homed; 8 FILED NEEDS_SOURCE — ⚠ `MiniLeaderboardDisplay` among them must NOT be re-homed. 2 map spellings corrected (`0x82340580`/`0x82340940` → `BandLabelCountDoneMsg`). Net +7 fns / +220 B is all masked-equal disclosure (masked 23,047 → 23,054): CROSSED IN 34 rows / 2,736 B, FELL OUT 30 rows / 2,516 B — the Ham-labelled twins fall out as the correctly-homed rows cross in. Negative result recorded in doc §2: geometry alone is ~66 % precise and DEF is vacuous for generic template COMDATs, so re-homing recovers PAIRING, never CORRECTNESS. `funclet_homing` MIS-PINNED 6 → 1 (survivor `0x823F4A30`), topology census 0 defects. Native PASS 18/18 skipped=0 on both worktree and main gates; wt-vs-main gate diff 0 lines. Doc `docs/decomp/W16BP_HAM_VEIN_REHOME_2026-09-15.md`. |
| **W16-BQ** | opus | **+55 / +924 B** (43,550 → 43,605; 4,041,980 → 4,042,904 B; merge `0b8c74e66ef3`). GemTrackDir ctor block re-homed from VocalTrackDir; `??0GemTrackDir` + both TrackDir NewObject factories named; VocalTrackDir ctor hole pinned; OBJ_MEM_OVERLOAD_INLINE_DEL on both dirs. Honest floor +4, rest masked_equal. Brief premise refuted (COMDAT-span vs .pdata-extent artifact). Gates green wt+main. Doc `docs/decomp/W16BQ_GEMTRACKDIR_CTOR_REHOME_2026-09-15.md` |
| **W16-BR** | opus | **+21 / +3,056 B** (43,605 → 43,626; 4,042,904 → 4,045,960 B; merge `5c4a5f7325af`). TourChar UI.cpp hole re-homed, TourCharRemote ported (both 100%), dsp cluster port (IIRFilter, PitchDetector; IIR4PoleFilter 224 B from retail), Chunk32 + three proven aliases. Honest floor +19. Seam corrected to `0x82B807C0`. Filed: `utl/MemMgr.h` MemAlloc macro forces align 0. Gates green wt+main. Doc `docs/decomp/W16BR_TOURCHAR_PITCHDETECTOR_REHOME_GETFRET_ALIAS_2026-09-15.md` |
| **W16-BS** | opus | **+25 / +9,004 B** (43,626 → 43,651; 4,045,960 → 4,054,964 B; merge `c207d1aeb297`). Five Tour-cluster reloc-name sites adjudicated on retail bytes (4 WRONG_MAP_NAME + 1 PROVEN_FOLD, one T1 alias); `Tour`'s two maps are `hash_map` not `std::map` (three independent retail reads; rb3-Wii oracle wrong here); `Hmx::Object::SyncProperty` is retail's 72 B empty PROPSYNCS terminal, `Tour::SyncProperty` comparison fixed. Honest floor +25. Refuted sympair-W18 `A_PROVEN_FOLD STRONG 25` at `0x8235cd38`; brief's InitializeTour 240 B is 360 B. Not done: spurious `?HasTourDesc@Tour@@` at `0x8258bab8` (BandProfile cluster, not renamed), naming `0x8235c2e0` needs an A/B. Gates green wt+main. Doc `docs/decomp/W16BS_TOUR_CLUSTER_RELOC_NAME_RESIDUE_2026-09-15.md` |
| **W16-BV** | opus | **+0 / +0 B** (43,651 → 43,651; 4,054,964 → 4,054,964 B; merge `ac629dedeaa0`). Whole-binary Δ0 on all four measures, aggregate `fuzzy_match_percent` 49.804825 → 49.826508 — a sub-100 residue lane registers on fuzzy, not on `matched_code`. Four rows moved: ApplyFontStyle 6.73 → 74.32 (rb3-Wii oracle packs a `Color32`, retail copies `Hmx::Color`), `fn_822EF438` identified as `GemTrackDir::PreLoad` and re-homed via splits+map 0 → 91.62, SetConfiguration 82.40 → 88.64 (BQ §9.2 wrong — the `__savegprlr_26/_24` gap was statement ORDER), `fn_822FA7E0` identified as `VocalTrackDir::Save` but deliberately NOT named (our Save is a 4 B stub). Three T1 fold candidates: PlayIntro refuted, ReleaseSmasherPlate refuted, `??3GemTrackDir` unadjudicable — no alias installed. Filed: `fold_thunk_gate` admits 7 pairs / 1,507 sites binary-wide; SetRange's one real charge is retail signed `cmpwi` vs our `cmplwi` on `ObjPtr<RndGroup>+8` (tree-wide question). Not done: PostLoad (57 sites), ApplyFontStyle 74 → 100 (frame-pointer shape), SetConfiguration's last 15 instructions, `??0GemTrackDir`. Gates green wt+main. Doc `docs/decomp/W16BV_VOCALTRACKDIR_GEMTRACKDIR_RESIDUE_2026-09-15.md` |
| **W16-BT** | opus | **+3 / +560 B** (43,651 → 43,654; 4,054,964 → 4,055,524 B; merge `99d8d70a631f`). Retail `bl` census of the allocators: 300 `?MemAlloc@@YAPAXHH@Z` sites (285× align 0, 6× `0x10`, 1× `0x80`, 1× `0x20`, 7 pass-throughs) and 21 `_MemAllocTemp` sites all align 0, after three instrument defects were caught (capstone `regs_access()` unimplemented for PPC; `.pdata` length decode wrong twice before agreeing with `symbols.txt` on 57,621/57,621 extents). The one live divergence: `XboxAllocator<T>` in `synth_xbox/FftIpp.h` — retail passes align `0x10`, guards `count == 0` and null-guards the free; our 5-arg debug spelling had all three swallowed by the `MemMgr.h` macro (MSVC's traditional preprocessor expands a macro called with fewer args). `MemMgr.h` arity-dispatched, proven with `cl /E`, Δ0 across 1,050 recompiled TUs. PitchDetector: the rb3-Wii oracle's Hz→note formula is WRONG, retail right (`39.863136 * log10(hz) - 36.376316`), hidden behind uncharged `lbl_` loads — behavioural fix. Brief corrections: PitchDetector address triple garbled, `_MemAllocTemp` IS in the map, ShiftedDotProduct is a known VMX128 wall. Not done: `Detect@VibratoDetector` (wrong skeleton), ShiftedDotProduct (VMX128 at `0x82B81758..`), `??0IIR4PoleFilter` (object `0x60..0xE0` unidentified), AnalyzeBlock's 122 charges, `0x82bbb3f0` align-`0x20` site (unpinned XDK). Gates green wt+main. Doc `docs/decomp/W16BT_MEMALLOC_ALIGN_CENSUS_DSP_RESIDUE_2026-09-15.md` |
| **W16-BU** | opus | **+24 / +11,444 B** (43,654 → 43,678; 4,055,524 → 4,066,968 B; merge `79ca02bd9b01`). Tour-cluster residue after BS §5: `??0Tour@@` re-homed from `BandSongMetadata.cpp` to `Tour.cpp` (the 660-vs-300 B reading was a COMDAT-span reader artifact; the ctor is word-identical to retail) +5 / +384 B; all four Tour source rows closed (BS's `SetTokenFmt<char*>` site was already alias-forgiven — the defect was evaluation order of the `String` vs the local-static guard); `0x8258bab8` = `BandProfile::HasCampaignKey` (not `Tour::HasTourDesc`) decoded off the compiler layout, renamed + re-homed + source repaired; `0x8235c2e0` = `Hmx::Object::SyncProperty` map+alias measured **+14 / +10,388 B** against a pre-registered prediction of 0 (the lane's "zero call sites" scan was vacuous — filter keyed on fields objdiff batch rows lack). Withdrew a fabricated 26-spelling alias before landing. Not done: `fn_8258BB08` 924 B (absent source), 3 `??0Tour@@` funclets at mpn 100/fuzzy<100, the 188 stale alias groups. Doc `docs/decomp/W16BU_TOUR_RESIDUE_CTOR_UNIT_BANDPROFILE_SYNCPROPERTY_2026-09-15.md`. |
| **W16-BX** | opus | **+1 / +528 B** (43,678 → 43,679; 4,066,968 → 4,067,496 B; merge `205586666f8b`). VocalTrackDir/`ObjPtr` residue after BV: the tree-wide `ObjPtr<T>`+8 signed-compare question is **REFUTED as a layout fact** — the word is a `T*` and `cmpwi` vs `cmplwi` is a per-site *spelling* (a direct member test emits `cmpwi`, binding to a `T*` local emits `cmplwi`; retail uses both, so it is a per-row diagnostic, not a sweep; census `tools/census/w16bx_objptr_bind_{census,rowcheck}.py`); `VocalTrackDir::SetConfiguration` 88.64 → 100 (+1 / +528 B, the whole delta); `fn_822EE768` named `??1GemTrackDir@@UAA@XZ` (0% unpaired → 86.46, +0 B by construction); fold-thunk lever re-gated with `fold_thunk_gate.py --tier` — DRAINED, and the 1,180-site pair rests on FT3 alone while the 154-site pair is FT-EMPTY (flagged, not acted on); `PostLoad` hoist was NEGATIVE (96.39 → 93.74, reverted — the ctor/dtor becomes unconditional; nested-scope / displacing-local arrangements untried); `??0GemTrackDir@@QAA@XZ` 2,548 B @79.27 untouched. Doc `docs/decomp/W16BX_OBJPTR_SIGNED_COMPARE_POSTLOAD_FOLDTHUNK_2026-09-15.md`. |
| **W16-BY** | opus | **+2 / +868 B** (43,679 → 43,681; 4,067,496 → 4,068,364 B; merge `31576aa36ccd`). BandWardrobe residue (`src/system/bandobj/`): `FindBestScoringHint` 55.96 → **100** (+828 B, the whole byte delta) — the brief's hypothesis was **INVERTED**: our body was 552 B vs retail's 828, i.e. *missing* code, not extra; six static `Symbol`s + a local static `Message` + a temporary-lifetime fix closed it and its two `REGISTER_SWAP` charges dissolved (12th instance of "swap is a symptom"); `ValidGenreGender` 50.28 → 99.89 fuzzy / mpn 100 (+1 fn, 0 B; `MILO_ASSERT` form + static `GetGenreGenderFlags`; the last 0.11 % is an `and.` operand order three spellings could not flip); `OnEnterVignette` 89.40 → 91.95 (headline-neutral, residue `__savegprlr_14` vs `_15`); **6 of 12 anonymous rows identified on retail bytes** (`AddDircut`, `OnGetMatchingDude`, `Merger` copy-ctor/`operator=`, `InstrumentMatch`, `NewObject`; Δ0 B by construction, rows 0 → 53–95 %); `0x82332298` REJECTED as `OnEnableDebugInterests` despite an exact 116 B size match (0x64 vector stride, no `DataNode` traffic); **220 B of the "12 rows" are not functions** — dtk carved two bodies into five rows (proven on symbolic branch targets), our counterparts are already exactly 88 B / 132 B ⇒ a carve fix is the cheapest structural item left. Not done: all five near-crossers declined (no charge names a source construct; `SetVenueDir`'s `MakeString` diff is an uncharged ICF fold); `SyncProperty` stays drained; the `SelectExtra` alias NOT installed, and the briefed 1,308 B alias lever is really **388 B** — `_S_sort<Symbol>` / `_S_sort<unsigned int>` sit at two distinct mapped addresses, so an alias is a fatal fabricated-alias refusal and `OnSelectExtras`' 920 B is uncollectable. Doc `docs/decomp/W16BY_BANDWARDROBE_SCORINGHINT_GENREGENDER_VIGNETTE_ANONROWS_2026-09-15.md` §7 tabulates the overturning evidence per row. |
| **W16-BW** | opus | **+1 / +312 B** (43,681 → 43,682; 4,068,364 → 4,068,676 B; merge `3b758bef0eaf`). The dsp residue BT left: the brief offered an out — if the retail accesses at `+0x60..+0xE0` off `IIR4PoleFilter*` were VMX128-width only, the ctor legitimately never inits that region — and the lane closed it the other way, then matched the row anyway. A whole-binary sweep bounded the population to **two** retail bodies (440 B), making it a proof rather than a sample: the tail is written (5 indexed `stvx` + 2 `std` pairs) and **read by nothing, anywhere**; "never read" is not "need not be written" for a matching target. Layout was necessary and not sufficient — rolled poles 1..3, store order, `align(16)` and assignment order closed `??0IIR4PoleFilter@@` 10.1923 → **100.0** (312 B, the whole byte delta). `?Detect@VibratoDetector@@` 20.8617 → **96.5426** on one folded-`d` defect; `?AnalyzeBlock@PitchDetector@@` 85.8584 → **93.2404**; `Time2IirA` unchanged but its CALLER moved — BR §10's Δ0 is now EXPLAINED rather than reproduced: retail's body has a `time>0` guard and the `1.0f -` inside, ours was a branch-free one-liner `/Ob2` inlines unconditionally, so **inlining follows the callee's SHAPE, not its placement** and no placement change could ever have moved it (+3.0 pp on AnalyzeBlock). Two further BT §8 claims REFUTED: "not layout-blocked", and "f28/f29 dissolves after the integer cluster" (it persists at idx 7/120/121/169 through two later fixes). Two method traps propagated: a displacement-keyed xref sweep is structurally BLIND to VMX128 `stvx` (indexed, offset in `rB`) and returns a false negative shaped like a decisive result; and `git diff main..<branch>` is NOT a lane's patch once main has moved (it listed five other lanes' files as reversals) — diff against the merge-base. NOT done with overturning evidence: `ShiftedDotProduct` untouched (out of scope, no evidence found against the VMX128 wall), Detect's 24 residual charges are permuter-class (one lever regressed 16 pp, one inert), AnalyzeBlock's 2 `bl` sites are ICF fold-alias and closable by no source work; the open thread is AnalyzeBlock idx 221, retail `lwz r8,0xc(r30)` vs our `lwz r10,0x24(r30)`, the only residual that is neither regalloc nor fold-alias, settled by `class_layout_report.py PitchDetector --offset 0xc/--offset 0x24`. ⚠ Δbytes is the ONE IIR4PoleFilter row: Detect (+75.7 pp) and AnalyzeBlock (+7.4 pp) buy zero bytes because `matched_code` is all-or-nothing at `fuzzy == 100`. |
| **W16-BZ** | fable → opus | **+92 / +5,320 B** (43,682 → 43,774; 4,068,676 → 4,073,996 B; merge `e0a87c15a322`). The brief's five separate rows were **one lever with two knobs**, both TU-local `#define`s — no header edited. Knob 1 (Fable's two commits) is *whether* retail inlines the `ObjPtr<T>` owner ctor: it does for every member whose `T` has a **virtual** `Hmx::Object` base, and calls the two-arg form out of line only where `T` derives from it **directly**; our source spelled all 31+50 sites the out-of-line way, costing surplus `bl`s and a wider prologue (`__savegprlr_20` vs retail's `_17`). Knob 2 (Opus, after Fable hit API capacity twice) is the **STORE ORDER** (`RB3_TU_OBJPTR_DEFER_OWNER` vs `..._DEFER_OBJECT`) — and `obj/Object.h` had ALREADY written down that getting it wrong "leaves a row stranded in the high 80s **looking like a scheduler wall**". `??0GemTrackDir@@` 79.27 → **99.97**; `?PreLoad@GemTrackDir@@` 91.62 → **100.0**; `??1GemTrackDir@@` 86.46 → **99.81**; `??0VocalTrackDir@@` 70.96 → **100.0**; `?PostLoad@VocalTrackDir@@` 96.39 → 96.96. ★ The lane recorded **its own prediction scorecard** (5 pre-registered: 1 exact, 3 wrong FAVOURABLY, 1 wrong unfavourably) — it had classified the GemTrackDir ctor's cluster A and an 8-site 16-byte `_Rb_tree` block copy as permuter-class and written them off, and **both closed under knob 2, the block copy without being touched** ⇒ a systematic bias toward under-estimating how much "scheduling" is downstream of ONE policy (12th instance of "a register/scheduling label is a SYMPTOM"). ⛔ **W16-BX's `PostLoad` diagnosis is REFUTED**: the `OFFSET_SWAP (0xa8,0xb8)` is NOT a `cols`/`streakPtr` stack-slot swap but the frame consequence of our out-of-line `ObjPtr<OverdriveMeter>` temp — the "reorder the slots WITHOUT moving the construction site" arrangement BX bequeathed, and that this lane's own brief carried forward as THE untried lever, **does not exist; do not fund it**. `??1GemTrackDir` does **NOT** release `mArpShapePool`/`mFingerShape` in retail (rb3-Wii does) on three independent signals — 17I/0D insert cluster with no compensating delete, r28/r29 being those blocks' only users (explaining `__savegprlr_28`), and −48 B vs the 44 B surplus — cleanup kept for the native host under `#ifdef HX_NATIVE`. Left with the test that settles it: **3,276 B in GemTrackDir rides entirely on 11 relocation-NAME charges** (`make_pair<String,String>` / `vector<String>::~vector` vs our `ObjPtr` instantiations), NOT installed as aliases because there is no retail-byte proof and `TEMPLATE_ARGS_DIFFER` is what a fold AND a wrong callee both look like — `tools/comdat_fold_gate.py` at a T1 verdict decides it, and a FAILURE is worth more than the bytes (our container types would be wrong); a `none`-ruler control cannot validate it by construction. `PostLoad` cluster 9 is plausibly the same lever on `SetObjConcrete`; clusters 2–8 must not be assumed permuter-class until 9 is closed and the row re-diffed. |
| **W16-CA** | opus | **+22 / +840 B** (43,774 → 43,796; 4,073,996 → 4,074,836 B; merge `e1b9ddb0e5ec`). Six rows worked, four improved 5–55 pp and collected **zero bytes** (`matched_code` is all-or-nothing per row) — the deliverable is a **four-step table nobody had measured**, run one step at a time on the documented naming hazard: (a) the map entry for `0x82606020` **alone cost −1,196 B** (`name_check` forgives a placeholder target, so naming converts forgiven call sites into CHECKED ones — the loss is precisely `?Poll@SetlistToStorePanel@@` falling off 100); (b) fixing OUR source to match recovered **NOTHING**, against the lane's pre-registered prediction of full recovery; (c) repairing the **TARGET-side** type name at `0x82e01810` recovered **all of it**; (d) +implicit `DataNode` conversion → 43,703 / 4,069,204. ⇒ ★★ **BOTH SIDES OF A NAME COMPARISON HAVE TO BE RIGHT — a source fix alone is not a fix.** ⛔ **`SetlistMetadataLoadedMsg` IS A FICTION**, asserted only by a map entry's own mangling (a function-local-static name encodes its class); the real one is `MetadataLoadedMsg`, and its missing **out-of-line ctor** is what the 0x50 phantom frame on `?Poll@BandStorePanel@@` was hiding — **ten `DataNode` temporaries, NOT a `MakeString` buffer** as the brief guessed; frame 0x140 → **0xf0 exactly**, on a chain of 19 BASE_ONLY `(addr,int)` stack-slot pairs + retail's single six-register `bl fn_82606020` + that callee's own `Type()` over `"metadata_loaded"`. ⛔ **Our own source COMMENT was refuted**: it claimed the Wii commerce clause on `user_can_do_input` was "dropped" on 360 — retail *translated* it to `!IsEnumerating() && !InCheckout()` and the surplus `mUserCanDoInput == 0` was OURS. `?Poll@UGCPurchasePanel@@`'s `TODO(unresolved)` singleton **was named outright by objdiff all along — `TheNet`** (`DAT_82cbfaec` is `TheNet+0x34`): 87.7 → **99.996**, sole remaining charge `li r3,0x28` vs `0x50`. ★ **Method error the lane reported against ITSELF**: it identified two callees from a **vtable slot name borrowed from a DIFFERENT class's vtable** and got the direction backwards — corrected only by header member offsets (`mEnum` 0x70 / `mPurchaser` 0x78). **A slot name from another class is not an identification.** Two briefed rows (`MakeNewOffer` briefed 60.92, `IsLoaded`) measured **100 already** — no work done. ⚠ **Coordinator commit on top of the lane's own**: the lane had re-serialized `target_symbol_map.json` **sorted by address**, moving all 29,510 entries and relocating `_dc3_only_pins_comment`, for a **60,341-line diff carrying two lines of meaning**, and escaping non-ASCII so the file no longer round-tripped the canonical dump; key order restored with the lane's parsed content preserved byte-for-byte (gate measured the reorder **inert**: +22 / +840 either way). ★ **Biggest thing left and NOTHING fences it**: `BandStorePanel.h` overrides `EnumerateSubsetOfOfferIDs() const { return true; }` but never overrides `GetOfferIDsToEnumerate` — `fn_82608B70` (452 B) IS that override, and implementing it instantiates the `vector<u64>`/`sort<u64>` family = **≈2,016 B behind ONE member function, entirely inside `src/band3/`**. Also NOT done, each with its overturning evidence: `sizeof(XboxPurchaser)` (+1,104 B, sole charge, blocked on engine header `StorePurchaser.h`), `GetIndexFile()` (needs `fn_8250FDF8` identified and `0x82c73fdc` read through the **`.data`** delta — the `.text` formula yields garbage), `Handle` idx 94–98 (blocked on `ObjMacros.h` spelling `(action);`). Flagged but unmeasured: `StorePanel.cpp:302` carries the same split-allocation-temp defect. |
| **W16-CB** | opus | **+4 / +7,280 B** (43,796 → 43,800; 4,074,836 → 4,082,116 B; merge `30f7070f3f91`). ★★ **The brief's headline row is SETTLED, and it was the MAP, not a fold.** `0x82684e90` is `?TotalBasePoints@SongDB@@` and **not** `Hmx::Object::TypeDef` — four independent lines, none of them a match-% reading: the body is `lwz r3,0x1c(r3); b <MultiplayerAnalyzer::TotalBasePoints>`; **ICF is excluded BY CONSTRUCTION** (our `Object::TypeDef` COMDAT has **zero relocations**, retail's carries a **branch reloc** — different-relocation COMDATs cannot fold); our compiled `SongDB::TotalBasePoints` reproduces retail's bytes; and the 2-caller set (`PostLoad`, `Handle`) is exactly where our source calls it. ★ The control that could have refuted it — "ICF picked an arbitrary survivor name" — swept **1,219 objects** for a twin, found **none**, and was proved **non-vacuous** by turning up a real **12-member fold family** at an adjacent shape. `?Handle@Game@@` **5,428 B crossed** (the largest size-if-it-crosses in the game layer, briefed to three lanes); `?Poll@Game@@` +1,036; `?LoadSong@Game@@` +408; `?PostLoad@Game@@` +408. ★★ **METHOD, transferable: a BODY cannot distinguish an unused parameter from an absent one — the CALLER can.** `ContentMgr::IsCorrupt` is **2-arg, not 3** (the `const char *&` was a DC3-ism); a prior lane saw `r5` unused in the body and **correctly declined to act**, and the settling evidence is that the **only** slot-0x88 call on `TheContentMgr` in the whole binary passes two arguments. Map row renamed in lockstep so the row did not un-pair — the single `FELL OUT` row is that same function under its old spelling, **100 % on both sides, net Δ0, zero genuine regressions**. ⛔ **The oracle was wrong and retail right** in `LoadSong`: rb3-Wii carries both a 2-way ternary *and* a `SetPracticeMode` call that retail's `LoadSong` **does not contain at all**. ★ **Failed prediction kept in the record**: the first `LoadSong` restructure was priced **+408 B** and measured **+0** — it moved 54.49 → 97.598, which buys nothing because `matched_code` is all-or-nothing per row; the second attempt (branchy `if`/`else if`, not a nested ternary) crossed it at **exactly +408 B**, and a three-charge **r26↔r27 inversion DISSOLVED on its own** (13th recorded instance of a register label being a SYMPTOM). ⛔ **`??1Game@@` is CAPPED and the vein is closed**: 6 of its 8 charges are symptoms of ONE 4-byte EH-state defect and the 7th is a **proven** fold, so its **792 B is uncollectable by source work even if the defect is fixed** — do not re-fund it. Two instrument traps recorded: a vtable read anchored on a **guessed** start was off by one slot and **would have justified inserting a phantom virtual** (fixed by anchoring on `IsMounted@0x74`, which retail had already proven), and `~Shuttle` was **declared but defined nowhere in the tree**. Also landed: 8 anonymous `Game.cpp` bodies identified (4,892 B) — **pairing, not bytes**, per the standing naming economics. |
| **W16-CC** | opus | **+0 / +0 B** (43,800 → 43,800; 4,082,116 → 4,082,116 B; merge `b6a18175bf29`). ★★★ **Zero bytes, zero aliases installed — that IS the result, and it CLOSES the largest game-layer gap on the board at dispatch.** The brief's two strata were both something other than they looked. **(1) The near-miss stratum is ENTIRELY ARGUMENT-ONLY, MEASURED not argued**: over all 44 rows / 8,524 B there are **2,131 instructions, 2,074 equal, 57 `diff_arg`, and ZERO `insert`/`delete`/`replace`/`diff_op`** — **there is no codegen work anywhere in it.** This is the instruction-vs-argument reading error at MAXIMUM divergence: a mismatch count calls the stratum *finished* while the grader withholds all 8,524 B. **(2) Fold adjudication: ADMIT 0 / REFUSE 7 / SKIP 34**, none of the 41 callee pairs already forgiven. ★ The 34 skips are the discipline: their only available evidence is "our spelling has no map address", which measures **identification coverage (41.7 %), NOT folding** — converting that into an alias would have lifted `name_check` **by construction** with no control able to catch it. MeshAO/Patch refuted on **compiled bytes** (96 vs 132 B) *and* class structure. **(3) The register class is CLOSED for a non-obvious reason: RETAIL IS THE INCONSISTENT SIDE** — it emits `lwzx r3,r29,r11` in `Gem::Hit`/`Release` but `lwzx r3,r11,r29/r28` in `UpdateTailPositions`/`PartialHit`, from the **identical** `mTails[i]` source, so **no uniform spelling fixes two without breaking two.** **(4) The zero stratum is UNPAIRED, not unnamed**: 30 of 31 rows have `base_size == 0` and the 7 largest have **zero same-size COMDATs among our 2,427** ⇒ naming buys 0 %-rows with no content. ★ **ACTIONABLE HAND-OFF: 2,644 B = 16.0 % of the unit's 16,484 B gap is `NowBar.cpp`, a file this tree has NEVER PORTED.** `0x82BAA660` is proven `?FillHit@NowBar@@QAAXHH@Z` — bounds-checked `mSmashers[i]` tail-calling `?FillHit@GemSmasher@@`, anchored on the **tail-call destination (image ground truth, not another map row)**; the band `0x82BAA400–0x82BAAF30` holds **13 zero rows against 13 declared `NowBar` members**, the map names nothing in it, and the 243-line source sits in the rb3-Wii oracle. Map name repaired and landed. ★ **SELF-CORRECTION**: the lane **retracted its own earlier finding** that `0x822AC058` is `OutfitConfig::Register` — it is `Init` with `Register` (an inline header function) folded in by `/Ob2`, the map was right all along, and the follow-on pin plan is moot. **Scorecard: 6 predictions REFUTED, 1 confirmed**, and five of the six closed a vein that looked fundable from outside. Tool fix landed: `tools/comdat_fold_gate.py` **crashed on every real run** (51 of 1,657 alias groups carry `address: null`); the guard skips them, leaving the branch-destination compare **strictly tighter**, so it cannot manufacture an ADMIT (`SELFTEST PASS`, 0 defects / 130 extents). Gate: 0 rows in, 0 out, `fuzzy` −0.000003 pp from the rename. |
| **W16-CD** | opus | **+16 / +2,732 B** (43,800 → 43,816; 4,082,116 → 4,084,848 B; merge `a26bd090662e`). ★★★ **78 % of the prize was IDENTIFICATION, not decomp — and the lane proved it with a pre-registered null.** The brief priced `GetOfferIDsToEnumerate` at ≈2,016 B and explicitly flagged that as an ESTIMATE the lane must re-price; measured **2,108 B**, close in total and **wrong in mechanism**: only **472 B** is the three named `_K` template rows (source), while **1,636 B** is `fn_82608B70` plus nine anonymous STL helpers (**map**). A lane that implemented the override perfectly and stopped would have booked **+472** and called the estimate 4× optimistic. ★ The effect was then isolated cleanly on `StoreOfferProvider`: both bodies implemented **with no map entries measured EXACTLY 0 B** (pre-registered); the map entries alone took them 0 → **89.27 / 92.56**, and only then did source fixes carry them to 100 for +572 B. ⇒ **when a retail row is ANONYMOUS, source quality is INVISIBLE to the ruler** — the lane's main transferable result, and the reason a byte estimate that does not separate the two channels cannot be trusted. **Two permuter-class verdicts overturned (instances 14 and 15)**: `FindSongOffer` carried three labels, two `RarelyHandFixable`, plus a permuter recommendation — **one named local dissolved all 15 charges** (92.56 → 100); `IsActive` was recorded `BOOL_MASK`/permuter-class and "left at the best-scoring shape" after two failed variants, and **the plain `return a \|\| b;` was never among them** (71.92 → 100). ★ **A FAILED PREDICTION did the useful work**: the lane predicted the else-if alone would close `ShowBrowserPurchased` and measured 95.561 → **95.551** — comparing that against the earlier attempt showed each had fixed one end and broken the other, which is what identified the two halves as independent and named the untried fix. **The in-tree record was most of the answer and partly stale**: both rows the brief called "unexamined" had already been reconstructed instruction-by-instruction by lane BV-1 in the file's own comments, every particular re-verified against retail bytes rather than trusted, and **two load-bearing claims in that block were false** — "not in splits.txt, not scored", and a blocker this same lane had already removed earlier in its own run; both corrected in-tree. Closed with evidence rather than deferred: `Handle` (1,556 B) — both RTTI descriptors decoded from retail, the two sides **semantically identical**, sole charge is which descriptor went to r11 vs r10; `BuildList` (2,536 B) — CF-7's `diff_op: none` stands; `PosToNextGroupPos` time-boxed but its 13 charges reduced to **one** real difference (retail re-loads `mElements`' start inside the loop because `srawi` overwrites that register), recorded so the next lane does not start from the `REGISTER_SWAP` label. **Hand-off:** `fn_82606280` (908 B) identified as the store index-`.dta` parser with callees, strings and oracle location — plus `0x82c73fdc` read through the **`.data`** delta yields `/dlc_top_%s_%s.dta`, the string W16-CA's `GetIndexFile` item was blocked on. Map **+12 lines edited in place**, canonical form byte-verified — W16-CA's 60,341-line churn did not recur; `splits.txt` and `objects.json` untouched. Gate: 16 in / **0 out**, native 18/18. |
| **W16-CE** | opus | **+14 / +2,684 B** (43,816 → 43,830; 4,084,848 → 4,087,532 B; merge `7d0d960a84eb`). ★★★ **The briefed prize was exact to the byte, and the identifications behind it were mostly wrong.** The 13 named rows sum to **2,644 B — precisely the figure the brief priced** — and the lane collected all of them; the extra 40 B is an anonymous EH funclet (`fn_82BAAE14`) that pairs by byte signature, which is also the whole of the `masked_equal` 23,222 → 23,223 move. But **3 of the brief's 13 address hypotheses were refuted per-body**: `0x82BAA400` is `HandleOutOfRangeKey` (140 B), `0x82BAA698` is `PopSmasher` (164 B), `0x82BAA740` is `SetSmasherGlowing` (184 B) — the brief warned that address order is not source order and told the lane to adjudicate per body, which is the only reason a correct total survived incorrect parts. ★ **Three map names in this band are ICF fold SURVIVORS wearing another class's name**, the standing trap restated with three fresh instances: `?GetRate@RndAnimatable@@` at `0x822e4460` is really `TrackConfig::GetMaxSlots()`, `?NewFrame@CameraInput@@` is `TrackConfig::TrackNum()`, and `?IsDone@Cache@@` is `GemSmasher::Null()` — a survivor's name is arbitrary, so reading it as identification manufactures defects that are not there. ★ **The brief's "13 declared members" was wrong and the lane corrected it**: the band is **12 members plus one NON-member template COMDAT** (`DeleteAll<vector<GemSmasher*>>`, 92 B) plus the funclet. **Four aliases (~1,084 B) installed, all in the UNWITNESSED class** — nothing in the tree had instantiated `vector<GemSmasher*>` before this port, so each new spelling joins an **existing** fold group rather than declaring a new fold — and each was verified to land in the group the lane named (102, 11, 3 and 4 members); **two of those groups are the exact decoys the brief flagged** (`push_back@?$vector@PAVChatReceiver` and `?GetCacheName@CacheXbox`), so the reading corroborates rather than coincides. **Containment was checked, not assumed:** the unit-level net summed across ALL units equals the whole-binary +14. ⚠ **A vacuous instrument was caught before dispatch and is recorded so it is not re-run**: probing the packed `orig/45410914/default.xex` for `.?AVNowBar@@` returns 0 hits — but so do the **controls** `.?AVGem@@` and `.?AVGemSmasher@@`, which are known present, so the probe could only ever return 0. Re-run on the decompressed `band.exe`, `.?AVTrackDir@@` and `.?AVRndDir@@` HIT while all three NowBar-band classes are absent ⇒ **NowBar has no vtable**, refuting my own `??_G` scalar-deleting-destructor hypothesis. **No `splits.txt` and no `objects.json` edit** — the band already sits inside `Gem.cpp`'s `.text` entry so a scatter-include reaches it with no pin, following two in-file precedents, whereas carving a `NowBar.cpp` heading would be a **re-home, which is measurably not metric-neutral** (PINHOME-1, +3 fns / +428 B). ★ **A 4-byte defect was caught at landing by a control that could fail**: the four new `folded` entries went in at 5 spaces of indent where the file uses 4, so `symbol_aliases.json` stopped being a fixed point of its own serializer — main and the merge-base round-trip True, this tree did not — fixed on the lane and proved semantics-free before writing (re-parsed document equal; texts identical under space-run collapse). Gate: 14 in / **0 out**, native 18/18, DIRTY 0. |
| **W16-CF** | opus | **+1 / +40 B** (43,830 → 43,831; 4,087,532 → 4,087,572 B; merge `7291904605e2`). ★★★ **Two of the three defects I briefed were REFUTED BY CENSUS — including the one I found myself — and that is the lane's most valuable output**, because both would have funded work on a premise that is simply false. **Defect 1, "retail does not call `Localize` at idx 580/639 at all": FALSE** — `bl Localize` appears **four times on EACH side** (target 298/523/578/637, ours 298/523/580/639), so the delete/replace rows are **displacement of a call that is present, not absence of a call**. **Defect 3, "retail sets up an indirect call where we do pointer arithmetic" (idx 940, `mtctr r10` vs `add r5,r11,r30`): FALSE, and this one was MINE** — new, found while pricing the lane, in no prior record; `mtctr` appears **thirty times on EACH side**, and clusters 12 and 13 hold **identical instruction multisets, merely re-interleaved**. It is scheduling. I asked for it to be shot down rather than confirmed by assumption, and it was. **Defect 2 (the `switch (mMode)` dispatch, idx 798-800) CONFIRMED and CLOSED, `diff_op` 1 → 0**: two earlier lanes had probed it with if/else forms and failed, and the missing ingredient was neither unsignedness nor arm order but that an **if/else *chain* cannot merge two compares** — MSVC's binary-search switch lowering emits one pivot compare and normalizes case values against the minimum, which is exactly where retail's `cmplwi` comes from. A second real fix at cluster 742-747, where **we CSE'd two loads retail does not**: binding the vector to a named local lets MSVC CSE them, and calling `.size()` on the temporary directly reproduces retail. ★ **The in-tree record was materially corrected**: W1-GAME's "**13 INDEPENDENT body divergences**" is wrong — the 8-cluster idx 516-648 family is **ONE coupled mechanism at three call sites**, proved by a single source edit (hoisting `Localize(...)` to a named local, which is what the rb3-Wii oracle spells) moving all three **simultaneously**. It moved them the **WRONG way** (row fuzzy 97.24219 → 96.12402), so it was reverted and the number recorded in-file; the mechanism is a **genuine conflict, not an unsolved puzzle** — retail wants the vptr load **before** `bl Localize` (implying Localize inline in the argument list) and the `kStrGlobalCacheName` load **after** it (implying it hoisted), and no pure source form delivers both. The correction is **additive**: W1-GAME's text is intact with a dated, evidenced correction appended, and its pricing conclusion is preserved because it survives. **The row did not cross and was never going to on this budget** — 4,096 B is all-or-nothing, so Δ0 was explicitly authorised at dispatch — but it moved **fuzzy 96.74512 → 97.24219, mpn 97.29199 → 97.75488, clusters 13 → 11, insert/delete 25 → 21, replace 4 → 3, `diff_op` 1 → 0**. **Register pressure is killed as an explanation for the 105 swaps**: both sides use the identical callee-saved set (r24-r31) and the same `0x170` frame, so there is nothing on that axis for them to be caused by. Two negative results banked in-file with their numbers so nobody re-hunts them: `kStrGlobalCacheName` given **external linkage** (mangled name really changed, codegen bit-identical, whole-binary Δ0) and given **`const`** (same, with the caveat that the object has a dynamic initializer so the test says nothing about invariance). ⚠ **The lane declined to attribute its +40 B to a symbol** — 150+ indistinguishable 40-byte anonymous candidates, and it said so rather than guessing — **and the gate's crossing set-diff then named it for free: `fn_82551998`**, same unit; the lane was right to refuse, it simply lacked this instrument. **Falsifiable verdict on the remainder:** 13/13 is not reachable by the source forms available — what would overturn it is, for idx 516-648, a construct that fixes argument evaluation order independently of load scheduling across an opaque call, and for idx 760 and the two scheduling clusters, nothing in source, since both sides already hold the same instructions. No register or stack-offset work, no `SaveLoadManager.h` edits, no permuter, alias or `splits.txt`, and nothing in the files W16-CD and W16-CE own. Gate: 1 in / **0 out**, native 18/18, DIRTY 0. |
| **W16-CH** | opus | **+1 / +6,416 B** (43,831 → 43,832; 4,087,572 → 4,093,988 B; merge `8275d0620a68`). ★★★ **The alias would have CONCEALED a real behavioural divergence, and the source fix had to land FIRST.** The sympair row proposed folding our `?TakeShot@HiResScreen@@QAAXPBDH@Z` into retail's `0x826C3888`. Retail there is **`4e 80 00 20 00 00 00 00` — a bare `blr`, zero relocations**, its universal empty-function ICF survivor with **1,116 direct callers** (read off the retail PE at file offset `0x6b8688` in Python, not inferred from a tool verdict). **Our body was 360 bytes and 37 relocations**, and different-size COMDATs cannot fold at all — so installing the alias as the tree stood would have been forgiveness of a divergence, the exact integrity hazard the alias policy names. Two independent channels explain it: retail's call site in `Rnd::Handle` at `0x82413C68` sets up r3/r4/r5 and branches to the empty survivor while retail's HiResScreen TU emits **no TakeShot extent at all**; and the rb3-Wii oracle guards this body with `#ifdef VERSION_SZBE69_B8`, its dev-build gate. Our DC3-derived port (DC3 is a dev build) inherited it **UNGATED** — the MILO_DEBUG force-define family — fixed with the house pattern `#if defined(MILO_DEBUG) && defined(HX_NATIVE)`. **The source fix alone is Δ0**, landed on accuracy; only then is the fold earned, and the alias collects the CALLER `?Handle@Rnd@@` at **6,416 B**, the largest row in the adjudicate worklist. ⚠ **My brief had the two sides of the sympair row REVERSED** — the TSV column order is `tgt_addr target_symbol our_symbol` — and the lane tested the briefed figure literally, caught it, and said so; verified three ways afterwards, including that `TakeShot` is **absent from `target_symbol_map.json` entirely** because it is our COMDAT name. Deliberately NOT done: the four hand-off leads (sympair lines 57, 126, 150, 200) were left uninstalled because **each needs its own emptiness check** — triage since shows they are **not interchangeable**, `?Init@MidiInstrumentMgr@@QAAXXZ` having **no definition anywhere in the tree** (absent is not empty). No splits or objects.json edits (hunk-body 0 / 0). Gate: 1 in / **0 out**, native 18/18, DIRTY 0, main-vs-worktree diff 0. |
| **W16-CI** | opus | **+5 / +892 B** (43,832 → 43,837; 4,093,988 → 4,094,880 B; merge `9fae75f0666f`). ★★★ **Identification is not ONE channel — first-naming paid +84 B in one wave and EXACTLY 0 in another, while repairing a WRONG name paid +524 B off a single `diff_arg`.** Unit `default/band3/meta_band/BandStorePanel` **7,820 → 8,712 B of 13,144 = 59.494827 % → 66.2812 %**, matched fns 115 → 120. Four waves, each **pre-registered before measurement, and every prediction held**: first-naming 4 rows (predicted +84 B exactly, measured +84 B); source port of 3 bodies (+284 B); map **repair** (predicted +524 B, +1 fn **and that `ALIAS_SUSPECT` would fire** — all three happened); first-naming the 908 B row (predicted **+0 B / +0 fn**, measured exactly that). Split: identification **608 B / 68.2 %** vs source **284 B / 31.8 %**, close to W16-CD's 78/22. The refinement worth carrying: **first-naming only pays when our body is ALREADY good enough to cross** — otherwise it pays in **pairability** alone, and wave 4 is the clean demonstration at 0 bytes but fuzzy 0 → 28.95, i.e. the row went from **invisible to adjudicable** (an unpaired row looks identical to a row with nothing wrong). The wave-3 repair: `Request` (524 B @ 99.962) had **exactly one charged site**, a `diff_arg` naming `?CreateCameraBufferMat@@` where we spell `MakeString<Symbol,String>` — and **the row reads ALL INSTRUCTIONS EQUAL**, so an equality count is structurally blind to it. Adjudicated on retail bytes at `0x826058E8` with this repo's own `fold_thunk_gate.mask_word`: our `MakeString` COMDAT **0 masked mismatches / 23 words** against a **CONTROL of 20 of 23 words differing** for the incumbent name ⇒ not a fold with two live spellings, the map name was simply **WRONG** and an alias would have been a fabrication. `ALIAS_SUSPECT` fired and **was right to** — that shape is shared by a fabricated alias and a genuine wrong-callee fix, and `none` is flat **by construction** so its flatness is never a clearance; what overrides it is the control that **FAILED for the incumbent**. `fn_82606280` **proven** to be `?OnMsg@BandStorePanel@@…MetadataLoadedMsg@@@Z` on retail bytes — a full `.text` scan finds exactly one caller and the guarding arm is literally HANDLE_MESSAGE, `bl` to `MetadataLoadedMsg::Type()` then fall through — which **refuted two of my own briefed claims** (it is NOT absent from the Wii oracle, and W16-CD's `/dlc_top_%s_%s.dta` coupling did not reproduce and was not needed). Negative results committed where the next lane will look: `0x826067C0` (232 B) is `OnMsg(LocalUserLeftMsg&)` but is **MIS-PINNED to `Mat.cpp`** — verified independently, `Mat.cpp` owns `0x826067c0`–`0x826068a8` and the address is absent from the map — so a map entry there would pin it at **0 % permanently**; and `fn_82608D38` (88 B) is **not ours at all** (both callers are `CalibrationPanel`). Remaining gap **4,432 B and it sums exactly**, handed off with retail callee censuses for `Handle` and `Poll`. ⚠ Two traps carried forward: the lane's own **`\| head -20` manufactured a decisive-looking FALSE NEGATIVE** that would have blocked a correct identification (re-run unbounded before believing any symbol-name negative), and **`target_symbol_map.json` is NOT address-sorted** — sorting it produced a 30,202-line diff for a 1-line change, so insert next to the address neighbour and never sort. Gate: 5 in / **0 out**, native 18/18, DIRTY 0, main-vs-worktree diff 0. |
| **W16-CG** | opus | **+0 / +0 B** (43,837 → 43,837; 4,094,880 → 4,094,880 B; merge `edd6b216b08d`). ★★★ **Δ0 AND THE ROW IS CLOSED, NOT DEFERRED — the source was never the variable.** The most-attempted row in the tree, 5,036 B behind exactly one charged instruction. ⚠ **Routing:** dispatched to Fable THREE times under the escalation rule and all three died on API capacity errors before executing a step; re-routed to Opus with a brief hard-banning the spelling program the prior lanes ran. **The routing worked — the lane returned a mechanism rather than another refusal.** ⚠ **Two corrections to my brief, both verified while landing:** it said "six consecutive lanes" where the in-tree record lists **SEVEN** (DQ-1 is cited inside it), so this was the **EIGHTH** attempt; and it inherited CLAUDE.md's claim that cflags carry **"exactly two `/D`s"**, which is **FALSE** — `build.ninja` carries **ELEVEN distinct `/D` flags** and the two named are the **rarest at 2 occurrences each**, while `/DRB3_HANDLE_LOCAL_STATIC` appears **143 times**; CustomizePanel.cpp itself carries three. ★ **The load-bearing conclusion SURVIVES** — none of the eleven is `HX_NATIVE`, so `MILO_ASSERT` is still a no-op; the count was wrong, the inference drawn from it was not. **THE MANDATED FALSIFICATION TEST MOVES:** under `/EHa` the ProbeX transplant emits `subfe r11,r11,r3 / clrlwi r11,r11,24`, the BandUI oracle's exact register form ⇒ ProbeX was **SUPPRESSED, not incapable**, so the codegen channel was genuinely real — and it still does not close `[530]`. The instrument was validated in **BOTH directions before anything was believed** (a `/FAs` listing compiled straight through wibo+cl, never via ninja, so the six obj patchers are untouched): **POSITIVE control fires 1 site / 187 insns** on `?OnMsg@BandUI@@…ContentReadFailureMsg`, **NEGATIVE control 0 / 1760** on `?Handle@CustomizePanel@@`, same code and only the PROC name differing — a detector that had never returned nonzero would have confirmed whatever it was pointed at. **28 codegen knobs swept and every one inert on BOTH the pristine arm and the transplant**, yet **proven LIVE rather than ignored** (`proc_insns` 1760 → 1858 `/O2`, 1724 `/Ob1`, 2822 `/Og-` — they reshape the function wholesale and still never emit the mask). The **other compiler build** (`16.00.11886.00`) is inert with a byte-identical verdict, and the leg was proven non-vacuous by reading the listing header's self-declared version rather than trusting an env var — the elision is stable across a **1,662-build gap**. ★ **A prior in-tree diagnostic is shown CONFOUNDED:** `/EHa` breaks the cross-jump and the mask lands on the **has_patch** arm, but W37's diagnostic broke the cross-jump by **replacing has_patch with `mRefreshingContent` — deleting the one arm that owns the mask** — so "neither arm owns it" could not have been observed. The rule fitting all six observations: our cl masks an int→bool materialisation **unless the operand is already known-boolean, or the arm was cross-jump-merged**; has_license is uniquely the only arm whose value is an already-widened bool. **AND RETAIL VIOLATES THAT CLAUSE ON A BYTE-IDENTICAL SEQUENCE** — retail also does `bl fn_82575670 / clrlwi r11,r3,24` and masks the `!= 0` anyway, our `addic r10,r11,-1` IS retail's `subic r10,r11,0x1` (both `0x314BFFFF`), with merge structure isomorphic and exonerated (both tails `stw r11,0(r28); b`, both exactly 3 predecessors). ⇒ **~22 spellings, an explicit phi and a verbatim transplant all measured inert because they varied the one thing already correct.** The discriminator is the **compiler binary** — a 10224 QFE that does not elide the narrowing over a known-boolean operand; we hold two builds and both elide. Closed by absence of material, **with a diagnosis**, and a falsifiable prediction left in the file: on a different 10224 QFE the mask appears with **zero source change** and the row crosses on its own. **Do not dispatch this row a ninth time on any source, pragma or alias channel.** Comment-only: 98 added lines, **0 non-comment source lines**. Gate: 0 in / **0 out**, Δ0 on every measure, native 18/18, DIRTY 0. |
| **W16-CJ** | opus | **+78 / +14,648 B** (43,837 → 43,915; 4,094,880 → 4,109,528 B; merge `84577bdccc9d`). ★★★ **The 76th fold was a REAL BUG, and refusing it is the result — not the 75 that landed.** `??1CriticalSection@@QAA@XZ` (612 B / 4 caller rows) compiles here to `addi r3,r3,4` + tail-call `RtlDeleteCriticalSection`; retail's sites branch to the 4-byte survivor. Three independent lines say retail's dtor is genuinely empty rather than mis-mapped: retail's import table holds `RtlInitializeCriticalSection`, `…AndSpinCount`, `TryEnter`, `Enter`, `Leave` and **no `RtlDeleteCriticalSection`**, with the 300 import slots contiguous and no gap at that block, so no retail code can call it; retail's map holds every other `CriticalSection` method **but not the destructor** — the signature of a fold's loser; and the call sites themselves branch to the survivor. ⇒ **our engine tears down OS critical sections that retail deliberately leaks** — the dc3-is-newer hazard landing exactly as CLAUDE.md predicts, and the second such save in two lanes after W16-CH's `TakeShot`. **92 callees adjudicated, 75 installed, 17 refused**, each admission verified independently **from COFF** rather than on the gate's word (**75/75** compile to exactly `4e800020` with **zero relocations**): `??1GameGem@@` 11 rows / 2,068 B, `get_allocator<vector<_Slist_node_base*>>` 1,568 B, `RGGemMatcher::GetState` ×2 1,560 B (`mState` is first, so `&mState == this` in r3 — genuinely a bare `blr`), `DoneLoading@{Data,File}Loader` 1,136 B. The **17 refusals split two ways and the split is the useful part**: ten are real bodies against retail's `blr` (`InvalidateProxies@RndMultiMesh`, `KeyboardPoll`, `Validate@Movie`, `SetShowing@RndConsole`, `RndUtlInit`, `SyncProperty@BandTrack`, …) forming a **coherent console/keyboard/render-util cluster — retail compiled out debug-console-adjacent code our DC3-sourced tree implements**, corroborated by the note already sitting at `Console.cpp:390`; six have **no compiled body at all in two different ways** — `Init@MidiInstrumentMgr` and `??1FretHand` have no `.cpp` anywhere, while `??1PitchCorrectedVoice`, `??1PeakDetector`, `Flush@HDCache`, `Reset@DistortionEffect` have source whose **TU is absent from `objects.json`**, i.e. unwired and never compiled — a cheaper future fix than writing source. ⚠ **Two corrections to my own brief, both verified:** the queue TSV **does** have a column-header line (at line 11, after ten prose comments — and its claimed absence was my stated explanation for an earlier field transposition; fields 13=retail / 14=ours are correct), and **"mm>1 pays zero" needs a qualifier** — three multi-pair callers crossed on this work alone (`??0NetSession@@` 632 B, `??1Singer@@` 244 B, `StartIntro@VocalPlayer` 144 B) because **every** pair on those rows sits at this one address, so the rule is that mm>1 pays zero when the charges are **independent**, not merely because there are several. ★ **Prediction reported honestly against itself**: pre-registered +78 rows / +15,108 B, measured +78 / +14,648 B — and the lane flagged that the exact row-count agreement was **partly coincidental**, since 3 predicted rows did not cross (`HandleChordLegend@RGTrainerPanel` 940 B @99.97872, two `_M_splice_insert_dispatch` @99.87180 carry residual charges the queue did not know about) and ~3 functions / 792 B came from rows outside the prediction; TSV sizes were **not** stale (Σ TSV == Σ report == 15,108, zero disagreements). ⛔⛔ **A vacuity worth more than the bytes:** the first corroboration searched `band.exe` for the string `RtlDeleteCriticalSection` and found **0** — which looks decisive and is **vacuous**, because `RtlEnterCriticalSection` and `RtlInitializeCriticalSection`, both provably imported, also return 0: **Xbox kernel imports are by ORDINAL and carry no name strings**. Testing five names instead of the one it wanted is the only reason it was caught. Secondary deliverable: `sympair-queue.tsv` regenerated **2,740 → 15 lines**, gated three ways before commit (independent `report.json` population agrees to the byte at 168 rows / 74,624 B; `--selftest` reproduces W2-ENGINE's fixture at 23 rows / 41,088 B; the three stale folded spellings gone) ⇒ ★ **the sympair NAMING vein is DRAINED** — 2 rows / 1,960 B remain crossable by naming alone and both are `OURS_UNMAPPED`, i.e. identification backlog rather than fold adjudication, and `?RGGetChordName@@` (1,652 B) is now correctly described as three unmapped **data** globals plus register swaps, not the empty-fold lead my earlier hand-off called it. ⚠ **Latent tool defect filed, not fixed**: `tools/fold_thunk_gate.py:208` reads `cd["raw"]` where `comdat_bytes.py` says to use `fn_raw`, over-counting our body on 4 EH-prefixed symbols — **no verdict changed here** (retail's side is one word, so over-counting can only refuse what was already refused, and the mirror risk was checked: no REFUSE has `fn_size == 4`), but in a vein where retail's survivor is larger it can produce **false refusals**. Gate: **95 rows crossed / 0 out** for **+78 functions** — and that 95-vs-78 gap is not an error but the documented two-ruler divergence (`matched_code` sums rows at `fuzzy == 100`, `matched_functions` counts rows at `mpn == 100`), so **17 rows were already counted as matched functions with their bytes withheld** and this fold released only the bytes. The lane predicted 78 *rows*; it got +78 *functions* and 95 byte-crossings. Native 18/18, DIRTY 0, main-vs-worktree diff 0. |
| **W16-CK** | opus | **+0 fns / +160 B** (measured JOINTLY with W16-CM in a single main gate — the shared span is 43,915 → 43,916; 4,109,528 → 4,109,688 B; merge `b86c340c61a7`). Ported the store index-`.dta` parser into `OnMsg` and un-comma'd one `Handle` arm. The +160 B is **four 40 B `~String` funclets** (`fn_8260660C/82606634/8260665C/82606684`) crossing `fuzzy` while `mpn` was **already 100** — the arg-only stratum behaving exactly as documented, so the yield is bytes and **zero** functions. ★ **What the metric cannot see:** `OnMsg`'s final lookup was spelled `"metadata"` where retail says `"offers"`; `name_check` **forgives** it because the target's argument is a placeholder `lbl_`, so a real correctness fix is worth **nothing** to the score. An apparent map contradiction was resolved **without a map edit** — `fn_8274B0F8` is mapped `DataNode::Int` yet feeds `FindArray` as `this`, because `Int` and `Array` are both `return mValue.<word>` once the asserts compile out: byte-identical COMDATs that ICF folded. ⛔ **Pre-registered negative, reverted:** a named `Symbol nullSym` local closed two `OnMsg` charges and measured **−96 B** — it claimed a dedicated 16-byte-aligned slot, growing the frame `0xf0 → 0x100`, turning 8 charges into 29 offset charges and costing the four funclets their 160 B. Refuted **independently of the metric**: retail re-constructs into `0x50` twice (`gNullStr`, then `"index_info"`), which a named local cannot do ⇒ `0x50` is a reused temp slot. Left as a source comment so the next lane does not re-run it. `Handle` (1,928 B) stays open: the general repair is `action;` instead of `(action);` in `ObjMacros.h`, which cascades tree-wide and is out of lane scope. No A/B (deltas are from settled full builds with exact baseline reproduction and exact post-revert restoration — strongly controlled, not A/B-certified). |
| **W16-CM** | opus | **+1 fn / +0 B** (same joint gate as W16-CK; see that row for the shared span; merge `db5538cfadcd`). Took `default/VocalTrack`, the largest game-layer gap (19,928/44,372 B), and **closed no row** — `matched_code` is all-or-nothing and neither improved row reached 100: `PrepareNoteTubes` 1,160 B **73.4035 → 90.4172** (charges 188 → 88), `UpdatePitchArrow` 928 B **98.2543 → 99.3534** with `mpn → 100` (the entire +1 fn), unit 178/220 → 179/220, unit fuzzy 86.0659 → 86.5012. The lever was two `TheDebug << MakeString(...)` blocks guarded with the house pattern `#if defined(MILO_DEBUG) && defined(HX_NATIVE)` — ★ **`TheDebug` is NOT a `MILO_` macro, so unlike `MILO_LOG` it does not compile away.** Evidence was **structural, not metric**: retail frame `stwu r1,-0x170` against our `-0x9d0` (2,144 B larger), `__savegprlr_14` vs `_19`, `__savefpr_15` vs `_18`. ★★ **The three refutations are the deliverable.** (a) `PollLyricAnimations` is **NOT dev spew** — retail HAS the block; guarding it turned `lbz lbl_82E4BCF9`, the flag test and `bl DumpLyricPlates` into delete charges and stopped our TU instantiating `??$MakeString@MPBD@@`, a 92 B row **that had been matching at 100%**; predicted ≥97, measured **+2.04**, cost **−92 B**. Reverted. (b) `sDumpLyricPlates` **IS** a file-static, proven on retail bytes (`lbl_82E4BCF9` is an anonymous data label) — the lane had the right change with the wrong mechanism and briefly reverted a **+92 B edit it had already measured as good**, then went and found the evidence. ★ The process lesson is the keeper: **hunt evidence; do not discard a vindicated change because you distrust your own explanation.** (c) `RebuildHUD` shares the surface signature (`__savefpr_27` vs `_24`, 0x110/0x130 frame) but is a **DIFFERENT** defect — `Vector3` `fadds` operand ordering and sunk `stfs`, no `TheDebug` sites — deliberately **not** folded into the tidy story. `UpdateScrolling` (8,948 B, 1,372 charges, fuzzy 72.278) was **never opened**: that is reconstruction, and the mid-band was the better buy. A 16-pair ICF fold adjudication returned 1 IDENTICAL / 6 SHAPE_ONLY / 5 DIFFERENT / 4 UNDECIDED with **both negative controls correct**; the single proven fold sits in an 868 B row and buys zero bytes, so **nothing was installed**. ⚠ **Handoff: 171 live `TheDebug <<` sites across 49 files**, concentrated in high-gap engine units (rndobj/Utl 28,964 B, rndobj/Mesh 18,900, AmbientOcclusion 16,980, MeshAnim 16,896, Font 9,472) — with this lane's earned health warning: 13 of 16 candidates hold **zero** `MakeString` rows at 100%, **but Font, CharBones and CharBonesSamples DO, and guarding a block out there can take a matching row from 100 to 0.** Verify per unit against the charge list first; **a delete cluster means retail HAS the block.** |
| **W16-CL** | opus | **+5 / +1,080 B** (43,916 → 43,921; 4,109,688 → 4,110,768 B; merge `fd62ddd8b0a9`). `default/MusicLibrary` 341/371 → **345/371** fns, `matched_code` 31,644 → 32,680 B, gap 12,504 → 11,468 B. ★★ **A map defect was concealing a real bug.** `0x8253dd58` was mis-named `?ByteCode@RemoveLastSongFromSetlistMsg@@` — but `NETMSG_BYTECODE` makes `ByteCode` ICF-fold into `StaticByteCode` (already mapped, already 100% at 60 B), the vtable at `0x8208F650` holds **no such slot**, and fan-in is exactly 2. Renaming it exposed a row at 94.116% whose **ELEVEN charges were ONE defect**: we called `ContentDir()` (Callback slot 11, `+0x2c`, `const char*`) where retail calls `HasSyncPermission()` (Synchronizable slot 3, `+0x30`, `bool`) — a correction our own source already documented for `PlaySetlist` and `AppendToSetlist`. All 11 charges dissolved, **including the r28/r29 swap and the `__savegprlr` prologue delta** — the **third** in-tree instance of `REGISTER_SWAP` being a symptom rather than a verdict. ★★★ **An in-tree comment was wrong and cost 864 B.** `MusicLibrary.cpp:396` asserted that retyping `unk19c` "pays 0 in both currencies" — a note that **predates the 2026-08-12 `name_check` flip**, under which a wrong callee NAME is charged. Two call-site casts (no instruction emitted) closed both rows. ⇒ **Any "pays 0" note dated before 2026-08-12 is mispriced BY CONSTRUCTION — re-price it, never inherit it.** Measured, not asserted: the A/B was `--revert`, so signs flip — Δmatched **−3**, Δcode_bytes **−1036** ⇒ the commit is worth +3 fns / +1,036 B, and 172 + 748 + 116 = 1,036 exactly. Four of five per-row predictions landed on the nose. **Two self-corrections:** "type 7 does not exist in our enum at all" was **FALSE** — `kNodeStoreSong = 7` was already present; the grep could not substring-match it against `kNodeSong` and a `head -25` hid it, **a false negative shaped exactly like a decisive finding**. And `Text` was predicted "plausibly 100", then "high 80s/low 90s"; measured **75.489 then 83.638** — directionally right, **optimistic on magnitude both times**. **Open rows, with the evidence that would close each:** `Handle` 6,160 B is **CLOSED AS A WALL** (its 8 charges are two `__RTDynamicCast` argument set-ups with provably identical inputs — the retail `??_R0` descriptors were decoded and all three types, `Hmx::Object`/`StoreSongSortNode`/`StoreOffer`, are exactly what our source spells; residue is emission order, permuter banned; it did gain +1 `matched_function`); `Text` 1,292 B at 83.638 is switch-arm **EMISSION ORDER** (objdiff pairs our new store arm against retail's `kNodeSong` arm crosswise because they are shape-identical); `PushSetlistToScreen` 216 B at 81.759% is a **real divergence** (retail calls `?Tell@ArkFile@@UAAHXZ`, we call `GetLocalMachine@BandMachineMgr`); `SkipToNextShortcut` 396 B and `GetSongFilterAsString` 200 B are **KILL class** (pure r28↔r29 and commutative `add` order, both already `mpn` 100); `fn_82540ED8` 192 B = `OnMsg(RemoteMachineLeftMsg)` calls `RebuildSharedSongData` where retail calls `RebuildRestrictedData`, map names confirmed **not** swapped. ⛔ **No alias installed** — `symbol_aliases.json` untouched. `ALIAS_SUSPECT` fired on the map A/B (`name_check` +44 B, `none` flat) and the lane treated that as **neither refutation nor clearance**, adjudicating on retail bytes with controls that could have failed. It left the other **EIGHT** `unk19c->` call sites alone on purpose: their retail callees are unmapped and `name_check` **forgives** placeholder targets, so "fixing" them converts forgiven sites into checked ones for no upside. |
| **W16-CO** | opus | **+0 / +0 B** (43,921 / 4,110,768 B unchanged — doc only, `Gem.cpp` byte-identical to base; merge `1e38b7377795`). ★★★ **The lane closed ZERO bytes and the zero IS the finding**: 93.2 % of `default/band3/bandtrack/Gem`'s near-miss surface is relocation-name/ICF-fold class it was forbidden to collect, and the remaining 6.8 % is compiler-scheduling-bound — **proved by experiment and control, not by accepting a label.** ⛔ **Two corrections to the COORDINATOR'S OWN BRIEF.** The brief called the `fuzzy==0` class "four rows, ~3,412 B"; re-read from `report.json` rather than inherited it is **EIGHTEEN rows / 5,276 B**, all `base_size == 0`, **two of them NAMED anonymous-namespace rows, not `fn_`**. And the near-miss band is not ordinary source work: **41 rows hold 8,176 B whose TOTAL instruction-level divergence is ≈ ELEVEN BYTES** — about one charged site each, withholding full size only because `matched_code` is all-or-nothing. Priced from the charged-site list on the graded ruler, **all 41 are charged only on `diff_arg`**: ALIAS_MISSING 13 / 1,740 B, NO_GROUP 23 / 5,876 B, NONRELOC 5 / 560 B. The fold reading is **evidenced, not the detector restating its input** — target-side callees are semantically impossible for their sites: `AddRep` calls `push_back<vector<Tail*>>` (and `mTails` **is** that type) against survivor `push_back<vector<ChatReceiver*>>`; `InitChordInfo` calls `GameGem::GetChordNameOverride` against `NetCacheMgr::GetXLSPFilter`. **Our source is already right.** ★★ **The withdrawal check paid for itself:** main took a retraction the same morning (`7ec1e249`) for reporting a withdrawn membership as collectable, so this lane checked **before** claiming — **3 of the 13 ALIAS_MISSING rows (776 B) carry our exact spelling under a standing withdrawal**, making the un-withdrawn surface **1,288 B, not 1,740 B**. ⛔ **Both pre-registered predictions FAILED, and the control is the keeper.** `PartialHit` (284 B): predicted `mask & mSlots` closes 2 of 3 `and.` charges → **no change at all**, fuzzy 99.57746 identical to 5 dp. `UpdateTailPositions` (156 B): predicted ~25 % that hoisting `Tail *t = mTails[i]` reshapes the `lwzx` → **REGRESSION**, fuzzy 99.7436 → 90.333336, charges **1 → 14**, −1 fn. The killing control: `Hit`, `Release` and `KillDuration` each contain one `lwzx` over the **identical** `mTails[i]->X()` construct and in all three it is `lwzx r3, r29, r11` **equal on both sides**, while the two target rows want the operands reversed ⇒ **one source spelling, two retail orders inside one TU = downstream scheduling, not a selectable spelling** (MSVC canonicalises commutative `and` operand order pre-codegen). Permuter territory; permuter is OFF. ⚠ **A false lead retired before it cost anyone else:** the two small NONRELOC rows (`0x70` vs `0x60`, `0x54` vs `0x94`) are **EH unwind funclets paired by BYTE SIGNATURE** — the target destroys a different member via an unnamed `fn_82308470` — so those offset deltas are **NOT a struct-layout oracle**. **Evidence that would close each open row:** 1,288 B = alias membership on an existing survivor group gated on T1 retail-byte proof (a coordinator decision, not a lane's); 776 B = a standing withdrawal **overturned on evidence**, not a new alias; 5,876 B = a **NEW** fold group proven by relocation-normalised body hashing; 440 B = a permuter run or a source form moving `lwzx` operand order without perturbing regalloc (the two obvious spellings are now ruled out); 5,276 B = identification + scatter-include + body port, starting at `fn_822A5A38` (1,164 B, block `0x822A59D0`–`0x822A6024`, OutfitConfig cluster). ★★★ **COORDINATOR CAVEAT, raised upward by the lane and recorded so it is not re-learned: if this unit's bytes are wanted, the lever is an ALIAS/FOLD DECISION, not a decomp lane. Dispatching another source-matching lane at Gem will produce another zero.** |
| **W16-CP** | fable → **opus** | **+0 / +0 B** (43,921 / 4,110,768 unchanged — doc only, `symbol_aliases.json` UNTOUCHED, nothing installed; merge `d3be434e0d53`). ★★★★ **The alias oracle conflict is SETTLED: retail bytes are authoritative, but the precedence is LAYERED — and that caveat is the whole result.** An ICF claim is a claim about **retail's link**; Oracle B's predicate resolves operands *"through the ICF congruence over our own build"* (quoted from the group's own `repair.why`) — a fact about our linker's **INPUT**, neither necessary nor sufficient for retail having folded, while Oracle A reads the artifact the question is about. ⇒ **Only a FLAT `L1_T1` proof outranks a recorded withdrawal; `L2_RECURSIVE` ("ICF fixpoint via chase") does NOT.** `PROVEN` is a set containing both, so **adjudicate on the LAYER, never the LABEL.** ⛔⛔ **THE RULE WAS ALREADY IN THE TREE AND BOTH THE LANE *AND THE COORDINATOR* MISSED IT.** W9-D (`c6711597`, 2026-09-13, `docs/decomp/ALIAS_HYGIENE_2026-09-13.md`) adjudicated the **same predicate, same lane, same withdrawal class** and reversed **65 of 74** — 69 were wrong, 4 were nonetheless enacted because their proof was closed under the fold class under test, which is exactly where the layered rule comes from. W16-CN escalated a question that was a `git log --grep` away, and this lane was dispatched without anyone finding it. ✅ **Precedent verified literally by the coordinator, with controls that could fail** (positive `alias` 22, negative `zzqqxx` 0): the commit and its 413-line doc exist, `L1_T1` ×4, `L2_RECURSIVE` ×2, `circular` ×2, `our own build` ×1 are all present, and line 156 reads *"the strongest **non-circular layer** available"*. ⚠ **One correction to the lane:** the phrase *"the sweep was reasoning about the wrong binary"* was presented in quotation marks but occurs **0 times** in that doc — it is the lane's paraphrase, not a quotation. The reasoning it describes is genuinely there, so the verdict is unaffected, but **a paraphrase dressed as a quote becomes a fabricated citation the moment nobody checks it.** ★★ **The lane settled it WITHOUT re-running the oracle** — re-running Oracle A would prove nothing, since a fresh instrument agreeing with the hypothesis that motivated running it is the weakest possible evidence. The discriminating test is re-running `compare()` **with the installed-alias map emptied**. ⛔ **Its own pre-registered prediction FAILED, informatively:** it expected the disputed membership to be the **circular** one; it is the **ONLY FLAT** one of W16-CN's three "all PASS" dtor pairs, the other two passing only **with** alias help ⇒ **W16-CN had the strength ordering backwards.** Corroborated by an adjudicator sharing no code with it — `alias_forgiveness_audit.Sides.verdict`, W9-D's own tool — partitioning the same five pairs **identically**, with positive *and* negative controls, on a freshly built tree (anti-vacuity: 27,603 mangled of 69,432). Retail-byte anchor: `??1GemTrackDir` @ `0x822ee768` branches to `0x822ec870` at **+508/+516 in its own bytes** — exactly the ×2 charges — so the survivor address is not merely a map row. **Per membership:** `~vector<pair<ObjPtr<ET>,ObjPtr<ET>>>` @ `0x822ec870` — **withdrawal REFUTED, admit**; the two `0x822d8cc0` pairs — not admitted (`L2_RECURSIVE` only, under no withdrawal, simply never proposed); `make_pair` @ `0x822e7538` — **irreducible**, destination `0x82829258` unnamed; `push_back` @ `0x822ecbc8` — not admitted, chained fold. ⛔ **The 728 B is NOT collectable, and W16-CN's *corrected* reason is ALSO wrong:** on the map objdiff actually consumes, bucket `822EC870` holds **only** the survivor and `822D8CC0` holds survivor + `ObjDirPtr`, so **none** of the three pairs is an installed equivalence; admitting the disputed one alone leaves 5 charges over 2 pairs and the row stays at fuzzy **99.80769**. ★ **Admit it because it is TRUE, not because it PAYS.** **The vein, sized rather than extrapolated:** all 456 `UNDER_PARTITIONED_ICF_CLOSURE` withdrawals adjudicated on retail bytes ⇒ **385 flat `L1_T1` (84.4 %)**, a near-replay of W9-D's 87.8 % on a **DISJOINT** population, of which **246 are live nowhere**. This row sat **outside** W9-D's live-and-withdrawn population by construction, so W9-D's **rule** applies but its **adjudication** never covered it. The 246 were **deliberately NOT priced**: unlike W9-D's already-live 65 (Δ0), these are not live, so admitting them moves the rendered map with a real delta **in both directions**. **Not settled, named rather than glossed:** whether a chase through an **independently-proven, non-overlapping** fold class counts as flat (the mediator here — group[670], 19 folded, T1, 0 withdrawn — is a *different* template family, so "alias-mediated" may be **compositional** rather than circular, a distinction `Sides` does not draw); the vein's price; `0x82829258` still unnamed; 26 `CONTRADICTED` and 29 `NEEDS_SOURCE` counted but uninspected; and the **9,393** `FABRICATED_CLOSURE_NOT_PARTITION` withdrawals — **twenty times this class** — not examined at all. ⚠ **Instrument hazard worth the merge on its own:** a **lowercase grep of the UPPERCASE-HEX `icf_aliases.map` returned a clean, decisive, FALSE NEGATIVE** on the exact question the lane turned on — same family as the binary-grep and false-ABSENT traps already on record. Re-admission must go through `load_overrides` (requires `overrides_class` matching the record); the override JSON is specified in the doc. |
| **W16-CR** CharacterCreatorPanel::Handle (opus) | `333feb1a` | **+1 / +5,164 B** | 224 charges, of which **203 were one r28↔r29 register swap and 15 were structural** — fixing the 15 dissolved all 203. Fourth recorded instance that a charge COUNT is not a workload, and the most lopsided yet at 13.5:1. Handoff: `fn_8260D418` IS `CharacterCreatorPanel::SetGender` (120 B, fuzzy 0, NOT `masked_equal`) — newly pairable *because* this fix stopped us inlining it away. |
| **W16-CQ** SaveLoadManager/ProfileMgr (opus) | `389a043e` | **+1 / +80 B** | predicted ~5 charges would close; **12** did. One wrong constant (`case 0x26` setting state `0x3` where retail sets `0x27`) made our switch arm byte-identical to another, and the assembler cross-jumped a shared `li r4,0x3; b <tail>` block — the count measured how far one defect had smeared, not the work. Also a **phantom declaration**: `ProfileMgr.h`'s decl-only `GetProfileForPad` ("definition lives in an unported TU") was invented by an earlier lane; the real symbol is `?GetProfileFromPad@ProfileMgr@@QAAPAVBandProfile@@H@Z` at `0x82545e90`, already fuzzy 100, and `GetProfileForPad` is a *WiiProfileMgr* method on the wrong class. **Unit closed out: no remaining source-work byte lever** — 4,096 of the 4,476 B named surface is one row that cannot cross. |
| **W16-CS** ChordbookPanel (opus) | `cd699bb8` | **+1 / +152 B** | `Poll` crosses — retail reads `mLefty` *after* `GetGameplayOptions()` returns but *before* the `GetLefty()` dispatch. **`SetFret`'s 716 B CLOSED as unreachable by source**: its two charges are commutative REGISTER-operand order, not relocation names (`diff_arg` is not a synonym for "relocation name"), and MSVC canonicalizes commutative ALU operand order independently of source term order — all three inlined copies moved zero. `Enter` (304 B) fully reconstructed against retail bytes and deliberately NOT applied, blocked on anon callee `fn_826B5848`. ★ **The lane caught itself** one step from a binary-wide `BandUser` layout commit: a vbtable entry for a vbase carrying a vtordisp points *at* the vtordisp word, so the header comments were right all along — and its fallback "then the comments are stale" was also false per `--check-header`. |
| **W16-CT** alias re-admission `0x822ec870` (coordinator) | `e6b04856` | **+0 / +184 B** — ⛔ **pre-registered prediction FAILED (Δ0 predicted)** | restored one `L1_T1` membership that W16-CP adjudicated `dead`. It is not dead. The four crossed rows are `GemTrackDir` this-adjustor thunks whose ONLY call is `bl fn_822EC870` (two at member `+0x63c`, two at `+0x648`), so the payment lands exactly where the fold predicts — a legitimate forgiveness, not a fabricated one. ★ **All four rows are ANONYMOUS**, so a liveness census keyed on row NAME is structurally blind to the stratum; CP labelled **304 of 456** rows `dead`. Reopened as W16-CU per the failure condition pre-registered with the prediction. ⛔⛔ **CORRECTED 2026-09-15 by W16-CU — THE ERROR WAS MINE, NOT CP'S, AND THIS ROW'S DIAGNOSIS ABOVE IS WRONG.** `dead` is not a byte price and never was: it means **"this spelling is not currently in any group's `folded[]`"** — a **LEDGER-STATE flag** — reproduced **456/456**, with both rival readings failing (folded-or-survivor 411/456; the group's own census-site count 307/456; the 45 rows that separate them are `repartitioned` singletons whose spelling became a *survivor* elsewhere). **No census computed the column at all**, so the "name-keyed census blind to `fn_` rows" hypothesis is **REFUTED**. ⇒ **The +184 B was never a contradiction**: `dead` = *not installed* is precisely the **candidate-for-restoration set**, so W16-CT restoring one and being paid is the label **WORKING, not failing**. The misreading entered at `ba713e02` ("WORTH ZERO BYTES… liveness `dead`, 0 census sites"), which read a ledger-state flag as a byte claim, pre-registered Δ0 on it, and then attributed the failed prediction to CP's instrument. **CP's layer column reproduced 456/456 in an independent tree with both controls discriminating — it should never have been reopened.** ★ The durable lesson is the one this campaign keeps re-learning from the other side: **I audited a confident label and was right to audit it, but I mis-read what the label MEANT before testing it — and a pre-registered prediction built on a mis-read definition fails for a reason that has nothing to do with the instrument it blames.** |
| **W16-CU** alias `dead`-label audit (opus) | `5790329a` | **+6 / +2,044 B** | ⛔ **MY DIAGNOSIS OF W16-CT WAS WRONG; W16-CP WAS RIGHT.** `dead` is a **ledger-state flag** — "this spelling is not in any group's `folded[]`" — reproduced **456/456**, while rival readings fail (folded-or-survivor 411/456; the group's own census-site count 307/456; the 45 rows between the first two are `repartitioned` singletons whose spelling became a *survivor* elsewhere). **No census computed the column at all**, so the "name-keyed census blind to anonymous `fn_` rows" mechanism I wrote into `ba713e02` / `e6b04856` / the CT row is not what happened — and there was never a contradiction: `dead` = *not installed* is exactly the **candidate-for-restoration set**, so CT restoring one and being paid +184 B is the label **working**. CP's layer column independently reproduced 456/456 in another tree. Vein priced: 242 of 245 flat-`L1_T1` rows restored across 88 groups (**3 excluded as self-aliases** — the spelling is the survivor of the very group carrying its own withdrawal; the lane's first installer missed them by checking `folded[]` and never `survivor`). 6 payers, each paying at the site its fold **structurally predicts**, and the sum of parts equals the all-at-once total exactly ⇒ no joint dependencies. ★ Permuted-decoy control **+0 B vs +2,044 B** — and the lane's **first decoy was vacuous, returning exactly its prior**, caught only by an anti-vacuity assertion now run inside the measured path. |
| **W16-CV** ChordbookPanel::Enter + `fn_826B5848` (opus) | `0f6dda2c` | **+1 / +304 B** | ⛔ **W16-CS's blocker was FALSE.** "`Enter` cannot cross while its callee is anonymous" does not hold under the shipped `name_check` ruler: objdiff **forgives placeholder relocation targets** (`is_placeholder_symbol_name`, `diff/code.rs:998`), so the `bl` was **already uncharged**. What was missing was a **body**, not a name — predicted +304 B with the callee left anonymous and no map edit, measured **exactly +304 B**. `Enter` is now fuzzy 100.0 / mpn 100.0. `fn_826B5848` identified as `HandleLegendLefty` (reads `mGemPlayer`/`mChordLegend`, writes `mLefty`, uses `"lefty_flip.anim"`; body line-for-line rb3-Wii's `RGTrainerPanel::HandleLegendLefty`) and landed **INFERRED, NOT PROVEN** on measurement (**Δ0 bytes**, `Enter` held 100.0 ⇒ the now-*checked* call site agrees with our spelling). ★ **The retail spelling is UNRECOVERABLE — vein drained:** rb3-Wii `symbols.txt:9215-9290` is the complete Wii-dev list for the TU and holds no lefty fn, Wii's `Enter` is **156 B vs retail's 304 B** so the path is retail-only, and DC3 has no `ChordbookPanel`. Residual 316 B row at fuzzy 89.49367: retail **duplicates** the 1.0/0.0 materialization into both branch arms, MSVC hoists ours. **V1 refuted — do not retry:** explicit per-arm `one`/`zero` locals measured 89.49367, identical to the last digit; **MSVC CSEs by VALUE, not by name.** Also: `SetFrame` takes `(0.0f, 1.0f)`, correcting rb3-Wii's own acknowledged `TODO(W8-argswap)`. |
| **W16-CW** VocalTrack::UpdateScrolling (**fable → opus**, 429) | `c83988ba` | **+0 / +0 B** — Δ0 **by construction** (doc only, no source edits) | **Not an unfinished body — a codegen-SHAPE wall.** 8,948 B at fuzzy 72.27805 / mpn 73.985695, 1,372 charges, 43% of VocalTrack's named gap, never opened before. Our source is semantically and structurally correct; the whole 27.7 pp is MSVC **basic-block layout + register allocation** over 2,511 instructions, and `matched_code` being all-or-nothing per row means **no partial work buys a byte**. Three independent instruments agree: call-graph parity (16 retail-only / 17 ours-only `bl`, of which 12 and 10 are **one reordered block**; ⚠ the rendered context window shows only 1,336 of 2,511 rows and **undercounts**), `target_size` 8,948 vs `base_size` 8,968 = **+5 instructions**, and an opcode-multiset control at **105/2,237 = 4.69%** with a pure layout-inversion profile (`bge +10 / beq −7`, `lwz +12 / addi −12`). **34% of all insert/delete (183 of 543) is ONE block in the wrong place** — the loop-exit tail, exactly 8 `GetLyricColor` + 4 `GetLyricAlpha` per side with none matched ⇒ reorder, not duplication. **Four priors refuted**, including the `MILO_WARN`/`TheDebug` debug-block — W16-CM's own lever worth 73.40→90.42 on `PrepareNoteTubes` **in this same file** — plus the `float &lastLyricX` reference, a wrong-constant defect, and retargeting `goto window_ok` at the oracle (ours already beats it; our `IsGameOver()` is right where the oracle's `IsNet()` is wrong). ★ Corrects W16-CM's read **in the other direction**: CM saw 72% as "reconstruction, not sculpt" — the source is *done*, the codegen shape is not. **Escalated to fable (W16-CY) rather than accepted**, because this repo's own record says a register-allocation residue is a **symptom, not a diagnosis** (12 recorded dissolutions; W16-CR hit 13.5:1 this session). |
| **W16-CX** install three stalled handoffs (opus) | `c99ce4ee` | **+3 / +492 B** (43,931 → 43,934; 4,118,696 → 4,119,188 B; gate **exact to the pre-registration**, 3 rows in / 0 out) | **All four items delivered, map/alias artifacts only, zero source changes.** Four independent `ab_measure` runs whose legs **chain exactly** (43931→43932→43933→43934, each run's leg A equalling the prior run's leg B) — an internal control that no reading drifted. **(1)** `0x8260D418` → `?SetGender@CharacterCreatorPanel@@QAAXVSymbol@@@Z` **+1 / +120 B**, with the `none` control **also +120** = `REAL_PAIRING` (a body that never paired now does), the opposite of the flat forgiveness signature; the row was genuinely unpaired (`masked_equal=False`), not merely uncredited. **(2)** `0x826b56c8` renamed `Load` → `?Unload@ChordbookPanel@@UAAXXZ` **+1 / +4 B**. ⚠ **The lane CORRECTED the coordinator's reasoning and the correction mattered**: my brief killed the ICF rival by showing `fn_82814120` is a real 0xa0-frame function, which rules out `UIPanel::Load ≡ ::Unload` but **not** our two ChordbookPanel forwarders folding into one arbitrarily-named survivor — and retail carries only ONE 4-byte stub in that span, so the question was live, not dead. **The vtable settled it**: the address occurs exactly once in `band.exe`, inside a run of code pointers; aligned against our vtable with the RTTI COL at the head, **twelve independent slots agree** and the disputed address lands on **slot 12 = Unload**. ★ Bonus: `ChordbookPanel::Load` **does** exist in retail — it folded under `?Load@SetlistToStorePanel@@UAAXXZ` (`0x825f5920`), which is why no second stub appears. Left unopened as a separate group — recorded as a lead. **(3)** `reserve<BandProfile*>` into alias group 1096 **+1 / +368 B**, where the **proof bar was the deliverable and two NEGATIVE results carry it**: flat T1 is **REFUTED** (masked bodies match, relocation targets disagree — template twin), and **FOLDPROVE-2's masked-body-uniqueness channel DOES NOT APPLY** (retail keeps **two** 188 B bodies, survivor + `fn_827A36F0`, so uniqueness cannot pick the membership — stated as a result, not assumed). Three channels that do discriminate: CHASED T1 PROVEN (with `--selftest` run FIRST, confirming the instrument can fail), **call-site displacement from retail bytes** (`??0SaveLoadManager@@QAA@XZ` carries `bl 0x823715e0` at `0x825522e4`, word `4be1f2fd`, to the survivor and *not* `fn_827A36F0`; objdiff shows our ctor equal on 91/92 instructions with the single `diff_arg` at index 65 being exactly that call), and heterogeneous fan-in naming the element type in callers' own signatures. The gate row surfaced as the **payer** `??0SaveLoadManager@@QAA@XZ`, consistent with that. **(4)** `operator=<BandProfile*>` into group 1581, **Δ0 exactly as pre-registered** — CQ's "worth 0 alone" was **verified, not inherited**: the only charged caller `?SetState@SaveLoadManager@@` carries **112** charged sites dominated by an r25/r26/r27/r29 regalloc swap, so one forgiven name cannot cross a 4,096 B row. ★★★ **The lane's most valuable result is epistemic and belongs to item 4: `ALIAS_SUSPECT` DID NOT FIRE** — the control read `FLAT: none UNMOVED and default not up`, because **a zero-forgiveness alias cannot move either ruler**. ⇒ **that guard is STRUCTURALLY SILENT exactly where an unproven install would be cheapest to sneak in**, which inverts how much weight a quiet `ALIAS_SUSPECT` deserves. The membership rests on retail bytes and nothing else; installed under the standing rule that classes forgiving 0 today go live as porting advances (a prior prune cost **+94,616 B** to reverse). ⚠ **Traps recorded:** the `.s` address/file-offset columns are synthetic — displacement arithmetic on the printed address disagreed with dtk's own symbolization by **0x1E8**, so retail must be re-read through the PE section table keyed on the map address; `target_symbol_map.json` carries **non-address keys and non-string values** (crashed two scans); and resolving a disputed address **through the map is circular** — the signal is the surrounding vtable slots, which come from independent rows. ★ The lane caught **its own** instrument bug before trusting it: a COFF probe using a capturing group returned `"Un"` instead of the symbol, which would have read as a **failed** verification of the load-bearing check. **Did NOT:** any source change; open the proven `Load`/`SetlistToStorePanel` fold; touch `??$__ucopy_aux@PBQAVBandProfile@@` in MetaPerformer (4 B, fuzzy 95.0, same single-charge shape, different symbol, out of scope). `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`. |
| **W16-CY** VocalTrack::UpdateScrolling escalation (**fable → opus**, 429) | `c8f94be2` | **Δ0 exactly as pre-registered (43,934 / 4,119,188 B unchanged; 0 rows in, 0 out; provenance identical)** (doc-only branch, pre-registered Δ0) | **CW's verdict CONFIRMED, its stated REASON CORRECTED, and the actual mechanism identified — which CW never named.** Reproduced the row independently before touching CW's argument: own build, graded ruler, fuzzy **72.27805** / mpn **73.985695**, target 8,948 / base 8,968, 2,511 rows, **1,372 charges** split 754 `diff_arg` / 274 insert / 269 delete / 58 replace / 17 `diff_op` — every digit matching CW, so both lanes are measuring the same object. ⚠ **CW's load-bearing claim was incomplete**: CW argued the misplaced tail block cannot be a source reordering *because it stores `itT - begin`*, but only the `*itPPtr` store depends on `itT` — the twelve `GetLyricColor`/`GetLyricAlpha` calls depend on `colorBase = (staticLyrics?8:0)|(lead?4:0)`, which is **loop-invariant and known long before the loop**. The argument was made from the one instruction that cannot move, about a block that is mostly instructions that can. ★ CY then tested the *consequence* on retail bytes instead of stopping at the logic: retail does **not** hoist those calls — the colour block at `0x8164`–`0x8228` sits **after** the loop-head `TickToMs` pair at `0x8080`/`0x80a8`. **So the conclusion survives and the reason does not.** ★★★ **The real mechanism is LOOP ROTATION.** Retail's back edge is an unconditional `b 0x8054` with the test at the **top**; ours is a **duplicated bottom test** (`cmplw cr6,r8,r10` + `blt cr6,0x1db3c` at `0x1e268`/`0x1e26c`), and rows 1647/1648/1652/1653 are four instructions present **only on our side** — most of the +5 instruction surplus. **Block placement is DOWNSTREAM of that**: unrotated, the exit block lands inline at the first `break`'s fall-through (retail); rotated, it follows the bottom test (ours). ⇒ the 183-instruction "reordered block" CW measured is a **symptom, not an independent defect** — the same disease as this repo's standing warning that a `REGISTER_SWAP` label is a symptom rather than a diagnosis. ★ **The falsification test was pre-registered and then measured**: the rb3-Wii oracle spells the loop `while (*curPhPtr < lyricPhrases.size())` where we spell `for(;;){ if(!cond) break; }` — exactly the axis that governs rotation. Rewriting ours to the oracle's form produced **bit-identical output** (fuzzy 72.27805, base_size 8,968, charge split 754/274/269/58/17, bottom-test `cmplw`/`blt` still present): **all four predictions held**, and the probe is **not vacuous** — the build log shows `[5/15] MSVC VocalTrack.obj`, so the TU really recompiled. ⇒ **MSVC canonicalises the two spellings; the rotation is not reachable through control-flow spelling.** ★ CY also verified one of CW's claims **more strongly than CW did**: our `goto window_ok` is **byte-correct** — retail emits `beq 0x828c` twice then falls through to the break, and `0x828c` **is** `window_ok`, so ours does not merely match better, it matches **exactly**. ⚠ **A PREDICTION THAT MISSED, recorded so nobody retries it**: the lone `SIGNEDNESS_MISMATCH` at index 2319 (retail `cmplw cr6,r9,r11` vs ours `cmpw cr6,r11,r9`) is a genuine divergence caused by a spurious `(int)` cast in `(int)lyricPhrases.size() == *curPhPtr`; dropping the cast was predicted to close the row and measured **fuzzy 72.225746 / mpn 73.92445 — WORSE by 0.052 pp**. Reverted. **Do not re-try that edit.** **No path to 100, so no bytes**: the reordered block is 229 of 543 insert/delete, leaving **314 diffuse** across every index band; of the 754 `diff_arg`, **398 are register-only** and **162 of 197** immediate rows are `r1`-relative stack slots. Both probes left whole-binary `matched_code` at exactly 4,118,696 — all-or-nothing per row, demonstrating itself. ⚠ **ROUTING NOTE, stated honestly**: this escalation ran on **opus, not fable** (fable HTTP 429), and opus-auditing-opus is weaker than the escalation rule intends. The caveat is *softened but not erased* — CY did not merely agree with CW, it reached the same verdict **by a different route** and **overturned CW's stated reason**, which is stronger than an echo. The row is left closed **for CY's reason** (our loop is rotated, retail's is not; `while` ≡ `for(;;)+break` under this compiler) and **not** for CM's "72% means reconstruction" or CW's "the tail block cannot move". **Did NOT:** port the body, run the permuter, chase the 37 DIFFER stack slots or the 4/5 TGT/BASE-only locals, or re-run CW's four already-refuted priors; the three fuzzy-0 `Unlockable` rows (**568 B**) CW flagged as an attribution question **remain open**. `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`. |
| **W16-DA** VocalTrack `TheDebug`/`MakeString` sink (opus) | `1f3e147d` | **Δ0 exactly as pre-registered — the joint gate with W16-DB measured **+1 / +1,072 B** and **all of it belongs to W16-DB**; this branch is comment-only (0 non-comment changed lines)** (comment-only branch, pre-registered Δ0) | ⛔⛔ **THE BRIEF'S CENTRAL PREMISE WAS FALSE, AND THE FALSE PREMISE WAS THE COORDINATOR'S — recorded here as my error, not the lane's.** The brief asked DA to find the unidentified sink through which retail's spew reaches `MakeString`, quoting an in-tree comment in `VocalTrack.cpp`: *"retail never loads `?TheDebug@@3VDebug@@A`, so retail's spew reaches MakeString by some other sink. That sink is unidentified."* **There is no other sink. The sink is `TheDebug`.** ★ **The instrument that produced the false claim was known-vacuous when I used it**: I string-searched `band.exe` for the mangled name, found **0**, and noted *at the time* that the retail image is **STRIPPED**, so a mangled name can **never** appear as a string there and the search proves nothing in either direction — then briefed the inverted conclusion anyway. ⇒ **The testable form is whether retail CODE references the ADDRESS**, and it does: `?TheDebug@@3VDebug@@A` lives at `0x82CC9874` and retail carries **68 `lis`/`addi` materialisations** of it feeding **651 `bl` sites** to the surviving `operator<<`. Decisive witness, one basic block at `?Print@CharBonesSamples@@UAAXXZ+0x2c`: `bl ??$MakeString@HHHH@@…` then `lis r11,-0x7d33` / `addi r27,r11,-0x678c` (`0x82CD0000−0x678C = 0x82CC9874`) / `mr r3,r27` / `bl ??6TextStream@@QAAAAV0@VSymbol@@@Z` — MakeString's result handed straight to TheDebug's stream operator. ⚠ **The 68 is a FLOOR, not a census**: MSVC hoists one `lis`/`addi` pair to serve many `<<` sites in a function, so materialisation count **structurally undercounts** use sites; **651 is the honest scale.** ⛔ **The coordinator's 520 B hazard table was ALSO wrong, in size as well as attribution**: only **2 of its 5 rows** are `TheDebug`-reachable, so real guarding exposure is ≈**184 B** — `CharBones::StringVal` *returns* MakeString's result rather than spewing it, `RndFont::CharAdvance` feeds `String::operator+=`, and `MakeString@MPBD@`'s sole caller is `DisplayEvents` through a vcall. ★ **The finding is a MEASUREMENT, not a confirmation of what it was pointed at — because the untreated population was run**: treated units carry retail `<<`/`TheDebug` at **49.0%** vs **3.6%** untreated, a **13.77× discrimination**. That control is what separates this from the enrichment-ratio trap this repo has been burned by (a ratio from a blind-spot-bearing detector describes the detector). **What lands is a comment-only commit** (`69aba5b7`, 45/8 lines in one file, **0 non-comment changed lines**) replacing the false claim with the measured one and recording the witness, so the next reader does not re-hunt a sink that does not exist. ★★ **What deliberately does NOT land: the guarding patch the brief commissioned.** The lane wrote it, measured it **pre-registered on both halves of an A/B at −92 B / −1 fn**, and **REVERTED it** — a net-negative patch is not landed merely because it was ordered. That is why this merge is pre-registered at Δ0 and why the joint gate's entire movement belongs to W16-DB. ⚠ **The lane caught its own hazard before committing**: its first comment draft contained a literal `#if` token — the exact `ScatterIncludes` class that broke the native link in `6c087cbd` — proving again that **"it's only a comment" is NOT safe**. Reworded before commit; verified at merge time as **0 added `#if`/`#endif` tokens**. **Did NOT:** land any guarding, touch the 651 call sites, or open the `RndFont`/`CharBones` rows the corrected table leaves outside `TheDebug` reach. `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`. |
| **W16-DB** alias remainder — the 213 `UNDER_PARTITIONED_ICF_CLOSURE` withdrawals (opus) | `8e62621d` | ****+1 / +1,072 B** (43,934 → 43,935; 4,119,188 → 4,120,260 B; gate **exact to the pre-registration**, 2 rows in / 0 out — payers `RndGroup::Handle` 1,012 B and `BandList::fn_8233DFE0` 60 B)** | **139 of 213 withdrawals RESTORED, 74 REFUSED with retail-byte reasons — and the headline finding is structural, not numeric: the 213 were PARKED, not DEAD.** ★★★ **`gen_symbol_alias_map.py` SKIPS address-less groups**, and **154 of the 155 `repartitioned` rows sit in exactly such groups** — live in the ledger, rendered into **no map line**, forgiving **nothing**. ⇒ they are **INVISIBLE, not REFUTED**, which is mechanically different from W16-CU's dead spellings and changes what a future lane should do with them: they need an address, not an argument. ★★ **The safety question was settled on objdiff's real semantics rather than on our helper's**: `parse_msvc_map` is a **NON-TRANSITIVE UNION** — a multi-address spelling keeps **every** asserted pair (**36 already ship that way**), so a restore **adds assertions and cannot corrupt a bucket**. ⚠ W16-CP's `if len(a)==1` helper is **not** objdiff's semantics, and reasoning about alias safety from our own loader would have refused a safe install. **Verified on the ARTIFACT, not the lane's report** — `main` vs `w16-db` on `scripts/symbol_aliases.json`: folded **5336 → 5475 (+139)**, withdrawn **10243 → 10104 (−139)**, restored **316 → 455 (+139)**, **22 groups changed, 0 added, 0 removed**, keys touched = `{withdrawn, restored, folded}`, and the one forbidden action **did not happen**: `FABRICATED_CLOSURE_NOT_PARTITION` **9393 → 9393 UNCHANGED**; the only class delta is `UNDER_PARTITIONED_ICF_CLOSURE` **213 → 74**. Both bare-string withdrawal records survive (`0x826936f8`, `0x82770730`). ⚠ **Structure traps the coordinator hit and recorded**: `withdrawn[]` holds **10,241 dicts AND 2 bare STRINGS** (a census assuming dicts **crashed** with `'str' object has no attribute 'get'`), the withdrawal class key is **`class`**, not `reason`, the class table is a **top-12 of 45 distinct classes**, and **5 records carry no `class` key at all** — so any census here must inspect the real structure before keying on it. ⚠ **The lane's own self-caught error is worth more than the count**: it **retyped a 217-char mangled name off a 110-char truncated print** and misreported payer 2's counterparty, caught it, and corrected it. ⇒ **NEVER RETYPE A MANGLED NAME** — read it from the artifact. ⚠ **A pre-registration that missed in the informative direction**: predicted **+0…+1,500 with a point estimate of +600**, measured ****+1,072 B**** — inside the band, but the point estimate was **79% low**, so the band was doing the work and the estimate was not. Recorded rather than quietly rounded. ⚠ **One of the 74 refusals is flagged REOPENABLE**: the single `L5_INCONSISTENCY` row, because **L5's decoy false-positive rate is uncontrolled** where L3's and L4's are published — a refusal resting on an ungated instrument is a weaker refusal than one resting on retail bytes, and it is labelled as such rather than folded into the 74. **Did NOT:** touch the 9,393 `FABRICATED_CLOSURE` records, restore any of the 74 refused withdrawals, address the 154 parked rows (they need a map identification, not an adjudication), or reopen W16-CU's §7.1 remainder (26 CONTRADICTED / 29 NEEDS_SOURCE / 8 L2_RECURSIVE / 7 NEEDS_MAP_ID / 1 L5_INCONSISTENCY). In-tree record: `docs/decomp/W16DB_UNDERPARTITIONED_213_ADJUDICATED_2026-09-16.md` (221 lines). `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`. |
| **W16-CZ** ChordbookPanel residue (opus) | `4be935fe` | ****+10 / +1,908 B** (43,935 → 43,945; 4,120,260 → 4,122,168 B; gate **exact to the pre-registration**, 0 fell out) — ★ **the crossing list spans SEVEN units, not one**: `GemTrainerPanel`, `GameGemDB`, `TrainerGemTab`, `GemPlayer`, `GemManager`, `GameGemList` alongside `ChordbookPanel`, which is the `0x826ab108` cascade paying out and is **independent evidence the new name is right** (a wrong name un-pairs, it does not cascade). ⚠ **The coordinator predicted "~10 rows in" and measured 19** — bytes and function delta exact, row count wrong, because I conflated the two rulers: `matched_functions` counts rows at `mpn == 100`, `matched_code` sums rows at `fuzzy == 100`, so **19 rows crossed the byte threshold while 10 newly reached `mpn` 100**. ⚠ Also pre-registered as a risk and **refuted**: W16-DB restored 139 alias memberships between CZ's base and this gate, and a map name and an alias group feed the *same* forgiveness mechanism — the two composed cleanly with no interaction** | **Five rows closed and TWO MAP NAMES CORRECTED — corrected, not aliased, which is the part that matters.** Unit `default/ChordbookPanel` **110 → 113 rows, 8,052 → 8,676 B, fuzzy 94.66434 → 99.10742**; rows closed: `OnDisplayChord` 108 B, `PickFretboardView` 300 B, `HasChords` 176 B, `fn_826B7C50` 40 B (EH funclet, crossed once its parent paired), `GetFretboardView` 60 B. Levers: function-local statics matching retail's two guard bits; `(i2 < 0) ? 0 : i2` plus an **arm inversion** (MSVC lays out `if (C) A else B` as branch-if-not-`C` to `B`, so retail's `C` is `i2 > last`); a named `GetGemList` temporary. ★★★ **Both map fixes were made by CORRECTING THE NAME rather than installing an alias**, and that distinction is load-bearing: an unproven alias lifts `name_check` **by construction** and the `none` control **cannot** catch a fabricated one, so an alias would have bought identical bytes while destroying the evidence. **(1) `0x826a9f70` — `GetFretboardView` returns `int`, not `bool` (`_N` → `H`).** ⚠ **An in-source note from lane INSDEL-3 said "do not re-open"; its DIAGNOSIS was right and its CONCLUSION was scoped too tightly.** It observed the surplus `clrlwi` is "pure bool materialization" — which **is** the finding, because a `bool` return *requires* that mask and retail emits none (60 B vs our 64 B) — then held the signature fixed and **never looked at the call site**, where `PickFretboardView` tests with `cmpwi r3, 0x0`. The map's `_N` came from the rb3-Wii oracle via `gen_game_target_map.py`, **never from retail**. The stale note was replaced in place rather than left to mislead the next reader. **(2) `0x826ab108` is `~vector<GameGem>`, not `push_back<vector<Hmx::Color>>`** — prior lanes had **proven the old name wrong** (`wrong-callee-triage`: `map_misassignment`; `comdat-fold-gate`: REFUSE) **without ever supplying the right one**. Five independent lines agree: destroy-range + `_M_deallocate`; callee `fn_826AAEC8` steps by **0x44 = `sizeof(GameGem)`** where `Hmx::Color` is 16; the caller set is exactly our objects defining `~vector<GameGem>`; our old spelling scored 2.91%; the 136 B extent matches. ★ **Verified by the coordinator ON THE ARTIFACT before merging, not from the lane's report**: the map's textual diff is **60,411 lines** but the **semantic delta is 3 added / 0 REMOVED / 2 changed**, and the 2 changed are exactly the two above — the remainder is key-reorder churn, and nothing rode along. ⛔ **FIVE CLAIMS IN THE COORDINATOR'S BRIEF WERE WRONG — my error, not the lane's:** (1) "`fn_826B7BA0` not identified" — it is `HasChords`, now at 100; (2) `SetFret`'s penalties are **register swaps**, not relocation-name charges, and my inference *"`mpn == 100` ⇒ a relocation-NAME charge"* is **invalid** — `mpn` excludes **all** non-immediate arg diffs, not just name ones; (3) the `OFFSET_SWAP (0x54,0x58)` is **not a struct defect** — the base register is **r31, the frame pointer**, and the compiler's own layout report confirms our offsets, so **objdiff mislabels stack slots as `this` fields**; (4) the `DECOMP_FORCEACTIVE` lead is **refuted** — the oracle carries `"lefty_flip.anim"` precisely **because** it has no `HandleLegendLefty`, and ours does; (5) objdiff's `COMMUTATIVE_OP_ORDER (LikelyFixable)` is **measured wrong** on `SetFret`. ⚠ **Predictions, including the failures.** Every wave was pre-registered: `OnDisplayChord`'s inversion predicted **+108 B / +1 fn** and measured **+108 B / +1 fn exactly**; wave G predicted three rows → 100 and measured **all three exactly**, but predicted **+536 B** and measured **+1,908 B** — the surplus is the `0x826ab108` cascade across the other GameGem units, which the lane identified and **deliberately declined to price**. **Two predictions failed outright and are recorded as failures**: wave A measured **Δ0**, and wave D predicted 82.0 → 100 and measured **84.8**. **Left open with evidence:** `DisplayChord` (3,436 B @ 97.18, 44 register swaps r14↔r15), `SetFret` (716 B @ 99.888), `HandleLegendLefty` (316 B @ 89.49) — all permuter-class, and the permuter is **off by standing directive**. ★ `SetFret` is the strongest negative: `SetCorrect` **is** inlined (the non-commutative `andc` at idx 110 pins `r9=mCorrect`, `r10=mask`), so wave A's Δ0 was **genuine, not absent-vs-absent**, and production order is already identical, so **no source edit can reach it**. **Did NOT:** run the permuter, install any alias, or re-home `0x826ab108` in `splits.txt` — that pin looks wrong, but re-homing an already-pinned address is measured **non-neutral** (PINHOME-1: +3 fns / +428 B), so it is **flagged for a pinning lane** rather than taken here. The map's 2 duplicate names were verified **pre-existing in `HEAD`**, not introduced by this lane. Doc: `docs/decomp/W16CZ_CHORDBOOKPANEL_RESIDUE_2026-09-16.md`. `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`. |
| **W16-EA** Track `unk50`/`mIntroPlaying` swap adjudication | `4e7ae9b0` | ⛔ **REFUTED — the field swap DOES NOT EXIST, and the claim it refutes was ALREADY LANDED ON MAIN** (W16-DA's `6e9f1543`, which I relayed forward into EA's own brief). The base register at objdiff idx 136/137 is **`r1`, the STACK POINTER** — two spilled locals reloaded in swapped slot order, `Track` not involved at either row. The function reads `unk50` **zero** times, so the *"and vice versa"* half was **structurally impossible**, not merely unproven. ★ **Four instruments all agree with OUR declaration order**: the compiler (`/d1reportSingleClassLayout`, authoritative) puts `unk50` @ 0x60 / `mIntroPlaying` @ 0x70 and `--check-header` reports every `// 0xHEX` comment in `Track.h` agreeing; the rb3-Wii oracle carries the **same order uniformly −0x10** (a base-class shift — `unk50` is merely NAMED for its Wii offset 0x50); retail's only `this`-relative byte access in the whole function is `lbz r11, 0x70(r3)` = our `if (mIntroPlaying) return;`, which objdiff **already scores EQUAL at idx 5**. ★★★ **The CONTROL settles it, not the argument** — 3 pre-registrations, all held: applying the swap **broke idx 5** (equal → `diff_arg`), dropped the row 84.26344 → 84.258064 fuzzy, and left idx 136/137 **BIT-IDENTICAL**. ⇒ the implied "fix" does nothing to the rows it blames while breaking a row that already matched; price of the refuted fix **−1 fn / −112 B / −0.00109 pp**, baseline reproduced exactly on revert. ★★★★ **THE GENERALISING RESULT IS THE TOOL DEFECT**: `mcp_server.py`'s `_resolve_offset_mismatches` captured the base register as group(3) of `_MEM_ARG_RE` and **NEVER READ IT** — it compared offsets only and looked them up in StructDB under the enclosing class, so **any two stack slots at different offsets rendered as a confident struct field-swap claim complete with a `"wrong field?"` hint**; driving it on the real diff reproduced DA's sentence **word for word**. Fixed with `_NON_STRUCT_BASE_REGS = {r1, r13}` + recorded base registers, and the guard is shown to **DISCRIMINATE, not suppress** (removes exactly {136,137}, **retains** {65,73,74} as genuine `r30`/`r31` candidates; disabling it brings the false pair straight back). ⚠ **I had already been bitten by this exact mechanism once this session** — I read CZ's `OFFSET_SWAP (0x54,0x58)` as a struct defect when the base was **r31** — and booked it as my own misreading. It was **both**. **Gate: Δ0 on every key** (`43945 / 4122168`, 0 in / 0 out), pre-registered exactly, `NATIVE PASS 18/18 skipped=0`. Real blocker on the 744 B row, priced from the **charged-site list** not a mismatch count: 26 insert/delete + **62 `diff_arg`** (29 register, 31 stack/mem offset, 2 reloc-name) = a regalloc cascade off one extra callee-saved GPR ⇒ **permuter-class, and the permuter is OFF** — so *"guard it and it crosses"* is also off the table. |
| **W16-EB** anonymous `Unlockable` attribution adjudication | `80461dd6` | ⛔ **REFUTED as a general claim, and the refutation is NOT UNIFORM — the non-uniformity is the result.** `Unlockable` is **Dance Central 3's**, not a band3 anonymous type: these are **ICF fold survivors wearing the twin binary's spelling**, arrived via the `dc3_content_match` pass and re-keyed at the TU5 flip. Every row is a real retail function (retail prologue, `.pdata`-authoritative size) in a real pinned block — **not** phantom carves. ★ **Three-way split over 1,612 B**: **4 rows / 464 B real, held, identity-recoverable**; **3 rows / 292 B UNDECIDABLE — and the lane says so rather than guessing**, because those fold classes span **400–484 of 1,219 objects** so an owner hit is **33–40% likely BY CHANCE** and `OWNER=YES` is near-vacuous there (**the denominator is the control**); **4 rows / 856 B genuinely not held**. ⇒ the standing *"we do not hold the body"* finding is **semantically misleading for 7 of 11 and literally true for 4** — do not generalise it here; and **0 of 1,612 B is reachable by writing C++** regardless, since we can never emit that mangled name. ★ **The lane ran its discrimination control FIRST**, which is the only reason its zeros are worth anything — a 0-hit agreeing with the prior is this repo's classic vacuity; 4 known-matched rows of comparable size were each found, 1–2 hits, in the right unit. ★★★★ **THE LIVE FINDING, and the lane MEASURED THE OPPOSITE OF ITS OWN DRAFT**: it wrote *"`anon_ns` cannot act, our objects contain zero `f8e4b4b5`"* and then found the hash **6× in `Sequence.obj` and `SkeletonClip.obj`** — the pass asks the **TARGET** obj which hashes belong, the only candidate there is the DC3 import, so it **stamps a foreign hash** onto our `Sequence.cpp` anon namespace and a `SkeletonClip.cpp` lambda; **9 of 2,517 target objs exposed**. This is a correctness exposure **on the scoring path**, because `anon_ns` is the **ONE patcher of six that moves the metric** (+82 fns / +14,604 B by ablation). **Filed with mechanism proven, DELIBERATELY NOT ACTED ON** (a patcher/map change needs a whole-binary A/B). ★ **Prior art corrected**: `bp4-vocaltrack` walled these as *"foreign COMDATs"* on a **source-absence warrant that is invalid under ICF** (2 of its 3 VocalTrack rows are VocalTrack's own instantiations); `lane-au-4`'s `WRONG-UNIT`/`n_definers: 0` label is **tautological** for this family; the DC3-ONLY-PINS `OBJ_CLASSNAME` string test has **no recall** on a POD struct in an anon namespace, so `Unlockable`'s absence from `band.exe` is **evidence of nothing**. ⚠ **2 of 3 pre-registrations FAILED and the three-way split exists BECAUSE they failed.** **Coordinator verification of the briefed figures** (I relayed an unverified lane claim into a brief earlier this session and it was false, so these were tested literally): map rows carrying `f8e4b4b5` = **11**, and it is the **top anon-ns hash in our map** of 70 distinct; DC3 map **81/81** `f8e4b4b5` lines from `meta_ham:MetagameRank.obj` ✅; the struct is at `MetagameRank.cpp:44`, anon namespace, size 0x20 ✅. ⚠ **One correction — do NOT relay EB's sentence verbatim**: *"every one from MetagameRank.obj"* holds for `f8e4b4b5` (81/81) but **NOT for `Unlockable`, which is 64 of 68**, the other 4 being `ProfileMgr` / `HamProfile` / `AccomplishmentManager` / `AccomplishmentCharacterListConditional`. Conclusion unaffected. **Gate: Δ0 on every key** (doc-only, 0 ruler-path files), pre-registered exactly, `NATIVE PASS 18/18 skipped=0`. ⛔⛔ **COORDINATOR CORRECTION, MEASURED AFTER LANDING — the `anon_ns` finding above is REAL AS A MECHANISM but its *"correctness exposure on the scoring path"* framing is WRONG, and I amplified it from EB's report into this row and into the merge message.** Dry-run tier attribution (`obj_anon_ns_patcher.py --batch --verbose`, non-writing, tree verified still a fixed point) resolves **all 6 occurrences by tier 7 `majority`** — `SkeletonClip.obj majority=5`, `Sequence.obj majority=1`. **Tiers 1–4 could not fire**: our objs and the paired target objs share **ZERO** templates (ours are `?RandomVal@?A0xf8e4b4b5@@` and three `<lambda0>` STL instantiations — **names retail never emitted**), which is exactly the population the patcher's docstring says tiers 5–7 exist for, and for which *"a fallback provably cannot manufacture a match: if a fallback-assigned name coincided with a retail name, the template lookup would have found that name first."* ⇒ **metric-neutral BY CONSTRUCTION**, not a scoring-path exposure. ⚠ And it is **not even anomalous**: tree-wide the assignment histogram is `majority=844, template=195, template_stripped=145, token=165, template_global=59, token_global=35` — **`majority` is the DOMINANT rule** (WaveFile.obj 279, Sfx.obj 87, CheatProvider.obj 87), so our 6 are unremarkable. ⇒ **W16-ED (the planned `anon_ns` follow-up lane) is CANCELLED, not deferred** — there is no defect to fix. ★ **My inference chain was wrong TWICE before the tool settled it**: I first reasoned from occurrence *counts* in the paired target objs to "tier 1, map-injected", then from 0-shared-templates to "benign" — both were guesses, and only the patcher's own per-tier report is evidence. Recorded because the counts *looked* decisive both times. |
| **W16-EC** wrong-callee map names (opus) | `746736dc` | **+5 / +432 B** | Four charged `bl` sites adjudicated on retail bytes: **our source was right and the MAP was wrong** in two of them. FIX A BandUser's `Symbol` wrappers transposed (`0x8268bab0`/`0x8268bb68`) — each tail-calls a *different* enum overload, and two wrappers whose relocations differ **cannot be ICF-folded**, so W16-CU's correct T1 byte-identity was mis-read as a fold; that group is **withdrawn**. FIX D `NetGameMsgs` `Dispatch` transposed, grounded three ways (callee, **vtable co-location in retail `.rdata`**, and the fold pattern). FIX B/C are real source wrong-callees (`GetStarsToken`, `ImportSettingsFromFont`). **No alias installed — `folded` went 5475 → 5474, a WITHDRAWAL**: an unproven alias lifts `name_check` by construction and the `none` control cannot catch a fabricated one. ★ **The MISS is the lane's most useful event**: wave 1 predicted +300 B/+4 and measured +168 B/+3 — every per-fix row prediction exact, the whole shortfall one unpriced collateral (`Dispatch@SetUserDifficultyMsg` 100 → 99.84849, −132 B) whose stated downside **named the wrong mechanism**; that regression is what located FIX D, which the queue never surfaced. Wave 2 predicted +432 B/+5 *including* that the pair nets +132 not +264 — **measured EXACT**. Corrects three inherited figures: queue liveness 203 → **140 live/376 dead/19 unknown**, NOGROUP1.tsv 313 → **91 live/102 dead/120 absent**, and the renamer docstring's "10 sub-100 `_bijection_arbitrary` rows" → measured **57 / 3,728 B**. `?SetControllerType@BandUser@@` deliberately dropped (5 map rows → 4): it sat at fuzzy 99.3333 and was never collecting, so removing a false claim costs 0. |
| **W16-EE** `_bijection_arbitrary` transposition (opus) | `35188831` | **+2 / +280 B** | Two `BandTrack` members were SWAPPED in the map (`0x8234dd18`/`0x8234dd30`, `UnisonStart` <-> `UnisonEnd`); both VAs carried the `_bijection_arbitrary` flag, i.e. **the map itself recorded which name went on which VA as unestablished**. The rows that crossed are the two *callers* (`TrackPanelDir::UnisonStart`/`UnisonEnd`, 140 B each) — the mis-named rows read 100 throughout: **a wrong name is financed by its callers.** Gate reproduced the pre-registration on EVERY key, and **both named failure modes were REFUTED rather than untested** — the 20 B `BandTrack` rows held at 100 (placeholder forgiveness applied; otherwise **+240 B / +0 fns**) and `SPLIT lines: 2` proves the map edit was live, not inert. ★★ **`ALIAS_SUSPECT` fired and is a FALSE POSITIVE**: `none` cannot register a relocation-NAME change at all, so its flatness is **vacuous in both directions** — what separates a transposition from fabricated forgiveness is that a swap is **SELF-INVERSE** (the wrong orientation is the status quo and scores *worse*). Precedent is the same shape and the paying class: **MAPDEF-3 `db9eb318`, +108 B from 9 wrong-name repairs, `none` unmoved at +0**. No alias touched. ⛔ **A vacuous discriminator the lane caught in its OWN reasoning, which would have sent the swap BACKWARDS**: "`fn_82305B00` never touches `r4` ⇒ takes no int" is false — `r4` is passed *through* and **both siblings leave it alone**. The evidence that works is asymmetric (`EndingBonus::UnisonStart(int)` consumes `r4`; `UnisonEnd` **clears** the `+0x1dd` flag `UnisonStart` sets), corroborated by member order read from **the compiler** (`mStartTrig@0x1e0`/`mEndTrig@0x1e4`), not from `// 0xHEX` comments. ⚠ **W16-EC's `PropKeys::Copy` verdict HOLDS but its stated MECHANISM does not** — both halves charge the identical `Key<float>` survivor, so it is **not a 2-cycle at all**. ⛔ **VEIN DRAINED — do not re-fund**: a mechanical 2-cycle detector over all **54 rows / 3,476 B** returns **exactly two** transpositions (this one, and `PropSync` priced at **0** by a shared `_M_fill_insert<vector<Object*>>` survivor, independently confirming EC). Only **8.1%** of the stratum was ever adjudicable. |
| **W16-EF** `Game` unit, ButtonMsg adjudication (opus) | `883430f5` | **+3 / +408 B** | Owned `default/band3/game/Game` (15 named sub-100 rows / 7,180 B; 7 of them named by W16-CB on 09-15, i.e. previously-invisible source divergence, not neglect). `RemoteLeaderLeftMsg` 232 B 53.4655->100 · `SetRealtime` 176 B 49.1364->100 · `ButtonUpMsg` 156 B -> **mpn 100 (+1 fn, +0 B)**. ★★ **CB's NAME IS RIGHT AND CB's FALSIFIER IS UNSOUND.** CB wrote *"a wrong identification lands near 0"*; the row sat at 3.24%. EF tested it on retail bytes BEFORE writing source — `0x8267b808` and `0x82679900` have instruction-for-instruction identical prologues differing in exactly ONE instruction (`addi r9,r9,1` vs `addi r9,r9,-1`, the press/release counter pair), matching 3-arg retbuf shape, one caller each inside `?Handle@Game@@` at ascending offsets matching our `HANDLE_MESSAGE` order. ⇒ **the falsifier is NECESSARY BUT NOT SUFFICIENT: a CORRECT name on a STUBBED body also lands near 0.** The 3.24% measures **body coverage, not name correctness**, and acting on it would have RETRACTED A CORRECT NAME — same family as an `AT_LIMIT` on a reloc-name-only row (a metric downstream of the thing under test cannot adjudicate it). ★★ **Its own pre-registration #2 MISSED and that is the deliverable**: `SetRealtime` was predicted NOT to cross on 13 register charges; **all 13 dissolved once ONE structural defect was fixed** (`addi r29, r28, 0x6c`) — the **13th recorded instance** of `REGISTER_SWAP` being a symptom, not a diagnosis. ⚠ **Alias caveat, recorded so it is never mis-cited**: one member (`??1Shuttle@@QAA@XZ`) added to group `0x826c3888` (86->87, 1,657 groups unchanged) via `tools/fold_thunk_gate.py`; the gate ADMITted at **`FT-EMPTY` and SELF-DECLARED ITS BYTE TEST VACUOUS** (8-byte body, zero relocations). The install rests on **CB's call-site evidence, NOT the fold gate** — no later lane may cite `FT-EMPTY` as proof of folding (the same error as the old "OK (grounded)" -> "OK (MAP-CONSISTENT)" rename). **260 B PRICED AND LEFT ON THE TABLE**: `OnSetShuttle` is a genuine ICF fold (`0x826f07b8` = `stb r4,8(r3); blr`, 8 B, zero relocations; `Shuttle::mActive` @0x8; call site passes `this+0xe0`; the map holds NO `SetActive@Shuttle`) — **our source is right**, and closing it needs a second alias this lane was not authorized to install. Best next target: `?OnMsg@Game@@...ButtonDownMsg@@@Z`, **1,664 B**, name now adjudicated, prologue already written and proven as `ButtonUpMsg`'s twin. |
| **W16-EG** the four rows with NO lane document (opus) | `50d494c8` | **+1 / +412 B** | Dispatched on a filter with nothing to do with size — **rows in the game layer carrying no prose document anywhere under `docs/`** — because ranking the same census BY SIZE selects rows previous lanes already finished (the size-ranked top five held one row W16-CQ closed as uncrossable and two that were W16-CA's own output). `RefreshAll@ManageBandPanel` **0.0000 -> 100.0** (+412 B); `Poll@PatchPanel` 91.4571 -> 94.5918 paying **exactly 0 B** (all-or-nothing per row); `Poll@StoreMainPanel` and `HandleExitExtent@PerfectSectionTracker` deliberately walled. ★★ **Pre-registration P2 MISSED and generalises**: `StoreMainPanel` looked 50/50 to cross because 12 charges sat in ONE cluster. Both sides already had identical predicates **and** identical per-branch null tests — the difference is **MSVC COLD-BLOCK OUTLINING**, retail's `else` arm sitting after `b __restgprlr_25`. ⇒ **a charge CLUSTER is not evidence of a source construct; the block's placement relative to the EPILOGUE is.** P1/P3/P3b/P4 held, P3b to the byte. ★★ **THE DISPATCH HYPOTHESIS WAS REFUTED — the 0% row is NOT a phantom or an identification problem.** `0x82625b58` is an exact `.pdata` BeginAddress, extent 412 == billed size, in the correct pinned block, correctly named. The cheap tell came BEFORE any `.pdata` parse: `report.json` gave it **`mpn = 2.378641`**, and **a truly unpaired row reads 0 on BOTH keys** — so a non-zero `mpn` proves objdiff paired and compared it. Reusable as a first-line screen. ⚠ **The rb3-Wii oracle was wrong THREE times on one function** (inlines a loop retail factors out of line; lacks retail's outer `if (mProfile)`; carries a `MetaPanel::sUnlockAll` cheat absent from all 332 B of retail's helper) **and right-side-up once** — it declares `HandleExitExtent` `void` while the retail MANGLED NAME says bool, i.e. our source was already right. Three "wrong callee" leads were checked FIRST and are ICF folds already forgiven by alias groups 2, 30/83/169/170 and 35. ★ Two single-cause findings: hoisting ONE static fixed the code motion, the `PROLOGUE_MISMATCH` **and** both save-helper charges together; removing two local member caches dissolved **four** separately-labelled patterns at once. ⛔ No map/alias/splits edit (verified, not inherited) ⇒ no merge-order dependency. `fn_826259E0` (332 B) and `fn_82624F88` (272 B) left UNCOLLECTED as identification work — **W16-CD's split holding a second time: implementing `RefreshAll` perfectly and stopping measures +412, not +744.** |
| **W16-EI** MaybePublish is ALIAS-GATED (opus) | `c2e92d00` | **+0 / +0 B** | Refuted its own brief. `?MaybePublish@UIStats@@` (2,604 B, fuzzy 99.579 / mpn 99.656) carries 65 charged instructions of 652; of 8 symbol diffs, **6 target `lbl_` placeholders that `name_check` forgives** and **2 name ICF fold survivors** no source edit can reach (`0x82801f78` → `GetContainerName@MemcardXbox`, an existing alias group folding `GetColor@UIColor`; `0x827c1378` → the `vector<int>` **copy** ctor, with no `vector<BandUser*>` copy-ctor spelling anywhere in the map). Since `matched_code` is all-or-nothing on `fuzzy == 100`, closing every instruction-level charge buys `mpn` 100 (+1 fn) and **exactly 0 bytes** — the `CustomizePanel` shape. **AT_LIMIT, 5th attempt, first to say why.** Declined to install the two aliases after sizing them at **zero collectable rows**. Gate reproduced the Δ0 pre-registration exactly. |
| **W16-EH** ButtonDownMsg head + `Shuttle::SetActive` (opus) | `e16ea909` | **+0 / +0 B** | Refuted its brief three ways on retail bytes. **(1)** "prologues identical but for ONE instruction" is FALSE — UP guards its decrement with `> 0` and re-loads `0x48(this)` (9 insns), DOWN increments unconditionally off a single load (5 insns), reversed `lwzx`/`stwx` order, frame `0xe0` vs `0x80`, saves `__savegprlr_23` vs `_28`; **copying UP and flipping `--`→`++` produces WRONG CODE**. **(2)** "call site passes `this+0xe0`" is FALSE — retail `lwz r3, 0xe0(r31)` LOADS the pointer, so `Shuttle *mShuttle` is right (EF's verdict survives, its reason does not). **(3)** construct list incomplete. Two real fixes landed on merit: the per-pad counter ButtonUp decrements was one **nothing ever incremented**, and `Shuttle::SetActive` was declared + called but **defined NOWHERE** (unresolved external). Row moved fuzzy **3.238 → 8.010** for **exactly 0 bytes** — all-or-nothing per row. Gate reproduced Δ0 bytes with Δfuzzy **+0.000795 pp** to 6 dp. |
| **W16-EJ** the hidden near-crossing band, enumerated (opus) | `5a4dfeef` | **+1 / +572 B** | Dispatched on my claim that all 22 game-layer rows at fuzzy ≥99.95 were source-reachable **because all 22 had `mpn < 100`**. It banks the one crossing and **refutes the rationale entirely**. `VocalGuidePitch.cpp` carried a legacy Metrowerks/Wii `extern "C"` shim (`NoteAt__13VocalNoteListCFf`) and called it instead of `VocalNoteList::NoteAt` — declared, defined, and already called normally at `VocalPlayer.cpp:526`. **THE REFUTATION IS WORTH MORE THAN THE BYTES:** of 26 charged sites across the 22 rows, **24 are relocation-name and only 2 are instruction-level** (both immediates); **20 of 22 rows carry ZERO instruction-level charges** despite every one reading `mpn < 100`. Mechanism: objdiff-core **`b14ba45`** `vetted_reloc_name_diff` excludes a vetted relocation-name diff from `arg_diff_score` while keeping it in `diff_score`, so it no longer cancels out of `mpn = diff_score − arg_diff_score`. ★ Free tell: **`mpn == fuzzy` to the digit ⇒ `arg_diff_score == 0`** — not "no relocation charges" but "every one was vetted and promoted into `mpn`". Also refuted my *third* screen the same day: dropping every row with a class-(c) charge would have discarded **the only row that crossed**. Deliberately did NOT land 17 `--chase`-proven aliases (13 rows / 10,136 B) — `--chase` is not a declared T1/T2/T3 tier and the pipeline cannot re-derive it. |
| **W16-EL** `AccomplishmentProgress` key type — **REFUTED** (opus) | `a50b1e19` | **+0 / +0 B** | Dispatched on W16-EJ's named top candidate: charge `addi r3, r30, 0x64c` reaches `mGigTypeCompletedMap`, declared `hash_map<int,int>` where retail was claimed to build `hash_map<Symbol,int>` (588 B, single charge). **The claim is refuted on retail's own symbol table, which spells the container outright.** `SaveFixed` (`0x825909b8`) calls `SaveStd<Symbol,H>` three times for the sibling maps and then `fn_82590198` = `SaveStd<H,H>` for `+0x64c`, whose parameter is literally `const hash_map<int,int>&`; `LoadFixed` mirrors it at `0x82591420`; `operator[]` and `_M_find<int>` differ from the siblings' `_M_find<Symbol>`. The three sibling `Symbol` maps are the **untreated control** and they read the other way, so the instrument discriminates. **Our source was right and the brief was wrong.** Economics, had it been true: **2,764 B of already-perfect rows at risk for 588 B**, and `SaveFixed`/`LoadFixed` break **by construction** under a `Symbol` key. ⚠ **No A/B was run because the change is not expressible** — `mGigTypeCompletedMap[i + 0x3E8]` (`:549`) cannot compile with a `Symbol` key. That is a **result, not an omission**. The 588 B is real but blocked elsewhere: our hashtable-ctor spelling is already placed at `0x825a07e0` by an existing 11-member group while retail's `0x8255d480` branches to `0x8255c968`. ★ And **"identical ⇒ folded" is empirically FALSE here** — retail `0x8255c968` and `0x825983d8` are byte-identical *including their `bl` target* and still sit at two addresses. |
| **W16-EK** fold-thunk gate read one side from COFF, the other from instruction form (opus) | `97f76e42` | **+1 / +260 B** | Dispatched to adjudicate two defects W16-EH *claimed* in `tools/fold_thunk_gate.py`. **Both hold — and EK's own finding is that they are COUPLED**, which EH missed and which is what makes the fix sound rather than a loosening. **(1)** Retail's relocation set was **inferred from instruction form** (`elif op in IMM16_OPS: targets[4*i] = None`) while ours was **read from COFF** (`cd["fn_relocs"]`), and `compare()` refused on the asymmetry — `stb` is in `IMM16_OPS`, so retail recorded a relocation slot where a linked image has none and the `8` is a literal. **(2)** `mask_word` zeroed the low 16 bits, so `stb r4,8(r3)`, `stb r4,0xc(r3)` and `stb r4,0x7ff(r3)` **all** mask to `0x98830000` — the displacement passed **by construction**. ★ Fixing the asymmetry ALONE would have been a real loosening, because the vacuous mask means masked words compare equal for **any** displacement; landed together the gate is **strictly tighter**, and EK proved it can still fail (`+0x8` admits ×2; `+0xc`/`+0x24`/`+0x0` REFUSE **by displacement**). Prize priced from charged sites, not a mismatch count: `?OnSetShuttle@Game@@` is 260 B over 65 rows with **exactly one charged row**. **Two corrections to my brief:** "0 of 29 REFUSEs change" is right but **scope-limited** — it reproduces on the 36-pair worklist (0 of 36) while across all **1,048** triage pairs the defect blocks **8**, **8× my briefed reach** (8/8 REFUSE→ADMIT, 0 the other way); and my precondition was **half-true** — `SetActive` was in source but the worktree's inherited `Shuttle.obj` was **stale with no COMDAT** until a build ran. **EK's own P3 pre-registration FAILED and it records it as failed** (predicted ≤2 flips, measured 8). ⚠ **One mid-lane error, self-caught by measuring:** its first patch threaded the COFF mask through `homonym()` — which is **linked-vs-linked**, where form-inference is correct *because it cancels* — flipping a **1,180-site** pair ADMIT→REFUSE. Gate reproduced the pre-registration to the byte. |
| **W16-EM** `XboxPurchaser` has **no `Hmx::Object` base** (opus) | `202531e5` | **+0 / +1,028 B** | Dispatched on a size discrepancy W16-EJ found and deferred: retail allocates with `li r3, 0x28` (40 B) while ours was `0x50` (80 B), the `0x28` difference being exactly the `Hmx::Object` contribution. **Retail's `XboxPurchaser` has no `Hmx::Object` base at all** — every site was allocating twice what retail does. DC3 declares the same multiple inheritance, so **a diff against the oracle shows nothing**: DC3 is newer and the `Object` base is one of its additions. ★ **My arithmetic was TRUE BUT INSUFFICIENT and the lane said so** — under MSVC MI a secondary base's slots get a `this` already adjusted to that subobject, so `: public Hmx::Object, public StorePurchaser` **also** reads `mState` at `this+0xc`. EM made that rival refutable and refuted it: a swap predicts `mdisp=0x28` + four `??_R2` entries; retail measures **`mdisp=0x0`, two entries, `attributes=0x0`** (MI bit CLEAR). Five instruments, each able to refute: `??_R4`/`??_R3`/`??_R2`; **exactly one** `??_R4` in `.rdata`; ctor `0x827b2800` stores **one** vptr and calls no base ctor; dtor `0x827b28a0` writes that same slot twice; three sites at `li r3,0x28`. **The correction that carries it:** member order came from the CTOR, not the header — `mOfferID` at `0x10` (an 8-byte store), and `unk3c` **does not exist** (no attestation, no referents), leaving `0x1c` free exactly where `Poll`'s `stb r11,0x1c` needs it. ★ **A real bug the metric cannot score:** `PurchaseMade()` returned `false` unconditionally, so the success path could never be observed. **Casualty named and arithmetic closed:** `??_GXboxPurchaser@@UAAPAXI@Z` fell out (76 B, 100 → 64) because an honest dtor became trivial enough for MSVC to inline; **+1,104 − 76 = +1,028 exactly**, 189 → 189 units at 100%. **EM's own pre-registration MISSED** — it predicted `??_G` holds, which was the risk it had itself named first, and records it as a miss. Left `XboxMultipleItemsPurchaser` untouched: it appears **0 times in retail** (DC3-only), so it has no retail layout to be wrong against. |
| **W16-EP** EK's 8 unblocked pairs were **already installed** (opus) | `079beb54` | **+0 / +0 B** | Dispatched to adjudicate the 8 alias pairs W16-EK's COFF-relocation mask flipped REFUSE→ADMIT, standing default REFUSE. **It installs nothing and zeroes its own brief.** EK's count reproduces exactly (relabelled all **1,048** triage pairs, ran the pre-EK gate `97f76e42^` and the current gate over one worklist and build: 7 → 15 ADMIT, **8 flips, all REFUSE→ADMIT, 8/8 carrying the asymmetry reason**; tree built to a fixed point of all six patchers first, with the stale-obj trap checked explicitly rather than assumed — 680,543 COMDATs, both `SetActive` and `Enable` present). **But 6 of the 8 folded spellings are ALREADY in `scripts/symbol_aliases.json`** — including *both* pairs that clear the evidence bar — so the install is a literal no-op (`0 new, 0 updated; 1658 total`, `git diff` empty), and the only two absent are the two the evidence refuses. ⇒ **EK's "8 blocked pairs" measured the defect's REACH correctly and its VALUE not at all**: the unblocked admissions are mostly the gate finally agreeing with memberships we already ship. That is a **reproducibility win, NOT uncollected headroom — and I briefed it as one.** ★★★ **The instrument is the durable part, and it refutes two pairs on retail bytes.** The lane built a whole-image body census over **every `symbols.txt` extent** — masked by form, *then* requiring identical **resolved branch destinations** — because `retail_bodytwins` counts only **PINNED** objs and therefore systematically undercounts retail's copies. Retail keeps **ELEVEN** byte-identical 84-byte `list<T*>::erase` bodies, identical **including their `bl` targets** (the complete `/OPT:ICF` condition), **unfolded at eleven distinct addresses**; the "41 of ours → 1 retail address" pigeonhole was a **pinning-coverage artifact pointing the wrong way**. ⛔ **CORRECTED THE SAME DAY — this specific evidence is WITHDRAWN as a masking artifact; see the correction section at the end of this file.** ✅ **And RESOLVED the same day (`7ba4d7f8`): EP re-ran its own census, reached zero survivors independently, and pairs 5 and 7 FLIP TO CLEAR** — the node-size word that refutes the eleven *confirms* those two. It also found that **ICF is iterative, so a flat one-pass class produces FALSE refutations** (pairs 2 and 6 land outside their own survivor's class), and left **pair 1 admissible but deliberately UNINSTALLED** as an unpriced change. The eleven bodies differ at a **non-branch immediate** (`li r3, 84` vs `li r3, 24`, the per-`T` node size), verified by me on raw retail bytes, so `/OPT:ICF` could never fold them and they are eleven *different* functions sharing a shape. The principle "byte-identical ⇏ folded" still stands, but on **W16-EL's** pair (`0x8255c968`/`0x825983d8`: 30 words, one differing `bl`, targets resolving to the **same** `0x8256ad18`), which I verified the same way. The rest: **pair 1 REFUSE** (8-byte body whose single relocation is the *shared* `MemAlloc`, so the discriminator does no work; 214 of our symbols share the body; retail keeps 2 unfolded copies); **pair 6 UNDECIDABLE** (survivor in neither the map nor any pinned obj); **pair 8 REFUSE** (vacuous — 411 masked-identical bodies); **pairs 2 and 4 clear the bar on a CALL-SITE argument** rather than a body argument, and were already installed. The **size-differing class is structurally absent** from the admitted set — `compare()` refuses on body length *before* admission — so it can only be hunted among the refusals. **Two corrections for anyone pricing off these files:** the 2026-08-12 triage worklist's `sites` column is **STALE** (`BSPFace::Update` carries 36 charges, *all* regalloc, zero fold-name), and `src/band3/meta_band/ContextChecker.cpp:262` `GetSongSpecificEntriesForCategory` is an **unimplemented stub** returning an empty vector whose caller consumes the result — 20 B, which is exactly why it is byte-identical to a `_Vector_base` ctor (reported, **not** aliased away; its membership is incidental to a 1,030-member blanket group). **Self-reported honesty items:** pre-registered 0 installs and measured 0 **but for the wrong reason** (it expected template-twin refutations; the relocation targets turned out name-equal) — logged as a **near-miss, not a hit**; and **no A/B was run, deliberately**, because the tree is byte-identical to HEAD and reporting settling noise as a delta would be fabrication. Pair 3's membership was **not** withdrawn (the gate has no `--withdraw` path and hand-editing the generated alias file is forbidden). Doc-only branch ⇒ **structurally incapable of moving any metric key**, which is what licensed co-landing it with W16-EO under a single gate while keeping every delta attributable. |
| **W16-EO** the MWCC shim vein is **EMPTY**; `ScoreSinger` identified for 0 bytes (opus) | `cc6b987a` | **+0 / +0 B** | Dispatched on my claim that `src/band3/game/VocalPart.cpp` repeats W16-EJ's **+572 B** — three MWCC shims, same repair. **The premise is REFUTED, and the reason is pairability, not source quality.** ★★ **The three shims do not have the same answer**, which is why a single verdict would have been wrong. `NoteAt__13VocalNoteListCFf` **FIXED** — the member is declared (`beatmatch/VocalNote.h:115`) and defined (`VocalNoteList.cpp:569`), and **retail names its callee too** (`?NoteAt@VocalNoteList@@QBAPBVVocalNote@@M@Z` at `0x82781078`), so this was a genuinely charged wrong name rather than a forgiven one; verified at the COFF level after a clean rebuild. `PitchAt__13VocalNoteListCFf` **FIXED but INERT BY CONSTRUCTION** — its only call site sits inside `#ifdef HX_NATIVE` and the match build never defines it (` /DHX_NATIVE` count in `build.ninja` is **0**); proved **non-metrically**, the *pre-fix* `VocalPart.obj` contains no `PitchAt__` symbol at all, because **MSVC emits nothing for an unused `extern` declaration**. `kInvalidPitch__11VocalPlayer` **DELIBERATELY KEPT** — it is a **data** symbol and **no C++ declaration for it exists anywhere**, *including the rb3-Wii oracle*, which carries the identical shim and whose own `VocalPlayer.cpp` writes the bare literal `1000.0f` instead of referencing it. On retail bytes it is **not** constant-folded (`0x447A0000` appears in neither `VocalPart.s` nor `VocalPlayer.s`); retail loads a real external float global via **`lbl_820F14B4` — a PLACEHOLDER name, which `name_check` FORGIVES** — so our wrong spelling costs **zero charges, paired or not**, while naming it would require inventing a static member plus a definition in a *different paired unit* to produce a **guessed** mangling (`?kInvalidPitch@VocalPlayer@@2MA` vs `@2MB` **cannot be discriminated from any evidence in the tree**, precisely because retail never folds the constant and the target is unnamed). Zero measurable upside, real fabrication risk. **ESCALATION ITEM: settling it needs retail `.rdata`/`.data` storage-class evidence at `0x820F14B4`.** ★★ **THE VEIN IS EMPTY — do not re-fund this sweep:** 239 raw validated MWCC-shape hits across `src/` reduce to **230 Ogg Vorbis codebook tables** and **9 real**, and all 9 are those three symbols plus their three definitions in `native/src/m10_support.cpp` (post-fix 5 remain, every one deliberate). `VocalGuidePitch.cpp` was the only sibling and W16-EJ already cleaned it — **there is no third file.** ⚠ Trap worth keeping: **length-validation does NOT exclude the vorbis tables** (`_vq_quantlist__8u0__p1_0` really *is* `__8` followed by 8 characters); only recognising the data family does. ⚠ **And EO re-derived the census after reproducing MY vacuous version of it** — `grep -rn … --include=*.cpp` printed `0 files` because **zsh errors on the unquoted glob and the command never ran**; the real census used Python, which is immune to globbing. **THE PIN, AND WHY IT PAYS NOTHING:** `fn_826F3658` (472 B) is identified as `?ScoreSinger@VocalPart@@QAAXMMMMHPAVTalkyMatcher@@AAVVocalScoreCache@@AAHAAM@Z` on retail bytes. Settled A/B with a forced re-split (`renamer_patched=1830`, both legs at a `symbols.txt` fixed point): **Δmatched +0, Δcode_bytes +0, Δfuzzy +0.004200 pp**, `none` control **FLAT** with no `ALIAS_SUSPECT` flag. The row goes from **unpaired-and-invisible (fuzzy 0.0, never compared)** to **fuzzy 90.881355 / mpn 91.135590**, and pays **0 bytes** because `matched_code` keys on `fuzzy == 100` and is **all-or-nothing per row** — EO pre-registered exactly this as the **75% branch** of three named failure modes, and that is the mode that fired. ★ Per the standing directive that **accuracy beats headline %**, an identification that pays 0 bytes while exposing genuine instruction-level divergence (`mpn < 100`, implied `diff_score` 1076) is worth landing: it converts a 472-byte blind spot into a legible follow-up target. ★★ **The gate PROVED the pin took rather than assuming it** — `ScoreSinger` absent pre-merge, present post-gate, `fn_826F3658` rows **1 → 0** — the assertion that separates a real Δ0 from the **inert-map-edit Δ0 that cost lane CF-1 an entire leg**. (`build.ninja:72-76` makes the renamer stamp depend on `scripts/target_symbol_map.json`, so ninja re-ran it automatically; the assertion confirms it did.) **Self-reported process defect:** EO's first COFF assertion **FAILED** — shim still present, member absent — and it was a **TORN READ**: it inspected `VocalPart.obj` while `ab_measure` owned the tree and had reverted it to build leg A (confirmed by an empty mid-run `git diff --stat`). Re-asserted on a quiescent tree, all five pass. The failure was **shaped exactly like a real refutation**, which is what makes it worth recording: **never read build outputs while an A/B owns the tree.** ⚠ **And it corrects a rule I had adopted three lanes earlier:** `pgrep -x ninja` reports **NO while a build is running**, because the process is `/bin/sh tools/ninja-locked`, not a binary named `ninja` — use `ps -eo pid,etimes,args` with cwd inspection. Deliberately not done: the remaining ~9% of `ScoreSinger`; the two now-dead shim definitions in `m10_support.cpp` (nonzero link risk, no benefit); and **no candidate address was planted for `?Poll@VocalPart@@`** — only that it is absent from the 34 named `VocalPart` rows. |
| **W16-EN** `ExtraTail`'s size was never wrong — the **assignment operator** was (opus) | `3ac74e78` | **+1 / +656 B** | Dispatched on **my** claim that `vector<ExtraTail>::_M_erase` being 88 B against retail's 96 B meant our **element size** was wrong. ★★★ **The brief is refuted by the compiler: `sizeof(TrainerGemTab::ExtraTail)` and `sizeof(RndLine::Point)` are BOTH 72 B (0x48)**, and retail agrees — its own erase body loads `li r10, 0x48` as the stride. The real cause is **assignment triviality**: `Transform` and `Hmx::Matrix3` both user-declare `operator=`, which makes `ExtraTail`'s implicit copy-assign **memberwise** (a `0x40` memcpy plus two scalar stores) rather than one whole-object memcpy, and that fatter loop body is why MSVC declines to inline STLport's `__copy` into `_M_erase` — an 88-byte out-of-line form where retail emits the 96-byte inlined memcpy loop it shares via `/OPT:ICF` with `vector<RndLine::Point>::_M_erase`. An explicit memcpy `operator=`, the same idiom `Transform` itself uses, restores retail's codegen (semantically identical; it additionally copies 3 tail padding bytes, unobservable). ★★★ **THE DURABLE RULE, and the one I failed to apply: a size disagreement between our COMDAT and retail's is evidence about our CODEGEN, not about a data type.** The withdrawal record my brief came from literally reads `our(S)=96 B vs our(F)=88 B [retail(S)=96]` — a disagreement *within our own build*, retail constant at 96 on both sides. ★★ **BOTH HALVES ARE LOAD-BEARING.** The row that crossed is `?Draw@TrainerGemTab@@QAAXH@Z` (656 B), which sat at `fuzzy 99.96951` **and** `mpn 99.96951` — equal to the digit, so `arg_diff_score == 0` and its single charged site (`diff_score` exactly 5.0, re-derived by me from `report.json`) was a **vetted relocation-name charge** on the call to `_M_erase`, promoted into `mpn` by objdiff-core `b14ba45`. The source fix alone could **not** cross it; it crossed only once the ICF membership was re-admitted — and the membership is only admissible once our body is 96 B. **THE RE-ADMISSION USED THE SANCTIONED ROUTE, not a quiet list-flip:** the withdrawal record is **KEPT** (history + denylist intact), a new `scripts/alias_withdrawal_overrides.json` names it explicitly, and `load_overrides` requires `overrides_class` to equal the ledger record's class — so the override **cannot be written without having read the record it overrides**, and it was accepted by the real loader. Audited at the gate before merging: `_comment` preamble unchanged, groups 1658 → 1658, **survivors +0 / −0, exactly ONE group changed, exactly ONE folded membership added and ZERO removed** ⇒ a re-admission, not a rewrite. ★ **The override is deliberately SELF-LIMITING** — valid only while `ExtraTail::operator=` exists, and if that is reverted the T1 adjudicator refuses the pair on its own, independently of the override. That property is what it was landed on. ⚠ **The `none` control is FLAT and that is NOT a clearance** — an alias lifts `name_check` by construction and `none` is blind to relocation names; `ab_measure` itself labelled the run **NOT_APPLICABLE** for alias adjudication because `source` is in the patch (default-UP/`none`-FLAT is *also* the wrong-callee-fix signature). The licence is **retail bytes**: FLAT T1 PROVEN (`retail_size 96 == our_size 96`, `reloc_tally {}` over 3 relocs, survivor map-resident), the adjudicator's `--selftest` passing **with a REFUTED negative control**, and two sibling STL functions on these same two element types (`__uninitialized_copy`, `__uninitialized_fill_n`) already folding in this same file. **Also corrected:** `TrainerGemTab.h` carried `// size 0x38` for `ExtraTail`, inherited from the rb3-Wii header where `Transform` is smaller — now `0x48`, compiler-verified **in the comment itself**; the latent-trap class that misleads the next lane. **New instrument:** `tools/retail_body_multiplicity.py` counts retail copies of a body with **branch destinations resolved**, which `retail_bodytwins` cannot do (it counts only **PINNED** objs and so undercounts retail copies) — its control finds 769 two-copy classes and a 40-byte body at **278** unfolded copies, and on this pair returns exactly **1**; an instrument that can report 278 and reports 1 is measuring. **Gate:** every pre-registered key hit exactly — `43958 → 43959`, `4125560 → 4126216`, `40.260880 → 40.267284`, CROSSED IN **1** row / 656 B, FELL OUT **0**, unit `TrainerGemTab` **10 → 11 of 22**, fuzzy `+0.0`, native `PASS 18/18 skipped=0`, provenance unchanged. The post-gate assertion re-read the row at `fuzzy 100.00000`, and the pre-merge assertion had proved it present at 99.96951 first — so the Δ is **this patch**, not an absent-vs-absent artifact. **Handoff, analysed and deliberately NOT attempted:** `?DrawTails@TrainerGemTab@@` (888 B, fuzzy 98.58108, **19** charged sites) — `matched_code` is all-or-nothing per row, so closing 1 of 19 buys **0 bytes** and a speculative edit would have added noise to the A/B; found an FMA-contraction difference (retail does **not** contract `fmuls`+`fadds`, we emit `fmadds`, our body 4 B short), `fmuls` operand-order flips at indices 176/178/180, and ~13 FPR-numbering shifts. **Self-reported process defect:** the lane printed `ALIASCHECK_RC=0` from a pipe-into-`tail` construct (`echo $?` after it), which reads **tail's** exit code — the real rc was 1; it caught and reported this itself. ⚠ **A defect in MY gate, not the lane's:** my first abort-gate C crashed (`'str' object has no attribute 'get'`) because `symbol_aliases.json` is `{"_comment": …, "groups": […]}` and I iterated the raw object, yielding dict *keys* — **the gate fired before the merge and main was untouched**, which is the guard working as designed. |
| **W16-ER** `ScoreSinger`'s 19 charges belonged to its **callee** (opus) | `7be13608` | **+1 / +472 B** | Dispatched on the row W16-EO identified but could not pay for. ★★★★ **The fix contains not one byte of the row being measured.** `?ScoreSinger@VocalPart@@…` went **90.881355 → 100.000000, diff_score 1076 → 0**, and every one of its 19 charges was *induced by its callee* `GetNoteRange`, which we had implemented as the wrong algorithm (a `std::upper_bound` binary search where retail does a backward linear scan from a cached index). ★★★★ **MECHANISM — MSVC X360 `/O1` PERFORMS INTRA-TU CALLEE-CLOBBER ANALYSIS.** Retail calls `GetBestHit` **without reloading `this` or `ms`**, which is sound only with interprocedural knowledge; dumping the callee (keyed on the `.fn` symbol, never the synthetic address column) shows `fn_826F1EC8` is a leaf that reads `r3`/`f1` and **writes neither**. Our heavier body forced BOTH an 8-byte reload surplus and different stack packing, so the caller's codegen is a function of what the callee clobbers. ★★★★ **REUSABLE PROBE, and the lane's most transferable output:** a `__declspec(noinline)` stub that provably touches neither `r3` nor `f1` separates *"our callee body is heavy"* from *"our row is wrong"* in a **single build** — baseline 90.881355 / 1076 / 19 charges / base_size 480, stubbed 94.805084 / 613 / 9 / **472** with stack slots already correct, faithful body **100.000000 / 0 / 0**. The stub's residual 9 were an identical instruction multiset in a different order, a scheduling artifact of the stub not reading `f1` where retail's body does. ★★★ **`REGISTER_SWAP` AND OFFSET-SHIFT LABELS ARE SYMPTOMS, NOT DIAGNOSES — DEMONSTRATED AGAIN:** the tool's `-4/+16` offset shift and four `r11↔r29` / `r11↔r9` swaps **dissolved untouched** once the callee was fixed; nobody edited a register. ⚠ **Two lore-sanctioned levers measured INERT here, each with the recompile confirmed in the build log so neither is absent-vs-absent:** local **declaration** order Δ0 byte-identical, and function **definition** order Δ0 byte-identical ⇒ `MSVC_X360_REGALLOC.md`'s "declaration order controls stack slots" does not reach this case, because the layout is decided by *callee-induced register pressure* and the callee's body is the only lever. ⛔⛔ **A LOAD-BEARING INFERENCE OF THE LANE'S OWN, SELF-RETRACTED: retail `.text` order is essentially UNCORRELATED with our source order** — it justified the definition-order probe by observing all three callees precede `ScoreSinger` in retail (`0x826F1EC8` < `0x826F26A0` < `0x826F2E58` < `0x826F3658`) while all three follow it in our source, then refuted it by noting `SetDifficultyVariables` is our line 26 at `0x826F1770` while `PostLoad` is line 48 at `0x826F3990`. **The linker reorders COMDATs; callee addresses say nothing about source order** and nobody should re-derive that argument. ★★★★★ **MAIN'S LIVE `report.json` IS NOT A BASELINE — the lane's third self-reported defect, and the most valuable.** It diffed its worktree against main's live `report.json` and got a confident **−184 B with a `TrainerGemTab::Draw` regression, in a unit its patch cannot reach**, because main moved under it mid-read (`43958/4125560 → 43959/4126216` — that was me, landing W16-EN at that moment). The artifact is shaped **exactly like a real regression somewhere else**. Same disease as W16-EO's torn read, different cause; the fix is that both legs must be measured in your own worktree, which is precisely what `ab_measure` enforces and why there is deliberately **no `--baseline` flag**. A fourth self-correction: a believed `CouldScoreAgainstPart` 97.08 → 100 was read off the **probe-3 stub** build, so its true baseline is unknown and no claim is made. ⚠ **`unk58` is genuinely maintained, verified before the port was trusted** — reset at `VocalPart.cpp:76/105/137` and updated at `:521` as the branchless `max(beginNote,0)` idiom `unk58 = beginNote & ~(beginNote >> 31)`. ⛔ **NO PIN for `GetNoteRange`, deliberately:** dtk **over-carves** it into `fn_826F1EC8` (104 B, ending on a conditional `bgelr` with no return) running contiguously into `fn_826F1F30`, so `fn_826F1EC8` is a **PHANTOM EXTENT** and pinning it would pin a mis-carve — the blocker is carve quality, not identification, and the identification is recorded for whoever fixes the carve. ⛔ **NO name planted for `?Poll@VocalPart@@`** either: naming under `name_check` converts a forgiven placeholder site into a checked one, and the lane had no retail-byte identification to W16-EO's standard, so it made none. **Gate:** every pre-registered key exact — `43959 → 43960`, `4126216 → 4126688`, `40.267284 → 40.271890`, CROSSED IN **1** row / 472 B, FELL OUT **0**, unit `VocalPart` **30 → 31 of 70**, native `PASS 18/18 skipped=0`, provenance unchanged; the pre-merge assertion proved the row present at 472 B and **below** 100 first, and the post-gate assertion re-read it at `fuzzy 100.000000`. **Handoff — the callee-clobber sweep is a general, unswept lever:** any caller whose diff shows stack-slot shifts plus register swaps concentrated in ONE call's argument setup, with `base_size` a couple of instructions above `target_size`, is a candidate, and the fix is in the *callee*. Nearest target: `?GetBestHit@VocalPart@@` (528 B, fuzzy 96.9697), itself a `GetNoteRange` sibling. |
| **W16-EQ** **caller-set fan-in** separates a mislabel from a fold (opus) | `fe9c8197` | **+9 / +3,428 B** | Dispatched to adjudicate six sub-100 rows (5,100 B) in the vetted relocation-name band — **10 of 10 charges `diff_arg`, 0 instruction-level**, so W16-EJ's characterisation of this band holds exactly. ★★★★ **THE INSTRUMENT IS THE RESULT, and it is non-circular where both rivals failed.** Byte identity cannot separate *"our callee name is wrong"* from *"retail folded two callees and kept the other's name"* — **W16-EP refuted byte-identity as a fold test** (retail keeps eleven identical `erase` bodies unfolded) — and name shape is circular, because the name is precisely what is in question. EQ's discriminator is the **CALLER SET of the target address, censused off retail `.text`**: *few callers, all spelled as the same function in our source ⇒ **MISLABEL***; *many callers, spelled as many different callees ⇒ **ICF FOLD***. Measured, and not a close call: the three mislabels have fan-in **4, 3, 2**; the four folds **15, 44, 10, 453**. ★★★ **Five map corrections, each proved on retail bytes, with a non-circular anchor** — where the callee is a Milo `Message`, the **interned type string in retail `.rdata` names the class outright with no reference to the map at all**: `0x8235BF00` `__copy_ptrs<const int*,int*>` → `?GetQuest@Tour@@` (the body is no copy loop, it lives in `Tour.s`, and its 4 retail callers are exactly our 4 `GetQuest()` call sites); `0x823E1E20` ↔ `0x823E2020` **SWAPPED** (`RemoteUserUpdatedMsg` / `RemovingRemoteUserMsg` — each ctor calls the OTHER's `Type()`, and the strings `"removing_remote_user"` / `"remote_user_updated"` settle it); `0x823E1B40` `??0SpeechEnableMsg` → `??0AddUserResultMsg` (its `Type()` interns `"add_user_result"`); `0x826F1080` `?Draw@RndGroup@@` → `?GetFinger@FretHand@@` (it indexes a 12-byte array and writes through `r5`/`r6`/`r7` — a no-arg `Draw()` cannot do that). ★★★★ **AND THE WRONG NAME WAS MASKING A REAL SOURCE BUG — this is the payout the record predicts for naming work: BUG EXPOSURE, NOT BYTES.** Retail's `AddUserResultMsg` 1-arg ctor takes a **`bool`, not an `int`**: retail opens `clrlwi r11, r4, 24` (an 8-bit zero-extend a `bool` parameter produces and an `int` does not) where our `int` version emitted `li r11, 0`; after the fix our compiled body emits `548B063E` at `+0x18`, **byte-matching retail**. Call sites spell `false`/`true` because `DECLARE_MESSAGE` also generates a `DataArray*` overload that made the literal `0` ambiguous (C2668); codegen is identical either way (`li r4, 0/1`). ★★ **Only 1,980 B of the +3,428 came from the six briefed rows, and BOTH halves of the adjudication were confirmed:** the brief offered 5,100 B, adjudication said **1,980 B collectable and 3,120 B fold-bound**, and the two pure-fold `OnMsg` rows (1,236 B, 664 B) **did not move at all** while `InitFretSteps` (1,220 B) improved `99.96722 → 99.983604` **WITHOUT crossing** — `matched_code` is all-or-nothing per row and it still carries one real fold, so that is **a correct repair worth exactly zero bytes, landed anyway**. The other 1,448 B is cascade into rows outside the brief. ⛔ **Two things deliberately NOT done so the +3,428 stays attributable:** **no aliases** (four of the ten charges are genuine folds and stay charged — an alias is forgiveness and lifts `name_check` **by construction**, and the `none` control provably cannot catch a fabrication; `control_none` read **−136** against default **+3,428**, correctly labelled **NOT_APPLICABLE** because `source` is in the patch), and **no pins re-homed** — two renamed rows now read **0.0** because their retail addresses are pinned to units (`SpeechMgr`, `ViewSetting`) whose objs do not compile our definitions and **objdiff pairs by name WITHIN a unit**. ★ **THE IDENTIFICATION IS RIGHT AND THE PIN IS WRONG** — that is the re-homing lever, ~172 B, handed off rather than taken. **Gate:** landed under a gate that **forces the re-split and asserts the map change TOOK in BOTH directions** — the three new names absent pre-merge and present post-gate, the three old names the mirror image — because a map edit that does not re-split measures nothing (lane CF-1 lost a full leg to exactly that); `MAP_CHANGE_TOOK=1`, renamer **1,830 files patched**, `symbols.txt` already at a fixed point. Every pre-registered key exact: `43960 → 43969`, `4126688 → 4130116`, `40.271890 → 40.305344`, **CROSSED IN 9 rows in the predicted order at the predicted sizes**, FELL OUT **0**, native `PASS 18/18 skipped=0`. ★ **The Δfuzzy is NEGATIVE and was PRE-REGISTERED as expected, not discovered as a regression** (`−0.001156` vs predicted `−0.001155`): three partially-matching row keys are renamed (`99.85294 → 0`, `43.23529 → 81.70588`, `33.33333 → 0`), **none of them at 100**, so `matched_code` is untouched and FELL OUT is 0. **The lane corrected two of my briefed figures** — `diff_score` is **not** a `report.json` field (it comes from `objdiff-cli diff`'s `diff_score.score`), and `OnMsg@NetSession` is **eight** overloads with **four** already at 100, not seven with three; neither was material. **Handoff:** a fourth instance of the same shape, located not fixed — `0x823F2C98` mapped `_M_allocate_and_copy<const unsigned long long*>` has **3 callers all inside NetSession** (`??_ENetSession`, `UpdateSyncStore`, `RegisterOnline`) where a genuine STL instantiation would show broad heterogeneous fan-in; `RegisterOnline` (136 B) and `Disconnect` (136 B) each carry **exactly one charge** and it is this one ⇒ ~272 B. |
| **W16-ES** MSVC contracts `a*b+c` only when the product has **one use** (opus) | `590d0139` | **+1 / +0 B** | Dispatched at `?DrawTails@TrainerGemTab@@` (888 B, 19 charged sites) with the economics stated up front — **`matched_code` is ALL-OR-NOTHING per row, so closing 18 of 19 buys exactly zero bytes** — and the lane claimed **+1 `matched_function` and NOT the 888 B** before measuring. ★★★★ **NEW COMPILER KNOWLEDGE, established on the real `16.00.10224.00` at project cflags across TEN spelling variants rather than on the metric: MSVC X360 fuses `a*b+c` into `fmadds` IFF the product has exactly ONE consumer.** Five one-use spellings all contract; every two-use spelling emits `fmuls`+`fadds`. Retail's product *looks* single-use because it is dead right after the add — the resolution is **two uses at the fuse decision that a later CSE merges back into one add**, so the separate pair costs no instruction. The whole fix is one expression: `0.1f * ((xfm.v.z + scaleX10) - unk12c)` in place of `0.1f * (endZ - unk12c)`. **No pragma** (`#pragma fp_contract(off)` is inert on this toolchain, lane AE2) and **no `volatile`** (which would add a stack round-trip retail does not have). **Two corrections to existing docs, both recorded at the site:** a *"named home"* for a subexpression is **NOT** sufficient to break contraction (our own one-use local proved it), and `XBOX360_FLOATING_POINT_CODEGEN.md`'s `NgFur` lever works only because a **member** adds a store — usually the wrong shape. ★★★ **THREE LEVERS REFUTED, each built and read individually.** (1) **Flipping the commutative `fmuls` operand order is byte-identical INERT**, verified non-vacuous (the TU recompiled and all six patchers ran) ⇒ **objdiff's `COMMUTATIVE_OP_ORDER` → "LikelyFixable" is WRONG here**, corroborated by `Vec.h`'s `operator*=` and `Scale()` both being value-first — another instance of the standing rule that **a tool's confident label is the claim most worth auditing**. (2) ⛔⛔ **Hoisting `yRange`/`tickRange` into pre-loop locals — WHICH IS THE rb3-Wii ORACLE'S OWN SHAPE — is HARMFUL**: MSVC performs LICM into callee-saved FPRs and grows the save set (`__savegprlr_24`/`__savefpr_21` vs retail's `_22`/`_23`), **17 → 69 charged sites, fuzzy 99.30 → 86.15**. **The oracle's source shape is refuted by retail bytes**, our inline spelling independently confirmed, and a comment now sits at the site so it is not "cleaned up" later. (3) Declaring `tickRange` inside the guarded block is inert — retail evaluates the *denominator* first where we evaluate the numerator, but that order is **scheduler-chosen, not source-chosen**. ★ **Self-reported process defect worth keeping:** levers 2 and 3 *look like* one hypothesis and are not — putting locals **before** the loop does not test evaluation order, it volunteers them for pre-loop placement, and lever 3 was the experiment that should have run first. ★★★★ **THIS LANDING IS THE `mpn`/`fuzzy` SPLIT DOING EXACTLY WHAT IT IS DOCUMENTED TO DO — a change can move FUNCTIONS with Δbytes = 0.** `matched_functions` counts rows at `match_percent_normalized == 100`; `matched_code` sums rows at `fuzzy_match_percent == 100`. The row crosses the first and not the second: **fuzzy `98.58108 → 99.30180`, mpn `99.27928 → 100.00000`**, our compiled body `884 → 888 B` (the 4-byte deficit closed). **Gate:** `43969 → 43970`, `4130116 → 4130116` (**+0 B, predicted**), code% `40.305344` unchanged, fuzzy `50.010094 → 50.010155` **exact to the digit**, unit `TrainerGemTab` **11 → 12 of 22**, native `PASS 18/18 skipped=0`. ⚠ **`CROSSED IN: 0 rows / NET +0 B` IS THE PRE-REGISTERED RESULT, NOT AN INERT PATCH** — the pre-registration says so in advance precisely so nobody reverts a correct fix on reading a zero, and the gate asserts the real evidence separately (`MPN_CROSSED=1`, `FUZZY_BELOW_100=1`, and W16-EN's `?Draw@TrainerGemTab@@` held at `fuzzy 100.00000` so nothing fell out of the neighbouring file). **What it deliberately did NOT do:** propose or invoke the permuter (OFF by directive, though two residual clusters are exactly its class); land the two inert edits (that is noise, and its own draft comment for one of them asserted something it then measured false); touch `scale`'s pooled-constant spelling, W16-EN's `ExtraTail::operator=`, the alias override, or the unit's 10 other sub-100 rows (**all at fuzzy 0 — an identification problem, not a source problem**). ★ **And it declined to call the row unfixable**, which is the correct move under this repo's `AT_LIMIT` policy: the narrower true statement is that the **17 residual charges are all `diff_arg` register slots (203/222 instructions equal) and are unreachable BY SOURCE SPELLING on three measured levers** — the next lane should attack the scheduler question, and §3 of the doc exists so it does not re-run these dead ends. |
| **W16-EU** re-home a pin, and two more int/bool ctor mislabels (opus) | `10a1d997` | **+6 / +1,360 B** | Briefed to test the re-homing lever on `0x826F1080` and chase a fourth `DECLARE_MESSAGE` mislabel. It did neither as briefed, because **it tested my figures first and four of them were wrong** — and the refutations are worth more than the task list. ★★★ **`?Disconnect@NetSession@@QAAXXZ` DOES NOT EXIST anywhere in `report.json`**, so briefed task 2 was worth 136 B not 272 B; **chasing that discrepancy is what produced task 3, the largest result of the lane** (+1,088 B / +4 fns). ★★★ **`0x826F1080` is NOT re-homable by any pin** — `?GetFinger@FretHand@@` is UNDEF in all three referencing objs and **there is no `FretHand.cpp` in the tree**, so the correct action was to do nothing; the check that killed it (*does a defining object exist?*) is the one to run FIRST on any re-home. ⛔ **My pin range was FABRICATED**: I briefed `0x826F1080–0x826F1188`; the probe had printed the integers `(2188316568, 2188316840)` = **`0x826F0F98–0x826F10A8`**, and I invented hex from the target address instead of converting the integers on my own screen — the *conclusion* (that address is ViewSetting's, not VocalPart's) survived, the *range* did not. And W16-EQ's caller **names** for `0x823F2C98` are unsupported (they resolve to unmapped `fn_823E4808`/`fn_823E4B68`) though EQ's **conclusion** reproduced exactly. ★★★★ **THE PAYOUT CAME FROM THE CALLER, NOT THE ROW THAT WAS NAMED** — task 2 renamed `0x823F2C98` to `??0MakeQuazalSessionJob`, and that row landed in `default/StorePurchaser` at 60 B / **fuzzy 0** (mis-homed, unmatched), while the entire +136 B arrived as `?RegisterOnline@NetSession@@QAAXXZ` crossing to 100: the **wrong** template name had been charging RegisterOnline's call site, and correcting it stopped the charge. That is *un-pairing is 80.5% of a map edit's delta* and *a wrong name is financed by its callers*, caught live in one row. ★★★ **Task 1 is the first re-home in this wave, and it is NOT metric-neutral** (PINHOME-1): moving `0x823E1B40–0x823E1C20` SpeechMgr→NetSession finally gives W16-EQ's `??0AddUserResultMsg` a home whose base obj can define it — it sat at **fuzzy 0.00000** in SpeechMgr purely because objdiff pairs by NAME. ⚠ **`default/SpeechMgr` goes 2 matched → 0, and that is CORRECT**: its only two at-100 rows were the 40-byte EH funclets `fn_823E1BC8`/`fn_823E1BF0`, which sit inside the re-homed range and MOVE to NetSession at 100 on both sides (pre-registered before the merge so it could not be misread as breakage); the unit keeps its other `.text` blocks, so the emptied-unit hazard does not apply. Task 3 hit the **top** of its registered +596..+1088 range because two register-allocation diffs **DISSOLVED** once the ctor signature was fixed — REGISTER_SWAP is a symptom, again. Gate: every key exact (43970→43976, 4130116→4131476, CROSSED IN 8 rows/1,440 B in predicted order and sizes, FELL OUT 2 rows/80 B), `RULER_CHANGES_TOOK=1` across **two** ruler paths, `symbols.txt` FIXED_POINT on iteration 1, native PASS 18/18 skipped=0. |
| **W16-ET** the callee-clobber **signature** does not find the callee-clobber **lever** (opus) | `06185b80` | **+2 / +180 B** | Dispatched to generalise W16-ER's mechanism — a callee whose register clobber set our source gets wrong — into a whole-binary signature, and to apply it to `?GetBestHit@VocalPart@@`. ★★★★ **THE GENERALISATION FAILED, AND THE MEASURED FAILURE IS THE LANE'S PRINCIPAL VALUE.** 2,659 rows swept, 0 objdiff failures; against arms where the mechanism is **impossible by construction** (byte-exact callee / external callee / no call in the block), enrichment is **0.87× — the signature is slightly LESS common where the mechanism can actually operate**. Concentration is not rare either: 58.9% of every sub-100 row puts ≥95% of its charges in one call-setup block, and the **highest** signature rate (74.7%) belongs to rows with **no call in the block at all**. ★★★ **The anti-vacuity control is what makes that negative worth anything** — reconstructing ER's own numbers, `ScoreSinger` scores conc 0.947 / 19 charges / `SAME_TU_SUB100` and **IS flagged**, so the detector can see its founding instance and the flatness is a real negative rather than a broken rig. ★★★ **The structural reason was in ER's case all along: ER's charges clustered around the `GetBestHit` call while the defect was in `GetNoteRange`, a DIFFERENT callee — the signature names the wrong callee even when it flags the right row.** `GetBestHit` itself is refuted in one query: its charges are all argument setup for `?ScoreNote@VocalPart@@`, which is at **fuzzy 100.0000 — byte-exact**, and a byte-identical callee has a byte-identical clobber set by construction; what remains is 4 instructions of pure scheduling with `target_size == base_size == 528`, which also refutes clause 3 of the briefed signature **on its own flagship row**. What landed is four retail-byte fixes in `VocalPart.cpp` found by *reading diffs*, not by the signature: `.end()` for `data()+size()` ×2, a `Clamp(0,1,·)` whose float specialisation expands to retail's `fneg`/`fsel`/`fsub`/`fsel` instruction for instruction, and a local-copy-of-global plus reversed `std::min` argument order. ★★ **Scope control worth copying:** the `data()+size()` idiom appears **8×** but 6 sites sit in unpaired functions with no retail bytes either way, so only the **2 adjudicable** sites were touched. Pre-registered in `10e2ed76`/`4fb7ac8d` before any A/B: both HIGH rows hit (`FramePhraseMeterFrac` 136 B, `InTambourinePhrase` 44 B, both →100 exactly as called), both MEDIUM rows missed (`IsEmptyPhrase` 96.5517, `GetNoteSliceWeight` 94.9256) and contribute **0 bytes** — landed anyway on accuracy. Three inert probes kept as negative results: statement order of two independent local initialisations (bit-identical, 1 confirmed recompile — **distinct from ER's *declaration*-order result**), a separate `int count` local, and un-hoisting `2.0f` (MSVC re-hoists it). Gate: every key exact (43976→43978, 4131476→4131656), FELL OUT 0, native PASS 18/18 skipped=0. ⛔ **Do not rebuild this census** — the surviving signature-selected vein caps at **13 rows / 3,760 B / 0.0367% of `total_code`**, and `GetBestHit` is a **scheduling** wall, not a clobber candidate. |
| **W16-EV** the stale-offset-comment surface is **drained**, and there is no layout bug behind it (opus) | `079d2abf` | **+0 / +0 B** (correct) | Dispatched to audit the game-layer headers carrying `// 0xHEX` offset comments against the compiler, keeping **class 1** (comment wrong, layout right — metric-neutral) strictly apart from **class 2** (our layout disagrees with retail — a real bug). ★★★ **Class 1 is DRAINED (69 rows fixed); CLASS 2 IS EMPTY.** Every key measured **exactly zero, three times** — and Δ0 is the *predicted, correct* result for a comment-only change, pre-registered as such before the merge so it could not be read as an inert patch. ★★★★ **THE RECOMPILE COUNTS ARE WHAT MAKE THOSE ZEROS MEASUREMENTS RATHER THAN ABSENT-VS-ABSENT** — 226 and 535 leg-B recompiles in EV's own A/Bs, and **537 MSVC compile edges** on the landing gate, which asserts a nonzero edge count alongside the four zeros; either half alone is worthless. ★★★★ **COVERAGE IS INSTRUMENTED, NOT ASSUMED, and this is the lane's durable method:** `audit_header()` returns `[]` both when a header is **clean** and when it audited **nothing**, so a raw "0 disagreements" cannot distinguish the two — every (header, class) was therefore audited **twice**, once as-is and once against a copy with every comment perturbed to `0xdeadbe`, and the rows that flag on the perturbed copy *are* the rows actually examined. Coverage 3,012 of 3,325 remit rows (90.6%), 35 disagreeing (1.16%). Self-validation: `SaveLoadManager.h` returns AUDITED at **26/26 rows**, 0 disagreements — the coverage figure is what makes that a real clean rather than a vacuous one; discrimination proved by corrupting one line and getting exactly the expected single WRONG row, no collateral. ★★★ **Class 2's one candidate adjudicates to not-a-bug:** `MetaPerformer` has **two layouts in one program** (`/DRB3_NO_WII_META_MEMBERS` on exactly 1 TU; 86 others compile it 12 B larger, `Object` vbase at `0x38c` vs retail's `0x380`), but the extra members are **tail-resident**, so only vbase adjustors shift and no matched code does that — Δ0 with 104 recompiles, then **REVERTED** rather than churn a documented retail-adjudicated config on a Δ0. ⛔ **A TOOL DEFECT FOUND AND DELIBERATELY NOT FIXED: `--check-header`/`--fix-header` CAN AUDIT THE WRONG HEADER AND REPORT A CONFIDENT FALSE CLEAN** — `main()` takes `find_header(...)[0]` and shortest path wins, so `--fix-header MetaPerformer` printed *"all `// 0xHEX` comments agree"* against a near-dead DC3-era `src/meta_ham/` copy while the live `meta_band/` header had **16 wrong rows**; same family as the cross-class and shadowed-base bugs the tool already pins, in **cross-file** form. ⚠ **My briefed scope figure was wrong**: "416 headers" does not reproduce — **369** band3+network headers carry `// 0xHEX` (512 exist, 384 contain any `0x`); **421** is the count of files of *any* extension, the likely origin. ★★ **Deliberately left undone, with reasons:** `Scheduler.h`'s 5 disagreeing rows (0/26 fns, **0.0% matched** — our layout is validated by *nothing*, while the comments **and** the member names independently encode the same RE-derived retail offsets) ⇒ **NEW GUARD RULE: `--fix-header` is only safe where matches validate our layout; on a 0%-matched unit it is EVIDENCE DESTRUCTION**; 74 tree-wide rows (52 in units <45% matched, 22 refused *automatically* because the class has >1 layout across TUs); and **1,142 rows / 89 headers UNAUDITED, which is not the same as clean**. ⚠ The landing gate's own Δ0 assertion raised a **FALSE ALARM** — `matched_code` is a JSON **string** in `report.json` and an int in the snapshot, so `'4131656' == 4131656` is False; the documented JSON-strings trap, firing as a confident *failure* rather than its usual confident *empty result*. Corrected by `int()` coercion and independently corroborated by the set-diff path (CROSSED IN 0 / FELL OUT 0). Native PASS 18/18 skipped=0. |
| **W16-EX** two instruments that printed a confident clean over a denominator of one (opus) | `c12c0b4d` | **+0 / +0 B** (correct) | Dispatched to fix `find_header`'s wrong-header selection (W16-EV found it and deliberately left it) and to tidy three `target_symbol_map.json` defects. **Both instruments were guilty, neither of what the brief charged, and the map defects do not exist.** ★★★★ **THE SELECTION BUG'S CAUSE IS SHARPER THAN "PICKS THE WRONG HEADER": `resolve_tu` and the audit walk the SAME candidate list under DIFFERENT rules, and diverge exactly when the audit's pick has no compiled `.cpp`** — `resolve_tu` falls through to the live TU, the audit does not, so the tool measured the layout from `meta_band/` and audited the dead `meta_ham/` copy. The surviving tiebreak is `len(path)`: 27 characters beat 35. ★★★★★ **A SECOND, INDEPENDENT DEFECT SURFACED WHILE CONFIRMING THE FIRST, AND IT IS THE MORE DANGEROUS ONE — *"all comments agree"* WAS PRINTED WHENEVER THE BAD-ROW LIST CAME BACK EMPTY**, i.e. the same sentence for a genuine clean and for a run that compared nothing: the live header audits **32 rows compared / 0 unchecked**, the dead one **1 compared / 43 unchecked**. The false clean was resting on a denominator of **one of forty-four**, worded identically to a real 32/32 — the `audit_header()` vacuity W16-EV had instrumented around by hand, now fixed at the source. ★★★ **Blast radius measured: 110 of 2,910 class names are ambiguous under `find_header`'s own predicate and 12 are decided purely by path length** (`DateTime`, `Instarank`, `Key`, `MetaPerformer`, `Stream`, …) ⇒ **any past `--check-header` clean on those 110 is UNPROVEN, not clean**. ★★★ **The same defect was live in BOTH tree-wide sweeps**, which matters because they are the instruments a census would have been built on: `header_offset_audit.py` *rationalised* `hdrs[0]` in a comment as "the best hit", and the parallel sweep's index returned one header per class, making ambiguity structurally unrepresentable. Both already held the TU and simply did not consult it. ★★★ **The fix is a PURE `choose_audit_header`** (a header is eligible only if the TU actually *reaches* it; anything but exactly one survivor **REFUSES**, exit 4) plus `audit_header_counted`, which discloses the denominator. **Proven able to fail:** pins G and H are offline and self-sabotaging — each reconstructs the OLD rule and *requires it to be wrong* — and reverting the fix turns G red. ⛔ **THE BRIEF'S TWO "VERIFIED" MAP DEFECTS ARE ARTIFACTS OF FLATTENING `_`-PREFIXED METADATA INTO THE VALUE POPULATION, AND "FIXING" THE FIRST WOULD HAVE CORRUPTED A PROVENANCE RECORD**: `0x826101b8` is **never a value anywhere in the file** — as a *key* it maps to a legitimate `hash_map` dtor, and its "duplicates" are memberships in `_icf_arbitrary`/`_bijection_arbitrary`, which are **lists of addresses**; `?NodeCmp ×3` is 2 real rows plus the `_internal_linkage_allow` entry that *licenses* them. The count was never convention-dependent either — `tools/map_name_injectivity.py` already pins it **in ninja** and reports injective (29,435 applied rows / 29,434 names). So the lane documented the schema *in the map* and added a schema-filter selftest leg that self-sabotages by requiring the naive flatten to manufacture the phantom. 7/7 and 14/14. ⚠ **MY GATE'S WORK-COUNT ASSERTION WAS MIS-SPECIFIED AND ABORTED THE LANDING TWICE, BOTH TIMES ON ME.** First it ran the non-vacuity check against **main** pre-merge and demanded pins G and H — *asserting the treatment before applying it* (main is 5/5, the branch 7/7; **the pins ARE the patch**); fixed by running it in the branch worktree plus a post-merge D2 leg on main. Then it demanded a nonzero **MSVC compile-edge** count and got 0 — correct, because **this patch contains no compiled source at all**, so for a map/tools patch the work-count that evidences a Δ0 is the **re-split + renamer (1830 files patched) + a re-run `REPORT` edge**, not compiles. ⇒ **"a Δ0 is only a measurement next to a nonzero work count" is right, but WHICH counter is patch-class-dependent.** Independent corroboration that the ruler path really re-ran: `CHECK MAP NAME-INJECTIVITY` executed as ninja edge 4/11 and passed — EX's own modified tool gating the build. Gate otherwise exact: applied address→name population **identical** (29,438 rows both sides), key delta exactly `{_schema_comment}`, CROSSED IN 0 / FELL OUT 0, all four keys Δ0, native PASS 18/18 skipped=0. ★ **Named follow-up deliberately NOT done:** re-run the tree-wide header sweep under the fixed selection — now possible, but a multi-hour campaign whose product is a census, not a fix. ⚠ **A hazard that fired on the lane itself:** `ab_measure`'s tree-restore **deleted the lane's own doc**, an untracked file written into the worktree mid-run; the warning existed and was read as a rule about scratch files rather than deliverables. |
| **W16-EZ** one ICF membership, proven on retail bytes, pays 1,004 B across three units (opus) | `6770e534` | **+3 / +1,004 B** | Dispatched at four sub-100 rows in `default/VocalPart`. **It landed from a row that was not on its list, and the briefed flagship is now a documented refutation.** ★★★★★ **THE WHOLE RESULT IS ONE MISSING ICF ALIAS MEMBERSHIP, AND THE METRIC IS STRUCTURALLY INCAPABLE OF ADJUDICATING IT.** `?CalcNoteWeights@VocalPart@@` (292 B) had exactly one charge — a `diff_arg` where retail names `vector<Dep*>::reserve` and we name `vector<float>::reserve`. An alias lifts `name_check` **by construction** and a `none` control reads flat for a *fabricated* alias too, so the only admissible evidence is retail bytes. ★★★★★ **BODY IDENTITY CANNOT SETTLE IT EITHER, BECAUSE RETAIL KEEPS TWO 188-BYTE TWINS OF THIS BODY** (`0x823715e0` and `fn_827A36F0`) — so masked-body uniqueness has nothing to say and FOLDPROVE-2 channel 1 does not apply. **What picks the membership is the call-site displacement**, decoded out of `orig/45410914/band.exe` through the PE section table: `0x826f3878` = word `4bc7dd69` = `bl 0x823715e0`, the survivor. Chased T1 PROVEN, and the prover's **in-family DECOY arm is REFUTED** under `--chase`, so it can fail. ★★★★ **THE PREDICTION MISSED IN THE LANE'S FAVOUR — +1,004 B / +3 fns against a registered +292 B / +1 fn — AND THAT IS AN INTEGRITY ALARM, NOT A BONUS.** Both unplanned crossings were adjudicated on retail bytes *before* landing: `WeightedCrowdLevel@BandPerformer` idx 20 and `TrackData::Init` idx 22 both decode to the survivor, and all three spell `vector<float>` in our own declarations. ★★★★★ **THE CHECK THAT SETTLES IT IS THE ROW THAT DIDN'T PAY: the only caller in the set reaching the OTHER twin is `??0DataArraySongInfo`, whose six `reserve` sites split 4→survivor / 2→twin — and that is exactly the row which improved (99.96795 → 99.98397) and BANKED 0 BYTES.** The row that would have been the false positive is the one the ruler declined to pay, which is the strongest evidence available that the membership forgives *folding* and not *wrongness*. ⚠ **I re-derived all of this myself at merge time rather than trusting the lane** — three crossing sites and all six `DataArraySongInfo` sites re-decoded from retail bytes in Python (grep is binary-blind), 4/2 split reproduced exactly — and the gate made the **merge conditional on that decode**, not on the metric. ⛔ **TWO NEGATIVES THAT CLOSE VEINS.** `SetDifficultyVariables` (768 B) is **not source-fixable and the naive fix REGRESSES it**: 8 charged sites from one construct, four preceding instructions byte-identical at every site, and **both sides emit both operand orders**, so a source flip fixes 2 and breaks 6 — **net −4**, pre-registered and honoured rather than attempted. `HandlePhraseEnd` (1120 B): lane AG2's `mullw` canonicalisation claim from 2026-07-26 **REPLICATES** — re-tested because the in-source note predates both the `name_check` flip and the `.end()` change to that body, and the flip left idx 132 bit-identical with the TU confirmed recompiled, so the negative is **non-vacuous**. It also refuted the lane's *own* model (a "lower register first" rule fits all four `mullw`s here but fails on `SetDifficultyVariables`) ⇒ **permuter-class, and the permuter is OFF**. ★★ **`IsEmptyPhrase` (116 B) NARROWED, not closed:** the missing instruction is **not** a self-move but `rlwinm r10,r10,0,0,31`, the **zero-extend-32-to-64 idiom** — which explains why W16-ET's plain `int` local was inert and why an explicit `(unsigned int)` cast was inert too (MSVC elides it; `lwz` is already clean). **The next lane should ask what makes the value unprovable, not try another cast spelling.** ⚠ **THREE CORRECTIONS TO MY BRIEF, ALL MINE.** I **missed a sub-100 row entirely** — `CalcNoteWeights`, 292 B — because I truncated my own listing at 8 rows; there are **six** sub-100 rows, not five, **and the one I dropped is the only one that moved**. I claimed nobody had opened `HandlePhraseEnd`; an in-source NOTE at the charged line records lane AG2 trying both operand orders in `0ac748fc` — **the refutation was inside the file the lever was about**, the in-tree-record rule paying out again. And "70 named rows, 31 at fuzzy 100" mislabels the denominator: 70 is *total* rows (36 named + 34 anonymous) and named-at-100 is 30 — integers right, noun wrong. ★ **Deliberately not done, stated because silence reads as coverage:** `GetNoteSliceWeight` (484 B) is **unattempted, not refuted** — the real gap in the lane's coverage; `GetBestHit` was not opened because ET's wall is around a byte-exact callee whose clobber set is identical by construction. Gate: every key exact (43978→43981, 4131656→4132660), CROSSED IN exactly the 3 adjudicated rows at 488/292/224 B, FELL OUT 0, `code%` **40.330170 — the float32 value predicted in advance**, renamer 1830 files, 1 MSVC edge (the comment-only TU), native PASS 18/18 skipped=0. ★ **HANDOFF: this alias vein is NOT exhausted and is the highest-yield thing in reach** — one membership bought 1,004 B across three units — with the caveat that **the displacement must be decoded PER CALLER**, because a group with two surviving twins cannot be settled any other way. |
| **W16-EW** the briefed pin move was impossible; the disease was a **missing translation unit** (opus) | `1d798aeb` | **+10 / +1,800 B** | Briefed to re-home `0x823F2C98` out of `StorePurchaser.cpp` (~60 B). The lane's kill-check found the symbol **UNDEF in the only object referencing it** — no pin move can bind an undefined symbol, so the briefed lever could not pay **at any size**; the coordinator brief was refuted outright. Porting `src/network/net/QuazalSession.cpp` was worth +92 B where the pin move was worth 0. ★★★★ **A NAMING A/B SYSTEMATICALLY UNDERSTATES ITS OWN VALUE:** task 3 measured **+0/+0** and was the lane's most valuable commit — it enabled **+6 fns / +1,164 B** in the three commits that followed. objdiff pairs **by name**, so an unnamed row is UNPAIRED and **indistinguishable from a row with nothing wrong**; naming `0x823E4B68` took `OnMsg` (756 B) from an invisible fuzzy 0 to a visible 79.01, exposing **two real source defects** (`fuzzy 0 → 79.01 named → 97.96 loop removed → 100.0 call removed`). ★★★★ **THE ORACLE WAS WRONG TWICE IN ONE FUNCTION:** our `OnMsg` was **verbatim identical** to rb3-Wii and retail still lacks both the `HasOnlinePrivilege` loop and the `JoinVoiceChannel` call (Wii **dev** build; XBL manages both differently) — **a file matching the oracle perfectly is not thereby correct**. Both removals were corroborated on retail bytes *before* the edit (the loop by a 16-byte frame excess, `0xe0` vs `0xd0`). ★★★★ **A 17-SITE REGISTER SWAP WAS A SYMPTOM:** after the loop fix `OnMsg` showed 3 inserts + 17 sites of pure `r26`↔`r27` exchange — the exact shape that invites `AT_LIMIT` on a 756 B row — and deleting three instructions cleared **all twenty** (12th recorded instance). **CLOSED: the int/bool `DECLARE_MESSAGE` family** — `SaveLoadMgrStatusUpdateMsg(int)` proven correct as `int` (`clrlwi` 0× in 132 B, `stw r4,0x58(r31)` full width, already 100/100 with no `masked_equal`). Gate: every pre-registered key **exact** — 43981→**43991**, 4132660→**4134460 B**, `total_code` **unchanged**, `masked_equal` +4, **honest +6** (the load-bearing prediction — the lane disclosed that task 3c's +4/+200 was *entirely* funclet pairing, so only +6 of the +10 are honest bodies), `code%` **40.347736 = the float32 value predicted in advance**, `NO_DOUBLE_COUNT=1` with MeshAnim shedding 2 rows and StorePurchaser 1 rather than appearing under both units, native PASS 18/18 skipped=0. ⚠ **Gate lesson, mine:** GATE C aborted (exit 12) because I reconstructed a mangled ctor name from a **50-char-truncated print** and invented a `BandUserMgr*` parameter that does not exist — the *same* truncation hazard booked against me in W16-EZ. The abort is the win: an **exact** assertion failed on a fabrication that a loose one ("some 60 B row in StorePurchaser") would have passed. ⚠ Second gate lesson: byte-exact-after-rebase on `target_symbol_map.json` **must fail by construction** here (W16-EX added `_schema_comment` to main), so that file needs a *semantic* invariant — main's map **plus exactly** the 7 predicted keys, 0 removed, 0 existing values changed. **Newly created work surface:** naming made `AssignLocalOwner` (180 B, f=37.38) and `IsFinished` (128 B, f=65.66) visible and sub-100 — 308 B that was structurally untargetable at fuzzy 0. |

Ledger: 42,846 / 3,892,688 B / 37.992 % (`bbcf979b`, wave start) →
**43,915 / 4,109,528 B / 40.1044 %** (ledger row `84577bdc`, objdiff **4.2.9** `5a51cd51`; W15-D, W15-E, W15-F, W16-A, W16-B, W16-C, W16-D, W16-E, W16-F, W16-G, W16-H, W16-I, W16-L, W16-K, W16-N, W16-M, W16-O, W16-P, W16-Q, W16-J, W16-S, W16-R, W16-T, W16-U and W16-V each additive to the byte on the 4.2.8 ruler, whose last figure is **43,290 / 3,962,580 B / 38.6746 %** at ledger row `93283bb4`; the +1 / +24,392 B between the two rows is **W16-W's RULER CHANGE**, not additive source work — same code, W16-X, W16-Y, W16-Z, W16-AA additive on the 4.2.9 ruler; W16-AB moved 7 named rows via map/splits/alias repairs, 0 out; W16-AD un-swapped 4 Accomplishment map rows — 7 in / 1 out, net +680 B — and withdrew 20 refuted alias memberships at Δ0; W16-AC wired rnddx9/Utl.cpp and crossed its four rows, +4 / +356 B, 0 out; W16-AG withdrew the 51 refuted alias memberships W16-AD left, Δ0 predicted and measured; W16-AF identified `0x825f32a8` as `GoalCmp::operator()` and rotated the comparator half of 56 map names across two STL bands, 35 in / 3 re-home fall-outs, net +32 / +6,016 B; W16-AI took the GoalCmp local static AF sized — the rb3-Wii oracle is wrong there, retail right — plus two vtordisp thunks and a misplaced TU boundary, +5 / +424 B predicted exactly; W16-AH withdrew 294 no-witness alias memberships by type existence at Δ0, refuting its own Route B map row on retail bytes; W16-AJ identified `0x826100F8` as the survivor of 26 hashtable dtors and completed CampaignSongInfoPanel 50/50 for +17 / +2,812 B; W16-AE refuted all four of W16-AB's "cannot be fixed" items — ported SessionSearcher/NetLog over seven circular pins, re-homed six mis-unit blocks, fixed two alias-tooling gaps — for +35 / +3,572 B; W16-AK refuted its own brief's ICF-fold premise on OvershellPanel — the two OnMsg charges were a Wii-dev-only block, 76 vs 132 B, removed on merit — for +3 / +684 B; W16-AM found the `list<T>` bijection is decidable by relocation TARGET, not arbitrary — 8 rows repaired, `fn_824CE130` identified, 0x823d3918 nulled as a folded `Handle` — for +5 / +980 B; W16-AN closed MainHubPanel 176/176 and CampaignSongInfoPanel 53/53 — retail materialises function-local statics where the Wii source uses externs, and re-homing the PhysicsManager mis-pin exposed a sret-vs-stack-slot divergence that was structurally invisible — for +12 / +2,736 B; W16-AL installed 12 of 20 alias proposals on retail bytes, refused 2 and held 14 — naming the `0x826100f8` hashtable-dtor survivor cost −176 B by design and exposed two real container-type dtor divergences, and AK7's "atexit dtor" was `BandUserMgr::GetBandUser` with six callers — for +14 / +14,820 B; W16-AO repaired 9 wrong list<T> resize rows plus one coupled transposition on retail bytes, leaving 0x8246af50 alone as a true fold, for +8 / +944 B; W16-AP closed GamePanel 91/97 → 93/97 by naming four anonymous rows, dropping RestartGameMsg's phantom payload and reproducing the function-local-static pattern, for +5 / +584 B; W16-AS +1 / +468 B; W16-AR +7 / +1,060 B; W16-AQ +7 / +2,248 B; W16-AU +2 / +216 B; W16-AT +1 / +628 B; W16-AV +12 / +3,500 B; W16-AW +1 / +328 B; W16-AX +2 / +296 B; W16-AY +1 / +144 B; W16-AZ +7 / +884 B; W16-BB +1 / +264 B; W16-BA +5 / +1,012 B; W16-BC +4 / +1,000 B; W16-BF +6 / +816 B; W16-BE +9 / +792 B; W16-BD +4 / +916 B; W16-BG +23 / +1,024 B; W16-BI +2 / +392 B; W16-BH +2 / +160 B; W16-BJ +2 / +32 B; W16-BK +3 / +180 B; W16-BL +1 / +12 B; W16-BM -35 / -1,096 B; W16-BN +10 / +1,244 B; W16-BO +10 / +1,008 B; W16-BP +7 / +220 B; W16-BQ +55 / +924 B; W16-BR +21 / +3,056 B; W16-BS +25 / +9,004 B; W16-BV +0 / +0 B; W16-BT +3 / +560 B; W16-BU +24 / +11,444 B; W16-BX +1 / +528 B; W16-BY +2 / +868 B; W16-BW +1 / +312 B; W16-BZ +92 / +5,320 B; W16-CA +22 / +840 B; W16-CB +4 / +7,280 B; W16-CC +0 / +0 B; W16-CD +16 / +2,732 B; W16-CE +14 / +2,684 B; W16-CF +1 / +40 B; W16-CH +1 / +6,416 B; W16-CI +5 / +892 B; W16-CG Δ0; W16-CJ +78 / +14,648 B on the empty-survivor fold vein). `total_code` 10,247,068
throughout — none of it is denominator movement.

### Three corrections to the coordinator's own briefs

1. **"The real-named-at-0 stratum is unpaired" — wrong.** 65 of 351 rows
   (15 %) are PAIRED and body-divergent. The stratum is four classes, not
   one, and the fix differs per class (W15-B's table).
2. **"A plurality vote over callee-set membership is a pin oracle" — wrong.**
   Measured enrichment 1.38× / 1.51× against its own null; the census is
   only useful as a *candidate generator* whose hits are adjudicated on
   internal linkage. The anon-hash premise (that `?A0x<hash>` in the map
   fingerprints retail's TU) is also false — it fingerprints the ORACLE's TU.
3. **"Five game units carry the gap" — misdescribed the work.** Across
   RockCentral, VocalTrack, GemManager, NextSongPanel and OvershellSlot there
   are only **75 named sub-100 rows / 65,740 B**; the units' headline gaps
   are placeholder-at-0 rows that naming cannot cross.

And one from W15-F's pre-registration: it predicted *no gain* from the
MoggClipMap move and measured +644 B, because it assumed a first-try dc3
body-port would not match. It did, five times out of seven. **Raise the prior
on engine body-ports from dc3** — the CLAUDE.md thesis "same Milo engine"
pays out literally, not approximately.

### The class this wave surfaced: source we hold that the match build cannot see

Two independent lanes hit it. The match build compiles but never links, so a
definition inside `#ifdef HX_NATIVE` (W15-B) or a TU absent from
`objects.json` (W15-F) is invisible to it — the retail row sits at fuzzy 0
and every caller carries a wrong-callee charge. Measured size of the second
half: of 1,211 `.cpp` under `src/`, **17 non-XDK files** are neither declared
nor `#include`d by any compiled TU (`ninja -t deps`); most are soundtouch
CLI/DLL code retail never shipped, two are plausible retail TUs
(`world/BeatClock.cpp` 6.4 kB, `synth/AudioDucker.cpp` 2.9 kB). The
`HX_NATIVE`-swallowed half is unsized and is wave 16's first lane.

A third face of the same class surfaced in W15-E: **symbols DECLARED in a
header and DEFINED NOWHERE** (`TickToMs`, and still `TickToSeconds`,
`BeatToTick`; W15-C's `PlayableBy__9VocalNoteCFi` was the same). Every
caller compiles against an undefined external, and the match build never
links, so nothing complains — the row sits unpaired and the callers carry a
name charge each. `TickToMs` alone was worth +9 fns / +4,000 B once
defined. A whole-tree sweep for declared-but-undefined externals
(compile-side symbol table vs. any definition in a compiled TU) is a lever
nobody has run; queued below.

### Wave 16 dispatch

| lane | model | task | expectation |
|---|---|---|---|
| **W16-A** swallowed definitions (**landed** `c8dbf582`, see table above) | opus | census every DEFINITION under `src/{system,band3,network}` whose enclosing preprocessor state includes `HX_NATIVE`; classify RETAIL_HAS_IT / NATIVE_ONLY / UNKNOWN on retail evidence; un-gate and port the first class per-site (never blanket); plus wire-and-pin `BeatClock.cpp` / `AudioDucker.cpp` if retail has them. Instrument: `tools/hx_native_swallowed_census.py` with a `--selftest` that can fail | +bytes where a correct body was merely invisible; a NATIVE_ONLY list so nobody re-hunts |
| **W16-C** escalation (**landed** `a427ff73`, see table above) | fable | `CountOrCreateExpandedDetails@NextSongPanel` — 12,220 B behind one commuted `add` (`add r3,r11,r28` vs `add r3,r28,r11`); W15-C's "ARITH_COMMUTE proved inert" tested ONE lever (a textual `a+b`→`b+a` swap), not whether operand order encodes evaluation shape (what retail materialises into the scratch r11 last); brief `~/tmp/brief_w16c.md` | either the row crosses (+12,220 B / +0 fns, mpn already 100) or the reason it cannot is written on retail bytes |
| **W16-E** (**landed** `62943770`, see table above) | opus | PlatformMgr follow-ups from W15-F (brief `~/tmp/brief_w16e.md`): `GetName` 88.9 (3-arg `Localize` per dc3 vs RB3's verified 2-arg form, `Locale.h`), `ShowGamercard` 35.8, the `0x8251CED0` DingoSvr / DingoSvr_Xbox pin, and the seven COMDAT-hosting blocks under the "which of OUR objects emits it" rule | small positive; one pin repair |
| **W16-B** escalation (**landed** `76a32373`, see table above) | fable | `Handle@RockCentral` 1,296 B: W15-E called the 28 residual charges scheduling noise, but they are 6/6 correlated with our `OnMsg` overloads being STUBS and the in-source note claiming the handlers are outside the pinned range is false (`fn_824F7C98` / `fn_824F7D48` are in `RockCentral.s`, folded for UserLogin + FriendsListChanged); port the bodies, fix the grouping, re-price; also define `TickToSeconds` / `BeatToTick`, and `AttemptRemoveUser@OvershellSlot` (same Wii-contamination class) | the label dissolves or the residue is characterised on retail bytes |
| **W16-D** (**landed** `83488617`, see table above) | opus | whole-tree sweep for declared-but-undefined externals PLUS the remaining unwired non-XDK TUs (brief `~/tmp/brief_w16d.md`; W16-A's residual list: `DoCompress@DxTex` 284 B, `SyncEffectParams@FxSendReverb360` 3,280 B with no in-tree TU, `TexelsPitch@DxTex` at 99.750) under `src/{system,band3,network}` (see above); define from retail bytes where a row exists | +bytes per definition, caller cascade |
| **W16-F** (**landed** `3b0a5277`, see table above) | opus | W15-D handoffs (`THUNK_PERMUTATION_2026-09-14.md` §5; brief `~/tmp/brief_w16f.md`): the 23 bodies proven misnamed by vtable geometry whose PIN must move — the five "kept (at 100; re-pin first)" rows first (`0x8232ac50`, `0x82340670`, `0x824e9da8`, `0x825748e8`, `0x8252a598`), the four DC3-onto-RB3 unit collisions (`HamLabel`→`BandLabel`, `PostProcer`→`SpotlightEnder`, `HamIKEffector`→`SetlistToStorePanel`, `CharUpperTwist`→`DialogDisplay`) as whole runs where evidence is uniform, the 13 not-acted thunk rows (seven name-holder chains, six "address not in any unit" incl. the `UILabelDir` `$4` block `0x82812018..68` and `Handle@GamePanel$4` `0x82697528`), and W15-B `probe.py`'s `.pdata` `>>2`→`>>8` decode | re-homing is NOT neutral: each move pre-registered and priced; false 100s falling to honest numbers are WINS |
| **W16-G** (**landed** `1fbefb24`, see table above) | fable | vtable/struct (`~/tmp/brief_w16g.md`): the six slot-count divergences (`TourSavable`/`PracticePanel` 20 vs 21, `Server` 20 vs 19, `PlatformMgr::Callback` 14 vs 3, `DxTexRenderer` 14 vs 9 / 14 vs 4) aligned slot-by-slot against retail RTTI vtables and fixed only where retail bytes prove the shape (rb3-Wii cross-check for the RB3-era declaration); the two r3/r4 thunk-SHAPE mismatches at 50 % (`SynthEmitter::Handle`, `UILabelDir::PreLoad` — hidden-struct-return signature divergence); the `DrawRect@DxRnd` r24/r28 residual | header cascades priced on full builds with AT_100 set-diff; rows falling off false 100 recorded as wins |
| **W16-H** (**landed** `adee9d93`, see table above) | opus | ContextChecker `0x8275B868` sliver — a 72 B body that byte-matches at a name its only caller (`DirLoader::LoadHeader`) cannot reach, i.e. an ICF survivor financed by its caller (adjudicate: rename+re-home / port body first / T1 alias); and W14-B's six `UNIMPLEMENTED_BODY` rows (body first, then name) | bug exposure; Δ≈0 expected |
| **W16-I** (**landed** `58735f7a`, see table above) | opus | W16-B's flagged residue: `DrawBeatLine@GemTrack` calls `GetLoopTick` where retail's site names `SpeechMgr::OnIsSpeechSupportable` (wrong map name vs fold vs source); `RemoveAllInstances@Gem` `_Rb_tree::clear` CharLipSync-vs-TrackWidget residue (T1 gate: alias or a container-type bug); `0x827C91A0` carries the refuted name `??__ETheLocale` + its GamePanel caller `0x82695178` unidentified; `Handle@OvershellSlot` priced from `report.json`'s charged-site list | adjudication; small or negative Δ acceptable |
| **W16-J** (**landed** `ada7a244`, see table above) | fable | ESCALATION of W16-D's blocked list with the Opus report attached: is `?MainThread@@YA_NXZ` an out-of-line body in retail (then our `inline` is WRONG for RB3 and every inlining caller is mis-scored, not just 192 B); split the 19-block grab-bag heading `0x822E4F70–0x8276D808` and port `JoypadPollCommon` 2,468 B + DataResultList 388 B; decompile the no-oracle bodies straight from retail asm (`FriendsProvider` 140 B as the cheap probe, `FxSendReverb360` 3,280 B, FFT quartet 6,432 B VMX128); size the 126 unpairable rows / 26,376 B by contiguous-run pins | +bytes and/or sharper reasons |
| **W16-K** (**landed** `faf3bf75`, see table above) | opus | W16-H's leftovers: write the two decoded Save bodies (`BandList::Save` 388 B @ `0x8233C4E0`, `EventAnim::Save` 172 B @ `0x824C9540`, priced +2 / +560 B), the `0x822AF178` re-pin, and the tree-wide `$4`-thunk FALSE-100 census as a tool (`tools/thunk_false100_census.py`) — every `$4` thunk at 100 whose head is a proven-wrong name is a false 100 of the `BandSwatch::Save` kind | +560 B priced; census = accuracy (rows falling off false 100 are wins) |
| **W16-L** (**landed** `bf9eeec7`, see table above) | opus | PlatformMgr cluster after W16-G fixed the SHAPE: `StoreInfoPanel::GetRecommendationIndexPath` calls `GetMasterProfileID()` where retail calls `GetPlayerID(padnum)` (real wrong-callee bug, W16-G side-finding, lead item); `Server`'s surplus virtuals (21 vs 19 still unsettled); the cluster's sub-100 rows priced from `report.json`'s charged-site lists | bug exposure first; bytes second |
| **W16-M** (**landed** `e004c07b`, see table above) | fable | ESCALATION of W16-L's two Opus stop verdicts with the report attached: (1) `?Poll@PlatformMgr@@` 1,844 B @ 99.245 called "uncollectable" on two UNPROVEN reloc-name folds (`??2Friend@@` vs `??2CriticalSection@@` class-scoped `operator new`; `push_back<Friend*>` vs `<ChatReceiver*>`) plus a 6-charge induction-base construct — prove or refute each fold on retail bytes (T1 standard; a non-fold means retail's `push_back<Friend*>` exists unnamed at the `bl` displacement and naming it PAYS), then ≤6 source variants for the hoisted `mFriendsBuffer+8` base; (2) Server surplus tail virtuals called "NOT PROVABLE" — identify `fn_82A89FF8`/`fn_82A89F38` in the Quazal `/Od` band via `XboxServer`'s `+0x7c` pointee type, which names `XboxServer`'s single tail override and hence which two of the six candidates retail lacks; remove only if proven (native gate), else record the narrowing. Secondary: `??0PlatformMgr` 364 B @68.8, `UpdateSigninState` 340 B @76.0, `??0ProfileSwappedMsg` 164 B @0, `fn_8251D378` = `PlatformMgr::Init` 220 B @0. Deliverable `docs/decomp/PLATFORMMGR_ESCALATION_2026-09-14.md` with a per-claim SURVIVES/REFUTED table. | +1 / +1,844 B if all three Poll claims close, else 0; Server removal is layout correctness at Δ0 by construction; secondary rows size-if-it-crosses |
| **W16-N** (**landed** `f73ccadb`, see table above) | opus | W16-K §8/§9 leftovers, unblocked now that W16-L has landed: (1) LEAD — the 9 CHARGED thunk rows whose named destination disagrees with the row's method (`PreLoad@UIComponent`→Highlight, `ClassName@ReviewDisplay`→a `ForceEmit_*`, `IsLocal@LocalUser`→`IsDirPtr@ObjDirPtr`, `GetRemoteUser@RemoteUser`→`GetLocalUser`, `CanSaveData@NullLocalBandUser`→`GetCrowdMeter`, `UserName@NullLocalBandUser`→`ContentPattern`, `SyncProperty@BandUser`→`GetCrowdMeter`, `Load@RndCam`→`Replace@RndParticleSys`, `SetTypeDef@UIPanel`→`Highlight@RndDir`): resolve each retail branch target by vtable-block bijection, decide whether the THUNK row or the DESTINATION row carries the wrong name, fix only what a base obj can define, price by fuzzy==100 row-set diff; (2) `BandList::Save` 388 B + `EventAnim::Save` 172 B — port the oracle's `operator<<(BinStream&, const ObjList<T>&)` to its non-PCH home if the oracle's placement allows, unnamed body first (Δ0 proof) then name+carve; (3) LAST, name `0x82403728` = `RndDir::Export` and `0x823F94E8` = `RndTransformable::Replace` (20-/24-fold), every falling row adjudicated or the naming reverted. Deliverable `docs/decomp/THUNK_CHARGED_ROWS_AND_SAVE_BODIES_2026-09-14.md`. | 9 thunk rows size-if-it-crosses (12 B each, +108 B ceiling; a −N that exposes a wrong destination name is a win); +2 / +560 B for the Save bodies plus crossing callers; the two namings Δ≤0 by construction (bug exposure, not bytes) |
| **W16-O** (**landed** `4193d816`, see table above) | opus | W16-N's four NOT-done items: (1) LEAD — the `fn_824C93C0` fold alias, 368 B behind a mechanical T1 byte-identity check of the `list<HamCamShot::Target>` half against our `list<EventAnim::KeyFrame>`/EventCall writers (reloc target names compared, anti-vacuity guards, element serializer at `+0x50` must be `fn_824C8F68`); install only if T1 holds, else record the differing word and stop; (2) the `ClassName@ReviewDisplay` `ForceEmit_*` metric-fitting row — pin move into `ReviewDisplay.cpp`, map row set to what the obj defines, expected −60 B recorded as an accuracy WIN; (3) `RndTransformable::Replace` @ 16.9% — fix the `dynamic_cast`/`??_R0` and `beq`/`bne` divergence at source, ≤6 variants, native gate; (4) re-home `0x82403728` = `Export@RndDir` from `Anim.cpp` into `Dir.cpp` (re-homing is NOT neutral — pairability changes). Deliverable `docs/decomp/W16O_ALIAS_REVIEWDISPLAY_REPLACE_2026-09-14.md`. | +3 / +368 B if the alias holds at T1, else 0 with the refuting word named; −60 B / −1 for the ReviewDisplay repair by construction; `Replace` size-if-it-crosses (24 `$4` callers must NOT move); re-home +1 if `Dir.obj` defines the spelling, else 0 |
| **W16-P** (**landed** `2f4c00c5`, see table above) | opus | W16-M's recorded-but-unactioned side findings: (1) LEAD — `fn_82516320` = `SetDiskError` geometry: dtk's listing confirms `blr` at +0 then 204 B with NO prologue, two raw branches into `DataSet`, and the `stw r4,0x34(r3)` that `PlatformMgr.h:125` rests on; adjudicate (a) retail SetDiskError is EMPTY and the tail is other functions' out-of-line blocks vs (b) mis-carve with the real prologue in `fn_825162F0`, via Ghidra xrefs INTO the extent + `.pdata` decode + the 8 callers; fix body/header/carve per the verdict; (2) `src/xdk/LIBCMT/stddef.h:20` `offsetof` precedence bug; (3) `fn_823EC788`/`fn_823EC980`/`fn_82A87F20` mis-pinned into `CharClipDriver.s` — identify on retail bytes, re-home (`.text` only), pre-register (re-homing is NOT neutral); (4) `scripts/dump_vtable.py` picks the first `??_7X…6B…` instead of the primary — fix with a must-fail `--selftest` on `Server`; (5) stretch: resolve `PlatformMgr::Init`'s three callees, name only if all resolve | (1) a truer carve that DROPS bytes is a win (row is 208 B @85.2 now; if (a), body becomes `{}` and the row either crosses at 4 B or the extent is re-carved); (2) Δ0 on the match build, native gate PASS; (3) Δ0 or small per row, 0 wrong names; (4) tooling — Δ0, selftest proven to fail; (5) +1 / +220 B only if all three callees resolve, else 0 |
| **W16-Q** (**landed** `1aaf901e`, see table above) | opus | W16-O's NOT-done list, adjudicated per site on retail bytes: (1) LEAD — the **base-chaining vein**: 22 un-adjudicated `Hmx::Object::Replace(from,to)` fallback sites (CharWeightable, SkeletonUpdate, CharBonesMeshes, FlowSetProperty ×2, Font, Anim ×2, Task ×3, Wind, LitAnim, MatAnim, EnvAnim, EventTrigger, FxSend, TransAnim, Movie, FlowAnimate, BandDirector, DefaultPhysicsManager, Env) — each is (a) no arm / (b) retarget to the intermediate base / (c) keep, decided on the disassembly, NOT swept; (2) `RefIs` X360 branch compares the raw `Ptr()` where retail compares the `Hmx::Object` subobject (vbase upcast) — fix and measure across all 11 callers; (3) STRUCT: `0x824c93c0` (108 B @ 99.81, `default/BandCamShot`) is map-named as DC3's `list<HamCamShot::Target>` but RB3 has no `HamCamShot`; retail's element serializer is 72 B / 2 relocs vs our 12-field 316 B — prove RB3's `Target` field list from the bytes and rb3-Wii, fix struct + serializer + map spelling (only if the base obj defines it); (4) wire `w33_fold_adjudicate.py --self-test` to a must-fail control; (5) stretch: the 316 B DC3 serializer's fate | (1) per-site Δ0 or +1 per row that crosses, 0 wrong changes (a site with no paired retail row is left alone); (2) shared header — expect several `Replace` rows to move, pre-registered, native gate PASS; (3) +2 rows / +180 B if both the 108 B and 72 B rows cross, else a documented struct finding; (4) Δ0 tooling, selftest proven to fail; (5) 0 |
| **W16-R** (**landed** `a2856953`, see table above) | **fable** (escalation) | W16-P's (opus) one "believed unfixable" plus its NOT-done list, escalated per the standing directive: (1) LEAD — settle the `0x82516320` `SetDiskError` mechanism (208 B extent = bare `blr` + a 24 B fragment in MUTUAL branch with `DataSet+0x70` 2.4 MB away + 180 B unreachable EH-bound own body; the only `blr`-headed `.pdata` record of 57,733). First probe is the untested **code-cave hypothesis**: a title-update/pre-ship hot-patch that disabled `SetDiskError` and reused its dead head as scratch for a patched `DataSet` — testable on the vanilla TU0 image `orig/45410914/tu0-archive/` (locate by shape, TU0 addresses differ), then bound the class with a census of cross-extent interior branches (W16-P's `blr` census only catches `blr`-headed caves), and settle what it means for the `DataSet` row (140 B @98.29, `default/DataFunc`). (2) VTABLE — `Server.h` 21 virtuals vs retail `??_7XboxServer` 19 slots (`.rdata 0x8205793C`, slot 15 = `fn_823EC788`): decide WHICH two are surplus by slot-by-slot alignment against our COMDAT (`dump_vtable.py --which`) and the vcall OFFSETS callers use on `Server*`/`XboxServer*` receivers; fix the header on proof; DC3/rb3-Wii `Server.h` diff as context, not oracle. (3) identify the platform-wrapper TU at `0x8283Dxxx`–`0x8283Exxx` (Sleep / XNotifyCreateListener wrappers, `fn_8283EAE0`, `fn_8283EB28`; pinning XDK allowed, porting not), then name `fn_8251D378` = `PlatformMgr::Init` iff all 3 callees resolve (`XNetStartup` guess REFUTED). (4) `PlatformMgr_Xbox.s` `lbl_8217E3B8` carries no RTTI COL — real vtable / EH IP map / plain function table, decided on retail bytes. (5) stretch: name the `XboxServer` slots item 2 proves. Deliverable `docs/decomp/W16R_SETDISKERROR_CAVE_SERVER_VTABLE_2026-09-14.md`. | (1) a settled mechanism with retail-byte evidence and a bounded class (expected: both rows structurally unmatchable if the cave is a TU5 patch — record as such so no lane re-hunts them; 0 bytes); (2) two named surplus virtuals with caller-offset proof, or a documented "cannot be decided from callers" with the offsets listed; bytes = whatever callers of later slots cross (unpriced); (3) one pinned/attributed TU (Δ0, reattribution) and `fn_8251D378` named or a stated blocker; (4) one corrected label or one named vtable; (5) 0–19 named rows, 0 bytes |
| **W16-S** (**landed** `93fa4303`, see table above) | opus | T1 alias identification-vs-fold sweep, W16-Q's own top recommendation: the T1 instrument is one-sided (retail-at-X ≡ our-COMDAT-for-N proves X IS N, not that N folded with the map's name for X; a fold needs our TWO COMDATs identical incl. relocations). Census at `22b583d5`: 1,476 of 1,630 groups cite T1, 980 with live `folded`, 4,675 memberships, **704 groups carry exactly one folded spelling** (the shape W16-Q caught). (1) LEAD — classify every live T1 membership with `tools/ourside_fold_sweep.py`'s our-side COMDAT identity (both our COMDATs present and reloc-identical ⇒ FOLD_CONFIRMED; our N ≡ retail-at-X but our S ≢ our N ⇒ IDENTIFICATION_NOT_A_FOLD; S has no COMDAT ⇒ NEEDS_SOURCE; N has no COMDAT ⇒ STALE_SPELLING, untouched), the 53 groups already carrying our-side proof as the control, each class sized by ablation (not a name-keyed census), then the IDENTIFICATION class corrected the W16-Q way — group kept, `folded` emptied, `withdrawn` record, survivor/map name corrected, non-vacuity checked on `icf_aliases.map` — priced by set-diff of the fuzzy==100 row set, Δ predicted per group before measured. ⛔ nothing pruned; ⛔ a re-home is a map edit with an un-pairing hazard — predict its sign from the caller population first. (2) W16-Q's three scattered bodies needing scatter-includes (`CharWeightable` 144 B, `CharBonesMeshes` 216 B, `RndLightAnim` 140 B). (3) the unexplained +1 fn on W16-Q's re-home — name the row. (4) stretch: `HamCamShot.cpp` scatter-include removal from `BandCamShot.cpp` (467 symbols; its own A/B). Deliverable `docs/decomp/W16S_T1_ALIAS_IDENTIFICATION_VS_FOLD_2026-09-14.md`. | (1) a four-class census over 4,675 memberships with bytes per class, 53/53 control confirmed, and every IDENTIFICATION_NOT_A_FOLD corrected with Δ predicted vs measured (expected mostly Δ0 per W16-Q's precedent, possibly net-negative where a corrected name exposes a wrong callee — accuracy over headline); (2) +3 fns / ~+500 B if the scatter-includes land; (3) one named row; (4) a measured Δ or a documented refusal |
| **W16-T** (**landed** `aa20b995`, see table above) | opus | W16-J's source-shaped leftovers, in priority order: (1) LEAD — the two EH funclets W16-J's `??1DataResultList` naming exposed (`fn_825800B0` MetaPerformer, `fn_824F9BAC` RockCentral, 40 B each at 99.5): our funclets destroy a `Message` / `DataArrayPtr` where retail destroys a `DataResultList` — find each parent function, adjudicate the local's type on retail bytes, fix the source (a wrong-type unwind is a behavioural bug the metric cannot see; land on merit); (2) `FriendsProvider.h` `vector<int>` → `vector<Friend*>` FIRST, then ctor+dtor+`Reload` (140 B) with the three-carve pin W16-J read out (re-homing is NOT neutral; set-diff); (3) audit `0x82529ae8` `?ReceiveUpstreamAccelerometerResponse@@` — the neighbour of W16-J's corrected `0x82529af0`, same naming family; (4) if budget: `?Release@VertexBufferData@DxMesh@@` 68 B, `??0Shuttle@@` 32 B, `fn_82654440` 76 B readings. Deliverable `docs/decomp/W16T_DATARESULTLIST_LEAD_FRIENDSPROVIDER_2026-09-14.md`. | worktree `~/tmp/wt-w16-t` off `94b0ddae` |
| **W16-U** (**landed** `5b9f31dc`, see table above) | opus | W16-S's leftovers, in priority order: (1) LEAD — the four VT1 addresses where retail PROVABLY folded two methods (RTTI vtable geometry places one address in both classes' vtables) yet OUR two COMDATs differ, so one of our bodies is wrong for RB3: `0x8234ebc8` BandTrack::Copy/BandDirector::Replace, `0x823af220` CharData::Handle/CharWeightable::Handle, `0x824863e8` RndCamAnim/RndLightAnim::SyncProperty, `0x82493ca0` RndMotionBlur::Save/CharTransDraw::Save — read retail's body, diff both our COMDATs (`tools/comdat_bytes.py`), decide which of ours matches and fix the other in SOURCE (⛔ never by editing the proven alias); (2) the 9 WEAK contradictions starting with `??_GUIPanel`/`??_EUIPanel` (closure fails closed on a callee our build does not define), leaving the one SIZE row `0x827f42a8` unless a two-sided normalization exists; (3) a first re-home batch (≤10 rows) from the 175 `IDENTIFICATION_NOT_A_FOLD` rows, per-row pairability + caller-sign prediction written BEFORE the map edit (re-homing is NOT neutral; `0x822dea78` gi=24 carries W16-I's restored membership, do not clobber); (4) if budget: W16-Q's three scattered bodies (CharWeightable 144 B, CharBonesMeshes 216 B, RndLightAnim 140 B). Deliverable `docs/decomp/W16U_VT1_DIVERGENCES_WEAK_CONTRADICTIONS_REHOME_2026-09-14.md`. | worktree `~/tmp/wt-w16-u` off `631973b1` |
| **W16-V** (**landed** `93283bb4`, see table above) | opus | W16-R's NOT-done list: `WinSockSocket::Init` 104-vs-100 B residue (source); the two unpinned xapilib gaps via DC3 `closehandle`/`getoverlappedresult` twins (pins); `0x82270E68` RhythmDetector mis-attribution (re-home, NOT neutral, prediction first); `?ReleaseAutoRelease@DxRnd@@` 652 B pin with an `auto_03` unit-identity proof; commit the landing kit (`rebase_drive.py`, `resolve_aliases.py` v2, `resolve_map.py`) under `tools/rebase_kit/` with a W16-J replay test (refs `f29132d7`/`a8f9e92b`/`95ebc590`/`dcd8d6fe`) that v1 must fail; `config.yml` lineage line + the CLAUDE.md "vanilla retail XEX" correction | `docs/decomp/W16V_XAPILIB_GAPS_WINSOCK_INIT_REBASE_KIT_2026-09-14.md` |
| **W16-W** (**landed** `a272c5d8`, see table above) | opus | objdiff funclet byte-signature pairing (a RULER change, shared by rb3-xenon/rb3/dc3-decomp): W16-T sized the `mpn==100 & fuzzy<100` class at 3,366 rows / 204,884 B (1,842 rows / 73,680 B at size 40) and showed the charge is an ARBITRARY assignment inside a byte-signature equivalence class — `funclet_signature` zeroes every reloc word and drops target names, then passes 2/2b zip by sort order (RockCentral: 0/5 correct). Add a relocation-target-NAME tiebreaker inside ambiguous groups (reuse `NamedSig`/`RelocDesc`, alias- and placeholder-aware like `reloc_eq`), tests that can fail, version bump 4.2.8→4.2.9, build ONLY with `CARGO_TARGET_DIR=~/tmp/objdiff-build`, measure in the worktree by pointing its build at the staged binary with `report.json`+cache wiped, set-diff + 5 hand-verified rows, `provenance.tool_binary_hash` pasted. Coordinator swaps the live binary after a byte-verified backup (`~/tmp/objdiff-backup/objdiff-cli.4.2.8-a5c35b15`) and re-measures rb3/dc3. Secondary: `fn_82654440` naming needs an oracle. | `docs/decomp/RULER_CHANGE_funclet_name_tiebreak_2026-09-14.md` |
| **W16-X** (**landed** `352ba26c`, see table above) | opus | W16-U's two REAL vbase-destructor divergences (`??_GGemTrainerLoopPanel`, `??_GTourChallengeResultsPanel`: ours 80 B routing through `??_D<Class>`, retail's fold survivor 68 B — a class-hierarchy / vtable truth: does our declaration carry a virtual base or a user dtor retail's does not? adjudicate on retail bytes + `class_layout_report.py` diff against a 68 B sibling), then the TWO-SIDED COMDAT size normalizer W16-S and W16-U both declined to build one-sided (EH funclets excluded on BOTH sides, self-test that can fail) applied to `0x827f42a8` (gi=1621) and the 67 `survivor_vs_retail == SIZE` census rows, then the `tools/ourside_fold_sweep.py` re-run after the `comdat_bytes` fix (new admissions need T1 proofs), then if budget remains a ≤10-row first batch from `UNDECIDED_MASKED` (643 / 41,360 B; the MAPID-1 lever, payout is bug exposure). Worktree `~/tmp/wt-w16-x` off `11fc1234`. | `docs/decomp/W16X_VBASE_DTOR_DIVERGENCE_TWO_SIDED_SIZE_2026-09-14.md` |
| **W16-Y** (**landed** `505d2bff`, see table above) | opus | Alias hygiene + naming, off main `b4104f3d` on the 4.2.9 ruler: (1) LEAD — the ONE duplicate survivor in `scripts/symbol_aliases.json` (group 147 `0x823c3ac8`, `folded: []`, 86 withdrawn, no map row, pinned inside `CharBlendBone.cpp`; group 1480 `0x823d14c0` in `CharClipSet.cpp`, 4 folded — both key `list<Hmx::Object*>::insert`), which is exactly the 2 red tests in `pytest tools` (`test_survivor_identifies_a_group`, `test_the_ledger_keys_do_not_alias_two_groups_together`): adjudicate `0x823c3ac8` on retail bytes (same instantiation? different function? phantom carve?), repair with `withdrawn` records, 326/0; (2) the **109 null-valued keys** in `target_symbol_map.json` — provenance, what the renamer does with `null`, fix at source; (3) `fn_82654440` (76 B, `SessionUsersProvider` ShowGamercard tail call, W16-T §4c) — name ONLY on a witnessed spelling (rb3-Wii, RTTI/handler tables, the OvershellSlot caller), else record the refusal | `docs/decomp/W16Y_ALIAS_ORPHAN_147_NULL_MAP_KEYS_FN82654440_2026-09-14.md` |
| **W16-Z** (**landed** `c6552cfa`, see table above) | opus | Source + struct truth, off main `b4104f3d` on the 4.2.9 ruler: (1) LEAD — write `DxRnd::ReleaseAutoRelease` (retail `0x8273CC08`, 652 B, identity PROVEN by W16-V, pinned + mapped but reads 0% because `Rnd_Xbox.cpp` does not define it) from DC3's `0x8261A5B0` body with retail-driven corrections — the member walked at retail `0x1c4` vs DC3 `0x224` is a `DxRnd` LAYOUT finding (`class_layout_report.py DxRnd`, second-witness rule, the 121 `Rnd_Xbox` rows at fuzzy 100 named as risk rows); (2) POSITIVE identification of the 8-row `default/auto_03_8273CEF0_text` remainder (2,248 B) starting from the bit-flag global at `0x82E04FFC` (who else touches it, callees of the 1,392 B row, DC3 twins by structure) — pin only on a witness, never adjacency; (3) only if W16-X has landed: one item from its NOT-done list | `docs/decomp/W16Z_RELEASEAUTORELEASE_BODY_AUTO03_REMAINDER_2026-09-14.md` |
| **W16-AA** (**landed** `7e6b2498`, see table above) | opus | W16-X Item 4, never started: the `UNDECIDED_MASKED` alias class (643 memberships / 41,360 B forgiven on a comparison that masks unnamed branch destinations). Top-10 by forgiven bytes (`TeleportTarget@HamCamShot` 432 B, three `AccomplishmentCmp` merge-sort instantiations, `~vector<String,StlNodeAlloc>`, `ObjDirItr<CharClipGroup>` ctor, two `MakeString` instantiations, `~NetLoaderXbox`, a `Key<vector<Vector2>>` `_M_clear`): identify each unnamed destination on retail bytes (call-site signature vs candidate body, and what our survivor vs folded COMDAT calls at that position), pre-register the sign from the caller population, batch ≤10 map names; withdraw with record where the alias is forgiving a wrong callee. Plus name `0x823c3960` (W16-Y's `list<ConstraintSystem>` node creator) after adjudicating the `MemOrPoolAlloc`/`MemOrPoolAllocSTL`/`_Copy_Construct` spellings it exposes. Payout is bug exposure, not bytes. |
| **W16-AB** (**landed** `120d205e`, see table above) | opus | W16-X Item 2's unactioned output: of 68 census SIZE rows, 44 REAL two-sided size mismatches / 24 NO_PDATA. LEAD: write bodies for the three our-side stubs (`RndMat::Terminate` 4→96 B, `RockCentral::CancelOutstandingCalls` 12→128 B, `StlNodeAlloc<String>::deallocate` 8→128 B) from retail bytes + dc3/rb3wii oracles and say what the alias hid; adjudicate the other 41 (which side is wrong: source, map, or membership — withdraw only with records); re-adjudicate the 24 NO_PDATA rows on `tools/icf_pair_adjudicate.py` (relocation-normalized hash) since size is unlicensed there. |
| **W16-AC** (**landed** `8ea97724`, see table above) | **fable** | `?DoPointTests@DxRnd@@AAAXXZ` (1,392 B, fuzzy 75.10 / mpn 76.86, `default/Rnd_Xbox`): W16-Z verified the layout premise WRONG and left a body-shape residual — retail prologue r16–r31 vs ours r19–r31, frame Δ −0x20, 201 charged sites ⇒ three more values live across calls in retail. Diagnose r16/r17/r18 liveness on retail asm (keyed on the `.fn` symbol), find the source construct, report every variant with its measured fuzzy. Secondary: identify the callees of the four 0% rows in `auto_03_8273D658_text` (356 B; all callees unmapped) and pin only with a source-side witness (Δ0 expected — reattribution). |
| **W16-AD** (**landed** `14ac3de1`, see table above) | opus | W16-AA's successor: mechanise the RETAIL-SIDE FOLD WITNESS (`tools/w16ad_fold_witness.py`) over all 643 `UNDECIDED_MASKED` memberships (41,360 B forgiven) — for each row's channel-(a) discriminator (`c_N` vs `c_S`, unnamed in the map), find map-named retail callers of each, decode their `bl`s on retail bytes, and partition WITNESS_CONFIRMED / WITNESS_REFUTED / NO_WITNESS / CHANNEL_B (self-test must reproduce W16-AA's hand verdicts on gi=444/1302/1050/730/95 AND fire on a constructed negative control). Withdraw only REFUTED rows, with `withdrawn` records and per-row byte predictions (≤20). Secondary: adjudicate the Accomplishment/Goal algorithm-instantiation name cluster on retail bytes (comparator rows are size-correct 3/3; `??RGoalCmp@@` absent from the map; `0x825f32a8` 176 B unnamed), repairing only where the base obj defines the name. |
| **W16-AE** (**landed** `8f434138`, see table above) | **fable** (escalation) | ESCALATION of W16-AB's four "cannot be fixed" items, with its doc attached (`docs/decomp/W16AB_SIZE_MISMATCH_44_STUB_BODIES_NOPDATA24_2026-09-14.md`). LEAD: `0x823eb548` is byte-proven `?Replace@MsgSource@@$4PPPPPPPM@BI@…` (vtordisp thunk) but our COMDAT is emitted only by four `band3/meta_band/` objs while the address sits in `Anim.cpp`'s engine span — find the vtable that references it (retail `.rdata` scan) and the engine TU that should emit it, or prove the Anim.cpp carve is wrong; no circular pin. Then the six non-island span carves (`0x822c83d0`, `0x82389608`, `0x8238c798`, `0x823ea1c8`, `0x822c5600`, `0x8231a578`) on `.pdata`/byte geometry only; the two tooling gaps (`_denylist` never polices the applied map — add a failing-capable check; `icf_alias_build.py` emitted two T1 claims `icf_pair_adjudicate.py` refutes — fix the generator, re-run over the full set); `0x827f42a8` undecided; optionally verify one 2-candidate arbitrary-bijection class on bytes. Baseline `bb9b7e55` 43,309 / 3,989,624 B, rows `~/tmp/rows_w16ab_main.json`. |
| **W16-AF** (**landed** `5b70c9eb`, see table above) | **opus** | MAP lane: the 3-way Accomplishment comparator ROTATION W16-AD left (doc §5.5) — the `0x825f3xxx` band map-named `<AccomplishmentCmp>` (`__linear_insert` `0x825f3db8`, `__lower_bound` `0x825f3480`, `__merge_backward` `0x825f3e38`) calls an UNNAMED 176 B comparator at `0x825f32a8`; hypothesis: these are `CampaignGoalsLeaderboardChoicePanel`'s `<GoalCmp>` instantiations = W16-AD's 6 `NOT_IN_MAP` rows. Identify `0x825f32a8` on retail bytes FIRST, then rename + re-home across units with pre-registered predicted-vs-measured set-diff (re-homing is NOT neutral). If the identity cannot be defended on bytes, do not guess a name — report the evidence both ways. Worktree `~/tmp/wt-w16-af`, branch `w16-af`. | `docs/decomp/W16AF_ACCOMPLISHMENT_ROTATION_0x825f32a8_2026-09-14.md` |
| **W16-AG** (**landed** `2473fb6b`, see table above) | **opus** | ALIAS lane: land the 43 remaining `WITNESS_REFUTED` memberships (2,580 B, `W16AD_withdrawn_20_2026-09-14.json` key `not_landed_refuted`) that W16-AD proved but did not land — re-run the witness per row first (the tool reproduced only 2/6 by hand and its negative control FIRED, so a verdict is not inherited), predict with `tools/w16ad_predict_withdrawal.py`, apply in ≤20 batches via `tools/w16ad_apply_withdrawals.py` keyed by `(survivor, address)` with `withdrawn` records, measure per batch; adjudicate the 7 guard-blocked pair-instances (492 B, §6) on bytes. Never prune a group; `NO_WITNESS` rows untouched. Worktree `~/tmp/wt-w16-ag`, branch `w16-ag`. | `docs/decomp/W16AG_REFUTED_43_WITHDRAWALS_GUARD7_2026-09-14.md` |
| **W16-AH** (**landed** `24494d6a`, see table above) | **opus** | ALIAS/MAP lane: the 496 `NO_WITNESS_FOLDED_SIDE` rows (30,864 B) W16-AG left — minus 4 rows / 724 B in the `0x825f3xxx` band W16-AF owns ⇒ **492 rows / 30,140 B collapsing onto 58 distinct folded-side callees `c_N`** (top: `ObjPtrVec<Spotlight>::Node` copy-ctor 42 rows; thirteen at 28 rows each). Route A: retail RTTI probe shows the `c_N` types **Flow/FlowLabel/FlowNode/FlowOutPort/HamCharacter/HamMove/RhythmDetector** (28 rows each) plus DepthBuffer3D/HamSupereasyData have NO `.?AV` descriptor in `band.exe` (controls Spotlight/ObjectDir present; ⚠ `Symbol` is non-polymorphic and reads absent — instrument speaks only for polymorphic types) ⇒ DC3-only instantiations that can never have been in a retail COMDAT group; adjudicate with the criterion stated so it can fail, report paired vs unpaired callers separately (a paired caller spelling a DC3-only type = a DC3 leak the alias hides), withdraw with records in ≤20 batches via `tools/w16ag_{predict,apply}_withdrawals.py` keyed `(survivor, address)`, all-row snapshot per batch. Route B: the retail-present remainder (~22 rows / 1,796 B) — ONE map identification of an EQ caller per `c_N`, then re-run `tools/w16ad_fold_witness.py --sweep` unchanged and report the verdict flips. Never prune, never guess a name, no source edits. Worktree `~/tmp/wt-w16-ah`, branch `w16-ah`, base `740e40f7` (43,319 / 3,990,660 B), rows `~/tmp/rows_w16ag_main.json`. | `docs/decomp/W16AH_NO_WITNESS_492_TYPE_EXISTENCE_AND_ONE_CALLER_2026-09-14.md` |
| **W16-AI** (**landed** `0667441b`, see table above) | **opus** | SRC lane: land the one source divergence W16-AF sized and deliberately left (§6) — `??RGoalCmp@@QBA_NVSymbol@@0@Z` at `0x825f32a8` uses an extern `campaign_metascore` where retail has a function-local static (guard `0x82E00398`), ours 144 B vs retail 176 B, predicted **+176 B / +1 fn**; then adjudicate the five remaining sub-100 rows of `CampaignGoalsLeaderboardChoicePanel` (296 B: `fn_825F57C0` 148, `fn_825F5854` 68, `fn_825F5244` 40 @99.5, three 8–16 B thunks) toward a unit completion (44/50 → 50/50). Native gate mandatory (edits `src/band3/`). | `docs/decomp/W16AI_CAMPAIGNGOALS_GOALCMP_LOCAL_STATIC_AND_UNIT_COMPLETION_2026-09-14.md` |
| **W16-AJ** (**landed** `c62ce4e0`, see table above) | **fable** (escalation) | ESCALATION of W16-AI's one unresolved identification, with its doc attached (§5 of `docs/decomp/W16AI_CAMPAIGNGOALS_GOALCMP_LOCAL_STATIC_AND_UNIT_COMPLETION_2026-09-14.md`): `fn_825F5244` (40 B, one reloc-name charge) is decided by what `0x826100F8` is — retail's `0x826101B8` thunk (map: `??1?$map@HPAVUIComponent@@…`) branches there while ours tail-calls `??1?$hashtable@…`; either a 40 B map repair or a real container divergence in the Provider ctor. Plus `fn_825F4288` (8 B `WDM@` adjustor — vtable-reachability as the distinguishing evidence) and the adjacent unit `CampaignSongInfoPanel` (37/54 rows, 2,344/5,124 B; 11 anonymous 0% rows ≈2,300 B that the rb3-Wii oracle names, `Refresh` 348 B at 48%). ⛔ No `symbol_aliases.json` edits (W16-AE/AH own that file); alias proposals go to the doc. | `docs/decomp/W16AJ_0x826100F8_IDENTIFICATION_AND_CAMPAIGNSONGINFOPANEL_2026-09-14.md` |
| **W16-AK** (**landed** `976a98a3`, see table above) | **opus** | SOURCE/MAP lane: `default/OvershellPanel` 250/272 fns, 18,564/33,500 B. Shared `diff_arg` charge on the 99.8x rows adjudicated by the coordinator: retail branches to `0x826c3888` (4 B `blr`, alias group 1537 survivor) where we call the empty `InputMgr::Set/ClearInvalidMessageSink` -- a real fold with a missing membership, routed to W16-AE (alias owner). AK does the rest: charged-site census of every named sub-100 row, `Poll` 100 B at 0% and `ResolveSlotStates` 1,416 B at 93.22 against the rb3-Wii oracle, and identification of ten anonymous 0% rows (3,140 B). No alias-file edits; alias proposals to `docs/decomp/W16AK_alias_proposals_2026-09-14.json`. | `docs/decomp/W16AK_OVERSHELLPANEL_CHARGED_SITES_AND_COMPLETION_2026-09-14.md` |
| **W16-AL** (**landed** `0196b0ce`, see table above) | **opus** | ALIAS/MAP lane, owns `scripts/symbol_aliases.json`: install the adjudicated proposals other lanes were barred from -- W16-AJ §6 proposals 1-7 (hashtable/hash_map dtor groups at `0x826100f8`/`0x826101b8` with the `0x826100f8` map row in the same commit, TourDescPanel `??_G` fold, repair of the wrong T1 group at `0x8261feb8` to MainHubPanel::Unload, two new Unload/Load folds, the AuditionSessionPanel slot-0 thunk), then W16-AH's 14 HELD ObjPtrVec survivor rows as a map-identification question, then W16-AK's 7 proposals once AK lands (L1_T1 pair needs `0cd974cb` in-tree). Every membership re-adjudicated on retail bytes (`icf_pair_adjudicate`, `alias_forgiveness_audit`), priced by set-diff with the `none` control, removals only via `withdrawn` records. No `src/` or splits edits. | `docs/decomp/W16AL_ALIAS_INSTALL_AJ_AH_AK_PROPOSALS_2026-09-14.md` |
| **W16-AM** (**landed** `80aa49ef`, see table above) | **opus** | IDENTIFICATION lane: W16-AE §9 NOT-done list. The five `_bijection_arbitrary` classes (`0x823d3918` x20, `0x823f0b50`, `0x8248f1c0`, `0x82787718`, `0x827d5bb0`) tested on the 2-candidate cases for reloc-target and `.xdata` identity before accepting "arbitrary"; `0x822b6538` / anonymous `fn_824CE130` (wrong-twin `BitmapOverride` spelling, lead in `1fdcb69e`); the unexplained `0x82c16aa0` denylist entry; re-derivation of the 26 non-refuted carry-path contradictions on a built tree; the pre-existing red test arms last. Map rows only for proven addresses; no alias-file edits (W16-AL owns it). | `docs/decomp/W16AM_BIJECTION_CLASSES_fn_824CE130_DENYLIST_CARRYPATH_2026-09-14.md` |
| **W16-AN** (**landed** `302fbd0e`, see table above) | **opus** | MAINHUBPANEL unit lane (166/176): `CheckProfileForTicker` 98.38 by the W16-B call-site cast precedent (`RockCentral.cpp:279-286`) — ⛔ NOT the `Server.h` unsigned-return change AJ §7 proposed, which the record already refutes (four retail sites at 100 with `int`); `?Poll@MainHubPanel` 51.80 (244 B) opened on retail bytes; the eight anonymous rows (388–180 B, no map rows) identified from the rb3-Wii oracle with the 2-candidate test; optional last: re-home the `PhysicsManager.cpp` mis-pin `0x825F5BF4–0x825F5D28` into CampaignSongInfoPanel per AJ §7, A/B'd (re-homing is not neutral). Bars: AL's `0x825f58c8`/`0x825f5920`, alias file, AM's addresses, OvershellPanel, Server vtable. | `docs/decomp/W16AN_MAINHUBPANEL_TICKER_POLL_ANON_ROWS_2026-09-14.md` |
| **W16-AO** (**landed** `622690b6`, see table above) | **opus** | MAP lane: W16-AM §6's ~10 remaining inconsistent `list<T>` bijection rows (`0x822b6798`, `0x824cf800`, `0x824cf770`, `0x823c40f0`, `0x823296d0`, `0x824a1b40`, `0x824a1ab0`, `0x824e1a28`, `0x8246af50`, `0x822a8668`) by AM §2's method — `erase<list<T>>` is 108 B and byte-identical once relocs are masked but the `~T` relocation TARGET names `T`; `resize<list<T>>` inherits `T` from its erase callee. Verbatim spelling substitution (MSVC back-refs), ninja-wired injectivity check, priced by set-diff with the `none` control; map rows only, no `src`/splits/alias edits. Expected a few hundred bytes positive. | `docs/decomp/W16AO_LIST_T_BIJECTION_ROWS_2026-09-14.md` |
| **W16-AP** (**landed** `440bc9db`, see table above) | **opus** | GAMEPANEL unit-closure lane (91/97, ~3,300 B open, masked rows excluded): identify + port `fn_82694860` 196 B, `fn_82694B00` 72 B, `fn_82695178` 628 B, `fn_82696B48` 1,812 B (all 0%), close `?IsLoaded@GamePanel` 99.84 and `??0GamePanel` 99.96 from `report.json`'s charged-site list, against oracle `../rb3/src/band3/game/GamePanel.cpp`; reuse W16-AN's function-local-static finding. Stretch: PerfectOverdriveTracker 41/43 (`fn_826E1740`, `Poll_` 99.60, `GetPlayerContributionString`). Fallbacks if drained: GuitarController 25/30 or DataArraySongInfo 39/45 (four anon rows each). ⛔ `Server.h:26` `GetPlayerID` stays `int`. | `docs/decomp/W16AP_GAMEPANEL_UNIT_CLOSURE_2026-09-14.md` |
| **W16-AQ** (**landed** `90dfdb79`, see table above) | **opus** | SOURCE lane for W16-AL's filed follow-ups: the two dtor rows AL let fall out (`??1TourProgress` 192 B, `??1NameGenerator` 88 B — verify the "container-type divergence" claim both ways before fixing), `0x823d3918` `CharLipSync::Handle` (our 412 B vs retail's 164 B forwarder, +164 B, needs source + map + membership), `ResolvePartWaitStates` idx-161 `beq` (1,356 B), the `0x82682668` splits re-home (+28 B); stretch: AH's 14 held memberships via an `ObjPtrList` source correction. Owns `scripts/symbol_aliases.json` and `splits.txt`. | `docs/decomp/W16AQ_AL_FOLLOWUPS_DTOR_TYPES_LIPSYNC_HANDLE_2026-09-14.md` |
| **W16-AR** (**landed** `2fd524f0`, see table above) | **opus** | W16-AO's filed items: repair the three wrong rows AO adjudicated outside its brief (`0x822b6ee8` ObjList wrapper spelled for the wrong T, `0x823c38e8` erase spelled `Sink@MsgSinks` while destroying `ObjRefConcrete<RndTransformable>`, `0x822a8bd0` an ObjVector resize forwarding to a list resize); hunt the spellings each repair displaces by caller channel; **re-derive the `matched_functions` ruler-invariance claim** (AO read 43,436 name_check vs 45,105 none against RULER-SWEEP's "bit-identical") with the objdiff-core mechanism and a proposed CLAUDE.md correction; stretch: AO's seven anonymous addresses, each adjudicated and measured separately. Bars: alias file + splits (AQ), GamePanel (AP), AO's eleven landed addresses. | `docs/decomp/W16AR_AO_FILED_ROWS_RULER_INVARIANCE_2026-09-14.md` |
| **W16-AS** (**landed** `2c9103b1`, see table above) | **opus** | IDENTIFICATION + VTABLE lane on W16-AP's two filed items: (1) `0x826ccc10` is named `??0Splash` but AP's retail-byte evidence (328 B body, `default/Splash` self-row 6.89 %, `??0GamePanel`'s only charged site) says it is `??0DirectInstrument` — settle it by the vtable pointer the ctor body stores, check the paired base obj defines the new name before renaming, locate the real Splash ctor, price by set-diff; (2) `UpdateNowBar` 628 B @ 5.93 — establish `TrackPanelDirBase` vtable slot `0xd4`'s true 4-parameter signature from every retail caller and override, measure the shared-header blast radius before and after, then port; correct the refuted in-tree comment. Alias file, splits and AQ/AR files barred. | — |
| **W16-AT** (**landed** `ea012adc`, see table above) | **fable** | port `?UpdateNowBar@GamePanel@@QAAXXZ` (628 B, fuzzy 8.19): model `TheSongDB`'s 0x24-byte record vector at `+0x20`, name the `lbl_82C78F5C` singleton behind thunk `fn_827C91A0`; owns `GamePanel.cpp/.h` + its map rows | — |
| **W16-AU** (**landed** `bc2cd5d1`, see table above) | opus | splits: `DirectInstrument.cpp` heading (AS filing, 344 B size-identical), AR-1 `0x823c38e8` (144 B) + AR-2 `0x822a8bd0` (328 B, A/B) re-homes, AQ's unhomed `__RTDynamicCast` thunk `0x82682688`; owns `splits.txt` + `objects.json` | — |
| **W16-AV** (**landed** `df358e10`, see table above) | opus | verify on retail bytes and install AO `0x8246af50`, AP-1/2/3, AR-3 rotation; decide `?Handle@DxCubeTex@@`/`NgFur` for group 478; ObjPtrList per-site repair if budget; owns `symbol_aliases.json` + `target_symbol_map.json` | — |
| **W16-AW** (**landed** `db54a396`, see table above) | opus | SOURCE lane, W16-AU's filed follow-ups: `??0DirectInstrument@@` 328 B (named `FilePath` local → temporary in argument position, AU's two-instruction diagnosis); A/B removal of the obsolete `TrainerPanel.cpp:465` scatter-include and analysis + A/B of the unanalysed `RhythmDetector.cpp:926` one (never blind); byte-signature search for `?Enabled@`/`?SetVolume@DirectInstrument@@` COMDATs (JSON proposals only); AU-2 `GetLocalBandUser` caller enumeration as proof backfill. Owns `DirectInstrument.cpp`/`TrainerPanel.cpp`/`RhythmDetector.cpp`; no splits/map/alias edits | — |
| **W16-AX** (**landed** `73927a9b`, see table above) | opus | SPLITS lane: re-home `0x824C9878` (156 B `_M_splice_insert_dispatch<EventCall@EventAnim>`, today under `EventTrigger.cpp:` at fuzzy 0) to `EventAnim.cpp:` plus the two EventAnim gaps, one move per commit; carve W16-AT's TimeConversion cluster `0x827C90B8`–`0x827C9370` out of `StringTable.cpp:`'s `0x827C8C88–0x827C97C8` block under a new `system/utl/TimeConversion.cpp:` heading (inventory the whole block first; re-homing is NOT neutral); verify-and-close the PAIRFIX "doubled heading" defect (UIStats/AccomplishmentProgress/Game — grep finds only path-qualified headings today). Owns `splits.txt`/`objects.json` only | — |
| **W16-AY** (**landed** `0ddb8a0c`, see table above) | opus | IDENTIFICATION/MAP lane: re-identify `0x827d2500` (12 B, SongInfoCopy, mis-named `_M_throw_length_error`, body reads `TheTempoMap`); diagnose W16-AV's two corrected ObjPtrVec rows (`0x8278b7f0` HamMove real divergence; `0x822abd60` ClipDistMap missing instantiation); install W16-AT's proved names `gDefaultTempoMap` (`0x82C78F60`) / `TheTaskMgr` (`0x82E051A0`) from COFF spellings, `fn_827C91A0` only with new proof; verify the `0x826cca78` `$4` adjustor row. Owns the map, aliases, `SongInfoCopy.cpp`/`HamMove.cpp`/`ClipDistMap.cpp`; no splits edits | — |
| **W16-AZ** (**landed** `256a6376`, see table above) | **fable** | splits+map+alias: PhraseAnalyzer's two holes (`0x8278B7F0–B838` from HamMove, `0x8278BB98–BD68` from FileChecksum) re-homed + rows `0x8278b7f0`/`0x8278bb98` renamed to `PhraseAnalyzer::PhraseData` spellings + the mis-oriented `0x8278b7f0` alias group reversed with a `withdrawn` record (AY §2a recipe, +484 B predicted; AY's "PhraseAnalyzer.cpp is not pinned" claim corrected in the brief — it has 17 blocks); `?SetTheTempoMap@@` `0x827d2500` SongInfoCopy→TempoMap boundary move (+12 B); `0x822abd60` ClipDistMap row adjudicated (36 B element, `vector<BandPatchMesh>` callee) | — |
| **W16-BA** (**landed** `cf8fac20`, see table above) | opus | splits+map+alias: AX-1 (`0x824C97D8–9878` EventTrigger→EventAnim + `0x824c97d8` rename in ONE commit, +116 B) and AX-2 (the 4 B zero-reloc `blr` `StlNodeAlloc<_List_node<int>>` ctor alias, byte-proved, +156 B); AW Item 2b — replace RhythmDetector.cpp's DirectInstrument.cpp scatter-include with the genuine `ObjDirPtr<ObjectDir>` instantiation if retail's TU has one; `fn_822709A8` / `??3DirLoader` adjudication; stretch `0x824C95F0–97D8` anonymous 488 B | — |
| **W16-BB** (**landed** `2b11726d`, see table above) | opus | source: `??0GlitchFinder@@QAA@XZ` 340 B fuzzy 0 (StringTable scatter-include, largest unmatched named row in the unit); AW-2 `SetVolume@DirectInstrument` `0x82A478D0` — adjudicate all 17 call sites + the 11 other signature-identical addresses, ship map row + full alias group or a written census; rewrite the stale CLAUDE.md doubled-headings paragraph (fixed by `b341d7ab`) as a dated record; stretch `fn_827C91A0` | — |
| **W16-BC** (**landed** `ce520e81`, see table above) | opus | MAP+ALIAS identification lane, NO splits: (1) `0x82787ed0` — the callee holding `default/SongLayout::push_back<SongSection>` (128 B) at 99.84; map names it `_M_insert_overflow_aux<pair<int,int>>` (8 B element) while our caller calls the `VSongSection` instantiation (16 B) — hand T1 of retail `fn_82787ED0` vs both our COMDATs ⇒ proven fold (alias) / wrong map name (repair only if the owning obj defines it) / recorded negative; (2) PhraseAnalyzer's six anonymous rows (`fn_8278C160` 408 B first) identified against `PhraseAnalyzer.obj` COMDATs, named only with definer + body match; (3) `fn_822ABCE0`/`fn_822ABDD8` only with new evidence. Files the `get_allocator<RawPhrase>` membership for BA's group. Baseline `43beee5c2647` = 43,497 / 4,034,268 B. | — |
| **W16-BD** (**landed** `3b479875`, see table above) | opus | SPLITS lane (sole `splits.txt` editor): BB-1 CheatsManager ctor re-home (`0x827C1A84–0x827C3340` one block, +1 fn / +340 B predicted) then the dead `StringTable.cpp:103` scatter-include as its own commit; BC §3.3 `0x822ab0b0` `<OldColorOption>` re-home HamCamTransform→OutfitConfig + rename in ONE commit (+232 B predicted, after AZ §3.4's masked-compare); BA's `0x82817ae8` UILabel→HamNavList + `ObjDirPtr<HamListRibbon>::operator=` name (300 B); BC §2.4 `fn_8278C2F8` = `??_GFillInfo@@` block split → SongData (lowest) | — |
| **W16-BE** (**landed** `01762c70`, see table above) | opus | SOURCE lane: BC §3.2 `?NewObject@OutfitConfig@@` 112 B at fuzzy 86.9 — retail is the inlined OBJ_MEM_OVERLOAD shape, our header carries rb3-Wii's `NEW_OVERLOAD` (ObjMacros.h's measured per-class lever, 4/4 retail contradictions); sweep other `bandobj/` NewObject rows with the same signature, per-class evidence, no bulk flip; BC §4 `get_allocator<RawPhrase>` membership on alias group `0x826c3888` (FT-EMPTY proof or a written refusal), lifts `??0?$vector@URawPhrase` 120 B | — |
| **W16-BF** (**landed** `61894b46`, see table above) | **fable** | BA Item 4: the 8 B liveness gap in `PropSync<ObjList<EventAnim::EventCall>>` (`fn_824C95F8` 388 B retail vs 396 B ours — retail keeps `i+1` in r6, ours writes it back and re-`mr`s; T-dependent, the ProxyCall instantiation is already 388 B, so every sibling instantiation must stay byte-identical); THEN re-home `0x824C95F0–97D8` EventTrigger→EventAnim + name the three rows (+388 B collectable only after the gap closes); owns only the `EventTrigger.cpp:`/`EventAnim.cpp:` headings | — |
| **W16-BG** (**landed** `adac046e`, see table above) | opus | MAP-ONLY lane: the 24 `__unwind$`/`__catch$`-named map rows (21 + 3; introduced by laneAP `783ebf34`) that objdiff flags Hidden and drops from BOTH numerator and denominator (BF §5.1) while ~22,900 sibling funclets stay `fn_`-named, in `total_code`, and pair by byte signature. Adjudicate each on retail bytes (true funclet? parent? our-side counterpart?), state the pairing mechanism from objdiff source with file:line, apply the uniform accurate handling (expected: delete the 24 names ⇒ `total_functions` +24, `total_code` +Σsizes, code% slightly DOWN = a truer denominator), measure by full build + set-diff + the `total_*` keys, sweep for siblings. No splits, no aliases, no `src/`. | — |
| **W16-BH** (**landed** `e3de73bd`, see table above) | opus | MAP + `Ham.cpp:` lane: `0x8227a828` `??0HamSong@@QAA@XZ` (164 B, the map's only HamSong row, pairs 100 in `default/Ham`) is suspected to be `??0BandSong@@QAA@XZ` — retail has NO `HamSong` byte string but has `.?AVBandSong@@` RTTI; `?StaticClassName@BandSong@@` `0x8227a7a8` sits in the gap between `Ham.cpp:`'s blocks 1 and 2; `?NewObject@BandSong@@` `0x8227b710` 112 B reads 99.821 in `default/BandCharacter`. Decide by RTTI walk (`.?AVBandSong@@` → `??_R4` → vtable → the vtable store inside the ctor); if BandSong, rename + re-home block `0x8227A800–0x8227A948` to the unit whose base obj defines the ctor, judge the other two `Ham.cpp:` blocks (whole pin a DC3 scaffold?), delete the entry if it drains; prize +164 B ctor + `NewObject@BandSong` → 100. Else prove the fold on bytes before any alias. Splits: `Ham.cpp:` + one receiving heading only. | — |
| **W16-BI** (**landed** `82f4a634`, see table above) | opus | SPLITS + MAP lane (`UI.cpp:`/`BandUser.cpp:` headings only): `0x8268b9a0` (76 B, map `?NewObject@UIPanel@@`, `default/UI` row 68.368, pinned as a one-function `UI.cpp:` island inside `BandUser.cpp:`'s territory by `36f6b4df`) reads on retail bytes as `li r3,0x108; bl operator-new; li r4,1; bl fn_8268B4E8` = `BandUser::NewRemoteBandUser`, sole caller `0x8268485c` in `BandUserMgr::BandUserMgr(int,int)`; `0x8268b4e8` (316 B, unnamed) = `??0RemoteBandUser@@QAA@XZ`. Adjudicate on bytes (vtable store, class sizes 0x108/0x68 via the compiler), rename + name (spellings from COFF, caller census for the new name), delete the island and merge `BandUser.cpp:` to `0x8268AEC8–0x8268C920` (predict +1 fn / +76 B; UI row leaves the report; read the ctor row), then find retail's REAL `?NewObject@UIPanel@@` (`REGISTER_OBJ_FACTORY(UIPanel)` `src/system/ui/UI.cpp:964`) by relocation-normalised body scan and name/pin it if unpinned. | — |
| **W16-BJ** (**landed** `39872374`, see table above) | opus | SPLITS-only funclet-homing lane: (0) BG's prose says 9 mis-pinned funclets, its own §2 table shows 10 — reconcile; (1) fan-in adjudication of all 24 BG rows on retail FuncInfo tables (every referencing parent, not first-hit): HOMED / MIS-PINNED / ORPHAN, 14 in-unit rows as control, expectation stated first (rival hypothesis: `/OPT:ICF` folded byte-identical `__unwind$` bodies across TUs, so a funclet physically inside unit X was emitted by X and BG's cross-unit "parent" is a fold referent); (2) tree-wide census by the same classifier, reconciled to `masked_equal`-flagged rows and `total_*`, report only; (3) priced pilot re-home (`.text` lines only, `.pdata` re-derives, drain-last-block ⇒ delete entry) of only those adjudicated MIS-PINNED, set-diff vs a copy of `~/tmp/rows_w16bg_main.json`, `fn_827799AC` (CharLipSync 99.3, parent SongData) pre-registered; "0 to move" is a legitimate result. No map rows, no aliases, no `src/`; no `Ham.cpp:`/`UI.cpp:`/`BandUser.cpp:`/BH's receiving heading, nothing inside `0x8227A7A8–0x8227A948` or `0x8268AEC8–0x8268C920`. Deliverable `docs/decomp/W16BJ_FUNCLET_HOMING_FANIN_CENSUS_REHOME_2026-09-15.md` | — |
| **W16-BK** (**landed** `a966b9db`, see table above) | opus | SPLITS + MAP lane: re-home the `UIPanel` factory `0x82802418` (`?NewObject@UIPanel@@SAPAVObject@Hmx@@XZ`, 100 B, reads 0 in `default/UIColor` because it sits in the tail of `UIColor.cpp:`'s `0x82802080–0x828024B0` block, adjacent to `UI.cpp:`'s `0x828024B0–…`) into `UI.cpp:` — BI proved the name and `sizeof(UIPanel)==104` but was barred from the heading; census the block tail (fn_82802240 120 B @0, fn_828023A0 72 B @0, two 40-B rows @93.4) on retail bytes and pick the boundary; ⚠ `0x828023A0` is NOT simply `UIScreen::NewObject` — `0x823f5c20` already carries that name at 100, so adjudicate the twin's class from its relocations before naming (never two rows with one name); annotate BE's withdrawn `UIPanel` layout recommendation with a dated correction; report-only sweep for other `NewObject@*` rows pinned outside their class's TU. Predict +1 / +100 B literally. Bars: none of BJ's headings, no `Line.cpp:`/`BandCharacter.cpp:`, no alias, no src. | — |
| **W16-BL** (**landed** `8c830054`, see table above) | opus | SPLITS + MAP lane: re-home `.text 0x82289748–0x82289754` (12 B) from `Line.cpp:` — where CY-1 `f592571a` pinned it, NOT unpinned as BH §8 said — into `BandCharacter.cpp:` (it closes the gap `0x82289710–0x82289748 | 0x82289754–0x8228A23C` exactly) and name it `?ClassName@BandCharacter@@$4PPPPPPPM@A@BA?AVSymbol@@XZ` (BH proved `b 0x822896E0`; BH's own gate showed our obj pairing that adjustor at 100 before the BandSong re-attribution). Predict +1 / +12 B literally. Then adjudicate the `Ham*`-named map rows BH §6 flagged (HamMove ×4 in the map, not ×7; HamLabelCountDoneMsg, HamIKEffector, HamNavProvider, HamMasterLoader — all pair ≥97 today, so this is accuracy-only: rename only on decisive retail RTTI/relocation evidence AND an our-obj-defines check, else keep-with-reason). `0x8227A528` is already `?StaticClassName@BandFaceDeform@@` at 100 — closed. Bars: none of BJ's headings, no `UIColor.cpp:`/`UI.cpp:`, no alias, no src. | — |
| **W16-BM** (**landed** `4f7d819e`, see table above) | opus | SPLITS-ONLY lane: re-home the tree-wide **MIS-PINNED** EH funclets to their parents' units — BJ §3/§5.9 sized the class on the post-pilot tree at **708 rows / 28,248 B** (pre-pilot: 419 rows / 16,548 B of today's `matched_code` credited to a false twin in a unit that never emitted the funclet); BJ's 26-row pilot priced 8/9 edits exactly at +2 / +32 B with 5 rows gaining and 4 losing, so this is an **accuracy play** — a net drop is a WIN. Extend `tools/funclet_homing.py` with an asserted-partition `--emit-splits` mode (text-only edits, identical coverage before/after, last-block drains delete the entry, bar-list honoured, idempotent), pre-register per-row predictions from our COFF, land in two measured batches (true-twin-available first, false-twin-loss second), iterate splits to a `symbols.txt` fixed point, then report the ORPHAN→HOMED sweep by `auto_*` cluster. Bars: no `.text` line under `UI.cpp:`/`UIColor.cpp:` (BK) or `Line.cpp:`/`BandCharacter.cpp:` (BL) as source or destination; no map, alias, or src. | — |
| **W16-BN** (**landed** `8bdc70ba`, see table above) | opus | MAP + SPLITS (+ one ported TU) lane, BK §4's coupled filing: retire the FALSE `?NewObject@UIScreen@@` credit at `0x823f5c20` (72 B, reads 100 in `default/UI` because the obj defines the name and the bodies are twins — BK proved it constructs a `NetSearchResult`, spelling unproven: header says `static NetSearchResult *New()`), install the real one at `0x828023A0` (BI's reserved island row, no map key today), optionally `0x823f5a90` → `??0NetSearchResult@@QAA@XZ` (264 B, fuzzy 0); port `NetSearchResult.cpp` from the rb3-Wii oracle (52 lines; game layer) so the rows become PAIRABLE, then adjudicate `UI.cpp:`'s 4,712 B block `0x823F4A30–0x823F5C98` function by function (every row fuzzy 0 but the false credit; `NetSearchResult` rows cluster below it) and re-home the proven sub-spans to a new `network/net/NetSearchResult.cpp:` heading. Predicted ≈ net 0 map-only (−1 / −72 B false credit, +1 / +72 B if UIScreen pairs) plus whatever the port matches; accuracy play. Bars: `UI.cpp:` and the new heading are its only splits domain (BM is barred from `UI.cpp:`), no other heading as destination (file instead), no alias edits unless `--validate` forces the minimal repair, no `src/` outside `network/net/NetSearchResult.{cpp,h}`. | — |
| **W16-BO** (**landed** `d68ec0fe`, see table above) | opus | SOURCE + MAP lane, BK §6/§8's "factory-shaped rows nobody emits": ten `fuzzy == 0`, unmapped rows (`0x82677bd0` VocalGuidePitch 144 B, `0x82b67a00` ExternalMic 120 B, `0x825557e8` PrefabMgr 120 B, `0x82b82260` JsonArray 108 B, `0x8253abb8` MusicLibraryStore 96 B, `0x82b6a0e8` PitchShiftEffect 72 B, `0x823e3e90` VoiceDataMsg 72 B, `0x823e3c88` NewUserMsg 72 B, `0x825aaff0` LockResponseMsg 72 B, `0x82802240` UIColor::Load 120 B). Adjudicate each on retail bytes as EMISSION gap (emit the factory in the owning TU, port `JsonUtils.cpp` — `default/JsonUtils` is pinned but compiles nothing), NAMING gap (`NewUserMsg`/`VoiceDataMsg` already emitted by `NetSession.obj`, `LockResponseMsg` by `LockStepMgr.obj` — BK §6's "(none)" assumed the `SAPAVObject@Hmx@@` spelling), UNIT gap (file the splits line, never move it), or not-a-factory. ⛔ No `splits.txt` edits (BM/BN own those); map rows only for proven VAs; src only in the owning TUs; native gate last. Predicted +2..+6 fns / +150..+600 B. | — |
| **W16-BP** (**landed** `24fee53a`, see table above) | opus | The Ham vein (BL §10/§10.1). Retail contains ZERO `.?AVHam*@@` RTTI, so all 18 `Ham*.cpp:` headings (33,708 B pinned, ~10,288 B matched) are DC3 labels on RB3 code. (1) Label-pair known-answer fixture: delete `HamLabel.cpp:`, add its 120 B block under `BandLabel.cpp:`, map `0x82340580`/`0x82340940` to the `BandLabelCountDoneMsg` spellings in ONE commit (pre-registered Δ0). (2) Adjudicate the other 17 + `system/hamobj/MiniLeaderboardDisplay.cpp:` on retail bytes (strings, `??_R4`/TypeDescriptors, `__RTDynamicCast`, callers); re-home atomically only where the destination base obj defines the spelling (retail names BandCharacter/BandDirector/BandIKEffector/BandCamShot), FILE as NEEDS_SOURCE where retail names a class we lack. (3) optional: BM §5.6 five non-UI survivors, BK §6 two Gesture rows, register `mutate_assertions.py`. Bars: BN/BO surfaces, no alias edits. Predicted Δ0 on the fixture; net anywhere from −1 kB to +1 kB on the adjudication, accuracy first. | — |
| **W16-BQ** (**landed** `0b8c74e6`, see table above) | opus | VocalTrackDir/GemTrackDir accuracy lane. Retail factory callers in BandCharacter.s pass `li r3,<sizeof>; bl MemAlloc; li r4,1; bl X` (MSVC most-derived flag): `fn_8227BCC0` allocates 0x7f0 = sizeof(GemTrackDir) and calls `fn_822ECC48`, `fn_8227BEA0` allocates 0x770 = sizeof(VocalTrackDir) and calls `fn_822FC508` ⇒ `0x822ECC48` is `??0GemTrackDir@@QAA@XZ`, yet its block `0x822ECC48–0x822EE498` is pinned under `VocalTrackDir.cpp:` (SongLayout abuts below, GemTrackDir above — geometry leg imperfect, sizeof leg primary). Tasks: census + re-home the block with the BP §2 two-leg instrument (predict DEF-leg pairability in GemTrackDir.obj first), name both unnamed factories `0x8227bcc0`/`0x8227bea0` and the ctor addresses against base-obj spellings, explain the ctor bloat (ours 6,312 / 4,880 B vs retail 3,428 / 2,548 B), correct the MISPIN doc's string→function misattribution (the `.grp` strings live in `0x822F8FF0–0x822FA1D0`, not at `0x822ECC48`), then the VocalTrackDir residue (PostLoad 96.39, `fn_822EF438`, ApplyFontStyle, `fn_822FA7E0`, SetRange, SetConfiguration). Deliverable `docs/decomp/W16BQ_GEMTRACKDIR_CTOR_REHOME_2026-09-15.md`. | — |
| **W16-BR** (**landed** `5c4a5f73`, see table above) | opus | The `0x82B8` tour/dsp band (BN §6.1 items 2/3). Two `UI.cpp:` blocks sit inside it: `0x82B801D8–0x82B80320` is TourChar's (`0x82b801d8` calls `??1TourChar` ⇒ the map's `??_GLocalePanel` name is wrong, it is `??_GTourChar`; `fn_82B80238` is `?Handle@TourChar@@`), and `0x82B8032C–0x82B80FC4` is `TourCharRemote.cpp` (SyncLoad/GetTexAtPatchIndex/Handle/ctor/dtor; `0x82b80768` map-named `??_GChooseProfilePanel` is `??_GTourCharRemote`) followed by HMX `system/dsp/PitchDetector.cpp` (`fn_82B80810` 1,780 B references `PD_FLOOR_*`/`tic_vocals`, calls `ShiftedDotProduct`/`FindCCPeak`/`RefinePeriod2`/`Time2IirA`). The gap block `auto_03_82B80FC4` holds PitchDetector's ctor/SetSampleRate/dtor AND `?Detect@VibratoDetector@@` (376 B, outside its own pin), and VibratoDetector's pin `0x82B81400–0x82B816F0` contains IIR4PoleFilter code after `Analyze` (`0x82B81538–0x82B816F0` = `system/dsp/IIRFilter.cpp`). Tasks: re-home block 1 into TourChar + fix both wrong `??_G` names; port TourCharRemote.cpp / PitchDetector.cpp / IIRFilter.cpp from Wii and pin them (path-qualified — a bare vendor `PitchDetector.cpp:` heading exists); re-bound VibratoDetector; prove the `GetFret@RGState`/`Chunk32@HxGuid` fold behind AddCustomSettings (784 B, 4 `diff_arg`, relocation-free 12 B survivor) T1 on retail bytes. Excludes BO §5 rows 7/9 and MessageBroker (BQ's). | — |
| **W16-BS** (**landed** `c207d1ae`, see table above) | opus | The Tour-cluster relocation-name residue: five `diff_arg` sites (all instructions equal) withholding 8,332 B across `Tour::Handle` (3,492 B, 99.989), `ChordShapeGenerator::SyncProperty` (2,644 B, 99.992) and `TourPerformerImpl::Handle` (2,196 B, 99.982). Two suspected DC3-transfer wrong map names whose current rows score 0.0 (`??0Tour@@` @`0x8235bbf8` — a Handle never calls its own ctor, the handler is `InitializeTour`; `?SetDancer@AppLabel@@` @`0x82360fb0` — no such method exists in src, the site passes a UILabel+BandUser pair), two fold-vs-wrong-name questions (`GetConclusionText`/`GetTourGigGuideMap` @`0x8235cd38`, `GetCurrentQuestSuccessMessage`/`DisplayName` @`0x823609f8`), one alias proof (`Tour::SyncProperty` vs `Hmx::Object::SyncProperty` @`0x8235c2e0`). Adjudicate each on retail `bl`/string bytes, land wrong-name repairs as one map edit, T1-prove or refuse aliases, close the InitializeTour 240-vs-284 B source gap. Surface disjoint from BQ (`0x822E/F`) and BR (`0x82B7F–0x82B82`). | — |
| **W16-BT** (**landed** `99d8d70a`, see table above) | opus | The `MemAlloc` macro swallows alignment: `#define MemAlloc(size, file, line, name, ...)` expands a 2-arg `MemAlloc(n, 0x10)` to `(MemAlloc)((n), 0)` — silent, compiling, plausible-looking wrong calls (BR fixed 3 in PitchDetector and filed the hazard). Census every retail `bl 0x827bcd38` site's `r4` against our spelling, fix DIVERGE sites, harden the header (`__VA_ARGS__` dispatcher proven by `cl /E`, 2-arg spelling made a compile error, grep guard in `test_tools.py`), then the dsp residue BR left (`AnalyzeBlock` 1,780 B @ 85.72, `VibratoDetector::Detect`, `IIR4PoleFilter` ctor, `ShiftedDotProduct`). | — |
| **W16-BU** (**landed** `79ca02bd`, see table above) | opus | Tour-cluster residue after BS (its §5): `??0Tour@@` at 0.0 whose retail row sits in unit `default/BandSongMetadata` (adjudicate on retail bytes before any re-home), the four source rows (`InitializeTour` 284 B @ 83.31, `UpdateTourPlayerContributionLabel` `SetTokenFmt<char*>`, `GetConclusionText`, `GetCurrentQuestDisplayName`'s extra null check), the `0x8258bab8` BandProfile `bool Has*(Symbol)` decode against the compiler layout (rename only if our obj defines the name), and the `0x8235c2e0` = `Hmx::Object::SyncProperty` naming A/B incl. the Tour vtable slot + T1 alias. | — |
| **W16-BW** (**landed** `3b758bef`, see table above) | opus | dsp residue after BT (its §7–§8): `??0IIR4PoleFilter@@` 312 B @ 10.19 — xref width sweep over every retail load/store at `+0x60..+0xE0` off `IIR4PoleFilter*` (128 B = 8×16 VMX128 scratch shape; if only `lvx`/`stvx` width, the ctor legitimately never inits it and the row is NOT layout-blocked), `?Detect@VibratoDetector@@` 376 B @ 20.86 from-scratch reconstruction against the retail extent (loop unrolling, `% 5` vs rotating pointer), `?AnalyzeBlock@PitchDetector@@` 1,780 B @ 85.86 integer cluster idx 94–111 first (f28/f29 swap is NOT a decl-order lever), optional `Time2IirA` anon-ns TU identification (no `noinline`). `ShiftedDotProduct` VMX128 wall explicitly OUT (Fable candidate). | — |
| **W16-BX** (**landed** `20558666`, see table above) | opus | VocalTrackDir/`ObjPtr` residue after BV (its §5–§8.5, §10): the `ObjPtr<T>`+8 signed-compare question tree-wide (retail `cmpwi` vs our `cmplwi` at `0x534(r31)` = `mTubeRangeGrp`+8 — census across classes with compiler-verified layouts, with a `cmplwi`-on-plain-`T*` null; DC3/rb3-Wii `ObjPtr` comparison; PCH-cascading `Object.h` edit priced only via `ab_measure`, Δfns=0 accepted on merit), `?PostLoad@VocalTrackDir@@` 3,656 B @ 96.39 `stack-layout` naming of the swapped 0xa8/0xb8 slots, `SetConfiguration`'s three identifications (BV §6), the four unnamed rows via BV §5 (name only if our obj defines a real body), and the fold-thunk lever re-run through `tools/fold_thunk_gate.py --subclass fold_thunk_naming` — install ONLY gate-ADMITted groups via the tool's own emit path, per-pair FT verdicts verbatim, ALIAS_SUSPECT expected by construction on a map-only patch. | — |
| **W16-BY** (**landed** `31576aa3`, see table above) | opus | BandWardrobe residue (`src/system/bandobj/`, 270 fns / 236 matched / 21,188 of 33,608 B): `FindBestScoringHint` 828 B @55.96 (ours carries a `static Symbol done` the Wii oracle lacks), `ValidGenreGender` 372 B @50.28 (source-identical to oracle ⇒ header/inlined-callee divergence), `OnEnterVignette` 1,692 B @89.4, identification of 12 anonymous 0% rows (~2,100 B, `fn_8232B7C0` 504 B refs `"female"`) against the unpaired unit-owned symbols, then near-crossers priced from `report.json` charged sites. ⛔ The SyncVignetteInterest/SyncEnableBlinks `mCurNames` CSE shape is DRAINED (BODYPORT-3, permuter-class) — not re-funded. Out of scope: `Object.h`/ObjPtr headers (BX), system/dsp (BW), Tour/BandProfile/VocalTrackDir/GemTrackDir. |
| **W16-BZ** (**landed** `e0a87c15`, see table above) | fable → opus | GemTrackDir/VocalTrackDir LARGE-ROW band — the escalation BX §9 priced and left: `??0GemTrackDir@@QAA@XZ` 2,548 B @79.27 (never attempted); `?PostLoad@VocalTrackDir@@` 3,656 B @96.39 (BX §4: hoisting `streakPtr` regressed to 93.74 — the untried arrangement is a nested scope / displacing local that reorders the `cols`/`streakPtr` slots WITHOUT moving the construction site, then the `WRONG_CALLEE`/`TEMPLATE_INSTANTIATION_MISMATCH` sites, then the SDA `subi`/`addi` pair); `?PreLoad@GemTrackDir@@` 1,532 B @91.6; `??1GemTrackDir@@` 728 B @86.46 (named by BX §5; our body 772 B vs retail 728 — a 44 B surplus); `??0VocalTrackDir@@QAA@XZ` 3,428 B @70.96; SetPitch/SetPlayerLocal/SetRange/ApplyFontStyle only with budget left. Out of scope: `Object.h`/ObjPtr headers (BX refuted the layout claim), BandWardrobe (BY), system/dsp (BW), the anonymous 40/44/48 B reloc-name-only stratum, the FT3-only/FT-EMPTY alias pairs BX flagged. | — |
| **W16-CA** (**landed** `e1b9ddb0`, see table above) | opus | The meta_band STORE band in `src/band3/meta_band/`: `?Poll@BandStorePanel@@` 980 B @45.04 — its frame is 0x50 BIGGER than retail (0x140 vs 0xf0, w23-frame-queue) with 210/215 instructions charged, i.e. a frame-shape defect, and the body carries every ingredient of the SOLVED `docs/plans/slm-setstate-reconstruction.md` phantom-frame pattern (function-local static `Message`, two `MakeString("%d", StoreBuildNum())` compare sites, a `TheDebug.Notify`-vs-`MILO_NOTIFY` form question, an apparently unused `String path`); then `?Handle@BandStorePanel@@` 1,928 B @93.12 (charges {opcode 5, relocname 2, register 8, branchdest 1, immediate 2} — check the 2 relocname sites for fold-alias before pricing the row), `SyncProperty` 120 B @68.33, `IsLoaded`, `MakeNewOffer`; `?Poll@UGCPurchasePanel@@` 1,104 B @87.69 (w23 COLLECTABLE); and the ~3.3 kB of anonymous 0 % rows plus three 0 % `sort<unsigned __int64>` template rows in BandStorePanel — byte geometry BEFORE naming anything (W16-BY §7: those "anon rows" were a dtk mis-carve). Stretch: `BuildList@StoreOfferProvider` 2,536 B @96.04. ⚠ The EC3 census `STUB_OURS_EMPTY` labels for `UpdateOffers@BandStorePanel` and `Exit@UGCPurchasePanel` are STALE — both matched at `cfb1ef85` / `48951737`. Out of scope: `src/system/meta/StorePanel.*` and engine headers, `Object.h`/ObjPtr, system/dsp (BW), GemTrackDir/VocalTrackDir (BZ). | — |
| **W16-CB** (**landed** `30f7070f`, see table above) | opus | `src/band3/game/Game.cpp` (356 rows, 36 sub-100 / 16,200 B): (1) `?Handle@Game@@` **5,428 B @99.9963** — lane CB-5 `5524a135` took this row to 100 on the OLD `none` ruler, so what survives the 2026-08-12 `name_check` flip is essentially ONE relocation-name charge on the largest size-if-it-crosses in the game layer; an exact-name search of `symbol_aliases.json` finds ZERO groups containing it, so it is NOT already forgiven — adjudicate wrong-map-name (PAYS, cf. MAPDEF-3 / MPNGAP-1's `Handle@GemPlayer`) vs genuine ICF fold (needs a PROVEN alias via `fold_thunk_gate`/`comdat_fold_gate`; an unproven one lifts `name_check` BY CONSTRUCTION). (2) ~4.6 kB of UNNAMED real `Game::` bodies sitting in map GAPS between named methods — `fn_8267B808` 1,664, `fn_8267BF30` 1,504 (both between `DropUser` and `IsLoaded`), `fn_8267AA48` 972, `fn_8267B670` 396, `fn_8235F858` 308 (⚠ a `0x8235xxxx` address in this unit — check its homing first), `fn_8267B000` 296, `fn_8267A4C0` 232, `fn_8267AE58` 200 — rb3-Wii names them; byte geometry BEFORE naming, DEF leg from the COFF table after a full build. (3) two census SOURCE_INSDEL defects: `?LoadSong@Game@@` 408 B @54.49 / 60 charges and `??1Game@@` 792 B @99.308 / 7 charges; plus `??0GemTrainerLoopPanel@@` 140 B @76.03. The 99.9x stratum (`Poll` 1,036 B, `PostLoad`, `Reset`, `OnSetShuttle`) is priced but NOT funded as a byte lever — ~91 % of it is irreducible fold/map noise. Out of scope: `src/system/**` headers, `Object.h`/ObjPtr, meta_band store (CA), GemTrackDir/VocalTrackDir (BZ), the Quazal 7-line scaffolds. | — |
| **W16-CC** (**landed** `b6a18175`, see table above) | opus | `band3/bandtrack/Gem` — a **three-source merged TU** (`Gem.cpp` `#include`s `bandobj/OutfitConfig.cpp` and `band3/bandtrack/GemRepTemplate.cpp`, so one object covers four pinned address bands). 222 rows / 30,264 B / 45.53 %, gap **16,484 B**, and it has **no mid-band grind**: 44 rows / **8,524 B** at `99.0 ≤ fuzzy < 100`, 28 anonymous rows / **7,676 B** at `fuzzy == 0`, 3 named 0 % rows (244 B), one row in between. Measured for the lane before dispatch: `?SyncProperty@OutfitConfig@@` (1,304 B, 99.985) is **326 instructions with exactly ONE charged site, a `diff_arg`** — retail calls `PropSync<ObjVector<TransformArea>>` where we emit `PropSync<ObjVector<MatSwap>>`, which is bit-for-bit what an ICF fold AND a wrong callee both look like, so it is adjudicated on retail bytes via `comdat_fold_gate.py` at T1 and **never by installing an alias** (an unproven alias lifts `name_check` by construction and a `none` control cannot catch it). A FAILED proof is worth more than the bytes — it would mean our `OutfitConfig` member is the wrong container type, a defect `mpn` reads as 100 before and after. The `PropSync<…>` family recurs five times in the stratum. Second task: the 28 unnamed bodies, where naming pays in **bug exposure, not bytes** (placeholder targets are already forgiven) and a name is only installed when our object defines it with a real body. Out of scope: permuter, `PartialHit` (mpn already 100), and the three files the live lanes own. | — |
| **W16-CD** (**landed** `a26bd090`, see table above) | opus | Continuation of W16-CA in the meta_band STORE band — the lane is told to read `docs/decomp/W16CA_BANDSTOREPANEL_POLL_HANDLE_UGCPURCHASE_ANONROWS_2026-09-15.md` **first** and not re-derive it. (1) **ONE unimplemented override**, verified against the tree before dispatch: `StorePanel.h:94-95` declares `EnumerateSubsetOfOfferIDs()` and `GetOfferIDsToEnumerate()` (empty default), `StorePanel.cpp:443-445` calls the second when the first is true, and `BandStorePanel.h:65` answers **true** while never overriding the second. `fn_82608B70` (452 B @0) is that override on its callee set (`__RTDynamicCast`, `StorePurchaseable::Exists`, `push_back<vector<u64>>`, `sort<u64>`, `adjacent_find`); implementing it instantiates the 64-bit-key family whose three **named** rows our object does not define at all — `__introsort_loop` 188 B, `__partial_sort` 152 B, `sort<PA_K>` 132 B = 472 B. ⚠ W16-CA's **≈2,016 B is an ESTIMATE, not a measurement** — it never implemented the function; the lane is told to price it itself and report if it comes to less. (2) 16 anonymous rows / **3,000 B** in BandStorePanel, largest `fn_82606280` at 908 B and unexamined. (3) `StoreOfferProvider` — W16-CA's task 5, **never started**: 93 rows / 11,076 B / 53.63 %, gap **5,136 B**, `BuildList` 2,536 B @96.04 and `Handle` 1,556 B @99.846. (4) The rest of CA's NOT-DONE list with its overturning evidence: `sizeof(XboxPurchaser)` (+1,104 B, sole charge, blocked on the shared header `src/system/meta/StorePurchaser.h`), `GetIndexFile()` (read `0x82c73fdc` through the **`.data`** delta — the `.text` formula yields garbage), `Handle` idx 94–98 (blocked on `ObjMacros.h`), and the unmeasured `StorePanel.cpp:302` split-allocation-temp defect. ⛔ The lane is explicitly told to edit the symbol map **in place** — W16-CA re-serialized it sorted by address and that had to be reverted before its merge. Out of scope: permuter, the three files W16-CC owns, splits/pin edits. | — |
| **W16-CE** (**landed** `7d0d960a`, see table above) | opus | **W16-CC's explicit hand-off: port `NowBar.cpp`, the largest never-written file in the Gem unit.** Verified against the tree before dispatch: `src/band3/bandtrack/NowBar.h` exists (29 lines, 13 declared members, 360-corrected offsets — `mCurrentGem` 0xc where Wii has 0x8, STLport's `vector` being 12 B) while **`NowBar.cpp` does not exist at all** — 0 hits in `objects.json`, no heading in `splits.txt`. Retail's NowBar code is nevertheless **already pinned**, inside `band3/bandtrack/Gem.cpp`'s `.text start:0x82BAA400 end:0x82BAC148`: the sub-band `0x82BAA400`–`0x82BAAF30` holds **17 rows / 2,796 B**, of which 3 are already at fuzzy 100 (112 B) and one at 93.4, leaving **13 rows / 2,644 B at fuzzy 0 against 13 declared members** — **16.0 % of the unit's 16,484 B gap**. ★ **The implementation question is settled before dispatch, and the answer is the cheaper one**: because the band is *already inside Gem's splits entry*, a scatter-`#include` into `Gem.cpp` needs **no `splits.txt` edit whatsoever** and follows two existing in-file precedents (`bandobj/OutfitConfig.cpp` line 617, `band3/bandtrack/GemRepTemplate.cpp` line 643); carving a separate `NowBar.cpp:` heading would be a **re-home**, which is **NOT metric-neutral** (measured +3 fns / +428 B, lane PINHOME-1 `8e6eb9be`) — the rb3-Wii oracle giving NowBar its own TU is **not** a reason to move a pin here. The lane is also told **not** to add the file to `objects.json` (scatter-include only; a double-emitted file is the `mtx.cpp` 17-duplicate-definition incident). ★ **Identification is pre-seeded, not left to the lane**: every call target in the band was extracted from `Gem.s` keyed on the **`.fn` symbol, never the synthetic address column**, yielding 13 hypotheses with their evidence — `0x82BAAC98` (340 B) is the **ctor** on `SystemConfig`/`ObjectDir::Find<RndDir>`/`GemSmasher::GemSmasher(int,RndDir*,bool)`, `0x82BAA8A8` (652 B) is **`Hit`** on `SongDB::GetGem`/`LeftHandSlide`/`CodaBurnChord`, `0x82BAAB38` (340 B) is **`PartialHit`**, `0x82BAA510` (300 B) is **`Miss`**, `0x82BAA7F8` is **`StopBurning`** on `GemSmasher::StopBurn`, `0x82BAA490` is **`Reset`**, `0x82BAAEA8` is the **dtor** and `0x82BAAE40` the **`DeleteAll<GemSmasher*>` template COMDAT**, not a member. ⚠ The lane is warned that **address order is NOT source order** (`FillHit` is 8th in the oracle but 6th by address), so names must be adjudicated per-body, and that five call targets in the band are **ICF fold survivors that look like defects** (`push_back<vector<ChatReceiver*>>` is really `<GemSmasher*>`, `??2CriticalSection@@` is `operator new`, `??3BinStream@@` is `operator delete`). ★ **One structural question answered pre-dispatch so the lane cannot get it wrong**: `0x82BAAEA8` has the shape of a `??_G` scalar deleting destructor, which would imply virtuals our header omits — but with `/GR` on, a `??_R0` type-name string is present for every polymorphic class, and on the **decompressed `band.exe`** `.?AVTrackDir@@` and `.?AVRndDir@@` HIT while `.?AVNowBar@@`, `.?AVGemSmasher@@` and `.?AVGem@@` are **absent** ⇒ **NowBar has no vtable**. ⚠ That probe run against the packed `default.xex` returns 0 hits for **every** class *including the controls* — a vacuity shaped exactly like a decisive negative, which is why the controls were run at all. Out of scope: the Gem near-miss stratum (W16-CC measured it **entirely argument-only** — 2,074 of 2,131 instructions equal, 57 `diff_arg`, zero insert/delete/replace — and it is drained), permuter, splits re-homes, and the files W16-CD owns. | — |
| **W16-CF** (**landed** `72919046`, see table above) | opus | `?SetState@SaveLoadManager@@IAAXW4State@1@@Z` — **4,096 B at fuzzy 96.740**, the largest single game-layer row with real codegen work in it (54 % of the unit's 7,612 B gap). ★ **Routed to Fable because the SHAPE is hard, not because the row is big**, and the shape was settled before dispatch by the **in-tree record, not by me**: lane **W1-GAME** left a 30-line measured block at `SaveLoadManager.cpp:703-733` concluding **13 INDEPENDENT body divergences, not one** — 25 insert/delete in **13 separate clusters** (idx 516…968), 4 real replaces at 4 distinct sites, 1 `diff_op`, and 109 `diff_arg` with **zero unexplained**. ⚠ **I had this priced as "the largest single-fix prize on the board" and the file refuted me**: there is no single defect for a cascade to be downstream of, and because `matched_code` is **all-or-nothing per row**, closing 12 of 13 buys **exactly ZERO bytes**. The lane is therefore **explicitly authorised to land at Δ0** and told to bank proved fixes rather than fabricate a crossing — accuracy over headline %. ★ **Three defects handed over located, two inherited and one new**: (1) idx 580/639, retail loads a static `lis r10, lbl_82C72830@h` where we `bl ?Localize@@YAPBDVSymbol@@PA_N@Z` — **retail does not call `Localize` at those sites at all** (distinct from the argument-EVALUATION-ORDER notes at lines 921-922/1074/1118, which the lane is told not to conflate); (2) idx 798-799, retail dispatches off ONE unsigned compare `lwz r11,0x1c(r30); cmplwi cr6,r11,0x1; blt/bne` — a **switch on `mMode` (0x1c) with cases 0 and 1** — where we emit a signed `cmpwi …,0x0; beq` if/else chain that re-compares; (3) **NEW, found while pricing the lane and in no prior record** — idx 940, retail `mtctr r10` (indirect call through `ctr`) where we `add r5, r11, r30`, almost certainly the cause of cluster 12 (idx 936-938) and the only lead no lane has touched. ⛔ The **105 register swaps across 28 distinct pairs** (dominant pair only 29 %, spanning idx 6→1022) are declared a **SYMPTOM, not work** — permuter is off by directive and swaps dissolve when the real defect is fixed — as is the `+8` dominant stack-offset shift. ⚠ **A briefed header defect was REFUTED pre-dispatch**: CLAUDE.md cited `SaveLoadManager.h` as "uniformly +4 stale", but `class_layout_report.py --check-header` reports **all `// 0xHEX` comments agree with the compiler**, as it does for the paragraph's other example `CharEyes.h` (was "20 wrong offsets") — both were repaired at some point and the note was never updated, so the doc was corrected in `d7881047` and the lane is told not to hunt a defect that does not exist. Also excluded after checking: the unit's 21 anonymous zero rows / 2,976 B carry **no base counterpart**, so they are **unpaired, not merely unnamed**, and naming them buys 0 %-rows with no content. Out of scope: permuter, splits edits, and the files W16-CD and W16-CE own. | — |
| **W16-CG** (**landed** `edd6b216`, see table above) | opus | `?Handle@CustomizePanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z` — **5,036 B behind exactly ONE charged site**, `[530] delete: clrlwi r11,r11,24`. Six consecutive lanes have tried source spellings; the in-tree record at `src/band3/meta_band/CustomizePanel.cpp:283-565` is their combined deliverable and it is a **PRICED REFUSAL**, not a mispricing — it also corrects the stale RESIDUAL-1 framing that four briefs kept re-issuing (the row is 5,036 B behind ONE instruction and closing it buys **+1 function AND +5,036 bytes**, not "+1 function and zero bytes"). The record explicitly forbids a seventh spelling lane and names the single untried channel: compiler/codegen (a different 10224 QFE, or a pragma affecting bool canonicalization). **Spelling attempts are hard-banned for this lane.** Mandatory first move is the record's own ProbeX transplant falsification test — a 20-second check whose burden is to move the transplant. Retail site `0x82619774`-`0x82619788`: `bl fn_82575670` (HasLicense → bool in r3), `clrlwi r11,r3,24`, then `.L_82619778` where `has_patch` jumps in with `FindPatchIndex()+1` in r11, `subic`/`stw`/`subfe`, and the missing `clrlwi r11,r11,24`. **Δ0 authorised** — if ProbeX refutes the channel, that verdict closes the vein and is the deliverable. | — |
| **W16-CH** (**landed** `8275d062`, see table above) | opus | `?Handle@Rnd@@UAA?AVDataNode@@PAVDataArray@@_N@Z` — **6,416 B, the largest single prize on the board**, behind one charged relocation-NAME site at `0x826c3888`: our side spells `??$?0H@?$StlNodeAlloc@V?$_List_node@H@stlpmtx_std@@@stlpmtx_std@@QAA@ABV?$StlNodeAlloc@H@1@@Z`, the target names `?TakeShot@HiResScreen@@QAAXPBDH@Z`. Flagged `SYMPAIR_ONLY_1 / FOLD_FANIN` in `docs/decomp/sympair-queue.tsv:14` and `DIFF_SIZE REVIEW TB bl` in `docs/decomp/relocname-genuine-worklist-WS4.tsv:2986`. **Fold-vs-wrong-callee adjudication; either verdict collects the 6,416 B.** Two caveats are load-bearing: `DIFF_SIZE` is precisely the flag lane STLPORT-1 proved can be an artifact of **our own COMDAT reader** billing the successor symbol's EH-funclet prefix into the span (a size test cannot catch it — the artifact cancels on both sides), and an unproven alias lifts `name_check` **by construction** while the `none` control reads +0 there by construction, so that flatness is the signature of the hazard and not a clearance. Primary instrument `fold_fanin_probe.py`; only relocation-normalised retail-byte identity with target NAMES compared settles it. **Δ0 authorised** — if the fold cannot be proven, bank the negative and do not install. | — |
| **W16-CI** (**landed** `9fae75f0`, see table above) | opus | Finish the `default/band3/meta_band/BandStorePanel` unit — 7,820 / 13,144 B = 59.494827 %, **gap 5,324 B**, concentrated not diffuse: `Handle` 1,928 B @98.008, `Poll` 980 B @90.220, **six anonymous rows at fuzzy 0 totalling 1,364 B** (`fn_82606280` 908, `fn_82605878` 104, `fn_82605B48` 100, `fn_82608D38` 88, `fn_826068A8` 84, `fn_82605720` 80), `Request` 524 B @99.962. **Identification-led, per the lesson W16-CD proved on this very unit with a pre-registered null**: objdiff pairs by NAME, so source quality is invisible while the retail row is anonymous — CD implemented two bodies with no map entries and measured **exactly 0 B**, and of its 2,108 B prize 472 B was source and **1,636 B was identification**. Pre-seeded: `fn_82606280`'s 44 calls resolve to 15 distinct callees of which **13 are already named** (Symbol ctor ×7, `String::operator=` ×6, `DataArray::FindArray` ×6, `DataNode::Int`/`Str` ×4 each, `Localize`, `substr`, `DataArray::Release`) — a `.dta` parser, so the row is an identification problem and NOT a callee-ID one. ⚠ **W16-CD's hand-off claim that `0x82c73fdc` (`/dlc_top_%s_%s.dta`) is coupled to `fn_82606280` does NOT survive check** — the string exists exactly once in `band.exe` at file offset `0xbf520`, but that function's own data references do not include `lbl_82C73FDC` (its most-referenced label is `lbl_82C71838`, 5×); the coupling was narration, not evidence, and the lane is told to re-derive or refute it. Unpaired methods to match against the zero rows: `GetIndexFile`, `StoreUser`, `FindOffer`, `GetLoneOffer`, `SortName`, `ShortcutTextAtData`, `ApplyShortcutProvider`, two `OnMsg` overloads — ⚠ but `fn_82606280` at 908 B is larger than any of them plausibly is, so it may be 360-only and absent from the Wii oracle, which **diverges on this class** (`GetIndexFile` is `TheStoreMetadata.mVersion->mBuildNumber` on Wii vs `StoreBuildNum()` here). **Δ0 per row authorised; rank by size-if-it-crosses and hand off the rest with evidence.** | — |
| **W16-CJ** (**landed** `84577bdc`, see table above) | opus | **The empty-survivor fold vein at `0x826c3888` — W16-CH proved ONE fold into it for +6,416 B; this lane runs the same method across the rest.** Retail there is a bare `blr` with zero relocations, the universal empty-function ICF survivor with **1,116 direct callers**. Priced from `sympair-queue.tsv` at `cead857c`: **132 rows point at that address, 101 of them mm==1 for 24,632 B**, of which 6,872 B / 4 callees are already aliased (including CH's TakeShot) leaving **17,760 B across 68 callees open**. Concentration is favourable — `??1GameGem@@QAA@XZ` alone is 11 caller rows / 2,068 B, and a large `get_allocator@?$vector<…>` family follows — so the work unit is the CALLEE, not the row: one emptiness proof unlocks every caller of that callee. ⛔ **The 31 mm>1 rows pay ZERO** and are excluded from the 17,760 B; `matched_code` is all-or-nothing per row, so `?RGGetChordName@@` (1,652 B, **mm=4**) is not a lead despite its size. The lane's governing rule is CH's: **prove our COMPILED body is a bare `blr` BEFORE aliasing** — an unproven alias lifts `name_check` by construction and a `none` control cannot catch a fabricated one, so a not-empty body means either a real divergence (worth more than the bytes) or not this fold. My own triage of four callees returned **three different verdicts**, including `?Init@MidiInstrumentMgr@@QAAXXZ` which has **no definition anywhere in the tree** — absent is not empty — so batch installation is banned. Secondary: regenerate `sympair-queue.tsv`, which advertises three already-folded spellings as open work and carries no column-header line. | — |
| **W16-CK** (**landed** `b86c340c`, see table above) | opus | Finish `default/band3/meta_band/BandStorePanel` — 8,712 / 13,144 B = **66.2812 %**, gap **4,432 B over 11 sub-100 rows**, of which **only 4,344 B is reachable**: W16-CI proved `fn_82608D38` (88 B) is **not ours at all** (both callers are `CalibrationPanel`). **87.8 % of the reachable gap is three rows** — `Handle` 1,928 B @98.008, `Poll` 980 B @90.220, `OnMsg` 908 B @28.947 — plus a 264 B `MetadataLoadedMsg` ctor @94.848, two small rows, and a **160 B arg-only stratum** (four 40 B rows already at `mpn` **100** with fuzzy 99.5, i.e. relocation-NAME charges only — a wrong callee OR an unproven fold, not closable by editing instructions). ⚠ **No `W16CI_*.md` was ever written** — CI's retail callee censuses and both negative results live in its commit bodies (`git log 29664c3b ^9ced11e2`) and in roadmap row 2345, and the lane is told this outright rather than being sent after a doc that does not exist. **Known-CLOSED, do not reopen:** `GetOfferIDsToEnumerate` **is implemented** (`BandStorePanel.cpp:155`) and the entire `sort<unsigned __int64>` family reads **100.0** (`__insertion_sort` 80 B, `__final_insertion_sort` 96 B, `__partial_sort` 152 B, `__introsort_loop` 188 B, `sort<PA_K>` 132 B), so W16-CD's *≈2,016 B behind one member function* lead is **spent** — it was an ESTIMATE, never a measurement; and `0x826067C0` is `OnMsg(LocalUserLeftMsg&)` **mis-pinned to `Mat.cpp`**, whose repair needs a splits re-home and is ⛔ **not metric-neutral** (PINHOME-1), so it is out of scope. Method is W16-CI's own refinement, measured on this very unit: **first-naming pays BYTES only when our body is already good enough to cross** — so `OnMsg`, now paired at fuzzy 28.95, is **source** work rather than identification work. Pre-register every prediction before measuring it (all four of CI's waves were, and all four held). Δ0 per row authorised — accuracy beats headline %. | — |
| **W16-CL** (**landed** `fd62ddd8`, see table above) | opus | `default/MusicLibrary` — 31,644 / 44,148 B = **71.6771 %**, gap **12,504 B over 37 sub-100 rows**, and it is a NEAR-MISS unit rather than a port: `?Handle@MusicLibrary@@UAA?AVDataNode@@PAVDataArray@@_N@Z` is **6,160 B at fuzzy 99.971 / mpn 99.997** — **49 % of the whole unit gap in one row**, and the best size-if-it-crosses shape currently on the board. Behind it: `Text` 1,292 B @68.248 (the only genuinely broken large body), `OnExit` 748 B @99.973, six anonymous fuzzy-0 rows totalling ~1,220 B (`fn_825400E8` 288, `fn_82540FC0` 256, `fn_8253D690` 244, `fn_82540790` 240, `fn_8253DBD8` 228, `fn_82540DE0` 204), `PushSetlistToScreen` 216 B @81.759. ★ Two rows sit at **`mpn` == 100 with fuzzy < 100** — `SkipToNextShortcut` 396 B @99.040 and `GetSongFilterAsString` 200 B @99.800 — i.e. **arg-only, relocation-NAME charges**: a wrong callee OR an ICF fold-alias, adjudicated on retail bytes and **never** by installing an alias (an unproven one lifts `name_check` by construction and the `none` control reads +0 there by construction). The lane is told to price `Handle` from `report.json`'s **charged-site list** before investing — not from a mismatch count and not from an "N/N instructions equal" reading, both of which are instruction-level and structurally blind to `diff_arg`; and that `matched_code` is all-or-nothing per row, so the 6,160 B pays nothing until the last INDEPENDENT charge closes (with W16-CJ's measured qualifier that several charges sharing ONE cause do cross together). `docs/decomp/playbooks/nearmiss-harvest.md` is the live, on-point playbook (sub-99.9 is mostly **wrong source shape**, not compiler noise; 60–75 % hit rate), while the 99 %-band fine-regalloc residue is a compiler fixed point and a KILL. Prior work not to re-derive: `a5af58e0` (W16-BO's five missing factories), `9f508b13` (FilterType enum misnumbered, +580 B), `57c92168` (ContentMounted → 100 %). ⛔ A permuter sweep shard on this file was landed and then REVERTED (`cd42a2d3` / `21daa110`) — permuter is banned by directive, do not resurrect it. Δ0 with evidence is publishable. Out of scope: `BandStorePanel.*` (W16-CK) and `VocalTrack.*` (W16-CM). | — |
| **W16-CM** (**landed** `db5538cf`, see table above) | opus | `default/VocalTrack` (`src/band3/bandtrack/VocalTrack.cpp`, 2,778 lines) — 19,928 / 44,372 B = **44.9112 %**, **gap 24,444 B over 72 sub-100 rows: the largest game-layer unit gap on the board.** `?UpdateScrolling@VocalTrack@@QAAXM@Z` is **8,948 B at fuzzy 72.278 / mpn 73.986** — 37 % of the unit gap in one row and **never worked by any lane** (zero occurrences in this roadmap) — but the brief states plainly that at 72 % this is a **reconstruction job, not a near-miss sculpt**, and tells the lane to consider taking the mid-band first instead: `RebuildHUD` 2,188 B @90.477, `UpdatePitchArrow` 928 B @98.254, `UpdateTubePlates` 772 B @90.668 and `Poll` 512 B @97.617 are **4,400 B** in the band `nearmiss-harvest.md` measures at a 60–75 % hit rate. Also open: `PrepareNoteTubes` 1,160 B @73.403, the ctor 868 B @84.654, `UpdateTambourineGems` 744 B @87.215, `PollLyricAnimations` 744 B @84.263, `GetNextLyricPlate` 516 B @54.558. ⚠ **`docs/decomp/research/2026-06-11-bp4-vocaltrack.md` is briefed as a DATED 2026-06-11 record, not as current state** — it reports `148 fns / 59 matched` and recommends a splits-only pin extension that has since happened (the unit is **178/220** today), so its "Verdict: GO" must not be re-run; read it for method and FontBase context only. ⛔ **Three spellings on this file are already REFUTED and are hard-banned**: `3bab1505` (Poll bool-normalization is retail failing to coalesce), `9b7291e8` (GetHarmonyScore `lwzx` operand order), plus `ca3e940a` (`sDumpPlateStates` is a file-static, +760 B) and `7d867293` (two wrong-callee fixes) as prior art to read before touching adjacent code — two of those commits are recorded refutations whose entire value is stopping the next lane re-deriving them. A permuter sweep shard here was landed and REVERTED (`0c36bc01` / `a3134311`); permuter stays banned. Oracle is `../rb3` (rb3-Wii DEV decomp) with the standing warning that it is Wii/MWCC-targeted and has been measured WRONG against retail on this class — **retail bytes outrank the oracle every time** — and that an inherited `#ifdef MILO_DEBUG` block is a SUSPECT (`MILO_DEBUG` is force-defined tree-wide, the `MILO_*` family is `#ifdef HX_NATIVE`, and this build defines eleven `/D` flags of which none is `HX_NATIVE`); fix per-site, ⛔ never blanket-remove (measured whole-binary control −21). Price from the charged-site list, rank by size-if-it-crosses, Δ0 or partial-with-evidence authorised. Out of scope: `BandStorePanel.*` (W16-CK) and `MusicLibrary.*` (W16-CL). | — |
| **W16-CN** (**landed** `0702a14f`+`7ec1e249`, docs only) | opus | Discharge W16-BZ §5 item 1: adjudicate the `GemTrackDir` ctor/dtor ICF fold memberships on retail bytes. **Verdict: dtor 3 pairs / 7 charges ALL PASS T1; ctor 2 pairs / 4 charges 0 PASS — but 0 of the 3,276 B is collectable today.** ⛔ The lane **retracted its own headline** in a follow-up commit (never an amend): `0702a14f` claimed "728 B collectable", and reading `symbol_aliases.json` to install revealed a **standing `withdrawn` record for that exact spelling** (compared byte-exactly, 155 chars, identical). ⚠ It also self-corrected a **false ABSENT**: a first pre-check reported "2 of 5 spellings MISSING" — decisive-looking and **wrong**, because the mangled names had been hand-retyped from output truncated at ~92 chars; a rescan keyed on programmatically-extracted names found **5 of 5 present**, with a positive control proving non-vacuity. Ctor blockers are exact: `make_pair`'s retail branch at `0x4` targets `0x82829258`, **unnamed in the map** (irreducible NEEDS_MAP_ID, and per MAPID-1 identification pays in **bug exposure, not bytes**), and `push_back` is a **chained fold** on `_Copy_Construct<SongPattern>` vs our `_Copy_Construct<pair<ObjPtr<EventTrigger>,…>>`. The gate's crash on `base_addr: null` is **correct behaviour** — the lane refused to fabricate an address to satisfy the parser. | |
| **W16-CO** (**landed** `1e38b737`, see table above) | opus | `default/band3/bandtrack/Gem` — 16,812/30,264 B = **55.55 %**, gap **13,452 B over 59 sub-100 rows**, fuzzy 82.545204, 178/222 fns. Led by `?SyncProperty@OutfitConfig@@` 1,304 B @99.9847 (**marked `sym`**), `fn_822A5A38` 1,164 B @0.000, `?AddChordInstance@Gem@@` 908 B @99.978, `?AddRep@Gem@@` 604 B @99.967. ⚠ Two traps briefed up front: SyncProperty carries a **relocation-NAME charge**, so its prize may be **uncollectable by instruction edits**; and the four `fz=0/mpn=0` rows (~3,412 B) are the **identification class, not source work**. | |
| **W16-CP** (**landed** `d3be434e`, see table above) | fable → **opus** | The **two-oracle alias conflict** W16-CN surfaced: map-resolved relocation destinations say **PASS** (T1, "32/34 words compared as FULL 32-bit values, 2 relocated branch destinations resolved through the map and name-equal") while a standing `withdrawn` record in `symbol_aliases.json` says **WITHDRAWN** for the same spelling. ⚠ First dispatch to Fable **never ran** — HTTP 429, `claude-fable-5-1` at capacity across all accounts; that is **infrastructure, not a result**, so the question is untouched. Re-routed to Opus with the escalation path intact. Briefed on the **STLPORT-1/GROUNDED-2 precedent** (8 memberships withdrawn on a premise that **did not exist** — a one-sided COMDAT-reader artifact — of which **6 were later restored at +1,728 B**) ⇒ **a standing `withdrawn` record is NOT automatically authoritative.** Deliverable is an **adjudication RULE**, not a verdict on 3,276 B; alias forgiveness carries **818,416 B / 7.93 pp** of `matched_code`, so the rule outranks the bytes. | |
| **W16-BV** (**landed** `ac629ded`, see table above) | opus | VocalTrackDir/GemTrackDir residue: identify the two 0 % unnamed rows in `default/VocalTrackDir` (`fn_822EF438` 1,532 B, `fn_822FA7E0` 1,000 B), `PostLoad` 3,656 B @ 96.39, `SetConfiguration` 528 B @ 82.40 (BQ §9.2's `__savegprlr_26` vs `_24` liveness gap), `ApplyFontStyle` 1,164 B @ 6.73, `SetRange` 700 B @ 93.54, and T1 fold proofs for `PlayIntro` / `ReleaseSmasherPlate` / `??3GemTrackDir` (install only if proven; `OnDrawSampleChord`'s `vector<int>` is oracle-right — do not metric-fit). | — |
| **W16-CQ** (**landed** `389a043e`, see table above) | opus | `default/band3/meta_band/SaveLoadManager` (`src/band3/meta_band/SaveLoadManager.cpp`, 2,412 lines) — **25,324 / 32,896 B = 76.982 %**, 289/314 fns, unit fuzzy 90.6719, gap **7,572 B over 26 sub-100 rows**. ★ **The NAMED and the `fn_` strata are stated SEPARATELY because a brief of mine got exactly this wrong four rows earlier and W16-CO corrected me**: **NAMED 4,556 B over 4 rows** — `?SetState@SaveLoadManager@@IAAXW4State@1@@Z` **4,096 B @ fuzzy 97.2422 / mpn 97.7549**, alone **54 % of the whole unit gap**, plus the ctor 368 B @99.9457, `GetProfile` 80 B @99.7500 and a 12 B vtable thunk @98.3333 — versus **ANON 3,016 B over 22 `fn_*` rows, which is the IDENTIFICATION class and explicitly NOT the lane's gap**. ⚠ The lane is told outright that briefs in this project are repeatedly wrong, to **re-derive before building on any figure, and that its measurement beats mine** if they disagree. ⛔ Told **not** to hunt the `SaveLoadManager.h` "+4 stale offsets" defect this repo's own CLAUDE.md still gestures at — `class_layout_report.py --check-header` now reports full agreement with the compiler for that header *and* for the paragraph's other example `CharEyes.h`; both were repaired and the note was never updated, so the brief would otherwise have sent a lane after a defect that does not exist. Carries the standing pricing rule (charge list from `report.json`; never a mismatch count, never an "N/N instructions equal" reading — both are instruction-level and structurally blind to `diff_arg`) and W16-CP's precedence rule (**adjudicate on the LAYER, not the label**: flat `L1_T1` outranks a recorded withdrawal, `L2_RECURSIVE` does not; check the withdrawal register **before** claiming any fold is collectable). Permuter banned; `REGISTER_SWAP` is a symptom, not a diagnosis. | — |
| **W16-CR** (**landed** `333feb1a`, see table above) | opus | `default/CharacterCreatorPanel` (`src/band3/meta_band/CharacterCreatorPanel.cpp`) — **8,892 / 21,368 B = 41.6136 %**, 167/206 fns, unit fuzzy 70.7701, gap **12,476 B over 47 sub-100 rows**, and **the split is the whole story**: **NAMED 5,800 B over 5 rows** (led by `?Handle@CharacterCreatorPanel@@UAA?AVDataNode@@PAVDataArray@@_N@Z` **5,164 B @ fuzzy 98.2440 / mpn 99.0496** — 89 % of the named surface in one row) against **ANON 6,676 B over 42 `fn_*` rows = 53 % of the gap, the IDENTIFICATION class, not source work**. ★ The brief flags **896 source lines for a 21,368 B unit** as a signal that bodies may be **missing rather than mismatched**, and tells the lane that if it concludes the value is in the `fn_` stratum (identification + scatter-include + body port, a different kind of work) it should **say so and stop rather than half-do it**. ⚠ Hazard briefed by name: `?Handle@CustomizePanel@@` was briefed to **three consecutive lanes** as "5,036 B behind 3 mismatches" when the real charged-site list had **five** sites, two of them fold-aliases — so closing all three instructions would have bought `mpn` 100 and **exactly zero bytes**. Get the charge list before believing a prize. Permuter banned, no alias installs, withdrawal register first, W16-CP layer rule. | — |
| **W16-CS** (**landed** `cd699bb8`, see table above) | opus | `default/ChordbookPanel` — source is **`src/band3/game/ChordbookPanel.cpp`, NOT `meta_band/`** (checked before dispatch; a wrong guess there costs the lane a search). **7,592 / 13,144 B = 57.7602 %**, 107/115 fns, unit fuzzy 91.2419, gap **5,552 B** = **NAMED 4,612 B / 5 rows** + **ANON 940 B / 5 `fn_*` rows (identification class, not source work)**. Led by `?DisplayChord@ChordbookPanel@@QAAXI@Z` **3,436 B @ fuzzy 97.1560 / mpn 97.4936**. ★ Two rows called out specifically: `?SetFret@ChordbookPanel@@QAAXHH@Z` (716 B) sits at **`mpn` 100 with fuzzy 99.8883 — the ARG-ONLY shape**, i.e. relocation-NAME charges that are a wrong callee **or** an ICF fold-alias, so if it is a fold the 716 B is **not collectable by editing instructions** and the lane is told to establish which **before** spending time rather than after; and `?Enter@ChordbookPanel@@UAAXXZ` (304 B @ **50.4474**) is the unit's only badly-broken row and its cheapest real divergence. Pricing rule is briefed **with its measured failure** — a lane once pre-registered **+96 B** off a "24/24 equal" reading and measured **−92 B**. Permuter banned, no alias installs, withdrawal register first, W16-CP layer rule. | — |
| **W16-CT** (**landed** `e6b04856`, see table above) | coordinator | re-admit the `0x822ec870` `vector<pair<ObjPtr<EventTrigger>,ObjPtr<EventTrigger>>>` dtor membership that ALIAS-REPAIR 2026-08-19 withdrew as `UNDER_PARTITIONED_ICF_CLOSURE` / `dropped_singleton`, on the strength of W16-CP's `L1_T1` adjudication. Spelling verified byte-exact by comparison, never retyping (W16-CN produced a false ABSENT this week by hand-retyping a truncated name). Restored per GROUNDED-2/W9-D. | Δ0 on CP's `dead` label — **FAILED, measured +184 B** |
| **W16-CU** (**landed** `5790329a`, see table above) | opus | **Audit W16-CP's liveness column.** W16-CT proved one `dead`-labelled row is LIVE and worth +184 B; CP labelled **304 of 456** rows `dead`. Determine how the label was actually computed, test (do not assume) the hypothesis that a census enumerating charged sites by row NAME is blind to anonymous `fn_` rows, re-price the class, and restore any row that is `L1_T1` **and** mispriced-dead — GROUNDED-2/W9-D convention: record out of `withdrawn[]`, spelling into `folded[]`, entry into `restored[]`, never delete and never merely annotate, because the guard reads `withdrawn`. ⚠ An alias lifts `name_check` **by construction**, so an unproven restore is an integrity hazard and the `none` ruler is flat there by construction and cannot clear one. Serialization is load-bearing: `json.dumps(indent=1)` with default `ensure_ascii=True` is byte-identical to the file. | ✅ **ANSWERED: the class was correctly priced and the coordinator's premise was wrong.** `dead` ⟺ not currently in any `folded[]` (456/456, rivals 411/456 and 307/456); no census computed it; CP's layer column reproduced 456/456 with discriminating controls. ⏳ Lane still running: the real vein it found is the **245 flat-`L1_T1`, not-installed, still-withdrawn memberships**, measured **INTERIM at +2,044 B / +6 fns / +7 rows, 0 fell out** against a null control that reproduced the baseline to the last digit — **unlanded** pending a permuted-decoy control, a guard-discrimination check, a full build and the native gate |
| **W16-CV** (**landed** `0f6dda2c`, see table above) | opus | **Identify `fn_826B5848`** (316 B anonymous) and land W16-CS's already-written, retail-verified reconstruction of `?Enter@ChordbookPanel@@UAAXXZ` (304 B), which is blocked solely on that identification because `matched_code` is all-or-nothing per row — the prologue alone buys 0 B while risking its 28 currently-equal instructions. Briefed with the naming economics so it does not read a negative delta as failure: objdiff FORGIVES placeholder targets, so naming converts a **forgiven** site into a **checked** one and the payout is **bug exposure, not bytes** (`0x827bcd38`→`MemAlloc` measured **−1,656 B** and was landed deliberately on accuracy, exposing 6 real wrong-callee divergences). | +304 B if `Enter` crosses; a negative whole-binary delta carrying a PROVEN name is an acceptable landing |
| **W16-CW** (**landed** `c83988ba`, see table above) | **fable → opus** (429 capacity) | `?UpdateScrolling@VocalTrack@@QAAXM@Z` (`src/band3/bandtrack/VocalTrack.cpp`) — **8,948 B at fuzzy 72.2780 / mpn 73.9857, ~1,372 charges**: **43 % of `default/VocalTrack`'s named gap** (20,948 B over 28 rows), 37 % of its total gap (24,444 B over 72 rows), and **never opened by any lane**. ★ W16-CM held this unit and **deliberately declined this row** — correctly: at 72 % it is a **reconstruction, not a near-miss sculpt**, and the mid-band was the better buy for one lane. So this is a **difficulty dispatch, NOT an escalation of a failure**; CM's work (`PrepareNoteTubes` 73.40 → 90.42, `UpdatePitchArrow` → mpn 100) is in the tree and good. ⚠ **ROUTING NOTE: dispatched to fable and terminated within seconds by an infrastructure 429 (all fable accounts at capacity) having done NO work — worktree verified pristine, 0 commits / 0 dirty / 0 untracked — so it was RE-DISPATCHED ON OPUS.** Recorded because the model column would otherwise misattribute this row's outcome to fable, and because the substitution was an availability contingency, not a judgement that the row got easier. Per the standing routing rule, if opus reports the row cannot close, that is the escalation trigger and fable takes it when capacity returns. ★★ Briefed with the measured rule that **a charge COUNT is not a workload, in BOTH directions** — W16-CR's 224 charges were 203 of one register swap plus 15 structural whose fix dissolved all 203 (13.5:1), and W16-CQ predicted ~5 and got 12 off one wrong constant cross-jumping a shared `li r4,0x3; b <tail>` block — so **cluster by CAUSE before estimating**, and `REGISTER_SWAP` is a symptom, not a diagnosis (12 recorded dissolutions). Carries the four hard-banned items on this file (`3bab1505` Poll bool-normalization REFUTED, `9b7291e8` `GetHarmonyScore` `lwzx` operand order REFUTED, `ca3e940a` and `7d867293` as prior art), the permuter ban with its reverted sweep shard on this very file (`0c36bc01` / `a3134311`), and the **DATED** flag on `docs/decomp/research/2026-06-11-bp4-vocaltrack.md` whose "Verdict: GO" must not be re-run (it predates the pin extension; the unit is 179/220 today). ⚠ CM's two earned warnings briefed by name: **a DELETE cluster means retail HAS the block** (CM predicted fuzzy ≥97 guarding `PollLyricAnimations` and measured **+2.04**), and **guarding a block can take a MATCHING row from 100 to 0** (−92 B, `??$MakeString@MPBD@@`). ⚠ Side-observation flagged as report-don't-chase: 568 B of anonymous-namespace `Unlockable` vector templates sit at fuzzy 0 inside this unit — meta_band material, so a pin/attribution question rather than source work. | Δ0-with-evidence and partial-with-evidence both authorised; `matched_code` is all-or-nothing so the 8,948 B pays nothing until the last INDEPENDENT charge closes |
| **W16-CX** (**landed** `c99ce4ee`, see table above) | opus | install the three handoffs CQ/CR/CS left uninstalled, **each re-verified against the artifacts before briefing**: (1) map `0x8260D418` → `?SetGender@CharacterCreatorPanel@@QAAXVSymbol@@@Z` — 120 B row at fuzzy 0, **not** `masked_equal`, no map row, no collision, mangling read from our COFF; (2) `0x826b56c8` is a **transposed** map row — retail is a lone `b UIPanel::Unload`, our obj defines `?Unload@ChordbookPanel@@UAAXXZ`, so the rename is safe; (3) add `reserve<BandProfile*>` to alias group `0x823715e0`. ⚠ **Two coordinator corrections carried in the brief**: the "needs a partner membership" caveat on (3) belongs to the already-installed OvershellSlot spelling and does **not** apply, and the `none` ruler **cannot** clear a fabricated alias. | ~490 B; items 1–2 near-certain, item 3 gated on retail-byte proof — **a recorded refusal is a valid deliverable** |
| **(coordinator, no lane)** `FABRICATED_CLOSURE_NOT_PARTITION` — **VEIN CLOSED, do not dispatch** | — | **9,393 of 10,243 withdrawal records (91.7%)** — the largest population in the ledger and the third time this campaign has picked it up. Closed in **both** forms it invites, read-only, no artifact touched. **(a) Not a restore lane:** 357 groups carry it and **340 have ZERO live folded members** (withdrawn:live = 24:1, 9,393 vs 391) — the withdrawals emptied the group, so there is no trapped fold to recover; the six 217-member `_ECX2SubmixVoice`/`_GCAudioSRC` groups and four `insert` survivors are all live=0, and idx 95 withdraws `list<int>::insert` against `list<float>::insert` — **distinct instantiations with different node deallocators, which cannot fold**. Alias forgiveness carries **818,416 B / 7.93 pp**, so restoring them lifts `name_check` **by construction** with the `none` control flat **by construction** — the hazard signature, not a clearance. **(b) Not a partition-repair lane either:** `icf_alias_build.py:668` records in its own banner that the star→clique fix was **built, measured and reverted** (`760cb450`, *"its predicate is the wrong court"*) at **164 TRUE vs ≤57 refuted = 3 true folds discarded per fabrication caught**; reopening requires a predicate anchored on the **RETAIL** image (CD-7 reloc-normalized body hashing), not our-build ICF congruence. ⚠ **Correction to my own prior framing: ALIAS_HYGIENE §3.4's "all 10 invariant-breakers are PROVEN" describes a 10-member survivor-collision set, NOT the 9,393** — conflating them inflated the apparent prize by ~3 orders of magnitude. ✅ **H1's denylist half is already LANDED, not pending** (ALIAS_VEIN marked it undone; ALIAS_DURABILITY shipped it) — verified live in source, guarded regeneration re-fabricates **0** where unguarded re-fabricated **110**, 14 of them this class. ★ **Coordinator-side instrument failure, recorded:** my first sample keyed on `reason` and printed **"groups carrying this class: 0"** against a census of 9,393 — the class lives under `class`. A decisive-looking emptiness is this repo's signature failure mode and it appeared on the coordinator's side, not a lane's. **Still open and NOT covered:** the 17 star-groups that do carry live folds (391 memberships) — an ordinary alias-audit question, not a lever. Full record: `docs/decomp/ALIAS_FABRICATED_CLOSURE_CLOSED_2026-09-16.md`. | — |
| **W16-CZ** (**landed** `4be935fe`, see table above) | opus | Own the **entire `default/ChordbookPanel` residue**: the unit is **115 rows / 13,144 B with 107 rows / 8,048 B already at `fuzzy == 100`**, so the whole surface is **8 rows / 5,096 B** — a bounded lane, not an open-ended unit. ⚠ **Baseline dependency:** CX renames the map row `0x826b56c8` → `?Unload@ChordbookPanel@@UAAXXZ`, which **changes pairing inside this unit**, so CZ must branch after CX lands; if `?Load@ChordbookPanel@@UAAXXZ` still reads 4 B / fuzzy 95.0 the lane is on a stale baseline. **Two identifications made by the coordinator and recorded here so they survive the brief file:** (1) **`fn_826B64B8` (300 B, fuzzy 0) = `?PickFretboardView@ChordbookPanel@@QAAXABVGameGem@@@Z`** — `this`→r29, the incoming `GameGem&` in r4 survives untouched into a 2-arg `TheGemTrainerPanel->GetFretboardView(gem)`, two symmetric arms each lazily initialising a function-local static through **one shared guard word** (`ori 0x1` / `ori 0x2` on `lbl_82E02CE4`) with per-arm atexit registration, `mChordLegend` at `this+0x54`; **the two strings were READ FROM THE RETAIL IMAGE, not inferred** — `lbl_820E7718` = `"show_high_frets"`, `lbl_820E76F0` = `"show_low_frets"`; our obj defines the symbol, **no map row at `0x826b64b8`**, **0 collisions across 29,549 map rows and all report rows**. The divergence is the **storage class**: retail uses function-local statics where our source uses the `Messages4.h` globals — and the local-static idiom is **already used by `DisplayChord` in this same file** (`??__Freset_chord_msg`, `?$S4@` guards in our COFF). (2) **`fn_826B7A80` (108 B, fuzzy 0) = `?OnDisplayChord@ChordbookPanel@@QAA?AVDataNode@@PBVDataArray@@@Z`** — the clamp is verbatim `i2 & ~(i2 >> 31)` (`srwi/subi/and`) against `mNumChords-1` at `this+0x7c`, then `bl fn_826B67C8` = `DisplayChord(idx)`, then a 2-word `{idx, 0}` store = a `DataNode` return. ⛔ **A REFUTED COORDINATOR HYPOTHESIS, recorded so the lane does not re-run it:** the two arms of `fn_826B64B8` looked like a lefty/non-lefty pair (`RGTrainerPanel.cpp:583-585` has exactly such a pair) and it looked like our source was missing a lefty branch — **it is not**; our `PickFretboardView` already has both arms and is shape-identical to the rb3-Wii oracle. The arms are high/low frets. Killed by reading the source and the image before briefing, not after. ★ **Two of the eight rows are arg-only and are NOT source work**: `SetFret` (716 B) and `fn_826B7C50` (40 B) sit at **`mpn == 100` with `fuzzy < 100`**, i.e. every penalty is a relocation-NAME charge — either a genuinely wrong callee (the most valuable fix class we have) or an ICF fold, and the brief **forbids installing an alias without retail-byte proof** since an unproven one lifts `name_check` by construction with the `none` control flat by construction. ★ One concrete lead on `?HandleLegendLefty@` (316 B @ 89.4937): our `DECOMP_FORCEACTIVE(ChordbookPanel, "string_%%02d.lbl")` vs the Wii oracle's `(..., "string_%%02d.lbl", "lefty_flip.anim")` — a **lefty** string absent from our forceactive in the unit whose *lefty* row is below 100. ⚠ **Instrument trap the coordinator hit and briefed:** the vaddr→file-offset delta is **PER-SECTION** — the `.text` delta `0x8200B200` resolved a `.rdata` label to the plausible-looking ASCII `"GemTrainerPanel"`, a wrong answer shaped like a right one; the data delta is `0x82000000`. | `DisplayChord` 3,436 B is the only row whose closure moves real bytes (`matched_code` is all-or-nothing per row); the two map-row additions are BETS, not freebies — `name_check` already forgives placeholder targets, so naming converts forgiven call sites into checked ones |
| **W16-DA** (**landed** `1f3e147d`, see table above) | opus | the `TheDebug <<` stratum. ⛔ **THE ROADMAP'S OWN FIGURE WAS A STALE SLICE AND IS CORRECTED HERE: not "171 sites across 49 files" but **185 sites across 52 files**, measured live on main `56e36a72` (`command grep -rn 'TheDebug *<<' src/`). Only **13** sit inside any `#if`; **172 are unguarded.** The coordinator caught this only by testing the briefed number instead of relaying it — the standing `READ THE IN-TREE RECORD FIRST` rule demonstrating itself. ⛔⛔ **This is NOT a guarding sweep and a sweep would be NET-NEGATIVE: the hazard is now SIZED at 520 B.** Guarding a site removes our TU's `MakeString` instantiation, which retail's TU defines, and `matched_code` is all-or-nothing per row — the row does not degrade, it LEAVES. Five such rows currently score fuzzy 100 / mpn 100: VocalTrack `??$MakeString@MPBD@@` 92 B · Font `??$MakeString@GEHH@@` 92 B · CharBones `??$MakeString@MMMMMMM@@` 152 B · CharBones `??$MakeString@MF@@` 92 B · CharBonesSamples `??$MakeString@HHHH@@` 92 B. ★ **The real question is an identification, not hygiene**, and a prior lane already wrote it into `VocalTrack.cpp:1036-1042`: *retail never loads `?TheDebug@@3VDebug@@A`, so retail's spew reaches MakeString by some other sink — and that sink is unidentified.* ⚠ **Instrument trap recorded before dispatch:** `?TheDebug@@3VDebug@@A` IS in `target_symbol_map.json` at `0x82cc9874`, but the image holds **zero occurrences of even the substring `TheDebug`** — retail is stripped, so a mangled name can never appear there and **string presence proves nothing in either direction**. The testable form is whether retail CODE references `0x82cc9874`. |
| **W16-DB** (**landed** `8e62621d`, see table above) | opus | the alias remainder, owning `scripts/symbol_aliases.json`. **Primary: the 213 surviving `UNDER_PARTITIONED_ICF_CLOSURE` withdrawals — verified on the artifact before dispatch, ALL 213/213 carry `lane: "ALIAS-REPAIR 2026-08-19"`**, split 155 `repartitioned` / 58 `dropped_singleton`. That is the same sweep W16-CU restored 242 memberships from (+2,044 B / 6 payers) on the ground that its predicate resolves operands *through the ICF congruence over OUR OWN build* while `/OPT:ICF` folded COMDATs in **retail's** link — unsound in this direction, the identical defect W9-D (`c6711597`) reversed in 65 siblings. ⛔ Dispatched as a **hypothesis to test per-membership on retail bytes, NOT a licence to bulk-restore** — inheriting a class label as a verdict is the exact error CU was sent to correct. **Secondary: the 71 rows CU left unadjudicated** (26 `CONTRADICTED` *counted, not adjudicated* · 29 `NEEDS_SOURCE` · 8 `L2_RECURSIVE` · 7 `NEEDS_MAP_ID` · 1 `L5_INCONSISTENCY`); ★ a `NEEDS_MAP_ID` close is expected to **cost bytes and expose bugs** (MAPID-1 measured −1,656 B and surfaced 6 real wrong-callee divergences), which is a **WIN** under `ACCURACY BEATS HEADLINE %`. ⛔⛔ The 9,393 `FABRICATED_CLOSURE_NOT_PARTITION` records are explicitly **OUT OF SCOPE — closed vein**. ⚠ Artifact traps carried into the brief: the class key is **`class`, not `reason`**; `withdrawn[]` holds **10,241 dicts AND 2 bare STRINGS** (a naive `w.get()` census crashes there — the coordinator's did, and those 2 malformed records are themselves a finding); and the `dropped_singleton` half needs the **`survivor` self-alias screen** CU's first installer missed. ★★★ The control discipline is the deliverable: the `none` ruler **cannot clear an alias** (flat by construction), **`ALIAS_SUSPECT` is structurally silent on zero-forgiveness aliases** (W16-CX), so the only control that discriminates is CU's **permuted decoy — whose FIRST version was vacuous and returned exactly the prior (+0 B)**, hence each leg must assert inside the measured path that the rendered map grew by exactly N lines and REFUSE otherwise. |
| **W16-EA** (**landed** `4e7ae9b0`, see table above) | opus | adjudicate, and if real fix, the claimed `Track::unk50` ↔ `Track::mIntroPlaying` field swap blocking `?PollLyricAnimations@VocalTrack@@` (**verified: 744 B, fuzzy 84.2634, mpn 85.9839, not `masked_equal`**). The claim's provenance is `6e9f1543` (W16-DA, landed today) — *"objdiff idx 136/137 show target reading `Track::unk50` where we read `Track::mIntroPlaying` and vice versa"* — DA's own reading of a diff, **not independently verified**. ⛔ **A COORDINATOR ERROR IS CARRIED IN THE BRIEF SO THE LANE DOES NOT INHERIT IT**: I first read the rb3-Wii oracle as corroborating the swap because it places `mIntroPlaying` at `0x60` where we place `unk50` — **that is wrong**, it is the uniform +0x10 base-class shift, and `unk50` is *named* for its Wii offset `0x50`. **The oracle AGREES with our declaration order**, so "swap the declarations" is not the obvious fix and three rivals must be discriminated (wrong 360 layout / read-site bug / DA misread). Ground truth is `class_layout_report.py` — the `// 0xHEX` comments and everything derived from them are **not** authoritative. ⛔ Must NOT land DA's guarding patch alone (**−92 B / −1 fn** standalone; it is a *prerequisite* for the 744 B row, never a win alone) and must NOT re-home the hand-placed 0x68 B splits block at `0x827efa60-0x827efac8` (re-homing is **non-neutral**, PINHOME-1 +3 fns / +428 B). | a struct fix whose blast radius is 10 read sites at once (`Track.cpp:30,39,56,82,83,341`; `VocalTrack.cpp:129,313,1017`) — **or** a recorded refutation of DA's claim, which is equally a result |
| **W16-EB** (**landed** `80461dd6`, see table above) | opus | adjudicate the anonymous `Unlockable` row family: **real membership, or address-pin attribution artifacts?** **Verified: 11 rows / 1,612 B, every one at fuzzy 0 AND mpn 0, none `masked_equal`**, all STLport `vector`/template instantiations over `Unlockable@?A0xf8e4b4b5@`, across **9 units** — VocalTrack 336+136+96 = **568** (this is the figure the old handoff carried, and it is right, but it is only 35% of the family), Morph 328, Character 136, `band3/game/Tracker` 124+108, MoveMgr 96, Sequence 96, Gem 96, PracticeSection 60. ★ `Unlockable` has **ZERO references in `src/` and ZERO in `../rb3/src`** (checked with quoted globs — an unquoted `--include=*.h` makes zsh refuse the command and print nothing, which reads as a decisive negative, and that error occurred in this session). ⚠ **The coordinator's hypothesis — that four of the nine (Morph, Sequence, MoveMgr, Character are ENGINE units) are attribution artifacts — is EXPLICITLY FLAGGED AS UNTESTED in the brief**, to be refuted if wrong. Either answer is a win: real ⇒ a sized vein needing a type nobody holds; artifact ⇒ the vein is correctly retired, and under **ACCURACY BEATS HEADLINE %** a truer denominator that *lowers* code% is still a win. ⛔ No pin chasing (**pinning is not decomp**), no re-homing, no stubbing the type into existence to buy 0% pairable rows. | an adjudication doc under `docs/decomp/`; bytes are secondary to closing or opening the vein honestly |
| **W16-EC** (**landed** `746736dc`, see table above) | opus | adjudicate live **candidate wrong-callee** pairs by correcting **map names** — an **accuracy/bug-exposure lane, explicitly NOT a byte chase**. ⚠ **The brief leads with the standing refutation rather than hiding it**: MPNGAP-1 measured this stratum as **~91% irreducible fold/map noise — "do not re-fund this as a byte lever"**, and that verdict **stands**; correcting two names and refuting thirty is a success. ★ **What justifies the lane anyway is measured TODAY**: W16-CZ corrected ONE name (`0x826ab108`, own row **136 B**) and the gate measured **+1,908 B across SEVEN units** — a correct name **re-pairs every caller**, so the cascade is the payoff and the row's own size is not, and a cascade is itself evidence the name is right (a wrong name **un-pairs**). ★ **Liveness re-checked against the CURRENT `report.json` before briefing, because both artifacts are dated 2026-08-12**: of the 535 pairs verdicted *"unclassified: candidate genuine wrong callee"*, **203 are STILL LIVE across 511 charged call sites** (330 dead, 2 unknown); `nogroup-wrong-callee-queue-NOGROUP1.tsv` has **91 live worth 11,048 B** against **102 ALREADY DEAD worth 26,104 B** — i.e. the vein is **~62% drained by bytes** and the remainder averages ~121 B/entry, which is exactly why this is briefed on cascade and accuracy, not on the solo column. ⛔ **The `map_misassignment` sub-class is DRAINED — 27 rows, ZERO still live — and the brief says so**, so the lane cannot burn budget re-running it. ⛔ Must **correct names, never install aliases** (an unproven alias lifts `name_check` by construction and the `none` control **cannot** catch it — that flatness is the hazard's signature, not a clearance); must check the **call site**, not just the signature (CZ reopened a row a prior lane closed by skipping exactly that). | 2–5 adjudicated names with cascade measured, plus a recorded refutation set — **the refutations are the durable half**, since a drained class found and written down is what stopped this lane wasting a day |
| **W16-CY** (**landed** `c8f94be2`, see table above) | **fable → opus** (429 capacity) | **escalation** — audit W16-CW's "permuter-bound" verdict on `UpdateScrolling`. Attack the one load-bearing claim: that the misplaced loop-exit tail *cannot* be source-reordered because it stores `itT - begin`. That argument shows the block cannot move **earlier**; it does not show our control-flow shape is forced, and "retail falls through where we branch" is downstream of loop rotation / single-vs-multi-exit shape / `break`-vs-`goto` — all source spellings. Secondary: `lwz +12 / addi −12` is a suspiciously clean trade, the signature of address materialisation differing. ⚠ **ROUTING NOTE: dispatched to fable and killed within seconds by an infrastructure 429 (all fable accounts at capacity) having done NO work — no worktree was ever created — so it was RE-DISPATCHED ON OPUS**, the same contingency W16-CP and W16-CW hit. Recorded so the model column does not misattribute this row's outcome to fable. The escalation target is unchanged and it is now opus-auditing-opus, which is a **weaker** instrument than the escalation rule intends; if this lane also returns "cannot close", that is NOT the two-lane confirmation the rule asks for and the row stays open for fable when capacity returns. | either a structural defect whose fix dissolves the 183 insert/delete **and much of the 754 regalloc `diff_arg`s** (the CR 13.5:1 shape), **or** independent confirmation — confirming CW closes the row on two lanes and is a success, not a failure |
| escalation slot | fable | anything an Opus lane reports as unfixable, with the Opus report attached | — |

W15-F's `RunXinputJoypadLoop` item is NOT queued as a body-port: our
`Joypad_Xbox.cpp` lacks the entire XInput2 raw-HID layer, so it is a
subsystem port and belongs to the native-port track, not a match lane.

### Coordinator repair, 2026-09-15 — the fold gates were refusing on a reader artifact (`22899f87`)

**My own filed note was right about the disease and wrong about the patient.** It
named `tools/fold_thunk_gate.py:208` reading `cd["raw"]` where `comdat_bytes.py`
offers `fn_raw`. Verified literally rather than inherited, and the census widened
it: **five** tools read the funclet-billed view — `fold_thunk_gate`,
`comdat_fold_gate` (3 sites), `alloc_fold_gate`, `alias_uniqueness_audit`,
`ourside_fold_sweep` — while comparing against a retail `.pdata` **function**
extent. `w16s_alias_census.py` keeps both views on purpose and was left alone.

`raw`/`relocs` run to the end of the section, so an EH-bearing `/Gy` COMDAT bills
its trailing `__unwind$` funclet into the body; in a **non-`/Gy` monolithic**
section it bills the rest of the section (`keygen_xbox.obj`: **2,456 B for a
144 B function**). Measured population: **41,463 of 680,523 compiled COMDATs
(6.09 %)** have `raw != fn_raw`. Same family as STLPORT-1's phantom "+8 B STLport
source bug" — a **one-sided** reader error, which is why a size test cannot catch
it (the artifact cancels on both sides).

⛔ **PREDICTION FAILED, and that is the finding.** I expected the 153
length-REFUSALs on `comdat_fold_gate`'s 911-pair worklist to be re-adjudicated.
Measured: **ADMIT 6 → 7 pairs (7 → 8 sites), REFUSE 905 → 904.** The size check
is only the FIRST screen — once lengths agree, the byte/relocation comparison
still refuses. `fold_thunk_gate` is **unchanged** (7 ADMIT / 29 REFUSE) because
retail's survivor there is a 1-word `blr` and our body is hundreds of words
either way ⇒ **the one tool my note named is the one where the fix changes
nothing.**

★ **What moved is the EVIDENCE, which is what a gate is for:**

| refusal class | before | after | Δ |
|---|---:|---:|---:|
| SIZE (reader artifact) | 673 | 528 | **−145** |
| RELOC/NAME | 194 | 337 | +143 |
| BYTE divergence | 22 | 23 | +1 |
| ADMIT | 6 | 7 | +1 |

**146 pairs left the artifact class** and now refuse (or admit) on real evidence;
`our_bytes` is corrected on **413 rows**. The surviving **528 size refusals are
genuine** — our body really is a different length.

The single flip is the case `comdat_bytes.py`'s own note predicted:
`??$_Copy_Construct@UEyeDesc@CharEyes@@@stlpmtx_std@@` was *"body size 60 bytes
(retail extent) vs 104 (our COMDAT)"* — the constant −44 B funclet — and is now
*"identical: 14/15 words compared as FULL 32-bit values, 1 relocated branch
destination resolved through the map and name-equal"* (CF2).

⚠ **It is NOT installed.** Nothing was aliased by this commit — an admission is
reported, never written, and installing it is a metric-moving act that needs the
gate chain and a measurement. **Available, priced, unclaimed** for a future lane.

Safety was checked in the direction that could LOSE bytes: exactly **one** pair
had `retail == len(raw) != len(fn_raw)` (`??1EventCall@EventAnim@@`), it is in
**ZERO alias groups**, all three of its rows stayed REFUSE, and **ADMIT →
non-ADMIT withdrawals: 0** — so nothing installed ever rested on the over-count.

Guard: `tools/test_fold_gate_function_extent.py`, registered in
`scripts/test_tools.py`, and **proven to fail** — reverting one site in a `~/tmp`
copy exits 1 naming the site. `comdat_fold_gate --selftest` stays PASS (0
defects); `test_fold_thunk_gate_install` stays PASS.

### Coordinator verification while wave 16 ran — the two biggest non-SYMBOL rows are PURE ARITH_COMMUTE, do not fund

Measured 2026-09-15 at main `a1b5e48f`, read-only, from `report.json` and the
charged-site lists (never a mismatch count). Ranking the `crossing_worklist
--adjudicate` top-25 by size-if-it-crosses puts two large non-SYMBOL rows
directly below the three lanes this wave dispatched, and both are tempting on
size alone:

| row | size | fuzzy | mpn |
|---|---:|---:|---:|
| `?Handle@TourProgress@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | 2,596 B | 99.9692 | **100.000** |
| `?Poll@CharIKHead@@UAAXXZ` | 1,820 B | 99.9341 | **100.000** |

**`mpn == 100` with `fuzzy < 100` is the arg-only shape**, so before briefing
either I pulled the actual charged sites. Every charge on both rows is a
`diff_arg` with the two *source* operands swapped on a **commutative** opcode:

```
TourProgress  [356] add    r4, r4, r9     vs  add    r4, r9, r4
              [412] add    r10, r10, r8   vs  add    r10, r8, r10
CharIKHead    [127] fmadds f13, f30, f0, f11  vs  fmadds f13, f0, f30, f11
              [129] fmadds f0, f29, f0, f10   vs  fmadds f0, f0, f29, f10
              [291] fadds  f10, f10, f0       vs  fadds  f10, f0, f10
```

⇒ Both are **PURE ARITH_COMMUTE**, the vein `crossing_worklist` already rates
**PROVEN INERT — do not fund** (70 rows / 36,108 B tree-wide). Recording the two
specific rows because the generic ledger line is invisible to anyone ranking
candidates by size: **4,416 B that looks like the obvious next target is not
one.** A future lane that wants to reopen this needs a *codegen* lever for
operand order, not a source spelling — the same class of channel W16-CG was
dispatched to test on `CustomizePanel`, so **wait for CG's verdict before
funding any operand-order work anywhere.**

⚠ Note what did the work here: the **class label alone was not enough**. The
worklist labels TourProgress `ARITH_COMMUTE` but does not mark it `sym`, and
`mm=2` looks eminently closeable. It was reading the charged instructions
literally that settled it — the standing "test a briefed figure literally"
rule, applied to my own candidate list.

### Coordinator check, 2026-09-16 — the parked `repartitioned` stratum is UNADDRESSABLE

Queued for several waves as "DB's 154 parked `repartitioned` rows — needs an address,
not an argument". Measured today against the live `scripts/symbol_aliases.json` and
`scripts/target_symbol_map.json`, read-only:

| measure | value |
|---|---:|
| records carrying `disposition: repartitioned` | **222** (206 `restored[].superseded_records[]`, 16 `withdrawn[]`) |
| distinct spellings | 222 (one record each) |
| **whose spelling resolves to ANY VA in the map** | **1** (`?Init@BandCharacter@@SAXXZ`) |
| composition | 157 ordinary symbols · 34 `MakeString` templates · 31 other templates |

⇒ **221 of 222 have no retail address at all**, so there is nothing to adjudicate them
*against*. The blocker was stated correctly and it is structural, not a matter of
effort. **Do not re-queue this as an adjudication lane.**

⚠ **Two honest limits on that, both of which I got wrong first and am recording so the
next reader does not inherit my errors:**

1. **This population does NOT reconcile with DB's.** DB tabulated **213** (154
   no-address + 2 different-address + 0 same-address + 57 dead); I count **222**. They
   are different selections — mine keys on the disposition field across all groups,
   DB's on its own partition classes — and the file has moved since (W16-EC withdrew a
   group today). **So this does not measure DB's 154**, and any claim that it does is
   unsupported.
2. **"Dominated by `MakeString` templates" is FALSE** — I asserted it from a six-item
   *alphabetically sorted* sample, which biases toward `??$` prefixes. The truth is
   **157 of 222 are ordinary symbols**, which makes the finding stronger, not weaker:
   the unaddressability is not a template-folding artifact.

★ The instrument was nearly discarded for the wrong reason. A 1-in-222 hit rate against
a 29,445-name index looks exactly like a wrongly-keyed lookup, and I called it vacuous
before testing it. A **positive control** — four names known to be in the map, including
`?MemAlloc@@YAPAXHH@Z` and the three W16-EC had just written — resolved 4/4, which is
what promoted the reading from "broken tool" to "finding". **Neither "it confirms my
prior" nor "it contradicts my prior" is evidence about an instrument; a control is.**

### Wave 16 continued — W16-EE and W16-EF dispatched (2026-09-16)

Two lanes in flight, both opus, both with worktrees already created so neither burns
budget on setup.

**W16-EE — the `_bijection_arbitrary` sub-100 vein** (**LANDED `35188831`, +2 fns / +280 B — see the results table and the "W16-EE landed" subsection below; the vein is DRAINED, do not re-fund**) (`~/tmp/wt-w16-ee`, off `af6b35a6`).
W16-EC landed +5 fns / +432 B out of exactly this census and left it open in its §7. The
signature that pays is **transposition between two same-shaped siblings**, not size. Told
to re-derive EC's own figure of 57 rows / 3,728 B rather than inherit it — the renamer
docstring's "10" is stale, and EC found three other inherited figures that did not survive
re-measurement. Told to prefer alias **withdrawal** over installation, and that a
well-evidenced "this vein is drained" is an acceptable deliverable.

**W16-EF — the `Game` rows W16-CB newly PAIRED** (**LANDED `883430f5`, +3 fns / +408 B — CB's name CONFIRMED and CB's falsifier REFUTED as unsound; see results table**) (`~/tmp/wt-w16-ef`, off `ace069a9`).
Measured fresh today: `default/band3/game/Game` carries **15 named sub-100 rows / 7,180 B**
payable (plus 17 anon `fn_*` rows / 1,740 B with **no byte upside**, since `name_check`
already forgives placeholder targets). Seven of the fifteen are rows CB named on 09-15 —
their low scores are previously-invisible source divergence, not neglect. CB's §8 hands
over five leads with evidence attached; its §6 names what it skipped.

★ **EF's first task is to test W16-CB against its own falsifier.** CB wrote that *"a wrong
identification lands near 0, because objdiff pairs by name and a misnamed row is compared
against an unrelated body"* — and the row it named at `0x8267b808` sits at **3.24%**. CB's
positive evidence (caller-set matching, 3-for-3) is real, so this is not a claim that CB
erred; it is a claim that the question is open and cheap to settle on retail bytes. Both
outcomes are deliverables: a right name means 1,664 B of genuine source work, a wrong one
means **removing a false identification, which is accuracy-positive** — W16-EC dropped
`?SetControllerType@BandUser@@` on exactly that reasoning at a cost of 0 bytes.

⚠ **Known collision risk, flagged to both lanes:** EE may edit `scripts/symbol_aliases.json`
and `scripts/target_symbol_map.json`; EF may edit the former if it installs CB's `~Shuttle`
fold membership (which must go through `tools/fold_thunk_gate.py`, never by hand). Both were
told to say so prominently so merge order can be managed. They are on separate branches in
separate worktrees, so the rebase handles the mechanics.

**Explicitly withheld from both:** the permuter (standing directive); `?Reset@Game@@` (a
plain r4/r6 regalloc swap); `fn_8235F858` (a mis-homed Tour body — re-homing a pinned
address is not metric-neutral); `map_misassignment` and the alias withdrawal ledger (both
drained); and the parked `repartitioned` stratum (unaddressable — see the subsection above).

### W16-EG dispatched — the rows with no lane document (2026-09-16)

> **LANDED `50d494c8` — +1 fn / +412 B. The dispatch hypothesis (phantom / identification problem) was REFUTED: the 0% row is ordinary source work. See the results table.**

Third lane of wave 16's tail, opus, worktree `~/tmp/wt-w16-eg` off `0351e376`. Target is
the four sizeable `src/band3/` rows that appear **only in census TSVs and in no lane
write-up at all**:

| size | fuzzy | row |
|---:|---:|---|
| 1256 | 92.0892 | `?HandleExitExtent@PerfectSectionTracker@@QAA_NMH_N@Z` |
|  980 | 91.4571 | `?Poll@PatchPanel@@UAAXXZ` |
|  664 | 92.9819 | `?Poll@StoreMainPanel@@UAAXXZ` |
|  412 |  0.0000 | `?RefreshAll@ManageBandPanel@@QAAXXZ` |

★ **How the target was chosen matters more than the target.** A size-ranked census of the
game layer (310 named sub-100 rows / 105,140 B) puts `?SetState@SaveLoadManager@@` on top
at 4,096 B — a row **W16-CQ already closed** as unable to cross. Two more of the top five,
`?Poll@BandStorePanel@@` (90.22) and `?Handle@BandStorePanel@@` (98.76), are not untouched
rows at all: they are **W16-CA's output** (it moved them from 45.04 and 93.12). And
`?MaybePublish@UIStats@@` plus `?Handle@StoreOfferProvider@@` are permuter-bound, which is
off by standing directive. ⇒ **Ranking by size selects, with high reliability, the rows
previous lanes already finished with.** The filter that actually found virgin work was
"has no prose document anywhere in `docs/`".

⚠ **The lane is briefed to expect identification, not decomp.** Every candidate unit
carries more ANONYMOUS sub-100 bytes than named ones (PatchPanel 3,120 vs 1,788;
PerfectSectionTracker 2,128 vs 1,256; StoreMainPanel 1,180 vs 1,760). That is the exact
shape W16-CD measured: briefed ≈2,016 B, measured 2,108 B, and only **472 B of it was
source** — the other 1,636 B was a map problem, because *when a retail row is anonymous,
source quality is invisible to the ruler*. EG must split every prediction into "source"
and "identification" before measuring, and check the 0.0000 row's **byte geometry** first,
since a phantom row from a dtk mis-carve is indistinguishable from an unidentified one.

⚠ **Collision management, three lanes live:** EG is told to keep off `src/band3/game/Game.cpp`
(W16-EF owns it) and off `scripts/symbol_aliases.json` / `scripts/target_symbol_map.json`
(W16-EE may edit both, W16-EF may edit the former), and to declare any such edit
prominently if it proves unavoidable.

⚠ **A tooling note recorded against myself:** the search that established "no prose
document" was run with `--include='*.md'` placed after the path, which the shell `grep`
shim silently ignored — it printed a "prose(.md)" count while listing `.tsv` files. The
conclusion survives only because the listed filenames were visibly all TSVs. **An option
the tool ignores produces a label that cannot fail**, which is the same vacuity family as
the `[p]attern` self-match and the unconditional annotation, both also hit today.

### Coordinator adjudication, 2026-09-16 — W16-EB's `0x822a3010` fold-gate flag is REFUTED ON SIGN

W16-EB flagged `comdat-fold-gate-2026-08-12.json`'s refusal at `0x822a3010` — *"body size
136 bytes (retail extent) vs 128 (our COMDAT)"* — as **"the exact shape of the STLPORT-1
reader artifact"**, correctly labelling it *flagged, not re-litigated*. Adjudicated today
from the recorded numbers alone; no gate re-run was needed.

**The artifact inflates OUR side, and this row is the opposite sign.** Per `22899f87`, the
five gates read our COMDAT's `raw` (definition → end of section, including a trailing
`__unwind$` funclet) while comparing against retail's `.pdata` **function** extent. So the
defect can only make our number too LARGE. At `0x822a3010` ours (128) is already SMALLER
than retail (136). Correcting the artifact shrinks our number further — it **widens** the
gap, it cannot close it. ⇒ **This refusal is not an instance of the artifact**, and the
flag should not be carried forward as an open lead.

**The artifact class itself is real, and this file corroborates it independently** — which
is worth recording because the file predates the fix and so is an uncontaminated witness.
Of 776 parsed size-refusals:

| direction | rows | share |
|---|---:|---:|
| ours BIGGER than retail (artifact-compatible) | 529 | 68.2% |
| ours SMALLER (artifact **cannot** explain) | 247 | **31.8%** |

and the surplus where ours is bigger peaks at **40 B (86 rows)** and **44 B (20 rows)** —
precisely the trailing `__unwind$` funclet signature `22899f87` named from the other
direction. Two independent artifacts agreeing on a mechanism is the strongest form this
evidence takes.

⚠ **But this JSON is STALE and must not be read for current verdicts.** Its last three
commits are `b606f610` / `44835bf7` / `1b26376b`; **`22899f87` is not among them**, and its
population (1,048 pairs, 1,025 REFUSE / 23 ADMIT) does not match the worklist the repair
re-measured (911 pairs, 905 REFUSE / 6 ADMIT → 904 / 7). The repair regenerated the gate's
worklist at runtime and left this dated file untouched. ⇒ **its 529 artifact-compatible
rows still carry PRE-FIX numbers.** Anyone wanting current verdicts must **re-run
`tools/comdat_fold_gate.py`**, not read this file — the same "dated artifact mistaken for
current state" trap that W16-EC hit with `NOGROUP1.tsv` (313 rows → 91 live / 102 dead /
120 absent) and with the renamer docstring.

★ And even a favourable fold signal at `0x822a3010` would be weak: **EB's own §5 measured
its owner-hit at 400 of 1,219 units — ~33% likely by chance**, so "the owner's obj defines
a byte-identical body" is very nearly vacuous for this row. EB flagged that itself. The
row has two independent reasons not to be funded, and neither is "we checked and it folds".

### W16-EE landed — a map TRANSPOSITION, and the vein behind it is drained (2026-09-16)

**`35188831`, +2 fns / +280 B, gate reproducing the pre-registration on every key.**
Two `BandTrack` members had their names swapped in `target_symbol_map.json`
(`0x8234dd18` / `0x8234dd30`, `UnisonStart` <-> `UnisonEnd`); both VAs were flagged
`_bijection_arbitrary`, i.e. **the map itself recorded that which name went on which
VA was never established.** The two rows that crossed are the *callers*
(`TrackPanelDir::UnisonStart` / `UnisonEnd`, 140 B each) — the mis-named rows
themselves read 100 throughout. **A wrong name is financed by its callers.**

★ **Both named failure modes were REFUTED, not merely untested.** The 20 B
`BandTrack` rows held at 100 (so `name_check`'s placeholder forgiveness applied —
their sole relocation targets are unnamed), and `SPLIT lines: 2` proves the
re-split ran, so the edit was live rather than the silent
`[APPLIED] ... 0 files patched` inert case. Had forgiveness not applied the row
would have read **+240 B / +0 fns**, which is why it was priced in advance.

★★ **ALIAS_SUSPECT fired and is a FALSE POSITIVE here — the reasoning generalises.**
`ab_measure` flags any map-only patch whose `none` leg is flat, because that is the
shape a *fabricated alias* makes. But `none` is structurally incapable of
registering a relocation-NAME change, so **its flatness carries no information in
either direction** — it cannot clear the patch and it cannot condemn it. What
separates the two cases is that **a transposition is SELF-INVERSE**: the wrong
orientation is the status quo and scores *worse*, so a swap cannot manufacture
agreement the way installed forgiveness can. In-tree precedent is the same shape
and the paying class: **MAPDEF-3 (`db9eb318`) measured +108 B from 9 wrong-name
repairs with `none` unmoved at +0.** No alias was touched (`symbol_aliases.json`
unchanged at 1,657 groups / 5,474 folded).

⛔ **A vacuous discriminator the lane caught in its OWN reasoning, which would have
sent the swap BACKWARDS.** "`fn_82305B00` never touches `r4` => it takes no int" is
false — `r4` is passed *through*, and **both siblings leave it alone**, so the test
cannot separate them at all. The evidence that does work is asymmetric:
`EndingBonus::UnisonStart(int)` consumes `r4` while `UnisonEnd` **clears** the
`+0x1dd` flag `UnisonStart` tests-and-sets; corroborated by member order taken from
**the compiler** (`UnisonIcon mStartTrig@0x1e0` / `mEndTrig@0x1e4`), not from
`// 0xHEX` comments.

⚠ **W16-EC's `PropKeys::Copy` verdict HOLDS but its stated MECHANISM does not.** EC
carried the 0-byte price by analogy to `PropSync`'s `_M_fill_insert_aux` story;
measured, both halves charge the identical `Key<float>` survivor, so **it is not a
2-cycle at all**. A right number for the wrong reason is what rots into a bad brief
two lanes later — the corrected mechanism is the deliverable, not the matching zero.

⛔ **DO NOT RE-FUND THE `_bijection_arbitrary` VEIN.** A mechanical 2-cycle detector
over all **54 census rows / 3,476 B** returns **exactly two** transpositions: this
one, and `PropSync` (+640 B nominal) which is priced at **0** because both halves
charge a shared `_M_fill_insert<vector<Object*>>` survivor — independently
confirming EC. Only **280 B of 3,476 B (8.1%)** of the stratum was ever adjudicable;
12 rows are unpaired and 16 are <=12 B thunks/stubs. The census reconciles EC's
57 rows / 3,728 B **exactly** by the 3 rows EC itself fixed.
⚠ Three artifacts state three different `_bijection_arbitrary` sizes (**1,008**
actual vs docstrings' 939 and 1,109), and **49 of the 1,008 VAs carry no name at
all** — another instance of a dated figure surviving in prose after the artifact moved.

**New baseline for every subsequent pre-registration: 43,952 fns / 4,122,880 B**
(`~/tmp/rows_w16ee.json`).

### Wave 16 E-series CLOSED — three lanes, three gates, three pre-registrations (2026-09-16)

| lane | commit | delta | pre-registration |
|---|---|---|---|
| W16-EE | `35188831` | +2 / +280 B | EXACT on every key; both named failure modes REFUTED |
| W16-EF | `883430f5` | +3 / +408 B | EXACT, including the row-count/function-count split of exactly 1 |
| W16-EG | `50d494c8` | +1 / +412 B | EXACT; the header-cascade risk REFUTED |
| **wave** | | **+6 fns / +1,100 B** | 43,950 / 4,122,600 -> **43,956 / 4,123,700** |

★★★ **THE BYTES ARE THE SMALL HALF. Each lane's MISS corrected a reusable belief:**
- **W16-EF** — `SetRealtime` was predicted NOT to cross on 13 register charges; **all 13
  dissolved once ONE structural defect was fixed** (`addi r29, r28, 0x6c`). **13th recorded
  instance** that `REGISTER_SWAP` is a symptom, not a diagnosis.
- **W16-EG** — a 12-charge cluster read as a misplaced statement; it was **MSVC cold-block
  outlining**. **A charge cluster is not evidence of a source construct.**
- **W16-EF** — W16-CB's falsifier *"a wrong identification lands near 0"* is **necessary but
  not sufficient**: a CORRECT name on a STUBBED body also lands near 0. Acting on it would
  have **retracted a correct name**.

★★ **Two coordinator dispatch hypotheses were REFUTED BY THE LANES, which is the system
working**: EG's 0% row was not a phantom (it is an exact `.pdata` BeginAddress, and
`mpn = 2.378641` proved pairing before any parse — **a truly unpaired row reads 0 on BOTH
keys**), and the `symbol_aliases.json` collision I predicted between EE and EF never
existed (EE touched only `target_symbol_map.json`; EF's alias file was untouched on main).

⚠ **Carried forward, NOT closed:** `?OnMsg@Game@@...ButtonDownMsg@@@Z` (**1,664 B**, name
now adjudicated correct, prologue already written and proven as `ButtonUpMsg`'s twin) —
the best-evidenced open row in the tree. `OnSetShuttle` 260 B priced and left (needs a
second alias). `fn_826259E0` 332 B + `fn_82624F88` 272 B are map work. CB handoffs #2
(EH-state lever) and #5 (practice-mode home) still open.

⛔ **CLOSED — do not re-fund:** the `_bijection_arbitrary` vein (2-cycle detector over all
54 rows returns exactly two; one landed, one priced at 0 by a shared fold survivor).

### ⛔ CORRECTION — my own `decomp.db is STALE` claim is REFUTED TWICE (2026-09-16)

I recorded earlier today that `decomp.db` was **stale for want of a re-ingest**, on the
evidence that `query_functions(status='workable')` returned rows printed as
`Match: 100.0%`. **Both halves of that are wrong**, and the correction runs OPPOSITE to
the claim.

**1. The database was not stale.** I ran the re-ingest
(`venv/bin/python scripts/ingest_report.py build/45410914/report.json`): **69,239
symbols, 67,804 updated, 1,436 inserted, 1,722 marked dead, 184 revived** — and the
symptom was **completely unchanged**. A refreshed DB reproducing the complaint exactly
refutes staleness as the cause.

**2. The rows were never at 100%.** Read from the DB directly:

| symbol | `current_percent` | displayed |
|---|---:|---|
| `?Load@PatchPanel@@UAAXXZ` | **99.99** | `100.0%` |
| `?NewObject@PracticePanel@@SAPAVObject@Hmx@@XZ` | **99.96** | `100.0%` |
| `?GetBandLogoTex@BandProfile@@QAAPAVRndTex@@XZ` | **99.95** | `100.0%` |

The listing formats with `.1f` (`mcp_server.py`, `f"{r['percent']:.1f}%"`), so **anything
at or above 99.95 prints as `100.0%`**.

★★★ **THE HAZARD IS THE OPPOSITE OF WHAT I WROTE.** These are the rows **CLOSEST TO
CROSSING** — the single best candidate class there is, since `matched_code` is
all-or-nothing per row and one charge separates them from paying their full size. The
display renders them **indistinguishable from finished work**, so a coordinator scanning
candidates skips exactly the rows most worth funding. I did precisely that and then wrote
down the wrong reason for it.

**What DOES survive, on a different mechanism:** `workable` excludes on **`verdict`**, not
on percentage (`unworkable_verdict_clause`, `database.py:78`). Over live rows the verdict
distribution is **60,135 NULL / 6,328 COMPLETE / 2,776 AT_LIMIT**, and **10,350 live rows
sit at `current_percent >= 100` with `verdict IS NULL`** — genuinely finished rows that
`workable` therefore returns, because `verdict` is agent-maintained via `report_result`
and most rows never receive one. ⇒ **"do not use `workable` alone as a candidate filter"
stands; "because the DB is stale" does not.**

⚠ **This is the same failure I have been crediting lanes for catching, committed by me**:
a right conclusion resting on wrong evidence (cf. W16-EE correcting W16-EC's
`PropKeys::Copy` **mechanism** while confirming its **number**). A reproducing symptom is
not evidence for its proposed cause — the cause has to be tested separately, and here the
re-ingest was the test that refuted it.

### The near-crossing stratum — the population the `.1f` display concealed (2026-09-16)

Directly downstream of the correction above. Once the display-rounding mechanism was
understood, the obvious question was **how much sits in the band `.1f` renders as
`100.0%`**. Censused from `report.json` at `ec15a785`, graded `name_check`:

| band | rows | bytes |
|---|---:|---:|
| whole binary, `fuzzy >= 99.95 < 100` (**prints as `100.0%`**) | 147 | **118,340** |
| whole binary, `fuzzy >= 99.5 < 100` | 4,576 | 580,140 |
| **game layer** (`band3/`+`network/`), `>= 99.95` | **22** | **17,052** |
| game layer, `>= 99.5` | 396 | 73,576 |

★★★ **The decisive number is not the size — it is the COMPOSITION.** Splitting the
game-layer band on `match_percent_normalized`, which separates instruction-level
penalties from relocation-name (arg-only) ones:

| band | source-reachable (`mpn < 100`) | arg-only (`mpn == 100`) |
|---|---:|---:|
| game `>= 99.95` | **22 rows / 17,052 B (100%)** | **0 rows / 0 B** |
| game `>= 99.5`, named | 209 rows / 60,028 B (90.6% of bytes) | 13 rows / 6,268 B |

⇒ **This stratum is the mirror image of the `mpn==100 / fuzzy<100` stratum**, which
MPNGAP-1 measured as **~91% irreducible** fold/map noise and explicitly told us not to
re-fund as a byte lever. Here the ratio inverts: at `>= 99.95` **every single row** is
instruction-level, i.e. reachable by ordinary source work.

⚠ **And the anonymous rows are worth almost nothing** — in the `>= 99.5` game band the
174 `fn_*` rows total **7,280 B** against the 222 named rows' **66,296 B**. Rank named.

**Why nobody worked it:** the concealment is *selective in the worst possible
direction*. A coordinator scanning candidates sees `100.0%` and skips — so the rows
skipped are precisely the ones **one or two charges from paying their full size**,
since `matched_code` is all-or-nothing per row. The last three lanes (W16-EE/EF/EG) were
all pointed at map/alias work in the drained stratum while this one sat unread.

⇒ **Dispatched against it (2026-09-16):**
- **W16-EI** (opus) — `?MaybePublish@UIStats@@QAAXPAVUIScreen@@@Z`, 2,604 B, fuzzy
  99.57911 / mpn 99.6559. The largest source-reachable near-crossing row in the game
  layer; source present (318 lines), no lane doc owned it.
- **W16-EJ** (opus) — the full 22-row `>= 99.95` band sweep, briefed to rank by
  **bytes-per-charge-to-close rather than by size**, because a partially improved row
  pays exactly zero bytes.

⚠ **Both briefs carry the standing warning to test the figures literally** rather than
inherit them — the same rule whose violation produced the correction immediately above
this section.

### ⛔ CORRECTION (same day, ~1 h later) — my `mpn < 100` screen does NOT certify source-reachability

**W16-EI refuted the section immediately above, on its own largest member class, and the
error is mine.** I wrote that the near-crossing band is *"100% source-reachable"* because
every row screened `match_percent_normalized < 100`. **That screen does not support the
claim.**

**The mechanism, stated correctly this time:**

- `mpn < 100` proves **at least one instruction-level charge EXISTS** (immediates and
  offsets are charged to `mpn` as well as to `fuzzy`).
- It proves **nothing** about whether the row *also* carries relocation-name charges.
- `matched_code` keys on **`fuzzy == 100`**, so **every** charge must close for the bytes
  to pay. One unclosable relocation-name charge withholds the row's full size no matter
  how much source work lands.

⇒ the screen is valid in **exactly one direction**:

| reading | verdict |
|---|---|
| `mpn == 100` | **all** charges are arg-only ⇒ genuinely not source-reachable. **SOUND.** |
| `mpn < 100` | instruction-level charges exist. **CERTIFIES NOTHING ELSE.** |

**The evidence (W16-EI, `?MaybePublish@UIStats@@QAAXPAVUIScreen@@@Z`, 2,604 B, fuzzy
99.57911 / mpn 99.655914):** 65 of 652 instructions charged — 54 offset/immediate, 10
register, 8 symbol, 1 insert, 1 replace. **Six of the eight symbol diffs target
`lbl_<hex>` placeholders, which `name_check` forgives** (objdiff-core `code.rs:1006`);
the **two** that are really charged name **ICF fold survivors**, and no source edit can
change which name the retail map holds for an address. Verified independently by this
coordinator: `0x82801f78` → `?GetContainerName@MemcardXbox@@UAAPBDXZ` and `0x827c1378`
→ the `vector<int>` **copy** ctor (`??0?$vector@HV?$StlNodeAlloc@H@…@@QAA@ABV01@@Z`),
with **no `??0?$vector@PAVBandUser…` copy-ctor spelling anywhere in the map.**
⇒ closing every instruction-level charge on that row buys `mpn` 100 (**+1 function**) and
**exactly 0 bytes** — the documented `CustomizePanel` shape. Reported **AT_LIMIT**, and
the lane declined to install the two aliases after sizing them at **zero collectable
rows**.

**What the census actually licensed, restated honestly:** *0% of the band is PURELY
arg-only.* That is a real and useful result — it excludes the stratum MPNGAP-1 measured
~91% irreducible — but it is an **exclusion, not a certification**. The band is worth
working; its yield is **not** 17,052 B and is not yet known.

⚠ **SCOPE, stated precisely so this correction is not itself over-read:** MaybePublish
sits at fuzzy 99.579, i.e. in the **wider `>= 99.5` band**, not in the 22-row `>= 99.95`
set. What is refuted is **the screen**, demonstrated on a row the screen admitted. The
22 rows have **not** been individually adjudicated — W16-EJ is doing exactly that now,
and was sent this correction mid-flight.

★ **The right screen is a per-row CHARGE CLASSIFICATION**, which `report.json`'s two
percentages cannot supply: each charge is (a) instruction-level, (b) a relocation name
targeting a `lbl_`/`fn_`/`data_` **placeholder** — forgiven, costs nothing, or (c) a
relocation name targeting a **real** name — a fold survivor or wrong callee, **not
closable by source**. **Any row with one class-(c) charge is alias-gated and pays zero.**

⚠⚠ **This is the SECOND time today I have read a NECESSARY condition as a SUFFICIENT
one** — the first being the `decomp.db` correction two sections above ("a reproducing
symptom is not evidence for its cause"). Same family: a screen that establishes
**existence** was read as establishing **exclusivity**. Recorded as a coordinator failure
mode, not a lane one: both times the screen was cheap, the conclusion was convenient, and
**no control was run that could have failed.**

### W16-EI landed — a refutation worth more than the bytes it did not buy

Gate: `c2e92d00`, **Δ0 on every key** (43,956 fns / 4,123,700 B unchanged, 0 crossed in,
0 fell out), `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0
failed=0 rc=0`, provenance unchanged. The pre-registration predicted exactly that and
named two ways it could fail — **both refuted**: the comment-only line shift perturbed no
codegen (0 `__LINE__` / `__FILE__` in the 42 added lines, and leg B recompiled the TU with
`msvc=1`, the falsifiable field), and the `6c087cbd` native-link shape was *structurally
impossible* (0 `#if`/`#endif`/`#include` added; the file carries **zero** line-anchored
directives).

★ **The lane's value is entirely in what it refuted, and the metric cannot see it.** A Δ0
gate says the commit is inert; it says nothing about whether the verdict is right. That
was checked separately, by this coordinator, on four map facts — all confirmed, though
**the first probe was itself defective** (it assumed `symbol_aliases.json`'s `groups` was
a dict when it is a list, and piped `grep` into `sort | head`, which makes a `|| echo`
fallback unreachable — the documented `cmd | tail; echo rc=$?` vacuity). The claim only
verified on the second attempt. ⚠ **A coordinator spot-check that "fails" is as likely to
be the instrument as the claim** — re-probe before contradicting a lane.

★★★ **Two reusable findings, both about instruments that mislead in the same direction:**

1. **`fuzzy%` DOES NOT TRACK MISMATCH COUNT.** 65 of 652 instructions charged — **10.0% of
   the function** — still reads **fuzzy 99.58**, because a `diff_arg` costs ~0.006 pp.
   `diagnose`'s own naive estimate for the same state is **90.0%**. ⇒ **never infer "nearly
   done" from a high fuzzy, or "few charges" from one.** This is the precise mechanism by
   which this row was re-briefed as almost-finished **five times**.
2. **`run_diff_inspect` mode `mismatches` IS NOT A CENSUS.** It reported *"65, all
   `diff_arg`"* where `diagnose` on the **same ruler** shows **63 `diff_arg` + 1 insert +
   1 replace**. Cross-check the two modes before classifying a row's charges.

✅ **And a stale in-tree caution is retired:** a prior lane's note that `stack-layout`'s
frame line is a vacuous `0 == 0` is **no longer true** — both bugs it filed are fixed
(`TGT 0x102f0 BASE 0x102f0`, `stwux` evidence on both sides, GPRs 18/18, plus self-disclosed
fingerprint degeneracy). The in-source note now says the instrument works. ⚠ A caution that
outlives its defect **actively costs yield** — it tells lanes to distrust a working tool.

⚠ **What was deliberately NOT done, with the sizing that justifies it:** the two aliases
were **not installed**. Both forgive charges at **zero collectable rows** — the 0-arg
`GetBandUsers` has two call sites (this row, which pays 0 regardless, and
`Game::PopulatePlayerLists`, absent from all 29,553 `target_symbol_map.json` rows *and*
from its unit's 356 named functions, hence unpairable since objdiff pairs by name), and
the `vector<BandUser*>` copy construction occurs **exactly once in the tree** — this row.
⇒ installing them would have lifted `name_check` **by construction** while creating no
byte agreement: the documented fabricated-alias integrity hazard, avoided by sizing first.
A proof path is recorded for a future lane.

### ⛔ CORRECTION — two claims in the W16-EF row are REFUTED on retail bytes (2026-09-16)

**W16-EH decoded the retail bytes the W16-EF row reasoned about, and refutes two of its
statements.** EF's row is left as written — it is a dated record and its *verdict* stands
— but the two claims below must not be inherited, because one of them actively invites
wrong code.

**1. ⛔ "The prologues are identical but for ONE instruction" — FALSE.**
`OnMsg(ButtonUpMsg)` and `OnMsg(ButtonDownMsg)` are not near-twins:

| | ButtonUp | ButtonDown |
|---|---|---|
| guard | tests `> 0` before decrementing, **re-loads `0x48(this)`** | **increments unconditionally**, single load |
| head length | 9 instructions | **5** |
| `lwzx`/`stwx` operand order | — | **reversed** |
| frame size | `0x80` | **`0xe0`** |
| save range | `__savegprlr_28` | **`__savegprlr_23`** |
| FPR saves | — | **differ** |

★★★ **The consequence is not cosmetic: copying UP and flipping `--` → `++` produces WRONG
CODE**, and that is precisely what the row's phrasing invites the next lane to do. A
"differs by one instruction" claim is a *targeting instruction*, so its falsity costs
real budget — this is the same failure class as the `fuzzy%`-tracks-mismatch-count
inference that got `MaybePublish` re-briefed five times (W16-EI, two sections above).

**2. ⛔ "The call site passes `this+0xe0`" — FALSE.** Retail does `lwz r3, 0xe0(r31)` — it
**LOADS the pointer** rather than passing the address of the field. ⇒ our
`Shuttle *mShuttle` member is **correct**. ⚠ **EF's verdict survives; EF's stated reason
does not** — exactly the shape this roadmap keeps recording (right conclusion, wrong
evidence), and the reason must not be reused as an oracle for a neighbouring row.

**3. Construct list incomplete** — `audition_jump_forward_ms`, `audition_jump_back_ms` and
`audition_jump_end_buffer_ms` were unnamed in the row's census.

⚠ **W16-EH deliberately did NOT edit the EF row itself** — another lane's record, outside
its staged paths. Correct. The correction is recorded here instead, by the coordinator.

### ⛔⛔ THIRD CORRECTION, same rule — `mpn < 100` implies NOTHING, not even that an instruction-level charge exists

**W16-EJ refuted the correction two sections above, which had itself refuted the section
above that. Three wrong screens in one day, all mine.** Recording the full sequence because
the *pattern* is the finding, not any one error.

| # | my claim | refuted by | how |
|---|---|---|---|
| 1 | `mpn < 100` ⇒ **source-reachable** | W16-EI | row carried unclosable fold-survivor charges |
| 2 | `mpn < 100` ⇒ **at least one instruction-level charge exists** | W16-EJ | **false on objdiff 4.2.9** — see below |
| 3 | drop any row with a class-(c) charge | W16-EJ | would have discarded **the only row that crossed** |

**The mechanism, and it was already in our own docs.** objdiff-core **`b14ba45`**
(2026-08-20) added `vetted_reloc_name_diff`: under `name_check` **only**, a relocation-name
diff passing three screens is **excluded from `arg_diff_score`** while staying in
`diff_score`. Since `mpn = diff_score − arg_diff_score`, such a charge **no longer cancels
and lands squarely in `mpn`**. ⇒ a row can sit at `mpn < 100` with **zero** instruction-level
charges. Measured across the 22-row band: of **26 charged sites, 24 are relocation-name and
only 2 are instruction-level** (both immediates); **zero** Replace, **zero** insert/delete.
**20 of 22 rows have no instruction-level charge at all.**

⚠⚠ **CLAUDE.md ALREADY RECORDS `b14ba45`** — as the reason `matched_functions` stopped being
ruler-invariant (lane W16-AR, 2026-09-14). Its consequence **for screening** had simply never
been drawn. ⇒ this was not new information; it was **unread information**, in the file whose
standing rule is `READ THE IN-TREE RECORD FIRST`. I wrote that rule into two briefs the same
day I violated it.

★★★ **The tell was FREE and sat in the data I already had: `mpn == fuzzy` to the digit on all
22 rows.** That means `arg_diff_score == 0` — which does **not** mean "no relocation charges",
it means **"every relocation charge present was VETTED and promoted into `mpn`."** I read
`mpn < 100` as a signal and never noticed the two keys were *identical*, which is the actual
diagnostic.

**⛔ And correction #3 was actively harmful.** I told W16-EJ: *"any row with even one class-(c)
charge cannot reach fuzzy 100 by source work — drop it immediately."* `VocalGuidePitch` was
exactly class (c) — real names on both sides (`NoteAt__13VocalNoteListCFf` vs
`?NoteAt@VocalNoteList@@QBAPBVVocalNote@@M@Z`) — and it closed **in a three-line edit** for
**+572 B**. ⇒ class (c) MUST be split:

- **fold survivor** — not closable by source;
- **wrong callee / our own bug** — closable, and per MPNGAP-1 **the most valuable class we
  have**.

**`tools/icf_pair_adjudicate.py` on retail bytes is the instrument that splits them. A
name-shape test cannot**, and my rule was a name-shape test.

★ **WHAT ACTUALLY SURVIVES, stated minimally:**

| reading | verdict |
|---|---|
| `mpn == 100` | no instruction-level charges ⇒ not source-reachable. **STILL SOUND.** |
| `mpn < 100` | **CERTIFIES NOTHING.** Not reachability, not even that an instruction-level charge exists. |
| any percentage pair | **no substitute for per-row charge ENUMERATION**, classified by KIND |

★ **A charge-counting instrument that needs no objdiff run** (W16-EJ, validated to 5 decimals
on two rows): **`diff_score = (100 − fuzzy) × size / 4`**. ⛔ **Corollary that kills the
ranking I briefed:** "bytes per charge" ranks **identically** to "fuzzy descending", because
`diff_score` is already size-normalised. The ranking that matters is **by charge KIND**, which
only enumeration gives.

⚠⚠ **The pattern, which is the real lesson:** three times in one day I substituted a cheap
percentage screen for enumeration, and each time the screen **agreed with the data I had** —
because each was a *necessary* condition read as *sufficient*. **Every one was refuted by a
lane that did the enumeration.** There is no cheap substitute. Coordinator briefs must ship
the enumeration requirement, not a percentage rule — and must state that a lane refuting the
brief is a successful outcome, which is the only reason all three refutations came back
instead of being quietly worked around.

### W16-EH landed — two bugs nobody was looking for, and a briefing claim that invited wrong code

Gate `e16ea909`: **Δ0 on `matched_functions` and `matched_code`** (43,956 / 4,123,700),
`Δfuzzy` **+0.000795 pp** (50.006115 → 50.00691), 0 crossed in, **0 fell out**,
`NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.
The pre-registration predicted every one of those, including the fuzzy delta to six decimal
places, and named three ways it could fail — the live one being **collateral in `Game.cpp`'s
unit**, whose function sizes `Handle`'s frame (`0x140`) and pairs its 21 EH funclets.
**Refuted: 0 fell out.**

★ **The signature to internalise: `fuzzy` rose while `matched_code` did not move at all.**
The 1,664 B row went fuzzy **3.2380 → 8.009615** (mpn 3.3702 → 8.262019) and that is worth
**exactly zero bytes**, because `matched_code` is all-or-nothing per row. A unit's largest row
tripling its fuzzy is **the most seductive kind of nothing**, and the lane said so itself
rather than letting the number speak.

**TWO REAL BUGS, neither of which the metric could have found:**

1. **`Game.cpp` — a live behavioural asymmetry.** The per-pad counter `OnMsg(ButtonDownMsg)`
   increments is the same one `OnMsg(ButtonUpMsg)` decrements. Our ButtonDown body was a
   **stub**, so **the release handler has been decrementing a counter nothing ever raised.**
   ⚠ The reconstructed head calls `msg.GetUser()->GetPadNum()` **without** the null guard the
   stub carried. That is what retail does and matching retail is the goal, but it is a genuine
   behavioural difference on the **native** track that the native gate **cannot see** — it
   builds and links, it does not run input handling. Recorded so it is not later found as a
   mystery crash.
2. **`Shuttle::SetActive` was declared in `Shuttle.h`, called from `OnSetShuttle`, and defined
   NOWHERE** — an unresolved external, so we compiled no COMDAT for it. ⇒ this is why
   `fold_thunk_gate.py` refused the 260 B fold **correctly** on its first pass ("no COMDAT for
   `?SetActive@Shuttle@@QAAX_N@Z`"): with no body on our side there was nothing to compare.

**The 260 B alias is a RECORDED REFUSAL, not an oversight.** The fold is proven on raw bytes
(`988300084e800020`, zero relocations both sides, retail `0x826f07b8`) — the lane never cites
`FT-EMPTY` as proof, correctly, since that tier self-declares its byte test vacuous. The
surviving refusal is a **one-sided instrument error**: retail relocations are **inferred by
instruction form** (`stb` ∈ `IMM16_OPS` ⇒ `targets[0]=None` unconditionally) while ours are
**read from COFF** (correctly empty), so `set(rt) != set(ot)` fires on **pure asymmetry** after
the masked words already compared equal; `mask_word` also zeroes the IMM16 displacement, so the
gate **never verified the `+8`** either. **Blast radius measured: 0 of 29 house-worklist
REFUSEs share this reason** — it blocks exactly one pair, ours. The lane diagnosed it with a
proposed fix rather than patching shared admission logic against 1,657 live groups late in
budget. ⇒ **dispatched as W16-EK**, now that the missing COMDAT exists on main.

★ **One instrument catch worth carrying forward:** reading a row as
`f.get('measures',{}).get('fuzzy_match_percent',0)` returns a confident **`0` for every row** —
the keys are **flat on the function object**, and protobuf-JSON **omits defaults**, so a wrong
key and a genuine zero are **indistinguishable**. Caught only because a unit-level read in the
same pass returned real values. ⚠ **On a row you hope you improved it reads as disaster; on a
stubbed row, as confirmation** — i.e. it fails in whichever direction you were expecting.

### W16-EJ landed — the band is 24/26 relocation charges, and the screen that funded it is dead

Gate: **43,956 → 43,957 fns / 4,123,700 → 4,124,272 B** (+1 / **+572 B**), code%
40.242733 → **40.248314**, `fuzzy` **unmoved**, 0 fell out,
`NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.
One row crossed and the pre-registration **named it**:
`default/band3/game/VocalGuidePitch::?Poll@VocalGuidePitch@@QAAXM@Z`.

★ **This is the first lane of the wave whose ABSOLUTES WERE UNUSABLE**, and the
pre-registration had to say so. EJ branched at `ec15a785`, predating both W16-EI and
W16-EH, so its leg A measured a tree main no longer has. The prediction was therefore an
**arithmetic composition** — `baseline + EJ's measured delta` — defensible only because
both intervening lanes are metric-inert on the keys that matter (EI Δ0 on every key; EH Δ0
on `matched_functions`/`matched_code`). The named failure mode was exactly that: *"if the
gate lands anywhere other than 43,957 / 4,124,272, the composition assumption is what
failed — not necessarily the patch — and the two must be distinguished."* It landed on the
nose. ⇒ **deltas compose, absolutes do not** — stated as a live constraint rather than
inherited folklore, and tested.

**THE SUBSTANTIVE RESULT IS A REFUTATION OF MY OWN BRIEF, AND IT IS WORTH MORE THAN 572 B.**
I dispatched EJ on the claim that all 22 rows were source-reachable *because* all 22 read
`mpn < 100`. Enumerated: of **26 charged sites across the 22 rows, 24 are relocation-name
and only 2 are instruction-level** (both immediates) — **zero** Replace, **zero**
insert/delete — and **20 of the 22 rows carry no instruction-level charge whatsoever.**

★★★ **THE MECHANISM, WHICH WAS ALREADY IN THIS TREE AND I DID NOT READ IT.** objdiff-core
**`b14ba45` (2026-08-20)** added `vetted_reloc_name_diff`: under `name_check` only, a
relocation-name diff passing three screens is **excluded from `arg_diff_score`** while
staying in `diff_score`. Since `mpn = diff_score − arg_diff_score`, it **no longer cancels
and lands in `mpn`**. Confirmed at `diff/code.rs:1793` — a single guarded `arg_diff_score
+=` site. **W16-AR already recorded `b14ba45`** as why `matched_functions` stopped being
ruler-invariant. It was unread information, in the file whose standing rule is
**READ THE IN-TREE RECORD FIRST**, which I had written into two briefs the same day.

★ **The free tell, worth keeping:** `mpn == fuzzy` to the digit ⇒ `arg_diff_score == 0`,
which does **not** mean "no relocation charges" but **"every relocation charge was vetted
and promoted into `mpn`."**

⇒ **SURVIVING RULE:** `mpn == 100` ⇒ no instruction-level charges ⇒ not source-reachable.
**`mpn < 100` certifies NOTHING.** No percentage pair substitutes for **per-row charge
enumeration by kind**.

⚠ **And my third screen the same day died here too:** "drop any row carrying a class-(c)
charge" would have discarded **VocalGuidePitch — the only row that crossed.**

**A charge-counting instrument needing no objdiff run:** `diff_score = (100 − fuzzy) × size / 4`,
validated to five decimals. **Corollary: "bytes per charge" ranks IDENTICALLY to "fuzzy
descending"**, because `diff_score` is already size-normalised — so that ranking buys nothing
over the sort we already had.

**DELIBERATELY NOT LANDED — 17 `--chase`-proven folds, 13 rows / 10,136 B.** `--chase`
relaxes relocation-target *name* equality to a recursively verified equivalence, and it is
**not one of the declared T1/T2/T3 evidence tiers** `tools/icf_alias_build.py` implements;
the JSON calls itself the source of truth yet **its own pipeline cannot re-derive a chase
membership**. Landing them would install forgiveness the generator could never reproduce.
Sized and handed up rather than taken — the correct call, and the single biggest uncollected
item in the band.

### ✅ RESOLVED COORDINATOR DECISION — `--chase` must NOT become a declared alias evidence tier

**Handed up by W16-EJ (2026-09-16), sized, and deliberately NOT decided yet.**

W16-EJ proved 17 ICF folds via `tools/icf_pair_adjudicate.py --chase` and **did not land
them**, worth **13 rows / 10,136 B** in the near-crossing band alone. The list is in
`~/tmp/w16ej/chase.txt`. Its reasoning, which I am recording rather than overriding:

* `--chase` relaxes relocation-target **name** equality to a *recursively verified*
  equivalence. That is a real proof technique, and its `--selftest` and `--chasetest`
  controls were both run and both discriminate.
* But `--chase` is **not one of the declared T1/T2/T3 evidence tiers** that
  `tools/icf_alias_build.py` implements, and **`icf_alias_build.py` does not pass
  `--chase`**. So `symbol_aliases.json` — which calls itself the source of truth — **could
  never re-derive a chase membership**. Landing them installs forgiveness the generator
  cannot reproduce.

⚠ **Why an alias is not a free win:** an alias is *forgiveness* — objdiff consults
`SymbolEquivalences` and DROPS the charge — so **an unproven alias lifts the score BY
CONSTRUCTION**. The `none` control **cannot** catch a fabricated one (`none` ignores
relocation names, so it reads +0 there by construction), and **that flatness is the SIGNATURE
of the hazard, not a clearance.** Ablation has sized this whole mechanism at **818,416 B /
7.93 pp — ~22% of everything we count as matched already rests on it.** That is the reason
to be slow here, not the reason to refuse.

**What would settle it, in order:**
1. **W16-EK first** — it is adjudicating a *different* defect in the adjacent fold gate
   (`fold_thunk_gate.py`'s one-sided relocation derivation). If the fold-gate instrument turns
   out to be less trustworthy than believed, the `--chase` question changes shape. **Do not
   decide this before EK reports.**
2. Then: either teach `icf_alias_build.py` to emit `--chase` as a declared **T4** tier (so the
   pipeline can re-derive what it ships), or reject `--chase` as evidence and record why.
   **What is NOT acceptable is landing chase-derived memberships under a T1/T2/T3 label** —
   that would make the tier labels lie, and the tier labels are the only thing standing
   between us and metric-fitted forgiveness.

⇒ **Status: OPEN. 10,136 B parked deliberately.** Whoever picks this up: it is a *pipeline*
task, not a byte grab, and the bytes are the least interesting part of it.


### W16-EL landed — the brief named a container type, and retail's symbol table spells it out

**Merge `a50b1e19`, doc-only, Δ0 on every key** (43,957 fns / 4,124,272 B / code% 40.248314 / fuzzy 50.00691,
`CROSSED IN 0 / FELL OUT 0`, native `PASS 18/18 skipped=0`). That Δ0 is the pre-registered result, not a
disappointment: EL was dispatched to *change* a member's key type and came back having proved it must not be
changed, so the only honest landing is the record.

**What was briefed.** W16-EJ handed off six fold-REFUTED pairs and named `AccomplishmentProgress` the top
candidate: a single charge, `addi r3, r30, 0x64c`, reaching `mGigTypeCompletedMap` — declared
`std::hash_map<int, int>` in our header — against a retail spelling believed to be `hash_map<Symbol, int>`.
588 B behind one charge is the best shape in that handoff.

**What EL measured.** Retail does not require inference here; its symbol table names the instantiation.
`SaveFixed` at `0x825909b8` calls `SaveStd<Symbol,H>` three times — once for each sibling map — and then calls
`fn_82590198`, which is `SaveStd<H,H>` and whose parameter type is written out as `const hash_map<int,int>&`,
for the `+0x64c` member. `LoadFixed` mirrors the same split at `0x82591420`. The member's `operator[]` and
`_M_find<int>` are distinct symbols from the siblings' `_M_find<Symbol>`. **Our declaration is correct and the
brief was wrong.**

★ **The control is what makes this a measurement rather than a reading.** The three sibling maps
(`mToursPlayedMap` `0x5f8`, `mTourMostStarsMap` `0x614`, `mToursGotAllStarsMap` `0x630`) really *are*
`Symbol`-keyed, and the same instrument says so. An instrument that returned "int-keyed" for all four would
have proved nothing about any of them.

⚠ **No A/B was run, and that is a result rather than a gap.** The proposed change is **not expressible**:
`AccomplishmentProgress.cpp:549` does `mGigTypeCompletedMap[i + 0x3E8] = 0;` across 50 iterations, and an
integer literal cannot index a `Symbol`-keyed map. There is no patch to price. Had it compiled, the economics
were already adverse — **2,764 B of rows that are currently perfect would have been put at risk to chase
588 B**, and `SaveFixed`/`LoadFixed` break by construction under the other key.

**The 588 B is real but blocked for an unrelated reason**, which is worth recording so the next lane does not
re-open it as a container-type question: our hashtable-constructor spelling is *already* placed at
`0x825a07e0` by an existing 11-member alias group, while retail's call goes to `0x8255d480`, which branches to
`0x8255c968`. The charge is an address disagreement about a shared template body, not a type error in our
source.

★★★ **And EL refuted a rule of thumb this campaign has leaned on: "byte-identical ⇒ folded" is FALSE here.**
Retail `0x8255c968` and `0x825983d8` are byte-identical **including their `bl` target** — the exact condition
`/OPT:ICF` folds on — and retail nonetheless keeps them at two distinct addresses. So observing that two
bodies meet the folding condition does **not** license concluding the linker folded them, and an alias
proposed on that reasoning alone is unproven. This matters because an alias lifts `name_check` **by
construction** while the `none` control provably cannot catch a fabrication.

**Deliberately not done:** EL did not install any alias, did not touch `AccomplishmentProgress.h`, and did not
rewrite the two consumer sites at `src/band3/net_band/RockCentral.cpp:1193-1194`.

⇒ **CLOSED — do not re-fund the `AccomplishmentProgress` key type.** The remaining four live fold-REFUTED
pairs from EJ's handoff (NetSession 460 B; TrainerGemTab 656 B; TourChallengeResultsPanel 1,392 B;
RGTrainerPanel 1,220 B) are untouched by this and stay queued; `StoreMenuPanel` remains **VACUOUS** (body under
four words), so its REFUTED verdict is not evidence of anything.


### W16-EK landed — a gate that read one side from COFF and the other from instruction form

**Merge `97f76e42`. Pre-registered +1 fn / +260 B; measured 43,957 → 43,958 and 4,124,272 → 4,124,532, code%
40.248314 → 40.25085, `CROSSED IN` exactly one row (`?OnSetShuttle@Game@@QAA?AVDataNode@@PAVDataArray@@@Z`,
260 B), `FELL OUT` 0, native `PASS 18/18 skipped=0`.**

**The treatment was proved real before it was priced.** `objdiff.json` carries no `symbolEquivalences` key —
aliases reach objdiff through `map_file = build/45410914/icf_aliases.map`, and `build.ninja` declares
`scripts/symbol_aliases.json` as an implicit **dependency** of that edge, with a second `--check` edge asserting
the rendered map still agrees with the JSON. `SetActive@Shuttle` was measured **absent (0)** from the live map
before the merge and **present (1)** after the gate. Without that check a ruler-path patch can measure
absent-vs-absent and report a confident zero.

**Both of W16-EH's claimed defects hold, and the load-bearing finding is that they are COUPLED.** Removing the
one-sided refusal alone would have been a genuine loosening, because a vacuous `mask_word` makes masked words
compare equal for *any* displacement. Landed as one change the gate is **strictly tighter** — and EK proved it
can still fail rather than asserting it: `+0x8` admits twice, `+0xc` / `+0x24` / `+0x0` refuse **by the
displacement**, i.e. by the very check that used to be vacuous. Made permanent as
`tools/test_fold_thunk_gate_mask.py`.

★ **The alias was adjudicated on retail bytes, not on the `none` control — and the control's flatness is the
signature of the hazard, not a clearance.** Retail `0x826f07b8` = `988300084e800020`, zero relocations; our
`Shuttle::SetActive` COMDAT is byte-identical to it and so is our `Metronome::Enable` COMDAT — two of *our own*
COMDATs independently meeting the `/OPT:ICF` condition — and `SetActive` is absent from the map, which is why
the survivor carries the other spelling. ⚠ This has to survive **W16-EL's same-day refutation** that
"byte-identical ⇒ folded" is false in general (retail `0x8255c968` and `0x825983d8` are byte-identical
*including their `bl` target* and still sit at two addresses). It does, for a stated reason: this is the
**relocation-free 8-byte leaf** case, where there is no `bl` target left to disagree about.

**Two corrections back to my brief, both accepted.** My "0 of 29 REFUSEs change" is correct but
**scope-limited** — it reproduces on the 36-pair worklist (0 of 36 verdict changes) while across all **1,048**
triage pairs the defect blocks **8**, eight times the reach I briefed, all 8 REFUSE→ADMIT with 0 the other way
and 8/8 carrying the asymmetry as their before-reason. And my precondition was **half-true**: `SetActive` was
defined in source, but the worktree's *inherited* `Shuttle.obj` was stale with no `SetActive` COMDAT, so the
gate would have kept refusing until a build ran — the reflinked-worktree trap, hit again.

**EK's own pre-registration failed on P3 and it records it as failed** (predicted ≤2 REFUSE→ADMIT, measured 8 —
a 4× miss). Its bound came from the 36-pair subclass where the answer is 0; it never estimated the rate on the
full population. It did not trip the stop condition for a stated reason — all 8 share one before-reason and each
then passes a *stricter* comparison — and any ADMIT→REFUSE, or heterogeneous reasons, would have stopped the
lane.

⚠ **One mid-lane error, self-caught by measuring rather than by reasoning.** EK's first patch threaded the
COFF-record mask through `homonym()`, flipping a **1,180-site** pair (`??3@YAXPAX@Z`) ADMIT→REFUSE by destroying
its FT3 witness. `homonym()` is **linked-vs-linked** (retail vs dc3) where *neither* side carries relocation
records, so form-inference is correct there **precisely because it cancels** — EK had removed a one-sided error
in one place and reintroduced it in another. Fixed with `canon(relocated=None)`.

**Deliberately NOT done, recorded so silence is not read as coverage:** the 8 newly-unblocked pairs are **not**
installed (each needs its own adjudication); `tools/comdat_fold_gate.py` is **not** audited for the same
one-sided read against our COFF objects — named as the obvious follow-up and **UNVERIFIED**;
`docs/plans/fold-thunk-alias-gate-2026-08-12.json` records 9 ADMIT / 27 REFUSE and no longer reproduces (current
tree gives 7/29), flagged but not rewritten because it is another lane's dated record; and FT-EMPTY's standing
was **not** promoted — only its now-false "vacuous" rationale was corrected.

### ✅ THE `--chase` DECISION, RESOLVED — reject the tier, on its own output

W16-EJ parked 17 `--chase`-only pairs (13 rows / 10,136 B) rather than landing them, and I recorded the tier
question as OPEN behind W16-EK. **EK has reported, so it is decidable — and `--chase`'s own artifact answers it
against promotion.** Read across all 23 pairs in the lane's `chase.txt`:

| what the artifact says | count |
|---|---|
| FLAT T1 **REFUTED** — "masked bodies match but relocation TARGETS disagree — template-twin, not a fold" | 20 of 23 |
| FLAT T1 **PROVEN** but self-labelled **VACUOUS** (body under 4 words / over half the words masked) | 1 |
| FLAT T1 **PROVEN** only because "our spelling is in no compiled obj" | 1 |
| masked bodies **DIFFER** — retail did not keep the code our spelling emits | 3 |

⇒ **zero of 23 are proven folds by any non-vacuous route.** The two PROVENs are a body too small to carry
information and a spelling we never compiled, neither of which is evidence that the linker folded anything.

★★★ **And the decisive column is one nobody had read: `our_bodytwins`.** For 21 of 22 pairs our side has **more
than one** symbol sharing that body — median **14**, max **568**, with the `vector<ChatReceiver*>::push_back`
shape alone carrying **149**. An alias justified by "our COMDAT is byte-identical to the survivor" is therefore
justified by a property **149 of our own symbols share**, which cannot identify *which* spelling the call site
meant. That is the same trap as ICF destroying the caller's intent, arriving from the other direction.

**Combined with W16-EL's refutation of "byte-identical ⇒ folded" (retail keeps two byte-identical bodies,
matching `bl` target included, at two addresses), the case for `--chase` as a declared tier fails twice over.**
An alias lifts `name_check` **by construction** and the `none` control provably cannot catch a fabrication, so
the burden of proof sits entirely on the evidence tier — and this one does not carry it.

⇒ **DECIDED: `--chase` is NOT promoted to a declared evidence tier. The 13 rows / 10,136 B stay unclaimed, and
W16-EJ's refusal to land them was correct.** This is deliberately a decision to forgo bytes that are available:
~7.9 pp of `matched_code` already rests on alias forgiveness, and accuracy beats headline %.


### W16-EM landed — the oracle declares a base class retail does not have

**Merge `202531e5`. Measured 43,958 fns (Δ0) / 4,124,532 → 4,125,560 B (+1,028) / code% 40.25085 → 40.26088.
`CROSSED IN` 1 row / 1,104 B; `FELL OUT` 1 row / 76 B; native `PASS 18/18 skipped=0`.**

`sizeof(XboxPurchaser)` goes **0x50 (80) → 0x28 (40)**. Retail has no `Hmx::Object` base, and every allocation
site was reserving twice what retail does.

★★★ **The durable finding is about the ORACLE, not the class.** DC3 declares the same multiple inheritance we
did, so a source diff against our primary engine oracle shows **nothing wrong**. DC3 is *newer* than RB3 and the
`Object` base is one of its additions — the standing trap that our engine is a verbatim DC3 copy and DC3 is the
later tree. Only retail bytes could settle this.

⚠ **My briefed arithmetic was true but insufficient, and the lane caught it.** I argued from `mState@0xc` vs our
`0x34`, difference `0x28`. Under MSVC multiple inheritance a secondary base's slots receive a `this` already
adjusted to that subobject, so `: public Hmx::Object, public StorePurchaser` *also* reads `mState` at `this+0xc`.
The arithmetic cannot discriminate "no `Object` base" from "`Object` base first". EM made the rival hypothesis
**refutable** — a swapped order predicts `StorePurchaser mdisp=0x28` and four `??_R2` entries — and then
**refuted it**: retail measures `mdisp=0x0`, **two** entries, `attributes=0x0` with the MI bit clear.

**The correction that actually carries the result** is that member order was recovered from the **ctor**, not
from the header's assumed order. `mOfferID` sits at `0x10` (an eight-byte store), not `0x18`, and `unk3c` has no
attestation and no referents and was removed. Real order — `mState 0xc`, `mOfferID 0x10`, `mUserIndex 0x18`,
`bool 0x1c`, `HRESULT 0x20`, pad → `0x28` — leaves `0x1c` as the only free byte, exactly where `Poll`'s
`stb r11,0x1c` and vtable slot [4]'s `lbz r3,0x1c` need it. The compiler confirms `sizeof = 40`. **No member was
invented to reach the size**; each is read by a named retail instruction.

★ **A real behavioural bug, which the metric is structurally incapable of scoring:** `PurchaseMade()` returned
`false` **unconditionally**. Retail returns the byte at `0x1c`, which `Poll` is the sole writer of — so the
success path could never be observed. Fixed on merit.

**The casualty was predicted, named, and arithmetically closed.** `??_GXboxPurchaser@@UAAPAXI@Z`, 76 B,
100 → fuzzy 64.0 (target 76 B vs our 68 B): making the dtor honest made it trivial, so MSVC inlined it, dropping
retail's out-of-line `bl fn_827B28A0` and a register save/restore pair. **+1,104 − 76 = +1,028 exactly**, with
189 → 189 units at 100%. EM declined to chase the 76 B — restoring it needs `XCancelOverlapped` and a
class-static `XOVERLAPPED`, i.e. a new undefined XDK symbol in a shared `src/system/` TU the native target
links, for 0.0007 pp — and refused to force out-of-lining with a dummy statement as metric fitting. Correct on
both counts.

⚠ **`fuzzy_match_percent` went DOWN** (50.00691 → 50.006645, −0.000265 pp) — that is the fell-out row, and it is
the expected sign. I deliberately did **not** pre-register a fuzzy figure because EM never reported one;
inventing a number to complete a scorecard is worse than an admitted gap. **Two coordinator errors of mine,
recorded:** I briefed an include list that was overstated (`MusicLibraryStore.cpp`, `SetlistToStorePanel.cpp`
and `StandIn.cpp` contain **zero** `Purchas` references), and I pre-registered the crossing row as
`?Poll@XboxPurchaser@@` when it is `UGCPurchasePanel::?Poll@UGCPurchasePanel@@UAAXXZ` — the bytes and the row
count were right, the identity was my transcription slip.

**EM's own pre-registration missed and it records the miss:** it predicted `??_G` would hold at 100, which is
the risk it had itself named as failure mode #1, and that is the one that fired. Predicted +1,104/+1, measured
+1,028/+0.

### ✅ W16-EK's named follow-up, CLOSED — `comdat_fold_gate.py` does NOT carry the one-sided read

W16-EK flagged one thing UNVERIFIED: whether `tools/comdat_fold_gate.py` repeats the defect it had just fixed.
Audited at `202531e5`, and the answer is **no, on both the question EK asked and one it did not**.

**The question EK asked.** `comdat_fold_gate.py` was written *knowing* about the defect — its own module
docstring names it as the reason the tool exists: *"fold_thunk_gate masks every relocation-CAPABLE field on both
sides … treats `lwz`/`stw` as relocation-capable while our COFF side lists only the offsets carrying an actual
relocation record, so the sets differ and the pair is refused."* It uses our COFF relocation table as the oracle
for **which fields are relocated on both sides**, comparing an unrelocated offset as a full 32-bit word. That is
the correct design, and it is the design EK converged on independently.

**The question EK did not ask, and which mattered more.** `comdat_fold_gate.py:231` does
`from fold_thunk_gate import parse_leaked_map, mask_word` — it **imports the very function EK rewrote**, and
calls it at `:407` and `:500`. Had EK changed `mask_word`'s semantics incompatibly, its merge would have
silently altered a second gate's verdicts through a shared import. It did not: EK changed the signature to
`mask_word(w, relocated=True)`, a **defaulted** parameter, and both call sites pass one argument, so they
receive `relocated=True` — bit-for-bit the pre-EK behaviour. **Blast radius on the sibling gate: zero.**

⇒ This is the shape worth generalising: a shared helper is a blast-radius surface a lane can miss precisely
because it is not in the lane's own diff. The check is cheap — grep the importers before landing a change to a
shared tool — and it is not something either the metric or the native gate can see.

### W16-EP landed — the eight "unblocked" pairs were already installed, and I briefed their value wrong

> ⛔ **CORRECTION, same day:** the "eleven unfolded `list<T*>::erase` bodies" claim below is a
> **masking artifact and is withdrawn** — verified on raw retail bytes. The lane's no-op
> finding is unaffected. See the correction section at the end of this file.

W16-EK measured that its COFF-relocation mask unblocked **8 alias pairs** across all
1,048 triage pairs — 8× the reach I had briefed, 8/8 REFUSE→ADMIT, 0 the other way.
I recorded that as reach *and implied it was headroom*. W16-EP was dispatched to
adjudicate the 8 with a standing default of REFUSE, and the answer is that **the
headroom does not exist**: 6 of the 8 folded spellings, including both pairs that
clear the evidence bar, are **already in `scripts/symbol_aliases.json`**. The install
is a literal no-op — `0 new, 0 updated; 1658 total`, `git diff` empty.

EK's count reproduces exactly, so nothing about EK is retracted. What changes is the
interpretation: **the defect's reach was measured correctly and its value was not
measured at all.** The unblocked admissions are mostly the gate finally *agreeing*
with memberships we already ship — a reproducibility win. That distinction matters
because an "8 blocked pairs" line in a handoff reads as uncollected bytes, and the
next lane would have spent a budget collecting zero.

**The census instrument is the durable output, and it is the third independent
refutation of "byte-identical ⇒ folded" in this wave.** `retail_bodytwins` counts
only **pinned** objs, so it undercounts retail's copies and points the wrong way.
EP replaced it with a whole-image census over **every `symbols.txt` extent** — masked
by form, *then* requiring identical **resolved branch destinations** — and found
retail keeping **eleven** byte-identical 84-byte `list<T*>::erase` bodies, identical
*including their `bl` targets*, unfolded at **eleven distinct addresses**. That is the
complete `/OPT:ICF` condition, satisfied, with no fold. The "41 of ours → 1 retail
address" pigeonhole that motivated the pair was a **pinning-coverage artifact**.

Running total for this refutation: W16-EL on a single pair (`0x8255c968` /
`0x825983d8`), W16-EP on eleven at once, and the open question put back to W16-EN.
⇒ **A `FLAT T1 PROVEN` on a relocation-free body is the weakest evidence tier we
have**, because with no relocations the discriminator does no work — exactly why
EP refused its pair 1 (8-byte body, single *shared* `MemAlloc` relocation, body
shared by 214 of our symbols, 2 unfolded retail copies).

Two items surfaced for other lanes rather than acted on: the 2026-08-12 triage
worklist's `sites` column is **stale**, and
`src/band3/meta_band/ContextChecker.cpp:262` `GetSongSpecificEntriesForCategory`
is an **unimplemented stub** whose caller consumes its empty result.

### W16-EO landed — the shim vein is empty, and an identification that pays zero bytes

My brief said `VocalPart.cpp` repeats W16-EJ's `+572 B`: same file family, three MWCC
shims, same repair. **It does not, and the reason is pairability rather than source
quality** — the two live shim call sites are inside an *unpaired* function, so fixing
them correctly moves nothing.

The result I most wanted from this lane is the one a single verdict would have
destroyed: **the three shims have three different answers.** `NoteAt` was a genuinely
charged wrong name (retail names its callee too) and is fixed. `PitchAt` is fixed but
**inert by construction** — its only call site is `HX_NATIVE`-gated, proved
*non-metrically* by the pre-fix object containing no such symbol at all, because MSVC
emits nothing for an unused `extern` declaration. And `kInvalidPitch` was
**deliberately kept**: retail loads it through a **placeholder** symbol that
`name_check` already forgives, so our wrong spelling costs zero charges, and naming it
would mean **guessing** `@2MA` vs `@2MB` with nothing in the tree able to discriminate.
Refusing to fabricate a mangling for zero measurable gain is the correct call, and the
storage-class question at `0x820F14B4` is logged as the escalation item.

**The vein is empty and must not be re-funded:** 239 raw MWCC-shape hits reduce to 230
Ogg Vorbis codebook tables and **9 real**, all already known. `VocalGuidePitch.cpp` was
the only sibling and W16-EJ cleaned it; there is no third file.

**The pin is a real identification that pays nothing, and both halves of that sentence
are load-bearing.** `fn_826F3658` is `VocalPart::ScoreSinger`, and the row moves from
*never compared* (fuzzy 0.0, unpaired) to **fuzzy 90.881355 / mpn 91.135590** — yet
`matched_code` keys on `fuzzy == 100` and is all-or-nothing per row, so the measured
delta is **+0 bytes**, with only `fuzzy` moving `+0.004200 pp`. Under the standing
directive that **accuracy beats headline percent**, that is worth landing: a 472-byte
blind spot becomes a legible target carrying genuine instruction-level divergence.

★ **The gate proved the pin took rather than assuming it.** An un-resplit map edit is
inert and produces a confident Δ0 that is indistinguishable from a real one — the
failure that cost lane CF-1 an entire leg. The drive asserted `ScoreSinger` **absent
pre-merge** and the post-gate check confirmed it **present**, with `fn_826F3658` rows
going 1 → 0. Without that pair of assertions this landing would have been a Δ0 of
unknown meaning.

Two instrument corrections came back from this lane, both of which correct *me*:
`grep -rn … --include=*.cpp` prints `0 files` under zsh because the unquoted glob
errors and **the command never runs** (my census was vacuous; EO re-derived it in
Python), and **`pgrep -x ninja` reports NO while a build is running**, because the
process is `/bin/sh tools/ninja-locked` rather than a binary named `ninja` — the rule
I had adopted three lanes earlier after `pgrep -f` self-matched. Use
`ps -eo pid,etimes,args` with cwd inspection; it is the only instrument here that has
not yet been caught lying.

### ⛔ CORRECTION (same day) — W16-EP's eleven "unfolded copies" are a MASKING ARTIFACT; W16-EL's pair is REAL

The W16-EP entry above cites **eleven byte-identical 84-byte `list<T*>::erase` bodies
unfolded at eleven addresses** as evidence that "byte-identical ⇒ folded" is false.
**That specific evidence is withdrawn.** W16-EN challenged it, and I verified the
question on **raw retail bytes** myself rather than through any masked comparator —
reading `orig/45410914/band.exe` directly (PE section walk, VA → file offset, 84 bytes
at each address):

```
0x822b1e60  vs  0x82447458           (list<TargetCache> vs list<Plane>)
  [ 7] 38600054 != 38600018   <- li r3, 84  vs  li r3, 24   NON-BRANCH IMMEDIATE
  [12] 4850abc1 != 483755c9   <- bl (PC-relative)
```

Word 7 is the **per-`T` node size**. A non-branch immediate differs, so `/OPT:ICF`
**can never fold these**, whatever their `bl` targets do. They are eleven *different
functions sharing a shape*, not eleven copies of one. W16-EN reached this
independently (`11 extents, 11 DISTINCT masked forms`), and CLAUDE.md already records
the identical mechanism for `_List_base<T>::clear` — 42 addresses, reloc-identical
surplus **0**, differing in per-`T` node deallocators.

★★★ **The cause is a mask that is too coarse, and it is a defect this tree already
documented one lane earlier.** W16-EK found that `mask_word` zeroes the low 16 bits,
so `stb r4,8(r3)` and `stb r4,0x7ff(r3)` both mask to `0x98830000` — the displacement
passes **by construction**. A census masking `li r3, imm16` the same way collapses
`li r3,84` and `li r3,24` to `38600000` and reports them equal. ⇒ **EK's vacuous-mask
defect propagated into a downstream lane's evidence within the same wave**, which is
exactly the blast-radius hazard EK's own follow-up warned about: a shared helper is a
surface a lane can miss *precisely because it is not in that lane's own diff*.

★★★ **But the PRINCIPLE SURVIVES — W16-EL's instance is REAL, and I verified it the
same way.** `0x8255c968` vs `0x825983d8`, both 120 B by `.pdata`, **30 words, exactly
ONE differing word**, and it is a `bl` whose targets **resolve to the same address**:

```
  [22] 4800e359 != 4bfd28e9   -> bl target 0x8256ad18  vs  0x8256ad18   SAME
  non-branch differences: 0
```

Identical modulo PC-relative encoding, identical call destination, **two distinct
addresses, not folded.** That is the complete `/OPT:ICF` condition, satisfied, with no
fold. **So "byte-identical ⇏ folded" stands on EL's evidence and must not be cited on
EP's.**

**Consequences, recorded so nothing is over- or under-retracted:**

- W16-EP's **no-op finding is unaffected** — 6 of EK's 8 pairs were already installed,
  the install really is `0 new, 0 updated`, and EK measured reach rather than value.
  That remains the lane's main result and it is correct.
- W16-EP's **pairs 5 and 7 lose their stated REASON**, not necessarily their verdict.
  Neither EN nor I adjudicated them; EP has been asked to re-run with resolved branch
  destinations **and** identical non-branch immediates required, and to say whether the
  same masking reaches its pair 1 and pair 8 counts (pair 8's "411 masked-identical
  bodies" is the most likely to be inflated the same way).
- The **stale `sites` column** and the `ContextChecker.cpp:262` unimplemented stub are
  independent of the comparator and stand.
- **W16-EN's alias admission is CONFIRMED, not weakened**: its survivor's 96-byte body
  has **exactly one** retail address, by a census whose control finds 769 two-copy
  classes and a 40-byte body at **278** unfolded copies. An instrument that can report
  278 and reports 1 is measuring; one that cannot report anything but "many" is not.

★★★ **The durable methodological rule, which is the real output of this exchange:**
**a masked comparator cannot be trusted to establish identity, and a fold claim must be
settled on RAW BYTES with only PC-relative fields excused.** Mask branch displacements
(they legitimately differ at different addresses) and **nothing else** — every
non-branch immediate is content. Two lanes this wave produced fold claims that turned
on a comparator detail rather than on retail's behaviour; the arbitration cost one
`python3` script reading the PE directly, and that should be the first instrument
reached for, not the last.


#### ✅ RESOLVED (same day, `7ba4d7f8`) — W16-EP re-ran its own census and reached my verdict independently, then went further

I retracted EP's eleven above on raw retail bytes. EP was then resumed with the
refutation and four questions, and its re-run **confirms zero of the eleven
survive** — reached by its own instrument, not by accepting mine.

Under the corrected rule (mask **only** branch displacements, require identical
resolved destinations, compare every other word as a full 32-bit value), the
eight sampled bodies have **eight distinct forms**: node-size immediates
**84, 16, 24, 92, 72, 88, 36, 20**, and one address (`0x82766828`) is not an
`erase` at all but `??0NetLoaderRef@@QAA@ABU0@@Z`, a 20-byte body the comparator
had pooled with `list<T*>::erase`. The cause is exactly W16-EK's documented
defect: the census called `mask_word(w)` with one argument, defaulting
`relocated=True`, and `li r3,N` is `addi` (opcode 14, in `IMM16_OPS`).

★★★★ **The part that outranks the verdict: each of the eight survivor bodies has
exactly ONE true copy image-wide — which is precisely what a COMPLETED fold looks
like.** So the defect did not merely inflate a number, it inflated it in the one
direction that **manufactures evidence AGAINST folding**. Phantom unfolded copies
are the raw material of a spurious REFUSE, and a REFUSE closes a vein nobody
reopens. This is the same disease as the tool's confident `AT_LIMIT`: *the
verdict most worth auditing is the one that closes work.*

**Pairs 5 and 7 flip to CLEAR.** Their sole refusal reason was the eleven. On
corrected evidence sizes are equal on both sides, every non-relocated word is
identical, and both element types are the same width on each side (4-byte
pointers for pair 5; 8-byte two-word structs for pair 7) — so **the node-size
word that REFUTES the eleven CONFIRMS these**. Both were already installed, so
the lane's no-op install result is unchanged; what changes is that two
memberships I recorded as refuted are justified.

★★★ **A methodology finding that outranks both verdicts: ICF is ITERATIVE, and a
flat one-pass class produces FALSE REFUTATIONS.** A single pass over masked body
+ relocation names puts **pairs 2 and 6 outside their own survivor's class**.
`_M_create_node<AccomplishmentCondition>` folds with `_M_create_node<Plane>`
first, and only *then* do the `insert` bodies match. Through
`icf_fold_evidence.icf_classes`, F is in S's class for **all 8** pairs (class
sizes 124, 5, 1031, 2, 41, 53, 11, 4).

**Blast radius, measured rather than assumed, and one-directional.** Masking can
only merge, so counts can only inflate: every count fell or held, **none rose**.
It inflated **4 of 8** (pairs 1, 3, 5, 7) and was already correct on 2, 4, 6, 8.
Pairs 2 and 4 are **SAFE as pre-registered** — a merging defect cannot produce a
count of 1. Pairs 1 and 3 do not flip; the census was never their only reason.
⚠ And **pair 8's 411 is a different figure, not the same inflation**: the doc's
table carried `our_bodytwins` **75**, and the our-side column is unaffected
because `collect()` masks through `masked_body(raw, relocs)`, zeroing only bytes
at **COFF relocation-record offsets** — so a non-relocated `li r3,84` is never
touched. That path is relocation-record-driven, not form-inferred.

**The control, shown discriminating** (`tools/icf_true_copy_census.py
--selftest`, which requires both directions and keeps the defective comparator as
its negative control): largest true-copy class **278**; **2,026** classes of
68,378 with ≥2 members; in the 84-byte stratum the shape-twin largest **66**
collapses to a true-copy largest of **2**; the eleven read **1** each.
★★ **That 278 is W16-EN's figure, recovered independently by a different
instrument in a different lane** — corroboration, not a shared assumption.

**Deliberately NOT done, and rightly.** Pair 1 is now **admissible** and was not
installed. Both spellings relocate to the same name `?MemAlloc@@YAPAXHH@Z` — not
`_MemAllocTemp`, so MAPID-1's different-allocator trap does not apply — and EP
withdrew its original refusal as partly wrong (it cited the inflated "2 unfolded
copies" and treated a name-equal relocation as evidence *against* a fold). But
installing on a lane that already landed, after the finding that made its output
zero, **turns a correction into an unpriced change**. It is handed over as a
candidate needing the gate's `--install --tier` path and its own `ab_measure`,
**8 sites** — and that restraint is why this merge is still Δ0.

⇒ The lane doc's own title now reads *"7 were already installed, 1 is not"*,
which is the honest consequence. **The standing principle "byte-identical ⇏
folded" is unaffected either way** — it rests on W16-EL's pair
(`0x8255c968`/`0x825983d8`), which I verified on raw retail bytes and which
survives: 30 words, exactly one differing `bl`, both targets resolving to
`0x8256ad18`.

### W16-EN landed — the size claim was about OUR build, and the defect was assignment triviality

I briefed this lane that `vector<ExtraTail>::_M_erase` being 88 B against retail's
96 B meant our **element size** was wrong. That was wrong, and EN refuted it with
the compiler: `sizeof(TrainerGemTab::ExtraTail)` and `sizeof(RndLine::Point)` are
**both 72 B (0x48)**, and retail agrees — its own erase body loads `li r10, 0x48`
as the stride.

The real cause is **assignment triviality**. `Transform` and `Hmx::Matrix3` both
user-declare `operator=`, which makes `ExtraTail`'s implicit copy-assign
**memberwise** (a 0x40 memcpy plus two scalar stores) instead of one whole-object
memcpy. That fatter loop body is why MSVC declines to inline STLport's `__copy`
into `_M_erase`, emitting an 88-byte out-of-line form where retail emits the
96-byte inlined memcpy loop it shares via `/OPT:ICF` with
`vector<RndLine::Point>::_M_erase`. An explicit memcpy `operator=` — the same
idiom `Transform` itself uses — restores retail's codegen.

★ **The durable rule, and the one I should have applied to the brief: a size
disagreement between our COMDAT and retail's is evidence about our CODEGEN, not
about a data type.** The withdrawal record the brief came from literally reads
`our(S)=96 B vs our(F)=88 B [retail(S)=96]` — a disagreement *within our own
build*, with retail constant at 96 on both sides. I read it as a statement about
a type.

**Both halves are load-bearing.** The row that crossed is
`?Draw@TrainerGemTab@@QAAXH@Z` (656 B), which sat at `fuzzy 99.96951` **and**
`mpn 99.96951` — equal to the digit, so `arg_diff_score == 0` and its single
charged site (`diff_score` exactly 5.0) was a vetted relocation-name charge on
the call to `_M_erase`, promoted into `mpn` by objdiff-core `b14ba45`. The source
fix alone could not cross it; it crossed only once the ICF membership was
re-admitted — and the membership is only admissible once our body is 96 B.

**The re-admission used the sanctioned route.** The withdrawal record is KEPT
(history and denylist intact); a new `scripts/alias_withdrawal_overrides.json`
names it explicitly, and `load_overrides` requires `overrides_class` to equal the
ledger record's class, so the override cannot be written without having read the
record it overrides. Audited before landing: **survivors added 0, removed 0,
exactly one group changed, exactly one folded membership added and zero removed**
— a re-admission, not a rewrite. The override is deliberately **self-limiting**:
valid only while `ExtraTail::operator=` exists, and if that is reverted the T1
adjudicator refuses the pair on its own. That property is what it was landed on.

⚠ The `none` control is flat, and that is **not** a clearance — an alias lifts
`name_check` by construction and `none` is blind to relocation names.
`ab_measure` itself labelled the run **NOT_APPLICABLE** for alias adjudication
because `source` is in the patch. The licence is retail bytes: **FLAT T1 PROVEN**
(retail_size 96 == our_size 96, `reloc_tally {}` over 3 relocs, survivor
map-resident), the adjudicator's `--selftest` passing *with* a REFUTED negative
control, and two sibling STL functions on these same two element types already
folding in this same file.

Also corrected: `TrainerGemTab.h` carried `// size 0x38` for `ExtraTail`,
inherited from the rb3-Wii header where `Transform` is smaller. Now `0x48`,
compiler-verified in the comment itself — the latent-trap class that misleads the
next lane.

New instrument: `tools/retail_body_multiplicity.py` counts retail copies of a
body with **branch destinations resolved**, which `retail_bodytwins` cannot do
(it counts only PINNED objs and so undercounts retail copies). Its control finds
769 two-copy classes and a 40-byte body at **278** unfolded copies; on this pair
it returns exactly **1**. An instrument that can report 278 and reports 1 is
measuring.

Handoff, analysed and deliberately not attempted: `?DrawTails@TrainerGemTab@@`
(888 B, fuzzy 98.58108, **19** charged sites) — `matched_code` is all-or-nothing
per row, so closing 1 of 19 buys 0 bytes. Found: an FMA-contraction difference
(retail does **not** contract `fmuls`+`fadds`, we emit `fmadds`, and our body is
4 B short), `fmuls` operand-order flips at indices 176/178/180, and ~13 FPR
numbering shifts.

### W16-ER landed — the 19 charges belonged to the callee, and three of the lane's own claims died on the way

**Merge `7be13608`.** `+1 function / +472 B`, whole-binary `43,959 → 43,960` /
`4,126,216 → 4,126,688 B` / `40.267284% → 40.271890%`, native
`PASS 18/18 skipped=0`, provenance unchanged (`5a51cd51fe0a353f` / 4.2.9 /
`a5f0ea903ec1`). Every pre-registered key hit exactly; the one estimated key
(fuzzy, predicted `~50.011265`) measured `50.011250`.

This lane picked up the row W16-EO **identified but could not pay for**, and the
result is the cleanest statement yet of a mechanism this repo had only circled:

> **A row's charges can belong entirely to a function that is not the row.**
> `?ScoreSinger@VocalPart@@…` went `90.881355 → 100.000000`, `diff_score
> 1076 → 0`, and **not one byte of the patch is inside `ScoreSinger`.** The
> whole fix is in its callee `GetNoteRange`.

#### The mechanism: MSVC X360 `/O1` does intra-TU callee-clobber analysis

Retail calls `GetBestHit` **without reloading `this` or `ms`** across the call.
That is only sound with interprocedural knowledge, and dumping the callee —
keyed on the `.fn` symbol, never the synthetic address column — supplies it:
`fn_826F1EC8` is a leaf that reads `r3` and `f1` and **writes neither**. Our
`GetNoteRange` was a `std::upper_bound` binary search where retail's is a
backward linear scan from a cached index (`unk58`), and that heavier body forced
**both** the `this`/`ms` reload surplus **and** a different stack packing.

So the caller's codegen is a function of what the callee clobbers, and the lever
on the caller is the **callee's body**.

#### The probe is the transferable part

A `__declspec(noinline)` stub that provably touches neither `r3` nor `f1`
separates *"our callee body is heavy"* from *"our row is wrong"* in **one build**:

| | fuzzy | diff_score | charges | base_size | slot 9 / 11 |
|---|---|---|---|---|---|
| baseline | 90.881355 | 1076 | 19 | 480 | 0x78 / 0x88 |
| callee stubbed (diagnostic, reverted) | 94.805084 | 613 | 9 | **472** | **0x88 / 0x8c** ✓ |
| retail algorithm | **100.000000** | **0** | **0** | **472** | **0x88 / 0x8c** ✓ |

The stub's residual 9 were an **identical instruction multiset in a different
order** — a scheduling artifact of the stub not reading `f1` where retail's body
does (`fsubs f0, f1, f0`). The faithful body closed them.

★ And the symptom labels behaved exactly as this repo's standing rule says they
do: the tool's `-4/+16` offset shift and four `r11↔r29` / `r11↔r9` register swaps
**dissolved untouched**. Nobody edited a register.

#### Three of the lane's own claims died, and it reported all three

1. **Local declaration order — Δ0, byte-identical.** With the recompile confirmed
   in the build log, so not absent-vs-absent.
2. **Function definition order — Δ0, byte-identical.** Same confirmation.
   ⇒ `MSVC_X360_REGALLOC.md`'s "declaration order controls stack slots" **does
   not reach this case**; the layout is decided by callee-induced register
   pressure.
3. ⛔⛔ **The inference that justified probe 2 is VOID: retail `.text` order is
   essentially uncorrelated with our source order.** The lane observed that all
   three callees precede `ScoreSinger` in retail (`0x826F1EC8` < `0x826F26A0` <
   `0x826F2E58` < `0x826F3658`) while all three follow it in our source — then
   refuted itself by finding `SetDifficultyVariables` at our line 26 sitting at
   `0x826F1770` while `PostLoad` at line 48 sits at `0x826F3990`. **The linker
   reorders COMDATs. Callee addresses say nothing about source order**, and
   nobody should re-derive that argument.

#### ★★★★★ Main's live `report.json` is not a baseline

The lane diffed its worktree against **main's live `report.json`** and got a
confident **−184 B with a `TrainerGemTab::Draw` regression** — a unit its patch
cannot reach. Main had moved under it mid-read: `43958 / 4125560` at the start of
the lane, `43959 / 4126216` an hour later. **That was me**, landing W16-EN at
that moment in the shared tree.

The artifact is shaped **exactly like a real regression somewhere else**, which
is what makes it dangerous: the obvious next move is to go hunting a
`TrainerGemTab` defect that does not exist. Same disease as W16-EO's torn read
during a running `ab_measure`, different cause.

⇒ **Both legs must be measured in your own worktree** — which is precisely what
`ab_measure` enforces, and exactly why it has **no `--baseline` flag**. A
baseline file is an absolute somebody else measured; main's build outputs are an
absolute somebody else is *still measuring*.

A fourth self-correction in the same section: a believed `CouldScoreAgainstPart`
`97.08 → 100` was read off the **probe-3 stub** build, not the baseline. Its true
baseline value is unknown and no claim is made about it.

#### Accuracy checks made before the port was trusted

`unk58` is genuinely maintained rather than a convenient field to read: reset at
`VocalPart.cpp:76/105/137` and updated at `:521` as
`unk58 = beginNote & ~(beginNote >> 31)`, the branchless `max(beginNote, 0)`
idiom. Declared `int unk58;` at `VocalPart.h:107`.

#### Two deliberate refusals

- ⛔ **No pin for `GetNoteRange`.** dtk **over-carves** it: `fn_826F1EC8` is
  104 B and ends on a conditional `bgelr` with no return, running contiguously
  into `fn_826F1F30` at `0xdb4 + 0x68 == 0xe1c`. `fn_826F1EC8` is a **phantom
  extent**, and pinning it would pin a mis-carve. The blocker here is **carve
  quality, not identification** — the identification is recorded in the doc for
  whoever fixes the carve.
- ⛔ **No name planted for `?Poll@VocalPart@@`** (the optional bonus). Naming
  under `name_check` converts a *forgiven* placeholder site into a *checked* one;
  the lane had no retail-byte identification to the standard W16-EO used, so it
  made none. A considered refusal, not an oversight.

#### Handoff — the callee-clobber sweep is general and unswept

Any caller whose diff shows **stack-slot shifts plus register swaps concentrated
in ONE call's argument setup**, with `base_size` a couple of instructions above
`target_size`, is a candidate — and **the fix is in the callee, not the row you
are looking at**. The `__declspec(noinline)` non-clobbering stub is the cheap
discriminator.

Nearest target in the same file: `?GetBestHit@VocalPart@@` (528 B, fuzzy
96.9697), itself a `GetNoteRange` sibling. Also still open in `VocalPart`:
`GetNoteSliceWeight` (484 B, 88.12), `FramePhraseMeterFrac` (136 B, 57.65),
`IsEmptyPhrase` (116 B, 65.24), `InTambourinePhrase` (44 B, 18.00).

### W16-EQ landed — caller-set fan-in, and a wrong name that was hiding an `int`/`bool` bug

**Merge `fe9c8197`.** `+9 functions / +3,428 B`, whole-binary `43,960 → 43,969` /
`4,126,688 → 4,130,116 B` / `40.271890% → 40.305344%`, native
`PASS 18/18 skipped=0`, provenance unchanged. **Every pre-registered key exact**,
including the nine crossing rows in the predicted order at the predicted sizes
and `FELL OUT 0`.

#### The instrument is the result

The band EQ worked is 100% relocation-name charges (10 of 10 `diff_arg`, 0
instruction-level). For a charge of that kind the only question is **fold or
mislabel**, and the two obvious tests both fail:

- **Byte identity fails** — W16-EP refuted it directly: retail keeps eleven
  byte-identical `erase` bodies *unfolded*.
- **Name shape fails** — it is circular, because the name is what is in question.

EQ's discriminator is the **caller set of the target address, censused off retail
`.text`**:

> **few callers, all spelled as the same function in our source ⇒ MISLABEL**
> **many callers, spelled as many different callees ⇒ ICF FOLD**

Measured, and not a close call: mislabels **4, 3, 2**; folds **15, 44, 10, 453**.

And where the callee is a Milo `Message`, the **interned type string in retail
`.rdata` names the class outright**, with no reference to the map at all — the
non-circular anchor everything else was checked against.

#### Five map corrections

| address | was | is | evidence |
|---|---|---|---|
| `0x8235BF00` | `__copy_ptrs<const int*,int*>` | `?GetQuest@Tour@@` | body is no copy loop, lives in `Tour.s`, its 4 retail callers are exactly our 4 `GetQuest()` sites |
| `0x823E1E20` ↔ `0x823E2020` | `RemoteUserUpdatedMsg` / `RemovingRemoteUserMsg` | **swapped** | each ctor calls the *other's* `Type()`; `.rdata` strings settle it |
| `0x823E1B40` | `??0SpeechEnableMsg` | `??0AddUserResultMsg` | its `Type()` interns `"add_user_result"` |
| `0x826F1080` | `?Draw@RndGroup@@` | `?GetFinger@FretHand@@` | indexes a 12-byte array and writes through `r5`/`r6`/`r7`; a no-arg `Draw()` cannot |

#### ★★★★ The wrong name was masking a real source bug

Retail's `AddUserResultMsg` 1-arg ctor takes a **`bool`, not an `int`**. Retail
opens `clrlwi r11, r4, 24` — an 8-bit zero-extend a `bool` parameter produces and
an `int` does not — where our `int` version emitted `li r11, 0`. After the fix
our compiled body emits `548B063E` at `+0x18`, **byte-matching retail**.

This is exactly the payout this repo's record predicts for naming work:
**bug exposure, not bytes** (cf. MAPID-1's `MemAlloc` identification, which was
deliberately net-negative and exposed six wrong-callee divergences).

The call sites spell `false`/`true` rather than `0`/`1` because `DECLARE_MESSAGE`
also generates a `DataArray*` overload, which made the literal `0` ambiguous
(C2668). Codegen is identical either way (`li r4, 0/1`).

#### The adjudication was confirmed in BOTH directions

The brief offered 5,100 B across six rows. Adjudication said **1,980 B
collectable, 3,120 B fold-bound** — and both halves held:

- the two pure-fold `OnMsg` rows (1,236 B, 664 B) **did not move at all**;
- `InitFretSteps` (1,220 B) improved `99.96722 → 99.983604` **without crossing**,
  because `matched_code` is all-or-nothing per row and it still carries one real
  fold — **a correct repair worth exactly zero bytes, landed anyway**.

The remaining 1,448 B of the +3,428 is cascade into rows outside the brief. A
brief that is *half right in a measurable way* is the useful kind.

#### Two deliberate refusals, so the delta stays attributable

- **No aliases.** Four of the ten charges are genuine folds and stay charged. An
  alias is forgiveness and lifts `name_check` **by construction**, and the `none`
  control provably cannot catch a fabrication. `control_none` read **−136**
  against default **+3,428**, correctly labelled **NOT_APPLICABLE** because
  `source` is in the patch.
- **No pins re-homed.** Two renamed rows now read **0.0**, because their retail
  addresses are pinned to units (`SpeechMgr`, `ViewSetting`) whose objs do not
  compile our definitions — and **objdiff pairs by name WITHIN a unit**.
  **The identification is right and the pin is wrong.** That is the re-homing
  lever, ~172 B, handed off rather than taken.

#### The gate, and why it was built differently

`scripts/target_symbol_map.json` is a **ruler path**, so this landing needed two
things an ordinary gate does not do:

1. **Force the re-split and iterate to a `symbols.txt` fixed point.** A map edit
   that does not re-split is **inert and measures nothing** — lane CF-1 lost a
   full leg to exactly that, and ABSPLIT-1 showed a single forced split
   *under-reports bytes invisibly*. Measured: fixed point on iteration 1,
   renamer **1,830 files patched / 86,457 symbol renames**.
2. **Assert the map change TOOK, in both directions.** Pre-merge: the three new
   names **absent**, the three old names **present** (at `99.85294 / 43.23529 /
   33.33333`, matching the pre-registration). Post-gate: the mirror image.
   `MAP_CHANGE_TOOK=1`.

★ **The Δfuzzy is negative and was pre-registered as expected, not discovered as
a regression** — `−0.001156` measured against `−0.001155` predicted. Three
partially-matching row keys are renamed, **none of them at 100**, so
`matched_code` is untouched and `FELL OUT` is 0. Booking an expected negative in
advance is what stops the next reader from opening a regression hunt.

#### Corrections the lane made to my brief

- `diff_score` is **not** a `report.json` field — it comes from
  `objdiff-cli diff`'s `diff_score.score`.
- `OnMsg@NetSession` is **eight** overloads with **four** already at 100, not
  seven with three.

Neither was material to the result. Both are recorded because a brief figure that
survives unchallenged becomes lore.

#### Handoff — a fourth instance of the same shape, located but not fixed

`0x823F2C98`, mapped `_M_allocate_and_copy<const unsigned long long*>`, has **3
callers, all inside NetSession** (`??_ENetSession`, `UpdateSyncStore`,
`RegisterOnline`), where a genuine STL instantiation shows broad heterogeneous
fan-in like the four proven folds. Our source spells that callee
`??0MakeQuazalSessionJob@@`. `RegisterOnline` (136 B) and `Disconnect` (136 B)
each carry **exactly one charge**, and it is this one ⇒ roughly **272 B**.

### W16-ES landed — a contraction rule, a refuted oracle, and a correct fix worth zero bytes

**Merge `590d0139`.** `+1 function / +0 B`, whole-binary `43,969 → 43,970` /
`4,130,116 B` unchanged / `40.305344%` unchanged / fuzzy `50.010094 → 50.010155`,
native `PASS 18/18 skipped=0`. Every pre-registered field hit exactly, fuzzy
included to the digit.

#### The economics were stated before the work, and they held

`?DrawTails@TrainerGemTab@@` is 888 B behind **19 charged sites**, and
`matched_code` is **all-or-nothing per row**. So closing 18 of 19 buys **exactly
zero bytes**. The lane was briefed on that and claimed **+1 `matched_function`
and explicitly not the 888 B** before measuring. That is the right shape for a
brief in this repo: price the *realisable* prize, not the row size.

#### ★★★★ The finding: single-use contraction

> **MSVC X360 fuses `a*b+c` into `fmadds` if and only if the product has exactly
> one consumer.**

Measured on the real `16.00.10224.00` at project cflags across **ten** spelling
variants — five one-use spellings all contract; every two-use spelling emits
`fmuls`+`fadds`. Not inferred from the metric.

Retail's product *looks* single-use, because it is dead immediately after the
add. The resolution is that there are **two uses at the fuse decision**, and a
later CSE merges the two identical adds back into one — so the separate pair
costs no instruction. The whole fix:

```cpp
float overhang = 0.1f * ((xfm.v.z + scaleX10) - unk12c);   // was 0.1f * (endZ - unk12c)
```

No `#pragma fp_contract(off)` — **inert on this toolchain** (lane AE2). No
`volatile` — it would add a stack round-trip retail does not have.

Two corrections to docs we already carry, both now recorded at the site:

- a **"named home" for a subexpression is NOT sufficient** to break contraction —
  our own one-use local proved it;
- `XBOX360_FLOATING_POINT_CODEGEN.md`'s `NgFur` lever works only because a
  **member** adds a store, which is usually the wrong shape to copy.

#### Three levers refuted, each built and read individually

1. **Flipping the commutative `fmuls` operand order — byte-identical INERT**,
   verified non-vacuous (the TU recompiled, all six patchers ran).
   ⇒ **objdiff's `COMMUTATIVE_OP_ORDER` → "LikelyFixable" is wrong here.**
   `Vec.h`'s `operator*=` and `Scale()` are both value-first, corroborating.
   Another instance of the standing rule: **a tool's confident label is the claim
   most worth auditing, because it closes veins.**
2. ⛔⛔ **Hoisting `yRange`/`tickRange` into pre-loop locals — the rb3-Wii
   oracle's own shape — is HARMFUL.** MSVC performs LICM into callee-saved FPRs
   and grows the save set (`__savegprlr_24`/`__savefpr_21` vs retail's `_22`/
   `_23`): **17 → 69 charged sites, fuzzy 99.30 → 86.15**. **The oracle is
   refuted by retail bytes here**, and our inline spelling independently
   confirmed. A comment now sits at the site so it is not "tidied up" later — the
   next reader would otherwise see code that disagrees with the oracle and fix
   the wrong one.
3. **Declaring `tickRange` inside the guarded block — inert.** Retail evaluates
   the *denominator* first where we evaluate the numerator, but that ordering is
   **scheduler-chosen, not source-chosen**.

★ **Self-reported process defect, kept because it generalises:** levers 2 and 3
*look like* one hypothesis and are not. Moving locals **before** the loop does not
test evaluation order — it volunteers them for pre-loop placement. Lever 3 was
the experiment that should have run first.

#### ★★★★ This is the `mpn`/`fuzzy` split, and the gate was built not to be misread

| measure | rule |
|---|---|
| `matched_functions` | count of rows at `match_percent_normalized == 100` |
| `matched_code` | Σ size of rows at `fuzzy_match_percent == 100` |

The row crosses the **first** and not the second:

```
?DrawTails@TrainerGemTab@@   888 B
   fuzzy  98.58108 -> 99.30180      (still < 100 -> zero bytes, by design)
   mpn    99.27928 -> 100.00000     (this is the +1 function)
   our compiled body 884 -> 888 B   (the 4-byte deficit closed)
```

⚠ **`CROSSED IN: 0 rows / NET +0 B` is the PRE-REGISTERED RESULT, not an inert
patch.** That sentence was written into the pre-registration *before* the merge
for one reason: a future reader who sees a zero and reverts a correct fix costs
more than the fix was worth. The gate therefore asserts the real evidence
separately — `MPN_CROSSED=1`, `FUZZY_BELOW_100=1`, unit `TrainerGemTab`
**11 → 12 of 22**, and W16-EN's `?Draw@TrainerGemTab@@` still at `fuzzy
100.00000` so nothing fell out of the neighbouring file in the same unit.

This is the standing directive in practice: **accuracy beats headline %**, and a
correct repair worth zero bytes gets landed anyway.

#### What it deliberately did not do

- **No permuter**, proposed or invoked — OFF by directive, even though two
  residual clusters are exactly its class.
- **Did not land the two inert edits.** That is noise, and its own draft comment
  for one of them asserted something it then measured false.
- **Did not touch** `scale`'s pooled-constant spelling (`0x3baaaaab`, deliberate,
  re-confirmed), W16-EN's `ExtraTail::operator=`, the alias override, or the
  unit's 10 other sub-100 rows — **all at fuzzy 0, which is an identification
  problem, not a source problem.**
- ★ **Did not claim the row is unfixable.** The narrower true statement is that
  the **17 residual charges are all `diff_arg` register slots (203 of 222
  instructions equal)** and are unreachable **by source spelling** on three
  measured levers. Under this repo's `AT_LIMIT` policy that distinction is the
  whole point: a confident "unfixable" closes a vein nobody reopens. The next
  lane should attack the scheduler question, and §3 of the lane doc exists so it
  does not re-run these dead ends.

### W16-EU landed — a re-home that finally pays out EQ's map fix, and four of my briefed figures refuted

Merged `10a1d997`, **+6 functions / +1,360 B**, `43,970 → 43,976` / `4,130,116 →
4,131,476` / code% `40.305344 → 40.318620` / fuzzy `50.010155 → 50.012623`.
Three tasks, each separately A/B'd, chaining `43969 → 43970 → 43971 → 43975`.

**1. The brief was the artifact that was wrong, and testing it first is what
produced the lane's largest result.** Four briefed figures died:

- ⛔ **`?Disconnect@NetSession@@QAAXXZ` does not exist anywhere in
  `report.json`.** Briefed task 2 was therefore worth **136 B, not 272 B** — and
  chasing *why* the figure disagreed is what turned up task 3 (+1,088 B / +4
  fns), the largest result of the lane. A brief that had been accepted would
  have produced a smaller lane.
- ⛔ **`0x826F1080` is not re-homable by any pin.** `?GetFinger@FretHand@@` is
  UNDEF in all three referencing objects and **there is no `FretHand.cpp` in the
  tree**, so no pin move can give it a defining object. Briefed at 172 B; the
  correct action was to do nothing. **The check that killed it — *does a
  defining object exist?* — is the one to run FIRST on any re-home candidate**,
  before measuring anything.
- ⛔ **My pin range was fabricated.** I briefed the ViewSetting range as
  `0x826F1080–0x826F1188`. The probe had printed the raw integers
  `(2188316568, 2188316840)`, which are **`0x826F0F98–0x826F10A8`**; I invented
  hex from the *target address* instead of converting the integers that were on
  my own screen. The **conclusion** (the address belongs to `ViewSetting.cpp`,
  not `VocalPart.cpp`) survived; the **quoted range** did not. Recorded here
  because a coordinator's transcription error is indistinguishable from a
  measurement until somebody re-derives it.
- ⛔ **W16-EQ's caller *names* for `0x823F2C98` are unsupported** — they resolve
  to unmapped `fn_823E4808` / `fn_823E4B68`. EQ's **conclusion** reproduced
  exactly; only its supporting names were wrong.

**2. ★★★★ The payout came from the CALLER, not from the row that was named.**
Task 2 renamed `0x823F2C98` from an STL `??$_M_allocate_and_copy@PB_K@…`
instantiation to `??0MakeQuazalSessionJob@@QAA@PAPAVQuazalSession@@_N@Z`. The
newly-named row landed in **`default/StorePurchaser` at 60 B and `fuzzy 0`** —
mis-homed and unmatched, contributing nothing. The whole **+136 B** arrived as
**`?RegisterOnline@NetSession@@QAAXXZ` crossing to 100**: the *wrong* template
name had been charging RegisterOnline's call site under `name_check`, and
correcting it stopped the charge. This is the recorded economics
(*un-pairing is 80.5% of a map edit's delta, the cascade only 19.5%*; *a wrong
name is financed by its callers*) observable in a single row, and it is the
cleanest instance the campaign has. ⇒ **When pricing a map fix, look at the
callers, not at the row you are renaming.**

**3. The re-home is the first in this wave, and it is NOT metric-neutral.**
Moving `0x823E1B40–0x823E1C20` from `SpeechMgr.cpp` to `NetSession.cpp` (merging
NetSession's two adjacent `.text` blocks into one `0x823E16A8–0x823E1D20` span)
is what finally pays out **W16-EQ's** map fix: EQ correctly identified the
address as `??0AddUserResultMsg`, but it was pinned into SpeechMgr, **whose base
obj cannot define that name**, so objdiff — which pairs by NAME — read the row
at **`fuzzy 0.00000`** however correct our source was. Adding a pin over `auto_*`
code is Δ0; **re-homing an already-pinned address is not** (PINHOME-1).

⚠ **`default/SpeechMgr` goes from 2 matched functions to 0, and that is the
predicted, correct result.** Its only two at-100 rows were the 40-byte EH
funclets `fn_823E1BC8` and `fn_823E1BF0`, which sit inside the re-homed range
and **move** to NetSession at 100 on both sides. This was pre-registered before
the merge — with the baseline's at-100 set for that unit enumerated and asserted
to be exactly those two rows — precisely so a future reader could not mistake it
for breakage. SpeechMgr keeps its other `.text` blocks (`0x824CD818…`), so the
emptied-unit hazard (an emptied unit still emits a 42-byte obj and `report.json`
hard-fails with *Invalid COFF/PE section headers*) does not apply; the gate
asserts the unit still exists.

**4. Task 3: the second and third `int`/`bool` ctor mislabels.** `0x823E1808` is
`??0SessionReadyMsg@@QAA@_N@Z`, not `??0ProcessedJoinRequestMsg` — which is
really at `0x823E1938`, a **new** map key — and `SessionReadyMsg`'s 1-arg ctor
takes a **bool**, not an `int` (`SessionMgr.h`, plus `NetSession.cpp:162`
`msg(0)` → `msg(false)`). It hit the **top** of its registered `+596..+1088`
range because two register-allocation differences **dissolved** once the
signature was fixed — the twelfth-plus recorded instance of **`REGISTER_SWAP`
being a symptom rather than a diagnosis**. The mechanical screen stands:
**`clrlwi rN,r4,24` at `+0x18` in a `DECLARE_MESSAGE` ctor** is a cheap test for
the rest of the family.

**5. Gate.** This lane touched **two** ruler paths — `config/45410914/splits.txt`
*and* `scripts/target_symbol_map.json` — which no prior lane in this wave did, so
the gate forced a re-split and iterated to a `symbols.txt` fixed point (CF-1: an
un-resplit map edit is inert; ABSPLIT-1: one forced split under-reports bytes).
`FIXED_POINT` on iteration 1, renamer `1830 files patched, 86453 total symbol
renames`. Every pre-registered key landed exact: **CROSSED IN 8 rows / 1,440 B in
the predicted order at the predicted sizes, FELL OUT 2 rows / 80 B**,
`RULER_CHANGES_TOOK=1`, `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18
skipped=0 partial=0 failed=0 rc=0`. The only deviation was code% `40.318620`
measured against my predicted `40.318616` — **not a discrepancy**:
`matched_code_percent` is a **float32** field and `4131476 / 10247068` =
`40.3186163` rounds to `40.31862` in float32. That gap was named in the
pre-registration in advance rather than explained afterwards.

**Handoffs.** (a) **`0x823F2C98` is now correctly named but mis-homed** into
`default/StorePurchaser` at 60 B / fuzzy 0 — a Task-1-shaped re-home worth 60 B,
**subject to the defining-object check that killed `GetFinger`**. (b) **Write
`FretHand.cpp`** — the header declares 8 methods and the tree defines none;
~36 B for `GetFinger` alone, and it is source work, not a pin move. (c)
`fn_823E4808` (544 B) is mis-pinned into `default/MeshAnim` and is really
NetSession; `fn_823E4B68` (756 B) is unmapped — both Task-1-shaped, same caveat.
(d) **Duplicate names in `target_symbol_map.json` are a counting-convention
artifact**: the map holds 29,555 keys of which **101 are `null` and 5 are
lists**, and the duplicate count reads **3 when list values are flattened and 4
when they are not** (EQ's doc says 2, EU says 4). What every convention agrees
on — and the only load-bearing part — is that **EU introduced zero**. One of the
duplicates is the literal string **`"0x826101b8"` used as a NAME**, which is a
real defect worth its own look.

### W16-ET landed — a measured negative: the signature does not find the lever

Merged `06185b80`, **+2 functions / +180 B**, `43,976 → 43,978` /
`4,131,476 → 4,131,656` / code% `40.318620 → 40.320374` / fuzzy
`50.012623 → 50.014150`. **The +180 B is the smaller half of this lane.**

**1. The negative, and the control that makes it worth keeping.** ET was sent
to turn W16-ER's mechanism — a callee whose register clobber set our source
gets wrong — into a whole-binary signature. It swept **2,659 rows** with 0
objdiff failures and measured, against arms where the mechanism is
**impossible by construction** (byte-exact callee, external callee, or no call
in the block), an enrichment of **0.87×**: the signature is slightly *less*
common where the mechanism can actually operate. Concentration turns out not
to be rare at all — **58.9%** of every sub-100 row puts ≥95% of its charges in
a single call-setup block, and the **highest** signature rate (74.7%) belongs
to rows with **no call in the block**.

★★★ **Without the anti-vacuity check the 0.87× would be worthless.**
Reconstructing W16-ER's own numbers, `ScoreSinger` scores conc 0.947 / 19
charges / `SAME_TU_SUB100` and **is flagged**. The detector can see its
founding instance, so "flat" means flat and not "broken". This is the standing
rule — *check a witness CAN discriminate before it blocks work* — applied to a
detector rather than a gate.

★★★ **The structural reason was visible in ER's case from the start, and
nobody looked:** ER's charges clustered around the **`GetBestHit`** call while
the actual defect lived in **`GetNoteRange`**, a *different* callee. ⇒ **The
signature names the wrong callee even when it correctly flags the row.** A
detector that identifies the right row for the wrong reason cannot be used to
find new instances, only to re-describe known ones.

**2. `GetBestHit` is refuted, and it was a one-query check.** Its charges are
entirely argument setup for `?ScoreNote@VocalPart@@`, which sits at **fuzzy
100.0000 — byte-exact**. A byte-identical callee has a byte-identical clobber
set *by construction*, so the mechanism cannot be operating. What remains is 4
instructions of pure scheduling (the same two instructions on both sides,
displaced two positions) with `target_size == base_size == 528` — which also
refutes clause 3 of the briefed signature **on the flagship row the brief was
built around**. ⇒ **Do not brief `GetBestHit` as a clobber candidate again; it
is a scheduling wall.**

**3. What landed came from reading diffs, not from the signature.** Four
retail-byte fixes in `VocalPart.cpp`: `.end()` instead of `data()+size()`
(×2); a `Clamp(0,1,·)` whose float specialisation expands to retail's
`fneg`/`fsel`/`fsub`/`fsel` instruction for instruction; and a local copy of a
global plus reversed `std::min` argument order in `GetNoteSliceWeight`.
★★ **Scope control worth copying:** the `data()+size()` idiom appears **8×**,
but 6 sites are in unpaired functions with no retail bytes on either side, so
ET changed only the **2 it could adjudicate**. Changing the other 6 would have
been unfalsifiable edits dressed as fixes.

**4. Pre-registration, honoured in both directions.** Committed in `10e2ed76`
/ `4fb7ac8d` **before** any A/B:

| row | predicted | measured | |
|---|---|---|---|
| `?FramePhraseMeterFrac@VocalPart@@QBAMXZ` (136 B) | 100 (HIGH) | **100.0000** | hit |
| `?InTambourinePhrase@VocalPart@@QBA_NXZ` (44 B) | 100 (HIGH) | **100.0000** | hit |
| `?IsEmptyPhrase@VocalPart@@QBA_NABQBVVocalPhrase@@@Z` (116 B) | 100 (MEDIUM) | 96.5517 | miss |
| `?GetNoteSliceWeight@VocalPart@@QBAMMMH@Z` (484 B) | 100 (MED-HIGH) | 94.9256 | miss |
| `?GetBestHit@VocalPart@@…` (528 B) | unchanged | 96.9697 | hit |

Both misses were flagged MEDIUM *in advance* and contribute **0 bytes**; they
are landed anyway because the source is right whether or not it scores.
`unit net across ALL units == the whole-binary delta`, so nothing regressed to
pay for the gain.

**5. Three inert probes kept as negative results** rather than discarded:
statement order of two independent local initialisations (bit-identical
residual, 1 confirmed recompile — **distinct from ER's *declaration*-order
result**, which is a different lever); a separate `int count` local; and
un-hoisting `2.0f`, which MSVC simply re-hoists.

⚠ **A coordinator error recorded against itself:** the landing gate's probe for
the two MEDIUM rows returned `None` for both, because **I fabricated their
mangled signatures** (`?IsEmptyPhrase@VocalPart@@QBA_NH@Z`,
`?GetNoteSliceWeight@VocalPart@@QBAMHM@Z`) instead of reading them out of
`report.json`. The real names are `…QBA_NABQBVVocalPhrase@@@Z` (116 B) and
`…QBAMMMH@Z` (484 B). The probe was **recorded-not-asserted**, so it could not
have failed the gate — but it reported nothing while looking like it reported
something, which is the vacuity pattern this repo exists to catch. This is the
**second** fabricated identifier from me in this wave (the first was the
`0x826F1080` pin range in the W16-EU brief); both times the correct value was
on screen. ⇒ **Read identifiers out of the artifact; never reconstruct a
mangled name or an address from memory.**

**Handoffs.** ⛔ **Do not rebuild this census** — the surviving
signature-selected vein caps at **13 rows / 3,760 B / 0.0367% of
`total_code`**, unvalidated. What survives from W16-ER is the **stub probe**,
not the signature, and you must arrive with a suspect by other means because
the diff will not name one. `?IsEmptyPhrase@VocalPart@@` is **one 4-byte
instruction from 100%** (+116 B): retail emits a redundant `clrrwi r10,r10,0`,
and a separate local does not produce it. `?GetNoteSliceWeight@VocalPart@@` at
94.9256 (+484 B) is **entirely one allocation decision** — retail spends `r30`
on a pool base and uses one fewer FPR (`__savefpr_21` vs `_20`). Six
unadjudicable `data()+size()` sites need their functions pinned first.

### W16-EV landed — Δ0 as the correct answer, and coverage as an instrument

Merged `079d2abf`, **+0 functions / +0 B**, every key unchanged:
`43,978` / `4,131,656` / code% `40.320374` / fuzzy `50.014150`. **Δ0 is the
predicted, correct result**, pre-registered as such *before* the merge so no
future reader mistakes it for a patch that never applied. Landed on the
standing directive that **accuracy beats headline %**.

**1. What was actually asked, and the answer.** EV audited the game-layer
headers carrying `// 0xHEX` offset comments against the compiler
(`/d1reportAllClassLayout`), keeping **class 1** — the comment is wrong and our
layout is right, metric-neutral bookkeeping — strictly apart from **class 2** —
our layout disagrees with retail, a real bug. **Class 1 is drained (69 rows
fixed across 15 headers). Class 2 is empty.**

**2. ★★★★ The recompile counts are what make the zeros measurements.** A Δ0
with zero recompiles is absent-vs-absent — the signature of a patch that never
reached the compiler — so the zeros only mean something next to evidence the
build really redid the work. EV's own A/Bs: **226** and **535** leg-B
recompiles. The landing gate additionally asserts a nonzero build-edge count
and measured **537 MSVC compile edges**. Either half alone is worthless; the
gate requires both.

**3. ★★★★ Coverage is instrumented, not assumed — the lane's durable method.**
`audit_header()` returns `[]` both when a header is **clean** and when it
audited **nothing**, so a raw "0 disagreements" cannot tell the two apart —
precisely the vacuity family this repo keeps being bitten by. Every
(header, class) was therefore audited **twice**: once as-is, and once against a
copy with every comment perturbed to `0xdeadbe`. The rows that flag on the
perturbed copy **are** the rows actually examined, which converts each clean
into a positively-evidenced one and doubles as a per-header discrimination
control.

| | rows | examined | disagreeing | headers |
|---|---:|---:|---:|---:|
| remit (band3 + network) | 3,325 | 3,012 (90.6%) | **35 (1.16%)** | 369 → 6 bad |
| tree-wide (extension) | 11,823 | 10,681 (90.3%) | **113 (1.06%)** | 1,042 → 24 bad |

Self-validation against this doc's own claims: **`SaveLoadManager.h` returns
AUDITED with coverage 26/26 rows and 0 disagreements**, and `CharEyes.h`
likewise — and *the coverage figure is what makes those real cleans rather than
vacuous ones*. Discrimination proved by corrupting line 143 `// 0x1c` →
`// 0x44`, which produced exactly the expected single WRONG row with no
collateral; restored immediately. Cost control worth reusing:
`/d1reportAllClassLayout` **once per TU** (3.1 s / 3,070 classes) instead of the
CLI's one-compile-per-class path, ~100× faster; all 385 remit TUs returned
`status=OK`, so no TU contributed a false zero.

**4. Class 2's single candidate adjudicates to not-a-bug.** `MetaPerformer` has
**two layouts in one program** — `/DRB3_NO_WII_META_MEMBERS` is on exactly 1 TU
while 86 others compile it 12 B larger, with the `Object` vbase at `0x38c`
against retail's `0x380`. Making the guards unconditionally false measured
**Δ0 with 104 recompiles**: the extra members are **tail-resident**, so only
vbase adjustors shift and no matched code does that. **Reverted** — a
documented retail-adjudicated config is not churned on a Δ0.

**5. ⛔ A tool defect, found and deliberately not fixed.**
`--check-header`/`--fix-header` **can audit the wrong header and report a
confident false clean.** `main()` takes `find_header(...)[0]` and shortest path
wins, so `--fix-header MetaPerformer` printed *"all `// 0xHEX` comments agree"*
against `src/meta_ham/MetaPerformer.h` — a near-dead DC3-era copy — while the
live `meta_band/` header had **16 wrong rows**. Same family as the cross-class
and shadowed-base bugs the tool already pins, in **cross-file** form. EV applied
the 16 rows against the explicitly-named header instead and left the fix to a
lane that can give it a regression pin. ⇒ **This is a shared instrument that
answers the wrong question and sounds certain doing it; treat any past
`--check-header` clean on an ambiguous class name as unproven.**

**6. My briefed scope figure was wrong.** "416 headers" does not reproduce:
**369** band3+network headers carry `// 0xHEX` (512 exist; 384 contain any
`0x`). **421** is the count of files of *any* extension in that tree — the
likely origin of my number. Third briefed figure of this wave to die on
contact.

**7. ★★ What was deliberately left undone, with reasons.** `Scheduler.h`'s 5
disagreeing rows are **left alone**: the unit is **0/26 fns, 0.0% matched**, so
our layout is validated by *nothing*, while the comments **and** the member
names (`unk4`, `unk8`, `unk34`) independently encode the same RE-derived retail
offsets; the `+4` comes from an empty `RootObject` base MSVC did not elide.
★ **New guard rule: `--fix-header` is only safe where matches validate our
layout — on a 0%-matched unit it is EVIDENCE DESTRUCTION.** Also left: 74
tree-wide rows (52 in units <45% matched or absent from `report.json`; **22
refused automatically** because the class has >1 layout across TUs —
`MeterDisplay` 11, `CharHair`'s `Point` 10, `Msg`'s `Message` 1), and **1,142
rows / 89 headers UNAUDITED** (uninstantiated templates, DDL scaffolds,
Wii-only classes) — *unaudited is not clean*.

⚠ **The landing gate's own Δ0 assertion raised a FALSE ALARM**, and it is worth
recording because it is this repo's most-documented trap firing in an unusual
direction. The post-gate check compared `matched_code` from `report.json`
against the snapshot and reported `⛔ MOVED` while printing two identical
values — because **`matched_code` is a JSON *string* in `report.json`** and an
int in the snapshot, so `'4131656' == 4131656` is False. The documented hazard
normally manufactures a confident **empty result** (a size filter reading
`0 rows`); here it manufactured a confident **failure**. Both come from the same
missing `int()`. Corrected by coercion and independently corroborated by the
set-diff path, which shares no arithmetic with it: **CROSSED IN 0 / FELL OUT 0**.
⇒ **Coerce every numeric read out of `report.json`, in assertions as well as in
censuses.** Native `PASS 18/18 skipped=0`.

**Handoffs.** Fix `find_header` to refuse or warn when >1 candidate matches,
preferring the header the resolved TU `#include`s, with its own regression pin.
`MeterDisplay` has 2 genuinely differing layouts **and a distinctive name**
(unlike the bare-name collisions `Point` and `Message`) — possibly a real
per-TU divergence worth one lane. Re-measure `MetaPerformer` if a future lane
matches code in those 86 TUs that dispatches virtually on it (the fix is a
2-line guard change, 104 recompiles). And **do not read the 39/168 "cross-TU
conflicts" as ODR bugs** — they are mostly artifacts of MSVC printing class
names without namespace qualification (`TourProgress`'s was a parser phantom
member named `pragma`).
