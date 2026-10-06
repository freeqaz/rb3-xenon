# W16-RF — large char and particle rows below 100 (2026-10-06)

Lane W16-RF, branch `w16-rf`, worktree `~/tmp/wt-w16rf`, based on main `5aa1d53b0`.

**Brief.** Bring the large `src/system/char` and `rndobj/Part` rows that sit below 100 up to 100 by
fixing our source against retail bytes, with DC3 as the closest reference. The named rows were:

- `CharHair::SimulateInternal`
- `CharIKHand::Poll`
- `RndParticleSys::InitParticle` and `RndParticleSys::Load`
- `CharIKFingers::SetName`

After those came every other char/Part row with fuzzy ≥ 95 and size ≥ 1 KB: 20 rows in all.
W16-QQ's record (`W16QQ_FRAME_CENSUS_ROWS_2026-10-06.md`) was the starting point for IKElbow and
MoveParticles.

**Rules.** Source only: no map, alias or splits edits, and no permuter. No headers were touched.

**Structure.** The work ran in seven forks, grouped by source file so edits could not collide.
Each fork had its own worktree `~/tmp/wt-w16rf-<x>` and branch `w16-rf-<x>`, and each was merged
into `w16-rf` with `--no-ff`. Each fork row-diffed the whole `report.json` after a full build
against a base copy taken after its worktree's first full build.

**Ruler.** `name_check`, objdiff 4.2.9 (`report.json` `provenance`).

## Result

| | |
|---|---|
| rows to fuzzy 100 | **3 of 20**: `CharIKHand::Poll`, `CharIKHead::Poll`, `CharEyes::Poll` |
| rows to mpn 100 only | `CharHair::SimulateInternal`: fuzzy 99.95, so +1 fn but 0 B |
| rows improved, not crossed | 5: `RndParticleSys::Load`, `CharCollide::Deform`, `CharLookAt::Poll`, `CharEyes::NextLook`, `CharEyes::LidTrackAndClampingUpdate` |
| merged-tree row diff vs base | **9 rows up, 0 down**, 0 rows on one side only (68,884 rows each) |
| merged-tree measures | 54,700 → 54,704 fns; 6,019,176 → 6,024,976 B (**+4 / +5,800 B**); `total_code` unchanged |
| whole-binary A/B | **+4 fns / +5,800 B / +0.056600 pp**; Δhonest +4, Δmasked_equal +0; Δfuzzy +0.002260 pp |
| units at 100 (mpn) | 590 → 590; 0 reached, 0 fell off |
| native gate | `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0` |

**Prediction vs measurement.** The sum of the forks' own row diffs was:

| fork | Δ fns / Δ B |
|---|---|
| A | +1 / 0 |
| B | +1 / +2,364 |
| D | +1 / +1,820 |
| E | +1 / +1,616 |
| C, F, G | 0 / 0 |

That predicted +4 / +5,800 B. The merged tree's full build read +4 / +5,800 B, which is that
prediction exactly, before the A/B ran. The A/B then measured +4 / +5,800 B too. Its per-unit
list is CharEyes, CharHair, CharIKHead and FileMerger, +1 each.

## Per-row outcome

Before → after is fuzzy / mpn, from `report.json` after full builds.

| row | size | before → after | fork |
|---|---|---|---|
| `?Poll@CharIKHand@@UAAXXZ` (unit FileMerger) | 2,364 | 98.26 / 98.68 → **100 / 100** | B |
| `?Poll@CharIKHead@@UAAXXZ` | 1,820 | 99.82 / 99.99 → **100 / 100** | D |
| `?Poll@CharEyes@@UAAXXZ` | 1,616 | 98.49 / 99.01 → **100 / 100** | E |
| `?SimulateInternal@CharHair@@IAAXM@Z` | 2,472 | 96.18 / 96.59 → 99.95 / **100** | A |
| `?Load@RndParticleSys@@UAAXAAVBinStream@@@Z` | 2,436 | 95.97 / 96.40 → 99.43 / 99.67 | C |
| `?Deform@CharCollide@@QAAXXZ` | 1,052 | 97.33 / 98.05 → 98.88 / 99.58 | G |
| `?Poll@CharLookAt@@UAAXXZ` | 2,268 | 99.84 / 100 → 99.93 / 100 | D |
| `?LidTrackAndClampingUpdate@CharEyes@@…` | 1,764 | 98.99 / 99.40 → 99.32 / 99.55 | E |
| `?NextLook@CharEyes@@IAAXXZ` | 1,684 | 99.60 / 99.99 → 99.67 / 99.99 | E |
| `?InitParticle@RndParticleSys@@…` | 2,492 | 98.21 / 98.80 unchanged | C |
| `?MoveParticles@RndParticleSys@@IAAXMM@Z` | 1,432 | 98.01 / 98.58 unchanged | C |
| `?IKElbow@CharIKHand@@…` (unit FileMerger) | 2,880 | 97.92 / 98.89 unchanged | B |
| `?SetName@CharIKFingers@@…` | 2,076 | 99.23 / 99.23 unchanged | D |
| `?Relativize@CharBonesSamples@@…` | 1,448 | 96.47 / 96.68 unchanged | F |
| `?RotateTo@CharBones@@QBAXAAV1@M@Z` | 1,644 | 99.76 / 100 unchanged | F |
| `?ScaleAdd@CharBones@@QBAXAAV1@M@Z` | 1,756 | 99.95 / 100 unchanged | F |
| `?RotateBy@CharBones@@QBAXAAV1@@Z` | 1,420 | 99.94 / 100 unchanged | F |
| `?DeformMesh@CharCuff@@…` | 1,364 | 99.40 / 99.98 unchanged | G |
| `?PostLoad@Character@@UAAXAAVBinStream@@@Z` | 1,440 | 99.65 / 99.68 unchanged | G |
| `?CalcBoundingSphere@Character@@UAAXXZ` | 1,340 | 99.90 / 99.99 unchanged | G |
| `?Poll@CharSleeve@@UAAXXZ` | 1,980 | 99.80 / 100 unchanged | G |

## Causes and levers for the rows that moved

### `CharHair::SimulateInternal` is also a behaviour fix

Our source put the whole per-point tail inside `if (pt.collides.size() != 0)`. The tail covers:

- the frame rebuild (`Scale` / `Cross` / `Normalize`)
- `SetWorldXfm`
- the force, friction and inertia update
- `t100.v = pt.pos`

In retail, the `size()==0` branch lands on the `Scale(m128.y, …)` line, so only the collide loop is
gated. DC3 has the same shape. With our version, a point with no collides never updated its bone,
its force or the running transform. Closing that one brace removed 28 register swaps and an
insert/delete cluster.

Three single-instruction commutative operand-order rows are left: idx 17 `fmuls`, idx 271 `add`
for `points[j-1]`, and idx 418 `fadds`. The following changed none of the three:

- all 252 dependency-valid orders of the seven top-of-function locals
- operand swaps in the source
- renaming locals
- four placements of `innerSumRad`
- four `points[j-1]` spellings

Several of those edits flipped other commutative rows far from the edit.

### `CharIKHand::Poll` → 100: three causes

1. **`self`.** Declare `self` before `destPos`/`destQuat` and read `mScalable` through it, as DC3
   does. This fixed `li r28,0` being scheduled before `subi r27,r3,0x20`. 98.26 → 98.46.
2. **Weights loop.** Use DC3's indexed `localWeights[i]` in both loops instead of a `weightPtr`
   and a hoisted `endIt`. Retail reloads `begin` in the else branch and forms the weight pointer
   inside each loop guard. Fuzzy 99.98.
3. **Constant load order.** Write `0.001f` as a literal instead of a local `static const
   kMinWeight`. Retail loads 0.001 before 144. → 100.

Inert: `mTargets[0]` for `front()`, swapping the two static const declarations, a file-scope
`kMaxWeight` (it still folds into `__real@43100000`). The RB3 game source's branch order gave 99.26.

### `CharIKHead::Poll` → 100: DC3's helpers plus a dead local

The fix returns to DC3's `Length(headOffset)` and
`ScaleAddEq(targetPos, headOffset, blendDist / headOffsetLen)`. It also keeps an unused
`float scale = blendDist / headOffsetLen;`. The dead local is created and then eliminated, and that
changes the operand order MSVC picks for later commutative FP operations:

- the component order of the spine `Subtract`
- the `fmadds` in its `ScaleAdd`
- the correction loop's `Add`

Without the local, the row scored 99.87. It also scored 99.84–99.87 with any of these instead:

- spelled as `ScaleAddEq(.., scale)`
- written as a `MILO_ASSERT`
- written as a `(void)` expression

About 140 combinations were screened. The local is documented in place. It has no behavioural
effect; clang may warn `-Wunused-variable`.

### `CharEyes::Poll` → 100

- **Named default consts.** `kDefaultMinLookTime` / `kDefaultMaxLookTime` replace the shared
  `1.0f`. Retail reloads the constant from an address held in r31; ours had kept the value in f31.
  DC3 fixed the same row the same way.
- **Compare order.** The compare is now `mCurrentInterest == mFocusInterest`.
- **Dead local removed.** An unused `interest` local is gone.

Those three edits together reached 99.975. The last row was the x-term `fmadds` of the dot
product. The inline `Dot()` helper gave identical bytes with either argument order. Of the eight
hand-written operand orders, only target-first on all three terms gives 100.

### Improved, not crossed

**`RndParticleSys::Load`, 95.97 → 99.43.** The save-revision constants were checked first and are
right. The difference was in the old-revision bounce-plane block:

- The `d` term is written `-(v1.z*c + (v1.y*b + a*v1.x))`, retail's order. This alone gave 99.38.
- The basis is built in retail's statement order: `tf.v = p150.On()`, then `tf.m.z = p150`, then
  two `Cross` calls.

Left: in the first `Cross`, the two zero multiplies land in f10/f9 where retail has f9/f10, and
the register naming differs through the rest of the block.

**`CharCollide::Deform`, 97.33 → 98.88.** Two edits:

- In the centre loop, the vertex index is read into a local and `vertPos` is built one component
  at a time.
- In the radius loop, `deformed` is computed in y, x, z order.

Both loops now share retail's base pointer. Left:

- the pointer anchor: s+8 where retail has s+0xc
- `Length` squares `vec.y` first in retail
- one extra `fmr`
- the operand order of the `1/len` multiply

**`CharLookAt::Poll`, 99.84 → 99.93 (mpn was already 100).** References `parentMat`, `filterMat`
and `xAxis` are bound before the first Multiply and Cross. They generate no code but fix four
operand-order rows. Four rows are left: the sourceFilter `*=` and the sourceFilter Multiply's x
terms. About 3,400 screened variants did not move them.

**`CharEyes::LidTrackAndClampingUpdate`, 98.99 → 99.32.** `newLowerPos += Vector3(...)` replaces
three field updates, which fixes every Vector3 stack slot. The sqrt sum is written in x, y, z
order. Left:

- the f21/f22 assignment of `1.0f` and `0.0f`
- the scheduling of the upper-lid `fneg`
- one `fadds` operand order

**`CharEyes::NextLook`, 99.60 → 99.67.** `dirXfm.v.z` is read into `dirZ` before the compare.
Left: the operand order of the multiplies and adds in the scale, extrapolation, projection and
`Set` rows, and the x/z/y load order of `headXfm.v`.

## Attempted, not moved

| row | what is left | tried |
|---|---|---|
| `InitParticle` | Four inlined shared helpers: two colour `Subtract`s, the vel rotate and the bubbleDir rotate. Retail sums their terms in a different order. DC3's source records the same leftover. | Bracketed file-local rotate helper (13-build sweep, best 98.37); direct terms on `p->vel` 97.36; reference local 97.04; unbracketed per-site helper 97.47. |
| `MoveParticles` | `dragFactor`/`rpmDragFactor` f16/f17 swap; load order in the dot product and the relForce add; retail reloads `vel.z` after storing it. | `pos.z` operand flip (inert); 12 `velDotN` term orders (bracketed 93.9–94.0, unbracketed 97.99–98.01); `OnOrAbove` orders (inert); `v <= pl` 94.11. |
| `IKElbow` | Schedule of the 16 quaternion products (see below). | Computing `sphereToAxisDist` through `IKDistance` fixes rows 433–444 but shifts the FPRs: 97.81, reverted. |
| `CharIKFingers::SetName` | At two right-hand sites (`bone_R-index03`, `spot_R-ringfinger_tip`), retail emits `mr r3,r29` before `addi r4`; ours emits them the other way round. Our source equals DC3's, and the `.rdata` string order matches. | About 620 compiles: no-code statements at 58 insertion points, per-site spellings, call-argument forms. Only real extra code changes the pattern. |
| `CharBonesSamples::Relativize` | One block-merge decision. The ShortQuat and uncompressed arms start with the same registers, so the offset loads are hoisted above `cmpwi 3` and the two arm entries merge. Retail hoists only `mStart`, and its uncompressed arm jumps into the quat loop's end test. | Inline offset accessors at the arm entries and the loop tests: byte-identical. |
| `CharBones::RotateTo` / `ScaleAdd` / `RotateBy` (mpn 100) | FP operand order only: 15 rows / 2 / 2. | Operand swaps, statement orders, `Quat(...)` ctor, aliases, `Add(...)` forms: byte-identical or lower (95.7–99.73). |
| `CharCuff::DeformMesh` (mpn 99.98) | About 25 multiply operand swaps. | Six axis-declaration orders (97.92–99.43; the best trades the fixed rows for an f20/f21 swap); plane-term spellings 99.34–99.41; a `Vector3` copy 91.23. |
| `Character::PostLoad` | Retail addresses the static revision data off the revision struct at 0/4/8; ours addresses it off `gCharMe` at −8/−4/0. Retail also has a dead `stw rev,0x54(r31)`. | Two separate statics, a `{stream,rev}` local struct, if/else, the `oldRev > 1` test, and the store order: none moves the addressing base. |
| `Character::CalcBoundingSphere` | Only the left-clavicle `Distance` computes z before y. The textually identical right-clavicle block matches. | Six spellings of the left block: all 99.8985, and swapped arguments 99.88. |
| `CharSleeve::Poll` (mpn 100) | Ten add/multiply operand swaps. | `-(mPosLength + absed)`: byte-identical. |

Retail's IKElbow quaternion product order, worked out by fork B for a later lane:
`y*dw, z*dx, z*dw, x*dx, y*dx, z*dy, x*dz, x*dy, y*dy, w*dy, w*dx, w*dz, x*dw, w*dw, y*dz, z*dz`.

## What this lane adds to the pattern record

- **Operand order of commutative FP ops is mostly a function-wide property.** Most of the 13 rows
  left below 100 are pure operand-order or FPR-naming residue. Edits far from a mismatch often
  flipped it, and the source operand order at the mismatch usually did not. Forks A, D and G each
  observed this independently.
- **Exceptions: a hand-written expression is not the same as its inline helper.**
  - `CharEyes::Poll`'s dot product reached 100 only when written out by hand. `Dot()` gave
    identical bytes in either argument order.
  - `CharIKHead::Poll` went the other way: it reached 100 only with the helpers (`Length`,
    `ScaleAddEq`) plus a dead local.

  So for a residual FP-order row, try helper versus hand-expanded, and a dead temporary, before
  trying source operand swaps.
- **Two rows are fixed by real structure, not spelling.** In `CharHair` it was the scope of an
  `if`. In `RndParticleSys::Load` it was statement order and bracketing in the plane block. Check
  for structure like this first, on any row below about 97.

## Files changed

Nine source files, +153/−88, all `.cpp`, no headers:

- `src/system/char/CharHair.cpp`, `CharIKHand.cpp`, `CharIKHead.cpp`, `CharLookAt.cpp`,
  `CharEyes.cpp`, `CharCollide.cpp`
- comments only: `src/system/char/CharBones.cpp`, `CharBonesSamples.cpp`
- `src/system/rndobj/Part.cpp`

Behaviour-relevant for the native build: the CharHair per-point tail now runs for every point,
which fixes hair simulation for points without collides. Every other change preserves behaviour.

## Reproduce

```
git diff 5aa1d53b0..w16-rf -- src > ~/tmp/w16rf_src.patch
python3 tools/ab_measure.py --worktree ~/tmp/wt-w16rf-ab --patch ~/tmp/w16rf_src.patch
```

`~/tmp/wt-w16rf-ab` is at main `5aa1d53b0`.

- Run dir: `.ab_measure_runs/20261006-203121-w16rf_src-3156690/`
- Log: `~/tmp/rb3_ab_w16rf.log`
- Patch kind `['source']`. Leg A settled in 2 iterations. Leg B had 15 recompiles and settled in 2.

Native gate: run on `~/tmp/wt-w16rf` at `fc2b50e6f` (the merged source tree); log `~/tmp/w16rf_native_gate.log`.

Fork helper scripts are in `~/tmp/`:

- `w16rf_rowdiff.py`: whole-report row diff
- `w16rf_a_perm.py`: CharHair declaration-order sweep
- `w16rf_d/`: 2.5 s scratch-compile screen
- `w16rf_c/`: Part sweeps
- `w16rf_f/score.sh`
