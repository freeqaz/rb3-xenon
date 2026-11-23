# Native-only additions in src/system: one-sided `HX_NATIVE` blocks judged against retail (2026-11-23, lane `narms5`)

Lane `narms4` (`NATIVE_ONLY_ADDITIONS_BAND3_2026-11-23.md`) judged the one-sided native-only
blocks in `src/band3`. This lane does the same for `src/system`, outside the areas another
session owns (rndobj, ui, char, world `Spotlight*`, net sync). The question: where does a
`#ifdef HX_NATIVE` / `#if defined(HX_NATIVE)` / `#if defined(MILO_DEBUG) && defined(HX_NATIVE)`
block with no `#else`, in a file some native target compiles, make native do something retail
(`band.exe`, TU5, read with `tools/va_disasm.py`) does not?

## Method

- **Native closure.** `~/tmp/narms3/mdeps.json` (lane narms3's `clang -MM -MG` over all 5,054
  native compile commands) filtered to `src/system/`: **1,445 files**. A fresh configure of this
  worktree (`~/tmp/narms5/nb`) has the same 5,054 commands over the same 1,054 sources, so the
  scan is current up to header edits since 00:32.
- **Enumeration.** narms4's `arms.py` over `src/system` (`~/tmp/narms5/arms.py`): 2,236
  `HX_NATIVE` conditionals, 2,013 in the closure, **1,092 one-sided native-true**. Of those,
  **478 sit in owned areas** (rndobj 225, ui 124, char 116, world `Spotlight*` 13; no
  `src/system/net` file matches the net sync code) and were listed, not judged. The other
  **614** (554 `HX_NATIVE`, 60 `MILO_DEBUG && HX_NATIVE`) were judged; `~/tmp/narms5/dump.txt`
  holds each with context, block ids follow `~/tmp/narms5/one.json`.
- **Retail reference and verdict classes** as in narms4: a block is a divergence when it
  changes state, calls or control flow on a path the app takes with default settings.
  Env-gated logs, dev cheats defaulting off, script handlers retail lacks, harness-only code and
  build/linkage lines are recorded, not changed.

Verdicts were drafted per directory group by three read-only passes (`~/tmp/narms5/verdict_{A,B,C}.md`: obj; bandobj/world/meta/movie/flow/hamobj; utl/synth/os/beatmatch/net/math/midi/dsp/track/gesture) and merged here; the fixes were checked against retail again before commit.

## Fixed (commit `332760fdf`; native now runs the retail body)

| block(s) | site | retail | native before |
|---|---|---|---|
| #525 | `world/CameraShot.cpp` `CamShotFrame::BuildTransform` | 0x824BF368: `GetCurrentTargetPosition` (bl 0x824BF278) straight into `WorldToScreen` (bl 0x824333A0), no zero/empty-target test | every target-less shot returned `tf = mWorldOffset`, skipping the `mPath` transform, the parent transform, the clamp height and the `b3` tail. The NaN case it was written for is still covered by the screenPos NaN guard (#526, kept) |
| #497 | `utl/Loader.cpp` `LoadMgr::PollUntilLoaded` | fn_827BF608: the only store to `this+0x14` is the in-loop `stfs f31,0x14(r29)` (1e30); no restore | DC3's save/restore of `mCurrentPeriod`, so a nested `PollUntilLoaded` re-budgeted the rest of that frame's `LoadMgr::Poll` to 10 ms where retail leaves it unbudgeted |
| #449 | `utl/Cheats.cpp` `CheatsInit` | 0x827C3208: `KeyboardSubscribe`, then `SystemConfig("long_cheats")` / `InitLongJoyCheats`; `quick_cheats` absent from band.exe | dev-build block read `quick_cheats` and armed the L/R shoulder-shift cheats (RB3 data maps R1+R2+Select to `{fake_win 3}`), with cheats on by default; with `_SHIP` and the body compiled out it would pass null to `InitQuickJoyCheats` |
| #47 | `meta/PreloadPanel.cpp` `Load` | 0x827B4880: `UIPanel::Load` straight to the `mPreloadResult` store | DC3's `TheLoadMgr.SetLoaderPeriod(14.0f)`, never restored |
| #48 | `meta/PreloadPanel.cpp` `PollForLoading` | 0x827B49E8: `DoneCaching` straight to the `FileCached` loop | extra `FileCache::PollAll()` (SystemPoll still polls every cache once a frame) |
| #49 | `meta/PreloadPanel.cpp` `StartCache` | 0x827B4668: `Clear` then `StartSet` | extra `SetSize(gMaxCacheSize)`, whose EndSet sorted and polled the cache between the two. Two stale comments that said StartCache re-applies the size were corrected |
| #41 | `meta/StorePreviewMgr.cpp` `Handle` | 0x827B2360: no bl to `MsgSource::Handle` (0x82768318) or any superclass Handle | unhandled messages forwarded to `MsgSource::Handle` (add/remove-sink dispatch, export to sinks) |
| #554, #555 | `bandobj/TrackPanelDirBase.cpp` `SetConfiguration` / `ReapplyConfiguration` | 0x823574B0 / 0x82357640: one call (`Handle`), then return | oracle's `if (!mPerformanceMode) SetShowing(gShowHUD)` after every configuration apply, kept in the native arm when the X360 arm was matched (`4d49e2375`) |
| #587, #588 | `bandobj/BandTrack.cpp` `Reset` / `SoloStart` | 0x82352828 / 0x82351808: no `solo_percent.lbl` access; `SoloHit` is its only writer | zeroed `solo_percent.lbl` via `SetTokenFmt(me_percent_format, 0)` (since `304c72705`) |

Native compile check: `-fsyntax-only` on every native compile command for the seven touched
TUs plus the four TUs that `#include` one of them (`rndobj/Lit_NG.cpp`, `synth/Sfx.cpp`,
`utl/MeasureMap.cpp`, `world/LightPreset.cpp`), against a fresh configure of the worktree
(`~/tmp/narms5/nb`, `~/tmp/narms5/synchk.py`): 21 + 10 commands, 0 failures. None of the
touched files uses `__LINE__` (asserts carry hex literals), so the line shifts are inert for
the match build.

Not done: no native binary was run and `tools/native_health.sh --selftest` was not run (both
excluded by the brief). The next lane should run the app through `no610m.sh` and check: (1)
venue camera shots with no targets (#525 changes every such shot's transform), (2) that the
HUD still shows after a track configuration apply now that #554/#555 no longer re-show it
(narms4 kept `GamePanel::StartGame`'s forced `SetShowing(true)` for the late `play_intro`),
(3) song preload time, since #47 drops the 14 ms loader period during preload.

## Flagged and left (owned elsewhere, or the fix needs a root cause first)

- **Char session:** `bandobj/BandWardrobe.cpp:660` (#607) maps the singer's `mic` to `vocals`
  when naming venue slots; retail 0x82330040 maps only `none` to `vocals`, so it builds
  `player_mic0`. `bandobj/BandCharacter.cpp` #569/#570 rebind skeleton and skin after
  `Character::Poll` by default (retail has neither); #582/#583 null-directory guards hide the
  female head directory never loading natively.
- **Native object model / file merger (obj):** #159, #305, #308 object-lookup fallbacks
  search the loader's parent and proxy dirs (and the main dir on load) where retail returns
  null, working around the incomplete native file merge; #149/#150 and #205/#206 keep the
  target's subdirs on a replace-style `MergeDirs` where retail `ObjectDir::Copy`
  (0x82750F80) replaces them; #304 erases a duplicate `ObjPtrVec` entry after a redirect
  (retail keeps both); #217 deletes a task started during directory teardown; flow #22-#24
  skip deactivation / child deletes during the teardown cascade (DC3-only classes).
- **Inert or near-inert dev policy (obj):** #252/#253 `disable_notify` / `filter_notify` run
  their bodies natively, retail registers both at a shared `return 0` (0x8228D358); no
  shipped script uses `disable_notify`, the one `filter_notify` is in the editor resave flow.
  #283/#298 DC3's 8-slot property-path pool vs retail's one static array (differ only on
  re-entrant `SetProperty`).
- **Fail policy (os):** #321 `Debug::Fail` is non-fatal natively. On the default path it
  matches retail (main thread, outside a try, returns); it differs inside `MILO_TRY`, on a
  worker thread, and in fail callbacks. Engine-wide policy, left.
- **Other:** #469 `BinStream >> String` aborts on a length over 10000 (retail reads it);
  #346 `Sequence` dtor skips deleting instances during directory teardown; #320 the
  main-thread notify list `gNotifies` grows without bound (memory only); #51 MoviePanel
  skips the movie-ready wait because the native movie stub never reports ready; #76 no
  splash worker thread natively (the threaded path would block in `WaitForState`).
- **Rendering (Rnd/Draw session):** #612 `track/TrackWidgetImp.cpp:348` queues the dirty text mesh to `RndText`'s deferred set before `DrawShowing` (retail draws directly). #78 the non-threaded splash draw pass.
- Two small notes from the obj pass: `TaskMgr`'s native ctor allocates the timelines and
  `Init` allocates them again (leaks 4, no behaviour change); `DirLoader::Find` always prints
  up to about 8 stderr lines for "director" lookups.

## Not divergent (recorded)

Everything else does not run with default settings or does not change behaviour: build lines
(includes, declarations, layout, linkage, LP64 union zeroing in `obj/Data.h`), env-gated logs,
dev checks that only print (natively `MILO_FAIL`/`MILO_ASSERT` print and continue unless
`MILO_FATAL_FAILS=1`), dev cheats and options defaulting off, script handlers retail lacks,
extra `SYNC_SUPERCLASS(Hmx::Object)` chains that only expose name/type/sinks properties
(retail `ObjectDir::SyncProperty` 0x82752A60 calls no superclass sync; 8 sites in chunk C
too), opt-in MemTracker reporting, native audio/IO plumbing, null and bad-data guards that
fire only where retail would crash, inert machinery (`sPathEval` never set,
`mHasEditorDir` never true, `QueueTaskDelete` never called), no-chart guards in
`BeatMatcher`, Holmes/CD/BlockMgr blocks inert when running from the ark, and DC3-only flow
classes absent from band.exe. Equivalents: `Crowd` Load's extra `Reset3DCrowd` (#537)
changes nothing on a fresh load; #539's random-colour call cannot fire (native Load forces 3D
crowds off); `LightPreset`'s extra list clear (#546); `BandDirector`'s `motion_blur` read
(#598, stored, never read) and `Replace` superclass calls (#599, land on a native no-op).

## Owned areas: listed, not judged (478 blocks)

Counts are one-sided native-true blocks per file in the native closure. No `src/system/net`
file matches the net sync code (that lives in `src/network`), so no net block is excluded.

- **char** (116): `char/Char.cpp` 6, `char/CharBlendBone.cpp` 1, `char/CharBoneTwist.cpp` 1, `char/CharBones.cpp` 1, `char/CharBonesMeshes.cpp` 3, `char/CharBonesSamples.cpp` 3, `char/CharBonesSamples.h` 1, `char/CharClip.cpp` 7, `char/CharClip.h` 3, `char/CharClipGroup.cpp` 1, `char/CharCollide.cpp` 1, `char/CharCuff.cpp` 1, `char/CharDriver.cpp` 7, `char/CharDriver.h` 4, `char/CharEyeDartRuleset.cpp` 1, `char/CharEyes.cpp` 13, `char/CharEyes.h` 1, `char/CharFaceServo.cpp` 1, `char/CharForeTwist.cpp` 4, `char/CharGuitarString.cpp` 1, `char/CharHair.cpp` 1, `char/CharIKFingers.cpp` 1, `char/CharIKFoot.cpp` 1, `char/CharIKHand.cpp` 2, `char/CharIKHead.cpp` 3, `char/CharIKHead.h` 1, `char/CharIKMidi.cpp` 1, `char/CharIKRod.cpp` 1, `char/CharIKScale.cpp` 1, `char/CharIKSliderMidi.cpp` 1, `char/CharInterest.cpp` 1, `char/CharLipSync.cpp` 1, `char/CharLipSyncDriver.cpp` 1, `char/CharMeshCacheMgr.h` 1, `char/CharMeshHide.cpp` 1, `char/CharMirror.cpp` 1, `char/CharNeckTwist.cpp` 2, `char/CharPollGroup.cpp` 2, `char/CharPosConstraint.cpp` 1, `char/CharServoBone.cpp` 1, `char/CharSleeve.cpp` 1, `char/CharTaskMgr.cpp` 1, `char/CharTransDraw.cpp` 1, `char/CharUpperTwist.cpp` 3, `char/CharWeightSetter.cpp` 1, `char/CharWeightable.cpp` 1, `char/Character.cpp` 5, `char/Character.h` 1, `char/CharacterTest.cpp` 2, `char/ClipCollide.cpp` 1, `char/FileMerger.cpp` 7, `char/FileMerger.h` 3, `char/FileMergerOrganizer.cpp` 1, `char/Waypoint.cpp` 2
- **rndobj** (225): `rndobj/AmbientOcclusion.cpp` 4, `rndobj/AnimFilter.cpp` 1, `rndobj/BaseMaterial.cpp` 1, `rndobj/Cam.cpp` 12, `rndobj/Cam.h` 1, `rndobj/CamAnim.cpp` 2, `rndobj/Console.cpp` 6, `rndobj/CubeTex.cpp` 1, `rndobj/DOFProc.cpp` 2, `rndobj/DOFProc.h` 1, `rndobj/DOFProc_NG.cpp` 1, `rndobj/Dir.h` 1, `rndobj/Draw.cpp` 1, `rndobj/Env.cpp` 1, `rndobj/EnvAnim.cpp` 1, `rndobj/Env_NG.cpp` 2, `rndobj/EventTrigger.cpp` 2, `rndobj/Flare.cpp` 2, `rndobj/Font.cpp` 2, `rndobj/Fur.cpp` 1, `rndobj/Gen.cpp` 1, `rndobj/Group.cpp` 1, `rndobj/HiResScreen.cpp` 1, `rndobj/Line.cpp` 2, `rndobj/Lit.cpp` 1, `rndobj/LitAnim.cpp` 3, `rndobj/MatAnim.cpp` 2, `rndobj/Mesh.cpp` 4, `rndobj/Mesh.h` 2, `rndobj/MeshAnim.cpp` 1, `rndobj/MeshDeform.cpp` 1, `rndobj/Morph.cpp` 1, `rndobj/MotionBlur.cpp` 1, `rndobj/Movie.cpp` 2, `rndobj/MultiMesh.cpp` 1, `rndobj/Part.cpp` 22, `rndobj/Part.h` 7, `rndobj/PartAnim.cpp` 1, `rndobj/PartLauncher.cpp` 1, `rndobj/PollAnim.cpp` 1, `rndobj/PostProc.cpp` 2, `rndobj/PostProc.h` 1, `rndobj/PostProc_NG.cpp` 1, `rndobj/PostProc_NG.h` 1, `rndobj/PropAnim.cpp` 9, `rndobj/PropAnim.h` 1, `rndobj/PropKeys.cpp` 8, `rndobj/PropKeys.h` 9, `rndobj/Rnd.cpp` 22, `rndobj/Rnd.h` 4, `rndobj/Rnd_NG.cpp` 1, `rndobj/ScreenMask.cpp` 1, `rndobj/Set.cpp` 1, `rndobj/Shader.cpp` 15, `rndobj/ShaderOptions.cpp` 5, `rndobj/ShaderOptions.h` 1, `rndobj/SoftParticles.cpp` 1, `rndobj/Tex.cpp` 2, `rndobj/Tex.h` 1, `rndobj/TexBlendController.cpp` 1, `rndobj/TexBlender.cpp` 1, `rndobj/TexProc.cpp` 1, `rndobj/TexRenderer.cpp` 2, `rndobj/Text.cpp` 7, `rndobj/Text.h` 1, `rndobj/Trans.cpp` 13, `rndobj/Trans.h` 1, `rndobj/TransAnim.cpp` 2, `rndobj/TransProxy.cpp` 1, `rndobj/Utl.cpp` 4, `rndobj/Utl.h` 1, `rndobj/VelocityBuffer.cpp` 1, `rndobj/Wind.cpp` 2
- **ui** (124): `ui/InlineHelp.cpp` 1, `ui/PanelDir.cpp` 11, `ui/ResourceDirPtr.h` 1, `ui/Screenshot.cpp` 2, `ui/UI.cpp` 22, `ui/UI.h` 1, `ui/UIColor.cpp` 1, `ui/UIComponent.cpp` 2, `ui/UIFontImporter.cpp` 1, `ui/UIGuide.cpp` 1, `ui/UILabel.cpp` 3, `ui/UILabel.h` 1, `ui/UILabelDir.cpp` 1, `ui/UIList.cpp` 9, `ui/UIList.h` 1, `ui/UIListCustom.cpp` 3, `ui/UIListDir.cpp` 7, `ui/UIListLabel.cpp` 2, `ui/UIListMesh.cpp` 7, `ui/UIListProvider.cpp` 1, `ui/UIListSlot.cpp` 7, `ui/UIListSlot.h` 1, `ui/UIListSubList.cpp` 3, `ui/UIListWidget.cpp` 2, `ui/UIPanel.cpp` 9, `ui/UIPicture.cpp` 2, `ui/UIScreen.cpp` 21, `ui/UITransitionHandler.cpp` 1
- **world** (13): `world/Spotlight.cpp` 4, `world/SpotlightDrawer.cpp` 5, `world/SpotlightDrawer_NG.cpp` 3, `world/SpotlightEnder.cpp` 1

## Measurement

`python3 tools/ab_measure.py --worktree ~/tmp/wt-narms5 --revert 332760fdf` (leg A = the fix,
leg B = reverted; run dir `~/tmp/wt-narms5/.ab_measure_runs/20261123-023315-332760fdf-3434414`):
name_check ruler matched 55,268 / masked_equal 25,360 / honest 29,908 / code 59.665310% on both
legs, `none` ruler 55,322 / 60.032020% on both, units at 100% 635 -> 635, objdiff-cli
`sha256:ec3badc0f33323e9`. **Δ0 on every reading**; leg B recompiled 19 objects. Expected: every
edit removes lines inside an `HX_NATIVE` arm or changes comments, so the X360 code is unchanged.

## Every block

Numbering follows `~/tmp/narms5/one.json` / `dump.txt`. Paths are under `src/system/`; the line is the opening `#if` before the fixes.

| # | block | verdict |
|---|---|---|
| 0 | `beatmatch/SongData.h:107` | build: native-only vtable entry for the dc3 HxSongData base, delegates to CalcSongPos(float) |
| 1 | `beatmatch/BeatMaster.cpp:70` | diag: BEATMASTER_DBG MILO_LOG only (unconditional log, no state) |
| 2 | `beatmatch/BeatMaster.cpp:84` | diag: BEATMASTER_DBG MILO_LOG only (unconditional log, no state) |
| 3 | `beatmatch/BeatMaster.cpp:90` | diag: BEATMASTER_DBG MILO_LOG only (unconditional log, no state) |
| 4 | `beatmatch/BeatMaster.cpp:96` | diag: BEATMASTER_DBG MILO_LOG only (unconditional log, no state) |
| 5 | `beatmatch/BeatMaster.cpp:102` | diag: BEATMASTER_DBG MILO_LOG only (unconditional log, no state) |
| 6 | `beatmatch/MercurySwitchFilter.h:53` | dev diag: warn/log/dump only (`MILO_DEBUG && HX_NATIVE`); overlay write behind `gGuitarOverlay->Showing()`, off by default |
| 7 | `beatmatch/MercurySwitchFilter.h:99` | dev diag: warn/log/dump only (`MILO_DEBUG && HX_NATIVE`); overlay write behind `gGuitarOverlay->Showing()`, off by default |
| 8 | `beatmatch/PhraseAnalyzer.cpp:57` | dev diag: warn/log/dump only (`MILO_DEBUG && HX_NATIVE`) (warn/log of phrase overlap; MakeString args only) |
| 9 | `beatmatch/PhraseAnalyzer.cpp:104` | dev diag: warn/log/dump only (`MILO_DEBUG && HX_NATIVE`): Verify() reads phrase masks and only warns |
| 10 | `beatmatch/BeatMatcher.cpp:110` | harness: no-chart guard, fires only when `mWatcher` is null (chartless song in the headless drivers); a charted song runs the retail body |
| 11 | `beatmatch/BeatMatcher.cpp:123` | harness: no-chart guard, fires only when `mWatcher` is null (chartless song in the headless drivers); a charted song runs the retail body |
| 12 | `beatmatch/BeatMatcher.cpp:394` | harness: no-chart guard, fires only when `mWatcher` is null (chartless song in the headless drivers); a charted song runs the retail body |
| 13 | `beatmatch/BeatMatcher.cpp:417` | harness: no-chart guard, fires only when `mWatcher` is null (chartless song in the headless drivers); a charted song runs the retail body |
| 14 | `beatmatch/BeatMatcher.cpp:495` | harness: no-chart guard, fires only when `mWatcher` is null (chartless song in the headless drivers); a charted song runs the retail body |
| 15 | `beatmatch/BaseGuitarTrackWatcherImpl.cpp:150` | equivalent: retail leaves f1 undefined (C4716 reproduced); native returns 0.0f, callers ignore it |
| 16 | `beatmatch/VocalNoteList.cpp:427` | build: libstdc++/two-phase-lookup portability shim |
| 17 | `beatmatch/SongData.cpp:204` | dev diag: warn/log/dump only (`MILO_DEBUG && HX_NATIVE`), gated by DataVariable log_midi_file_load (unset = 0; string absent from band.exe) |
| 18 | `beatmatch/SongData.cpp:1226` | build: native-only vtable entry for the dc3 HxSongData base, delegates to CalcSongPos(float) |
| 19 | `beatmatch/SongParser.cpp:2633` | diag: MIDI_DBG MILO_LOG per text event only |
| 20 | `flow/FlowQueueable.cpp:11` | no retail: DC3-only class (class name absent from band.exe); ctor init of the native ObjPtrList mListeners (two-sided member type); layout/init only |
| 21 | `flow/FlowAnimate.cpp:207` | no retail: DC3-only class (class name absent from band.exe); robustness: null-mAnim guard; fires only when the node has no anim |
| 22 | `flow/Flow.cpp:48` | no retail: DC3-only class (class name absent from band.exe); **flagged, left (native object model)**: skips Deactivate during ObjectDir::DeleteObjects cascade (ObjPtrList nulling); the X360 arm deactivates in every dtor |
| 23 | `flow/FlowNode.cpp:26` | no retail: DC3-only class (class name absent from band.exe); **flagged, left (native object model)**: same cascade guard as #22 for ~FlowNode |
| 24 | `flow/FlowNode.cpp:37` | no retail: DC3-only class (class name absent from band.exe); **flagged, left (native object model)**: erases instead of deleting children during cascade (avoids native double delete) |
| 25 | `flow/FlowNode.cpp:138` | no retail: DC3-only class (class name absent from band.exe); robustness: abort on a misparsed numEntries (>256 or <0); bad data only |
| 26 | `flow/FlowNode.cpp:411` | no retail: DC3-only class (class name absent from band.exe); robustness: null-child guard |
| 27 | `gesture/Skeleton.h:76` | build: include / declaration / linkage / layout, no behaviour |
| 28 | `hamobj/HamNavProvider.cpp:153` | diag: `MILO_DEBUG_CHOOSE_MODE` env log only (HamNavProvider is DC3-only, absent from band.exe) |
| 29 | `hamobj/HamNavProvider.cpp:246` | diag: `MILO_DEBUG_CHOOSE_MODE` env log only |
| 30 | `hamobj/HamNavProvider.cpp:272` | diag: `MILO_DEBUG_CHOOSE_MODE` env log only |
| 31 | `hamobj/HamCharacter.h:62` | build: native-only inline setter (HamCharacter, DC3 class) |
| 32 | `hamobj/HamNavList.h:150` | build: declaration only (HamNavList.cpp, DC3 class, is not in the native closure) |
| 33 | `math/Trig.cpp:32` | equivalent: file-scope TrigTableInit(); retail fills the same table in SystemInit, the call is idempotent |
| 34 | `math/Key.h:70` | build: libstdc++/two-phase-lookup portability shim |
| 35 | `math/Easing.h:356` | build: `inline` linkage for a header-defined table, value identical |
| 36 | `math/kdTree.h:337` | equivalent: native computes pNode0/pNode1 by typed pointer arithmetic (64-bit stride) in the two-sided arm above; this block is the Pack(pNode0) call the X360 arm makes inside its own branch, same order |
| 37 | `meta/Sorting.cpp:5` | build: include / declaration / linkage / layout, no behaviour |
| 38 | `meta/CreditsPanel.h:61` | build: dev-build member `mCheatOn` (layout; after the matched layout natively) |
| 39 | `meta/Achievements.cpp:37` | platform: XDK achievement stubs (PlatformInit / GetAchievementData / SubmitAchievementsFunc return empty) |
| 40 | `meta/SongMetadata.cpp:163` | dev handler: `id` script arm retail fn_827AB080 lacks; reachable only by a script naming it |
| 41 | `meta/StorePreviewMgr.cpp:121` | **FIXED**: retail Handle 0x827B2360 calls no superclass Handle (callees: Symbol/String/DataNode helpers, PathName, five own methods; no bl 0x82768318 MsgSource::Handle). Native forwarded unhandled messages to MsgSource::Handle (add_sink/remove_sink, Hmx::Object handlers, Export to sinks) |
| 42 | `meta/StorePanel.cpp:75` | offline shim (net area): null `TheNetCacheMgr` guard (null offline natively, cf. SystemPoll); online takes the retail path. Listed, left |
| 43 | `meta/StorePanel.cpp:436` | diag: DC3 ValidateOffers is MILO_NOTIFY / TheDebug.Notify only (duplicate short names, song in >1 album/pack); no state change |
| 44 | `meta/StorePanel.cpp:717` | dev handler: toggle_test_offers / test_offers script arms retail lacks |
| 45 | `meta/PreloadPanel.cpp:15` | build: native definition of `PreloadPanel::sCache` |
| 46 | `meta/PreloadPanel.cpp:57` | inert: `max_cache_size` FindData (string absent from band.exe and from the shipped/Deluxe dta) plus CheckTypeDef warnings; after #49 nothing reads gMaxCacheSize |
| 47 | `meta/PreloadPanel.cpp:71` | **FIXED**: retail 0x827B4880 is UIPanel::Load (bl 0x82812CB8) then `stw 0,0x40(r31)` (mPreloadResult) then RegisterCallback; no SetLoaderPeriod. Native set the loader period to 14 ms on every preload and never restored it |
| 48 | `meta/PreloadPanel.cpp:136` | **FIXED**: retail PollForLoading 0x827B49E8 calls DoneCaching (bl 0x82518A08) then FileCached (bl 0x82518F30); no PollAll (0x82519788). The native extra PollAll after DoneCaching could only DumpOverSize-evict before the FileCached check |
| 49 | `meta/PreloadPanel.cpp:253` | **FIXED**: retail StartCache 0x827B4668 calls Clear (bl 0x825192E8) then StartSet (bl 0x825191F8); no SetSize/EndSet between them. Native SetSize ran EndSet (sort + Poll) between Clear and StartSet |
| 50 | `meta/MoviePanel.cpp:140` | robustness: skips ChooseMovie on an empty movie list (retail indexes mMovies[RandomInt(0,0)]) |
| 51 | `meta/MoviePanel.cpp:258` | **flagged, left (platform)**: native MovieSys is the base stub (rb3_app_native_glue.cpp gRb3AppMovieSys), whose MovieImpl::Ready() returns 0, so the retail `mMovie.Ready()` wait would block the screen forever. Remove when a real native movie impl lands |
| 52 | `meta/CreditsPanel.cpp:50` | dev handler: debug_toggle_autoscroll script arm retail lacks |
| 53 | `meta/CreditsPanel.cpp:93` | equivalent: clears dev member mCheatOn, read only by the dev is_cheat_on arm (default false = retail constant false) |
| 54 | `meta/CreditsPanel.cpp:184` | dev handler body for #52 |
| 55 | `meta/FixedSizeSaveable.cpp:45` | robustness: MILO_ASSERT (native abort) on a pad read past the stream end; corrupt save only |
| 56 | `meta/SongPreview.cpp:26` | build: ctor init of native-only members (mTexMovie/mInitted/mSameSongRequested) |
| 57 | `meta/SongPreview.cpp:74` | robustness: not-initted guard (retail derefs mMusicFader); fires only before Init |
| 58 | `meta/SongPreview.cpp:87` | robustness: not-initted guard (retail derefs mCrowdSingFader); fires only before Init |
| 59 | `meta/SongPreview.cpp:104` | inert: clears mTexMovie, which is always null natively (every caller reaches Start(song, nullptr)) |
| 60 | `meta/SongPreview.cpp:140` | inert: as #59 |
| 61 | `meta/SongPreview.cpp:165` | inert: stores the texMovie argument, always nullptr |
| 62 | `meta/SongPreview.cpp:238` | build: native 1-arg forwarder to Start(song, nullptr) |
| 63 | `meta/SongPreview.cpp:250` | diag: MILO_LOG on every OnStart (print only) |
| 64 | `meta/SongPreview.cpp:263` | diag: MILO_LOG on every OnStart (print only) |
| 65 | `meta/SongPreview.cpp:298` | inert: DC3 preview-movie path gated on mTexMovie, always null natively |
| 66 | `meta/SongPreview.cpp:357` | inert: mSameSongRequested is never set true (ctor 0, only other store is false) |
| 67 | `meta/SongPreview.cpp:408` | inert: HasMovie() is false (mTexMovie null) |
| 68 | `meta/SongPreview.h:47` | build: native-only overload declaration |
| 69 | `meta/SongPreview.h:112` | build: native-only members, after the retail 0x70 layout |
| 70 | `midi/DataEventList.cpp:169` | diag: notify only (retail compiles the notify out) |
| 71 | `midi/MidiParser.cpp:117` | prop: extra `SYNC_SUPERCLASS(Hmx::Object)`; retail has no superclass chain here, so natively the native-only name/type/sinks props of Hmx::Object (obj/Object.cpp, retail 0x8235C2E0 is the empty 72 B terminal) answer by name; reachable only by property name, no default-path caller |
| 72 | `midi/MidiParserMgr.h:32` | build: accessor for the native Game::SetPaused arm (W16-UB 64-bit offset), no call of its own |
| 73 | `movie/MovieSys.cpp:9` | platform: native TheMovieSys binding (gBinkMovieSys symbol provided by the native glue) |
| 74 | `movie/Movie.h:48` | build: native-only accessor |
| 75 | `movie/Splash.cpp:14` | build: include / declaration / linkage / layout, no behaviour |
| 76 | `movie/Splash.cpp:29` | **platform, left**: no splash worker thread natively (mThreaded=false); the threaded path would block in WaitForState |
| 77 | `movie/Splash.cpp:216` | **rendering-coupled, listed**: clears RndCam::sCurrent before deleting splash dirs (dangling native camera); retail leaves it |
| 78 | `movie/Splash.cpp:296` | **rendering-coupled, listed**: non-threaded splash draw pass (pairs with #76; retail draws on the worker thread) |
| 79 | `movie/Splash.cpp:488` | robustness: skips a splash .milo with neither camera nor movie |
| 80 | `movie/Splash.cpp:573` | diag: Splash Time print under DebugArkOrder |
| 81 | `movie/TexMovie.cpp:78` | dev: is_localized / is_empty properties retail lacks (CQ-3: retail enumerates 3 literals) |
| 82 | `movie/TexMovie.cpp:99` | dev: property retail lacks (native Hmx::Object propsync is name/type/sinks, all native-only; retail Hmx::Object::SyncProperty 0x8235C2E0 is empty); reachable only by a script naming it |
| 83 | `movie/TexMovie.cpp:161` | equivalent: rev 6..8 fields; RB3 data and native saves are rev 5 |
| 84 | `net/SessionJobs_Xbox.h:37` | net (offline): Xbox session-job layout / OnCompletion overrides; the jobs never start offline |
| 85 | `net/SessionJobs_Xbox.h:47` | net (offline): Xbox session-job layout / OnCompletion overrides; the jobs never start offline |
| 86 | `net/SessionJobs_Xbox.h:56` | net (offline): Xbox session-job layout / OnCompletion overrides; the jobs never start offline |
| 87 | `net/SessionJobs_Xbox.h:79` | net (offline): Xbox session-job layout / OnCompletion overrides; the jobs never start offline |
| 88 | `net/SessionJobs_Xbox.h:132` | net (offline): Xbox session-job layout / OnCompletion overrides; the jobs never start offline |
| 89 | `net/SessionJobs_Xbox.h:142` | net (offline): Xbox session-job layout / OnCompletion overrides; the jobs never start offline |
| 90 | `net/SessionJobs_Xbox.h:167` | net (offline): Xbox session-job layout / OnCompletion overrides; the jobs never start offline |
| 91 | `net/JsonUtils.cpp:21` | diag: native-only type assert; natively MILO_ASSERT reaches Debug::Fail, which prints and returns (#321), so it is a print (retail 0x82B81F40 Str is `lwz r3,4(r3); b json_object_get_string`) |
| 92 | `net/JsonUtils.cpp:34` | diag: native-only type assert; natively MILO_ASSERT reaches Debug::Fail, which prints and returns (#321), so it is a print (retail 0x82B81F40 Str is `lwz r3,4(r3); b json_object_get_string`) |
| 93 | `net/JsonUtils.cpp:41` | diag: native-only type assert; natively MILO_ASSERT reaches Debug::Fail, which prints and returns (#321), so it is a print (retail 0x82B81F40 Str is `lwz r3,4(r3); b json_object_get_string`) |
| 94 | `net/json-c/config.h:133` | build: include / declaration / linkage / layout, no behaviour |
| 95 | `obj/DirLoader.h:30` | build: include / declaration / linkage / layout, no behaviour |
| 96 | `obj/DirLoader.h:50` | build: include / declaration / linkage / layout, no behaviour |
| 97 | `obj/DirLoader.h:128` | build: layout, native-only mParentDir + owning mProxyDir (retail `new DirLoader` is 0xa8 B) |
| 98 | `obj/Data.h:64` | LP64: zeroes the 8-byte union before a 4-byte store; retail union is one word, value identical |
| 99 | `obj/Data.h:72` | LP64: zeroes the 8-byte union before a 4-byte store; retail union is one word, value identical |
| 100 | `obj/Data.h:80` | LP64: zeroes the 8-byte union before a 4-byte store; retail union is one word, value identical |
| 101 | `obj/Data.h:88` | LP64: zeroes the 8-byte union before a 4-byte store; retail union is one word, value identical |
| 102 | `obj/Data.h:95` | LP64: `unsigned int` overload (distinct from `unsigned long` on LP64), same value and type |
| 103 | `obj/Data.h:115` | LP64: zeroes the 8-byte union before a 4-byte store; retail union is one word, value identical |
| 104 | `obj/Data.h:128` | LP64: zeroes the 8-byte union before a 4-byte store; retail union is one word, value identical |
| 105 | `obj/Data.h:136` | LP64: zeroes the 8-byte union before a 4-byte store; retail union is one word, value identical |
| 106 | `obj/Data.h:310` | build: include / declaration / linkage / layout, no behaviour |
| 107 | `obj/Data.h:332` | build: layout, native-only mContextPath (DTA trace) |
| 108 | `obj/Data.h:381` | diag: DataArray_LogAccess, prints only under DTA_TRACE / DTA_VALIDATE env |
| 109 | `obj/Data.h:387` | diag: DataArray_LogAccess, prints only under DTA_TRACE / DTA_VALIDATE env |
| 110 | `obj/Data.h:398` | diag: DataArray_LogAccess, prints only under DTA_TRACE / DTA_VALIDATE env |
| 111 | `obj/Data.h:416` | diag: DataArray_LogAccess, prints only under DTA_TRACE / DTA_VALIDATE env |
| 112 | `obj/Data.h:422` | diag: DataArray_LogAccess, prints only under DTA_TRACE / DTA_VALIDATE env |
| 113 | `obj/Data.h:440` | diag: DataArray_LogAccess, prints only under DTA_TRACE / DTA_VALIDATE env |
| 114 | `obj/Data.h:446` | diag: DataArray_LogAccess, prints only under DTA_TRACE / DTA_VALIDATE env |
| 115 | `obj/Data.h:755` | equivalent: DataArrayPtr assignment with ref counting; retail assigns member-wise, and its only such uses (std::sort moves) end with the same counts |
| 116 | `obj/Data.h:790` | build: include / declaration / linkage / layout, no behaviour |
| 117 | `obj/TypeProps.cpp:69` | lifetime: native TypeProps mObjects ref tracking for embedded objects (layout #130, remove #117) |
| 118 | `obj/TypeProps.cpp:272` | guard: null typeDef warns and returns null; retail asserts (stripped) and dereferences it |
| 119 | `obj/TypeProps.cpp:293` | guard: null array warns and returns; retail dereferences it |
| 120 | `obj/TypeProps.cpp:385` | dev diag: log only, and only under TheLoadMgr.EditMode() (retail fn_827661C0 has no report) |
| 121 | `obj/TextFile.cpp:11` | dev: opens the `_append` log file only when Holmes is connected (UsingHolmes needs gHolmesStream); retail 0x8276D248 never opens a file. Does not run by default |
| 122 | `obj/Object.h:24` | build: include / declaration / linkage / layout, no behaviour |
| 123 | `obj/Object.h:100` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 124 | `obj/Object.h:123` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 125 | `obj/Object.h:138` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 126 | `obj/Object.h:146` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 127 | `obj/Object.h:150` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 128 | `obj/Object.h:163` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 129 | `obj/Object.h:250` | build: include / declaration / linkage / layout, no behaviour |
| 130 | `obj/Object.h:2074` | lifetime: native TypeProps mObjects ref tracking for embedded objects (layout #130, remove #117) |
| 131 | `obj/Object.h:2143` | build: include / declaration / linkage / layout, no behaviour |
| 132 | `obj/Object.h:2220` | build: layout, DC3 Object-level mSinks (retail Object dtor fn_82738050 has none; SINKS_NATIVE_DUAL_SINK_SYSTEMS: inert) |
| 133 | `obj/Object.h:2231` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 134 | `obj/Object.h:2273` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 135 | `obj/Object.h:2408` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 136 | `obj/Object.h:2629` | guard: aborts on a corrupt ObjVector length > 0x10000 (retail would resize to it); never fires on shipped data |
| 137 | `obj/Object.h:2772` | build: include / declaration / linkage / layout, no behaviour |
| 138 | `obj/Object.h:2780` | guard: aborts on an empty rev stack (retail reads past the end) |
| 139 | `obj/Object.h:2793` | dev diag: rev-stack mismatch report (MILO_LOG + MILO_FAIL, non-fatal natively, Debug.cpp:204); retail 0x822CA890 has no compare; it pops the same way |
| 140 | `obj/Dir.cpp:26` | build: include / declaration / linkage / layout, no behaviour |
| 141 | `obj/Dir.cpp:55` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 142 | `obj/Dir.cpp:213` | diag: live-dir counter read only by the rb3-render venue gate |
| 143 | `obj/Dir.cpp:219` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 144 | `obj/Dir.cpp:290` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 145 | `obj/Dir.cpp:320` | script handler retail lacks (`get_file_name`; retail ObjectDir::Handle ends at get_path_name); no shipped script uses it |
| 146 | `obj/Dir.cpp:346` | editor-only: the SyncSubDir "exists in dir and subdir" replace loop runs only on kPropSet/kPropInsert of `subdirs`; shipped scripts only read it (world_objects.dta:2131-2143) |
| 147 | `obj/Dir.cpp:386` | editor-only: duplicate-subdir NOTIFY on set/insert of `subdirs` (same path as #146) |
| 148 | `obj/Dir.cpp:449` | equivalent (tree-wide native propsync policy, sweep 3): retail ObjectDir::SyncProperty 0x82752A60 makes no superclass call (bls: PropSyncSubDirs 0x82752668, PropSync 0x82280290, no Object::SyncProperty); the native chain adds only the native name/type/sinks props, which no shipped script reads on a dir |
| 149 | `obj/Dir.cpp:637` | **FLAGGED, left**: during MergeDirs a kReplace (kCopyDeep) dir copy keeps toDir's mSubDirs; retail ObjectDir::Copy 0x82750F80 runs RemovingSubDir/assign/AddedSubDir. kMerge (kCopyFromMax) copies no subdirs on either build. Native workaround for ring corruption (FileMerger path); needs that root cause first |
| 150 | `obj/Dir.cpp:651` | **FLAGGED, left**: during MergeDirs a kReplace (kCopyDeep) dir copy keeps toDir's mSubDirs; retail ObjectDir::Copy 0x82750F80 runs RemovingSubDir/assign/AddedSubDir. kMerge (kCopyFromMax) copies no subdirs on either build. Native workaround for ring corruption (FileMerger path); needs that root cause first |
| 151 | `obj/Dir.cpp:822` | dev diag: FAIL print on an out-of-range viewport id (non-fatal natively), then the same index |
| 152 | `obj/Dir.cpp:937` | allocation placement only: MemTemp around the hash/string-table resize; retail 0x8274F1F0 calls only KeylessHash::Resize 0x8274EFB0 and StringTable::Reserve 0x827C8E50. No state or control change |
| 153 | `obj/Dir.cpp:954` | guard: self-subdir (bad data only); retail LoadSubDir 0x8274FEF8 loads unconditionally |
| 154 | `obj/Dir.cpp:965` | state for the native lookup fallback (#159/#305/#308): SetParentDir on the subdir loader |
| 155 | `obj/Dir.cpp:1036` | equivalent: clears the native superclass memo of #158 |
| 156 | `obj/Dir.cpp:1063` | build: include / declaration / linkage / layout, no behaviour |
| 157 | `obj/Dir.cpp:1197` | dev diag: `objects->FindArray(s2)` result unused; prints a non-fatal FAIL only for a class missing from the objects config. Retail Iterate 0x8274FCE8 has no SystemConfig call (bls) |
| 158 | `obj/Dir.cpp:1205` | equivalent: memoizes IsASubclass per (class, filter) pair; retail 0x8274FCE8 calls IsASubclass 0x82757DA0 per object, a pure function of the pair |
| 159 | `obj/Dir.cpp:1288` | **FLAGGED, left**: FindObject falls back to the DirLoader proxy dir and parent dir when Dir()==this; retail FindObject 0x82750188 returns null there. Native merge-pipeline workaround that can return an object retail would not find; needs root cause first |
| 160 | `obj/Dir.cpp:1349` | equivalent: clears the native superclass memo of #158 |
| 161 | `obj/Dir.cpp:1564` | inert: ShouldBlockSubdirLoad returns false unless sPathEval is set, and no source calls SetPathEvalCallback/SetPathEvalFunc |
| 162 | `obj/Dir.cpp:1599` | inert: ShouldBlockSubdirLoad returns false unless sPathEval is set, and no source calls SetPathEvalCallback/SetPathEvalFunc |
| 163 | `obj/Dir.cpp:1617` | inert: ShouldBlockSubdirLoad returns false unless sPathEval is set, and no source calls SetPathEvalCallback/SetPathEvalFunc |
| 164 | `obj/Dir.cpp:1725` | state for the native lookup fallback (#159/#305/#308): SetParentDir on the subdir loader |
| 165 | `obj/Dir.cpp:1737` | state for the native lookup fallback (#159/#305/#308): SetParentDir on the subdir loader |
| 166 | `obj/Dir.h:13` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 167 | `obj/Dir.h:35` | diag: MILO_DEBUG_CHOOSE_MODE env log only |
| 168 | `obj/Dir.h:123` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 169 | `obj/Dir.h:157` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 170 | `obj/Dir.h:173` | diag: MILO_DEBUG_CHOOSE_MODE env log only |
| 171 | `obj/Dir.h:187` | diag: MILO_DEBUG_CHOOSE_MODE env log only |
| 172 | `obj/Dir.h:207` | diag: MILO_DEBUG_CHOOSE_MODE env log only |
| 173 | `obj/Dir.h:240` | diag: MILO_DEBUG_CHOOSE_MODE env log only |
| 174 | `obj/Dir.h:299` | build: include / declaration / linkage / layout, no behaviour |
| 175 | `obj/Dir.h:343` | build: include / declaration / linkage / layout, no behaviour |
| 176 | `obj/Dir.h:612` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 177 | `obj/Dir.h:758` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 178 | `obj/DirLoader.cpp:21` | build: include / declaration / linkage / layout, no behaviour |
| 179 | `obj/DirLoader.cpp:27` | diag: RB3_STREAM_AUDIT env / venue-gate audit hooks; also the sPathEval definition (never set) |
| 180 | `obj/DirLoader.cpp:101` | build: include / declaration / linkage / layout, no behaviour |
| 181 | `obj/DirLoader.cpp:120` | build: native member init (mHasEditorDir 0, mSubDir, mParentDir, owning mProxyDir) |
| 182 | `obj/DirLoader.cpp:189` | equivalent: natively DirLoader owns one forwarding ref, mProxyDir (ObjOwnerPtr(this)); mObjects has no owner (nullptr, kObjListAllowNull). So the test is true whenever Replace runs, as in retail 0x82754A70 |
| 183 | `obj/DirLoader.cpp:422` | guard: stops the marker hunt on a failed stream (retail spins); shipped data never fails |
| 184 | `obj/DirLoader.cpp:493` | diag: helper for the two-sided Find arm; its one-sided part is an unconditional stderr print of the first 5 director matches. Print only |
| 185 | `obj/DirLoader.cpp:524` | diag: unconditional stderr print of the first 3 director lookups. Print only |
| 186 | `obj/DirLoader.cpp:551` | diag: unconditional stderr print of the first 3 director lookups. Print only |
| 187 | `obj/DirLoader.cpp:803` | diag: object/type mem dump, off unless sObjectMemDumpFile/sTypeMemDumpFile is set |
| 188 | `obj/DirLoader.cpp:858` | diag: object/type mem dump, off unless sObjectMemDumpFile/sTypeMemDumpFile is set |
| 189 | `obj/DirLoader.cpp:942` | guard: real EOF in LoadObjs clears and returns (truncated file only) |
| 190 | `obj/DirLoader.cpp:950` | guard: skips an object whose native class is a stub vtable, with a NOTIFY (class missing natively) |
| 191 | `obj/DirLoader.cpp:975` | diag: stream audit (env or gate); when disabled #195 runs the same ReadDead |
| 192 | `obj/DirLoader.cpp:985` | diag: stream audit (env or gate); when disabled #195 runs the same ReadDead |
| 193 | `obj/DirLoader.cpp:1019` | diag: stream audit (env or gate); when disabled #195 runs the same ReadDead |
| 194 | `obj/DirLoader.cpp:1027` | diag: stream audit (env or gate); when disabled #195 runs the same ReadDead |
| 195 | `obj/DirLoader.cpp:1045` | diag: stream audit (env or gate); when disabled #195 runs the same ReadDead |
| 196 | `obj/DirLoader.cpp:1116` | inert: mHasEditorDir is only ever set 0 (ctor #181) |
| 197 | `obj/DirLoader.cpp:1234` | guard: null factory result / stub vtable class skipped, with a NOTIFY |
| 198 | `obj/DirLoader.cpp:1291` | guard: real EOF in the header loop calls Cleanup (retail spins on a missing milo) |
| 199 | `obj/DirLoader.cpp:1318` | diag: RB3_TRACE_DIRHEADER env log |
| 200 | `obj/DirLoader.cpp:1333` | diag: log on the rev < 7 failure path |
| 201 | `obj/DirLoader.cpp:1442` | diag: file-source ledger (weak symbol, log only) |
| 202 | `obj/DirLoader.cpp:1746` | diag: MessageTimer; AddTime records only while MessageTimer::sActive (script `message_timer` start) |
| 203 | `obj/Utl.cpp:12` | build: include / declaration / linkage / layout, no behaviour |
| 204 | `obj/Utl.cpp:103` | diag: MILO_DEBUG_MERGE env log |
| 205 | `obj/Utl.cpp:465` | **FLAGGED, left**: during MergeDirs a kReplace (kCopyDeep) dir copy keeps toDir's mSubDirs; retail ObjectDir::Copy 0x82750F80 runs RemovingSubDir/assign/AddedSubDir. kMerge (kCopyFromMax) copies no subdirs on either build. Native workaround for ring corruption (FileMerger path); needs that root cause first |
| 206 | `obj/Utl.cpp:472` | **FLAGGED, left**: during MergeDirs a kReplace (kCopyDeep) dir copy keeps toDir's mSubDirs; retail ObjectDir::Copy 0x82750F80 runs RemovingSubDir/assign/AddedSubDir. kMerge (kCopyFromMax) copies no subdirs on either build. Native workaround for ring corruption (FileMerger path); needs that root cause first |
| 207 | `obj/Task.cpp:13` | lifetime: native live-task registry; #212 skips polling a freed task |
| 208 | `obj/Task.cpp:21` | lifetime: native live-task registry; #212 skips polling a freed task |
| 209 | `obj/Task.cpp:25` | lifetime: native live-task registry; #212 skips polling a freed task |
| 210 | `obj/Task.cpp:29` | lifetime: native live-task registry; #212 skips polling a freed task |
| 211 | `obj/Task.cpp:89` | port: native ObjPtrList in owner-control mode stands in for retail's raw list + AddRef, so an object dying forwards Replace to ScriptTask as retail's ring does |
| 212 | `obj/Task.cpp:417` | lifetime: native live-task registry; #212 skips polling a freed task |
| 213 | `obj/Task.cpp:444` | native: timelines exist before Init (retail ctor 0x82747998 leaves them null). Init 0x8274A7F8 reallocates, which leaks the ctor array (4 timelines). No task is queued before Init on the app path |
| 214 | `obj/Task.cpp:529` | harness: MILO_HEADLESS fixed-step clock, env only |
| 215 | `obj/Task.cpp:549` | harness: MILO_HEADLESS fixed-step clock, env only |
| 216 | `obj/Task.cpp:555` | inert: unk84 is filled only by QueueTaskDelete, and nothing calls it (rndobj/Anim.cpp:450 and ui/UIScreen.cpp:155 only mention it in comments); retail Poll 0x82749B10 calls only TaskTimeline::Poll 0x827499A8 |
| 217 | `obj/Task.cpp:599` | **FLAGGED, left (lifetime)**: a task started during a dir cascade is deleted instead of scheduled; retail TaskMgr::Start schedules it. Part of the native cascade model, so it stays with it |
| 218 | `obj/Task.cpp:671` | inert: unk84 is filled only by QueueTaskDelete, and nothing calls it (rndobj/Anim.cpp:450 and ui/UIScreen.cpp:155 only mention it in comments); retail Poll 0x82749B10 calls only TaskTimeline::Poll 0x827499A8 |
| 219 | `obj/Task.cpp:668` | inert: unk84 is filled only by QueueTaskDelete, and nothing calls it (rndobj/Anim.cpp:450 and ui/UIScreen.cpp:155 only mention it in comments); retail Poll 0x82749B10 calls only TaskTimeline::Poll 0x827499A8 |
| 220 | `obj/Task.h:29` | lifetime: native live-task registry; #212 skips polling a freed task |
| 221 | `obj/Task.h:37` | lifetime: native live-task registry; #212 skips polling a freed task |
| 222 | `obj/Task.h:166` | native: frees the timelines at exit |
| 223 | `obj/Task.h:198` | inert: unk84 is filled only by QueueTaskDelete, and nothing calls it (rndobj/Anim.cpp:450 and ui/UIScreen.cpp:155 only mention it in comments); retail Poll 0x82749B10 calls only TaskTimeline::Poll 0x827499A8 |
| 224 | `obj/Task.h:222` | inert: unk84 is filled only by QueueTaskDelete, and nothing calls it (rndobj/Anim.cpp:450 and ui/UIScreen.cpp:155 only mention it in comments); retail Poll 0x82749B10 calls only TaskTimeline::Poll 0x827499A8 |
| 225 | `obj/Msg.cpp:8` | build: include / declaration / linkage / layout, no behaviour |
| 226 | `obj/Msg.cpp:516` | lifetime: native MsgSource sink-ref ledger cleanup |
| 227 | `obj/DataArray.cpp:23` | diag: DTA_TRACE / DTA_VALIDATE env tracing and validation |
| 228 | `obj/DataArray.cpp:417` | build: initializes the native mContextPath |
| 229 | `obj/DataArray.cpp:428` | build: initializes the native mContextPath |
| 230 | `obj/DataArray.cpp:902` | diag: `array_exec` AutoTimer (timer stats, plus a glitch report only when gGlitchCallback is set) |
| 231 | `obj/DataArray.cpp:944` | dev diag: "not function or object" warning (MILO_FAIL_DTA is TheDebugWarner natively); returns 0 as retail |
| 232 | `obj/DataArray.cpp:986` | diag: DataCallStackFrame (error-report call stack); retail fn_82728A60 has none |
| 233 | `obj/DataFile.cpp:47` | guard: DTA parse depth / array size limits, bad data only |
| 234 | `obj/DataFile.cpp:64` | guard: DTA parse depth / array size limits, bad data only |
| 235 | `obj/DataFile.cpp:433` | guard: DTA parse depth / array size limits, bad data only |
| 236 | `obj/DataFile.cpp:455` | guard: DTA parse depth / array size limits, bad data only |
| 237 | `obj/DataFile.cpp:525` | diag: file-source ledger (weak symbol, log only) |
| 238 | `obj/DataFile.cpp:554` | diag: file-source ledger (weak symbol, log only) |
| 239 | `obj/DataFile.cpp:671` | port: saves and restores the native flex scanner's hold char across a nested text-DTA #include; text DTA only |
| 240 | `obj/DataFile.cpp:688` | port: saves and restores the native flex scanner's hold char across a nested text-DTA #include; text DTA only |
| 241 | `obj/DataFile.cpp:700` | diag: file-source ledger (weak symbol, log only) |
| 242 | `obj/DataFile.cpp:776` | build: include / declaration / linkage / layout, no behaviour |
| 243 | `obj/DataFunc.cpp:555` | guard: null object/array returns 0; retail dereferences null |
| 244 | `obj/DataFunc.cpp:577` | guard: null object/array returns 0; retail dereferences null |
| 245 | `obj/DataFunc.cpp:619` | guard: null object/array returns 0; retail dereferences null |
| 246 | `obj/DataFunc.cpp:751` | dev script func, print only: retail registers notify / notify_beta / notify_once at the shared `return DataNode()` 0x8228D358 (DataInitFuncs 0x82763A00, registration order checked); natively the args are evaluated and printed. Shipped call sites pass strings or sprint |
| 247 | `obj/DataFunc.cpp:765` | dev script func, print only: retail registers notify / notify_beta / notify_once at the shared `return DataNode()` 0x8228D358 (DataInitFuncs 0x82763A00, registration order checked); natively the args are evaluated and printed. Shipped call sites pass strings or sprint |
| 248 | `obj/DataFunc.cpp:779` | dev script func: `fail` calls Debug::Fail (prints FAIL:, non-fatal unless MILO_FATAL_FAILS); retail 0x8228D358. Shipped calls (slot_states.dta:1194/1361, global.dta:111, meta_loading.dta:46/103/107) are error branches with string args |
| 249 | `obj/DataFunc.cpp:865` | guard: null object/array returns 0; retail dereferences null |
| 250 | `obj/DataFunc.cpp:975` | guard: object-not-found warns and returns kDataUnhandled; retail (fn_82761160 / 0x82761370) calls through null |
| 251 | `obj/DataFunc.cpp:1030` | guard: object-not-found warns and returns kDataUnhandled; retail (fn_82761160 / 0x82761370) calls through null |
| 252 | `obj/DataFunc.cpp:1467` | **FLAGGED, not default path**: `disable_notify` / `filter_notify` run their command bodies natively; retail registers both at 0x8228D358, so the bodies never run. No shipped `disable_notify`; one `filter_notify` at char/char_objects.dta:68, inside the editor perforce_resave flow |
| 253 | `obj/DataFunc.cpp:1484` | **FLAGGED, not default path**: `disable_notify` / `filter_notify` run their command bodies natively; retail registers both at 0x8228D358, so the bodies never run. No shipped `disable_notify`; one `filter_notify` at char/char_objects.dta:68, inside the editor perforce_resave flow |
| 254 | `obj/DataFunc.cpp:1640` | dev script func, print only: retail registers notify / notify_beta / notify_once at the shared `return DataNode()` 0x8228D358 (DataInitFuncs 0x82763A00, registration order checked); natively the args are evaluated and printed. Shipped call sites pass strings or sprint |
| 255 | `obj/DataNode.cpp:143` | guard: null property warns and returns a 0 node; retail dereferences null |
| 256 | `obj/DataNode.cpp:343` | dev diag: type-mismatch warning print, same return value |
| 257 | `obj/DataNode.cpp:362` | dev diag: type-mismatch warning print, same return value |
| 258 | `obj/DataNode.cpp:388` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 259 | `obj/DataNode.cpp:421` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 260 | `obj/DataNode.cpp:408` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 261 | `obj/DataNode.cpp:447` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 262 | `obj/DataNode.cpp:434` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 263 | `obj/DataNode.cpp:474` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 264 | `obj/DataNode.cpp:461` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 265 | `obj/DataNode.cpp:500` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 266 | `obj/DataNode.cpp:487` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 267 | `obj/DataNode.cpp:514` | dev diag: type-mismatch warning print, same return value |
| 268 | `obj/DataNode.cpp:537` | dev diag: type-mismatch warning print, same return value |
| 269 | `obj/DataNode.cpp:570` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 270 | `obj/DataNode.cpp:557` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 271 | `obj/DataNode.cpp:595` | dev diag: GetObj not-found warning |
| 272 | `obj/DataNode.cpp:616` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 273 | `obj/DataNode.cpp:649` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 274 | `obj/DataNode.cpp:636` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 275 | `obj/DataNode.cpp:671` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 276 | `obj/DataNode.cpp:658` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 277 | `obj/DataNode.cpp:693` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 278 | `obj/DataNode.cpp:680` | dev diag + type-error-only return: warns and, on a mismatched node, returns ""/Symbol("")/nullptr where retail reinterprets the wrong union member. Script type errors only |
| 279 | `obj/DataNode.cpp:724` | LP64: full-pointer compare for pointer types (retail 0x8274ABF0 compares the whole word) |
| 280 | `obj/DataNode.cpp:881` | LP64: clears the union before the 4-byte load (retail Load 0x8274B980; union is one word) |
| 281 | `obj/Object.cpp:6` | build: include / declaration / linkage / layout, no behaviour |
| 282 | `obj/Object.cpp:20` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 283 | `obj/Object.cpp:88` | **FLAGGED, left**: DC3's 8-slot property-path pool; retail SetProperty(Symbol) 0x8275B090 shares one static path. They differ only on re-entrant SetProperty(Symbol), where retail's shared array is reused |
| 284 | `obj/Object.cpp:140` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs) |
| 285 | `obj/Object.cpp:153` | layout/lifetime: native TypeProps is a pointer released before RemoveFromDir; retail runs ~TypeProps 0x82274048 after the ring walk (bls 0x8275CBF0). Same release, earlier |
| 286 | `obj/Object.cpp:157` | sinks: DC3 Object-level sink system (retail has none; Export slot 14 is the empty blr 0x826C3888). Lane `sinks` (SINKS_NATIVE_DUAL_SINK_SYSTEMS_2026-11-23.md) found no RB3-path caller: 0 to fix. #288 name/type props are the tree-wide propsync policy (see #148) |
| 287 | `obj/Object.cpp:203` | sinks: DC3 Object-level sink system (retail has none; Export slot 14 is the empty blr 0x826C3888). Lane `sinks` (SINKS_NATIVE_DUAL_SINK_SYSTEMS_2026-11-23.md) found no RB3-path caller: 0 to fix. #288 name/type props are the tree-wide propsync policy (see #148) |
| 288 | `obj/Object.cpp:277` | sinks: DC3 Object-level sink system (retail has none; Export slot 14 is the empty blr 0x826C3888). Lane `sinks` (SINKS_NATIVE_DUAL_SINK_SYSTEMS_2026-11-23.md) found no RB3-path caller: 0 to fix. #288 name/type props are the tree-wide propsync policy (see #148) |
| 289 | `obj/Object.cpp:461` | sinks: DC3 Object-level sink system (retail has none; Export slot 14 is the empty blr 0x826C3888). Lane `sinks` (SINKS_NATIVE_DUAL_SINK_SYSTEMS_2026-11-23.md) found no RB3-path caller: 0 to fix. #288 name/type props are the tree-wide propsync policy (see #148) |
| 290 | `obj/Object.cpp:577` | lifetime: native ref-ring / cascade-teardown machinery (no retail counterpart; retail ~Object walks the ring with ReplaceRefs); NullifyAllRefs owner-control drain (X16 seed restore) |
| 291 | `obj/Object.cpp:744` | sinks: DC3 Object-level sink system (retail has none; Export slot 14 is the empty blr 0x826C3888). Lane `sinks` (SINKS_NATIVE_DUAL_SINK_SYSTEMS_2026-11-23.md) found no RB3-path caller: 0 to fix. #288 name/type props are the tree-wide propsync policy (see #148) |
| 292 | `obj/Object.cpp:751` | sinks: DC3 Object-level sink system (retail has none; Export slot 14 is the empty blr 0x826C3888). Lane `sinks` (SINKS_NATIVE_DUAL_SINK_SYSTEMS_2026-11-23.md) found no RB3-path caller: 0 to fix. #288 name/type props are the tree-wide propsync policy (see #148) |
| 293 | `obj/Object.cpp:759` | sinks: DC3 Object-level sink system (retail has none; Export slot 14 is the empty blr 0x826C3888). Lane `sinks` (SINKS_NATIVE_DUAL_SINK_SYSTEMS_2026-11-23.md) found no RB3-path caller: 0 to fix. #288 name/type props are the tree-wide propsync policy (see #148) |
| 294 | `obj/Object.cpp:777` | sinks: DC3 Object-level sink system (retail has none; Export slot 14 is the empty blr 0x826C3888). Lane `sinks` (SINKS_NATIVE_DUAL_SINK_SYSTEMS_2026-11-23.md) found no RB3-path caller: 0 to fix. #288 name/type props are the tree-wide propsync policy (see #148) |
| 295 | `obj/Object.cpp:783` | sinks: DC3 Object-level sink system (retail has none; Export slot 14 is the empty blr 0x826C3888). Lane `sinks` (SINKS_NATIVE_DUAL_SINK_SYSTEMS_2026-11-23.md) found no RB3-path caller: 0 to fix. #288 name/type props are the tree-wide propsync policy (see #148) |
| 296 | `obj/Object.cpp:789` | sinks: DC3 Object-level sink system (retail has none; Export slot 14 is the empty blr 0x826C3888). Lane `sinks` (SINKS_NATIVE_DUAL_SINK_SYSTEMS_2026-11-23.md) found no RB3-path caller: 0 to fix. #288 name/type props are the tree-wide propsync policy (see #148) |
| 297 | `obj/Object.cpp:814` | sinks: DC3 Object-level sink system (retail has none; Export slot 14 is the empty blr 0x826C3888). Lane `sinks` (SINKS_NATIVE_DUAL_SINK_SYSTEMS_2026-11-23.md) found no RB3-path caller: 0 to fix. #288 name/type props are the tree-wide propsync policy (see #148) |
| 298 | `obj/Object.cpp:835` | **FLAGGED, left**: DC3's 8-slot property-path pool; retail SetProperty(Symbol) 0x8275B090 shares one static path. They differ only on re-entrant SetProperty(Symbol), where retail's shared array is reused |
| 299 | `obj/Object.cpp:1104` | guard: unknown class returns null (retail asserts, stripped, then dereferences end()) |
| 300 | `obj/Object.cpp:1166` | dev diag: FAIL print on a missing/foreign dir entry (non-fatal), then the same store as retail 0x8275A460 |
| 301 | `obj/Object.cpp:1201` | diag: MessageTimer; AddTime records only while MessageTimer::sActive (script `message_timer` start) |
| 302 | `obj/Object.cpp:1300` | sinks: DC3 Object-level sink system (retail has none; Export slot 14 is the empty blr 0x826C3888). Lane `sinks` (SINKS_NATIVE_DUAL_SINK_SYSTEMS_2026-11-23.md) found no RB3-path caller: 0 to fix. #288 name/type props are the tree-wide propsync policy (see #148) |
| 303 | `obj/Object.cpp:1426` | port: Hmx::Object copy constructor body (retail 0x822774F8 is the memberwise copy, called by SongRecord 0x82598190); needed to link rb3-app |
| 304 | `obj/ObjPtr_p.h:766` | **FLAGGED, left**: a NoNull ObjPtrVec erases a duplicate node after Replace (MergeObject redirect); retail keeps both nodes. Native lifetime workaround |
| 305 | `obj/ObjPtr_p.h:852` | **FLAGGED, left** (with #159): ObjPtr/ObjPtrList Load falls back to parent dirs and ObjectDir::Main() for a name the dir lacks; retail resolves in the dir only |
| 306 | `obj/ObjPtr_p.h:914` | build: include / declaration / linkage / layout, no behaviour |
| 307 | `obj/ObjPtr_p.h:1135` | lifetime guard: skips a null insert into a NoNull list (nulls come from the native cascade); retail asserts (stripped) and inserts |
| 308 | `obj/ObjPtr_p.h:1251` | **FLAGGED, left** (with #159): ObjPtr/ObjPtrList Load falls back to parent dirs and ObjectDir::Main() for a name the dir lacks; retail resolves in the dir only |
| 309 | `obj/ObjPtr_p.h:1525` | port: generic ObjPtrList/ObjPtrVec template bodies (retail has explicit specializations) |
| 310 | `os/ContentMgr.cpp:25` | script handler retail lacks (is_mounted / refresh_synchronously absent from band.exe), reachable only by name |
| 311 | `os/File.h:84` | build: include / declaration / linkage / layout, no behaviour |
| 312 | `os/AsyncFile.cpp:73` | host: Holmes file paths; inert on the app path: UsingHolmes() is false (gHolmesStream never set, #325) and UsingCD() is true when gen/main_xbox.hdr exists, so control reaches retail's AsyncFileWin line |
| 313 | `os/CritSec.h:19` | equivalent: class operator new to MemAlloc(0x20); retail inlines the same MemAlloc(0x20,0) (comment in CritSec.h) |
| 314 | `os/HDCache.cpp:403` | dev diag: warn/log/dump only (`MILO_DEBUG && HX_NATIVE`) |
| 315 | `os/PlatformMgr.cpp:16` | build: native definitions of statics/globals, no behaviour |
| 316 | `os/PlatformMgr.cpp:242` | host: platform stub (Xbox sign-in / virtual keyboard / DC3 HongKongExceptionMet=false); no RB3 retail body on this path |
| 317 | `os/AsyncFile.h:26` | build: native-engine virtual with a default body (StreamReceiver::IsOutputDrained serves the native receiver only) |
| 318 | `os/Debug.cpp:103` | equivalent: mAlwaysFlush init 0; retail leaves it to TheDebug's static zero-init |
| 319 | `os/Debug.cpp:120` | diag: native output sinks for Debug::Print (log file, reflect, stdout); retail's Print body is empty |
| 320 | `os/Debug.cpp:194` | diag: main-thread Notify goes to Modal -> AppDebugModal (notify_level 2) -> DebugModal, i.e. MILO_LOG; retail 0x8250F538 returns. Side effect: DebugModal appends every notify to gNotifies, which is only drained in SetModalCallback (unbounded growth, memory only) |
| 321 | `os/Debug.cpp:205` | **FLAGGED, left**: native Fail prints and returns. Retail 0x8250F6D0 (bls: MemFindHeap/MemPushHeap, MainThread, Timer::Sleep loop, bl 0x82829A08 throw, list clear, MemPopHeap) also returns on the main thread with mTry==0, so the default path matches, but natively a fail inside MILO_TRY (MidiReader.cpp:499, MidiParserMgr.cpp:172, Watcher.cpp:34, DrivenPropertyMathOps SetTry) does not throw, a worker-thread fail does not park, and the fail callbacks (NetSession DisconnectOnFail) never run. Engine-wide fail policy, not changed here |
| 322 | `os/FileCache.cpp:17` | build: native definitions of statics/globals, no behaviour |
| 323 | `os/BlockMgr.cpp:67` | host: scratch buffer for the native two-sided SpinUp read arm |
| 324 | `os/BlockMgr.cpp:331` | inert on the app path: UsingCD() is true with an ark, so the retail spin-up body runs (the sector choice is the two-sided arm below) |
| 325 | `os/HolmesClient.cpp:419` | equivalent on the app path: retail's own test `!UsingCD() || gHostConfig || gHostLogging` is false with an ark and no host config, so retail does not connect either |
| 326 | `os/HolmesClient.cpp:877` | host: empty HolmesClientPrint stub |
| 327 | `os/ArkFile.cpp:90` | diag: weak NativeFileLedger record only |
| 328 | `os/CDReader.cpp:67` | inert: gFakeFileErrors defaults false and UsingCD() is true with an ark, so the retail overlapped-result path runs |
| 329 | `os/Joypad.cpp:133` | exit/teardown only (terminate / exit callback), not on the run path |
| 330 | `os/VirtualKeyboard.cpp:54` | host: platform stub (Xbox sign-in / virtual keyboard / DC3 HongKongExceptionMet=false); no RB3 retail body on this path |
| 331 | `os/File.cpp:23` | build: include / declaration / linkage / layout, no behaviour |
| 332 | `os/File.cpp:92` | exit/teardown only (terminate / exit callback), not on the run path |
| 333 | `os/File.cpp:435` | dev: DataRegisterFunc for toggle_fake_file_errors / enumerate_frame_rate_results (strings absent from band.exe), script-only; HolmesClientInit returns at once natively |
| 334 | `os/File.cpp:452` | exit/teardown only (terminate / exit callback), not on the run path |
| 335 | `os/System.cpp:462` | inert: HolmesClientReInit returns when gHolmesStream is null |
| 336 | `os/System.cpp:496` | diag: notify only |
| 337 | `os/System.cpp:606` | equivalent: restores TheArchive exactly as retail 0x82510A08's X360 arm does (the gUsingCD difference is the two-sided arm above, not this block) |
| 338 | `os/System.cpp:703` | dev: force_cd option, default off |
| 339 | `os/System.cpp:809` | inert: gPristineSystemArgs is written here and read nowhere |
| 340 | `os/System.cpp:966` | host: platform stub (Xbox sign-in / virtual keyboard / DC3 HongKongExceptionMet=false); no RB3 retail body on this path |
| 341 | `synth/WavMgr.cpp:5` | build: native definitions of statics/globals, no behaviour |
| 342 | `synth/StreamReader.h:38` | build: native-engine virtual with a default body (StreamReceiver::IsOutputDrained serves the native receiver only) |
| 343 | `synth/FxSendMeterEffect.cpp:53` | prop: channel1/channel2 read-only props retail lacks (objdiff CP-2), reachable only by name |
| 344 | `synth/SampleData.h:90` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 345 | `synth/Sequence.cpp:7` | build: include / declaration / linkage / layout, no behaviour |
| 346 | `synth/Sequence.cpp:42` | teardown guard (DC3 scaffold): during ObjectDir deletion the native dtor erases its SeqInsts instead of deleting them (retail deletes every one); only on dir teardown, needs a run to see whether the insts are owned elsewhere; listed |
| 347 | `synth/Sequence.cpp:68` | prop: extra `SYNC_SUPERCLASS(Hmx::Object)`; retail has no superclass chain here, so natively the native-only name/type/sinks props of Hmx::Object (obj/Object.cpp, retail 0x8235C2E0 is the empty 72 B terminal) answer by name; reachable only by property name, no default-path caller |
| 348 | `synth/StreamReceiver.h:65` | build: native-engine virtual with a default body (StreamReceiver::IsOutputDrained serves the native receiver only) |
| 349 | `synth/Faders.h:41` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 350 | `synth/Faders.h:59` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 351 | `synth/MoggClip.h:68` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 352 | `synth/MoggClip.h:112` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 353 | `synth/MoggClip.h:129` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 354 | `synth/MoggClip.h:156` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 355 | `synth/SampleInst.h:45` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 356 | `synth/SampleInst.h:76` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 357 | `synth/SampleInst.h:85` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 358 | `synth/SampleInst.h:102` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 359 | `synth/SampleInst.h:136` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 360 | `synth/ThreeDSound.h:45` | inert: sets Fader::mTranspose, a DC3-only field nothing reads (no GetTranspose caller) |
| 361 | `synth/StandardStream.h:110` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 362 | `synth/StandardStream.h:178` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 363 | `synth/FxSend.cpp:51` | prop: extra `SYNC_SUPERCLASS(Hmx::Object)`; retail has no superclass chain here, so natively the native-only name/type/sinks props of Hmx::Object (obj/Object.cpp, retail 0x8235C2E0 is the empty 72 B terminal) answer by name; reachable only by property name, no default-path caller |
| 364 | `synth/MidiInstrument.h:90` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 365 | `synth/MidiInstrument.h:97` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 366 | `synth/Emitter.cpp:38` | prop: extra `SYNC_SUPERCLASS(Hmx::Object)`; retail has no superclass chain here, so natively the native-only name/type/sinks props of Hmx::Object (obj/Object.cpp, retail 0x8235C2E0 is the empty 72 B terminal) answer by name; reachable only by property name, no default-path caller |
| 367 | `synth/MoggClip.cpp:26` | support for the native-only DC3 Sound class (synth/Sound.cpp, not in the X360 build); retail data never reaches it |
| 368 | `synth/MoggClip.cpp:56` | prop: extra `SYNC_SUPERCLASS(Hmx::Object)`; retail has no superclass chain here, so natively the native-only name/type/sinks props of Hmx::Object (obj/Object.cpp, retail 0x8235C2E0 is the empty 72 B terminal) answer by name; reachable only by property name, no default-path caller |
| 369 | `synth/MoggClip.cpp:109` | inert at PostLoad: EnsureLoaded has already released mLoader and no stream exists before Play, so LoadNumChannels sets mNumChannels=-1 and returns; only a reload of a playing clip would hit its 200 ms poll and Stop() |
| 370 | `synth/MoggClip.cpp:220` | support for the native-only DC3 Sound class (synth/Sound.cpp, not in the X360 build); retail data never reaches it |
| 371 | `synth/MoggClip.cpp:319` | inert at PostLoad: EnsureLoaded has already released mLoader and no stream exists before Play, so LoadNumChannels sets mNumChannels=-1 and returns; only a reload of a playing clip would hit its 200 ms poll and Stop() |
| 372 | `synth/SampleData.cpp:8` | build: include / declaration / linkage / layout, no behaviour |
| 373 | `synth/SampleData.cpp:38` | host: native free path / UBSan-correct MemFree wrapper for SampleData |
| 374 | `synth/SampleData.cpp:103` | build: fills the native-only SampleData::mNumChannels |
| 375 | `synth/SynthSample.h:77` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 376 | `synth/MidiInstrument.cpp:142` | teardown only: native dtor deletes active voices (retail's implicit dtor frees the list nodes only) |
| 377 | `synth/MidiInstrument.cpp:177` | prop: extra `SYNC_SUPERCLASS(Hmx::Object)`; retail has no superclass chain here, so natively the native-only name/type/sinks props of Hmx::Object (obj/Object.cpp, retail 0x8235C2E0 is the empty 72 B terminal) answer by name; reachable only by property name, no default-path caller |
| 378 | `synth/MidiInstrument.cpp:230` | equivalent restructure (W16-TM): native polls MidiInstrument through SynthPollable and MidiInstrumentMgr::Poll's X360 call is gated off, so each instrument is polled once per frame; diverges only for an instrument that is not the manager's |
| 379 | `synth/SampleInst.cpp:10` | support: native SampleInst<->SynthSample registration for teardown Stop(); no effect while both live |
| 380 | `synth/SampleInst.cpp:17` | support: native SampleInst<->SynthSample registration for teardown Stop(); no effect while both live |
| 381 | `synth/SampleInst.cpp:25` | support: native SampleInst<->SynthSample registration for teardown Stop(); no effect while both live |
| 382 | `synth/SampleInst.cpp:36` | inert: Start/CancelPolling adds the inst to the synth poll list, but SynthPoll returns at once without an event receiver, which only the native Sound class sets |
| 383 | `synth/SampleInst.cpp:55` | inert: Start/CancelPolling adds the inst to the synth poll list, but SynthPoll returns at once without an event receiver, which only the native Sound class sets |
| 384 | `synth/SampleInst.cpp:98` | support for the native-only DC3 Sound class (synth/Sound.cpp, not in the X360 build); retail data never reaches it |
| 385 | `synth/SampleInst.cpp:124` | support for the native-only DC3 Sound class (synth/Sound.cpp, not in the X360 build); retail data never reaches it |
| 386 | `synth/Synth.cpp:45` | build: include / declaration / linkage / layout, no behaviour |
| 387 | `synth/Synth.cpp:118` | inert: optional track_levels read; RB3 config has no track_levels (string absent from band.exe) |
| 388 | `synth/Synth.cpp:129` | host: DefaultADSR object for the native synth / Sound path |
| 389 | `synth/Synth.cpp:300` | host: native platform shim / native audio-I/O plumbing that replaces an X360 facility, no retail counterpart to diverge from (mogg decoder, native synth factory, WavMgr init) |
| 390 | `synth/Synth.cpp:648` | host: native platform shim / native audio-I/O plumbing that replaces an X360 facility, no retail counterpart to diverge from (mogg decoder, native synth factory, WavMgr init) |
| 391 | `synth/Synth.cpp:670` | host: native platform shim / native audio-I/O plumbing that replaces an X360 facility, no retail counterpart to diverge from (mogg decoder, native synth factory, WavMgr init) |
| 392 | `synth/Synth.cpp:682` | inert: optional master_vol read; band_keep.dta has none (lane A1b), retail 0x82701710 never reads it |
| 393 | `synth/StandardStream.cpp:11` | build: include / declaration / linkage / layout, no behaviour |
| 394 | `synth/StandardStream.cpp:17` | build: include / declaration / linkage / layout, no behaviour |
| 395 | `synth/StandardStream.cpp:25` | build: include / declaration / linkage / layout, no behaviour |
| 396 | `synth/StandardStream.cpp:98` | host: native platform shim / native audio-I/O plumbing that replaces an X360 facility, no retail counterpart to diverge from: no decode thread natively, so Play pumps the reader to kReady and pre-fills the ring |
| 397 | `synth/StandardStream.cpp:119` | host: native platform shim / native audio-I/O plumbing that replaces an X360 facility, no retail counterpart to diverge from: no decode thread natively, so Play pumps the reader to kReady and pre-fills the ring |
| 398 | `synth/StandardStream.cpp:164` | host: null-mRdr guard (retail 0x82701C88 has no test); fires only for a stream without a reader |
| 399 | `synth/StandardStream.cpp:478` | host: headless audio-clock fallback; fires only when audio output runs <10% of wall clock for 500 ms (no audio device); with a device the retail drift-corrected path runs |
| 400 | `synth/StandardStream.cpp:1000` | host: null-mRdr guard (retail 0x82701C88 has no test); fires only for a stream without a reader |
| 401 | `synth/VorbisReader.cpp:189` | error path: corrupt-page bail-out so a native read cannot spin; retail only fails |
| 402 | `synth/VorbisReader.cpp:654` | host: native platform shim / native audio-I/O plumbing that replaces an X360 facility, no retail counterpart to diverge from: native VorbisReader read/decrypt/decode (no X360 decode thread) |
| 403 | `synth/SynthSample.cpp:31` | support: native SampleInst<->SynthSample registration for teardown Stop(); no effect while both live |
| 404 | `synth/SynthSample.cpp:69` | prop: extra `SYNC_SUPERCLASS(Hmx::Object)`; retail has no superclass chain here, so natively the native-only name/type/sinks props of Hmx::Object (obj/Object.cpp, retail 0x8235C2E0 is the empty 72 B terminal) answer by name; reachable only by property name, no default-path caller |
| 405 | `synth/SynthSample.cpp:126` | inert: sDisabled is set only by SynthSample::Disable(), which nothing calls |
| 406 | `synth/SynthSample.cpp:174` | inert: native LoadMgr platform is kPlatformXBox (#493), never kPlatformPC |
| 407 | `synth/SynthSample.cpp:189` | support: native SampleInst<->SynthSample registration for teardown Stop(); no effect while both live |
| 408 | `synth/SynthSample.cpp:195` | support: native SampleInst<->SynthSample registration for teardown Stop(); no effect while both live |
| 409 | `synth/SynthSample.cpp:206` | host: native free path / UBSan-correct MemFree wrapper for SampleData |
| 410 | `utl/ChunkStream.h:83` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 411 | `utl/Symbol.h:3` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 412 | `utl/FileStream.cpp:28` | diag: printf on a short read only |
| 413 | `utl/Cheats.h:51` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 414 | `utl/Cheats.h:86` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 415 | `utl/Song.h:105` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 416 | `utl/AllocInfo.cpp:88` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 417 | `utl/AllocInfo.cpp:134` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 418 | `utl/AllocInfo.cpp:184` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 419 | `utl/AllocInfo.h:12` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 420 | `utl/AllocInfo.h:71` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 421 | `utl/AllocInfo.h:79` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 422 | `utl/MemHeap.h:74` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 423 | `utl/MemTracker.h:23` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 424 | `utl/MemTracker.h:42` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 425 | `utl/MemTracker.h:77` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 426 | `utl/Song.cpp:57` | script handlers retail lacks (seconds_from_mbt / tick_from_mbt absent from band.exe), reachable only by name |
| 427 | `utl/Song.cpp:84` | prop: extra `SYNC_SUPERCLASS(Hmx::Object)`; retail has no superclass chain here, so natively the native-only name/type/sinks props of Hmx::Object (obj/Object.cpp, retail 0x8235C2E0 is the empty 72 B terminal) answer by name; reachable only by property name, no default-path caller |
| 428 | `utl/Song.cpp:141` | support for the native two-sided Song::Play / SyncState arms (non-blocking audio wait); acts only when those arms deferred |
| 429 | `utl/Song.cpp:167` | support for the native two-sided Song::Play / SyncState arms (non-blocking audio wait); acts only when those arms deferred |
| 430 | `utl/Locale.h:41` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 431 | `utl/Locale.h:71` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 432 | `utl/Magnu.cpp:23` | inert: Magnu strings load only through Holmes (MagnuRefreshStrings needs UsingHolmes(2)), so mMagnuStrings stays null; the assert is a print |
| 433 | `utl/Str.cpp:394` | equivalent: self-assignment guard, strcpy onto itself is a no-op |
| 434 | `utl/Loader.h:51` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 435 | `utl/Locale.cpp:78` | inert: Magnu strings load only through Holmes (MagnuRefreshStrings needs UsingHolmes(2)), so mMagnuStrings stays null; the assert is a print |
| 436 | `utl/Locale.cpp:100` | inert: SendDebugDataPoint -> DataPointMgr::RecordDebugDataPoint, whose mDebugDataPointRecorder is never set; 'debug/locale/token' absent from band.exe |
| 437 | `utl/Locale.cpp:150` | inert: Magnu strings load only through Holmes (MagnuRefreshStrings needs UsingHolmes(2)), so mMagnuStrings stays null; the assert is a print |
| 438 | `utl/Locale.cpp:166` | equivalent: mSize already asserted 0 |
| 439 | `utl/Locale.cpp:174` | inert: devkit locale override; DmMapDevkitDrive is a weak stub returning 0, so it stats devkit:\\locale\\<lang>\\locale_keep.dta, which does not exist natively, and keeps the retail cfg |
| 440 | `utl/Locale.cpp:201` | inert: devkit locale override; DmMapDevkitDrive is a weak stub returning 0, so it stats devkit:\\locale\\<lang>\\locale_keep.dta, which does not exist natively, and keeps the retail cfg |
| 441 | `utl/Locale.cpp:219` | inert: mInitialized is set true in the ctor and never cleared |
| 442 | `utl/Locale.cpp:319` | dev: toggle_show_tokens_cheat DataFunc (absent from band.exe), script-only |
| 443 | `utl/Cheats.cpp:179` | build: include / declaration / linkage / layout, no behaviour (inits of native-only members) |
| 444 | `utl/Cheats.cpp:187` | inert: cheats_ctrl_mode absent from config and band.exe; SetName(cheats_mgr) has no reader in src, native or data |
| 445 | `utl/Cheats.cpp:202` | script handler retail lacks (set_unsafe_cheat_used), reachable only by name |
| 446 | `utl/Cheats.cpp:207` | inert: KeyboardKeyReleaseMsg acts only when mIsOverridingKeyboard, which needs cheats_ctrl_mode |
| 447 | `utl/Cheats.cpp:372` | inert: KeyboardKeyReleaseMsg acts only when mIsOverridingKeyboard, which needs cheats_ctrl_mode |
| 448 | `utl/Cheats.cpp:386` | inert: ctrl-override arm needs mCtrlOverriddeMode (cheats_ctrl_mode, absent) |
| 449 | `utl/Cheats.cpp:425` | **FIXED**: the native dev build read config `quick_cheats` in CheatsInit, which retail never does. Retail CheatsInit 0x827C3208 (bls): KeyboardSubscribe, then SystemConfig(Symbol), then InitLongJoyCheats, with no quick-cheat calls, and 'quick_cheats' is absent from band.exe. Natively there are two outcomes depending on data. (a) If the shipped config's `#ifndef _SHIP` body is present, the L1+L2 / R1+R2 shift cheats are armed (RB3 data: R1+R2+Select = `{fake_win 3}`) for any player, because cheats default on (disable_cheats FALSE). (b) With `_SHIP` defined (System.cpp:629, W16-UA) the array is empty, so `FindArray("left")` FAILs, returns null, and InitQuickJoyCheats dereferences it. Not verified which one a native boot hits |
| 450 | `utl/GlitchFinder.cpp:10` | build: native definitions of statics/globals, no behaviour |
| 451 | `utl/MemHeap.cpp:209` | build: init of native-only mMinFreeBytes |
| 452 | `utl/MemHeap.cpp:371` | inert: alloc_fail.txt dump needs gMemTracker (tracking off by default) |
| 453 | `utl/MemHeap.cpp:393` | error path: out-of-memory after MILO_FAIL; native returns null, retail falls through |
| 454 | `utl/MemTrack.cpp:72` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 455 | `utl/MemTrack.cpp:116` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 456 | `utl/MemTrack.cpp:278` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 457 | `utl/MemTrack.cpp:340` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 458 | `utl/MemTrack.cpp:346` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 459 | `utl/MemTrack.cpp:356` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 460 | `utl/MemTracker.cpp:232` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 461 | `utl/MemTracker.cpp:325` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 462 | `utl/MemTracker.cpp:418` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 463 | `utl/MemTracker.cpp:468` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 464 | `utl/MemTracker.cpp:589` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 465 | `utl/MemTracker.cpp:686` | diag/opt-in: MemTracker reporting, reached only when MemInit's enableTracking config is on (off by default) |
| 466 | `utl/BinStream.cpp:139` | diag: failed-read counter (W16-UJ gate) |
| 467 | `utl/BinStream.cpp:148` | diag: failed-read counter (W16-UJ gate) |
| 468 | `utl/BinStream.cpp:153` | diag: AutoGlitchReport timing only |
| 469 | `utl/BinStream.cpp:201` | **FLAGGED, left**: aborts on a serialized String length >10000 or <0; retail operator>>(String) resizes and reads any length. A sanity guard against a misaligned stream; diverges only on a legitimately long string |
| 470 | `utl/BinStream.cpp:223` | host: native spin-wait helper, called only from native arms |
| 471 | `utl/BinStream.h:20` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 472 | `utl/BinStream.h:77` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 473 | `utl/BinStream.h:83` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 474 | `utl/ChunkStream.cpp:50` | host: native read-ahead depth 2 vs retail 3 (W16-UL, recorded there) |
| 475 | `utl/ChunkStream.cpp:166` | equivalent: explicit inits of members retail sets before use (native-only init) |
| 476 | `utl/MemMgr.h:3` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 477 | `utl/MemMgr.h:224` | build: include / declaration / linkage / layout, no behaviour (native-only member / declaration / DC3-shape virtual) |
| 478 | `utl/MakeString.cpp:136` | diag: MakeString overflow / type-mismatch notify only |
| 479 | `utl/MakeString.cpp:156` | diag: MakeString overflow / type-mismatch notify only |
| 480 | `utl/MakeString.cpp:193` | diag: MakeString overflow / type-mismatch notify only |
| 481 | `utl/MakeString.cpp:208` | diag: MakeString overflow / type-mismatch notify only |
| 482 | `utl/MakeString.cpp:270` | diag: MakeString overflow / type-mismatch notify only |
| 483 | `utl/MakeString.cpp:295` | diag: MakeString overflow / type-mismatch notify only |
| 484 | `utl/MakeString.cpp:320` | diag: MakeString overflow / type-mismatch notify only |
| 485 | `utl/MakeString.cpp:345` | diag: MakeString overflow / type-mismatch notify only |
| 486 | `utl/MakeString.cpp:370` | diag: MakeString overflow / type-mismatch notify only |
| 487 | `utl/MakeString.cpp:395` | diag: MakeString overflow / type-mismatch notify only |
| 488 | `utl/MakeString.cpp:421` | diag: MakeString overflow / type-mismatch notify only |
| 489 | `utl/MakeString.cpp:465` | diag: MakeString overflow / type-mismatch notify only |
| 490 | `utl/MakeString.cpp:508` | diag: MakeString overflow / type-mismatch notify only |
| 491 | `utl/Loader.cpp:17` | build: include / declaration / linkage / layout, no behaviour |
| 492 | `utl/Loader.cpp:83` | inert: mLoadStartMs is -1 from the ctor and never set, so the decrement never runs |
| 493 | `utl/Loader.cpp:260` | equivalent: explicit init; retail leaves mPlatform/mEditMode/mCacheMode to static zero-init and LoadMgr::Init stores the constant kPlatformXBox to sysplatform (`li r11,2`) |
| 494 | `utl/Loader.cpp:319` | guard: empty-queue return where retail would read front() of an empty list (UB); fires only for PollUntilLoaded before its loader is queued |
| 495 | `utl/Loader.cpp:353` | diag: duplicate-factory notify only |
| 496 | `utl/Loader.cpp:437` | dev diag: warn/log/dump only (`MILO_DEBUG && HX_NATIVE`) (circular-dependency MILO_FAIL, prints natively) |
| 497 | `utl/Loader.cpp:454` | **FIXED**: native restored mCurrentPeriod after PollUntilLoaded; retail fn_827BF608 (248 B) stores this+0x14 only once, the in-loop 1e30 (`stfs f31,0x14(r29)`), and has no restore, so a PollUntilLoaded nested in LoadMgr::Poll leaves the rest of that Poll unbudgeted. Natively the restore cut it back to mPeriod |
| 498 | `utl/Loader.cpp:510` | dev: null_platform option, default off |
| 499 | `utl/Loader.cpp:519` | dev: loadmgr_debug / loadmgr_print DataFuncs, script-only |
| 500 | `utl/MemMgr.cpp:80` | build: native definitions of statics/globals, no behaviour |
| 501 | `utl/MemMgr.cpp:84` | build: native definitions of statics/globals, no behaviour |
| 502 | `utl/MemMgr.cpp:104` | build: native definitions of statics/globals, no behaviour |
| 503 | `utl/MemMgr.cpp:937` | host allocator: temp-heap push/pop skipped only while gNumHeaps==0 (before MemInit / malloc-backed native heaps) |
| 504 | `utl/MemMgr.cpp:947` | host allocator: temp-heap push/pop skipped only while gNumHeaps==0 (before MemInit / malloc-backed native heaps) |
| 505 | `utl/MemMgr.cpp:1115` | build: native definitions of statics/globals, no behaviour |
| 506 | `world/Crowd3DCharHandle.cpp:54` | no retail: DC3-only class (class name absent from band.exe); stub SyncProperty for WorldCrowd3DCharHandle (factory native-only, #509) |
| 507 | `world/PhysicsVolume.cpp:16` | build: static definition for native-only PhysicsVolume |
| 508 | `world/World.cpp:35` | dev: object factory for BeatClock (class absent from band.exe); inert unless data names it |
| 509 | `world/World.cpp:41` | dev: object factory for WorldCrowd3DCharHandle (absent from band.exe); inert unless data names it |
| 510 | `world/World.cpp:52` | dev: object factories for PhysicsVolume / PostProcer (absent from band.exe); inert unless data names them |
| 511 | `world/ColorPalette.cpp:14` | dev: property retail lacks (native Hmx::Object propsync is name/type/sinks, all native-only; retail Hmx::Object::SyncProperty 0x8235C2E0 is empty); reachable only by a script naming it |
| 512 | `world/LightHue.cpp:16` | dev: property retail lacks (native Hmx::Object propsync is name/type/sinks, all native-only; retail Hmx::Object::SyncProperty 0x8235C2E0 is empty); reachable only by a script naming it |
| 513 | `world/LightPresetManager.cpp:84` | dev diag: light_preset overlay print, only while the overlay shows |
| 514 | `world/LightPresetManager.cpp:200` | dev diag: as #513 (not in retail StartPreset) |
| 515 | `world/LightPresetManager.cpp:283` | dev diag: as #513 (not in retail Poll 0x824B8A28) |
| 516 | `world/Dir.cpp:197` | dev: show_hud property retail lacks; no native code sets it |
| 517 | `world/Dir.cpp:619` | diag: RndGraph debug-line camera only (draws only queued debug graphs) |
| 518 | `world/CameraManager.cpp:71` | **rendering-coupled, listed**: NaN/inf frame clamp in CalcFrame; never fires on a finite time |
| 519 | `world/CameraManager.cpp:82` | inert: EditMode-only milo.anim camera (EditMode false in the app) |
| 520 | `world/CameraManager.cpp:169` | inert: MiloCamera() is null outside EditMode |
| 521 | `world/CameraShot.h:191` | build: native-only setter for mParentDir |
| 522 | `world/CameraShot.h:289` | build: native-only member mParentDir (after the matched layout) |
| 523 | `world/CameraShot.cpp:47` | build: native static definition |
| 524 | `world/CameraShot.cpp:322` | **rendering-coupled, listed**: skips LookAt at zero target distance (retail LookAt would NaN); fires only when camera sits on its target |
| 525 | `world/CameraShot.cpp:398` | **FIXED**: retail BuildTransform 0x824BF368 calls GetCurrentTargetPosition (bl 0x824BF278) then WorldToScreen (bl 0x824333A0) with no zero/empty test; native returned `tf = mWorldOffset` for every target-less shot, dropping the path, parent, clamp-height and b3 tail. The NaN case is covered by #526 |
| 526 | `world/CameraShot.cpp:415` | **rendering-coupled, listed**: zeroes a NaN screenPos (degenerate projection); never fires on a finite projection |
| 527 | `world/CameraShot.cpp:598` | **rendering-coupled, listed**: zero-distance LookAt guard (as #524) |
| 528 | `world/CameraShot.cpp:605` | **rendering-coupled, listed**: zero-distance LookAt guard (as #524) |
| 529 | `world/CameraShot.cpp:979` | build: ctor init of native-only mParentDir |
| 530 | `world/CameraShot.cpp:1071` | dev: property retail lacks (native Hmx::Object propsync is name/type/sinks, all native-only; retail Hmx::Object::SyncProperty 0x8235C2E0 is empty); reachable only by a script naming it |
| 531 | `world/CameraShot.cpp:1478` | **rendering-coupled, listed**: rejects NaN/inf SetFrame; never fires on a finite frame |
| 532 | `world/Instance.cpp:284` | robustness (dev): unregistered-class notify + DeleteObjects; retail 0x824EB9B8 calls NewObject unchecked |
| 533 | `world/Reflection.cpp:32` | dev: property retail lacks (native Hmx::Object propsync is name/type/sinks, all native-only; retail Hmx::Object::SyncProperty 0x8235C2E0 is empty); reachable only by a script naming it |
| 534 | `world/Crowd.cpp:51` | **rendering-coupled, listed**: native impostor cache storage (#542) |
| 535 | `world/Crowd.cpp:187` | **rendering-coupled, listed**: releases the native impostor cache (#542) |
| 536 | `world/Crowd.cpp:230` | dev: property retail lacks (native Hmx::Object propsync is name/type/sinks, all native-only; retail Hmx::Object::SyncProperty 0x8235C2E0 is empty); reachable only by a script naming it |
| 537 | `world/Crowd.cpp:327` | equivalent on the default path: retail Load 0x824E6F88 goes RndDrawable::Load (bl 0x82406B78) then mPlacementMesh load, no Reset3DCrowd (0x824E5538). On a fresh crowd mCharacters is empty, so Reset3DCrowd only sets mFlatFullness=1, which CreateMeshes sets again; differs only on a re-Load of a live crowd |
| 538 | `world/Crowd.cpp:400` | robustness: null-mMMesh guard (retail derefs) |
| 539 | `world/Crowd.cpp:999` | inert natively: retail Set3DCharList 0x824E57E8 forced path (`bne 0x824E59A8`) is the epilogue, no AssignRandomColors (0x824E2510). Native re-rolls colours per call, but mForce3DCrowd is forced false on native Load (two-sided arm) and only the force_3D_crowd property sets it. Crowd rendering area; left |
| 540 | `world/Crowd.cpp:1010` | robustness (dev): bad mesh-index notify + continue; retail walks past the list end |
| 541 | `world/Crowd.cpp:1026` | robustness (dev): bad 3D-char-index notify + continue; retail walks past the list end |
| 542 | `world/Crowd.cpp:1225` | **rendering-coupled, listed**: native cached impostor render path replaces retail's per-frame impostor RTT (characters do not animate natively) |
| 543 | `world/LightPreset.cpp:402` | native object model: frees the heap ring nodes (#544) in the dtor |
| 544 | `world/LightPreset.cpp:412` | native object model: HoldRef/DropRef stand in for retail Hmx::Object::AddRef(ObjRefOwner*) 0x8275BD08 / Release 0x8275B378 |
| 545 | `world/LightPreset.cpp:579` | dev: property retail lacks (native Hmx::Object propsync is name/type/sinks, all native-only; retail Hmx::Object::SyncProperty 0x8235C2E0 is empty); reachable only by a script naming it |
| 546 | `world/LightPreset.cpp:830` | equivalent: also empties mSpotlightDrawers after releasing them; every Clear caller (dtor, Load with rev>9 resize + Find refill, Copy reassign) overwrites or discards the list |
| 547 | `world/LightPreset.cpp:1169` | inert: EditMode only |
| 548 | `world/LightPreset.cpp:1665` | native object model: drops the held node on the not-found Replace path (retail's ring walk already removed the ref) |
| 549 | `world/LightPreset.h:269` | build: native ring-node members/declarations (after the matched layout) |
| 550 | `bandobj/BandCharacter.h:144` | **char session, listed**: X-lane rebind declarations |
| 551 | `bandobj/BandCharacter.h:293` | **char session, listed**: native-only member unk6d8 (layout) |
| 552 | `bandobj/BandCharacter.h:298` | **char session, listed**: rebind bookkeeping members (layout) |
| 553 | `bandobj/BandConfiguration.cpp:66` | diag: MILO_WARN when a waypoint slot does not resolve |
| 554 | `bandobj/TrackPanelDirBase.cpp:163` | **FIXED**: retail SetConfiguration 0x823574B0 has one bctrl (Handle) and no bl to SetShowing 0x823570C8. Native re-showed the HUD (SetShowing(gShowHUD) when not in performance mode) on every configuration apply |
| 555 | `bandobj/TrackPanelDirBase.cpp:176` | **FIXED**: retail ReapplyConfiguration 0x82357640 likewise has one bctrl (Handle), no SetShowing. Native re-showed the HUD on every reapply, overriding any intro/script hide |
| 556 | `bandobj/TrackPanelDir.cpp:18` | build: include / declaration / linkage / layout, no behaviour |
| 557 | `bandobj/BandPatchMesh.h:7` | build: include / declaration / linkage / layout, no behaviour |
| 558 | `bandobj/SongSectionController.cpp:260` | dev state: clears debug pool/section syms, read only by DebugActivate (debug handler) and Save |
| 559 | `bandobj/SongSectionController.cpp:294` | dev diag: cheat.song_section_ctrl DataVariable drives the debug overlay only |
| 560 | `bandobj/VocalTrackDir.cpp:398` | equivalent: empty TypeProps stand-in for native lazily-allocated TypeProps (zero iterations either way) |
| 561 | `bandobj/CharKeyHandMidi.cpp:134` | dev diag: editor Highlight() debug spheres; retail slot is the shared empty blr 0x826C3888 |
| 562 | `bandobj/GemTrackResourceManager.cpp:151` | lifetime: native ArpeggioShape / pool dtor bodies (no retail row); run only via #602 |
| 563 | `bandobj/BandCharacter.cpp:12` | **char session, listed**: includes |
| 564 | `bandobj/BandCharacter.cpp:129` | **char session, listed**: init of native-only members |
| 565 | `bandobj/BandCharacter.cpp:263` | **char session, listed**: dev no_anim DataVariable (default 0) |
| 566 | `bandobj/BandCharacter.cpp:415` | **char session, listed**: cc_poll AutoTimer only |
| 567 | `bandobj/BandCharacter.cpp:449` | **char session, listed**: EditMode-only clip starvation (inert in the app) + unk6d8 store |
| 568 | `bandobj/BandCharacter.cpp:517` | **char session, listed**: BAND_ANIM_PROBE env probe (default off) |
| 569 | `bandobj/BandCharacter.cpp:544` | **char session, listed**: probe print + **default-on** RebindOutfitBonesToOwnSkeleton / RebindSharedSkinMatsToOwn after Character::Poll (native renderer/skeleton workaround; retail has neither) |
| 570 | `bandobj/BandCharacter.cpp:836` | **char session, listed**: bodies of the two rebind workarounds (#569) |
| 571 | `bandobj/BandCharacter.cpp:1339` | **char session, listed**: RB3_X21_TRACE env trace |
| 572 | `bandobj/BandCharacter.cpp:1405` | **char session, listed**: SET_SKEL_REBIND / SKEL_REBIND_PROBE env probe (default off) |
| 573 | `bandobj/BandCharacter.cpp:1499` | **char session, listed**: RB3_TRACE_KEEPMESH env trace |
| 574 | `bandobj/BandCharacter.cpp:1703` | **char session, listed**: dev show_spheres debug draw (DataVariable default 0) |
| 575 | `bandobj/BandCharacter.cpp:1725` | **char session, listed**: dev show_slot debug text (DataVariable default 0) |
| 576 | `bandobj/BandCharacter.cpp:1987` | **char session, listed**: RB3_X21_TRACE env trace |
| 577 | `bandobj/BandCharacter.cpp:2005` | **char session, listed**: RB3_NO_DEFORM env opt-out (default off) |
| 578 | `bandobj/BandCharacter.cpp:2009` | **char session, listed**: RB3_X21_TRACE env trace |
| 579 | `bandobj/BandCharacter.cpp:2525` | **char session, listed**: toggle_interests_overlay script arm retail lacks |
| 580 | `bandobj/BandCharacter.cpp:3226` | **char session, listed**: RB3_X21_TRACE env trace |
| 581 | `bandobj/BandHeadShaper.cpp:7` | **char session, listed**: include for the RB3_NO_HEAD_SHAPER getenv |
| 582 | `bandobj/BandHeadShaper.cpp:85` | **char session, listed**: null-dir guard; the comment says gFemaleDir stays null natively because Init loads the female head through gMaleDir, so it hides a native head-shaper bug |
| 583 | `bandobj/BandHeadShaper.cpp:120` | **char session, listed**: null-dir guard, as #582 |
| 584 | `bandobj/BandHeadShaper.cpp:135` | **char session, listed**: RB3_NO_HEAD_SHAPER env opt-out (default off) |
| 585 | `bandobj/BandHeadShaper.cpp:159` | **char session, listed**: RB3_NO_HEAD_SHAPER env opt-out (default off) |
| 586 | `bandobj/BandHeadShaper.cpp:383` | **char session, listed**: dev TestMesh check |
| 587 | `bandobj/BandTrack.cpp:192` | **FIXED**: retail Reset 0x82352828 goes SendTrackerDisplayMessage (bl 0x8234F880) then the mFailedFeedback HandleType; no Find<UILabel> (0x8234E170) / SetTokenFmt (0x822D79B8). Native zeroed solo_percent.lbl on every reset |
| 588 | `bandobj/BandTrack.cpp:509` | **FIXED**: retail SoloStart 0x82351808 goes start_solo HandleType (bl 0x8275ABD8) then Find<EventTrigger> (bl 0x822B1308); no UILabel write. Native zeroed solo_percent.lbl at solo start, with Find(..., true) that fails if the label is missing. SoloHit 0x82351B10 stays the only writer, as in retail |
| 589 | `bandobj/OutfitConfig.cpp:23` | **char session, listed**: includes for env opt-outs |
| 590 | `bandobj/OutfitConfig.cpp:156` | **char session, listed**: native compose-scope flag for BandRnd::DrawRect (renderer workaround) + RB3_X21_TRACE |
| 591 | `bandobj/OutfitConfig.cpp:538` | **char session, listed**: RB3_X21_TRACE env trace |
| 592 | `bandobj/OutfitConfig.cpp:552` | **char session, listed**: RB3_X21_TRACE env trace |
| 593 | `bandobj/OutfitConfig.cpp:599` | **char session, listed**: RB3_X22_AUTHORED_DIFFUSE diagnostic (default off) |
| 594 | `bandobj/OutfitConfig.cpp:1152` | **char session, listed**: RB3_X21_TRACE env trace |
| 595 | `bandobj/OutfitConfig.cpp:1167` | **char session, listed**: EditMode-only reprojection (inert in the app) |
| 596 | `bandobj/OutfitConfig.cpp:1181` | **char session, listed**: InMilo-only posing (inert in the app) |
| 597 | `bandobj/MicInputArrow.cpp:23` | build: `static` linkage for a per-TU global, value identical |
| 598 | `bandobj/BandDirector.cpp:159` | inert: dead store; sMotionBlurBlendAmount has no reader, and `(rnd (motion_blur 0.3))` exists in system/run/config/default.dta so the lookup cannot fail (band.exe has only motion_blur_* prefixed strings) |
| 599 | `bandobj/BandDirector.cpp:413` | inert: native Hmx::Object::Replace only forwards to mSinks (no native site uses Object-level sinks, lane `sinks`); RndDrawable has no Replace, so both calls land there. Retail body is the empty blr 0x826C3888 |
| 600 | `bandobj/BandDirector.cpp:466` | dev diag: MILO_LOG when no BandCamShot matches |
| 601 | `bandobj/BandDirector.cpp:934` | dev handler: four debug script arms |
| 602 | `bandobj/GemTrackDir.cpp:77` | lifetime: native releases mArpShapePool / mFingerShape, which retail ??1GemTrackDir leaks; no other observable effect. Left |
| 603 | `bandobj/GemTrackDir.cpp:1190` | robustness: null-mFingerShape guard (retail 0x822E36B8 tail-calls through) |
| 604 | `bandobj/GemTrackDir.cpp:1236` | inert: EditMode-only chord mesh names |
| 605 | `bandobj/ChordShapeGenerator.cpp:257` | diag: Notify when chord params are missing |
| 606 | `bandobj/ChordShapeGenerator.cpp:333` | inert: EditMode-only Sync |
| 607 | `bandobj/BandWardrobe.cpp:660` | **char session, listed**: **divergent, X8 lane**: retail LoadMainCharacters 0x82330040 remaps only "none" -> "vocals" (`bl 0x8227C6D0` Symbol==, then Symbol(0x8201DB6C "vocals")) before MakeString("player_%s0") (0x82034CE8); there is no mic remap, so retail builds player_mic0. Native maps mic -> vocals. How retail places the singer is unresolved (X8 found no player_mic0 in any venue) |
| 608 | `bandobj/BandWardrobe.cpp:896` | dev handler: four debug script arms (retail Handle builds 14 Symbols, ending at get_matching_dude) |
| 609 | `bandobj/BandWardrobe.cpp:1386` | build: `static` linkage for a per-TU comparator |
| 610 | `dsp/PitchDetector.cpp:85` | dev diag: warn/log/dump only (`MILO_DEBUG && HX_NATIVE`) (PitchDetector dump behind sDump, off) |
| 611 | `dsp/PitchDetector.cpp:207` | dev diag: warn/log/dump only (`MILO_DEBUG && HX_NATIVE`) (PitchDetector dump behind sDump, off) |
| 612 | `track/TrackWidgetImp.cpp:348` | **other session (Rnd/Draw)**: SetMeshForceNoUpdate queues the dirty text mesh to RndText's deferred set before DrawShowing (retail draws directly); render-path change, listed only |
| 613 | `track/TrackWidget.cpp:323` | inert: TrackWidget::CheckValid is empty and LOADMGR_EDITMODE is false by default |
