# W16-LA — bandobj + char anonymous rows: 239 named, two classes written, six template families re-typed (2026-10-01)

**Branch** `w16-la`, rebased onto main `e040b3c2e` (after W16-LD). Not merged.
**Ruler** `name_check` (graded; `report.json` `provenance.diff_config`).
**Scope** the anonymous `fn_` rows at fuzzy 0 in every unit whose source is under
`src/system/bandobj/` or `src/system/char/`: **445 rows / 65,668 B** at main `9d627e235`.
W16-JB (`W16JB_BANDOBJ_CHAR_SWEEP_2026-10-01.md`) and W16-KA left them unnamed for lack of
a caller or vtable witness; the passes below use evidence those lanes did not exhaust.

## 1. Whole-branch A/B

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-la-ab --patch <w16-la-ab2..w16-la, docs excluded>`,
one run, fresh worktree at main `e040b3c2e`. `ab_measure` refuses a patch that touches
`symbols.txt`, so leg A's base commit carries the branch's `symbols.txt` hunk (over-carve
merges plus the three carve fixes of §6) and the same-unit join of CharDriver's two `.text`
blocks (the grown `Starved` carve would otherwise straddle the boundary) — W16-HZ's recipe.
Leg B settled after 2 iterations (530 recompiles in the first). Both legs at a split fixed
point. Run dir archived to `~/tmp/w16la/ab_run/20261001-164140-w16-la-branch-on-ld-3531303/`.

```
leg A: matched=49656 masked=24337 honest=25319 code%=51.203156
leg B: matched=49957 masked=24398 honest=25559 code%=51.628090
Δmatched=+301  Δmasked_equal=+61  Δhonest=+240  Δcode%=+0.424934pp  Δcode_bytes=+43544
Δfuzzy=+0.486843pp   (legA 58.784412 -> legB 59.271255)
units at 100% [mpn]: 391 -> 395 (BandSwatch, CharFaceServo, CharInterest, Label3d; 0 fell off)
units at 100% [all-rows-fuzzy]: 317 -> 319 (BandSwatch, Label3d; 0 fell off)
[control none] +44,036 B -- NOT_APPLICABLE (source in patch)
```

**Prediction, written before the run:** +301 functions / ≈ +43.5 KB — the sum of the
per-step in-tree deltas (49,553 → 49,854 fns, 5,216,756 → 5,260,340 B before the first
rebase). **Measured: +301 / +43,544 B.** The same patch measured identically on main
`d183547ca` (W16-LC) before main moved again (run `…163449-w16-la-branch-3469270`).

**Row-level diff of the two archived legs: 66 rows up, 0 down, 0 off 100.** 340 GONE/NEW
pairs are names and re-homes. Three rows at 100 leave under their old names and read 100
under the corrected ones at the same addresses (§4): `_M_fill_insert<ObjPtr<RndPartLauncher>>`
→ `<ObjPtr<RndAnimatable>>`, `_M_fill_insert<ObjPtr<RndPropAnim>>` → `<ObjPtr<EventTrigger>>`,
`pair<const String,DataNode>` ctor → `pair<ObjPtr<EventTrigger>,ObjPtr<EventTrigger>>` ctor.

## 2. Outcome for the 445 starting rows

| state now | rows | bytes |
|---|---:|---:|
| named | 239 | 44,592 |
| folded into a neighbour by an over-carve merge | 14 | 1,012 |
| still anonymous | 192 | 20,064 |

Named rows: 205 at 100, 21 at 99–100, 4 at 90–99, 9 below 90; 71 re-homed. Full table §12.
Remaining anonymous fuzzy-0 rows in scope: **177 / 19,316 B** (some start rows left scope by
re-home, some out-of-scope rows entered it).

## 3. Identification

### 3.1 Aligned caller binding (any caller score)
For every retail `bl` into a population row from a **named** caller, objdiff's instruction
alignment of that caller pairs the retail placeholder with the callee our source calls at the
same instruction. JB's binding needed callers at `mpn == 100` with equal size; this one takes
any aligned `bl` row. Rows whose named callers spell **two** names are ICF folds and were not
named. 219 of 445 had a named caller; classes: own-unit 90, foreign 46, no base definition 23,
mapped elsewhere 18, multi 34.

- **Own unit:** 79 named in place. Three replaced null pins from `be7cfdb4a` (DC3 class names).
- **Over-carve heads** (head falls through into contiguous anonymous successors; W16-HZ pass F):
  `IsLoading`, `Starved`, `FindTarget`, `Outfit::operator=`, `__median<ObjOwnerPtr<CharClip>>`.
- **Foreign** (name defined by the unit owning the neighbouring pins): 23 ranges re-homed, e.g.
  `RndText::SetText` BandCharDesc → Text, `FingerShape::UpdateAnim/UpdateFretNumber` BandTrack →
  FingerShape, the `ObjOwnerPtr<CharClip>` helpers CharMeshHide → CharClipGroup,
  `ObjVector<BandPatchMesh>::resize` and `operator>>` BandSwatch → OutfitConfig.
- **No base definition** (declared in our headers, never written): 17 named first, bodies written
  by the forks (§5): `CharCollide::Deform` (re-homed out of CharIKScale), `CharCuff::Deform`,
  `CharClip::MakeMRU/InGroup`, eight `ArpeggioShape` methods + `ArpeggioShapePool::Release`,
  `CharMeshHide::HideAll`, `CharBoneOffset::ApplyToLocal` (out of CharFaceServo),
  `Character::RepointSphereBase`, `BandFaceDeform::DeltaArray::Save`.
- **Withdrawn in-tree:** `0x823C3828` (callers spell two list clears; naming it took
  `_Destroy<ConstraintSystem>` 100 → 95).

### 3.2 Neighbour-unit body rank (W16-HZ pass G, scoped here for the first time)
Pre-registered: each anonymous row ≥ 48 B against the unclaimed functions of its own unit's
object and of the units owning the nearest pinned blocks either side (≤ 0x2000), 0.5–2× size;
top 5 by opcode-sequence similarity, then the graded ruler; accept iff top ≥ 60 and
top − runner-up ≥ 15. 35 passed; refused 5 whose named callers spell two names (folds) or
whose top name appeared twice. **19 named, 9 re-homed.**

**A body score cannot name a destructor.** `0x823B0048` ranked `~FlowSwitchCase` (84.5) over
`~CharPollGroup` (56.1), but every vtable it stores resolves through RTTI to `CharPollGroup`.
Every accepted dtor, `??_G` and `??_D` was checked the same way
(`~/tmp/w16la/vtstores.py`: vtables stored + callees).

### 3.3 Classes our tree compiled nowhere
- **PatchRenderer** (fork F5): all of `0x822AE130–0x822AF1C8`, sitting in BandSwatch's span.
  Written as `bandobj/PatchRenderer.cpp`, `#include`d into BandSwatch.cpp. Removed
  BandSwatch's `ForceEmit_PatchRenderer_StaticClassName` shim and Band.cpp's stub class.
- **Label3d** (fork F6): RTTI `Label3d : RndTransformable, RndDrawable`, size 0x15C; code
  `0x822EFBE0–0x822F18A8` between TransConstraint and PitchArrow, pinned to CharBoneDir
  (whose code is at `0x823B7xxx`). New TU `bandobj/Label3d.{h,cpp}` with its own splits heading;
  member offsets from ctor/Save/PreLoad, property names from SyncProperty's strings. BandInit
  registers Label3d (retail `0x8227ACC8` builds "Label3d"), where ours registered ObjectDir.
  Non-virtual helper names (`SyncText`, `LoadResource`, `MeshXExtent`) are descriptive.
  All 58 Label3d rows read 100.

## 4. Existing map names that were wrong (corrected on retail bytes)

The ruler forgives a placeholder operand, so a template body scores 100 under the wrong `T`
whenever the only `T`-specific operands are unnamed. The instruments that decide `T`:
element-size immediates (`li/mulli/addi` step), the vtable an element copy-ctor stores, and
the `~X` a `_M_fill_insert_aux` calls.

- **OutfitConfig span, mapped `Character::Lod` (0x1C) since the TU5 flip:** `0x822A4FB8`,
  `0x822A3518`, `0x822A6028` step 0xC and copy-construct `ObjPtr<RndTex>`;
  `0x822A6AB0` builds/destroys an `ObjPtr<RndTex>` temp → the `vector/ObjVector<ObjPtr<RndTex>>`
  family. `0x822A3F58` steps 0x20 calling `~Piercing::Piece`; `0x822A54B8` destroys a
  `vector<Piece>` at +0x50 then an `ObjPtr<RndTransformable>` (`~Piercing`). This lane's own
  caller binding had named `0x822A3518` `vector<Lod>` by inheriting its caller's wrong name.
- **StreakMeter / BandStarDisplay span, three interleaved `vector<ObjPtr<T>>` families:**
  `ObjPtr<RndAnimatable>` (copy-ctor `0x822CB430`, mapped as RndPartLauncher — by this lane),
  `ObjPtr<EventTrigger>` (`0x822CBA60` etc., mapped RndPropAnim; copy-ctor `0x822B1728`),
  `ObjPtr<RndPropAnim>` (`0x822D8D78` was mapped `vector<InlineHelp::ActionElement>` but steps
  12 B) and `ObjPtr<RndPartLauncher>` (`0x822D8460` was mapped `vector<SongPattern>`).
  `0x822E4F70` (mapped `pair<const String,DataNode>` ctor) calls the EventTrigger copy-ctor twice.
  The real ActionElement instance is `0x82315DF0`.
- **PatchRenderer thunks** (F5): `0x822AE7A0` was BandSwatch's Save thunk but is slot 0 of
  PatchRenderer's vtable (BandSwatch's is `0x822ADBA0`); `0x822AE780`, `0x822AE440` likewise.
- **Label3d thunks** (F6): `0x822F0EF8/1080/1090/10B0` carried CharBoneTwist/CharBoneDir names but
  branch into Label3d; those names moved to `0x823B7FB0/85A0/7958/71D0`, which do.
- **F3/F4:** `0x822873D0` (mapped `__uninitialized_copy<GemTrack::RangeShift>`) and `0x823D1AF0`
  are `ObjDirItr<CharClip>` / `<CharClipGroup>::operator++` (their Advance dynamic-casts to the
  class); `0x82356230` (mapped `ObjDirItr<ObjectDir>::operator++`) is
  `ArpeggioShapePool::GetArpeggioShape`; `0x822C71E8` is `DeltaArray::Save`, not `operator<<`;
  `0x82287F00` is `OnHackFixClipsPreMerge` ("toggle_interests_overlay" is absent from retail).

## 5. Body work (four forks per file group, then mine)

| owner | rows to 100 / notable | highlights |
|---|---|---|
| F1 ChordShapeGenerator | ConnectVertProfiles, BuildChordMesh(uint,int); BuildContourCap 62.7 → 99.97 | colors are `Hmx::Color` by value (five mangled names change); BuildEndCap rewritten; CrossSec::AddEdge out of line |
| F2 char | MakeMRU, InGroup, HideAll, ApplyToLocal, RepointSphereBase, OnCopyBoundingSphere; CharCollide::Deform 0 → 97.3, CharCuff::Deform 0 → 99.98 | DeformMesh + recursive AddBoneChildren; `/DRB3_NOTIFY_ONCE_EVAL` for CharCollide |
| F3 bandobj | SavePrefabFromCloset, HeadNormVariant, ComputeDeformWeights, BandCamShot ctor/dtor, ~Outfit, NameToDrumVenue | BandCharacter's hiding NameToDrumVenue redeclaration removed |
| F4 bandobj | MeshAO::Apply, (Un)SwapResource, PropSync(Piercing), ~OutfitConfig, 7 ArpeggioShape methods, FingerShape | RndGroup::AddObjectAtFront, RndMatAnim::SetMat added to the engine |
| F5, F6 | PatchRenderer, Label3d | §3.3 |
| this lane | CharLipSync::PlayBack, UILabel::CopyMembers, CharInterest::ComputeScore, ~NoteTube, CharBonesMeshes::Replace, ~CharPollGroup, CharEyes (EyeDesc ctor, OnAddInterest, +9 funclets), GetPlayerDifficultySym | below |

- **CharLipSync::PlayBack:** retail layout is `vector 0x0 · mLipSync 0xc · RndPropAnim* 0x10 ·
  ObjectDir* mClips 0x14 · mIndex 0x18 · mOldIndex 0x1c · mFrame 0x20` (the out-of-line ctor
  zero-stores with no vtable store; ours had an `ObjPtr<ObjectDir>`). `Set`/`Poll` gain the
  prop-anim branch (one weight per PropKeys, sampled by vtable slot 0x60); no `viseme_list`
  message; `SetClips` had no retail counterpart. Reset → 100, Poll 30.7 → 99.2, Set 0 → 88.9.
- **Static-const lever** (from a sibling lane): naming the 0.1 band tolerance a
  `static const float` restores retail's per-compare reload: GetCrossSection 96.08 → 99.95,
  BuildContourCap 97.58 → 99.97, BuildEndCap 92.26 → 96.87.
- **ObjOwnerPtr owner-only inline** in CharEyes.cpp (`RB3_OBJOWNERPTR_INLINE_OWNER_CTOR` +
  `RB3_TU_OBJPTR_OWNER_CTOR_DEFER_OBJECT`): +15 fns.

## 6. Carve fixes (symbols.txt)
An instruction left in no function between a head and its tail:
`StreakMeter::Overdrive` 0x822D6338 is 0x14 B (bctr carved off as `fn_822D6348`),
`CharDriver::Starved` 0x82376D80 is 0x40 B, `CharMeshHide::HideDraws` 0x823A0AA0 is 0x78 B.

## 7. Aliases
- **Added** (each re-run here with `tools/icf_pair_adjudicate.py --chase`, 0 CYCLE leaves;
  logs `~/tmp/w16la/adj_deleteall.log`, `adj_label3d_del.log`):
  `ObjPtrList<RndMesh>::DeleteAll` → `ObjPtrList<RndTransformable>::DeleteAll` @0x822F0408
  (FLAT T1 PROVEN, 116/116 B; CHASED T1 PROVEN); `??3Label3d` into the `??3BinStream` group
  (FLAT UNDECIDABLE, CHASED T1 PROVEN vacuous-but-identical).
- **Refused:** `set<unsigned short>::clear` and `map<unsigned short,unsigned short>::clear` into
  the `_Rb_tree<Symbol>::clear` group @0x822DEA78. Both reach CHASED T1 PROVEN only through a
  CYCLE-ASSUMED `_M_erase` leaf (it recurses), the same reason W16-IC's `set<int>` membership was
  withdrawn. That name is the last charge on GetCrossSection (424 B) and part of the residue on
  ExtendProfile, AddVertProfile, BuildContourCap.
- **Withdrawn name:** `0x822C9048` (`_Copy_Construct<MidiParser::VocalEvent>`, body rank) —
  `--validate` rejects it: that spelling is a folded member of the group at 0x827EB7E0.

## 8. Behaviour fixes
UILabel::CopyMembers now copies `mEditText`; PlayBack drives prop-anim lipsyncs; BandCamShot's
ctor sets CamShot near/far 10/10000; BandCharDesc::ComputeDeformWeights and HeadNormVariant,
SavePrefabFromCloset, the six `nodef` bodies and both new classes now do what retail does
instead of nothing; ChordShapeGenerator's contour cap takes the fret height and spans take
colors in `(colPrev, col)` order; the native build's inert deformation stubs are gone.

## 9. Rows left, by blocker
- **Network code in char/bandobj pins** (~1.7 KB with RTTI owners `XboxSession`, Quazal
  `_DOC_MessageBroker`/DDL declarations, `FriendsProvider`, `DeleteSessionJob`, `Net`):
  `0x823F0310–0x823F0A68` (CharIKSliderMidi), `0x823F4168–0x823F4590` (TrackPanelDir),
  `0x823F6380–0x823F64AC` (ClipCollide), `0x826664A8` (BandLabel). Out of this lane's scope;
  the classes are absent or not compiled.
- **Template folds whose callers spell several T:** `ObjPtrList<CharCollide>::sort`,
  `~ObjOwnerPtr<CharWeightable>`, `ObjVector<ObjOwnerPtr<Waypoint>>::resize` (×2 addresses),
  `__uninitialized_fill_n<Edge>`, `fn_8227D0E8` (`ObjPtrList::Replace`) — need proven aliases.
- **328-B `_M_insert_overflow_aux` rows not yet re-typed:** `fn_823DBB78` (ObjOwnerPtr<Waypoint>
  by its callees, but its caller is mapped `vector<ObjPtr<SeqInst>>`), `fn_8237BEC8`,
  `fn_82371D20` (Lod, now free), `fn_82308128` (ObjPtr<BandTrack>), `fn_823A8CC0`
  (CharHair::Point; `0x8263B468` is mapped with that name but steps 60 B and copy-constructs
  `StoreMainPanel::NewReleaseEntry`), `fn_822A7DC8` (MeshAO), `fn_822A3930`, `fn_822A5768`,
  `fn_822CABD0`. Each needs the same family correction as §4.
- **Residue:** CharCollide::Deform 97.33 (register/induction choice), PlayBack::Set 88.9
  (a stack home for the prop-anim pointer), GemTrackDir::SetScreenRectX 93.6 (signed compare on
  the camera pointer), InterpolateXfm 89.3, BuildEndCap 96.87, ReplaceRefs 99.97 (.bss order),
  DeformTri::Contains 99.71 (fmsubs operand order), SetHeadNormMap 92.45.

## 10. Gates (rebased tip `2e4598070`, full build, forced re-split)
- `python3 tools/map_name_injectivity.py`: **OK**, 33,156 applied rows, 33,155 distinct names,
  injective (+1 enumerated internal-linkage exception).
- `python3 tools/icf_alias_finder.py --validate`: **PASS**, 1,645 map-consistent, 277 tolerated,
  **0 contradicted**, 1,923 total.
- `tools/native_build_gate.sh` (run last; only this docs commit follows it):
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.
  An earlier run on this branch
  failed 16/18 on fork changes (inert native stubs duplicating the new bodies; a removed
  redeclaration; an undeclared `hack_fix_clips_pre_merge` Symbol) and was fixed in `0cf0bf38e`.
- `scripts/verify_objs_patched.py --verify-manifest`: OK.

## 11. Traps met
- **Caller binding inherits the caller's wrong name.** A template caller mapped with the wrong
  `T` propagates that `T` to its callees; check element-size immediates and stored vtables
  before trusting a template name (both of this lane's own mis-names came this way).
- **A correction cascades through a family.** Renaming one member charges its callers that still
  spell the old `T`; each such dip was another member of the same family (five dips, all fixed).
- **Funclets byte-pair to any same-unit funclet**, so re-homing rows can unpair a funclet left
  behind (`fn_822CC714` 93 → 0 until its block moved too).
- **A bare `#include "Memory.h"` resolves into `xdk/LIBCMT`** case-insensitively and records a
  dependency that does not exist on Linux: BandSwatch.obj recompiled on every build and
  `ab_measure` could not settle leg B. Fixed with `"../../Memory.h"`.
- **Helper scripts that import `tools/anon_proposal_adjudicate.py` `chdir` to its repo root.**
- **zsh does not word-split `$VAR` in `for f in $VAR`** (rebase-loop script).

## 12. Rows named (start population)

See the table below (generated from the final build; `after` = fuzzy on the graded ruler).

| after | rows |
|---|---:|
| 100 | 205 |
| 99-100 | 21 |
| 90-99 | 4 |
| <90 | 9 |

| retail row | size | name | unit before → after | after |
|---|---:|---|---|---:|
| `0x822e1b18` | 1752 | `?BuildContourCap@ChordShapeGenerator@@QAAXPAVRndMesh@@AAV?$map@GGU?$less@G@stlpmtx_std@...` | ChordShapeGenerator | 99.97 |
| `0x822f0568` | 1524 | `?SyncText@Label3d@@QAAXXZ` | CharBoneDir → Label3d | **100** |
| `0x8239ec40` | 1364 | `?DeformMesh@CharCuff@@QAAXPAVRndMesh@@HPAVSyncMeshCB@@@Z` | CharCuff | 99.40 |
| `0x822e1560` | 1352 | `?BuildEndCap@ChordShapeGenerator@@QAAXPAVRndMesh@@AAV?$map@GGU?$less@G@stlpmtx_std@@V?$...` | ChordShapeGenerator | 96.87 |
| `0x8239ad88` | 1052 | `?Deform@CharCollide@@QAAXXZ` | CharIKScale → CharCollide | 97.33 |
| `0x8239f1a0` | 1004 | `?Deform@CharCuff@@QAAXPAVSyncMeshCB@@PAVFileMerger@@@Z` | CharCuff | 99.98 |
| `0x822f13a8` | 828 | `?SyncProperty@Label3d@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | CharBoneDir → Label3d | **100** |
| `0x822824f8` | 820 | `?SavePrefabFromCloset@BandCharacter@@QAA?AVDataNode@@PBD@Z` | BandCharacter | **100** |
| `0x82459ba8` | 720 | `?SetText@RndText@@QAAXPBD@Z` | BandCharDesc → Text | 64.36 |
| `0x822e25a8` | 680 | `?BuildSpan@ChordShapeGenerator@@QAAXPAVRndMesh@@AAV?$map@GGU?$less@G@stlpmtx_std@@V?$St...` | ChordShapeGenerator | 99.76 |
| `0x82287f00` | 660 | `?OnHackFixClipsPreMerge@BandCharacter@@QAA?AVDataNode@@PAVDataArray@@@Z` | BandCharacter | **100** |
| `0x823ddfc0` | 584 | `?Load@CharacterTest@@QAAXAAVBinStream@@@Z` | CharacterTest | **100** |
| `0x822aedc8` | 560 | `?SyncProperty@PatchRenderer@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | BandSwatch | **100** |
| `0x82355c18` | 504 | `??0ArpeggioShape@@QAA@PAVRndGroup@@PAVRndMesh@@PAVRndText@@2PAVRndMat@@PAVRndMatAnim@@@Z` | GemTrackResourceManager | **100** |
| `0x823bd7a8` | 472 | `?ComputeScore@CharInterest@@QAAMABVVector3@@00MH_N@Z` | CharInterest | **100** |
| `0x8229fd60` | 420 | `?Apply@MeshAO@OutfitConfig@@QAAXPAV2@PAVSyncMeshCB@@@Z` | OutfitConfig | **100** |
| `0x822f0b90` | 416 | `??0Label3d@@QAA@XZ` | CharBoneDir → Label3d | **100** |
| `0x823d5668` | 408 | `?Set@PlayBack@CharLipSync@@QAAXPAV2@PAVObjectDir@@@Z` | CharLipSync | 88.88 |
| `0x827f4778` | 408 | `?CopyMembers@UILabel@@UAAXPBVUIComponent@@W4CopyType@Object@Hmx@@@Z` | VocalTrackDir → UILabel | **100** |
| `0x822ae4b0` | 396 | `??0PatchRenderer@@QAA@XZ` | BandSwatch | **100** |
| `0x822a2a20` | 392 | `?_M_fill_insert_aux@?$vector@V?$ObjPtr@VRndTex@@@@V?$StlNodeAlloc@V?$ObjPtr@VRndTex@@@@...` | OutfitConfig | **100** |
| `0x822d82a8` | 392 | `?_M_fill_insert_aux@?$vector@V?$ObjPtr@VRndPropAnim@@@@V?$StlNodeAlloc@V?$ObjPtr@VRndPr...` | StreakMeter | **100** |
| `0x822c27b0` | 388 | `?ApplyPosConstraints@BandIKEffector@@IAAMAAVVector3@@ABV2@PAV1@@Z` | BandIKEffector | 82.40 |
| `0x822b6928` | 376 | `??0BandCamShot@@QAA@XZ` | BandCamShot | **100** |
| `0x822a8118` | 360 | `?PropSync@@YA_NAAVPiercing@OutfitConfig@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | OutfitConfig | **100** |
| `0x822e01d0` | 360 | `?AddVertProfile@ChordShapeGenerator@@QAAXPAVRndMesh@@ABVTransform@@MABVCrossSec@1@AAV?$...` | ChordShapeGenerator | 99.94 |
| `0x82398100` | 344 | `??4?$vector@UIKTarget@CharIKHand@@V?$StlNodeAlloc@UIKTarget@CharIKHand@@@stlpmtx_std@@@...` | FileMerger | 99.88 |
| `0x8235f408` | 336 | `?_M_insert_overflow_aux@?$vector@V?$set@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@V?$StlN...` | CharClip → Tour | 99.76 |
| `0x822f0240` | 332 | `?Handle@Label3d@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | CharBoneDir → Label3d | **100** |
| `0x82298268` | 328 | `?_M_insert_overflow_aux@?$vector@UPropertyFilter@CameraManager@@V?$StlNodeAlloc@UProper...` | BandDirector | **100** |
| `0x822a3518` | 328 | `?_M_insert_overflow_aux@?$vector@V?$ObjPtr@VRndTex@@@@V?$StlNodeAlloc@V?$ObjPtr@VRndTex...` | OutfitConfig | **100** |
| `0x822a9450` | 328 | `?_M_insert_overflow_aux@?$vector@VPiercing@OutfitConfig@@V?$StlNodeAlloc@VPiercing@Outf...` | OutfitConfig | 99.94 |
| `0x822d8f20` | 328 | `?_M_insert_overflow_aux@?$vector@V?$ObjPtr@VRndPartLauncher@@@@V?$StlNodeAlloc@V?$ObjPt...` | StreakMeter | **100** |
| `0x82315df0` | 328 | `?_M_insert_overflow_aux@?$vector@UActionElement@InlineHelp@@V?$StlNodeAlloc@UActionElem...` | CrowdAudio | **100** |
| `0x82374060` | 324 | `?_M_insert_overflow_aux@?$vector@V?$ObjVector@ULod@Character@@@@V?$StlNodeAlloc@V?$ObjV...` | Character | **100** |
| `0x822ae7b8` | 316 | `?SetType@PatchRenderer@@UAAXVSymbol@@@Z` | BandSwatch | **100** |
| `0x822f0f20` | 316 | `?SetType@Label3d@@UAAXVSymbol@@@Z` | CharBoneDir → Label3d | **100** |
| `0x82354cf0` | 316 | `?UpdateFretNumber@FingerShape@@QAAXABVRGState@@_N@Z` | system/bandobj/BandTrack → FingerShape | **100** |
| `0x822efe58` | 288 | `?Save@Label3d@@UAAXAAVBinStream@@@Z` | CharBoneDir → Label3d | **100** |
| `0x8237e118` | 284 | `?MakeMRU@CharClip@@QAAXXZ` | CharClip | **100** |
| `0x8229f018` | 264 | `?UnSwapResource@MatSwap@OutfitConfig@@QAAXXZ` | OutfitConfig | **100** |
| `0x8229f148` | 252 | `?SwapResource@MatSwap@OutfitConfig@@QAAXXZ` | OutfitConfig | **100** |
| `0x822f10e8` | 252 | `??1Label3d@@UAA@XZ` | CharBoneDir → Label3d | **100** |
| `0x823855c8` | 252 | `?DartUpdate@CharEyes@@IAAXXZ` | CharEyes | 51.94 |
| `0x8238df40` | 244 | `?Load@?$ObjOwnerPtr@VCharClip@@@@QAA_NAAVBinStream@@_NPAVObjectDir@@@Z` | CharMeshHide → CharClipGroup | **100** |
| `0x82281f00` | 236 | `?ReplaceRefs@@YAXPAVObject@Hmx@@0@Z` | BandCharacter | 99.97 |
| `0x82355e10` | 236 | `?SetChordLabel@ArpeggioShape@@QAAXABVString@@M_N@Z` | GemTrackResourceManager | **100** |
| `0x822ae358` | 232 | `?DrawAfter@PatchRenderer@@UAAXXZ` | BandSwatch | **100** |
| `0x822cda80` | 232 | `?Load@?$ObjPtr@VBandStarDisplay@@@@QAA_NAAVBinStream@@_NPAVObjectDir@@@Z` | BandStarDisplay → BandScoreboard | **100** |
| `0x823cd0b0` | 232 | `?Load@?$ObjPtr@VCharServoBone@@@@QAA_NAAVBinStream@@_NPAVObjectDir@@@Z` | CharDriverMidi → CharMirror | **100** |
| `0x82335810` | 228 | `?HeadNormVariant@BandCharDesc@@QAAPBDXZ` | BandCharDesc | **100** |
| `0x82354bc8` | 228 | `?UpdateAnim@FingerShape@@QAAXPAVRndAnimatable@@M_N@Z` | system/bandobj/BandTrack → FingerShape | **100** |
| `0x822f0480` | 224 | `?PreLoad@Label3d@@UAAXAAVBinStream@@@Z` | CharBoneDir → Label3d | **100** |
| `0x8237b338` | 216 | `?Replace@CharBonesMeshes@@MAAXPAVObjRef@@PAVObject@Hmx@@@Z` | CharBonesMeshes | **100** |
| `0x822839f8` | 212 | `?Advance@?$ObjDirItr@VCharClip@@@@AAAXXZ` | BandCharacter | **100** |
| `0x8229ff30` | 212 | `?SetHeadNormMap@@YA_NPBDHVSymbol@@PAVObjectDir@@2@Z` | OutfitConfig | 92.45 |
| `0x822e37a0` | 212 | `?SetScreenRectX@GemTrackDir@@QAAXM@Z` | GemTrackDir | 93.60 |
| `0x8239d678` | 212 | `?Advance@?$ObjDirItr@VCharCuff@@@@AAAXXZ` | CharCuff | **100** |
| `0x823d13e8` | 212 | `?Advance@?$ObjDirItr@VCharClipGroup@@@@AAAXXZ` | CharClipSet | **100** |
| `0x823557d8` | 208 | `?HookupToParentGroup@ArpeggioShape@@QAAXXZ` | GemTrackResourceManager | **100** |
| `0x823558a8` | 208 | `?UnhookFromParentGroup@ArpeggioShape@@QAAXXZ` | GemTrackResourceManager | **100** |
| `0x823aba10` | 208 | `?ChannelName@CharBones@@SA?AVSymbol@@PBDW4Type@1@@Z` | CharHair → CharBones | **100** |
| `0x822c93e0` | 204 | `??$PropSync@VRndAnimatable@@@@YA_NAAV?$ObjPtr@VRndAnimatable@@@@AAVDataNode@@PAVDataArr...` | CharCollide → BandLeadMeter | **100** |
| `0x8238e1e0` | 200 | `??$__unguarded_linear_insert@PAV?$ObjOwnerPtr@VCharClip@@@@V1@UAlphabetically@@@stlpmtx...` | CharMeshHide → CharClipGroup | **100** |
| `0x822f0000` | 196 | `?MeshXExtent@Label3d@@QAAXAAM0PAVRndMesh@@@Z` | CharBoneDir → Label3d | **100** |
| `0x822aeaf8` | 192 | `?Handle@PatchRenderer@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | BandSwatch | **100** |
| `0x822efd98` | 192 | `?Copy@Label3d@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z` | CharBoneDir → Label3d | **100** |
| `0x8238da40` | 192 | `??$__find@PBV?$ObjOwnerPtr@VCharClip@@@@PAVCharClip@@@stlpmtx_std@@YAPBV?$ObjOwnerPtr@V...` | CharLipSyncDriver → CharClipGroup | **100** |
| `0x822ae298` | 188 | `?DrawBefore@PatchRenderer@@UAAXXZ` | BandSwatch | **100** |
| `0x822f0178` | 188 | `?GetDistanceToPlane@Label3d@@UAAMABVPlane@@AAVVector3@@@Z` | CharBoneDir → Label3d | **100** |
| `0x823c49d8` | 184 | `??4?$list@UConstraintSystem@CharBlendBone@@V?$StlNodeAlloc@UConstraintSystem@CharBlendB...` | CharIKRod → CharBlendBone | 99.89 |
| `0x82355a58` | 180 | `?FadeOutChordShape@ArpeggioShape@@QAAXXZ` | GemTrackResourceManager | **100** |
| `0x8238add8` | 180 | `?OnAddInterest@CharEyes@@IAA?AVDataNode@@PAVDataArray@@@Z` | CharEyes | **100** |
| `0x822a0870` | 176 | `?NumIndices@OutfitConfig@@QBAHH@Z` | OutfitConfig | **100** |
| `0x822af0a0` | 176 | `?Init@PatchRenderer@@SAXXZ` | BandSwatch | **100** |
| `0x822c71e8` | 176 | `?Save@DeltaArray@BandFaceDeform@@QBAXAAVBinStream@@@Z` | system/bandobj/BandFaceDeform | **100** |
| `0x823b82e8` | 168 | `?Load@CharBoneTwist@@UAAXAAVBinStream@@@Z` | CharBoneDir | **100** |
| `0x8229d2a0` | 164 | `??$Find@VRndTexBlendController@@@ObjectDir@@QAAPAVRndTexBlendController@@PBD_N@Z` | OutfitConfig | **100** |
| `0x822ae1a8` | 164 | `?Terminate@PatchRenderer@@SAXXZ` | BandSwatch | **100** |
| `0x822c5888` | 164 | `??$Find@VBandIKEffector@@@ObjectDir@@QAAPAVBandIKEffector@@PBD_N@Z` | BandIKEffector → BandRetargetVignette | **100** |
| `0x822c5930` | 164 | `??$Find@VCharPollGroup@@@ObjectDir@@QAAPAVCharPollGroup@@PBD_N@Z` | BandIKEffector → BandRetargetVignette | **100** |
| `0x822aed20` | 160 | `?Load@PatchRenderer@@UAAXAAVBinStream@@@Z` | BandSwatch | **100** |
| `0x82333418` | 160 | `??1Outfit@BandCharDesc@@UAA@XZ` | BandCharDesc | **100** |
| `0x823b0048` | 156 | `??1CharPollGroup@@UAA@XZ` | CharPollGroup | **100** |
| `0x82356050` | 152 | `?CreateArpeggioShape@ArpeggioShapePool@@QAAXXZ` | GemTrackResourceManager | 99.87 |
| `0x822a07d8` | 148 | `?NumColorOptions@OutfitConfig@@QBAHXZ` | OutfitConfig | **100** |
| `0x823559c0` | 144 | `?Reset@ArpeggioShape@@QAAXXZ` | GemTrackResourceManager | **100** |
| `0x82334250` | 140 | `?NameToDrumVenue@BandCharDesc@@SA?AVSymbol@@PBD@Z` | BandCharDesc | **100** |
| `0x82335380` | 140 | `?Contains@DeformTri@?A0x572d2021@@QBA_NABVVector2@@@Z` | BandCharDesc | 99.71 |
| `0x822aea68` | 136 | `?Save@PatchRenderer@@UAAXAAVBinStream@@@Z` | BandSwatch | **100** |
| `0x822eff78` | 136 | `?MakeWorldSphere@Label3d@@UAA_NAAVSphere@@_N@Z` | CharBoneDir → Label3d | **100** |
| `0x82384b68` | 136 | `??0EyeDesc@CharEyes@@QAA@PAVObject@Hmx@@@Z` | CharEyes | **100** |
| `0x8238e0a8` | 136 | `??$__median@V?$ObjOwnerPtr@VCharClip@@@@UAlphabetically@@@stlpmtx_std@@YAABV?$ObjOwnerP...` | CharMeshHide → CharClipGroup | **100** |
| `0x822ae9e0` | 132 | `?Copy@PatchRenderer@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z` | BandSwatch | **100** |
| `0x8239e290` | 132 | `?AddBoneChildren@@YAXAAV?$list@PAVRndTransformable@@V?$StlNodeAlloc@PAVRndTransformable...` | CharCuff | 99.85 |
| `0x823a45e0` | 132 | `?ApplyToLocal@CharBoneOffset@@QAAXXZ` | CharFaceServo → CharBoneOffset | **100** |
| `0x82355f00` | 128 | `?SetFretNumber@ArpeggioShape@@QAAXABVString@@ABVVector3@@@Z` | GemTrackResourceManager | **100** |
| `0x82287358` | 120 | `??0?$ObjDirItr@VCharClip@@@@QAA@PAVObjectDir@@_N@Z` | BandCharacter | **100** |
| `0x8229dc70` | 120 | `??0?$ObjPtr@VRndTransformable@@@@QAA@ABV0@@Z` | OutfitConfig | **100** |
| `0x822aba48` | 120 | `??1OutfitConfig@@UAA@XZ` | OutfitConfig | **100** |
| `0x822b1728` | 120 | `??0?$ObjPtr@VEventTrigger@@@@QAA@ABV0@@Z` | BandCamShot | **100** |
| `0x822cb430` | 120 | `??0?$ObjPtr@VRndAnimatable@@@@QAA@ABV0@@Z` | StreakMeter → BandStarDisplay | **100** |
| `0x822cd9b8` | 120 | `?Replace@?$ObjPtr@VBandStarDisplay@@@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z` | BandStarDisplay → BandScoreboard | **100** |
| `0x823254c0` | 120 | `??0?$ObjDirItr@VUILabel@@@@QAA@PAVObjectDir@@_N@Z` | PlayerDiffIcon | **100** |
| `0x82358738` | 120 | `??0?$ObjDirItr@VObject@Hmx@@@@QAA@PAVObjectDir@@_N@Z` | TrackPanelDirBase → Rot | **100** |
| `0x8239df78` | 120 | `??0?$ObjDirItr@VCharCuff@@@@QAA@PAVObjectDir@@_N@Z` | CharCuff | **100** |
| `0x823af230` | 120 | `?Load@CharWeightable@@UAAXAAVBinStream@@@Z` | CharPollGroup → CharWeightable | **100** |
| `0x823ccf40` | 120 | `?Replace@?$ObjPtr@VCharServoBone@@@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z` | CharDriverMidi → CharMirror | **100** |
| `0x822f12d8` | 116 | `?PostLoad@Label3d@@UAAXAAVBinStream@@@Z` | CharBoneDir → Label3d | **100** |
| `0x82332298` | 116 | `?push_back@?$vector@UMerger@FileMerger@@V?$StlNodeAlloc@UMerger@FileMerger@@@stlpmtx_st...` | BandWardrobe | **100** |
| `0x8227bd60` | 112 | `?NewObject@Label3d@@SAPAVObject@Hmx@@XZ` | BandCharacter | **100** |
| `0x8227f138` | 112 | `??0?$ObjPtr@VRndMesh@@@@QAA@PAVObject@Hmx@@PAVRndMesh@@@Z` | BandCharacter | **100** |
| `0x8228e530` | 112 | `??0?$ObjPtr@VRndPropAnim@@@@QAA@PAVObject@Hmx@@PAVRndPropAnim@@@Z` | BandDirector | **100** |
| `0x8229db58` | 112 | `??0?$ObjPtr@VRndTransformable@@@@QAA@PAVObject@Hmx@@PAVRndTransformable@@@Z` | OutfitConfig | **100** |
| `0x8229eaf0` | 112 | `??0?$ObjPtr@VRndDir@@@@QAA@PAVObject@Hmx@@PAVRndDir@@@Z` | OutfitConfig | **100** |
| `0x822aec18` | 112 | `?NewObject@PatchRenderer@@SAPAVObject@Hmx@@XZ` | BandSwatch | **100** |
| `0x822bed88` | 112 | `?_M_fill_insert@?$vector@V?$ObjPtr@VRndGroup@@@@V?$StlNodeAlloc@V?$ObjPtr@VRndGroup@@@@...` | BandCrowdMeter | **100** |
| `0x822fa160` | 112 | `??$?6HVColor@Hmx@@@@YAAAVBinStream@@AAV0@ABV?$map@HVColor@Hmx@@U?$less@H@stlpmtx_std@@V...` | VocalTrackDir | **100** |
| `0x826346e8` | 112 | `?_M_clear_after_move@?$vector@U?$pair@V?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmt...` | ClipDistMap → SetlistMergePanel | **100** |
| `0x822a6ba8` | 108 | `?_M_fill_insert@?$vector@VPiece@Piercing@OutfitConfig@@V?$StlNodeAlloc@VPiece@Piercing@...` | Character → OutfitConfig | **100** |
| `0x822a88a8` | 108 | `?resize@?$ObjVector@VPiece@Piercing@OutfitConfig@@@@QAAXI@Z` | OutfitConfig | **100** |
| `0x822aecb0` | 108 | `??_DPatchRenderer@@QAAXXZ` | BandSwatch | **100** |
| `0x822f17e8` | 108 | `??_DLabel3d@@QAAXXZ` | CharBoneDir → Label3d | **100** |
| `0x823a0d60` | 108 | `?HideAll@CharMeshHide@@SAXABV?$ObjPtrList@VCharMeshHide@@VObjectDir@@@@H@Z` | CharMeshHide | **100** |
| `0x823d35d0` | 108 | `?Reset@PlayBack@CharLipSync@@QAAXXZ` | CharLipSync | **100** |
| `0x8229eb90` | 104 | `??0?$ObjPtr@VRndMat@@@@QAA@ABV0@@Z` | OutfitConfig | **100** |
| `0x8229ec28` | 104 | `??0?$ObjPtr@VColorPalette@@@@QAA@ABV0@@Z` | OutfitConfig | **100** |
| `0x822ab2c8` | 104 | `?resize@?$ObjVector@VPiercing@OutfitConfig@@@@QAAXI@Z` | OutfitConfig | **100** |
| `0x82333988` | 104 | `??1InstrumentOutfit@BandCharDesc@@UAA@XZ` | BandCharDesc | **100** |
| `0x82355f80` | 104 | `?SetYPos@ArpeggioShape@@QAAXM@Z` | GemTrackResourceManager | **100** |
| `0x8236f230` | 104 | `?OnCopyBoundingSphere@Character@@IAA?AVDataNode@@PAVDataArray@@@Z` | Character | **100** |
| `0x8238deb0` | 104 | `??0?$ObjOwnerPtr@VCharClip@@@@QAA@ABV0@@Z` | CharMeshHide → CharClipGroup | **100** |
| `0x82274608` | 100 | `??0PatchSticker@@QAA@XZ` | PatchDir | 69.16 |
| `0x8228d400` | 100 | `??0?$ObjPtr@VObject@Hmx@@@@QAA@PAVObject@Hmx@@0@Z` | BandDirector | **100** |
| `0x8228e6f0` | 100 | `??0?$ObjPtr@VRndPostProc@@@@QAA@PAVObject@Hmx@@PAVRndPostProc@@@Z` | BandDirector | **100** |
| `0x8229d9c8` | 100 | `??0?$ObjPtr@VRndMat@@@@QAA@PAVObject@Hmx@@PAVRndMat@@@Z` | OutfitConfig | **100** |
| `0x8229ddc0` | 100 | `??0?$ObjPtr@VColorPalette@@@@QAA@PAVObject@Hmx@@PAVColorPalette@@@Z` | OutfitConfig | **100** |
| `0x822a7c78` | 100 | `??$_M_allocate_and_copy@PBVMatSwap@OutfitConfig@@@?$vector@VMatSwap@OutfitConfig@@V?$St...` | OutfitConfig | **100** |
| `0x822a8ee0` | 100 | `??$_M_allocate_and_copy@PBVPiercing@OutfitConfig@@@?$vector@VPiercing@OutfitConfig@@V?$...` | OutfitConfig | **100** |
| `0x822ad340` | 100 | `?Save@BandSwatch@@UAAXAAVBinStream@@@Z` | BandSwatch | **100** |
| `0x822cb100` | 100 | `??0?$ObjPtr@VSequence@@@@QAA@PAVObject@Hmx@@PAVSequence@@@Z` | StreakMeter → BandStarDisplay | **100** |
| `0x822cb1f8` | 100 | `??1?$ObjPtr@VSequence@@@@UAA@XZ` | StreakMeter → BandStarDisplay | **100** |
| `0x822e4470` | 100 | `??0?$ObjPtr@VChordShapeGenerator@@@@QAA@PAVObject@Hmx@@PAVChordShapeGenerator@@@Z` | GemTrackDir | **100** |
| `0x823134f8` | 100 | `?NewObject@CrowdAudio@@SAPAVObject@Hmx@@XZ` | CrowdAudio | **100** |
| `0x8234e0e0` | 100 | `??0?$ObjPtr@VTrackInterface@@@@QAA@PAVObject@Hmx@@PAVTrackInterface@@@Z` | system/bandobj/BandTrack | **100** |
| `0x823b60c0` | 100 | `??5@YAAAVBinStream@@AAV0@AAVRecenter@CharBoneDir@@@Z` | CharBoneDir | **100** |
| `0x82c29ac0` | 100 | `??1NoteTube@@UAA@XZ` | NoteTube | **100** |
| `0x82296b78` | 96 | `??$__uninitialized_copy@PAUPropertyFilter@CameraManager@@PAU12@@stlpmtx_std@@YAPAUPrope...` | BandDirector | 99.79 |
| `0x822ae448` | 96 | `?SetPatch@PatchRenderer@@QAAXPAVRndDir@@@Z` | BandSwatch | **100** |
| `0x822bda58` | 96 | `??$__uninitialized_fill_n@PAV?$ObjPtr@VEventTrigger@@@@IV1@@stlpmtx_std@@YAPAV?$ObjPtr@...` | BandCrowdMeter | **100** |
| `0x822cb190` | 96 | `?Replace@?$ObjPtr@VSequence@@@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z` | StreakMeter → BandStarDisplay | **100** |
| `0x822ccd68` | 96 | `?push_back@?$ObjVector@V?$ObjPtr@VEventTrigger@@@@@@QAAXABV?$ObjPtr@VEventTrigger@@@@@Z` | StreakMeter → BandStarDisplay | **100** |
| `0x822d8020` | 96 | `??$__uninitialized_fill_n@PAV?$ObjPtr@VRndPropAnim@@@@IV1@@stlpmtx_std@@YAPAV?$ObjPtr@V...` | StreakMeter | 99.79 |
| `0x822d80c8` | 96 | `??$__uninitialized_fill_n@PAV?$ObjPtr@VRndPartLauncher@@@@IV1@@stlpmtx_std@@YAPAV?$ObjP...` | StreakMeter | 99.79 |
| `0x822d8170` | 96 | `??$__uninitialized_copy@PAV?$ObjPtr@VRndPartLauncher@@@@PAV1@@stlpmtx_std@@YAPAV?$ObjPt...` | StreakMeter | 99.79 |
| `0x822e35d0` | 96 | `?SemitoneToWhiteKey@@YAHH@Z` | GemTrackDir | 45.79 |
| `0x82338730` | 96 | `??$__uninitialized_fill_n@PAVPatch@BandCharDesc@@IV12@@stlpmtx_std@@YAPAVPatch@BandChar...` | BandCharDesc | **100** |
| `0x82355fe8` | 96 | `?ReleaseArpeggioShape@ArpeggioShapePool@@QAAXAAPAVArpeggioShape@@@Z` | GemTrackResourceManager | 99.79 |
| `0x8236f1d0` | 96 | `?RepointSphereBase@Character@@QAAXPAVObjectDir@@@Z` | Character | **100** |
| `0x8238e6c0` | 96 | `??$__uninitialized_fill_n@PAV?$ObjOwnerPtr@VCharClip@@@@IV1@@stlpmtx_std@@YAPAV?$ObjOwn...` | CharClipGroup | **100** |
| `0x82397910` | 96 | `??$__uninitialized_fill_n@PAUIKTarget@CharIKHand@@IU12@@stlpmtx_std@@YAPAUIKTarget@Char...` | FileMerger | **100** |
| `0x823991d8` | 96 | `?resize@?$ObjVector@UIKTarget@CharIKHand@@@@QAAXI@Z` | CharIKHand | 73.54 |
| `0x823c46f8` | 96 | `?resize@?$ObjList@UConstraintSystem@CharBlendBone@@@@QAAXI@Z` | CharForeTwist → CharBlendBone | **100** |
| `0x823db4b8` | 96 | `??$__uninitialized_fill_n@PAV?$ObjOwnerPtr@VWaypoint@@@@IV1@@stlpmtx_std@@YAPAV?$ObjOwn...` | Waypoint | 99.79 |
| `0x82634800` | 96 | `??$__uninitialized_copy@PAU?$pair@V?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_st...` | ClipDistMap → SetlistMergePanel | **100** |
| `0x82280148` | 92 | `??$?6VBandCharDesc@@@@YAAAVBinStream@@AAV0@ABV?$ObjOwnerPtr@VBandCharDesc@@@@@Z` | BandCharacter | **100** |
| `0x822e1320` | 92 | `?AddEdge@CrossSec@ChordShapeGenerator@@QAAXABVEdge@2@@Z` | ChordShapeGenerator | 99.78 |
| `0x822f0118` | 92 | `?Highlight@Label3d@@UAAXXZ` | CharBoneDir → Label3d | **100** |
| `0x82320450` | 92 | `??_DScoreDisplay@@QAAXXZ` | CharacterTest → ScoreDisplay | **100** |
| `0x823c6318` | 92 | `??1CharForeTwist@@UAA@XZ` | CharIKRod → CharForeTwist | **100** |
| `0x8227acc8` | 88 | `?StaticClassName@Label3d@@SA?AVSymbol@@XZ` | CharBoneDir → Label3d | **100** |
| `0x822a0130` | 88 | `??6@YAAAVBinStream@@AAV0@ABVOverlay@OutfitConfig@@@Z` | OutfitConfig | **100** |
| `0x822ac0b0` | 88 | `??$?5VOldColorOption@@@@YAAAVBinStream@@AAV0@AAV?$ObjVector@VOldColorOption@@@@@Z` | OutfitConfig | **100** |
| `0x822aca98` | 88 | `??$?5VBandPatchMesh@@@@YAAAVBinStream@@AAV0@AAV?$ObjVector@VBandPatchMesh@@@@@Z` | BandSwatch → OutfitConfig | **100** |
| `0x822b6d88` | 88 | `??1BandCamShot@@UAA@XZ` | BandCamShot | **100** |
| `0x8234d670` | 88 | `?GetPlayerDifficultySym@BandTrack@@QBA?AVSymbol@@XZ` | system/bandobj/BandTrack | **100** |
| `0x8236e7d0` | 88 | `??0Lod@Character@@QAA@PAVObject@Hmx@@@Z` | Character | **100** |
| `0x8237e0b8` | 88 | `?InGroup@CharClip@@QAA_NPAVObject@Hmx@@@Z` | CharClip | **100** |
| `0x82287140` | 84 | `??1?$ObjPtrList@VCharBoneOffset@@VObjectDir@@@@UAA@XZ` | BandCharacter | **100** |
| `0x82325538` | 84 | `??E?$ObjDirItr@VUILabel@@@@QAAAAV0@XZ` | PlayerDiffIcon | **100** |
| `0x8237c788` | 84 | `?resize@?$ObjVector@V?$ObjOwnerPtr@VRndTransformable@@@@@@QAAXI@Z` | CharBonesMeshes | 33.67 |
| `0x8239dff0` | 84 | `??E?$ObjDirItr@VCharCuff@@@@QAAAAV0@XZ` | CharCuff | **100** |
| `0x823c47a8` | 84 | `??$?5UConstraintSystem@CharBlendBone@@@@YAAAVBinStream@@AAV0@AAV?$ObjList@UConstraintSy...` | CharForeTwist → CharBlendBone | **100** |
| `0x822a83c0` | 80 | `??$_Destroy_Range@PAVPiercing@OutfitConfig@@@stlpmtx_std@@YAXPAVPiercing@OutfitConfig@@0@Z` | OutfitConfig | **100** |
| `0x822ae930` | 80 | `??1PatchRenderer@@UAA@XZ` | BandSwatch | **100** |
| `0x822af178` | 80 | `??_GPatchRenderer@@UAAPAXI@Z` | BandSwatch | **100** |
| `0x822f1858` | 80 | `??_GLabel3d@@UAAPAXI@Z` | CharBoneDir → Label3d | **100** |
| `0x8238e500` | 80 | `?MakeMRU@CharClipGroup@@QAAXPAVCharClip@@@Z` | CharClipGroup | **100** |
| `0x823a0aa0` | 80 | `?HideDraws@CharMeshHide@@QAAXH@Z` | CharMeshHide | **100** |
| `0x823c4630` | 80 | `??1CharBlendBone@@UAA@XZ` | CharForeTwist → CharBlendBone | **100** |
| `0x8228c8b0` | 76 | `??_GBandCharacter@@UAAPAXI@Z` | BandCharacter | 99.74 |
| `0x822dfac0` | 76 | `?clear@?$vector@VEdge@ChordShapeGenerator@@V?$StlNodeAlloc@VEdge@ChordShapeGenerator@@@...` | CharLipSync → ChordShapeGenerator | **100** |
| `0x822f00c8` | 76 | `?DrawShowing@Label3d@@UAAXXZ` | CharBoneDir → Label3d | **100** |
| `0x822f1350` | 76 | `?LoadResource@Label3d@@QAAXXZ` | CharBoneDir → Label3d | **100** |
| `0x8227d1d8` | 72 | `??$New@VWaypoint@@@Object@Hmx@@SAPAVWaypoint@@XZ` | BandCharacter | **100** |
| `0x8227da80` | 72 | `??$New@VBandCharDesc@@@Object@Hmx@@SAPAVBandCharDesc@@XZ` | BandCharacter | **100** |
| `0x8229d0b8` | 72 | `??$New@VRndMat@@@Object@Hmx@@SAPAVRndMat@@XZ` | OutfitConfig | **100** |
| `0x8229d100` | 72 | `??$New@VRndCam@@@Object@Hmx@@SAPAVRndCam@@XZ` | OutfitConfig | **100** |
| `0x822a93d8` | 72 | `??1BandPatchMesh@@QAA@XZ` | OutfitConfig | **100** |
| `0x822ad270` | 72 | `??$New@VUIColor@@@Object@Hmx@@SAPAVUIColor@@XZ` | BandSwatch | **100** |
| `0x822ae250` | 72 | `??$New@VRndDir@@@Object@Hmx@@SAPAVRndDir@@XZ` | BandSwatch | **100** |
| `0x8230eb60` | 72 | `?_M_create_node@?$list@VPracticeSectionMapping@SongSectionController@@V?$StlNodeAlloc@V...` | SongSectionController | **100** |
| `0x82314140` | 72 | `??$New@VBandLabel@@@Object@Hmx@@SAPAVBandLabel@@XZ` | CrowdAudio | **100** |
| `0x82355978` | 72 | `?SetChordShape@ArpeggioShape@@QAAXPAVRndMesh@@@Z` | GemTrackResourceManager | **100** |
| `0x82355bd0` | 72 | `??$New@VRndMatAnim@@@Object@Hmx@@SAPAVRndMatAnim@@XZ` | GemTrackResourceManager | **100** |
| `0x822aca18` | 68 | `?resize@?$ObjVector@VBandPatchMesh@@@@QAAXI@Z` | BandSwatch → OutfitConfig | **100** |
| `0x8231f630` | 68 | `??_GUIListCustomTemplate@@UAAPAXI@Z` | system/bandobj/ReviewDisplay → ScoreDisplay | **100** |
| `0x823362c8` | 64 | `??4Outfit@BandCharDesc@@QAAAAV01@ABV01@@Z` | BandCharDesc | **100** |
| `0x82355b48` | 60 | `?GetYPos@ArpeggioShape@@QBAMXZ` | GemTrackResourceManager | **100** |
| `0x822f0e98` | 48 | `?ClassName@Label3d@@UBA?AVSymbol@@XZ` | CharBoneDir → Label3d | **100** |
| `0x823d4c90` | 48 | `??0PlayBack@CharLipSync@@QAA@XZ` | CharLipSync | **100** |
| `0x822bb8d0` | 44 | `?Disabled@BandCrowdMeter@@QBA_NXZ` | Waypoint → BandCrowdMeter | **100** |
| `0x8232ae70` | 24 | `?FindTarget@BandWardrobe@@QAAPAVBandCharacter@@VSymbol@@ABVTargetNames@1@@Z` | BandWardrobe | **100** |
| `0x82287d40` | 20 | `?IsLoading@BandCharacter@@QAA_NXZ` | BandCharacter | **100** |
| `0x822cd8f8` | 20 | `?SetNumStars@BandScoreboard@@QAAXM_N@Z` | BandStarDisplay → BandScoreboard | **100** |
| `0x82355b30` | 20 | `?ShowChordShape@ArpeggioShape@@QAAX_N@Z` | GemTrackResourceManager | **100** |
| `0x82376d80` | 20 | `?Starved@CharDriver@@QAA_NXZ` | CharDriver | 77.19 |
| `0x822ae790` | 16 | `?Highlight@RndDrawable@@$4PPPPPPPM@HM@AAXXZ` | BandSwatch | **100** |
| `0x822adba0` | 12 | `?Save@BandSwatch@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` | BandSwatch | **100** |
| `0x822ae918` | 12 | `?SetType@PatchRenderer@@$4PPPPPPPM@A@AAXVSymbol@@@Z` | BandSwatch | **100** |
| `0x822aebe0` | 12 | `?Copy@PatchRenderer@@$4PPPPPPPM@A@AAXPBVObject@Hmx@@W4CopyType@23@@Z` | BandSwatch | **100** |
| `0x822aebf0` | 12 | `?Handle@PatchRenderer@@$4PPPPPPPM@A@AA?AVDataNode@@PAVDataArray@@_N@Z` | BandSwatch | **100** |
| `0x822aec00` | 12 | `?Save@PatchRenderer@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` | BandSwatch | **100** |
| `0x822af078` | 12 | `?Load@PatchRenderer@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` | BandSwatch | **100** |
| `0x822af088` | 12 | `?SyncProperty@PatchRenderer@@$4PPPPPPPM@A@AA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | BandSwatch | **100** |
| `0x822bb3a0` | 12 | `?Disable@BandCrowdMeter@@QAAXXZ` | CrowdMeterIcon → BandCrowdMeter | **100** |
| `0x822bb3b0` | 12 | `?Enable@BandCrowdMeter@@QAAXXZ` | CrowdMeterIcon → BandCrowdMeter | **100** |
| `0x822d6338` | 12 | `?Overdrive@StreakMeter@@QBAXXZ` | StreakMeter | **100** |
| `0x822f0ec8` | 12 | `?Load@Label3d@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` | CharBoneDir → Label3d | **100** |
| `0x822f0ee8` | 12 | `?ClassName@Label3d@@$4PPPPPPPM@A@BA?AVSymbol@@XZ` | CharBoneDir → Label3d | **100** |
| `0x822f10a0` | 12 | `?Copy@Label3d@@$4PPPPPPPM@A@AAXPBVObject@Hmx@@W4CopyType@23@@Z` | CharBoneDir → Label3d | **100** |
| `0x822f10c0` | 12 | `?Highlight@Label3d@@$4PPPPPPPM@A@AAXXZ` | CharBoneDir → Label3d | **100** |
| `0x822f10d0` | 12 | `?PreLoad@Label3d@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` | CharBoneDir → Label3d | **100** |
| `0x822f17c8` | 12 | `?PostLoad@Label3d@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` | CharBoneDir → Label3d | **100** |
| `0x822f17d8` | 12 | `?SyncProperty@Label3d@@$4PPPPPPPM@A@AA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | CharBoneDir → Label3d | **100** |
| `0x823b71d0` | 12 | `?PostLoad@CharBoneDir@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` | CharBoneDir | **100** |
| `0x823b7958` | 12 | `?PreLoad@CharBoneDir@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` | CharBoneDir | **100** |
| `0x823b7fb0` | 12 | `??_ECharBoneTwist@@$4PPPPPPPM@A@AAPAXI@Z` | CharBoneDir | **100** |
| `0x823b85a0` | 12 | `?Load@CharBoneTwist@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` | CharBoneDir | **100** |

## 13. Rebase
1. `9d627e235` → `d183547ca` (W16-LC), `--rebase-merges`: map head-insert conflicts resolved by
   keeping both sides; `0x823DDFC0` (`CharacterTest::Load`) was named identically by both;
   `0x822F1080` taken from this branch (main had nulled the wrong CharBoneTwist thunk name; this
   branch names it Label3d's SetType thunk, consistent with main's finding that it is a SetType).
2. `d183547ca` → `e040b3c2e` (W16-LD): same map resolution; `symbol_aliases.json` — both sides
   appended groups, joined. The first attempt committed conflict markers into the aliases file
   (a resolver crashed after `git add`); it was redone from the saved tip and every commit in
   `main..w16-la` was checked for markers.
Both times: the lane's 277 map rows and main's rows are all present (checked key by key); full
build with forced re-split; the only row lower than the pre-rebase tip is
`VocalPlayer::Poll` 95.025 → 94.985, which W16-LD documents as its own (§3.4 of its doc).

## 14. Reproduce (`~/tmp/w16la/`, not committed)
```
python3 bind.py callers.json bind.json && python3 classify.py && python3 scoreall.py   # §3.1
python3 bodynb2.py                                                                       # §3.2
python3 vtstores.py <va>:<size> ...      # vtables a body stores + callees (dtor / ObjPtr<T> checks)
python3 shape.py <va>:<size> ...         # element-size immediates + callees (template T, §4)
python3 plan.py <plan.json> <moves.json> && python3 clamp.py <moves.json> && \
  python3 ~/tmp/w16hz/rh/apply_moves.py config/45410914/splits.txt <moves.json> config/45410914/symbols.txt
./mapwrite.sh <rows.json>                # gated_map_write + textual replacement of nulled pins
./b.sh ~/tmp/wt-w16-la <tag> [--split]   # full build to a symbols.txt fixed point + row diff
```
