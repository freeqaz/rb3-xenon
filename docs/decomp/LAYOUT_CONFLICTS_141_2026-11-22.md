# The 141 class-layout "conflicts" from the Ghidra sync: all artifacts (2026-11-22)

Lane `odr141`, branch `odr141`, worktree `~/tmp/wt-odr141`, base `997e35516`.

## Question

The Ghidra sync (`docs/plans/ghidra-data-sync-2026-11-22.md`, phase 4) ran
`tools/ghidra/emit_class_layouts.py` over src/band3 plus the 17 src/system dirs native
compiles (900 TUs, one `/d1reportAllClassLayout` compile each, through
`scripts/harvest/class_layout_report.py`) and dropped 141 names whose TUs disagreed on
size or member offsets (`~/tmp/ghidra-sync/layouts.json` `conflicts`). Among them were
real classes: RndCam, SongStatusMgr, RockCentral, LightPreset, RndMultiMesh. A real
disagreement would be a memory-corruption bug in both builds, yet `tools/layout_odr.py`
(which the native gate runs) passes. Which is right?

## Answer

**No real layout disagreement among the 141.** layout_odr is right; the emitter was wrong.
Every name is one of:

| Cause | Names | Where the fault is |
|---|---:|---|
| Compiler-warning splice (emitter/class_layout_report bug) | 101 | `class_layout_report.build_command` did not pass `/w` |
| Different classes sharing a bare name | 33 | inherent to MSVC's report; the emitter compared by bare name |
| Template patterns / template-nested names | 5 | `size(0)` uninstantiated patterns; per-instantiation nested names |
| `<unnamed-tag>` placeholder | 1 | not a class name |
| PracticeSection: two classes (DC3 hamobj vs RB3 band3) | 1 | known; PIN entry in `config/45410914/layout_odr_allow.json` |

No source change was needed, so there is nothing for `ab_measure` to price and no retail-byte
adjudication was required (no name had two layouts of one class). layout_odr misses none
of these; see "Does layout_odr miss anything" below.

## How it was measured

1. **Fresh per-TU layouts without a compile.** layout_odr's cache
   (`~/.cache/rb3-layout-odr`) holds, per TU, every printed class name with a fingerprint
   (sha of the compiler's own block text, compiled with `/w`) and the block text, keyed on
   the hashes of the TU's full include closure. Looked up against main `997e35516`: 1,267 of
   1,268 TUs were current (the miss is today's new `AuditionMgr.cpp`, outside the 141's story).
   Restricted to the emitter's 900 TUs.
2. **Per name, count distinct layout texts.** 101 of the 141 have **one** layout text in every
   TU that prints them (RndCam: 1 text in 331 TUs). Parsing that one text with the emitter's own
   parser (`clr.parse`) gives one signature, so the emitter could only have seen two signatures if
   its *input text* differed from layout_odr's. 39 names have 2-15 texts; `<unnamed-tag>` has none
   (layout_odr skips it).
3. **The splice, caught in the act.** Re-ran the emitter's exact compile path
   (`clr.run_report(..., all_classes=True)`) on 6 TUs and diffed each class's signature against
   the clean `/w` block. Without `/w`, warnings are printed into the same stdout stream as the
   report, **mid-line**:
   ```
   BandCamShot::$vftable@RndAnimatablsrc/system\obj/ObjMacros.h(45) : warning C4005: 'OBJ_SET_TYPE' : macro redefinition
   ```
   (ObjMacros.h re-defines Object.h's HANDLE/SYNC_PROP/... macros; C4005 fires once per macro.)
   The broken vftable header no longer matches `RE_VFT`, so its slots (`0 | &RndAnimatable::Loop`)
   parse as **members of BandCamShot**. Elsewhere a splice lands inside a member row and the member
   is renamed: in SongStatusMgr.cpp's report, `RockCentral`'s `4 | mSinks` read as `pragma`; in
   BandCamShot.cpp, `_List_base<RndAnimatable*>`'s `_M_node` read as `redefinition`. Where the splice
   lands depends on the TU's include order, so the same class reads differently from TU to TU.
   4 of the 101 were caught directly in those 6 TUs (RockCentral, BandCamShot,
   `_List_base<RndAnimatable*>`, `_List_iterator<BSPFace,const>`); the rest are shown by step 2
   (identical text everywhere) and by the re-run below.
4. **The 39 multi-text names attributed to definitions** with layout_odr's own
   `classify_x360` (each layout tied to the definition whose body holds its printed members,
   among the definitions the TU includes): 33 COLLISION (each layout a different qualified class),
   5 TEMPLATE, 1 SPLIT (PracticeSection, the reviewed DC3-vs-RB3 pin; both fingerprints equal the
   pin's). Spot-checked by hand: every "global" identity in the collisions (`Node` =
   band3 `SongSortNode.h`, `Line` = `rndobj/Graph.h`, `Pose` = `hamobj/Pose.h`, `TrackData` =
   `beatmatch/MasterAudio.h`, `GemInProgress` = `beatmatch/TrackWatcherImpl.h`, `RGGemInfo` =
   `beatmatch/GemInfo.h`, `CharData` = `meta_band/CharData.h`, `Weight` = `math/Key.h`) has exactly
   one definition, and its rival is a nested class of the same bare name. `DebugGraph`'s second
   layout is Synth.cpp's anonymous-namespace struct (seen in the TUs that scatter-include
   Synth.cpp), TU-local by construction. `Params` is seven different `XxxEffect::Params`.

## Fixes (tools only)

On branch `odr141`:

- `scripts/harvest/class_layout_report.py`: `build_command` adds `/w` (after the `cl.exe`
  element, in every mode), and `run_report` cuts `cl : Command line warning ...` lines (the
  driver's D9025 survives `/w`). This is what layout_odr has done since 2026-10-06 (its parse
  trap 1). `/w` does not silence errors, so `diagnostics()` and the C2079 sizeof probes are
  unaffected. **Every caller of class_layout_report inherits the fix**: `/struct-info`, the
  header audits, `sizeof_probe_census`, `retail_sizeof_witness`, the MCP server. Until now any of
  them could have read a vtable slot as a member, or a warning word as a member name, in a TU
  that includes ObjMacros.h after Object.h; the 2026-07-30 single-class mode was exposed the same
  way, though a single class is spliced only if a warning fires while it prints.
- `tools/ghidra/emit_class_layouts.py`:
  - writes `conflict_detail` (every layout of a dropped name: size, first members, TU count,
    first TUs) so the next "N conflicts" can be adjudicated without re-running anything;
  - drops a name the report prints **twice with different blocks inside one TU** (`clr.parse()`
    keys by bare name and kept the last one, so a name could be kept with the wrong class's layout
    whenever every TU printed the same one last);
  - skips `<unnamed-tag>` and `size(0)` blocks (uninstantiated template patterns), as layout_odr
    does.
  It still compares by bare name, so the 33 collisions stay dropped. That is the safe outcome for
  Ghidra: the loader cannot tell `BandCamShot::Target` from `HamCamShot::Target` by the printed
  name either.

## Re-run with the fixed tools (the falsification test)

Prediction before the run: the 101 single-text names and `<unnamed-tag>` disappear, the 39
multi-text names stay, and the intra-TU check adds a handful of nested bare names.

`python3 tools/ghidra/emit_class_layouts.py --project-dir ~/tmp/wt-odr141 --out
~/tmp/odr141_layouts_fixed.json -j 12` (log `~/tmp/odr141_emit_fixed.log`; 901 TUs, since
`AuditionMgr.cpp` landed after the original run; 0 failed):

- **16,549 classes kept, 49 dropped** (was 16,458 / 141).
- All 102 predicted names are gone; all 39 multi-text names remain; 0 surprises in either set.
- 10 new drops, all intra-TU duplicates the old emitter had KEPT (and phase 4 put into Ghidra)
  with whichever block printed last: `Callback`, `CharInfo`, `DeferOwner`, `EventHandler`,
  `EventSink`, `EventSinkElem`, `FingerStep`, `Sink`, `Vert`, `_Buffers`. Each is a bare nested
  name printed twice with different layouts in one TU (e.g. `Vert`: `RndMesh::Vert` and another
  `Vert` in the same report; `Sink`/`EventSink`: MsgSource's nested types beside a same-named
  one). The kept layout was consistent across TUs only because the same one printed last
  everywhere, so a Ghidra class of that simple name may carry the other class's members.
- Classes kept by both runs with a different signature: 2 of 16,320. `AuditionMgr` (the TU
  changed: audmgr landed at 12:51, after the original run) and
  `?$_Ht_iterator@V?$_Slist_iterator@U?$pair@$$CBVSymbol@@PAVUI...` whose only member read
  `pragma` in the old run, i.e. one class went into Ghidra spliced.

## Does layout_odr miss anything?

Not here: each of the 141 is either one layout everywhere (101), or a layout_odr COLLISION /
TEMPLATE / allowlisted SPLIT whose attribution was checked above. Its two known passes-without-proof
remain as documented in the tool: TEMPLATE (template-nested classes MSVC prints by bare name
cannot be tied to an instantiation, counted not checked) and `size(0)` blocks (skipped). Neither
hid a split in this set. layout_odr was not modified.

## Not done

- No source edits (none needed), so no `ab_measure` run and no native gate run.
- The Ghidra program was not re-synced. Phase 4 applied layouts from the spliced run; see the
  re-run section for how many kept classes changed. Re-running
  `tools/ghidra/ghidra_sync.sh --layouts <new layouts.json>` after this lands is the
  coordinator's call (it swaps the live :8002 program).
- `class_layout_report.py --selftest` was not run (box at load ~165); the 3-TU emitter smoke test
  and the full 901-TU re-run exercised the changed path.

## Full table

| # | Name (as MSVC prints it) | Verdict | Cause / evidence |
|---|---|---|---|
| 1 | `<unnamed-tag>` | ARTIFACT | placeholder name of every anonymous struct/union (the emitter keyed on it); layout_odr skips it as TU-local |
| 2 | `?$ObjDirItr@VObject@Hmx@@` | ARTIFACT | warning splice: one layout text in all 19 TUs of the /w compile |
| 3 | `?$StlNodeAlloc@U?$_Rb_tree_node@U?$pair@$$CBVSymbol@@P6A?AVDataNode...` | ARTIFACT | warning splice: one layout text in all 99 TUs of the /w compile |
| 4 | `?$_Alloc_traits@PAVRemoteUser@@V?$StlNodeAlloc@PAVRemoteUser@@@stlp...` | ARTIFACT | warning splice: one layout text in all 6 TUs of the /w compile |
| 5 | `?$_Alloc_traits@U?$_Rb_tree_node@U?$pair@$$CBVSymbol@@M@stlpmtx_std...` | ARTIFACT | warning splice: one layout text in all 2 TUs of the /w compile |
| 6 | `?$_Alloc_traits@UCamCatEntry@@V?$StlNodeAlloc@UCamCatEntry@@@stlpmt...` | ARTIFACT | warning splice: one layout text in all 14 TUs of the /w compile |
| 7 | `?$_Alloc_traits@UCompEv@DataEventList@@V?$StlNodeAlloc@UCompEv@Data...` | ARTIFACT | warning splice: one layout text in all 72 TUs of the /w compile |
| 8 | `?$_Alloc_traits@UGroupDrawDist@@V?$StlNodeAlloc@UGroupDrawDist@@@st...` | ARTIFACT | warning splice: one layout text in all 10 TUs of the /w compile |
| 9 | `?$_Alloc_traits@V?$_List_node@PAVRndDrawable@@@stlpmtx_std@@V?$StlN...` | ARTIFACT | warning splice: one layout text in all 38 TUs of the /w compile |
| 10 | `?$_Alloc_traits@VOldColorOption@@V?$StlNodeAlloc@VOldColorOption@@@...` | ARTIFACT | warning splice: one layout text in all 2 TUs of the /w compile |
| 11 | `?$_Alloc_traits@VVector2@@V?$StlNodeAlloc@VVector2@@@stlpmtx_std@@` | ARTIFACT | warning splice: one layout text in all 550 TUs of the /w compile |
| 12 | `?$_ConstNonLocalHashMapTraitsT@U?$pair@$$CBVSymbol@@PAVCampaignLeve...` | ARTIFACT | warning splice: one layout text in all 3 TUs of the /w compile |
| 13 | `?$_Const_traits@VBSPFace@@` | ARTIFACT | warning splice: one layout text in all 17 TUs of the /w compile |
| 14 | `?$_Ht_iterator@V?$_Slist_iterator@U?$pair@$$CBVSymbol@@V1@@stlpmtx_...` | ARTIFACT | warning splice: one layout text in all 7 TUs of the /w compile |
| 15 | `?$_List_base@PAVRndAnimatable@@V?$StlNodeAlloc@PAVRndAnimatable@@@s...` | ARTIFACT | warning splice: one layout text in all 26 TUs of the /w compile |
| 16 | `?$_List_base@PAVRndMat@@V?$StlNodeAlloc@PAVRndMat@@@stlpmtx_std@@` | ARTIFACT | warning splice: one layout text in all 33 TUs of the /w compile |
| 17 | `?$_List_iterator@PAVRndDrawable@@U?$_Nonconst_traits@PAVRndDrawable...` | ARTIFACT | warning splice: one layout text in all 38 TUs of the /w compile |
| 18 | `?$_List_iterator@U?$pair@VTimer@@VTimerStats@@@stlpmtx_std@@U?$_Non...` | ARTIFACT | warning splice: one layout text in all 7 TUs of the /w compile |
| 19 | `?$_List_iterator@VBSPFace@@U?$_Const_traits@VBSPFace@@@stlpmtx_std@@` | ARTIFACT | warning splice: one layout text in all 17 TUs of the /w compile |
| 20 | `?$_MapTraitsT@U?$pair@$$CBVSymbol@@P6A?AVDataNode@@PAVDataArray@@@Z...` | ARTIFACT | warning splice: one layout text in all 99 TUs of the /w compile |
| 21 | `?$_Nonconst_traits@PAUCompressTexDesc@Rnd@@` | ARTIFACT | warning splice: one layout text in all 6 TUs of the /w compile |
| 22 | `?$_Nonconst_traits@VBSPFace@@` | ARTIFACT | warning splice: one layout text in all 18 TUs of the /w compile |
| 23 | `?$_Nonconst_traits@VSongRanking@BandSongMgr@@` | ARTIFACT | warning splice: one layout text in all 2 TUs of the /w compile |
| 24 | `?$_Rb_tree@HU?$less@H@stlpmtx_std@@U?$pair@$$CBHM@2@U?$_Select1st@U...` | ARTIFACT | warning splice: one layout text in all 389 TUs of the /w compile |
| 25 | `?$_Rb_tree@UEdge@RndAmbientOcclusion@@U?$less@UEdge@RndAmbientOcclu...` | ARTIFACT | warning splice: one layout text in all 5 TUs of the /w compile |
| 26 | `?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@U?$pair@$$CBVSym...` | ARTIFACT | warning splice: one layout text in all 99 TUs of the /w compile |
| 27 | `?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@U?$pair@$$CBVSym...` | ARTIFACT | warning splice: one layout text in all 57 TUs of the /w compile |
| 28 | `?$_Rb_tree_base@U?$pair@$$CBVSymbol@@VDataNode@@@stlpmtx_std@@V?$St...` | ARTIFACT | warning splice: one layout text in all 57 TUs of the /w compile |
| 29 | `?$_Rb_tree_iterator@U?$pair@$$CBHM@stlpmtx_std@@U?$_ConstMapTraitsT...` | ARTIFACT | warning splice: one layout text in all 374 TUs of the /w compile |
| 30 | `?$_STLP_alloc_proxy@PAPADPADV?$StlNodeAlloc@PAD@stlpmtx_std@@` | ARTIFACT | warning splice: one layout text in all 596 TUs of the /w compile |
| 31 | `?$_STLP_alloc_proxy@PAPAUDep@CharPollableSorter@@PAU12@V?$StlNodeAl...` | ARTIFACT | warning splice: one layout text in all 3 TUs of the /w compile |
| 32 | `?$_STLP_alloc_proxy@PAPAVUIComponent@@PAV1@V?$StlNodeAlloc@PAVUICom...` | ARTIFACT | warning splice: one layout text in all 2 TUs of the /w compile |
| 33 | `?$_STLP_alloc_proxy@PAUCamCatEntry@@U1@V?$StlNodeAlloc@UCamCatEntry...` | ARTIFACT | warning splice: one layout text in all 14 TUs of the /w compile |
| 34 | `?$_STLP_alloc_proxy@U_List_node_base@stlpmtx_std@@V?$_List_node@PAV...` | ARTIFACT | warning splice: one layout text in all 26 TUs of the /w compile |
| 35 | `?$_STLP_alloc_proxy@U_List_node_base@stlpmtx_std@@V?$_List_node@PAV...` | ARTIFACT | warning splice: one layout text in all 38 TUs of the /w compile |
| 36 | `?$_STLP_alloc_proxy@U_List_node_base@stlpmtx_std@@V?$_List_node@PAV...` | ARTIFACT | warning splice: one layout text in all 33 TUs of the /w compile |
| 37 | `?$_STLP_alloc_proxy@U_Rb_tree_node_base@stlpmtx_std@@U?$_Rb_tree_no...` | ARTIFACT | warning splice: one layout text in all 57 TUs of the /w compile |
| 38 | `?$_Slist_iterator@U?$pair@$$CBVSymbol@@PAVLesson@@@stlpmtx_std@@U?$...` | ARTIFACT | warning splice: one layout text in all 2 TUs of the /w compile |
| 39 | `?$_Vector_base@PADV?$StlNodeAlloc@PAD@stlpmtx_std@@` | ARTIFACT | warning splice: one layout text in all 596 TUs of the /w compile |
| 40 | `?$_Vector_base@PAUEventEntry@@V?$StlNodeAlloc@PAUEventEntry@@@stlpm...` | ARTIFACT | warning splice: one layout text in all 4 TUs of the /w compile |
| 41 | `?$_Vector_base@PAVBandMachine@@V?$StlNodeAlloc@PAVBandMachine@@@stl...` | ARTIFACT | warning splice: one layout text in all 5 TUs of the /w compile |
| 42 | `?$_Vector_base@PAVFileCacheEntry@@V?$StlNodeAlloc@PAVFileCacheEntry...` | ARTIFACT | warning splice: one layout text in all 144 TUs of the /w compile |
| 43 | `?$_Vector_base@UCompEv@DataEventList@@V?$StlNodeAlloc@UCompEv@DataE...` | ARTIFACT | warning splice: one layout text in all 72 TUs of the /w compile |
| 44 | `?$_Vector_base@UObjVersion@@V?$StlNodeAlloc@UObjVersion@@@stlpmtx_s...` | ARTIFACT | warning splice: one layout text in all 833 TUs of the /w compile |
| 45 | `?$_Vector_base@VTransform@@V?$StlNodeAlloc@VTransform@@@stlpmtx_std@@` | ARTIFACT | warning splice: one layout text in all 98 TUs of the /w compile |
| 46 | `?$__move_traits@UCamCatEntry@@` | ARTIFACT | warning splice: one layout text in all 14 TUs of the /w compile |
| 47 | `?$__move_traits@UCompEv@DataEventList@@` | ARTIFACT | warning splice: one layout text in all 72 TUs of the /w compile |
| 48 | `?$__move_traits@UPanInfo@BinkClip@@` | ARTIFACT | warning splice: one layout text in all 15 TUs of the /w compile |
| 49 | `?$__move_traits@V?$vector@V?$RangedData@U?$pair@HH@stlpmtx_std@@@?$...` | ARTIFACT | warning splice: one layout text in all 6 TUs of the /w compile |
| 50 | `?$__move_traits@VTransform@@` | ARTIFACT | warning splice: one layout text in all 98 TUs of the /w compile |
| 51 | `?$list@PAVRndAnimatable@@V?$StlNodeAlloc@PAVRndAnimatable@@@stlpmtx...` | ARTIFACT | warning splice: one layout text in all 26 TUs of the /w compile |
| 52 | `?$list@PAVRndDrawable@@V?$StlNodeAlloc@PAVRndDrawable@@@stlpmtx_std@@` | ARTIFACT | warning splice: one layout text in all 38 TUs of the /w compile |
| 53 | `?$map@VSymbol@@P6A?AVDataNode@@PAVDataArray@@@ZU?$less@VSymbol@@@st...` | ARTIFACT | warning splice: one layout text in all 99 TUs of the /w compile |
| 54 | `?$set@W4ScoreType@@U?$less@W4ScoreType@@@stlpmtx_std@@V?$StlNodeAll...` | ARTIFACT | warning splice: one layout text in all 6 TUs of the /w compile |
| 55 | `?$vector@HV?$StlNodeAlloc@H@stlpmtx_std@@` | ARTIFACT | warning splice: one layout text in all 555 TUs of the /w compile |
| 56 | `?$vector@PADV?$StlNodeAlloc@PAD@stlpmtx_std@@` | ARTIFACT | warning splice: one layout text in all 596 TUs of the /w compile |
| 57 | `?$vector@UPropertyFilter@CameraManager@@V?$StlNodeAlloc@UPropertyFi...` | ARTIFACT | warning splice: one layout text in all 15 TUs of the /w compile |
| 58 | `?$vector@V?$TickedInfo@VString@@@@V?$StlNodeAlloc@V?$TickedInfo@VSt...` | ARTIFACT | warning splice: one layout text in all 8 TUs of the /w compile |
| 59 | `?$vector@VArtEntry@StoreArtLoaderPanel@@V?$StlNodeAlloc@VArtEntry@S...` | ARTIFACT | warning splice: one layout text in all 36 TUs of the /w compile |
| 60 | `?$vector@VSectionInfo@Stats@@V?$StlNodeAlloc@VSectionInfo@Stats@@@s...` | ARTIFACT | warning splice: one layout text in all 271 TUs of the /w compile |
| 61 | `?$vector@VTransform@@V?$StlNodeAlloc@VTransform@@@stlpmtx_std@@` | ARTIFACT | warning splice: one layout text in all 98 TUs of the /w compile |
| 62 | `BandCamShot` | ARTIFACT | warning splice: one layout text in all 39 TUs of the /w compile |
| 63 | `BinkClip` | ARTIFACT | warning splice: one layout text in all 15 TUs of the /w compile |
| 64 | `Buffer` | ARTIFACT | different classes sharing a bare name: `LiveCameraInput::Buffer`, `Quazal::Buffer` (2 layouts) |
| 65 | `BuildPoly` | ARTIFACT | warning splice: one layout text in all 89 TUs of the /w compile |
| 66 | `CharData` | ARTIFACT | different classes sharing a bare name: `CharData`, `WorldCrowd::CharData` (2 layouts) |
| 67 | `CharMeshCacheMgr` | ARTIFACT | warning splice: one layout text in all 4 TUs of the /w compile |
| 68 | `ContentLoadingPanel` | ARTIFACT | warning splice: one layout text in all 2 TUs of the /w compile |
| 69 | `CriticalSection` | ARTIFACT | different classes sharing a bare name: `CriticalSection`, `Quazal::CriticalSection` (2 layouts) |
| 70 | `Data` | ARTIFACT | different classes sharing a bare name: `MultiplayerAnalyzer::Data`, `Quazal::Data` (2 layouts) |
| 71 | `DataPoint` | ARTIFACT | warning splice: one layout text in all 43 TUs of the /w compile |
| 72 | `DateTime` | ARTIFACT | different classes sharing a bare name: `DateTime`, `Quazal::DateTime` (2 layouts) |
| 73 | `DebugGraph` | ARTIFACT | different classes sharing a bare name: `src/system/synth/Synth.cpp#DebugGraph`, `DebugGraph` (2 layouts) |
| 74 | `DircutEntry` | ARTIFACT | different classes sharing a bare name: `BandDirector::DircutEntry`, `HamDirector::DircutEntry` (2 layouts) |
| 75 | `Edge` | ARTIFACT | different classes sharing a bare name: `ChordShapeGenerator::Edge`, `RndAmbientOcclusion::Edge` (2 layouts) |
| 76 | `Entry` | ARTIFACT | different classes sharing a bare name: `JobQueue::Entry`, `KerningTable::Entry`, `LocalePanel::Entry` (4 layouts) |
| 77 | `GemInProgress` | ARTIFACT | different classes sharing a bare name: `GemInProgress`, `SongParser::GemInProgress` (2 layouts) |
| 78 | `HamWardrobe` | ARTIFACT | warning splice: one layout text in all 12 TUs of the /w compile |
| 79 | `Instance` | ARTIFACT | different classes sharing a bare name: `RndGenerator::Instance`, `STLPORT::RndMultiMesh::Instance` (2 layouts) |
| 80 | `Job` | ARTIFACT | different classes sharing a bare name: `Job`, `Quazal::Job` (2 layouts) |
| 81 | `JoypadMidiController` | ARTIFACT | warning splice: one layout text in all 2 TUs of the /w compile |
| 82 | `Layer` | ARTIFACT | different classes sharing a bare name: `HamDriver::Layer`, `LayerDir::Layer` (2 layouts) |
| 83 | `LightPreset` | ARTIFACT | warning splice: one layout text in all 81 TUs of the /w compile |
| 84 | `Line` | ARTIFACT | different classes sharing a bare name: `Line`, `RndText::Line` (2 layouts) |
| 85 | `Message` | ARTIFACT | different classes sharing a bare name: `Message`, `Quazal::Message` (2 layouts) |
| 86 | `NameGenerator` | ARTIFACT | warning splice: one layout text in all 9 TUs of the /w compile |
| 87 | `NgMat` | ARTIFACT | warning splice: one layout text in all 13 TUs of the /w compile |
| 88 | `NgRnd` | ARTIFACT | warning splice: one layout text in all 25 TUs of the /w compile |
| 89 | `Node` | ARTIFACT | different classes sharing a bare name: `ClipDistMap::Node`, `Node` (15 layouts) |
| 90 | `NonDestructiveTransitionEvent` | ARTIFACT | warning splice: one layout text in all 38 TUs of the /w compile |
| 91 | `Note` | ARTIFACT | different classes sharing a bare name: `MidiChannel::Note`, `MidiParser::Note` (2 layouts) |
| 92 | `PanInfo` | ARTIFACT | different classes sharing a bare name: `BinkClip::PanInfo`, `MoggClip::PanInfo` (2 layouts) |
| 93 | `Params` | ARTIFACT | different classes sharing a bare name: `src/system/synth/BitCrushEffect.h#Params`, `src/system/synth/CompressionEffect.h#Params`, `src/system/synth/DelayEffect.h#Params`, `src/system/synth/DistortionEffect.h#Params`, `src/system/synth/EQEffect.h#Params`, `src/system/synth/FlangerEffect.h#Params`, `src/system/synth/WahEffect.h#Params` (6 layouts) |
| 94 | `PatchVerts` | ARTIFACT | warning splice: one layout text in all 504 TUs of the /w compile |
| 95 | `PlayerStreakData` | ARTIFACT | different classes sharing a bare name: `PerfectOverdriveTracker::PlayerStreakData`, `PerfectSectionTracker::PlayerStreakData`, `StreakTracker::PlayerStreakData` (3 layouts) |
| 96 | `Point` | ARTIFACT | different classes sharing a bare name: `CharHair::Point`, `CharIKHead::Point`, `RndLine::Point` (3 layouts) |
| 97 | `Pose` | ARTIFACT | different classes sharing a bare name: `Pose`, `RndMorph::Pose` (2 layouts) |
| 98 | `PracticeSection` | ARTIFACT (allowlisted) | two classes: DC3 hamobj/PracticeSection.h vs RB3 band3 PracticeSection; PIN in layout_odr_allow.json (fps match) |
| 99 | `PracticeSectionProvider` | ARTIFACT | warning splice: one layout text in all 123 TUs of the /w compile |
| 100 | `PrefabMgr` | ARTIFACT | warning splice: one layout text in all 12 TUs of the /w compile |
| 101 | `ProKeysData` | ARTIFACT | warning splice: one layout text in all 5 TUs of the /w compile |
| 102 | `ProfileMgr` | ARTIFACT | warning splice: one layout text in all 87 TUs of the /w compile |
| 103 | `RGFretButtonUpMsg` | ARTIFACT | warning splice: one layout text in all 6 TUs of the /w compile |
| 104 | `RGGemInfo` | ARTIFACT | different classes sharing a bare name: `RGGemInfo`, `SongParser::RGGemInfo` (2 layouts) |
| 105 | `RangeShift` | ARTIFACT | different classes sharing a bare name: `GemTrack::RangeShift`, `VocalTrack::RangeShift` (2 layouts) |
| 106 | `Ray` | ARTIFACT | warning splice: one layout text in all 550 TUs of the /w compile |
| 107 | `RndCam` | ARTIFACT | warning splice: one layout text in all 331 TUs of the /w compile |
| 108 | `RndMatAnim` | ARTIFACT | warning splice: one layout text in all 87 TUs of the /w compile |
| 109 | `RndMultiMesh` | ARTIFACT | warning splice: one layout text in all 210 TUs of the /w compile |
| 110 | `RndMultiMeshProxy` | ARTIFACT | warning splice: one layout text in all 17 TUs of the /w compile |
| 111 | `RndOverlay` | ARTIFACT | warning splice: one layout text in all 393 TUs of the /w compile |
| 112 | `RndShaderMgr` | ARTIFACT | warning splice: one layout text in all 34 TUs of the /w compile |
| 113 | `RndTransProxy` | ARTIFACT | warning splice: one layout text in all 24 TUs of the /w compile |
| 114 | `RockCentral` | ARTIFACT | warning splice: one layout text in all 29 TUs of the /w compile |
| 115 | `SetlistMergePanel` | ARTIFACT | warning splice: one layout text in all 5 TUs of the /w compile |
| 116 | `SongStatusMgr` | ARTIFACT | warning splice: one layout text in all 31 TUs of the /w compile |
| 117 | `SpotlightEntry` | ARTIFACT | different classes sharing a bare name: `LightPreset::SpotlightEntry`, `SpotlightDrawer::SpotlightEntry` (2 layouts) |
| 118 | `Stack` | ARTIFACT | size(0) uninstantiated template pattern beside the instantiated size(12) one, both in the same TUs (math/Rot.cpp, rndobj/AmbientOcclusion.cpp) |
| 119 | `Stats` | ARTIFACT | warning splice: one layout text in all 271 TUs of the /w compile |
| 120 | `Stream` | ARTIFACT | different classes sharing a bare name: `Quazal::Stream`, `Stream` (2 layouts) |
| 121 | `String` | ARTIFACT | different classes sharing a bare name: `Quazal::String`, `String` (2 layouts) |
| 122 | `Target` | ARTIFACT | different classes sharing a bare name: `BandCamShot::Target`, `HamCamShot::Target` (2 layouts) |
| 123 | `TargetCache` | ARTIFACT | different classes sharing a bare name: `BandCamShot::TargetCache`, `HamCamShot::TargetCache` (2 layouts) |
| 124 | `Tour` | ARTIFACT | warning splice: one layout text in all 28 TUs of the /w compile |
| 125 | `TourChar` | ARTIFACT | warning splice: one layout text in all 274 TUs of the /w compile |
| 126 | `TrackData` | ARTIFACT | different classes sharing a bare name: `SongDB::TrackData`, `TrackData` (2 layouts) |
| 127 | `TrackPanelInterface` | ARTIFACT | warning splice: one layout text in all 67 TUs of the /w compile |
| 128 | `UIListSlot` | ARTIFACT | warning splice: one layout text in all 58 TUs of the /w compile |
| 129 | `UnkTU5GuidePitchOwner` | ARTIFACT | warning splice: one layout text in all 2 TUs of the /w compile |
| 130 | `Viewport` | ARTIFACT | different classes sharing a bare name: `NgRnd::Viewport` (X, Y, Width, Height, MinZ, MaxZ; 24 B) and `ObjectDir::Viewport` (obj/Dir.h, `Transform mXfm`; 64 B). layout_odr tied only the first to a definition (2 layouts) |
| 131 | `VoiceChatMgr` | ARTIFACT | warning splice: one layout text in all 20 TUs of the /w compile |
| 132 | `WaveFile` | ARTIFACT | warning splice: one layout text in all 7 TUs of the /w compile |
| 133 | `Weight` | ARTIFACT | different classes sharing a bare name: `CharLipSync::Generator::Weight`, `CharLipSync::PlayBack::Weight`, `Weight` (3 layouts) |
| 134 | `_XVIDEO_MODE` | ARTIFACT | warning splice: one layout text in all 845 TUs of the /w compile |
| 135 | `__file_modes` | ARTIFACT | warning splice: one layout text in all 891 TUs of the /w compile |
| 136 | `__file_state` | ARTIFACT | warning splice: one layout text in all 891 TUs of the /w compile |
| 137 | `const_iterator` | ARTIFACT | nested in container templates: one bare name per instantiation; both layouts print in the same TUs |
| 138 | `iterator` | ARTIFACT | different classes sharing a bare name: `ObjRef::iterator`, `RndMeshDeform::VertArray::iterator` (6 layouts) |
| 139 | `kdTreeNode` | ARTIFACT | size(0) uninstantiated template pattern beside the size(8) instance, same TUs |
| 140 | `kdTriList` | ARTIFACT | size(0) uninstantiated template pattern beside the instance, same TUs |
| 141 | `value_compare` | ARTIFACT | nested in map<K,V> templates: one bare name per instantiation; both layouts print in the same TUs |
