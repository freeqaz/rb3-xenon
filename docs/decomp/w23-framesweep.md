# W23-FRAMESWEEP — the frame-shortfall signature, swept whole-binary, ranked and collectability-tested

**2026-08-17.** Baseline verified in-worktree before any edit, reproducing the
brief on five of six figures exactly: `matched_functions` **44,503** ·
`matched_code` **3,756,568 B** · **36.398514%** · honest **21,593** ·
`total_code` **10,320,664** · `total_functions` **69,226**. Ruler `name_check`,
read from `report.json`'s own `provenance.diff_config` (22 keys), not assumed.
The brief's `36.398511%` differs in the 6th decimal — far inside the documented
`name_check` aggregate instability (~0.05 pp) and not chased.

W22 closed `?Handle@VocalPlayer@@` for +5,296 B off a signature nobody had swept
for. This lane makes that signature **mechanical**, sweeps the whole binary with
it, and — the part that decides where the next lane should go — **prices which of
the hits can actually be collected.**

**Headline: the shape is common (317 rows), and mostly NOT collectable.** Of the
head of the queue, 41 rows / 50,076 B are name-blocked against 19 rows /
21,636 B collectable. And the collectable half splits again by *direction*, only
one of which has a known lever.

**Shipped: +1 matched function / +1 honest / +0 B**, measured by
`ab_measure --revert` of the one functional commit, both legs settled. Tree with
the fix: **44,504 matched · 3,756,568 B · 36.398514%**. The byte total is
deliberately unmoved — see §5, where a frame fix that closes 7 of 9 charged sites
is shown to buy a function and no bytes, and where the reason my own prediction
missed is the most reusable thing in this document.

---

## 1. The detector — `tools/w23_frame_scan.py`

Decodes the PowerPC prologue straight out of the COFF bodies of both the
dtk-split target obj and our compiled obj, so it covers **all 1,048 paired units
in 26 s** rather than 69k objdiff invocations.

    frame:   stwu r1, -N(r1)     word>>26 == 37, rS == rA == 1
    funclet: addi rD, r12, -N    word>>26 == 14, rA == 12
             (the disassembler spells this `subi rD,r12,N`)

**Result: 317 frame-differing rows out of 17,122 compared (1.85%).** Selective,
not a detector that fires on everything.

### It fires AND clears, on disjoint known answers

A detector that only ever fires proves nothing, so both directions were required
before any verdict was trusted:

| fixture | expected | measured |
|---|---|---|
| `?Poll@VocalPlayer@@` (still short 0x10) | FIRE | 0x1d0 vs 0x1c0, **3,388 B + 4 funclets × 40 B** — W22's exact figures |
| `?Handle@VocalPlayer@@` (fixed by W22, in this tree) | CLEAR | **absent**; 68 of 70 frames in the unit read equal |

### ⛔ Two funclet-pairing spellings REFUTED — both read a SILENT ZERO

Recorded in the tool's docstring because a zero here is indistinguishable from
"found nothing", which is the failure mode that closes veins:

1. **Mask the parent-frame immediate, compare the remaining bytes.** Every
   funclet carries at least one relocation (the `bl` to a dtor) whose word
   differs between sides by construction — different displacement, and often a
   different symbol entirely (`fn_82B69618` against our mangled spelling).
   Matched nothing.
2. **objdiff's own `funclet_signature`** (mask the immediate *and* every
   relocated word). Right in principle, still zero — because it was driven
   through `coff_bodies_ext.function_bodies_ext`, whose `is_aux_code_symbol()`
   drops `__unwind$*`, **which on our side IS the cleanup funclet**. Target
   funclets are anonymous `fn_826E95D0`; ours are `__unwind$344432`; they never
   share a name.

★ **What works is the per-unit MULTISET of displacements**, exactly as
`guard_funclet_census.py` does for guard bits. The displacement immediate *is*
the parent's frame size, so the multiset is self-keying and needs no pairing at
all. Surplus on both sides — target has N at `0x1d0` we lack, we have N at
`0x1c0` it lacks — is the corroboration.

⚠ The funclets are **not independent rows**; they are one defect observed N more
times. They are priced into the prize because they cross for free with the
frame, which W22 measured (9 × 40 B).

## 2. Collectability — `tools/w23_collectable.py`, run BEFORE any porting

`matched_code` keys on `fuzzy == 100`, all-or-nothing per row, so a perfect
body-port pays **zero** if what remains is a relocation-name charge against an
ICF fold-survivor where our source is already correct. Uses W19's rule, not the
naive one: only a **bare** `arg:{Symbol}` is a real name charge;
`arg:{Register,Symbol}` is charged by the register.

**Known-answer validation:** `?Poll@VocalPlayer@@` reads **exactly 10 name
charges**, reproducing W22's independently measured figure.

Top 60 rows at fuzzy ≥ 80:

| verdict | rows | bytes |
|---|---:|---:|
| COLLECTABLE (source work alone can cross) | 19 | **21,636 B** |
| NAME-BLOCKED (needs a proven alias or map fix) | 41 | **50,076 B** |

⇒ **~70% of the head of this queue is not collectable by source work.** That is
the single most useful number here and it is why porting was not started off the
raw ranking.

### The cross-instrument control, and proof it can fail

`--verify-frame` checks the detector (raw COFF prologue decode) against a
*different* instrument (objdiff's graded diff) by requiring a charged site whose
two immediates differ by exactly the detected delta. **60/60 of the shortlist
passed.** That would be worthless if the check could not fail, so it was tested:

| population | real delta seen | bogus delta+4 seen |
|---|---|---|
| 12 rows at fuzzy < 50 | **7 / 12** | **0 / 12** |

So `NO-FRAME-SITE` genuinely fires (5 of 12 badly-diverged rows), and the check
does not match by accident.

## 3. ★ The delta is BIDIRECTIONAL, and the two directions are not equally tractable

This is the finding that should shape the next lane, and it was not in the brief.

| delta | rows |
|---|---:|
| retail frame **bigger** (W22's direction) | **94** |
| **ours** bigger | **88** |
| other magnitudes | 135 |

±0x10 alone is 182 of 317 (57%) — W22's exact delta.

* **Retail-bigger** = we lack a local retail has. **W22 proved this fixable** by
  restoring a real body so a temp survives.
* **Ours-bigger** = MSVC merged a slot we did not. **No source lever was found —
  see §4.**

Collectable rows split almost evenly by bytes (retail-bigger 9 rows / 10,940 B;
ours-bigger 10 rows / 10,696 B), so a lane that ignores the distinction will
spend half its budget on the direction with no lever.

## 4. ⛔ REFUTED: lexical scope and named-vs-temporary are INERT on stack-slot merging

`?Load@SampleData@@` is the cleanest row the sweep found: **440 B, 105/110
instructions equal, zero hard diffs, zero name charges**, and all 5 charged sites
are one stack slot. Retail overlays both of the function's `FilePath` temps onto
slot `0x60`; we give the second a private slot at `0x70`, and that one slot *is*
the 0x10 frame surplus.

Hypothesis: retail's are **unnamed temporaries** (`MILO_LOG("%s", fp)` building
an argument) and MSVC overlays unnamed temporaries in disjoint full-expressions
where it will not overlay two *named* locals in sibling scopes. It rhymed with
W22, where the cause turned out to be escape / memory-effect analysis rather
than inlining.

**Refuted twice, and the refutation is stronger than a null:**

| spelling | result |
|---|---|
| `(void)FilePath(fp);` in both branches | **byte-identical** |
| the above **plus a brace-less `else`**, matching rb3-Wii's own shape | **byte-identical** |

Same 5 `diff_arg` sites, same `0x70` slot, same `0xa0` frame in all three. *Byte*-identical
(not merely "no better") means MSVC canonicalizes them — this is not a near-miss
to tune. Companion to `MSVC_X360_REGALLOC.md`'s corrected claim that declaration
order is inert for register-only swaps. Recorded in-source at
`src/system/synth/SampleData.cpp` so nobody retries it.

⚠ Vacuity check: the obj was confirmed 52 s newer than the source, so the null is
a real compile, not a stale artifact.

## 5. The port: `?GeoInit@@` — hoisting to fix evaluation order costs a stack slot

The cleanest **retail-bigger** row: 400 B, zero hard diffs, retail `0xa0` vs our
`0x90`.

**Cause, read off retail bytes.** Retail gives each of the five `bsp_*` `Symbol`
temps its own 4-byte slot (`0x54/0x58/0x5c/0x60/0x64`, reusing `0x64` for the
`set_bsp_params` Symbol). We had hoisted two of the `FindArray` calls into their
own statements, so those temps died at the semicolon and MSVC recycled the slot —
**five slots collapsed into three, which is exactly the 0x10.**

The hoisting had been added deliberately to control **construction order**, and
it was never needed: MSVC evaluates call arguments **right-to-left**, so the
single expression already yields retail's order, verified against retail bytes —
`math, bsp_check_scale, bsp_max_candidates, bsp_max_depth, bsp_dir_tol,
bsp_pos_tol, set_bsp_params`.

⇒ **Generalises: hoisting a subexpression to fix evaluation order can silently
cost a stack slot, and the two effects are separable.** The old spelling bought
the order and paid in slots; inlining buys both.

**Measured: 9 charged sites → 2.** Frame now `0xa0`, all five slots present,
every frame/slot immediate closed.

### ★ Prediction missed — and the miss is the useful half

I pre-registered that the two `r30`↔`r31` charges at [87]/[89] would **dissolve
with the frame**, on the strength of the 13 instances CLAUDE.md records of
exactly that (and W22's own 212-site dissolution). **They did not.** Evaluation
order and store order both already match retail; what remains is purely which
callee-saved register holds `max_candidates` vs `max_depth` across the
intervening calls.

⇒ **"a REGISTER_SWAP dissolves with the frame" is a real pattern but NOT a law.**
Here the frame closed *completely* and the swap was untouched. Left as diagnosed;
permuter is OFF by standing directive.

**This row pays 0 bytes** — `matched_code` needs `fuzzy == 100` and the two
register charges hold it at 99.900. It was landed anyway under the standing
accuracy-over-headline directive: the source was modelling retail wrongly, and
the lever is now documented in-source.

⚠ RESIDUAL-1 trap in a fresh instance: **closing 7 of 9 charged sites bought no
bytes.** Price from the charged-site list, never from a mismatch count.

### Predicted vs measured (whole-binary A/B, `--revert` of the one functional commit, both legs settled)

Leg A = fix present, leg B = reverted, so the fix's value is the negation of the
reported Δ.

| | predicted | measured |
|---|---|---|
| Δ`matched_code` | 0 B | **+0 B** ✓ |
| Δ`matched_functions` | 0 | **+1** ✗ |
| Δhonest | 0 | **+1** ✗ |
| Δcode% | 0 | **+0.000000 pp** ✓ |
| `none` control | 0 | **+0 B** ✓ |

`44,503 → 44,504 matched`, unit `default/Geo` 31 → 32, all other units unmoved
(unit net −1 == whole-binary Δ −1 on the revert leg).

### ★ The missed prediction corrects a model I had built EARLIER IN THIS LANE

I predicted Δfunctions 0 because I had concluded, from
`?SyncObjects@BandCrowdMeter@@` reading `mpn` 100.0 with 13 immediate and 10 name
charges, that **`mpn 100 ⟺ hard == 0`** — i.e. that `mpn` is blind to every
`diff_arg` class. GeoInit refutes it: `hard == 0` on *both* sides of the change,
yet `mpn` moved **99.930 → 100.000**.

Attributed to the row, not assumed: the archived leg reports show `?GeoInit@@`
alone changed in the unit. What moved was the **immediate** class — the 7
frame/slot immediates — while the 2 register charges stayed and cost nothing.
That matches MPNGAP-1's rule (`arg_diff_score`, which `mpn` excludes, counts only
**non-immediate** arg diffs) and means my SyncObjects inference was wrong about
which bucket its charges were in, not about the rule.

⇒ **Consequence, and it re-prices this whole queue's function yield:** for the
retail-bigger frame class, **closing the frame immediates pays +1 matched
function per row even when the bytes stay locked behind residual register
charges.** A frame fix is therefore never worth zero, which is the opposite of
what I briefed myself before measuring. Rows in the queue at `hard == 0` with
immediate charges — e.g. `?UtilDrawPlane@@` (49) and `?Load@SampleData@@` (5) —
are each a probable +1 function on the same mechanism.

## 6. The queue — `docs/decomp/w23-frame-queue.tsv`

All 317 rows, ranked by **SIZE-IF-IT-CROSSES** (target body extent from the ASM
EXTENT + 40 B per corroborating funclet), never by penalty or fuzzy%, with
direction and collectability verdict per row. The 60 head rows carry a full
charged-site profile.

Best next targets, both **retail-bigger and COLLECTABLE**:

| prize | fuzzy | hard | diagnosis |
|---:|---:|---:|---|
| **972 B** `?ReadMetaEvent@MidiReader@@` | 99.44 | 2 | retail keeps a slot at `0x80` for `buf[0x100]` we merge onto `0x70`; plus 1 SWAPPED pair (`ts_b`↔`ts_t`) and 4 PERMUTED. Declaration-order work — the documented lever for SWAPPED/SHIFTED. |
| **1,124 B** `?DrawShowing@RndFlare@@` | 98.53 | 3 | not opened |
| **1,688 B** `?Enter@BandDirector@@` | 98.10 | 8 | not opened |
| **1,040 B** `?AddLoaderRef@NetCacheMgr@@` | 96.44 | 6 | **8 corroborating funclets** — the largest funclet bonus in the collectable set |
| **3,320 B** `?DrawToTexture@RndTexRenderer@@` | 94.18 | 43 | largest collectable row; delta +0x60, 113 register charges — a real body job |

## 7. Deliberately NOT done

* **`?ReadMetaEvent@MidiReader@@` not attempted.** It needs several declaration
  reorders *plus* an extra local, each iteration a build, with the prize gated on
  all of them landing together. Recorded with its diagnosis instead of
  half-finished — a bounded look at the source was taken and stopped there.
* **No alias added, no map edit.** The 41 NAME-BLOCKED rows / 50,076 B are the
  large share, and an unproven alias lifts the score *by construction* while the
  `none` control cannot catch a fabrication. Proving those folds on retail bytes
  is a separate lane, and it is now **sized**.
* **Permuter not run** (standing directive), so `GeoInit`'s residual two register
  charges and the ours-bigger direction generally are left as diagnosed, not as
  attempted.
* **The 257 rows below fuzzy 80 were not charge-profiled** — only ranked. On the
  low-fuzzy population the frame is usually a *symptom* of a wholly different
  body, which the fuzzy<50 control shows directly (5 of 12 have no frame site at
  all).
* **`?DataInitFuncs@@` (8,068 B, the largest row in the sweep) not triaged** —
  fuzzy 70.5 with a 0x90 delta; it is a body rewrite, not a frame fix.
