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

Ledger: 42,846 / 3,892,688 B / 37.992 % (`bbcf979b`, wave start) →
**43,558 / 4,040,604 B / 39.4318 %** (ledger row `8c830054`, objdiff **4.2.9** `5a51cd51`; W15-D, W15-E, W15-F, W16-A, W16-B, W16-C, W16-D, W16-E, W16-F, W16-G, W16-H, W16-I, W16-L, W16-K, W16-N, W16-M, W16-O, W16-P, W16-Q, W16-J, W16-S, W16-R, W16-T, W16-U and W16-V each additive to the byte on the 4.2.8 ruler, whose last figure is **43,290 / 3,962,580 B / 38.6746 %** at ledger row `93283bb4`; the +1 / +24,392 B between the two rows is **W16-W's RULER CHANGE**, not additive source work — same code, W16-X, W16-Y, W16-Z, W16-AA additive on the 4.2.9 ruler; W16-AB moved 7 named rows via map/splits/alias repairs, 0 out; W16-AD un-swapped 4 Accomplishment map rows — 7 in / 1 out, net +680 B — and withdrew 20 refuted alias memberships at Δ0; W16-AC wired rnddx9/Utl.cpp and crossed its four rows, +4 / +356 B, 0 out; W16-AG withdrew the 51 refuted alias memberships W16-AD left, Δ0 predicted and measured; W16-AF identified `0x825f32a8` as `GoalCmp::operator()` and rotated the comparator half of 56 map names across two STL bands, 35 in / 3 re-home fall-outs, net +32 / +6,016 B; W16-AI took the GoalCmp local static AF sized — the rb3-Wii oracle is wrong there, retail right — plus two vtordisp thunks and a misplaced TU boundary, +5 / +424 B predicted exactly; W16-AH withdrew 294 no-witness alias memberships by type existence at Δ0, refuting its own Route B map row on retail bytes; W16-AJ identified `0x826100F8` as the survivor of 26 hashtable dtors and completed CampaignSongInfoPanel 50/50 for +17 / +2,812 B; W16-AE refuted all four of W16-AB's "cannot be fixed" items — ported SessionSearcher/NetLog over seven circular pins, re-homed six mis-unit blocks, fixed two alias-tooling gaps — for +35 / +3,572 B; W16-AK refuted its own brief's ICF-fold premise on OvershellPanel — the two OnMsg charges were a Wii-dev-only block, 76 vs 132 B, removed on merit — for +3 / +684 B; W16-AM found the `list<T>` bijection is decidable by relocation TARGET, not arbitrary — 8 rows repaired, `fn_824CE130` identified, 0x823d3918 nulled as a folded `Handle` — for +5 / +980 B; W16-AN closed MainHubPanel 176/176 and CampaignSongInfoPanel 53/53 — retail materialises function-local statics where the Wii source uses externs, and re-homing the PhysicsManager mis-pin exposed a sret-vs-stack-slot divergence that was structurally invisible — for +12 / +2,736 B; W16-AL installed 12 of 20 alias proposals on retail bytes, refused 2 and held 14 — naming the `0x826100f8` hashtable-dtor survivor cost −176 B by design and exposed two real container-type dtor divergences, and AK7's "atexit dtor" was `BandUserMgr::GetBandUser` with six callers — for +14 / +14,820 B; W16-AO repaired 9 wrong list<T> resize rows plus one coupled transposition on retail bytes, leaving 0x8246af50 alone as a true fold, for +8 / +944 B; W16-AP closed GamePanel 91/97 → 93/97 by naming four anonymous rows, dropping RestartGameMsg's phantom payload and reproducing the function-local-static pattern, for +5 / +584 B; W16-AS +1 / +468 B; W16-AR +7 / +1,060 B; W16-AQ +7 / +2,248 B; W16-AU +2 / +216 B; W16-AT +1 / +628 B; W16-AV +12 / +3,500 B; W16-AW +1 / +328 B; W16-AX +2 / +296 B; W16-AY +1 / +144 B; W16-AZ +7 / +884 B; W16-BB +1 / +264 B; W16-BA +5 / +1,012 B; W16-BC +4 / +1,000 B; W16-BF +6 / +816 B; W16-BE +9 / +792 B; W16-BD +4 / +916 B; W16-BG +23 / +1,024 B; W16-BI +2 / +392 B; W16-BH +2 / +160 B; W16-BJ +2 / +32 B; W16-BK +3 / +180 B; W16-BL +1 / +12 B). `total_code` 10,247,068
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
| **W16-BM** (dispatched; brief `~/tmp/brief_w16bm.md`) | opus | SPLITS-ONLY lane: re-home the tree-wide **MIS-PINNED** EH funclets to their parents' units — BJ §3/§5.9 sized the class on the post-pilot tree at **708 rows / 28,248 B** (pre-pilot: 419 rows / 16,548 B of today's `matched_code` credited to a false twin in a unit that never emitted the funclet); BJ's 26-row pilot priced 8/9 edits exactly at +2 / +32 B with 5 rows gaining and 4 losing, so this is an **accuracy play** — a net drop is a WIN. Extend `tools/funclet_homing.py` with an asserted-partition `--emit-splits` mode (text-only edits, identical coverage before/after, last-block drains delete the entry, bar-list honoured, idempotent), pre-register per-row predictions from our COFF, land in two measured batches (true-twin-available first, false-twin-loss second), iterate splits to a `symbols.txt` fixed point, then report the ORPHAN→HOMED sweep by `auto_*` cluster. Bars: no `.text` line under `UI.cpp:`/`UIColor.cpp:` (BK) or `Line.cpp:`/`BandCharacter.cpp:` (BL) as source or destination; no map, alias, or src. | — |
| **W16-BN** (dispatched; brief `~/tmp/brief_w16bn.md`) | opus | MAP + SPLITS (+ one ported TU) lane, BK §4's coupled filing: retire the FALSE `?NewObject@UIScreen@@` credit at `0x823f5c20` (72 B, reads 100 in `default/UI` because the obj defines the name and the bodies are twins — BK proved it constructs a `NetSearchResult`, spelling unproven: header says `static NetSearchResult *New()`), install the real one at `0x828023A0` (BI's reserved island row, no map key today), optionally `0x823f5a90` → `??0NetSearchResult@@QAA@XZ` (264 B, fuzzy 0); port `NetSearchResult.cpp` from the rb3-Wii oracle (52 lines; game layer) so the rows become PAIRABLE, then adjudicate `UI.cpp:`'s 4,712 B block `0x823F4A30–0x823F5C98` function by function (every row fuzzy 0 but the false credit; `NetSearchResult` rows cluster below it) and re-home the proven sub-spans to a new `network/net/NetSearchResult.cpp:` heading. Predicted ≈ net 0 map-only (−1 / −72 B false credit, +1 / +72 B if UIScreen pairs) plus whatever the port matches; accuracy play. Bars: `UI.cpp:` and the new heading are its only splits domain (BM is barred from `UI.cpp:`), no other heading as destination (file instead), no alias edits unless `--validate` forces the minimal repair, no `src/` outside `network/net/NetSearchResult.{cpp,h}`. | — |
| escalation slot | fable | anything an Opus lane reports as unfixable, with the Opus report attached | — |

W15-F's `RunXinputJoypadLoop` item is NOT queued as a body-port: our
`Joypad_Xbox.cpp` lacks the entire XInput2 raw-HID layer, so it is a
subsystem port and belongs to the native-port track, not a match lane.
