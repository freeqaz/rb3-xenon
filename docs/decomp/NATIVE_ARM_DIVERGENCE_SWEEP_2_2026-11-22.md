# Native-arm divergence sweep, part 2: obj, os, utl, synth (2026-11-22, lane `narms2`)

This continues `NATIVE_ARM_DIVERGENCE_SWEEP_2026-11-22.md`, which skipped the two-sided
`HX_NATIVE` arms in `src/system/obj`, `os`, `utl` and `synth` for lack of budget. The question
is the same: where does the native arm behave differently from the X360 arm (the retail one)?
Byte matching is not the goal here; behaviour is.

## Method

- **Enumeration.** The earlier lane's `arms.py`, re-run on these four dirs
  (`~/tmp/narms2/arms.json`), found 935 HX_NATIVE conditionals, **420 of them two-sided**
  (obj 170, utl 134, synth 61, os 55). `dump.py` split them into 199 short arms (≤2 code lines
  per side, read as a one-line-per-side listing) and 221 longer arms (read in full, by file).
- **Native reach.** The native compile database (5,035 commands) names most TUs in these dirs.
  It does not name `os/System_Xbox.cpp` or `os/PlatformMgr_Xbox.cpp`, so their arms are dead
  natively and were not judged.
- **Retail reference.** The X360 arm, with `python3 tools/va_disasm.py` used to confirm it
  wherever a fix relied on it: `DataMin` 0x8275EB80 (two `Evaluate`s, float `fsel` path if
  either is type 1, else integer compare), `DateTime::ToDateString` 0x82523120 (`bl
  ToMiniDateString`, then the year + 0x76C formatted and appended), `LoadMgr::PollFrontLoader`
  0x827BF348 (saves `mLoaderPos`@0x54, stores `front->mPos`, pushes `front->mHeap`, calls
  `PollLoading`, pops, restores). The other fixes rest on the X360 arm's own retail-address
  comment (`NextName` 0x82757E68, `OnFind`, WaveFile labels).

## Findings

Reachable: **boot** = on the rb3-app boot/UI path; **game** = when songs, characters or venues
load; **script** = only when a script uses that form; **edit** = only in edit or save paths.

### Fixed (native arm deleted; native now runs the retail body)

| site | native vs retail | reachable |
|---|---|---|
| `obj/DataFunc.cpp` `DataMin`, `DataMax` | Native was DC3's variadic form: it evaluates each operand **twice** (`array->Float(i)` and then `Evaluate(min)`), so side-effecting operands run twice, and it returns the winning operand node unchanged. Retail takes two operands and returns a float if either is a float (`{min 2 3.5}` is `2.0` in retail, int `2` in native). | script (common) |
| `obj/DataFunc.cpp` `DataMultiply` | Native evaluates operands 1 and 2 once into unused locals and again in its loop (**double evaluation**), and multiplies every operand; retail multiplies two. | script (common) |
| `obj/Utl.cpp` `NextName` | Native widened the numeric suffix to the old suffix's width (`foo` → `foo1`, `foo7` → `foo8`); retail always uses `%02d` (`foo01`, `foo08`). Different generated object names. | edit / runtime object copies |
| `obj/Dir.cpp` `ObjectDir::OnFind` | Native `MILO_FAIL`s when the optional fail flag is set and nothing is found; retail evaluates the flag and returns null. | script |
| `obj/DirLoader.cpp` `DirLoader::Cleanup` | Native wrapped the loader's final `SyncObjects` in DC3's `SetSubDirFlag(mSubDir)` … `SetSubDirFlag(false)`. Every subdir load passes `mSubDir = true` (`Dir.cpp` `LoadFile(..., true)`), so native ran `RndDir::SyncObjects` with `IsSubDir()` true and **skipped `SyncDrawables` and the anim/poll harvest** for a freshly loaded subdir. Retail's `ObjectDir::PreLoad` has already cleared `mIsSubDir` by then, so retail harvests. Also dropped the native-only `AutoGlitchReport`. | game (every subdir load) |
| `obj/Dir.h` `operator<<(BinStream &, const ObjDirPtr &)` | Native wrote the dir's object name; retail writes the file path relative to the root. | edit (save) |
| `os/DateTime.cpp` `DateTime::ToDateString` | Native used DC3's locale switch (MDY/DMY/YMD/ISO, two-digit year); retail is `ToMiniDateString` + `/%04d` year in every locale. | UI (dates) |
| `utl/Loader.cpp` `LoadMgr::PollFrontLoader` | Native polled the front loader without setting `mLoaderPos` to `front->mPos` or pushing its heap. `ObjDirPtr::LoadFile` reads `GetLoaderPos()` to keep a stay-back loader's nested loads stay-back, so native queued them at the outer position. Now the retail body, with a native-only empty-queue guard kept. | boot/game (every load) |
| `utl/WaveFile.cpp` marker labels | Native took the next chunk of any type per cue and also turned `ltxt` chunks into labels; retail seeks the next `labl` chunk for each cue. Native could spend iterations on non-`labl` chunks and miss labels. | game (wav markers) |
| `synth/StandardStream.cpp` `setJumpSamplesFromMs` | Native rounded the jump target up by one sample when the truncation landed before `toMs`; retail truncates. Restoring retail also restores its two out-of-range warnings. | game (looping streams) |
| `synth/VorbisReader.cpp` `VorbisReader::Init` | Native passed the song length to `InitInfo`; retail passes −1. The length only feeds the jump warnings above (`unk154`), so retail never warns; restored to keep that. | game |

### Deferred (divergent, but coupled to other work)

| site | divergence | why deferred |
|---|---|---|
| `obj/Utl.cpp` `MergeObjectsRecurse` subdir loop | Native runs DC3's `FilterSubdir` pass in the loop: a `kMergeReplace` subdir is moved into `toDir` and **erased from `fromDir`**, and a merged subdir gets `FilterSubdir` called twice (once in the loop, once at the top of the recursive call). Retail just recurses. | The BandCharacter native `FilterSubdir` shim (`BandCharacter.cpp` ~:2586, "char textures rendering white") and its compensator `RebindSharedSkinMatsToOwn` depend on this path. Fix them together with a render check. |
| `obj/Task.cpp` `ScriptTask` watched objects | Retail `AddRef`s each object in `mObjects` so a dying watched object deletes the task. Native `Hmx::Object` cannot ring-reference the task, so `mObjects` is raw pointers and **a watched object's death does not end the task** (only `mThis` is tracked). | ObjRef-model work: `mObjects` needs real ObjRef nodes (an `ObjPtrList`) on native. |
| `obj/Object.cpp` `SetProperty`, `ExportPropertyChange`, `ChainSource`, `GetOrAddSinks`; `obj/Msg.cpp` `MsgSource::Replace` | Native carries DC3's Object-level `mSinks` (prop-sync handlers, chained event sinks) alongside RB3's `MsgSource` sinks. | Already deferred as "two sink systems" in the first sweep. The `MsgSource::Replace` arm itself is equivalent: native's `MsgSourceSinkRef` runs `RemoveSink` on sink death. |

### Reviewed and left (equivalent on RB3 data, or deliberate native platform work)

- **ObjRef/ObjPtr model** (`ObjPtr_p.h`, `Object.h`, `Dir.h` `ObjDirPtr`, `TypeProps.cpp`,
  `Task.cpp` Message/Thread `Replace`, `Utl.cpp` `ReplaceRefs`/`MergeDirs` ring walks,
  `Object.cpp` dtor/`ReplaceRefs`/`ReleaseRefs`, `Dir.cpp` `HasDirPtrs`/`DeleteObjects`): the
  native polymorphic ring re-expresses the same intent. `ObjPtrList::sort` is a front-to-back
  insertion sort natively and back-to-front in retail; both are stable, so the result is the same.
- **LP64 / endianness ports**: `DataArray` `Remove`/`Contains` pointer-vs-int compare,
  `FindArray(Symbol)`, `DataUtl`/`DataFunc` symbol `FindArray`, `DataNode::Equal` symbol
  compare, `DataFile` size bytes, `TypeProps` key compares, `Locale` binary search on `Str()`,
  `BinStream` swap polarity and `long` ops, `WAV_SAMPLE`/`WAV_LE`, `VorbisReader` nonce,
  `OggMap` hash XOR, `OSCMessenger`, `NetStream`, `trie`, `Timer` cycles, `MemStream`, `Str`.
- **`DataNode::operator==`** → `Equal`: same result on every type pair except retail's
  object-vs-int case, which reads the int as a `String*` (a wild read on LP64).
- **Robustness shims**: `Object::OnGet` out-of-range default, `PropertySize` bail after a
  missing property, `MoggClipMap` copy ctor, `Utl.cpp` `ReplaceObject` null `to`, DataArray
  `#else`/`#endif` underflow, `SystemConfig` `SafeFind`, `JoypadClient` defaults,
  `TheCacheMgr`/`TheNetCacheMgr` null checks, `SampleData` malloc fallback,
  `StandardStream::EnableReads` null guard.
- **Equivalent on RB3 data**: `ObjectDir::HasSubDir` (retail's `NextSubDir(0)` returns `this`
  and recurses, so the native recursion is the same set); `InlineProxy(bs)` and
  `InlineProxyType()` (RB3 writes ObjectDir rev 0x1B, so native reads the bool form and stores
  0/1; the only reader that cares about kInlineAlways vs kInlineCached is `Flow`, which RB3
  does not have); `PreloadSharedSubdirs` (nested-array recursion that RB3 data never uses);
  `DirLoader::LoadObjs/LoadDir` (retail plus diagnostics, stub-vtable skip and RealEof bail;
  the rev 0x1E/0x1F trailers never fire on RB3 data); `SetSystemLanguage` first-set guard
  (skips a redundant early `Locale::Init`; `InitSystem` re-inits the locale anyway);
  `sAudioOffsetMs` (always 0, no setter); `OnSysPlatformSym`/`sysplatform` (native
  `mPlatform` is kPlatformXBox unless `-null_platform`); `Dir.cpp` superclass memo.
- **Deliberate native platform work**: the Debug macros (`MILO_FAIL`/`WARN`/`NOTIFY`/
  `ASSERT` live on native), `ArkFile`/`CDReader`/`BlockMgr`/`File_Win`/`AsyncFile` native I/O,
  `ChunkStream` synchronous decompression and read-ahead, `System.cpp` init/poll/exec (Holmes,
  no MemInit/MC/VirtualKeyboard init), `UsingCD()` in `DirLoader`/`OnUsingCD`, `Song.cpp`
  async wait states instead of busy-waits, `Synth` native stream/mogg open, mogg key
  derivation (`KeyChain`, `ByteGrinder` software AES), `StreamReceiver`, `StandardStream`
  buffers, `CacheWav`, the Mem*/PoolAlloc/AllocInfo/MemTracker allocator arms, `HxGuid`
  null retry, extra script funcs (`system_locale`, `switch_system_language`).

## Measurement

Prediction: Δ0 on every ruler. Every change deletes text the X360 preprocessor never sees, or
(in `PollFrontLoader`) adds an `#ifdef HX_NATIVE` guard and changes a comment.

- **Src commit** `ee7513f5f` (10 files): `python3 tools/ab_measure.py --worktree
  ~/tmp/wt-narms2 --revert HEAD --label narms2-c1`, shipped `name_check` ruler plus the `none`
  control leg, objdiff sha256 `ec3badc0f33323e9`. Both legs: matched=55265, masked_equal=25360,
  honest=29905, code%=59.622105; `none` leg matched=55321, code%=60.015200 on both;
  Δfuzzy +0.000000pp (64.704510 on both); units at 100% 633→633 (mpn) and 547→547
  (all-rows-fuzzy). Leg B recompiled 1,010 objects (`obj/Dir.h` reaches most TUs), split=0,
  settled in 2 iterations. **Δ0, as predicted.**
- **Native build.** Private configure of the worktree's `native/` (`~/tmp/narms2/nb`, no edits
  under `native/`): `cmake --build --target rb3-milo rb3-render` rc 0, 1,183 steps, 0 errors, no
  warnings in the touched files.
- **Native run.** Through `rb3-scout/coord/no610m.sh` (3090s only), `rb3-milo <assets>
  ui/track/gen/tracksystem_meshes.milo_xbox` gives rc 0 and `RESULT: ALL GATES PASSED (0 gate
  failure(s))`. Its two `Could not find overlay "char_test"` lines also show up in runs from
  before this change.
- **rb3-render was not run on this branch on a sanctioned GPU.** Under `no610m.sh` it returns
  rc 2 `NO GPU`: the wrapper does not carry the nv615 userspace overlay, so the NVIDIA ICD fails
  to load. The coordinator's landing gate runs it through `~/tmp/selftest_overlay.sh`. One
  earlier **bare** run (by mistake) opened the Radeon 610M (`renderD130`) and passed (rc 0, 0
  gate failures), but it disturbed another session's GPU timing measurement and is not
  evidence for this branch.
- The native health selftest was not run (lane brief: do not merge; the coordinator gates at
  landing).
