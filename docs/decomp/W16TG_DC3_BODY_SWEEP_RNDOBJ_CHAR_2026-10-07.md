# W16-TG — DC3-newer body sweep over the 70 unnamed VIA-DC3 register/reorder rows (rndobj, char)

Lane W16-TG, 2026-10-07, worktree `~/tmp/wt-w16tg`, branch `w16-tg`, started at main `6390a7aa9`.
Ruler: shipped graded `name_check`, read from `report.json`.

Brief: `CAMPAIGN_STATE_2026-10-07b.md` §6 lever 5. That doc's §5 lists 70 VIA-DC3 register/reorder rows (34,436 B) in
`rndobj` and `char` that no lane has named and W16-QA never body-compared (QA skipped those two directories). Run
W16-QA's comparison against current `../dc3-decomp` on them, apply what retail bytes support, and give each row a
disposition.

## 1. Result

Whole-binary A/B (`tools/ab_measure.py --patch`, the branch diff against its merge base `6390a7aa9`, settled both legs,
17 leg-B recompiles):

| | leg A | leg B | Δ |
|---|---:|---:|---:|
| matched_functions | 54,896 | 54,905 | **+9** |
| matched_code (B) | 6,070,876 | 6,074,440 | **+3,564** |
| matched_code_percent | 59.240520 | 59.275300 | +0.034780 pp |
| honest (matched − masked_equal) | 29,686 | 29,695 | +9 |
| units at 100 (mpn / all-rows-fuzzy) | 620 / 543 | 620 / 543 | 0 |

The prediction from my own full builds was +9 fns / +3,564 B (54,896 → 54,905). The A/B read exactly that. Leg A
reproduces my baseline `report.json` row for row (0 rows differ).

| | rows | bytes |
|---|---:|---:|
| population | 70 | 34,436 |
| rows raised | **14** | — |
| of which to 100 (fuzzy) | 6 | 3,564 |
| rows lowered | **0** | — |

All 70 rows still had W16-TA's fuzzy at main `6390a7aa9` (0 moved), so the population was live as briefed.

**Behaviour defects fixed (native-visible), each confirmed on retail bytes:**

1. **`Bloom_Blur` sampled the destination and drew into the source.** Retail copies param 1 (r3) to r27 and param 2
   (r4) to r30. Every later instruction matched ours register for register, so the role retail keeps in r30, the
   draw target, is param 2. MSVC picks a parameter's callee-saved register from how it is used, never from its name,
   so a swap of exactly those two `mr` is a role swap. Our signature named them `(texDst, texSrc, ...)` against our
   own body. DC3's `(texSrc, texDst, ...)` is retail's. 99.90 → 100. Callers are unchanged: they already pass
   retail's order.
2. **`ComputeFaceTangentBasis` reported zero-area faces as bad UVs.** Retail reaches the notify only from the three
   `BadUV()` failures. Its four degenerate-edge and degenerate-UV tests branch straight to the epilogue with
   `outBasis` left as the identity. Our notify sat after the nested ifs, so it ran on every non-BadUV exit too.
   DC3's body has the right placement. 77.72 → 100.
   - **DC3 corrected for RB3:** DC3 emits it as `TheDebug << MakeString("NOTIFY: ...")`. Retail has no format
     string or `MakeString` call there (ours inserted both at idx 199+ when taken verbatim, 95.32). It stays a
     stripped `MILO_NOTIFY`, which in native still prints.
   - **DC3's `inline bool BadUV`:** with an out-of-line same-TU body, MSVC used `BadUV`'s register summary and kept
     the vertex pointers in volatile registers. Retail keeps them in r29–r31. The `BadUV` row stays at 100 either
     way.

The other twelve rows that rose are codegen only. Each was checked to compute the same values as before:
- reorders: `DistanceSH`, `BuildSphereStratified`, `CharEyes::Enter`, `UtilDrawSphere`, `Vector3Keys::SetFrame`;
- re-association under `/fp:fast`: `CalculateHandDest`, `CalculateFingerDest`;
- one reused local: `MeasureLengths`; `frame` reused in place of a copy: `MakeTransform`;
- `OnToggleHeap`: `SetShowing(x)` is set-plus-`Restart()`, and DC3 hoists one `Restart()` below the branch;
- `DrawTimers`: three `DrawStringScreen` calls in place of one through a `text` temp;
- `AppendWeights`: the insert position through `VertArray::end()`.

## 2. Method

W16-QA's tools, repointed (`~/tmp/w16tg/{body_cmp,swap,rowdiff}.py`; QA's doc §2 describes them).

1. **Population.** `~/tmp/w16ta-gap/viabreak.json['30']['none']`: W16-TA's "neither named since 10-06 nor
   QA-compared" list, 70 rows / 34,436 B (49 rndobj, 20 char, 1 synth by unit dir).
2. **Body comparison.** DIFF 39, SAME 13, NO_QNAME 10, OURS_ONLY 4, PARTIAL 3, NOT_FOUND 1.
3. **Mechanical swap.** 41 candidates, each compile-checked alone with `OBJCACHE=off` to a scratch `/Fo`: 28
   compiled, 13 did not.
4. **Batch, then keep.** All 28 applied together, full build: **10 up, 18 down**, so DC3's spelling was worse for
   our image here as it was for QA. Only the 10 risers were kept and rebuilt alone: 10 up, 0 down
   (+6 fns / +1,456 B).
5. **By hand.**
   - I read the 13 compile failures and the 10 NO_QNAME rows (operators and templates the extractor cannot name)
     against DC3 and against retail with `run_diff_inspect`.
   - Where DC3's change carried a retail shape, I ported that shape into our body. DC3-only API was not taken.
   - Every adopted DC3 comment was rewritten. All of their addresses were DC3-image addresses (`0x8262CE..`,
     `0x8237..`, `0x8238..`; ours are e.g. `ComputeFaceTangentBasis` `0x82439ee8`), and they carried DC3 lane tags.
     DC3's bodies had also dropped two RB3 retail comments, which were restored (`DrawTimers`' static guard,
     `MakeTransform`'s `mFollowPath` test). That commit moved 0 rows. The branch adds no address and no
     Wii-provenance wording.

## 3. Tried and reverted (row went down)

| row | change | before → after |
|---|---|---:|
| `Hmx::operator*(Matrix4, Matrix4)` (`Shader`, 848 B) | DC3's `Dot4` row·column form in `Mtx.h` | 77.14 → 21.31 |
| `Hmx::operator*(Transform, Matrix4)` (`Cam`, 688 B) | DC3's `Dot3` / `Dot3ZSeed` form | 85.61 → 19.97 |
| `CharBonesMeshes::AcquirePose` | DC3's single `char *pos` walk | 99.79 → 99.59 |
| `CharBonesMeshes::PoseMeshes` | `char *` walk + inline ratios | 99.89 → 99.84 |
| 18 batch-1 swaps | DC3 body verbatim | see §4 (`DC3 WORSE`) |

- The two `Hmx::operator*` collapses mean retail is the row-at-a-time form we already have, with each element
  stored before the operands are reloaded. DC3's dot form is fitted to DC3's image.

## 4. Per-row dispositions (all 70)

Columns: size, W16-NA class, body-compare class, fuzzy at base → fuzzy at leg B, disposition.
- `DC3 WORSE`: the swap compiled, fell in batch 1, and was reverted.
- `DC3 FLAT`: the swap compiled and moved nothing (`CharBones::RotateBy`, `RndTransformable::SetWorldXfm`).
- `SAME`: our body is identical to DC3's, so the residual is not in the function text.

| row | file | size | class | cmp | base | final | disposition |
|---|---|---:|---|---|---:|---:|---|
| `RndScaleObject` | rndobj/Utl.cpp | 3112 | I1 | DIFF | 89.58 | 89.58 | DC3 WORSE: swap compiled, 89.58->87.58 in batch, reverted |
| `CharBones::RotateBy` | char/CharBones.cpp | 1420 | P1 | DIFF | 99.94 | 99.94 | DC3 FLAT: swap compiled, row unmoved, not kept |
| `RndTransAnim::MakeTransform` | rndobj/TransAnim.cpp | 1160 | I1 | DIFF | 95.06 | 99.59 | RAISED (codegen): DC3 body adopted |
| `CharIKFingers::CalculateFingerDest` | char/CharIKFingers.cpp | 1100 | I1 | DIFF | 89.15 | 100.00 | TO 100 (codegen): DC3 body adopted |
| `Rnd::DrawTimers` | rndobj/Rnd.cpp | 1080 | P2 | DIFF | 97.97 | 99.75 | RAISED (codegen): DC3 body adopted |
| `CharHair::Hookup` | char/CharHair.cpp | 1012 | P1 | PARTIAL | 99.88 | 99.88 | extractor paired the wrong overload; row is register-only |
| `MakeTangentsLate` | rndobj/Utl.cpp | 952 | P1 | DIFF | 99.87 | 99.87 | DC3 WORSE: swap compiled, 99.87->87.55 in batch, reverted |
| `CharIKFoot::DoFSM` | char/CharIKFoot.cpp | 892 | P1 | DIFF | 99.96 | 99.96 | DC3 is a different revision (immediates 0.98f/kPlantHeight, native IK experiments); ours matches RB3 immediates |
| `Hmx::operator*` | rndobj/Shader.cpp | 848 | I1 | NO_QNAME | 77.14 | 77.14 | NO_QNAME, hand-compared: DC3 Dot4 form 77.14->21.31, reverted |
| `NgFur::Shell` | rndobj/Lit_NG.cpp | 816 | P1 | NO_QNAME | 99.95 | 99.95 | NO_QNAME, hand-compared: DC3 body differs only in comments/our fmuls-then-fadds lever; one register |
| `ComputeFaceTangentBasis` | rndobj/Utl.cpp | 812 | I1 | DIFF | 77.72 | 100.00 | FIXED (behaviour): DC3 body + retail notify shape (stripped, BadUV-fail only); DC3 TheDebug<<MakeString refuted |
| `RndVelocityBuffer::Draw` | rndobj/VelocityBuffer.cpp | 792 | P2 | DIFF | 98.69 | 98.69 | DC3 API only (BaseMaterial, kVelocityRefFrameMs); ours keeps RB3 SetDrawMode(5); left REGISTER |
| `CharIKFingers::CalculateHandDest` | char/CharIKFingers.cpp | 784 | P1 | DIFF | 99.85 | 99.90 | RAISED (codegen): DC3 body adopted |
| `CharBonesMeshes::PoseMeshes` | char/CharBonesMeshes.cpp | 752 | P1 | DIFF | 99.89 | 99.89 | DC3 WORSE: char* walk + inline ratios, 99.89->99.84, reverted |
| `Hmx::operator*` | rndobj/Cam.cpp | 688 | I1 | NO_QNAME | 85.61 | 85.61 | NO_QNAME, hand-compared: DC3 Dot3/Dot3ZSeed form 85.61->19.97, reverted |
| `EventTrigger::Replace` | rndobj/EventTrigger.cpp | 636 | P1 | DIFF | 99.87 | 99.87 | DC3 newer ObjRef revision (bool Replace/SetObj); not taken |
| `Strand::SetRoot` | char/CharHair.cpp | 596 | P1 | DIFF | 99.93 | 99.93 | DC3 WORSE: swap compiled, 99.93->96.26 in batch, reverted |
| `Vector3Keys::SetFrame` | rndobj/PropKeys.cpp | 584 | I2 | DIFF | 98.42 | 99.28 | RAISED (codegen): DC3 nextIdx order; DC3 3-arg signature is API |
| `RndTexBlender::DrawBlendList` | rndobj/TexBlender.cpp | 548 | P1 | DIFF | 99.45 | 99.45 | DC3 API only (BaseMaterial); retail charge is a pure register rotation |
| `SetBloomBlurWeightsStreak` | rndobj/Utl.cpp | 540 | I1 | DIFF | 95.42 | 95.42 | DC3 WORSE: swap compiled, 95.42->95.41 in batch, reverted |
| `CharDriver::OnGetClipOrGroupList` | char/CharDriver.cpp | 532 | P1 | DIFF | 99.92 | 99.92 | DC3 API (SortNodes(int)); residual is one commutative add |
| `FileMerger::MergeAction` | char/FileMerger.cpp | 520 | P1 | DIFF | 99.92 | 99.92 | DC3 WORSE: swap compiled, 99.92->80.83 in batch, reverted |
| `VertArray::AppendWeights` | rndobj/MeshDeform.cpp | 516 | P2 | DIFF | 98.29 | 100.00 | FIXED (codegen): MemResizeElem insert via end(), DC3 spelling |
| `CharForeTwist::Poll` | char/CharForeTwist.cpp | 504 | I1 | DIFF | 89.33 | 89.33 | DC3 WORSE: swap compiled, 89.33->83.32 in batch, reverted |
| `QuatKeys::SetFrame` | rndobj/PropKeys.cpp | 500 | P1 | DIFF | 99.92 | 99.92 | DC3 diff is the 3-arg signature only (API); register-only |
| `NgLight::SetShadowTransforms` | rndobj/Lit_NG.cpp | 460 | P1 | DIFF | 99.99 | 99.99 | DC3 WORSE: swap compiled, 99.99->99.92 in batch, reverted |
| `FixVertOrder` | rndobj/Utl.cpp | 448 | P1 | DIFF | 99.91 | 99.91 | DC3 WORSE: swap compiled, 99.91->81.26 in batch, reverted |
| `SIVideo::Load` | rndobj/SIVideo.cpp | 444 | P1 | SAME | 99.91 | 99.91 | SAME as DC3: residual not in the body text; left |
| `FileMergerOrganizer::Init` | char/FileMergerOrganizer.cpp | 444 | P1 | SAME | 99.91 | 99.91 | SAME as DC3: residual not in the body text; left |
| `RndGenerator::SetFrame` | rndobj/Gen.cpp | 440 | P1 | SAME | 99.91 | 99.91 | SAME as DC3: residual not in the body text; left |
| `RndFlare::CalcRect` | rndobj/Flare.cpp | 428 | P1 | DIFF | 99.81 | 99.81 | DC3 WORSE: swap compiled, 99.81->65.85 in batch, reverted |
| `Bloom_Blur` | rndobj/PostProc_NG.cpp | 412 | P1 | DIFF | 99.90 | 100.00 | FIXED (behaviour): param roles swapped vs retail; DC3 order taken |
| `Rnd::SetupFont` | rndobj/Rnd.cpp | 400 | P1 | SAME | 99.90 | 99.90 | SAME as DC3: residual not in the body text; left |
| `GatherObjectsFromGroup` | rndobj/AmbientOcclusion.cpp | 396 | P1 | NO_QNAME | 99.85 | 99.85 | NO_QNAME, hand-compared: DC3 diff is its IsWorldInstance helper; charge is three RTTI relocations to unnamed retail labels + register |
| `RndText::GetStringWidthUTF8` | rndobj/Text.cpp | 392 | P1 | OURS_ONLY | 99.95 | 99.95 | OURS_ONLY: DC3 file lacks the definition; left |
| `CharBonesMeshes::AcquirePose` | char/CharBonesMeshes.cpp | 388 | P1 | DIFF | 99.79 | 99.79 | DC3 WORSE: char* walk hand-ported, 99.79->99.59, reverted |
| `RndBitmap::Create` | rndobj/Bitmap.cpp | 380 | P1 | PARTIAL | 99.79 | 99.79 | extractor paired the wrong overload; row is register-only |
| `operator>>` | synth/MidiSynth.cpp | 380 | P2 | NO_QNAME | 97.89 | 97.89 | NO_QNAME, hand-compared (world/Dir.cpp): DC3 diff is stripped-macro choice only; charge is one bl Find<RndTex> one slot apart |
| `UtilDrawSphere` | rndobj/Utl.cpp | 368 | I1 | DIFF | 85.15 | 100.00 | FIXED (codegen): DC3 statement order; DC3 RndMat* arg is a newer revision |
| `PropSync<Character::Lod>` | char/Character.cpp | 356 | I2 | NO_QNAME | 96.40 | 96.40 | NO_QNAME, hand-compared: PropSync_p.h template SAME as DC3; retail keeps i+1 in r6 across both calls, ours moves it (two extra mr = the 8 B); register |
| `CharEyes::Enter` | char/CharEyes.cpp | 356 | I1 | DIFF | 88.09 | 100.00 | TO 100 (codegen): DC3 body adopted |
| `BuildSphereStratified` | rndobj/Utl.cpp | 340 | P1 | DIFF | 99.76 | 99.88 | RAISED (codegen): DC3 body adopted |
| `CharIKFingers::MeasureLengths` | char/CharIKFingers.cpp | 332 | I2 | DIFF | 97.17 | 98.98 | RAISED (codegen): DC3 body adopted |
| `AddMotionSphere` | rndobj/Utl.cpp | 324 | P1 | OURS_ONLY | 99.88 | 99.88 | OURS_ONLY: DC3 file lacks the definition; left |
| `VertArray::CopyVert` | rndobj/MeshDeform.cpp | 324 | P1 | OURS_ONLY | 99.88 | 99.88 | OURS_ONLY: DC3 file lacks the definition; left |
| `RndTransformable::WorldXfm_Force` | rndobj/Trans.cpp | 320 | P1 | DIFF | 98.81 | 98.81 | DC3 WORSE: swap compiled, 98.81->71.10 in batch, reverted |
| `GatherObjectsFromDir` | rndobj/AmbientOcclusion.cpp | 316 | P1 | NO_QNAME | 99.87 | 99.87 | NO_QNAME, hand-compared: DC3 diff is its IsWorldInstance helper for our dynamic_cast; charge is RTTI-label + register |
| `CharBonesSamples::Print` | char/CharBonesSamples.cpp | 308 | P1 | DIFF | 99.55 | 99.55 | DC3 WORSE: swap compiled, 99.55->84.34 in batch, reverted |
| `RndFlare::CalcScale` | rndobj/Flare.cpp | 304 | P2 | SAME | 95.33 | 95.33 | SAME as DC3: residual not in the body text; left |
| `CharBone::StuffBones` | char/CharBone.cpp | 268 | P1 | SAME | 99.70 | 99.70 | SAME as DC3: residual not in the body text; left |
| `RndMesh::CollidePlane` | rndobj/Mesh.cpp | 264 | P1 | SAME | 99.85 | 99.85 | SAME as DC3: residual not in the body text; left |
| `TestTextureSize` | rndobj/Utl.cpp | 248 | I2 | DIFF | 97.82 | 97.82 | DC3 WORSE: swap compiled, 97.82->71.24 in batch, reverted |
| `ProcCounter::SetEmulateFPS` | rndobj/PostProc.cpp | 232 | P2 | SAME | 96.55 | 96.55 | SAME as DC3: residual not in the body text; left |
| `SongSectionController::Load` | char/CharIKFingers.cpp | 232 | P1 | OURS_ONLY | 99.40 | 99.40 | OURS_ONLY: DC3 file lacks the definition; left |
| `RndOverlay::Init` | rndobj/Overlay.cpp | 224 | P1 | SAME | 99.82 | 99.82 | SAME as DC3: residual not in the body text; left |
| `CharServoBone::MoveToDeltaFacing` | char/CharServoBone.cpp | 216 | P1 | SAME | 99.63 | 99.63 | SAME as DC3: residual not in the body text; left |
| `RndShaderMgr::AllocShader` | rndobj/ShaderMgr.cpp | 192 | I1 | DIFF | 82.60 | 82.60 | DC3 WORSE: swap compiled, 82.60->76.25 in batch, reverted |
| `RndTransformable::OnCopyLocalTo` | rndobj/Trans.cpp | 188 | P1 | SAME | 99.15 | 99.15 | SAME as DC3: residual not in the body text; left |
| `list<RndMultiMesh::Instance>::operator=` | rndobj/MeshAnim.cpp | 180 | P1 | NO_QNAME | 99.11 | 99.11 | NO_QNAME: STLport list::operator=, header shared with DC3; register-only |
| `NgPostProc::DoVelocity` | rndobj/PostProc_NG.cpp | 180 | I1 | PARTIAL | 95.56 | 95.56 | DC3 adds TheHiResScreen test (newer); retail charge is one global load one slot late, target unnamed (lbl_82C76CE0) |
| `RndAmbientOcclusion::DistanceSH` | rndobj/AmbientOcclusion.cpp | 176 | I1 | DIFF | 73.61 | 97.27 | RAISED (codegen): DC3 body adopted |
| `RndFont::~RndFont` | rndobj/Font.cpp | 156 | P2 | SAME | 94.87 | 94.87 | SAME as DC3: residual not in the body text; left |
| `RndMesh::BurnXfm` | rndobj/Mesh.cpp | 140 | P1 | SAME | 99.29 | 99.29 | SAME as DC3: residual not in the body text; left |
| `Rnd::OnToggleHeap` | rndobj/Rnd.cpp | 124 | I1 | DIFF | 93.55 | 96.77 | RAISED (codegen): DC3 body adopted |
| `Rnd::CompressTexture` | rndobj/Rnd.cpp | 124 | P2 | DIFF | 93.39 | 93.39 | DC3 WORSE: swap compiled, 93.39->34.29 in batch, reverted |
| `RndTransformable::SetWorldXfm` | rndobj/Trans.cpp | 124 | P2 | DIFF | 93.39 | 93.39 | DC3 FLAT: swap compiled, row unmoved, not kept |
| `NormalizeScale` | char/CharEyes.cpp | 120 | P1 | NOT_FOUND | 99.33 | 99.33 | NOT_FOUND in either tree by the extractor (file-static helper); left |
| `operator<<(BinStream&, vector<CtrlPoint>)` | rndobj/Spline.cpp | 100 | I2 | NO_QNAME | 87.52 | 87.52 | NO_QNAME, hand-compared: BinStream.h template SAME as DC3; left |
| `RndXfmCache::GetXfms` | rndobj/VelocityBuffer.cpp | 96 | P2 | DIFF | 83.12 | 83.12 | DC3 WORSE: swap compiled, 83.12->73.96 in batch, reverted |
| `BoneDesc::operator=` | rndobj/MeshDeform.cpp | 96 | P1 | NO_QNAME | 99.17 | 99.17 | NO_QNAME: compiler-generated; BoneDesc members identical to DC3 (ours adds a non-virtual decl); register-only |

## 5. What is left, and what I did not do

- **56 rows stay open.** Every one is either register or scheduling class by its charges, or a DC3 change that is
  newer API or a different revision. None carries an unexplained immediate or callee:
  - `AllocShader`'s immediates are field offsets from a store order (retail stores count, alloc, then pool). DC3's
    reorder of that statement measured worse (82.60 → 76.25).
  - `NgPostProc::DoVelocity`'s only charge is one global load one slot late. Its retail target `lbl_82C76CE0` is
    unnamed.
  - The `GatherObjects*` symbol charges are RTTI descriptors at unnamed retail labels.
- **I did not grind codegen residue** past the DC3 comparison: no permuter, no decl-order sweeps.
  `RndScaleObject` (3,112 B, 89.58) is the largest row left, and DC3's body for it scored 87.58.
- **No map, splits, alias or `symbols.txt` edits.**
- **Native gate:** `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.
- Scripts and the per-row table generator are in `~/tmp/w16tg/` and are not committed: `dispo.py`, `showpair.py`,
  `pairs/*.diff`, and `report_*.json` per build.
