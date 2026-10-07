# W16-TR: callers for the seven files rb3-render discarded

Lane W16-TR, 2026-10-07. Worktree `~/tmp/wt-w16tr`, branch `w16-tr`, base main `5b9a8f56c`.

W16-TM (`W16TM_VIA_DC3_NATIVE_LINK_2026-10-07.md` §3) linked seven VIA-DC3 files into `rb3-render`.
The linker then discarded them whole, because nothing in the target called them:
MetaMusic, MidiSynth, StorePanel, FlowValueCase, FlowOnStop, HAQManager and DeJitterPanel.
This lane gives each one a caller, or records why it cannot have one.
Every caller is a gate that checks behaviour against retail and fails when the code under test is
broken.

The short version:
- **Four now run under gates:** MetaMusic, MidiSynth, StorePanel and DeJitterPanel.
  - There are 16 new gates in `native/src/w16tr_phase.cpp` (`--no-w16tr` skips them).
  - All 16 pass. Each was shown to fail under sabotage (§5).
- **Three cannot be gated against retail, because retail RB3 does not contain them:** FlowValueCase,
  FlowOnStop and HAQManager. The same evidence shows that retail has **no Flow system at all**,
  which bears on W16-TM's FlowIf and FlowSwitchCase gates (§2).
- **One native bug was found by a gate and fixed.** `Synth::NewBufStream` swapped the start time and
  the buffer length (§4.1).
- **Native StorePanel now dispatches the messages retail does.** The DC3-era native Handle arms for the
  two enum-complete messages are removed (§4.2).
- **X360:** both `src/` edits are native-only or comments. The A/B is in §7.

## 1. Disposition

`kept/own` counts the TU's own strong text symbols (`nm` `T`/`t`) that survive into the final
`rb3-render`, with W16-TM's measure. Before = W16-TM §3. After = this lane's final binary
(`~/tmp/w16tr_kept.json`).

| file | before | after | disposition |
|---|---:|---:|---|
| synth/MetaMusic.cpp | 2/25 | 24/25 | RUN: 8 `metamusic-*` gates. Its retail caller is MetaPanel; the gates drive it on shipped moggs and the shipped config. |
| synth/MidiSynth.cpp | 2/10 | 10/10 | RUN: `midisynth-ctor`, plus the Mic.cpp code native emits from this TU (`midisynth-ringbuffer`, `midisynth-mic-set`). |
| meta/StorePanel.cpp | 0/41 | 36/41 | RUN: 3 `storepanel-*` gates through a probe subclass. Two native fixes (§4.2, §4.3). |
| meta/DeJitterPanel.cpp | 0/13 | 12/13 | RUN: `dejitter-panel-enter`, `dejitter-panel-poll`. |
| flow/FlowValueCase.cpp | 0/17 | 0/17 | **No gate: not in retail RB3** (§2). |
| flow/FlowOnStop.cpp | 0/24 | 0/24 | **No gate: not in retail RB3** (§2). |
| meta/HAQManager.cpp | 0/23 | 0/23 | **No gate: not in retail RB3** (§2). |

Newly linked for StorePanel: `meta/StreamPlayer.cpp` (12/12 kept) and `meta/Sorting.cpp` (3/4).

## 2. FlowValueCase, FlowOnStop and HAQManager are not in retail RB3

The instrument is retail `band.exe` (clean TU5), scanned in Python, because the shell `grep` cannot
match binary files. It looks for two things:
- **MSVC RTTI type descriptors** (`.?AV<Class>@@`). Retail is built `/GR`, so every polymorphic class
  linked into the image has one.
- **The class's own strings**: the class name, its handler symbols and its log formats.

The controls are classes retail is known to contain. Each one is found:

| class | RTTI | name occurrences |
|---|---|---:|
| Fader | `0xc5ae68` | 17 |
| MidiInstrument | `0xc63370` | 3 |
| StreamNull | `0xc63c54` | 1 |
| UIPanel | `0xc52634` | 2 |
| StoreOffer | `0xc6016c` | 3 |
| MetaMusic | `0xc63b7c` | 6 |
| DeJitterPanel | `0xc6189c` | 2 |
| StorePanel | `0xc61bc4` | 5 |

The three classes in question are not found:

| class | RTTI | name | its strings |
|---|---|---:|---|
| FlowValueCase | none | 0 | `Key frame value` 0 |
| FlowOnStop | none | 0 | `kRequestStopOnly` 0, `Deactivated, which can cause` 0, `Timed Release From Parent` 0 |
| HAQManager | none | 0 | `haq_mgr` 0, `HAQ_` 0, `toggle_enabled` 0, `display_all` 0, `raw_print` 0, `<NONE>` 0 |

**MidiSynth is not a counterexample.** It has no RTTI because it has no virtual functions. Its ctor is
matched at 100% (116 B).

**The whole Flow system is absent, not just these two nodes.** FlowNode, FlowIf, FlowSwitchCase,
FlowManager and FlowSound all have no RTTI and zero name occurrences, and so does FlowIf's
`use_last_value`. The report agrees:
- None of the 11 `default/Flow*` units holds a function whose name contains `Flow`.
- The only near hit is one `__linear_insert<FlowNode **>` template name in FlowSlider.
- Their rows are unnamed 32–40 B funclets, pinned under those file names.

HAQManager is consistent with this: `MetaPanel.cpp` already says its HAQManager "exists only in
HX_NATIVE". So all three files are DC3/dev-era code, and no retail behaviour exists to check them
against. They are left compiled and discarded, as W16-TM left them. A caller written for them could
only check DC3's own text against itself.

⚠ **Consequence for W16-TM, not acted on here.** Its `flow-if-operators`,
`flow-switchcase-operators` and `flow-switchcase-transition` gates drive code that retail RB3 does not
contain. Their references are plain C++ comparison semantics, so they are valid as tests of the DC3
text. They are not tests against retail. W16-TM's §6.3 "FlowValueCase Save/Load asymmetry" likewise
has no retail side to adjudicate it.

## 3. Gates (phase `W16-TR`, `native/src/w16tr_phase.cpp`)

The phase runs after W16-TM in rb3-render's default mode. It reuses the Synth that W16-TM installs,
and installs its own if `--no-w16tm` left none.

### MetaMusic

MetaMusic is driven the way `MetaPanel` drives it:
- `MetaPanel::Load` → `Load("sfx/streams/<name>", vol, true, true)`.
- `PollForLoading` → `Poll()` until `Loaded()`.
- `FinishLoad` → `AddFader`.
- The scene change → `Start`.

The fx dir is null (§6). The stream is the real one, `Synth::NewBufStream` → StandardStream →
VorbisReader. The receivers capture samples, volume and pan.

| gate | reference | measured |
|---|---|---|
| metamusic-load | shipped `config/synth.dta`: volume −20, fade_time 1.0, play_from_memory TRUE. The loaded buffer is byte-equal to the `.mogg` read directly. | 2,180,893 B equal; −20 / 1.0 / 1; loader and file released |
| metamusic-fade-in | Poll on a ready stream: SetVal(−96), DoFade(volume, fade_time), Play. The fader follows −96 + 76·t/1000 and lands exactly on −20. Receivers get 10^((−20−3)/20) = 0.07079 (the extra fader is at −3). | at 452 ms −61.62 (line −63.56…−61.62); end −20; 6/6 receivers at 0.07079 |
| metamusic-mute | `{mute}` through Handle: mute fader → −96. UnMute → 0. | match |
| metamusic-stop | Stop fades to −96 over fade_time. Poll releases the stream once the fade has ended at −96. | released at 1,005 ms; pollables 1 → 0 |
| metamusic-loop | MetaPanel passes loop=true (`SetJump(kStreamEndMs, 0)`). Pass 1 equals a decode from 0 through the file path, and pass 2 starts over from sample 0. | 160,089 samples/ch, not finished; 0 samples differ in pass 1 (53,363/ch) and in pass 2's first 4,096/ch |
| metamusic-pan-wide | UpdateMix with no fx dir, 2 channels, Load's third arg set: pans −2/+2 on the stream and on the receivers. | match |
| metamusic-pan-narrow | the same with the arg clear: −1/+1. | match |
| metamusic-start-point | `(start_points_ms 500)` injected (retail `MetaMusic::Load`, 100%, reads it; the shipped config has none). The decode begins at sample 22,050 of a decode from 0. A `fade_time` set to 0.25 for this load is read. | ChooseStartMs 500; stream time 500 ms; 31,313 samples/ch, 0 differ; fade 0.25 |

The decode-from-0 reference is W16-TM's mogg path. W16-TM checked it against ffmpeg (sync_clap:
53,363 samples, ≤2 LSB).

`fade_time` needed its own injection. The shipped 1.0 equals MetaMusic's ctor default, so
`metamusic-load` cannot see whether Load reads it. Sabotage A1 confirms this: dropping the read leaves
`metamusic-load` passing and fails `metamusic-start-point`.

### DeJitterPanel

The panel's own `enter`/`poll` type-def handlers call a registered DataFunc. The DataFunc records
`TheTaskMgr.Seconds(kRealTime)` and `DeltaSeconds()`, so the TaskMgr time is observed while the
`DeJitterSetter` scope is active. Before each call the gate sets a known outside time, which the
scope must restore.

| gate | reference | measured |
|---|---|---|
| dejitter-panel-enter | Enter → DeJitterSetter(no timer): seconds/delta 0/0 inside, outside time (123.5, 0.25) restored exactly | match |
| dejitter-panel-poll | 40 frames ~5 ms apart. Frame 0 primes and reports 0/0. Later frames report the dejittered split: non-decreasing, delta = output − previous output, within 16 ms of the raw clock (DeJitter's clamp). The outside time is restored on every frame. | 39/39/39, worst 0.09 ms; 40/40 restored |

### StorePanel

A probe subclass supplies the platform virtuals that BandStorePanel would (`MakeNewOffer` builds a
real `StoreOffer` from a DataArray). The offers come from DataArrays, with ids parsed by
`OfferStringToID`.

| gate | reference | measured |
|---|---|---|
| storepanel-populate | Behaviour of the retail-matched PopulateOffers (100%):<br>• `load_ok` 0 → nothing;<br>• `load_ok` set through SyncProperty → 2 live offers, test offer hidden, 64-bit id / album / pack parsed;<br>• the pending fill keeps the live list;<br>• test offers shown → 3. | match |
| storepanel-update-offers | Behaviour of the retail-matched UpdateOffers (100%):<br>• a listed offer gets purchased/cost/available, and so does its album;<br>• the pack (not listed) and an unlisted offer are untouched;<br>• an unlisted test offer gets cost 9999;<br>• nothing matched → NoContent (1), empty list → SignedOut (6), test offers shown → Success (0);<br>• `operator==(EnumProduct, StorePurchaseable)`. | match |
| storepanel-source | `set_source` through Handle: only `backup=1` sets the backup; `set_source_to_backup` | match |

### MidiSynth

| gate | reference | measured |
|---|---|---|
| midisynth-ctor | retail ctor (100%): `mChannels.resize(16)` of default MidiChannels (volume 1.0) | 16, all default |
| midisynth-ringbuffer | RingBuffer (Mic.cpp, emitted from MidiSynth's TU) against a model: a FIFO of ≤ size bytes that drops the oldest on overflow, where Write returns the bytes dropped (negative while there is room) and Peek returns the newest `len` written. 3,000 random ops on a 1,000 B ring. | 0 differ |
| midisynth-mic-set | `Mic::Set` on `(gain 0.75) (dma 1) (compressor 1 0.3)` | match |

### Full run

Final binary, `~/tmp/w16tr_run4.log`:
- `RESULT: ALL GATES PASSED`.
- 144 `[PASS]`, against 128 on this lane's base binary. The difference is exactly the 16 above.
- The 302-line `FAIL:` multiset is **identical** to the base binary's, so this lane adds none.
- Wall time 4.0 s → 6.7 s, nearly all of it the two real-time 1 s fades (fade-in and Stop).

## 4. Source edits

### 4.1 `Synth::NewBufStream` passes the start time as the start time (native)

The StandardStream ctor is `(File*, startMs, bufSecs, ext, floatSamples, pollingEnabled)`. Native
`Synth::NewBufStream` built `StandardStream(file, 0, f1, …)`, which had two effects:
- MetaMusic's chosen start point became the buffer length in seconds.
- The stream always started at 0.

Retail `Synth360::NewBufStream` (176 B, 100%) builds `StandardStream(BufFile, startMs, 0.0f, …)`, and
native now does the same. Native `NewStream` was already in the right order.

Nothing native had hit this, because every caller passes 0 and the shipped config has no
`start_points_ms`. Natural control, on the unfixed binary:

```
[FAIL] metamusic-start-point — ChooseStartMs 500 (want 500); stream time 0 ms after Play; 53363 samples/ch (want 53363 - 22050 = 31313), -1 differ ...
```

This was predicted before the run. After the fix the gate passes with 0 differing samples.

### 4.2 StorePanel::Handle dispatches what retail does (native)

Native carried `HANDLE_MESSAGE(SingleItemEnumCompleteMsg)` and `HANDLE_MESSAGE(MultipleItemsEnumCompleteMsg)`
under `HX_NATIVE`. The file's own comment says retail's Handle (`0x827B5510`, 1,192 B, 100%) has no arm
for either message. Both arms are removed.

Their only sender is the Marketplace backend, which native does not have. Removing them also drops
the link's need for `SingleItemEnumCompleteMsg::OfferID`, which `os/PlatformMgr_Xbox.cpp` defines.

The DC3-era `OnMsg` bodies for these two messages stay in the file, unreferenced. They have no retail
rows.

### 4.3 Link closure of a live StorePanel (native only)

The live StorePanel vtable reaches four things:
- **StreamPlayer** (StorePreviewMgr's previews) and **FirstSortChar** (StoreOffer::FirstChar):
  portable, so `meta/StreamPlayer.cpp` and `meta/Sorting.cpp` are now linked
  (`W16TR_DEP_SOURCES`).
- **XboxEnumeration** (EnumerateOffers) and **XboxPurchaser** (CheckOut): the Xbox Live Marketplace
  backend that W16-TM replaced by rule.
  - `native/src/w16tr_link_stubs.cpp` defines their ctors and every virtual.
  - Each one prints its name and aborts, following the `bandtrack_link_stubs.cpp` convention. A stub
    that returned an object would silently stand in for the platform.
  - No rb3-render path reaches them.

### 4.4 A gate that crashed instead of failing (lane's own code)

Sabotage round B left PopulateOffers with two offers, and `storepanel-update-offers` then indexed
`mOffers[2]` (rc 134). It now reports a failure, and so does `storepanel-source` after it
(commit `W16-TR: update-offers fails instead of indexing a missing offer`).

## 5. Sabotage controls

Each round was applied to the code under test, built, run and reverted. The predictions were written
before each run.

| round | sabotage | predicted to fail | result |
|---|---|---|---|
| natural | `NewBufStream` unfixed | start-point | start-point only |
| A | A1 Load drops `fade_time`; A2 Poll releases at −95, not −96; A3 UpdateMix `unk8c` inverted; A4 test-offer cost 999; A5 SetSource `backup` inverted; A6 RingBuffer::Write keeps `mReadIx` on overwrite; A7 MidiSynth `resize(15)`; A8 ~DeJitterSetter restores nothing | start-point, stop, pan-wide, pan-narrow, update-offers, source, ringbuffer, ctor, dejitter-enter, dejitter-poll (10); the other 6 pass | **exactly those 10**; the 6 passed |
| B | B1 Start drops the loop `SetJump`; B2 DeJitterSetter drops `* 0.001f`; B3 PopulateOffers test filter inverted; B4 Mic::Set inverts `dma`; B5 AddFader stores nothing; B6 Mute fades to −90 | loop, dejitter-poll, populate, mic-set, fade-in, mute (6) | those 6, **plus** update-offers and source (they depend on populate's offers; first run crashed, §4.4), **plus metamusic-stop** (not predicted) |
| C | B1 alone | loop, and stop if B1 caused it | **exactly loop and stop** |

Round C settles the unpredicted stop failure. Without the loop, the capture receivers decode faster
than real time, and the shellmusic stream reaches `kFinished` during the stop fade. Retail `Poll`
(596 B, 100%) releases only a stream that `IsPlaying()`, so a finished non-looping stream is never
released. That is retail behaviour, not a gate defect. It means `metamusic-stop` relies on the
looping configuration that MetaPanel uses.

## 6. Findings, not changed

- **MetaMusic's fx path is not gated.**
  - `MetaPanel` constructs `MetaMusic("sfx/shell_fx.milo")`, which makes Start load that milo six
    times and route each channel through its `eq.send`.
  - SetScene's mixes come from `MetaMusicScene`, which rb3-render does not compile.
  - The gates use the null fx dir, which is the other arm of the same retail functions.
  - UpdateMix's scene branch (1,572 B, 99.85%) is therefore not driven.
- **Native PopulateOffers still calls `ValidateOffers` (`#ifdef HX_NATIVE`).**
  - The comment in place says retail has no such call, and retail PopulateOffers is 100% matched
    without it.
  - Its only effects are notifies, so no gate can see it. It is left as is.
  - Also `ValidateOffers` reads `OfferType()`, which requires a `type` field. Native would therefore
    fail on a `type`-less offer that retail accepts.
- **W16-TM's note that `stream_buf_size` is not in the preinit config is wrong.**
  - rb3-render's config has it (1.2), measured by this phase.
  - It comes from `system/run/config/default.dta`, which `band_preinit_keep.dta` `#merge`s.

## 7. X360 A/B

Command: `tools/ab_measure.py --worktree ~/tmp/wt-w16tr-ab --patch ~/tmp/w16tr_src.patch --jobs 8`.
The patch is `git diff 5b9a8f56c..HEAD -- src/` (StorePanel.cpp, Synth.cpp), applied to main
`2394ddee8`. Run dir `~/tmp/wt-w16tr-ab/.ab_measure_runs/20261007-124421-w16tr-2351191`, ruler
`name_check`.

```
leg A: matched=54938 masked=25214 honest=29724 code%=59.307816  (recompiles: 0, settled)
leg B: matched=54938 masked=25214 honest=29724 code%=59.307816  (recompiles: 6, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   (legA 64.316590 -> legB 64.316590)
units at 100% [mpn ruler]: legA 626 -> legB 626  (Δ+0; 0 reached 100, 0 fell off; pairable units 1753->1753)
units at 100% [all-rows-fuzzy ruler]: legA 547 -> legB 547  (Δ+0; 0 reached 100, 0 fell off; pairable units 1753->1753)
[control none] Δmatched_code=+0 B Δcode%=+0.000000 (default ruler +0 B)
```

The prediction was Δ0:
- Synth.cpp's edit is inside `#ifdef HX_NATIVE`.
- StorePanel.cpp's removed lines were inside `#ifdef HX_NATIVE`, and what replaced them is comments.
- The line shifts are inert: no macro on those paths expands `__LINE__`, and X360 `MILO_*` is
  `((void)0)`.

Leg B recompiled 6 TUs, so the patch was compiled and the run is not absent-vs-absent.

## 8. Native gates

`tools/native_health.sh ~/tmp/wt-w16tr` on the final source (`~/tmp/w16tr_health1.log`), with its
own lines:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=181 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=13 scatter_dirb=0 scatter_multihost=13 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
LAYOUT_ODR_RESULT verdict=PASS x360_tus=1267 x360_failed=0 x360_split=0 x360_unresolved=0 x360_allowed=138 x360_stale=0 native_tus=1916 native_failed=0 native_split=0 native_unresolved=0 native_allowed=2 native_stale=0 rc=0
```

Compared with W16-TM §8:
- `gates_pass` 165 → 181: the 16 W16-TR gates.
- `native_tus` 1,912 → 1,916: the phase, the link stubs, StreamPlayer and Sorting.
- The scatter counts are unchanged.

The lane's last action is `tools/native_build_gate.sh`, run after the final commit. Its result line
is in the lane report.
