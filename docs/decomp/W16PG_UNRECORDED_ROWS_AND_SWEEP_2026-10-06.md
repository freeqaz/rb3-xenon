# W16-PG — the unrecorded M1/M2/I2 rows and the correctness sweep (2026-10-06)

Lane W16-PG, branch `w16-pg`, worktree `~/tmp/wt-w16-pg`. Brief: work items 2 and 4 of
`docs/decomp/CAMPAIGN_STATE_2026-10-03b.md`. Item 2 is the M1/M2/I2 rows nobody had recorded (50 rows,
17,132 B). Item 4 is the ≈4.8 KB correctness sweep. For each row I opened, fix it to retail's behaviour or
record why it stops. `src/network` and `src/xdk` were out of scope.

## 1. Result

Whole-binary A/B, `tools/ab_measure.py --worktree ~/tmp/wt-w16-pg --revert <tmp>`. Leg A is the branch with
all eight W16-PG commits reverted by a temporary commit; leg B is the branch. Ruler `name_check`, both legs at the
split fixed point, rc=0. Run dir `.ab_measure_runs/20261006-090245-w16pg-branch-final-3919534`.

| measure | leg A | leg B | Δ |
|---|---:|---:|---:|
| matched_functions | 53,449 | 53,463 | **+14** |
| honest (matched − masked_equal) | 28,258 | 28,271 | +13 |
| matched_code % | 57.611786 | 57.660030 | **+0.048244 pp (+4,944 B)** |
| fuzzy % | 63.684340 | 63.688587 | +0.004247 pp |
| units at 100 (mpn) | 545 | 547 | +2 (BandConfiguration, Str) |

**Per row, leg A vs leg B reports: 12 UP, 6 NEW, 6 GONE, 0 DOWN.** All six GONE rows are renames whose
replacement is among the NEW rows:

- the three MoveDetector heap spellings became FileCache `Priority` spellings;
- `fn_826B8EBC` moved MemTracker → VocalNoteList;
- `fn_827D60A0` became the MemDiffEntry spelling;
- NetCacheMgr `_List_iterator::operator--` became `RndText::DeferUpdateText`.

An earlier A/B over the first four commits only (run `20261006-082000-w16pg-branch-3511080`) measured
+8 fns / +4,504 B with 0 DOWN. The four later commits added +6 / +440 B, which matches their per-commit
predictions (+1/+160, +1/+104, +2/+12, +2/+164).

Leg A is the branch's own base (merge-base `6f1d1d794`), not today's main. The
coordinator's rebase will re-measure against current main.

Every row I opened is either fixed (§2) or recorded with the reason it stops (§3). Every commit
message carries the per-row before/after for the rows it moves.

## 2. Fixed

| row | size | before → after | what was wrong | commit |
|---|---:|---|---|---|
| `ChordShapeGenerator::BuildContourCap` | 1,752 | 99.991 → 100 | `.bss` order of the two file statics `vertIt`/`faceIt` (retail co-addresses `vertIt` at `faceIt-4`) | `5cbbf1704` |
| `ChordShapeGenerator::BuildChordMesh` | 1,680 | 99.995 → 100 | same | `5cbbf1704` |
| `ChordShapeGenerator::BuildEndCap` | 1,352 | 96.911 → 96.923 | same (remaining residual is unrelated) | `5cbbf1704` |
| `BandPatchMesh::ProjectPatches` | 1,248 | 99.599 → 99.663 | `wvCount++` came before the `meshIndices` swap; retail increments after | `48ca361ee` |
| FileCache `__adjust_heap` / `__make_heap` / `sort_heap` | 204+108+100 | 97.373 → 100 (adjust); the other two re-pair at 100 under their true names | **map defect.** All three were named as Dance Central `MoveDetectorCmp` instantiations, which came in through a scatter `#include "hamobj/MoveAsyncDetector.cpp"` in `FileCache.cpp`. Retail's `__adjust_heap` compares a signed int at `+0x28` with `>`, which is `FileCacheEntry::mPriority` under FileCache's own `Priority` comparator. Map renamed, the include deleted, and the alias survivors relabelled. The two MoveDetector spellings were withdrawn (`CALLEE_REFUTED_CANNOT_FOLD`) because their callee is the refuted body. | `48ca361ee` |
| `vector<MemDiffEntry>::_M_insert_overflow_aux` | 404 | 99.782 → 100 | **map + splits defect.** Retail's `MemDiffEntry` is 0x48 (its own `__push_heap` strides 0x48). The 0x38-stride body at `0x826b8d20` is `vector<VocalPhrase>`'s overflow: it sits among VocalNoteList's COMDATs and calls VocalPhrase helpers. Its `.text` block moved from MemTracker to VocalNoteList. The real MemDiffEntry body is the anonymous `fn_827D60A0`, now named. Two new folds were admitted, both CHASED T1 PROVEN with 0 CYCLE-ASSUMED: the `RndLine::Point` overflow folds into `0x827d60a0`, and MemDiffEntry's `__uninitialized_copy` joins the ExtraTail group at `0x826efbe0`. | `3aa935462` |
| `vector<VocalPhrase>::_M_insert_overflow_aux` + its 44 B funclet | 404+44 | newly paired at 100 | same commit | `3aa935462` |
| `RndText::DeferUpdateText` (was "NetCacheMgr `_List_iterator::operator--`", 84.5) | 16 | newly paired at 100 | **map + splits defect.** The 16 B body is `mDeferUpdate(+0x178)++`, called from UILabel (×3, via `RndTextUpdateDeferrer`) and TrackDir. Its one-function `.text` block sat in NetCacheMgr between two Text blocks. Map renamed, block re-homed to Text.cpp, and the iterator spelling REFUTED on bytes. | `6dd4e4203` |
| `BandConfiguration::OnStoreConfiguration` | 160 | 98.125 → 100 | Bind `const Transform &xfm = bchar->LocalXfm()` before the name store. Retail keeps `bchar` in r3 across the null test and forms the memcpy source first. | `106622ad4` |
| `WorldDir::SetCrowds` | 104 | 96.154 → 100 | Retail's not-found path has `mr r11,r10` (iterator = end): the lookup is a `FindCrowd` helper that `return vec.end()`s. Retail keeps no standalone copy (only EH funclets precede SetCrowds at `0x824CBA88`), so the helper is file-static. | `4a6f8ce7d` |
| `String::insert(unsigned, const String &)` | 12 | 46.667 → 100 | Name the c-string (`const char *s = str.mStr`). Retail loads it into r6 before `li r5,0`. | `94b4cf777` |
| `String::rfind(const char *)` | 140 | 93.000 → 98.571 | Retail sets its offset counter only after the first `*p` test, which is a compiler induction variable. Writing the offset as `(p - str)` removes our hoisted `li`. The rest is an r29/r30 swap (§3). | `94b4cf777` |
| `PatchDir::GetSticker(Symbol,int,bool)` | 108 | 99.815 → 100 | **wrong-callee row of the correctness sweep.** Retail calls the Symbol `_M_find` at `0x82557770` (128 B). Our `PatchDir::SymbolHash` took `Symbol` by value, so our `_M_find` compiled to 120 B and the fold was REFUTED on bytes. With `operator()(const Symbol &)` the body is 128 B, FLAT and CHASED T1 are both PROVEN, and the spelling is admitted into group `CVEIN1__M_find`. | `978495f16` |
| `PatchDir::GetStickers(Symbol)` | 56 | 99.643 → 100 | same | `978495f16` |
| `PatchDir::LoadStickerData` | 732 | 98.852 → 98.880 | same call site | `978495f16` |

### 2.1 How `.bss` placement of internal statics is decided (ChordShapeGenerator)

Five knobs were measured, one compile each, against the one placement that matters (`vertIt` at
`faceIt-4`):

- declaration order of the plain `static int vertIt; static unsigned faceIt;`: **inert**
- first-reference order (swapping the two assignments): **inert**
- renaming one static: **inert**
- **explicit `= 0` on both: controls it.** The front end then defines each static at its declaration, and
  declaration order becomes ascending `.bss` order.

Plain uninitialised statics keep an order the code generator picks. Its only visible rule here was "the
static first referenced in the *last* function of the file gets the higher address".

## 3. Recorded stops

| row | size | fuzzy | why it stops |
|---|---:|---:|---|
| `TrackPanelDir::UpdateTimeInfo` | 948 | 99.093 | Callee-saved numbering (r29/r30) on the function-static guard and `in_mode` Message. Retail re-materialises the `FilePath` temporary's address after `SetProxyFile`, where we keep it. A named `FilePath` local was measured worse (99.093 → 96.857), so it was reverted. |
| `GemManager::GemManager` | 896 | 99.946 | Two `stb` stores (`0xcc`, `0xc4`) swap places. Store scheduling inside the init list; reordering the initialisers does not move them. |
| `MemInit`, `MemPrintOverview`, `MemFindHeap`, `MemFindAddrHeap` | 788+300+184+96 | 99.5–99.99 | **Layout.** Retail's anchor register addresses MemMgr's `.bss` statics at offsets ours does not have (`MemInit`: retail `anchor+0x4`, ours `anchor+0x3f4`). That takes a reconstruction of the whole MemMgr/MemHeap `.bss` block, not a one-row edit. The coordinator has given this to lane **W16-PI** ("the MemMgr memory layout"), so I left it there. |
| `BandSongMgr::SyncSharedSongs` | 668 | 99.952 | The 16-byte memberwise copy inside the second inlined `std::set<int>` constructor. Retail emits word `0xc` first (as it does for the *first* set's constructor in the same function); ours copies in order. The preceding instructions are identical, so this is list scheduling. No source construct was found that steers it. |
| `PreInitSystem`, `InitSystem` | 652+228 | 99.98 / 99.90 | The `.bss` layout is correct. Retail picks a different static as the co-addressing anchor, and five knobs were measured inert (zero-init, rename, function order, declaration order, reference order). |
| `BandDirector::OnGetFaceOverrideClips` | 648 | 99.167 | Retail threads the null test through `ObjPtr OverrideDir` (a `cmplwi` on cr0 against our cr6 compare). The ternary form was measured worse and reverted. |
| `ClosetPanel::CycleCamera` | 480 | 99.983 | Temp `String` slot permutation (the `substr` temporary is at 0x78 in retail, 0x88 in ours). A scoped `substr` was measured worse. |
| `SndAnalysis RefinePeriod2` | 352 | 99.977 | Load order. Swapping operands is inert, and a pointer walk was measured worse. |
| `Stats::EndMultiplier`, `BandCharacter::OnClosetTeleport` | 192, 112 | 99.75 / 99.21 | Already recorded in source by earlier lanes; nothing new found. |
| `JoypadController::IsCymbal` | 264 | 99.848 | Basic-block placement only. |
| `GameGemList __adjust_heap` | 264 | 99.818 | Load order only. |
| `TambourineManager::TambourineSwing` | 288 | 95.833 | Retail hoists the shared `mr r3,r31` (`this`) above the three-way branch; we emit it per path. The per-branch shape (increment and return inside each branch) was measured **worse** (95.8 → 74.9) and reverted. |
| `UTF8toASCIIs` | 128 | 96.250 | Retail carries a dead `mr r11,r4` (a copy of `len` that is overwritten unread). No source construct identified. |
| `GameMicManager SetEnable` (static) | 112 | 88.214 | Our bool select goes through an extra `mr r9,r11`. A named `bool` local was **inert**; an `if`-assigned bool was **worse** (64.8). |
| `GameGem::PackRealGuitarData` | 108 | 95.185 | Prologue scheduling of `idx = 0` against `&mFrets`. A `for (i = 0, idx = 0; …)` init was inert, and initialising `idx` before the store was worse. |
| `BandCamShot::Target::operator=` | 248 | 99.597 | The first bitfield copy (the bool at mask 0x80) is emitted as "insert the old byte's 0x7f into the new" where retail emits "insert the new 0x80 into the old". This is the same semantics with a complementary `rlwimi` mask; the declared field types match retail's units (see `BandCamShot.h`). |
| `PatchSticker::MakeLoader` | 92 | 98.696 | The `__RTDynamicCast` target descriptor is the right type: retail's unnamed `lbl_82C6B478` decodes to `.?AVFileLoader@@`. The only difference is which descriptor `lis` is scheduled first, plus `li r4`/`li r7` order. |
| `String::rfind` residual | 140 | 98.571 | r29/r30 swap between `start` and the induction variable. Add-operand order is inert; defining `start` before the null test is worse. |
| `?SyncProperty@BandUser@@$4PPPPPPPM@PPPPPPCE@…` (thunk) | 16 | 97.250 | **Map defect, no witness.** Retail `0x8268ed50` is a vtordisp{-4,4} thunk (`lwz r11,-4(r3); subf; subi r3,r3,4; b fn_823591E8`). Its target is `li r3,0; blr`. The thunk sits in two vtables whose COLs name **`.?AVAutoplayAuditionUser@@`** (subobject offsets 0x70 and 0x2C). That class has no source in any tree. The map's BandUser spelling is therefore unproven, and renaming needs a class we do not have. |

## 4. Unpaired named rows (correctness-sweep item 4b, 27 rows / 1,852 B)

All 27 were classified by whether any of our objects defines the map's spelling:

- **14 rows: no object defines the name.** The source needed for them is absent.
- **13 rows: the name is defined only in a different unit.** These are COMDAT re-homing cases.

Re-homing is not metric-neutral (`pin-neutrality-scoped`), and each needs the owning TU to instantiate the
template. None was attempted. The list is in `~/tmp/w16pg_unpaired.txt`, reproduced below for pickup:

```
(class U3 = defined in another unit; U4 = defined nowhere)
U4  252 system/obj/Dir                 base=m/obj/Dir.obj                            defs=[] ??$__find_if@PAVFilePath@@P6A_NABV1@@Z@stlpmtx_std@@YAPAVFilePath@@PAV
U3  164 GemTrackDir                    base=m/bandobj/GemTrackDir.obj                defs=['system/bandobj/TrackPanelDir.obj', 'system/gesture/SkeletonViz.obj', 'system/movie/Splash.obj'] ??$Find@VRndCam@@@ObjectDir@@QAAPAVRndCam@@PBD_N@Z
U4  116 GemTrackResourceManager        base=m/bandobj/GemTrackResourceManager.obj    defs=[] ?_M_clear@?$vector@VSmasherPlateInfo@GemTrackResourceManager@@V?$StlNo
U4  116 BandStarDisplay                base=m/bandobj/BandStarDisplay.obj            defs=[] ??1?$ObjPtr@VBandStarDisplay@@@@UAA@XZ
U3  104 Msg                            base=m/obj/Msg.obj                            defs=['system/obj/Object.obj', 'system/obj/DataFunc.obj', 'system/obj/DataNode.obj', 'system/obj/PropSync.obj'] ?SetObjConcrete@?$ObjPtr@VObject@Hmx@@@@QAAXPAVObject@Hmx@@@Z
U4  100 MetaPerformer                  base=/meta_band/MetaPerformer.obj             defs=[] ??$SendDataPoint@PBDH@@YAXPBD0H@Z
U4   96 band3/bandtrack/Gem            base=/bandtrack/Gem.obj                       defs=[] ??$__destroy_range_aux@V?$reverse_iterator@PAUUnlockable@?A0xf8e4b4b5@
U4   96 band3/bandtrack/Gem            base=/bandtrack/Gem.obj                       defs=[] ??$__destroy_range_aux@V?$reverse_iterator@PAULabel@?A0x81ddebd1@@@stl
U4   88 system/obj/Dir                 base=m/obj/Dir.obj                            defs=[] ??$remove_copy_if@PAVFilePath@@PAV1@P6A_NABV1@@Z@stlpmtx_std@@YAPAVFil
U4   88 system/obj/Dir                 base=m/obj/Dir.obj                            defs=[] ??$remove_if@PAVFilePath@@P6A_NABV1@@Z@stlpmtx_std@@YAPAVFilePath@@PAV
U4   88 Msg                            base=m/obj/Msg.obj                            defs=[] ??$_M_find@VSymbol@@@?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@stlpmtx_std
U4   88 band3/game/PerfectSectionTracker base=/game/PerfectSectionTracker.obj          defs=[] ??$_M_find@H@?$_Rb_tree@HU?$less@H@stlpmtx_std@@U?$pair@$$CBHVSongStat
U4   80 DataPointMgr                   base=m/utl/DataPointMgr.obj                   defs=[] ?GameModeTerminate@@YAXXZ
U3   72 Tour                           base=/tour/Tour.obj                           defs=['system/obj/Object.obj'] ?SyncProperty@Object@Hmx@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@
U4   68 Accomplishment                 base=/meta_band/Accomplishment.obj            defs=[] ?push_back@?$ObjPtrList@VRndGroup@@VObjectDir@@@@QAAXPAVRndGroup@@@Z
U4   68 band3/game/Stats               base=/game/Stats.obj                          defs=[] ??_ETrainerPanel@@UAAPAXI@Z
U3   60 SongData                       base=m/beatmatch/SongData.obj                 defs=['system/synth/VorbisReader.obj'] ??$_Param_Construct@V?$vector@FV?$StlNodeAlloc@F@stlpmtx_std@@@stlpmtx
U4   24 Joypad                         base=m/os/Joypad.obj                          defs=[] ?NuipTrueColorSetPlayer@TrueColor@@YAXW4_NUIP_CAMERA_OWNER@@K@Z
U4   16 Object                         base=m/obj/Object.obj                         defs=[] ?SetFileChecksumData@@YAXXZ
U3   16 MetaPanel                      base=/meta_band/MetaPanel.obj                 defs=['system/bandobj/MiniLeaderboardDisplay.obj', 'system/hamobj/MiniLeaderboardDisplay.obj'] ?Highlight@UIComponent@@$4PPPPPPPM@3AAXXZ
U3   12 VocalTrackDir                  base=m/bandobj/VocalTrackDir.obj              defs=['system/ui/UILabel.obj', 'system/hamobj/HamCamTransform.obj'] ?Copy@UILabel@@$4PPPPPPPM@A@AAXPBVObject@Hmx@@W4CopyType@23@@Z
U3   12 system/ui/UIProxy              base=m/ui/UIProxy.obj                         defs=['system/ui/UIPicture.obj'] ?Load@UIPicture@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z
U3    8 User                           base=m/os/User.obj                            defs=['band3/game/BandUser.obj'] ?GetRemoteUser@RemoteUser@@UAAPAV1@XZ
U4    8 band3/game/BandPerformer       base=/game/BandPerformer.obj                  defs=[] ?SetReleaseSmoothing@PitchCorrectedVoice@Synapse@DSP@@QAAXM@Z
U3    4 SongSortMgr                    base=/meta_band/SongSortMgr.obj               defs=['system/beatmatch/SongData.obj', 'system/bandobj/BandList.obj', 'system/bandobj/VocalTrackDir.obj', 'system/bandobj/BandCharDesc.obj'] ??1?$map@HMU?$less@H@stlpmtx_std@@V?$StlNodeAlloc@U?$pair@$$CBHM@stlpm
U4    4 band3/meta_band/AssetMgr       base=/meta_band/AssetMgr.obj                  defs=[] ??1?$_Rb_tree@HU?$less@H@stlpmtx_std@@U?$pair@$$CBHVString@@@2@U?$_Sel
U3    4 system/bandobj/BandHeadShaper  base=m/bandobj/BandHeadShaper.obj             defs=['system/bandobj/BandDirector.obj', 'system/bandobj/BandLabel.obj', 'system/bandobj/BandCamShot.obj', 'system/rndobj/PropKeys.obj'] ??1MemHeapTracker@@QAA@XZ
```

## 5. What I did not do

- Did not touch `src/network`, `src/xdk`, or the allocator TUs W17 owns (`PoolAlloc`, and `MemHeap::Print`
  beyond reading it). The MemMgr layout is left to W16-PI.
- Ran no permuter (deferred by directive).
- Did not re-home any of the 13 cross-unit COMDATs in §4.
