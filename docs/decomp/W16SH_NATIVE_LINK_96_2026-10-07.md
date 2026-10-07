# W16-SH: the 96 in-scope files native did not compile (2026-10-07)

Lane W16-SH, branch `w16-sh`, from main `351d4bf21`. Brief: CAMPAIGN_STATE_2026-10-07
§6 lever 5 (the brief called it lever 4) — link into the native build as many of the
**96 in-scope files / 97,340 B of gap rows** that native does not compile, replace
native stubs with the real code, add runtime gates for what newly runs, and keep the
X360 match build unchanged.

## 1. The 96, re-measured: 8 were already compiled

I reproduced the list from W16-SD's own artifacts (`~/tmp/w16sd-gap/dispo_rows.json`
keyed against `native_src.json`): **96 files, 97,340 B**, the same figures as §4.3.

W16-SD's "native-compiled" set is the **compile edges** of `native/build/build.ninja`.
That misses scatter guests, which are compiled inside their host's TU and have no
edge of their own. `tools/native_linked_tus.py`, which asks the preprocessor
(`clang++ -M`), reported on main's native build (UNION 520 `src/` TUs, self-validation
passed) that **8 of the 96 (28,376 B, 51 rows) were already compiled natively**:

| file | gap B | emitted by |
|---|---:|---|
| bandobj/BandPatchMesh.cpp | 7,584 | world/LightPreset.cpp (listed in MILO_TARGET_COMMON_SOURCES; ScatterIncludes prunes the standalone compile) |
| bandtrack/GemManager.cpp | 5,492 | char/CharBonesMeshes.cpp |
| bandobj/BandDirector.cpp | 4,620 | rndobj/Font.cpp, ui/UIList.cpp, bandobj/BandLabel.cpp, game/Stats.cpp (guarded in some) |
| bandobj/BandCamShot.cpp | 4,124 | bandobj/BandCharDesc.cpp |
| bandtrack/GemTrack.cpp | 2,784 | ui/UIList.cpp |
| bandobj/BandWardrobe.cpp | 2,132 | rndobj/Console.cpp → world/Crowd.cpp |
| bandtrack/Tail.cpp | 1,064 | rndobj/Font.cpp, bandtrack/GemSmasher.cpp |
| bandobj/SongSectionController.cpp | 576 | char/CharIKFingers.cpp |

So the true not-compiled set was **88 files / 68,964 B**. The §4.3 split
(111,824 B native-compiled) is low by those 28,376 B.

## 2. What this lane linked: 83 files, into rb3-render

All 83 go into **rb3-render only** (`W16SH_LINK_SOURCES` in `native/CMakeLists.txt`),
next to W16-PX's `VOCALTRACK_GAME_SOURCES`; rb3-render is the one target that already
carries the gameplay layer these files sit on. They hold **66,932 B / 194 gap rows**.

Method: `clang++ -fsyntax-only` with rb3-render's exact flags over all 88 (77 clean,
11 failed), then link, read the undefined/duplicate set, fix, repeat. Link rounds:

| round | change | undefined | duplicate |
|---|---|---:|---:|
| 1 | the 77 syntax-clean files | 65 | 156 |
| 2 | drop 4 Quazal/XDK files, add 6 deps, guard 2 scatter edges, retire stubs | 17 | 0 |
| 3 | DataResults + JsonUtils + NetMessage + MicClientMapper, NetSession stubs | 25 | 0 |
| 4 | json-c, Quazal::String subset, RockCentral stubs | **0** | **0** |
| 5 | the 9 fixed non-compiling files | 0 | 173 |
| 6 | 2 more scatter edges, 1 duplicate member pair, GamePanel stubs | **0** | **0** |

The closure outside the 96 (`W16SH_DEP_SOURCES`, 17 TUs): BandProfile, meta/Profile,
BandMachineMgr, synth/Faders, TourBand, SavedSetlist, net/JsonUtils, net/NetMessage,
synth/MicClientMapper, the six json-c `.c` files, dsp/IIRFilter, synth/Common_Xbox
(portable despite its name: `MemAlloc` + `XMemSet`, which `xdk_shims.cpp` supplies).

### 2.1 Source edits (all native-only or token-identical on X360)

- **Scatter edges skipped natively** (`#if !HX_NATIVE`, the house pattern): PatchDir →
  BandCamShot and OvershellPanel → BandWardrobe (rb3-render already emits both),
  ViewSetting → CharIKScale (compiled standalone), TrainerPanel → gesture/SpeechMgr
  (Kinect speech, 360-only; TrainerPanel calls none of it).
- **Duplicate definitions:** `sNullMicClientID` gets internal linkage natively in
  OvershellSlot, OvershellPanel and MicInputArrow (W16-PX's pattern). MusicLibrary's
  copy of `SavedSetlist::SetTitle/SetDescription` is skipped natively; retail emits them
  in MusicLibrary and SongSortMgr, and SongSortMgr's copy is the one compiled.
- **Did not compile natively:**
  - InterstitialMgr.h, TourWeightManager.h: `hash<Symbol>` gets the `std::` spelling
    under HX_NATIVE, which NextSongPanel.h already uses. That was all of
    InterstitialMgr's 75 and Tour's 255 errors.
  - SaveLoadManager, PitchDetector: the 2-arg `(_MemAllocTemp)/(MemAlloc)(size, align)`
    is X360-only (utl/MemMgr.h). A file-local macro passes the 5-arg form natively and
    expands to the old tokens on X360.
  - SongSort: STLport's vector iterator is a pointer, libstdc++'s is not; the insert
    position is rebuilt natively, old tokens on X360.
  - GamePanel: a native-only `GAME_DBG` log spelled `TheUI.` on a pointer.
  - ViewSetting, MusicLibrary: they call `UILabel::SetDisplayText`, which UILabel.h
    declares protected. Retail's cl accepts the calls (both TUs build in the match build).
    They compile with `-fno-access-control` as a per-TU option. Editing UILabel.h
    instead would touch every UI TU.
- **json-c `config.h`:** HAVE_STRNCASECMP / HAVE_VASPRINTF are 1 under HX_NATIVE. The X360
  mapping to LIBCMT `strnicmp` is MSVC-only.

### 2.2 Stubs: what was retired, what stays

**Retired from `bandtrack_link_stubs.cpp`, replaced by the real TU:**
- ProfileMgr: 10 stubs, the 2 bodies W16-PX had copied verbatim, and the zero-filled
  `TheProfileMgr`.
- MetaPerformer: 3 stubs.
- BandUserMgr: 2 stubs and `TheBandUserMgr`.
- BandProfile: 4 stubs.
- SongStatusMgr: 1 stub.
- GamePanel: 1 stub and `TheGamePanel`.

`TheProfileMgr` is now the real object, built by its real constructor at static init.

**Added, every one aborts with its own name if reached:**
- RockCentral's six called methods. RockCentral.cpp is the Quazal RB* client; see §4.
- NetSession `IsJoining` and three `SendMsg*` (SessionMgr's sends). TheNetSession is
  null here.
- `PlatformMgr::GetOnlineID` (the body is in PlatformMgr_Xbox.cpp).
- Zero-filled `TheNet` and `TheRockCentral`. Both are held by value. Net.cpp is the
  Quazal session layer and does not compile natively.

The file now holds 19 aborting stubs and 7 singletons, against 27 stubs, 2 copied
bodies and 8 singletons before.

**Native bodies with no portable TU** (`native/src/w16sh_link_support.cpp`):
- The five `Quazal::String` / `RootObject` members DataResultList needs: ctor, dtor,
  `operator=(const wchar_t*)`, new, delete. They are ported from
  network/quazal/Core/String.cpp, keeping its null-source rule. That file is /Od NetZ
  code with its own ILP32 CRT prototypes and cannot compile natively.
- `__vmaddfp`, `__vspltw` and `__vperm`, which SndAnalysis's VMX fast path uses.
  `vperm` is emulated on the big-endian byte image, so retail's selector table means the
  same thing here.

## 3. What now runs: 17 gates (rb3-render, `w16sh_phase.cpp`)

Linking alone runs only static initializers, and under `--gc-sections` the rest is
dropped: `nm` found no `EQEffect::`, `PitchDetector::` or `DelayEffect::` symbol in the
linked rb3-render before the phase existed. The phase runs after W16-PX's bandtrack
phase in the default mode, which is what `native_health.sh` runs. `--no-w16sh` opts
out. Each gate has an oracle outside the code under test:

| gate | oracle | gap rows it executes |
|---|---|---|
| dsp-pitch-110/220/440hz | 69 + 12·log2(f/440), ±0.25 semitone, on a synthesized sine | PitchDetector::AnalyzeBlock 1,780; FindCCPeak 920; ShiftedDotProduct 356; RefinePeriod2 352 |
| dsp-pitch-silence | silence is gated: pitch 0, level 0 | (same) |
| fx-eq-identity | all bands disabled ⇒ output == input | EQEffect::Process 476 |
| fx-eq-treble-shelf / -bass-shelf | a first-order shelf has unity at one end, 10^(dB/20) at the other | EQEffect::SetParameter 1,644; Process |
| fx-eq-lowpass / -highpass | RBJ biquad: unity in the passband, the double zero at the other end | (same) |
| fx-delay-echo | impulse returns after delay·48000 samples scaled by 10^(dB/20), then squared; zero elsewhere | DelayEffect::Process 384 |
| fx-distort-identity / -curve | drive 0 is the identity; any drive keeps ±1, is odd, monotone and adds gain | DistortionEffect::Process 168 |
| fx-flanger-delay | depth 0 + no feedback is a pure delay of delayMs·48 samples | FlangerEffect::Process 768 |
| pm-lag-table | the static-init table equals GetJoypadExtraLagInits on all 329 cells | ProfileMgr::GetJoypadExtraLagInits 448 (through the ctor) |
| pm-lag-retail | retail 0x82545AC8's decoded cells, incl. the 74.0 at `.rdata` 0x82091FC0 (W16-PW) | (same) |
| pm-latency | sync offset = −video latency; song-to-taskmgr = video − audio; 3 mic volumes | ProfileMgr::ProfileMgr |
| tracker-multiplier-map | a DTA threshold table read back by threshold | TrackerMultiplierMap::InitFromDataArray 268 |

**Prediction against measurement.** I wrote the tolerances from the oracles before the
first run. All 17 passed on the first run, so the gates had not yet been shown to fail.
I ran two sabotage controls, and both are reverted:
- Byte-swapping the `vperm` selector (a little-endian emulation) failed all three
  pitch gates (pitch 0.000). This also shows the VMX path really runs.
- Flipping the treble shelf's sign in `EQEffect::Process` failed `fx-eq-treble-shelf`
  (DC 1.9953, Nyquist 1.0000) and nothing else.

**Finding (comments only, no behaviour change):** EQEffect.cpp labels band 0 "Low
shelf" and band 2 "High shelf". By the transfer function, and by the gates, it is the
other way round. Band 0 is `x + k·(x − allpass)`: 0 at DC and 2x at Nyquist, so its
boost is at the top, which fits its 12 kHz default corner. Band 2 is `x + k·(x + allpass)`,
so its boost is at the bottom. The code is right; the comments are swapped.

**About 7,564 B of in-scope gap rows now execute under a gate.** The rest of the 83
files are linked but not driven. Their code is compiled and its symbols resolve, so
the ODR, undefined-symbol and compile classes are covered, but `--gc-sections` drops
whatever nothing reaches.
`tools/native_runtime_rank.py` was not rerun; it needs a separate instrumented build of
all 18 targets.

## 4. Not linked: 5 files / 2,032 B / 10 rows, with reasons

| file | gap B | reason |
|---|---:|---|
| net_band/RockCentral.cpp | 664 | the Quazal RB* data-service client: needs Quazal::RBDataClient, RBBinaryDataClient, ServiceClient, Protocol, Buffer and XNet (round-1 link: 30 Quazal/XNet undefineds from this TU alone). Quazal is out of the native port's scope. Its six methods the linked code calls are loud stubs. |
| net_band/XboxEntityUploader.cpp | 436 | XDK `XStringVerify`, the Xbox Live string-verification service. Platform-only. |
| net_band/ContextWrapper.cpp | 220 | wraps `Quazal::ProtocolCallContext`; it exists to carry RockCentral calls. |
| os/AsyncFile_Win.cpp | 460 | the Win32 platform backend (`<io.h>`); native excludes `_(Xbox\|Win\|Wii\|X360).cpp` by rule and supplies its own file backend |
| os/ThreadCall_Win.cpp | 252 | the Win32 platform backend (`<process.h>`); same rule |

## 5. X360 build: unchanged, measured

Prediction: Δ0 on every key. Every `src/` edit in §2.1 is either `HX_NATIVE`-only
or expands to the same tokens on X360.

I measured the `src/` part of the branch (`git diff main -- src/`, 14 files, byte-identical to the patch that was measured) with
`tools/ab_measure.py --patch ~/tmp/w16sh/src.patch --jobs 8` on a fresh worktree
(`~/tmp/wt-w16sh-ab`), using the shipped `name_check` ruler:

```
Δmatched=+0 Δmasked_equal=+0 Δhonest=+0 Δcode%=+0.000000pp Δcode_bytes=+0 Δfuzzy=+0
leg A = leg B: matched 54,755, code% 59.016273; units at 100% 599 -> 599
leg B recompiles: 66 (so the patch reached the compiler; this is not absent-vs-absent)
```

The prediction held. The `native/` and `config/` changes are not inputs to the
X360 build. The one config edit is the `layout_odr_allow.json` entry in §6, which
only `tools/layout_odr.py` reads.

### 5.1 Linked translation units, compiler truth (`tools/native_linked_tus.py`)

The UNION across the 18 targets goes from **520 on main to 612 on the branch**, and
the tool's self-validation passed on both. That is +93 TUs (the 83 files in §2
plus 10 deps) and −1.

The −1 is `src/system/os/MasterAudio.cpp`. It is a measurement artifact, not a
regression: main's working tree has a **0-byte, gitignored stray file** at that
path (`.gitignore:118`, mtime Jul 2). Main's native configure globs it into a
compile (it appears in main's `native/build/CMakeFiles/VerifyGlobs.cmake`). A
worktree has no copy of an ignored file, so the branch cannot see it. The real
`beatmatch/MasterAudio.cpp` is linked on both sides.

## 6. Gates

### 6.1 One layout-ODR finding, accepted as reviewed

The first `tools/native_health.sh` run failed the link leg. The build itself was
clean (rc=0, 0 linker diagnostics, 18/18 targets good), but `layout_odr` reported
one native SPLIT: `json_object`, with layout `34f3a714…` from
`json-c/json_object.c` and `fe4a1040…` from `net/JsonUtils.cpp`.
`tools/layout_odr.py show --domain native json_object` shows the two layouts
differ in exactly one row: the nested union's printed type name, `union data`
from C (C has no nested scopes) versus `union json_object::data` from C++. Every
member and offset is the same.

Both sides compile `json_object_private.h`, so this is one struct seen from two
languages, not an ODR defect. I recorded it as a PIN entry in
`config/45410914/layout_odr_allow.json`. A PIN accepts exactly these two
fingerprints, so any real change to either layout fires again. I did not change
the tool.

### 6.2 Scatter audit

`multi_host` went from 17 on main to 15. The two guests that dropped out,
`bandobj/BandCamShot.cpp` and `bandobj/BandWardrobe.cpp`, are exactly the
duplicate scatter edges §2.1 guards with `#if !HX_NATIVE`
(PatchDir→BandCamShot and OvershellPanel→BandWardrobe). No new multi-host guest
appeared, and `direction_b` stayed at 0. `scatter_unlinked=16` is the recorded
steady-state value.

### 6.3 Results

`tools/native_health.sh`, worktree `~/tmp/wt-w16sh`, after §6.1:

```
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=94 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=16 scatter_dirb=0 scatter_multihost=15 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
```

All 17 §3 gates PASS in health's own rb3-render run (57 rb3-render gates, all
passing). `selftest=SKIPPED` is the default; `--selftest` is opt-in. The negative
controls for the new gates are the two sabotage runs in §3.

The `tools/native_build_gate.sh` line for the final committed tree is in the
lane report. That gate ran last, after this doc was committed.
