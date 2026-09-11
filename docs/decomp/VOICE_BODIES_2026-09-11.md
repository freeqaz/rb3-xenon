# Voice.cpp bodies from retail (lane W4-D, 2026-09-11)

**Branch** `w4-voice-bodies` (worktree `~/tmp/wt-w4-d`, base `3ab3f494`, rebased onto
main before landing). Follows lane W3-D's `XAUDIO_LAYOUT_AUDIT_2026-09-11.md`, which
re-homed Voice.cpp's pin and left ten partial rows visible. Every retail figure here
was read off `build/45410914/asm/*.s` keyed on the `.fn fn_<addr>` symbol, never the
synthetic address column; our-side offsets are the compiler's
(`scripts/harvest/class_layout_report.py Voice`, `sizeof 0x74`, all offsets as in
`Voice.h`). Percentages are the graded ruler (`name_check`, `report.json`).

**Standing correction (same as W3-D's):** `native/CMakeLists.txt` excludes all of
`synth_xbox/`, so nothing here pays as native impact. It pays in X360 bytes and in
accuracy — and several of the corrections below are behavioural bugs in this tree's
source, not just matching.

**Denominator note:** the brief said Voice was 52/89 after W3-D; at `3ab3f494` it
reads **52/91** (two rows joined the unit between W3-D's measurement and this base).

---

## 1. Per-row table

Sizes are `report.json` sizes. "Before" = at `3ab3f494`; "after" = at the lane tip.
Each run was `tools/ab_measure.py --from-dirty`, both legs settled and at a split fixed
point; predictions were written down before the run (§3).

| row | size | before | after | what changed (retail address) | retail vs dc3 |
|---|---:|---:|---:|---|---|
| `??0Voice@@QAA@_N00@Z` (was `_NH0`) | 296 | 66.6 | **100** | signature `(bool xma, bool synchronized, bool stereo)`; `gEvent = INVALID_HANDLE_VALUE` in `.data`; CreateThread's handle is a local; no `MILO_ASSERT` code (0x82B66B20) | dc3 takes `(bool, int channels, bool)` and stores `gVoiceThread`; retail stores r5→`mSynchronized`, r6→`mStereo` as bytes with no compare, and all four callers pass bools |
| `?GetAddr@Voice@@QAAHXZ` | 208 | 47.0 | **100** | mono byte address `(addr % (mAudioBytes/2)) << 1` into a temp, then if `mStereo`: `addr %= mAudioBytes/4`, `addr << 2`; unsigned `divwu` + `twllei`, `SamplesPlayed` truncated `clrrwi 0` (0x82B65160) | dc3 multiplies by `mChannels` |
| `?HasPendingVoices@Voice@@SA_NXZ` | 148 | 77.8 | **100** | `CritSecTracker` + `size()+size() != 0` (0x82B66038) | dc3 has a `gShutdownVoiceThread` early-out; RB3 has no such flag |
| `?blockingStart@Voice@@QAAX_N@Z` | 116 | 46.3 | **100** | tracker scope, `Init`, `Start(0, mSynchronized)`, `mState = 3` (0x82B66630) | dc3 guards on the shutdown flag and `OutputVoice()`; retail has neither |
| `?SetData@Voice@@QAAXPBXHH@Z` | 64 | 21.9 | **100** | `bytes/2`, halved again if `mStereo` (0x82B64D20) | dc3 divides by `mChannels` |
| `fn_82B66AA4/ACC/AF4` (thread-entry funclets) | 3×40 | 0 | **100** | the three `CritSecTracker` dtors at `r31+0x58/0x5c/0x60` | — |
| `?Init@Voice@@QAAX_N@Z` | 600 | 89.1 | **100** | no `OutputVoice()` early-out; dry send is `mFxSend ? mFxSend->unk4 : 0` with no mastering fallback; `pSends = data` unconditionally before the count; tail `if (GetVoice()) GetVoice()->SetFrequencyRatio(mSpeed, 0)` with inline pointer-returning `GetVoice()` (0x82B663A8) | dc3 has the early-out and the fallback (`unkf0`) and a `count ? data : 0` ternary |
| `?createOrReuse@Voice@@…` | 312 | 99.9 | **100** | `unk4b = (sends == 0 \|\| sends->SendCount > 0)`; descriptor as aggregate `{eg, 0, 1}` then the chain assigned; `MemDoTempAllocations` guard object (0x82B64DD8) | dc3's `unk54` has the SAME polarity — W3-D's `<= 0` was the inversion; dc3 has an `OutputVoice()` early-out and an hr-failure arm retail lacks |
| `?Pause@Voice@@QAAX_N@Z` | 232 | 96.6 | **100** | `Stop` through the real `IXAudio2SourceVoice` interface (0x82B65428) | — |
| `?SetSpeed@Voice@@QAAXM@Z` | 136 | 93.8 | **100** | `speed > min ? &speed : &min` (select polarity) (0x82B658B8) | — |
| `?StartVoiceThreadEntry@@YAKPAX@Z` | 972 | 56.6 | 99.69 | `for(;;)`; drains ≤ **2** per wake-up; 64-bit tick age with wrap fix-up; 2-arg `PoolFree`; three trackers; `delete` of the EG; two adjacent **static scalar** counters; `Synth360 *synth = TheXboxSynth` local (0x82B666D8) | dc3: `while(!gShutdownVoiceThread)`, ≤ 4 per pass, 32-bit age, `int[2]` counters |
| `fn_82B64F38` (createOrReuse funclet) | 40 | 93.4 | **100** | destroys the `MemDoTempAllocations` guard at `r31+0x6c`; needed the map repair of `0x82345030` | — |
| `?Stop@Voice@@QAAXXZ` (was `_N`) | 112 | 100 | 100 | no-arg: `SlipStop` (0x82B6C240) calls it with r4 never set | dc3 `Stop(bool immediate)` with an immediate arm |
| `?GetSlipOffset@StreamReceiver360@@` | 248 | 0 (anon) | **100** | named `0x82b6bf90`; buffer stride `mNumBufs * 0xC000` | dc3 `<< 14` |
| `?StartSendImpl@StreamReceiver360@@` | 32 | 0 (anon) | **100** | named `0x82b6bb08`; `idx * 0xC000` | dc3 `<< 14` |
| `fn_82B6BF64` (SetSlipOffset funclet) | 40 | 0 | **100** | crossed with the naming | — |
| `?SetSlipOffset@StreamReceiver360@@` | 508 | 0 (anon) | 97.28 | named `0x82b6bd68`; `new Voice(false, true, false)` (0x74, not `PoolAlloc(0x7c …)`); `* 0xC000` | residue: `startSamp` lives in r10 then `mr r4` (liveness), and `SetStartSamp`'s callee name (repaired in C4; the r10 shape remains) |
| `?SlipStop@StreamReceiver360@@` | 108 | 0 (anon) | 99.81 | named `0x82b6c240`; `Stop()` | residue: `list<Voice*>::insert` → retail's folded `list<Hmx::Object*>::insert`; the fold gate REFUSES one level down (§4) |
| SampleInst360 `StartImpl`/`StopImpl`/`IsPlaying`/`SetSpeedImpl` | 4×8 | 0 (anon) | **100** | named `0x82b6e100/108/118/130`; `IsPlaying` now forwards to the voice | this tree's `IsPlaying() { return false; }` was a real bug |
| map `0x82345030` | 4 | "100" (false) | 0 | `??3BandHighlight@@SAXPAX@Z` → `??1MemDoTempAllocations@@QAA@XZ` (body `b fn_827BC2A0`; 67 funclet callers in 30 units) | the false 100 was a `b <placeholder>` pairing with any `b <x>` |
| map `0x82774068` | 4 | 95 | 0 | `??1MemDoTempAllocations` nulled — the body is `b fn_82773FB0`, an unrelated SongData dtor | — |
| map `0x827bc2a0` | — | — | — | `?MemPopTemp@@YAXXZ`: decrements the per-thread temp-alloc refcount at `+0x44`, mirror of `MemPushTemp` at `0x827BC270` | — |
| map `0x8280e248` | 8 | — | — | `??0?$reverse_iterator@PAH@…` → `?SetStartSamp@Voice@@QAAXH@Z`: retail body `stw r4,0x18(r3); blr` is byte-identical to ours (fold-gate comparator, 2/2 words) and impossible for a reverse_iterator ctor (stores at +0) | — |

Voice unit: **52/91 → 65/91**, `matched_code` 5,384 → 7,696 B of 11,488 (in-worktree at the
tip; the tail block `0x82C42958–0x82C436F0` is the `.text$yc` dynamic-initialiser cluster —
see §5 — and is not Voice.cpp's to close). StreamReceiver360: 25/31 → 28/31.

## 2. Behavioural differences retail vs dc3 (one-line mechanisms)

1. **Voice ctor is `(xma, synchronized, stereo)`** — r5/r6 stored as bytes to 0x49/0x4a, no
   `i > 1`; song-stream voices (`StreamReceiver360`, `(0,1,0)`) are the *synchronized* ones,
   which is what `gPendingSyncVoices` exists for; `mStereo` is never set by any retail caller.
2. **`gEvent` starts at `INVALID_HANDLE_VALUE`** — the ctor's `cmpwi r11, -1` gate; with this
   tree's zero-initialised `gEvent` the voice thread was never created.
3. **No `gShutdownVoiceThread` / `TerminateVoiceThread` / `gVoiceThread`** — no byte global in
   `0x82E120C4..C7` other than `gHasPendingStopCommits`/`gCommitSyncVoices`/`gWasCommitSyncVoices`
   is referenced anywhere in Voice.s; the thread loop is `for(;;)`.
4. **GC drains ≤ 2 voices per wake-up** (`cmpwi cr6, r30, 2`), not 4, and ages them in 64-bit
   with `if (age < 0) age += 1<<32` (`rldicl` / `cmpdi` / `rldicr r12,r12,32,63`).
5. **`unk4b = (sends == 0 || SendCount > 0)`** — "has some output"; W3-D's `<= 0` made
   `dispose()` detach exactly the voices with nothing to detach from.
6. **`Voice::Stop()` takes no argument** — `SlipStop` calls it with r4 unset.
7. **`Init` has no mastering-voice fallback and no `OutputVoice()` early-out** — a voice with
   no FxSend gets no explicit send (XAudio2 default-routes it).
8. **StreamReceiver360 buffers are 0xC000 bytes** (`mullw` by `0xc000`), not 16 KB.
9. **`SampleInst360::IsPlaying` forwards to the voice.**

## 3. Predicted vs measured

| run | change | predicted | measured |
|---|---|---|---|
| C1 `db9a7085` | ctor (+map), GetAddr, HasPendingVoices, blockingStart, SetData, thread entry v1, globals | +8 fns / +952 B / +0.0093 pp (+0..1 fn on createOrReuse at 0 B) | **+8 / +952 B / +0.009293 pp** (42505/37.426590 → 42513/37.435883, honest +5) |
| C2 `a1eed2ed` | Init, createOrReuse, Pause, SetSpeed, thread entry v2 | +4 fns / +1,280 B / +0.0125 pp | **+5 / +1,280 B / +0.012487 pp** (→ 42518/37.448370; the 5th is a masked-equal funclet twin) |
| C3 `98eaea54` | `Stop()`, SR360/SampleInst360 fixes + 8 names (+map) | +7 fns / +352 B / +0.0034 pp | **+7 / +352 B / +0.003440 pp** (→ 42525/37.451810) |
| C4 `4a087317` | map-only: guard dtor, SongData null, MemPopTemp, SetStartSamp | ≥ +36 B (funclet +40, BandHighlight −4), upside unknown from 67 callers | **+0 fns / +1,964 B / +0.019168 pp** (→ 37.470978); `none` control +40 B = the two real pairing changes; four SongSort units reached all-rows-fuzzy 100 |

Lane total (deltas compose): **+20 matched functions / +4,548 B / +0.044388 pp** over
`3ab3f494` (42,505 / 37.426590 → 42,525 / 37.470978).

## 4. What this lane did NOT do

* **`StartVoiceThreadEntry` past 99.69** — two `bl` sites spell `list<Voice*>::operator=`
  where retail calls the folded `list<Symbol>::operator=` (0x822782D0);
  `tools/comdat_fold_gate.py`'s comparator refuses because retail's branch at `+0x98`
  targets `0x82277270`, which the map does not name (its splice helper). Also a
  scratch-register numbering permutation in the prologue (`r6..r9` ascending vs retail
  descending, same emission order) with an r9/r10/r11 knock-on in the commit block —
  register-only, recorded, not ground.
* **`SlipStop` past 99.81** — `list<Voice*>::insert` vs `list<Hmx::Object*>::insert`
  (0x823D14C0): the comparator refuses at `+0x24` because retail's `_M_create_node`
  survivor is the `CharPollableSorter::Dep` instantiation and that pair is not aliased.
* **`SetSlipOffset` past 97.28** — `startSamp` computed in r10 then `mr r4, r10`.
* **The tail block `0x82C42958–0x82C436F0`** is the `.text$yc` dynamic-initialiser cluster:
  every anonymous row there ends in `bl atexit` (`fn_8282A418`); `fn_82C42958` initialises a
  list at `0x82E11C98` (another synth_xbox TU's global); the twelve 144-B rows each build a
  `0x42c`-byte `XAPO_REGISTRATION_PROPERTIES` (`memset 0x1ec` / `memcpy 0x50` / `memset 0x1b0`,
  `MajorVersion=1, Flags=0x3f, buffer counts=1`) — the `??__E?m_regProps@…` initialisers of
  the twelve XAPO effect TUs that W3-D said the map lacks. Splits + source handoff (needs
  `m_regProps` definitions with `__uuidof`); not Voice.cpp's.
* **`fn_82B64C20`** (12 B, `li r3,0x94; b PoolFree`) is `EnvelopeGenerator::operator delete`
  emitted as a COMDAT in Voice's TU — a naming bet not taken.
* **SampleInst360's `SetReverbMixDb`/`SetReverbEnable` thunks** (0x82B6E190/198) — this tree
  has no such methods; its ctor `0x82B6DFB8` (248 B) takes only the sample in retail (it calls
  the sample's own loop getters); ours takes `(sample, loop, start, end)`.
* Did not pin Voice's `.data` (`sHeadsetTarget`/`gEvent` at `0x82CA69BC/C0`).
* Did not run the permuter; every change is a retail-bytes correction.

## 5. Two instrument notes worth reusing

* **`b <placeholder>` thunks read 100 against any `b <x>`** — `??3BandHighlight@@SAXPAX@Z`
  was a 4-byte 100% on `b fn_827BC2A0`; read the body before believing a 4-byte 100.
* **Assignment order, not declaration order, is the store-order lever — and an aggregate
  initialiser is a third state.** For `createOrReuse` this compiler pulled the first chain
  store to second place under every field-by-field order tried (three) and declaration order
  was inert (W3-D + one more); only `{eg, 0, 1}` kept the three descriptor stores ahead of the
  chain's, emitted in reverse field order.
