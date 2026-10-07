# W16-SF: the unopened VIA-DC3 source-level gap rows

Lane W16-SF, 2026-10-07, worktree `~/tmp/wt-w16sf`, branch `w16-sf`, base main `351d4bf21`.
Brief: lever 1 of `CAMPAIGN_STATE_2026-10-07.md`, the 82 VIA-DC3 rows / 32,476 B that no lane
had opened, mostly in `rndobj`. Fix what retail's bytes show is wrong, using DC3's source where
it agrees with retail. Done when every row is at 100 or has a recorded reason.

Ruler: the shipped graded `name_check`, read from `report.json`. Per-row numbers come from
the worktree's own `report.json` after a full `tools/ninja-locked`, compared against a
main-baseline `report.json` taken at the start. The whole-binary price is in §4, from
`tools/ab_measure.py`.

## 1. Result

- **13 of 82 rows at 100** (3,240 B), against 0 at the start.
- **15 further rows moved up**.
- **0 rows moved down** in the whole binary over the per-row comparison.

Several of the fixes are behaviour fixes, not codegen. The retail bytes show the old code did
something different:

| row | what was wrong |
|---|---|
| `RndBitmap::PixelOffset` | the 8-bpp path used the wrong nibble tables, and the DXT block width was wrong; DC3's body agrees with retail |
| `TransformKeys` | multiplied the scale keys by `v48.x` on every axis; retail scales component-wise, as in `Scale(it->value, v48, it->value)` (DC3) |
| `Rnd::TestPoint` | an early return on `TheHiResScreen.IsActive()` that retail does not have (retail reads `RndCam::sCurrent` first). Both the DC3 source and the RB3 game source carry that test, so this is a TU5 difference. The non-native path also returned the wrong way. Our `HX_NATIVE` point-tester hook is kept. |
| `SetBloomBlurWeights` | constants went to RB3's pixel-shader slots (`0x1f+i`, `0x2f+i`) from static const tables; the old file-scope mutable tables are gone |
| `CharClip::LockAndDelete` | now `delete toDelete` (DC3) |
| `EventTrigger::UnregisterEvents` | called `Hmx::Object::RemoveSink`; retail `dynamic_cast`s `Dir()` to `MsgSource` and calls `MsgSource::RemoveSink`, the mirror of `RegisterEvents` |
| `vector<Triangle>::_M_erase`, `vector<SpotMeshEntry>::_M_erase` / `_M_fill_insert_aux` (hand-written) | the tail count was `(ptr - ptr) / sizeof(T)`. A pointer difference is already an element count, so only 1/64 (Triangle) or 1/80 (SpotMeshEntry) of the tail was moved. Retail does one signed `srawi.`/`divw.` |
| `Plane(point, normal)` (`math/Mtx.h`) | `d` now reads `normal`, not the `a/b/c` it just stored. This takes `CharCollide::Highlight` 96.32 → 99.98; no other row moved |

Codegen-shape fixes:

| row | change |
|---|---|
| `TexBlender::DrawShowing` | ObjPtr ternary residue |
| `RndText::ParseMarkup` | the cursor is the parameter |
| `RndPropAnim::FindKeys` | direct `mProp` read |
| `RndLine::UpdateLinePair` | retail statement order, out-of-line `Normalize`, and Add/Subtract helpers |
| `CharDriverMidi` | no user-declared destructor; retail's scalar deleting destructor calls `~CharDriver` directly and has no `??1CharDriverMidi` |
| `CharClipGroup::MakeMRU` | stores `which + 1` instead of re-reading `mWhich` |
| `RndGroup::AddObjectAtFront` | break-out membership loop |
| `BoxMapLighting::ApplyLight<Point>` | `dir` written componentwise and re-read for `distSq` |
| `RndText::ComputeCharWidths` | `u7 = us68` placed after the key store |
| `MakeNormals`, `UtilDrawCigar`, `CharEyes`, `CharGuitarString::Poll`, `CharPollableSorter::AddDeps`, `Waypoint::ShapeDeltaBox`, `BlendVert`, `RndShaderMgr::UpdateCache`, `RndXfmCache::CacheXfms` | current DC3 bodies, each kept only where the row went up |

## 2. Per-row disposition

Column key: `B` = fuzzy before, `A` = fuzzy after (graded). Disposition classes:

- **FIXED**: the row is at 100.
- **PARTIAL**: a source fix was applied; the reason given is what remains.
- **FOLD**: retail's body at this address belongs to a different instantiation or type. The
  element stride, the callee or the argument shape proves it. This is an identification or ICF
  fold matter, and no source edit can close it.
- **SCHED**: what remains is register, FP-load-order or scheduling only. No insert or delete
  names a source construct.
- **RECORDED**: an existing in-tree or DC3 record already covers the residual.

| # | row | size | B | A | disposition |
|---|---|---:|---:|---:|---|
| 00 | RndTexBlender::DrawShowing | 1880 | 97.62 | 98.79 | PARTIAL. Size now equals retail; the remainder is register-only |
| 01 | BuildVisit | 1344 | 93.26 | 93.26 | SCHED/EH. Source is identical to DC3. Retail is +44 B of repeated `stw rX,0x60(r31)` frame-temp spills before each list push (EH-region object spills); we construct the temporaries without them |
| 02 | MakeNormals | 1332 | 89.79 | 99.87 | PARTIAL. DC3 body; four FP load/store order swaps remain |
| 03 | RndMeshDeform::Reskin | 1232 | 95.98 | 95.98 | SCHED. Member load-order swaps, one `extsw`, and a loop-bound compare; no named construct. DC3 body differs only in non-codegen spelling |
| 04 | RndTransformable::ApplyDynamicConstraint | 1224 | 98.28 | 98.28 | SCHED. Moving the z reference is inert |
| 05 | TessellateMesh | 1176 | 87.60 | 87.60 | SCHED. DC3's body regressed the row, so ours is kept |
| 06 | RndLine::UpdateLinePair | 1076 | 68.52 | 76.14 | PARTIAL. We keep pt2 in volatile r7 and retail in callee-saved r28, which is a callee-clobber difference (cf. W16-ER). Reordering the definitions is inert |
| 07 | RndAmbientOcclusion::CalculateAO | 984 | 99.83 | 99.83 | SCHED. DC3 body regressed |
| 08 | kdTreeNode::Pack | 972 | 89.62 | 89.62 | SCHED/EH. Retail spills temporaries to `0x54(r31)`, the same EH-temp shape as 01; the rest is load order |
| 09 | RndText::ParseMarkup | 924 | 97.90 | 100 | FIXED |
| 10 | UtilDrawCigar | 872 | 89.35 | 97.22 | PARTIAL. DC3 body; FP register and stack-slot order remain |
| 11 | RndBitmap::PixelOffset | 820 | 74.41 | 98.88 | PARTIAL. Behaviour fix (DC3); one `divwu` is scheduled one slot apart |
| 12 | RndLight::Projection | 764 | 89.01 | 89.01 | SCHED. Same size, matrix-temp stack slots `0x50`/`0xb0` swapped, and FP load order (`/fp:fast`) |
| 13 | CharCollide::Highlight | 656 | 96.32 | 99.98 | PARTIAL. Plane ctor fix; four FP load-order swaps remain |
| 14 | RndAmbientOcclusion::CalculateAOAtPoint | 608 | 99.87 | 99.87 | SCHED |
| 15 | RndShaderPostProc::CalcShaderOpts | 572 | 88.22 | 88.22 | SCHED. Same size; the order of the `lbz`/`rldimi` bit-pack is permuted. DC3 body regressed |
| 16 | RndAmbientOcclusion::BlendVert | 544 | 63.39 | 85.83 | PARTIAL. DC3 body; FP add and store order remain |
| 17 | kdTreeNode::FindSplit_Mean | 500 | 95.81 | 95.81 | SCHED. Retail re-extracts the 2-bit axis field from the raw word (`clrlwi`+`clrlslwi`) where we reuse it; `rlwimi` operand roles |
| 18 | RndText::AddLineUTF8 | 500 | 99.12 | 99.12 | SCHED. One branch-destination arg |
| 19 | Waypoint::ShapeDeltaBox | 496 | 99.08 | 99.12 | PARTIAL. DC3 body; FP load order remains |
| 20 | Merger::Clear | 480 | 99.00 | 99.00 | SCHED. Retail compares on cr0 (`cmplwi r28,0`) where we use cr6 |
| 21 | TransformKeys | 448 | 84.65 | 90.80 | PARTIAL. Behaviour fix (DC3 `Scale` operand); member reload and FP order remain |
| 22 | RndMesh::LoadVertices | 444 | 94.53 | 94.53 | SCHED. `slwi` hoist and member load order. DC3's body does not compile against our Mesh API |
| 23 | RndMesh::SkinVertex | 436 | 99.25 | 99.25 | SCHED. Register-only |
| 24 | RndParticleSys::UpdateRelativeXfm | 412 | 99.96 | 99.96 | RECORDED. DC3 w21-bf scheduling residual |
| 25 | CharLipSync::PlayBack::Set | 408 | 88.88 | 88.88 | SCHED. Retail spills the prop-anim pointer to `0x50` twice and places the String destructor after the next `addi`; we spill the list iterator instead. DC3's function is a different, later algorithm, so it is no reference for this row |
| 26 | RndScreenMask::DrawShowing | 404 | 91.64 | 91.64 | SCHED. Colour store order; DC3 body regressed |
| 27 | vector<SpotMeshEntry>::_M_fill_insert_aux | 396 | 97.12 | 98.59 | PARTIAL. Behaviour fix (count); an r28/r31 swap remains, because retail keeps `old_finish` as the copy destination where we re-derive it from `src + n` |
| 28 | kdTreeNode::FindSplit_SAH | 388 | 97.99 | 97.99 | SCHED. Same bit-field `rlwimi` shape as 17 |
| 29 | CharGuitarString::Poll | 376 | 96.84 | 100 | FIXED (DC3) |
| 30 | CharClip::Transitions::AddNode | 368 | 98.80 | 98.80 | SCHED. Retail forms `resized + size*8` twice (no CSE). Spelling the source as `&nodes[size]` regressed to 94.35 and was reverted |
| 31 | BoxMapLighting::ApplyLight<Spot> | 360 | 96.72 | 96.72 | RECORDED. DC3's w7-at/w7-bl residual (register permutation plus one store) |
| 32 | RndText::ComputeCharWidths | 360 | 83.39 | 85.61 | PARTIAL. Retail's markup zero-fill is a counted loop storing `f31` (`stfsu`); ours becomes a `ctr` loop of integer stores. There is no DC3 counterpart |
| 33 | KerningTable::SetKerning | 352 | 99.98 | 99.98 | SCHED. Swapping the xor operands is inert |
| 34 | RndText::UpdateMesh | 340 | 98.18 | 98.18 | SCHED. Retail leaves a `clrrwi rX,rX,0` (store-to-load forward residue) |
| 35 | BoxMapLighting::ApplyLight<Point> | 332 | 81.27 | 87.06 | PARTIAL. Store and register order of the direction triple remain |
| 36 | Rnd::TestPoint | 332 | 85.52 | 97.47 | PARTIAL. Behaviour fix; register order remains |
| 37 | vector<Key<vector<Vector3>>>::_M_insert_overflow_aux | 328 | 93.50 | 93.50 | FOLD. Retail stride 0x50, ours 0x10 |
| 38 | vector<CharHair::Point>::_M_insert_overflow_aux | 328 | 93.26 | 93.26 | FOLD. Retail stride 0x3c, ours 0x80 |
| 39 | BoxMapLighting::CacheData | 316 | 80.00 | 80.00 | SCHED. Retail reloads members after the colour test; the 2.0 constant is at the non-COMDAT `0x820F3A40` (forgiven placeholder) |
| 40 | RndGroup::AddObjectAtFront | 288 | 90.21 | 91.60 | PARTIAL. Loop shape fixed. The remaining charges are the PoolAlloc / `ObjPtrList<Task>::Link` / `vector<Object*>::_M_fill_insert` names, which are ICF fold survivors, plus one saved register |
| 41 | EventTrigger::UnregisterEvents | 284 | 82.89 | 100 | FIXED (behaviour) |
| 42 | kdTree<Triangle> ctor | 264 | 98.48 | 98.48 | SCHED. We have one extra `addi`, an address rematerialization |
| 43 | RndText::RotateLineVerts | 260 | 97.55 | 97.55 | SCHED. FP load order |
| 44 | CharCollide::GetRadius | 260 | 98.49 | 98.49 | SCHED. FP load order |
| 45 | RndBitmap::DxtColor | 256 | 97.19 | 97.19 | SCHED. DC3 body is byte-identical in effect |
| 46 | SetBloomBlurWeights | 248 | 88.10 | 100 | FIXED (behaviour) |
| 47 | CharClip::LockAndDelete | 244 | 87.33 | 100 | FIXED (DC3) |
| 48 | CharClipGroup::MakeMRU | 232 | 87.59 | 100 | FIXED |
| 49 | RndXfmCache::CacheXfms | 216 | 79.52 | 95.28 | PARTIAL. DC3 body; the pointer-cursor base differs (`subi` from r10 vs r3) |
| 50 | CharEyes::EnforceMinimumTargetDistance | 216 | 97.46 | 100 | FIXED (DC3) |
| 51 | CharClip::Transitions::Resize | 200 | 98.00 | 98.00 | SCHED. One extra stack store before `MemRealloc` |
| 52 | CharPollableSorter::AddDeps | 200 | 80.46 | 100 | FIXED (DC3) |
| 53 | RndAmbientOcclusion::TransformNormal | 184 | 94.37 | 94.37 | SCHED. DC3 body regressed |
| 54 | TransformNormal (Mesh) | 164 | 94.66 | 94.66 | SCHED. FP load order and stack slots |
| 55 | RndFont::Kerning | 160 | 94.12 | 94.12 | SCHED (callee clobber). We hold `this` in volatile r7 across `KerningTable::Find`, so our build knows Find's clobber set. Retail saves r31 and treats Find as opaque. Find is byte-identical on both sides and precedes Kerning in both, so the mechanism is not source text in these rows |
| 56 | EventTrigger::CleanupEventCase | 140 | 88.43 | 88.43 | SCHED. Retail spills `&*it` to the frame; DC3 body regressed |
| 57 | operator>>(BinStream&, ObjectStage&) | 132 | 79.39 | 79.39 | RECORDED trade-off. Retail inlines this one `ObjPtr<ObjectDir>` ctor. `RB3_OBJPTR_FORCEINLINE_CTOR` takes the row to 100 but drops `operator<<` (→63.94) and `ObjectKeys::SetToCurrentVal` (→71.88) in the same TU, net −132 B, so it was not landed. Passing a variable instead of `nullptr` does not inline it |
| 58 | vector<Key<vector<Vector2>>>::resize | 128 | 88.44 | 88.44 | FOLD. Retail stride 0x24 (callee `_M_erase<PracticeSection>`), ours 0x10 |
| 59 | vector<IKTarget>::resize | 128 | 88.59 | 88.59 | FOLD. Retail stride 0x1c (`_M_erase<Character::Lod>`) |
| 60 | RndPropAnim::FindKeys | 128 | 98.12 | 100 | FIXED |
| 61 | vector<Key<Weight>>::resize | 124 | 48.06 | 48.06 | FOLD/inline. Retail inlines `erase` (calls folded `_M_erase<IKTarget>`) where we tail-call `erase`; this is an STLport inline decision on a shared template |
| 62 | RndConsole::MoveLevel | 112 | 67.68 | 67.68 | RECORDED. DC3 w9-c measured ten spellings: a `Clamp` store-forward that the image's build left in place |
| 63 | RndShaderMgr::UpdateCache | 112 | 98.71 | 100 | FIXED (DC3) |
| 64 | vector<Key<vector<Vector3>>>::_M_fill_insert | 112 | 88.50 | 88.50 | FOLD. Retail stride 0x50 (`_M_fill_insert_aux<PatchLayer>`) |
| 65 | vector<RndSpline::CtrlPoint>::_M_erase | 112 | 99.82 | 99.82 | FOLD. Retail stride 0x24, ours 0x58 |
| 66 | vector<IKTarget>::_M_fill_insert | 112 | 88.50 | 88.50 | FOLD. Retail stride 0x1c (`_M_fill_insert_aux<Lod>`) |
| 67 | MatPerfSettings::Load | 104 | 95.96 | 95.96 | RECORDED. W17-MAT in-source co-addressing record |
| 68 | __destroy_range_aux<Key<vector<Vector3>>> | 100 | 99.92 | 99.92 | FOLD. Retail stride 0xc, ours 0x10 |
| 69 | __uninitialized_fill_n<Key<vector<Vector3>>> | 96 | 99.88 | 99.88 | FOLD. Retail stride 0x50 |
| 70 | __uninitialized_fill_n<pair<DataArray*,DataNode>> | 96 | 99.88 | 99.88 | FOLD. Retail stride 0x20, ours 0xc |
| 71 | __uninitialized_copy<pair<DataArray*,DataNode>> | 96 | 99.83 | 99.83 | FOLD. Retail stride 0x20 |
| 72 | vector<SpotMeshEntry>::_M_erase | 96 | 87.29 | 100 | FIXED (behaviour) |
| 73 | __uninitialized_fill_n<BoneDesc> | 96 | 86.00 | 86.00 | FOLD (misidentified). Retail's body takes (first, last, dest), loops `first != last` and calls `_Copy_Construct`; it is `__uninitialized_copy`, not `fill_n` |
| 74 | Key<vector<Vector3>> scalar deleting dtor | 92 | 72.30 | 72.30 | FOLD (misidentified). Retail's body tests bit 0x10 of `+4` and calls a release; it is not a vector destructor |
| 75 | vector<Triangle>::_M_erase | 92 | 95.43 | 100 | FIXED (behaviour) |
| 76 | vector<JumpInstance>::erase | 92 | 21.48 | 21.48 | FOLD (misidentified). Retail stride 0x10 with a trivial inline copy; `sizeof(JumpInstance)` is 0x2c |
| 77 | CharDriverMidi scalar deleting dtor | 88 | 58.14 | 100 | FIXED |
| 78 | RndFont::CharAdvance(GG) | 84 | 88.86 | 88.86 | SCHED (callee clobber). Same mechanism as 55: we keep r5/r6 across `Kerning` |
| 79 | __destroy_range<DistEntry> | 84 | 99.71 | 99.71 | FOLD. Retail stride 0x10, calls `??_G vector<float>` |
| 80 | _Destroy_Range<IKTarget> | 80 | 99.70 | 99.70 | FOLD. Retail stride 0x1c, calls `~Lod` |
| 81 | _Destroy_Range<LocalePanel::Entry> | 80 | 96.70 | 96.70 | FOLD. Retail stride 0x10 (calls `_List_base<OldMatOption>::clear`), ours 0x28 |

## 3. Tried and reverted (negative results)

- **DC3 bodies that regressed** and were reverted:
  - CalculateAO, MoveLevel, CleanupEventCase, TransformNormal (AO)
  - ScreenMask::DrawShowing, CalcShaderOpts, TessellateMesh
  - DxtColor (no change)
- **DC3 bodies that do not compile** against our headers, because of DC3-only APIs and signatures: Mesh, Font, CharLipSync, and four others. These were reverted, not ported.
- **Inert spellings**:
  - SetKerning xor order
  - ApplyDynamicConstraint z-ref placement
  - UpdateLinePair definition order
  - Plane y/z order
- **Regressions**:
  - `RB3_OBJPTR_FORCEINLINE_CTOR` in PropKeys.cpp (row 57, net −132 B).
  - `&nodes[size]` in Transitions::AddNode (row 30, 98.80 → 94.35).

## 4. Whole-binary price

See the commit message of the commit that adds this file for the `ab_measure` legs.

Measured with `tools/ab_measure.py --patch` (branch diff `351d4bf21..177eb8143`, the code commits on this branch) on a worktree
detached at main `351d4bf21`. Both legs settled, graded `name_check` ruler, objdiff-cli
`sha256:c1b7d95240a35cd6`, 1,011 recompiles in leg B:

```
leg A: matched=54755 masked=25203 honest=29552 code%=59.016273
leg B: matched=54769 masked=25203 honest=29566 code%=59.047897
Δmatched=+14  Δmasked_equal=+0  Δhonest=+14  Δcode%=+0.031624pp  Δcode_bytes=+3240
Δfuzzy=+0.009520pp   units at 100% [mpn]: 599 -> 602 (CharDriverMidi, CharGuitarString, TexBlender)
```

The +3,240 B equals the summed size of the 13 rows that reached 100. The 15 partial rows add
fuzzy but no bytes, because `matched_code` is all-or-nothing per row.

The measured patch carried an extra comment clause in `Rnd.cpp` that was later removed by a
history rewrite (a provenance note). That is a comment-only difference.

## 5. Native gate

This was run last, on the final code tree (`177eb8143`):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
