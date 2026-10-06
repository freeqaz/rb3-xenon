# W16-RQ — twenty-two RB3 game-layer partial rows (2026-10-06)

Branch `w16-rq`, base `f22f71f15`. The two `ModifierMgr` rows were worked on
the lane branch. The other twenty were split by source file across six forks
(`w16-rq-a` … `w16-rq-f`), each in its own worktree. The fork branches were
merged into `w16-rq` with `--no-ff`. Every score below is
`fuzzy_match_percent` / `match_percent_normalized` on the shipped `name_check`
ruler. They are read from the A/B leg reports (`legA_report.json.gz`,
`legB_report.json.gz`, run cited at the end), not from fork-local builds.

## Result

**Whole binary (`tools/ab_measure.py --patch`, combined diff `f22f71f15..w16-rq`):
Δmatched +7, Δcode_bytes +2,376, Δcode% +0.023187 pp, Δfuzzy +0.002430 pp,
units at 100 % (mpn) 595 → 598, 0 units fell off.**

Prediction before the run: +2,376 B from the rows that reach fuzzy 100
(588 + 264 + 672 + 280 + 532), plus the 40 B EH funclet `fn_82356CA0` that
completes with InitSmasherPlates. I also predicted +8 functions, and that was
wrong by one. The funclet was already at mpn 100 in leg A (fuzzy 99.5), so it
adds bytes but not a function. The +7 is the five fuzzy-100 rows plus
ToggleFilter and AddUV, which cross on mpn only. The funclet is the only row
outside the list that moved. No row in the binary went down.

## Rows (in brief order)

| # | row | size | before | after | outcome |
|---|---|---:|---|---|---|
| 1 | `VocalTrack::UpdateScrolling` | 8,948 | 96.971 / 97.355 | 97.349 / 97.729 | raised; stuck |
| 2 | `MusicLibrary::Text` | 1,292 | 94.211 / 94.226 | unchanged | stuck |
| 3 | `PerfectSectionTracker::HandleExitExtent` | 1,256 | 94.768 / 95.962 | 97.232 / 98.331 | raised; stuck |
| 4 | `LayerProvider::GetMatForData` | 508 | 88.008 / 89.504 | unchanged | stuck |
| 5 | `OutfitConfig::SetSkinTextures` | 1,192 | 95.074 / 95.493 | unchanged | stuck |
| 6 | `GemManager::SetupGems` | 3,404 | 98.537 / 98.625 | 98.677 / 98.765 | raised; stuck |
| 7 | `BandIKEffector::ComputeHandPullAndQuat` | 444 | 89.198 / 90.009 | 93.009 / 93.685 | raised; stuck |
| 8 | `GemTrackResourceManager::InitSmasherPlates` | 672 | 94.494 / 94.524 | **100 / 100** | done (overturns prior AT_LIMIT) |
| 9 | `VocalTrackDir::SetRange` | 700 | 95.057 / 95.086 | unchanged | stuck |
| 10 | `Tail::UpdateVerts` | 1,064 | 96.797 / 96.985 | unchanged | stuck |
| 11 | `BandFaceDeform::DeltaArray::AppendDeltas` | 584 | 95.069 / 95.890 | unchanged | stuck |
| 12 | `GemTrack::DrawBeatLine` | 684 | 95.848 / 95.906 | unchanged | stuck |
| 13 | `LayerDir::RefreshLayer` | 1,588 | 98.234 / 98.385 | unchanged | stuck |
| 14 | `BandRetargetVignette::Poll` | 280 | 90.114 / 91.400 | **100 / 100** | done |
| 15 | `CustomizePanel::MovePatch` | 264 | 89.697 / 90.909 | **100 / 100** | done |
| 16 | `FingerShape::Update` | 532 | 95.414 / 96.090 | **100 / 100** | done |
| 17 | `BandPatchMesh::MeshVert::AddUV` | 492 | 95.537 / 96.065 | 99.756 / **100** | mpn 100; fuzzy stuck on 3 swaps |
| 18 | `PitchArrow::SyncObjects` | 588 | 96.463 / 96.463 | **100 / 100** | done |
| 19 | `Campaign::Campaign` | 400 | 94.900 / 95.000 | unchanged | stuck |
| 20 | `MusicLibrary::ToggleFilter` | 240 | 91.583 / 91.667 | 99.833 / **100** | mpn 100; fuzzy stuck on 1 swap |
| 21 | `ModifierMgr::IsHidden` | 28 | 28.571 / 28.571 | unchanged | stuck (phantom EH state) |
| 22 | `ModifierMgr::IsActive` | 28 | 28.571 / 28.571 | unchanged | stuck (phantom EH state) |

Five rows are at 100 on both rulers, and two more are at mpn 100. Five rows
were raised but are not at 100, and ten are unchanged. Every row has a
`decomp.db` entry from `report_result`.

## What closed the rows that reached 100

- **PitchArrow::SyncObjects.** This uses the W16-RK `OverdriveMeter` recipe.
  All 17 `mX = Find<T>(...)` lines become `mX.SetObjConcrete(Find<T>(...))`,
  which removes the inline `ObjPtr::operator=` layer that hoisted `mr r3` on
  calls 10 and 11.
- **GemTrackResourceManager::InitSmasherPlates.** This row had a prior
  AT_LIMIT. Adding `#define RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT`, an
  existing `Object.h` TU switch, makes the inlined owner-only `ObjPtr` ctor
  store `mObject` after the vptr, as retail does.
  - `RB3_TU_OBJPTR_DEFER_OWNER` gives the same bytes.
  - The `_EH` variants also close this row, but they drop the
    `GemTrackResourceManager` ctor to 51.8, so they were rejected.
- **CustomizePanel::MovePatch.** The patch now reads `oldUV` and builds a
  fresh `newUV` from `oldX + dx` / `oldY + dy`, instead of adding in place to a
  copy. Its same-size neighbour `ScalePatch` was already at 100 with this
  shape.
- **BandRetargetVignette::Poll.** Two values are now named before use:
  `&TheBandWardrobe->mVignetteNames` as a const reference before
  `FindTarget`, and the `Find<BandIKEffector>` result before `Poll()`.
- **FingerShape::Update.** `int next = i + 1;` is now used for both
  `GetFret(next)` and the contour subscripts.
- **MusicLibrary::ToggleFilter (mpn 100).** `HasFilter` and `AddFilter` go
  through one `SongSortMgr::SongFilter &filter = mTask.filter`, while
  `RemoveFilter` uses `mTask.filter`. That reproduces retail's reload of the
  set address in the remove branch only.
  - **Residue:** one `add` with its operands the other way round. The
    `GetFilter()` spelling fixes that add but moves the reload to the wrong
    branch. Ten spellings were tried.
- **BandPatchMesh::MeshVert::AddUV (mpn 100).** Three changes:
  - the uv delta is a `Vector2`, scaled by `recipsq` and then added, so the
    final adds are not fused;
  - `unk1c` is updated with `+= Vector2(v50x, v50y)`;
  - the projections go through `Dot()`, with `dot4` declared before `dot5`.
  - **Residue:** three commutative operand orders on `x` components. About 12
    spellings were tried.

## Raised but stuck

- **VocalTrack::UpdateScrolling (97.349).** Three changes raised it:
  - the lyric cursor is bound through a reference ternary, giving retail's
    `mr r20, r11` copy and bringing the body to retail's 8,948 B;
  - the front `LyricShift`'s start is read into a local before the window
    select;
  - the deploy tube's length is a local computed before `SetPointPos`.
  - The third change merges our `RangeShift` deque `size()` test into a shared
    tail, so the body is now 8,940 B against 8,948.
  - **Negative, committed then reverted:** a `startMs` local in the drain loop
    (97.112).
  - **Residue:** load order inside STLport's inlined `deque::size()`,
    `Vector3`/iterator copy order, colour-block scheduling, and where
    `part++` lands. This is permuter class.
- **PerfectSectionTracker::HandleExitExtent (97.232).** Declaring the gem-count
  local before the hit-count local gives retail's evaluation order, with the
  hit count in slot 0xb8. W16-ID's 13 spellings had all declared the
  numerator first.
  - **Residue:** we compute the denominator as two adds where retail
    subtracts. The `i118`/`i11c` loads come before the `unk0` store in our
    build and after it in retail's. The r27/r28 numbering also differs.
- **GemManager::SetupGems (98.677).** Declaring `lastArpeggioEndTick` after the
  trill locals moves `inTrill` into retail's slot 0x61.
  - **Residue:** seven compiler spill slots in 0x68–0x90 are in a different
    order, and declaration order does not reach them. Retail also stores
    `gem.mSlots` twice into `rollSlots`' slot. W16-QZ's trainer-clamp and
    dead-`addi` residue remains.
- **BandIKEffector::ComputeHandPullAndQuat (93.009).** Two changes raised it:
  - the elbow matrix is built with one `m.Set(...)` call instead of nine
    stores (89.50);
  - `mEffector->` is read directly instead of through a cached
    `const ObjPtr &` (93.01).
  - **Residue:** retail stores `outQuat.v.y` before computing `dz`, which
    leads to an extra `fmr`. Every source placement of that store regresses
    to between 87.7 and 88.1.

## Unchanged and stuck

- **MusicLibrary::Text.** The size is equal to retail's. All mismatches come
  from one placement decision. The Owned and Store cases end in identical song
  code, and both builds merge the two copies. Retail keeps the merged copy in
  the Owned case and ours keeps it in the Store case. Six variants were tried.
  Same class as W16-RK `GemTrack::DrawFill`.
- **GemTrack::DrawBeatLine.** Both builds share one `Symbol` ctor call between
  the two arrow arms. Retail keeps the copy in the second arm, ours in the
  first. Eleven spellings were each worse or byte-identical. Same class.
- **LayerProvider::GetMatForData.** Retail keeps `zero*scaleHW` in two
  callee-saved FPRs (f24/f25), one of them dead, and ours keeps one.
  - A 9-argument `Matrix3` ctor and a scaled identity local were both
    byte-identical.
  - `tf.Reset()` plus in-place `Scale` dropped to 64.96.
  - Permuter class.
- **OutfitConfig::SetSkinTextures.** Retail indexes `skinMats` by `i` with an
  `i < 5` test. Ours strength-reduces to a pointer plus an end pointer, which
  costs one extra saved GPR. Six spellings across two lanes were tried.
- **LayerDir::RefreshLayer.** Retail stores the colour index to a stack
  temporary just before the second `Symbol("colors")` is built in the same
  slot. That shifts the DataNode temps and swaps r28/r30.
  - Reading `layer.mColorIdx` directly scored 97.577.
  - Open lead: which source construct takes the index's address.
- **VocalTrackDir::SetRange.** The size is equal. One 17-instruction cluster
  remains: `60.0f - min` is scheduled before the dirty-flag load in ours and
  after it in retail's. The flag store comes from `SetTexXfm` in the shared
  `BaseMaterial.h`, which was not touched for one row. Three rewrites were
  byte-identical.
- **Tail::UpdateVerts.** The size is equal. One `add r28` sits in a different
  place. The first face's stores are v1, v3, v2 in ours and v1, v2, v3 in
  retail's, and the face-offset stepping differs. Six rewrites were each
  identical or worse.
- **BandFaceDeform::DeltaArray::AppendDeltas.** `this` and `pos` sit in swapped
  registers (r24/r25), and retail keeps `base`'s data pointer in r11 across
  the inner loop. Fourteen variants were tried.
- **Campaign::Campaign.** Retail is 4 B longer. In the first `AddSink`, it
  copies `Type()`'s result to r10 and loads it late, and the second call has
  one pair of instructions swapped. Spelling out the default arguments was
  byte-identical, and hoisting `Type()` regresses to 88.9.
- **ModifierMgr::IsHidden / IsActive (28.571).** The logic already matches.
  Retail is `li r3,0` / `li r3,1` inside an r31 frame. Ours folds to
  `li r3,N; blr`, and the five missing instructions are that frame.
  - **FuncInfo:** decoded from `band.exe`, all three of these rows plus
    `IsModifierActive(Symbol)` share FuncInfo `0x820A1EF0`. It has
    maxState 1, unwind `{-1, NULL}`, 0 try blocks and **0 IP-to-state
    entries**. That is a phantom state: a state was allocated for an object
    with a destructor, and both the object and its cleanup were then removed.
  - **Corroboration:** retail `ModifierMgr::Handle` has maxState 6, and its
    state 2 is the same `{-1, NULL}` with no IP range. Ours has maxState 5.
    State 2 falls exactly where retail inlines `IsModifierActive(Symbol)`
    (`bl GetModifier; lbz 0x4; subfe`). So the construct travels with the
    inlined `IsModifierActive(Modifier*)` → `IsModifierUnlocked` chain.
  - **Probes:** two probes in `IsModifierUnlocked` (a local with an inline
    empty ctor/dtor, and `DataNode p(1);`) emitted no state at all. The
    construct is still unknown. W16-HX lists the same shape for
    `Campaign::GetLaunchUser` and `SongDB::PostLoad`.

## Files

`src/band3/bandtrack/{GemManager.cpp, VocalTrack.cpp}`,
`src/band3/game/PerfectSectionTracker.cpp`,
`src/band3/meta_band/{CustomizePanel.cpp, MusicLibrary.cpp}`,
`src/system/bandobj/{BandIKEffector.cpp, BandPatchMesh.cpp, BandRetargetVignette.cpp, FingerShape.cpp, GemTrackResourceManager.cpp, PitchArrow.cpp}`.
No headers, maps, aliases, splits or symbols files were touched.

## Provenance

- A/B run: `~/tmp/wt-w16rq-ab/.ab_measure_runs/20261006-233608-w16rq_combined-3040346/`.
  - patch sha256/16: `0238c83a22cd44c1`
  - objdiff-cli sha256: `c1b7d95240a35cd6`
  - leg A: 54,738 / 58.927630 %
  - leg B: 54,745 / 58.950817 %
  - leg B recompiled 23 TUs, split 0.
- Native gate on merged `w16-rq`:
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.
