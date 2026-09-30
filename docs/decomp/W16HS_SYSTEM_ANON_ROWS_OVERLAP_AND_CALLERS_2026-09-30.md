# W16-HS — more anonymous src/system rows named (callee/string overlap, then caller binding), repaired, and the ~VarStack fold proven (2026-09-30)

Continues W16-HN (`W16HN_SYSTEM_ANON_ROWS_NAMED_AND_REPAIRED_2026-09-30.md`) on the anonymous
fuzzy-0 rows left in compiled `src/system/` units outside `bandobj/`, `rndobj/`, `char/` and
`rnddx9/`. HN cut `hamobj/`/`gesture/` foreign pins (except MeterDisplay and
MiniLeaderboardDisplay); so does this lane. `obj/` template instantiations were left to the
other session. Branch `w16-hs`, rebased onto main `77d7317db`.

**Result.** 178 rows (37,340 B) now carry names; 89 of them (16,572 B) read fuzzy 100. One
whole-binary A/B over the branch diff: **+113 fns / +93 honest / +17,448 B (+0.170268 pp) /
fuzzy +0.297047 pp**, exactly the pre-registered prediction (§5). 61 units improved, 0
regressed, 4 reached 100 on `mpn`.

## 1. Population

Taken on the settled tree at lane start (main `fdfbdb090`): **1,911 rows / 262,808 B**; after
HN's scope cuts, **1,625 targets / 218,304 B**. After this lane: 1,526 / 193,004 B before the
caller pass, then 86 fewer.

## 2. The ~VarStack fold at 0x82768b18 — PROVEN, alias and name installed

HN withdrew `??1VarStack@@QAA@XZ` here: naming it without an alias charged 279 retail EH funclets
that unwind `pair<const Symbol,DataNode>` through this address (−11,184 B).

- `tools/icf_pair_adjudicate.py --chasetest` and `--self-break` were run first. Every control
  went the right way, and the vacuous decoy went red under `--self-break`.
- `--pairs --chase --family`, `fn_82768B18` against each of our five 24-byte DataNode-at-+4
  destructors: `~VarStack`, `~pair<const Symbol,DataNode>`, `~ScriptTask::Var`,
  `~pair<DataArray*,DataNode>`, `~pair<const char*,DataNode>`.
  - **FLAT T1 PROVEN for all five.** The single relocation `?Release@DataArray@@QAAXXZ` agrees
    by name, not only by shape.
  - CHASED PROVEN.
  - FAMILY: **5 of ours → 1 retail address**. `retail_bodytwins` is 1 across all 3,115 pinned
    target objs.
- 281 retail call sites land on `0x82768B18`; 280 are EH funclets. Every folded spelling is
  referenced by our objs (`~pair<const Symbol,DataNode>` in 22).
- Installed group `w16hs_0x82768b18` (survivor `~VarStack`) and restored the map row.
- **Measured +1 fn / +24 B.** I had predicted +11.2 KB, and that was wrong: on this base the
  address was anonymous, so `name_check` already forgave the funclets. The real check was that
  naming it *with* the alias costs nothing, and none of the 279 funclets moved.

## 3. Identification

**A. Callee/string overlap (W16-HO's matcher, `~/tmp/w16hs/sem.py`).**
- Retail features: fingerprint strings plus map-named callees.
- Candidate features: our base obj's relocation names (static `Symbol`s, string literals,
  callees).
- The pool is every unclaimed function in the unit's base obj. **Template-signature methods
  are in, which HN's pool dropped**; the `Obj*` template families are out.
- A second source: HN's own matrix rows whose top was 40–70 with margin ≥ 5. HN's cut at 70
  had dropped these drifted identities (`Song::Handle` 67, `UILabel::Save` 61,
  `HttpGet::Poll` 60).
- Header inlines defined in more than 2 compiled objs were dropped, which removed 148
  `BinStream::operator<<`/`Rand`/`DataNode`-style candidates.
- That left **245 proposals**. `tools/anon_proposal_adjudicate.py` was run normally and with
  `--independent`; the tallies were identical (48 SUPPORTED / 179 CONTRADICTED / 18 UNANCHORED).
- The accept rule was HN's, written as code (`~/tmp/w16hs/decide.py`) before any row was read.
  Both adjudications gave the same verdict on every row.

| bucket | rows | bytes |
|---|---:|---:|
| accept: MECH (0 body negatives, ≥ 2 positives) | 43 | 9,040 |
| accept: drift-anchored (AGREE ≥ 3, ≥ 2× body negatives, runner-up ≤ half) | 28 | 10,860 |
| accept: caller witness (≥ 1 matched caller names it, runner-up none) | 20 | 3,616 |
| refuse: identity contradicted (caller / bound / mapped elsewhere) | 61 | 7,632 |
| refuse: drift without anchor | 72 | 8,576 |
| refuse: weak | 21 | 1,296 |

**91 rows / 23,516 B named.** In-tree: +12 fns / +1,876 B, 0 rows anywhere off fuzzy 100.
One dtk over-carve merge followed, committed separately (`symbols.txt` only):
`MetaMusic::Loaded` 0x30 → 0x38. It is a leaf whose `b`/`beq` land in the absorbed
`clrlwi; blr`.

**B. Caller binding (W16-HR's method, coordinator tip; `~/tmp/w16hs/callers.py`).** This covers
small and featureless rows that pass A cannot see. W16-HR measured the unanimous caller vote as
right on 9,187 of 9,188 already-named addresses.

| caller class, over the 1,526 remaining rows | rows | bytes |
|---|---:|---:|
| no caller witness | 1,008 | 118,620 |
| 1 name, defined in another unit's base obj (foreign pin) | 234 | 36,208 |
| **1 name, defined in the row's own unit** | **120** | **18,168** |
| 1 name, no base definition | 100 | 12,052 |
| 1 name, mapped/bound elsewhere | 28 | 3,600 |
| several names (fold soup) | 36 | 4,356 |

The own-unit class was adjudicated with `--independent` and decided by
`~/tmp/w16hr/decide_call.py`, unchanged. It refuses when the name is bound elsewhere, when a
caller contradicts with a non-fold name, on a vtable-class mismatch, or when our size is
outside 1/3–3×. Result: 94 accepted, 17 refused (15 on size), 9 held. Eight obj/ template
names were dropped for the other session, leaving **86 rows / 13,576 B named**. In-tree:
+25 fns / +2,788 B, 0 rows off fuzzy 100.

Three carve heads merged (`symbols.txt` only). Our bodies confirm the merged extents:

| row | old → merged extent | our body |
|---|---|---:|
| `FormatString::UpdateType` | 0x5C → 0xCC | 204 B (exact) |
| `UIManager::CancelTransition` | 0x40 → 0x88 | 136 B (exact) |
| `MicClientMapper::GetMicIDForClientID` | 0x40 → 0x60 | 100 B |

The caller vote settles the HN follow-up at `0x8278e200`: it is **`GameGemList::Finalize`**.
The byte score had proposed `MergeChordGems`, which is refused (caller contradiction).

Gates after every naming step and on the final tip: `tools/icf_alias_finder.py --validate`
**PASS** (1,476 map-consistent, 264 tolerated, **0 contradicted**, 1,741 total);
`tools/map_name_injectivity.py` **OK** (30,745 applied rows, injective).

## 4. Body repair (pass A's 80 sub-100 rows)

The work was split across four forks on disjoint directories (`w16-hs-{A,B,C,D}`, each merged
`--no-ff`; the per-row commits are in the history). The rules were HN's: full `ninja-locked`
builds only; a whole-report snapshot diff after every edit, keeping an edit only if nothing
dropped; about three ideas per row.

| fork | dirs | rows | now at 100 | fork snap Δ (fns / B) |
|---|---|---:|---:|---|
| A | ui | 18 | 14 | +17 / +3,572 |
| B | utl, os, movie, math | 17 | 12 | +24 / +3,224 |
| C | synth, synth_xbox, midi, hamobj | 15 | 13 | +12 / +2,660 |
| D | beatmatch, net, obj (non-template), meta, world | 29 | 20 | +22 / +3,304 |

The fixes by pattern:

| pattern | examples |
|---|---|
| DC3-era code absent from retail, moved behind `HX_NATIVE` | `Song::Handle` MBT handlers, `CheatsManager` key-release handler, Holmes paths (`FileGetStat`, `AsyncFile::New`, `CacheWav`), `XboxSessionJob` `mSuccess`/`OnCompletion` (six rows to 100), `TaskMgr::Poll` deferred delete, `DirLoader::CreateObjects` factory check |
| rb3-Wii shape where DC3 drifted | `UILabelDir::PostLoad`/`Copy`, `UIPicture::PreLoad`, `Screenshot::Load`, `SpotlightEnder::Load`, `MeterDisplay::PreLoad` (file-static rev shorts), `FindResource` via `equal_range`, `GetMatVariationName` via `rfind`/`substr`, `Debug::Exit`, `MidiReceiver::Error` `MILO_WARN` |
| implicit instead of user-declared empty dtors | `~UIProxy`, `~MidiInstrument`, `~EventAnim` |
| TU5 code rebuilt from retail asm | `HDCache::OpenFiles` pending-priority loop, `StandardStream::DoJump` (PROVISIONAL tail replaced), `HttpGet::Poll` state machine, `VoiceBeat::Analyze` (the rb3-Wii text has a sign-flipped voice filter) |
| layout / revision | `SynthSample` is rev 5 in retail and still serializes the loop fields; `SampleInst360` is 0x58; `HttpGet`+0x68 is a bool; `WriteCareerLeaderboardJob` holds two `XUSER_PROPERTY` |
| per-TU `/D` gate | `/DRB3_HANDLE_LOCAL_STATIC` for MetaMusic.cpp (`objects.json`) |

Real behaviour bug fixed: `CacheXbox::ThreadGetFileSize` parked the size only when
`GetFileSize` *failed*.

Two recorded exceptions to "nothing dropped". Both are anonymous 40 B `masked_equal` funclets,
so no matched bytes were involved:

- **Loader `fn_827BF39C` 100 → 99.4.** It was paired with our `FileLoader::AllocBuffer`'s EH
  funclet, but retail AllocBuffer has no EH, so the pairing was already false. The fix nets
  +64 B.
- **DataPointMgr `fn_827CD434` 100 → 99.5.** It re-paired next to the newly named
  `DataPoint::AddPair`.

Fork C also cost one never-at-100 row: `__uninitialized_copy<SampleMarker>` (96 B, 99.79). The
SampleInst ctor fix removed our only instantiation of it (masked_equal −1).

## 5. Measurement

`python3 tools/ab_measure.py --worktree ~/tmp/wt-w16-hs-ab --patch <main+symbols.txt .. w16-hs>`,
**one run**. Leg A is main `77d7317db` plus the lane's two `symbols.txt` over-carve commits
(HK/HR recipe: `ab_measure` refuses a patch that touches `symbols.txt`). The patch covers 63
files: configgen, map and source. Both legs were read at a split fixed point (0 extra re-splits)
on `name_check`. Leg B recompiled 155 TUs. Result:
`~/tmp/w16hs/ab_result.json` (run dir
`~/tmp/wt-w16-hs-ab/.ab_measure_runs/20260930-185330-branch-1222033/`; tree restore verified).

| | leg A | leg B | Δ |
|---|---:|---:|---:|
| matched_functions | 45,689 | 45,802 | **+113** |
| masked_equal | 23,573 | 23,593 | +20 |
| honest | 22,116 | 22,209 | **+93** |
| matched_code_percent | 44.184640 | 44.354908 | **+0.170268 pp (+17,448 B)** |
| fuzzy_match_percent | 53.623077 | 53.920124 | **+0.297047 pp** |
| units at 100 (mpn) | 222 | 226 | +4 (SongMetadata, UIListHighlight, UIListProvider, UITrigger) |

- **Predicted vs measured.** The pre-registered prediction was the sum of the in-tree steps:
  VarStack +1 / +24, overlap names +12 / +1,876, forks +75 / +12,760, caller names +25 /
  +2,788. That totals **+113 / +17,448 B**. Measured: **+113 / +17,448 B**. Leg B equals the
  branch's own in-tree build byte for byte.
- **Units:** 61 improved, **0 regressed** (the unit sum is +113, equal to the net).
- **Rows off fuzzy 100 anywhere:** the two 40 B funclets in §4 (80 B).
- `none`-ruler control: +19,404 B. NOT_APPLICABLE, because the patch carries source.

## 6. Left, and where

- **ICF fold-survivor callee names only (alias work, not source):**
  - `PanelDir::Copy` 99.75
  - `DataEventList::InsertEvent` 99.94
  - `TaskTimeline::AddTask` 99.84
  - `WaveFile::ReadMarkers` 99.9
- **Needs a signature change plus a map rename:**
  - `TypeProps::Load` 92.6: retail is `Load(BinStream&, bool)`; ours still spells `BinStreamRev`.
  - `DebugModal` 58.1: rb3-Wii's `(bool&, char*, bool)`; `Debug.h` is a PCH input.
  - `MakeSessionJob` ctor 37.0: RTTI confirms the identity; TU5 takes 7 arguments.
- **Likely map mislabel:** `BeatMaster::Load` 99.89. The map names retail's callee
  `??0HamMasterLoader`, a DC3 class, at the address our `BeatMasterLoader` ctor calls.
- **Shared headers outside this scope:**
  - `UILabel::UpdateAndDrawHighlightMesh` 73.2 and `LabelUpdate` 84.9: inline
    `RndGroup::GetEnv`, `RndEnviron::SetAmbientAlpha`, `RndFont::TextureOwner` (rndobj).
  - `UIPicture::HookupMesh` 60.5: a `BaseMaterial` clear helper.
  - `TickedInfoCollection<String>::CopyFrom` 80.4: `reserve()` in `utl/TickedInfo.h`.
- **Register/scheduling after about three ideas:**
  - `Profiler::Stop` 85.6
  - `CacheXbox::ThreadGetFileSize` 90.6
  - `MakeEulerScale` 62.1
  - `MasterAudio::FillSwing` 61.5
  - `VoiceBeat::Analyze` 86.5
  - `JsonConverter::LoadFromString` 95.7
  - `HttpGet::Poll` 97.1
  - `SongData::Load` 98.8
  - `HDCache::OpenFiles` 99.2
  - `MemTrackInit` 99.8 (MemTracker/AllocInfo sizes)
- **Not kept:** `AsyncFileWin::_ReadDone` 42.1. The correct removal scored lower.
- **The 86 caller-bound rows were named, not repaired:** 23 read 100 and 63 are below. The
  biggest are `DirLoader::LoadObjs`/`SetupDir`/`LoadDir`, `LabelShrinkWrapper` ctor/draw,
  `StandardStream::setJumpSamplesFromMs`, `TrackWidget::SyncImp` and
  `GameGemList::Finalize`. This is the next repair round.
- **Next identification veins:**
  - **234 caller-proven rows / 36 KB are foreign-pinned.** The name is defined in another
    unit's base obj, so each needs a re-home and its own A/B.
  - 100 rows / 12 KB have a caller-proven name that no base obj defines (write the function).
  - 1,008 rows / 119 KB have no caller and no feature.
  - Pin problems seen by fork D: the retail `HttpGet` ctor (`fn_827DC9E8`) sits in JsonUtils'
    pin, and the `WriteCareerLeaderboardJob` caller sits in CheatProvider's.

## 7. Rows, before and after

On main `fdfbdb090` **every row below was an anonymous `fn_` at fuzzy 0, unpaired**. "at naming"
is the report score right after the map row went in, before repair. "after" is the rebased
branch build, which equals leg B.

| after | rows | bytes |
|---|---:|---:|
| 100 | 89 | 16,572 |
| 99–100 | 10 | 3,664 |
| 95–99 | 10 | 2,332 |
| 90–95 | 7 | 1,828 |
| < 90 | 62 | 12,944 |
| **all** | **178** | **37,340** |

| retail row | size | name | unit | how identified | at naming | after |
|---|---:|---|---|---|---:|---:|
| `0x82717d38` | 1408 | `?Analyze@VoiceBeat@@QAAXPAMH_N1M@Z` | VoiceBeat | overlap/mech | 56.56 | 86.55 |
| `0x827c7fe8` | 1304 | `?Handle@Song@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | Song | overlap/drift | 66.68 | **100** |
| `0x82534940` | 1016 | `?OpenFiles@HDCache@@AAAXH@Z` | HDCache | overlap/drift | 69.73 | 99.21 |
| `0x827d3a70` | 760 | `?ReadMarkers@WaveFile@@AAAXXZ` | WaveFile | overlap/drift | 63.33 | 99.95 |
| `0x827f2e98` | 740 | `?Save@UILabel@@UAAXAAVBinStream@@@Z` | UILabel | overlap/drift | 61.02 | **100** |
| `0x827dcab0` | 720 | `?Poll@HttpGet@@QAAXXZ` | HttpGet | overlap/mech | 59.52 | 97.13 |
| `0x82810ed0` | 648 | `?PostLoad@UILabelDir@@UAAXAAVBinStream@@@Z` | UILabelDir | overlap/drift | 54.01 | **100** |
| `0x827796f8` | 612 | `?Load@SongData@@QAAXPAVSongInfo@@HPAVPlayerTrackConfigList@@AAV?$vector@PAVMidiReceiver...` | SongData | overlap/drift | 82.73 | 98.82 |
| `0x827f3180` | 596 | `?UpdateAndDrawHighlightMesh@UILabel@@QAAXXZ` | UILabel | overlap/drift | 48.95 | 73.23 |
| `0x82711ce0` | 488 | `?Handle@MetaMusic@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | MetaMusic | overlap/mech | 56.25 | **100** |
| `0x827a6200` | 476 | `?Poll@SongPreview@@QAAXXZ` | SongPreview | overlap/mech | 98.32 | **100** |
| `0x82756260` | 476 | `?LoadObjs@DirLoader@@AAAXXZ` | DirLoader | callers (2) | 0.00 | 0.00 |
| `0x82826930` | 476 | `?UpdateAndDrawWrapper@LabelShrinkWrapper@@IAAXXZ` | LabelShrinkWrapper | callers (1) | 47.97 | 47.97 |
| `0x827d2660` | 464 | `?Stop@Profiler@@QAAXXZ` | Profiler | overlap/witness | 41.61 | 85.59 |
| `0x827f6258` | 464 | `?LabelUpdate@UILabel@@IAAX_N0@Z` | UILabel | overlap/drift | 68.66 | 84.93 |
| `0x82756f38` | 464 | `?CreateObjects@DirLoader@@AAAXXZ` | DirLoader | overlap/drift | 52.55 | **100** |
| `0x828043b0` | 464 | `??$__equal_range@U?$_List_iterator@PAVUIResource@@U?$_Nonconst_traits@PAVUIResource@@@s...` | UI | callers (1) | 100.00 | **100** |
| `0x827025d8` | 460 | `?setJumpSamplesFromMs@StandardStream@@AAAXMM@Z` | StandardStream | callers (1) | 29.63 | 29.63 |
| `0x827050d0` | 448 | `?DoJump@StandardStream@@AAAXXZ` | StandardStream | overlap/drift | 64.96 | **100** |
| `0x827560a0` | 440 | `?SetupDir@DirLoader@@AAA_NVSymbol@@@Z` | DirLoader | callers (3) | 21.54 | 21.54 |
| `0x82787a30` | 404 | `?_M_insert_overflow_aux@?$vector@UPartInfo@@V?$StlNodeAlloc@UPartInfo@@@stlpmtx_std@@@s...` | SongParser | overlap/mech | 100.00 | **100** |
| `0x827e2d68` | 392 | `?SyncImp@TrackWidget@@QAAXXZ` | TrackWidget | callers (3) | 40.82 | 40.82 |
| `0x827661c0` | 368 | `?Load@TypeProps@@QAAXAAVBinStreamRev@@@Z` | TypeProps | overlap/drift | 36.07 | 92.61 |
| `0x8276f510` | 368 | `?Load@BeatMaster@@QAAXPAVSongInfo@@HPAVPlayerTrackConfigList@@_NW4SongDataValidate@@PAV...` | BeatMaster | overlap/drift | 96.79 | 99.89 |
| `0x82713420` | 364 | `??0NoteVoiceInst@@QAA@PAVMidiInstrument@@PAVSampleZone@@EEHHM@Z` | MidiInstrument | callers (1) | 94.03 | 94.03 |
| `0x827c2c28` | 356 | `?Handle@CheatsManager@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | Cheats | overlap/drift | 66.26 | **100** |
| `0x82826ba0` | 348 | `??0LabelShrinkWrapper@@IAA@XZ` | LabelShrinkWrapper | callers (1) | 93.16 | 93.16 |
| `0x827761b8` | 336 | `?_M_insert_overflow_aux@?$vector@V?$vector@V?$RangedData@U?$pair@HH@stlpmtx_std@@@?$Ran...` | SongData | callers (2) | 100.00 | **100** |
| `0x82776358` | 336 | `?_M_insert_overflow_aux@?$vector@V?$vector@V?$RangedData@VRGRollChord@@@?$RangedDataCol...` | SongData | callers (2) | 100.00 | **100** |
| `0x827d3770` | 328 | `?_M_insert_overflow_aux@?$vector@VWaveFileMarker@@V?$StlNodeAlloc@VWaveFileMarker@@@stl...` | WaveFile | callers (1) | 99.88 | 99.88 |
| `0x827edc30` | 324 | `?InsertEvent@DataEventList@@QAAXMMABVDataNode@@H@Z` | DataEventList | overlap/witness | 69.91 | 99.94 |
| `0x8278e200` | 320 | `?Finalize@GameGemList@@QAAXXZ` | GameGemList | callers (1) | 19.34 | 19.34 |
| `0x827566f0` | 316 | `?LoadDir@DirLoader@@AAAXXZ` | DirLoader | callers (4) | 0.00 | 0.00 |
| `0x8274ffc8` | 292 | `?PreloadSharedSubdirs@@YAXVSymbol@@@Z` | Dir | callers (3) | 24.81 | 24.81 |
| `0x8278db80` | 288 | `??$_M_range_insert_realloc@PBVGameGem@@@?$vector@VGameGem@@V?$StlNodeAlloc@VGameGem@@@s...` | GameGemList | callers (1) | 100.00 | **100** |
| `0x82772f70` | 284 | `??$_M_range_insert_realloc@PBV?$TickedInfo@VString@@@@@?$vector@V?$TickedInfo@VString@@...` | DrumMixDB | callers (1) | 100.00 | **100** |
| `0x8278f0a0` | 280 | `?CopyGem@GameGem@@QAAXPAV1@H@Z` | GameGem | overlap/witness | 100.00 | **100** |
| `0x82804c98` | 276 | `?FindResource@UIManager@@QAAPAVUIResource@@PBVDataArray@@@Z` | UI | overlap/mech | 76.10 | **100** |
| `0x82728680` | 272 | `?PreLoad@SynthSample@@UAAXAAVBinStream@@@Z` | SynthSample | overlap/drift | 64.79 | **100** |
| `0x82749b10` | 264 | `?Poll@TaskMgr@@QAAXXZ` | Task | overlap/mech | 44.29 | **100** |
| `0x827a4d00` | 264 | `?Load@DataArraySongInfo@@UAAXAAVBinStream@@@Z` | DataArraySongInfo | overlap/drift | 1.70 | **100** |
| `0x8278ce50` | 264 | `??$__adjust_heap@PAVGameGem@@HV1@U?$less@VGameGem@@@stlpmtx_std@@@stlpmtx_std@@YAXPAVGa...` | GameGemList | overlap/mech | 99.82 | 99.82 |
| `0x827c4bb8` | 256 | `?MemTrackInit@@YAXHH_N@Z` | MemTrack | overlap/drift | 47.66 | 99.81 |
| `0x82810048` | 256 | `?Handle@UILabelDir@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | UILabelDir | overlap/drift | 56.66 | **100** |
| `0x824b9710` | 256 | `?insert_unique_noresize@?$hashtable@U?$pair@$$CBVSymbol@@V?$vector@PAVLightPreset@@V?$S...` | LightPresetManager | overlap/mech | 98.28 | 98.28 |
| `0x82721470` | 252 | `?SetType@FxSendDistortion@@UAAXVSymbol@@@Z` | FxSendDistortion | overlap/mech | 100.00 | **100** |
| `0x827ab080` | 248 | `?Handle@SongMetadata@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | SongMetadata | overlap/drift | 65.02 | **100** |
| `0x8270c1a8` | 248 | `?Poll@FaderTask@@QAAXXZ` | Faders | overlap/witness | 47.11 | **100** |
| `0x8231ae78` | 240 | `?PreLoad@MeterDisplay@@UAAXAAVBinStream@@@Z` | MeterDisplay | overlap/witness | 54.30 | **100** |
| `0x828018b8` | 232 | `?SetData@DataProvider@@QAAXPAVDataArray@@@Z` | UIListProvider | overlap/mech | 61.71 | **100** |
| `0x824ef600` | 232 | `?FastInvert@@YAXABVMatrix3@Hmx@@AAV12@@Z` | mtx | callers (2) | 30.53 | 30.53 |
| `0x827bc670` | 232 | `?MemAllocSize@@YAHPAX@Z` | MemMgr | callers (1) | 29.43 | 29.43 |
| `0x824ee750` | 224 | `?MakeEulerScale@@YAXABVMatrix3@Hmx@@AAVVector3@@1@Z` | Rot | overlap/mech | 58.02 | 62.07 |
| `0x824ea8d0` | 224 | `?TryEnter@SharedGroup@@QAAXPAVWorldInstance@@@Z` | Instance | callers (1) | 53.54 | 53.54 |
| `0x827eb3a0` | 216 | `?ParseNote@MidiParser@@QAAXHHE@Z` | MidiParser | callers (1) | 96.09 | 96.09 |
| `0x82818d98` | 208 | `?GetMatVariationName@UIFontImporter@@QBA?AVSymbol@@I@Z` | UIFontImporter | overlap/witness | 50.54 | **100** |
| `0x82728288` | 208 | `?Save@SynthSample@@UAAXAAVBinStream@@@Z` | SynthSample | overlap/drift | 50.48 | **100** |
| `0x827f02c8` | 208 | `?ReenterScreen@UIScreen@@QAAXXZ` | UIScreen | callers (1) | 69.77 | 69.77 |
| `0x827c4018` | 204 | `?UpdateType@FormatString@@AAAXXZ` | MakeString | callers (3) | 100.00 | **100** |
| `0x827a2b60` | 200 | `?SaveFixedSymbol@FixedSizeSaveable@@SAXAAVFixedSizeSaveableStream@@ABVSymbol@@@Z` | FixedSizeSaveable | callers (4) | 57.46 | 57.46 |
| `0x827da7c0` | 196 | `?ThreadGetFileSize@CacheXbox@@IAAHXZ` | Cache_Xbox | overlap/mech | 65.51 | 90.61 |
| `0x828114a8` | 192 | `?Copy@UILabelDir@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z` | UILabelDir | overlap/drift | 43.23 | **100** |
| `0x82535bc8` | 188 | `?_ReadDone@AsyncFileWin@@MAA_NXZ` | AsyncFile_Win | overlap/mech | 42.13 | 42.13 |
| `0x82b82440` | 188 | `?LoadFromString@JsonConverter@@QAAPAVJsonObject@@ABVString@@@Z` | JsonUtils | overlap/mech | 32.02 | 95.74 |
| `0x8275eb80` | 188 | `?DataMin@@YA?AVDataNode@@PAVDataArray@@@Z` | DataFunc | callers (2) | 43.38 | 43.38 |
| `0x8275ec40` | 188 | `?DataMax@@YA?AVDataNode@@PAVDataArray@@@Z` | DataFunc | callers (2) | 43.28 | 43.28 |
| `0x82533b48` | 184 | `?CDRead@@YAHHHHPAX@Z` | CDReader | overlap/mech | 55.17 | **100** |
| `0x827462b0` | 184 | `?DrawToTexture@TexMovie@@QAAXXZ` | TexMovie | overlap/witness | 65.41 | **100** |
| `0x8270f050` | 184 | `?CacheWav@@YAPBDPBDAAW4CacheResourceResult@@@Z` | Utl | overlap/witness | 10.72 | **100** |
| `0x82771e90` | 184 | `?FindRangeAtTick@?$RangedDataCollection@U?$pair@HH@stlpmtx_std@@@@QAAPBV?$RangedData@U?...` | SongData | callers (2) | 94.89 | 94.89 |
| `0x82771ff0` | 184 | `?FindRangeAtTick@?$RangedDataCollection@VRGRollChord@@@@QAAPBV?$RangedData@VRGRollChord...` | SongData | callers (2) | 94.89 | 94.89 |
| `0x827720a8` | 184 | `?FindRangeAtTick@?$RangedDataCollection@VRGTrill@@@@QAAPBV?$RangedData@VRGTrill@@@1@HH@Z` | SongData | callers (2) | 94.89 | 94.89 |
| `0x824ba100` | 176 | `?Poll@CameraManager@@QAAXXZ` | CameraManager | overlap/witness | 53.93 | **100** |
| `0x827c8bd8` | 176 | `?Load@Song@@UAAXAAVBinStream@@@Z` | Song | overlap/witness | 59.05 | **100** |
| `0x8275a7e8` | 176 | `?SaveRest@Object@Hmx@@QAAXAAVBinStream@@@Z` | Object | callers (1) | 4.61 | 4.61 |
| `0x8275f348` | 176 | `?DataMultiply@@YA?AVDataNode@@PAVDataArray@@@Z` | DataFunc | callers (3) | 20.02 | 20.02 |
| `0x827ce998` | 176 | `?Poll@NetCacheMgr@@UAAXXZ` | NetCacheMgr | callers (1) | 29.80 | 29.80 |
| `0x8277b2e8` | 172 | `?RGUnpackChordShapeID@@YAXIAAV?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@PA...` | RGUtl | overlap/mech | 89.84 | **100** |
| `0x8275b5e8` | 172 | `?OnSet@Object@Hmx@@AAA?AVDataNode@@PBVDataArray@@@Z` | Object | callers (1) | 48.21 | 48.21 |
| `0x827a5b70` | 168 | `?PrepareFaders@SongPreview@@AAAXPBVSongInfo@@@Z` | SongPreview | callers (1) | 33.57 | 33.57 |
| `0x8278d388` | 168 | `??$__unguarded_partition@PAVGameGem@@V1@U?$less@VGameGem@@@stlpmtx_std@@@stlpmtx_std@@Y...` | GameGemList | callers (1) | 71.86 | 71.86 |
| `0x82815d88` | 164 | `?HookupMesh@UIPicture@@IAAXXZ` | UIPicture | overlap/witness | 60.49 | 60.49 |
| `0x82316840` | 164 | `?CopyMembers@InlineHelp@@UAAXPBVUIComponent@@W4CopyType@Object@Hmx@@@Z` | InlineHelp | overlap/mech | 100.00 | **100** |
| `0x827cd340` | 164 | `?AddPair@DataPoint@@QAAXVSymbol@@VDataNode@@@Z` | DataPointMgr | callers (9) | 51.98 | 51.98 |
| `0x827b5150` | 164 | `?HandleNetCacheLoaderFailure@StorePanel@@QAAXH@Z` | StorePanel | callers (2) | 47.20 | 47.20 |
| `0x82802978` | 160 | `?OverloadHorizontalNav@UIManager@@QBA_NW4JoypadAction@@W4JoypadButton@@VSymbol@@@Z` | UI | overlap/mech | 22.82 | **100** |
| `0x828093b0` | 160 | `?Copy@PanelDir@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z` | UISlider | overlap/mech | 94.75 | 99.75 |
| `0x828201c0` | 156 | `?Fill@UIListSubListElement@@UAAXABVUIListProvider@@HH@Z` | UIListSubList | overlap/drift | 56.26 | **100** |
| `0x82521798` | 148 | `FileGetStat` | File_Win | overlap/drift | 56.32 | **100** |
| `0x828171a8` | 148 | `?PreLoad@UIPicture@@UAAXAAVBinStream@@@Z` | UIPicture | overlap/witness | 0.00 | **100** |
| `0x82824640` | 148 | `?PreLoad@UIProxy@@UAAXAAVBinStream@@@Z` | UIProxy | overlap/mech | 100.00 | **100** |
| `0x8278c900` | 148 | `??$__unguarded_linear_insert@PAVGameGem@@V1@U?$less@VGameGem@@@stlpmtx_std@@@stlpmtx_st...` | GameGemList | callers (2) | 72.92 | 72.92 |
| `0x8250fc88` | 144 | `?DebugModal@@YAXAAW4ModalType@Debug@@AAVFixedString@@_N@Z` | Debug | overlap/drift | 58.06 | 58.06 |
| `0x8274eec0` | 144 | `?HasDirPtrs@ObjectDir@@QBA_NXZ` | Dir | callers (4) | 48.47 | 48.47 |
| `0x8277dcd8` | 140 | `?FillSwing@MasterAudio@@UAAXHHHH_N@Z` | MasterAudio | overlap/mech | 51.80 | 61.51 |
| `0x824ca0c8` | 140 | `??1EventAnim@@UAA@XZ` | EventAnim | overlap/drift | 59.69 | **100** |
| `0x8275b798` | 140 | `?OnGet@Object@Hmx@@IAA?AVDataNode@@PBVDataArray@@@Z` | Object | callers (1) | 0.00 | 0.00 |
| `0x824dd920` | 140 | `?Generate@Spotlight@@IAAXXZ` | Spotlight | callers (1) | 10.31 | 10.31 |
| `0x82780d78` | 140 | `?GetNumPracticePhrases@VocalNoteList@@QBAHABV?$vector@VVocalPhrase@@V?$StlNodeAlloc@VVo...` | VocalNoteList | callers (1) | 73.37 | 73.37 |
| `0x82774a40` | 136 | `?CopyFrom@?$TickedInfoCollection@VString@@@@QAAXABV1@@Z` | DrumMixDB | overlap/mech | 80.41 | 80.41 |
| `0x82761370` | 136 | `?DataHandleRet@@YA?AVDataNode@@PAVDataArray@@@Z` | DataFunc | callers (2) | 43.18 | 43.18 |
| `0x827a2830` | 136 | `?Submit@Achievements@@QAAXPAVLocalUser@@VSymbol@@H@Z` | Achievements | callers (1) | 30.88 | 30.88 |
| `0x82802fa8` | 136 | `?CancelTransition@UIManager@@AAAXXZ` | UI | callers (2) | 100.00 | **100** |
| `0x8272a940` | 132 | `??0SampleInst@@QAA@PAVSynthSample@@@Z` | SampleInst | overlap/drift | 43.67 | **100** |
| `0x823f6258` | 132 | `??0MakeSessionJob@@QAA@PAPAXKH@Z` | SessionJobs_Xbox | overlap/mech | 37.00 | 37.00 |
| `0x824e9d20` | 132 | `?Load@SpotlightEnder@@UAAXAAVBinStream@@@Z` | SpotlightEnder | overlap/witness | 49.48 | **100** |
| `0x8275a460` | 132 | `?RemoveFromDir@Object@Hmx@@AAAXXZ` | Object | callers (1) | 41.70 | 41.70 |
| `0x82814fb8` | 132 | `?Load@UIListSlot@@UAAXAAVBinStream@@@Z` | UIListSlot | callers (1) | 42.52 | 42.52 |
| `0x82749738` | 128 | `?AddTask@TaskTimeline@@AAAXABUTaskInfo@1@@Z` | Task | overlap/mech | 67.97 | 99.84 |
| `0x827c4240` | 128 | `??6FormatString@@QAAAAV0@M@Z` | MakeString | callers (26) | 75.62 | 75.62 |
| `0x827cc1e8` | 124 | `?StartChecksum@BufStream@@QAAXPBD@Z` | BufStream | overlap/mech | 100.00 | **100** |
| `0x827c42c0` | 124 | `??6FormatString@@QAAAAV0@ABVString@@@Z` | MakeString | callers (12) | 69.61 | 69.61 |
| `0x82761160` | 120 | `?DataHandleTypeRet@@YA?AVDataNode@@PAVDataArray@@@Z` | DataFunc | overlap/witness | 35.93 | **100** |
| `0x82824e10` | 120 | `?Load@Screenshot@@UAAXAAVBinStream@@@Z` | Screenshot | overlap/witness | 0.00 | **100** |
| `0x828187c0` | 120 | `?OnGetGennedBitmapPath@UIFontImporter@@IAA?AVDataNode@@PAVDataArray@@@Z` | UIFontImporter | callers (1) | 53.00 | 53.00 |
| `0x8250f820` | 112 | `?Exit@Debug@@QAAXH_N@Z` | Debug | overlap/witness | 55.00 | **100** |
| `0x827829a8` | 112 | `?CheckDrumMapMarker@SongParser@@QAA_NHH_N@Z` | SongParser | overlap/mech | 50.54 | **100** |
| `0x8252dee8` | 112 | `?New@AsyncFile@@SAPAV1@PBDH@Z` | AsyncFile | overlap/drift | 0.00 | **100** |
| `0x82802b88` | 112 | `?NewObject@UIPicture@@SAPAVObject@Hmx@@XZ` | UI | overlap/mech | 100.00 | **100** |
| `0x82802c28` | 112 | `?NewObject@UIProxy@@SAPAVObject@Hmx@@XZ` | UI | overlap/mech | 100.00 | **100** |
| `0x823f6780` | 112 | `?IsFinished@XboxSessionJob@@UAA_NXZ` | SessionJobs_Xbox | overlap/mech | 62.43 | **100** |
| `0x827d7220` | 112 | `?Print@AllocInfo@@QBAXAAVTextStream@@@Z` | AllocInfo | callers (1) | 0.00 | 0.00 |
| `0x82809e28` | 112 | `?Save@UISlider@@UAAXAAVBinStream@@@Z` | UISlider | callers (1) | 78.54 | 78.54 |
| `0x8231ad28` | 108 | `??_DMeterDisplay@@QAAXXZ` | MeterDisplay | overlap/mech | 100.00 | **100** |
| `0x824f3160` | 108 | `?RandomFloat@@YAMMM@Z` | Rand | callers (37) | 55.37 | 55.37 |
| `0x8280f888` | 108 | `?PreLoad@UIButton@@UAAXAAVBinStream@@@Z` | UIButton | callers (4) | 30.48 | 30.48 |
| `0x8278ed38` | 108 | `?PackRealGuitarData@GameGem@@QAAXXZ` | GameGem | callers (1) | 95.19 | 95.19 |
| `0x8281f2c0` | 104 | `??0UIListHighlight@@IAA@XZ` | UIListHighlight | overlap/witness | 69.62 | **100** |
| `0x827beac8` | 104 | `?AllocBuffer@FileLoader@@AAAXXZ` | Loader | overlap/witness | 16.23 | **100** |
| `0x82bb1b90` | 100 | `??1OggMap@@UAA@XZ` | OggMap | overlap/mech | 100.00 | **100** |
| `0x82801bf8` | 100 | `??0UITransitionHandler@@QAA@PAVObject@Hmx@@@Z` | UITransitionHandler | callers (3) | 24.60 | 24.60 |
| `0x827033c8` | 96 | `?Play@StandardStream@@UAAXXZ` | StandardStream | overlap/mech | 58.33 | **100** |
| `0x827153d0` | 96 | `??1MidiInstrument@@UAA@XZ` | MidiInstrument | overlap/drift | 43.83 | **100** |
| `0x828047e8` | 96 | `?PushScreen@UIManager@@UAAXPAVUIScreen@@@Z` | UI | callers (1) | 0.00 | 0.00 |
| `0x8270f490` | 96 | `?GetMicIDForClientID@MicClientMapper@@QBAHABVMicClientID@@@Z` | MicClientMapper | callers (3) | 95.83 | 95.83 |
| `0x827f4c18` | 92 | `?SetIcon@UILabel@@QAAXD@Z` | UILabel | overlap/drift | 53.17 | **100** |
| `0x823f6d90` | 92 | `?Start@AddLocalPlayerJob@@UAAXXZ` | SessionJobs_Xbox | overlap/mech | 77.83 | **100** |
| `0x8251bfe8` | 92 | `?ShowPartyUI@PlatformMgr@@QAA_NH@Z` | PlatformMgr_Xbox | callers (1) | 51.96 | 51.96 |
| `0x82780ee8` | 88 | `??$RemoveInvalidFreestyle@V?$binder1st@V?$pointer_to_binary_function@PAVDataArray@@ABU?...` | VocalNoteList | overlap/mech | 95.45 | 95.45 |
| `0x823f6e50` | 88 | `?Start@RemoveLocalPlayerJob@@UAAXXZ` | SessionJobs_Xbox | overlap/mech | 77.09 | **100** |
| `0x823f6fe8` | 88 | `?Start@WriteCareerLeaderboardJob@@UAAXXZ` | SessionJobs_Xbox | overlap/mech | 63.64 | **100** |
| `0x8274f1f0` | 88 | `?Reserve@ObjectDir@@QAAXHH@Z` | Dir | callers (7) | 43.55 | 43.55 |
| `0x827d8e90` | 88 | `??1IDataChunk@@UAA@XZ` | Chunks | callers (4) | 63.23 | 63.23 |
| `0x828238b8` | 80 | `??1UIProxy@@UAA@XZ` | UIProxy | overlap/witness | 0.00 | **100** |
| `0x823f7040` | 80 | `?Start@StartSessionJob@@UAAXXZ` | SessionJobs_Xbox | overlap/mech | 60.10 | **100** |
| `0x82b6e280` | 80 | `??_GSampleInst360@@UAAPAXI@Z` | SampleInst360 | overlap/mech | 99.95 | **100** |
| `0x824b9590` | 80 | `?_M_create_node@?$slist@U?$pair@$$CBVSymbol@@V?$vector@PAVLightPreset@@V?$StlNodeAlloc@...` | LightPresetManager | callers (1) | 100.00 | **100** |
| `0x827a2b10` | 80 | `??5@YAAAVFixedSizeSaveableStream@@AAV0@AAVFixedSizeSaveable@@@Z` | FixedSizeSaveable | callers (32) | 0.00 | 0.00 |
| `0x8278ec30` | 80 | `?IsMuted@GameGem@@QBA_NXZ` | GameGem | callers (10) | 100.00 | **100** |
| `0x823f7090` | 76 | `?Start@EndSessionJob@@UAAXXZ` | SessionJobs_Xbox | overlap/mech | 68.53 | **100** |
| `0x827abf78` | 76 | `??1MemcardMgr@@UAA@XZ` | MemcardMgr_Xbox | callers (2) | 100.00 | **100** |
| `0x8278f1b8` | 76 | `?GetHighestFret@GameGem@@QBADXZ` | GameGem | callers (1) | 100.00 | **100** |
| `0x827efa18` | 72 | `?Error@MidiReceiver@@QAAXPBDH@Z` | MidiReceiver | overlap/mech | 58.94 | **100** |
| `0x82803668` | 72 | `??1UIResource@@QAA@XZ` | UI | overlap/mech | 100.00 | **100** |
| `0x8274dec8` | 72 | `?NewObject@Object@Hmx@@SAPAV12@XZ` | Dir | callers (2) | 100.00 | **100** |
| `0x827a2ac8` | 72 | `??6@YAAAVFixedSizeSaveableStream@@AAV0@ABVFixedSizeSaveable@@@Z` | FixedSizeSaveable | callers (35) | 0.00 | 0.00 |
| `0x82825080` | 72 | `??$New@VUIComponent@@@Object@Hmx@@SAPAVUIComponent@@XZ` | UITrigger | callers (1) | 100.00 | **100** |
| `0x82524688` | 72 | `??$New@VMsgSource@@@Object@Hmx@@SAPAVMsgSource@@XZ` | OnlineID | callers (1) | 100.00 | **100** |
| `0x8251ca08` | 72 | `?ShowOfferUI@PlatformMgr@@QAAXH@Z` | PlatformMgr_Xbox | callers (1) | 22.11 | 22.11 |
| `0x827d4508` | 68 | `?StopLog@MemTracker@@QAAXXZ` | MemTracker | overlap/mech | 64.12 | **100** |
| `0x828268d8` | 68 | `?PostLoad@LabelShrinkWrapper@@UAAXAAVBinStream@@@Z` | LabelShrinkWrapper | callers (1) | 100.00 | **100** |
| `0x8251bfa8` | 60 | `?ShowFriendsUI@PlatformMgr@@QAAXH@Z` | PlatformMgr_Xbox | callers (1) | 26.53 | 26.53 |
| `0x82772d28` | 60 | `??$_Copy_Construct@V?$vector@V?$RangedData@I@?$RangedDataCollection@I@@V?$StlNodeAlloc@...` | SongData | callers (1) | 99.67 | 99.67 |
| `0x8270fab8` | 56 | `?Loaded@MetaMusic@@QAA_NXZ` | MetaMusic | overlap/witness | 53.21 | **100** |
| `0x8278eb28` | 48 | `?RightHandTap@GameGem@@QBA_NXZ` | GameGem | callers (2) | 58.33 | 58.33 |
| `0x82811e00` | 48 | `?ClassName@UILabelDir@@UBA?AVSymbol@@XZ` | UILabelDir | callers (1) | 100.00 | **100** |
| `0x827b4ae0` | 32 | `??8@YA_NABUEnumProduct@@ABVStorePurchaseable@@@Z` | StorePanel | callers (1) | 100.00 | **100** |
| `0x82768b18` | 24 | `??1VarStack@@QAA@XZ` | DataUtl | fold proven (alias) | 100.00 | **100** |
| `0x827732d8` | 24 | `?GetRGTrillAtTick@SongData@@QBA_NHHAAVRGTrill@@@Z` | SongData | callers (3) | 98.33 | 98.33 |
| `0x827732f0` | 24 | `?RGTrillStartsAt@SongData@@QAA_NHHAAH@Z` | SongData | callers (1) | 98.33 | 98.33 |
| `0x828030e0` | 20 | `?PushDepth@UIManager@@QBAHXZ` | UI | callers (2) | 100.00 | **100** |
| `0x8278eb80` | 16 | `?GetRGNoteType@GameGem@@QBA?AW4RGNoteType@@I@Z` | GameGem | callers (10) | 100.00 | **100** |
| `0x827be4a0` | 12 | `?insert@String@@QAAAAV1@IABV1@@Z` | Str | callers (1) | 46.67 | 46.67 |
| `0x82b81dd8` | 8 | `?GetType@JsonObject@@QBA?AW4EType@1@XZ` | JsonUtils | callers (11) | 0.00 | 0.00 |
| `0x8278eb70` | 8 | `?GetImportantStrings@GameGem@@QBAEXZ` | GameGem | callers (5) | 100.00 | **100** |
| `0x8278eb98` | 8 | `?GetRGChordID@GameGem@@QBAHXZ` | GameGem | callers (7) | 100.00 | **100** |
| `0x8278eba0` | 8 | `?GetRootNote@GameGem@@QBAEXZ` | GameGem | callers (7) | 100.00 | **100** |
| `0x8251c178` | 4 | `?DisableXMP@PlatformMgr@@QAAXXZ` | PlatformMgr_Xbox | callers (2) | 100.00 | **100** |

## 8. Reproduce

```
python3 tools/icf_pair_adjudicate.py --chasetest && python3 tools/icf_pair_adjudicate.py --self-break
python3 tools/icf_pair_adjudicate.py --pairs ~/tmp/w16hs_varstack_pairs.json --chase --family
python3 ~/tmp/w16hs/sem.py                                   # overlap matcher -> sem.json
python3 ~/tmp/w16hs/props.py 16                              # union + header-inline cut -> props.json
python3 tools/anon_proposal_adjudicate.py ~/tmp/w16hs/props.json --independent --json-out adj_ind.json
python3 ~/tmp/w16hs/decide.py adj_ind.json dec_ind.json      # pre-registered rule
python3 ~/tmp/w16hs/callers.py && python3 ~/tmp/w16hs/props_call.py
python3 tools/anon_proposal_adjudicate.py ~/tmp/w16hs/props_call.json --independent --json-out adj_call.json
python3 ~/tmp/w16hr/decide_call.py adj_call.json props_call.json acc_call.json oursize_call.json
python3 tools/gated_map_write.py --target scripts/target_symbol_map.json --rows-json <rows.json>
python3 tools/icf_alias_finder.py --validate && python3 tools/map_name_injectivity.py
python3 tools/ab_measure.py --worktree <main + symbols.txt commits> --patch <that..w16-hs>
```

Scratch (proposals, adjudications, decisions, snapshots, per-fork reports) lives in
`~/tmp/w16hs/` and is not committed.
