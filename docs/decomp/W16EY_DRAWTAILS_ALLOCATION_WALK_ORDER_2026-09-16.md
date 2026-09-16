# W16-EY — `TrainerGemTab::DrawTails`: are the 17 register-slot charges reachable by source spelling?

**Date:** 2026-09-16 · **Branch:** `w16-ey` · **Base:** `2d9c4ead`
**Row:** `?DrawTails@TrainerGemTab@@QAAXABVGameGem@@HHMM@Z`, 888 B, unit `default/band3/game/TrainerGemTab`.
**Entry state (read from `report.json` in the worktree, ruler `name_check`):** fuzzy **99.3018** / mpn **100.0**.
**Prize:** **+888 B and +0 functions.** `mpn == 100` means the row is already in `matched_functions`;
`matched_code` keys on `fuzzy == 100` and withholds every byte. ⇒ **Δfunctions = 0 is the PREDICTED
result of a successful fix**, not a failure. Do not revert a correct fix on seeing a zero there.

## 1. Pre-registration (written and committed BEFORE the first probe build)

Whole-binary leg A, settled (zero-work `./tools/ninja-locked`, only the three CHECK edges ran):
`43,978 fns / 4,131,656 B / 40.320374 % / fuzzy 50.014150 / total_code 10,247,068` — reproduces the
brief's figures exactly.

Charged sites (graded ruler, `run_diff_inspect mismatches`, 17 of 222, all `diff_arg` register slots):

| cluster | idx | shape |
|---|---|---|
| A | 40–61 (14) | 4-rotation `f0→f13→f12→f11→f0` over {A=tick, B=startTick, C=endTick, yRange}, plus idx 58/59 subtractions in swapped order |
| C | 176/178/180 (3) | `fmuls` operand slot: retail `f31,value`, ours `value,f31` — ONLY for the `overhang` (f31, callee-saved) scalar; the `drawScale`/`scale` (f0) sites at 137–141 / 201–204 match value-first |

### Working model
Volatile FPRs are handed out from the pool `f0, f13, f12, f11` in the order the allocator *walks*
the temporaries. Retail: `{C:f0, A:f13, B:f12, yRange:f11}` ⇒ walk `C, A, B, yRange`.
Ours: `{yRange:f0, C:f13, A:f12, B:f11}` ⇒ walk `yRange, C, A, B`. The ONLY difference is where
`yRange` sits relative to the three int→float conversions. Our order is what right-to-left tuple
creation of `quotient * yRange + fStartY` predicts (right operand first). Retail's is what you get if
`yRange`'s tuple is created AFTER the quotient subtree.

### Experiments (one source edit, one full `./tools/ninja-locked`, read via `run_diff_inspect mismatches`)

| # | edit | prediction under the model | falsifier |
|---|---|---|---|
| A1 | `(endY - fStartY) * (num/den) + fStartY` | cluster A collapses 14→0 incl. idx 58/59 | byte-identical ⇒ operand order is canonicalised at tuple creation for this shape too |
| A2 | named quotient local inside the loop, `t * (endY - fStartY) + fStartY` | cluster A collapses | byte-identical ⇒ walk order ≠ statement order; REGRESSION (bigger save set) ⇒ LICM hoisted `t` (ES lever-2 shape) |
| A3 | `fStartY + (num/den) * (endY - fStartY)` | INERT (control: flat sum canonicalised) | movement ⇒ the add's operand order is live, model incomplete |
| C1 | `extra.mXfm.m.y *= overhang` (`Vector3::operator*=`, inlined) | inert | movement ⇒ the inlined param copy re-keys the canonical order |
| C3 | second syntactic spelling of `overhang` at the ExtraTail site, CSE'd back to f31 (ES's FMA trick applied to tuple order) | cluster C collapses 3→0 IF canonical order keys on tuple id | inert ⇒ canonical order keys on something else (physreg class?) |
| — | cluster C may move as a SIDE EFFECT of any cluster-A fix (same allocation-order mechanism) | — | — |

Success criterion for landing: charged sites 17 → 0 with NO new site anywhere in the unit, then
`ab_measure --from-dirty` reading **Δmatched = 0 / Δcode_bytes = +888 / Δcode% = +0.008666 pp
(888 / 10,247,068)**, unit `TrainerGemTab` functions 12→12, `matched_code` 4,748 → 5,636.
Anything that closes fewer than all 17 buys **zero bytes** and is NOT landed (all-or-nothing per row);
byte-identical results are recorded as negatives and NOT landed.

## 2. Results
(filled in as measured)
