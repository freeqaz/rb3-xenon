# W16-HZ — anonymous src/system rows: 175 named (71 re-homed), bodies repaired, three new identification passes (2026-09-30)

Continues W16-HS (`W16HS_SYSTEM_ANON_ROWS_OVERLAP_AND_CALLERS_2026-09-30.md`) and W16-HV
(`W16HV_REHOME_CALLER_PROVEN_FOREIGN_PINS_2026-09-30.md`) on the anonymous fuzzy-0 rows left in compiled
`src/system/` units outside `bandobj/`, `rndobj/`, `char/`, `rnddx9/` (HS's hamobj/gesture cut kept). `obj/`
template instantiations (Obj* families) were left to the other session. Rows named on main before this lane were
not body-edited (W16-HY owns the 1–90% band); `LightPreset.cpp` was fenced for part of the lane and never edited.
Branch `w16-hz`, rebased onto main `61bb82227`.

## Result

One `tools/ab_measure.py --patch` run over `main..w16-hz` (46 files: configgen, map, source, splits; the lane's
`symbols.txt` over-carve hunk committed into leg A, HK/HR/HS/HV recipe — `ab_measure` refuses `symbols.txt`
patches). `name_check` ruler, both legs settled and at a split fixed point (0 extra re-splits), leg B recompiled 995
TUs. Run dir archived to `~/tmp/w16hz/ab_run/`.

| | leg A | leg B | Δ |
|---|---:|---:|---:|
| matched_functions | 46,403 | 46,539 | **+136** |
| masked_equal | 23,689 | 23,699 | +10 |
| honest | 22,714 | 22,840 | **+126** |
| matched_code_percent | 45.650580 | 45.849037 | **+0.198457 pp (+20,336 B)** |
| fuzzy_match_percent | 54.578014 | 54.917706 | **+0.339692 pp** |

- Leg B equals the branch's in-tree build exactly (46,539 / 45.849037).
- **Predicted vs measured.** The in-tree step deltas recorded before the run (naming +25/+3,768; HV re-homes
  +4/+688; neighbour overlap +9/+1,844; over-carve heads +6/+916; body-rank +17/+1,712; forks U+X +56/+6,784;
  fork W +9/+2,680; fork R +9/+1,800; DrumMap +1/+144) sum to **+136 / +20,336 B** — reproduced exactly. My
  unsummed pre-launch guess (~+400 fns / ~60 KB) was wrong; I had not added them up.
- Units: 57 improved (+138), 2 regressed by one row each (Debug, synth/SynthSample — reattribution of rows moved
  out). Units at 100 (mpn) 244 → 243: UserMgr and DrumMap completed; World, Submix and MoggClipMap fell off because
  re-homed rows joined their denominators (truer denominator, not a loss of matched rows).
- **0 rows fell off fuzzy 100 anywhere.** 9 rows outside the lane's own set went down, none from 100 (listed in §8).
- `none`-ruler control: +22,584 B, NOT_APPLICABLE (patch carries source).

## 1. Population

Taken with HS's `sem.py` on the settled tree at lane start (main `b72a90b0e`): **1,362 targets / 171,612 B** after
HS's scope cuts (the brief's ~205 KB counts before the hamobj/gesture cut). HS's `callers.py` over it:

| caller class | rows | B |
|---|---:|---:|
| no caller witness | 979 | 116,552 |
| 1 name, defined in another unit's obj (HV's "foreign") | 180 | 29,668 |
| 1 name, no base definition | 100 | 12,052 |
| 1 name, own unit | 36 | 4,836 |
| fold soup / mapped or bound elsewhere | 67 | 8,504 |

## 2. Identification (all names written through `tools/gated_map_write.py`)

Every accept rule below was fixed before its rows were read, and every name passed retail-byte adjudication
(`tools/anon_proposal_adjudicate.py --independent`) with **no identity contradiction** (caller-contra, name bound or
mapped elsewhere). Body-only CONTRADICTED verdicts were accepted, as HV did, only when the name was top-ranked in its
destination obj; they are DC3-era source divergence (the forks then repaired most of them).

| pass | method | rows | B | at 100 now |
|---|---|---:|---:|---:|
| A | HS overlap union (ov ≥ 2, > runner-up) + own-unit caller class, HS/HR decide rules unchanged | 8 | 2,296 | 8 |
| B | **own-unit caller rows the single-entry def index misfiled as foreign** (below) | 45 | 9,848 | 37 |
| C | HV's mixed-gap / non-adjacent leads re-homed (gap audit + `runnerup.py` + `adj_rehome.py`) | 16 | 3,672 | 8 |
| D | **neighbour-obj overlap** + re-home | 16 | 6,656 | 9 |
| E | body rank of the 8 anonymous System-range rows against System.obj + re-home | 7 | 2,328 | 2 |
| F | **over-carve heads** (below) | 11 | 1,560 | 6 |
| G | **neighbour-obj body rank** + re-home (after the ??_G corrections, §3) | 31 | 5,000 | 16 |
| H | fork X: 40 "no base def" functions written, then the caller chain | 40 | 5,012 | 36 |
| I | `??0DrumMap` (fork R moved the ctor out of line) | 1 | 144 | 1 |
| **all** | | **175** | **36,516** | **123** |

71 of the 175 were re-homed (`.text` moved via HV's `apply_moves.py`; `.pdata` re-derived by the split).

**B — the def index was single-entry.** HS/HV classified "defined in another unit's obj" from one `base_index`
entry per name. Scatter-include hosts (`DepthBuffer3D.cpp` includes `ui/UIList.cpp`, `StringTable.cpp` includes
`movie/Movie.cpp`, `MoveMgr.cpp` includes `world/CameraManager.cpp`, …) define many names twice, so rows whose own
unit also defines the name read as foreign: **63 of HV's 180** "foreign" rows were own-unit. 56 non-Obj-template ones
went through HR's `decide_call.py` unchanged (49 accepted).

**D / G — neighbour objs.** Many anonymous rows sit in the wrong unit's pin, so an own-unit pool cannot see them.
D: pool = unclaimed functions of the units owning the nearest pinned blocks either side (≤ 0x2000), overlap
matcher, propose top if ov ≥ 2 and > runner-up (41 proposals → 20 in scope → 17 after destination-obj ranking).
G: the same pool scored by body on the graded ruler (19,020 diffs over 589 rows ≥ 64 B); accept iff top ≥ 60 and
top − runner-up ≥ 15 (79) → drop templates / anon-ns / non-bijective / mapped (43) → adjudication (36) → §3 (31).
Largest finds: `MetaPanel::Init` 1,676 B in Flow's pin (135 AGREE), `BandSongMgr::ContentDone` 1,220 B in SongMgr's,
`SongData::AddTrack` 916 B in UIList's, TrackWidget/CharWidgetImp/MultiMeshWidgetImp in MidiParser's, six World
`NewObject`/`WorldInit` rows in LightPreset's, `HttpGet` ctor in JsonUtils' (the pin problem HS fork D noted).

**F — over-carve heads.** 15 caller-bound rows were refused on size (retail 8–176 B vs our 44–288 B). Retail
`.pdata` shows each head is sub-`.pdata` leaf code inside another extent that **branches into** its contiguous
anonymous successor(s) — dtk split one function at a branch target (e.g. `StrNCopy` 0x827BDD60 is
`lbz; addi; b +0x1c` into 0x827BDD6C). Rule: branch-into-tail and |head + tail − ours| ≤ 8. Naming the head let
jeff's over-carve merge fold every tail; merged extents matched ours exactly for 6 (FileGetPath 200, findLatest 152,
NearestSustainRate 88, NextFillExtents 64, GetGemInProgressWithSlot 92, `__median` 288) and within 8 B for 5. This
explains HV's two "body contradicts the name" refusals of `findEarliest`/`findLatest` and `CapLastFreestyleSection`
(they scored the head alone).

## 3. Corrections and withdrawals (all on retail bytes)

- **`??_G` is ICF bait, and a body score of 100 does not identify it.** The discriminating operand (the stored vtable,
  or the `??1` callee) is often an unnamed placeholder, which `name_check` forgives. The ICF gate caught it. RTTI
  (vtable[−1] → COL → TypeDescriptor) and the `??1` callee settled each row:
  - 0x82531448 **is** `??_GNetworkSocket` and 0x82790490 **is** `??_GTrackWatcherParent`, and 0x827296C0 stores
    Mic's vtable (so it is `??_GMic`, not `??_GMicNull`). Group `_GCMemoryManagedUnknown` @0x82bf6f58 listed all
    three as folded there; its T1 proof masked exactly the vtable operand. The three memberships are **withdrawn**
    with `RTTI_PROVES_OWN_ADDRESS` records; the group and its other members are kept, nothing pruned.
    Cost of the withdrawals, measured: 0 on every key.
  - Withdrawn, re-homes reverted: `??_GMeterEffect` (stores `CSampleXAPOBase<HeadsetXferEffect>`'s vtable),
    `??_GPose`/`??_GUIListMesh` (callees are `??1UIListMesh`/`??1UIListSlot`: shifted neighbours),
    `??_GUsbMidiGuitar` (calls the CriticalSection ctor + `atexit`: a dynamic initializer), `??_GWavFileCacheHelper`.
- `??0PresetOverride` @0x824cb940 withdrawn: the alias group at 0x824cb7f0 proves it an ICF fold (L1_T1, W16-CU).
- Fork U refuted two own-unit caller-bound template names: 0x8234C898 is `vector<BandPatchMesh::MeshPair>`'s
  `_M_fill_insert_aux`, and 0x827CDAE8 is `list<NetLoaderRef>::_M_create_node`. The rebase met main's W17-BPM
  names for 0x8234C898 and 0x823462A8 (`__adjust_heap<MeshVert*>`, where I had named `<ObjEntry*>`); main's
  names are kept.
- Fork R: 0x827815E8 is `TrimExcess<VocalNote>` (template), not `VocalNoteList::Finalize`; withdrawn.
- HV-refused rows re-examined: `EventTracker` findEarliest/findLatest were over-carve heads (pass F), not
  wrong names; the rest of HV's 10 refusals stand.

## 4. Body repair (four forks, disjoint files, each merged `--no-ff`)

HS's rules: full `ninja-locked` builds, whole-report snapshot diff after each edit, keep only if nothing dropped
(exceptions recorded), about three ideas per row, DC3-era code behind `HX_NATIVE` rather than deleted.

| fork | files | result | snapshot Δ |
|---|---|---|---|
| U | ui/UIList*, utl/Locale, NetCacheMgr, synth/Synth, midi/MidiParserMgr, obj/Dir, DirLoader | 14 of 17 to 100 (rb3's 6-arg `UIListDir::DrawWidgets`, file-static rev pairs, `PropSyncSubDirs`/`SyncSubDir` forceinline) | +18 / +3,508 B |
| X | Mic, ExternalMic, MidiInstrument, FixedSizeSaveable(+Stream), Joypad, PlatformMgr_Xbox, User(Mgr), StorePanel, Rot, BeatMatchController | 40 written, 36 at 100 | +38 / +3,236 B |
| W | world/CameraShot(.h), CameraManager, SpotlightDrawer, Dir, Crowd, obj/PropSync | 8 to 100, 3 improved; CamShot layout fix (member at 0xa8 is `vector<RndDrawable*>`) | +8 / +2,456 B |
| R | os/System, Timer.h, Memcard_Xbox, MemMgr, obj/Utl, JoypadController, SongData, DrumMap | 15 up, 8 to 100 (PlatformSymbol, System init/terminate chain, MergeObjectsRecurse, NextName) | +9 / +1,800 B |

Fix patterns: rb3-Wii shape where DC3 drifted (UIListDir revs, PlatformSymbol 6-entry table, MergeObjectsRecurse
ring replace, CamShot::GetCam); DC3-era code absent from retail moved behind `HX_NATIVE` (Locale Magnu override,
Synth `Sound` branch, SystemInit DataPointMgr/WebSvc, ParseText try/catch); TU5 code from retail asm (IsCymbal R1
branch, SongData::PostLoad drum-mix loop); out-of-line/inline policy (DrumMap ctor out of line, SyncSubDir and
GetPropSize forceinline). Behaviour-relevant: `SetSystemArgs` had no space split in our tokenizer.
**Flag for review:** `SpotDrawParams::Load` reads the obsolete rev<4 `Key<float>` pair through a
`reinterpret_cast` into `float[2]` (retail reads uninitialised locals); behaviour is identical, the spelling is not
plausible original source.

After the rebase, FxSendSynapse.cpp held two copies of five setters (main's W16-HX4 and fork X both wrote them);
main's block was kept.

## 5. Gates (rebased tip, full build)

- `tools/map_name_injectivity.py`: `OK: 31262 applied rows, 31261 distinct names, injective (+1 enumerated
  internal-linkage exception(s))`.
- `tools/icf_alias_finder.py --validate`: `VALIDATE: PASS -- 1484 map-consistent, 261 tolerated (enumerated above),
  0 contradicted, 1746 total`.
- `tools/native_build_gate.sh`: run last; the result line is in the lane report.

## 6. Left, and where

- **Pin problems with a known name, no base definition:** Playback (5 rows, BeatMatcher's pin), IntPacker (4,
  BinStream's), `ParseMBT` (MeasureMap's), `SymToTrackType` (MasterAudio's), UIResource (UIListWidget's).
- **Probable separate TU:** `MidiInstrumentMgr` — fork X put its four functions in MidiInstrument.cpp where retail
  pins them, but retail calls the 8-byte `KillAllVoices` out of line (`UnloadInstrument` 57.5), so retail likely has
  a MidiInstrumentMgr.cpp.
- **Map questions on rows named before this lane (not touched):** 0x827145F0 is `MidiInstrument::KillAllVoices`,
  mapped as `ATanInterpolator::Reset` (BinDiff transfer `aa86fb412`); `JoypadController::OnMsg(ButtonDown/Up)`
  return `bool` in retail; `PlatformMgr::PreInit`'s site names `DisableXMP`; `GameGemDB::Finalize`'s names
  `MergeChordGems`; 0x82745C50 carries `??1CShareableTransducerNetwork` but `??_GWavMgr` calls it.
- **Needs a header named on main:** `__adjust_heap<ObjEntry*,ObjSort>` comparator (`obj/MessageTimer.h`);
  `MergeObject` (inlined refs loop needs `ObjRef::next`, Object.h is a PCH input).
- **Fold-survivor callee names only:** `Set3DCharList` 99.91, `RandomizeCategory` 99.95, `FillChannelList` 99.8,
  `SubmixCollection` ctor 99.8, SystemInit/SystemTerminate/InitSystem residues.
- **Refused identifications:** `PollFrontLoader` (retail 84 B vs our 420 B), `MsgSinks::AddPropertySink`,
  `HamNavList::Copy`, `TokenRedemptionPanel::Poll`, the 7 identity-contradicted body-rank rows (~MoggClipMap,
  ??2UIComponent, ??4GameGem, ??_GEventAnim, ??_GFlangerEffect, Achievements::Terminate, FixedString::find_last_of),
  System-range ties `fn_82510060`/`fn_825107D8`.
- **Open TU-boundary question:** retail 0x8250FDF8–0x8251142C is System.cpp content in DC3/rb3-Wii order. The
  07-19 reunification hosts part of it in Debug.cpp; this lane followed HV's split-move precedent. Whether retail
  had one TU or two there is not settled by placement.
- Still anonymous: 979-class featureless rows (~110 KB) with no caller, feature, or neighbour-body signal.

## 7. Rows changed

Every row below was an anonymous `fn_` at fuzzy 0 at lane start. "after" is the rebased branch build (= leg B).
Pass letters as in §2.

| after | rows |
|---|---:|
| 100 | 123 |
| 99–100 | 18 |
| 95–99 | 9 |
| 90–95 | 4 |
| < 90 | 21 |

| retail row | size | name | pass | unit before → after | after |
|---|---:|---|---|---|---:|
| `0x82574e20` | 1676 | `?Init@MetaPanel@@SAXXZ` | D | Flow → MetaPanel | 91.30 |
| `0x82b67288` | 1300 | `?sampleProcessThread@ExternalMic@@QAAKXZ` | H | ExternalMic | 99.84 |
| `0x82579f50` | 1220 | `?ContentDone@BandSongMgr@@UAAXXZ` | G | SongMgr → BandSongMgr | 98.67 |
| `0x827778c8` | 916 | `?AddTrack@SongData@@UAAXHUAudioTrackNum@@VSymbol@@W4SongInfoAudioType@@W4TrackType@@_N@Z` | D | UIList → SongData | 99.28 |
| `0x82752668` | 804 | `?PropSyncSubDirs@@YA_NAAV?$vector@V?$ObjDirPtr@VObjectDir@@@@V?$StlNodeAlloc@V?$ObjDi...` | A | system/obj/Dir | **100** |
| `0x824b29b8` | 752 | `?Replace@LightPreset@@UAAXPAVObjRef@@PAVObject@Hmx@@@Z` | B | LightPreset | 62.28 |
| `0x827e4098` | 704 | `??0TrackWidget@@QAA@XZ` | D | MidiParser → TrackWidget | 96.65 |
| `0x82510bb8` | 652 | `?PreInitSystem@@YAXPBD@Z` | E | Debug → System | 99.92 |
| `0x8276a438` | 604 | `?PropSync@@YA_NAAVBox@@AAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | B | PropSync | **100** |
| `0x824ba6f0` | 600 | `?ShotMatches@CameraManager@@AAA_NPAVCamShot@@ABV?$vector@UPropertyFilter@CameraManage...` | B | CameraManager | **100** |
| `0x827586a0` | 552 | `?MergeObjectsRecurse@@YAXPAVObjectDir@@0AAVMergeFilter@@_N@Z` | C | DirLoader → system/obj/Utl | **100** |
| `0x8279b498` | 496 | `?GetVirtualSlot@JoypadController@@UBAHH@Z` | D | TrackWatcherImpl → JoypadController | 60.27 |
| `0x82510ec8` | 492 | `?SystemPreInit@@YAXPBD@Z` | C | Debug → System | 99.07 |
| `0x82759748` | 476 | `?ListProperties@@YAXAAV?$list@VSymbol@@V?$StlNodeAlloc@VSymbol@@@stlpmtx_std@@@stlpmt...` | C | DirLoader → system/obj/Utl | **100** |
| `0x824e57e8` | 460 | `?Set3DCharList@WorldCrowd@@QAAXABV?$vector@U?$pair@HH@stlpmtx_std@@V?$StlNodeAlloc@U?...` | B | Crowd | 99.91 |
| `0x82b60cb0` | 460 | `?AddData@MicXbox@@QAAXPAXH@Z` | H | Mic | 99.35 |
| `0x827e5668` | 432 | `?PushInstance@CharWidgetImp@@UAAXAAVTextInstance@@@Z` | D | MidiParser → TrackWidgetImp | **100** |
| `0x824bc418` | 428 | `?RandomizeCategory@CameraManager@@AAAXAAV?$ObjPtrList@VCamShot@@VObjectDir@@@@@Z` | B | CameraManager | 99.95 |
| `0x824b14e8` | 404 | `?_M_insert_overflow_aux@?$vector@UEnvironmentEntry@LightPreset@@V?$StlNodeAlloc@UEnvi...` | B | LightPreset | **100** |
| `0x827e5f60` | 404 | `??0CharWidgetImp@@QAA@PAVRndFont@@PAVRndText@@HHW4Alignment@2@VColor32@Hmx@@3_N@Z` | D | MidiParser → TrackWidgetImp | 0.00 |
| `0x824b0538` | 400 | `?Save@Keyframe@LightPreset@@QBAXAAVBinStream@@@Z` | B | LightPreset | 42.95 |
| `0x827e5a38` | 396 | `?RemoveInstances@CharWidgetImp@@UAAXAAV?$list@VTextInstance@@V?$StlNodeAlloc@VTextIns...` | G | MidiParser → TrackWidgetImp | 95.84 |
| `0x824bfbf0` | 388 | `?DoHide@CamShot@@IAAXXZ` | B | CameraShot | **100** |
| `0x824d63d0` | 376 | `?Load@SpotDrawParams@@QAAXAAVBinStream@@H@Z` | B | SpotlightDrawer | **100** |
| `0x82777748` | 376 | `?PostLoad@SongData@@QAAXPAVPlayerTrackConfigList@@@Z` | C | UIList → SongData | 99.95 |
| `0x8279c290` | 372 | `?Handle@JoypadController@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | D | GuitarController → JoypadController | 97.59 |
| `0x82793cd8` | 364 | `??0Submix@@QAA@PAVDataArray@@@Z` | D | system/beatmatch/SlotChannelMapping → system/beatmatch/Submix | **100** |
| `0x827bc2d0` | 344 | `?AddHeap@@YAXHHPAVDataArray@@@Z` | G | MemHeap → MemMgr | 81.10 |
| `0x826ab1c0` | 340 | `?_M_insert_overflow_aux@?$vector@VGameGem@@V?$StlNodeAlloc@VGameGem@@@stlpmtx_std@@@s...` | B | system/beatmatch/GameGemList | **100** |
| `0x82757e68` | 336 | `?NextName@@YAPBDPBDPAVObjectDir@@@Z` | C | DirLoader → system/obj/Utl | **100** |
| `0x82776018` | 336 | `?_M_insert_overflow_aux@?$vector@V?$vector@V?$RangedData@I@?$RangedDataCollection@I@@...` | A | SongData | **100** |
| `0x825108b8` | 324 | `?LanguageInit@@YAXXZ` | E | Debug → System | **100** |
| `0x82773500` | 324 | `?_M_insert_overflow_aux@?$vector@V?$TickedInfo@VString@@@@V?$StlNodeAlloc@V?$TickedIn...` | B | SongData | **100** |
| `0x825112e8` | 320 | `?SystemInit@@YAXPBD@Z` | E | Debug → System | 99.88 |
| `0x8280ce60` | 320 | `?PostLoad@UIListDir@@UAAXAAVBinStream@@@Z` | B | UIListDir | **100** |
| `0x828210c8` | 316 | `?Load@UIListWidget@@UAAXAAVBinStream@@@Z` | B | UIListWidget | **100** |
| `0x82510590` | 304 | `?SetSystemLanguage@@YAXVSymbol@@_N@Z` | E | System | **100** |
| `0x824cc038` | 292 | `?Sync@BitmapOverride@WorldDir@@QAAX_N@Z` | B | MidiSynth | **100** |
| `0x82751510` | 292 | `?PostLoadInlined@ObjectDir@@IAA?AV?$ObjDirPtr@VObjectDir@@@@XZ` | A | system/obj/Dir | **100** |
| `0x827e5c50` | 292 | `?Poll@CharWidgetImp@@UAAXXZ` | G | MidiParser → TrackWidgetImp | **100** |
| `0x827bffa0` | 288 | `??$__median@PBDUAlpha@@@stlpmtx_std@@YAABQBDABQBD00UAlpha@@@Z` | F | Symbol | **100** |
| `0x82510270` | 280 | `?SystemPoll@@YAX_N@Z` | E | Debug → System | 98.50 |
| `0x824aaba8` | 264 | `?WorldInit@@YAXXZ` | D | LightPreset → system/world/World | **100** |
| `0x8279b390` | 264 | `?IsCymbal@JoypadController@@QBA_NH@Z` | C | TrackWatcherImpl → JoypadController | 99.85 |
| `0x827584f8` | 260 | `?IsPropPathValid@@YA_NPAVObject@Hmx@@PAVDataArray@@@Z` | B | DirLoader | **100** |
| `0x82b6c8f0` | 252 | `?SetType@SynthSample360@@UAAXVSymbol@@@Z` | D | system/synth/SynthSample → system/synth_xbox/SynthSample | **100** |
| `0x824b5010` | 248 | `?Load@Keyframe@LightPreset@@QAAXAAVBinStream@@@Z` | B | LightPreset | 79.79 |
| `0x827f89c8` | 248 | `?CalcBoundingBox@UIList@@QAAXAAVBox@@@Z` | B | UIList | **100** |
| `0x827e6190` | 244 | `?AddTextInstance@CharWidgetImp@@UAAHVTransform@@VString@@_N@Z` | D | MidiParser → TrackWidgetImp | 93.92 |
| `0x825110e0` | 240 | `?SetSystemArgs@@YAXPBD@Z` | C | Debug → System | 94.75 |
| `0x827c07b8` | 236 | `??$__unguarded_partition@PAPBDPBDUAlpha@@@stlpmtx_std@@YAPAPBDPAPBD0PBDUAlpha@@@Z` | F | Symbol | **100** |
| `0x82510a08` | 228 | `?InitSystem@@YAXPBD@Z` | E | Debug → System | 99.89 |
| `0x824d57e8` | 224 | `??0SpotDrawParams@@QAA@PAVSpotlightDrawer@@@Z` | B | SpotlightDrawer | **100** |
| `0x8279be58` | 224 | `?GetVelocityBucket@JoypadController@@UBAHH@Z` | A | JoypadController | **100** |
| `0x82803d38` | 224 | `??$__lower_bound@U?$_List_iterator@PAVUIResource@@U?$_Nonconst_traits@PAVUIResource@@...` | A | UI | **100** |
| `0x82803e18` | 224 | `??$__upper_bound@U?$_List_iterator@PAVUIResource@@U?$_Nonconst_traits@PAVUIResource@@...` | A | UI | **100** |
| `0x82511208` | 220 | `?SystemTerminate@@YAXXZ` | E | Debug → System | 99.27 |
| `0x82523578` | 216 | `?GetRemoteUsers@UserMgr@@QBAXAAV?$vector@PAVRemoteUser@@V?$StlNodeAlloc@PAVRemoteUser...` | H | UserMgr | **100** |
| `0x827e5438` | 216 | `??1MultiMeshWidgetImp@@UAA@XZ` | G | MidiParser → TrackWidgetImp | **100** |
| `0x82516550` | 200 | `FileGetPath` | F | File | **100** |
| `0x827a2c28` | 200 | `?SaveFixedString@FixedSizeSaveable@@SAXAAVFixedSizeSaveableStream@@ABVString@@@Z` | H | FixedSizeSaveable | **100** |
| `0x82793ea0` | 196 | `??0SubmixCollection@@QAA@PAVDataArray@@@Z` | C | system/beatmatch/SlotChannelMapping → system/beatmatch/Submix | 99.80 |
| `0x827583e0` | 192 | `?MergeObject@@YAXPAVObject@Hmx@@0PAVObjectDir@@W4Action@MergeFilter@@@Z` | C | DirLoader → system/obj/Utl | 71.21 |
| `0x827f27a8` | 188 | `?InqMinMaxFromWidthAndHeight@UILabel@@QAAHMMW4Alignment@RndText@@AAVVector3@@1@Z` | B | UILabel | **100** |
| `0x82524c20` | 180 | `?UserHas22FretGuitar@@YA_NPAVLocalUser@@@Z` | H | Joypad | **100** |
| `0x827f8140` | 176 | `?DrawShowing@UIList@@UAAXXZ` | B | UIList | **100** |
| `0x8250fdf8` | 172 | `?PlatformSymbol@@YA?AVSymbol@@W4Platform@@@Z` | C | Debug → System | **100** |
| `0x8279b2e8` | 168 | `?GetWhammyBar@JoypadController@@UBAMXZ` | G | TrackWatcherImpl → JoypadController | 83.19 |
| `0x827178f0` | 160 | `?findEarliest@EventTracker@@QAAHMH@Z` | F | system/synth/FxSend → system/synth/VoiceBeat | 81.50 |
| `0x827cc6c8` | 160 | `?DecodeUTF8@@YAIAAGPBD@Z` | G | BufStream → UTF8 | 89.38 |
| `0x824bdae0` | 156 | `?GetCam@CamShot@@QAAPAVRndCam@@XZ` | B | CameraShot | **100** |
| `0x82781548` | 156 | `?CapLastFreestyleSection@VocalNoteList@@QAAXM@Z` | F | DrivenPropertyEntry → VocalNoteList | 97.38 |
| `0x824ee5f0` | 148 | `?MakeVertical@@YAXAAVMatrix3@Hmx@@@Z` | H | Rot | **100** |
| `0x82717990` | 148 | `?findLatest@EventTracker@@QAAHMH@Z` | F | system/synth/FxSend → system/synth/VoiceBeat | 95.68 |
| `0x827a2ed8` | 148 | `?LoadStd@FixedSizeSaveable@@SAXAAVFixedSizeSaveableStream@@AAV?$list@VSymbol@@V?$StlN...` | H | FixedSizeSaveable | **100** |
| `0x8280ddb8` | 148 | `?PreLoad@UIListDir@@UAAXAAVBinStream@@@Z` | B | UIListDir | **100** |
| `0x8278c3a8` | 144 | `??0DrumMap@@QAA@XZ` | I | system/beatmatch/DrumMap | **100** |
| `0x827dc9e8` | 144 | `??0HttpGet@@QAA@IGPBD0@Z` | G | JsonUtils → HttpGet | 87.25 |
| `0x82b606e0` | 144 | `?Shutdown@MicManagerXbox@@QAAXXZ` | H | Mic | **100** |
| `0x827900c8` | 136 | `?SlotToButton@BeatMatchController@@UBAHH@Z` | H | BeatMatchController | **100** |
| `0x827a3078` | 136 | `?SaveStd@FixedSizeSaveable@@SAXAAVFixedSizeSaveableStream@@ABV?$list@VSymbol@@V?$StlN...` | H | FixedSizeSaveable | **100** |
| `0x827cdca0` | 136 | `?EnterUnloadState@NetCacheMgr@@AAAXXZ` | G | DataPointMgr → NetCacheMgr | 78.76 |
| `0x827f9100` | 136 | `?SetProvider@UIList@@QAAXPAVUIListProvider@@@Z` | B | UIList | **100** |
| `0x8251cb20` | 132 | `?GetOwnerOfGuest@PlatformMgr@@QAAHH@Z` | H | PlatformMgr_Xbox | **100** |
| `0x825c20f0` | 132 | `??0NewRemoteMachineMsg@@QAA@PAVRemoteBandMachine@@@Z` | G | Memcard → BandMachineMgr | **100** |
| `0x827cd830` | 132 | `?IsLoadedOrFailed@NetLoaderRef@@QAA_NXZ` | B | DataPointMgr | **100** |
| `0x827e55e0` | 128 | `?Clear@MultiMeshWidgetImp@@UAAXXZ` | G | MidiParser → TrackWidgetImp | **100** |
| `0x827e5568` | 120 | `?PushInstance@MultiMeshWidgetImp@@UAAXAAUInstance@RndMultiMesh@@@Z` | G | MidiParser → TrackWidgetImp | **100** |
| `0x827a2a50` | 116 | `?LoadFixedString@FixedSizeSaveable@@SAXAAVFixedSizeSaveableStream@@AAVString@@@Z` | H | FixedSizeSaveable | **100** |
| `0x827c96d8` | 116 | `?Localize@Locale@@QBAPBDVSymbol@@_N@Z` | B | StringTable | **100** |
| `0x824aa768` | 112 | `?NewObject@WorldCrowd@@SAPAVObject@Hmx@@XZ` | D | LightPreset → system/world/World | **100** |
| `0x824aa808` | 112 | `?NewObject@LightPreset@@SAPAVObject@Hmx@@XZ` | D | LightPreset → system/world/World | **100** |
| `0x824aa930` | 112 | `?NewObject@WorldReflection@@SAPAVObject@Hmx@@XZ` | D | LightPreset → system/world/World | **100** |
| `0x824aa9d0` | 112 | `?NewObject@SpotlightEnder@@SAPAVObject@Hmx@@XZ` | D | LightPreset → system/world/World | **100** |
| `0x824aab10` | 112 | `?NewObject@EventAnim@@SAPAVObject@Hmx@@XZ` | G | LightPreset → system/world/World | 86.93 |
| `0x824b9e68` | 112 | `?GetFreeCam@CameraManager@@QAAPAVFreeCamera@@H@Z` | C | LightPresetManager → CameraManager | **100** |
| `0x82523768` | 112 | `?UpdateOnlineID@LocalUser@@QAAXXZ` | H | User | **100** |
| `0x826fe780` | 112 | `?PauseAllSfx@Synth@@QAAX_N@Z` | B | CheatProvider | **100** |
| `0x827e62e0` | 112 | `??1CharWidgetImp@@UAA@XZ` | G | MidiParser → TrackWidgetImp | 88.89 |
| `0x827f2710` | 112 | `?NewObject@UILabelDir@@SAPAVObject@Hmx@@XZ` | B | UILabel | **100** |
| `0x82713608` | 108 | `?GlideToNote@NoteVoiceInst@@QAAXEH@Z` | H | MidiInstrument | **100** |
| `0x82524cd8` | 104 | `?UserHasButtonGuitar@@YA_NPAVLocalUser@@@Z` | H | Joypad | **100** |
| `0x8271cbd8` | 104 | `??4MoggClipMap@@QAAAAV0@ABV0@@Z` | G | Sfx → MoggClipMap | 88.08 |
| `0x827eb4d8` | 104 | `?ParseText@MidiParserMgr@@AAAPAVDataArray@@PBDH@Z` | B | MidiParserMgr | **100** |
| `0x824ce060` | 100 | `?insert@?$list@UPresetOverride@WorldDir@@V?$StlNodeAlloc@UPresetOverride@WorldDir@@@s...` | B | PropSync | **100** |
| `0x824ce0c8` | 100 | `?insert@?$list@UBitmapOverride@WorldDir@@V?$StlNodeAlloc@UBitmapOverride@WorldDir@@@s...` | B | PropSync | 99.80 |
| `0x825236f0` | 100 | `?SetUserGuid@User@@QAAXABVUserGuid@@@Z` | H | User | **100** |
| `0x826ab020` | 100 | `?_M_erase@?$vector@VGameGem@@V?$StlNodeAlloc@VGameGem@@@stlpmtx_std@@@stlpmtx_std@@IA...` | B | system/beatmatch/GameGemList | **100** |
| `0x827162c0` | 100 | `??0MidiInstrumentMgr@@QAA@XZ` | H | MidiInstrument | **100** |
| `0x8271bd90` | 100 | `??$_M_allocate_and_copy@PBVSfxMap@@@?$vector@VSfxMap@@V?$StlNodeAlloc@VSfxMap@@@stlpm...` | B | Sfx | **100** |
| `0x82b5e8a8` | 100 | `?SetNoiseGate@@YA?AVDataNode@@PAVDataArray@@@Z` | H | Mic | **100** |
| `0x824b0bc8` | 96 | `??$__uninitialized_fill_n@PAUSpotlightEntry@LightPreset@@IU12@@stlpmtx_std@@YAPAUSpot...` | B | LightPreset | **100** |
| `0x8251c7d0` | 96 | `?CanSeeUserCreatedContent@PlatformMgr@@QBA_NPBVOnlineID@@@Z` | H | PlatformMgr_Xbox | **100** |
| `0x8271b9a0` | 96 | `??$__uninitialized_copy@PBVSfxMap@@PAV1@@stlpmtx_std@@YAPAVSfxMap@@PBV1@0PAV1@ABU__fa...` | B | Sfx | **100** |
| `0x82774d50` | 96 | `??$__uninitialized_fill_n@PAV?$vector@V?$RangedData@U?$pair@HH@stlpmtx_std@@@?$Ranged...` | A | SongData | **100** |
| `0x82774df0` | 96 | `??$__uninitialized_fill_n@PAV?$vector@V?$RangedData@VRGRollChord@@@?$RangedDataCollec...` | A | SongData | **100** |
| `0x827937d0` | 96 | `?FillChannelList@Submix@@QBAXAAV?$list@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@H@Z` | C | BeatMatcher → system/beatmatch/Submix | 99.79 |
| `0x82b5e9c0` | 96 | `?SetRemoteGain@@YA?AVDataNode@@PAVDataArray@@@Z` | H | Mic | **100** |
| `0x82c2c8b0` | 96 | `ogg_page_granulepos` | G | block → framing | **100** |
| `0x824cb860` | 92 | `??0MatOverride@WorldDir@@QAA@PAVObject@Hmx@@@Z` | B | PropSync | 54.96 |
| `0x824e42f0` | 92 | `??0CharData@WorldCrowd@@QAA@PAVObject@Hmx@@@Z` | B | Crowd | 80.13 |
| `0x827131d8` | 92 | `?SetFineTune@NoteVoiceInst@@QAAXM@Z` | H | MidiInstrument | **100** |
| `0x82795d00` | 92 | `?GetGemInProgressWithSlot@TrackWatcherImpl@@QAAPAUGemInProgress@@H@Z` | F | TrackWatcherImpl | 72.39 |
| `0x824ed408` | 88 | `??_GFreeCamera@@UAAPAXI@Z` | G | Instance → FreeCamera | **100** |
| `0x8251c778` | 88 | `?HasOnlinePrivilege@PlatformMgr@@QBA_NH@Z` | H | PlatformMgr_Xbox | **100** |
| `0x82716248` | 88 | `?UnloadInstrument@MidiInstrumentMgr@@QAAXXZ` | H | MidiInstrument | 57.50 |
| `0x82729f48` | 88 | `?NearestSustainRate@Ps2ADSR@@QBAHM@Z` | F | ADSR | 94.55 |
| `0x824aa6e0` | 84 | `?NewObject@ColorPalette@@SAPAVObject@Hmx@@XZ` | D | LightPreset → system/world/World | **100** |
| `0x82524de0` | 84 | `?JoypadStageKitSetRaw@@YAXHH@Z` | B | Joypad | **100** |
| `0x82790178` | 84 | `?GemNumSlots@@YAHH@Z` | G | BeatMatchController → system/beatmatch/BeatMatchUtl | 97.86 |
| `0x827bdd60` | 84 | `?StrNCopy@@YA_NPADPBDH@Z` | F | Str | **100** |
| `0x827f8368` | 84 | `?NewObject@UIListArrow@@SAPAVObject@Hmx@@XZ` | B | UIList | **100** |
| `0x827f83f0` | 84 | `?NewObject@UIListCustom@@SAPAVObject@Hmx@@XZ` | B | UIList | **100** |
| `0x827f85a0` | 84 | `?NewObject@UIListLabel@@SAPAVObject@Hmx@@XZ` | B | UIList | **100** |
| `0x827f8628` | 84 | `?NewObject@UIListSubList@@SAPAVObject@Hmx@@XZ` | B | UIList | **100** |
| `0x827f8be8` | 84 | `?NewObject@UIListSlot@@SAPAVObject@Hmx@@XZ` | B | UIList | **100** |
| `0x82b5e910` | 84 | `?SetLowCut@@YA?AVDataNode@@PAVDataArray@@@Z` | H | Mic | **100** |
| `0x82b5e968` | 84 | `?SetLocalGain@@YA?AVDataNode@@PAVDataArray@@@Z` | H | Mic | **100** |
| `0x825107d8` | 80 | `?OnSystemExec@@YA?AVDataNode@@PAVDataArray@@@Z` | G | Debug → System | 67.90 |
| `0x82793780` | 80 | `?Find@SubmixCollection@@QAAPAVSubmix@@VSymbol@@@Z` | C | BeatMatcher → system/beatmatch/Submix | **100** |
| `0x827b4cc0` | 80 | `?IsEnumerating@StorePanel@@QBA_NXZ` | H | StorePanel | **100** |
| `0x82359fc8` | 76 | `??_GQuestManager@@UAAPAXI@Z` | G | Object → QuestManager | **100** |
| `0x825cac18` | 76 | `??_GStorePurchaseable@@UAAPAXI@Z` | G | Mic → RockCentral | **100** |
| `0x82712e30` | 76 | `??_GFxSendSynapse@@UAAPAXI@Z` | G | system/synth/FxSendPitchShift → system/synth/FxSendSynapse | 99.74 |
| `0x82745d08` | 76 | `??_GWavMgr@@UAAPAXI@Z` | G | PoolAlloc → WavMgr | 99.74 |
| `0x827f20f0` | 76 | `??_GLabelStyle@UILabel@@QAAPAXI@Z` | G | UIScreen → UILabel | **100** |
| `0x8281e628` | 76 | `??_GUIListArrow@@UAAPAXI@Z` | G | UIListCustom → UIListArrow | 99.74 |
| `0x824d1a38` | 72 | `??$New@VSpotlightDrawer@@@Object@Hmx@@SAPAVSpotlightDrawer@@XZ` | B | SpotlightDrawer_NG | **100** |
| `0x826fcdc8` | 72 | `??$New@VFxSendPitchShift@@@Object@Hmx@@SAPAVFxSendPitchShift@@XZ` | B | system/synth/Synth | **100** |
| `0x82b6cab0` | 72 | `?Init@SynthSample360@@SAXXZ` | G | system/synth/SynthSample → system/synth_xbox/SynthSample | **100** |
| `0x8251f848` | 68 | `??_GContent@@UAAPAXI@Z` | G | ContentMgr → ContentMgr_Xbox | **100** |
| `0x82531448` | 68 | `??_GNetworkSocket@@UAAPAXI@Z` | G | MapFile_Xbox → NetworkSocket_Win | **100** |
| `0x827296c0` | 68 | `??_GMic@@UAAPAXI@Z` | G | ByteGrinder | **100** |
| `0x82790490` | 68 | `??_GTrackWatcherParent@@UAAPAXI@Z` | G | system/beatmatch/BeatMatchUtl → BeatMatcher | **100** |
| `0x827e59e8` | 68 | `?Size@MultiMeshWidgetImp@@UAAHXZ` | G | MidiParser → TrackWidgetImp | **100** |
| `0x8251c208` | 64 | `?IsGuestOnlineID@PlatformMgr@@QBA_NPBVOnlineID@@@Z` | H | PlatformMgr_Xbox | **100** |
| `0x827921d8` | 64 | `?NextFillExtents@FillInfo@@QBA_NHAAUFillExtent@@@Z` | F | system/beatmatch/FillInfo | **100** |
| `0x824adbd0` | 60 | `??$_Param_Construct@USpotlightEntry@LightPreset@@U12@@stlpmtx_std@@YAXPAUSpotlightEnt...` | B | LightPreset | **100** |
| `0x827ab288` | 56 | `?ReadFloat@FixedSizeSaveableStream@@QAAMXZ` | H | FixedSizeSaveableStream | **100** |
| `0x827ab250` | 52 | `?ReadInt@FixedSizeSaveableStream@@QAAHXZ` | H | FixedSizeSaveableStream | **100** |
| `0x82802808` | 44 | `?FocusComponent@UIManager@@QAAPAVUIComponent@@XZ` | F | UI | **100** |
| `0x8274a978` | 36 | `?LiteralFloat@DataNode@@QBAMPBVDataArray@@@Z` | C | Task → DataNode | 77.78 |
| `0x8250fec8` | 32 | `?PlatformLittleEndian@@YA_NW4Platform@@@Z` | C | Debug → System | **100** |
| `0x8251cba8` | 32 | `?SetNotifyUILocation@PlatformMgr@@QAAXW4NotifyLocation@@@Z` | H | PlatformMgr_Xbox | **100** |
| `0x82712270` | 20 | `?SetAttackSmoothing@FxSendSynapse@@QAAXM@Z` | H | system/synth/FxSendSynapse | **100** |
| `0x82712288` | 20 | `?SetReleaseSmoothing@FxSendSynapse@@QAAXM@Z` | H | system/synth/FxSendSynapse | **100** |
| `0x827122a0` | 20 | `?SetAmount@FxSendSynapse@@QAAXM@Z` | H | system/synth/FxSendSynapse | **100** |
| `0x827122b8` | 20 | `?SetProximityEffect@FxSendSynapse@@QAAXM@Z` | H | system/synth/FxSendSynapse | **100** |
| `0x827122d0` | 20 | `?SetProximityFocus@FxSendSynapse@@QAAXM@Z` | H | system/synth/FxSendSynapse | **100** |
| `0x827162a0` | 20 | `?Poll@MidiInstrumentMgr@@QAAXXZ` | H | MidiInstrument | **100** |
| `0x82793768` | 20 | `?GetNumSlots@Submix@@QBAHXZ` | C | BeatMatcher → system/beatmatch/Submix | **100** |
| `0x827b4d10` | 16 | `?InCheckout@StorePanel@@QBA_NXZ` | H | StorePanel | **100** |
| `0x82713410` | 8 | `?SetFineTune@MidiInstrument@@QAAXM@Z` | H | MidiInstrument | **100** |
| `0x827147e0` | 8 | `?PlayNote@MidiInstrument@@QAAXEEH@Z` | H | MidiInstrument | **100** |
| `0x82716240` | 8 | `?SetInstrument@MidiInstrumentMgr@@QAAXPAVMidiInstrument@@@Z` | H | MidiInstrument | 97.50 |

## 8. Other rows that moved (leg A → leg B, not in §7)

Up (21): the DrumMap funclet `fn_8278C438` 0 → 100; 13 funclets to 100 in System, Mic, obj/Dir, DirLoader,
UIListDir, UIListWidget, MidiInstrument, Crowd; `fn_82B613DC` (Mic) 0 → 93.4; `fn_827588C8` 93.4 → 99.5,
`fn_82759944` 93.9 → 99.8, `fn_824D58F4` 99.4 → 99.5; `CamShot::Copy` 95.82 → 97.60, `CamShot::Load` 96.43 → 96.97,
`??0CamShot` 86.99 → 87.43 (fork W's layout fix).

Down (9, none from 100):
- `??1ObjRef` (MatAnim) 25 → 0, 16 B: the old `BitmapOverride::Sync` held MatAnim.obj's only instance.
- Funclets re-paired: `fn_827CA010` (Locale) 99.3 → 93.4, `fn_825AD6D4` 99.3 → 93.9 and `fn_825AD65C` 99.4 → 99.3
  (CameraManager).
- Callee-name/fold exposure on rows named before this lane, now calling newly named functions:
  `BandSongMgr::AddSongData` 66.08 → 65.95, `~Queue` (UsbMidiGuitar) 34.27 → 34.16, `vector<LocalePanel::Entry>::
  _M_insert_overflow_aux` 99.78 → 99.72, `TrackWidget::SyncImp` 97.69 → 97.64, `GemPlayer::SetPitchShiftRatio`
  99.96 → 99.91.

## 9. Reproduce (`~/tmp/w16hz/`, not committed)

```
python3 sem.py && python3 union.py && python3 props.py 16            # pass A (overlap)
python3 tools/anon_proposal_adjudicate.py props.json --independent --json-out adj_ind.json && python3 decide.py adj_ind.json dec_ind.json
python3 callers.py && python3 props_call.py && python3 decide_call.py adj_call.json props_call.json acc_call.json oursize_call.json
python3 defs.py      # multi-definition index -> other2.json -> props_call2.py (pass B)
python3 rh/plan*.py && python3 rh/runnerup.py && python3 rh/mkprops.py && python3 rh/adj_rehome.py props.json adj.json   # C, D, E, F, G
python3 sem_nb.py    # pass D proposals;   python3 bodynb.py   # pass G matrix
python3 rh/apply_moves.py config/45410914/splits.txt <moves.json> config/45410914/symbols.txt
python3 tools/gated_map_write.py --target scripts/target_symbol_map.json --rows-json <rows.json>
python3 rows_table.py report_base.json report_final.json rows_final.json X_map_rows.json
python3 tools/ab_measure.py --worktree <main + lane symbols.txt hunk> --patch <main..w16-hz minus symbols.txt>
```
