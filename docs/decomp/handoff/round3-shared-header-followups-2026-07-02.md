# Round-3 near-miss wave — preserved shared-header follow-ups (2026-07-02)

The round-3 Stream-3 mislabel wave (5 Fable subagents) pushed **10 functions to
100%**, but three of the fixes were **shared-header** changes with real gains AND
real regressions (incomplete cascade). The **clean isolated subset** was landed
(commit on `bp-round3-clean`, cherry-picked to main). The **mixed shared-header
changes are preserved on branch `followup/round3-full-batch @ 3879248`** (the full
batch — all 10 fixes + the 16-file shared edits) for a proper follow-up pass.

This doc records exactly what those changes were, the evidence behind them, and
why they regressed, so they can be re-attempted with correct cascade handling.

## Composed A/B of the FULL batch (all changes together)
`NET +32 (gained 55, regressed 23 strict + 14 fuzzy)` vs base @9b938ea.
Not landable as-is. Bisection isolated the regressions to two bundles below.

## Bundle 1 — CollideListSubParts de-virtualization (NET-NEGATIVE, reverted)
Files: `src/system/rndobj/Dir.h`, `src/system/char/Character.h`,
`src/system/ui/PanelDir.cpp`, + `scripts/target_symbol_map.json` (U→Q rename of
`0x823F0890`).

Claim (Fable lane C, read from `band.exe` vtables): retail RB3-360 RndDir +
Character vtables run `...ChainSourceSubdir -> <next derived virtual>` with **no
CollideListSubParts slot** — i.e. it's a DC3-added virtual, and demoting it to
non-virtual would shift every later slot back by 4 to match retail (fixing
PanelDir::PanelNav's vcall `+0x40`→`+0x3c`).

Why it regressed: the composed A/B **disproved** this for RB3-360. De-virtualizing
shifted vtable slots and **broke 15+ already-matching functions** that vcall
through the RndDir/Character/ObjectDir family at post-CollideListSubParts slots:
`Character::{EnableBlinks,ForceBlink,SetFocusInterest,SetInterestFilterFlags,
SetInterestObjects}` (all vcall off −4), `CameraManager` ×2, `CharClipSet` ×2,
`Dir fn_8272D208`, `BandWardrobe` (one method → 0% unpaired). Its *only* intended
beneficiary, `PanelNav`, **never even reached 100** (stayed 96.8% — the residual
was proven a genuine retail-source body divergence: dc3-decomp compiles the same
source to Matching, so RB3's source differed). **Verdict: net-negative, do not
re-attempt without new vtable evidence.** Confirmed by incremental probe:
reverting it returned `Character::EnableBlinks` to 100.

## Bundle 2 — FileLoader + ObjDirItr DC3-drift reverts (MIXED, preserved)
Files: `src/system/utl/Loader.h`, `src/system/utl/Loader.cpp`,
`src/system/obj/Dir.h` (ObjDirItr layout), + 13 call-site TUs
(`world/LightHue.cpp`, `rndobj/Utl.cpp` [GetNormalMapTextures],
`rndobj/Tex.cpp`, `synth/{Synth,MoggClip,BinkClip}.cpp`, `hamobj/HamAudio.cpp`,
`os/FileCache.cpp`, `utl/{NetLoader,NetCacheLoader}.cpp`,
`gesture/LiveCameraInput.cpp`).

Claim (Fable lane A, oracle-confirmed against rb3-Wii):
- **FileLoader**: retail ctor is 7-arg + `operator new(0x48)`, no trailing
  `const char* heapName` and no `String mHeapName` member (DC3 added them;
  `mState` moves 0x54→0x44). `AllocBuffer` uses `MemFindHeap("main")`.
- **ObjDirItr**: retail layout `{mDir, mSubDir, mEntry, mObj@0xC, mWhich}` with
  no `std::list mSubDirs` and no list-clear in the dtor path (DC3-shaped ours had
  `mObj@0x4` + inlined `_List_base::clear`). Added inline
  `ObjectDir::NextSubDir(int&)` ported from rb3-Wii.

These are **genuinely correct DC3-drift reverts** and produced a large positive
cascade (LightHue::Sync→100, GetNormalMapTextures→100, plus much of the Char*/
AmbientOcclusion gains in the +55). BUT the cascade was **incomplete**: some
funclets/methods that depended on the old layout became **unpaired stubs (0%)** or
frame-shifted near-misses:
- `DirLoader fn_82735{358,380,470}` → 0% (Loader subclass funclets)
- `BandWardrobe fn_82322468` → 0% stub (calls fn_8228B5C8; still 0% after the
  devirt revert, so attributed here)
- `MetaMusic fn_826F1C20` → 67% (frame Δ −0x20), `fn_826F1BF4` → 94%
- `NetSync fn_825860xx` cluster (10 funclets, frame `subi −96`) → 99.9%
- `TrackDir fn_827B9D{18,6C}` → 0%, `CharBoneDir fn_823A5A90` → 94%

**Why preserved, not discarded:** the reverts are the right direction (oracle +
retail asm agree). The regressions are almost certainly **missed call sites / a
funclet that must also change shape** — i.e. finishable, not wrong. Re-attempt:
apply Bundle 2 from `followup/round3-full-batch`, then for each 0%-stub
(`DirLoader`, `BandWardrobe`, `TrackDir` funclets) diff the funclet and update the
remaining site so it's emitted again; re-run whole-binary A/B until the loader
cascade is regression-free. Expected net strongly positive (LightHue + Char*
family) once the ~6 stubs are repaired.

## GameGem.h sizeof 0x2c→0x44 (LANDED in clean subset)
Evidence: retail `mulli rX,rY,0x44` at `0x8269DE58/0x8269DFC0` (GemPlayer asm) =
`sizeof(GameGem)` is 0x44, both oracles stop at 0x2c. Added `int unk2c[6]` tail.
Narrow blast radius (beatmatch gem loops), no by-value embeds; kept in the clean
landing and A/B-gated. Unblocks `SongDB::GetSustainGemCount` and possibly other
gem-loop near-misses.

## HamCamTransform lever (BANKED, needs target-map fix — lane E)
`TransformCrowd` is 0xc not 0x10 in retail (DC3 added `mCrowdRotate`);
`TransformArea` is 0x70 (correct). TU-local removal of `mCrowdRotate` flips
`ObjVector<TransformCrowd>::operator=` 92.9→100 and `fn_82296304` 94.5→99.9, but
is **net −3 unit-wide** because `scripts/target_symbol_map.json` misnames several
0x10-family (TransConstraint-owned) fns as `vector<TransformCrowd>` — our wrong
0x10 code was false-100 matching them. Edited files saved at
`/home/free/tmp/hct_edited.{h,cpp}`. Unblock: fix the target-map naming for the
unit (name the real 0xc family as `vector<TransformCrowd>`; re-attribute the 0x10
TransConstraint family + 3 shifted `Load/op=` names), then re-apply → net-positive.
