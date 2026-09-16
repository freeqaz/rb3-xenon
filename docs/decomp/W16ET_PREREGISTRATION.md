# W16-ET — pre-registration (written BEFORE any A/B measurement)

Committed before `tools/ab_measure.py` was run, so the git history proves these
numbers were not written after the fact.

Baseline measured in THIS worktree (`build/45410914/report.json`, ruler
`name_check` read from `provenance.diff_config`, build settled to zero work):

```
matched_functions 43969   masked_equal 23224   honest 20745
matched_code 4130116      matched_code_percent 40.305344
fuzzy_match_percent 50.010094
total_functions 69240     total_code 10247068
```

⚠ This is NOT W16-ER's baseline (43958 / 4125560). Main moved between the lanes.
Both of my legs are measured in my own worktree; no main figure is inherited.

## The change

Three source edits in `src/band3/game/VocalPart.cpp`, all backed by retail bytes:

1. `IsEmptyPhrase` — `mPhrases.data() + mPhrases.size()` -> `mPhrases.end()`.
   Retail loads the vector's stored `_M_finish` at `0x4(vec)`; we recomputed it
   as `begin + (end-begin)/0x38*0x38` (6 surplus instructions: `subf`, `divw`,
   `mulli`, `add`, plus the `li 0x38` and a `clrrwi`).
2. `InTambourinePhrase` — same `.end()` fix, plus the return shape changed from
   `bool result = false; ... result = true; return result;` to two returns.
   Retail materialises `li r3,1` mid-body and falls to a shared `li r3,0`;
   our accumulator form materialised `li r3,0` first and `li r3,1` last.
3. `FramePhraseMeterFrac` — branchy clamp -> `Clamp(0.0f, 1.0f, ratio)`, and the
   ratio computed with an if/**else** instead of initialise-then-overwrite.
   `Clamp`'s float specialisation in `src/system/math/Utl.h` is
   `Min(Max(min,value),max)`, which expands to exactly retail's
   `fneg; fsel; fsubs; fsel` — matched instruction-for-instruction, so this is
   an identification off retail bytes, not a guess.

## Predictions

| row | size | now | predicted | confidence |
|---|---:|---:|---:|---|
| `?InTambourinePhrase@VocalPart@@QBA_NXZ` | 44 | 18.0000 | **100** | high |
| `?FramePhraseMeterFrac@VocalPart@@QBAMXZ` | 136 | 57.6471 | **100** | high |
| `?IsEmptyPhrase@VocalPart@@QBA_NABQBVVocalPhrase@@@Z` | 116 | 65.2414 | **100** | MEDIUM — residual reg/`clrrwi` diffs at idx 18/19/21/24/27/28 may survive the `.end()` fix |
| `?GetBestHit@VocalPart@@…` | 528 | 96.9697 | **96.9697 (unchanged)** | high — not edited, see below |

Whole binary, if all three close:

```
Dmatched      = +3
Dcode_bytes   = +296
Dcode%        = +296/10247068*100 = +0.002889 pp
Dunits at 100 = 0   (VocalPart has 33 unpaired fn_ rows; it cannot reach 100%)
```

Conservative (IsEmptyPhrase does not close): `Dmatched=+2`, `Dcode_bytes=+180`.

## Named failure modes

- The `.end()` change is inside a TU whose other functions are `/Ob2`-inlined;
  an unpaired caller could be perturbed. That is the very mechanism this lane
  is studying, so it is a real risk, not a formality.
- `Clamp` is a template; if it fails to inline, `FramePhraseMeterFrac` gains a
  call and regresses.
- `IsEmptyPhrase`'s residual may be a genuine second defect.
- Settling noise / an `ab_measure` refusal.

## What I am deliberately NOT changing

- **6 further `data() + size()` sites** in this file (lines ~339, 359, 370, 402,
  445, 507, 645) sit in functions with **no target row at all** (unpaired), so I
  have no retail bytes for them in either direction. Changing them would be an
  unverifiable guess. Handed off instead.
- `?GetBestHit@VocalPart@@` — see the lane doc; its residual is 4 instructions
  of pure scheduling around a **byte-exact** callee, and the permuter is OFF.

---

## Addendum — a FOURTH edit, pre-registered separately (still before any A/B)

Added after the three above were already committed; committed before measuring.

`GetNoteSliceWeight` (484 B, fuzzy 88.1240, 60 charges) — loop 1 held
`float frameMs = kFrameTimeMs;`, a **local copy of the global**, and passed
`std::min(spC, frameMs)`. Retail passes `kFrameTimeMs` directly.

One cause explains all four symptoms simultaneously, which is why I believe it:

| symptom | target | ours | explained by |
|---|---|---|---|
| FPR save helper | `__savefpr_21` (f21-f31) | `__savefpr_20` (f20-f31) | our extra float local costs one FPR |
| GPR saves | `r30` **and** `r31` | `r31` only | retail spends a GPR holding `&kFrameTimeMs` |
| the `min` operand | `mr r11, r31` (global's address) | `addi r11, r1, 0x54` (stack copy) | `std::min` takes `const float&`; a local copy must be spilled |
| idx 33-36 | absent | `lis`/`lfs`/`stfs` -> `0x50(r1)` | materialising the copy |

Prediction: `88.1240 -> **100**`, `+484 B`, confidence MEDIUM-HIGH (60 charges is
a lot, and most are FPR renumbering that should follow the pressure change; if
any real second defect hides under them it will survive).

Revised whole-binary prediction if all four close:
`Dmatched = +4`, `Dcode_bytes = +780`, `Dcode% = +0.007612 pp`, `Dunits@100 = 0`.
Conservative (GetNoteSliceWeight and IsEmptyPhrase do not close): `+2 / +180 B`.
