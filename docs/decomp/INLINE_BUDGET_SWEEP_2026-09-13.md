# Lane W7-B — generalising W6-A's inline-budget finding into a sweep

**Branch** `w7-inline-budget` · worktree `~/tmp/wt-w7-b` · base `3a6bfe40`
(`git merge-base --is-ancestor` asserted before the first edit).

**Result: +36 matched functions / +864 B / +0.008433 pp**, in four landed steps
and one measured revert.

| | matched | masked_equal | honest | matched_code | code% |
|---|---|---|---|---|---|
| baseline (settled leg A) | 42,676 | 22,937 | 19,739 | 3,859,912 | 37.672540 |
| final | **42,712** | 22,971 | 19,741 | **3,860,776** | **37.680973** |

⚠ **Read the headline honestly: 34 of the +36 are `masked_equal`, so Δhonest is
+2.** Two rows crossed and paid bytes; three more gained 14–34 pp of fuzzy and
paid **nothing**, because `matched_code` is all-or-nothing per row. The lane is
best described as *four correctness fixes, two of which happened to cross*.

Anti-vacuity first (FOLDPROVE-2): the worktree was FULLY BUILT before any
name-keyed work, and `verify_objs_patched.py --verify-manifest` returned exit 0
over 1,205 decomp + 3,084 target objects.

---

## 1. The detector

`tools/inline_budget_sweep.py`. W6-A's diagnosis was arithmetic, and the
arithmetic is what is mechanised — the model is **two independent measurements
of the same quantity**, and the detector fires only when they agree.

Let `R` = retail body length in instructions (`target_size/4`), `s` =
instructions the helper occupies when inlined, `M` = sites MSVC did inline,
`K` = sites it gave up on (each now a `bl helper` in our body), `N = M+K`.

```
R  ~=  overhead + N*s  =  overhead + P + K*s
  =>   s ~= (R - P) / K      (A)  measured from the un-inlined TAIL
       P  = M * s            (B)  measured from the aligned HEAD
```

Clauses, in the order they reject:

| | clause | rejects |
|---|---|---|
| C1 | `target_size >= 400 B` | too short to show a long aligned stretch |
| C2 | `MIN_P <= P`, `P/R <= 0.95` | no aligned stretch / aligned throughout |
| C3 | `(insert+delete)/R >= 0.05` | alignment never actually lost (arg-charge rows) |
| C4 | helper occurrences not on `equal` rows | retail calls it there ⇒ fold, not un-inlined |
| C5 | `\|(R-P)/K - s\| <= 0.35`, `\|P - M*s\| <= 2`, `s>=2`, `K>=2`, `M>=3` | (A) and (B) disagree |
| C6 | retail's callee on those rows is not the **same helper modulo template args** | container/instantiation divergence |

### 1.1 ⛔ Three of the definitions were WRONG, and only a live control caught it

My first detector scored the **known positive** `C2_no_prefix` — it would have
**missed the very case it was built from**. The control was built by reverting
W6-A's `__forceinline` and rebuilding, then re-running the detector.

* **`P` is the longest contiguous run of rows that are neither `insert` nor
  `delete`** — *not* the leading run of `equal` rows. W6-A's row has a leading
  equal-run of **2** and a longest equal-run of **13**; the prologue differs
  (our frame `-0x100` vs retail `-0x190`) and **495 rows inside the aligned
  stretch are `diff_arg`**. Only insert/delete break the 1:1 correspondence.
  With the corrected definition the run is rows 4..901 and the first `insert` is
  at **902**, whose base side is `DataRandomFloat` — registration **#70,
  "random_float"**, exactly as W6-A recorded.
* **`P mod s == 0` is too brittle.** The run boundary moves with the prologue
  (I measure 898 where W6-A counted 897), so `898 % 13 = 1`. Require
  `|P - M*s| <= 2`.
* **`n_insert >= K/2` is wrong.** Only **1** of the 85 un-inlined calls sits on
  an `insert` row (80 are `diff_arg`, 4 `replace`).

On the live pre-fix build the finished tool recovers, with nothing hardcoded:

```
8068B fz=71.4566 R=2017 P=898 K=85 s=13(13.1647) M=69 N=154 | ?DataInitFuncs@@YAXXZ
```

**K=85, s=13, M=69, N=154 — all four of W6-A's numbers, derived de novo.**

### 1.2 The control can fail

`--selftest` carries **14 fixtures**. A mutation harness disables each detector
clause in turn and requires the suite to go red: **10/10 clauses covered**.

⚠ **Two vacuities were caught inside the selftest itself**, both of the family
that returns the answer you expect:

1. A mutation whose pattern was mis-escaped **never applied**, so the file was
   unmutated and its "PASS" meant nothing.
2. Fixture 9 was broken and fired, which made the suite red for **every**
   mutation — so the first "all clauses covered" reading was worthless. Fixing
   the fixture revealed that three clauses had **no isolating control at all**.

## 2. The population, and the measured false-positive rate

Screen = every **paired** (`fuzzy > 0`) sub-100 row in `report.json`. Unpaired
rows score `fuzzy == 0` and cannot be instruction-diffed at all, so they are
outside any instrument of this kind. Binary-wide that is **7,737 rows**; 1,365
at ≥200 B; **680 at ≥400 B** (the detector's own floor makes ≥400 B the complete
in-scope set at default settings). Instruction diffs came from
`objdiff-cli diff --batch --include-instructions` on the **graded** ruler.

| detector | in scope | fires | true | false | **FP rate** |
|---|---:|---:|---:|---:|---:|
| C1–C5, floor 400 B | 681 | 1 | 0 | 1 | **100%** |
| C1–C5, floor 200 B | 1,365 | 2 | 1 | 1 | **50%** |
| C1–C6, floor 200 B | 1,365 | 1 | 1 | 0 | 0% *(fitted — see below)* |

**Both fires were adjudicated on retail bytes, so the FP rate is exact, not
sampled.** Reject histogram at floor 200 (C1–C6): `C2_aligned_throughout` 839,
`C4C5_no_helper_or_arith_disagrees` 305, `C3_not_misaligned` 170,
`C2_no_aligned_stretch` 50, `FIRE` 1.

⚠ **The 0% after C6 is NOT an independent measurement.** C6 was added *because
of* the FP it removes; quoting it as the detector's error rate would be fitting
the model to its own test set. The honest number for a fresh binary is the
**50%** row.

### 2.1 The false positive, and why the obvious fix would have killed the detector

`?StartRefresh@XboxContentMgr@@` (932 B, fuzzy 69.38) fired with
K=2 s=29 M=6 N=8 on `list<Content*>::insert`. Retail bytes refute it:

```
120 diff_arg TGT bl ?insert@?$list@PAVObject@Hmx@@...  | BASE bl ?insert@?$list@PAVContent@@...
```

Retail **does** call insert there; the divergence is the container element type
(`list<Hmx::Object*>` vs our `list<Content*>`). Nothing was un-inlined.

The tempting tightening — *"the target side must not be a `bl`"* — **would kill
the known positive**, because 80 of `?DataInitFuncs@@`'s 85 un-inlined rows pair
against retail's inlined `bl ??A?$map@...`. The working discriminator is whether
retail's callee is the *same helper modulo template arguments* (first two
`@`-separated mangled tokens). Fixture 14 is the positive control for this:
identical geometry with a genuinely different retail callee must still fire, so
C6 cannot be satisfied by "the target row happens to hold any `bl`".

A third instance of the same family, caught earlier by the `equal`-row rule:
`?SetName@CharIKFingers@@` calls `SetObjConcrete<RndTransformable>` 46 times
where retail calls `SetObjConcrete<BandCharacter>`.

⇒ **The dominant false-positive mechanism for this detector is template /
container instantiation divergence, and it appears on BOTH `equal` and charged
`diff_arg` rows.**

## 3. ★ The strict arithmetic is narrow BY CONSTRUCTION — the second tier

The model assumes the body is **dominated** by the repeated site. In
`?DataInitFuncs@@` the overhead is ~15 of 2,017 instructions (**0.7%**). In a
class constructor most of the body is other member initialisation, so
`(R-P)/K` is simply not `s`, and C5 rejects every one of them.

So the qualitative half of the signature was run separately: *a helper we call
≥2 times that retail never calls, no `equal` rows, no sibling instantiation*,
**with no arithmetic test**. Binary-wide that is **153 distinct functions / 177
(function, helper) pairs**, of which **22 functions / 15,396 B** are the
`ObjPtr` / `ObjRefConcrete` family.

⚠ **Tier B is a candidate pool, not a diagnosis.** It has no arithmetic
agreement behind it, so it is exactly the sort of screen that confirms whatever
you point it at. Every Tier-B row landed below was adjudicated on retail bytes
first, and one of them still measured **negative** (§4).

★ The family turns out to have an **existing per-TU lever** —
`RB3_TU_OBJPTR_FORCEINLINE_CTOR` (lanes NCCC f187/f332), already carried by
CharServoBone, CharIKHead, CharMeshHide, CharBonesBlender, GemTrack,
ChordShapeGenerator, UIListLabel and others. The detector independently
rediscovered its population; the value here is *which TUs still qualify*.

## 4. Per-candidate: predicted vs measured

All legs settled both sides, `functionRelocDiffs=name_check`, via
`tools/ab_measure.py --from-dirty`, **one change per run**, each committed
immediately after measuring.

| # | change | tier | predicted | measured | |
|---|---|---|---|---|---|
| 1 | `CharNeckTwist` ObjPtr force | A (fires) | fuzzy ≥90; bytes **bimodal** +224/+0, prior on **+0** | 69.571 → **100.0**; **+5 fns / +292 B** | ✓ direction, **magnitude LOW** |
| 2 | `CharIKFingers` ObjPtr force | B | fuzzy ≥90; +776 B or +0; downside risk on `?SetName@` | 64.608 → **98.675**; **+12 fns / +0 B** | ✓ (lower branch) |
| 3 | `CharLookAt` + `CameraShot` | B | both >95; **one crosses, ≈+400–600 B** | 73.303 → **100.0** / 68.387 → 97.632; **+4 fns / +572 B** | ✓ **exact** |
| 4 | `BandCharacter` + `BandDirector` | B | 0..+1100, **genuine regression risk** | **+14 fns but −128 B / −1 honest** | ✗ risk MATERIALISED |
| 4b | `BandCharacter` alone | B | — | 77.965 → **91.945**; **+15 fns / +0 B** | kept |
| — | `BandDirector` | B | — | **−128 B** | **REVERTED** |
| | **lane total** | | | **+36 fns / +864 B / +0.008433 pp** | |

Step 1 was measured twice (identical patch hash `ff7a6d72608f33b7`, identical
result) after I contaminated the first run — see §6.

### 4.1 The negative, in full — `BandDirector`

Predicted regression risk, and it happened. The joint run read **+14 matched**
(headline UP) while bytes went **−128** and honest **−1**; per-unit attribution
split it in one line. `BandDirector` failed *two* ways at once:

* it made its **own target row worse** — `??0BandDirector@@` 83.1397 → 82.9890;
* it **broke an unrelated already-perfect row** in the same TU —
  `?Add@ObjKeys@@QAAHPAVObject@Hmx@@M_N@Z`, 128 B, **100.0 → 69.625**.

That is CLAUDE.md's recorded hazard — an inline-policy change perturbs untracked
helper inlining elsewhere in the TU, the same mechanism that made the PCH
experiment regress `char rndobj world ui` — **measured here rather than
assumed**. It is also the concrete answer to the brief's warning that inlining
can go the wrong way (lane W5-C's 12 B tail-call).

⇒ **Had I measured the two together and stopped at the +14, I would have landed
a byte regression while reporting a gain.** Per-unit attribution is what caught
it; the whole-binary headline actively concealed it.

## 5. Negatives and non-candidates worth not re-hunting

* ⛔ **`?Transform@CSHA1@@`** (5,856 B, fuzzy 55.69) is **not** this disease
  despite looking ideal. P=1, zero repeated un-matched helpers, 912 `diff_arg` —
  regalloc/scheduling.
* ⛔ **`?transform@MD5@Quazal@@`** (6,068 B, 82.19) is the **inverse**: retail
  calls `fn_82B45630` **four** times where we call `decode` once. We over-inline
  relative to retail. A separate lever, not touched.
* ⛔ **`?Save@RndEnviron@@`** (1,056 B, 43.95) reached the ObjPtr shortlist but
  retail bytes refute it: 141 `delete` rows and retail calling
  `bl ?WriteEndian@BinStream@@` where we call a templated `operator<<`. A
  different defect; **not** an inline-budget case. Not touched.
* ⛔ **`?StartRefresh@XboxContentMgr@@`** — the measured FP (§2.1). The real
  finding is a **container element-type divergence** (`list<Content*>` vs
  retail's `list<Hmx::Object*>`), which is a genuine handoff, just not this one.

## 6. What this lane did NOT do

* ⛔ **Did not touch `BandDirector.cpp`** — measured negative, reverted; the
  branch leaves it unchanged.
* ⛔ **Did not work the remaining Tier-B ObjPtr rows**: `?Load@RndMesh@@`
  (3,452 B, 88.19), `?Load@OutfitConfig@@` (1,120 B, 99.88),
  `??1BandCharacter@@` (584 B, 99.86), `?SetupScore@BandScoreboard@@` (556 B,
  99.89), `??0RandomGroupSeqInst@@` (444 B, 99.91), `??1StreakMeter@@` (404 B,
  99.75). The ≥99.7 ones are one or two charges from crossing and are the best
  remaining size-if-it-crosses bets; `RndMesh` is the largest but `rndobj/` is
  the dir CLAUDE.md records as perturbation-prone, and BandDirector just
  demonstrated that hazard costing 128 B.
* ⛔ **Did not chase the other 131 Tier-B functions** outside the ObjPtr family.
* ⛔ **Did not lower `MIN_SIZE` below 400 in the committed default.** The one
  true positive found binary-wide (`CharNeckTwist`, 224 B) is only visible at
  200; the default is left at 400 and the floor is a documented parameter rather
  than silently retuned to the one case that benefits from it.
* ⛔ **Did not rebuild jeff, objdiff, wibo or objcache**; did not touch
  `symbols.txt`, `splits.txt`, `objects.json` or any map file. Every change on
  this branch is a source `#define` plus the new tool and this document.
* ⚠ **Did not re-run the whole-binary sweep on the FINAL tree.** The population
  figures are measured at the lane's base commit; four TUs have since changed,
  so the fire list would need regenerating before the next lane trusts it.

## 7. Reusable lessons

* ★ **A detector must be validated against a LIVE positive, reconstructed.** The
  fixture version of W6-A's geometry passed while three of my clause definitions
  were wrong; only rebuilding the actual pre-fix object exposed that `P` meant
  something different from what I assumed. A synthetic fixture inherits your
  misunderstanding.
* ★ **A selftest can be vacuous in two independent ways at once** — a mutation
  that never applies, and a broken fixture that makes the suite red
  unconditionally. Both produce "all clauses covered". Check that each clause's
  control fails *individually*.
* ★ **The tempting tightening after an FP is often the one that kills the true
  positive.** "Retail must not call anything at that row" is exactly wrong here;
  the discriminator is *which* callee.
* ★ **A joint A/B hides a regression inside a positive headline.** +14 matched
  concealed −128 bytes and a destroyed 100% row. Per-unit attribution, not the
  whole-binary delta, is what makes a multi-TU run safe.
* ★ **An arithmetic screen is narrow because its model has an assumption** —
  here, that the repeated site dominates the body. Knowing *why* it is narrow
  turns "0 fires" from a dead end into a second, clearly-labelled tier.
* ★ **`__forceinline` remains a linkage change, not a match-local one.** W6-A's
  native-gate failure is why every gate used here is the per-TU `#define` form
  that leaves the out-of-line body intact.
