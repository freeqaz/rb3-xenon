# W16-TY: VIA-DC3 unentered engine rows, gated natively on shipped RB3 data (2026-10-07)

Branch `w16-ty` (worktree `~/tmp/wt-w16ty`, off main `fa2443370`). Not merged or pushed.

## 0. Headline

`CAMPAIGN_STATE_2026-10-07c.md` §6 lever 2, VIA-DC3 half: engine rows that native compiles
but no target enters (§4.3: 173 rows / 145,432 B). Taken largest first. Each row is driven
on shipped RB3 data in `rb3-render`'s default mode (`native/src/w16ty_phase.cpp`, after
W16-TS's phase; `--no-w16ty` opts out). Each gate compares against a reference computed
without the code under test: a shipped member read directly, a closed form, or a retail
body decoded from the target asm.

| | rows | bytes |
|---|---:|---:|
| §4.3 population | 173 | 145,432 |
| taken: every row ≥ 1,332 B (33 rows), plus 9 smaller rows the same fixtures reach | 42 | 74,648 |
| gated directly (§2) | 19 | 33,064 |
| ran under a gate that checks them only through a caller (§2) | 3 | 1,656 |
| recorded reason, not gated (§6) | 20 | 39,928 |

- **23 W16-TY gates, all pass.** The full `rb3-render` run reads 186 PASS, 0 FAIL, rc 0,
  `RESULT: ALL GATES PASSED (0 gate failure(s))` (163 before this lane + 23).
- **Three native-only bugs found and fixed** (§5), each by a gate on shipped data:
  - `CharBones::RotateTo` blended in the wrong quaternion order natively (28 of 28 quat
    channels wrong);
  - `RndAmbientOcclusion::Tessellate` aborted on an empty scratch vector;
  - `KerningTable` left half its bucket heads uncleared on a 64-bit host, and a shipped
    `RndText` crashed in `Find`.
- **No gate found a behaviour difference between our X360 source and retail.** The three
  bugs are native-only: two sat in `HX_NATIVE`/LP64 paths and one was a libstdc++
  assertion.
- **Every gate fails on a broken body** (§4): three sabotage builds, 14 defects,
  predictions written first. All 14 were caught. Two were caught differently from the
  prediction, and both corrected a gate:
  - the bounding gate could not see one arm at all;
  - the kerning defect crashes the load after the ctor gate fails.
- **X360 A/B: Δ0 on every key** (§7), 5 leg-B recompiles.

## 1. Fixtures

- **Vignette** `world/vignette/transition/gen/tv11_a.milo_xbox`, loaded through
  `ObjDirPtr::LoadFile`: 5,215 objects in about 300 ms. `ty-fixture` checks five class
  counts against counts taken separately from the file's own directory tables: Spotlight
  8, ParticleSys 3, Light 17, Environ 7, TransAnim 2. The tables were read with a
  stand-alone decompressor (`~/tmp/w16ty/milo_classes.py`), not through the engine. The
  gate also checks Mesh > 300, CharClip > 100 and `sizeof(RndMesh::Vert) == 0x60`.
  - The walker deduplicates objects: `ObjDirItr(d, true)` already recurses, so a subdir's
    objects would otherwise be counted twice. That double count once made the AO
    receiver and the TessellateMesh mesh the same object.
- **Font milos.** All 32 files under `ui/resource/fonts/gen` (32/32 load): 60 fonts, 6
  kerning tables, 1,096 pairs. The vignette holds no font.
- **Census** (the same decompressor over all 4,455 shipped `.milo_xbox` files), used for
  the reasons in §6. CharSleeve appears in 0 files, CamShot in 2 (`ui/splash`,
  `ui/main/credits`), CharIKHand in 4 (the rigging milos), CharIKFingers in 1
  (`rigging/keyboard`), CharHair in 292 and CharCuff in 346.

## 2. Gates

| gate | row(s) checked | reference |
|---|---|---|
| `ty-fixture` | — | class counts read from the file (§1) |
| `ty-spot-sync-get` | `Spotlight::SyncProperty` 4,728 (99.39) | 38 reads per spotlight, by retail property name (the names are NUL-delimited strings in `band.exe`), each read through `Property()` and compared with the member read directly; covers `flare_visibility_test` (inverted), `color` (owner's packed colour), `intensity` (owner's); 304 reads |
| `ty-spot-sync-set` | `SyncProperty`, `Spotlight::SetColor` 176 | 200 writes through `SetProperty()`, each member read back and then restored |
| `ty-spot-xfms` | `Spotlight::UpdateTransforms` 1,340 | light can, lens, beam position/rotation (cone keeps identity, otherwise retail's `rot`), flare and floor spot, against the world transform |
| `ty-part-init` | `RndParticleSys::InitParticle` 2,492 | 3 shipped emitters × 400 particles. Each is checked against the emitter's own ranges: life and 1/life, box, speed, pitch, start colour, size, size velocity, mid/end colour paths |
| `ty-bones-fixture` | — | shipped clip `player0_f` (compression 2): 55 channels, 19 samples |
| `ty-bones-scale-add` | `CharBones::ScaleAdd` 1,756 | `q += sign(q·src)·f·src`, weight sum; every other source quat negated, so the negative-dot arm runs (14 of 28) |
| `ty-bones-rotate-by` | `CharBones::RotateBy` 1,420 | `q = src * q` (Hamilton), decoded from retail's uncompressed arm at `0x823AD090` |
| `ty-bones-rotate-to` | `CharBones::RotateTo` 1,644 | `q = q * b`, b = f·src with `b.w ± (1−f)` by the sign of src.w; product order decoded from retail's uncompressed arm (`0x823ADA1C`–`0x823ADAB8`) |
| `ty-make-normals` | `MakeNormals` 1,332 | angle-weighted face normals over 0.001 weld classes, first matching corner per face; 24 meshes / 16,149 verts |
| `ty-reset-normals` | `ResetNormals` 1,980 | the same sum over every corner, plus UV-gradient tangents with retail's bad-UV rules; 16,140 tangents (9 ill-conditioned excluded, counted) |
| `ty-scale-object` | `RndScaleObject` 3,112 (89.58) | retail's arm order (the RTTI chain of its `dynamic_cast` ladder) and per-field multipliers, at s = 2 and fov = 0.5, which give exact expected values; the inverse call must restore the shipped bits; 304 objects / 4,260 fields |
| `ty-char-bounding` | `Character::CalcBoundingSphere` 1,340 | 0.1 spheres at the head, ankles and toes, then a 7.0 sphere per clavicle raised by the clavicle-to-hand distance, merged in that order; local transform restored. The gate requires that an arm's radius be visible on at least one character (§4) |
| `ty-propanim-foreach` | `RndPropAnim::ForeachKeyframe` 2,256 | `foreach_keyframe` over 321 key sets / 2,350 keys of 16 shipped PropAnims, totals summed from the keys directly; then a `replace_keyframe`/`replace_frame` pass on 310 float sets |
| `ty-ao-fixture` | `BuildSphereStratified` 340 | `BuildTrees(0)` asks for 300 directions; the stratified rule gives 17 × 17 = 289; 28 cast meshes / 4,182 triangles |
| `ty-ao-open-sky` | `CalculateAOAtPoint` 608 | an unoccluded point facing +z reads the hemisphere SH integrals, (0.886, 0.5, 1 (clamped), 0.5); measured (0.8844, 0.5016, 1.0, 0.5038) at 289 samples |
| `ty-ao-occluded` | `CalculateAOAtPoint`; `kdTreeNode::Pack` 972 and `FindSplit_Mean` 500 through the tree | 1 cm in front of a cast face, facing it: DC 0 |
| `ty-ao-smooth` | `RndAmbientOcclusion::SmoothResults` 1,672 (74.79); `TransformNormal` 184 through it | each vertex colour = (angle-weighted mean of its weld class's face AO + old colour) / 2, weld at squared distance 0.001 |
| `ty-ao-tessellate` | `RndAmbientOcclusion::Tessellate` 4,796 (74.68) | invariants: original verts unchanged; valid, distinct indices; total area, vector area and open-edge length preserved; 398 → 1,106 faces |
| `ty-tessellate-mesh` | `TessellateMesh` 1,176; `RndAmbientOcclusion::BlendVert` 544 | exact 4-way split per face in retail's order, midpoints by the BlendVert rule; 288 → 1,152 faces |
| `ty-kerning-ctor` | `KerningTable` ctor | a table built over 0xA5 bytes clears all 32 heads |
| `ty-kerning-shipped` | `KerningTable` Load/Find | every head is null or the table's own entry; `Find` resolves each key to its last entry (one shipped key repeats; prepend-to-chain makes the later entry shadow the earlier, as in retail) |
| `ty-kerning-set` | `KerningTable::SetKerning` 352 | the largest table's 999 pairs, set into a table whose heads held 0xA5 and read back through `Kerning()` |

The AO family and TessellateMesh are ~74.7 to 94.7 fuzzy. Their gates check rules and
invariants, not retail bytes; §8 says what that leaves open. `ty-ao-occluded` checks the
kd-tree rows only through their result: a tree that lost the face would read open sky.

## 3. Wiring

- `native/src/w16ty_phase.cpp`: `RunW16TYPhase(gate)`, compiled with `-fno-access-control`
  so gates read members directly.
- `native/src/main_render.cpp`: calls the phase after W16-TS's phase; `--no-w16ty` skips it.
- `native/CMakeLists.txt`: adds the source and the per-file flag.
- `Gate()` flushes stdout, so a later fault cannot swallow verdicts already printed.

## 4. Sabotage controls (predictions written before each build, all reverted)

Predictions: `~/tmp/w16ty/sabotage_predictions.txt`. Each defect went into production
`src/`, and only `rb3-render` was rebuilt. The phase then ran with the other lanes'
phases off.

**Batch A.** 8 defects (A7 moved to A7′, see below). All 8 targeted gates failed, and every
other gate passed.

| defect | predicted | measured |
|---|---|---|
| A1 `SyncProperty` `top_radius` → `mBottomRadius` | sync-get and sync-set FAIL | FAIL, 8 of 8 spotlights each |
| A2 `InitParticle` life + 1 | part-init FAIL, ~1,200 of 1,200 | **FAIL, 5 of 1,200**. Prediction wrong in size: the gate checks the emitter's range, so a +1 shift only moves the top of the distribution out |
| A3 `ScaleAdd` negative-dot arm removed | scale-add FAIL, exactly 14 | FAIL, 14 |
| A4 `RotateBy` operands swapped | rotate-by FAIL, ~28 | FAIL, 28 |
| A5 `RotateTo` back to `q * other` (the old native bug) | rotate-to FAIL, ~28 | FAIL, 28 |
| A6 `MakeNormals` angle weight → 1 | make-normals FAIL; reset-normals FAIL if it goes through MakeNormals | make-normals FAIL, 15,601 of 16,149; reset-normals PASS (it does not) |
| A7 `CalcBoundingSphere` right-clavicle radius 7 → 6 | char-bounding FAIL | **PASS: not caught** |
| A7′ the same on the left clavicle | char-bounding FAIL, 13 | FAIL, 13 of 13 |
| A8 `ForeachKeyframe` float replace × 0.5 | propanim FAIL on the replace pass | FAIL, 122 of 310 float sets |

A7 was not a broken gate, but it was a blind one. `Sphere::GrowToContain` ignores a sphere
already inside the union, and on all 13 characters the right arm's sphere sits inside the
union of the head and the left arm. The gate now counts, per side, the characters where
that arm's radius changes the result: left 13, right 0. It fails unless the left count is
non-zero. A right-arm defect is invisible on this data by construction, and the gate
message says so.

**Batch B.** All 5 targeted gates failed, and the others passed.

| defect | predicted | measured |
|---|---|---|
| B1 `RndScaleObject` ParticleSys emit rate `/ fov` → `* fov` | scale-object FAIL, 3 ParticleSys | FAIL, 3 objects (`stage_fog.part value 22: 0.001 -> 0.0005, want x2`) |
| B2 `SmoothResults` alpha blend 0.5 → 0.75 | ao-smooth FAIL, most of 239 | FAIL, 239 of 239 |
| B3 AO `Tessellate` centre face winding flipped | ao-tessellate FAIL on vector area, area unchanged | FAIL: area 41.4135 kept, vector area 10.7162 → 5.1687 |
| B4 `TessellateMesh` f3 winding flipped | tessellate-mesh FAIL | FAIL, first difference at face 2 |
| B5 `UpdateTransforms` beam offset + 1 | spot-xfms FAIL, 8 beams | FAIL, 8 (`beam position`) |

**Batch C.** After the kerning gates were added: C1 put all three `KerningTable` clears
back to `memset(…, 0x80)`.

| predicted | measured |
|---|---|
| ctor gate FAIL, exactly 16 of 32 heads | **FAIL, 16 of 32** |
| set gate FAIL; shipped gate depends on heap garbage | **not reached**: SIGSEGV (rc 139) during the font milo loads, in `KerningTable::Find` ← `RndFont::Kerning` ← `CharAdvance` ← `RndText::ComputeCharWidths` ← `WrapText` ← `UpdateText` ← `RndText::Load` (gdb). The same stack as the original crash |

Batch C's first build printed nothing, because the ctor gate ran after the font loads. The
ctor gate now runs first, and `Gate()` flushes stdout. The run above is the second build.

## 5. Bugs found (all native-only; X360 tokens unchanged, §7)

1. **`CharBones::RotateTo` quaternion order** (`src/system/char/CharBones.cpp`, commit
   `6ce7e8b54`).
   - Three `#ifdef HX_NATIVE` blocks computed `q * other` where X360 calls
     `RotateToMultiply(other, q, other)`.
   - Retail's uncompressed arm (`0x823ADA1C`–`0x823ADAB8`) stores
     `x = q.w*o.x + q.x*o.w + q.z*o.y − q.y*o.z`, which is the Hamilton product `o * q`.
   - `ty-bones-rotate-to` read 28 of 28 quat channels wrong before the fix, and 0 after.
   - Every native `rotate_to` blend turned the wrong way. `RotateBy` (`0x823AD090`) was
     decoded too and is `src * dst` as written.
2. **`RndAmbientOcclusion::Tessellate` abort on an empty vector**
   (`src/system/rndobj/AmbientOcclusion.cpp`, commit `3e15e234a`).
   - Six sites take `&v[0]` of scratch vectors that are empty on a pass that splits
     nothing. STLport returns the begin pointer; libstdc++ asserts
     (`stl_vector.h:1253 … Assertion '__n < this->size()' failed`).
   - On the shipped `female_neck_ao.mesh`, the first pass split 600 faces and the next
     aborted.
   - Native now uses `AO_VEC_BASE(v)` (`data()`); X360 keeps `&(v)[0]`.
   - The same commit corrects the `kQualityLUT` comment. Retail's table is at
     `0x82070E04` and holds `{300, 150, 2, 0}`; the old comment gave an address inside EH
     data.
3. **`KerningTable` LP64 clear** (`src/system/rndobj/Font.cpp`, commit `726d3bea5`).
   - `mTable` is 32 pointers: 0x80 bytes on X360, 0x100 on a 64-bit host. The ctor,
     `SetKerning` and `Load` each cleared a literal 0x80, so buckets 16 to 31 held heap
     garbage natively.
   - A shipped venue `RndText` crashed in `Find` (pair 0x20/0x52, bucket 18).
   - Now `sizeof(mTable)`, which is the constant 0x80 on X360.

## 6. Recorded reasons (rows taken, not gated)

| row | B | fuzzy | reason |
|---|---:|---:|---|
| `RndTexRenderer::DrawToTexture` | 3,320 | 97.51 | draw row: its result is GPU state and pixels, and these gates compare CPU-side results only |
| `LightPreset::Load` | 3,024 | 99.71 | LightPresets live in the venue milos. This lane's native load of `small_club_15` desynced: an object under-read and `ReadDead` (`DirLoader::LoadObjs`) read past the end of the stream, so `Load` never ran on a completed shipped venue. Not root-caused; venue loading is a separate lane |
| `CharIKHand::IKElbow` | 2,880 | 97.92 | CharIKHand is only in 4 rigging milos and acts on a posed character driven to an instrument target; needs a rig-plus-character fixture |
| `CharHair::SimulateInternal` | 2,472 | 99.95 | hair in 292 milos, but the row is a time-stepped simulation on a posed, moving character; needs a reference integrator |
| `CharLookAt::Poll` | 2,268 | 99.93 | needs a posed character and an interest target |
| `NgSpotlightDrawer::SetupXSection` | 2,104 | 76.45 | draw row (NG spotlight drawer) |
| `CharIKFingers::SetName` | 2,076 | 99.23 | CharIKFingers is in exactly 1 shipped milo (`rigging/keyboard`); needs the keyboard rig on a character |
| `WorldCrowd::DrawShowing` | 2,072 | 96.85 | draw row |
| `CharSleeve::Poll` | 1,980 | 99.80 | CharSleeve is in 0 of 4,455 shipped milos: no shipped data drives it |
| `NgMat::SetRegularShaderConst` | 1,896 | 99.51 | shader-constant upload (GPU) |
| `RndTexBlender::DrawShowing` | 1,880 | 98.79 | draw row (TexBlender is in the 2 head milos) |
| `CamShotFrame::Interp` | 1,772 | 99.29 | CamShot is only in `ui/splash` and `ui/main/credits`; not taken |
| `CharEyes::LidTrackAndClampingUpdate` | 1,764 | 99.32 | needs a posed head with eyes and a look target |
| `Spotlight::BuildNGCone` | 1,692 | 74.90 | builds the NG beam mesh. At 74.90 fuzzy, a gate needs the retail body decoded first; not taken |
| `CharEyes::NextLook` | 1,684 | 99.67 | as `LidTrackAndClampingUpdate` |
| `NgMat::RefreshState` | 1,456 | 98.57 | GPU state |
| `CharBonesSamples::Relativize` | 1,448 | 96.47 | runs only when a clip is set relative to another (`CharClip::SetRelative`); not taken |
| `RndParticleSys::MoveParticles` | 1,432 | 98.01 | time-stepped integration over frames; gateable on the 3 fixture emitters with a reference integrator; not taken |
| `CharCuff::DeformMesh` | 1,364 | 99.40 | needs a skinned character wearing a cuffed item (346 milos carry CharCuff) |
| `BuildVisit` | 1,344 | 93.26 | `BuildFromBSP`'s walker; 0 of the vignette's 417 meshes carry a BSP tree (census printed by the phase) |

## 7. X360 A/B

Prediction: Δ0 on every key. The three `src/` edits change only `HX_NATIVE` arms, a macro
that expands to the old X360 tokens, and `sizeof(mTable)`, which is 0x80 on X360.

`tools/ab_measure.py --worktree ~/tmp/wt-w16ty --patch reverse_src.patch`. The patch is the
inverse of the lane's `src/` diff (`git diff HEAD fa2443370 -- src/`), so leg A is the lane
and leg B is the lane without its three source changes:

```
leg A: matched=54947 masked=25223 honest=29724 code%=59.315186  (recompiles: 0, settled)
leg B: matched=54947 masked=25223 honest=29724 code%=59.315186  (recompiles: 5, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   (legA 64.316340 -> legB 64.316340)
units at 100% [mpn ruler]: legA 617 -> legB 617  (Δ+0; 0 reached 100, 0 fell off; pairable units 1743->1743)
units at 100% [all-rows-fuzzy ruler]: legA 538 -> legB 538  (Δ+0; 0 reached 100, 0 fell off; pairable units 1743->1743)
```

## 8. Not done

- **131 of the 173 rows were not taken** (70,784 B). All of them are under 1,332 B;
  every row of 1,332 B or more was taken.
- **The 31 executed-but-ungated rows** (W16TF §6, 22,212 B) were not gated. The kerning
  gates' font loads run `RndText::Load` → `WrapText` → `ComputeCharWidths` →
  `RndFont::CharAdvance` → `RndFont::Kerning` (seen in the batch C stack), but no gate
  checks those rows' output.
- **The AO and TessellateMesh gates check rules and invariants, not retail's exact
  float results.** `Tessellate` and `SmoothResults` are ~74.7 fuzzy, so their bodies may
  still differ from retail in ways that preserve the invariants.
- **Possible LP64 sibling, not investigated:** `src/system/rndobj/VelocityBuffer.cpp:105`
  clears `mViewProjXfm` with a literal `memset(…, 0, 0xa4)`.

## 9. Result lines

(pasted from the runs below)

## Reproduce

```
scripts/setup_worktree.sh ~/tmp/wt-w16ty w16-ty
cd ~/tmp/wt-w16ty/native && cmake --build build --target rb3-render
cd build && ./rb3-render ~/code/milohax/rb3/orig-assets/xbox-zip <outdir> \
    --no-bandtrack --no-w16sh --no-w16tf --no-w16tj --no-w16tm --no-w16tr --no-w16ts   # this phase only
```
