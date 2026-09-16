# W16-GF — `GemManager::UpdateLeftyFlip` 99.41666 → 100.0 (+1,584 B)

Base: main `6159dfd5`. Worktree `~/tmp/wt-w16gf`, branch `w16-gf`.
Ruler: **`functionRelocDiffs=name_check` (graded)**, read from `report.json`
`provenance.diff_config` (objdiff 4.2.9, `a5f0ea903ec1`). Fix commit `0f26704b`.

## Result

| | leg A | leg B |
|---|---|---|
| row fuzzy / mpn | 99.41666 / 99.49242 | **100.00000 / 100.00000** |
| whole `matched_functions` | 44,138 | 44,139 (**+1**) |
| whole `matched_code` | 4,168,232 | 4,169,816 (**+1,584 B**) |
| whole `matched_code_percent` | 40.677315 | 40.692772 (**+0.015457 pp**) |
| `masked_equal_functions` | 23,323 | 23,323 (**+0**) |
| honest (`matched − masked_equal`) | 20,815 | 20,816 (**+1**) |

Settled A/B via `tools/ab_measure.py --from-dirty` (both legs settled to zero
work; leg B recompiled 2 TUs and ran all 6 patch steps). Pre-registered in
`~/tmp/w16gf_prereg.md` before measuring: **every key hit, zero misses**,
including all five falsifiers. `Δmasked_equal = 0` is the control that matters —
the crossing is a real byte match, not funclet byte-signature forgiveness.
`0 units fell off 100%` on both rulers.

## The defect: one scheduling decision wearing six hats

The charged-site list was `1 insert + 1 delete + 5 diff_arg`, 390/397
instructions equal, with a `REGISTER_SWAP (r8↔r9)` label and a
`MaybeFixable` verdict recommending a permuter sweep.

Six of the seven sites were **one** difference. Retail:

```
88  subi   r10, r3, 0x3          ours: subi   r9, r3, 0x3
89  lwz    r11, guard@l(r29)     (equal)
90  cntlzw r8, r10               ours: lis    r10, &msg@h      <-- ours EARLY
91  lis    r10, &msg@h           ours: cntlzw r9, r9
92  rlwinm. r9, r11, 0, 30, 30   ours: rlwinm. r8, ...
93  extrwi r27, r8, 1, 26        ours: extrwi r27, r9, ...
```

Retail computes `subi r10` → `cntlzw r8, r10` across **two** registers, which
leaves `r10` dead and immediately recyclable by the `lis` that forms `&msg`.
We computed `cntlzw r9, r9` **in place**, so we needed a separate register for
the `lis` and emitted it one slot early. The insert/delete pair is that one
`lis` at two different positions; the four register `diff_arg`s are its fallout.

Removing the `_tmp1` named temporary restored retail's scheduling and **all six
sites dissolved at once**. This is the 13th recorded instance of `REGISTER_SWAP`
being a symptom rather than a diagnosis — do not defer a row as permuter-bound
on that label.

The seventh site, `[172]`, was independent: `lwz r11, 0x70(r31)` (retail) vs
`lwz r11, 0x0(r3)` (ours). Retail re-materializes the just-constructed
`Symbol("drum_lefty")` temp from the frame; we reused the constructor's returned
`this`, still live in `r3`. Naming the temporary gives it a stack home the
compiler reloads from.

## Negative result — do not re-buy

**Hoisting `static Message msg("set_lefty", 0)` above the `isKeys` computation
is a −13.1 pp REGRESSION** (99.41666 → **86.29798**). The obvious reading of
"our `lis` is too early, so move the static earlier" is exactly backwards: the
local static's guard-check block moves wholesale ahead of the
`GetBandUser()->GetControllerType()` call and restructures the region. The
static's current position is load-bearing and correct. Tried, measured,
reverted.

## Lever attribution (both required, independent)

| lever | row fuzzy | closes |
|---|---|---|
| baseline | 99.41666 | — |
| name the `drum_lefty` temp, alone | 99.43182 | `[172]` only |
| drop `_tmp1`, alone | 99.98485 | the whole 88–94 cluster |
| **both** | **100.00000** | all 7 |

## Two corrections to the lane brief

1. **The 5 `diff_arg` charges were NOT relocation-name charges.** They are
   `[reg:…]` and `[off:…]` — register and offset arguments. `diff_arg` is an
   argument-level *kind*, not a relocation-name class. The ICF
   fold-alias-vs-wrong-callee adjudication was therefore **not applicable to any
   charged site on this row**, and running it would have been wasted budget.
   The row's genuine fold-aliases (`GetBandUser`↔`DataArrayPtr::operator`,
   `IsLefty`/`UseLeftyGems`↔`fn_82BAA1C8`, `GetControllerType`↔`GetCacheName`,
   `GetGameCymbalLanes`↔`UncompressedSize`) sit in objdiff's *Function Call
   Diff* block and are **already forgiven** via `icf_aliases.map` — which is
   precisely why they carry zero charges. **Read the charge list's bracket tags
   before choosing an instrument**; a `diff_arg` count alone does not name a
   defect class.
2. Briefed mpn was 99.41666; the measured value is **99.49242**. The two rulers
   legitimately differ here — the insert/delete pair is instruction-level and so
   is charged by `mpn`, while the five `diff_arg`s are argument-level and are
   excluded from it. That is the documented `mpn ≥ fuzzy` relation, not an error.

## ⚠ A tool misattribution worth not inheriting

`run_objdiff`'s **"Offset Mismatches (resolved)"** reported site `[172]` as

> `lwz`: target 0x70 vs base 0x0 (**GemManager::mTrackDir (TrackDir \*)**)

**This resolution is wrong.** The prologue is `subi r31, r1, 0x1a0` / `mr r30,
r3` — in this function **`r31` is the frame pointer and `r30` is `this`**. So
`0x70(r31)` is a *stack slot* (the `Symbol` temporary), not a member at
`this+0x70`. The resolver assumes `r31 == this`, which holds in most functions
but not this one, and it prints the false attribution with no hedge.

A lane that trusted it would have gone hunting a `mTrackDir` layout/offset
defect that does not exist. **Confirm which register holds `this` from the
prologue before believing any resolved offset**, exactly as the house rule says
to ask the compiler rather than the header comments.

## Deliberately not done

- **No permuter run.** It was the tool's own recommendation, and it would have
  been the wrong instrument: the cause was a source-level temporary, and the
  source fix was found by reading the prologue and the scheduling window. The
  standing directive also defers the permuter.
- **No declaration-order lever** — known inert for register-only swaps, and the
  register diffs here were fallout rather than the cause.
- **No map/alias edit.** The row needed none, and an unproven alias would have
  lifted the score by construction.
