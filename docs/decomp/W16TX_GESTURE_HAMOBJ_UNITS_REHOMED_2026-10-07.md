# W16-TX: the gesture and hamobj units held other TUs' code; every row re-homed

Lane W16-TX, 2026-10-07. Worktree `~/tmp/wt-w16tx`, branch `w16-tx`, started at main `fb2b60432` and
rebased onto `fa2443370` (W16-TV) before step 4.

W16-TU (`W16TU_FLOW_UNITS_REHOMED_2026-10-07.md` §4–§5) left three items: the 9 `gesture` (Kinect) and
`hamobj` (Dance Central) units pinned in `config/45410914/splits.txt`, the Flow scatter includes that
existed only to pair Flow-unit rows, and the unpinned `0x82272A60`. This lane does for them what W16-TU
did for Flow.

The short version:
- **All 9 units are gone from `splits.txt`.** They held 17 `.text` blocks: 14 rows / 1,592 B plus 6
  pad/EH-prefix slivers. Every block now sits in the unit whose region it occupies on retail bytes (§2).
- **14 of 14 rows have a home.** 13 pair at 100 there. One, the `vector<Symbol>` copy constructor at
  `0x8235CAE0` (116 B), reads 0 in Tour, because our `Tour.cpp` does not instantiate it while retail's
  does (§2.3).
- **Six rows were renamed** from a Dance-Central/Kinect or Flow spelling to the instantiation the owning
  TU emits. One also needed a source move: dc3-decomp defines `CharBlendBone::ConstraintSystem`'s writer
  in `CharIKHand.cpp`, and DC3's own map puts it in `CharBlendBone.obj` (§2.2).
- **The four dead Flow includes are removed** (§3). Native gate **PASS 18/18, 0 skipped**.
- **`0x82272A60` stays unpinned**, now with a candidate owner and the reason it is not pinned (§4).
- **Price: −1 fn / −116 B net**, the Tour row, across 4 A/Bs that each came in at their prediction
  (§5). The composite reverse A/B on the rebased base agrees.

## 1. The units, and how owners were chosen

Retail has no RTTI for any of the 28 `gesture` or 74 `hamobj` derived classes (W16-TU §1). The 9 pinned
units were:

| unit | .text blocks | rows |
|---|---|---|
| RhythmDetector | `82270690-822707C0`, `82270848-82270B1C`, `8227173C-82271748` | 7 (1,016 B) |
| CameraInput | `822740C8-822740D0` | 1 (8 B) |
| BustAMoveData | `82274538-8227459C` | 1 (100 B) |
| SongLayout | `822C76E4-822C76F0`, `822D16F8-822D1768`, `8235CAE0-8235CB54`, `8235CB54-8235CB7C` | 3 (268 B) |
| HamRibbon | `822F0408-822F047C` | 1 (116 B) |
| HamIKEffector | `823C3300-823C3354` | 1 (84 B) |
| DepthBuffer3D | `826FB9C8-826FB9D0`, `82701FF0-82701FF8` | 0 |
| SkeletonClip | `827041A0-827041A8` | 0 |
| FitnessFilter | `8270CDB8-8270CDC0` | 0 |

The rules are W16-TU's, plus three instruments it did not need:
- **Region.** Retail `.text` is laid out object by object, in link order. Each object's functions and
  the COMDATs the linker selected from it are contiguous. A row belongs to the object whose own
  non-template functions surround it.
- **Byte identity.** A reloc-masked body search compares retail bytes (`tools/retail_body.Img`) against
  every function slice of every compiled object (`tools/coff_bodies_ext.function_bodies_ext`), and lists
  the callee names on both sides.
- **Retail data references.** `.rdata` vtable slots, read back through the Complete Object Locator's
  type descriptor.
- **DC3's leaked map** (`ham_xbox_r.map`) for which object DC3 attributes a spelling to.

Slivers were decoded on retail bytes. Each is a 4 B zero pad plus an 8 B EH prefix, or the prefix
alone: handler `0x82829530` and an `.rdata` FuncInfo. Each goes to the unit of the function after it.

## 2. Disposition of every row

Before = main's report at `fb2b60432` (step 1's leg A). After = final.

| old unit | block | new unit | rows: fuzzy before -> after | evidence |
|---|---|---|---|---|
| RhythmDetector | `82270690-822707C0` | App | `ObjDirPtr<ObjectDir>::operator=` 300: 100 -> 100 | App.cpp opens `.text` (`??1App` at `0x82270000`) and resumes at `0x82270B84`. Its own functions `AppExceptionFilter`/`App::Run` (`0x822703A8`) and `AppDebugModal` (`0x82270B90`) bracket the block. App.obj defines all five ObjDirPtr spellings |
| RhythmDetector | `82270848-82270B1C` | App | `LoadFile` 352, `fn_822709A8` 40, `~ObjDirPtr` 88, `fn_82270A30` 40, `Replace` 120, `??_G` 76: all 100 -> 100 | same; the two funclets follow their parents and pair by bytes |
| RhythmDetector | `8227173C-82271748` (12 B) | TextFileStream | (no rows) | pad + EH prefix of `??1TextFileStream` |
| CameraInput | `822740C8-822740D0` | Object | `NewFrame@CameraInput` 8: 100 -> renamed `TypeProps::RefOwner` 100 | retail `.rdata` `0x8200EBFC` -> `??_GTypeProps` (`0x822740D0`), `0x8200EC00` -> `0x822740C8`. The COL at `0x8200EBF8` names `.?AVTypeProps@@`, so this is TypeProps's vtable, slot 1 = `RefOwner`. Object.obj defines it byte-identically. Five other vtables share the folded body |
| BustAMoveData | `82274538-8227459C` | PatchDir | `operator<<(BAMPhrase)` 100: 100 -> renamed `operator<<(const PatchDescriptor&)` 100 | inside PatchDir's region (PatchDir rows `0x82273E28`…`0x822741B0` and `0x822745A0`…); the next function is `operator>>(PatchDescriptor&)`. PatchDir.obj OWNS (NO_DUPLICATES) a byte-identical `operator<<(const PatchDescriptor&)` |
| SongLayout | `822C76E4-822C76F0` (12 B) | system/bandobj/BandFaceDeform | (no rows) | pad + EH prefix of BandFaceDeform's `_Copy_Construct<DeltaArray>` |
| SongLayout | `822D16F8-822D1768` | system/bandobj/CharKeyHandMidi | `push_back<Symbol>` 112: 100 -> renamed `push_back<CharKeyHandMidi::KeyboardKey>` 100 | inside CharKeyHandMidi; CharKeyHandMidi.obj's `push_back<KeyboardKey>` is byte-identical, and retail's callee is already mapped `_M_insert_overflow_aux<KeyboardKey>` |
| SongLayout | `8235CAE0-8235CB54` | Tour | `vector<Symbol>(const vector&)` 116: 100 -> **0** | see §2.3 |
| SongLayout | `8235CB54-8235CB7C` | Tour | `fn_8235CB54` 40: 100 -> 100 | the copy ctor's unwind funclet; pairs by bytes in Tour |
| CharClip | `8235CAD8-8235CAE0` (8 B) | Tour | (no rows) | EH prefix of the copy ctor above; moved with it |
| HamRibbon | `822F0408-822F047C` | Label3d | `DeleteAll<RndTransformable>` 116: 100 -> renamed `DeleteAll<RndMesh>` 100 | inside Label3d; Label3d.obj's `DeleteAll<RndMesh>` is byte-identical, and retail's callee is already mapped `Unlink<RndMesh>` |
| HamIKEffector | `823C3300-823C3354` | CharBlendBone | `operator<<(HamIKEffector::Constraint)` 84: 100 -> renamed `operator<<(const CharBlendBone::ConstraintSystem&)` 100 | see §2.2 |
| DepthBuffer3D | `826FB9C8-826FB9D0` | VocalScoreHistory | (no rows) | EH prefix of `VocalScoreHistory::VocalScoreHistory` |
| DepthBuffer3D | `82701FF0-82701FF8` | StandardStream | (no rows) | EH prefix of StandardStream's `_Copy_Construct<Marker>` |
| SkeletonClip | `827041A0-827041A8` | StandardStream | (no rows) | EH prefix of the StandardStream function at `0x827041A8` |
| FitnessFilter | `8270CDB8-8270CDC0` | Faders | (no rows) | EH prefix of `Fader::SetType` |
| band3/meta_band/ViewSetting | `82768FF8-8276904C` | DataUtl | `map<FlowNode*,QueueState>::_M_create_node` 84: 100 -> renamed `map<Symbol,DataArray*>::_M_create_node` 100 | the Flow spelling W16-TU §4 left. It sits between DataUtl's `map<Symbol,DataArray*>::_M_erase` (`0x82768F98`, W16-TU's rename) and `DataInit`. DataUtl.obj's `<Symbol,DataArray*>` `_M_create_node` is byte-identical |

### 2.1 The renames

`scripts/target_symbol_map.json`, six rows. None of the new names was mapped at any other address. Each
is defined by its new home's compiled object.

| address | old | new |
|---|---|---|
| `0x822740C8` | `?NewFrame@CameraInput@@QBAPBUSkeletonFrame@@XZ` | `?RefOwner@TypeProps@@UBAPAVObject@Hmx@@XZ` |
| `0x82274538` | `??6@YAAAVBinStream@@AAV0@ABVBAMPhrase@@@Z` | `??6@YAAAVBinStream@@AAV0@ABVPatchDescriptor@@@Z` |
| `0x822D16F8` | `push_back` on `vector<Symbol>` | `push_back` on `vector<CharKeyHandMidi::KeyboardKey>` |
| `0x822F0408` | `?DeleteAll@?$ObjPtrList@VRndTransformable@@VObjectDir@@@@QAAXXZ` | `?DeleteAll@?$ObjPtrList@VRndMesh@@VObjectDir@@@@QAAXXZ` |
| `0x823C3300` | `??6@YAAAVBinStream@@AAV0@ABVConstraint@HamIKEffector@@@Z` | `??6@YAAAVBinStream@@AAV0@ABUConstraintSystem@CharBlendBone@@@Z` |
| `0x82768FF8` | `_M_create_node` on `_Rb_tree<FlowNode*, pair<FlowNode* const, FlowQueueable::QueueState>>` | `_M_create_node` on `_Rb_tree<Symbol, pair<const Symbol, DataArray*>>` |

Each is an ICF-folded body, so retail's own spelling cannot be recovered. The new name is the one the
object at that position emits.

Alias groups 204, 334, 1497, 1628, 1755 and 1899 had the old spellings as survivors. They were
relabelled by `tools/alias_survivor_relabel.py --write`, run **after** a build, so the renamer had
applied the new names first. That avoids the UNDECIDABLE `MISSING(retail)` verdicts W16-TU got. Result:
18 folded memberships re-chased PROVEN, and 6 old labels PROVEN and folded. `tools/icf_alias_finder.py
--validate`: PASS, 0 contradicted. `tools/alias_survivor_drift.py`: OK.

After the rebase onto W16-TV, its new `CHECK ALIAS CALLEE NAMES VS MAP` edge flagged 13 groups: these 6
and 7 groups that call a renamed address (`0x8229e508`, `0x8233cab8`, `0x823599e8`, `0x82359f28`,
`0x823966a8`, `0x8256c938`, `0x827c68c0`). `tools/alias_callee_name_drift.py --reprove --write`
re-proved 51 memberships: 48 PROVEN, 3 UNDECIDABLE, **0 regressions**. The 3 UNDECIDABLE (two
`insert_unique<Symbol,…>` spellings in `0x823599e8`, `CheckContextDiff` in `0x8256c938`) were already
UNDECIDABLE in main's snapshot. `scripts/alias_callee_names.json` is committed.

### 2.2 The ConstraintSystem writer belonged to CharBlendBone.cpp

`0x823C3300` follows `??_GCharPosConstraint` and opens the region where CharForeTwist and CharBlendBone
interleave (`0x823C3360`–`0x823C69B0`). CharBlendBone's `list<ConstraintSystem>` code follows from
`0x823C3828`. Retail's `operator<<(ObjList<ConstraintSystem>)` at `0x823C3878` calls `0x823C3300` out
of line. CharIKHand's own region is `0x82397xxx` (`operator>>(IKTarget&)` at `0x823972C0`), far away.

The byte search found the body in CharIKHand.obj under both spellings, `IKTarget` and `ConstraintSystem`,
because our `CharIKHand.cpp:88` defined CharBlendBone's writer. That came from dc3-decomp
(`dc3-decomp/src/system/char/CharIKHand.cpp:85`). DC3's own map disagrees with DC3's decomp:

```
??6@YAAAVBinStream@@AAV0@ABUConstraintSystem@CharBlendBone@@@Z 82386738 f   char:CharBlendBone.obj
??6@YAAAVBinStream@@AAV0@ABUIKTarget@CharIKHand@@@Z            82386738 f   char:CharIKHand.obj
```

So the definition moved to `CharBlendBone.cpp`, next to its `operator>>`. The `IKTarget` writer stays
in CharIKHand.cpp, and group 334 folds it.

### 2.3 The one row at 0: Tour's `vector<Symbol>` copy constructor

`0x8235CAE0` (116 B) sits among Tour's own functions. `set<Symbol>` helpers precede it at `0x8235CA80`,
and `_Copy_Construct<set<Symbol>>`, `Tour::HasTourProperty` and `Tour::GetTourProperty` follow from
`0x8235CB88`. Retail Tour.obj therefore emitted it. Its three retail callers are elsewhere
(LicenseMgr's `hash_map<Symbol, vector<Symbol>>` at `0x8264eaa8`/`0x8264f514`, and
`Stats::Stats(const Stats&)` at `0x826580cc`), which is normal for a COMDAT.

Our `band3/tour/Tour.obj` does not instantiate `vector<Symbol>(const vector&)`. Its 116 B
`vector<list<int>>` copy constructor differs on bytes. Neither our `Tour.cpp` nor its Wii-target counterpart copies a
`vector<Symbol>`; both only declare one local (`Tour.cpp:621`; `:601` in its Wii-target counterpart). So retail's Tour TU
contains something both decomps lack. The row is pinned to Tour and reads 0, as an honest gap. The
SongLayout pin read 100 only because a DC3 object that retail lacks happened to instantiate the same
template. Its funclet (`fn_8235CB54`) does pair in Tour by bytes.

## 3. The dead Flow includes

| file | removed | why it was there |
|---|---|---|
| `src/system/flow/FlowCommand.cpp` | `#include "os/ContentMgr.cpp"` | paired the ContentMgr rows W16-TU moved to ContentMgr |
| `src/system/flow/FlowMultiSetProperty.cpp` | `#include "band3/game/GemPlayer.cpp"`, `"band3/game/Band.cpp"` | paired FlowMultiSetProperty-unit rows |
| `src/system/flow/FlowSetProperty.cpp` | body-dup of `Song::SetLoopStart` | paired `0x827C6690`, now in Song |
| `src/band3/meta_band/CriticalUserListener.cpp` | `#include "flow/FlowManager.cpp"` (`#if !HX_NATIVE`) | paired `0x82768FF8` through ViewSetting, which includes CriticalUserListener.cpp |

Before removing the last one I checked that no ViewSetting or CriticalUserListener row shares a name
with FlowManager.obj's functions; there were 0. `native/CMakeLists.txt`'s note about that guard is
updated. The 13 Flow `.cpp` files are still compiled from `objects.json`, with no split.

**Not removed:** Flow scatter includes in retail-present TUs that W16-TU did not list:
`GemManager.cpp:1644` (`FlowManager.cpp`), `BandDirector.cpp:2236` and `Morph.cpp:273` (`FlowNode.cpp`),
`EventTrigger.cpp:967/972` (`FlowTrigger.cpp`, `Flow.cpp`), `UI.cpp:1373` (`FlowQueueable.cpp`),
`Cheats.cpp:457` (`Flow.cpp`), and `HamCamTransform.cpp:230/251`. Each may still pair a Flow-spelled
fold survivor in a real unit. Each needs the per-row adjudication this lane did, in a separate lane.

## 4. `0x82272A60` stays unpinned

It is `vector<…*>::_M_fill_insert`, 108 B, 70 retail callers. Alias group 1713 already folds
`vector<int>`, `vector<RndDrawable*>` and other 4-byte-element spellings into the
`vector<Hmx::Object*>` survivor. The name is generic, not Flow-only, so it is unchanged.

The owner is still not identified, but there is now a candidate. Retail lays the start of `.text` out
in alphabetical object order: `App` (`0x82270000`), then an unpinned run, then `ChecksumData_xbox`
(`0x82272E40`), `Main` (`0x82272E68`), `Memory_Xbox`. The run from `0x822716F8` to `0x82272E3C` holds
only template/inline COMDATs: `TextFileStream`, `Message`, `map<int,float>`, `MakeString<float,float,float>`,
`sort<float*>` and `vector<int>::resize`. Wii-target source has two TUs that sort between App and
ChecksumData and use exactly these: `BandOffline.cpp` (`map<int,float>`, `map<String,float>`) and
`BudgetScreen.cpp` (`TextFileStream`, `sort<float*>`, `MakeString(fmt, min, mean, max)`, `mDist.resize`
on a `vector<int>`, `Find<UIScreen>`, a static `Message`). Retail has **no** RTTI for either class. That
fits an object whose own code `/OPT:REF` removed, while the linker still selected its COMDAT copies,
because it precedes the other definers in link order.

That is a hypothesis, not a proof. Neither TU exists in rb3-xenon, so the row cannot pair wherever it
is pinned. It stays in `auto_*`, in the denominator. The other rows in the run (pinned to PropKeys,
GemManager, MessageTimer, MeshAnim, Synapse_dsp, MatAnim, CalibrationPanel) are pairing choices of the
same kind. They are not touched here.

## 5. Price

Four `tools/ab_measure.py` runs, one change each, in this worktree, on the `name_check` ruler. Every leg
settled to zero work and was read at a `symbols.txt` split fixed point. Run dirs are under
`~/tmp/wt-w16tx/.ab_measure_runs/`.

| step | commit | kind | predicted | measured | units at 100% (mpn) |
|---|---|---|---|---|---|
| 1. remove 9 units, re-home 17 blocks | `b63dba98c` | splits | −660 B / −8, or −620 / −7 if the Tour funclet pairs | **−7 fns / −620 B** | 617 → 609 |
| 2. six renames + relabel | `37c964627` | map | +420 B / +5 | **+5 / +420 B** (`none` +420) | 609 → 611 |
| 3. ConstraintSystem writer to CharBlendBone.cpp | `6dde457dd` | source | +84 B / +1 | **+1 / +84 B**, 4 recompiles | 611 → 611 |
| 3b. callee-name snapshot after rebase | `3f748982e` | gate input | 0 | not run: not a report input | |
| 4. dead Flow includes | `04bbb7413` | source | 0 | **+0 / +0 B**, 5 recompiles | 611 → 611 |
| **total** | | | | **−1 fn / −116 B** | **−6** |

Steps 1–3 were measured on base `fb2b60432`, step 4 on the rebased branch. The composite check, the
reverse of the whole lane (`git diff HEAD main`) on the rebased base, reads **+1 fn / +116 B**: main 54,947 / 59.315186% against the lane's 54,946 / 59.314053%. That equals the sum of the steps, so W16-TV's 74 withdrawals in between did not interact with this lane's rows.

Step 1's prediction held in its second branch: the funclet `fn_8235CB54` pairs by bytes in Tour. The
seven rows at 0 after step 1 were exactly the seven whose names their new home did not define. Steps 2
and 3 brought six of them back. The seventh is the Tour row of §2.3, so the lane's net −116 B is that
one row. Of the units at 100%, 6 were gesture/hamobj units that no longer exist. Tour was not at 100%
before.

Native gate, run after step 4:
`NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.

## 6. Not done

- The Flow scatter includes in retail-present TUs listed in §3.
- Tour's missing `vector<Symbol>` instantiation (§2.3). It needs the retail Tour function that copies
  one, and none of Tour's own retail functions calls `0x8235CAE0`.
- Pinning `0x82272A60` (§4).
- The 12 B CharPosConstraint pin at `0x823C3354` is the pad + EH prefix of `CharForeTwist::Handle`, and
  by the sliver rule it belongs to CharForeTwist. It is outside this lane's units and was left.
- The `.?AV`-less classes in the other directories W16-TU §4 listed.

Merging and pushing are left to the coordinator.
