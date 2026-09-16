# W16-ES — `TrainerGemTab::DrawTails`: MSVC contracts `a*b+c` only when the product has ONE use

**Date:** 2026-09-16 · **Branch:** `w16-es` · **Base:** `5b59364a`
**Measured:** `Δmatched=+1 · Δcode_bytes=+0 · Δcode%=+0.000000pp · Δhonest=+1`, 0 regressions.
**Claim: +1 `matched_function`. NOT +888 B.** Both halves are stated explicitly in §6.

## 1. The row, and what W16-EN handed over

`?DrawTails@TrainerGemTab@@QAAXABVGameGem@@HHMM@Z`, 888 B, unit
`default/band3/game/TrainerGemTab`. Baseline read from `report.json` (not a displayed
percentage): **fuzzy 98.58108 / mpn 99.27928, 19 charged sites**, our body **884 B — 4 B short**.

W16-EN analysed it and deliberately did not touch it, because `matched_code` is
all-or-nothing per row and a partial close buys zero bytes. Its §6 named three clusters.
**All three reproduced exactly.** EN's analysis was correct in every particular; what
follows is what happens when you try to *act* on it.

| cluster | idx | shape |
|---|---|---|
| **A** | 40–61 (14) | FPR numbering rotation + the two subtractions in swapped order |
| **B** | 127–128 (2) | FMA contraction: target `fmuls`+`fadds`, ours `fmadds` — the whole 4 B deficit |
| **C** | 176/178/180 (3) | `fmuls` commutative operand slot: target `f31,value`, ours `value,f31` |

## 2. Cluster B — the mechanism, measured on the real compiler

The in-tree record already says the obvious lever is dead:
`#pragma fp_contract(off)` is **INERT on this toolchain** (lane AE2, `aba678b3`), and
`volatile` is both disallowed and wrong (it forces a stack round-trip retail does not have).
`XBOX360_FLOATING_POINT_CODEGEN.md` prescribes "give the multiply a named home" — but our
source **already** had `float scaleX10 = 10.0f * scale;` and contracted anyway. So a plain
named local is not the barrier the doc implies.

Rather than guess, I compiled ten variants with the **real** `X360/16.00.10224.00` cl.exe at
the project cflags (`/O1 /Oi /GR /EHsc`), in a context mirroring the real function (`scale`
reused later, `endZ` consumed by a compare):

| variant | spelling | result |
|---|---|---|
| v1 | `s10 = 10.0f*scale; endZ = vz + s10;` (ours) | `fmadds` |
| v2 | `endZ = vz + 10.0f*scale;` | `fmadds` |
| v3 | `s10 = scale*10.0f;` | `fmadds` |
| v5 | `endZ = vz; endZ += s10;` | `fmadds` |
| v6 | `endZ = s10 + vz;` | `fmadds` |
| **v4** | **product used a second time** | **`fmuls` + `fadds`** |
| v7 | product homed in a struct member, read back | `fmuls`+`fadds`, **plus a store** |
| **v8/v9/v10** | **the sum re-spelled so the product has 2 syntactic uses** | **`fmuls` + one `fadds`** |

⇒ **The rule is single-use: MSVC X360 fuses `a*b+c` only when the product has exactly one
consumer.** Five one-use spellings all contract; every two-use spelling emits the separate
pair. v7 (the doc's "named home" lever) does break contraction, but costs a store retail
does not have — so it is the wrong instrument here even though it moves the right needle.

**Why retail looks single-use anyway.** Retail's `f11` is *dead* immediately after the add
(reloaded at idx 133), so retail has one *materialised* use. v8/v9/v10 resolve the apparent
contradiction: two uses at the *fuse decision*, then a later CSE merges the two identical
adds back into one. Final code is `fmuls` + **one** `fadds` with the product dead — retail's
shape exactly, at no instruction cost.

**The fix** (one line):

```cpp
 float endZ = xfm.v.z + scaleX10;
-float overhang = 0.1f * (endZ - unk12c);
+float overhang = 0.1f * ((xfm.v.z + scaleX10) - unk12c);
```

Semantically identical (CSE restores the shared sum), no pragma, no `volatile`.
Result: idx 127/128 close, body **884 → 888 B**, fuzzy **98.58108 → 99.3018**,
**mpn 99.27928 → 100.0**.

★ **The oracle is not the authority here.** rb3-Wii spells `overhang` with one use of the
product. rb3-Wii's source is a *reconstruction written to match MWCC*, not the original
text — per the standing rule that retail bytes outrank the oracle. Retail's bytes say the
product had ≥2 uses when MSVC decided.

## 3. Three levers tried on clusters A and C — all refuted, with the measurement

Each was built and read individually so attribution is clean. Each rebuild was a full
`./tools/ninja-locked` (never `ninja <one>.obj`, which skips the six patchers).

| # | lever | predicted | **measured** |
|---|---|---|---|
| 1 | **cluster C**: flip source to `overhang * value` | 3 sites close | **INERT — byte-identical output** |
| 2 | **cluster A**: hoist `yRange`/`tickRange` to pre-loop locals (the oracle's shape) | inert (~65%) | ⛔ **HARMFUL: 17 → 69 sites, fuzzy 99.30 → 86.15** |
| 3 | **cluster A**: declare `tickRange` *inside* the guarded block | subtraction order flips | **INERT — byte-identical output** |

**(1) Commutative operand order is compiler-canonicalised, not source-order.** The null was
verified non-vacuous: the build log shows `MSVC …/TrainerGemTab.obj` recompiled and all six
patchers ran, and the edit was present in the file. Corroboration: `Vector3::operator*=(float)`
and the free `Scale(const Vector3&, float, Vector3&)` in `math/Vec.h` are **both** value-first
(`x *= f`, `v1.x * f`), so no Milo idiom produces scalar-first either.
⇒ objdiff's `COMMUTATIVE_OP_ORDER` → **"LikelyFixable"** label is **wrong for this row**. It is
the detector restating its own input — the same disease CLAUDE.md records for the
`LINKER_MERGED`/`AT_LIMIT` and `REGISTER_SWAP` labels.

**(2) The oracle's own shape is refuted by retail bytes.** Named pre-loop locals let MSVC
perform loop-invariant code motion: `yRange`/`tickRange` are computed *before* the loop into
callee-saved FPRs, growing the save set to `__savegprlr_24`/`__savefpr_21` against retail's
`_22`/`_23`. Retail recomputes both **inside** the loop every iteration, so retail's source
**inlines them** — exactly as our source already did. A comment now records this at the site
so the next lane does not re-hunt it.
⚠ My own framing of this experiment was sloppy and the transcript should say so: putting the
locals *before* the loop does not test evaluation order at all — it volunteers them for
pre-loop placement. Test 3 is the experiment I should have run first.

**(3) The subtraction order is scheduler-chosen, not source-chosen.** Re-reading the listing
showed cluster A is not merely renaming — the two subtractions are genuinely swapped:

| idx | target | ours |
|---|---|---|
| 58 | `endTick − startTick` (tickRange) | `tick − startTick` (numerator) |
| 59 | `tick − startTick` | `endTick − startTick` |

Our source spells numerator-then-denominator and emits that order. Naming `tickRange` first,
inside the guard, gives denominator-first **source** order. LICM correctly did **not** fire
(the value stayed in the loop, so the guarded-scope reasoning was right) — and the emitted
order **did not move**. Output byte-identical.

## 4. Why clusters A and C are where they are

After the cluster-B fix, **all 17 remaining charges are `diff_arg` register-slot differences**
— 203 of 222 instructions equal, same opcodes, same schedule apart from the one swapped
subtraction pair. This is the class CLAUDE.md describes as following **liveness and
scheduling**, for which declaration/source reordering is measured inert (12+ byte-identical
hand variants across 4 functions, two zero-gain beam sweeps). The permuter is **OFF** by
standing directive and was neither invoked nor proposed.

That `mpn` reached exactly **100.0** is itself the structural confirmation: `mpn` excludes
arg-only penalties, so a row whose every remaining charge is a register slot *must* read 100 there.
The contraction was the only instruction-level defect in the row, and it is fixed.

## 5. Measurement

`tools/ab_measure.py --worktree /home/free/tmp/wt-w16-es --from-dirty`, both legs settled to
zero work, leg B `msvc=1 patch=6`, ruler `name_check` (graded, from `objdiff.json` options):

```
leg A: matched=43959 masked=23224 honest=20735 code%=40.267284
leg B: matched=43960 masked=23224 honest=20736 code%=40.267284
Δmatched=+1  Δmasked_equal=+0  Δhonest=+1  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000061pp   (50.010845 -> 50.010906)
unit improvements: +1  default/band3/game/TrainerGemTab  (11->12)
unit net (ALL units) = +1  vs whole-binary Δmatched = +1
units at 100% [mpn]: 189 -> 189 (0 fell off); [all-rows-fuzzy]: 169 -> 169 (0 fell off)
```

**Pre-registered prediction was Δmatched +1 / Δcode_bytes +0 / Δcode% 0.000000pp /
Δhonest +1 / unit 11→12. Hit exactly on every field.**

⚠ The `none` control is flat, and `ab_measure` itself labels it **NOT_APPLICABLE**: with
`source` in the patch, default-UP/none-FLAT is also the wrong-callee-fix signature. Do not
read it as adjudicating anything here.

## 6. What is claimed, and what is NOT

* **Claimed: +1 `matched_function`** (mpn 99.27928 → 100.0), measured whole-binary.
* **NOT claimed: the 888 B.** `matched_code` keys on `fuzzy == 100`; 17 charged sites remain,
  so the row contributes **zero bytes**, and Δcode% is **0.000000pp** — as predicted.
* The row also stops being **4 B short**, which is a correctness-of-match improvement
  independent of either headline number.

## 7. What I did NOT do

* **Did not invoke or propose the permuter** — OFF by standing directive, though clusters A
  and C are exactly its class.
* **Did not touch `scale`'s spelling.** `(float)ticks * (2.5f/480.0f)` is deliberate and
  verified (one pooled `0x3baaaaab` at `lbl_820F38D0`, idx 126 `fmuls f0, f0, f27`). The
  oracle's `2.5f * (ticks/480.0f)` would emit two constants. Confirmed still correct; left alone.
* **Did not touch `ExtraTail::operator=`** or the alias override W16-EN installed in
  `src/band3/game/TrainerGemTab.h` / `scripts/alias_withdrawal_overrides.json`. Nothing here
  depends on or disturbs them.
* **Did not land the two inert edits** (cluster C flip, `tickRange` local). Both measured
  byte-identical; landing them would be noise, and the cluster-C comment I had drafted
  asserted something I then measured to be **false**.
* **Did not touch the other 10 sub-100 rows** in the unit (`fn_826F0170` 776 B, `fn_826EECE0`
  96 B, `?Render@` 88 B, six 8-byte stubs) — all at fuzzy 0, an identification/body problem,
  unchanged from EN's assessment.
* **Did not claim the row is unfixable.** Clusters A and C are unreachable *by source
  spelling*, on three measured levers. That is a narrower statement, and §3's table is there
  so the next lane can attack the scheduler question rather than re-run my dead ends.

## 8. Durable lessons

1. **MSVC X360 contracts `a*b+c` iff the product has a single use.** This is a *source-shape
   rule*, deterministic, and it is the working lever now that `#pragma fp_contract(off)` is
   known inert. A second syntactic use that CSE later merges costs no instruction.
2. **A "named home" is not sufficient to break contraction** — our one-use local proved it.
   The doc's `NgFur::Shell` lever works because a *member* adds a store, which is usually
   the wrong shape.
3. **`COMMUTATIVE_OP_ORDER` / "LikelyFixable" is not a diagnosis.** Measured inert here.
4. **The oracle can be refuted by retail bytes on source *shape*, not just content** — the
   pre-loop hoist is a −52-site regression, and rb3-Wii spells it that way.
5. **State the experiment you are actually running.** Lever 2 and lever 3 look like the same
   hypothesis and are not; running 2 first cost a build and produced a scary-looking
   regression that said nothing about evaluation order.
