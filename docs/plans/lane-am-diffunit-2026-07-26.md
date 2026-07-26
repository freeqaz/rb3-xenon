# laneAM — the DIFFERENT-UNIT gap pool: a margin rule and its precision/yield curve (2026-07-26)

Baseline at lane start: **34,469** strict (`match_percent_normalized == 100.0`),
main HEAD `16ba25b9`. Baseline pickle `/home/free/tmp/laneAM_base_strict.pkl`.

Direct predecessor: `docs/plans/lane-al-autocarve-2026-07-26.md`. Read it first.

## The pool, and why laneAL declined it

`config/45410914/splits.txt` pins per-object `.text` ranges. Every maximal
address interval no unit claims is a **gap**. laneAL split the gap set in two and
swept only the first:

* **interior** — the *same* unit is pinned on both sides. Retail packs a TU
  contiguously, so an interior hole is overwhelmingly that unit's own unpinned
  code. laneAL swept all 1,084: **+2,287 at a 53.6% hit rate**. Now drained
  (this lane re-measures 692 interior gaps holding **4** functions).
* **different-unit** — the gap is fenced by *two different* units. **There is no
  contiguity argument.** Retail genuinely interleaves TUs, and the
  "87.2% of splits holes are genuine COMDAT scatter" calibration was measured
  *here*, between different units. laneAL sampled 19 gaps, hit 26% by argmax
  (~+1,900 extrapolated), and refused to sweep, in its own words:

  > "without a contiguity argument an argmax fill on a 32-byte ICF-prone
  > accessor can plant a genuinely FAKE match. Needs a deliberate lane with a
  > MARGIN rule, not a tie-break."

**That margin rule is this lane's deliverable.** The two calibrations must never
be conflated: 53.6% is the interior class, 87.2%-genuine-scatter is this one.

Re-measured from `splits.txt` directly (never from `report.json` auto-unit
boundaries, which dtk coalesces — that error cost laneAL a 27× undercount):

| | gaps | functions |
|---|--:|--:|
| pinned `.text` blocks | 5,576 | |
| raw gaps | 2,469 | |
| vendor window `0x828–0x82C` excluded *inside* the funnel | −343 | |
| interior (same unit both sides) | 692 | 4 |
| **DIFFERENT-UNIT** | 1,434 | **7,235** |
| …of which hold ≥1 function | **992** | |

Composition: median function 40 B, mean 95 B; 65.4% ≤ 44 B, 71.7% ≤ 68 B — the
MSVC PPC EH-cleanup (40 B) and static-init-guard (32 B) shapes. 111 functions
carry a non-`fn_` name. **0 of the 7,235 match at baseline**, so every gain is
genuinely new.

Tool: `scripts/harvest/diffunit_gap_funnel.py`.

## ★ Correction: `pair_funclets_by_bytes` is NOT uniqueness-gated

Both `docs/plans/lane-al-autocarve-2026-07-26.md` and `splits_move.py`'s
docstring describe objdiff's anonymous-symbol pairing as a
"uniqueness-gated reloc-masked byte signature". Reading
`objdiff-core/src/diff/mod.rs` (the freeqaz fork at `../objdiff`) shows
uniqueness is required in **pass 1 only** (~line 1471):

* **pass 2** (~1507) pairs ambiguous exact-signature groups **greedily**
  (name-sorted zip);
* **pass 2b** (~1553) pairs over-subscribed groups **many-to-one** onto an
  already-consumed base funclet, deliberately, and does not mark `right_used`
  ("byte-identical funclets diff to the same result regardless of which copy
  they pair with");
* **pass 3** (~1578) does same-size **fuzzy** pairing at ≥50% masked byte
  equality.

So a target funclet reaches 100% **iff its masked bytes equal at least one
funclet-shaped Code symbol in the assigned unit's obj — multiplicity is
irrelevant.** laneAL's conclusion that "a wrong owner does not silently score,
it simply fails to pair" is therefore too optimistic: for the modal 32/40-byte
shapes, which occur in hundreds of our objs, a wrong owner scores just fine.
This is the mechanism the margin rule exists to bound, and it is *larger* than
the prior lane believed.

## The measurement: two whole-binary probe builds

Attribution is not a heuristic here — it is measured. Two complete builds:

* **leg L** — every one of the 992 gaps assigned to its LEFT neighbour
  (`diffunit_gap_apply.py --dir left`): **+2,389 strict, 0 losses**.
* **leg R** — every gap assigned to its RIGHT neighbour: **+1,934 strict,
  6 losses**.

Both legs: `AUDIT CLEAN`, `applied 992 skipped 0`, built **twice** with
`touch config.yml` + `rm -f report.cache` (the `dynamic_init` patcher is unstable
on a first build; a symmetric 2-build baseline reads **34,471**, +2 over the
1-build 34,469 — every number below uses the symmetric baseline).

Classifying every gap function **unit-agnostically by name** (this lane moves
functions between units, so a unit-scoped diff shows spurious losses):

| class | count | meaning |
|---|--:|---|
| **L-only** | 1,010 | matched under left-assignment only — evidence for LEFT |
| **R-only** | 559 | evidence for RIGHT |
| **BOTH** | **1,379** | **identity-unresolved**: scores under either owner |
| NEITHER | 4,287 | dead — we do not compile a matching body at all |

**46.8% of every match this pool can yield is a coin flip.** That single number
is the honest core of the lane. It also explains laneAL's 26% argmax sample:
argmax "hits" include the coin flips.

Exclusive evidence favours the **LEFT** neighbour 1,010 to 559. This lane first
read that as residual contiguity; **the held-out calibration falsified that
reading** — see the retraction below. (These four counts are unchanged by the
address fix documented later; only the dead class grew, 4,254 → 4,287.)

## ★ The rule: sub-range cuts, not a direction

Whole-gap argmax is not merely risky, it is **structurally wasteful**, because a
splits pin is a *range* and the two neighbours extend from *opposite ends*. Left
can claim a PREFIX (`left.end` moves right), right a SUFFIX (`right.start` moves
left), and the middle can stay unowned. Both edits are overlap-free by
construction. The decision variable is therefore a pair of cuts
`va_lo ≤ p ≤ q ≤ va_hi`, not a direction.

Per side, take the cut that maximises evidenced functions subject to a margin
against the unresolved riders it drags in:

> include a prefix of length *k* iff `ev_L(k) ≥ 1` **and**
> `ev_L(k) − un(k) ≥ T`, ties to the smaller *k*
> (`ev_L(k)` = functions among the first *k* matched under LEFT only;
> `un(k)` = matched under BOTH). Symmetric for the suffix with `ev_R`.
> If the cuts cross, the side with more evidence keeps its cut.

`T = 0` is argmax-equivalent. Implementation:
`scripts/harvest/diffunit_subrange.py`; application (with a full-file audit that
refuses to write on any overlap / inversion / duplicate block / **sectionless
block**) in `scripts/harvest/diffunit_gap_apply.py --subranges`.

### Measured yield curve (real pool)

| rule | gaps | cuts | evidenced | unresolved | total | **evidenced share** |
|---|--:|--:|--:|--:|--:|--:|
| whole-gap argmax | 380 | — | 1,528 | 747 | 2,275 | 67.2% |
| whole-gap `T≥3` | 136 | — | 1,213 | 428 | 1,641 | 73.9% |
| whole-gap `T≥8` | 45 | — | 790 | 200 | 990 | 79.8% |
| **sub-range `T=0`** | 355 | 363 | 1,448 | 263 | 1,711 | 84.6% |
| **sub-range `T=1`** | 321 | 327 | 1,397 | 208 | 1,605 | **87.0%** |
| **sub-range `T=2`** | 163 | 165 | 1,189 | 161 | 1,350 | 88.1% |
| **sub-range `T=3`** | 109 | 109 | 1,032 | 115 | 1,147 | 90.0% |
| sub-range `T=5` | 63 | 63 | 859 | 93 | 952 | 90.2% |
| sub-range `T=8` | 37 | 37 | 690 | 67 | 757 | 91.1% |

Sub-ranging alone cuts the coin flips **3.6×** at essentially unchanged
evidenced yield (1,397 vs 1,528). The theoretical ceiling of the exclusive
classes is 1,010 + 559 = **1,569**; `T=1` captures 89% of it.

### The hazard is a size effect, and it is measured

Exclusive-evidence rate by function size (`excl% = (L+R)/(L+R+BOTH)`):

| size | L-only | R-only | BOTH | NEITHER | excl% |
|---|--:|--:|--:|--:|--:|
| ≤ 32 B | 403 | 239 | 742 | 1,060 | 46.4% |
| 33–44 B | 543 | 284 | 595 | 846 | 58.2% |
| 45–68 B | 61 | 27 | 35 | 327 | 71.5% |
| 69–128 B | 3 | 8 | 6 | 964 | 64.7% |
| > 128 B | 0 | 1 | 1 | 1,057 | 50.0% |

laneAL's hypothesis — that the fake-match hazard concentrates in short bodies —
is **confirmed**: 1,337 of the 1,379 coin flips (96.9%) are ≤ 44 B. It also
shows why a *size-gated* rule adds little on top of the margin: 94% of the
available evidence is itself ≤ 44 B (at `T=1`: 1,327 evidenced ≤44 B, 83 at
45–68 B, 9 above), so gating on size mostly just raises `T` by another route.
The margin rule dominates because it prices the riders directly.

### The pool's honest ceiling

| | functions | share |
|---|--:|--:|
| different-unit gap pool | 7,235 | 100.0% |
| reachable under *some* owner | 2,948 | **40.7%** |
| …exclusive evidence (fundable) | 1,569 | 21.7% |
| …identity-unresolved (coin flip) | 1,379 | 19.1% |
| unreachable under either owner | 4,287 | **59.3%** |

laneAL's "~+1,900 extrapolated" was right in magnitude — all-left measures
+2,389 — but **roughly half of that headline is coin flips**, and 59% of the
pool cannot be reached by attribution at all (we compile no matching body). Its
instinct to refuse the argmax sweep was correct.

## ★ A defect worth carrying forward: `report.json`'s `address` is not the address

The first sub-range application **failed to build**:

```
Failed: Split RhythmDetector.cpp .text (0x82270B84..0x822715F0)
        ends within symbol 'fn_822715D8' (0x822715D8..0x822715F8)
```

`diffunit_gap_funnel.py` had derived each gap function's VA as
`auto_03_<base>_text` + the report's `address` field. Measured across all
**17,801** anonymous functions in auto units, only **1,917** agree with the true
address; the rest drift *low* by +4, +8, … +40 and beyond, because inter-symbol
alignment padding is not represented in that field. 179 of 327 cuts at `T=1` and
67 of 109 at `T=3` therefore landed *inside* a real symbol.

The authoritative VA is **in the name** — dtk names an anonymous function
`fn_<retail VA>` — with `config/45410914/symbols.txt` authoritative for size.
The 111 mangled-name functions are absent from `symbols.txt` (it only ever emits
`fn_` names; mangled ones are painted on afterwards by the target-symbol
renamer), so they resolve by inverting `scripts/target_symbol_map.json`. After
the fix: 992 gaps / **7,235** functions, and **327/327 and 109/109 cuts land
exactly on a symbol boundary**.

**The classification was unaffected** — it is unit-agnostic *by name* — so every
evidence number above stands. Only the geometry was wrong. Generalisation:
**never use `report.json`'s `address` for anything address-like**; it is a
section offset with padding elided.

## The static byte-signature predictor — validated, and it guts the attribution

An early revision of this document called this channel failed. **That was wrong**
— it sampled a mid-run artifact whose COFF symbol-size inference was broken
(MSVC emits class-6 `$M<N>` label symbols *inside* a funclet, so a naive
"next symbol" size rule sized `__unwind$110213` at 16 B instead of 40 and
silently zeroed the predictor). Repaired, the predictor reproduces objdiff
faithfully and is **the most important result of this lane**.

Method: target-side signatures from a scratch gap-only `dtk xex split` under
`~/tmp` (cross-validated against a real pinned unit — byte-identical sizes and
relocation offsets, so dtk's reloc set is split-independent); base side from a
faithful reimplementation of objdiff's COFF size inference. Calibrated against
`objdiff-cli` itself over 60 pinned units × 12 anonymous functions:
**TP 350 / FP 0 / FN 0 / TN 197 = 547/547**. Direct gap validation 316/320, all
4 disagreements conservative.

It independently reproduces the two probe builds — static vs build-measured:
`L-only 1,009 / 1,010`, `R-only 522 / 559`, `BOTH 1,328 / 1,379`,
`NEITHER 4,406 / 4,287`. A static analysis and two whole-binary builds agreeing
this closely is strong mutual corroboration.

### ★★ Tree-wide multiplicity: only 128 functions are identity-resolved

There are **3,202 distinct masked signatures** among the 7,265 gap functions,
and the common ones are ubiquitous:

| rank | size | occurrences in pool | **# of our 1,024 objs containing it** |
|---|--:|--:|--:|
| 1 | 32 B | 428 | **694** |
| 3 | 32 B | 159 | 502 |
| 4 | 40 B | 122 | 621 |
| 5 | 32 B | 116 | 432 |

Over scoring-capable functions, 95.5% have a signature in ≥2 of our units, 91.4%
in ≥10, 88.9% in ≥20. **This applies to the *evidenced* class too**: median
tree-multiplicity is **122 units for LEFT_ONLY**, 199 for RIGHT_ONLY, 408 for
BOTH.

> A `LEFT_ONLY` verdict therefore usually means only that *the right neighbour
> happens not to contain that stereotyped shape, while 122 other units do.*
>
> **Only 128 of the 1,531 evidenced functions have a signature unique to one
> unit tree-wide — 1.8% of the pool. That is the honest identity-resolved
> count.** Everything else, including every "decisive" gap verdict, is byte-true
> but attribution-arbitrary.

This is the quantitative form of the leg-X result, reached independently, and it
is why the recommendation below covers all 1,627 fills rather than only the 208.

### Three further facts from objdiff's actual code

1. **Both sides must be funclet-shaped**, not just the target: `is_funclet_like`
   gates `right_candidates` too. Our objs contribute only `__unwind$N` /
   `__catch$N` / `??__E*` / `??__F*`. A gap function can therefore never pair
   with a normal mangled method — which is why **no function larger than 84
   bytes can score at all**, in either direction. Funding large gaps is
   pointless, not merely risky.
2. **The report driver ignores relocations entirely**
   (`objdiff-cli/src/cmd/report.rs:381`, `function_reloc_diffs: None`), whereas
   `objdiff-cli diff` defaults to `DataValue`. So once a masked pair is made,
   two funclets with identical instruction bytes but **different callees and
   different string/data references** score a true 100%. The hazard is not
   "byte-identical" but **"identical modulo every relocation target"** — wider
   than this lane assumed throughout.
3. Pass 3's ≥50% fuzzy channel adds only 110 more scoring-capable functions
   (2,859 → 2,969), leaving a ~1.25% conservative floor on the predictor.

### Operational notes

* **`Rnd.cpp` is an unwired neighbour** (objdiff unit exists, no `base_path`):
  any gap fenced by it can never pair on that side. Decisive, and free.
* All 593 neighbour headers map unambiguously to a base obj via `target_path`;
  the 45 basename collisions resolve cleanly by that route. This independently
  confirms the basename trap does not apply to pin resizing.
* 112 gap functions have a `target_symbol_map` entry and would be renamed by the
  pre-compile renamer; **all 112 are class NEITHER** and none of their mangled
  names is defined in either neighbour — the rename channel contributes zero
  ownership evidence here.

Artifacts: `/home/free/tmp/laneAM_sig_predictor.json` (992 gaps),
`/home/free/tmp/laneAM_sig_perfn.json` (7,265 functions), tooling under
`/home/free/tmp/laneAM/`. **Re-running this lane statically costs no builds** —
the earlier "two builds, not zero" claim is withdrawn.

## ★ What exclusivity actually proves — and what it does not

Exclusive evidence discriminates **left vs right**. It does **not** discriminate
against a *third* unit owning the code, and that limitation is quantifiable from
this lane's own measurements.

Let `p` = the probability that an arbitrary non-owning unit's obj happens to
contain a function's masked byte shape. Estimating it from the `both` class,
`p = 1,379 / (1,379 + 1,569) = 0.468`. For a function whose true owner is a
third unit W (neither neighbour):

| outcome | probability |
|---|--:|
| looks **exclusive** to one neighbour | `2p(1−p)` = **0.50** |
| looks `both` | `p²` = 0.22 |
| looks `neither` | `(1−p)²` = 0.28 |

**So a third-unit-owned function produces an exclusive classification half the
time.** Given the neighbourhood density measured above (median 5–6 distinct
pinned units within ±4 KB; only 4.7% of gaps at ≤2), third-unit ownership is a
live hypothesis across much of the pool, and *exclusivity cannot rule it out*.

### ⚠ RETRACTED: the "left bias is a genuineness floor" argument

This lane first argued: third-unit coincidence is symmetric in left/right, so the
observed `exL/exR = 1,010/559` excess of **451** (z = 11.4 vs the symmetric null)
is a *floor* on genuinely neighbour-owned functions. **That argument is wrong and
is withdrawn.** The held-out calibration falsifies its premise directly: on a set
with near-balanced ground truth (123 truth-left / 113 truth-right) the same
statistic came out **201 L-only vs 46 R-only**, because handing a gap LEFT
manufactures a match for **78.8%** of truth-*right* functions while handing it
RIGHT manufactures for only **46.0%** of truth-*left* ones. The two directions
have materially different coincidence rates, so the null is not symmetric and the
excess bounds nothing. **The left bias in the real pool is an artifact of
asymmetric manufacturing, not evidence of left ownership.**

This is exactly what the held-out set was funded to catch, and it is the single
most valuable thing it produced.

## ★ The held-out calibration (the primary deliverable)

236 synthetic gaps / **589 carved functions**, built by un-pinning contiguous
runs of already-100% anonymous `fn_` functions at abutting cross-unit
boundaries, so the true owner is known: 123 truth-LEFT, 113 truth-RIGHT. Five
whole-binary legs off base 34,471 — C (carved, unowned) 33,882, L 34,425,
R 34,270, **X (random certainly-wrong unit) 34,098**. Noise floor **0** (no
collateral outside the carves); arithmetic closes exactly (C + 543 = L,
C + 388 = R); leg C recovers **0 / 589** while unowned. Detail:
`docs/plans/lane-am-heldout-calibration-2026-07-26.md` (branch `laneAM-cal`).
Sample ceiling is real: only ~255 of 1,368 abutting boundaries have a
100%-matching anonymous function at the edge — most edges are *named*.

### Precision / yield

| rule | gap precision | **function precision** | fns claimed | unresolved riders |
|---|--:|--:|--:|--:|
| whole-gap **argmax** | 59.7% | **72.0%** | 589 | — (165 fake = 28.0%) |
| …argmax on gaps *with* exclusive evidence | 100% | 100% | 99 gaps | |
| …argmax on gaps *without* | 30.7% | | 137 gaps | |
| **sub-range T=0** | 100% | **100%** | 249 | 15 |
| **sub-range T=1** | 100% | **100%** | 235 | 8 |
| sub-range T=2 | 100% | 100% | 191 | 7 |
| sub-range T=3 | 100% | 100% | 155 | 5 |
| sub-range T=5 | 100% | 100% | 93 | 1 |
| sub-range T=8 | 100% | 100% | 56 | 0 |

**There is no knee.** Precision jumps 59.7% → 100% between "fund on no evidence"
and "require ≥1 exclusive function", then never moves again. **The binding
constraint is *requiring exclusive evidence at all*, not the size of the
margin** — which is also why sub-range `T=0` does not collapse: its cut already
demands `ev ≥ 1`. **`T = 1` is the recommendation**; `T = 3` buys no measurable
precision and costs 439 matches. This settles the T3-vs-T1 question: the higher
whole-run "evidence share" of `T=3` (89.4% vs 86.9%) is a composition statistic,
not a precision statistic, and precision is flat between them.

The sibling lane's refusal is vindicated quantitatively: **whole-gap argmax
plants 165 fake attributions in 589 functions (28.0%)**, concentrated entirely
in the 137 gaps that carry no exclusive evidence (30.7% precision).

### ★ The honest ceiling on all of this: leg X

The 100% precision figure is **a theorem of the construction, not a
measurement**. Every carved function matches under its true owner by
construction (restoring the true owner restores the original pin byte-for-byte),
so `P(match | true owner) = 1` makes "exclusive" *logically equivalent* to
"true". What it does prove is that objdiff's byte-signature fallback never
**inverts** a label (0 / 247; rule-of-three ≤ 1.2%). It **cannot** see the error
mode that actually matters here: a third unit owning the body.

Leg **X** measures that error mode from the other side. Handing every gap to a
random, *certainly wrong* unit still drove **216 / 589 = 36.7%** of functions to
100%:

| function size | wrong *adjacent* unit | wrong *random* unit |
|---|--:|--:|
| 17–32 B | 81.9% | **57.1%** |
| 33–44 B | 38.5% | 19.5% |
| 45–68 B | 15.0% | 2.5% |

Adjacency is worth only 1.4–6×, rising with size. And **95.1% of this lane's
entire yield (2,805 / 2,948) sits in the 17–44 B band**, where a certainly-wrong
unit scores 19–57% of the time. Functions > 68 B essentially never gain
(17 of 2,040); ≤ 16 B gains nothing.

**Conclusion: a match here is largely a statement about the target obj's shape
inventory, not about ownership.** The rule is sound *relative to the two
candidates* and provably never inverts them — but it cannot establish ownership
in an absolute sense, and the honest disposition is annotation, not confidence.

## What was landed

**`T = 1`, sub-range, applied to 321 gaps / 327 cuts.**

| | |
|---|--:|
| baseline (this lane's own pickle) | 34,469 |
| post | **36,071** |
| gained | 1,604 |
| lost | 2 |
| **net** | **+1,602** |
| units touched | 203 |
| **gains in units the diff does not touch** | **0** |

Predicted from the probe builds: 1,605. Measured: 1,604 gains. The probe→apply
prediction is essentially exact, which independently validates both the
classification and the corrected cut geometry.

The 2 losses are `default/band3/meta_band/MakeupProvider` `fn_8266F8D0` /
`fn_8266F8FC` — a local funclet pairing shuffle inside a unit the fill touches,
identical across every leg, and net strongly positive there.

The **0 gains in untouched units** is the anti-stale-obj check: a stale `.obj` is
stable across repeated clean rebuilds, so "build twice" cannot catch it; the tell
is a gain somewhere the diff does not reach. There is none.

Measured alternative, for the record: `T = 3` lands **+1,053** (89.4% evidenced)
on the pre-fix geometry. `T = 1` was chosen because the marginal cohort between
them is still **79% evidenced** — far above the pool's 53% base rate — while the
dominant uncertainty (third-unit ownership) is identical at both thresholds, so
the extra 439 matches do not buy less honesty per match.

## ★ Identity status of these fills — annotate them

**Every one of the 1,604 new matches is byte-true**: objdiff compared bytes and
found them equal. What is *not* established is **which unit owns them**. Per the
analysis above the attribution is *pairwise* evidence only. So these fills belong
in the same doctrine as `target_symbol_map.json`'s `_icf_arbitrary` (25) and
`_bijection_arbitrary` (1,207): **bytes true, attribution not evidence.**

Two tiers, listed in **`docs/plans/laneAM/fills-T1.json`**
(`{va, name, size, claimed_by, evidence}`):

* **1,419 `exclusive_byte_signature`** — our claimed unit's obj carries the byte
  shape and the other candidate's does not. Real evidence, with a ≥451 genuine
  floor, but ~50% of any third-unit-owned function would look like this.
* **208 `identity_unresolved`** — matched under *either* neighbour. Pure coin
  flip; funded only because they are geometrically interior to an
  evidence-carrying cut and a splits pin is a contiguous range.

**Recommend annotating the whole set as unit-attribution-unresolved**, with the
208 flagged as fully arbitrary. Leg X is the reason the recommendation covers
all 1,627 and not just the 208: at the sizes that dominate this yield, even a
certainly-wrong unit scores 19–57% of the time.

## Remaining, honestly

* **This pool is now largely worked.** Of 7,235 functions: 1,604 landed, ~1,344
  more are reachable but declined (1,379 `both` minus the 208 funded riders,
  plus exclusive functions unreachable by a contiguous cut), and **4,254 (59%)
  are unreachable under either owner** — we compile no matching body. That
  residue is a *source* problem, not an attribution one, exactly as laneAL
  concluded for its own residue.
* **Re-running is cheap**: the validated static predictor (547/547 against
  objdiff-cli) reproduces the probe builds, so a re-measure needs no build at
  all. Re-run after objs move substantially.
* **Do not raise the threshold hoping for precision** — it is flat in T. The
  only way to improve attribution here is a *third* signal that can exclude
  third-unit owners: parent-funclet association (`.pdata` parent → owning TU) is
  the obvious candidate and was not built in this lane.
* The 111 map-named gap functions remain source work, not attribution work: 23
  have a map VA and **0** are defined by any compiled obj of ours.



## What has NO evidence here

`report.json` and `target_symbol_map.json` were joined against a COFF symbol-table
parse of all 1,024 compiled objs:

* of the 7,202 gap functions, **23** have a `target_symbol_map` VA entry and
  **0** are defined by any compiled obj of ours. The name-evidence channel for
  this pool is **completely empty** — independently confirming laneAL Branch 1.
  (The gaps file's `n_named = 111` is a different signal — "report name does not
  start with `fn_`" — and must not be conflated with map membership.)
* binary-wide, only **2** VAs are map-named, defined by exactly one obj, and
  unpinned (`DirectionGestureFilterDoubleUser::Draw`,
  `MultiTempoTempoMap::AddTempoInfoPoint`), and **neither falls inside any of the
  992 gaps**. laneAL's trap "subtract evidenced sub-ranges before applying" has
  **no live ammunition** in this pool. Recorded at
  `/home/free/tmp/laneAM_evidenced_unpinned.json` anyway.

**Correction to the basename trap.** laneAL's −613 measurement came from *wiring
a new file into `objects.json`* whose objdiff unit name (`default/<stem>`)
collided with an existing one. It does **not** apply to resizing pins:
`system/rndobj/Utl.cpp`, `system/obj/Utl.cpp` and `system/synth/Utl.cpp` already
appear as three *distinct* report units, and this lane only edits `start`/`end`
of blocks keyed by the exact `(header, start, end)` triple. The 120 gaps flagged
for "basename ambiguity" are therefore not a decline reason; the empirical L/R
legs (0 losses under all-left) corroborate this.

## Neighbourhood density

Distinct pinned units within ±4 KB of each gap: median 5–6, max 22 — the dense
multi-TU interleave laneAL found in the big runs. Only **47 / 992 (4.7%)** sit at
≤2 neighbouring units. 17 gaps hold ≥50 functions and read as *whole missing
TUs* rather than scatter holes; the largest are `HamMove.cpp`↔`Waypoint.cpp`
(127 fns / 8,792 B), `RockCentral.cpp`↔`CheatProvider.cpp` (126 / 12,392) and
`VocalTrackDir.cpp`↔`TrackPanelDir.cpp` (106 / 14,672). Per-gap counts:
`/home/free/tmp/laneAM_gap_neighbourhood.json`.
