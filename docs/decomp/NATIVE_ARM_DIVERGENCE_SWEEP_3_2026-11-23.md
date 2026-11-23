# Native-arm divergence sweep, part 3: flow, midi, track, movie, net and the rest (2026-11-23, lane `narms3`)

This continues `NATIVE_ARM_DIVERGENCE_SWEEP_2026-11-22.md` and
`NATIVE_ARM_DIVERGENCE_SWEEP_2_2026-11-22.md` over the dirs neither covered:
`src/system/{flow,midi,track,movie,moviebink,net,synth_xbox,gesture,rnddx9,dsp}` and
`src/network/net`. The question is the same: where does the native arm of an `HX_NATIVE`
conditional behave differently from retail? Behaviour is the target, not bytes.

## Method

- **Native closure.** A fresh configure of the worktree's `native/` (`~/tmp/narms3/nb`, 5,054
  compile commands), then `clang -MM -MG` over every command (`~/tmp/narms3/mdeps.py`, 0
  failures). That gives every source and header any native target compiles: **193 files** in
  these dirs (flow 61, network/net 35, net 27, gesture 20, midi 15, movie 11, dsp 7, track 6,
  rnddx9 5, synth_xbox 5, moviebink 1). A first try with `ninja -t deps` on the narms2 build
  dir saw only the 1,180 objects that lane had built and missed 38 conditionals (all of
  `Splash.cpp`, `TexMovie.cpp`, `XLSPConnection.cpp`, `SessionJobs_Xbox.h`): the deps log is
  not a closure.
- **Enumeration.** The first lane's `arms.py` over the eleven dirs found 166 `HX_NATIVE`
  conditionals; **82 sit in native-compiled files, 36 of them two-sided** (movie 24, flow 22,
  net 16, midi 7, dsp 4, network/net 4, track 3, gesture 1, rnddx9 1). The other 84 (all of
  gesture's `.cpp`, `synth_xbox`, `moviebink/*.cpp`, `rnddx9/*.cpp`, `SessionJobs_Xbox.cpp`,
  `Movie_Xbox.cpp`) are dead natively and were not judged.
- **Retail reference.** `python3 tools/va_disasm.py` on `band.exe` at the addresses cited
  below. `Flow*` has no retail at all: `band.exe` holds no `FlowNode`, `FlowQueueable` or
  `flow_label` string and `splits.txt` pins no `flow/` unit (Flow is DC3's).

## Fixed (native now runs the retail body)

| site | native vs retail | reachable |
|---|---|---|
| `net/JsonUtils.cpp` `JsonConverter::LoadFromString` | Retail 0x82B82440 allocates the `JsonObject` first and stores whatever `json_tokener_parse` returns, with no test. Native returned `nullptr` when the parse returned NULL (the text `null`), and `DataResultList::Update` (native-compiled, `band3/net_band/DataResults.cpp`) dereferences the result untested. Now the retail body. A malformed text still yields json-c's error pointer in both builds (neither tests `is_error`); unchanged. | online data results only |
| `movie/TexMovie.cpp` `TexMovie::Save` | Retail 0x82746B88 writes the rev from the global at 0x82C77238 (= 5), then `Hmx::Object`, `RndDrawable`, pollable, `mTex`, `mLoop`, the relative file path and `Movie::Save` (0x82745B40). Native wrote DC3's rev 8 and an extra `mIsLocalized` byte, a file the game cannot read. Now rev 5 and no extra byte. `mIsLocalized` has no reader besides the native-only `is_localized` property; the native-only `rev > 5` Load reads stay for DC3 files and never fire on RB3 data. | edit (save) |

## Recorded, not edited (owned by another session)

- `net/SessionJobs_Xbox.h` (`XboxJob::mSuccess`, native `OnCompletion` overrides on
  `StartSessionJob`, `EndSessionJob` and three more): divergent layout and vtables (retail
  allocates `StartSessionJob` at 0x28 bytes, 0x823EF0D0, and the slot-4 `OnCompletion` is
  `Job`'s empty one). The header's only native consumer is `network/net/NetSession_Xbox.cpp`,
  and `SessionJobs_Xbox.cpp` itself is not native-compiled. That is the NetSession /
  game-start sync area another session owns, so it was left alone.
- `network/net/Synchronize.cpp` (`Synchronizable` ctor/dtor skip `TheSyncStore`, and
  `SynchronizeIfDirty` clears the dirty state instead of sending `SyncObjMsg`): deliberate
  offline behaviour, no online session natively. It is the generic sync object, not the
  game-start code, but it sits next to it; left as is.

## Reviewed and left

- **No retail counterpart (DC3-only Flow):** `Flow.cpp` (5 `mTypeProps` pointer-vs-value
  arms, cascade-teardown guard, null `Obj()` guard), `FlowNode.cpp` (teardown guards, `Load`
  `numEntries` abort, null-child return), `FlowQueueable.cpp/.h` (`ObjPtrList` listeners),
  `FlowAnimate.cpp` (null anim finishes), `FlowCommand.cpp` (4 `mTypeProps` arms),
  `FlowSetProperty.cpp` (`RefOwner`). RB3 data has no Flow objects.
- **Robustness shims:** `MidiParserMgr.cpp` (retail 0x827EB4D8 calls `DataReadString` bare;
  native catches a bad-format text event and notifies), `MidiReader.cpp` (retail 0x827EF848
  inlines `ReadNextEventImpl` without try/catch; native fails the reader on an error),
  `Splash.cpp` missing-file skip (retail `MILO_FAIL`s; native skips the TU5-only screen),
  `Splash.cpp` `else if (mCurrentCam)` (retail derefs a null cam), `Splash::Show` skip when a
  milo has neither cam nor movie, `RndCam::ClearCurrent` before the splash dirs are deleted.
- **Diagnostics only:** `DataEventList.cpp` notify (retail compiles it out), `MidiParser.cpp`
  X360-only `MILO_NOTIFY` redefinitions (the X360 forms still evaluate the arguments, so side
  effects match), `PitchDetector.cpp` `dump`/`sDump` and `PD_MEM_ALLOC` file/line form,
  `Splash` timing print, `rnddx9/Rnd.h` live `DX_ASSERT`, `SyncStore::AddSyncObj` duplicate-tag
  `MILO_FAIL` (retail 0x823EA3E0 only push_backs; natively unreachable because the native
  `Synchronizable` never registers), `JsonObject::Str/Int/Double` type asserts (their line
  numbers 0x20/0x2c/0x32 run in step with `Bool`'s 0x26, so they are the dev build's
  asserts, which native keeps live tree-wide).
- **Native platform work:** `Movie.h`/`Movie.cpp` `MovieImpl` forwarding (Bink is 360-only),
  `MovieSys.cpp` `TheMovieSys`, `TexMovie::DrawToTexture` FFmpeg/web arms (rb3-app, built
  without `HX_FFMPEG`, runs the retail body), `Splash` non-threaded path (`mThreaded = false`,
  draw inside `Draw`, 15 s movie cap in place of `ShowNext()`), `TheRnd.mReleaseImmediate`
  named instead of `+0x150`, `IIRFilter.h` alignment spelling, `json-c/config.h`,
  `Skeleton.h` friend, `XLSPConnection.cpp` `s_addr` spellings (4 arms), `TrackDir.cpp`
  X360-only scatter include of `rndobj/Anim.cpp`.
- **Equivalent:** `MidiParser::SyncProperty` and `TexMovie::SyncProperty`
  `SYNC_SUPERCLASS(Hmx::Object)` (retail 0x827E8C08 returns false after `song_name` with no
  chain; natively the chain adds only the native `name`/`type`/`sinks` props of
  `Hmx::Object`, the tree-wide native policy the earlier sweeps left), `TexMovie`
  `is_localized`/`is_empty` props (no RB3 script names them), `MidiParserMgr.h`
  `SetPlaybackEnabled` (LP64 accessor), `TrackWidget::SetType` `CheckValid` (empty) + `Init`
  only under `LOADMGR_EDITMODE`, `Movie::LocalizationTrack`'s `HongKongExceptionMet()` (native
  `System.cpp` returns false, so it is retail's `MoviePanel` lookup at 0x827B0A20).
- **`CharWidgetImp::DrawInstances` `SetMeshForceNoUpdate`:** retail 0x827E4968 tests `Valid()`
  and tail-calls `mText->DrawShowing()`, with no other call. The native call is a compensator
  for the native `RndText` dirty-mesh model: native `UpdateText` marks the mesh dirty instead
  of rebuilding it (retail 0x82459660 rebuilds), and native `DrawShowing` rebuilds dirty meshes
  from the string, which would overwrite the per-instance geometry the widget writes. Removing
  it is only correct together with `Text.cpp`, which another session owns.

## Measurement

Prediction: Δ0 on every ruler. Both edits delete text the X360 preprocessor never sees.

- **Native build.** Private configure of the worktree's `native/` (`~/tmp/narms3/nb`, no
  edits under `native/`): `ninja -j12 rb3-app rb3-render` rc 0, 1,644 steps, 0 `error:` lines,
  no warning in either touched file. These are the two targets that compile `JsonUtils.cpp`;
  `TexMovie.cpp` is compiled by `rb3-app` only. No new file under `native/src`, so
  `tools/native_stub_census.py` needed no entry.
- **Native run: not done.** `rb3-milo` (the one target `no610m.sh` can run headless) compiles
  neither touched file, `rb3-app` is linked but not run by the gate, and `rb3-render` needs
  the sanctioned GPU overlay that `no610m.sh` lacks (sweep 2 record). The native health
  selftest was not run (lane brief).
- **X360 A/B: PENDING at the lane's budget cap.** `python3 tools/ab_measure.py --worktree
  ~/tmp/wt-narms3 --revert HEAD --label narms3-c1` (PID 1077486, started 00:35, log
  `~/tmp/narms3/ab1.log`, written only at exit because stdout is buffered) was still in its
  obj-patcher phase at 01:32 under a box load average of 75-100, with a second lane's
  ab_measure running alongside. Read it with `tail -40 ~/tmp/narms3/ab1.log`: the run ends
  in an `rc=` line; any rc other than 0 means the tool refused and printed no numbers.
  Expected Δ0 on `name_check` and on `none`.
