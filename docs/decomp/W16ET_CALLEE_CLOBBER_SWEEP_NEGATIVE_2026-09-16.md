# W16-ET — the callee-clobber signature does NOT identify the callee-clobber lever

Lane W16-ET, 2026-09-16, worktree `~/tmp/wt-w16-et`, branch `w16-et`, off main `fa93fb76`.
Ruler: shipped graded `name_check`, read from `build/45410914/report.json`
`provenance.diff_config` — not assumed. Baseline measured in **this** worktree, settled to
zero work:

```
matched=43969  masked_equal=23224  honest=20745  code%=40.305344  fuzzy=50.010094
total_functions=69240  total_code=10247068   tool_commit a5f0ea903ec1
```

⚠ This is **not** W16-ER's baseline (43958 / 4125560). Main moved between the lanes. Both of
my legs were measured in my own worktree; I inherited no figure I did not measure in-run.

## Result in one line

The mechanism W16-ER discovered is real, but **the diff-shape signature the brief prescribed
for finding more of it carries no information** — measured against arms where the mechanism
is *impossible by construction*, its enrichment is **0.87×**. Separately, four retail-byte
source fixes in `VocalPart` bought **+2 functions / +180 B**.

---

## 1. The brief verified literally — and then refuted

Verified exactly before building anything on it: `?GetBestHit@VocalPart@@…` is **528 B at
fuzzy 96.9697**, and all four secondary figures are exact too (`GetNoteSliceWeight` 484/88.1240,
`FramePhraseMeterFrac` 136/57.6471, `IsEmptyPhrase` 116/65.2414, `InTambourinePhrase` 44/18.0000).
The brief's *numbers* are all correct.

⛔ **The brief's central claim about `GetBestHit` is false, and was checkable in one query.**
It names `GetBestHit` "a `GetNoteRange` sibling, i.e. the most likely second instance of the
same mechanism". `GetBestHit` does not call `GetNoteRange` at all — it calls
`?ScoreNote@VocalPart@@`, and **`ScoreNote` is at `fuzzy 100.0000 / mpn 100.0000`, byte-exact**.
A byte-identical callee has a byte-identical clobber set *by construction*, so the
callee-clobber mechanism cannot be operating on this row. ("Sibling" was true only in the weak
sense that both are callees of `ScoreSinger`.)

`GetBestHit`'s real residual is 4 instructions of **pure scheduling**: `stfs f28, 0x54(r1)` and
`mr r5, r31` appear on *both* sides, each displaced by two positions, around the byte-exact
`ScoreNote` call. `target_size == base_size == 528`.

⛔ **That also refutes clause 3 of the briefed signature on its own flagship row** — the brief
asks for "`base_size` above `target_size` by a couple of instructions"; here they are equal.

**Negative result (mine, distinct from ER's).** ER measured *declaration* order inert (a
declaration with no initialiser). I measured **statement order of two independent local
initialisations** inert: retail orders the `octaves` init before the `pitch` init and we do the
reverse, so I swapped the two statements. Result: whole binary **byte-identical**
(43969 / 4130116 / 40.305344 / 50.010094) with **1 confirmed recompile**, and the residual was
**bit-identical — the same four charges at the same four indices**. MSVC normalises the reorder
away. `GetBestHit` is a scheduling wall; the permuter is OFF by standing directive, so I left
it. I did **not** re-fund definition order, which the brief forbids and ER measured.

---

## 2. The sweep — how it was built, and why its controls are the point

`tools/callee_clobber_sweep.py` (committed). Two stages.

**Stage 1 — structural, free.** The mechanism needs the callee in the **same TU** (the analysis
is intra-TU; this build has no LTCG, verified) and needs **our callee body to be wrong**. Both
are checkable without objdiff: build an intra-TU call graph from the relocations of our **own**
compiled objects and intersect it with `report.json`.

⚠ `REL24 = 0x06` was **validated empirically, not assumed**: it is the only relocation type that
ever targets a defined function symbol (275 hits in `VocalPart`; types `0x10/0x11/0x12` score
zero), and the known `GetBestHit`→`ScoreNote` edge is `0x06`.

Population: **8,255 named sub-100 rows / 3,304,912 B across 1,083 units**; 2,659 of them live in
units with a base object and were carried into stage 2.

⛔ **Stage 1 fires on almost everything, so I ran the untreated-population control before
reporting it as a lever** — and that control is the single most important number here:

| predicate | TREATED (sub-100 callers) | CONTROL (**byte-exact** callers) | enrichment |
|---|---|---|---|
| any non-exact same-TU callee | 1963/2107 = 93.17% | 6816/11559 = **58.97%** | 1.58× |
| has a SUB100 same-TU callee | 373/2107 = 17.70% | 1021/11559 = 8.83% | **2.00×** |
| has an UNPAIRED same-TU callee | 1857/2107 = 88.13% | 6110/11559 = 52.86% | 1.67× |

**59% of byte-exact callers also sit on top of a non-exact same-TU callee.** "Your callee is
wrong" says almost nothing about whether your row is wrong. This is the same disease the
campaign already recorded for the "callee absent from map ⇒ fold-alias" model (~1.95× used as
a deterministic classifier); 2.00× is not a classifier either.

**Stage 2 — diff shape, parameter-free.** Partition each function's instructions into
call-setup blocks at `bl` boundaries and measure the largest fraction of charges falling in one
block (ER's `ScoreSinger` had 18 of 19 in one such block). Classify that block's callee into arms
where the mechanism is **possible** vs **impossible by construction**. 2,659 rows, 0 objdiff
failures, ~0.28 s each.

Concentration is not rare: **58.9% of all sub-100 rows** put ≥95% of their charges in one block.

| block callee class | rows | signature | rate |
|---|---:|---:|---:|
| `SAME_TU_SUB100` — possible | 105 | 60 | **57.1%** |
| `SAME_TU_UNPAIRED` — possible | 180 | 88 | 48.9% |
| `SAME_TU_EXACT` — **impossible** (byte-exact callee) | 216 | 120 | **55.6%** |
| `EXTERNAL` — **impossible** (no LTCG) | 1775 | 1010 | 56.9% |
| `NO_CALL_IN_BLOCK` — no call at all | 383 | 286 | **74.7%** |

⚠ **The arms must be corrected using ER's own case**: ER's culprit `GetNoteRange` is
**UNPAIRED** (the `fn_826F1EC8` phantom extent), so "our callee body is wrong" has to include
`SAME_TU_UNPAIRED`. Corrected:

```
mechanism POSSIBLE   : 148/285  = 51.9%
mechanism IMPOSSIBLE : 1416/2374 = 59.6%
enrichment           : 0.87x
```

**The signature is slightly LESS common where the mechanism can operate than where it cannot.**
And the highest rate of all belongs to rows with **no call in the block at all** (74.7%) —
because "charges bunch together" is simply what any localised defect looks like.

### Why the signature cannot work — the structural reason

⛔⛔ **In ER's own founding instance, the call whose argument setup carried the charges was NOT
the call whose callee was at fault.** ER's 19 charges sat in the argument setup for
**`GetBestHit`**; the defect was in **`GetNoteRange`**, a different callee entirely, whose
heavier body created the register pressure visible at the `GetBestHit` site. So "look at which
call the charges cluster around" points at the **wrong function** even when it correctly flags
the row. That is a structural defect in the signature, not a tuning problem, and it predicts
exactly the flat discrimination measured above.

### Anti-vacuity check — can the instrument see ER's case?

A negative result is worthless if the detector is blind. Reconstructed from ER's recorded
numbers: `ScoreSinger` had 19 charges, 18 in idx 47–72 ⇒ conc = 18/19 = **0.947 ≥ 0.8**,
n_charges = 19 ≤ 30 ⇒ **signature TRUE**, block callee `GetBestHit`, same-TU and (then) sub-100
⇒ **`SAME_TU_SUB100`**. The census **would have flagged it**. So the flat discrimination is a
real property of the population, not an instrument failure.

### Honest sizing of what is left

Rows with the **full** ER shape (≥5 charges, conc ≥ 0.8, same-TU non-exact callee):
**13 rows / 3,760 B = 0.0367% of `total_code`**, and those are unvalidated — most will be
ordinary own-body defects. Largest: `?SetFrame@RndMorph@@` (780 B, 14 charges),
`?InitSmasherPlates@GemTrackResourceManager@@` (672 B, 11), `?Poll@CharGuitarString@@` (376 B, 22).

⇒ **The callee-clobber lever is real but not targetable by this signature, and the
signature-selected vein caps out at 0.037% of `total_code`. Do not fund a census here again.**
What survives from ER is the **probe**, not the signature: stubbing a *suspected* callee with
`__declspec(noinline)` still separates "our body is heavy" from "our body is wrong" in one
build — but you must arrive with a suspect by other means, because the diff does not name one.

---

## 3. What I changed — four retail-byte fixes in `VocalPart`

None of these is the callee-clobber mechanism. They came from reading the diffs.

1. **`IsEmptyPhrase`** and **`InTambourinePhrase`** — `mPhrases.data() + mPhrases.size()` →
   `mPhrases.end()`. Retail loads the vector's stored `_M_finish` at `0x4(vec)`; we recomputed
   it as `begin + (end-begin)/0x38*0x38` (a `li 0x38`/`subf`/`divw`/`mulli`/`add` block).
   `InTambourinePhrase` also needed the return shape changed from an accumulator
   (`bool result = false; … result = true;` ⇒ `li r3,0` first) to two returns (retail
   materialises `li r3,1` mid-body and falls to a shared `li r3,0`).
2. **`FramePhraseMeterFrac`** — branchy clamp → `Clamp(0.0f, 1.0f, ratio)`, plus if/**else**
   for the ratio instead of initialise-then-overwrite. `Clamp`'s float specialisation in
   `src/system/math/Utl.h` is `Min(Max(min,value),max)`; expanded, `Max(0,x)` is
   `fneg`+`fsel(-x,0,x)` and `Min(·,1)` is `fsub`+`fsel(x-1,1,x)` — **matching retail's idx
   29–34 instruction for instruction**. That is an identification off retail bytes, not a guess.
3. **`GetNoteSliceWeight`** — loop 1 held `float frameMs = kFrameTimeMs;`, a **local copy of the
   global**, and `std::min` takes `const float&`, so the copy had to be spilled. One cause
   predicted four symptoms at once, which is why I believed it: `__savefpr_20`→`_21` (one fewer
   FPR), retail's extra `r30` save (a GPR holding `&kFrameTimeMs`), the stack temp at
   `0x50(r1)`, and `mr r11,r31` vs `addi r11,r1,0x54`. A follow-up probe then showed the two
   arms of the `std::min` select swapped with inverted branch polarity (`blt` vs `bgt`) —
   reversed argument order — so loop 1 became `std::min(kFrameTimeMs, spC)`, matching loop 2's
   existing order. 88.1240 → 93.8926 → **94.9256**.

### Control that shaped the scope

The `data() + size()` idiom occurs **8 times** in this file, not twice. I checked whether any
function containing it is already at 100% — that would prove retail used the idiom and the
change is wrong. **None is.** But 6 of the 8 sites sit in functions with **no target row at
all** (`IsPhraseMarkerAtEnd`, `AtPhraseEnd`, `PhraseHasUnpitchedNotes`, `UpdateMinMaxPitch`,
`CalculateRemainingTambourineTicks`, `GetFreestyleSectionDurationMs`, `Poll`), so I have retail
bytes in neither direction. **I changed only the two sites I can adjudicate** and handed off the
rest rather than guess.

### Two more negative results

| probe | measured |
|---|---|
| `IsEmptyPhrase`: separate `int count` local for `unk10` (to reproduce retail's redundant `clrrwi r10,r10,0` self-move) | **inert**, 96.5517 unchanged — reverted |
| `GetNoteSliceWeight`: un-hoist `float two = 2.0f;` to a literal, so MSVC would load it from the pool in-loop as retail does | **inert**, 94.9256 unchanged — MSVC re-hoists it by loop-invariant code motion. Unverifiable *and* Δ0 ⇒ reverted rather than land churn |

---

## 4. Prediction vs measured

Pre-registered in `docs/decomp/W16ET_PREREGISTRATION.md`, committed in `10e2ed76` and
`4fb7ac8d` **before** `ab_measure` was run, so the numbers cannot be back-fitted.

| row | size | before | predicted | measured | verdict |
|---|---:|---:|---:|---:|---|
| `InTambourinePhrase` | 44 | 18.0000 | **100** | **100.0000** | ✅ HIT |
| `FramePhraseMeterFrac` | 136 | 57.6471 | **100** | **100.0000** | ✅ HIT |
| `IsEmptyPhrase` | 116 | 65.2414 | 100 (MEDIUM) | 96.5517 | ❌ MISS — improved, did not cross |
| `GetNoteSliceWeight` | 484 | 88.1240 | 100 (MED-HIGH) | 94.9256 | ❌ MISS — improved, did not cross |
| `GetBestHit` | 528 | 96.9697 | unchanged | 96.9697 | ✅ HIT |

Whole binary: predicted **+4 / +780 B** if all four closed, **+2 / +180 B** conservative.

```
Δmatched=+2  Δmasked_equal=+0  Δhonest=+2  Δcode%=+0.001758pp  Δcode_bytes=+180
Δfuzzy=+0.001526pp  (50.010094 -> 50.011620)
units at 100%: 189 -> 189 (0 reached, 0 fell off)   [as predicted]
unit improvements: +2  default/VocalPart (31->33)
unit net (ALL units) = +2  ==  whole-binary Δmatched = +2
```

**The conservative prediction hit exactly.** `Δmasked_equal=+0` with `Δhonest=+2` ⇒ real honest
matching, not a funclet/masked artifact. `unit net == whole-binary Δ` ⇒ no hidden regression
anywhere paid for this. Both misses were flagged MEDIUM confidence in advance, and
`IsEmptyPhrase`'s surviving charge is **exactly one of the residual indices I named as the
risk** — it is down to a single 4-byte instruction (`clrrwi r10, r10, 0`, 112 vs 116 B).

⚠ The two misses contribute **0 bytes** (`matched_code` is all-or-nothing per row) but are
landed anyway: both are genuine source corrections adjudicated on retail bytes, and accuracy
outranks the headline.

---

## 5. What I did NOT do

- **No permuter** (OFF by standing directive), and I did not propose one for `GetBestHit`
  despite it being a textbook scheduling residual.
- **Did not re-fund declaration or definition order** — ER measured both inert and the brief
  forbids it. My statement-order probe is a *different* lever and is reported above.
- **Did not pin `GetNoteRange`** (`fn_826F1EC8` + `fn_826F1F30` is an over-carve; ER's reason
  stands) and **planted no name for `?Poll@VocalPart@@`** — a third lane declining it, still for
  want of retail-byte evidence. Naming under `name_check` converts a forgiven placeholder site
  into a checked one.
- **Did not change the 6 unadjudicable `data() + size()` sites** (see §3).
- **Did not validate the 13 full-ER-shape rows** — they are handed off, unverified, and I have
  said so rather than presenting them as a candidate list.

## 6. Handoff

1. ⛔ **Do not rebuild the callee-clobber census.** It is done (`tools/callee_clobber_sweep.py`),
   the discrimination is **0.87×** against impossible-by-construction controls, and the
   signature points at the wrong callee even when it flags the right row. The remaining
   signature-selected vein is **13 rows / 3,760 B / 0.0367% of `total_code`**.
2. **`IsEmptyPhrase` is one 4-byte instruction from 100%** (+116 B): retail emits a redundant
   `clrrwi r10, r10, 0` self-move between `bne cr6` and `subic. r10, r10, 0x1`. A separate
   `int count` local does *not* produce it (measured). Likely a signed/unsigned or width
   conversion in retail's source.
3. **`GetNoteSliceWeight` is at 94.9256** (+484 B if closed). The whole residual is one
   allocation decision: retail spends a GPR (`r30`) on a constant-pool base and loads `2.0f`
   in-loop, using one fewer callee-saved FPR (`__savefpr_21` vs our `_20`). Un-hoisting the
   literal does not reproduce it (MSVC re-hoists). Something else in the loop-2 body is holding
   an extra FPR live.
4. **6 unadjudicable `data() + size()` sites remain in `VocalPart.cpp`**; they need their
   enclosing functions identified/pinned before the change can be adjudicated in either
   direction.
5. **`GetBestHit` (528 B) is a scheduling wall** around a byte-exact callee. Do not brief it as
   a callee-clobber candidate again.
