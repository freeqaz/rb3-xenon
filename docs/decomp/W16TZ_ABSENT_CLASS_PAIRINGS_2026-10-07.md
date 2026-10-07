# W16-TZ: rows that paired only through code retail does not contain

Lane W16-TZ, 2026-10-07. Worktree `~/tmp/wt-w16tz`, branch `w16-tz`, base main `b3015fcb2`.

This lane finishes what W16-TU (`W16TU_FLOW_UNITS_REHOMED_2026-10-07.md`) and W16-TX
(`W16TX_GESTURE_HAMOBJ_UNITS_REHOMED_2026-10-07.md` §3, §6) left open:

- the seven Flow scatter includes that sit in real units;
- the 12 B CharPosConstraint pin at `0x823C3354`;
- a sweep of every unit in `config/45410914/splits.txt` for rows that pair only through code retail
  does not contain.

The short version:
- **All seven Flow include sites are settled (§1).** Six were removed. The seventh, HamCamTransform, is
  itself a retail-absent file with no split unit, so it can pair nothing and was left.
- **CharPosConstraint now holds only its own code (§2).** Its three `.text` slivers moved to the units of
  the functions they precede.
- **The sweep found two populations (§3, §4):**
  - **58 rows spell a class with no `/GR` descriptor in retail.** 8 were fixed. The other 50 have a
    recorded reason; none of them is absent code.
  - **20 scatter includes in real units pulled in a retail-absent `.cpp`.** All 20 were removed.
    The 8 rows that had paired only through them now pair in their own region's unit.
- **Price: 7 `tools/ab_measure.py` runs, one change each. Net +1 fn / +60 B** (§6). Every step's
  prediction is recorded. Units at 100% go 611 → 607; the four lost are removed units.
- **Native:** removing the includes broke the rb3-milo/rb3-render links. They had been getting the Flow
  bodies from those includes. The fix lists those bodies directly in the native build (§5):
  `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.

## 0. Instrument

Retail is built `/GR`, so a polymorphic class that is linked in has a `.?AV`/`.?AU` type descriptor.
The scan reads retail `band.exe` (clean TU5) in Python; the shell `grep` cannot match binary files. It
builds two sets:
- retail's descriptor set;
- our descriptor set, read from every compiled `.obj`.

Before comparing, it normalises `V` against `U` (class vs struct) and anonymous-namespace `?A0x…` hashes.

- **Ours minus retail: 494 descriptors, 287 plain classes and 207 template instantiations.**
- **The template axis is excluded, with a reason.** Retail spells `ObjPtr<T,ObjectDir>` and
  `ObjOwnerPtr<T,ObjectDir>` with two template arguments where our DC3-derived headers use one.
  Every such descriptor differs by arity alone. On that axis 818 rows hit, and they measure the arity
  difference, not absent code.
- **Row sweep:** every non-`auto_`, non-Quazal row with a nonzero score whose demangled name names one
  of the 287 classes. Script: `~/tmp/w16tz_sweep/rowsweep.py`.
- **Include sweep:** every `#include "<x>.cpp"` in a split unit's source, followed transitively, whose
  target file defines one of the 287 classes.

A name sweep cannot see a row that is spelled with a present class but whose body reaches the unit's
object only through an absent `.cpp`. The A/B of the include removal (§4) catches those.

## 1. The seven Flow include sites

W16-TX §3 listed them. The probe for each: compile the TU with and without the include (`OBJCACHE=off`,
same `/Fo` on both legs), then list which of the unit's own rows lose their base symbol.

| unit | include removed | rows that lost a base symbol | disposition |
|---|---|---|---|
| GemManager | `flow/FlowManager.cpp` | `MakeString<float,float,float>` @ `0x82272240` (112 B) | instantiated directly in GemManager's `#ifndef HX_NATIVE` force-emit block |
| BandDirector | `flow/FlowNode.cpp` | none in BandDirector; PropKeys' `ObjectDir::Find<Hmx::Object>` @ `0x82270438` paired through PropKeys → BandDirector.cpp → FlowNode.cpp | re-homed to App (step 2): it sits in App's region and App.obj defines it |
| Morph | `flow/FlowNode.cpp` (+ its native note) | none | removed |
| EventTrigger | `flow/FlowTrigger.cpp`, `flow/Flow.cpp` (+ dangling header comment) | none | removed |
| UI | `flow/FlowQueueable.cpp` | none | removed |
| Cheats | `flow/Flow.cpp` | none | removed |
| HamCamTransform | `flow/Flow.cpp`, `flow/FlowTrigger.cpp` | n/a | **left.** HamCamTransform is a Dance Central class with no RTTI in retail and no `splits.txt` unit (W16-TX removed the hamobj units). Its object pairs no row, so the include cannot pair anything. |

Cheats is a PCH TU, so `tools/gate_liveness.py` hit a `-Zm` inconsistency. Its probe was compiled by
hand from `ninja -t commands`: `/DW16TZ_NOFLOW`, `OBJCACHE=off`, the same `/Fo`.

The per-host probe missed the PropKeys row because the chain is transitive. The A/B caught it
(−1 fn / −136 B). Step 2 fixed it.

## 2. CharPosConstraint's slivers

Retail bytes were decoded at each sliver. Each is the padding before a function, and in the first and
third cases also that function's 8-byte EH prefix. A sliver belongs to the unit of the function that
follows it.

| range | content | moved to |
|---|---|---|
| `0x823C3354-0x823C3360` | 4 B pad + EH prefix of `CharForeTwist::Handle` | CharForeTwist |
| `0x823C3454-0x823C3458` | pad before `CharBlendBone::Poll` | CharBlendBone |
| `0x823C35CC-0x823C35D8` | pad + EH prefix of CharBlendBone's `PropSync(ConstraintSystem&)` | CharBlendBone |

No rows; measured +0 / +0 B as predicted.

## 3. Rows spelled with a class retail does not contain (58)

### 3.1 Fixed (8)

Each was found by a reloc-masked byte search against the object of the unit whose region holds it.

| address | old unit, old name | now | evidence |
|---|---|---|---|
| `0x822A20C8` | Spline, `operator<<(vector<RndSpline::CtrlPoint>)` (87.52) | OutfitConfig, `operator<<(vector<OutfitConfig::Piercing>)` (100) | body match; the CtrlPoint membership REFUTED and withdrawn |
| `0x822DD188` | Font3d, `_M_find` on `map<ushort,RndFont3d::CharInfo*>` | ChordShapeGenerator, `_M_find` on `map<ushort,ushort>` | body match in the region owner |
| `0x82535708` (+ funclet) | AsyncFileHolmes, `~AsyncFileHolmes` | AsyncFile_Win, `??1AsyncFileWin@@UAA@XZ` | the dtor stores AsyncFileWin's vtable |
| `0x82B6C830` | system/net/HttpReq, `HttpReq::HasSucceeded` | system/synth_xbox/SynthSample, `SynthSample360::IsXMA` | body match in the region owner |
| `0x827AB2C0` | DingoSvr, `WebSvcRequest::GetBaseURL` | FixedSizeSaveableStream, `GetSymbolCount` | body match in the region owner |
| `0x82638C10-0x82638CC0` | PropKeys, `~LocalePanel::Entry` + 2 funclets | StoreInfoPanel, `~StoreInfoPanel::RecommendedEntry` | byte-identical to StoreInfoPanel's own instantiation |
| `0x82815040` | UIListSlot, `for_each<PoseElement**,Delete>` | same unit, `for_each<UIListSlotElement**,Delete>` | byte-identical |
| `0x8258FDE8` | AccomplishmentProgress, `for_each<list<WebSvcRequest*>>` | same unit, `for_each<list<GamerAwardStatus*>>` | byte-identical |

The five units Spline, Font3d, AsyncFileHolmes, system/net/HttpReq and DingoSvr each held one block,
inside another unit's region. They are removed from `splits.txt`. Every map rename went through
`tools/alias_survivor_relabel.py --write` and `tools/alias_callee_name_drift.py --reprove --write`
after a build.
- Relabels: 4 + 21 folded memberships kept PROVEN; old labels folded PROVEN except the two noted.
- Callee re-proves: 8 memberships (7 PROVEN, 1 UNDECIDABLE carried) and 48 (all PROVEN).
- `icf_alias_finder.py --validate` PASS after each step.

### 3.2 Recorded (50), re-swept on the final tree

| class | rows | units | reason it stays |
|---|---|---|---|
| `ObjRefOwner` | 23 | Object ×2, MatAnim ×3, and the `ObjOwnerPtr<T>` ctors in 18 units | **Retail code, DC3 spelling.** RB3's class is `ObjRef` (`.?AVObjRef@@` is in retail). Retail's `??_G` at `0x822702A8` sits in that vtable. Fixing the spelling is an engine-wide `ObjRefOwner` → `ObjRef` rename, not a per-row fix, and was not attempted. |
| `PracticeSection` | 13 | PracticeSectionProvider ×6, SongDB ×3, Player, GemPlayer, VocalPlayer, AppLabel | **False positive: name collision.** The absent descriptor is DC3's polymorphic `hamobj/PracticeSection`. RB3's `PracticeSection` (`PracticeSectionProvider.h:5`) has no virtual functions, so it has no RTTI either way. |
| `EnterFlowMsg`, `JoinEntryPointEvent` | 6 + 1 | WaitingUserGate | **False positive: present in retail** as `.?AVEnterFlowMsg@?A0x5b3730ba@@` and `.?AVJoinEntryPointEvent@?A0x5b3730ba@@`. Our copies are outside the anonymous namespace; the classes are retail's. |
| `FixedString` | 4 | Str | **Not testable by RTTI**: the class has no virtual functions. The rows are retail bytes at 100 in Str's region. Whether RB3 called the class `FixedString` (a DC3 name) is unknown. |
| `StringStoppedMsg` | 1 | UsbMidiGuitarMsgs | **Not testable by RTTI**: a `DECLARE_MESSAGE` class with no virtual functions of its own. The row is its static `Type()`, which retail has. |
| `BinkMovieSys` | 2 | BinkMovieSys_Xbox | `PlatformStoreCache` (40 B, `0x82746128`) and `PlatformInit` (24 B, `0x82746150`) are static functions. Their bodies match only `BinkMovieSys_Xbox.obj`. They sit in the Movie/TexMovie region. Retail has no `BinkMovieSys` descriptor, but no source in this tree or DC3 names these two functions, so there is nothing to re-home them to. Kept. |

Re-running the sweep on the final tree finds exactly these 50 rows / 4,936 B: 58 found originally,
minus the 8 fixed.

## 4. Scatter includes of retail-absent `.cpp` files (20)

| host | removed include(s) |
|---|---|
| band3/game/Stats.cpp | `hamobj/RhythmDetector.cpp` |
| band3/game/TrainerPanel.cpp | `gesture/SpeechMgr.cpp` |
| band3/meta_band/AccomplishmentProgress.cpp | `net/WebSvcMgr.cpp` |
| bandobj/BandLabel.cpp | `hamobj/HamLabel.cpp` |
| bandobj/CrowdAudio.cpp | `hamobj/HamMove.cpp` |
| bandobj/BandDirector.cpp | `gesture/SkeletonHistory.cpp` |
| beatmatch/RealGuitarTrackWatcherImpl.cpp | `hamobj/HamRibbon.cpp` |
| char/CharLipSync.cpp | `gesture/SkeletonClip.cpp` |
| meta/StorePanel.cpp | `hamobj/DancerSequence.cpp` |
| rndobj/PartAnim.cpp | `hamobj/HamSupereasyData.cpp` |
| rndobj/PropKeys.cpp | `hamobj/StarsDisplay.cpp`, `hamobj/FilterQueue.cpp` |
| rndobj/EventTrigger.cpp | `gesture/SkeletonClip.cpp` |
| rndobj/Morph.cpp | `hamobj/HamMove.cpp` (+ header comment) |
| synth/ByteGrinder.cpp | `hamobj/HamBattleData.cpp`, `gesture/SkeletonClip.cpp` |
| track/TrackWidget.cpp | `char/ClipCollide.cpp` |
| ui/UIListSlot.cpp | `hamobj/Pose.cpp` |
| world/CameraShot.cpp | `hamobj/DanceRemixer.cpp` |
| world/LightPreset.cpp | `ui/LocalePanel.cpp` |
| ui/UI.cpp | the "SCAFFOLD -- LocalePanel COMDAT donor" block and `#include "ui/LocalePanel.h"` (no LocalePanel row was left to donate to) |

Not removed: the `rndobj/Line.cpp` includes in Anim, MatAnim, ShaderOptions, TrackDir and
UITransitionHandler. The scan flagged them for `.?AVLine@@`, but `Line.cpp` defines `RndLine`, and
`.?AVRndLine@@` is in retail. After this lane, no split unit's source includes a hamobj, gesture or flow
`.cpp`, `net/WebSvcMgr.cpp`, `ui/LocalePanel.cpp` or `char/ClipCollide.cpp`. The only remaining includers
are files inside those directories.

**The removal alone measured −8 fns / −1,428 B against a prediction of 0.** Eight rows had paired only
through the removed bodies. Each was adjudicated with `tools/retail_callers.py` and the alias groups,
then moved to the unit whose region holds it, under a name that unit's object defines:

| address | size | was | now | why this owner and spelling |
|---|---|---|---|---|
| `0x82270340` | 100 | PropKeys, `ObjDirPtr<ObjectDir>::PostLoad` | App, same name | App's region (between `App::Run` and App's `ObjDirPtr` code); App.obj defines it. Block `0x822702F0-0x822703A4` split at `0x82270340`. |
| `0x82270418` | 28 | PropKeys, `ObjDirPtr<ObjectDir>` ctor | App, same name | as above |
| `0x8268C920` | 84 | PropKeys, `set<int>::erase` | BandUser, `set<TrackType>::erase` | BandUser's region; T1 fold member; BandUser.obj defines it |
| `0x8268C998` | 80 | PropKeys, `set<int>::erase_unique` | BandUser, `set<TrackType>::erase_unique` | its **only** retail caller is `LocalBandUser::SetShownIntroHelp(TrackType)`, so the spelling is not arbitrary |
| `0x8233CAB8` | 160 | Rot, `map<int,int>::operator[]` | BandList, `map<int,BandList::AnimState>::operator[]` | BandList's region; 33 retail callers, all in BandList; T1 fold member (with RevealState and TrackType). The name among members is arbitrary; AnimState is one BandList.obj defines. |
| `0x82440830` | 356 | rndobj/Utl, `vector<Vector3>::operator=` | same unit, `vector<LightPreset::SpotlightDrawerEntry>::operator=` | T1 fold member that Utl.obj defines (Utl reaches LightPreset through UIListDir.cpp → LightPreset.cpp, a retail class) |
| `0x824AFFF8` | 472 | LightPreset, `vector<Vector3>::_M_fill_insert_aux` | same unit, SpotlightDrawerEntry spelling | T1 fold member; LightPreset's own `PropSync`/`Load`/`Keyframe` are among the callers |
| `0x824B2478` | 108 | LightPreset, `vector<Vector3>::_M_fill_insert` | same unit, SpotlightDrawerEntry spelling | as above |

**The second A/B missed its prediction too: −4 fns / −1,136 B against 0 / −40 B.**
- The four re-homed rows came back at 100.
- The four respelled bodies paired but read 99.81–99.96. Each had one callee-name charge: our spelling
  calls the SpotlightDrawerEntry or AnimState twin of a callee that retail's map names by its
  Vector3 or `<int,Symbol>` survivor.

The four callee pairs were chased on retail bytes with `tools/icf_pair_adjudicate.py --chase`:

| callee group (retail address) | admitted spelling | FLAT T1 | CHASED T1 |
|---|---|---|---|
| `_M_insert_overflow_aux<Vector3>` (`0x82787C00`) | SpotlightDrawerEntry | REFUTED (relocation targets are the twin's) | PROVEN, 0 cycle-assumed |
| `_M_allocate_and_copy<Vector3>` (`0x827877D0`) | SpotlightDrawerEntry | REFUTED (same) | PROVEN, 0 cycle-assumed |
| `__uninitialized_fill_n<Vector3>` (`0x823E3C40`) | SpotlightDrawerEntry | PROVEN | PROVEN |
| `insert_unique<int,Symbol>` (`0x827C68C0`) | `<int,AnimState>` | REFUTED (same) | PROVEN, 0 cycle-assumed |

Each spelling was added to its group's `folded` list, with an `admitted` record giving the evidence and
the caller. W16-NF had already admitted `<int,int>` to the last group, citing call site `0x8233CB3C`,
which is inside the row moved here. The callee names were re-proved (4 groups, 8 memberships PROVEN) and
`--validate` passed.

**Third A/B: predicted 0 fns / −40 B, measured +0 / −40 B.** The −40 B is the funclet `fn_8248F818`
(AmbientOcclusion, the EH funclet of `kdTree<Triangle>::kdTree`):
- objdiff pairs funclets by byte signature;
- with fewer candidate funclets in AmbientOcclusion.obj, it now pairs with one that calls `~FilePath`;
  retail's calls the `~_Vector_base<StreakList>` survivor;
- `mpn` stays 100, so the function count is unchanged.

That pick is objdiff's choice among byte-equal funclets, not a source defect, so it is recorded and was
not fixed.

## 5. Native

The X360 removals were also how rb3-milo and rb3-render got the Flow bodies. The GemManager, BandDirector,
EventTrigger and UI includes were not `HX_NATIVE`-gated.
- The first gate run failed 16/18 with 268 linker diagnostics: FlowNode, FlowManager, `typeinfo for Flow`,
  and `typeinfo for FlowQueueable` (needed by `UISlider`'s `ObjDirItr<Flow>` and the W16-TM flow gates).
- Fix: `Flow.cpp`, `FlowNode.cpp`, `FlowManager.cpp`, `FlowQueueable.cpp` and `FlowTrigger.cpp` are now
  listed in `MILO_TARGET_COMMON_SOURCES`.
- The CriticalUserListener note at `native/CMakeLists.txt` ~L1431 used to say FlowManager arrives through
  GemManager. It now points at that list.

The hamobj and net TUs in `L4_SCATTER_WIRE` used to be deduplicated against their removed includers.
They now compile standalone, which is what that list asks for.

Gate, run after the last `src/` or `native/` change:
`NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0`.

## 6. Price

Each row is one `tools/ab_measure.py` run in this worktree: `name_check` ruler, both legs settled to zero
work, splits and map changes read at a `symbols.txt` fixed point. Run dirs are under
`~/tmp/wt-w16tz/.ab_measure_runs/`.

| step | commit | kind | predicted | measured | units at 100% (mpn) |
|---|---|---|---|---|---|
| 1. six Flow includes; GemManager `MakeString` instantiation | `f9a20ed9e` | source | 0 | **−1 fn / −136 B** (PropKeys `Find<Hmx::Object>`) | 611 → 611 |
| 2. `0x82270438` PropKeys → App | `bec60d48b` | splits | +1 / +136 B | **+1 / +136 B** | 611 → 611 |
| 3. CharPosConstraint slivers | `de3f14051` | splits | 0 | **+0 / +0 B** | 611 → 611 |
| 4. five absent-class units removed, rows re-homed and respelled | `150276e42` | map + splits | +1 / +100 B | **+1 / +100 B** (Spline row 87.52 → 100 in OutfitConfig) | 611 → 607 (the four removed units were at 100) |
| 5. three respellings + StoreInfoPanel re-home | `83b469609` | map + splits | 0 | **+0 / +0 B** (StoreInfoPanel +3 rows, PropKeys −3) | 607 → 607 |
| 6a. 20 absent-class includes, removal alone (not committed) | | source | 0 | **−8 / −1,428 B** | 607 → 607 |
| 6b. + 8 rows re-homed/respelled (not committed) | | source + map + splits | 0 / −40 B | **−4 / −1,136 B** (respelled bodies short one callee each) | 607 → 606 |
| 6c. + 4 callee admissions | `d282256a6` | source + map + splits | 0 / −40 B | **+0 / −40 B** | 607 → 607 |
| 7. native Flow sources | `67e50618a` | native only | not an X360 input | not run | |
| **total (1–5, 6c)** | | | | **+1 fn / +60 B** | **611 → 607** |

Step 4's first run was REFUSED by `CHECK ALIAS SURVIVORS VS MAP`. Every later map step did
build → relabel → build → re-prove → build before measuring.

## 7. Not done

- The `ObjRefOwner` → `ObjRef` rename (23 rows, §3.2). It is engine-wide, the rows already score 100, and
  it is a naming fix, not a pairing fix.
- `BinkMovieSys_Xbox`'s two static functions (§3.2). They need an identification of the retail owner.
- The `fn_8248F818` funclet re-pairing (−40 B, §4).
- The template-descriptor axis (§0), excluded as an arity artifact.
- `0x82272A60` is still unpinned (W16-TX §4). Tour's missing `vector<Symbol>` instantiation (W16-TX §2.3)
  is untouched.

Merging and pushing are left to the coordinator.
