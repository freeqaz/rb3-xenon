# W16-TM: the 27 VIA-DC3 files no native target compiled

Lane W16-TM, 2026-10-07. Worktree `~/tmp/wt-w16tm`, branch `w16-tm`, base main `b79d790b8`.
The brief is `CAMPAIGN_STATE_2026-10-07b.md` §6 lever 4: 27 VIA-DC3 files, 16,408 B of gap, that the
native build did not compile. For each file the lane either compiles it and links it into a native
target, or records why native replaces it by rule. It fixes the behaviour bugs this exposes and gates
whatever now runs.

The short version:
- 25 files are now compiled into `rb3-render`'s link.
  - 12 of them run under a gate.
  - 6 have code in the binary but no gate.
  - 7 are discarded whole by `--gc-sections`, because nothing in `rb3-render` calls them (§2, §3).
- `StoreOffer.cpp` was already linked by W16-TJ.
- Two files are replaced by rule:
  - `StoreEnumeration.cpp` (Xbox Live Marketplace backend).
  - The Bink-player half of `Movie.cpp`, which holds all three of its gap rows.
- 16 new runtime gates pass, and each one has been shown to fail under sabotage (§6.1).
- Two native harness bugs were found and fixed:
  - libogg skipped its CRC check under `HX_NATIVE`, so a wrong magic hash still decoded and passed.
  - `rb3-render` never called `Timer::Init`, so every timer read 0 ms.
- Four further native fixes follow retail or DC3 (§4.3–§4.5, §4.2's `MoggClipMap`).
- One X360-visible fix removes a wrong scatter macro (§4.6).
- X360 A/B: Δ0 on every measure, with 16 TUs recompiled in leg B (§7).
- The X360 A/B is in §7.

## 1. The population

The list comes from `~/tmp/w16tm_27.json` (source → bytes, rows, row list). It was re-derived from
`report.json` and the in-scope/VIA-DC3 classification, not transcribed. It has 27 files and
16,408 B, which matches the campaign doc's figure exactly.

At the lane's own measurement, three of the rows had already reached 100 through other lanes:
`StreamReceiver::Poll`, the `MetaMusicLoader` ctor and `StandardStream::PollStream`. That leaves a
live gap of 14,288 B. The table below keeps the campaign doc's 16,408 B list so the two can be
compared.

## 2. What "linked" means here, measured

`rb3-render` links with `--gc-sections`, so a TU in the link inputs can still be discarded whole when
nothing reaches it. For each file, the table counts that TU's own strong text symbols (`nm` type
`T`/`t` on its `.o`) that survive into the final binary. The last column of the table is that count.
The script and its output are at `~/tmp/w16tm_kept.json`, measured on the final binary.

The tiers:
- **RUN**: a W16-TM gate executes the file's code.
- **KEPT**: its code is in the binary but no gate drives it.
- **DISCARDED**: it is compiled and in the link, but nothing in `rb3-render` reaches it, so the linker
  drops it.

## 3. Disposition, all 27 files

| file | gap B | disposition | kept/own |
|---|---:|---|---:|
| synth/StandardStream.cpp | 2,568 | RUN: four mogg gates decode through `StandardStream` (§5). Native `InitInfo` fix (§4.5). | 71/72 |
| synth/VorbisReader.cpp | 2,048 | RUN: mogg gates. Native compile fix (§4.2). | 25/26 |
| synth/MetaMusic.cpp | 1,704 | DISCARDED except 2 symbols. `rb3-render` has no shell-music player, so nothing constructs a `MetaMusic`. One scatter edge guarded (§4.1). | 2/25 |
| movie/Movie.cpp | 1,568 | **Replaced by rule (codec backend).** The gap rows (`Impl::Draw`, `MovieInternalBuffers::New`, `Impl::DiscContentionPublish`) are RB3's Bink player, which sits in the file's `#else // !HX_NATIVE` half. Native compiles the `MovieSys`/`MovieImpl` front end from the same file, which is in the link and discarded. | 0/26 |
| synth/OggMap.cpp | 1,228 | RUN (reader half). `Read` and `GetSongLengthSamples` run under the mogg gates, through `VorbisReader`. The gap rows `OpenMogg`/`SetKey` sit behind `OggMap::Validate`, which needs `OggValidator::Create`; that is declared and has no body in either tree. | 9/15 |
| meta/StoreOffer.cpp | 948 | Already linked by W16-TJ; not this lane's. | 2/40 |
| meta/StoreEnumeration.cpp | 856 | **Replaced by rule (platform backend).** `XboxEnumeration` drives the Xbox Live Marketplace content enumeration. Not compiled; see the comment in `native/CMakeLists.txt`. | — |
| synth/CompressionEffect.cpp | 632 | RUN: three compressor gates. | 5/5 |
| synth/StreamReceiver.cpp | 632 | RUN: `StandardStream::PollStream` calls `StreamReceiver::Poll` for each channel on every poll of the mogg gates. | 11/12 |
| synth/Emitter.cpp | 536 | KEPT (reached through `Synth`'s factory list). No gate. | 35/38 |
| meta/StorePanel.cpp | 528 | DISCARDED: a Marketplace UI panel that no `rb3-render` path opens. One scatter edge guarded. | 0/41 |
| synth/MidiSynth.cpp | 460 | DISCARDED except 2. Two scatter edges and `MemTracker::StopLog` guarded. | 2/10 |
| synth/MidiInstrument.cpp | 408 | KEPT (20/42). One scatter edge guarded. `MidiInstrumentMgr::Poll` double-poll fixed (§4.4). | 20/42 |
| synth/ByteGrinder.cpp | 368 | RUN: the v0E and v10 mogg gates derive their keys through the native `GrindArray`. Three scatter edges guarded. | 143/144 |
| synth/StreamNull.cpp | 304 | RUN: `synth-streamnull`. | 13/13 |
| synth/SampleInst.cpp | 248 | KEPT. Native compile fix (§4.2). | 28/28 |
| synth/tomcrypt/ctr.c | 244 | RUN: SP 800-38A vector, the fast path against the byte path, and the v0E/v10 moggs. | 5/5 |
| flow/FlowIf.cpp | 232 | RUN: `flow-if-operators`. | 17/18 |
| flow/FlowValueCase.cpp | 200 | DISCARDED. Its gap rows are five 40 B unnamed funclets. One Save/Load asymmetry is recorded and left alone (§6.3). | 0/17 |
| meta/HAQManager.cpp | 164 | DISCARDED. Its gap rows are two unnamed rows (68 B, 96 B). | 0/23 |
| flow/FlowSwitchCase.cpp | 148 | RUN: `flow-switchcase-operators` and `flow-switchcase-transition`. | 25/26 |
| meta/Achievements.cpp | 136 | KEPT (2). The gap row `Achievements::Submit` is the X360 `XUserWriteAchievement` half; native has its own logging branch in the same function. Treated as a platform backend. | 2/13 |
| flow/FlowOnStop.cpp | 80 | DISCARDED. Its gap rows are an 8 B unnamed row and a 72 B `RndMesh` template instance. | 0/24 |
| synth/Utl.cpp | 80 | RUN: `synth-utl-transpose` and `synth-utl-tempo-sync`. Its two gap rows are unnamed 40 B funclets. | 7/9 |
| synth/tomcrypt/aes.c | 68 | RUN: FIPS-197 vector and every mogg. | 5/5 |
| synth/FxSendPitchShift.cpp | 12 | KEPT. Its gap row is a 12 B unnamed stub. | 7/8 |
| meta/DeJitterPanel.cpp | 8 | DISCARDED. Its gap row is an 8 B unnamed stub. | 0/13 |

Tier counts:
- 12 RUN: StandardStream, VorbisReader, OggMap's reader half, CompressionEffect, StreamReceiver,
  ByteGrinder, StreamNull, ctr, aes, FlowIf, FlowSwitchCase, Utl.
- 6 KEPT without a gate: Emitter, MidiInstrument, SampleInst, FxSendPitchShift, Achievements (2), and
  StoreOffer, which belongs to TJ.
- 7 DISCARDED: MetaMusic, MidiSynth (2 kept), StorePanel, FlowValueCase, FlowOnStop, HAQManager,
  DeJitterPanel.
- 2 replaced by rule: Movie's Bink half and StoreEnumeration.

A DISCARDED file needs a caller in a native target before it can be observed. Each one's row above
says what is missing: a shell-music player, a UI panel flow, or a Flow graph that loads those node
types. This lane did not build those callers.

`FlowSwitchCase` and `StreamNull` were DISCARDED in the lane's first pass (0/26, 0/13). The two gates
in §5 moved them to RUN.

## 4. Source edits

### 4.1 Scatter edges native skips (commit `f3dc14561`)

Each edge is a host that re-emits a guest which the native link already compiles. Once the hosts
were linked, these edges caused 240 duplicate definitions.

Each is guarded with the house pattern `#if !HX_NATIVE  // native: skip X360 scatter/COMDAT-pairing
include`, so the X360 tokens are unchanged:
- ByteGrinder: HamBattleData, MicNull, SkeletonClip.
- MetaMusic: rndobj/PropAnim.
- MidiSynth: obj/PropSync. Also `MemTracker::StopLog()`, which native `utl/MemTracker.cpp` defines too.
- MidiInstrument: bandtrack/GemTrack.
- StorePanel: hamobj/DancerSequence.

MidiSynth's `synth/Mic.cpp` edge is **kept**. Mic is portable (`Mic::Set`, the ring buffer) and has
no other native emitter. Native now emits it through MidiSynth.

### 4.2 Native-only compile fixes (commit `5ec80376d`)

- `SampleInst::SynthPoll` (native block) reads `mSample`. DC3's `Sample()` accessor does not exist
  in this tree.
- `VorbisReader`'s native `DoFileRead` calls `::Decrypt`. The member `Decrypt(uchar*, int)` hid the
  file static.
- `MoggClipMap`'s copy ctor default-constructs its `Hmx::Object` base natively.
  - `Object(const Object&)` is declared in `obj/Object.h` and defined nowhere.
  - Retail's body (`fn_822774F8`) is a memberwise copy that copy-constructs the ref list at +0x20.
  - On native, that copy would duplicate the ref list and name entry.
  - The X360 branch keeps `Hmx::Object(mogg)`.
- `MidiInstrumentMgr::Poll` no longer polls the instrument natively (§4.4).

### 4.3 `Synth::PauseAllSfx` follows retail (commit `2a2c0d8e2`)

The native branch also paused DC3's `Sound` objects through `dynamic_cast<Sound*>`. RB3 has no
`Sound` class:
- `Sound.cpp` is not in `objects.json`.
- No factory registers one.
- Natively, `Sound.cpp` failed with 25 errors against this tree's `PlayableSample`.

Retail `fn_826FE780` pauses Sfx only, and native now does the same.

### 4.4 `MidiInstrumentMgr::Poll` double poll

Natively, `MidiInstrument` derives `SynthPollable` and is polled once per frame by
`SynthPollable::PollAll` (`MidiInstrument::SynthPoll`). The manager's `Poll` called
`mInstrument->Poll()` as well, so each instrument would have run twice per frame. The call is now
`#ifndef HX_NATIVE`. It was found as a compile error (the native `MidiInstrument` has no `Poll`), not
by a gate, and no gate drives a `MidiInstrumentMgr`.

### 4.5 `StandardStream::InitInfo` length (commit `221489a22`)

`unkec` (+0xf4) is `i4 / sampleRate` (the song length in seconds) in the X360 branch and in DC3. The
native branch stored the *previous* channel count over the rate, an integer 0.

Retail only ever stores the field: there are two `stfs …, 0xf4` in the unit and no load, and
`GetFileLength` returns a constant. So this fix is about fidelity and changes nothing a gate can see.

### 4.6 CharMeshHide's Sfx.cpp scatter include (commit `f5fddca34`, X360-visible)

The wrapper `#define gRev gRev_Sfx` / `gAltRev_Sfx` had nothing to rename in `Sfx.cpp`, which has no
file-static revs. It renamed `Sfx.h`'s `SfxMap::gRev` instead. As a result, `Sfx::Load` in that TU
referenced an undefined `SfxMap::gRev_Sfx`:
- The native link failed on it.
- On X360 the reference was wrong too, but nothing links there.

The defines are removed. `default/Sfx` scores `Sfx::Load` from the standalone `Sfx.obj` (304 B, 100),
and `default/CharMeshHide` has no `Sfx::Load` row, so no scored row should move. The A/B in §7 checks
this.

### 4.7 libogg verifies page CRCs natively, as retail does (commit `63f23f589`)

`ogg_sync_pageseek` skipped the checksum under `#ifndef HX_NATIVE`. Its comment said the game never
validates Ogg CRCs, because v0E decryption corrupts them. **Retail does validate them.** Retail
`ogg_sync_pageseek` (`0x82C2D198`, 100% matched with the check compiled in):
- loads page+0x16,
- zeroes it,
- calls `ogg_page_checksum_set` (`0x82C2CA88`),
- compares the result.

The magic-hash XOR covers bytes 20–23, which include CRC bytes 22–23, and `VorbisReader` undoes the
XOR before the page reaches libogg. With the check on, all four gate moggs decode to exactly the same
sample counts as before, so no page was dropped.

This mattered for the gates. Without the check, a wrong magic hash (round A below) still **passed**
the v0E gate: the granule and serial bytes it corrupts are not otherwise checked on the BOS page.
With the check on, the same sabotage fails v0E and v10 (round C).

### 4.8 `rb3-render` calls `Timer::Init` (native harness, commit `919d443c9`)

`SystemPreInit` calls `Timer::Init`, which sets the timebase-to-ms factors. `rb3-render` never called
it, so every `Timer` and `VarTimer` in the target read 0 ms. The first `synth-streamnull` run showed
this: the clock was frozen at 250 ms across a 20 ms spin.

`main` now calls `Timer::Init()` before `Symbol::Init()`. Every gate that ran before still passes, and
the run's `FAIL:` line set is identical to main's binary (below).

### 4.9 Native link set

`native/CMakeLists.txt` adds `W16TM_LINK_SOURCES` (the 25 files, plus tomcrypt aes/ctr) and
`W16TM_DEP_SOURCES` (the link closure) to `rb3-render`. The closure contains:
- `tomcrypt/crypt.c`;
- synth Stream, ADSR, SynthSample, SampleZone, SfxMap, MoggClip, MoggClipMap, MidiInstrumentMgr,
  MidiChannel, StreamReceiverFile and the FxSend family;
- flow DrivenPropertyEntry, DrivenPropertyMathOps, FlowPtr, FlowLabel, PropertyEventListener;
- the in-tree oggvorbis codec: the 18 retail C TUs, plus `lpc.c`, which `block.c`'s encoder path
  needs, and `registry.c` for the `_floor_P`/`_residue_P`/`_mapping_P` tables.

The in-tree codec is linked instead of the system libvorbis. `VorbisReader` compiles against the
in-tree headers, and pairing them with a system library's structs would be a layout mismatch.

These TUs carry `RB3_SYNCPROP_LOCAL_STATIC;RB3_HANDLE_LOCAL_STATIC`, as their X360 compile does. The
oggvorbis TUs get an include dir for their angled `<ogg.h>` includes.

## 5. Gates (phase `W16-TM`, `native/src/w16tm_phase.cpp`; `--no-w16tm` skips it)

| gate | reference | measured |
|---|---|---|
| crypto-aes-fips197 | FIPS-197 C.1, `69c4e0d8…` | match |
| crypto-ctr-sp800-38a | SP 800-38A F.5.1 block 1, `874d6191…` | match |
| crypto-ctr-keystream-4k | RB1 key, nonce 00..0f, 4,096 B; fast path vs byte path; CRC32 `ba1dfcca` | both `ba1dfcca` |
| mogg-v0b-sync-beep | `~/tmp/w16tm/mogg_ref.py` + ffmpeg: 1 ch, 220,500 samples, rms 1186.126, peak 15685 | 220,500, rms 1186.032, peak 15684 |
| mogg-v0b-sync-clap | 2 ch, 53,363 samples, rms 2839.074/2765.360, peak 32768 (ffmpeg's clip) | 53,363, rms 2838.904/2765.192, peak 32767 |
| mogg-v0e-shellmusic | OggMap: last 1,393,088, max step 38,656; samples in [last, last+2·step] | 6 ch × 1,421,517 |
| mogg-v10-trainer | OggMap: last 1,091,584, step 117,568 | 3 ch × 1,191,723 |
| fx-compress-bypass | ratio 1.0 is bit-for-bit identity | identical |
| fx-compress-ratio | slope above/below the −6 dB threshold = 1/4 | 0.2497 |
| fx-compress-gate | −60 dB input decays with the gate's 1.01 s release: 0.001·e^(−96000/48480) = 1.38042e-4 (ungated: 1.678e-3) | 1.38091e-4 |
| synth-utl-transpose | 2^(st/12) and inverse, −24..24 | worst 5.8e-8 |
| synth-utl-tempo-sync | 120 bpm measure table; unknown symbol at 90 bpm = 1.5 | match |
| flow-if-operators | 6 operand pairs × 6 operators against plain numeric comparison; symbol > 1 false | 0/36 wrong |
| flow-switchcase-operators | same 36 cases through `IsValidCase`, symbol operand, `use_last_value` | 0/36 wrong |
| flow-switchcase-transition | to/from both match, and types must match exactly (3→5.0f is false); kDefault never | match |
| synth-streamnull | idle state, 250 ms start, clock advances while playing, speed, `Resync`, 20 distinct faders | 250 → 270.0 ms over 20 ms |

Notes on the mogg references:
- The two v0B files were checked sample by sample with `RB3_W16TM_DUMP` against the ffmpeg reference
  (`~/tmp/w16tm/ref/*.s16`):
  - sync_beep: 220,500 samples, max |diff| 1 LSB, 95.0% exact.
  - sync_clap: 106,726 interleaved samples, max |diff| 2 LSB, 83.9% exact.
- The residue is decoder float rounding (ffmpeg's own Vorbis decoder against libvorbis).
- The reference decrypts with a **little-endian** CTR counter. The big-endian counter yields one
  CRC-valid page per file, against 9/9 and 4/4.
- The fx-compress-gate reference was first written wrong, as "gated to silence". The first run showed
  1.38091e-4. Reading `Process` shows the gate branch sets a target gain of 0 that the envelope
  reaches through `mPeakReleaseTime` (1.01 s), which predicts 1.38042e-4. The gate was corrected to
  that model, and the implementation was not touched.

Full `rb3-render` run on the final binary:
- `RESULT: ALL GATES PASSED (0 gate failure(s))`.
- 128 `[PASS]` lines, against 112 on main's binary (`native/build/rb3-render`, built 11:08). The
  difference is exactly the 16 gates above.
- The `FAIL:` line multiset (302 lines, 293 of them `Locale.cpp:472 mSymTable` from the W16-PX
  bandtrack phase) is **identical** to main's binary. W16-TM adds none.

## 6. Controls and findings

### 6.1 Sabotage runs (each built, run, then reverted)

| round | sabotage | expected | result |
|---|---|---|---|
| A | `gRB1Key[0]` 0x37→0x38; magic-hash A `^1`; compressor `/ mRatio` → `/ (mRatio·0.5)`; FlowIf `>=` → `>` | v0B ×2, v0E, ratio, flow-if fail | v0B ×2, ratio (0.4998) and flow-if (2/36) failed; **v0E passed** |
| B | grinder op 32 `^0x1F` → `^0x1E`; HMXA probe | v0E/v10 fail | **both passed**: op 32 is not on either file's key schedule, so this control was inert. The probe showed exactly one HMXA page per file, at offset 0 (hashes `13b285b2/8e7ceb41`, `f6fcada9/479c98dd`) |
| — | CRC check enabled (§4.7) | — | — |
| C | magic-hash A `^1` again | v0E, v10 fail | both failed (`StreamInit -1`); v0B passed |
| D | CTR fast-path word 2 `^1` | keystream gate fails | keystream failed (`ad582808` vs `ba1dfcca`); v0E/v10 failed (truncated, PacketOut errors); **v0B passed** |
| E | key mask byte 0 skipped | v0E, v10 fail | both failed; v0B passed |
| F | FlowSwitchCase `>` → `>=`; transition type check removed | both switchcase gates fail | operators 2/36 wrong; transition 3→5.0f = 1 |
| (natural) | no `Timer::Init` | streamnull fails | failed, clock frozen at 250 ms |

Round A's v0E pass is what found §4.7. Round D's v0B pass corrects a claim made earlier in this lane:
the v0B moggs never reach `ctr_encrypt_fast`. Their reads leave the pad partly consumed, so every
block takes the byte path. The keystream gate is the only thing that covers the fast path for v0B,
and the v0E/v10 moggs cover it end to end.

### 6.2 Other findings, not changed

- The native `VorbisReader::Decrypt` scans every HMXA occurrence in a read buffer. Retail checks only
  the start of each 0x400 block. The probe found one hit per file, at offset 0. Now that CRCs are
  checked, a false match inside page data would drop that page instead of passing it silently.
- `OggValidator::Create` is declared in `OggMap.cpp` and has no body anywhere. `OggMap::Validate` and
  the gap rows behind it cannot run natively until one exists.
- Native `Synth` was constructed bare by the mogg gates (`GateSynth` adds the three faders from
  `Synth::Init`). The rest of `Synth::Init` (synth_hud overlay, mics, security) does not run in
  `rb3-render`.

### 6.3 FlowValueCase Save/Load asymmetry (unadjudicated)

`Save` writes `SAVE_REVS(2,0)` and `mValue` **before** the superclass. `Load` reads the superclass
first and accepts revs ≤ 1 (`ASSERT_REVS(1,0)`). A save→load round trip of this tree's own output
would therefore fail. Retail's `Save`/`Load` bodies are not identified (the unit has only funclets in
the report), so there is nothing to read them against. No change was made.

## 7. X360 A/B

`tools/ab_measure.py --worktree ~/tmp/wt-w16tm-ab --patch ~/tmp/w16tm_src.patch --jobs 8`. The patch
is `git diff b79d790b8..HEAD -- src/` (13 files), applied to main `44f33e78b`.

Result (run dir `~/tmp/wt-w16tm-ab/.ab_measure_runs/20261007-120357-w16tm-1912985`, ruler `name_check`):

```
leg A: matched=54939 masked=25214 honest=29725 code%=59.312300  (recompiles: 0, settled)
leg B: matched=54939 masked=25214 honest=29725 code%=59.312300  (recompiles: 16, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   (legA 64.316590 -> legB 64.316590)
units at 100% [mpn ruler]: legA 626 -> legB 626  (Δ+0; 0 reached 100, 0 fell off)
units at 100% [all-rows-fuzzy ruler]: legA 547 -> legB 547  (Δ+0)
```

The prediction was Δ0. Every edit except §4.6 sits behind `HX_NATIVE`, and the §4.6 copy of
`Sfx::Load` is not a scored row. Leg B recompiled 16 TUs, so the patch was applied and compiled; this
was not an absent-vs-absent run. The `none` control was flat too.

## 8. Native gates

`tools/native_health.sh` on the final source (`~/tmp/w16tm_health2.log`), its own lines:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=165 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=13 scatter_dirb=0 scatter_multihost=13 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
LAYOUT_ODR_RESULT verdict=PASS x360_tus=1267 x360_failed=0 x360_split=0 x360_unresolved=0 x360_allowed=138 x360_stale=0 native_tus=1912 native_failed=0 native_split=0 native_unresolved=0 native_allowed=2 native_stale=0 rc=0
```

`gates_pass` went from 162 to 165 against this lane's own earlier health run (before the
switchcase/streamnull gates and the `Timer::Init` fix). Those are the three gates added in
`919d443c9`.

Scatter, compared with the most recent earlier health JSON on this machine
(`~/tmp/native_health_scatter_4110399199.json`, 11:09, run from another lane's tree, so treat it as
an approximate baseline):
- `scatter_unlinked` went from 15 to 13. `synth/Mic.cpp` and `bandtrack/GemTrack.cpp` now reach a
  target. Mic comes in through MidiSynth's kept edge (§4.1).
- `scatter_multihost` went from 14 to 13. `gesture/SkeletonClip.cpp` lost its ByteGrinder host to
  the §4.1 guard, and `obj/PropSync.cpp` lost MidiSynth as one of its three hosts.

The lane's last action is `tools/native_build_gate.sh`, run after the final commit. Its result line
is in the lane report, not here, so that nothing follows it.
