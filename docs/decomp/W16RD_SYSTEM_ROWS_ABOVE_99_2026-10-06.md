# W16-RD — large `src/system` rows at or above 99, fixed against retail bytes (2026-10-06)

Branch `w16-rd`, off main `1b62177bd`. Scope: the src/system rows at fuzzy ≥ 99
and ≥ 1.5 KB, starting with the eight the brief named. The source of truth was
retail bytes. The current DC3 source was used as the reference text, and every
divergence was settled against the target `.s`.

## Result

| row | size | before | after | how |
|---|---:|---:|---:|---|
| `?Save@ObjectDir@@UAAXAAVBinStream@@@Z` | 2,108 | 99.58 | **100** | source (codegen shape) |
| `?Load@RndFont@@UAAXAAVBinStream@@@Z` | 2,140 | 99.99 | **100** | source (**behaviour fix**) |

Whole-binary A/B (`tools/ab_measure.py --patch`, both commits as one patch, against `1b62177bd`):

Predicted **+2 fns / +4,248 B** (2,108 + 2,140). Measured:

```
leg A: matched=54696 masked=25213 honest=29483 code%=58.733390  (recompiles: 0, settled)
leg B: matched=54698 masked=25213 honest=29485 code%=58.774845  (recompiles: 5, settle iterations: 2)
Δmatched=+2  Δmasked_equal=+0  Δhonest=+2  Δcode%=+0.041455pp  Δcode_bytes=+4248
unit improvements: default/Font 96->97, default/system/obj/Dir 200->201
```

The prediction held exactly. Ruler: `name_check`. objdiff-cli sha256 `c1b7d95240a35cd6` was stable across both legs.


## The two fixes

### `ObjectDir::Save` (`src/system/obj/Dir.cpp`): codegen shape only

The sub-dir loop copied `mSubDirs[i]` into a raw `ObjectDir *subDir` and then
tested and used that copy. Retail tests the `ObjDirPtr` itself: the null test
goes through `operator ObjectDir*()`, and every use goes through `operator->`,
which re-reads the pointer field each time. The loop now binds
`ObjDirPtr<ObjectDir> &curSubDir = mSubDirs[i]` and uses it directly.
Behaviour is identical. 99.58 → 100.

### `RndFont::Load` (`src/system/rndobj/Font.cpp`): real behaviour fix

For `gRev < 4` the old cell-size pair is read and converted to a kerning
scale. Our code read the pair as width-then-height. Retail `0x82475A20` reads
two values and divides `Height()` by the **first** and `Width()` by the
**second**, so the stream order is height then width. Fonts saved at revs < 4
therefore had their cell width and height swapped in our build. The `gRev < 2`
integer path has the same order, and its assignments are ordered `h = hi;
w = wi;`, which is what pins the int→float slot order (the opposite statement
order leaves 4 diffs). 99.99 → 100.

## Rows tried and left (with what was tried)

- **`Spotlight::SyncProperty`** (4,728 B, 99.39 / mpn 99.83). The large
  cluster is the inlined `SetIntensity` → `SetColorIntensity` self-copy:
  retail keeps the object base and `&mColor` in separate registers, while ours
  CSEs them. The remaining 95 instructions are an r24↔r25 coloring of the
  `_val`/`_i` parameters. A prior lane ran six spellings, all inert or worse.
  They were not re-run. Coloring of parameters is inert to declaration order.
  This row needs the permuter.
- **`ObjectDir::PreLoad`** (3,092 B, 99.70 / mpn 100). The only difference is
  the `bs`↔`i20` r18↔r17 coloring. Two probes were inert: removing the
  `BinStream &d = bs` alias, and hoisting `int i20` to function scope.
- **`RndMesh::Load`** (3,452 B, 99.41). Retail spills `&mPatches`, `&mVerts`
  and `&mBones` to the shared temp slot `0x54(r31)` at four sites (diff idx
  424, 604, 809–811, 818). That slot is the write-only spill an inlined
  accessor leaves behind; the matched `mMat` sites at idx 143/148/154 have the
  same shape. Two probes were inert: `!mBones.empty()` with a `Vert*`
  iterator, and indexing `VertVector` through `(*this)[i]`. The accessor that
  produces the spill was not found.
- **`ParseNode`** (2,432 B, 99.98 / mpn 100). One `add` has swapped operands
  in the `PushBack(macro->Node(i))` loop. `mNodes` is private, so the index
  arithmetic cannot be respelled at the call site. Commutative swaps of this
  kind are inert in >99% of tries, so this row was not attempted.
- **`BandDirector::OnFileLoaded`** (3,816 B, 99.78). One `addi` schedule swap.
  The source already carries a negative-results block listing three inert
  spellings. Permuter class.
- **`CharLookAt::Poll`** (2,268 B, 99.84 / mpn 100). Nine FP commutative
  operand swaps remain after earlier lanes. Permuter class.
- **`CharIKFingers::SetName`** (2,076 B, 99.23). Every string literal matches
  retail (`0x8204CA3C` and `0x8204C940` checked). The residue is a 2-slot
  schedule of `addi r4` at 2 of about 20 identical statements. Not attempted.
- **`LightPreset::Load`** (3,024 B, 99.71). Retail's three statics sit at
  `0x82CC6E8C` (AltRev), `0x…90` (Rev) and `0x…92` (sLoading), with a hole at
  `0x…8E` and one base register anchored at AltRev. In our jumbo TU the order is
  `sPresetAltRev@0x24`, `sLoading@0x26` and `sPresetRev@0x28`. Reproducing
  retail's layout needs TU-wide `.bss` packing. Declaration order controls
  `.bss` order only, and the existing in-source comment from W16-HP already
  records this.
- **`XMAReader::Poll`** (1,556 B, 99.85). Retail copies with
  `clrrwi r11,r10,0`, a 32-bit unsigned copy; ours emits `extsw`.
  - `unsigned int skip` removes the copy altogether, which is worse.
  - `unsigned __int64` produces `rldicl` plus `cmpd`.
  - The current `__int64 skip = (unsigned)…` stays.
  - A prior lane also found that `Min()` swaps the compare operands.
- **`HDCache::Init`** (1,504 B, 99.79 / mpn 100). Two `_M_fill_insert` call
  sites name `vector<File*>`, but retail names the fold survivor
  `vector<Hmx::Object*>`. `tools/icf_pair_adjudicate.py --chase --size`
  returns **CHASED T1 PROVEN, size 108 == 108, but 1 CYCLE-ASSUMED**
  (`_M_fill_insert_aux<RndDrawable*>` vs `<File*>`). The house bar for
  admission is 0 CYCLE-ASSUMED, so **no alias was added**. Seven `lwzx`
  base/index operand swaps also remain.
- **`BandWardrobe::LoadMainCharacters`** (1,776 B, 99.79 / mpn 99.99). Two
  probes were reverted:
  - Swapping the comparison operands flips only the compare.
  - `Symbol inst = GrabInstrument(...); SetInstrumentType(inst);` fixes idx
    250/254 but rotates the volatile registers of the `lis` block at idx
    344–354, so mismatches go from 17 to 25.

  The load-order diff at idx 294/295 remains.
- **`json_tokener_parse_ex`** (4,872 B, 99.25). Every difference is in the
  **placement** of the out-of-line `goto out` tail stubs near idx 1170–1192.
  Retail places the `printbuf_memappend_fast` stub second and the
  `tok->err` (`stw r26`) stub sixth. Ours places them seventh and ninth. Two
  `printbuf_memappend_fast(...); goto out;` sites are candidates (the number
  and object_field states), and the stub order is the compiler's block layout,
  not source order. No reorder was attempted, because moving cases in a
  vendored json-c parser to steer block layout is not a defensible spelling.

### Surveyed only (permuter class, no source lever seen)

`CharBones::ScaleAdd` (2 FP commutative), `CharBones::RotateTo` (FP),
`EQEffect::SetParameter` (2 FP commutative), `BandCamShot::Load` (volatile
r10/r11), `MetaMusic::UpdateMix` (FP regs), `CharIKHead::Poll` (FP plus stack
permute), `NgMat::SetRegularShaderConst` (FP regswap plus a 3-word store
order), `CharSleeve::Poll`, `ResetNormals`, `CharEyes::NextLook`,
`DxTex::SyncBitmap` (regswap), `PlatformMgr::Poll` (load scheduling of the
anonymous-namespace `mFriendsBuffer`), `NoteTube::DrawToPlate` (mixed),
`CamShotFrame::Interp` (extra `fmr` plus load order), `NgEnviron::Select`
(regswap plus stack).

## Not done

- The permuter was not run on any row (it is OFF).
- No alias was added, including the HDCache pair, which proves T1 but carries one assumed cycle.
- Nothing was merged or pushed.

## Native gate

This was run on `96b4c3075` (both source commits; this doc commit is the only change after it):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
