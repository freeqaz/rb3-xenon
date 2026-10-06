# W16-PR — behaviour read of the lever-3 rows (in-scope gap rows in files native does not link yet)

Lane W16-PR, 2026-10-06. Branch `w16-pr`, worktree `~/tmp/wt-w16pr`, base main `dc238f637`.
Brief: lever 3 of [`CAMPAIGN_STATE_2026-10-06.md`](CAMPAIGN_STATE_2026-10-06.md) §5. Read each long-worked in-scope
gap row in a file the native build does not compile yet against retail's bytes **for behaviour only** (what it
computes, calls, stores and returns), fix every behavioural difference, and ignore register, scheduling and layout.

## Result

- **78 rows / 75,112 B, every row has a verdict:** 7 FIXED (15,868 B), 69 behaviour-equal, 1 behaviour-equal up
  to a `/fp:fast` contraction (`MeshVert::AddUV`, 492 B), 1 already closed by W16-PG (`BuildChordMesh`, 1,680 B).
- **Ten behaviour defects in seven rows**, all in RB3 game/bandobj code (list below; `PatchLayer::Draw` carries
  three). **Three of them, in two rows, are invisible to the score**: they are wrong constants that retail loads
  through placeholder `lbl_` labels, which `name_check` forgives, so the row reads the same before and after the
  fix. Two more in `PatchLayer::Draw` are also placeholder-label constants; that row rose only because the fix
  changed its codegen.
- **Whole-binary A/B (one run, all four fix sets combined): +2 fns / +624 B / +0.006092 pp code /
  +0.001675 pp fuzzy; 6 rows up, 0 down** out of 68,909. Predicted beforehand from the forks' own A/Bs as
  +2 / +624 B with exactly those six rows up. It came out exactly.

## Population

The selection reproduces the campaign doc's figure exactly. From `~/tmp/w16pn-gap/dispo_rows.json` (W16-PN's
pipeline), rows with disposition `09 source divergence: opened and left` or `08 I1/I3/I4: worked by W16-PC and
left`, whose `src` is not in `native_src.json`: **78 rows / 75,112 B** (doc: 75,112 B). `band3/net_band`
(RockCentral, ContextWrapper) and `system/os` (AsyncFile_Win, NetworkSocket_Win) are in the set; nothing is in
`src/network` or `src/xdk`. Re-read on this lane's fresh build of `dc238f637`: 76 rows unchanged, and 2
ChordShapeGenerator rows moved through W16-PG's `.bss` fix (`BuildChordMesh` 99.995 → 100, `BuildEndCap`
96.911 → 96.923).

## Method

Six forks, one worktree each, split by subsystem so no two touched the same file. Each read its rows with graded
`run_objdiff` full listings against retail asm, and for each charge asked: is this a different operation, operand,
callee, constant, store target, branch condition or return value, or is it the same operation in another register,
order or block? Codegen spellings were not ground. A fix was kept only if retail's bytes showed the behaviour.
Every fork that changed source ran `ab_measure --from-dirty` and a row-by-row leg comparison.

### The constant instrument, and why it mattered

Fork G1 noticed that a constant loaded through a retail placeholder label (`lbl_XXXXXXXX`) is never charged by
`name_check`, so a wrong float or string reads as equal. It wrote a checker that pairs each retail `lbl_` operand
with our named constant (`__real@…`, `??_C@…`) on the same instruction and compares retail's bytes in `band.exe`.
It was handed to the other forks mid-run, and it found the G1 and G2 defects after G2 had first marked its row
equal.

Two versions were then run over all 78 rows, with a control for each:

| instrument | what it catches | blind to | controls |
|---|---|---|---|
| position-paired (`constcheck2.py`) | wrong value **and** swapped constants | load reordering (false flags) | G3 sabotage (expected XOR 1) flagged 19/46 floats; **0 undecoded** strings over 339 pairs (G3 worried that undecodable strings pass silently; none were undecodable) |
| multiset per function (`constmulti.py`) | wrong value, reorder-proof | **swaps** (same multiset) | on unfixed main: flags `PatchLayer::Draw` and `UpdateScrolling` (250 vs 1000), and reads `DrawBeatLine`'s swap EQUAL, as predicted |

| tree | pairs | position flags | flags explained |
|---|---:|---:|---|
| main `dc238f637` | 340 | 12 | 8 real (G1 ×3, G2 ×2, G5 ×3: Draw ×2, LoadPacked ×1) + 4 reorder artifacts (UIStats ×2, GetMatForData ×2) |
| `w16-pr` merged | 339 | 9 | all reorder artifacts: multiset EQUAL on all 5 flagged rows; the 3 remaining position-flagged rows (Draw, UIStats, GetMatForData) were also value-tracked per register by G5/G6, which rules out a swap |

**Prediction that failed:** I expected only the 4 known artifacts after the fixes. `PatchLayer::Draw` instead went
from 2 flags to 5, because the fix reordered its constant loads. The multiset check resolved it (EQUAL), and G5's
per-register value tracking covers the swap case the multiset cannot see.

Unnamed **call** targets (`fn_…`) were checked for kind in every group. 7 were found, and all match our callee:
the Gem ctor, Find/ObjPtr<RndPartLauncher> ×3 (RTTI), DrawToPlate, GetObjectAsString, ReadFile and a set
`insert_unique` fold.

## The defects (retail evidence, fix)

| row | defect | retail evidence | score Δ |
|---|---|---|---|
| `VocalTrack::UpdateScrolling` | `SongSectionOnly` out-params initialised **inverted** (start=FLT_MAX, end=-FLT_MAX); the inherited source carried the same inversion | start ← `0x82071744` = -FLT_MAX, end ← `0x8201C818` = +FLT_MAX | 0 (placeholder labels) |
| `VocalTrack::UpdateScrolling` | `FreeOldGems(ms - 250)`; retail frees at **ms − 1000** | `0x820010B4` = 1000.0f | 0 |
| `GemTrack::DrawBeatLine` | `key_shift_left.wid` / `key_shift_right.wid` **swapped in both arms**, so every key-shift arrow pointed the wrong way | `lbl_8219E06C` = "key_shift_left.wid", `lbl_8219E058` = "key_shift_right.wid", 0x82B94674..0x82B946C4 | 0 |
| `GemTrack::DrawFill` | crash widget `FillHit(3)` gated on `isDrum` (always true there); retail gates on `!DrumFillsMod()` | value kept live in r17 from the `cntlzw/extrwi` of Game+0x145; after the fix our save drops to `__savegprlr_17`, as retail | 87.99 → 88.80 |
| `ChordShapeGenerator::BuildEndCap` | `flip = contour == (orient == right)`; retail compares against **left**, so every end cap had x-scale sign and winding inverted (`4de3aa6f3` read the wrong static) | `lwz r11,0(r29)`, r29 = &left built from "left" @`0x82025094` | 96.92 → 99.05 (mpn 100) |
| `LayerDir::RefreshLayer` | walked `Refs()` **backwards**; retail walks the `std::list` forwards, so texture renderers got `SetFrame` in reverse order | node at `0x24`, data at +8, advance `lwz r30,0(r30)` (next) | 96.55 → 98.23 |
| `PatchLayer::Draw` | (a) stored the global `hackyScaleValue`, which retail never references and nothing else reads; (b) scaled the y row by **1.0**, retail by **0.0**; (c) `mRot` scaled in two steps, retail by one folded constant | (b) `fmuls` by f31 = `0x82000D78` = 0.0 on m.y.x/y/z; (c) `0x8200F4B0` = `0x3c497495` = 360/511·DEG2RAD, bit-exact | 76.41 → 87.35 |
| `PatchLayer::LoadPacked` | deform frame by two multiplies (≤1 ulp off); retail folds one constant | `0x8200EDB0` = `0x3f7fbfff` = 0.048828125·20.46, bit-exact | 98.08 → **100** |

`FingerShape::Update` also lost a counter that always equals the loop index (no `continue` skips it), which is
behaviour-neutral. Its row rose 93.11 → 95.41.

All constants above were re-read from `band.exe` by the coordinator, independently of the forks.

## A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16pr --patch combined.diff --label w16pr-combined`, ruler
`name_check` (objdiff.json), with the `none` control leg. Run dir:
`~/tmp/wt-w16pr/.ab_measure_runs/20261006-094050-w16pr-combined-4174355`.

```
Δmatched=+2  Δmasked_equal=+0  Δhonest=+2  Δcode%=+0.006092pp  Δcode_bytes=+624
Δfuzzy=+0.001675pp   (legA 63.695568 -> legB 63.697243)
units at 100% [mpn]: 551 -> 551 (0 reached, 0 fell off)
[control none] Δmatched_code=+624 B
row-by-row (legA vs legB report.json, key (unit, symbol)): 68,909 rows, up 6, down 0, gone 0, new 0
```

The six rows up are DrawFill, RefreshLayer, BuildEndCap, PatchLayer::Draw, LoadPacked and FingerShape::Update. The
`none` control moving with the graded ruler is the source-fix shape, not alias forgiveness.
Per-fork A/Bs (archived in each fork's worktree): G1 +0/+0, G2 +0/+0 (1 row up), G3 +1/+0 (2 up), G5 +1/+624
(3 up), all with 0 rows down; G4 and G6 changed no source.

After the A/B, the G1, G3 and G5 commits were amended before merging, so that source comments and commit messages
carry no port-provenance wording (standing user rule). The landed `src/` diff differs from the measured patch in
exactly two comment lines in `PatchDir.cpp`, replaced 1:1 so line numbers are unchanged; `diff` of the two patch
files shows nothing else. The native runs recorded below predate that clean-up; both were rerun on the landed
tree as the lane's last actions.

## Per-row verdicts

EQUAL = behaviour equal; the residue named is codegen only. Fuzzy is graded, before (W16-PN snapshot) → after (this
branch).

| row | file | B | class | fuzzy | verdict | evidence / residue |
|---|---|---:|---|---|---|---|
| `GemManager::SetupGems` | band3/bandtrack/GemManager.cpp | 3,404 | I4 | 97.56 → 97.56 | **EQUAL** | register/stack cascade, dead spills; fn_82BAC148 is the Gem ctor (field-checked) |
| `GemManager::DrawTrackMasks` | band3/bandtrack/GemManager.cpp | 1,600 | I3 | 98.78 → 98.78 | **EQUAL** | register swaps; retail reloads 0x12c after the store |
| `GemManager::CheckRemoveChordBracket` | band3/bandtrack/GemManager.cpp | 372 | I3 | 98.60 → 98.60 | **EQUAL** | `and` operand order; GetBandUser is a fold |
| `GemManager::IsEndOfFill` | band3/bandtrack/GemManager.cpp | 100 | I3 | 92.80 → 92.80 | **EQUAL** | register allocation, bool-return mask |
| `GemTrack::DrawFill` | band3/bandtrack/GemTrack.cpp | 1,652 | I3 | 87.99 → 88.80 | **FIXED** | crash-widget FillHit(3) gated on isDrum (always true there); retail tests !DrumFillsMod() kept live in r17. Save register now __savegprlr_17 as retail |
| `GemTrack::DrawBeatLine` | band3/bandtrack/GemTrack.cpp | 684 | I3 | 95.85 → 95.85 | **FIXED** | key_shift_left/right.wid swapped in both arms (retail lbl_8219E06C/lbl_8219E058, 0x82B94674..0x82B946C4); placeholder labels, score-invisible |
| `VocalTrack::UpdateScrolling` | band3/bandtrack/VocalTrack.cpp | 8,948 | I4 | 96.97 → 96.97 | **FIXED** | SongSectionOnly range initialised inverted (retail -FLT_MAX @0x82071744 / FLT_MAX @0x8201C818); FreeOldGems(ms-250) vs retail ms-1000 (@0x820010B4). Both placeholder-label constants: score-invisible. Rest: copy order, registers |
| `VocalTrack::UpdateTambourineGems` | band3/bandtrack/VocalTrack.cpp | 744 | I1 | 87.22 → 87.22 | **EQUAL** | same deque size from the same iterator fields; load order, registers |
| `VocalTrack::ProcessStaticLyrics` | band3/bandtrack/VocalTrack.cpp | 312 | I3 | 97.69 → 97.69 | **EQUAL** | we reuse a just-stored value, retail reloads; registers |
| `BandUser::SetChar` | band3/game/BandUser.cpp | 448 | I2 | 98.79 → 98.79 | **EQUAL** | lastdata=0 in its own register; push_back is a fold |
| `PerfectSectionTracker::HandleExitExtent` | band3/game/PerfectSectionTracker.cpp | 1,256 | I3 | 94.77 → 94.77 | **EQUAL** | ratio algebraically the same, calls/stores match; map<int>/map<TrackType> is a fold |
| `TrainerGemTab::GetLane` | band3/game/TrainerGemTab.cpp | 96 | I2 | 93.54 → 93.54 | **EQUAL** | register allocation (one extra mr in retail) |
| `VocalTrainerPanel::CopyTubes` | band3/game/VocalTrainerPanel.cpp | 944 | I3 | 99.58 → 99.58 | **EQUAL** | retail extra stw to a stack slot overwritten before any read; call-name diffs are ICF folds |
| `AccomplishmentProvider::Mat` | band3/meta_band/AccomplishmentPanel.cpp | 360 | I3 | 97.11 → 97.11 | **EQUAL** | constants 1.0/0.25 byte-checked; shared-base constant addressing |
| `Campaign::Campaign` | band3/meta_band/Campaign.cpp | 400 | I2 | 94.90 → 94.90 | **EQUAL** | AddSink argument scheduling; hash_map ctor names are folds |
| `Campaign::GetLaunchUser` | band3/meta_band/Campaign.cpp | 68 | I3 | 82.06 → 82.06 | **EQUAL** | same [0xa4] null test and call; frame-pointer residue |
| `CustomizePanel::Handle` | band3/meta_band/CustomizePanel.cpp | 5,036 | I3 | 99.92 → 99.92 | **EQUAL** | extra clrlwi re-narrowing a 0/1 bool |
| `MetaPerformer::SaveAndUploadScores` | band3/meta_band/MetaPerformer.cpp | 684 | I3 | 99.42 → 99.42 | **EQUAL** | same unk338/unk33c sequence and RecordScore arg; one extra spill |
| `ModifierMgr::IsModifierActive` | band3/meta_band/ModifierMgr.cpp | 60 | I3 | 79.67 → 79.67 | **EQUAL** | same GetModifier(s,true), same mod->[4]!=0 test; frame-pointer residue |
| `ModifierMgr::IsHidden` | band3/meta_band/ModifierMgr.cpp | 28 | I4 | 28.57 → 28.57 | **EQUAL** | both return 0; retail r31 frame-pointer prologue |
| `ModifierMgr::IsActive` | band3/meta_band/ModifierMgr.cpp | 28 | I4 | 28.57 → 28.57 | **EQUAL** | both return 1; frame-pointer residue |
| `MusicLibrary::Text` | band3/meta_band/MusicLibrary.cpp | 1,292 | I1 | 94.21 → 94.21 | **EQUAL** | retail cross-jumps the shared song dispatch of the StoreSong and Song arms; same casts and branches (block placement) |
| `MusicLibrary::ToggleFilter` | band3/meta_band/MusicLibrary.cpp | 240 | I2 | 91.58 → 91.58 | **EQUAL** | same find then erase-or-insert; set names are folds |
| `NextSongPanel::FillExpandedDetails` | band3/meta_band/NextSongPanel.cpp | 1,640 | I3 | 99.76 → 99.76 | **EQUAL** | one extra float stack-temp store |
| `LayerProvider::GetMatForData` | band3/meta_band/PatchPanel.cpp | 508 | I2 | 88.01 → 88.01 | **EQUAL** | constants value-tracked per register; callee 0x822750d0 is a T1-proven fold of FindEmptyLayer; FPR assignment, texgen store order |
| `ProfileMgr::GetJoypadExtraLagInits` | band3/meta_band/ProfileMgr.cpp | 448 | I3 | 97.28 → 97.28 | **EQUAL** | 16 float constants byte-checked, all 42 jump-table cases map to the same arm; retail duplicates a tail |
| `SaveLoadManager::OnMsg` | band3/meta_band/SaveLoadManager.cpp | 456 | I4 | 60.92 → 60.92 | **EQUAL** | both compare trees simulated over 4,788 (state,result) inputs: 60 outcomes, 0 disagreements |
| `StoreMainPanel::FinishLoad` | band3/meta_band/StoreMainPanel.cpp | 592 | I3 | 99.32 → 99.32 | **EQUAL** | one extra stack-temp store |
| `StoreOfferProvider::BuildList` | band3/meta_band/StoreOfferProvider.cpp | 2,536 | I3 | 99.24 → 99.24 | **EQUAL** | stack offsets, 2 dead stores; same three byte fields written in another order |
| `UIStats::MaybePublish` | band3/meta_band/UIStats.cpp | 2,604 | I3 | 99.59 → 99.59 | **EQUAL** | stack offsets; cmplw operands swapped into a beq; six hoisted string loads give the same register->value map (retail strings checked) |
| `ContextWrapperPool::FailAllContexts` | band3/net_band/ContextWrapper.cpp | 220 | I3 | 96.00 → 96.00 | **EQUAL** | one stack-temp store, cr0 vs cr6 |
| `RockCentral::DataPointToQString` | band3/net_band/RockCentral.cpp | 460 | I4 | 91.43 → 91.43 | **EQUAL** | same calls and dispatch; loop entry test placed differently; fn_82B81F40 is GetObjectAsString in kind |
| `BandCamShot::SetPreFrame` | system/bandobj/BandCamShot.cpp | 476 | I3 | 98.32 → 98.32 | **EQUAL** | two clrrwi 0 no-op masks (iterator node casts); --mShotIter not expressible on ObjPtrList |
| `BandDirector::OnFileLoaded` | system/bandobj/BandDirector.cpp | 3,816 | I1 | 99.78 → 99.78 | **EQUAL** | one address computation scheduled differently; SetSongAnimGenre is a recorded fold |
| `BandDirector::OnMidiShot5Cleanup` | system/bandobj/BandDirector.cpp | 1,172 | I3 | 98.36 → 98.36 | **EQUAL** | Symbol temp slot reuse, merged tail; vector<CamCatEntry> names are recorded folds |
| `BandDirector::BandDirector` | system/bandobj/BandDirector.cpp | 1,088 | I1 | 98.27 → 98.27 | **EQUAL** | same -1.0f store to +0x114 and +0x12c store, other order; reserve is a fold |
| `BandLeadMeter::SyncScores` | system/bandobj/BandLeadMeter.cpp | 424 | I1 | 86.93 → 86.93 | **EQUAL** | ordering, register choice |
| `BandLeadMeter::PostLoad` | system/bandobj/BandLeadMeter.cpp | 356 | I2 | 98.18 → 98.18 | **EQUAL** | order of two 16-bit stores |
| `BandPatchMesh::FindXfm` | system/bandobj/BandPatchMesh.cpp | 1,268 | I4 | 75.18 → 75.18 | **EQUAL** | every call, test, store and return value matches retail 0x823468E8; stack layout and FPR-vs-memory scheduling |
| `WorkVerts::SetVertsAndFaces` | system/bandobj/BandPatchMesh.cpp | 940 | I3 | 99.00 → 99.00 | **EQUAL** | hoisted constant in f31 vs f4; one fadds operand swap |
| `WorkVerts::TryAddFace` | system/bandobj/BandPatchMesh.cpp | 824 | I4 | 90.87 → 90.87 | **EQUAL** | same clamped projection and sign test; registers, scheduling |
| `WorkVerts::ExtendTwin` | system/bandobj/BandPatchMesh.cpp | 716 | I4 | 84.96 → 84.96 | **EQUAL** | same perpendicular step, fsel sign, Matrix2 rows, Invert epsilon (byte-checked) |
| `MeshVert::AddUV` | system/bandobj/BandPatchMesh.cpp | 492 | I4 | 95.54 → 95.54 | **EQUAL*** | same values, but retail rounds the product before the add where we contract to one fmadds (<=1 ulp, /fp:fast contraction); retail-order spelling lowered the row, reverted |
| `Invert` | system/bandobj/BandPatchMesh.cpp | 128 | I1 | 92.28 → 92.28 | **EQUAL** | all inputs loaded before stores (aliasing-safe both sides); store order, one commutative multiply |
| `PatchPair::PatchPair` | system/bandobj/BandPatchMesh.cpp | 96 | I4 | 45.00 → 45.00 | **EQUAL** | retail calls ObjPtr<RndTex>(owner,0) out of line, we inline it; same stores (inlining policy) |
| `BandTrack::Reset` | system/bandobj/BandTrack.cpp | 1,116 | I1 | 98.45 → 98.45 | **EQUAL** | two loads ordered differently |
| `BandTrack::SetCrowdRating` | system/bandobj/BandTrack.cpp | 528 | I3 | 98.46 → 98.46 | **EQUAL** | retail writes mTrackIdx to a never-read stack slot; frame size |
| `BandTrack::SetupCrowdMeter` | system/bandobj/BandTrack.cpp | 320 | I3 | 98.75 → 98.75 | **EQUAL** | same dead mTrackIdx stack write |
| `BandWardrobe::SyncProperty` | system/bandobj/BandWardrobe.cpp | 2,416 | I2 | 99.74 → 99.74 | **EQUAL** | volatile r5/r11 swap |
| `BandWardrobe::LoadMainCharacters` | system/bandobj/BandWardrobe.cpp | 1,776 | M1 | 99.79 → 99.79 | **EQUAL** | two loads swapped, different stack temp |
| `BandWardrobe::OnGetMatchingDude` | system/bandobj/BandWardrobe.cpp | 232 | I2 | 94.40 → 94.40 | **EQUAL** | retail loads bc->0x5ec earlier (liveness) |
| `ChordShapeGenerator::BuildChordMesh` | system/bandobj/ChordShapeGenerator.cpp | 1,680 | M1 | 100.00 → 100.00 | **CLOSED** | at 100 after W16-PG (.bss order), before this lane |
| `ChordShapeGenerator::BuildEndCap` | system/bandobj/ChordShapeGenerator.cpp | 1,352 | I2 | 96.91 → 99.05 | **FIXED** | flip compared orient against `right`; retail compares against `left` (lwz r11,0(r29), r29=&left from "left" @0x82025094), so every end cap had x-scale sign and winding inverted. mpn now 100; rest registers |
| `ChordShapeGenerator::InterpolateXfm` | system/bandobj/ChordShapeGenerator.cpp | 784 | I3 | 89.29 → 89.29 | **EQUAL** | FPR choice, ordering, one fadds with swapped operands |
| `FingerShape::Update` | system/bandobj/FingerShape.cpp | 532 | I4 | 93.11 → 95.41 | **EQUAL** | behaviour equal; a redundant counter identical to the loop index removed (no continue skips it) and the row rose |
| `SemitoneToWhiteKey` | system/bandobj/GemTrackDir.cpp | 96 | I4 | 50.00 → 50.00 | **EQUAL** | jump table and six case bodies byte-identical; score is a dtk carve of the case tails into fn_822E3630 |
| `GemTrackResourceManager::InitSmasherPlates` | system/bandobj/GemTrackResourceManager.cpp | 672 | I1 | 94.49 → 94.49 | **EQUAL** | prologue scheduling; SetObjConcrete is a fold |
| `LayerDir::RefreshLayer` | system/bandobj/LayerDir.cpp | 1,588 | I4 | 96.55 → 98.23 | **FIXED** | walked Refs() backwards; retail walks the std::list forwards (next at +0, end() recomputed through the virtual-base adjust), so texture renderers got SetFrame in reverse order |
| `LayerDir::GetBitmapList` | system/bandobj/LayerDir.cpp | 692 | M2 | 99.65 → 99.65 | **EQUAL** | DataNode temp address from ctor return vs recomputed; Int/Array, PoolAlloc folds |
| `NoteTube::CreateMeshes` | system/bandobj/NoteTube.cpp | 392 | I4 | 80.71 → 80.71 | **EQUAL** | same Min() adjustments and plate init/draw calls; fn_82C29E98 is DrawToPlate in kind |
| `OverdriveMeter::SyncObjects` | system/bandobj/OverdriveMeter.cpp | 332 | I1 | 69.16 → 69.16 | **EQUAL** | mr r3 ordering; SetObjConcrete names are folds |
| `PatchLayer::Draw` | system/bandobj/PatchDir.cpp | 1,020 | I4 | 76.41 → 87.35 | **FIXED** | stored hackyScaleValue, a global retail never references; y row scaled by 1.0 where retail uses 0.0 (@0x82000D78); mRot multiplied in two steps where retail folds 360/511*DEG2RAD (0x3c497495 @0x8200F4B0). Rest: temp copy, store order, frame size |
| `PatchLayer::LoadPacked` | system/bandobj/PatchDir.cpp | 624 | I3 | 98.08 → 100.00 | **FIXED** | deform frame computed with two multiplies; retail folds 0.048828125*20.46 (0x3f7fbfff @0x8200EDB0), 1 ulp |
| `PitchArrow::SyncObjects` | system/bandobj/PitchArrow.cpp | 588 | I1 | 96.46 → 96.46 | **EQUAL** | scheduling; string label is spotlight_end.trig on both |
| `SongSectionController::Handle` | system/bandobj/SongSectionController.cpp | 1,240 | I3 | 96.91 → 96.91 | **EQUAL** | register numbering, symbol held vs reloaded, frame +0x10 |
| `StreakMeter::SyncObjects` | system/bandobj/StreakMeter.cpp | 1,328 | I1 | 99.40 → 99.40 | **EQUAL** | one addi a slot apart; unnamed callees are Find<RndPartLauncher> and ObjPtr<RndPartLauncher> ctor/dtor (RTTI) |
| `TrackPanelDir::ConfigureCrowdMeter` | system/bandobj/TrackPanelDir.cpp | 336 | I3 | 98.01 → 98.01 | **EQUAL** | block layout; both store (gamemode && show_crowd_meter) to +0xA8 |
| `VocalTrackDir::SetRange` | system/bandobj/VocalTrackDir.cpp | 700 | I3 | 93.71 → 93.71 | **EQUAL** | same texXfm copy, dirty flag, (60-min) scaling; reordered |
| `JoypadController::GetVirtualSlot` | system/beatmatch/JoypadController.cpp | 496 | I1 | 84.84 → 84.84 | **EQUAL** | block order (every target mapped) |
| `KeyboardTrackWatcherImpl::CheckForFatFinger` | system/beatmatch/KeyboardTrackWatcherImpl.cpp | 300 | I4 | 95.73 → 95.73 | **EQUAL** | retail materialises the ==5 test as a bool before branching |
| `MasterAudio::FillSwing` | system/beatmatch/MasterAudio.cpp | 140 | I3 | 76.86 → 76.86 | **EQUAL** | retail recomputes mTrackData[num] after the call; a re-indexing spelling lowered the row to 51.80, reverted |
| `MasterAudio::SetButtonMashingMode` | system/beatmatch/MasterAudio.cpp | 88 | I1 | 86.50 → 86.50 | **EQUAL** | ordering, register choice |
| `PitchDetector::AnalyzeBlock` | system/dsp/PitchDetector.cpp | 1,780 | I1 | 96.27 → 96.27 | **EQUAL** | same modulo stored at 0x1c; reordered |
| `ShiftedDotProduct` | system/dsp/SndAnalysis.cpp | 356 | I4 | 80.56 → 80.56 | **EQUAL** | same vector/scalar loops, permute table byte-identical; loop-counter form, registers |
| `AsyncFileWin::_ReadAsync` | system/os/AsyncFile_Win.cpp | 460 | I2 | 94.39 → 94.39 | **EQUAL** | same flag stored at 0x5c; fn_8283FC58 has ReadFile shape and gets the same 5 args |
| `WinSockSocket::RecvFrom` | system/os/NetworkSocket_Win.cpp | 136 | I3 | 96.91 → 96.91 | **EQUAL** | same stores (port=-1, addr=0) and error return; extra register for zero |
| `ImmediateWidgetImp::DrawInstances` | system/track/TrackWidgetImp.cpp | 600 | I3 | 95.65 → 95.65 | **EQUAL** | register cascade; same (end-begin)/0x4C with swapped operands |
| `CharWidgetImp::RemoveInstances` | system/track/TrackWidgetImp.cpp | 396 | I3 | 95.84 → 95.84 | **EQUAL** | argument scheduling; push_back names are a fold |

## What this says about lever 3

- **Yield in bytes is what the campaign doc priced (≈1%: 624 B of 75,112).** Yield in behaviour is not. Seven of
  78 "long-worked" rows, opened by three to four lanes each, still carried real behaviour bugs. Three of the ten
  moved no score at all and could never have been found by grinding.
- **Placeholder-label constants are a score-invisible defect class.** A wrong float or string, or a swapped pair,
  loaded through a retail `lbl_` reads identical under `name_check`. The two checkers here make the class
  checkable. Run both on any row read for behaviour: position-paired to catch swaps, multiset to clear reorder
  artifacts. Over this population they found 8 wrong constant pairs in 340.
- **The inherited source was the origin of two defects** (the inverted section range in VocalTrack, the 1.0 y scale
  and `hackyScaleValue` in PatchLayer::Draw), and a prior "rewritten from retail" commit was the source of a third
  (BuildEndCap). Retail bytes outrank both.
- None of these files is in a native target yet, so native cannot observe these fixes until the files are linked.

## Not done

- No codegen grinding on the 69 behaviour-equal rows; their residue is recorded per row above.
- `SemitoneToWhiteKey` (96 B, 50.0) is a dtk carve of the case tails into `fn_822E3630`, a split question, not
  source.
- `MeshVert::AddUV` keeps our `fmadds` contraction (≤1 ulp vs retail's separately rounded product). The retail-order
  spelling lowered the row, so it was reverted.
- `JsonObject::GetObjectAsString`'s body (in `src/network`) was not inspected, by directive.
- The checkers are in `~/tmp/w16pr/` (`constcheck2.py`, `constmulti.py`, `rowdiff.py`, `mktable.py`) and in the
  fork dirs `~/tmp/w16pr-g{1..6}/`. They are not committed.

## Native

`tools/native_build_gate.sh` and `tools/native_health.sh` on the merged tree before the history clean-up (rerun as the
lane's last actions on the committed tree):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=57 gates_fail=0 unrunnable=none selftest=SKIPPED ... runtime_crashed=0 runtime_failed=none rc=0
```
