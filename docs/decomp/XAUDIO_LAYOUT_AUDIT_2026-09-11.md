# XAudio2 / XAPO / XMA layout audit across the synth path (lane W3-D, 2026-09-11)

**Branch** `w3-xaudio-layout` (worktree `~/tmp/wt-w3-d`, base `814e3a60`).
**Scope** generalise lane `16b3c93b`/`814e3a60`'s finding — RB3 retail (cl 10224,
older XDK) does not share DC3's XDK struct layouts — across every XAudio2/XAPO/XMA
struct `src/system/synth_xbox/` touches, apply the bool-vs-short discipline to the
synth class headers, and settle `synth/Sound.cpp` / `ThreeDSound.cpp`.

Every retail figure below was read off `build/45410914/asm/*.s` keyed on the
`.fn fn_<addr>` symbol (never the synthetic address column), or off a
`report.json` row at `fuzzy == 100` (a 100% row verifies every offset the body
touches, by construction). Our-side offsets are the compiler's
(`scripts/harvest/class_layout_report.py`), not header comments.

**Standing correction to the brief:** `native/CMakeLists.txt` excludes all of
`synth_xbox/` (platform-only guest, checked at lines 1380-1384), so nothing here
pays as native impact. It pays as X360 accuracy and bytes.

---

## 1. Struct-by-struct table

Legend: **our** = this tree before this lane; **dc3** = `../dc3-decomp/src/xdk/xaudio2/*.h`;
**retail** = RB3 `band.exe`. Verdicts: `SAME` (dc3 == retail == ours), `FIXED`
(differed, corrected this lane or by `16b3c93b`), `UNVERIFIED` (no retail access
found on the synth path — no claim made).

### 1.1 `XAUDIO2_BUFFER` (0x24) — SAME

| field | our | dc3 | retail | evidence |
|---|---|---|---|---|
| Flags | 0x0 | 0x0 | 0x0 | `?InitSourceBuffer@Voice@@` 0x82B64F88 (100%): `li r11,0x40; stw r11,0x0(r4)` |
| AudioBytes | 0x4 | 0x4 | 0x4 | same fn: `lwz r10,0xc(r3); stw r10,0x4(r4)` (from `mAudioBytes`) |
| pAudioData | 0x8 | 0x8 | 0x8 | same fn: `lwz r10,0x8(r3); stw r10,0x8(r4)` |
| PlayBegin | 0xc | 0xc | 0xc | same fn: `lwz r10,0x18(r3); stw r10,0xc(r4)`; by-value copy read at `0xbc(r1)` in `InitVoiceParameters` 0x82B652E0 |
| PlayLength | 0x10 | 0x10 | 0x10 | `stw r11(0),0x10(r4)` |
| LoopBegin / LoopLength / LoopCount | 0x14/0x18/0x1c | same | same | `stw …,0x14/0x18/0x1c(r4)`; `li r11,0xff; stw r11,0x1c(r4)` |
| pContext | 0x20 | 0x20 | 0x20 | `stw r11(0),0x20(r4)` |

### 1.2 `tWAVEFORMATEX` (0x12, packed) and `XMA2WAVEFORMATEX` (0x34, packed) — SAME

Verified in full by `?InitVoiceParameters@Voice@@` 0x82B652E0 at 100% (the
`814e3a60` fix): `sth 0x0/0x2/0xc/0xe/0x10/0x12`, `stw 0x4/0x8/0x14/0x18/0x1c/0x20/
0x24/0x28/0x2c`, `stb 0x30/0x31`, `sth 0x32`. The `0x12`-byte copy in
`createOrReuse` 0x82B64DD8 (`li r5,0x12; bl memcpy`) fixes `sizeof(WAVEFORMATEX)`.

### 1.3 `XAUDIO2_VOICE_STATE` (0x10) — SAME

| field | our | dc3 | retail | evidence |
|---|---|---|---|---|
| pCurrentBufferContext | 0x0 | 0x0 | (0x0) | not read |
| BuffersQueued | 0x4 | 0x4 | 0x4 | `IsPlaying` 0x82B65058: `addi r4,r1,0x50` / GetState / `lwz r11,0x54(r1)` |
| SamplesPlayed (u64) | 0x8 | 0x8 | 0x8 | same fn `ld r11,0x58(r1)`; `GetAddr` 0x82B65160 `ld r10,0x58(r1)` |

### 1.4 `XAUDIO2_VOICE_DETAILS` — FIXED by `16b3c93b` (dc3 0x10 / four fields; RB3 0xc / three)

`UpdateMix` 0x82B65510: `addi r4,r1,0x58` / GetVoiceDetails (slot 0) / `lwz r28,0x5c(r1)`
= `InputChannels` at **+4**; dc3 reads +8. Re-confirmed this lane on the same
listing (line `822D8FB4`). No other field is read anywhere on the synth path.

### 1.5 `XAUDIO2_EFFECT_DESCRIPTOR` (0xc) / `XAUDIO2_EFFECT_CHAIN` (0x8) — SAME

`createOrReuse` 0x82B64DD8 builds one on the stack: `stw r11(eg),0x60(r31)` pEffect,
`stw r10(0),0x64` InitialState, `stw r25(1),0x68` OutputChannels; chain at 0x58:
`stw r25(1),0x58` EffectCount, `stw r9(&desc),0x5c` pEffectDescriptors. Also
covered by `?PreInit@Synth360@@` 0x82B5D438 at 100% (1,304 B).

### 1.6 `XAUDIO2_SEND_DESCRIPTOR` (0x8) — FIXED this lane (alignment, not offsets)

Offsets `Flags 0x0 / pOutputVoice 0x4` are the same everywhere (`UpdateSends`
0x82B65948: `stw r30,0x58(r1)`, `stw r9,0x5c(r1)`). But dc3 wraps the struct in
`#pragma pack(push,1)` and ours had dropped it. Retail copy-constructs elements
through **memcpy of 8 bytes**, which MSVC emits only for an alignment-1 type:

| function | address | copy |
|---|---|---|
| `??$__uninitialized_copy@PAUXAUDIO2_SEND_DESCRIPTOR…` | 0x82B5C710 | `li r5,0x8; bl fn_8282A900` per element |
| `??$__uninitialized_fill_n@PAUXAUDIO2_SEND_DESCRIPTOR…` | 0x82B5C828 | same |
| `?push_back@?$vector@UXAUDIO2_SEND_DESCRIPTOR…` | 0x82B5D3B0 | same |

Restored in `99fa7de5`. **Predicted** +3..5 fns / +292..1,280 B; **measured**
+5 fns / +736 B (§5).

### 1.7 `XAUDIO2_VOICE_SENDS` (0x8) — SAME

`UpdateSends`: `stw r29(1),0x50(r1)` SendCount, `stw r11,0x54(r1)` pSends.

### 1.8 `XAUDIO2_FILTER_PARAMETERS` — UNVERIFIED

No field access in `synth/` or `synth_xbox/` (the `BinkReader.cpp` `Frequency`
hits are a Bink struct). No vtable call to `SetFilterParameters` (slot 0x20) was
found in the Voice/Synth listings. No claim.

### 1.9 `IXAudio2` (engine interface, defined locally in `synth_xbox/Synth.cpp`)

| slot | ours | retail evidence |
|---|---|---|
| 0x20 CreateSourceVoice | index 8 | `createOrReuse` 0x82B64DD8: `lwz r3,0xc8(TheXboxSynth); lwz r7,0(r3); lwz r11,0x20(r7); bctrl` with r4=&PoolVoice.sourceVoice, r5=wfx, r6=0, f1=maxratio, r8=0, r9=pSends, r10=&chain |
| 0x24 CreateSubmixVoice / 0x28 CreateMasteringVoice | index 9/10 | `?PreInit@Synth360@@` 0x82B5D438 at 100% |

### 1.10 `IXAudio2Voice` / `IXAudio2SourceVoice` vtable order

Slots seen used by retail on the synth path, all agreeing with `xaudio2.h`:

| slot | method | retail site |
|---|---|---|
| 0x00 | GetVoiceDetails | UpdateMix 0x82B65510 |
| 0x04 | SetOutputVoices | UpdateSends 0x82B65948 |
| 0x18 | SetEffectParameters | Stop 0x82B64D60 (`li r6,0x10`), SyncEffectParams (mis-carved, `li r6,0x34`) |
| 0x1c | GetEffectParameters | IsPlaying 0x82B65058 (`li r6,0x10`) |
| 0x30 | SetVolume | UpdateMix |
| 0x40 | SetOutputMatrix | UpdateMix |
| 0x4c | Start | SafeRestart 0x82B65230, blockingStart 0x82B66630 |
| 0x50 | Stop | Pause 0x82B65428, ~Voice 0x82B662E8 |
| 0x64 | GetState | IsPlaying, GetAddr — **ONE argument on RB3's XDK** (see below) |
| 0x68 | SetFrequencyRatio | SetSpeed 0x82B658B8 |

**`IXAudio2SourceVoice::GetState` takes one parameter on RB3's XDK — FIXED this
lane.** Both retail call sites set r4 only (`addi r4,r1,0x50; lwz r11,0x64(r11);
mtctr; bctrl`; no `li r5`). The `Flags` parameter (`XAUDIO2_VOICE_NOSAMPLESPLAYED`)
is an XAudio2-2.8-era addition that DC3's XDK has and RB3's does not; our
two-argument spelling emitted a dead `li r5, 0x0` at each site (visible as the
`replace` at idx 22 of `IsPlaying`'s diff). `xaudio2.h` and the two Voice.cpp
call sites corrected. Every other slot's argument count observed on the synth
path (`SetVolume(f,0)`, `Start(0,sync)`, `Stop(0,0)`, `SetEffectParameters(0,p,0x10,0)`,
`GetEffectParameters(0,p,0x10)`, `SetOutputMatrix(dst,src,dstCh,m,0)`,
`SetFrequencyRatio(f,0)`, `SubmitSourceBuffer(&b,0)`, `CreateSourceVoice(7 args)`)
agrees with the header.

Slots 0x08-0x14, 0x20-0x2c, 0x34-0x3c, 0x44-0x48, 0x54-0x60, 0x6c-0x70 were not
observed on the synth path in this audit (several are exercised by
`Voice::Init` / `dispose`, which were unpaired until this lane — see §5 run 2).
No slot was found *disagreeing*.

### 1.11 XAPO: `XAPO_PROCESS_BUFFER_PARAMETERS`, `XAPO_LOCKFORPROCESS_BUFFER_PARAMETERS`, `CXAPOBase` (0x20), `CXAPOParametersBase` (0x40), `ATG::CSampleXAPOBase` (`mParams` 0x40, `mWav` 0x58) — SAME

No direct field access in our synth sources; every access is inside the
`CSampleXAPOBase<>` template. Those instantiations are at 100% for
GainEffect (7/7), MeterEffect (6/6), HeadsetPlaybackEffect (7/7),
EnvelopeGenerator (9/9), PitchShiftEffect (10/10), e.g.
`?Process@?$CSampleXAPOBase@VGainEffect…` 0x82B6CC18 and
`??0?$CSampleXAPOBase@VGainEffect…` 0x82B6CBA0 — which pins the base-class
sizes and the two template members. `XAPO_REGISTRATION_PROPERTIES` (0x42c) is
only ever passed by pointer.

Not done: dc3 carries `__declspec(uuid(...))` on `IXAPO`/`IXAPOParameters`
and a real `CSampleXAPOBase<>::m_regProps` definition (with `__uuidof`);
ours has neither. RB3's map has **no** `??__E?m_regProps@…` dynamic
initialisers and no `_GUID_a90bc001…` symbols, so this is not the same
situation as DC3 and was left alone. `?QueryInterface@CXAPOBase@@` (196 B) and
`?Release@CXAPOBase@@` (112 B) are XDK library bodies with no source here (0%).

### 1.12 `xaudio2fx.h` — **header absent from this tree; RB3 layout DIFFERS from dc3's**

dc3 has `src/xdk/xaudio2/xaudio2fx.h`; we do not, and our
`synth_xbox/FxSendReverb.cpp` is a ctor/dtor stub (no `SyncEffectParams`, no
`CreateFx`). Retail has both, and the converter:

| struct | dc3 | retail | evidence |
|---|---|---|---|
| `XAUDIO2FX_REVERB_I3DL2_PARAMETERS` | 0x34 | 0x34, same fields | `?ReverbConvertI3DL2ToNative@@` 0x82B67D30 reads src +0 `lwz`, +4 `lwa` (Room), +8 `lwa` (RoomHF), +0x10, +0x14, +0x18 `lwa` (Reflections), +0x1c, +0x20 `lwa` (Reverb), +0x24, +0x28, +0x2c, +0x30 |
| `XAUDIO2FX_REVERB_PARAMETERS` | **0x38** (trailing `WetDryMixPct` at 0x34) | **0x34** | caller `fn_82B68070` passes `li r6, 0x34` to SetEffectParameters; converter writes +0 (`stw`), +8..+0x13 (twelve `stb`), +0x14/0x18/0x1c/0x20/0x24/0x28/0x2c/0x30 (`stw`); **nothing at +4 or +0x34** |

So dc3's 0x38-byte struct is the newer XDK's; RB3's is the XAudio2-2.7 shape.
Not fixed here: (a) the header does not exist in this tree, (b) the 740-B
converter is XDK inline code (out of scope by standing directive), (c) the
function that would consume it is mis-pinned (next paragraph). Recorded so
that whoever wires `FxSendReverb360::SyncEffectParams` uses 0x34.

**Splits handoff:** `sslgen.c: .text 0x82B68070–0x82B68D40` is ONE 3,280-B
function that constructs ~30 `Symbol`s (30 × `bl fn_827C0728`), calls the
converter and `SetEffectParameters(0, r31+0x60, 0x34, 0)`. That is
`FxSendReverb360::SyncEffectParams(IXAudio2SubmixVoice*) const` (dc3's has the
30-entry I3DL2 preset table), not OpenSSL. Not moved by this lane — it needs
source to be worth pinning and the source needs the header above.

---

## 2. Field-width table (bool vs short discipline)

Method: for each class, our compiler layout (`class_layout_report.py`) joined
against a census of every `lbz/stb/lhz/sth/lwz/stw` in the owning unit's
retail `.s` (`~/tmp/w3d_widths.json`, 30 units). A 1-byte field should see only
`lbz/stb` at its offset; a 2-byte one only `lhz/sth`. Offsets ≥ 0x8000 are
reached through `addis` and are invisible to this census (MicXbox's 0x90xx
fields — **no claim**). Offset collisions with other objects' fields are
possible, so every flag was adjudicated by reading the instruction.

### 2.1 `synth_xbox` (Voice.cpp listing, every access)

| class | off | our type | retail width | sites |
|---|---|---|---|---|
| Voice | 0x38 `mXMA` | bool | `lbz` ×4 | InitSourceBuffer, GetAddr, InitVoiceParameters, SetSpeed |
| Voice | 0x40 `mReverbEnabled` | bool | `lbz` ×3, `stb` ×1 | UpdateMix, UpdateSends, SetReverbEnable (`stb r4,0x40`) |
| Voice | 0x48 `unk48` | bool | `lbz` ×1, `stb` ×2 | UpdateMix, UpdateSends |
| Voice | 0x49 `mSynchronized` | bool | `lbz` ×4 | SafeRestart, Pause, ~Voice, blockingStart; ctor `stb` |
| Voice | 0x4a `mStereo` | bool | `lbz` ×4 | SetData, GetAddr, UpdateMix, InitVoiceParameters (`cntlzw/extrwi/xori/addi` = `?2:1`); ctor `stb` |
| Voice | 0x4b `unk4b` | bool | `lbz` ×1, `stb` ×3 | createOrReuse (`stb r11,0x4b(r24)` = `pSends==0 \|\| SendCount<=0`), UpdateMix, UpdateSends; ctor `stb` |
| Synth360 | 0xc4 `unkc4` | bool | `lbz`/`stb` | (other widths at 0xc4 are other objects) |
| Synth360 | 0xdc `mDolbyEnabled` | bool | `lbz` ×2, `stb` ×3 | |
| Synth360 | 0xdd `mDolbyPending` | bool | `lbz` ×1, `stb` ×3 | |
| Synth360 | 0x110 `unk110` | bool | `stb` ×1 | |
| Synth360 | 0x124 `unk124` | bool | — | untouched in the Synth listing |
| SampleInst360 | 0x50/0x51/0x52 (`SampleInst` bytes) | bool/bool/short | — | untouched in SampleInst360's own listing (accesses at 0x50 there are `Voice::mSourceVoice` through `mVoice`) |

No `lhz`/`sth` was found at any of these offsets. The ctor `fn_82B66B20`
(now `??0Voice@@QAA@_NH0@Z`) stores bytes at 0x49/0x4a/0x4b individually,
which is the strongest single witness that 0x4a/0x4b are not one halfword.

### 2.2 `synth/` (platform layer — what native links)

Classes screened: Sfx, SfxInst, Sequence, SeqInst (+ the six Seq subclasses),
SynthEmitter, MoggClip, BinkClip, Stream, Synth, Fader, SampleInst, SynthSample,
SampleZone, MidiChannel, MidiSynth, MidiInstrument, MoggClipMap, StandardStream,
StreamReceiver, ADSR/ADSRImpl, FxSend, FxSendReverb, VoiceBeat, WavMgr,
WavReader, VorbisReader, BinkReader, StreamNull, MetaMusic, AudioDucker, SlipTrack,
OggMap. **No bool-vs-short disagreement was found.** The screen raised four
flags, all adjudicated as non-defects:

| flag | resolution |
|---|---|
| MoggClip / BinkClip `mPlaying` @0x7e "size 2" | size-inference artifact: bool followed by padding; retail `lbz/stb` ×3 agrees with bool |
| ExternalMic `mQuit` @0x8, ChatReceiver `unk8` @0x8 | mixed widths at offset 8 are other objects' fields (offset 8 of everything); the `lbz/stb` present agree with bool |

Fields the census could not see (retail never touches them in the unit):
`SampleInst` 0x50-0x53, `StreamReceiver` 0x18018/0x18020, `FxSend` 0x4c
`mReverbEnable`, `VorbisReader` 0x44/0x118-0x11b, `BinkReader` 0xd0, `MetaMusic`
0x38, `MoggClip` 0x7c. Classes the layout tool could not report (ambiguous TU):
`Mic` (base), `PlayableSample`, `StreamReader`, `SoundSeq`, `ChainSeq`, `Emitter`.

---

## 3. `synth/Sound.cpp` and `synth/ThreeDSound.cpp` — recommendation

Every fact re-verified this lane (Python over `orig/45410914/band.exe`,
14,363,648 B; `command grep -a`; never the shell `grep`):

| fact | result | control |
|---|---|---|
| `.?AVSound@@` in band.exe | **0** | `.?AVSfx@@`, `.?AVSfxInst@@`, `.?AVSequence@@`, `.?AVSeqInst@@`, `.?AVSynthEmitter@@`, `.?AVMoggClip@@`, `.?AVStream@@`, `.?AVSynth360@@`, `.?AVStreamReceiver360@@`, `.?AVSampleInst360@@` = **1 each** |
| `.?AVThreeDSound@@` | **0** | as above |
| `ThreeDSound` (plain string) | **0** | |
| `fader_pan` (its DTA property) | **0** | |
| in `config/45410914/objects.json` | **neither** | |
| in `../rb3` (Wii dev decomp) | **neither file** | |
| in `../dc3-decomp` | both present | (origin: `c5c1650f` scaffold) |
| `#include "synth/ThreeDSound.h"` anywhere | **0** | |
| `#include "synth/Sound.h"` anywhere | **8** — `char/CharLipSync.h`, `flow/FlowSound.{h,cpp}`, `hamobj/{HamListRibbon.h,HollaBackMinigame.h,HamCharacter.cpp,HamNavList.cpp}`, `native/src/native_link_glue.cpp:82` | ⚠ contradicts the previous lane's "nothing compiles them" — the *header* is compiled into 5 match-build TUs (FlowSound, Flow, CharLipSync, HamNavList, HamCharacter are all in objects.json) and into the native link glue (`OBJREFCONCRETE_COPYREF(Sound)` at line 175) |
| native compiles `Sound.cpp`/`FlowSound.cpp` | **no** (`native/CMakeLists.txt` lists neither); the glue needs only the type | gate PASSes today with both `.cpp` uncompiled |

The two bugs are real in the source text and are reasons the files must not be
kept *silently*:

* `Sound::SetSpeed(float f1, Hmx::Object*)` (line 454) clamps `speedTranspose`
  where dc3 clamps the argument `speed`; `f1` is never used.
* `ThreeDSound::IsPlaying()` (line 193) is `!mIsLooping && (…)`; dc3's is
  `mIsLooping || Sound::IsPlaying()` — inverted on the looping case.

**Recommendation:** delete `src/system/synth/Sound.cpp`, `ThreeDSound.cpp` and
`ThreeDSound.h` — zero includers, zero compile edges in either build, zero
retail presence, two known bugs. **Keep `Sound.h`** (with a banner: "DC3-only
class, absent from RB3 retail; kept as a type for the flow/hamobj/CharLipSync
DC3 scaffolds and native_link_glue") until those five scaffold TUs are
themselves adjudicated — deleting the header today breaks five compile edges
and the native link. Not done by this lane; the merge owner decides.

---

## 4. Identity repairs found while auditing (each proven on retail bytes)

### 4.1 `0x822d8610` is `StreakMeter::CombineMultipliers`, not `Voice::SetReverbEnable`

The map said `?SetReverbEnable@Voice@@QAAX_N@Z` (97.6%), and
`scripts/symbol_aliases.json` carried a "T1-proven" fold with that as survivor
and `?CombineMultipliers@StreakMeter@@QAAX_N@Z` as folded. The body is
`lbz r10,0x1e8(r3); clrlwi r11,r4,24; cmplw; beqlr; stb r4,0x1e8(r3); b fn_822D8210`
where `fn_822D8210` = `?MultiplierChanged@StreakMeter@@`, and it sits between
StreakMeter.cpp's `…–0x822D8460` and `0x822D8630–…` pins. `sizeof(Voice)` is
0x74 — nothing at 0x1e8. Retail's real `Voice::SetReverbEnable` is
`fn_82B65C58`: `stb r4,0x40(r3); b fn_82B65948(UpdateSends)` — 8 bytes, no
compare — and SampleInst360's `lwz r3,0x54(r3); b fn_82B65C58` thunk confirms it.
The two bodies differ, so `/OPT:ICF` cannot have folded them; T1 passed
because OUR `SetReverbEnable` carried the compare, i.e. **the alias was
financed by our own source defect** (exactly the hazard CLAUDE.md's alias
section describes). Repaired four ways: source (compare removed), map
(`0x822d8610` → CombineMultipliers, `0x82b65c58` → SetReverbEnable), splits
(the 0x822D8610 block re-homed from Voice.cpp to StreakMeter.cpp), alias group
(survivor reassigned, `SetReverbEnable` withdrawn with a `repair` record).

### 4.2 Voice.cpp's `.text` pin ended 0x8B4 bytes short

`ExternalMic.cpp: .text 0x82B6639C–…` began with four anonymous 0% rows that
are Voice.cpp: `fn_82B663A8` (600 B) calls `push_back<SEND_DESCRIPTOR>`×3,
`InitSourceBuffer`, `InitVoiceParameters`, `createOrReuse`, `UpdateMix` =
`Voice::Init(bool)`; `fn_82B66630` (116 B) = `blockingStart(bool)`;
`fn_82B666D8` (972 B) = `StartVoiceThreadEntry`; `fn_82B66B20` (296 B) stores
every Voice field 0x4–0x58 = the ctor; `fn_82B66C48` (8 B, `li r4,0; b
blockingStart`) = `Voice::Start()`. `??1ExternalMic` at 0x82B66C50 is the true
boundary. Pins moved accordingly (Voice `end:0x82B66C50`, ExternalMic
`start:0x82B66C50`).

`fn_82B66C48` was in the map as `??2OutfitConfig@@SAPAXI@Z` at 100% — a
byte-shape identification of `li r4,0; b <placeholder>` — and
`ExternalMic.cpp` carried a "sw2 scatter-include" of `bandobj/OutfitConfig.cpp`
(`1a203fb8`) whose only effect was to make that mis-named row pair. Both
removed.

### 4.3 Thirteen Voice rows named (DC3 `ham_xbox_r.map` spellings; identity from this-offset signatures)

| address | name | signature evidence |
|---|---|---|
| 0x82b64d20 | `?SetData@Voice@@QAAXPBXHH@Z` | `stw r4,0x8; stw r5,0xc; stw r6,0x10`, `lbz 0x4a` halving |
| 0x82b64d60 | `?Stop@Voice@@QAAX_N@Z` | egParams[+8]=1.0, SetEffectParameters slot 0x18 size 0x10, `mState=1` |
| 0x82b64dd8 | `?createOrReuse@Voice@@AAAJ…@Z` | `new(0x94)`→EnvelopeGenerator ctor, `new(0x10)`, effect chain, `CreateSourceVoice` slot 0x20, `memcpy 0x12`, `stb 0x4b` |
| 0x82b65058 | `?IsPlaying@Voice@@QAA_NXZ` | state 2/1/4 tests, GetState, GetEffectParameters, `== 0.0f` |
| 0x82b65160 | `?GetAddr@Voice@@QAAHXZ` | GetState, `+mStartSamp`, `/ (mAudioBytes>>1 or >>2)` by `lbz 0x4a`, `<<1` |
| 0x82b658b8 | `?SetSpeed@Voice@@QAAXM@Z` | clamp, `lbz 0x38`, `stfs 0x2c`, SetFrequencyRatio slot 0x68 |
| 0x82b65c58 | `?SetReverbEnable@Voice@@QAAX_N@Z` | §4.1 |
| 0x82b66038 | `?HasPendingVoices@Voice@@SA_NXZ` | walks `gPendingVoices`/`gPendingSyncVoices` under `gLockPendingLists` |
| 0x82b663a8 | `?Init@Voice@@QAAX_N@Z` | §4.2 |
| 0x82b66630 | `?blockingStart@Voice@@QAAX_N@Z` | §4.2 |
| 0x82b666d8 | `?StartVoiceThreadEntry@@YAKPAX@Z` | §4.2 |
| 0x82b66b20 | `??0Voice@@QAA@_NH0@Z` | §4.2 |
| 0x82b66c48 | `?Start@Voice@@QAAXXZ` | §4.2 |

### 4.4 Two Voice.cpp bodies brought to retail's shape

* `Voice::Stop(bool)` — retail 0x82B64D60 never reads r4 and has no
  immediate-stop arm (that `Stop` slot-0x50 call lives only in `~Voice`). dc3's
  two-arm body is DC3's. The parameter stays (callers pass it; DC3 spelling
  `_N`).
* `Voice::IsPlaying()` — ours tested three words of `XAUDIO2_VOICE_STATE` and
  ignored the envelope generator; retail (and dc3, verbatim) returns `false`
  only when `BuffersQueued==0 && SamplesPlayed==0`, then reads the EG params
  back under `Synth360+0x88` (`TryEnter`/`Exit`) and returns
  `params.unkc == 0.0f`. **Behavioural bug**: a voice in envelope release was
  reported "playing" until XAudio2 ran dry, and a voice whose release had
  completed likewise. Fixed by porting dc3's body (which is retail's shape).

### 4.5 `StreamReceiver360::PlayImpl` / `GetPlayCursor` were swapped in the map

Both are 16-B `lis/ori/lwzx r3; b <Voice fn>` thunks at 0x82b6bae8 / 0x82b6baf8.
Retail 0x82b6bae8 tail-calls `fn_82B66C48` (= `Voice::Start`) and 0x82b6baf8
tail-calls `fn_82B65160` (= `Voice::GetAddr`); the map had the names the other
way round. The old 98.8 on "GetPlayCursor" was our `b GetAddr` charged against
the mis-named `??2OutfitConfig` target; naming `Start` then dropped "PlayImpl"
100 → 98.8 (its callee stopped being a forgiven placeholder), which is what
exposed the swap. Names swapped; both rows read 100 after.

### 4.6 `Voice::createOrReuse` and `Voice::dispose`

* `createOrReuse` was declared, called from `Init`, and **defined nowhere** —
  the `UpdateSends` class of defect (`814e3a60`): the match build only
  compiles. Written from retail 0x82B64DD8. Differs from dc3's in four ways:
  no `OutputVoice()` early-out; `effectDesc.OutputChannels` is the literal 1;
  no hr-failure `snprintf`/`MILO_FAIL` arm (returns `hr` directly); and
  `unk4b = (sends == 0 || sends->SendCount <= 0)` — dc3 stores the **opposite
  polarity** (`SendCount > 0`), so dc3's `unk54` and RB3's `unk4b` mean
  different things. The engine call is bracketed by `MemPushTemp/PopTemp`
  and a `CritSecTracker` on `Synth360+0x88` (its EH funclet `fn_82B64F60`
  calls `??1CritSecTracker@@`; `fn_82B64F10` is the `new EnvelopeGenerator`
  cleanup calling the 0x94-byte pool free).
* `dispose` (248 B, `fn_82B661F0`) was pinned under **`Lit.cpp`** — a lighting
  unit whose other seven blocks are at 0x82497xxx–0x8249Axxx. It sits in the
  hole between Voice.cpp's two `.text` blocks and is what `~Voice` calls. Retail
  additionally does `if (unk4b) { SetOutputVoices(&{0,0}); unk4b = 0; }` after
  `FlushSourceBuffers`, and moves the two voice counters (`0x82E120BC`,
  `[0]--`, `[1]++`) inside the GC lock; ours did neither. Ported; the Lit.cpp
  claim removed; Voice.cpp's two blocks merged into one
  (`0x82B64C20–0x82B66C50`). Named `?dispose@Voice@@AAAXPAUPoolVoice@@I@Z`
  (DC3 spelling; our declaration took `int *` and was changed to match).

### 4.7 A lesson about scatter-includes

`ExternalMic.cpp`'s `#include "bandobj/OutfitConfig.cpp"` (`1a203fb8`, "COMDAT-
scatter sweep") was credited with pairing `??2OutfitConfig@@SAPAXI@Z` — which
turned out to be `Voice::Start`. Removing the include on that basis measured
**−92 B**: it also supplies `??$_M_allocate_and_copy@PBH@…` (60 B) and a
funclet twin at 0x82B6779C (32 B) that retail's ExternalMic TU genuinely emits.
Restored with a corrected rationale. The general point: a scatter-include's
*stated* reason can be wrong while the include is still load-bearing — measure
the removal, do not reason it away.

---

## 5. Predicted vs measured (every run `tools/ab_measure.py --from-dirty`, settled both legs, ruler `name_check`, objdiff-cli `a5c35b15d7d46ac4`)

Whole-lane total, composed from the four measured legs (deltas compose; absolutes do not): **+15 matched functions, +1,608 B matched_code, +0.015693 pp** (42,439 → 42,454; 37.293660 → 37.309353), honest 19,515 → 19,526 (+11). Voice unit 42/80 → 52/89 (the denominator grew by the nine re-homed rows).

| run | change | predicted | measured | rows |
|---|---|---|---|---|
| 1 `99fa7de5` | `pack(1)` on `XAUDIO2_SEND_DESCRIPTOR` | +3..5 fns / +292..1,280 B | **+5 fns / +736 B / honest +4** (37.293660→37.300842) | `__uninitialized_copy` 47.1→100 (84 B), `__uninitialized_fill_n` 40.8→100 (80), `push_back` 53.8→100 (128), `_M_insert_overflow_aux` 93.8→100 (400), `fn_82B5CF50` 99.9→100 (44); unit `synth_xbox/Synth` 145→150/169; nothing else moved |
| 2a | (REFUSED — no verdict) same patch, first attempt | — | split-guard: dtk re-derived `.pdata` for the moved `.text` pins and rewrote splits.txt, so the input was not a fixed point; the tool restored the tree | — |
| 2b | Voice identity repair, first measurement (§4.1–4.4 + the thirteen names, scatter-include removed) | certain +2 fns / +36 B; likely `Stop` +112, `Start` +8; ten pairing bets; range +2..+15 fns / +36..+2,900 B | **Δmatched −1 / Δhonest +1 / Δcode +80 B** (37.300842→37.301624); Voice 42→46, StreakMeter 164→165, ExternalMic 20→15, StreamReceiver360 24→23 | +: `CombineMultipliers` 28, `SetReverbEnable` 97.6/28 B→100/8 B, `Start` 8, `Stop` 112, funclet `fn_82B66600` 99.5→100 (40); −: `_M_allocate_and_copy<PBH>` 100→0 (60) and funclet `fn_82B6779C` 100→0 (32) — both supplied by the OutfitConfig scatter-include (§4.7); `??2OutfitConfig` gone (8); `PlayImpl` 100→98.8 (16, §4.5). Ten Voice bodies now visible as partial rows: ctor 66.6, GetAddr 43.9, HasPendingVoices 77.8, Init 89.1, IsPlaying 86.4, SetData 21.9, SetSpeed 93.8, thread entry 56.6, blockingStart 46.3, createOrReuse 0.0 (no body) |
| 2c `c7f16aba`..`9bc2b2c3` | + scatter-include restored, `GetState` one-arg, `IsPlaying` retail shape, `createOrReuse` written, `dispose` ported + re-homed from Lit.cpp, `PlayImpl`/`GetPlayCursor` swapped, `CreateFx` | vs run-2's leg A: +4..+6 fns / +724..+972 B | **Δmatched +7 / Δhonest +6 / Δcode +584 B** (37.300842→37.306545); Voice 42→49, StreakMeter +1, StreamReceiver360 +1, FxSendReverb +1, ExternalMic 20→17 (re-homed anonymous rows + the mis-named 8 B; no real row lost) | +: `IsPlaying` 260, `Stop` 112, `CombineMultipliers` 28, `SetReverbEnable` 8, `Start` 8, `GetPlayCursor` 16 (98.8→100), `CreateFx` 40, funclets `fn_82B64F10` 99.8→100 / `fn_82B64F60` 0→100 / `fn_82B66600`→100 (120); −: `??2OutfitConfig` 8. Sum = +584 exactly. Below the byte prediction because `createOrReuse` read **90.5** (the if/else + swapped-declaration variant was *worse* than the plain 98.0 form) and `dispose` **89.1** (bare `Enter()/Exit()` where retail has a `CritSecTracker`). Functions above prediction because three funclets crossed with their parents |
| 2d | `dispose`: `CritSecTracker` on `gVoiceGC`, `[1]++` before `[0]--`; `createOrReuse`: back to the expression store + original declaration order, tracker kept | +1 fn / +208 B (`dispose` 52/52 equal by `run_objdiff`; `createOrReuse` 99.9 canonical, 3 instructions — 0x58/0x68 slot swap + one `ble`/`bgt` — no bytes) | **Δmatched +3 / Δhonest +1 / Δcode +288 B** (37.306545→37.309353); Voice 49→52 | `dispose` 89.1→100 (208), `createOrReuse` 90.5→99.9 (0 B), funclets `fn_82B662C0` / `fn_82B666A4` 0→100 (40 each — the tracker's cleanup twins, not predicted) |

---

## 6. What this lane did NOT do

* Did not add `xaudio2fx.h`, `ReverbConvertI3DL2ToNative` (XDK inline, 740 B)
  or `FxSendReverb360::SyncEffectParams` — the struct sizes are recorded in
  §1.12 for whoever does; the `sslgen.c` mis-pin is a splits-lane handoff.
* Did not touch `?QueryInterface@CXAPOBase@@` / `?Release@CXAPOBase@@`
  (XDK library bodies) or add `__declspec(uuid)`s (RB3 has no `m_regProps`
  initialisers in its map — different situation from DC3).
* Did not delete `Sound.cpp`/`ThreeDSound.*` (merge owner decides, §3).
* Did not claim any width for MicXbox's `0x90xx` fields (census-blind) or
  for the `synth/` fields retail never touches (§2.2 list).
* Did not audit `StreamReceiver360`'s three anonymous 0% rows
  (`fn_82B6BD68` 508 B, `fn_82B6BF90` 248 B, `fn_82B6C240` 108 B) or
  `Mic`/`ExternalMic`'s beyond the Voice spill-over; `Voice`'s far tail block
  `0x82C42958–0x82C436F0` (39 rows, mostly 144-B STLport COMDATs) was left
  alone.
* Did not close `createOrReuse` past 99.9 canonical: the two effect-chain
  locals sit in each other's slots (0x58/0x68) and the `unk4b` store branches
  `bgt` where retail branches `ble`. Declaration order was measured **inert**
  for the slot swap (as `MSVC_X360_REGALLOC.md`'s correction predicts), and
  the if/else spelling of the store made the whole function worse (98.0 →
  90.5) by growing the prologue to r23.
* Did not close `SetSpeed` (93.8, a two-instruction scheduling residual around
  the min-speed pointer trick), `Pause` (96.6), or the Voice rows that are now
  honest partials (ctor, `Init`, `blockingStart`, `StartVoiceThreadEntry`,
  `SetData`, `GetAddr`, `HasPendingVoices`) — body-port work for a lane that
  owns Voice.cpp; they were invisible 0%s before this lane and are targets now.
* Did not run `permuter`; every change here is a retail-bytes correction.
