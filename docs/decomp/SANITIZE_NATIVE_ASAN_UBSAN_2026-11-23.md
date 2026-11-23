# Lane sanitize — the headless native targets under ASan + UBSan (2026-11-23)

Branch `sanitize`, worktree `~/tmp/wt-sanitize`. Measured on main `98f66d185`; rebased onto main `c99af95d2` (rbx-uaf-a20) before reporting, rerun in §7.
Build: `native/build-ubsan`, configured `-DRB3X_SANITIZE=address,undefined` (new option
value, `native/CMakeLists.txt`): W16-UO's ASan flags plus `-fsanitize=undefined
-fsanitize-recover=undefined`. That is the default UBSan group, `vptr` and `function` included
(RTTI is on). Each target ran once per pass through
`/home/free/tmp/claude-1000/rb3-scout/coord/no610m.sh`, with `native_health.sh`'s argv and data.
`ASAN_OPTIONS` matched `tools/native_asan.sh` (`halt_on_error=0:suppress_equal_pcs=0:detect_leaks=0`
plus the mismatch checks) and `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0`. UBSan prints
each source location once per process. The runner script is in scratch
(`~/tmp/sanitize/run_all.sh`), not committed.

## 0. Headline

- **ASan: 0 reports** on every target, in every pass: rb3-milo, rb3-save, rb3-song, rb3-crowd,
  rb3-score, rb3-score2..4, rb3-vocal, rb3-vocal2, rb3-harmony, rb3-app construct-only, and
  rb3-app's frame loop (`RB3_APP_RUN=1`, 300 frames). The four items another session owns
  (`~RndTex` share table, `CharClipDriver::DeleteRef`, `UIScreen::UnloadPanels`,
  `sCubeTexGpuData`) did not report here. Neither did the `FileGetPath` strcpy-param-overlap
  (lp64 lane): none of these runs reaches cal_welcome, and rb3-app's 300 frames stay on
  splash_screen.
- **UBSan: 26 distinct sites.** Run-1 counts were 2–5 per small target and 31 for rb3-app
  construct-only; the frame loop added 2 more sites (§1).
  - **8 fixed** (§2). After the fixes the small targets carry only the GameGem copy (or none),
    and rb3-app 14 retail-faithful or benign reports (§3).
  - **1 fix backed out after measurement.** The FileMerger `mForceReload` init changed rb3-app
    behaviour; the bisect is in §4.
- **rb3-app runs without a GPU:** with `VK_ICD_FILENAMES=/nonexistent`, Dawn falls back to the
  Null backend and the run reaches `RESULT: APP CONSTRUCTED` (and `APP RAN 300 FRAMES`). It is
  in scope.
- **Every target exited rc 0 and printed its completion line in every pass.** Leaks were not
  looked at (`detect_leaks=0`, as in `native_asan.sh`).
- **X360:** see §5 for the ab_measure Δ.

## 1. Every report and its verdict

"Targets" is where run 1 hit the site. Retail addresses are band.exe VAs, read with
`tools/va_disasm.py`.

| # | site | kind | targets | verdict |
|---|---|---|---|---|
| 1 | `math/Rand.cpp:16,18` `Rand::Seed` | signed overflow (`s * 0x41C64E6D`) | all 12 (static init of `Rand::sRand`) | **fixed**: `unsigned int s` (§2.1) |
| 2 | `band3/game/TrainerGemTab.cpp:30` ctor | index 5 out of bounds, `RndMesh *[5]` | rb3-app | **fixed**: header bug, `mTails[6]` (§2.2) |
| 3 | `net/XLSPConnection.cpp:64,70,71,107,109,132`, `.h:16` | invalid `State` load (4, −1) | rb3-app | **fixed**: the enum was empty (§2.3) |
| 4 | `beatmatch/TrackWatcherImpl.cpp:524` `CheckForCodaLanes` | invalid bool (190 = ASan fill 0xBE: unset) | rb3-score | **fixed** natively (§2.4) |
| 5 | `ui/UIList.cpp:931` `SetProvider` | invalid bool (0xBE) | rb3-app | **fixed** natively (§2.5) |
| 6 | `synth/SampleData.cpp:52` via `SynthSample::Init` | call through wrong function type: 4-arg `MemFree` as `void(*)(void*)` | rb3-app | **fixed** natively (§2.6) |
| 7 | `native/src/xdk_shims.cpp:345` → `MyThreadFunc` | call through wrong function type: `void*` routine as `DWORD(*)(void*)` | rb3-app | **fixed** (§2.6) |
| 8 | `char/CharClip.cpp:358` `Transitions::Load` | null passed to `memcpy` (0 bytes) | rb3-app, rb3-milo | **fixed** natively (§2.7) |
| 9 | `synth/ByteGrinder.cpp:1001,1002` `magicNumberGeneratorNative` | signed overflow (LCG) | rb3-app frame loop | **fixed**: unsigned (§2.1) |
| 10 | `char/FileMerger.cpp:488` `NeedsLoad` | invalid bool (0xBE), `Merger::mForceReload` | rb3-app | **retail-faithful, open** — fix backed out (§4) |
| 11 | `beatmatch/GameGem.h:84,85` via `GameGem::operator=` | invalid `RGNoteType` (garbage words) | crowd, harmony, score, score3, score4, vocal, vocal2 | **retail-faithful, not fixed** (§3.1) |
| 12 | `network/net/MatchmakingSettings.cpp:45` `Save` | invalid bool, `mRanked` | rb3-app | **retail-faithful, not fixed** (§3.2) |
| 13 | `char/CharClip.cpp:173` `Transitions::Resize(0, nullptr)` | non-zero offset applied to null | rb3-milo | **benign, not fixed** (§3.3) |
| 14 | `utl/BinStream.cpp:33`, `bandobj/BandFaceDeform.cpp:185–189`, `.h:15,16` | misaligned `short`/`Delta` access | rb3-app | **benign on x86, not fixed** (§3.4) |
| 15 | `utl/ChunkStream.cpp:501` `DecompressChunk` | misaligned `unsigned int` load | rb3-app | **benign on x86, not fixed** (§3.4) |
| 16 | engine `gfx/mikktspace.c:1667` `QuickSortEdges` | shift exponent 32 | rb3-app | **engine, recorded, not fixed** (§3.5) |
| 17 | engine `platform/Keygen_Stub.cpp:63` `random` | signed overflow (LCG) | rb3-app frame loop | **engine, recorded, not fixed** (§3.5) |

(Sites 3, 11 and 14 each span several lines; counted line by line, the table holds the
26 distinct sites of the headline, plus the 2 frame-loop sites, rows 9 and 17.)

## 2. Fixes

### 2.1 Signed-overflow LCGs (rows 1, 9)
- `Rand::Seed`: `s = s * 0x41C64E6D + 0x3039` on an `int` overflows from the first step (seed
  0x29A). Retail's `mullw` wraps; C++ signed overflow is undefined. `s` is now `unsigned int`.
  The bits are identical (the `>> 16` was already on an unsigned cast), and MSVC emits the same
  `mullw`/`addi` either way (§5).
- `magicNumberGeneratorNative` (ByteGrinder, under `HX_NATIVE`): the same fix.

### 2.2 `TrainerGemTab::mTails` is six entries (row 2)
- Retail ctor `0x826EFC38`: `li r10, 6` / `addi r9, r3, 0x90` / the `stwu` loop at `0x826EFCA0`
  zeroes six words from 0x90. The next store is `mTrackGroup` at 0xA8.
- `Init` stores `mTails[5] = gem_sustain_purple.mesh` (real guitar's sixth string).
- The header had `mTails[5]` + `int unk90` at 0xA4. Nothing names `unk90`, and every
  `mTails[5]` access wrote it through an out-of-range index. Now `RndMesh *mTails[6]`;
  layout-neutral.

### 2.3 `XLSPConnection::State` was an empty enum (row 3)
- Every value but 0 was outside the enum's range: the ctor's −1 sentinel and states 1–5.
- Now names −1..5 from the state machine's transitions: destroyed, idle, enumerating,
  gateway-connecting, connected, failed, disconnecting. No retail symbol names them.
- Uses still cast `(State)N`.

### 2.4 `TrackWatcherImpl::mButtonMashingMode` (row 4)
- Retail ctor `0x82797298` stores 0x68 (`mTrack`) and 0x70 (`mLastGemPassed`) but never 0x6C.
  So retail's first `CheckForCodaLanes` compares against heap garbage too.
- If the garbage is non-zero, that first poll calls `BeatMatcher::SetButtonMashingMode(false)`
  once. That call only rewrites TrackData's already-false flag and stamps `mLastMashTime`.
- Natively it is an invalid-bool load whose low bit steers control flow. The native ctor arm now
  starts it false, the value a zeroed heap gives. The X360 path is unchanged.

### 2.5 `UIList::mLimitCircularDisplayNumToDataNum` (row 5)
- Retail's ctor `0x827FB7C8` ends its stores at `stb r29, 0x239` (`0x827FBA70`), so 0x23A is
  unset there too.
- Retail never reads it unset. The reads are the native-only `LimitCircularDisplay` calls in
  `SetProvider` and `Refresh`, a DC3-era addition.
- So this is a native-only bug: an odd heap byte would cap a circular list's display count.
  Initialized in the native ctor arm.

### 2.6 Calls through the wrong function type (rows 6, 7)
- `SynthSample::Init` passed `(SampleDataFreeFunc)MemFree`. Natively that is the 4-arg debug
  `MemFree(mem, file, line, name)`, called as `void(*)(void*)`, so `file/line/name` were
  garbage registers.
  - Harmless today: native `MemFree` is `free(mem)`.
  - Natively it now registers a one-argument wrapper. The X360 line is unchanged; retail's
    MemFree takes only the pointer.
- `native/src/platform/ThreadCall_Native.cpp` `MyThreadFunc` returned `void*` and was cast to
  `DWORD(*)(LPVOID)` for `CreateThread`. It now returns `DWORD`, as the retail TU's
  `uint MyThreadFunc(void *)` (`os/ThreadCall_Win.cpp:17`) does, and the cast is gone.

### 2.7 `CharClip::Transitions::Load` memcpy to null (row 8)
- For a clip with no transitions, `Resize(0, nullptr)` frees and nulls `mNodeStart`, then
  `memcpy(mNodeStart, start, 0)` runs.
- That is UB (the pointer arguments are `nonnull`), and an optimizing build may use it to drop
  later null checks. The memcpy now sits behind a native-only `if (mNodeStart)`.

## 3. Not fixed, and why

### 3.1 `GameGem` note types (row 11)
- Retail `GameGem(const MultiGemInfo &)` (`0x8278EDA8`, read in full) stores 0x0–0x12, 0x3C
  and 0x40. It never stores `mRGNoteTypes` (0x14–0x2B), `mStrumType`, `mFrets` or `mRGChordID`.
  So every non-real-guitar gem carries unset note types on the 360 too, and our `operator=`
  copies them. Retail's `operator=` (`0x8278E758`) was not read here.
- Nothing reads them for those gems: `GetRGNoteType` is real-guitar only.
- Clang does not assume enum ranges without `-fstrict-enums`, so copying the words changes
  nothing.

### 3.2 `MatchmakingSettings::mRanked` (row 12)
- Retail's inlined ctor (inside `NetSearchResult::NetSearchResult`, `0x823F5AD8..0x823F5B2C`)
  stores the vtable, the Symbol and the two vectors, not 0x8 (`mModeFilter`) or 0xC
  (`mRanked`). So these fields are unset on retail too.
- Offline, the value is only round-tripped through a MemStream into the search result, and it
  is src/network (360-ONLY tier).

### 3.3 `Transitions::Resize(0, nullptr)` (row 13)
- `n = old - mNodeStart` with `old == nullptr`, and the returned `mNodeStart + n` is computed
  after `mNodeStart` has been freed and nulled.
- `Clear` discards the return. The X360 arm of the source computes the same value; retail's
  `Resize` was not read here.

### 3.4 Misaligned accesses (rows 14, 15)
- `BandFaceDeform::DeltaArray::Load` byte-swaps packed shorts in place, and ChunkStream reads a
  `uint` from a packed header.
- x86 and Xenon both perform these loads; no behaviour depends on it.

### 3.5 Engine sites (rows 16, 17)
- Both sit in `../milo-native-engine`, the live working tree every consumer builds from.
  Neither was edited from this lane.
- **mikktspace** `t=(uSeed<<t)|(uSeed>>(32-t))` with `t == 0` shifts by 32.
  - On x86 the shift count is masked, so it yields `uSeed`, which is what a rotate by 0 gives.
  - Fix when someone owns the engine: `t ? rot : uSeed`.
- **Keygen_Stub** `random`: `s_seed * 0x19660E + 0x3C6EF35F` on `int`. The comment there wants
  32-bit wrap, which needs `unsigned int`.

## 4. The FileMerger fix, measured and backed out (row 10)

- **Retail.** Retail's `Merger(Hmx::Object *)` (`0x8232DCF8`) stores `mProxy` (0x28) and
  `mPreClear` (0x2A) but not `mForceReload` (0x29), and `operator=` (`0x8232DE80`) does not copy
  it. So `NeedsLoad` (`FileMerger.cpp:488`) reads it unset until `SetSelected`.
- **The fix tried.** Both native ctors started it false.
- **What it changed.** rb3-app's frame loop (`RB3_APP_RUN=1`, 300 frames, Null GPU) behaved
  differently:

  | binary | runs | `VorbisReader::CheckHmxHeader` reached (Keygen/ByteGrinder sites fire) |
  |---|---:|---|
  | before the fixes | 3 | 3/3, by frame ~25, under `StandardStream::PollStream` ← `Synth::Poll` |
  | all fixes | 3 | 0/3 |
  | all fixes minus synth/thread/Rand/CharClip | 1 | 0/1 |
  | all fixes minus FileMerger.h and UIList.cpp | 1 | 1/1 |
  | all fixes minus FileMerger.h | 1 | 1/1 |

- **Reading.** An unset `mForceReload` makes some merger reload, and that reload is followed by
  a splash-screen stream starting. With the byte forced false, the stream never starts in 300
  frames.
- **Unknown.** Which value the 360 sees (and so which behaviour is retail's) is not settled
  here. Leads: which merger reloads, and what the splash stream is.
- **Decision.** The hunk is backed out (commit `aa753bc48`) rather than land a behaviour change
  whose direction is unknown.

## 5. X360 Δ

`tools/ab_measure.py --patch` of the net `src/` change, measured as its REVERSE (`git diff HEAD
4b6935720 -- src`, 8 files; FileMerger.h is not in it, being backed out) on main `98f66d185` + this
branch. Ruler `functionRelocDiffs=name_check`, objdiff `sha256:ec3badc0f33323e9`. Leg B
recompiled 42 MSVC objs, so the edits reached X360 TUs.

- Δmatched **+0**, Δmasked_equal +0, Δhonest +0, Δcode% +0.000000 pp, Δcode_bytes +0,
  **Δfuzzy +0.000000 pp**. Units at 100%: 635 → 635 (mpn), 548 → 548 (fuzzy).
- Leg A: matched 55,268, honest 29,908, code 59.665310%. Since the patch is the reverse, the
  fixes' own Δ is the negation: 0 on every key.
- Run dir: `~/tmp/wt-sanitize/.ab_measure_runs/20261123-000752-sanitize-reverse-680976/`.
- The `Rand::Seed` int→unsigned and the two header changes (`mTails[6]`, the State
  enumerators) are therefore codegen-neutral. Every other `src/` edit is inside `HX_NATIVE`.

## 6. What this lane did not do

- No leak triage (`detect_leaks=0`, per brief). No `native_health.sh --selftest`, per brief.
- No edit to `../milo-native-engine` (§3.5).
- `tools/native_asan.sh` is not extended to UBSan: its parser counts only AddressSanitizer
  reports. The CMake option is the reusable part.
- rb3-app's frame loop was run to 300 frames only. It stays on splash_screen, so the
  cal_welcome path (the UnloadPanels UAF another session owns) was not reached.
- No new native/src file, so `tools/native_stub_census.py` needed no row.

## 7. Rerun after the rebase onto `c99af95d2`

Same build dir (rebuilt), same runner, same options:

- **ASan: 0 reports** on all 12 targets and on rb3-app's 300-frame loop.
- Every target rc 0 with its completion line.
- UBSan counts as before the rebase:
  - 2 per small target that loads charts (row 11), 0 for rb3-song, rb3-score2 and rb3-save;
  - rb3-milo 2;
  - rb3-app construct-only 15, frame loop 17 (rows 10, 12–17).
- The frame loop still reaches `VorbisReader::CheckHmxHeader` (the Keygen site fires), so the
  splash stream starts as before this lane.
