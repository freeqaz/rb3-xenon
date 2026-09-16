# W16-EZ pre-registration — VocalPart residual, six rows

Committed BEFORE any measurement. Ruler: shipped graded `name_check`, read from
`build/45410914/report.json` `provenance.diff_config` (tool_commit a5f0ea903ec1,
objdiff 4.2.9) — not assumed.

Baseline read in THIS worktree at `2d9c4ead`, settled to zero compile work:

```
matched_functions=43978  masked_equal=23224  matched_code=4131656
matched_code_percent=40.320374  fuzzy_match_percent=50.014150
total_functions=69240    total_code=10247068
```

## Corrections to the brief, established before any edit

1. **The brief missed a sub-100 row.** `?CalcNoteWeights@VocalPart@@QAAXXZ`,
   292 B, fuzzy 99.93150, **mpn 99.93150** — so it is worth **+1 function AND
   +292 B**, not bytes-only. The brief lists five rows; there are six.
2. **"70 named rows, 31 at fuzzy 100" mislabels the denominator.** 70 is the
   TOTAL row count (36 named + 34 anonymous); named rows at fuzzy 100 = 30,
   all rows at fuzzy 100 = 31. The integers are right, the noun is wrong.
3. **"Nobody has opened either" of the bytes-only pair is FALSE for
   `HandlePhraseEnd`.** `src/band3/game/VocalPart.cpp` carries an in-source NOTE
   at the charged line, written by lane AG2 in `0ac748fc` on **2026-07-26**,
   recording that both operand orders and int/float temps were already tried.

## E1 — `HandlePhraseEnd` (1120 B): flip the `mullw` operand order

Sole charge: idx 132, `diff_arg`, target `mullw r10, r29, r3` vs our
`mullw r10, r3, r29`. r3 is the live return of the `bctrl` at idx 131
(`GetIndividualMultiplier()`); r29 is `total`. The other three `mullw` sites
(150/157/165) and both `mulli` sites AGREE, so exactly one expression is at
fault and a source flip moves exactly one site.

Our source already reads `total * indMult` — retail's textual order — and emits
the REVERSE. So MSVC reverses this shape, and `indMult * total` should produce
retail's bytes. The rb3-Wii oracle independently says `(float)(indMult * total)`.

**PREDICTION: fuzzy 99.96429 -> 100.0; whole binary +1120 B, +0 functions.**
A zero function delta is the CORRECT result here (mpn is already 100.0).
Confidence **MEDIUM**.

Named ways this fails:
- (a) AG2's claim still holds on the current body: instruction unchanged, Δ0.
- (b) The flip perturbs allocation elsewhere in this 280-instruction function and
  REGRESSES currently-equal instructions ⇒ net negative.
- (c) It fixes idx 132 but moves a different row in the TU (watch unit net).

Why I believe it is worth testing despite the in-source note: the note is 7 weeks
old, predates the 2026-08-12 `name_check` ruler flip, and predates changes to
this very function body (it now uses ET's `mPhrases.end()` idiom). Scheduling and
allocation are global, so a canonicalisation claim measured on a different body is
not binding on this one.

## E2 — `CalcNoteWeights` (292 B): the missing ICF alias membership

Sole charge: idx 18, `diff_arg` on a `bl`. Target names
`?reserve@?$vector@PAUDep@CharPollableSorter@@...`, we name
`?reserve@?$vector@M...` (vector<float>::reserve). The target spelling is the
SURVIVOR of the existing group at `0x823715e0` in `scripts/symbol_aliases.json`;
our spelling is not among its 2 folded members. Positive control in the same row:
`erase` shows the same target/base split and is NOT charged, because BOTH its
spellings are already in the group at `0x824b06c8`.

**PREDICTION: installing the membership moves fuzzy 99.93150 -> 100.0,
+292 B, +1 function.** Confidence that the SCORE moves: HIGH — mechanically
certain, objdiff consults SymbolEquivalences and drops the charge.

⛔ **That is exactly why it must not be landed on the score.** An unproven alias
lifts `name_check` BY CONSTRUCTION, and a `none` control is flat for a fabricated
alias by construction too, so flatness there is the hazard's signature, not a
clearance. **I will land this ONLY if the fold is proven on retail bytes**
(`tools/icf_pair_adjudicate.py`). If it is not provable, the correct outcome is
NO LANDING and a recorded negative. Confidence that it is PROVABLE: **UNKNOWN** —
that is the actual experiment.

## E3 — `SetDifficultyVariables` (768 B): pre-registered NEGATIVE, no attempt

2 charges, idx 45 and 84, both `add r11,r11,r31` (target) vs `add r11,r31,r11`
(ours). The detector calls this COMMUTATIVE_OP_ORDER / REGISTER_SWAP.

I have already established, before editing anything, that **no source-level
operand flip can fix this**. There are 8 such sites, all from the identical
construct `voxCfg->FindArray(X)->Float(diff + 1)`, and the four preceding
instructions are byte-identical at every site on BOTH sides:

| idx | 30 | 45 | 69 | 84 | 99 | 114 | 143 | 172 |
|---|---|---|---|---|---|---|---|---|
| target | A | A | B | A | A | A | B | A |
| ours   | A | **B** | B | **B** | A | A | B | A |

(A = `r11,r31`; B = `r31,r11`.) Both sides emit BOTH orders from one construct, so
the order is a per-site compiler tiebreak, not a source property.

**PREDICTION: any source-level operand reorder of the node-index expression moves
all 8 sites together — fixing 2 and breaking 6, net −4 charges and a LOWER fuzzy.**
Confidence **HIGH**. I will therefore not attempt it, and I record the structural
reason so the next lane does not re-hunt it. This is permuter-class; the permuter
is OFF by standing directive.

## E4 — `IsEmptyPhrase` (116 B): reproduce the zero-extend

Sole charge: a `delete` at idx 14 — retail emits `clrrwi r10, r10, 0` that we do
not (base 112 B vs target 116 B). On PPC64 `clrrwi rA,rS,0` = `rlwinm rA,rS,0,0,31`
is NOT a no-op move: it zeroes the upper 32 bits, i.e. it is MSVC's
zero-extend-32-to-64 idiom. ET already measured a separate `int count` local
INERT, which is consistent — a plain `int` copy needs no extension.

**PREDICTION: forcing an unsigned/width conversion of `unk10` on the path to the
decrement reproduces the instruction; fuzzy 96.55173 -> 100.0, +116 B, +1 fn.**
Confidence **LOW-MEDIUM**. `unk10`/`unk14` are both plain `int` in
`src/system/beatmatch/VocalNote.h`, so the conversion must come from the
expression, not the member types. Named failure mode: MSVC folds the conversion
away because `lwz` already zero-extends on PPC64, making the extension provably
redundant and unreachable from source.

## E5 — `GetNoteSliceWeight` (484 B): not planned, reason recorded

Retail's prologue saves `r30` AND `r31` with `__savefpr_21` (f21..f31, 11 FPRs);
ours saves only `r31` with `__savefpr_20` (f20..f31, 12 FPRs). The whole residual
is a shifted FPR numbering (target f27/f28/f21/f22 where we use f28/f27/f20/f21).
One extra FPR live in our loop. ET already refuted the two obvious causes (local
copy of the global; un-hoisting the `2.0f` literal — MSVC re-hoists). This is an
allocation-pressure residual with no identified source lever, and I will only open
it if E1/E2/E4 finish early. Saying so rather than leaving silence.

## E6 — `GetBestHit` (528 B): NOT attempted

ET established it is 4 instructions of pure scheduling around a BYTE-EXACT callee
(`ScoreNote`, fuzzy 100/mpn 100), with `target_size == base_size == 528`, and that
statement reordering is normalised away by MSVC. A byte-identical callee has a
byte-identical clobber set by construction. I have no non-clobber lever that is
different from what ET already refuted, so I am not opening it. Permuter is OFF.

## Protocol

Each kept change is A/B'd separately with
`tools/ab_measure.py --worktree /home/free/tmp/wt-w16-ez --from-dirty`.
Per-function iteration uses a full `./tools/ninja-locked` with `report.json` +
`report.cache` wiped before each read — never a targeted `.obj` build, which skips
the six obj patchers that are part of the ruler.

---

# OUTCOME — prediction vs measured (appended after measurement)

| exp | prediction | measured | verdict |
|---|---|---|---|
| E1 `HandlePhraseEnd` | fuzzy -> 100, +1120 B, +0 fns (MEDIUM) | unchanged 99.96429, whole binary byte-identical | ❌ MISS — failure mode (a) as named: AG2's claim replicates. TU recompiled (edge 5/14), so non-vacuous |
| E2 `CalcNoteWeights` | +292 B, +1 fn, conditional on retail-byte proof | **+1004 B, +3 fns** — proof obtained on all 3 channels | ⚠ MISS IN MY FAVOUR — 2 extra rows crossed; both adjudicated on retail bytes before landing |
| E3 `SetDifficultyVariables` | a source flip fixes 2 and breaks 6, net -4 (HIGH) | not attempted; structural table stands | ✅ registered negative, honoured |
| E4 `IsEmptyPhrase` | unsigned/width conversion reproduces the clrrwi (LOW-MED) | explicit `(unsigned int)` cast INERT, 96.55173 unchanged, recompiles=1 | ❌ MISS — and the named failure mode was right: MSVC elides it because `lwz` is already clean |
| E5 `GetNoteSliceWeight` | not planned | not attempted | unattempted, not refuted |
| E6 `GetBestHit` | not attempted | not attempted | honoured |

**Net landed: +3 functions / +1004 B / +0.009796 pp**, from E2 alone.

The E2 miss is the one worth reading: an over-delivering prediction is a warning, not a
win. Two of the three crossings were unplanned, and an unproven crossing is exactly the
fabricated-alias hazard. Both were proven on retail bytes before landing, and the row that
reaches the *other* twin is precisely the row that did not cross.
