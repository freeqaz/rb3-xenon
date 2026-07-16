# ≥99% near-miss triage — 100-fn sample (2026-07-16)

> **Method.** 20 Sonnet workflow agents (run `wf_bf7a93ab-34b`), read-only, one
> function per unit across the 100 hottest ≥99%-fuzzy units — the LARGEST
> near-miss in each unit, so each verdict proxies for its unit's whole cluster
> (see `cascade_hint` per row). Full structured verdicts:
> **`triage-nearmiss-100-2026-07-16.ndjson`** (same dir). Baseline: 15,100
> strict, 1,437 fns at ≥99% fuzzy.

## Class distribution (of 100)

| classification | n | action mix |
|---|---|---|
| EH_FUNCLET_MIRAGE | 33 | skip-mirage (parent-frame drift echoes, not independent bugs) |
| STRUCT_OFFSET_DRIFT | 31 | mostly recon-needed; TU5 member-insert remnants |
| REGALLOC_PERMUTER | 14 | /permute queue — do NOT hand-crack |
| PAIRING_TOOLING | 10 | symbol-map / ICF-fold artifacts |
| BODY_DIVERGED | 5 | real rewrites |
| SPILL_QUANTIZED | 2 | wall (EH-homing/reg-pressure) |
| FIXABLE_SOURCE_EDIT | 2 | concrete source bugs w/ fix sketch |
| CONSTANT_DIFF | 2 | single-immediate fixes |
| VTABLE_SLOT_DRIFT | 1 | missing virtual |

Actions: fix-now-cheap **6**, recon-needed **37**,
permuter **14**, skip-mirage **39**, skip-wall **4**.

## Extrapolation to the full 1,437-fn band

~39% of the band is mirage (EH funclets + pairing artifacts — funclets echo their
parent's frame layout and flip for free when the parent is fixed), ~14% is
permuter-class, ~31% is struct-offset drift concentrated per-unit (a handful of
header/layout causes each covering many readers), and only ~10% needs real body
work. **The efficient path is unit-cause recon on the 31 struct-drift units +
the 6 cheap fixes + a /permute batch on the 14**, not per-function grinding.

## Fix-now-cheap (6)

| BandDirector | `?SyncProperty@BandDirector@@UAA_NAAVDataNode@@PAVDataArray` | high | 5 nearly-identical mismatched blocks (idx 637-638, 692-693, 734-735, 776-777, 818-819), one per instrument: target does `lis r10, lbl_82013F… |
| Gen | `?SetFrame@RndGenerator@@UAAXMM@Z` | medium | Single mismatch at idx 88/110: `fadds f0, f0, f31` (implied) vs `fadds f0, f31, f0` — objdiff's own COMMUTATIVE_OP_ORDER pattern detector fl… |
| StreamReceiver360 | `?Poll@StreamReceiver360@@UAAXXZ` | high | 3 of 4 mismatches are `lis r11,0x1`/`ori r11,r11,0x803c` (target, i.e. absolute this-relative offset 0x1803c) vs `lis r11,0x0`/`ori r11,r11,… |
| SongCollision | `?_M_fill_insert_aux@?$vector@UBeatCollisionData@@V?$StlNod` | high | 13 diff_arg instructions all differ by a uniform delta: target's `li r5,0x38` / `mulli r27,r27,0x38` / `subi r31,r31,0x38` / `addi r31,r31,0… |
| RhythmDetector | `??$__uninitialized_copy@PAUFrame@RhythmDetector@@PAU12@@st` | high | 24-instr templated copy loop over RhythmDetector::Frame*. `addi r29,r29,0x14` / `addi r30,r30,0x14` (target, both pointer advances) vs `addi… |
| band3/meta_band/BandSongMgr | `?IsInExclusionList@BandSongMgr@@QBA_NPBDH@Z` | high | Single mismatch at instr[21]: `cmplwi cr6, r7, 0x10` (target/retail) vs `cmplwi cr6, r7, 0x20` (ours) -- the outer do-while loop's exit boun… |

## Permuter queue (14)

| Morph | `?SetFrame@RndMorph@@UAAXMM@Z` | high | 195 total instructions, only 16 diff_arg forming 2 clean register-swap pairs: `addi r11,...->r10` / `addi r10,...->r11` and `lfs/stfs r10<->… |
| PropKeys | `?SetFrame@QuatKeys@@UAAXMMM@Z` | high | Only 1 of 125 instructions mismatches: idx[87] `fmuls f12, ..., ...` vs `fmuls f6, ..., ...` with operand registers swapped (f12<->f6) -- ob… |
| system/rndobj/Utl | `?LinearizeKeys@@YAXPAVRndTransAnim@@MMMMM@Z` | high | Normalized diff shows only 4 mismatched instructions out of 324, all pure register renumbers with no diff_op/insert/delete: idx11/13 `li` re… |
| BandCharacter | `?FastInvert@@YAXABVTransform@@AAV1@@Z` | high | diagnose fully explains all 9 diff_arg instructions with 0 unexplained: f30<->f31 FPR swap on 4 instructions (idx17/19 `fmuls [reg:f31->f30]… |
| CharIKFingers | `?Poll@CharIKFingers@@UAAXXZ` | high | 1/219 mismatch: idx128 `lbzx r9,r8` (retail) vs `lbzx r8,r9` (ours) -- pure base/index register swap on an indexed byte load inside the mFin… |
| CharBonesMeshes | `?PoseMeshes@CharBonesMeshes@@QAAXXZ` | high | 12/188 diff_arg instructions, all pure operand reorderings, no diff_op/insert/delete. Pattern detector: REGISTER_SWAP (12 instrs/3 pairs, do… |
| CharHair | `?SetRoot@Strand@CharHair@@QAAXPAVRndTransformable@@@Z` | high | Only 1 real diff_arg out of 149 instructions (99.9% normalized). Instruction [135]: target `fmadds f0, ..., f12` vs base `fmadds f12, ..., f… |
| Rnd_Xbox | `?BeginTiling@DxRnd@@AAAXABVColor@Hmx@@MI@Z` | high | 4 of 57 instructions mismatch, all in one contiguous cluster (idx 30-33): target `fctidz f13,f0 / stfd f13,-0x10(..) / fctidz f0,f11 / stfd … |
| system/synth_xbox/Synth | `?Terminate@Synth360@@UAAXXZ` | high | Single diff_arg at instr 82: target `lwz r5, 0x4, r30` vs base `lwz r5, 0xb8, r31`. r30 was set at instr 43 (`addi r30, r31, 0xb4`, common t… |
| BandWardrobe | `?OnEnterCloset@BandWardrobe@@QAA?AVDataNode@@PAVDataArray@` | medium | 9/89 instrs mismatch, all a clean r23<->r24 swap with identical opcodes/counts: `li r24,0x0`/`li r23,0x0`; `mr r23,r3`/`mr r24,r3`; and the … |
| Geo | `?CheckBSPTree@@YA_NPBVBSPNode@@ABVBox@@@Z` | high | 67/334 mismatches, 63 of them are register-number swaps in float regs (f25<->f27 dominant, also f21<->f22/f23, f26<->f28, f30<->f31), e.g. i… |
| Mesh | `?FillCompressedVertex@@YAXAAUCompressedVertex_Xbox@@ABVVer` | medium | 8/139 mismatches: a register swap r28<->r29 (idx34-35, `rlwimi`) plus symmetric local-stack float offset swaps (+/-4, +/-12 on `lfs`/`stfs` … |
| band3/meta_band/EditSetlistPanel | `?DoneEditing@EditSetlistPanel@@QAAXXZ` | high | 89-instr fn, only 4 diff_arg instructions, all in one contiguous block: `lwz [reg:r10->r11]` / `lwz [reg:r11->r10, reg:r10->r11]` / `addi [r… |
| Waypoint | `?ShapeDeltaBox@Waypoint@@AAAXABVVector3@@MMAAV2@@Z` | high | 20 diff_arg of 124 instrs, all register operands on floating-point ops (fmadds/fsubs/fsel/fmuls/lfs/stfs), dominated by f10<->f12 swaps (6/1… |

## High-confidence struct-drift units (12 of 31 — recon each unit's cause once)

| BandLabel | `fn_82341868` | high | `addi r3, r11, 0x218` (target) vs `addi r3, r11, 0x16c` (ours), a -172 (0xAC) byte drift feeding a `~UILabel` base-dtor tail call (`bl fn_82… |
| Character | `?resize@?$vector@ULod@Character@@V?$StlNodeAlloc@ULod@Char` | high | Element-size constants used for both the pointer-difference divide and the offset multiply: `li r8,0xc` (target) vs `li r8,0x1c` (base) [off… |
| BandCamShot | `?ListNextShots@BandCamShot@@IAA_NAAV?$list@PAVBandCamShot@` | high | 5/50 instrs mismatch, all field-offset drifts on `this` (BandCamShot's own guard bool + list, which live past CamShot's inherited region): `… |
| StreamReceiver360 | `?Poll@StreamReceiver360@@UAAXXZ` | high | 3 of 4 mismatches are `lis r11,0x1`/`ori r11,r11,0x803c` (target, i.e. absolute this-relative offset 0x1803c) vs `lis r11,0x0`/`ori r11,r11,… |
| band3/game/Player | `?SetQuarantined@Player@@UAAX_N@Z` | high | Single mismatch: `stb r11, 0x2fc, r31` (target/retail) vs `stb r11, 0x2f8, r31` (base/ours) -- both storing the tail `li r11,0x0` (false) in… |
| CharIKHead | `?UpdatePoints@CharIKHead@@IAAX_N@Z` | high | All 3 scored mismatches are the SAME constant used 3 different ways: `li r26, 0x24` (target=36) vs `li r26, 0x34` (base=52) as the divisor i… |
| SongCollision | `?_M_fill_insert_aux@?$vector@UBeatCollisionData@@V?$StlNod` | high | 13 diff_arg instructions all differ by a uniform delta: target's `li r5,0x38` / `mulli r27,r27,0x38` / `subi r31,r31,0x38` / `addi r31,r31,0… |
| SkeletonClip | `?_M_fill_insert_aux@?$vector@URecordedFrame@@V?$StlNodeAll` | high | Every one of the 13 real mismatches is the same constant, 336 bytes, appearing consistently: stack frame `stwu r1,-0x120,r1` (base/ours) vs … |
| RhythmDetector | `??$__uninitialized_copy@PAUFrame@RhythmDetector@@PAU12@@st` | high | 24-instr templated copy loop over RhythmDetector::Frame*. `addi r29,r29,0x14` / `addi r30,r30,0x14` (target, both pointer advances) vs `addi… |
| CharClip | `??4?$vector@V?$map@HMU?$less@H@stlpmtx_std@@V?$StlNodeAllo` | high | 8 mismatches, all the same constant swap: `li r30, 0x18`/`mulli r11,r25,0x18`/`addi r28,r28,0x18` (target/retail) vs `0x1c` (ours) -- sizeof… |
| HamNavProvider | `?push_back@?$vector@UNavItem@HamNavProvider@@V?$StlNodeAll` | high | Single diff_arg: after the copy-construct call, `addi r11,r11,0xc`(target/retail) vs `addi r11,r11,0x28`(base/ours) -- this is `finish += si… |
| VorbisReader | `?DoFileRead@VorbisReader@@AAA_NXZ` | high | 5 diff_arg, all the same field accessed via lbz/stb: `0x11c(r3)`(target/retail) vs `0xef(r3)`(base/ours), a consistent Δ=0x2d=45 bytes. look… |

## Notable single findings

- **BandDirector::SyncProperty** — the known local-static Symbol lever: retail
  pre-interns `bass/drum/guitar/mic/keyboard` as function-local statics in the
  SYNC_PROP_SET block (src/system/bandobj/BandDirector.cpp ~1817); fix sketch in
  the NDJSON row. Likely cascades across BandDirector's 42-fn near-miss cluster.
- **HamCamTransform** — pairing artifact: `scripts/target_symbol_map.json` maps a
  callee VA to `ScrollAnims::operator=` inside a TransformArea template; map
  recheck, not code.
- **BandSongMgr::IsInExclusionList** — single-constant diff (CONSTANT_DIFF),
  cheap flip.
