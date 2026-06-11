# obj_orphan Cleanup Worklist — 2026-06-11

**Tool:** `tools/map_lint.py --check obj_orphan`  
**Map:** `scripts/target_symbol_map.json` (12,261 entries)  
**Units scanned:** 628 pinned units (from `config/45410914/splits.txt`)  
**Baseline:** 6932 matched functions @ main `154a11a`

---

## Summary Counts

| Category | Units | Orphan Entries |
|---|---|---|
| CLEANUP-SAFE (foreign class, safe to remove) | 157 | 911 |
| INVESTIGATE-MIS-PIN (split-boundary leakage) | 9 key cases | 72 |
| DO-NOT-TOUCH / AMBIGUOUS (own-class orphans, will pair when source lands) | 13 | 92 |
| No compiled obj found (C libs, not built yet) | 28 | 28 (sentinel) |
| **Total findings** | **179 units** | **1103** |

The 1103 headline figure from the tool includes 28 "no compiled obj found" sentinels (all C
library units: `aes.c`, `block.c`, `deflate.c`, `json_object.c`, Vorbis codecs, etc. — these
are correct to skip; the obj_orphan check can't run without a built obj).  The 1075
**real orphan entries** (those with a VA) are what this doc covers.

**All 179 orphan-bearing units have source files present** (every `src/` path resolved in
`report.json`).  None are unported; all are compiled and partially matched.  The
CLEANUP-SAFE verdict applies because the orphan class names are definitively not defined by the
unit's compiled obj — by construction, those map entries can never pair.

---

## Precedent: BandIKEffector (wave-2 batch-3)

12 stale MidiInstrument/SampleZone/BoneOp entries were purged from the BandIKEffector unit.
A/B before/after: **6932 → 6932** (zero count change, hygiene only).  The cleanup eliminated
`[sym]` mismatches that were polluting unit fuzzy and could have masked future real matches.
This is the model for CLEANUP-SAFE work below.

---

## CLEANUP-SAFE — Top 20 Units (ranked by orphan count)

All units below have source landed, compiled objs present, and orphan classes that are
definitively foreign (no own-class orphans; orphan VAs are not clustered at a split
boundary adjacent to the class's real pin).

| Rank | Unit | Orphans | Fuzzy% | Fns matched | Dominant foreign class | Evidence |
|---|---|---|---|---|---|---|
| 1 | Utl | 82 | 62.5% | 70/167 | RndMesh(7), RndMultiMesh(6), Edge(5) | ALL-FOREIGN; scattered throughout range |
| 2 | TexBlender | 57 | 10.4% | 70/281 | ectorSort(13), RndAmbientOcclusion(12) | ALL-FOREIGN; AO/mesh helper instantiations |
| 3 | Dir | 39 | 13.4% | 50/208 | ObjectDir(22), InlinedDir(6) | ALL-FOREIGN; ObjectDir is separate unit |
| 4 | System | 29 | 0.0% | 0/40 | CLeapSystem(27), CThreadBase(1) | ALL-FOREIGN; `CLeapSystem@LEAPCORE` is XAudio2 subsystem |
| 5 | StreamNull | 27 | 5.8% | 15/190 | MoggClipMap(11), Sfx(3), SfxMap(2) | ALL-FOREIGN; map oracle mixed unrelated synth classes |
| 6 | HamCamTransform | 26 | 21.4% | 38/171 | EyeDesc(5), DistEntry(4), NavItem(3) | ALL-FOREIGN |
| 7 | CharClipSet | 23 | 22.2% | 49/174 | String(6), Weight(3), MoveReplacer(3) | ALL-FOREIGN |
| 8 | DirLoader | 22 | 15.2% | 93/233 | Object(9), Symbol(3), DataNode(3) | ALL-FOREIGN |
| 9 | Mic | 21 | 19.5% | 28/110 | MicXbox(5), MicManagerXbox(4), ChatBuffer(3) | ALL-FOREIGN |
| 10 | Env_NG | 19 | 36.1% | 9/41 | NgRnd(8), NgLight(3) | ALL-FOREIGN |
| 11 | EventTrigger | 18 | 32.7% | 152/265 | PropTriggerDefn(9), RndShaderProgram(4) | ALL-FOREIGN |
| 12 | MeshAnim | 17 | 35.1% | 81/271 | RndMultiMesh(5), RndShaderMgr(5) | ALL-FOREIGN |
| 13 | MemHeap | 15 | 11.7% | 5/62 | String(4), FixedString(2) | ALL-FOREIGN |
| 14 | CubeTex | 15 | 7.4% | 12/84 | DxCubeTex(4), DxMultiMesh(3) | ALL-FOREIGN |
| 15 | Console | 14 | 38.0% | 40/86 | RndMultiMesh(10), Transform(1) | ALL-FOREIGN |
| 16 | Debug | 14 | 19.2% | 18/69 | DataArray(6), DataNode(2) | MOSTLY-FOREIGN; 1 Timer near tail — see INVESTIGATE |
| 17 | MeshDeform | 14 | 25.4% | 57/130 | LeaderboardRow(2), Viewport(2) | ALL-FOREIGN |
| 18 | NetworkSocket_Win | 13 | 0.0% | 0/21 | WinSockSocket(13) | **HIGH-VALUE** — see below |
| 19 | UIGuide | 12 | 7.2% | 19/166 | LabelNumberTicker(6), EH-runtime(6) | **HIGH-VALUE** — see below |
| 20 | DataArraySongInfo | 12 | 10.2% | 14/36 | TrackChannels(10) | ALL-FOREIGN |

### Special Notes on High-Value CLEANUP-SAFE Entries

**NetworkSocket_Win (13 orphans, rank 18)**  
The unit's compiled obj defines `NetworkSocket` methods (base class, `ResolveHostName`,
etc.).  All 13 orphan entries are `WinSockSocket` methods — a *derived* class that has its
**own separate split** at a completely different VA range.  The map oracle pinned
`WinSockSocket` methods into the `NetworkSocket_Win` range in error.  The `NetworkSocket_Win`
compiled obj defines **zero** `WinSockSocket` symbols.  Removing all 13 is zero-risk.

**UIGuide (12 orphans, rank 19)**  
6 orphans are `LabelNumberTicker` methods (`Save`, `UpdateDisplay`, `Poll`,
`SetDesiredValue`, `CountUp`, `SyncProperty`) at VAs `0x82802F30`–`0x828036E8`, which are
inside the UIGuide range `0x82801070`–`0x82804770`.  `LabelNumberTicker` has its own
compiled obj (`src/system/ui/LabelNumberTicker.obj`) defining all 87 `LabelNumberTicker`
methods, and `UIGuide.obj` defines zero.  The remaining 6 orphans are EH-runtime helper
symbols (`_GetEstablisherFrame`, `_ExecutionInCatch`, `__FrameUnwindToEmptyState`,
`__SehTransFilter`, `_GetRangeOfTrysToCheck`, and `?Run@App`) — these are clearly not
UIGuide's.  All 12 are safe to remove.

**System (29 orphans, rank 4)**  
All 29 are `CLeapSystem@LEAPCORE` or `CThreadBase` methods — XAudio2/LEAP subsystem code.
`System.obj` (`src/system/os/System.cpp`) defines `DataArray`, `Timer`, and OS helper
symbols, none of which are `CLeapSystem`.  The `System` unit's VA range `0x82BAF878`–
`0x82BB0EB0` is in the high-address block isolated from the main `System.cpp` cluster.
This looks like the oracle mapped `CLeapSystem` into the wrong `System` unit by name
collision.  All 29 are safe to remove.

---

## Exact Removal Procedure for Top CLEANUP-SAFE Units

### Verify command (run after any map edit)

```bash
rm build/45410914/target_symbol_renames.stamp && \
  touch config/45410914/config.yml && \
  NINJA_JOBS=12 tools/fresh_report.sh
```

Judge by `report.json measures.matched_functions` staying at **6932** (±0).  If it
drops, the removed entry was actually matching something — revert and investigate.

### 1. NetworkSocket_Win — 13 entries to delete

Delete these keys from `scripts/target_symbol_map.json`:

```
"0x8251DF68"  ?Init@WinSockSocket@@SAXXZ
"0x8251DFD0"  ??0WinSockSocket@@AAA@I_N@Z
"0x8251E038"  ?SetNoDelay@WinSockSocket@@UAA_N_N@Z
"0x8251E088"  ?Connect@WinSockSocket@@UAA_NIG@Z
"0x8251E110"  ?Accept@WinSockSocket@@UAAPAVNetworkSocket@@XZ
"0x8251E188"  ?GetRemoteIP@WinSockSocket@@UAAXAAIAAG@Z
"0x8251E1E8"  ?Disconnect@WinSockSocket@@UAAXXZ
"0x8251E2C0"  ?InqBoundPort@WinSockSocket@@UBA_NAAG@Z
"0x8251E320"  ?BroadcastTo@WinSockSocket@@UAAHPBXIG@Z
"0x8251E390"  ?Recv@WinSockSocket@@UAAHPAXI@Z
"0x8251E400"  ?CanSend@WinSockSocket@@UBA_NXZ
"0x8251E460"  ?CanRead@WinSockSocket@@UBA_NXZ
"0x8251E550"  ??1WinSockSocket@@UAA@XZ
```

### 2. System — 29 entries to delete

Delete these keys from `scripts/target_symbol_map.json`:

```
"0x82BAF878"  ?InitializeLock@CLeapSystem@LEAPCORE@@QAAJXZ
"0x82BAF8D0"  ?OnProcessingPassStart@CLeapSystem@LEAPCORE@@UAAXXZ
"0x82BAF8F8"  ?OnProcessingPassEnd@CLeapSystem@LEAPCORE@@UAAXXZ
"0x82BAF920"  ?OnUpdatePerformanceData@CLeapSystem@LEAPCORE@@UAAXPAUPIX_XAUDIO2_COUNTERS@@@Z
"0x82BAF948"  ?GetRendererCount@CLeapSystem@LEAPCORE@@UAAJPAI@Z
"0x82BAFA18"  ?DestroyLeapBuffer@CLeapSystem@LEAPCORE@@UAAXH@Z
"0x82BAFAA0"  ?AddSkinToExecutionList@CLeapSystem@LEAPCORE@@AAAJPAVCBaseSkin@2@0@Z
"0x82BAFB08"  ?WakeThread@CThreadBase@@IAAXXZ
"0x82BAFB60"  ?MemoryAlloc@?$CNonBlockingQueue@PAUCommand@LEAPCORE@@@@CAPAXPAXK@Z
"0x82BAFB78"  ?QueryInterface@CLeapSystem@LEAPCORE@@UAAJABU_GUID@@PAPAX@Z
"0x82BAFBF8"  ?CreateFilter@CLeapSystem@LEAPCORE@@UAAJPAUIUnknown@@PAUILeapFilter@@IPAHI2IIPAPAX@Z
"0x82BAFCC0"  ?CreateVoice@CLeapSystem@LEAPCORE@@UAAJPBUtWAVEFORMATEX@@IPAUIUnknown@@1PAUILeapVoiceCallback@@1PAUILeapSubmixVoice@@K1IPAPAX@Z
"0x82BAFDC0"  ?NotifyAllSkins@CLeapSystem@LEAPCORE@@AAAJW4SystemNotification@2@PAVCGraphManager@2@@Z
"0x82BAFF48"  ?DisconnectRenderer@CLeapSystem@LEAPCORE@@UAAJPAUILeapRendererConnection@@@Z
"0x82BB00D8"  ?StartGraph@CLeapSystem@LEAPCORE@@UAAJIPAKI@Z
"0x82BB0168"  ?StopGraph@CLeapSystem@LEAPCORE@@UAAJI@Z
"0x82BB0258"  ?Commit@CLeapSystem@LEAPCORE@@UAAJI@Z
"0x82BB0330"  ?ExecuteCommand@CLeapSystem@LEAPCORE@@UAAJPAUCommand@2@@Z
"0x82BB0340"  ?ScheduleRegisterCommand@CLeapSystem@LEAPCORE@@AAAJPAVCBaseSkin@2@0I@Z
"0x82BB03D8"  ?DestroyAllSkins@CLeapSystem@LEAPCORE@@AAAXXZ
"0x82BB0458"  ?DestroyAllRenderers@CLeapSystem@LEAPCORE@@AAAXXZ
"0x82BB04D0"  ?GetRendererDetailsXbox@CLeapSystem@LEAPCORE@@AAAJIPAURendererDetails@@@Z
"0x82BB0608"  ?RemoveFromSkinList@CLeapSystem@LEAPCORE@@AAAXPAVCBaseSkin@2@@Z
"0x82BB0728"  ??0CLeapSystem@LEAPCORE@@QAA@XZ
"0x82BB0878"  ?Uninitialize@CLeapSystem@LEAPCORE@@AAAXXZ
"0x82BB0930"  ?GetRendererDetails@CLeapSystem@LEAPCORE@@UAAJIPAURendererDetails@@@Z
"0x82BB0AA0"  ?AddToSkinList@CLeapSystem@LEAPCORE@@AAAJPAVCBaseSkin@2@@Z
"0x82BB0B10"  ??1CLeapSystem@LEAPCORE@@UAA@XZ
"0x82BB0D60"  ?ConnectRenderer@CLeapSystem@LEAPCORE@@UAAJPB_WIPBUtWAVEFORMATEX@@PAXPAPAUILeapRendererConnection@@@Z
```

### 3. UIGuide — 12 entries to delete

```
"0x82802F30"  ?Save@LabelNumberTicker@@UAAXAAVBinStream@@@Z
"0x82803230"  ?UpdateDisplay@LabelNumberTicker@@IAAXXZ
"0x82803288"  ?Poll@LabelNumberTicker@@UAAXXZ
"0x82803408"  ?SetDesiredValue@LabelNumberTicker@@AAAXH@Z
"0x828034A8"  ?CountUp@LabelNumberTicker@@QAAXXZ
"0x828036E8"  ?SyncProperty@LabelNumberTicker@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z
"0x82804048"  ?Run@App@@QAAXXZ
"0x828040A8"  ?_GetEstablisherFrame@@YAPAKPAKPBU_s_FuncInfo@@@Z
"0x828040D0"  ?_ExecutionInCatch@@YAHPAU_xDISPATCHER_CONTEXT@@PBU_s_FuncInfo@@@Z
"0x82804158"  ?__FrameUnwindToEmptyState@@YAXPAKPAU_xDISPATCHER_CONTEXT@@PBU_s_FuncInfo@@@Z
"0x82804248"  ?__SehTransFilter@@YAHPAU_EXCEPTION_POINTERS@@PAUEHExceptionRecord@@PAKPAU_CONTEXT@@PAUEHRegistrationNode@@@Z
"0x82804388"  ?_GetRangeOfTrysToCheck@@YAPBU_s_TryBlockMapEntry@@PAKPBU_s_FuncInfo@@HPAI2PAU_xDISPATCHER_CONTEXT@@IIPAI@Z
```

### 4. DataArraySongInfo — 12 entries to delete

```
"0x8277E888"  ??5@YAAAVBinStream@@AAV0@AAUTrackChannels@@@Z
"0x8277E8E8"  ??$__uninitialized_copy@PBUTrackChannels@@PAU1@@stlpmtx_std@@YAPAUTrackChannels@@PBU1@0@Z
"0x8277E9F0"  ?_M_erase@?$vector@UTrackChannels@@V?$StlNodeAlloc@UTrackChannels@@@stlpmtx_std@@@stlpmtx_std@@QAAXPAUTrackChannels@@0@Z
"0x8277EA68"  ??$__uninitialized_fill_n@PAUTrackChannels@@IU1@@stlpmtx_std@@YAPAUTrackChannels@@PAU1@IU1@@Z
"0x8277EB08"  ??$_M_allocate_and_copy@PBUTrackChannels@@@?$vector@UTrackChannels@@V?$StlNodeAlloc@UTrackChannels@@@stlpmtx_std@@@stlpmtx_std@@QAAPAUTrackChannels@@IPBUTrackChannels@@0@Z
"0x8277EBA8"  ?_M_fill_insert_aux@?$vector@UTrackChannels@@V?$StlNodeAlloc@UTrackChannels@@@stlpmtx_std@@@stlpmtx_std@@QAAXPAUTrackChannels@@IABU1@@Z
"0x8277EE18"  ?_M_insert_overflow_aux@?$vector@UTrackChannels@@V?$StlNodeAlloc@UTrackChannels@@@stlpmtx_std@@@stlpmtx_std@@QAAXPAUTrackChannels@@ABU1@@Z
"0x8277EFA8"  ?reserve@?$vector@UTrackChannels@@V?$StlNodeAlloc@UTrackChannels@@@stlpmtx_std@@QAAXI@Z
"0x8277F050"  ?_M_fill_insert@?$vector@UTrackChannels@@V?$StlNodeAlloc@UTrackChannels@@@stlpmtx_std@@QAAXPAUTrackChannels@@IABU1@@Z
"0x8277F0C0"  ?push_back@?$vector@URecurseInfo@@V?$StlNodeAlloc@URecurseInfo@@@stlpmtx_std@@QAAXABURecurseInfo@@@Z
"0x8277FCF0"  ?resize@?$vector@UTrackChannels@@V?$StlNodeAlloc@UTrackChannels@@@stlpmtx_std@@QAAXI@Z
"0x8277FD78"  ??$?5UTrackChannels@@V?$StlNodeAlloc@UTrackChannels@@@stlpmtx_std@@@stlpmtx_std@@YAAAVBinStream@@AAV1@AAU?$_Rb_tree_node@U?$pair@IVTrackChannels@@@stlpmtx_std@@@1@@Z
```

### 5. DataPointMgr — 12 entries to delete

```
"0x827A8318"  ?GameModeTerminate@@YAXXZ
"0x827A8368"  ??4NetLoaderRef@@QAAAAU0@ABU0@@Z
"0x827A83D8"  ?IsReady@NetCacheMgr@@QBA_NXZ
"0x827A8418"  ?IsLocalFile@NetCacheMgr@@QBA_NPBD@Z
"0x827A8460"  ?SetFail@NetCacheMgr@@IAAXW4NetCacheMgrFailType@@@Z
"0x827A85A0"  ??0?$pair@$$CBVString@@VChallengeBadgeInfo@@@stlpmtx_std@@QAA@ABVString@@ABVChallengeBadgeInfo@@@Z
"0x827A85F8"  ?IsUnloadStateDone@NetCacheMgr@@ABA_NXZ
"0x827A8758"  ??$_Copy_Construct@UMotdData@MainMenuPanel@@@stlpmtx_std@@YAXPAUMotdData@MainMenuPanel@@ABQAU12@@Z
"0x827A87C8"  ?_M_create_node@?$list@UMotdData@MainMenuPanel@@V?$StlNodeAlloc@UMotdData@MainMenuPanel@@@stlpmtx_std@@@stlpmtx_std@@QAAPAUMotdData@MainMenuPanel@@ABU23@@Z
"0x827A8900"  ?DeleteNetCacheLoader@NetCacheMgr@@QAAXPAVNetCacheLoader@@@Z
"0x827A8940"  ?DeleteNetLoader@NetCacheMgr@@QAAXPAVNetLoader@@@Z
"0x827A8A08"  ?insert@?$list@UMotdData@MainMenuPanel@@V?$StlNodeAlloc@UMotdData@MainMenuPanel@@@stlpmtx_std@@QAAPAUMotdData@MainMenuPanel@@PAU23@ABU23@@Z
```

### Recommended batch order for remaining CLEANUP-SAFE units

After the top-5 above (removes 78 orphans), continue in order of orphan count.
Only units with ≥5 orphans are listed here as priority; units with 1–4 orphans
can be batched in a single pass.

| Unit | Orphans | Notes |
|---|---|---|
| Utl | 82 | ALL-FOREIGN; large but entirely scattered |
| TexBlender | 57 | AO/mesh helpers; all foreign |
| Dir | 39 | ObjectDir class orphans; Dir.cpp doesn't own ObjectDir |
| HamCamTransform | 26 | EyeDesc/DistEntry/NavItem all foreign |
| CharClipSet | 23 | String/Weight/MoveReplacer all foreign |
| DirLoader | 22 | Object/Symbol/DataNode all STL-attributed foreign |
| Mic | 21 | MicXbox/MicManagerXbox/ChatBuffer all foreign |
| Env_NG | 19 | NgRnd/NgLight all foreign |
| EventTrigger | 18 | PropTriggerDefn/RndShaderProgram all foreign |
| MeshAnim | 17 | RndMultiMesh/RndShaderMgr all foreign |
| MemHeap | 15 | String/FixedString all foreign |
| CubeTex | 15 | DxCubeTex/DxMultiMesh all foreign |
| Console | 14 | RndMultiMesh(10) all foreign |
| MeshDeform | 14 | LeaderboardRow/Viewport all foreign |
| DataPointMgr | 12 | NetCacheMgr/MotdData all foreign |
| Anim | 11 | RndDir/ObjectDir all foreign |
| WaveFile | 11 | CartRow/CuePoint all foreign |
| UIListDir | 11 | LabelSort/WidgetDrawSort all foreign |

---

## INVESTIGATE-MIS-PIN — 9 Key Cases

These units have orphan VAs **clustered at the end of the unit's split range** adjacent to a
neighboring unit whose class matches the orphan names.  This is the
MidiInstrument/BandIKEffector class of bug: the split boundary is incorrectly drawn, so some
functions that properly belong to the next unit are captured inside this unit's range.

**These are NOT just hygiene — they block the neighboring unit's pairing and can cause
objdiff to read 0% for those functions in BOTH units.**

| Unit | Orphans | Adjacent class | VAs | Action |
|---|---|---|---|---|
| **AsyncFileHolmes** | 18 | MusicLibrary (12) | `0x82527920`–`0x825285D0` IN AsyncFileHolmes range; MusicLibrary range starts at `0x82528C50` | Split boundary wrong: 12 MusicLibrary functions land 0x6730 before the pinned MusicLibrary start. Map entries should be in MusicLibrary's map entries OR the AsyncFileHolmes split should end at `0x82527920`. |
| **MidiParser** | 6 | MidiParserMgr (5) | `0x827C5E38`–`0x827C6270` at tail of MidiParser `0x827C1218`–`0x827C62D0`; MidiParserMgr starts `0x827C62D0` | 5 MidiParserMgr methods (`SetMidiReader`, `FinishLoad`, `Reset`×2, `Poll`) in the last 0x498 bytes of MidiParser range. MidiParserMgr.obj defines them. Classic split over-extension: MidiParser split should end at `0x827C5E38`. |
| **UIGuide** | 12 | LabelNumberTicker (6) | `0x82802F30`–`0x828036E8` in UIGuide `0x82801070`–`0x82804770`; LabelNumberTicker's own split is tiny stub at `0x82582F78` (0x58 bytes) | LabelNumberTicker's main body is inside UIGuide's range. UIGuide.obj defines 0 LabelNumberTicker symbols; LabelNumberTicker.obj defines 87. Likely LabelNumberTicker.cpp was merged with UIGuide.cpp in the oracle, needs a re-pin of LabelNumberTicker to this range. |
| **Debug** | 14 | Timer (1), DataArray (6) | `Timer::StopLog` at `0x824FE2D0` near Debug range tail `0x824FE310` | Mostly STL/DataArray helpers mis-attributed to Debug's range (DataArray/DataNode/Symbol are `#include` sites, not Debug methods). Purge all 14 except verify Timer entry. |
| **keygen_xbox** | 3 | ByteGrinder (2) | `0x827090A8`–`0x82709128` at tail of keygen_xbox `0x82706A20`–`0x827091A8`; ByteGrinder has its own split | 2 `ByteGrinder::pickOneOf32` methods at the very end of keygen_xbox range. ByteGrinder's own range is `0x82709848` (not adjacent). These may be genuinely keygen's cross-TU callee inlines OR a short over-extension. |
| **MidiSynth** | 5 | StreamNull (2) | `0x826FBD28`–`0x826FBD40` at tail of MidiSynth `0x826FAAA8`–`0x826FBD98`; StreamNull starts `0x826FBD98` | StreamNull::IsFinished/Resync at the exact boundary. MidiSynth's range should end at `0x826FBD28`. |
| **Task** | 4 | DataNode (3) | `0x82725988`–`0x82725CF8` at tail of Task `0x82724FE8`–`0x82725DA8` | DataNode ctors at the tail. Task.obj doesn't define DataNode; DataNode has its own split at `0x82725DA8`. Likely Task split is over-extended by 0x1C0 bytes. |
| **UI** | 4 | PanelDir (3) | `0x827E0ED0`–`0x827E1390` in UI `0x827DF8B8`–`0x827E1458`; PanelDir starts `0x827E1458` | PanelDir::EnableComponent/DrawShowing/Save at the tail of UI. UI.obj defines 0 PanelDir symbols. UI split over-extended. |
| **VocalTrack** | 6 | Gem (2) | `0x82B78F10`–`0x82B79008` near tail of VocalTrack `0x82B727B8`–`0x82B7A2A0`; Gem range starts `0x82B7A2A0` | Gem::OnScreen and Gem::~Gem at the tail. VocalTrack split should end earlier. |

### Recommended Investigation Steps (per unit)

1. Run `python3 tools/map_lint.py --check obj_orphan --unit <UnitName>` to confirm the
   specific orphan VAs.
2. In the `.s` artifact for the unit, check what function is at the orphan VA to confirm
   it matches the symbol name.
3. If confirmed mis-pin: either (a) shrink the split range in `splits.txt` to exclude
   the orphan VAs, or (b) move the map entries to the correct unit's range if they
   should be re-attributed.
4. After any `splits.txt` change: `python3 configure.py && ninja && fresh_report.sh`.
5. Judge by matched_functions staying at 6932 or **increasing** (a true mis-pin fix
   can unlock previously-blocked pairings in both units).

**Highest-value mis-pin: MidiParser/MidiParserMgr.**  Five MidiParserMgr methods are
inside MidiParser's range; MidiParserMgr.obj defines all five.  Shrinking MidiParser's
split from `0x827C62D0` to `0x827C5E38` and re-attributing those 5 map entries to
MidiParserMgr could unlock pairings in both units simultaneously.

**Second-highest: AsyncFileHolmes/MusicLibrary.**  12 MusicLibrary methods at VAs
`0x82527920`–`0x825285D0` are deep inside the AsyncFileHolmes range but belong to
MusicLibrary.  This is NOT just a boundary issue — the gap between the orphan VAs
(`0x82527920`) and the MusicLibrary split start (`0x82528C50`) is `0xD30` bytes, suggesting
the oracle's MusicLibrary range was itself pinned too conservatively.  The entire block
`0x82527920`–`0x82528C50` may need to be moved from AsyncFileHolmes to MusicLibrary.

---

## DO-NOT-TOUCH — Own-Class Orphans (13 units)

These units have orphan map entries where the **class name matches the unit's own class**.
The symbol is defined in the map at the correct address, but the compiled obj doesn't define
it yet because the source is partially ported (method bodies not yet written).  Removing
these would create a permanent gap — these **will pair once the source is completed**.

| Unit | Own-class orphans | Example |
|---|---|---|
| BandCamShot | 7 | `?SetFrameEx@BandCamShot@@MAAXMM@Z` |
| GranularSynth | 4 | `?ExtractGranules@GranularSynth@Synapse@DSP@@QAAXXZ` |
| ExternalMic | 3 | `?gatherGainAttribs@ExternalMic@@QAAJK@Z` |
| PitchDetector | 3 | `?Detect@PitchDetector@Synapse@DSP@@QAAXI@Z` |
| PitchCorrectedVoice | 3 | `??0PitchCorrectedVoice@Synapse@DSP@@QAA@XZ` |
| PeakDetector | 3 | `??0PeakDetector@Synapse@DSP@@QAA@ABV?$vector@M...` |
| SpectralAnalysis | 2 | `?Analyze@SpectralAnalysis@DSP@@QAAXPBMPAM@Z` |
| PitchShiftEffect | 2 | `??0PitchShiftEffect@@QAA@XZ` |
| SynapseAPO | 2 | `?OnSetParameters@SynapseAPO@DSP@@EAAXABUSynapseAPOParams@2@@Z` |
| HeadsetXferEffect | 2 | `?DoProcess@HeadsetXferEffect@@UAAXABUHeadsetXferEffectParams@@PIAMII@Z` |
| BandDirector | 1 | `?SendCurWorldMsg@BandDirector@@IAAXVSymbol@@_N@Z` |
| FIFOSampleBuffer | 1 | `?ptrBegin@FIFOSampleBuffer@soundtouch@@UBAPAMXZ` |
| FftIpp | 1 | `?FftReal@FftIpp@@QAAXPIBMPIAM1@Z` |

Note: BandCamShot (7 own-class) also has 20 foreign-class orphans that ARE safe to remove
independently; only skip the 7 own-class ones.  BandDirector (1 own-class) similarly has 9
foreign orphans that are safe to remove.

---

## Remaining 139+ CLEANUP-SAFE Units (1–4 orphans each)

All units below are ALL-FOREIGN (no own-class orphans) and CLEANUP-SAFE.  They can be
batched in a single map-edit pass.  Full list with representative orphan class:

```
VocalTrack(4), Spotlight(4), MidiReader(4), Synapse_dsp(4), ShaderMgr(4), rtti(4),
MatAnim(4), MoviePanel(4), Accomplishment(4), VocalPlayer(4), AccomplishmentProgress(4),
BandCharacter(3), BandCharDesc(3), FxSendDistortion(3), FxSendChorus(3), CharSleeve(3),
CharLipSync(3), PropKeys(3), SongInfoCopy(3), Rnd_Xbox(3), Cache_Xbox(3),
FxSendMeterEffect(3), MetaMusicScene(3), Shockwave(3), AccomplishmentTourConditional(3),
ShaderOptions(2), VocalTrackDir(2), SHA1(2), BlockMgr(2), FxSendEQ(2), NetCacheMgr_Xbox(2),
FilterCoeffs(2), Rnd(2), Flow(2), MoveMgr(2), Draw(2), Joypad_Xbox(2), SpotlightDrawer(2),
Memcard_Xbox(2), PartAnim(2), Profile(2), PostProc_NG(2), Archive(2), FxSendSynapse(2),
JoypadMsgs(2), UITransitionHandler(2), Singer(2), GemTrack(2), Stats(2),
MusicLibrary(1), CalibrationPanel(1), Rot(1), Rand(1), AsyncFile(1), VelocityBuffer(1),
TexBlendController(1), LightHue(1), CharFaceServo(1), MapFile_Xbox(1), SoftParticleBuffer(1),
CDReader(1), ContentMgr_Xbox(1), FIRFilter(1), TextFileStream(1), CharBoneDir(1),
Synth(1), Font(1), FileMerger(1), CharHair(1), CharUtl(1), CharBonesSamples(1),
Tex(1), SongMgr(1), File(1), Joypad(1), StreamPlayer(1), ErrorNode(1),
DataEventList(1), SoundTouch(1), Cheats(1), ChunkStream(1), MidiParserMgr(1),
PanelDir(1), AllocInfo(1), OnlineID(1), FxSendPitchShift(1), FxSendCompress(1),
MicNull(1), FxSendReverb(1), MovieSys(1), CharIKScale(1), MidiInstrument(1),
CharBone(1), TexRenderer(1), Group(1), FlowCommand(1), PhysicsVolume(1), ColorPalette(1),
DirUnloader(1), StorePanel(1), Chunks(1), UIEvent(1), NetSync(1), Gem(1)
```

Use `python3 tools/map_lint.py --check obj_orphan --unit <name> --json /tmp/unit_lint.json`
to get the exact map keys for any individual unit before editing.

---

## DO-NOT-TOUCH — C Library Units (28 units, no compiled obj)

The following units have no compiled `.obj` in `build/45410914/src/`:

```
aes.c, arraylist.c, bitrate.c, bitwise.c, block.c, codebook.c, crc32.c,
crypt.c, ctr.c, DataFlex.c, deflate.c, floor0.c, floor1.c, framing.c,
inflate.c, info.c, json_object.c, json_tokener.c, linkhash.c, lsp.c,
mapping0.c, mdct.c, printbuf.c, psy.c, res0.c, sharedbook.c, smallft.c, window.c
```

These are zlib, tomcrypt, libjansson, and Ogg/Vorbis C library files compiled with special
flags.  The obj_orphan check can't run without a built obj and emits a sentinel finding.
Leave alone until these units are built and matched independently.

---

## Execution Recommendation

**Wave-2 close analogy:** The BandIKEffector cleanup was 12 entries, 0 count change,
executed in one commit.  The full CLEANUP-SAFE backlog here is **911 entries across 157
units** — that's ~75x the precedent, so it should be broken into logical batches:

1. **Batch A (highest-value hygiene, ~66 entries):** NetworkSocket_Win(13) + System(29) +
   UIGuide(12) + DataArraySongInfo(12) = one commit, verify 6932 stable.
2. **Batch B (investigate first, ~72 entries):** Investigate-mis-pin units — start with
   MidiParser/MidiParserMgr split shrink, then AsyncFileHolmes/MusicLibrary re-pin.
   These can **increase** matched_functions if the split fix works.
3. **Batch C (remaining large units, ~400 entries):** Utl(82) + TexBlender(57) + Dir(39)
   + StreamNull(27) + HamCamTransform(26) + CharClipSet(23) + DirLoader(22) + Mic(21) +
   Env_NG(19) + EventTrigger(18) + MeshAnim(17) + MemHeap(15) + CubeTex(15) + Console(14)
   + MeshDeform(14) — one large commit after A and B verify.
4. **Batch D (tail, ~445 entries):** All remaining 1–12 orphan units in a single pass.

Total expected outcome: **6932 → 6932** (pure hygiene; unit fuzzy improves without count
change).  The INVESTIGATE cases in Batch B have potential to **increase** matched_functions.

---

*Generated by mechanical analysis on 2026-06-11.*  
*Tool: `tools/map_lint.py --check obj_orphan --json /tmp/map_lint_orphans.json`*  
*All source paths and obj paths verified against live build artifacts.*
