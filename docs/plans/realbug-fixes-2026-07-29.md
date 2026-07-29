# laneBI — the 109 content-proven wrong constants/strings in 100%-scoring functions (2026-07-29)

Input: `docs/plans/laneBH_realbugs.json` (109 entries), produced by
`scripts/harvest/reloc_correspondence.py --census` and documented in
`docs/plans/reloc-correspondence-audit-2026-07-29.md` (laneBH §5).
Worktree `/home/free/tmp/wt-laneBI-1` (branch `laneBI-1`, from main `4a9a81f9`);
full `./tools/ninja-locked` run BEFORE any obj-derived measurement.

**Baseline 39,736 `matched_functions` → after all fixes 39,736. 0 lost, 0 gained**
(strict sets compared by `(unit, name)` AND by bare name; `report.cache` removed
before every read; two full builds).

Ground truth for every adjudication below is **`orig/45410914/band.exe`** — the
decompressed retail PE (imagebase `0x82000000`). Every "retail value" quoted is a
byte read out of that image at the VA the target relocation actually points at,
not an inference from a map or an oracle. Oracles (`../rb3` = rb3-Wii DEV build,
`../dc3-decomp` = DC3) are corroboration only, never the decider.

Scratch expander used throughout: `/home/free/tmp/laneBI_triage.py` — prints, per
DIVERGE slot, OUR operand next to the retail bytes at the paired VA. It is the
one tool this lane would keep.

## 0. TL;DR

1. **The class is real and worth having opened. 35 behavioural defects fixed**
   (plus 5 ordering-only edits, 40 source edits over 37 files), none of which any
   near-miss scanner can see, because they all live behind a masked relocation in
   a function that already reads 100.0.
2. ★ **But 47 of the 109 (43%) are NOT source defects.** The dominant mechanism
   is **pairing mislabel**: `scripts/target_symbol_map.json` / `splits.txt`
   assigned our symbol's name to a retail VA belonging to a *different function
   of the same shape*. laneBH's instrument is not wrong — the relocation really
   does point elsewhere — but "points elsewhere" and "our source is wrong" are
   different claims, and the worklist conflated them.
3. ★★ **The discriminator, which any future run must apply before editing (§3.5):**
   *if retail's diverging operands coherently describe a different function — a
   sibling in the same TU, another template instantiation, or another class
   entirely — the defect is in the map, not in the source.* Two of the four
   examples laneBH highlighted by name fall to it.
4. **Metric movement is 0 and that is the correct outcome** (§5). Correctness and
   count were never traded against each other.

## 1. Defect-shape frequency table

All 109 entries, classified by what the DIVERGE slots point at:

| shape | n | share | verdict |
|---|--:|--:|---|
| data/label target (`lbl_`, `jumptable_`) — constants, strings, tables | 53 | 48.6% | mostly REAL |
| named **twin** target (same template family, different instantiation) | 24 | 22.0% | REFUTED (map artifact) |
| anonymous `fn_` target only | 15 | 13.8% | UNDECIDABLE |
| other named target (sibling / different class) | 17 | 15.6% | mixed |

By operand kind (a function can contribute several):

| operand kind | n |
|---|--:|
| function / call target | 29 |
| template-twin STL symbol (`?$`) | 35 |
| string literal `??_C@` | 20 |
| float constant `__real@` | 19 |
| compiler-generated (`$T` jump table, `$SG`, `$LN`) | 3 |
| RTTI `??_R0` type descriptor | 1 |

Adjudicated totals: **35 behavioural fixes / 5 ordering-only / 47 refuted or
undecidable / 22 confirmed-real but deferred (deep body or table work)**.

## 2. Fixes landed

### 2.1 Data constants (engine)

| file:line | symbol | ours | retail (band.exe VA) | class |
|---|---|---|---|---|
| `rndobj/Draw.cpp:42` | `gSaveRev_RndDrawable` | 4 | **3** (`0x82C6FB04`) | BEHAVIORAL — writes a wrong save revision into every `.milo` |
| `rndobj/AmbientOcclusion.cpp:25` | `kQualityLUT[]` | `{256,1024,0,2}` | **`{300,150,2,0}`** (`0x82070E04`) | BEHAVIORAL — all four entries wrong (sample counts *and* packDepth) |
| `rndobj/AmbientOcclusion.cpp:266` | `Box box(...)` | `Box(-FLT_MAX…, +FLT_MAX…)` | **`Box(+FLT_MAX…, -FLT_MAX…)`** | BEHAVIORAL — retail seeds the classic *empty* accumulator box; ours seeded a full box, so the subsequent `GrowToContain` loop could never shrink it |
| `rndobj/Bitmap.cpp:16` | `BITMAP_REV` | 2 | **1** (`0x82C6F87A`) | BEHAVIORAL |
| `meta/SongMgr.cpp:52` | `gSongCacheSaveVer` | 12 | **13** (`0x82C78B7C`) | BEHAVIORAL — song-cache format gate |
| `synth/Utl.cpp:10` | `measuresMs[7]` | `{0.0625…1.0}` | **`{0.25,0.5,0.75,1,1.5,2,4}`** (`0x82C75F18`) | BEHAVIORAL — ours was retail/4 throughout; tempo-sync rates 4× off |
| `os/Joypad.cpp:29` | `gKeepaliveThresholdMs` | `-1` | **`0x7FFFFFFF`** (`0x82C71AFC`) | BEHAVIORAL — `-1` vs `INT_MAX` invert every threshold comparison |
| `world/SpotlightDrawer_NG.cpp:113` | `sSheetIntensity` | `3.0f` | **`8.0f`** (`0x82C71194`) | BEHAVIORAL |
| `world/SpotlightDrawer_NG.cpp:439` | `kFogScale` | `256.0f` | **`10.0f`** (`0x82C71188`) | BEHAVIORAL — used as `1/kFogScale` shader param |
| `rndobj/Rnd.cpp:1307` | `sDefSize[8][2]` | all `{8,8}` | **`{8,8}×5, {64,64}, {256,8}, {128,128}`** (`0x8205EA88`) | BEHAVIORAL |
| `rndobj/Rnd.cpp:1312` | `sDefColor[8][4]` | retail's row 1 `{0,0,0,0}` **missing**, everything shifted up, last row wrong | full retail table (`0x8205EA68`) | BEHAVIORAL — every default texture past index 0 had the wrong colour |
| `synth/ByteGrinder.cpp:92,119` | `s_seed` in `getRandomSequence32A/B` | A=`0x521`, B=`0x303F` | **A=`0x303F`, B=`0x521`** (`0x82C7629C`/`0x82C76298`) | BEHAVIORAL — the two PRNG seeds were swapped |

### 2.2 Floating-point constants

Every replacement was verified with `struct.pack` to reproduce retail's exact bit
pattern before being applied.

| file:line | ours (hex) | retail (hex) | class |
|---|---|---|---|
| `math/Rot.h:6` + `math/Trig.h:12` — `RAD2DEG` / `RadiansToDegrees` | `57.29578f` = `0x42652EE1` | **`57.295776f` = `0x42652EE0`** | BEHAVIORAL — 1 ULP, but it reaches **8 functions across 6 units** (`PropSync(CamShotFrame)`, `Waypoint::SyncProperty`, `AngleVectorSync`, `RndTransformable::OnGetWorldRot`/`OnGetLocalRotIndex`, `RndMultiMesh::OnGetRot`, `DataASin`/`DataACos`/`DataATan`). `char/CharEyes.cpp:694` already had the correct literal — the tree was internally inconsistent. |
| `soundtouch/AAFilter.cpp:53` — `PI` | `3.141592655357989` = `0x400921FB5480EE4F` | **`0x400921FB60000000`** (= float-precision π widened); `TWOPI` follows to `0x401921FB60000000` | BEHAVIORAL. ★ Corrects laneBH's characterisation: this is **not** "π where retail has 2π" — both constants are π and 2π on both sides; retail's are *float-rounded*. |
| `synth/EQEffect.cpp:282` | `1.3089969e-04f` = `0x3909421E` | **`0x3909421F`** | BEHAVIORAL (1 ULP) |
| `meta_band/CalibrationPanel.cpp:665` | `14.0f` = `0x41600000` | **`8.0f` = `0x41000000`** | BEHAVIORAL. rb3-Wii also has 14.0f — a genuine retail-360-vs-Wii divergence in a hardware-latency constant. |
| `meta/StreamPlayer.cpp:73` — `SetJump` | `-0.25f` = `0xBE800000` | **`0xB4000000`** = −2⁻²³ | BEHAVIORAL, **MEDIUM confidence** — band.exe is unambiguous but *both* DC3 and rb3-Wii say −0.25f. Flagged for a second opinion. |
| `band3/game/VocalPart.cpp:30` | `std::log(0.1)` = `0x3FB999999999999A` | **`0x3FB99999A0000000`** = `(double)0.1f` | BEHAVIORAL. Used the exact double literal rather than `0.1f`, because `0.1f` would select `std::log`'s float overload and change the call target. |
| `band3/game/VocalGuidePitch.cpp:65,72` | `60.0f*dur*(1.0f/1000.0f)` folds to `0x3D75C290` | **`0x3D75C28F`** (= `0.06f`) | BEHAVIORAL |
| `rndobj/PostProc_NG.cpp:384` | 2π and deg2rad loaded in our order | retail's order | **ORDERING-ONLY** (independent statements; no data dependency) |

### 2.3 String operands (engine)

| file | ours | retail | class |
|---|---|---|---|
| `rndobj/Part.cpp` `RndParticleSys::SyncProperty` | `motion_parent` | **`relative_parent`** | BEHAVIORAL — rb3-Wii agrees with retail; DC3 (newer) is where our wrong name came from |
| `rndobj/Lit.cpp` `RndLight::SyncProperty` | `light_type` | **`type`** | BEHAVIORAL — same DC3-rename provenance |
| `ui/UIList.cpp` `UIList::Handle` | `allow_highlight` | **`set_draw_manually_controlled_widgets`** | BEHAVIORAL. Decisive: `strings band.exe \| grep -x allow_highlight` → **zero hits anywhere in retail.** The handler simply does not exist in RB3; it is a DC3 feature name grafted onto retail's message. |
| `world/LightPreset.cpp` `StartAnim` | `start_anim_msg` | **`start_anim`** | BEHAVIORAL |
| `ui/UIScreen.cpp` `Poll` | `poll_msg` | **`poll`** | BEHAVIORAL |
| `ui/UIFontImporter.cpp` ctor | `temp.bmp` | **`temp.BMP`** | BEHAVIORAL (360 FS calls are case-preserving) |
| `synth_xbox/FxSendMeterEffect.cpp` `InitParams` | `mono` | **`center`** | BEHAVIORAL |
| `os/Timer.cpp` `AutoTimer::Init` | `show_timer` | **`show_tier`** | BEHAVIORAL — a genuine **retail typo**, reproduced deliberately. Proof: the retail literal pool at `0x82086F70` reads `timer / print_timers / timer_ms / set_timer_ms / show_tier / %s.dta / …` — unmistakably Timer.cpp's own pool, and every other slot in the function CORRESPONDS. |
| `rndobj/Rnd_NG.cpp` `NgRnd::Terminate` | `…NgDOFProc::Terminate(); RndShadowMap::Terminate();` | **ShadowMap before DOFProc** | BEHAVIORAL (teardown order). ★ The *stated* premise ("we call `ReleaseTex` where retail calls `Terminate`") was **REFUTED** — our source already called `NgPostProc::Terminate()` — but positional analysis found a real call-order bug next to it. |
| `obj/DirLoader.cpp` `FixClassName` | `TexRenderer`/`CompositeTexture`, `BandFx`/`WorldFx`, `View`/`Group` declaration order | retail's order (verified in the function-local literal pool at `0x821064AC–0x82106590`) | **ORDERING-ONLY.** ★ Corrects laneBH: this is **not** "the remap table is permuted". The literals only feed `static Symbol` declarations; all mapping *directions* (`RenderedTex→TexRenderer`, `CompositeTexture→LayerDir`, `BandFx→WorldFx`, `View→Group`) are confirmed correct against rb3-Wii. No behaviour changes. |

### 2.4 String and call operands (game)

| file:line | ours | retail | class |
|---|---|---|---|
| `meta_band/EditSetlistPanel.cpp:232` | `unk64 ? setlist_save_local : setlist_save_share` | **inverted** (retail's `this+0x68` test branches TRUE→`share`) | BEHAVIORAL — the panel showed the wrong title. (A separate struct-offset drift, source 0x64 vs retail 0x68, was observed and left alone.) |
| `meta/MemcardMgr_Xbox.cpp:37` | `SetContainerName("savegame")` | **`"band3"`** (`0x8208954C`) | BEHAVIORAL |
| `meta_band/AppLabel.cpp:779` | `"%s%s"` | **`"<alt>%s</alt> %s"`** (`0x820B0278`) | BEHAVIORAL |
| `meta_band/StoreMenuPanel.cpp:100` | `"%s%s"` | **`"%s::%s"`** (`0x820B0978`) | BEHAVIORAL |
| `meta_band/MusicLibrary.cpp:2223` | `HANDLE_ACTION(rebuild_restricted_data, RebuildRestrictedData())` | **`RebuildSharedSongData()`** (retail's arm `bl 0x8253DF28`; zero calls to `RebuildRestrictedData` anywhere in the function) | BEHAVIORAL |
| `meta_band/AccomplishmentPanel.cpp:725` | `SelectGroup` → `UpdateForGoalSelection()` | **`UpdateForGroupSelection()`** (`0x825FE8A0`) | BEHAVIORAL |
| `band3/game/GemPlayer.cpp:2268` | `SetFilling` else-branch called `FadeOutDrums` (a copy-paste of the true branch) | **`RestoreDrums`** (`0x8277DF08`) | BEHAVIORAL — drums never came back |
| `ui/UIListProvider.cpp:122` | `mList->GetUIListDir()` | **`mList->ResourceDir()`** (`0x827FE8A8`) | BEHAVIORAL |
| `meta_band/AccomplishmentPanel.cpp:582` | `acc_multiplayersession / acc_createsetlist / acc_HMXrecommends` decl order | retail's order | ORDERING-ONLY |
| `net_band/RockCentral.cpp:1506` | `art_id` / `art` decl order | retail's order | ORDERING-ONLY (key↔value pairing was already correct) |

## 3. Refuted — the false-positive analysis

**47 of 109 (43%) have no source defect to fix.** Three mechanisms, all proven by
direct byte evidence.

### 3.1 Template twins — 24 entries (22%)

Our symbol was paired, by name from `target_symbol_map.json`, with a retail VA
that holds a *different instantiation of the same template*. Worked example
(`default/BandCamShot`):

```
our  ?resize@?$list@UBitmapOverride@WorldDir@@…      paired to retail 0x822B6538
its  erase() call lands on 0x822B5648
map: 0x822B5648 = ?erase@?$list@UTarget@HamCamShot@@…
map: 0x822B56B8 = ?erase@?$list@UBitmapOverride@WorldDir@@…   <-- also present!
```

Both instantiations exist in the map. The retail function at `0x822B6538` is
really `resize<list<Target>>`; the label is wrong. Our source is correct. The
same shape covers all the `resize`/`erase`/`_M_splice_insert_dispatch`/
`_List_base::clear`/`ObjRefConcrete<>`/`ObjDirPtr<>`/`Find<T>@ObjectDir` entries
and the whole `AccomplishmentPanel` `__merge_*`/`__lower_bound`/`__upper_bound`
family (which is symmetric: `AccomplishmentCmp`↔`GoalAlpaCmp` each pointing at
the other — the signature of a crosswise map assignment).

### 3.2 Whole-function attribution errors — 2 entries, both flagged by laneBH

* **`BaseMaterial::Handle` — retail's function at that VA is `TextFile::Handle`.**
  Its three arms read `print`/`printf`/`reflect` and its three callees resolve to
  `?OnPrint@TextFile@@`, `?OnPrintf@TextFile@@`, `?OnReflect@TextFile@@`. Our own
  `src/system/obj/TextFile.cpp:72` literally contains `HANDLE(printf, OnPrintf)`.
  Retail's slot-0 string `print` sits in a pool with `set_default_clip` /
  `transfer` / `set_beat_scale` — not a material's pool. Our `BaseMaterial::Handle`
  is correct; the split/map is not.
* **`Watcher::Handle` — retail's function at that VA is `BandConfiguration::Handle`.**
  Its arms are `store_configuration` / `release_configuration` / `sync_play_mode`,
  which is *verbatim* `../rb3/src/system/bandobj/BandConfiguration.cpp:145-147`.

Both reached 100% because a three-arm `BEGIN_HANDLERS` body is byte-identical
once its relocations are masked. That is exactly laneBH §4's "shape, not
reproduction" — but at the level of *which function*, not *which pointer*.

### 3.3 Same-TU sibling mispairs — 3 entries

* `StoreInfoPanel::PushRecommendationsReady` → retail's VA disassembles to the
  sibling `PushRecommendationFailure` (our own `StoreInfoPanel.cpp:155` already
  has it, correctly, with `no_recommendations_msg`).
* `TrackerBroadcastDisplay::ShowBriefBandMessage` → mispaired with `fn_826D54F0`;
  our `SetBandMessage`/`ShowBriefBandMessage` pair is self-consistent (a prior
  session had already left a comment in the file recording this).
* `PropSync(MsgSource::EventSink)` → retail's operands (`category`, `texture`,
  `PropSync<int>`) describe a different struct's `PropSync` in the same TU.

### 3.4 Instrument/premise errors — 3 entries

* **`Symbol::PrintSymbolTable`** — the premise ("we call `UsedSize()` then
  `Size()`, retail the other way") compares *source text order* with *compiled
  call order*. Under MSVC's right-to-left argument evaluation our written order
  `MILO_LOG(fmt, UsedSize(), Size())` compiles to `Size()` first — which is
  exactly retail's order. No defect. (All four `MILO_LOG` format strings are
  absent from retail entirely, consistent with the known MILO_DEBUG-drift
  pattern; only the argument sub-expressions with real side effects survive.)
* **`BeatMatcher::FretButtonUp` / `RGFretButtonDown`** — two ~136 B forwarders,
  byte-identical once masked; the divergence is perfectly symmetric. Map artifact.
* **`BandCharacter::PlayFaceClip`** — rb3-Wii has the identical call
  `Play(clip, 4, -1.0f, 1e+30f, 0.0f)`. A named-function oracle confirming our
  argument order outweighs a constant-load-order difference, which at `/O1` is
  schedulable. Left alone.

### 3.5 ★ The discriminator (use this before editing anything)

> If retail's diverging operands **coherently describe a different function** —
> a sibling in the same TU, another template instantiation, or another class —
> the defect is in `target_symbol_map.json` / `splits.txt`, **not in our source**.
> Only when the operand is a *value* that the same function would legitimately
> hold (a revision number, a LUT, a format string, a shared math constant) is it
> a source defect.

Corollary that made every hard call in this lane: **a permutation of the same
string set is usually `static Symbol` declaration order** (behaviourally inert),
whereas **a genuinely different string is a real defect**. Never invert a
mapping's direction on the strength of literal ordering alone.

### 3.6 Undecidable — 15 entries

The retail callee is an anonymous `fn_<VA>` with no map entry, so no oracle can
say whether the pointer is right. Not refuted, not confirmed. (`BandDirector::Handle`,
`Tour::Handle`, `SigninScreen::ReEvaluateState`, `VocalPlayer::Restart`/`HookupTrack`,
`TourProgress` ×3, …)

## 4. Confirmed real but deferred

| symbol | what is wrong | why deferred |
|---|---|---|
| `MusicLibrary::Handle` (6,160 B) | 3 `??_R0` RTTI slots: we `dynamic_cast` to `Hmx::Object` where retail casts to `StoreSongSort` / `StoreOffer`; plus one unresolved callee | 300/537 slots correspond, so the pairing is sound and these are real — but it is a body-port, not an operand edit |
| `SaveLoadManager::GetDialogMsg` (6,592 B) | our `$T186281` switch jump-table content differs from retail's | switch arm set/order; deep |
| `CustomizePanel::GetWearing` | same (jump table + `$LN29`) | deep |
| `NgRnd::UpdateOverlay` | retail prints `multimesh / flares / motion blur / spotlights`; we print `multimesh instances / multimesh batches / flares / motion blur` | our `NgStats` has **no spotlight counter at all**; fixing the labels alone would print the wrong quantity, and fabricating a counter was explicitly out of bounds |
| `Quazal::MemoryManager` ctor | `__FILE__` expands to our build path; retail's is `.\Core\MemoryManager.cpp` | BUILD-ENV class, tree-wide, not a logic defect — worth its own sweep |
| 17 others | one-off `lbl_` targets in `.data`/`.rdata` with no obvious source site | need per-case work |

## 5. The two totals, reported separately

* **Correctness fixes landed: 35 behavioural + 5 ordering-only = 40 source edits
  across 37 files.**
* **Strict-count movement: 0** (39,736 → 39,736, 0 lost, 0 gained).

That is the designed outcome, not a disappointment: every operand fixed here is
reached through a relocation, and scoring runs with `functionRelocDiffs=none`, so
by construction the count cannot move. A fix that clears divergence and holds the
count is the win condition for this lane.

## 6. Is a standing scanner worth it?

**Yes, and the unexamined remainder is large.** From laneBH's own census
(`/home/free/tmp/laneBH_census_full.json`, 39,520 rows):

| population | n |
|---|--:|
| functions ≥128 B at 100.0 | **6,474** |
| …of which DIVERGENT | 248 |
| …of which laneBH surfaced (named + content-proven) | **109** |
| …unexamined in that band (consistency- or map-proven divergence) | **139** |
| named + content-proven DIVERGENT, **tree-wide, all sizes** | **620** |
| …of which examined by this lane | 109 (17.6%) |
| …unexamined, <128 B, across 271 units | **511** |

So the ≥128 B slice laneBH cut is about one sixth of the content-proven
population. Recommendations:

1. **Wire the discriminator into the tool, not into the reader's head.** Before
   emitting a "real bug", `reloc_correspondence.py` should check whether the
   retail operand is explainable as a sibling/twin — cheaply: is there another
   map entry whose name differs from ours only in a template argument, or whose
   VA lies in the same pinned unit span? That single filter would have removed 27
   of the 109 automatically and would raise the worklist's precision from 57% to
   near 90%.
2. **Run it over the <128 B population next**, but expect the false-positive rate
   to be *higher* there (small functions are shape-degenerate). The discriminator
   is a prerequisite, not an optimisation.
3. **Feed the refuted set back to the map channel.** The 24 template twins and
   the 5 attribution errors are free, precise, byte-proven corrections for
   `target_symbol_map.json` — a different lane's yield, but this lane found them.
4. Keep `/home/free/tmp/laneBI_triage.py` (or fold it into the tool as a
   `--explain` mode). Reading "OURS=`allow_highlight` / RETAIL=`set_draw_manually
   controlled_widgets`" side by side is what made 40 edits tractable in one pass.

## 7. Reproduction

```bash
scripts/setup_worktree.sh ~/tmp/wt-laneBI-1 laneBI-1
cd ~/tmp/wt-laneBI-1 && ./tools/ninja-locked          # MANDATORY: dirty-obj reflink trap
rm -f build/45410914/report.cache                     # before EVERY report read
python3 scripts/harvest/reloc_correspondence.py --census --out /tmp/after.json
python3 /home/free/tmp/laneBI_triage.py [unit-or-symbol-substring]
```
