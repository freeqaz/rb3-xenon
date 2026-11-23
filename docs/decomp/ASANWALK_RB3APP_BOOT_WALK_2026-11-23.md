# Lane asanwalk — rb3-app's boot walk under ASan + UBSan (2026-11-23)

Branch `asanwalk`, worktree `~/tmp/wt-asanwalk`, base `ca6000f36` (A26's tip: the owner-pointer
Replace retargets). Scratch: `/home/free/tmp/claude-1000/rb3-scout/asanwalk/`.

## 0. Headline

- The walk (`rb3-scout/a21/walk.txt`: splash, first-time calibration, hints, main hub, song select,
  difficulty, song load) runs to game_screen's Enter under ASan + UBSan, and ends there at the
  known BandDirector::Enter `motion_blur` SEGV. Two runs (w1 before the fixes, w2 after) visited
  the same screen sequence (md5 of the `screen=/transition=` trace identical).
- **ASan: one report, the known SEGV.** No heap, stack or use-after-free report before it.
- **UBSan: 46 distinct sites in w1** (counted line:column). 22 belong to classes the
  sanitize lane already triaged (`SANITIZE_NATIVE_ASAN_UBSAN_2026-11-23.md`) or to the known list.
  The other 24 are new; by cause they are 13 findings (§1).
  - **3 fixed** (10 sites): JoypadPollCommon signed overflow, Hmx::CRC::ComputeHash signed
    overflow, empty PassiveMessageType enum. w2 shows exactly those 10 sites gone and none added.
  - **2 real native bugs recorded, not fixed** (out-of-bounds areas): RndText's uninitialised
    native-only `mManualLines`/`mFramesSinceDraw` (Rnd/Text), and CharBonesSamples::Clone's
    memcpy from null (src/system/char).
  - The rest are retail-faithful or benign, with retail evidence below.
- X360 A/B of the fix commit: §4.

## Method

- Build: `native/build-asan` in the worktree, `-DRB3X_SANITIZE=address,undefined` (the option the
  sanitize lane added: ASan + the default UBSan group, both recover mode, `-g`, no `-O`), engine
  `rb3-scout/eng-6b34583` (= `MILO_ENGINE_PIN`, = live engine HEAD today). Script
  `asanwalk/build_asan.sh`. Frozen binaries `asanwalk/bin1` (base) and `asanwalk/bin2` (fixes).
- Run: `asanwalk/run.sh` = a copy of `a17/run.sh` with `X=~/tmp/wt-asanwalk`, `ulimit -d unlimited`,
  `ASAN_OPTIONS=halt_on_error=0:suppress_equal_pcs=0:detect_leaks=0:alloc_dealloc_mismatch=1:new_delete_type_mismatch=1:symbolize=1`,
  `UBSAN_OPTIONS=print_stacktrace=1:halt_on_error=0`, through `coord/no610m.sh` under the gpu1 lock.
  `SECS=400 TMO=440`, 3000 frames. Logs: `asanwalk/r_w1/run.log`, `asanwalk/r_w2/run.log`.
- Both runs: game_screen transition at frame 1043 (w1, 61.1 s) / 1034 (w2, 57.2 s), then rc 1
  (ASan abort on the SEGV).
- Retail bytes: `tools/va_disasm.py` on `orig/45410914/band.exe` (clean TU5).

## 1. Every report and its verdict

### 1.1 Known (listed only)

| site | in this walk? |
|---|---|
| BandDirector::Enter `motion_blur` SEGV at game_screen (`DataNode::Evaluate`, with UBSan `Data.h:515` null+16 one line before it) | **yes**, both runs: the walk's end |
| TypeProps::Replace, CharBonesMeshes::Replace | no report |
| FileGetPath strcpy overlap (File.cpp:163) | no report |
| `gfx/mikktspace.c:1667` shift 32 | yes (engine) |
| `platform/Keygen_Stub.cpp:63` signed overflow (2 columns) | yes (engine) |
| NetSession::mRanked uninitialised | no NetSession site; see the MatchmakingSettings rows below |
| RndCubeTex's address-keyed table | no report |

### 1.2 Already triaged by lane sanitize (same site, same verdict; not re-litigated)

`ChunkStream.cpp:501` (misaligned), `BandFaceDeform.cpp:185-189` + `.h:15,16` + `BinStream.cpp:33`
(misaligned, 11 sites), `FileMerger.cpp:488` (mForceReload, open), `MatchmakingSettings.cpp:45`
(mRanked), `CharClip.cpp:173` (Resize(0,null)), `GameGem.h:84,85` (note types).

### 1.3 New

| # | site | kind | verdict |
|---|---|---|---|
| N1 | `os/Joypad.cpp:770,915` JoypadPollCommon | signed overflow: `SystemMs() - mLastActivityMs`, `SystemMs() + 0x7FFFFFFF` | **fixed** (§2.1) |
| N2 | `utl/Crc.cpp:9,11(x3),12` Hmx::CRC::ComputeHash | signed overflow (FNV multiplies) | **fixed** (§2.2) |
| N3 | `meta_band/PassiveMessenger.cpp:47,290`, `.h:37` | load of 3 as `PassiveMessageType` (empty enum) | **fixed** (§2.3) |
| N4 | `rndobj/Text.cpp:1852` RndText::Draw | invalid bool 0xBE, `mManualLines` | **real native bug, recorded** (§3.1) |
| N5 | `CharBonesSamples.cpp:330` (x2) Clone | null passed to memcpy (0 bytes) | **real UB, recorded** (§3.2) |
| N6 | `beatmatch/SongParser.cpp:234` (x2) OnEndOfTrack | invalid TrackType 0xBEBEBEBE | **retail-faithful, harmless** (§3.3) |
| N7 | `synth/StandardStream.cpp:685` InitInfo | invalid bool 0xBE, `mJumpSamplesInvalid` | **retail-faithful, harmless** (§3.4) |
| N8 | `meta_band/SongRecord.cpp:154` UpdateSharedStatus | invalid bool 64, `mIsShared` | **retail-faithful, harmless** (§3.5) |
| N9 | `net/MatchmakingSettings.cpp:98` SessionSettings::SetRanked | invalid bool 0xBE, `mRanked` | **retail-faithful** (sanitize §3.2's field, a second read site); src/network, not fixed |
| N10 | `utl/BeatMap.cpp:90` Beat(float) | float -4.8e32 to int | **benign, same result as retail** (§3.6) |
| N11 | `bandobj/BandHeadShaper.cpp:317,318(x2),320` AddFrame | misaligned `Delta`/`ushort` | **benign on x86**, the BandFaceDeform packed-delta class (sanitize §3.4) |
| N12 | engine `gfx/mikktspace.c:1460` QuickSort | shift exponent 32 | **engine, benign on x86**; the same rotate as the known :1667 (§3.7) |
| N13 | `obj/Data.h:515` DataArray::Node | null + 16 | part of the known motion_blur SEGV (`SystemConfig("rnd","motion_blur")` is null) |

## 2. Fixes (commit `6f0cce731`)

### 2.1 JoypadPollCommon keepalive (N1)
- Retail `0x82526DBC` `subf r10,r10,r3` then `cmpw cr6,r10,r11` against `gKeepaliveThresholdMs`:
  a wrapping subtract and a signed compare. The re-arm is `addis r11,r3,-0x8000; addi r11,r11,-1`
  (`0x82526DD4`, again at `0x825272E0`), also wrapping. `SystemMs() + 0x7FFFFFFF` is a far-future
  stamp only because it wraps negative, and the next subtract wraps back to a large negative.
- In C++ both are signed overflow. Now `(int)((unsigned)SystemMs() - (unsigned)mLastActivityMs)`
  and `(int)((unsigned)SystemMs() + 0x7FFFFFFFu)`: the same bits, defined.

### 2.2 Hmx::CRC::ComputeHash (N2)
- A DC3-era file (`utl/Crc.cpp`, NonMatching, no `.text` pin). band.exe has no `0x811C9DC5`
  constant anywhere in `.text` (scanned for `lis` 0x811C / 0x811D), so RB3 retail holds no body.
- Native caller: RndTex::SetBitmap names a bitmap by `Hmx::CRC(path)`.
- The multiplies now run unsigned; the `>> 7` and `>> 0x11` stay on `int`, so they remain
  arithmetic. Checked: 200,000 random strings hash identically to the original under `-fwrapv`;
  a control with logical shifts mismatched 100,384 of them, so the check can fail.

### 2.3 PassiveMessageType (N3)
- The enum was empty, so every value but 0 was out of range (the sanitize lane's XLSPConnection
  State case).
- Named 0..4 from HandlePassiveMessage's switch (retail `0x825CD3A8`), after the message each
  case sends: setup_message_text, _icon, _careerstep, _careergoal, _careermulti. The source
  already casts 3 and 4.
- MSVC enums are int-sized either way, and `W4PassiveMessageType` mangles the same.

## 3. Not fixed, and why

### 3.1 RndText native-only flags uninitialised (N4) — Rnd/Text, record only
- Site: `rndobj/Text.cpp:1852`, `if (!mShowing && !mManualLines)` in RndText::Draw.
- Retail: the ctor (`??0RndText@@IAA@XZ`, `0x82456CB0`) ends with `stb r29,0x18c(r30)` at
  `0x82456F3C`; nothing stores 0x18D-0x18F. Text.h says 0x18D-0x18F have no retail access, and
  that `mMeshDirty`/`mManualLines`/`mFramesSinceDraw` are native-only bookkeeping living in that
  padding.
- The native ctor arm (`Text.cpp` ~l.608) sets only `mMeshDirty = false`. So `mManualLines` and
  `mFramesSinceDraw` start as heap garbage.
- Effect: a non-zero `mManualLines` skips Draw's "hidden: mark the meshes for rebuild" path, and
  stops the `> 4` frames-since-draw release at l.375. A garbage `mFramesSinceDraw` can trigger
  that release early. Native-only bug, no retail counterpart.
- Proposed fix: in the same `#ifdef HX_NATIVE` arm, add `mManualLines = false; mFramesSinceDraw = 0;`.
  X360 unaffected.

### 3.2 CharBonesSamples::Clone memcpy(null, null, 0) (N5) — src/system/char, record only
- Site: `char/CharBonesSamples.cpp` Clone, `memcpy(mRawData, samp.mRawData, AllocateSize())`.
  The report prints `CharBonesSamples.cpp:330` because the file's line numbering differs from the
  checked-in text; the call is at l.335.
- Reached from `CharClip::Copy` <- MergeObject <- FileMerger::FinishLoading, for a clip with no
  samples. AllocateSize() is 0, and native MemAlloc(0) gives null.
- Retail `?Clone@CharBonesSamples@@QAAXABV1@@Z` is `0x823DF8C0`. A 0-byte memcpy is harmless on
  the 360, but natively it is UB (`nonnull` args), the same as sanitize row 8's
  CharClip::Transitions::Load.
- Proposed fix: the row-8 shape, a native-only `if (mRawData)` (or `if (AllocateSize())`) around
  the memcpy.

### 3.3 SongParser::OnEndOfTrack reads mTrackType unset (N6)
- Retail OnEndOfTrack (`0x827849E0`) reads it the same way: `lwz r11,0xf8(r28)` /
  `cmpwi 4` / `cmpwi 5` at `0x82784A6C`.
- The only store is in Reset (`0x82788BD8`, `stw r11,0xf8(r27)` at `0x82788F48`). The retail ctor
  (`0x8278A658..0x8278ACB4`) neither stores 0xF8 nor calls Reset, and ReadMidiFile
  (`0x82788888`) does not either. So the first track's end, before any PrepareTrack, reads
  garbage on the 360 too.
- Harmless: the result goes to `SongData::OnEndOfTrack(mTrack = -1, b)`, whose
  `i < mGemDBs.size()` is false for -1 (unsigned compare), so `b` is never used. Not fixed.

### 3.4 StandardStream::InitInfo reads mJumpSamplesInvalid unset (N7)
- Retail InitInfo (`0x827046B8`) reads it, `lbz r11,0x94(r31)` at `0x82704884`, then loads
  0x8C/0x90 for setJumpSamplesFromMs.
- Neither the retail ctor (`0x827052F0`) nor Init (`0x82704C38`) stores 0x94; only SetJump and
  SetJumpSamples do. Init does store 0x84/0x88/0x8C/0x90 (`0x82704E0C..18`), as the native Init
  does (from/to ms and samples all 0).
- So a garbage-true flag calls `setJumpSamplesFromMs(0, 0)`. That gives from = MsToSamp(0),
  to = 0: no jump, the same as the cleared state. Retail-faithful, harmless.

### 3.5 SongRecord::UpdateSharedStatus compares mIsShared unset (N8)
- The retail ctor (`0x825BBCE0`) stores 0x2D (`0x825BBDE0`) and 0x2E (`0x825BBDFC`), then calls
  UpdateSharedStatus (`bl 0x825BA970` at `0x825BBE8C`) without storing 0x2C.
- UpdateSharedStatus always ends with 0x2C equal to the computed value, whatever the compare
  says, and the ctor drops its return. Retail-faithful, harmless.

### 3.6 BeatMap::Beat(float) with tick -4.8e32 (N10)
- Retail `0x827D2B40` converts with `fctiwz` (`0x827D2B80`), which saturates to 0x80000000. x86
  `cvttss2si` gives the same 0x80000000. Then `i2 <= firstTick` clamps i2 to 0 on both.
- The value is a huge-negative sentinel reaching DataEventList::SecOffset from
  MidiParser::OnSecOffsetAll. Same result as retail; not fixed.

### 3.7 mikktspace.c:1460 (N12), engine
- `t = uSeed & 31; t = (uSeed << t) | (uSeed >> (32 - t))`: with t = 0 the right shift is by 32.
- x86 masks the count to 0, so the result is `uSeed | uSeed` = uSeed, which is the value a
  rotate by 0 should give. Benign. The engine fix is the same as the known :1667:
  `t ? rot : uSeed`. No engine edit made.

## 4. X360 A/B

- `tools/ab_measure.py --worktree ~/tmp/wt-asanwalk --revert 6f0cce731` (log
  `asanwalk/ab_measure-fix1.log`, run root `asanwalk/ab-fix1/`). Being a revert, it measures
  removing the fixes, so read every Δ with its sign flipped.
- Expected Δ0. Crc.cpp has no pin. PassiveMessageType only gains enumerators. Joypad's
  unsigned subtract and add are the same subf/cmpw and addis/addi.
- Result: see §6. (The commit hash in §2 is the fix commit; this record is a later commit.)

## 5. Not done

- Not run: native_health --selftest (per brief), native_build_gate, rb3-render.
- No edit to the engine, Rnd/Draw/Text/Spotlight, UnloadPanels, net sync/game-start or
  src/system/char.
- No walk past game_screen: the motion_blur read is known and stops every run there.
- Leaks not looked at (`detect_leaks=0`).

## 6. A/B result

`ab rc=0`. Default ruler `functionRelocDiffs=name_check`, objdiff-cli sha256 `ec3badc0f33323e9`
(the same on both legs). Leg A is with the fixes and leg B is reverted; leg B recompiled 1,285
objects and settled in 2 iterations.

- leg A = leg B: matched 55,268, masked_equal 25,360, honest 29,908, code% 59.665310, fuzzy
  64.709015. Units at 100%: 635 (mpn) and 548 (all-rows-fuzzy).
- **Δmatched +0, Δhonest +0, Δcode_bytes +0, Δfuzzy +0.000000pp.** The `none` control is +0 B.
- Result file: `asanwalk/ab-fix1/20261123-015107-asanwalk-fix1-revert-2652403/result.json`.

## 7. Follow-up at landing (coordinator, 2026-11-23)

The session that owns `rndobj` and `char` asked for the two §3 proposals to land with this branch, and
they are in commit `c81f90a0c`:

- `RndText::RndText` now clears `mManualLines` and `mFramesSinceDraw` beside `mMeshDirty`, in the native
  branch.
- `CharBonesSamples::Clone` gets a native-only `if (mRawData)` before the `memcpy`. The X360 line is
  unchanged.

Neither edit can change X360 code: cl 10224 `/EP` output for both files equals the pre-edit output apart
from blank lines, and no macro in their closure uses `__LINE__`. Because `RndText` is render code, the
landing gate added `coord/render_cmp.sh` (11 rb3-render cells: main twice as the no-change control, the
integration tree once). The counts are in the merge message.

Commit hashes: the rebase onto main `d61653c14` (`--onto ca6000f36`) renamed this branch's commits. `6f0cce731`
in the sections above is now `4564a55d9`.
