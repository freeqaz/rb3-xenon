# W16-PC: in-scope source-divergence rows (I1/I3/I4), 2026-10-03

Lane W16-PC took lever 4(b) from `CAMPAIGN_STATE_2026-10-03.md` §4.4: the in-scope I1/I3/I4 rows that had
no per-row record. That is 157 rows / 56,640 B in `src/band3` and
`src/system/{bandobj,beatmatch,obj,os,utl,math,track,midi,dsp}`. Rows already worked by the W16-N\*/O\* docs
were skipped. The rows were split into five groups, each worked largest size×(100−fuzzy) first:

| group | scope | worked by |
|---|---|---|
| g1a | bandobj, first half | the lane itself |
| g1b | bandobj, second half | fork `w16-pc-g1b` |
| g2 | math / dsp / track / midi | fork `w16-pc-g2` |
| g3 | os / obj / utl | fork `w16-pc-g3` |
| g4 | band3 + beatmatch | fork `w16-pc-g4` |

Each fork ran in its own worktree and checked its rows against a baseline `report.json` with a full build.
The forks did not run `ab_measure` or the native gate; the lane runs both once, on the merged branch.

The campaign doc predicted about 1–4 KB from this lever. The measured +11,820 B is about three times the top of that range; part of it comes from map and pin fixes and from side-effect rows outside the I1/I3/I4 list (see §A/B).

## A/B (whole branch, `tools/ab_measure.py`)

- **What was measured:** one run of `tools/ab_measure.py --worktree ~/tmp/wt-w16pc-ab --patch branch.patch`.
  - The patch is `git diff main HEAD` on `w16-pc`, after rebasing onto main `1e7c0c1aa`.
  - Patch kinds: source, map, splits, configgen.
  - Both legs settled and both were at a split fixed point (0 extra re-splits).
  - Leg B recompiled 1,013 TUs and the renamer patched 1,845 files.
  - Run dir: `~/tmp/wt-w16pc-ab/.ab_measure_runs/20261003-101500-w16pc-branch-1046635/`.

| measure | leg A (main) | leg B (branch) | Δ |
|---|---:|---:|---:|
| matched_functions | 53,351 | 53,449 | **+98** |
| masked_equal | 25,155 | 25,191 | +36 |
| honest | 28,196 | 28,258 | **+62** |
| matched_code_percent | 57.496445 | 57.611786 | **+0.115341 pp (+11,820 B)** |
| fuzzy | 63.626110 | 63.684340 | +0.058230 pp |
| units at 100% (mpn) | 533 | 545 | +12 (11 MATCHED_ROSE, 1 DENOMINATOR_SHRANK: Symbol, after `fn_82C40F08` was re-homed to Locale) |

- **The `none` control** read +12,452 B. It is NOT_APPLICABLE as an alias check, because the patch has
  source in it.
- **No row went down.** A row-level compare of the archived `legA_report.json.gz` and `legB_report.json.gz`
  (both `fuzzy_match_percent` and `match_percent_normalized`) found:
  - 68,909 rows on both sides: **94 up, 0 down**.
  - 28 rows left one unit and 28 appeared in another. These are the re-homes and map names in the tables
    below. Each moved address scores at least what it scored before.
- **MidiParser's −2 is reattribution.** `ab_measure` lists one unit regression, `MidiParser` (−2 matched).
  Its 22 TrackWidget/TrackWidgetImp EH funclets were re-homed out of it. Two of them were already at
  `mpn` 100 in MidiParser, and both are 100 in their new units.


## Findings that generalise

1. **Retail's rev statics are a file-local adjacent pair.** Retail addresses `gAltRev` and `gRev` off one base
   register, at +0 and +4. `DECLARE_REVS` class statics give two separate relocations, so the PreLoad/Load
   rows sit at 84–89%.
   - Writing `static unsigned short gAltRev = 0; static unsigned short gRev = 0;` in the .cpp makes them 100.
   - That was 11 classes in this lane: BandCharacter, BandFaceDeform, BandIKEffector, and eight in g1b.
   - **Trap, measured on BandCharacter:** inside a member function, an unqualified `gRev` finds a *base
     class's* `gRev` before the file static. BandCharacter derives from BandCharDesc, so after the header
     statics were removed, `LOAD_REVS` silently wrote `BandCharDesc::gRev`. It compiled cleanly. The only
     sign was the shape: PreLoad still showed two relocations.
   - The fix is to spell the macro out with `::gRev` / `::gAltRev` in PreLoad, and qualify every use in
     PostLoad.
   - Before converting any other class, check its bases for `DECLARE_REVS`.
2. **The shared-modify tail.** In BandCharDesc's custom PropSyncs, retail places a single
   `if (!(op & (kPropSize|kPropGet))) SetChanged(); return true;` tail after the last property arm, and
   every arm jumps to it.
   - A TU-local `SYNC_PROP_SHARED_MODIFY` that ends each arm in `goto _modified` reproduces it.
   - Both rows went 0 → 99.33; the residue is register-only.
   - The macros are deliberately TU-local and not in `ObjMacros.h`, which is a PCH input.
3. **Out-of-line "inline" helpers.** AppendDeltas in retail calls a separate CompressDelta.
   - We placed that helper as an `inline` function, and the compiler kept it out of line.
   - Its retail address was pinned inside BandRetargetVignette. It is now re-homed to BandFaceDeform
     (a splits.txt boundary move plus a map name) and matches at 100.
4. **Conditional-expression returns build a temporary.** `return c ? a() : Symbol();` builds the value in a
   stack temp and copies it out. The if/else form constructs straight into the return slot.
   - `Object::Type` needed the conditional form.
   - `Object.h` is a PCH input, so this change was measured with a full rebuild: 0 rows down across the
     whole binary.
5. **Control-flow shape is real behaviour, not style.**
   - In BandCharacter::Filter, retail checks the outfit/resource/to dir only in the `else` of the
     instrument-dir test. An instrument object that does not resolve goes straight to the bone logic.
   - Retail tests `o2` once and checks one class per arm: with no `o2`, drop an AmbientOcclusion; with an
     `o2`, keep a CharWeightSetter. Ours had applied both checks to the null-`o2` case only.
   - Matching retail's flow took the row from 73.63 to 100.
6. **Placeholder forgiveness hides wrong strings and wrong variables.** These were all real behaviour or
   identity defects that objdiff only partly charged, because the retail side's names are placeholders:
   - BandCharacter::OnInstallFilter looked up `"bone_pelvis.mesh"`; retail looks up `"world.wind"`.
   - Filter's static Symbols were guarded in the wrong order (retail's string order proves the right one).
   - SetInterestFilterFlags did not raise CharEyes's filters-changed byte.
7. **Wrong map names that are cheap to verify on retail bytes.**
   - `0x8228D2B8` and `0x822B0B88` are `Hmx::Object::New<RndPropAnim>` and `New<EventAnim>`. Each body calls
     `T::StaticClassName`, then `NewObject`, then `__RTDynamicCast` to T.
   - `0x822B7BA8` is `list<EventAnim::KeyFrame>::_M_create_node`, not `_Destroy_Range<SingerStats>`. Its body
     is `MemOrPoolAlloc(0x18)` plus `_Copy_Construct<KeyFrame>` at node+8, inside an EH frame.
   - Our objs already define all three names, so naming them pairs all three rows at 100.

## Per-row record

### g1a (bandobj, lane itself)

Before/after are graded fuzzy from `report.json`. "Before" is main `83e92ed07`; "after" is this branch's full
build. Rows are listed in the order worked.

| row | size | before → after | what changed / why it stops |
|---|---|---|---|
| `BandCharDesc` PropSync(Patch) | 656 | 0 → 99.33 | Shared-modify tail (§2). Residue: register-only (`_op` r24 vs r26). |
| `BandCharDesc` PropSync(OutfitPiece) | 480 | 0 → 99.33 | Same as the Patch PropSync. |
| `DeltaArray::AppendDeltas` | 584 | 0 → 95.07 | Retail's shape around an out-of-line CompressDelta, which keeps its `fctidz` byte conversion. Residue: a this/pos register swap and one missed base-load CSE. |
| `CompressDelta` (new pair) | 148 | — → 100 | Re-homed from BandRetargetVignette's pin and named in the map. |
| `BandIKEffector::DoFancyElbow` | 1,040 | 47.73 → 99.85 | Rewritten around ScaleAdd/ScaleAddEq, which retail calls out of line. Residue: commutative fmuls operand order on the elbow quat. |
| `BandIKEffector::NeutralLocalXfm` | 440 | 76.15 → 99.82 | One `channel` Symbol reassigned; rows scaled with `*=`. Residue: commutative operand order. |
| `BandIKEffector::Load` | 256 | 88.97 → 100 | Rev-statics pair (§1). |
| `BandIKEffector::ComputeElbowPullAndQuat` | 184 | 92.5, stop | Retail reloads `outQuat.v.x` and evaluates dy first. Two rewrites scored 72.65 and 91.52; both reverted. |
| `BandCharacter::Filter` | 1,220 | 73.63 → 100 | Retail's control flow (§5). Also: an inline or-chain predicate, `= 0` dir statics, static-Symbol guard order, `strnicmp(literal, name)`, and `action == kMerge \|\| kReplace`. |
| `BandCharacter::SetState` | 356 | 84.10 → 100 | `b4` written as one or-chain. |
| `BandCharacter::AddDircut` | 232 | 87.76 → 100 | Mask as a conditional on `mGender == "female"`. |
| `BandCharacter::ComputeScreenSize` | 40 | 19.0 → 100 | Early return on a null outfit dir. |
| `BandCharacter::DrawShowing` | 68 | 0 → 100 | The debug overlay is gated `MILO_DEBUG && HX_NATIVE`. Its strings (`bandcharacter.show_spheres`, `slot%d pos%d`) are absent from retail. |
| `BandCharacter::SetInterestFilterFlags` | 32 | 75.0 → 100 | Also raises CharEyes+0x168, as `Character::SetInterestFilterFlags` does. **Behaviour.** |
| `BandCharacter::OnInstallFilter` | 904 | 97.34 → 100 | Retail looks up `"world.wind"`, not `"bone_pelvis.mesh"`. **Behaviour.** |
| `BandCharacter::PreLoad` | 140 | 84.29 → 100 | Rev-statics pair, written with `::` (§1 trap). |
| `BandCharacter::StartLoad` | 292 | 81.73 → 94.16 | Retail's logic order. Residue: retail gives the old `mInCloset` byte a stack home; no spelling tried reproduces it. |
| `BandCharacter` ReplaceRefs | 236 | 99.97 → 100 | Moved as a side effect of the Filter work. |
| `Hmx::Object::Type` | 96 | 73.63 → 100 | Conditional-expression return (§4). PCH input; full rebuild, 0 rows down. |
| `BandWardrobe::AddDircut` | 504 | 80.79 → 99.92 | `/DRB3_NOTIFY_ONCE_EVAL`: retail evaluates `PathName(shot)` in the stripped NOTIFY_ONCE. Also `(genderflags & flag) != genderflags`. Residue: operand order of one add in the strength-reduced walk. |
| `BandWardrobe::OnGetMatchingDude` | 232 | 77.55 → 94.40 | One condition chain comparing the two GetAnimInstrument temporaries. Residue: retail pre-loads bc's instrument before the first call. The swapped operand order scored 94.31. |
| `BandWardrobe::InstrumentMatch` | 152 | 84.87 → 100 | `score != 0 && MinEq(bestScore, score - human)`. |
| `BandCharDesc::DrumCallback` | 152 | 76.61 → 100 | `char buf[256]` and `sDrumVenueMappings[i * 2]`. |
| `BandCamShot::Freeze` | 148 | 85.08 → 100 | Freeze inside the null test, with no stored bool. |
| `BandFaceDeform::Load` | 120 | 88.67 → 100 | Rev-statics pair. |
| `fn_8228D2B8` → `Hmx::Object::New<RndPropAnim>` | 72 | 61.94 → 100 | Map name (§7). |
| `fn_822B0B88` → `Hmx::Object::New<EventAnim>` | 72 | 61.94 → 100 | Map name (§7). |
| `0x822B7BA8` → `list<EventAnim::KeyFrame>::_M_create_node` | 72 | 0 → 100 | Wrong map name corrected (§7). |
| `BandPatchMesh::FindXfm` | 1,268 | 75.18, stop | Retail keeps posOut's rows in f26–f31 across three calls (Normalize, the Vert ctor, MeshVert::Normalize). That needs the compiler to treat posOut as not escaped, and no source shape found does it. Direct indexing and an inner scope were both byte-identical (75.18); reverted. |
| `WorkVerts::ExtendTwin` | 716 | 84.96, stop | Register allocation and fsub operand order. Diffed, not rewritten. |
| `WorkVerts::TryAddFace` | 824 | 90.87, stop | FP scheduling and operand order. |
| `MeshVert::AddUV` | 492 | 95.54, stop | Retail does not fuse `v50 += a*b*c` (fmuls + fadds), and it loads the unk10 dot first. Parens scored 95.58 (no fusion change); a split temp scored 95.50; reverted. |
| `Invert(Matrix2)` | 128 | 92.28, stop | Commutative operand order in the determinant. Store-order variants scored 86.28 and 92.28; reverted. |
| `PatchPair::PatchPair` | 96 | 45.0, stop | Retail calls `ObjPtr<RndTex>`'s constructor out of line while inlining `ObjPtr<RndMesh>`'s. No per-site source lever. |
| `BandDirector::BandDirector` | 1,088 | 98.27, stop | One constant load and store scheduled differently. |
| `SetMeshAnim` | 852 | 98.78, stop | One `fmr`/`add` scheduling slip. |

### g1b (bandobj, fork `w16-pc-g1b`)

| row | size | before → after | what changed / why it stops |
|---|---|---|---|
| LayerDir::GetBitmapList | 692 | 84.85 → 99.65 | Reads the layer name through `this->Property`. **Behaviour bug:** the old code called Property on `*(Object**)arg`. The 10 LayerDir EH funclets went to 100. Residue: one register copy. |
| GemTrackDir::SetPlayerLocal | 152 | 65.21 → 100 | Retail's single `(parent && HasNetPlayer) \|\| simulated` condition. |
| WhiteKeyToSemitone | 140 | 76.26 → 100 | Loop bound `> kWhiteKeyB`; explicit empty C case. |
| SemitoneToWhiteKey | 96 | 50, stop | dtk carves the case bodies off as `fn_822E3630`. **Native bug fixed anyway:** C/C# had no case and fell into MILO_FAIL. |
| BandRetargetVignette::Poll | 280 | 81.01 → 90.11 | `/DRB3_NOTIFY_ONCE_EVAL`. Residue: load and argument order. |
| BandList::StartFocusAnim | 320 | 89.22 → 100 | Reads `mFocusAnim` directly and branches on the state. |
| BandList::UpdateConcealState | 228 | 80.0, stop | Retail merges the duplicated ForceConcealed blocks into the first copy; three spellings tried. |
| BandTrack::SetTourMomentGoalText | 248 | 84.89 → 100 | `new BandLabel*(label)`, with the use and delete outside the null test. |
| BandTrack::SoloHide | 116 | 87.93 → 100 | `SetFrame(anim->EndFrame(), 1)`. |
| BandTrack::SpotlightPhraseSuccess | 144 | 94.14 → 100 | Passes `mTrackIdx` directly. |
| BandTrack::Reset / SetupCrowdMeter | 1,116 / 320 | 98.45 / 98.75, stop | Load scheduling, and one unused stack store. |
| PitchArrow::SetVolume | 132 | 78.48 → 100 | Float `Clamp`. Differs only when `mVolume > 1.05`. |
| VocalTrackDir::RecalculateLyricZ | 148 | 89.19 → 100 | The lead-Z test compares against the stored members. |
| PatchLayer::Rotation | 36 | 65.0 → 100 | `mRot * (360.0f / 511.0f)`. |
| PreLoad/Load of ScoreDisplay, DialogDisplay, ReviewDisplay, PatchDir, CrowdMeterIcon, BandHighlight, OverdriveMeter, UnisonIcon | 108–192 | 78–90 → 100 | Rev-statics pair (§1). |
| OverdriveMeter::SyncObjects, ChordShapeGenerator::InterpolateXfm, NoteTube::CreateMeshes, BandLeadMeter::SyncScores | 332–784 | stop | Argument and FP scheduling. BandLeadMeter: three variants scored the same or worse. |
| PatchDir `Object::operator=` | 112 | 77.61, stop | Retail copies a `list<Symbol>` at +0x20, where our Object has `mRefs`. That needs `obj/Object.h` (see g3's Object::Release). |

### g2 (math / dsp / track / midi, fork `w16-pc-g2`)

| row | size | before → after | what changed / why it stops |
|---|---|---|---|
| MakeBSPTree | 1,580 | 84.68 → 95.45 | **Behaviour bug:** faces were added to child lists at `begin()` (each list reversed), and a split face whose back half clipped away was skipped. Residue: frame and slot layout. A tighter tail reached 98.12 but dropped three EH funclets, so it was backed out. |
| Intersect(Triangle, Box) | 848 | 76.19 → 97.16 | Vector3 vertex locals, a min/max per axis, and the axes table in retail's order. |
| Multiply(Transform) | 248 | 61.73 → 100 | Rotates into `out` directly; uses a temporary when `out` aliases the input. |
| Multiply(Matrix3) | 680 | 74.15 → 74.82 | Store order; stops on FP association. |
| Multiply(Vector3, Quat) | 192 | 76.42 → 85.73 | Term order (all six statement orders swept); stops on FP register numbering. |
| QuatSpline | 524 | 94.50 → 100 | Flat Catmull-Rom sum per component. |
| DefaultMidiLess | 88 | 76.27 → 100 | `MidiRank` made `inline` (a COMDAT, so the caller has no IPA register knowledge). |
| TrackWidget::Mats | 192 | 89.17 → 100 | Mask is `0xC \| bit`. |
| Intersect(Segment, Sphere) | 284 | 93.80 → 100 | Interp into one vector; early out. |
| Rand::Seed | 80 | 82.25 → 100 | Unsigned shift, then or. |
| ~CharWidgetImp / CharWidgetImp ctor | 112 / 404 | 88.89 / 97.91 → 100 | Implicit destructor; style read through `GetSingleStyle()`. |
| `fn_827E4438` and 10 siblings | 44 | 21.27 → 100 | TrackWidget ctor EH funclets, re-homed from MidiParser.cpp. Another 11 CharWidgetImp funclets: 6 at 100, 5 at 99.5 (callee-name residue). |
| `fn_823710B8` → `Character::RemoveFromPoll` | 44 | 75.45 → 100 | **Behaviour/native bug:** the function had no body, and the native stub was inert. Pin moved from TrackDir to Character.cpp, map name added, native stub deleted. |
| operator>(Sphere, Frustum) | 372 | 97.96 → 99.89 | `\|\|` chain over an inline BehindPlane helper. Residue: one operand order. |
| MakeEulerScale, ShiftedDotProduct, MakeRotQuat, Intersect(V3, V3, Box) | 188–356 | stop | Retail's memory reload order, its loop induction pointers, and FP/loop register allocation. |
| FindCCPeak, Quat::Set, Clip, Intersect(Segment, Triangle) | — | not worked | FP ordering only. `fn_82397870` was a stale row. |

### g3 (os / obj / utl, fork `w16-pc-g3`)

| row | size | before → after | what changed / why it stops |
|---|---|---|---|
| ReadSingleXinputJoypad | 812 | 81.83 → 99.78 | Retail's flow. **Six behaviour bugs:** the SubType switch, the guitar flag, drum-stick scaling, the rx drum path, the keyboard sustain bit, and trigger bits set only on kJoypadAnalog. |
| NewFile | 424 | 72.23 → 84.39 | **Behaviour bug:** the AsyncFile mask was 0x4000; retail clears 0x30000. Stops on a NullFile branch we keep. |
| Profiler::Stop | 464 | 85.59 → 100 | MinEq/MaxEq; the timer is re-read after each update, as in retail. |
| FormatString::Str | 60 | 0 → 100 | No `kNone` check in retail; made `noinline`, because inlining it dropped 3 MemHeap rows. |
| LocalUser::IsJoypadConnected | 48 | 0 → 100 | No `fake_controllers` override in retail. |
| IDataChunk::~IDataChunk | 88 | 63.23 → 100 | ChunkHeader has no class operator new/delete, so there is no EH frame. |
| `fn_82C40F08` → TheLocale static initializer | 32 | 14.38 → 100 | Re-homed from Symbol.cpp to Locale.cpp. Retail's Locale is 0x1C bytes. |
| MemAllocSize | 232 | 90.84 → 99.98 | Inlines the real `MemHeap::AllocSize`. Residue: the gNumHeaps `.bss` offset. |
| Archive::Merge | 516 | 92.39 → 93.71 | Holds entries and hash table by reference. Residue: store scheduling. |
| Object::Release / Object::Object | 152 / 96 | 77.34 → 77.61 / 72.08, stop | Retail's `mRefs` is a `std::list<ObjRefOwner*>`, which is an `obj/Object.h` change. |
| WaveFile `_Destroy_Range<Label>` | 96 | 34.63, not landed | The map name is wrong: retail's body is `__uninitialized_copy<WaveFileMarker>`. The rename scores 100 but shifts the anon_ns patcher's fallback hash, which costs 5 WaveFile rows 432 B. The patcher needs fixing first. |
| ThreadMemStack, MemFree, MemAlloc, AddHeap | 188–644 | stop | MemMgr `.bss` layout (MemHeap.cpp is included into the unit, interleaving the externs). |
| JoypadPollCommon, MemTracker ctor, FileMakePath, GetLoopTick, MemHeap::Init, ChunkStream::Eof, NetCacheMgr::AddLoaderRef, UsbMidiKeyboard::Poll, map<Symbol,DataNode>::operator[] | — | stop | Register, scheduling or stack-layout residue, or a PCH-header edit. |
| LoadDtz, DirLoader::SaveObjects, ThreadCallPoll, DataPoint::AddPair, DataStringFlags, ObjectDir::HasSubDir, CDReadExternal, Object::~Object, fn_82C3FA94, JoypadClient::Poll, BlockMgr::AddTask, DirLoader::~DirLoader | — | not worked | Turn budget. |

### g4 (band3 + beatmatch, fork `w16-pc-g4`)

| row | size | before → after | what changed / why it stops |
|---|---|---|---|
| GameGemList `__unguarded_partition` / `__unguarded_linear_insert` | 168 / 148 | 71.86 / 72.92 → 100 | Compares through the `less<GameGem>` functor. |
| AccomplishmentProgress::HandlePendingGamerRewards | 176 | 86.43 → 100 | Each award call's result is tested in its own branch. |
| VocalScoreHistory::CalculateSum | 64 | 65.94 → 100 | Reads through a local `const vector&`. |
| MassChannelMapping ctor | 148 | 91.35 → 100 | Inserts at `begin()` (the list is empty there). |
| JoypadController::GetVirtualSlot | 496 | 69.26 → 84.84 | Retail's switch over 0..4. Residue: block layout. |
| SymToTrackType | 108 | 23.48 → 40.37 | Tests the bound first. Retail's top-tested loop shape was not reproduced. |
| SongParser::StartVocalNote | 1,104 | 96.27 → 98.51 | The end tick is computed once. |
| RockCentral::DataPointToQString | 460 | 89.56 → 91.43 | Reads the node type at every use. |
| RndParticleSysAnim::Load (side effect) | — | 89.94 → 99.96 | Emit-rate keys built in place. |
| MasterAudio::FillSwing (side effect) | — | 61.51 → 76.86 | Indexes mTrackData once. |
| RemoteBandUser::GetLocalBandUser | 80 | 7.0, stop | Map defect: the address is a deleting destructor of an unidentified class. |
| RemoteBandUser::Handle thunk | 16 | 73.75, stop | Probable map defect: the thunk adds 0xdc into `BandUser::Handle`. Left for a map/vtable lane. |
| UpdateTambourineGems, TrimOverlappingGems, CalcPhraseScoreMax, UnflipGems, IsActive, IsModifierActive, SetLeaderboardRankAndName, SetButtonMashingMode, ParseText, AddGameGem, ProcessStaticLyrics, IsEmptyPhrase | — | stop | Scheduling, stack slots, an empty-frame residue, or loop shape; each was tried and reverted. |

## Behaviour bugs fixed (native-visible, not just codegen)

- **BandCharacter**
  - OnInstallFilter looked up the wrong object: `"bone_pelvis.mesh"` instead of `"world.wind"`.
  - Filter had the wrong o2 checks and the wrong dir test.
  - SetInterestFilterFlags did not raise CharEyes's filters-changed byte.
  - StartLoad's closet/reload logic was in a different order from retail's.
- **LayerDir::GetBitmapList** read the Property from the wrong object.
- **SemitoneToWhiteKey** fell into MILO_FAIL for C and C#.
- **MakeBSPTree** reversed the child face lists and skipped split faces whose back half clipped away.
- **Character::RemoveFromPoll** had no body. Its native stub is retired.
- **ReadSingleXinputJoypad**: six input-mapping divergences (see the g3 table).
- **NewFile** used the wrong AsyncFile flag mask.
- **FormatString::Str / LocalUser::IsJoypadConnected / Profiler::Stop** had dev-build branches that retail
  does not have.

## Left for other lanes (out of this lane's scope, or a shared-header change)

- **`obj/Object.h` `mRefs`.** Retail's `mRefs` is a `std::list<ObjRefOwner*>`, which affects
  Object::Release / Object::Object and PatchDir `operator=`. That is a PCH-input, whole-tree change.
- **WaveFile.** `_Destroy_Range<Label>` at its address is really `__uninitialized_copy<WaveFileMarker>`. The
  anon_ns patcher's fallback hash moves when the name changes, so the patcher needs fixing before the map
  edit can land without costing 432 B.
- **MemMgr `.bss` layout.** MemHeap.cpp is `#include`d into the unit, which interleaves its externs. Affects
  ThreadMemStack, MemFree, MemAlloc and AddHeap.
- **RemoteBandUser::GetLocalBandUser / RemoteBandUser::Handle thunk.** Map/vtable defects: the first address
  is a deleting destructor, and the thunk lands in BandUser::Handle.
- **MeasureMap.** Seen during g4 as a mis-pin and left untouched.

