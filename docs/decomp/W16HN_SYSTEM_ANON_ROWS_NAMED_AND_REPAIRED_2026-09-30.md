# W16-HN — naming and repairing anonymous src/system rows our source already defines (2026-09-30)

Branch `w16-hn`, rebased onto main `835ab0289`. It applies the W16-HG/W16-HK method
(`W16HG_ANON_ROWS_WE_ALREADY_DEFINED_2026-09-30.md`, `W16HK_ANON_ROWS_NAMED_AND_REPAIRED_2026-09-30.md`)
to compiled `src/system/` units, minus `bandobj/`, `rndobj/`, `char/` and `rnddx9/`. The files
another lane was editing were not touched: `synth/{FxSendDistortion,MidiInstrument,SampleZone}`,
`ui/{InlineHelp,LabelNumberTicker,LabelShrinkWrapper,UI,UIComponent,UIList,UIScreen,UISlider}`
and `world/{CameraShot,ColorPalette,Instance,Reflection,SpotlightDrawer}`.

**Result: 177 names added to `scripts/target_symbol_map.json`, and 102 of those rows now score
fuzzy 100 (26,280 B), up from 15 at naming. Whole-binary A/B: **+155 matched functions / +28,228 B `matched_code` / +0.275475 pp code% / +0.526153 pp fuzzy, with no unit regressed**.**

## 1. Population and what was left out

On the settled tree the scope holds **2,123 anonymous fuzzy-0 rows / 322,368 B**. Three cuts were
made before any scoring:

| cut | rows / bytes | why |
|---|---|---|
| the other lane's files | (their units skipped) | brief |
| map `null` / denylisted / already mapped | — | not ours to name |
| `hamobj/` and `gesture/` units except MeterDisplay and MiniLeaderboardDisplay | ~42 KB | the pin is foreign code. HamCamTransform, MoveMgr and DepthBuffer3D match **0** functions of their own class and only generic templates, so their anonymous rows are not these classes' functions and our base objects cannot name them. This is a pin problem, not body drift. |

That leaves **1,631 targets / 248,928 B**.

**Scoring.** HK's full matrix (every candidate in a 0.33–3× size band) would have been
94,346 scratch-rename diffs here. At the ~1.6 diffs/s this box managed under load 50–70 (base objs
such as `Rot.obj` are 4.9 MB, 1.7 s per diff), that is about 16 hours. It was stopped after 2,000
scores. Each target was instead capped to its **20 shape-nearest non-template candidates** in the
same band (`anon_candidate_scorer.prefilter`), giving 22,599 scores. Template instantiations
(`?$`/`??$`/`??_`) were kept out of the pool, as in HK. That also keeps this lane clear of the
`ObjPtr`/`ObjPtrList`/`ObjRefConcrete`/`ObjOwnerPtr` families another session is naming.

**Proposals** follow HK's rule: best ≥ 70, a margin of ≥ 1 over the runner-up, bijective, and generic
helpers excluded. That gave **267 proposals / 63,820 B**.

## 2. Adjudication

`tools/anon_proposal_adjudicate.py` was run both normally and with `--independent`. The two runs
gave identical tallies: 125 SUPPORTED, 126 CONTRADICTED, 16 UNANCHORED.

The accept rule was fixed before any row was read. Mechanical accepts use W16-HI's engine rule,
because W16-HI measured a 4.5% wrong-accept rate for ≥ 1 positive in engine units and 1.1% for ≥ 2.

| bucket | rule | rows | bytes | verdict |
|---|---|---:|---:|---|
| MECH | 0 content negatives, ≥ 2 positives, runner-up ≥ 1 negative | 100 | 24,848 | accept |
| lone candidate | 0 negatives, ≥ 2 positives, no runner-up negative (e.g. `DataArray::Load`, 23 positives) | 10 | 3,544 | accept |
| drift, anchored | agrees ≥ 3, agrees ≥ 2× body negatives, runner-up agrees ≤ half ours | 55 | 22,192 | accept |
| drift, identity witness | matched-caller witness naming exactly this function (runner-up has none), or retail RTTI naming the class of a ctor/dtor | 13 | 4,324 | accept |
| identity contradicted | matched callers call a different name here, or the name is bound elsewhere | 25 | 3,320 | refuse |
| weak | ≤ 1 positive; mostly 8–120 B generic accessors (the family W16-HI measured at 4.5% wrong) | 31 | 2,580 | refuse |
| drift, no anchor | body negatives without enough independent agreement | 33 | 3,012 | refuse |

**178 accepted / 54,908 B.** In the drift buckets, the negatives were read as drift when they were
DC3-era calls, symbol literals, or inlined `ObjPtr` vtables, the same classes HK found.

Some refusals are worth a follow-up:

- `0x8278e200` / `0x8278ddf8` look like `GameGemList::Finalize` / `MergeChordGems` crossed; the
  callers say the reverse of the score.
- `HxGuid::Clear` vs `Generate`.

**Validation, final map:** `icf_alias_finder.py --validate` **PASS** — 1,482 map-consistent,
254 tolerated, **0 contradicted**. `map_name_injectivity.py` **OK** — 30,219 applied rows,
injective.

## 3. Map events after naming

- **Three dtk over-carve merges**, committed separately (`symbols.txt` only). Naming
  `GameGem::GetLowestString` (`0x8278ebc8`), `GetHighestString` (`0x8278ebf8`) and
  `MemcardMgr::ThreadStart` (`0x827acb08`) let dtk merge a following 8/8/12-byte fragment into each.
  On retail bytes all three are leaf functions with no `.pdata`, and each fragment is the target of a
  conditional branch inside the named body (`mr r3,r11; blr`, and ThreadStart's `beq +0x50`). The
  merges are correct. The A/B base carries them on both legs, as in HK.
- **Rebase.** Main's W16-HP retired the `BinStreamRev` spellings. The three `Load` rows mapped here
  as `…AAVBinStreamRev@@@Z` read 0 after the rebase and were renamed to `…AAVBinStream@@@Z`.
- **One name withdrawn: `0x82768b18 = ??1VarStack@@QAA@XZ`.** The identity is right: 3 agrees,
  2 matched-caller witnesses, and a body byte-identical to ours. But retail's ICF folded it with the
  byte-identical `~pair<const Symbol,DataNode>`. Both destroy a `DataNode` at +4: they test the type
  at +8 and call `DataArray::Release`. **279 retail EH funclets** call this address to unwind that
  pair, 273 of them in RockCentral. While it was a placeholder, `name_check` forgave those calls.
  Named `~VarStack`, every one is charged against our funclet, which calls the `~pair` spelling. The
  first A/B (below) measured it: **281 rows / 11,264 B fell off fuzzy 100, and 279 / 11,184 B of
  those trace to this one name**, attributed row by row from the leg B listings. Keeping the name
  needs a retail-proven alias group (`~VarStack` ≡ `~pair<const Symbol,DataNode>` ≡
  `~pair<const char*,DataNode>` ≡ `~ScriptTask::Var`), which is alias work and outside this lane.
  The address is left anonymous. The other 2 lost rows (80 B) are `~BufStream` unwinds our
  RockCentral does not have. That is a real exposure, and the name stays.

## 4. Body repair

Six forks, each in its own worktree on disjoint directories, followed HK's rules:

- full `ninja-locked` builds only;
- a whole-report snapshot diff after every edit, with a change kept only if no row anywhere
  dropped;
- about three ideas per row, then stop.

| fork | dirs | rows | now at 100 | fork snap Δ (fns / B) |
|---|---|---:|---:|---|
| A | beatmatch | 34 | 22 | +41 / +6,816 |
| B | world, ui, hamobj, movie | 34 | 15 | +27 / +3,556 |
| C | obj, flow, math | 24 | 14 | +21 / +4,496 |
| D | synth, synth_xbox, oggvorbis | 19 | 10 | +14 / +3,132 |
| E | os | 16 | 10 | +15 / +3,072 |
| F | utl, meta, net | 36 | 18 | +20 / +4,392 |

Three drops were recorded and kept, all on anonymous 40 B EH funclets that were already below 100,
so no matched bytes were involved:

- C: `fn_82759944` 99.9 → 93.9, `fn_8275AE0C` 99.4 → 99.3
- F: `fn_8251BBC4` 99.4 → 99.3

The fixes by pattern:

| pattern | examples |
|---|---|
| DC3-era code absent in TU5, moved behind `HX_NATIVE` or removed | `SynthInit` `master_vol`, `Synth` ctor `track_levels`/`ADSRImpl`, `SynthSample::Sync` PC WAV branch, `ContentMgr::PollRefresh` `extra_songs`, `ContentMgr::Handle` `is_mounted`/`refresh_synchronously`, `ReadError` `UsingCD`, `SongPreview` `mSameSongRequested` |
| TU5 code absent from the oracle, rebuilt from retail bytes | `MasterAudio::ResetTrack` fader removal, `MasterAudio::Load` SongInfo slot 0x4c, `OnFillStart` third branch, `HDCache::WriteAsync`, two `OnJoypadStageKitRaw` helpers (names ours, unmapped) |
| Layout | `GameGem` real-guitar data kept unpacked (0x14…0x40, size 0x44), `RGGemInfo::chord_name` `char[64]`, `OggMap` 0x40 with a `MemStream` at +8 (VorbisReader's 0x2C "TU5 placeholders" were OggMap's tail), `MemcardMgr` bases in rb3-Wii order plus a +0x79 bool |
| Unused function-local static Symbols retail constructs | `chord_hopos`, `drum`, `guitar`/`bass`, `end`, `mute` |
| Inline policy | `DrumFillTrackName` `__declspec(noinline)`; `ObjPtr<UIColor>` / `ObjPtr<RndMat>` ctors kept out of line by a declaration-only specialization; a forced-inline `ObjPtrList<RndFont>::Set` in UIFontImporter.cpp |
| Retail's own `AddRef(this)` before `push_back` | LightPreset `Add*` (guarded `#ifndef HX_NATIVE`; natively `AddRef` takes an `ObjRef*` and the gate failed without the guard) |

Real behaviour bugs fixed on the way:

- `Loader`'s ctor loop never advanced its iterator.
- `CacheMgrXbox::PollDelete` had an inverted "device missing" condition.
- `HasPadNumsSigninChanged` fell through after `MILO_FAIL`.

## 5. Measurement

`tools/ab_measure.py --worktree ~/tmp/wt-w16-hn-ab --patch <281f99e5a..w16-hn>`. The base is main
`835ab0289` plus the `symbols.txt` merge commit `281f99e5a`, so the patch carries only the map and
source changes. The patch touches 87 files; `symbols.txt` is not among them.

| | leg A (base) | leg B (branch) | Δ |
|---|---:|---:|---:|
| matched_functions | 45,183 | 45,338 | **+155** |
| masked_equal | 23,495 | 23,535 | +40 |
| honest (matched − masked_equal) | 21,688 | 21,803 | **+115** |
| matched_code_percent | 43.435623 | 43.711098 | **+0.275475 pp** (**+28,228 B**) |
| fuzzy_match_percent | 52.531937 | 53.058090 | +0.526153 pp |

- **Setup.** Both legs were settled and read at a split fixed point (0 extra re-splits each). The
  ruler was `name_check`, the shipped one. Leg B recompiled 1,038 TUs.
- **Units.** 70 improved and **0 regressed**, measured as each unit's `mpn == 100` count, leg A vs
  leg B. Units at 100% went from 220 to 221 on the `mpn` ruler (DataArray reached 100) and from
  192 to 193 on all-rows-fuzzy.
- **Rows.** 2 rows fell off fuzzy 100 anywhere in the binary: 80 B of RockCentral `~BufStream`
  unwinds (§3).
- **`none` control:** +34,592 B. `NOT_APPLICABLE`, because the patch carries source.
- **Predicted vs measured.**
  - **First A/B (with `~VarStack` named):** I predicted about +150 fns and +28.4 KB from the fork
    snapshots plus the naming-only rows. It measured **+156 / +17,068 B**. The functions held and
    the bytes fell 11 KB short. Leg-report row diffs traced the gap to 281 funclets, 279 of them
    through one name (§3).
  - **Final A/B (after withdrawing that name):** I predicted **+155 / about +28,200 B**, i.e.
    +17,068 + 11,184 − 24. It measured **+155 / +28,228 B**.
- **Where the +155 comes from.** 102 named rows reach 100 (26,280 B). The rest are EH funclets
  that newly pair by bytes (Δmasked_equal +40), plus neighbours that the repairs fixed on the way,
  such as `GeoInit`, `SelectDevice` and `IsJoypadDetectMatch`.

## 6. What is left, and where

- **Only an ICF fold-survivor callee name is left**, so these need a proven alias, not source:
  - LightPreset `Add*` 99.7–99.93
  - CheckRTs 99.95
  - DoBeginMovieFromFile 99.87
  - HandleRGRollStop 99.94
  - AddGameGem 91.1 (the `operator<` / `CompareTimes` fold)
  - Cheats ButtonDown/KeyboardKey
  - MemTracker Free/Realloc
  - StringTable Reserve/AddBuf
  - Loader ctor
  - HandleNetCacheMgrFailure
  - SampleInst360 ctor
  - OggMap ctor
  - CheckHmxHeader
  - `ContentMgr::PollRefresh` 99.99: retail's `bl 0x827be570` is mapped `allocate<String>`, which looks like a map label problem.
- **Register or scheduling only**, after about three ideas each:
  - HandleRGGemStop 97.35 (2,176 B)
  - PrepareTrack 95.9
  - TrimOverlappingGems
  - WillBeNoStrum
  - `_ReadAsync` 94.4
  - FileLocalize
  - ThreadCallPoll
  - DirLoader::SaveObjects 98.0 / Cleanup / ~DirLoader
  - Frustum::Set
  - Quat::Set
  - `??0UILabelDir` 98.1
  - BuildDrawState 93.0 (frame 0x200 vs 0x1c0)
  - SetFullness
  - ThreadDone
  - XboxEnumeration::Start
  - ~BufStream
  - PopulateOffers 84.7
  - TalkyMatcher::Analyze
  - FftRealCcs
  - StreamReceiver::Poll
- **Shared-base global addressing (co-addressing):**
  - MemPrintOverview (`gNumHeaps` as `gHeaps+0x254`)
  - InitMakeString
  - MemTrackLogDF
  - BeamDef/CharDef::Load (the revision's +4 folded into each load)
  - Synth::DrawMeterScale (one shared constant block)
- **Needs a signature or map change:**
  - `NewFile`: retail has no `NullFile` path; `?Write/ReadDone@NullFile` map names look mislabelled.
  - `XboxMapFile::ParseStack`: the fourth parameter is a raw buffer, not `FixedString&`.
  - `SetTokenFmtImp`: retail uses the older 3-arg `SuperFormatString`.
- **Out of scope:**
  - `~CharClip` (char)
  - `BandIKEffector::SetDeformClip` (bandobj)
  - `SfxInst` ctor (needs `MidiInstrument.cpp`, the other lane's file)
- **Not kept:** `OnSyncWithResourceFile` reached 99.86 but pushed four funclets 99.3 → 93.4. It is
  saved at `~/tmp/w16hn/B/UIFontImporter.cpp.onsync`.
- **Split problems found, not fixed:**
  - `BeatMatcher::SetSyncOffset` and `GameGem::GetNumStrings` rows are cut 4 B short; the `blr`
    sits in the next symbol.
- **Next identification pass.** With the GameGem layout fixed, the remaining anonymous GameGem
  accessors (`fn_8278EB28` … `fn_8278F0A0`) are likely identifiable.

## 7. Rows, before and after

On main (`835ab0289`, before this branch) **every row below is an anonymous `fn_` at fuzzy 0,
unpaired**; the "main" column is omitted for that reason. "at naming" is the row's
`fuzzy_match_percent` right after the map entry went in, before body repair. "after" is
`report.json` on the final full build of `w16-hn`.

| after | rows | bytes |
|---|---:|---:|
| 100 | 102 | 26,280 |
| 99–100 | 30 | 10,240 |
| 95–99 | 19 | 9,292 |
| 90–95 | 12 | 4,628 |
| < 90 | 14 | 4,472 |
| **all** | **177** | **54,912** |


| retail row | size | name | unit | at naming | after | fork: note |
|---|---:|---|---|---:|---:|---|
| `0x82785e00` | 2176 | `?HandleRGGemStop@SongParser@@QAA_NHAAVDifficultyInfo@1@EH@Z` | SongParser | 72.09 | 97.35 | A: rebuilt as retail loops, chord_name char[64]; residual r22/r23 regalloc + one base-pointer schedule |
| `0x8251f1e0` | 1504 | `?PollRefresh@ContentMgr@@UAAXXZ` | ContentMgr | 89.66 | 99.99 | E: dev extra_songs alt-dir limit removed. Left: one callee name -- retail bl 0x827be570 is mapped ?allocate@StlNodeAlloc... |
| `0x8280d4a0` | 1444 | `?BuildDrawState@UIListDir@@QBAXAAUUIListWidgetDrawState@@ABVUIListState@@W4St...` | UIListDir | 85.64 | 93.02 | B: retail highlight = showing==selected (no allowHighlight), Provider() before elem fill, numDisplay-1 bound. Left: regi... |
| `0x8274d460` | 1176 | `?Load@DataArray@@QAAXAAVBinStream@@@Z` | DataArray | 100.00 | **100** | -: fuzzy 100 on naming alone |
| `0x827569a0` | 1044 | `?SaveObjects@DirLoader@@SAXAAVBinStream@@PAVObjectDir@@@Z` | DirLoader | 86.62 | 98.01 | C: rev 0x1c, no trailing bool, SetName(Name(),dir) instead of NextName; residue = sorter copy stb + loop-iterator regist... |
| `0x82789108` | 956 | `?PrepareTrack@SongParser@@QAAXPBDPAUPartInfo@@@Z` | SongParser | 76.27 | 95.88 | A: 2 unused local statics, hopo type chain, find(); residual: difficulty-char switch still unsigned (retail extsb) and a... |
| `0x82524ee8` | 896 | `?IsJoypadDetectMatch@?A0x47b254f1@@YA_NPAVDataArray@@ABVJoypadData@@@Z` | Joypad | 95.80 | **100** | E: retail compare operand order for type/stick/trigger |
| `0x828115e8` | 880 | `??0UILabelDir@@IAA@XZ` | system/ui/UILabelDir | 77.55 | 98.13 | B: RB3_TU_OBJPTR_FORCEINLINE_CTOR + declared-only ObjPtr<UIColor> ctor specialization (retail keeps that one out-of-line... |
| `0x827accf8` | 792 | `?Handle@MemcardMgr@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | MemcardMgr_Xbox | 98.30 | **100** | F: HANDLE_SUPERCLASS(MsgSource) (retail forwards at this+0x6c) |
| `0x82700e18` | 760 | `??0Synth@@QAA@XZ` | Sfx | 90.54 | **100** | D: rb3-Wii ctor: no track_levels/ADSRImpl; new MidiInstrumentMgr at +0x78 |
| `0x8251ec68` | 740 | `?Handle@ContentMgr@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | FlowCommand | 76.00 | **100** | C: TU5 has 6 handlers (to delete_content); is_mounted/refresh_synchronously HX_NATIVE-only (os/ContentMgr.cpp) |
| `0x827c21a0` | 688 | `?OnMsg@CheatsManager@@AAAHABVButtonDownMsg@@@Z` | Cheats | 89.58 | 99.97 | F: early return on no user; pad from localUser->GetPadNum(); residual = vector copy-ctor fold name |
| `0x82c2cd48` | 648 | `ogg_stream_flush` | framing | 85.01 | **100** | D: libogg 1.1 body (granule_pos from granule_vals[0], unconditional assign) |
| `0x8272a5b8` | 632 | `?Poll@StreamReceiver@@UAAXXZ` | StreamReceiver | 86.59 | 99.26 | D: 0xC000 send block, 100000 wrap, unsigned switch, subf. test, decremented memset; left: switch compare tree (if-chain ... |
| `0x82bb2e70` | 632 | `?CheckHmxHeader@VorbisReader@@AAA_NXZ` | VorbisReader | 95.57 | 99.96 | D: version in a stack local; left: s64 slot swap (3 variants inert) + setupCypher ICF-survivor name MakeOSCAddress |
| `0x824e4c18` | 612 | `?SetFullness@WorldCrowd@@QAAXMM@Z` | Crowd | 82.32 | 94.47 | B: rb3-Wii shape: recount after total, count-toward-target loops, re-read mMMesh, no AssignRandomColors; grow test in lo... |
| `0x8231af68` | 592 | `?DrawShowing@MeterDisplay@@UAAXXZ` | MeterDisplay | 72.84 | 89.16 | B: rb3-Wii body: assert not early return, signed itouse, no UpdateDisplay, diff*f+start, trailing SetWorldXfm, out-of-li... |
| `0x82782e70` | 584 | `?OnFillStart@SongParser@@QAAXHE@Z` | SongParser | 90.41 | **100** | A: TU5 third warn branch |
| `0x8281c5b0` | 576 | `?OnSyncWithResourceFile@UIFontImporter@@IAA?AVDataNode@@PAVDataArray@@@Z` | UIFontImporter | 75.93 | 75.97 | B: not kept: rb3-Wii MakeString/FileRoot/PostLoad form reached 99.86 (residual = ICF fold-alias callee names) but re-pai... |
| `0x8277ec48` | 572 | `?ResetTrack@MasterAudio@@QAAXUAudioTrackNum@@_N@Z` | MasterAudio | 71.06 | **100** | A: TU5 FindLocal(mute/remote/drum_fill) removes; b3 = b1 && b |
| `0x82755e78` | 548 | `?Cleanup@DirLoader@@QAAXPBD@Z` | DirLoader | 81.96 | 99.96 | C: no AutoGlitchReport; FilePath copy of stripped log arg; residue = temp slot 0x58 vs 0x50 (frame 0x90 vs 0x80) |
| `0x82528ea8` | 536 | `?OnMsg@JoypadClient@@AAAHABVButtonUpMsg@@@Z` | JoypadClient | 92.76 | **100** | E: retail tests msg.GetUser() (as User*) before anything, like rb3-Wii |
| `0x82775460` | 536 | `?TrimOverlappingGems@SongData@@QAAXHHH@Z` | SongData | 85.34 | 87.41 | A: no empty-list warn, list binding order; residual: retail reuses one stack slot (0x58) for several temps |
| `0x8275b8b8` | 528 | `?PropertyArray@Object@Hmx@@QAA?AVDataNode@@VSymbol@@@Z` | Object | 88.79 | **100** | C: static DataArrayPtr d(DataNode(1)) |
| `0x8278ee58` | 524 | `??0GameGem@@QAA@ABURGGemInfo@@@Z` | system/beatmatch/GameGem | 72.15 | **100** | A: GameGem TU5 tail layout (unpacked RG fields), int duration conversion |
| `0x82768318` | 508 | `?Handle@MsgSource@@UAA?AVDataNode@@PAVDataArray@@_N@Z` | Msg | 89.41 | **100** | C: no unhandled-msg PathName tail |
| `0x824ac808` | 500 | `?GetKey@LightPreset@@IBAXMAAH0AAM@Z` | LightPreset | 95.64 | 99.92 | B: fmod result written back into frame, mid=(after+before)>>1. Left: one add operand order |
| `0x8271e7b0` | 496 | `??0SynthEmitter@@IAA@XZ` | Emitter | 86.13 | 86.13 | D: NOT FIXED: retail inlines only ObjPtr<RndTransformable> ctor, calls Sfx/SfxInst ones; per-TU RB3_OBJPTR_FORCEINLINE_C... |
| `0x8281b070` | 472 | `?HandmadeFontChanged@UIFontImporter@@IAAXXZ` | UIFontImporter | 75.21 | **100** | B: rb3-Wii body (Set on begin, scan from ++begin, no RndFont3d probe) + TU-local forceinline ObjPtrList<RndFont>::Set sp... |
| `0x825359f8` | 460 | `?_ReadAsync@AsyncFileWin@@MAAXPAXH@Z` | AsyncFile_Win | 71.40 | 94.39 | E: no gFakeFileErrors path; count/buf/Offset re-read from members. Left: register allocation of the zero/aligned flag (r... |
| `0x824db060` | 440 | `?Load@BeamDef@Spotlight@@QAAXAAVBinStream@@@Z` | Spotlight | 98.73 | 98.73 | B: not fixed: retail folds rev's +4 into each lhz @l (external-style addressing); making gRevs_Spotlight external droppe... |
| `0x8277da60` | 436 | `?SetTrackFader@MasterAudio@@QAAXUAudioTrackNum@@HVSymbol@@MM@Z` | MasterAudio | 76.42 | **100** | A: local static Symbol mute |
| `0x8271ba38` | 432 | `??0SfxInst@@QAA@PAVSfx@@@Z` | Sfx | 96.90 | 96.90 | D: NOT FIXED: retail calls NewInst() with no args (rb3-Wii signature); changing the virtual needs MidiInstrument.cpp (ot... |
| `0x824ee9c8` | 424 | `?Set@Quat@Hmx@@QAAXABVMatrix3@2@@Z` | Rot | 98.16 | 98.16 | C: left: load order/regalloc of the two fadds operand loads; operand swap was inert (/fp:fast) |
| `0x825173e0` | 424 | `?NewFile@@YAPAVFile@@PBDH@Z` | File | 72.23 | 72.23 | E: not changed. Retail has no gNullFiles/NullFile path, but removing it un-emits NullFile/File vtable members whose rows... |
| `0x82516e28` | 408 | `?FileLocalize@@YAPBDPBDPAD@Z` | File | 96.72 | 99.95 | E: work on iFilename directly, splice via q+1. Left: one dead addi lands in r30 (retail) vs r11 (ours) |
| `0x824ad030` | 396 | `?Load@SpotlightEntry@LightPreset@@QAAXAAVBinStream@@@Z` | LightPreset | 93.13 | **100** | B: mTarget.Load(d, ...) -- d is the stream under the cast model |
| `0x824ccbe0` | 380 | `??5@YAAAVBinStream@@AAV0@AAUBitmapOverride@WorldDir@@@Z` | MidiSynth | 87.31 | 87.31 | D: NOT TRIED: body lives in src/system/world/Dir.cpp (outside fork D dirs); retail inlines the ObjPtr release/clear inst... |
| `0x82765d40` | 380 | `?SetKeyValue@TypeProps@@QAAXVSymbol@@ABVDataNode@@_N@Z` | TypeProps | 78.68 | 99.89 | C: AddRef/Release with TypeProps as owner; residue = add operand order in Node() address |
| `0x8281a070` | 380 | `?Copy@UIFontImporter@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z` | UIFontImporter | 93.68 | **100** | B: mItalics copied after mFontSupersample (rb3-Wii order) |
| `0x825312a8` | 372 | `?ParseStack@XboxMapFile@@SA_NPBDPAUStackData@@HAAVFixedString@@@Z` | MapFile_Xbox | 73.33 | 73.33 | E: not changed. Retail inlines strcat on the 4th parameter itself (no lwz of mStr), i.e. the parameter is the char buffe... |
| `0x82746550` | 372 | `??0TexMovie@@IAA@XZ` | TexMovie | 97.85 | **100** | B: retail leaves mIsLocalized/mPaused uninitialized |
| `0x8277ff08` | 372 | `?Load@MasterAudio@@QAAXPAVSongInfo@@PAVPlayerTrackConfigList@@@Z` | MasterAudio | 90.82 | **100** | A: NewStream bool from SongInfo slot 0x4c; ternary count |
| `0x8278cbb0` | 368 | `?WillBeNoStrum@GameGemList@@QAA_NABVGameGem@@@Z` | system/beatmatch/GameGemList | 82.59 | 98.70 | A: unused static chord_hopos, operand order; residual: retail spills gem.mSlots to the stack before GemNumSlots |
| `0x82794d90` | 368 | `?IsFillCompletion@TrackWatcherImpl@@QAA_NMHAAH@Z` | TrackWatcherImpl | 80.37 | **100** | A: end-tick temp + within flag (GetDrumFillInfo callee name is an ICF fold with GetFillInfo) |
| `0x824d1c70` | 364 | `?CheckRTs@NgSpotlightDrawer@@KA_NPAVSpotlightResources@1@@Z` | SpotlightDrawer_NG | 74.91 | 99.95 | B: no DX_ASSERT after CreateTexture (house-pattern gated); SetBitmap dims as call args. Left: GetDepthRT ICF fold-surviv... |
| `0x827ac548` | 364 | `?ThreadDone@MemcardMgr@@UAAXH@Z` | MemcardMgr_Xbox | 96.76 | 98.70 | F: unsigned switch value; residual is the case-range compare form (<1/<3/<5 vs ==0/<=2/<=4); tried removing kS_None case... |
| `0x82756470` | 356 | `??1DirLoader@@UAA@XZ` | DirLoader | 98.71 | 98.71 | C: left: one extra stack spill (stw r11,0x50) and a register swap; no source lever found |
| `0x82789508` | 356 | `?HandleRGRollStop@SongParser@@QAA_NHE@Z` | SongParser | 74.35 | 99.94 | A: string-count loop; residual is only the resize<> callee name (ICF fold survivor Key<Transform>) -- alias, not source |
| `0x82775730` | 348 | `?RestoreTrackFromBackup@SongData@@QAAXH@Z` | SongData | 96.89 | **100** | A: single loop counter |
| `0x82785938` | 348 | `?CheckDrumCymbalMarker@SongParser@@QAA_NHH_N@Z` | SongParser | 78.92 | **100** | A: unused static Symbol drum, plain range |
| `0x826fde58` | 344 | `?DrawMeterScale@Synth@@QAAXAAM@Z` | system/synth/Synth | 90.58 | 90.58 | D: NOT FIXED: retail loads 0.2/0.7 from a co-addressed .rdata aggregate (0x820F4C60 = 0.2,40,0.7); static const scalars ... |
| `0x827427e8` | 344 | `?UpdateThread@Splash@@IAAXXZ` | Splash | 80.92 | 96.80 | B: worker-side NgRnd Suspend/Resume order (slot proof from Splash::Suspend at 100); Splash Time print compiled out. Left... |
| `0x82741560` | 340 | `?CheckWorkerSuspend@Splash@@IAAX_N@Z` | Splash | 97.62 | 97.65 | B: Suspend/Resume order swapped to retail slots. Left: dead lwz of mState in MILO_ASSERT |
| `0x82783a70` | 340 | `?OnMidiMessageBeat@SongParser@@QAAXHEEE@Z` | SongParser | 75.27 | **100** | A: 13/12/11 test order with grid branch first |
| `0x82701570` | 332 | `?SynthPreInit@@YAXXZ` | system/synth/Synth | 75.88 | **100** | D: Synth::New + RELEASE on Fail (friend decl); InitWavMgr native-only |
| `0x827b80e0` | 328 | `?Start@XboxEnumeration@@UAAXXZ` | StoreEnumeration | 96.62 | 99.39 | F: content type 2 not 0x100002; residual r28/r29 swap (regalloc) |
| `0x827da5e8` | 328 | `?GetFreeSpaceSync@CacheXbox@@UAA_NPA_K@Z` | Cache_Xbox | 74.72 | 94.60 | F: positive-form error ladders; residual: block 1's li 8 tail-merged with block 2's; 3 ideas |
| `0x827d7e38` | 324 | `?PollSearch@CacheMgrXbox@@AAAXXZ` | CacheMgr_Xbox | 97.41 | **100** | F: nonzero result ends the search; 0x65B only guarded MILO_FAIL |
| `0x825345b0` | 320 | `?WriteAsync@HDCache@@QAA_NHHPBX@Z` | HDCache | 72.79 | **100** | E: retail writes when the block bit is clear; size check, LockCache, mLastCacheWriteMs/mWriteBlock, WriteDone on failure |
| `0x8275b418` | 316 | `?OnIterateRefs@Object@Hmx@@AAA?AVDataNode@@PBVDataArray@@@Z` | Object | 93.39 | **100** | C: advance iterator before the body |
| `0x827bf118` | 312 | `?OpenFile@FileLoader@@AAAXXZ` | Loader | 93.63 | **100** | F: no UsingCD() save, SetUsingCD(true) |
| `0x827c2480` | 312 | `?OnMsg@CheatsManager@@AAA?AVDataNode@@ABVKeyboardKeyMsg@@@Z` | Cheats | 99.92 | 99.94 | F: CallCheatScript 4th arg true; residual = vector copy-ctor fold name |
| `0x827d51a8` | 304 | `?Free@MemTracker@@QAAXPAX@Z` | MemTracker | 86.38 | 99.93 | F: log filter info->mHeap==0; bare operator delete; residual = AllocInfo::Validate fold name |
| `0x827b5c78` | 300 | `?FinishCheckout@StorePanel@@QAAXXZ` | StorePanel | 83.36 | **100** | F: no HandleType call |
| `0x827bc838` | 300 | `?MemPrintOverview@@YAXHAAVTextStream@@@Z` | MemMgr | 93.77 | 93.77 | F: NOT FIXED: retail addresses gNumHeaps as gHeaps+0x254 (one aggregate across MemMgr/MemHeap); gNumHeaps=0 init was ine... |
| `0x824b5768` | 296 | `?AddSpotlight@LightPreset@@IAAXPAVSpotlight@@_N@Z` | LightPreset | 77.96 | 99.93 | B: x->AddRef(this) before push_back, uint loop (rb3-Wii). Left: push_back ICF fold-alias name |
| `0x827b6f80` | 292 | `?PopulateOffers@StorePanel@@MAAXPAVDataArray@@_N@Z` | StorePanel | 81.03 | 84.66 | F: single delete condition; ValidateOffers native-only; residual = block order + r29/r30/r31 roles; positive form tried ... |
| `0x824f0d60` | 288 | `?Set@Frustum@@QAAXMMMM@Z` | Geo | 73.26 | 99.21 | C: Vector2 Normalize kept out of line (retail 0x824791A0, defined inline in Geo.cpp); residue = f28/f30 swap for fovY/ra... |
| `0x827f2d78` | 288 | `?SetTokenFmtImp@UILabel@@IAAXVSymbol@@PBVDataArray@@1H_N@Z` | UILabel | 75.93 | 75.93 | B: not fixed: retail uses RB3-era SuperFormatString(fmt, da, bool) + RawFmt; ours is the DC3 5-arg class in src/system/u... |
| `0x827c8ef8` | 284 | `?Add@StringTable@@QAAPBDPBD@Z` | StringTable | 70.55 | **100** | F: step to next buffer once, else AddBuf(Size()) |
| `0x8276e570` | 280 | `?SongDurationMs@BeatMaster@@UAAMXZ` | system/beatmatch/BeatMaster | 78.37 | **100** | A: local static Symbol end |
| `0x8252d240` | 276 | `?WriteAsync@AsyncFile@@UAA_NPBXH@Z` | AsyncFile | 94.71 | **100** | E: separate source pointer in the buffered path |
| `0x8278e530` | 272 | `?AddGameGem@GameGemList@@QAA_NABVGameGem@@W4NoStrumState@@@Z` | system/beatmatch/GameGemList | 91.15 | 91.15 | A: not changed: comparator is an ICF fold (retail names ??MGameGem, we call CompareTimes) plus a stack-slot schedule |
| `0x82701710` | 268 | `?SynthInit@@YAXXZ` | system/synth/Synth | 71.72 | **100** | D: no master_vol (native-only) |
| `0x827f5410` | 268 | `?AdjustHeight@UILabel@@QAAX_N@Z` | UILabel | 88.36 | **100** | B: direct b && mReservedLine > 0 branch, no bool temp |
| `0x82753128` | 264 | `?PreInit@ObjectDir@@SAXHH@Z` | system/obj/Dir | 90.91 | **100** | C: no sSuperClassMap.clear, SetCacheMode(true) unconditional |
| `0x827f2528` | 260 | `?CenterWithLabel@UILabel@@QAAXPAV1@_NM@Z` | UILabel | 73.32 | **100** | B: branchless num = b ? -1 : 1; retail measures this label first and assigns the widths crosswise |
| `0x8276af70` | 256 | `?LoadFile@DataLoader@@AAAXXZ` | DataFile | 71.92 | **100** | C: DataLoaderThreadObj ctor inline (no mLocal / FileIsLocal) |
| `0x825277c8` | 252 | `?ThreadCallPoll@@YAXXZ` | ThreadCall_Win | 92.22 | 92.22 | E: not changed. Retail has a no-op clrrwi r9,r9,0 on oldType and a different store schedule; tried int oldType, rb3-Wii ... |
| `0x82712d10` | 252 | `?SetType@FxSendPitchShift@@UAAXVSymbol@@@Z` | system/synth/FxSendPitchShift | 100.00 | **100** | -: fuzzy 100 on naming alone |
| `0x8275d9f8` | 252 | `?DataFindElem@@YA?AVDataNode@@PAVDataArray@@@Z` | DataFunc | 95.79 | **100** | C: DataNode::operator== |
| `0x824b3260` | 248 | `?AddEnvironment@LightPreset@@IAAXPAVRndEnviron@@@Z` | LightPreset | 75.16 | 99.92 | B: x->AddRef(this) before push_back, uint loop (rb3-Wii). Left: push_back ICF fold-alias name |
| `0x8275b1c0` | 248 | `?PropertySize@Object@Hmx@@QAAHPAVDataArray@@@Z` | Object | 82.18 | **100** | C: fail path keeps only PathName(this) |
| `0x82b6dfb8` | 248 | `??0SampleInst360@@QAA@PAVSynthSample360@@_NHH@Z` | system/synth_xbox/SampleInst360 | 78.81 | 99.76 | D: reads sample loop via new out-of-line getters; left: 3 ICF-survivor callee names |
| `0x827a5a78` | 244 | `?OnStart@SongPreview@@QAA?AVDataNode@@PAVDataArray@@@Z` | SongPreview | 82.93 | **100** | F: retail 0x6d is mSecurePreview (mSameSongRequested DC3-only); LOG args not evaluated |
| `0x827bf3d0` | 244 | `??0Loader@@QAA@ABVFilePath@@W4LoaderPos@@@Z` | Loader | 83.00 | 99.84 | F: mLoadCount(0) + DC3 backward insertion walk (ours looped forever); residual = list::insert fold name |
| `0x82818f18` | 244 | `?SyncWithGennedFonts@UIFontImporter@@IAAXXZ` | UIFontImporter | 73.62 | **100** | B: rb3-Wii body: no i==0 case, Mat()==mDefaultMat check, unconditional delete |
| `0x8276e478` | 240 | `?LoaderPoll@BeatMaster@@QAAXXZ` | system/beatmatch/BeatMaster | 83.68 | **100** | A: calls IsLoaded() out of line |
| `0x82771208` | 240 | `?EnableGems@SongData@@QAAXHMM@Z` | SongData | 83.57 | **100** | A: end time computed before the range test |
| `0x82728170` | 236 | `?Sync@SynthSample@@MAAXW4SyncType@1@@Z` | system/synth/SynthSample | 75.29 | **100** | D: no sDisabled / PC branch (native-only) |
| `0x827cc978` | 232 | `?ASCIItoUTF8@@YAXPADHPBD@Z` | UTF8 | 93.48 | 98.14 | F: DC3 pointer-walk loop; residual = retail's dead stack home of the char (DC3 notes the same, unreproduced; address-tak... |
| `0x827da888` | 228 | `?ThreadRead@CacheXbox@@IAAHXZ` | Cache_Xbox | 89.02 | **100** | F: err {2,3,0x15} -> 8, shared connected?-1:8 tail |
| `0x82380a78` | 224 | `??1CharClip@@UAA@XZ` | LightPreset | 95.36 | 95.36 | B: not attempted: CharClip dtor lives in src/system/char (excluded dir) |
| `0x824b3430` | 224 | `?AddSpotlightDrawer@LightPreset@@IAAXPAVSpotlightDrawer@@@Z` | LightPreset | 75.18 | 99.73 | B: x->AddRef(this) before push_back, uint loop (rb3-Wii). Left: push_back ICF fold-alias name |
| `0x827d9230` | 220 | `??0IListChunk@@QAA@AAV0@@Z` | Chunks | 81.95 | **100** | F: copy-ctor inits only mHeader/mLocked/mSubHeader/mRecentlyReset |
| `0x824b3358` | 216 | `?AddLight@LightPreset@@IAAXPAVRndLight@@@Z` | LightPreset | 73.61 | 99.91 | B: x->AddRef(this) before push_back, uint loop (rb3-Wii). Left: push_back ICF fold-alias name |
| `0x827c4168` | 216 | `??6FormatString@@QAAAAV0@ABVDataNode@@@Z` | MakeString | 73.85 | **100** | F: no float->int case; CRT _snprintf |
| `0x8278d560` | 212 | `?ClosestMarkerIdx@GameGemList@@QBAHM@Z` | system/beatmatch/GameGemList | 91.60 | **100** | A: closer flag, prev on tie |
| `0x8279e720` | 212 | `?Poll@LowPassMercurySwitchFilter@@UAA_NMM@Z` | MercurySwitchFilter | 100.00 | **100** | -: fuzzy 100 on naming alone |
| `0x82514408` | 204 | `??0Archive@@QAA@PBDH@Z` | Archive | 87.96 | **100** | E: rb3-Wii init list (no mNumArkfiles/mMaxArkfileSize/permission init) |
| `0x825246d0` | 204 | `??0JoypadData@@QAA@XZ` | OnlineID | 86.84 | **100** | E: retail skips mNewPressed/mNewReleased/mDistFromRest/mEepromWriteDone |
| `0x82771430` | 204 | `?ChangeTrackDiff@SongData@@QAAXHH@Z` | SongData | 77.75 | **100** | A: only track != -1 guard; equality phrase-type test |
| `0x827184b0` | 200 | `?Analyze@TalkyMatcher@@QAAXPBFHM@Z` | system/synth/VoiceBeat | 85.76 | 90.20 | D: size() != 0; left: up-counting loop vs our bdnz |
| `0x82771310` | 192 | `?DrumFillTrackName@@YA?AVSymbol@@PBVSongData@@HH@Z` | SongData | 78.02 | **100** | A: noinline, unsigned switch |
| `0x82784548` | 192 | `?HandleRGGemStart@SongParser@@QAA_NHAAVDifficultyInfo@1@EEEH@Z` | SongParser | 94.46 | **100** | A: plain 24..29 range |
| `0x827d80e8` | 192 | `?PollDelete@CacheMgrXbox@@AAAXXZ` | CacheMgr_Xbox | 93.60 | **100** | F: listed error or bad device state -> Missing (ours was inverted) |
| `0x828255a8` | 192 | `?PlayStartOfAnims@UITrigger@@QAAXXZ` | UITrigger | 87.85 | **100** | B: 5-arg Animate(start, end, units, period, blend) overload |
| `0x82825668` | 192 | `?PlayEndOfAnims@UITrigger@@QAAXXZ` | UITrigger | 87.85 | **100** | B: 5-arg Animate(start, end, units, period, blend) overload |
| `0x824efed0` | 188 | `?Intersect@@YA_NABVVector3@@0ABVBox@@AAM2@Z` | Geo | 93.94 | 93.94 | C: left: retail loads tmin at loop top; a curMin local dropped it to 85.9 (tried twice) |
| `0x8275a500` | 188 | `?SetNote@Object@Hmx@@QAAXPBD@Z` | Object | 92.77 | 95.32 | C: MemOrPoolAlloc + two-pointer copy loop; MSVC still indexes the store (3 spellings tried) |
| `0x8275b2b8` | 188 | `?RemoveProperty@Object@Hmx@@QAAXPAVDataArray@@@Z` | Object | 100.00 | **100** | -: fuzzy 100 on naming alone |
| `0x827abe68` | 184 | `??0MemcardMgr@@QAA@XZ` | MemcardMgr_Xbox | 91.30 | **100** | F: base order MsgSource, ThreadCallback (rb3-Wii); +0x79 bool member init |
| `0x822c11a8` | 180 | `?SetDeformClip@BandIKEffector@@SAXPAVObject@Hmx@@@Z` | mtx | 99.53 | 99.53 | C: not attempted: source is src/system/bandobj (out of scope); residue is a Symbol compare load order |
| `0x824f10c0` | 180 | `?SetBSPParams@@YA?AVDataNode@@PAVDataArray@@@Z` | Geo | 99.78 | **100** | C: globals: MaxDepth @+8, MaxCandidates @+c (retail .data 20/40); store order |
| `0x825355d0` | 176 | `?ReadError@@YAXPBD@Z` | AsyncFile_Win | 81.52 | **100** | E: no UsingCD gate; stripped log keeps a by-value String copy; DC3 return shape |
| `0x8278eda8` | 176 | `??0GameGem@@QAA@ABUMultiGemInfo@@@Z` | system/beatmatch/GameGem | 93.02 | **100** | A: int duration conversion |
| `0x827f4b68` | 176 | `?SetEditText@UILabel@@QAAXPBD@Z` | UILabel | 93.86 | **100** | B: MILO_ASSERT(AllowEditText()) first (retail calls and discards it) |
| `0x82774850` | 172 | `?AddKeyboardRangeShift@SongData@@UAAXHHMHH@Z` | SongData | 88.49 | 88.49 | A: section keyed on tick (i2); residual: size() test is srawi. in retail vs our mask, and -1.0 load order |
| `0x827c3ef8` | 172 | `?InitMakeString@@YAXXZ` | MakeString | 99.67 | 99.67 | F: NOT CHANGED: residual is co-addressing of the init flag vs gBuf (retail base = flag symbol); declaration order is ine... |
| `0x827cd210` | 172 | `?AddPair@DataPoint@@QAAXPBDVDataNode@@@Z` | DataPointMgr | 78.84 | **100** | F: no empty-name guard |
| `0x827c8e50` | 164 | `?Reserve@StringTable@@QAAXH@Z` | StringTable | 74.63 | 99.88 | F: no MemTemp scope; residual = push_back fold name |
| `0x82719b18` | 160 | `?Peek@RingBuffer@@QAAHPAXH@Z` | MidiSynth | 74.85 | **100** | D: reference std::min(avail, len) |
| `0x8278f900` | 160 | `?FretMatch@RGGemMatcher@@QBA_NABVGameGem@@MMMM_N1W4RGMatchType@@@Z` | RGGemMatcher | 99.88 | **100** | A: FAIL branch first |
| `0x827b50b0` | 160 | `?HandleNetCacheMgrFailure@StorePanel@@QAAXXZ` | StorePanel | 81.90 | 99.88 | F: DC3 shape, cases 1-2 assign CacheRemoved; residual = GetFailType fold name |
| `0x827d5918` | 160 | `?Realloc@MemTracker@@QAAXPAXHH0@Z` | MemTracker | 85.78 | 99.88 | F: MemTracker::Alloc takes no file/line in retail; residual = Validate fold name |
| `0x8274e2b0` | 156 | `?NextSubDir@ObjectDir@@QAAPAV1@AAH@Z` | system/obj/Dir | 80.46 | 80.46 | C: not attempted: inline in obj/Dir.h (PCH-scale rebuild ~1.5 h at current load); retail presets r3=0 before the loop |
| `0x82746218` | 152 | `?DoBeginMovieFromFile@TexMovie@@IAAXPAVBinStream@@@Z` | TexMovie | 74.68 | 99.87 | B: no localization-track path: BeginFromFile(path, 0, false, mLoop, true, false, 0, stream). Left: Movie::End ICF fold-s... |
| `0x827bfda8` | 148 | `??0LoadMgr@@QAA@XZ` | Loader | 89.00 | **100** | F: retail leaves mPlatform/mEditMode/mCacheMode to static zero-init (native keeps) |
| `0x82bb1c78` | 148 | `??0OggMap@@QAA@XZ` | system/synth/OggMap | 81.73 | 99.86 | D: OggMap is 0x40 in TU5 (MemStream +8, words +0x28/+0x30, vector +0x34); VorbisReader placeholders removed; left: push_... |
| `0x827283e0` | 144 | `?Copy@SynthSample@@UAAXPBVObject@Hmx@@W4CopyType@23@@Z` | system/synth/SynthSample | 80.97 | **100** | D: copies mIsLooped/mLoopStartSamp/mLoopEndSamp |
| `0x82770650` | 144 | `?SendGems@SongData@@QAAXH@Z` | SongData | 99.31 | 99.44 | A: inline DB lookup; residual: lwzx operand order (scheduling) |
| `0x827a8ec8` | 144 | `?ContentName@SongMgr@@QBAPBDH@Z` | SongMgr | 75.94 | **100** | F: TU5 excludes song ID 99000001 (mID direct read) |
| `0x824eda00` | 140 | `??0FreeCamera@@QAA@PAVWorldDir@@MMH@Z` | FreeCamera | 77.23 | **100** | B: mSlewRate(f2) unscaled, mEnableDOF(1) (rb3-Wii) |
| `0x82796588` | 140 | `?ClosestUnplayedGem@TrackWatcherImpl@@UAAHMH@Z` | TrackWatcherImpl | 99.14 | 99.14 | A: not changed: r29/r30 swap only (one idea tried, neutral, reverted) |
| `0x82b82500` | 140 | `?GetValue@JsonConverter@@QAAPAVJsonObject@@PAVJsonArray@@H@Z` | JsonUtils | 95.43 | 97.14 | F: one json_object_get on the element then push; residual = no-op clrrwi r3,r3,0 |
| `0x827756a8` | 136 | `?AddLyricEvent@SongData@@UAAXHHPBD@Z` | SongData | 82.59 | **100** | A: track named via DrumFillTrackName |
| `0x827ac838` | 136 | `?OnMsg@MemcardMgr@@IAA?AVDataNode@@ABVUIChangedMsg@@@Z` | MemcardMgr_Xbox | 94.12 | **100** | F: +0x79 bool replayed to ShowDeviceSelector |
| `0x827baf70` | 136 | `??0ChunkAllocator@@QAA@XZ` | PoolAlloc | 96.74 | **100** | F: FixedSizeAlloc new inlined, delete out of line |
| `0x824e1168` | 132 | `?Load@CharDef@WorldCrowd@@QAAXAAVBinStream@@@Z` | Crowd | 96.52 | 96.52 | B: not fixed: same revs-aggregate co-addressing as BeamDef::Load (lever measured harmful in Spotlight) |
| `0x82728358` | 132 | `?PostLoad@SynthSample@@UAAXAAVBinStream@@@Z` | system/synth/SynthSample | 87.58 | **100** | D: Cached() ? sync2 : sync0 |
| `0x8275a6f8` | 132 | `?InsertProperty@Object@Hmx@@QAAXPAVDataArray@@ABVDataNode@@@Z` | Object | 100.00 | **100** | -: fuzzy 100 on naming alone |
| `0x82780cf0` | 132 | `?EndPlayerPhrase@VocalNoteList@@QAAXHH@Z` | VocalNoteList | 100.00 | **100** | -: fuzzy 100 on naming alone |
| `0x827ac468` | 132 | `??0MCResultMsg@@QAA@W4MCResult@@@Z` | MemcardMgr_Xbox | 100.00 | **100** | -: fuzzy 100 on naming alone |
| `0x8275d0f0` | 128 | `?SwitchMatch@@YA_NABVDataNode@@0@Z` | DataFunc | 85.16 | **100** | C: DataNode::operator== |
| `0x82b76120` | 128 | `?FftRealCcs@FftIpp@@QAAXPIBMPIAM@Z` | SpectralAnalysis | 96.25 | 96.25 | D: NOT FIXED: second size test compares into cr0 in retail (cr6 ours); n>0 spelling inert |
| `0x824e10c0` | 124 | `??0CharDef@WorldCrowd@@QAA@PAVObject@Hmx@@@Z` | Crowd | 93.55 | 93.55 | B: not fixed: one-store ObjPtr init order governed by Crowd.cpp's TU-wide ObjPtr gates |
| `0x827c4b30` | 124 | `?MemTrackLogDF@@YA?AVDataNode@@PAVDataArray@@@Z` | MemTrack | 86.13 | 95.97 | F: StopLog() stops the tracker itself (retail fn_827C46A0); residual = gLog addressed via gMemTrackLogState+4 vs retail ... |
| `0x82814758` | 124 | `??0UIListMesh@@IAA@XZ` | UIListMesh | 77.74 | **100** | B: FORCEINLINE_CTOR + DEFER_OWNER, ObjPtr<RndMat> ctor kept out-of-line via declared-only specialization |
| `0x824b8bd8` | 120 | `?StartPreset@LightPresetManager@@IAAXPAVLightPreset@@_N@Z` | LightPresetManager | 93.33 | **100** | B: no UpdateOverlay() (house-pattern gated) |
| `0x824e9e18` | 120 | `?SyncProperty@SpotlightEnder@@UAA_NAAVDataNode@@PAVDataArray@@HW4PropOp@@@Z` | SpotlightEnder | 100.00 | **100** | -: fuzzy 100 on naming alone |
| `0x8271ac10` | 120 | `?Pause@SfxInst@@QAAX_N@Z` | Sfx | 100.00 | **100** | -: fuzzy 100 on naming alone |
| `0x82816b48` | 120 | `?SetTex@UIPicture@@QAAXABVFilePath@@@Z` | UIPicture | 73.33 | **100** | B: no LoadMgr edit-mode deferral |
| `0x8251d458` | 112 | `?OnSignInUsers@PlatformMgr@@AAA?AVDataNode@@PAVDataArray@@@Z` | PlatformMgr_Xbox | 88.04 | **100** | E: calls XShowSigninUI directly |
| `0x825263f0` | 112 | `?Export@?A0x47b254f1@@YAXABVMessage@@@Z` | Joypad | 100.00 | **100** | -: fuzzy 100 on naming alone |
| `0x82750390` | 112 | `?OnFind@ObjectDir@@IAA?AVDataNode@@PAVDataArray@@@Z` | system/obj/Dir | 71.43 | **100** | C: TU5 only evaluates Int(3) |
| `0x827acb08` | 112 | `?ThreadStart@MemcardMgr@@UAAHXZ` | MemcardMgr_Xbox | 100.00 | **100** | -: fuzzy 100 on naming alone |
| `0x8274a3c8` | 108 | `??0ThreadTask@@QAA@PAVDataArray@@0@Z` | Task | 92.15 | **100** | C: no mWait init |
| `0x82bbaa20` | 108 | `??1MultiTempoTempoMap@@UAA@XZ` | MultiTempoTempoMap | 88.48 | **100** | F: implicit dtor (no own-vtable store) |
| `0x82704ad8` | 104 | `?ClearJumpMarkers@StandardStream@@AAAXXZ` | StandardStream | 99.85 | **100** | D: end marker (+0x14c) cleared before start (+0x138) |
| `0x827cc398` | 104 | `??1BufStream@@UAA@XZ` | BufStream | 84.42 | 92.31 | F: StreamChecksum/Validator dtors implicit (delete has no null test); residual = one scheduling slot (&mName computed ea... |
| `0x824baa00` | 100 | `?PrePoll@CameraManager@@QAAXXZ` | CameraManager | 75.80 | **100** | B: no MiloCamera() gate in PrePoll |
| `0x82514a20` | 100 | `?HasUserSigninChanged@PlatformMgr@@QBA_NPBVLocalUser@@@Z` | PlatformMgr | 84.00 | **100** | E: HasPadNumsSigninChanged returns false after MILO_FAIL on a negative pad |
| `0x8251c1a0` | 100 | `?IsPadAGuest@PlatformMgr@@QBA_NH@Z` | PlatformMgr_Xbox | 100.00 | **100** | -: fuzzy 100 on naming alone |
| `0x827c8de8` | 100 | `?AddBuf@StringTable@@AAAXH@Z` | StringTable | 72.60 | 99.80 | F: no MemTemp scope; residual = push_back fold name |
| `0x823f6198` | 92 | `??0XboxSessionJob@@QAA@PAX@Z` | SessionJobs_Xbox | 85.83 | **100** | F: no mSuccess init (native keeps) |
| `0x82526660` | 88 | `?OnJoypadStageKitRaw@?A0x47b254f1@@YA?AVDataNode@@PAVDataArray@@@Z` | Joypad | 85.36 | **100** | E: forwards to stage-kit helpers ported from retail 0x82524D40/0x82524DE0 (names ours, unmapped) |
| `0x8275d998` | 88 | `?DataEq@@YA?AVDataNode@@PAVDataArray@@@Z` | DataFunc | 89.68 | **100** | C: DataNode::operator== |
| `0x82761678` | 88 | `?OnFileExists@@YA?AVDataNode@@PAVDataArray@@@Z` | DataFunc | 95.00 | **100** | C: 2-arg FileExists |
| `0x82784d18` | 84 | `?HandleFillEnd@SongParser@@QAA_NHE@Z` | SongParser | 87.38 | 87.38 | A: not changed: retail re-normalizes CheckDrumFillMarker result (narrow non-bool return in retail); a (unsigned char) ca... |
| `0x8251be30` | 80 | `?IsSignedIntoLive@PlatformMgr@@QBA_NH@Z` | PlatformMgr_Xbox | 100.00 | **100** | -: fuzzy 100 on naming alone |
| `0x8277b888` | 80 | `?IsFinished@MasterAudio@@QBA_NXZ` | MasterAudio | 76.45 | **100** | A: plain && |
| `0x827876c8` | 80 | `?OnMidiMessageCommonOff@SongParser@@QAA_NHE@Z` | SongParser | 85.00 | 85.00 | A: not changed: same bool re-normalization of HandleFillEnd result |
| `0x8278ebc8` | 48 | `?GetLowestString@GameGem@@QBAIXZ` | system/beatmatch/GameGem | 99.92 | **100** | A: GameGem layout (frets at 0x32) |
| `0x8278ebf8` | 44 | `?GetHighestString@GameGem@@QBAIXZ` | system/beatmatch/GameGem | 99.91 | **100** | A: GameGem layout (frets at 0x32) |
| `0x82790a90` | 24 | `?SetSyncOffset@BeatMatcher@@QAAXM@Z` | BeatMatcher | 83.33 | 83.33 | A: not source: body identical, retail row carved 4 B short (trailing blr in next symbol) -- split issue |
| `0x82b6c7a0` | 16 | `?SampleAlloc@@YAPAXH@Z` | system/synth_xbox/SynthSample | 100.00 | **100** | -: fuzzy 100 on naming alone |


## 8. Reproduce

Scratch lives in `~/tmp/w16hn/` and is not committed: the matrix, proposals, adjudications,
buckets, per-fork results and snapshots.

```
python3 ~/tmp/w16hn/matrix.py matrix.json 14                  # 1,631 targets x 20 shape-nearest candidates
python3 ~/tmp/w16hn/propose.py matrix.json 70 1 props.json     # 267 proposals
python3 tools/anon_proposal_adjudicate.py props.json --independent --json-out adj_ind.json
python3 ~/tmp/w16hn/bucket.py                                  # buckets; drift rule in drift_rule.json
python3 tools/gated_map_write.py --target scripts/target_symbol_map.json --rows-json acc_rows.json
python3 tools/icf_alias_finder.py --validate && python3 tools/map_name_injectivity.py
python3 tools/ab_measure.py --worktree <main + 281f99e5a> --patch <281f99e5a..w16-hn>
```
