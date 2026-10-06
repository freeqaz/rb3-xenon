# W16-QQ — the 42 open rows of the W16-QO frame census (2026-10-06)

Lane W16-QQ, branch `w16-qq`, worktree `~/tmp/wt-w16qq`, based on main `ca61b767f`.
The input was the 42 rows of `docs/decomp/w16qo-frame-census.tsv` still below 100 after W16-QO
(`W16QO_FRAME_SIZE_CENSUS_2026-10-06.md`). The brief was to fix our source against retail bytes,
largest prize first. Tessellate (name-blocked) and the five rows QO recorded as inert were skipped.

The work ran in eight forks, each in its own worktree and on its own branch, grouped by source file
so that edits could not collide:

- round 1: `w16-qq-a` rndobj, `-b` char/synth, `-c` os/ui/math, `-d` world/lights, `-e` bandobj/game
- round 2: `-f` bandobj save counts, `-g` save-count rows, `-h` round-1 near-misses

Each fork branch was merged into `w16-qq` with `--no-ff`. Source only: no map, alias or splits
edits, and no permuter.

**Ruler.** `name_check`, objdiff 4.2.9, read from `report.json` `provenance`.

## Result

| | |
|---|---|
| census rows to fuzzy 100 | **17 of 42** (plus 2 more at mpn 100: ConsumeData, BuildFromBSP) |
| all rows crossed (A/B leg diff) | 34 to fuzzy 100: 20 functions + 14 funclets/fragments; 1 row fell (see below) |
| whole-binary A/B | **+35 fns / +9,864 B / +0.096254 pp**; Δhonest +21, Δmasked_equal +14 |
| row-level diff of the A/B legs | **41 rows up, 1 down**, 0 rows on one side only (68,909 rows each) |
| units at 100 (mpn) | 578 → 580 (`CharPollGroup`, `UIListDir`); 0 fell off |
| native gate | `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0` |

**Prediction vs measurement.** Before the A/B I read `report.json` in `~/tmp/wt-w16qq` after a full
build of the merged branch: 54,629 → 54,664 functions and 5,994,476 → 6,004,340 B, so I predicted
**+35 / +9,864 B**. The A/B measured **+35 / +9,864 B**. Round 2 composed exactly on top of round 1:
the forks' own deltas were F +1/+96, G +1/+0 and H +2/+1,936, and the merge read +4/+2,032.

### The one row that fell — a fold-name pairing artifact, not a code regression

`ContentMgr_Xbox`: `list<CharPollableSorter::Dep*>::_M_create_node` (64 B) went from 100 to 0.

- `ContentMgr_Xbox.cpp:593` scatter-includes `char/CharPollGroup.cpp`.
- The SortPolls fix gates `CharPollableSorter`'s members out of that file under `#ifdef HX_NATIVE`,
  because retail has them only in `Character.cpp`.
- Retail's body at `0x82520150`, inside the ContentMgr_Xbox span, is an ICF-folded
  `list<T*>::_M_create_node`. The map names it with the alias-group survivor spelling
  `list<CharPollableSorter::Dep*>` (`scripts/symbol_aliases.json`, group `0x82520150`).
- Before the fix, that exact name was defined in `ContentMgr_Xbox.obj` only because the
  scatter-included sorter code compiled there. objdiff pairs rows **by exact name**; the alias map
  only forgives relocation names. So the row lost its partner, even though `ContentMgr_Xbox.obj`
  still defines `list<Content*>::_M_create_node`, which is a member of the same fold group.

I kept the SortPolls fix. Putting the sorter back into the ContentMgr TU just to supply a name would
be metric fitting: retail compiled it in Character.cpp. **Follow-up for an alias/map lane:** at
`0x82520150`, name the row with a spelling ContentMgr_Xbox.obj defines (`list<Content*>`, same group).
That should recover the 64 B. This lane did not test it, because map edits were out of scope.

## Rows crossed, with cause and lever

| row | before → after | cause | lever | fork |
|---|---|---|---|---|
| `SongSectionController::Handle` (+7 funclets) | 96.91 → **100** | Retail calls `sym.Str()` once and reuses the result in the `prc_` branch; ours called it three times. | Hold it in a local `symStr`. | E |
| `UIListDir::BuildDrawState` | 93.02 → **100** | Element temporaries shared a slot; `SetElementPos` conversion temporaries; then the spill slot 0x54 held `fadeCountEnd` where retail has `numDisplayWithData`. | Empty-slot element declared in the loop body; three `SetElementPos` calls; the clamp written once in each branch, which raises `fadeCountEnd`'s spill cost. | C, H |
| `RndMesh::OnSync` | 94.67 → **100** | Ours kept values in volatile registers across `HasVert`/`FaceCenter`. | `HasVert` made `inline`, plus three statement-order fixes. | A |
| `PatchLayer::Draw` | 87.35 → **100** | The frame was smaller than retail's 0x140. | Position built through a `Vector3(...)` temporary; compare as `GetFrame() != deform`. | E |
| `BandCharDesc::CopyCharDesc` (+1 fragment) | 96.30 → **100** | `Head::operator==` and `Patch::operator==` were `inline`, so they were compiled after the caller. | Remove `inline` so they are compiled first. | E |
| `Spotlight::Poll` | 96.49 → **100** | Retail computes the full-object pointer from the poll sub-object (this+0xd8) once. | `Spotlight *self = this`; route the member calls through it. | D |
| `RndVelocityBuffer::DrawMesh` | 73.19 → **100** | Body shape (DC3's, with RB3's differences), then one `add` scheduled after the `divw`. | Rewrite; `mesh->DrawFaces()` through vtable slot 0x38 as retail calls it; `NumBones` read after the two cache references. | C, H |
| `CharPollGroup::SortPolls` | 87.90 → **100** | `Sort`'s body was visible in the TU, so MSVC hoisted `polls` begin/end out of the loop. | `CharPollableSorter` members are `#ifdef HX_NATIVE` here; Character.cpp keeps the match-build copy. | B |
| `CharHair::Point` / `Strand` ctors (+2 funclets) | 55.96 / 54.67 → **100** | Retail inlines the owner-only `ObjPtr` ctor. | `ObjPtrInlineOwner()` tag at each site; both ctors `inline` so `ObjVector<Point>::resize` keeps retail's saves. | B |
| `ObjVector<EyeDesc>::resize` (+ PropSync, 1 funclet) | 73.90 → **100** | Retail keeps `this`/size in volatile r8/r7 across the `EyeDesc` ctor call. | `EyeDesc` ctor defined non-inline at the top of `CharEyes.cpp` (header keeps the declaration). | B |
| `PatchVerts::Add`, `PatchVerts::HasVert` | 95.03 / 80.67 → **100** | Same mechanism as OnSync, with `GreaterEq` as the callee. | `GreaterEq` made `inline`. | A |
| `set<Edge>` `_M_find`, `insert_unique` (+ `_M_insert`) | 86.23 / 99.16 / 97.91 → **100** | Same mechanism, with `Edge::operator<` as the callee. | Match-build `Edge::operator<` made `inline`. | A |
| `WorldCrowd::CharData` ctor (+ PropSync) | 80.13 → **100** | Retail reuses the `this` returned by the `CharDef` ctor. | Both ctors moved out of line, `CharData`'s before `CharDef`'s. | D |
| `BandPatchMesh::PatchPair` ctor | 45.0 → **100** | Retail inlines `ObjPtr<RndMesh>` but calls `ObjPtr<RndTex>` out of line; a TU-wide force-inline switch inlined both. | Switch removed; `ObjPtrInlineOwner()` at the three sites retail inlines. | F |
| `StandardStream::ConsumeData` | 96.29 → 99.58 (**mpn 100**) | A named `copySize` cost r26 and 0x10 of frame. | memcpy size computed inline; the 0x800 clamp as a ternary. Left: an r27/r28 swap. | G |
| `BuildFromBSP` | 72.39 → 99.65 (**mpn 100**) | Our body shape differed from retail. | DC3's body, with `faceIdx` declared before `vertIdx`. Left: the mesh/faceIdx r25/r26 swap. | A |

### ★ The lever this lane adds: callee compile order decides the caller's register class

On MSVC X360 a caller keeps live values in **volatile** registers across a call to a same-TU
callee only if the callee **was compiled first**, i.e. a non-inline definition earlier in the TU.
If the callee is an inline COMDAT (in-class or `inline`), it is compiled later, so the caller does
not know the callee's register footprint and uses **callee-saved** registers instead. Both
directions are in retail:

- **Retail saves more:** make the callee `inline`. That closed OnSync, HasVert, Add, `_M_find`,
  `insert_unique` and `_M_insert`. Moving `GreaterEq` below HasVert instead was inert, so plain
  definition order is not what matters.
- **Retail saves fewer:** define the callee non-inline before the caller. That closed the
  `EyeDesc` resize and CopyCharDesc. Making the `EyeDesc` ctor `inline` at the same spot left
  resize at 73.90.
- **The lever has side effects.** It can move other callers of the same callee. Fork B's first
  CharHair commit put both ctors at 100 but dropped `ObjVector<Point>::resize` 100 → 56.18; making
  the ctors `inline` recovered it.
- **Where it was inert (round 2):**
  - JoypadPollCommon: moved `TranslateSticksToButs`.
  - Intersect(Triangle,Box): moved `Intersect(Plane,Box)`.
  - FindXfm: moved it before `MeshVert::Normalize`.
  - SphereConeTest and GetMatForData: the lever cannot apply, because their callees are in other TUs.

Two smaller levers:

- **A face index read through a by-value `unsigned short` helper** reproduces retail's
  `mr`-before-`mulli 0x60` copy (ResetNormals). DC3 lane w7-j had recorded that copy as
  unexplained. `(short)` reproduces the copy but loads with `lhax`; the plain casts were inert.
- **A conditional body written in each branch** raises the variable's spill cost and moves
  which variable gets the spill slot (BuildDrawState).

## Attempted, not crossed

| row | now | finding | tried |
|---|---|---|---|
| `RndTexRenderer::DrawToTexture` | 96.66 | Five separate problems. (1) The frame is 0x60 short because ours overlays `m1a8`/`m1cc` on `tf180`/`tf120`. (2) The Vector3×Matrix3 sums are ordered differently. (3) The argument order for the two `__RTDynamicCast` calls differs. (4) The `tf120.v` copy is scheduled differently. | Hoisting `m1a8`/`m1cc` above `v294`: identical bytes. Judged a wall. |
| `CharIKHand::IKElbow` | 94.00 → 97.92 | The FP term order is fixed (frame now retail's 0x280). The schedule of the 16 quaternion products is left. | Named products (frame back to 0x270); named sum temporaries 97.01; z,x,y spelling 97.18; per-component stores 97.96 (treated as noise). |
| `JoypadPollCommon` | 94.21 | Ours keeps 0.0f in f30 for the whole function; retail reloads it at each use. Constant 2 in r14. | `pro_guitar` before `sensors` 93.7; `sensors` in the loop scope (identical); moving the callee (identical). |
| `WorldCrowd::DrawShowing` | 88.68 | The two blocking charges are names: retail resolves `vector<JumpInstance>::erase` and `vector<GemInProgress>::push_back` where ours calls `vector<Hmx::Rect>`. The Rect spellings are in no alias group. | Not attempted; alias work. |
| `NgSpotlightDrawer::SetupXSection` | 76.45 | The by-value copies are the same as retail's; their stack placement differs (Vector4 order left/right/vis at 0xf0..0x110 plus a perp copy at 0x120). | Analysis only. w7-bj and w21-d are stuck at the same place. |
| `ResetNormals` | 86.78 → 99.88 | Left: two operand-order rows in the faceTangents index adds, w-sign `fmadds` order, one crossProd pair, the dot's choice of x as the `fmuls`, and the y/z load order in Negate. | `norm += weighted` 98.11; component adds 98.15; negation spellings 93.99–97.63; six dot orders give the same schedule. |
| `RndParticleSys::MoveParticles` | 70.46 → 98.01 | **Also a behaviour fix:** the bubble wobble and second colour phase read the wrong particle fields. Left: dragFactor/rpmDragFactor f16/f17 swap; retail reloads the just-stored `pos.z`/`vel.z`. | `ScaleAddEq` 97.68; nested plane sum 94.44; Vector4 members 91.88; named 1/30 97.24; `p->pos` update 96.79. |
| `BandPatchMesh::FindXfm` | 75.18 → 77.47 | The gradient rows are now read into axis locals before the calls. Saves are r27–r31/f23–f31 vs retail's r26–r31/f22–f31. Left: uncoalesced `fmr`s and swapped posOut/posMat slots. | Scope block (identical); whole-vector copy 65.88; `sqrtf` 77.47 / `std::sqrt` 77.55 (both leave a double multiply); moving the callee (identical). |
| `NgLight::SphereConeTest` | 60.35 | Retail stores and reloads topPoint/botPoint/cone; ours loads them early into f26–f29. | Evaluation order of `dirBot *= botR`: same score. w17-lit's 200-step reorder failed. |
| `OutfitConfig::SetSkinTextures` | 95.07 | Retail indexes `skinMats` (`slwi`+`lwzx`) each pass; ours steps a pointer, which costs a GPR and 0x10 of frame. | 2D array, `i < 5`, file-scope array (all 95.5 mpn); unsigned index 94.56. |
| `fft_matrix_inverse_columnwise` | 85.96 | Every slot matches except one extra vector spill; this is vector register allocation. | Diagnosis only. |
| `Intersect(Triangle, Box)` | 97.16 | Retail's extra f22/f23 are used only in the nine-axis store block. | `radii[i]`, projecting through `pfAxis` 93.3, `axes[i]` loads 94.0, aggregate initialisers 96.1. |
| `BoxMapLighting::ApplyQueuedLights` | 73.77 | Retail negates each direction component twice and gives each light buffer its own address setup. | `0.0f - x` (identical); non-static buffers (identical). |
| `RndFont::SetCharInfo` | 78.45 → 89.46 | The frame now matches retail's. Left: which saved register each of this/bmap/pos/top/bottom gets. | Reading the width after `charW` (identical). |
| `LayerProvider::GetMatForData` | 88.01 | Retail keeps a dead copy of `zero*scaleHW` in a second FPR. | `tf.m = scale` 87.2; `Scale(...)` 79.3 / 64.96; commuted products (identical). |
| `SongDB::PostLoad` | 84.36 | Retail sets up an EH frame for an object whose code was entirely removed. | Local struct with an empty dtor, `if (false) { String s; }`, `bool spew=false` (all identical). |
| `UtilDrawPlane` | 99.90 | Retail's induction pointer addresses `.z` first. | Ten Dot-sum spellings, best 99.93, so the source was left as it was. |

`ModifierMgr::IsHidden`/`IsActive` (0x825896c0/e8, 28.57) share PostLoad's EH data and are
probably mis-named. They were noted by fork E and not touched.

## Not attempted

- **Skipped per brief:** `Tessellate` (name-blocked), and the five rows QO recorded as inert:
  `FocusTracker::Poll_`, `NetSession::Poll`, `BandTrack::SetCrowdRating`,
  `Spotlight::UpdateTransforms`, `RndFlare::DrawShowing`. None of their recorded findings is a
  saved-register-count difference, so the compile-order lever has no obvious purchase on them:

  | row | recorded finding |
  |---|---|
  | `FocusTracker::Poll_`, `Spotlight::UpdateTransforms` | slot overlay |
  | `NetSession::Poll` | conditional-temporary flag |
  | `BandTrack::SetCrowdRating` | dead stores from an inlined helper |
  | `RndFlare::DrawShowing` | unused frame with identical slots |
- No map, alias or splits edits; no permuter.

## Files changed

`src/system/rndobj/{Mesh,Utl,Part,Font,VelocityBuffer}.cpp`,
`src/system/char/{CharIKHand,CharPollGroup,CharHair,CharEyes}.cpp`, `src/system/char/CharEyes.h`,
`src/system/world/{Crowd,Spotlight}.cpp`, `src/system/world/Crowd.h`,
`src/system/bandobj/{BandCharDesc,PatchDir,SongSectionController,BandPatchMesh}.cpp`,
`src/system/bandobj/BandPatchMesh.h`, `src/system/ui/UIListDir.cpp`,
`src/system/synth/StandardStream.cpp`. 20 files, +446/−430.

Behaviour-relevant for the native build:

- the MoveParticles field reads (a fix);
- `DrawMesh` now calls `mesh->DrawFaces()` rather than `GetGeomOwner()->DrawFaces()`, as retail
  does. The gate checks that this builds, not that motion blur still draws;
- the native build keeps its own `CharPollableSorter` copy in CharPollGroup.cpp.

## Reproduce

**A/B:**

```
git diff ca61b767f..w16-qq -- src > ~/tmp/w16qq_src.patch
python3 tools/ab_measure.py --worktree ~/tmp/wt-w16qq-ab --patch ~/tmp/w16qq_src.patch
```

- Worktree `~/tmp/wt-w16qq-ab` at main `ca61b767f`.
- Run dir: `.ab_measure_runs/20261006-162902-w16qq_src-827584/`; log `~/tmp/rb3_ab_w16qq.log`.
- Patch kind `['source']`; leg B had 396 recompiles and settled in 2 iterations.

**Native gate:** run on `~/tmp/wt-w16qq` at `d59997bb1`, the final source tree; log
`~/tmp/w16qq_native_gate_final.log`. The round-1 merge was gated separately and also read
PASS 18/18.
