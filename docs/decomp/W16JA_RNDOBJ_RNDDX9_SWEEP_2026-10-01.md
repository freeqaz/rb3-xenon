# W16-JA — rndobj / rnddx9 sweep (2026-10-01)

**Branch** `w16-ja`, rebased onto main `8091a837c` (pre-rebase tip kept as `w16-ja-prerebase`).
**Ruler** `name_check` (graded; read from `report.json` `provenance.diff_config`).
**Not merged to main**, per the brief.

## 0. Result

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-ja-ab --patch <main 8091a837c..w16-ja, docs excluded>`
was run once.

- **Worktree:** fresh, at main `8091a837c`.
- **Diff:** the whole branch at `d3e8f8543`. The two commits after it are the native-only stub file and
  this doc, and neither is a match-build input.
- **objdiff-cli:** pinned to one binary across both legs.
- **Run dir:** `~/tmp/wt-w16-ja-ab/.ab_measure_runs/20261001-113025-branch-895204`.

```
leg A: matched=48315 masked=24035 honest=24280 code%=48.756096  (recompiles: 0, settled)
leg B: matched=48496 masked=24083 honest=24413 code%=49.030983  (recompiles: 1066, split=1, patch_steps=7, settle iterations: 2)
Δmatched=+181  Δmasked_equal=+48  Δhonest=+133  Δcode%=+0.274887pp  Δcode_bytes=+28168
Δfuzzy=+0.437012pp   (legA 57.369328 -> legB 57.806340)
units at 100% [mpn ruler]: legA 318 -> legB 321  (Δ+3; 3 reached 100, 0 fell off)
units at 100% [all-rows-fuzzy ruler]: legA 271 -> legB 271  (Δ+0; 0 fell off)
[control none] Δmatched_code=+30156 B -- NOT_APPLICABLE (source in patch)
```

**No row fell off 100.** Comparing the two legs' reports row by row:

- No row present in both legs dropped from 100, on either the fuzzy ruler or the mpn ruler.
- Seven leg-A rows at 100 have no leg-B row under the same unit and name. Each is at 100 in leg B under
  its corrected name or new unit:
  - `SetKey@FloatKeys` and `Keys<float>::Add` moved to PropKeys;
  - two funclets moved to DepthBuffer3D;
  - `~list<Instance>` is now `DxEnviron::Select`;
  - `~_List_base<ShaderTree>` is now `DxTex::PresyncBitmap`;
  - `UniqueFilename(PBD0)` is now `UniqueFilename(PBD)`.

Across the whole report, **86 rows rose and 3 fell**, none of the three from 100:

- `TexBlender::DrawShowing` 77.01 → 75.94: register reshuffle after the argument-less
  `RndMesh::DrawFaces()`, which matches every retail call site.
- HamMove `_M_fill_insert<OldNodeWeight>` 99.79 → 99.61: a newly named callee exposes a
  relocation-name charge, which is W16-JE's class.
- FilterVersion `Sym@DataArray` 20.75 → 20.44: the same mechanism.

On the same two legs, the 87-unit population's weight went from **9,915,872 to 5,003,820 (−49.5%)**,
and its rows below 100 from **1,196 to 991**.

## 1. Population and ranking

The brief named "the src/system/rndobj and src/system/rnddx9 units". I first selected units whose
**name** contained `rndobj`/`rnddx9`, which gave 11 units and a weight of 2,588,112. That filter was
wrong: most of these units have bare names (`default/Text`, `default/Rnd_Xbox`, `default/Mesh`, …).
Selecting by `objdiff.json` `metadata.source_path` gives **87 units**:

| | weight (Σ size × (100 − fuzzy)) | rows below 100 |
|---|---:|---:|
| main `169512b3e` (start of lane) | 10,152,828 | 1,218 |
| main `8091a837c` (A/B leg A) | 9,915,872 | 1,196 |
| `w16-ja` (A/B leg B) | 5,003,820 | 991 |

The work was split into the lane itself plus five forks, each in its own worktree on disjoint units:

| part | units | weight before → after (fork's own full builds) |
|---|---|---|
| lane | rnddx9 Rnd/ShaderMgr/CubeTex/Movie, rndobj Utl/Rnd/Movie/CubeTex | 2,588,112 → about 0.8 M |
| fork A | Text, Font, Overlay, Flare, Line | 1,647,684 → 716,752 |
| fork B | Rnd_Xbox, RenderState (TexMgr has no unit) | 742,272 → 80,148 |
| fork C | AmbientOcclusion, Mesh, MeshAnim, MeshDeform, Morph | 1,650,288 → 1,324,132 |
| fork D | Shockwave, Anim, PostProc_NG, PostProc, DOFProc_NG, Env_NG, Lit_NG, Cam, Shader | 1,629,760 → 881,244 |
| fork E | the remaining 56 rndobj units (EventTrigger excluded: another branch edits it) | 1,723,844 → 1,200,244 |

Every change, in the lane and in every fork, was measured with a full `./tools/ninja-locked` build
and a whole-report row diff against the previous build. No row fell off 100 at any step.

## 2. Identification method

Anonymous `fn_` rows were named only on retail evidence that does not read the map:

- **Vtable slots.** `tools/vtable_side_by_side.py` joins retail's COL-named table to ours offset by
  offset. A slot is used only when named neighbours on both sides agree (the table is anchored) and the
  retail address has no fold multiplicity.
- **Call edges.** `tools/retail_callers.py` gives the callers; a body is named when its only caller
  calls our function at that point (for example, `Compress` → StartCompress/DoCompress/FinishCompress).
- **Address taken.** Comparators passed to `std::sort`/`list::sort` were found by scanning `.text` for
  `lis`/`addi` pairs that form the function's address.
- **Callee and string sets.** These came from `tools/retail_body.py`.

A name was inserted (always with `tools/gated_map_write.py`) only when the pinned unit's object
defines it. Otherwise the row would just become an unpairable 0. `tools/map_name_injectivity.py`
passed after every edit.

## 3. Lane work (rnddx9 and rndobj Utl/Rnd/Movie)

### 3.1 rnddx9 / ShaderMgr — the DxTex and DxShaderMgr bodies

- **The DxShaderMgr constant-setter family was missing.** We had nine bodies declared but undefined,
  one of them an empty stub. They are now written and named from vtable slots 6–19:
  - five at 100;
  - the Matrix4 pair at 96.4 (one load scheduled across the dirty-mask store);
  - the Vector4 pair at 70.0 (a `srwi`/`rldicl` fusion).
- **`SetPConstant(RndCubeTex*)`** unbinds with a null `D3DDevice_SetTexture` in retail. It does not
  fall back to Rnd's null texture.
- **RB3's DxTex has three movie buffers, not two.** The constructor zeroes 0x8C/0x90/0x94, and
  `SwapMovieSurface` advances `% 3`. That moves `mLockedRect`/`unka4`/`unka8`/`unkac` to
  0x98/0xA0/0xA4/0xA8.
- **No texture registry in RB3.** The ctor and dtor call only RndTex's. DC3's `gAllTextures` debug
  list is kept under `HX_NATIVE`.
- **Bodies written in RB3's shape:**
  - SyncBitmap (1,644 B). It has no TexMgr/CRC path, no lowest-mip substitution, no mip cap, no
    memory-track file names, and a two-way scratch/L8 tail.
  - LockBitmap, UnlockBitmap, Select, ResolveMipChain, MakeDrawTarget, FinishDrawTarget, TexelsLock,
    StartCompress, FinishCompress.
  - The constructor and destructor; ResetSurfaces, GetMovieSurface and SwapMovieSurface.
- **`DX_ASSERT`/`DX_ASSERT_CODE` compile down to evaluating their operand** (`rnddx9/Rnd.h`, house
  guard).
  - Evidence: retail stores every create result untested; `GetMovieSurface` tail-calls
    `D3DTexture_GetSurfaceLevel` straight through `GetSurfaceLevel`'s check; `UnlockBitmap` has no
    `DxRnd::Error` call.
  - Whole-report diff: 12 up, 0 down.
- **`D3DFORMAT_BitsPerPixel`** was declared and never defined. RB3's case table was read off the
  retail compare tree (94.2; the rest is switch-lowering tail merge).
- **`DxShader::Compile`** went 51.0 → 84.7:
  - each shader buffer is created right before its compile, and the compiler writes into `mBuffer`;
  - both compiles share a zeroed parameter block with `TempRegisterLimit = 0x24`;
  - the include data is freed directly.
  - XDK header fix: `D3DXCompileShaderExA`'s out-buffers are `ID3DXBuffer**`, and the parameter
    block is 0x44 bytes, as retail zeroes.
- **`DxRnd::AutoDelete`** frees with the one-argument `PhysicalFree`, as `ReleaseAutoRelease` does
  (86.8 → 100).
- **Map fix.** `0x82735740` is DxTex vtable slot 31, body `b ResetSurfaces`. That is `PresyncBitmap`,
  not `~_List_base<ShaderTree>`: `clear<ShaderTree>` folds to `0x822724E8`, so the real list dtor
  cannot branch here. The old name only read 100 while ResetSurfaces was an unnamed placeholder.

### 3.2 rnddx9 / CubeTex and Movie

- **DxMovie.**
  - The Movie TU is one contiguous block `0x8273E040–0x8273EC04`. Earlier lanes had re-homed only the
    named rows; the CubeTex slivers are gone.
  - 19 rows are named from the DxMovie/DxTex/DxShaderMgr/DxCam/DxCubeTex vtables.
  - SetFrame written (no auto-timer in RB3).
  - `Update` swaps the movie buffer **before** fetching the surface to fill. Ours filled the other
    buffer. Its row pitch is `(width·bpp·4) >> 3` (74.5 → 100).
  - `SetFile`'s stripped "not found" report copies the FilePath by value, right to left
    (`MiloStripEval`; native keeps `MILO_NOTIFY`), 87.4 → 100.
- **DxParticleSys.**
  - Init, DrawParticles and DrawShowing written in RB3's shape: no UV-tile channel, a 0x20-byte
    vertex, only constants 0x31/0x2F/0x30, no NgStats.
  - **`Part.h` gains `NEW_OBJ(DxParticleSys)`.** Without it `REGISTER_OBJ_FACTORY` silently
    registered the inherited `RndParticleSys::NewObject`, so the factory built the wrong class. This
    was a real bug.
- **DxMultiMesh.**
  - DrawBatchedNewGfx is the RB3 batching: a local identity transform, no visibility test, fixed
    0x81-register batches, no stats.
  - DrawShowing has no skinned check.
  - UpdateGeometryBuffers was rewritten (43.1 → 99.9).
- **DxMat.** `0x827410B8–0x82741350` is DxMat code that DepthBuffer3D scatter-includes, so the block
  moved there. All five rows are at 100.
- **DxCam.** SetViewport has no hi-res tiling and uses unsigned conversions (7.9 → 100). Select has
  no gfx-mode gate (75.8 → 97.9).
- **DxCubeTex::Sync.** Each face is uploaded per level through the cube lock (56.2 → 99.96). The old
  body tiled into a null destination and never locked a face.
- **DxRnd.** DrawRectDepth written (100); SetViewport without the `kNewGfx` test (100); Offscreen
  named (89.1).
- **XDK out-parameter wrappers.** Creating through the `IDirect3DDevice9_Create*` inline wrappers
  reproduces retail's forwarded-store `clrrwi r3,r3,0` (found by fork B). The new
  `IDirect3DDevice9_CreateCubeTexture` takes SyncBitmap to 99.76 and DxCubeTex::Sync to 99.96.

### 3.3 rnddx9 / Rnd — the Validator stretch

The first 4.2 kB of the rnddx9/Rnd `.text` pin, `0x827319CC–0x82732A94`, is the RBN/UGC Validator's
MIDI check. The in-tree record (`nobody-class-censused-by-unit-2026-08-14.md`) had left it pinned
because "nothing proves it". The evidence now:

- **49 calls in, all from Validator.** All 49 `bl` call sites into the stretch's 27 functions come
  from the Validator block `0x8272BAC8–0x827319CC` or from inside the stretch.
- **The data references are Validator's too.** The four functions reached only by address are
  referenced from `.rdata` interleaved with Validator's strings.
- **The test can fail.** Run on the rest of the pin, which is genuine DxRnd code, the same test finds
  25 calls from elsewhere and 0 from Validator.

The pin now starts at `0x82732A94`, and the stretch rejoins the `auto_` carve. This is a
reattribution only: matched functions, matched code, total functions and total code are **exactly**
unchanged, and fuzzy moves −0.000012 pp.

### 3.4 rndobj / Utl, Rnd, Movie

- **`CacheResource`** in its retail shape (0 → 38.4):
  - the platform is the constant `kPlatformXBox`;
  - there is no PS3 rewrite and no Holmes round-trip;
  - the buffer is 256 B;
  - `MovieExtension` is kept out of line. That `noinline` is a lever: retail keeps it out of line.

  The block order is not steered by any spelling I tried.
- **`SortPolls`** (`0x82439CB0`; its address is taken in `RndDir::SyncObjects`). RB3 polls
  CharTransCopy objects first, then sorts by name; there is no PollEnabled test (0 → 100). This changes
  the poll order in native too.
- **Two wrong map names corrected:**
  - `0x8240F0C8` was "LocalTalkerIsHeadsetPresent" (an XHV API). Its body and caller make it
    `Rnd::UpdateHeap`. It is now 100, and its caller UpdateOverlay is also 100.
  - `0x824113E0` was `RndLight::Intensity`. It is Rnd-family vtable slot 28 and calls
    `UniqueFilename`, so it is `Rnd::ScreenDumpUnique`.
- **`UniqueFilename` takes one argument in RB3.** Retail's only caller never sets r5, and our body
  ignored its second parameter anyway. The signature, both callers and the map row were renamed
  together, so File stays at 100; ScreenDumpUnique 90.9 → 100.
- **`RndMovie::PreLoad`/`PostLoad`** use RB3's file-static load revision, not a BinStreamRev push/pop
  (32.8 / 8.9 → 100 / 100).
- **Named:** `XfmSort` (address taken in SortXfms) at 96.9, `Rnd::TestPoint` (only caller is in Flare)
  at 85.5.

## 4. Fork work (merged into the branch, `--no-ff`)

- **Fork A — Text/Font/Overlay/Flare/Line.**
  - Text: WrapText, CreateLines, SetupCharVerts, RotateLineVerts and ComputeCharWidths identified from
    call edges.
  - `0x82457790` is `??_DRndText`; the real `??1RndText` is `0x82457278`.
  - The RndText font-chain walks follow `mNextFont` as retail does. UpdateText calls UpdateMesh
    directly (no dirty-mesh set).
  - RB3's two-character `CharAdvance` and `GetTexCoords` replace DC3's fused API.
  - Bug fixed: `SetCharInfo` tested one pixel column, which gave every glyph full-cell width.
  - Left: Overlay's `0x82326DF8–0x82327608` holds LayerDir code that belongs in `bandobj/LayerDir`.
- **Fork B — Rnd_Xbox/RenderState.**
  - RenderState's tail re-homed to Rnd_Xbox.
  - DxRnd slots 30/33/69 written: DrawString, EndDrawing, Suspend.
  - DxMesh draw/fur/copy bodies written.
  - Bugs fixed: InitRenderState now builds RB3's colour-ramp texture; SavePreBuffer cleared with an
    uninitialised alpha; PreInit calls `DOFProc::Init`.
  - Map fix: `0x8273A428` is `New<DxTex>`.
  - One row down (not off 100): TexBlender::DrawShowing 77.0 → 75.9, after RndMesh's slot 0x38 became
    the argument-less `DrawFaces()` that every retail call site uses.
- **Fork C — mesh/AO.**
  - `RndMeshDeform::Reskin` written (it was declared, never defined), plus IsExoBone,
    `BoneDesc::ExportWorldXfm` and a scale helper. **`ScaleEq` is our name**: the body is
    retail-proven, but retail gives no name.
  - Bugs fixed: `kdTree::FindSplit_Mean` overwrote its split axis and used the wrong sample points;
    `BurnTransform` works on local transforms; CalculateAO was missing its editor notification.
  - Three wrong map names corrected.
- **Fork D — post/anim/light.**
  - NgEnviron::Select written in RB3's shape. NgPostProc DoPost/DoBloom/Bloom_Downsample/
    CheckHallOfTime identified from the call order.
  - The `RndShaderVelocity`/`VelocityCamera` Select fold (a 224 B body and a 208 B body) withdrawn
    with a record.
  - DxEnviron::Select named, and `rnddx9/Env.cpp` wired into CubeTex; it was in no TU.
  - RndAnimatable::SyncProperty and RndDir::CollidePlane/Enter toward retail; a wrong DoPost name at
    `0x82360DC8` nulled.
  - Left:
    - a dtk mis-carved Matrix4 product. Our shared `Hmx::operator*` computes b·a where retail computes
      a·b; the fix is header-wide, so it was not touched.
    - ObjList readers declared on `BinStreamRev&`.
- **Fork E — the other 56 rndobj units.**
  - Retail builds no `BinStreamRev` in these Load bodies. The revision is an int or a file-static
    `{altRev, rev}` pair. Twelve Load bodies were rewritten that way; native keeps the old bodies.
  - RndTransformable::Load is an if/else ladder.
  - Two wrong names corrected: SymbolKeys vs BoolKeys `CloneKey`, and RndParticleSysAnim::SyncProperty
    vs the `Key<Color>` printer.
  - Five `.text` re-homes.
  - Left: `0x82461948` is `TexPtr::TexPtr()`, not `??0ObjectStage`; fixing it needs a DeferOwner
    redesign.

## 5. Alias folds

**No new fold was added.** Four memberships were withdrawn, each with a per-membership record. No
group was pruned.

| group | spelling withdrawn | why |
|---|---|---|
| `Replace @ 0x823afe88` | `PreLoad`/`PostLoad@RndMovie$4…CM@` | same-shape thunks with different branch targets (`0x823ae888` vs RndMovie::PreLoad/PostLoad) |
| `CloneKey @ 0x8242c868` | `CloneKey@SymbolKeys` | BoolKeys' clone is its own body at `0x8242c810` (calls `Keys<bool>::Add`) |
| `$?6VColor @ 0x8247ed78` (W16-JE) | `SyncProperty@RndParticleSysAnim` | the address *is* that function; the printer's own body is `0x82461320` |
| fork D: Velocity Select | `Select@RndShaderVelocityCamera` | 224 B vs 208 B bodies cannot be one function |

Each one became a validator contradiction once a correct name landed at the folded spelling's own
address. Final state: `VALIDATE: PASS -- 1614 map-consistent, 261 tolerated, 0 contradicted`.

Rows whose **only** charges are relocation-name arguments belong to lane W16-JE and were left alone.
They include PreInit's `DOFProc::Init` callee name at `0x82466298` (fork B), HamMove's
`_M_fill_insert<OldNodeWeight>` charge (fork C) and DxCubeTex::Sync's unlock name.

## 6. Rebase onto main

The rebase used `git rebase --rebase-merges`, so all five fork merge commits are preserved.
`~/tmp/w16ja/rebase_drive2.py` drove it with resolutions decided in advance; it refuses to continue
while any file holds a conflict marker.

- **`target_symbol_map.json`**: the key-level three-way resolver (`~/tmp/w16id/resolve_map.py`, from
  W16-ID). It refuses on any key that both sides changed differently, and it never refused.
- **`splits.txt`, Shockwave**: main's W16-JD had independently identified the same WorldDir
  ctor/PostLoad and re-homed it to **PropSync**. Fork D had re-homed it to world/Dir. I took main's.
- **`splits.txt`, Wind**: main's new `0x8245D53C` block and fork E's merged `0x8245D8D8–0x8245DD0C`
  block were combined.
- **`ScreenMask.cpp`**: comment-only. Main's wording was kept, with fork E's added note.
- **`Text.cpp`**: the conflicted hunks took fork A's side. One of them is comment-only, but main's
  wording there describes DC3's fused width/advance/UV API, which fork A's code removes. Keeping it
  would have made the comment false, so this is the one deliberate exception to "take main's comment
  wording".

A first attempt was aborted and redone: a mislabelled resolution let a file with markers be staged.
No branch commit contains markers.

## 7. Gates

All four were run on the tip `e83768486`, after the A/B:

```
[map-injectivity] OK: 32589 applied rows, 32588 distinct names, injective (+1 enumerated internal-linkage exception(s))
VALIDATE: PASS -- 1614 map-consistent, 261 tolerated (enumerated above), 0 contradicted, 1876 total
[patch-state] OK: tree is a fixed point of 6 post-compile passes
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```

The native gate first failed 16/18. `native/src/x7_band_stubs.cpp` still stubbed
`RndMeshDeform::Reskin`, which fork C had written, so the symbol was defined twice. The stub was
removed in `e83768486`; native now runs the real reskin.

## 8. Not done

- The permuter was not run. Register-allocation and scheduling residue was skipped after about three
  spellings each. The named examples are the DxTex ctor/MakeDrawTarget/UnlockBitmap dead-address
  rows, the ResolveMipChain prologue hoist, StartCompress's induction anchoring and DxCam::Select's
  `mType` reload.
- No `symbols.txt` edits.
- MakeTangentsLate, TessellateMesh, RndScaleObject, ResetNormals and the other big rndobj/Utl
  geometry rows were not attempted.
- Not merged to main.
