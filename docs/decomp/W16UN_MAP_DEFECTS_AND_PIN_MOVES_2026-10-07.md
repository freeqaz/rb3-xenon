# W16-UN: map defects and pin moves from W16-UH / W16-UM (2026-10-07)

Branch `w16-un`, worktree `~/tmp/wt-w16un`. Inputs:
`W16UH_PLACEHOLDER_CALLEE_CENSUS_2026-10-07.md` ("Map defects", six rows) and
`W16UM_NAME_PROVEN_PLACEHOLDERS_2026-10-07.md` (the five kept-charge rows, 476 B,
and the 28 NOT-IN-UNIT-OBJ addresses).

Every change was adjudicated against retail bytes: `icf_pair_adjudicate.chase` for
identity and fold membership, retail vtables (`tools/retail_rtti.py`) for thunks,
and link order for pins. Each wave was priced with
`tools/ab_measure.py --revert HEAD` (ruler `name_check`); the effect is the
negated delta. The unit of every delta is whole-binary `matched_functions` /
`matched_code`.

## Result

| wave | commit | kinds | Δmatched | Δcode_bytes | Δmasked_equal | `none` control |
|---|---|---|---:|---:|---:|---|
| 1 | `222716cdf` | map, alias | **+5** | **+608** | 0 | flat (+0 B) |
| 2 | `02780e686` | map, splits | **+5** | **+404** | +3 | +284 B (not adjudicable: splits) |
| 3 | `146f72b83` | source, map, alias | **+1** | **+8** | 0 | +8 B (not adjudicable: source) |
| 4 | `b447f5c3c` | map, splits, alias | **+21** | **+1,612** | +1 | +1,572 B (not adjudicable: splits) |
| **total** | | | ****+32**** | ****+2,632**** | | |

No unit reached or fell off 100% in waves 1-3 (621 units at 100 on `mpn` in both
legs of each). Wave 4 took **8 units to 100 on `mpn`** (621 → 629) and **6 on the
all-rows-`fuzzy` ruler** (537 → 543): BandStarDisplay, SampleInst,
TrackPanelDirBase, band3/game/Game, sharedbook, system/beatmatch/BeatMaster,
system/rnddx9/Mat, system/synth/Synth (Game and Synth on `mpn` only). Each did so
because the 0% row that was not its code left it (BandStarDisplay and Synth also
lost the matched EH funclet that follows that row, which moved with it). Every negative row in wave 4's row diff is an EH funclet moving to the unit
its parent moved to (the same funclet scores 100 in its new unit), and
`fn_8237EB58` in CharClip went 99.5 → 100 (+40 B) as a side effect.

## The five rows W16-UM exposed (476 B) and W16-UH's six map defects (wave 1-3)

W16-UM's names made five rows lose 100 because a callee they reached had a wrong
map name. All five are four of W16-UH's six defects; fixing the map fixed them.

| # | defect (W16-UH) | retail evidence | change | wave |
|---|---|---|---|---|
| 1 | `clear<ObjPtrList<EventTrigger>>` mapped at `0x8249D1F0` | that body calls `0x8271A138`, an `Unlink` with no virtual-base step. `EventTrigger`'s `Object` base is virtual, so its `Unlink` has the step; the body belongs to a `T` with a non-virtual base. | `0x8249D1F0` renamed `?clear@?$ObjPtrList@VTask@@VObjectDir@@@@QAAXXZ` (chase PROVEN). The EventTrigger spelling withdrawn there (class `WRONG_VBASE_UNLINK`) and admitted at `0x823B8618`, the clear that retail's `~ObjPtrList<EventTrigger>` and `Load` call. | 1 |
| 2 | `~_Rb_tree<TrackType,int>` row calls `0x826DA438`, a clear of another tree type | `0x826DAB90` chases PROVEN to `~_Rb_tree<TrackType,PlayerStreakData>` | `0x826DAB90` renamed to that; the `~map` spelling folded there; `~_Rb_tree<TrackType,int>` admitted at `0x825971F0` | 1 |
| 3 | `0x825AEE78` mapped `__ucopy_ptrs<_Slist_node_base**>`, a 4-byte `b 0x8260EED8` named by shape | the target is an slist clear | `0x825AEE78` renamed `~slist<pair<Symbol,DataArray*>>`; the UIScreen variant folded. The `__ucopy_ptrs` spelling has no retail home and no caller in our objs, so it is not re-placed. | 1 |
| 4 | `__u64tod` mapped at `0x8282EF50`, the second-to-last instruction of the routine | the routine starts at `0x8282EF30` | key moved to `0x8282EF30` | 1 |
| 5 | `0x823CFAD0` mapped `?SyncProperty@CharDriver@@$4PPPPPPPM@A@...` | the thunk sits in CharSleeve's vtable (`0x82051A3C`, slot 16) and branches to `0x823CEDE8`, which chases PROVEN to our `CharSleeve::SetName`. The real CharDriver vtordisp thunk is CharDriver vtable `0x8204350C` slot 7 = `0x8237B208`. | `0x823CFAD0` → `?SetName@CharSleeve@@$4PPPPPPPM@A@AAXPBDPAVObjectDir@@@Z`; `0x823CEDE8` → `?SetName@CharSleeve@@UAAXPBDPAVObjectDir@@@Z`; `0x8237B208` → the CharDriver thunk. Four CharDriver `.text` blocks (`823CEDE4-823CEE60`, `823CF7D4-823CF918`, `823CFACC-823CFAE0`, `823CFAEC-823CFAF8`) moved to `CharSleeve.cpp`: they are CharSleeve's code. `0x823CED00` stays: `??_GCharNeckTwist` is REFUTED there. | 2 |
| 6 | `0x824EB260` mapped `?PreSave@WorldInstance@@$4...` | slot is a vtordisp{-4,0} thunk to `0x824EA298` = `this -= 0x14; b RndDir::Replace`. Retail has an explicit `WorldInstance::Replace` override; our source had none, so MSVC emitted one vtordisp{-4,20} thunk straight to `RndDir::Replace`. | source: `WorldInstance::Replace(ObjRef*, Hmx::Object*)` restored as a forwarder (`src/system/world/Instance.{h,cpp}`); `0x824EB260` → its thunk, `0x824EA298` → its body. The PreSave thunk spelling folded at `0x8234EBC8` (the `Copy@BandTrack` thunk fold; retail `PreSave` is empty). | 3 |

Row-level attribution of wave 1 (`rowdiff.py` on the two `report.json`s): the
476 B are the five W16-UM rows back at 100, plus three EH funclets of 44 B each
whose pairing follows their parent.

The `none` control is flat in wave 1 (map + alias only). That is the shape of a
fabricated alias **and** of a wrong-name repair; here every admission and rename
carries a chase-PROVEN record on retail bytes in `symbol_aliases.json` (lane
`W16-UN 2026-10-07`), so it is the second.

## The 28 NOT-IN-UNIT-OBJ addresses (wave 4)

W16-UM left 28 addresses unnamed because the retail row's unit compiled none of
our spellings. The pin rule used here is link order: MSVC places each obj's
COMDATs contiguously, and an ICF survivor keeps the position of the obj that
contributed it, so a function at a unit's edge (or inside its range) comes from
that unit's obj. Where the adjacent unit's compiled obj defines the retail body
(a census spelling, or a byte twin of it), the block moves there and takes that
name; other census spellings are admitted as folds by chase.

### Pinned and named (20)

| address | B | moved to | name (survivor) | folds admitted | row |
|---|---:|---|---|---:|---|
| `0x822784A8` | 60 | `PatchDir.cpp` | `??$_Copy_Construct@VPatchLayer@@@stlpmtx_std@@YAXPAVPatchLayer@@AB...` | 1 | 100 |
| `0x822A2EA0` | 112 | `OutfitConfig.cpp` | `?_M_clear@?$vector@VOverlay@OutfitConfig@@V?$StlNodeAlloc@VOverlay...` | 1 | 100 |
| `0x822BB3C8` | 128 | `BandCrowdMeter.cpp` | `??$__push_heap@PAPAVRndDrawable@@HPAV1@P6A_NPAV1@0@Z@stlpmtx_std@@...` | 1 | 100 |
| `0x822C9048` | 60 | `BandLeadMeter.cpp` | `??$_Copy_Construct@UObjVersion@@@stlpmtx_std@@YAXPAUObjVersion@@AB...` | 1 | 100 |
| `0x822CDB70` | 120 | `BandScoreboard.cpp` | `??0?$ObjPtr@VRndMesh@@@@QAA@ABV0@@Z` | 0 | 100 |
| `0x8230E5B8` | 76 | `SongSectionController.cpp` | `?clear@?$_List_base@VPracticeSectionMapping@SongSectionController@...` | 0 | 100 |
| `0x82359680` | 224 | `QuestManager.cpp` | `?insert_unique@?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@...` | 2 | 100 |
| `0x8235BA70` | 8 | `Tour.cpp` | `?GetTourProgress@Tour@@QBAPAVTourProgress@@XZ` | 1 | 100 |
| `0x8235C9A0` | 224 | `Tour.cpp` | `?insert_unique@?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@...` | 2 | 100 |
| `0x8239C5D8` | 92 | `CharMeshCacheMgr.cpp` | `?_M_erase@?$vector@VVert@SyncMeshCB@@V?$StlNodeAlloc@VVert@SyncMes...` | 1 | 100 |
| `0x823F1898` | 28 | `SessionMessages.cpp` | `?GetUserData@AddUserRequestMsg@@QBAXAAVBinStream@@@Z` | 2 | 100 |
| `0x825459E0` | 8 | `ProfileMgr.cpp` | `?GetSecondPedalHiHat@ProfileMgr@@QBA_NXZ` | 2 | 100 |
| `0x826FCCE8` | 100 | `Sfx.cpp` | `??1?$ObjPtr@VSynthSample@@@@UAA@XZ` | 0 | 100 |
| `0x82741340` | 8 | `Splash.cpp` | `?SetWaitForSplash@Splash@@QAAX_N@Z` | 0 | 100 |
| `0x8276F6F8` | 8 | `PlayerTrackConfigList.cpp` | `?SetAutoVocals@PlayerTrackConfigList@@QAAX_N@Z` | 0 | 100 |
| `0x82774148` | 104 | `SongData.cpp` | `??_GMeasureMap@@QAAPAXI@Z` | 7 | 100 |
| `0x82782950` | 8 | `SongParser.cpp` | `?OnNewTrack@SongParser@@UAAXH@Z` | 1 | 100 |
| `0x827B74D0` | 96 | `StoreArtLoaderPanel.cpp` | `??$__destroy_range_aux@V?$reverse_iterator@PAVArtEntry@StoreArtLoa...` | 1 | 100 |
| `0x82BB34A8` | 60 | `VorbisReader.cpp` | `??$_Copy_Construct@V?$vector@FV?$StlNodeAlloc@F@stlpmtx_std@@@stlp...` | 4 | 100 |
| `0x82C30B58` | 8 | `JsonMemory.cpp` | `JsonRealloc` | 1 | 100 |

### Named, not pinned (1)

`0x822DC828` (8 B, `li r4,0; b ?resize@VertVector@RndMesh@@`) is
`?clear@VertVector@RndMesh@@QAAXXZ`, and stays in `ChordShapeGenerator.cpp`.
Moving it to the adjacent `Mesh.cpp` block was tried: the row read 0, because
our `Mesh.obj` defines `resize` in the same TU and inlines it into an 80-byte
`clear` (it calls `MemFree`), while `NoteTube.obj` and `Gem.obj` emit the 8-byte
tail call that retail has. An 80-byte COMDAT would not fold with retail's 8-byte
survivor, so the evidence for the Mesh pin is not there and the move was undone
before the commit. The name is kept: retail's bytes are exactly that body, and
the row is unpaired either way. ChordShapeGenerator's next function (`New<RndMesh>`)
follows it directly, and our `ChordShapeGenerator.obj` calls `resize` directly,
so who emitted retail's out-of-line `clear` is left open.

### VorbisReader needed one more admission

`0x82BB34A8` (`_Copy_Construct<vector<short>>`) paired at 99.67: its one call is
the copy ctor retail names `vector<unsigned short>` at `0x82BB3430`, where ours
calls `vector<short>`. The two are a byte fold; the `short` spelling was admitted
at `0x82BB3430` (`--extra`, chase PROVEN) and the row reads 100.

### Recorded, not changed (7)

No adjacent unit's obj compiles a spelling or twin of these bodies. Under the
link-order rule each is code that the containing unit's retail obj emitted and
ours does not; that is a source omission in that unit, not a pin or a name.

| address | B | retail unit | census spelling | why not |
|---|---:|---|---|---|
| `0x82272308` | 164 | MessageTimer | `Find<UIScreen>` | neighbours compile neither |
| `0x82272548` | 196 | MessageTimer | `PropSync<UIPanel>` | neighbours compile neither |
| `0x82272B90` | 72 | CalibrationPanel | `~UIScreen` | neighbours compile neither |
| `0x822B0D48` | 112 | BandCamShot | `ObjPtr<EventTrigger>(Object*, EventTrigger*)` | our BandCamShot does not instantiate that ctor |
| `0x824417D8` | 112 | system/rndobj/Utl | `vector<Key<TexPtr>>::_M_erase` | neighbours compile neither |
| `0x825122B0` | 104 | Archive | `MakeString<5 x const char*>` | neighbours compile neither |
| `0x82667FC8` | 16 | CharProvider | `OvershellProfileProvider::GetUser` | our `OvershellProfileProvider.cpp` does not emit it |

Also seen and left alone: `0x82BB33D8`, `0x82BB3430` and `0x82BB3510` are
probably VorbisReader's by the same rule, but they are paired today under
Gem / Mesh / StreamReceiver360 fold names, and moving a paired row is a
re-homing, which is not neutral. Out of scope for this lane.

## Alias bookkeeping

- `alias_survivor_relabel.py --write --lane "W16-UN 2026-10-07" --rechase-key
  rechase_w16un` after each map change. In wave 4 it folded the placeholder
  labels `fn_822784A8` and `fn_822C9048`; those were taken out of `folded` and
  recorded under `relabelled.held`, as W16-UM did.
- The old `clear<EventTrigger>` member at `0x8249D1F0` was withdrawn by hand
  (class `WRONG_VBASE_UNLINK`, with the evidence above) before it was admitted
  at `0x823B8618`.
- `alias_callee_name_drift.py --reprove --write` after every rename; no group
  stopped proving.

## Tool changes

- `tools/name_proven_placeholders.py`: `--lane` and `--workdir` (were hard-coded
  to W16-UM and `~/tmp/w16um`); evidence text uses the lane id.
- `tools/alias_survivor_relabel.py`: `--rechase-key` (default `rechase_w16os`).

## Hazards met

- The `gated_map_write` P7 injectivity gate reads the current split objs, so a
  name moving between addresses (the CharDriver thunk name, `0x823CFAD0` →
  `0x8237B208`) has to be written in two steps with a re-split between.
- A `.text` move makes the next build fail at the split guard ("THE SPLIT
  REWROTE ITS OWN INPUT") because `.pdata` is re-derived; the second build
  passes and the re-derived lines are committed with the move.

## Not done

- The seven recorded addresses (source omissions in their units).
- The 13 NO-BASE-OBJ addresses in W16-UM (units with no obj; identification,
  not pins).
- W16-UH's "right callee, body differs" table (bodies, not map).
- Merging and pushing (coordinator).

## Native gate

`tools/native_build_gate.sh` at `2513b562f` (the only source change is
`src/system/world/Instance.{h,cpp}`):

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
```
