# W16-QA — open src/system rows whose copied body predates current DC3

Lane W16-QA, 2026-10-06, worktree `~/tmp/wt-w16qa`, branch `w16-qa`. Started at main `9480cb75e`, rebased onto
main `c2e3a92f6` (after W16-PU). Ruler: shipped graded `name_check`, read from `report.json`.

Brief: W16-PO found five open engine rows that current `../dc3-decomp` had already fixed, in files we copied before
the fix. Sweep every open (fuzzy < 100) row in `src/system` outside `rndobj/` and `char/` (another lane owns those)
for the same thing. Adopt DC3's newer body wherever it matches retail better, and fix anything else found on the way.

## 1. Result

| | rows | bytes |
|---|---:|---:|
| open rows examined | 967 | — |
| rows raised | **54** | — |
| of which to 100 (fuzzy) | 29 | 7,184 |
| rows lowered | **0** | — |

In-worktree, before the rebase (`report.json` of base `9480cb75e` vs the last commit, full builds both):
**+31 fns / +7,184 B, 54 rows up, 0 down** (matched 53,525 → 53,556, `matched_code` 5,939,568 → 5,946,752).
The whole-binary A/B on the rebased branch is in §5.

**Behaviour defects fixed (native-visible), each confirmed on retail bytes:**

1. **`DxShaderMgr::LoadShaderFile` registered record 0 as the vertex shader.** Retail's `subi/cntlzw/extrwi./beq`
   makes record **1** the vertex shader (`bool isVertexShader = !(k - 1)`). Taken from DC3's body.
2. **`fft_matrix_forward_columnwise` had its two half sizes swapped.** The outer guard is `half_cols > 0` and
   `temp2 = temp + half_rows * 0x10`; ours used rows for the guard and cols for the offset. Taken from DC3's body.
3. **`CreateBackBuffers` used `D3DSURFACE_PARAMETERS` uninitialised.** Retail zeroes it first
   (`memset(&params, 0, sizeof(params))`). Taken from DC3's body.
4. **`StorePanel::CheckOut` passed 0 as the `XboxPurchaser` flags.** Retail passes the signed-in player's server ID
   (`TheNet.GetServer()->GetPlayerID(pad)` when the server is connected, else 0). This is the same tagging
   `MusicLibraryStore` and `AssetStore` already do. Row 55.66 → 96.52.
5. **`Locale::Init` passed the wrong error context to `LiteralArray`.** From the second chunk on, retail passes the
   previous chunk array, not the file's array. Retail moves `curArr` into r4 once before the k loop and then carries
   the last `LiteralArray` result in the same register. Only the file/line of a "Data %s is not Array" failure
   depends on it.

Everything else is codegen-only. Each change was checked to compute the same values as before.
- `op59`: DC3's history called its spelling a behaviour fix, but our previous body was already value-identical
  (0 of 768 inputs differ).
- `fft_matrix_inverse_columnwise`: its "walk copies, not temp/temp2" note was already true of ours.

**Three DC3 changes were corrected for RB3 rather than taken verbatim:**
- `LoadShaderFile`: DC3 adds a `BeginMemTrackFileName` / `EndMemTrackFileName` pair that retail does not have.
  Dropping it took the row to 100.
- `json_tokener_parse_ex`: DC3 frees `obj_field_name` with `operator delete`; retail calls `free`. Ours was kept.
- `DxShaderMgr::SetPConstant(PShaderConstant, RndCubeTex *)`: DC3's `GetNullTexture()->Select` spelling took the
  row from 100 to 0. Ours was kept.

## 2. Method

1. **Population.** Every function row in `report.json` (base `9480cb75e`) whose unit's source lives under
   `src/system/` but not `rndobj/` or `char/`, with `fuzzy_match_percent < 100`: **967 rows**. This includes rows
   reached through `#include "x.cpp"` (the extractor follows `.cpp`/`.c` includes in both trees).
2. **Body comparison** (`~/tmp/w16qa/body_cmp.py`). Demangle with `llvm-undname`, find every definition of the
   qualified name in our file and in DC3's, strip comments and whitespace, and compare. `BEGIN_HANDLERS` /
   `BEGIN_PROPSYNCS` / `BEGIN_COPYS` / `BEGIN_LOADS` / `BEGIN_SAVES` blocks are found by macro. DC3's own graded
   fuzzy for the same mangled name is joined from `../dc3-decomp/build/373307D9/report.json`. That number was
   measured on DC3's binary, so it was used as a hint and never as proof.

   | class | rows | meaning |
   |---|---:|---|
   | ANON | 502 | `fn_` row (mostly EH funclets); no name to pair with DC3 |
   | DIFF | 164 | both trees define it, bodies differ |
   | NO_DC3_FILE | 148 | no counterpart file in DC3 (RB3-only engine files: bandobj-adjacent, meta, net, …) |
   | SAME | 37 | body identical to DC3 |
   | OURS_ONLY | 37 | DC3's file lacks the definition |
   | NO_QNAME | 31 | no qualified name derivable from the symbol |
   | NOT_FOUND | 28 | extractor found no definition in either tree |
   | PARTIAL | 13 | overload set, some overloads identical |
   | DC3_ONLY | 7 | extractor found DC3's definition but not ours |

3. **Mechanical swap** (`~/tmp/w16qa/swap.py`). Each DIFF/PARTIAL row with fuzzy > 0 and a one-to-one definition
   pairing became a candidate: **170**. Each candidate's DC3 body was dropped into our file alone and compile-checked
   with `OBJCACHE=off` to a scratch `/Fo`, with the file and its mtime restored afterwards. **98 compiled**, 72 did
   not.
4. **Batch, then keep.** All 98 were applied together and built in full (`./tools/ninja-locked`), and the two
   `report.json` were diffed row by row. **93 rows went down.** DC3's newer spelling is often tuned to DC3's own
   image and is worse for ours. Only the 35 swaps whose own row rose were kept. The set was rebuilt and re-diffed
   until no row fell.
5. **By hand.** The 72 compile failures and the higher-value SAME rows were read one at a time against retail with
   `run_diff_inspect` (mismatches / stack-layout / asm_listing). Where DC3's change named a real retail shape, the
   shape was ported into our body. Where the change was DC3-only API (a newer revision of the class), it was not
   taken.
6. Every comment that came in with a DC3 body was rewritten to cite RB3 retail, or to name DC3 as the source of a
   measurement, never a DC3-image address or a DC3-build percentage. That commit moved 0 rows.
   - Every address left in the branch's added lines (`0x823E0648`, `0x823E07B8`, `0x82726EE8`, `0x827366F0`,
     `0x82B759D8`, `0x82C3EB80`) was checked to be a row in our `target_symbol_map.json`.
   - One adopted comment was wrong for RB3 and was corrected. It is in `UsbMidiKeyboard::Poll`, which says MSVC turns
     the high-hand sum into a Horner chain. On ours the add chain already matches; the residual is one load issued a
     slot early.

## 3. Rows raised (54)

| file | row | size | base | final |
|---|---|---:|---:|---:|
| net/json-c/json_tokener.c | `json_tokener_parse_ex` | 4872 | 93.20 | 99.25 |
| synth_xbox/FFT.cpp | `fft_matrix_forward_columnwise` | 1200 | 55.08 | 79.39 |
| synth_xbox/SpectralAnalysis.cpp | `DSP::SpectralAnalysis::Analyze` | 496 | 70.72 | 95.00 |
| meta/StorePanel.cpp | `StorePanel::CheckOut` | 260 | 55.66 | 96.52 |
| rnddx9/ShaderMgr.cpp | `DxShaderMgr::LoadShaderFile` | 644 | 84.83 | 100.00 |
| synth_xbox/Synth.cpp | `Synth360::SetupHeadsetSubmixes` | 588 | 85.95 | 99.88 |
| synth_xbox/FFT.cpp | `CalculateSinCosTable` | 260 | 74.06 | 100.00 |
| synth_xbox/FFT.cpp | `SquareComplexTransposeVector` | 296 | 79.24 | 97.03 |
| synth_xbox/PitchDetector.cpp | `DSP::Synapse::PitchDetector::Detect` | 676 | 92.63 | 100.00 |
| rnddx9/Rnd_Xbox.cpp | `CreateBackBuffers` | 300 | 83.91 | 100.00 |
| world/Spotlight.cpp | `Spotlight::BuildNGSheet` | 1232 | 95.13 | 98.56 |
| utl/MultiTempoTempoMap.cpp | `MultiTempoTempoMap::GetLoopTick` | 164 | 76.95 | 100.00 |
| movie/Splash.cpp | `Splash::Splash` | 420 | 92.27 | 100.00 |
| os/Archive.cpp | `Archive::Merge` | 516 | 93.71 | 100.00 |
| synth_xbox/FFT.cpp | `fft_scalar` | 772 | 76.24 | 80.26 |
| math/Geo.cpp | `MakeBSPTree` | 1580 | 95.45 | 97.27 |
| synth/MoggClip.cpp | `MoggClip::SetupPanInfo` | 124 | 78.71 | 100.00 |
| synth_xbox/StreamReceiver360.cpp | `StreamReceiver360::Tag` | 120 | 80.33 | 100.00 |
| synth_xbox/FFT.cpp | `fft_matrix_inverse_columnwise` | 1160 | 84.13 | 85.96 |
| rnddx9/RenderState.cpp | `RndRenderState::SetBorderColor` | 88 | 77.05 | 100.00 |
| synth_xbox/Mic.cpp | `MicManagerXbox::AddRemoteMic` | 252 | 93.65 | 100.00 |
| synth_xbox/FftIpp.cpp | `FftIpp::FftReal` | 188 | 92.64 | 100.00 |
| synth_xbox/ExternalMic.cpp | `ExternalMic::dataReady` | 236 | 94.15 | 100.00 |
| rnddx9/Rnd.cpp | `DxRnd::Offscreen` | 116 | 89.14 | 100.00 |
| ui/PanelDir.cpp | `PanelDir::PanelNav` | 384 | 96.77 | 100.00 |
| rnddx9/ShaderMgr.cpp | `SetPConstant(PShaderConstant, const Vector4 &)` | 88 | 69.55 | 83.18 |
| rnddx9/ShaderMgr.cpp | `SetVConstant(VShaderConstant, const Vector4 &)` | 88 | 69.55 | 83.18 |
| utl/MemMgr.cpp | `ThreadMemStack` | 348 | 85.17 | 88.16 |
| math/Geo.cpp | `BSPFace::Update` | 612 | 97.79 | 99.47 |
| utl/NetCacheMgr.cpp | `NetCacheMgr::AddLoaderRef` | 696 | 96.44 | 97.84 |
| os/Archive.cpp | `Archive::GetFileInfo` | 432 | 97.92 | 100.00 |
| midi/MidiReader.cpp | `pow` | 80 | 90.00 | 100.00 |
| os/UsbMidiKeyboard.cpp | `UsbMidiKeyboard::Poll` | 1164 | 98.12 | 98.78 |
| synth/ByteGrinder.cpp | `op9` | 96 | 92.21 | 99.58 |
| rnddx9/Rnd.cpp | `DxRnd::DrawLine` | 360 | 98.09 | 100.00 |
| utl/Locale.cpp | `Locale::Init` | 1304 | 96.47 | 96.84 |
| synth/MicNull.cpp | `MicNull::GetRecentBuf` | 72 | 93.33 | 100.00 |
| os/NetworkSocket_Win.cpp | `WinSockSocket::RecvFrom` | 136 | 96.91 | 100.00 |
| utl/Cache_Xbox.cpp | `CacheXbox::ThreadWrite` | 420 | 99.52 | 100.00 |
| synth_xbox/FFT.cpp | `fft_real_forward_scalar` | 420 | 86.24 | 86.67 |
| synth/ByteGrinder.cpp | `op59` | 108 | 98.81 | 100.00 |
| math/Geo.cpp | `Sphere::GrowToContain` | 372 | 99.12 | 99.46 |
| synth_xbox/Mic.cpp | `ChatReceiver::ProcessChatData` | 636 | 99.81 | 100.00 |
| world/CameraShot.cpp | `CamShot::Shake` | 1148 | 99.76 | 99.86 |
| rnddx9/RenderState.cpp | `RndRenderState::SetTextureClamp` | 136 | 99.41 | 99.71 |
| meta/MoviePanel.cpp | `MoviePanel::SyncProperty` | 228 | 99.91 | 100.00 |
| utl/NetCacheMgr.cpp | eight `AddLoaderRef` EH funclets `fn_827CE6C8`–`fn_827CE7F8` | 40–64 | 99.30–99.90 | 99.50–100.00 |

### Hand-ports (DC3's body did not drop in)

- **`StorePanel::CheckOut`** (behaviour fix 4). `meta/StorePanel.cpp` is a system TU, and including `net/Net.h`
  there pulls `game/BandUser.h` in through `net/VoiceChatMgr.h`. That header's macro dialect breaks `SYNC_PROP` in
  this TU (`load_ok` undeclared). Pushing and popping the macros then hits a `Difficulty` enum redefinition.
  - Fix: the `Net` class, `TheNet` and `TerminateTheNet` moved to a new `src/network/net/NetCore.h`. It includes only
    `obj/Object.h` and forward-declares the rest. `Net.h` includes it, so no existing includer changes.
- **`Synth360::SetupHeadsetSubmixes`**: DC3's body with the real `IXAudio2` virtual calls, with DC3's leading log
  line stripped (retail has none).
- **`op9`**: one byte op through a `u8` temp. **`pow(float, int)`**: through the `PowInt` template.
- **`DxRnd::Offscreen`**: `D3DDevice_GetRenderTarget(Device(), 0)`. `Rnd.h`'s `Device()` accessor became `const`.
- **`MicManagerXbox::AddRemoteMic`**: `ChatBuffer` declares the xuid as a `u64 mXuid` member (`Mic.h`), with DC3's
  declaration order. The three `Poll` reads use it.
- **`ExternalMic::dataReady`**: counter declaration order and `0 < numFrames` guard.
- **`NetCacheMgr::AddLoaderRef`**: `NetLoaderRef` gained a zeroing default ctor and a 4-arg ctor (`NetCacheMgr.h`).
  Eight of its funclets rose with it.
- **`Archive::Merge`**: `FileEntry` gained a 5-arg ctor (`Archive.h`), and the local hash-table copy was dropped.
- **`MakeBSPTree`**: retail has three return paths, the last two sharing one `frontFaces` clear with the result in
  r30. Only the early-return form produces that, so it was taken from DC3.
  - DC3 also drops the explicit `clear()` calls. That shrinks our frame by 0x10 (0x1a0 against retail's 0x1b0) and
    drops the three 40 B EH funclets from 100 to 99.8, measured. So the clears stay on the left-failure path. DC3's
    `pow` spelling was inert here.
- **`Locale::Init`** (behaviour fix 5). Only the `chunkArr` carry was taken. The rest of DC3's body is DC3-only
  devkit-locale and `mInitialized` code, which ours already excludes on retail evidence.

## 4. Rows not raised, by reason

The full per-row record is the appendix (§7). Summary:

- **DC3 body compiled but scored worse here (63 rows).** Applied in batch 1 and reverted. These are spellings tuned
  to DC3's image; on ours the row fell or stayed flat.
- **DC3 body is a newer revision (most of the 72 compile failures).** It depends on members or API RB3 does not
  have: `mCharForceLod`, `TryAlloc` / `mMinFreeBytes`, `HttpGet::mFlags` / `mHttpStatus`, `Spotlight::GetCastShadow`,
  `RndDrawable::DrawShadow(xfm, float)`, `BinStreamRev` 3-arg ctors, `ObjDirItr::Ptr`, `XMARKETPLACE` struct access.
  None of these was taken.
  - `MemHeap::Alloc` is a clean example. DC3 splits out `TryAlloc`; retail's 652 B `Alloc` is the inline form we
    already have.
- **Tried by hand and reverted (row went down):**

  | row | change | before → after |
  |---|---|---:|
  | `MemHeap::Init` | reorder | 82.81 → 81.83 |
  | `MeterEffect::DoProcess` | DC3 port | 95.48 → 79.23 |
  | `~VorbisReader` | DC3 port | 97.41 → 93.38 |
  | `WorldInstance::SavePersistentObjects` | DC3 spelling | 97.63 → 97.10 |
  | `NoteVoiceInst` ctor | DC3 `SetBankVolume` operand order | 97.55 → 95.41 |

- **SAME (37 rows).** Our body already equals DC3's, so the residual is not in the function text. Checked by hand:
  - `Intersect(Vector3, Vector3, Box, float &, float &)`, 93.94 vs DC3 100: retail keeps `tmin` in f12 across
    iterations (loaded once, then reused from the bottom compare's reload), while we reload it at the loop top.
    Scheduling.
  - **`SynthEmitter::SynthEmitter`**, 86.13 vs DC3 100: retail inlines `ObjPtr<RndTransformable>(Object *, T *)` and
    we call it out of line.
    - The house per-TU lever `#define RB3_TU_OBJPTR_FORCEINLINE_CTOR` is **inert here** because `synth/` is a
      PCH-eligible directory. `Object.h` is precompiled before a TU-top `#define` can reach it.
    - ⇒ The `RB3_*OBJPTR*` per-TU gates cannot reach any TU in the nine PCH dirs. A row there that needs one must be
      fixed some other way.
  - `BufStream::~BufStream` (one `addi` moved), `UTF8toASCIIs` (one `mr`), `FIRFilter::setCoefficients`,
    `PeakDetector::Detect`: scheduling / regalloc residues with the body already right.
- **Overloads the extractor could not pair (`Geo.cpp` `Intersect`).** Checked by hand. Apart from the Vector3/Box one
  above, each carries a DC3 spelling that DC3 itself scores at or below ours on DC3's image:

  | overload | ours | DC3 |
  |---|---:|---:|
  | Triangle/Box | 97.16 | 97.08 |
  | Segment/Triangle | 97.19 | 90.38 |
  | Vector3/Triangle | 98.92 | 98.78 |

- **ANON / NO_DC3_FILE / OURS_ONLY / NO_QNAME / NOT_FOUND / DC3_ONLY (753 rows).** Nothing in DC3 to compare against
  by name. These are the funclets, the RB3-only engine files, and definitions that live in macros. This sweep has
  nothing to say about them.
- `MakeSessionJob::Start`: DC3's body uses `mSessionFlags`, an RB3-irrelevant session API. Skipped.

## 5. Measurement

Whole-binary A/B: `python3 tools/ab_measure.py --worktree ~/tmp/wt-w16qa-ab --patch ~/tmp/w16qa/branch.patch`,
where the patch is `git diff c2e3a92f6 w16-qa` (source only, 38 files). It runs in a fresh worktree at main
`c2e3a92f6`.

**Prediction, written before the result:** +30 fns / +6,548 B, 53 rows up, 0 down. That is the in-worktree
+31 / +7,184 minus `ChatReceiver::ProcessChatData` (636 B), because W16-PW's identical constant fix (the only rebase
conflict) landed first on main.

| leg | matched | honest | code% | fuzzy |
|---|---:|---:|---:|---:|
| A (main `c2e3a92f6`) | 53,531 | 28,338 | 57.979477 | 63.704975 |
| B (+ branch) | 53,562 | 28,361 | 58.049576 | 63.719975 |

- **Δmatched +31 / Δhonest +23 / Δmasked_equal +8 / Δcode_bytes +7,184 / Δcode% +0.070099 pp.** The `none`
  control read +7,304 B. That movement is expected for a source patch, so it is not an alias signal.
- Leg B recompiled 218 TUs (split 0) and settled in 2 iterations.
- Units at 100 % (mpn ruler) 554 → 563 (+9, 0 fell off): FftIpp, MicNull, MidiReader, MultiTempoTempoMap,
  NetworkSocket_Win, RenderState, Splash, MoggClip, PitchDetector.
- **Row diff of the two archived reports (68,909 rows on both legs): 54 up, 0 down.** It is the same 54-row set as
  the in-worktree diff in §1 and §3, checked by name.
- **The prediction failed by +1 fn / +636 B.** `ChatReceiver::ProcessChatData` was still 99.81 on main. W16-PW's
  constant (`0.00039269909f`, kept in the rebase) is the same float as DC3's, so the constant was not what crossed
  the row. The rest of the adopted DC3 body is.
- Run dir: `~/tmp/wt-w16qa-ab/.ab_measure_runs/20261006-113446-branch-1784703` (`result.json`, both legs'
  `report.json.gz`).
- ⚠ The tool's restore deleted the new untracked `src/network/net/NetCore.h` from the A/B worktree, as designed.
  The file is committed on the branch; only the scratch worktree lost it.

## 6. Native

Both run on the rebased branch (`20d36d47b`) in `~/tmp/wt-w16qa`:

```
NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0
NATIVE_HEALTH_RESULT verdict=PASS link=PASS link_verified=18 link_expected=18 link_skipped=0 runtime=PASS runtime_ran=18 runtime_total=18 gates_pass=73 gates_fail=0 unrunnable=none selftest=SKIPPED scatter_unlinked=16 scatter_dirb=0 scatter_multihost=17 rc=0 handpose_controls=- handpose_baseline_fail=- runtime_crashed=0 runtime_failed=none
```

Shared headers touched, all covered by the gate: `net/Net.h` → `net/NetCore.h`, `rnddx9/Rnd.h` (`const Device()`),
`synth_xbox/Mic.h` (`ChatBuffer`), `utl/NetCacheMgr.h` (`NetLoaderRef` ctors), `os/Archive.h` (`FileEntry` ctors).

## 6a. Not done

- No permuter (deferred by directive). The SAME-class scheduling residues and the remaining FFT / SpectralAnalysis
  vector-register rows are its market.
- `rndobj/` and `char/` were excluded per the brief.
- No attempt to make the DC3-revision bodies compile by importing DC3's newer members: that would change RB3's class
  layouts to DC3's, which is the opposite of the goal.
- The SynthEmitter ObjPtr inlining was not pursued past the PCH finding. A per-TU `/D` in `objects.json` would
  mismatch the PCH and was not tried.

## 7. Appendix — every row checked

Base and final are graded fuzzy from `report.json`: base at `9480cb75e`, final at the last pre-rebase commit
(same source as the branch). "Class" is §2's body-comparison class. Rows are sorted by file. Overloaded names are
shown mangled.

| file | row | size | class | base | final | outcome |
|---|---|---:|---|---:|---:|---|
| bandobj/BandCamShot.cpp | `??$_M_splice_insert_dispatch@PAPAVRndDrawable@@@?$list@PAVRndDrawable@@V?$StlNodeAlloc@PAVRndDrawable@@@stlpmt` | 156 | NO_DC3_FILE | 99.74 | 99.74 | no counterpart file in ../dc3-decomp |
| bandobj/BandCamShot.cpp | `??4?$list@UTarget@BandCamShot@@V?$StlNodeAlloc@UTarget@BandCamShot@@@stlpmtx_std@@@stlpmtx_std@@QAAAAV01@ABV01` | 176 | NO_DC3_FILE | 99.89 | 99.89 | no counterpart file in ../dc3-decomp |
| bandobj/BandCamShot.cpp | `??4Target@BandCamShot@@QAAAAU01@ABU01@@Z` | 248 | NO_DC3_FILE | 99.60 | 99.60 | no counterpart file in ../dc3-decomp |
| bandobj/BandCamShot.cpp | `BandCamShot::Load` | 1588 | NO_DC3_FILE | 99.90 | 99.90 | no counterpart file in ../dc3-decomp |
| bandobj/BandCamShot.cpp | `BandCamShot::SetPreFrame` | 476 | NO_DC3_FILE | 98.32 | 98.32 | no counterpart file in ../dc3-decomp |
| bandobj/BandCamShot.cpp | `BandCamShot::StartAnim` | 948 | NO_DC3_FILE | 99.49 | 99.49 | no counterpart file in ../dc3-decomp |
| bandobj/BandCamShot.cpp | `fn_822B0D48` | 112 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCamShot.cpp | `fn_822B4DC0` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCamShot.cpp | `fn_822B4E7C` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCamShot.cpp | `fn_822B4F5C` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCamShot.cpp | `fn_822B4FEC` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCamShot.cpp | `fn_822B52F4` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCamShot.cpp | `fn_822B54D4` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCamShot.cpp | `fn_822B66EC` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCamShot.cpp | `fn_822B7D30` | 100 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCamShot.cpp | `fn_822B853C` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCharDesc.cpp | `BandCharDesc::CopyCharDesc` | 636 | NO_DC3_FILE | 96.30 | 96.30 | no counterpart file in ../dc3-decomp |
| bandobj/BandCharDesc.cpp | `DeformTri::Contains` | 140 | NO_DC3_FILE | 99.71 | 99.71 | no counterpart file in ../dc3-decomp |
| bandobj/BandCharDesc.cpp | `fn_82339260` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCharDesc.cpp | `fn_82339F3C` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCharDesc.cpp | `fn_8233B0D4` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCharacter.cpp | `??_EBandSong@@WBA@AAPAXI@Z` | 8 | NO_DC3_FILE | 97.50 | 97.50 | no counterpart file in ../dc3-decomp |
| bandobj/BandCharacter.cpp | `BandCharacter::AddOverlays` | 112 | NO_DC3_FILE | 92.46 | 92.46 | no counterpart file in ../dc3-decomp |
| bandobj/BandCharacter.cpp | `BandCharacter::OnClosetTeleport` | 112 | NO_DC3_FILE | 99.21 | 99.21 | no counterpart file in ../dc3-decomp |
| bandobj/BandCharacter.cpp | `BandCharacter::StartLoad` | 292 | NO_DC3_FILE | 94.16 | 94.16 | no counterpart file in ../dc3-decomp |
| bandobj/BandCharacter.cpp | `fn_8227DB30` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCharacter.cpp | `fn_82288420` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCharacter.cpp | `fn_82288EB0` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCharacter.cpp | `fn_8228965C` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCharacter.cpp | `fn_82289C64` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCharacter.cpp | `fn_8228A1B8` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandCrowdMeter.cpp | `BandCrowdMeter::Poll` | 652 | NO_DC3_FILE | 97.64 | 97.64 | no counterpart file in ../dc3-decomp |
| bandobj/BandDirector.cpp | `BandDirector::AddDircut` | 284 | NO_DC3_FILE | 99.93 | 99.93 | no counterpart file in ../dc3-decomp |
| bandobj/BandDirector.cpp | `BandDirector::BandDirector` | 1088 | NO_DC3_FILE | 98.27 | 98.27 | no counterpart file in ../dc3-decomp |
| bandobj/BandDirector.cpp | `BandDirector::OnFileLoaded` | 3816 | NO_DC3_FILE | 99.78 | 99.78 | no counterpart file in ../dc3-decomp |
| bandobj/BandDirector.cpp | `BandDirector::OnGetFaceOverrideClips` | 648 | NO_DC3_FILE | 99.17 | 99.17 | no counterpart file in ../dc3-decomp |
| bandobj/BandDirector.cpp | `BandDirector::OnMidiShot5Cleanup` | 1172 | NO_DC3_FILE | 98.36 | 98.36 | no counterpart file in ../dc3-decomp |
| bandobj/BandDirector.cpp | `fn_82295948` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandDirector.cpp | `fn_8229931C` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandDirector.cpp | `fn_82299344` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandDirector.cpp | `fn_82299394` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandDirector.cpp | `fn_822993BC` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandDirector.cpp | `fn_82299640` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandFaceDeform.cpp | `BandFaceDeform::DeltaArray::AppendDeltas` | 584 | NO_DC3_FILE | 95.07 | 95.07 | no counterpart file in ../dc3-decomp |
| bandobj/BandFaceDeform.cpp | `fn_822C7A80` | 112 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandHeadShaper.cpp | `BandHeadShaper::Init` | 1144 | NO_DC3_FILE | 98.88 | 98.88 | no counterpart file in ../dc3-decomp |
| bandobj/BandHeadShaper.cpp | `MemHeapTracker::~MemHeapTracker` | 4 | NO_DC3_FILE | 0.00 | 0.00 | no counterpart file in ../dc3-decomp |
| bandobj/BandHeadShaper.cpp | `SetMeshAnim` | 852 | NO_DC3_FILE | 98.78 | 98.78 | no counterpart file in ../dc3-decomp |
| bandobj/BandHeadShaper.cpp | `fn_822B0970` | 40 | ANON | 99.30 | 99.30 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandHeadShaper.cpp | `fn_822B09E0` | 40 | ANON | 99.30 | 99.30 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandIKEffector.cpp | `BandIKEffector::ComputeElbowPullAndQuat` | 184 | NO_DC3_FILE | 92.50 | 92.50 | no counterpart file in ../dc3-decomp |
| bandobj/BandIKEffector.cpp | `BandIKEffector::ComputeHandPullAndQuat` | 444 | NO_DC3_FILE | 89.20 | 89.20 | no counterpart file in ../dc3-decomp |
| bandobj/BandIKEffector.cpp | `BandIKEffector::DoFancyElbow` | 1040 | NO_DC3_FILE | 99.85 | 99.85 | no counterpart file in ../dc3-decomp |
| bandobj/BandIKEffector.cpp | `BandIKEffector::NeutralLocalXfm` | 440 | NO_DC3_FILE | 99.82 | 99.82 | no counterpart file in ../dc3-decomp |
| bandobj/BandIKEffector.cpp | `BandIKEffector::Poll` | 928 | NO_DC3_FILE | 99.96 | 99.96 | no counterpart file in ../dc3-decomp |
| bandobj/BandIKEffector.cpp | `fn_822C23E8` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandIKEffector.cpp | `fn_822C2488` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandIKEffector.cpp | `fn_822C5880` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandLabel.cpp | `fn_82341CA4` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandLeadMeter.cpp | `BandLeadMeter::PostLoad` | 356 | NO_DC3_FILE | 98.18 | 98.18 | no counterpart file in ../dc3-decomp |
| bandobj/BandLeadMeter.cpp | `BandLeadMeter::SyncScores` | 424 | NO_DC3_FILE | 86.93 | 86.93 | no counterpart file in ../dc3-decomp |
| bandobj/BandLeadMeter.cpp | `fn_822CAE38` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandList.cpp | `BandList::UpdateConcealState` | 228 | NO_DC3_FILE | 80.00 | 80.00 | no counterpart file in ../dc3-decomp |
| bandobj/BandList.cpp | `fn_8233D0EC` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandList.cpp | `fn_8233D170` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandList.cpp | `fn_8233D1C8` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandList.cpp | `fn_8233E540` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandList.cpp | `fn_8233E5C4` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandList.cpp | `fn_8233E61C` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandPatchMesh.cpp | `BandPatchMesh::FindXfm` | 1268 | NO_DC3_FILE | 75.18 | 75.18 | no counterpart file in ../dc3-decomp |
| bandobj/BandPatchMesh.cpp | `BandPatchMesh::MeshPair::PatchPair::PatchPair` | 96 | NO_DC3_FILE | 45.00 | 45.00 | no counterpart file in ../dc3-decomp |
| bandobj/BandPatchMesh.cpp | `BandPatchMesh::MeshVert::AddUV` | 492 | NO_DC3_FILE | 95.54 | 95.54 | no counterpart file in ../dc3-decomp |
| bandobj/BandPatchMesh.cpp | `BandPatchMesh::MeshVert::Normalize` | 696 | NO_DC3_FILE | 99.94 | 99.94 | no counterpart file in ../dc3-decomp |
| bandobj/BandPatchMesh.cpp | `BandPatchMesh::ProjectPatches` | 1248 | NO_DC3_FILE | 99.66 | 99.66 | no counterpart file in ../dc3-decomp |
| bandobj/BandPatchMesh.cpp | `BandPatchMesh::WorkVerts::AddUvs` | 196 | NO_DC3_FILE | 90.80 | 90.80 | no counterpart file in ../dc3-decomp |
| bandobj/BandPatchMesh.cpp | `BandPatchMesh::WorkVerts::ExtendTwin` | 716 | NO_DC3_FILE | 84.96 | 84.96 | no counterpart file in ../dc3-decomp |
| bandobj/BandPatchMesh.cpp | `BandPatchMesh::WorkVerts::SetSameVerts` | 756 | NO_DC3_FILE | 99.95 | 99.95 | no counterpart file in ../dc3-decomp |
| bandobj/BandPatchMesh.cpp | `BandPatchMesh::WorkVerts::SetVertsAndFaces` | 940 | NO_DC3_FILE | 99.00 | 99.00 | no counterpart file in ../dc3-decomp |
| bandobj/BandPatchMesh.cpp | `BandPatchMesh::WorkVerts::TryAddFace` | 824 | NO_DC3_FILE | 90.87 | 90.87 | no counterpart file in ../dc3-decomp |
| bandobj/BandPatchMesh.cpp | `BandPatchMesh::WorkVerts::WorkVerts` | 460 | NO_DC3_FILE | 99.96 | 99.96 | no counterpart file in ../dc3-decomp |
| bandobj/BandPatchMesh.cpp | `Invert` | 128 | NO_DC3_FILE | 92.28 | 92.28 | no counterpart file in ../dc3-decomp |
| bandobj/BandPatchMesh.cpp | `fn_823474F8` | 48 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandRetargetVignette.cpp | `??_DRndPollable@@QAAXXZ` | 8 | NO_DC3_FILE | 97.50 | 97.50 | no counterpart file in ../dc3-decomp |
| bandobj/BandRetargetVignette.cpp | `BandRetargetVignette::Enter` | 4 | NO_DC3_FILE | 95.00 | 95.00 | no counterpart file in ../dc3-decomp |
| bandobj/BandRetargetVignette.cpp | `BandRetargetVignette::EnterDir` | 772 | NO_DC3_FILE | 99.48 | 99.48 | no counterpart file in ../dc3-decomp |
| bandobj/BandRetargetVignette.cpp | `BandRetargetVignette::Poll` | 280 | NO_DC3_FILE | 90.11 | 90.11 | no counterpart file in ../dc3-decomp |
| bandobj/BandRetargetVignette.cpp | `fn_822C6F94` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandRetargetVignette.cpp | `fn_822C7038` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandScoreboard.cpp | `fn_822CE170` | 116 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandSongPref.cpp | `fn_822C0CF8` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandStarDisplay.cpp | `??1?$ObjPtr@VBandStarDisplay@@@@UAA@XZ` | 116 | NO_DC3_FILE | 0.00 | 0.00 | no counterpart file in ../dc3-decomp |
| bandobj/BandStarDisplay.cpp | `fn_822CDB70` | 120 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/BandTrack.cpp | `BandTrack::Reset` | 1116 | NO_DC3_FILE | 98.45 | 98.45 | no counterpart file in ../dc3-decomp |
| bandobj/BandTrack.cpp | `BandTrack::SetCrowdRating` | 528 | NO_DC3_FILE | 98.46 | 98.46 | no counterpart file in ../dc3-decomp |
| bandobj/BandTrack.cpp | `BandTrack::SetupCrowdMeter` | 320 | NO_DC3_FILE | 98.75 | 98.75 | no counterpart file in ../dc3-decomp |
| bandobj/BandWardrobe.cpp | `BandWardrobe::LoadMainCharacters` | 1776 | NO_DC3_FILE | 99.79 | 99.79 | no counterpart file in ../dc3-decomp |
| bandobj/BandWardrobe.cpp | `BandWardrobe::OnEnterCloset` | 356 | NO_DC3_FILE | 99.47 | 99.47 | no counterpart file in ../dc3-decomp |
| bandobj/BandWardrobe.cpp | `BandWardrobe::OnGetMatchingDude` | 232 | NO_DC3_FILE | 94.40 | 94.40 | no counterpart file in ../dc3-decomp |
| bandobj/BandWardrobe.cpp | `BandWardrobe::SyncProperty` | 2416 | NO_DC3_FILE | 99.74 | 99.74 | no counterpart file in ../dc3-decomp |
| bandobj/CharKeyHandMidi.cpp | `CharKeyHandMidi::FindPreferredFinger` | 228 | NO_DC3_FILE | 92.37 | 92.37 | no counterpart file in ../dc3-decomp |
| bandobj/CharKeyHandMidi.cpp | `CharKeyHandMidi::OnFingersUp` | 136 | NO_DC3_FILE | 99.71 | 99.71 | no counterpart file in ../dc3-decomp |
| bandobj/CharKeyHandMidi.cpp | `CharKeyHandMidi::Poll` | 2236 | NO_DC3_FILE | 84.28 | 84.28 | no counterpart file in ../dc3-decomp |
| bandobj/ChordShapeGenerator.cpp | `ChordShapeGenerator::BuildEndCap` | 1352 | NO_DC3_FILE | 99.05 | 99.05 | no counterpart file in ../dc3-decomp |
| bandobj/ChordShapeGenerator.cpp | `ChordShapeGenerator::BuildSpan` | 680 | NO_DC3_FILE | 99.76 | 99.76 | no counterpart file in ../dc3-decomp |
| bandobj/ChordShapeGenerator.cpp | `ChordShapeGenerator::CrossSec::~CrossSec` | 96 | NO_DC3_FILE | 99.79 | 99.79 | no counterpart file in ../dc3-decomp |
| bandobj/ChordShapeGenerator.cpp | `ChordShapeGenerator::GetCrossSection` | 424 | NO_DC3_FILE | 99.95 | 99.95 | no counterpart file in ../dc3-decomp |
| bandobj/ChordShapeGenerator.cpp | `ChordShapeGenerator::InterpolateXfm` | 784 | NO_DC3_FILE | 89.29 | 89.29 | no counterpart file in ../dc3-decomp |
| bandobj/ChordShapeGenerator.cpp | `fn_822E1AE8` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/ChordShapeGenerator.cpp | `fn_822E2230` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/ChordShapeGenerator.cpp | `fn_822E2258` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/ChordShapeGenerator.cpp | `fn_822E2580` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/ChordShapeGenerator.cpp | `fn_822E2F28` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/FingerShape.cpp | `FingerShape::Update` | 532 | NO_DC3_FILE | 95.41 | 95.41 | no counterpart file in ../dc3-decomp |
| bandobj/GemTrackDir.cpp | `??$Find@VRndCam@@@ObjectDir@@QAAPAVRndCam@@PBD_N@Z` | 164 | NO_DC3_FILE | 0.00 | 0.00 | no counterpart file in ../dc3-decomp |
| bandobj/GemTrackDir.cpp | `GemTrackDir::SetPitch` | 380 | NO_DC3_FILE | 99.89 | 99.89 | no counterpart file in ../dc3-decomp |
| bandobj/GemTrackDir.cpp | `SemitoneToWhiteKey` | 96 | NO_DC3_FILE | 50.00 | 50.00 | no counterpart file in ../dc3-decomp |
| bandobj/GemTrackDir.cpp | `fn_822E3630` | 48 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/GemTrackDir.cpp | `fn_822E3730` | 108 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/GemTrackDir.cpp | `fn_822E3878` | 232 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/GemTrackDir.cpp | `fn_822EAB90` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/GemTrackDir.cpp | `fn_822EE490` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/GemTrackResourceManager.cpp | `GemTrackResourceManager::InitSmasherPlates` | 672 | NO_DC3_FILE | 94.49 | 94.49 | no counterpart file in ../dc3-decomp |
| bandobj/GemTrackResourceManager.cpp | `_M_clear` | 116 | NO_DC3_FILE | 0.00 | 0.00 | no counterpart file in ../dc3-decomp |
| bandobj/GemTrackResourceManager.cpp | `fn_82356CA0` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/LayerDir.cpp | `LayerDir::GetBitmapList` | 692 | NO_DC3_FILE | 99.65 | 99.65 | no counterpart file in ../dc3-decomp |
| bandobj/LayerDir.cpp | `LayerDir::RefreshLayer` | 1588 | NO_DC3_FILE | 98.23 | 98.23 | no counterpart file in ../dc3-decomp |
| bandobj/LayerDir.cpp | `fn_82326BB4` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/LayerDir.cpp | `fn_82326C2C` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/LayerDir.cpp | `fn_82326C54` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/LayerDir.cpp | `fn_82326CA4` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/LayerDir.cpp | `fn_82326CCC` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/LayerDir.cpp | `fn_82328800` | 120 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/MeterDisplay.cpp | `fn_8231A930` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/MicInputArrow.cpp | `MicInputArrow::Update` | 1176 | NO_DC3_FILE | 99.97 | 99.97 | no counterpart file in ../dc3-decomp |
| bandobj/MiniLeaderboardDisplay.cpp | `fn_823199C8` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/MiniLeaderboardDisplay.cpp | `fn_823199D8` | 120 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/NoteTube.cpp | `NoteTube::CreateMeshes` | 392 | NO_DC3_FILE | 80.71 | 80.71 | no counterpart file in ../dc3-decomp |
| bandobj/OutfitConfig.cpp | `OutfitConfig::Piercing::Deform` | 1012 | NO_DC3_FILE | 98.99 | 98.99 | no counterpart file in ../dc3-decomp |
| bandobj/OutfitConfig.cpp | `OutfitConfig::SetSkinTextures` | 1192 | NO_DC3_FILE | 95.07 | 95.07 | no counterpart file in ../dc3-decomp |
| bandobj/OutfitConfig.cpp | `fn_822A314C` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/OutfitConfig.cpp | `fn_822A385C` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/OutfitConfig.cpp | `fn_822A640C` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/OutfitConfig.cpp | `fn_822A73A4` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/OutfitConfig.cpp | `fn_822A7FD4` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/OutfitConfig.cpp | `fn_822AA2DC` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/OverdriveMeter.cpp | `OverdriveMeter::SyncObjects` | 332 | NO_DC3_FILE | 69.16 | 69.16 | no counterpart file in ../dc3-decomp |
| bandobj/PatchDir.cpp | `??$_Destroy@VEventCall@EventAnim@@@stlpmtx_std@@YAXPAVEventCall@EventAnim@@@Z` | 4 | NO_DC3_FILE | 95.00 | 95.00 | no counterpart file in ../dc3-decomp |
| bandobj/PatchDir.cpp | `PatchDir::LoadStickerData` | 732 | NO_DC3_FILE | 98.88 | 98.88 | no counterpart file in ../dc3-decomp |
| bandobj/PatchDir.cpp | `PatchLayer::Draw` | 1020 | NO_DC3_FILE | 87.35 | 87.35 | no counterpart file in ../dc3-decomp |
| bandobj/PatchDir.cpp | `PatchSticker::MakeLoader` | 92 | NO_DC3_FILE | 98.70 | 98.70 | no counterpart file in ../dc3-decomp |
| bandobj/PatchDir.cpp | `fn_822774F8` | 164 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/PatchDir.cpp | `fn_822775C4` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/PatchDir.cpp | `fn_822785C0` | 96 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/PitchArrow.cpp | `PitchArrow::SyncObjects` | 588 | NO_DC3_FILE | 96.46 | 96.46 | no counterpart file in ../dc3-decomp |
| bandobj/ReviewDisplay.cpp | `fn_8231F620` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/SongSectionController.cpp | `??$_M_splice_insert_dispatch@U?$_List_iterator@VContentPoolMapping@SongSectionController@@U?$_Const_traits@VCo` | 156 | NO_DC3_FILE | 99.87 | 99.87 | no counterpart file in ../dc3-decomp |
| bandobj/SongSectionController.cpp | `SongSectionController::Exit` | 4 | NO_DC3_FILE | 95.00 | 95.00 | no counterpart file in ../dc3-decomp |
| bandobj/SongSectionController.cpp | `SongSectionController::Handle` | 1240 | NO_DC3_FILE | 96.91 | 96.91 | no counterpart file in ../dc3-decomp |
| bandobj/SongSectionController.cpp | `SongSectionController::ResetAll` | 192 | NO_DC3_FILE | 95.83 | 95.83 | no counterpart file in ../dc3-decomp |
| bandobj/SongSectionController.cpp | `fn_8230C628` | 220 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/SongSectionController.cpp | `fn_8230EE88` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/SongSectionController.cpp | `fn_82310540` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/SongSectionController.cpp | `fn_823105A8` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/SongSectionController.cpp | `fn_823105F0` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/SongSectionController.cpp | `fn_82310638` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/SongSectionController.cpp | `fn_82310660` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/SongSectionController.cpp | `fn_82310688` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/SongSectionController.cpp | `fn_823106B0` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/StreakMeter.cpp | `StreakMeter::SetPitch` | 280 | NO_DC3_FILE | 99.86 | 99.86 | no counterpart file in ../dc3-decomp |
| bandobj/StreakMeter.cpp | `StreakMeter::SyncObjects` | 1328 | NO_DC3_FILE | 99.40 | 99.40 | no counterpart file in ../dc3-decomp |
| bandobj/StreakMeter.cpp | `fn_822DB278` | 32 | ANON | 99.75 | 99.75 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDir.cpp | `TrackPanelDir::ConfigureCrowdMeter` | 336 | NO_DC3_FILE | 98.01 | 98.01 | no counterpart file in ../dc3-decomp |
| bandobj/TrackPanelDir.cpp | `TrackPanelDir::UpdateTimeInfo` | 948 | NO_DC3_FILE | 99.09 | 99.09 | no counterpart file in ../dc3-decomp |
| bandobj/TrackPanelDir.cpp | `fn_823084F0` | 140 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDir.cpp | `fn_823F4168` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDir.cpp | `fn_823F4178` | 12 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDir.cpp | `fn_823F4188` | 88 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDir.cpp | `fn_823F41E0` | 136 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDir.cpp | `fn_823F43A0` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDir.cpp | `fn_823F43B0` | 12 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDir.cpp | `fn_823F43C0` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDir.cpp | `fn_823F43D0` | 20 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDir.cpp | `fn_823F43E8` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDir.cpp | `fn_823F4400` | 100 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDir.cpp | `fn_823F4498` | 112 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDir.cpp | `fn_823F4508` | 52 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDir.cpp | `fn_823F4540` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/TrackPanelDirBase.cpp | `fn_82359680` | 224 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/VocalTrackDir.cpp | `?Copy@UILabel@@$4PPPPPPPM@A@AAXPBVObject@Hmx@@W4CopyType@23@@Z` | 12 | NO_DC3_FILE | 0.00 | 0.00 | no counterpart file in ../dc3-decomp |
| bandobj/VocalTrackDir.cpp | `VocalTrackDir::PostLoad` | 3656 | NO_DC3_FILE | 99.34 | 99.34 | no counterpart file in ../dc3-decomp |
| bandobj/VocalTrackDir.cpp | `VocalTrackDir::SetRange` | 700 | NO_DC3_FILE | 93.71 | 93.71 | no counterpart file in ../dc3-decomp |
| bandobj/VocalTrackDir.cpp | `fn_822FA7B8` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/VocalTrackDir.cpp | `fn_822FC4F8` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/VocalTrackDir.cpp | `fn_827F4298` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| bandobj/VocalTrackDir.cpp | `fn_827F42C8` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/BeatMaster.cpp | `fn_8276F6F8` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/BeatMatchUtl.cpp | `GemNumSlots` | 84 | NO_DC3_FILE | 97.86 | 97.86 | no counterpart file in ../dc3-decomp |
| beatmatch/DrumTrackWatcherImpl.cpp | `DrumTrackWatcherImpl::CheckCymbal` | 88 | NO_DC3_FILE | 99.55 | 99.55 | no counterpart file in ../dc3-decomp |
| beatmatch/GameGem.cpp | `GameGem::PackRealGuitarData` | 108 | NO_DC3_FILE | 95.19 | 95.19 | no counterpart file in ../dc3-decomp |
| beatmatch/GameGemList.cpp | `??$__adjust_heap@PAVGameGem@@HV1@U?$less@VGameGem@@@stlpmtx_std@@@stlpmtx_std@@YAXPAVGameGem@@HHV1@U?$less@VGa` | 264 | NO_DC3_FILE | 99.82 | 99.82 | no counterpart file in ../dc3-decomp |
| beatmatch/GameGemList.cpp | `??$__partial_sort@PAVGameGem@@V1@U?$less@VGameGem@@@stlpmtx_std@@@stlpmtx_std@@YAXPAVGameGem@@000U?$less@VGame` | 220 | NO_DC3_FILE | 99.64 | 99.64 | no counterpart file in ../dc3-decomp |
| beatmatch/GameGemList.cpp | `GameGemList::AddGameGem` | 272 | NO_DC3_FILE | 97.34 | 97.34 | no counterpart file in ../dc3-decomp |
| beatmatch/GameGemList.cpp | `fn_8278C8D8` | 28 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/GameGemList.cpp | `fn_8278CD40` | 96 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/GameGemList.cpp | `fn_8278CDA4` | 12 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/GameGemList.cpp | `fn_8278CDB0` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/GameGemList.cpp | `fn_8278CDC0` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/GuitarController.cpp | `fn_827A2400` | 80 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/JoypadController.cpp | `JoypadController::GetVirtualSlot` | 496 | NO_DC3_FILE | 84.84 | 84.84 | no counterpart file in ../dc3-decomp |
| beatmatch/JoypadController.cpp | `JoypadController::IsCymbal` | 264 | NO_DC3_FILE | 99.85 | 99.85 | no counterpart file in ../dc3-decomp |
| beatmatch/KeyboardTrackWatcherImpl.cpp | `KeyboardTrackWatcherImpl::CheckForFatFinger` | 300 | NO_DC3_FILE | 95.73 | 95.73 | no counterpart file in ../dc3-decomp |
| beatmatch/MasterAudio.cpp | `MasterAudio::FillSwing` | 140 | NO_DC3_FILE | 76.86 | 76.86 | no counterpart file in ../dc3-decomp |
| beatmatch/MasterAudio.cpp | `MasterAudio::SetButtonMashingMode` | 88 | NO_DC3_FILE | 86.50 | 86.50 | no counterpart file in ../dc3-decomp |
| beatmatch/MasterAudio.cpp | `fn_8277D3C8` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/MasterAudio.cpp | `fn_8277F838` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/RGUtl.cpp | `AddChordLevel` | 488 | NO_DC3_FILE | 99.22 | 99.22 | no counterpart file in ../dc3-decomp |
| beatmatch/SlotChannelMapping.cpp | `fn_82793E44` | 44 | ANON | 93.45 | 93.45 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/SlotChannelMapping.cpp | `fn_82793E70` | 40 | ANON | 99.80 | 99.80 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/SlotChannelMapping.cpp | `fn_82793F8C` | 40 | ANON | 99.30 | 99.30 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/SongData.cpp | `??$_Param_Construct@V?$vector@FV?$StlNodeAlloc@F@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_std@@YAXPAV?$vector@F` | 60 | NO_DC3_FILE | 0.00 | 0.00 | no counterpart file in ../dc3-decomp |
| beatmatch/SongData.cpp | `SongData::Load` | 612 | NO_DC3_FILE | 98.82 | 98.82 | no counterpart file in ../dc3-decomp |
| beatmatch/SongData.cpp | `SongData::TrimOverlappingGems` | 536 | NO_DC3_FILE | 87.41 | 87.41 | no counterpart file in ../dc3-decomp |
| beatmatch/SongData.cpp | `SongData::UnflipGems` | 656 | NO_DC3_FILE | 95.70 | 95.70 | no counterpart file in ../dc3-decomp |
| beatmatch/SongData.cpp | `SongData::ValidateVocalSPPhrases` | 720 | NO_DC3_FILE | 99.56 | 99.56 | no counterpart file in ../dc3-decomp |
| beatmatch/SongData.cpp | `fn_82771DD8` | 184 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/SongData.cpp | `fn_82773394` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/SongData.cpp | `fn_82774068` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/SongData.cpp | `fn_82774330` | 84 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/SongData.cpp | `fn_82774388` | 84 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/SongData.cpp | `fn_827743E0` | 84 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/SongData.cpp | `fn_82774438` | 84 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/SongData.cpp | `fn_82774514` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| beatmatch/SongParser.cpp | `SongParser::HandleRGGemStop` | 2176 | NO_DC3_FILE | 97.38 | 97.38 | no counterpart file in ../dc3-decomp |
| beatmatch/SongParser.cpp | `SongParser::HandleRGTrillStop` | 276 | NO_DC3_FILE | 99.93 | 99.93 | no counterpart file in ../dc3-decomp |
| beatmatch/SongParser.cpp | `SongParser::ParseText` | 696 | NO_DC3_FILE | 98.68 | 98.68 | no counterpart file in ../dc3-decomp |
| beatmatch/SongParser.cpp | `SongParser::Reset` | 960 | NO_DC3_FILE | 99.98 | 99.98 | no counterpart file in ../dc3-decomp |
| beatmatch/SongParser.cpp | `SongParser::StartVocalNote` | 1104 | NO_DC3_FILE | 98.51 | 98.51 | no counterpart file in ../dc3-decomp |
| beatmatch/Submix.cpp | `?_M_insert_overflow_aux@?$vector@V?$list@HV?$StlNodeAlloc@H@stlpmtx_std@@@stlpmtx_std@@V?$StlNodeAlloc@V?$list` | 176 | NO_DC3_FILE | 99.89 | 99.89 | no counterpart file in ../dc3-decomp |
| beatmatch/TrackWatcherImpl.cpp | `RndParticleSysAnim::Load` | 508 | NO_DC3_FILE | 99.96 | 99.96 | no counterpart file in ../dc3-decomp |
| beatmatch/TrackWatcherImpl.cpp | `TrackWatcherImpl::ClosestUnplayedGem` | 140 | NO_DC3_FILE | 99.14 | 99.14 | no counterpart file in ../dc3-decomp |
| beatmatch/TrackWatcherImpl.cpp | `TrackWatcherImpl::GetNextRoll` | 104 | NO_DC3_FILE | 99.23 | 99.23 | no counterpart file in ../dc3-decomp |
| beatmatch/TrackWatcherImpl.cpp | `TrackWatcherImpl::TrackWatcherImpl` | 1080 | NO_DC3_FILE | 99.98 | 99.98 | no counterpart file in ../dc3-decomp |
| beatmatch/VocalNoteList.cpp | `VocalNoteList::NotesDone` | 1836 | NO_DC3_FILE | 98.65 | 98.65 | no counterpart file in ../dc3-decomp |
| beatmatch/VocalNoteList.cpp | `fn_82782950` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| dsp/PitchDetector.cpp | `PitchDetector::AnalyzeBlock` | 1780 | NO_DC3_FILE | 96.27 | 96.27 | no counterpart file in ../dc3-decomp |
| dsp/SndAnalysis.cpp | `FindCCPeak` | 920 | NO_DC3_FILE | 97.17 | 97.17 | no counterpart file in ../dc3-decomp |
| dsp/SndAnalysis.cpp | `RefinePeriod2` | 352 | NO_DC3_FILE | 99.98 | 99.98 | no counterpart file in ../dc3-decomp |
| dsp/SndAnalysis.cpp | `ShiftedDotProduct` | 356 | NO_DC3_FILE | 80.56 | 80.56 | no counterpart file in ../dc3-decomp |
| dsp/VibratoDetector.cpp | `VibratoDetector::Detect` | 376 | NO_DC3_FILE | 96.54 | 96.54 | no counterpart file in ../dc3-decomp |
| flow/Flow.cpp | `??$__destroy_range@PAUDynamicPropertyEntry@Flow@@U12@@stlpmtx_std@@YAXPAUDynamicPropertyEntry@Flow@@00@Z` | 80 | NO_QNAME | 99.70 | 99.70 | could not derive a qualified name from the symbol |
| flow/Flow.cpp | `??1?$vector@VString@@V?$StlNodeAlloc@VString@@@stlpmtx_std@@@stlpmtx_std@@QAA@XZ` | 136 | NO_QNAME | 0.00 | 0.00 | could not derive a qualified name from the symbol |
| flow/Flow.cpp | `fn_822D8D48` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| flow/Flow.cpp | `fn_82574D90` | 100 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowCommand.cpp | `?_M_create_node@?$list@VDataNode@@V?$StlNodeAlloc@VDataNode@@@stlpmtx_std@@@stlpmtx_std@@IAAPAU_List_node_base` | 72 | NOT_FOUND | 99.67 | 99.67 | definition not located in either tree by the extractor |
| flow/FlowIf.cpp | `fn_823C7000` | 136 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowIf.cpp | `fn_823C7088` | 48 | ANON | 99.42 | 99.42 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowIf.cpp | `fn_823C70B8` | 48 | ANON | 99.42 | 99.42 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowIf.cpp | `fn_823C75D8` | 68 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowNode.cpp | `fn_82308470` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowOnStop.cpp | `??$New@VRndMesh@@@Object@Hmx@@SAPAVRndMesh@@XZ` | 72 | NO_QNAME | 0.00 | 0.00 | could not derive a qualified name from the symbol |
| flow/FlowOnStop.cpp | `fn_822DC828` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowOnStop.cpp | `fn_826B2918` | 68 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowSetProperty.cpp | `fn_827C6708` | 204 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowSetProperty.cpp | `fn_827C67D8` | 232 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowSwitchCase.cpp | `fn_823B08D0` | 68 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowSwitchCase.cpp | `fn_823B0AA8` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowSwitchCase.cpp | `fn_823B0AD0` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowValueCase.cpp | `fn_823BA5AC` | 40 | ANON | 99.30 | 99.30 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowValueCase.cpp | `fn_823BA5D4` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowValueCase.cpp | `fn_823BA5FC` | 40 | ANON | 99.30 | 99.30 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowValueCase.cpp | `fn_823BA624` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| flow/FlowValueCase.cpp | `fn_823BA64C` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| hamobj/RhythmDetector.cpp | `fn_822703A8` | 36 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| hamobj/RhythmDetector.cpp | `fn_822703D0` | 68 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| math/Color.cpp | `InterpTangent` | 280 | OURS_ONLY | 99.57 | 99.57 | definition not present in the DC3 file |
| math/Geo.cpp | `??$_S_sort@VBSPFace@@V?$StlNodeAlloc@VBSPFace@@@stlpmtx_std@@U?$less@VBSPFace@@@3@@stlpmtx_std@@YAXAAV?$list@V` | 424 | NO_QNAME | 97.97 | 97.97 | could not derive a qualified name from the symbol |
| math/Geo.cpp | `??O@YA_NABVSphere@@ABVFrustum@@@Z` | 372 | NO_QNAME | 99.89 | 99.89 | could not derive a qualified name from the symbol |
| math/Geo.cpp | `?Intersect@@YA_NABVSegment@@ABVTriangle@@_NAAM@Z` | 432 | PARTIAL | 97.19 | 97.19 | Geo overload set: the extractor paired overloads wrongly, so the batch swap failed to compile; checked by hand -- Vector3/Box overload body is IDENTICAL to DC3 (residual: retail keeps tmin in f12 across iterations, codegen), the others carry DC3-image-tuned spellings DC3 itself scores at or below ours |
| math/Geo.cpp | `?Intersect@@YA_NABVTriangle@@ABVBox@@@Z` | 848 | PARTIAL | 97.16 | 97.16 | Geo overload set: the extractor paired overloads wrongly, so the batch swap failed to compile; checked by hand -- Vector3/Box overload body is IDENTICAL to DC3 (residual: retail keeps tmin in f12 across iterations, codegen), the others carry DC3-image-tuned spellings DC3 itself scores at or below ours |
| math/Geo.cpp | `?Intersect@@YA_NABVVector3@@0ABVBox@@AAM2@Z` | 188 | PARTIAL | 93.94 | 93.94 | Geo overload set: the extractor paired overloads wrongly, so the batch swap failed to compile; checked by hand -- Vector3/Box overload body is IDENTICAL to DC3 (residual: retail keeps tmin in f12 across iterations, codegen), the others carry DC3-image-tuned spellings DC3 itself scores at or below ours |
| math/Geo.cpp | `?Intersect@@YA_NABVVector3@@0ABVTriangle@@AAM@Z` | 296 | PARTIAL | 98.92 | 98.92 | Geo overload set: the extractor paired overloads wrongly, so the batch swap failed to compile; checked by hand -- Vector3/Box overload body is IDENTICAL to DC3 (residual: retail keeps tmin in f12 across iterations, codegen), the others carry DC3-image-tuned spellings DC3 itself scores at or below ours |
| math/Geo.cpp | `?Multiply@@YAXABVPlane@@ABVTransform@@AAV1@@Z` | 256 | DIFF | 98.12 | 98.12 | DC3 body compiled, tried in batch: row flat (98.12->98.12); kept ours |
| math/Geo.cpp | `BSPFace::Update` | 612 | DIFF | 97.79 | 99.47 | ADOPTED DC3 body (batch swap) |
| math/Geo.cpp | `Clip` | 512 | DIFF | 97.61 | 97.61 | DC3 body compiled, tried in batch: row DOWN (97.61->95.07); kept ours |
| math/Geo.cpp | `Frustum::Set` | 288 | DIFF | 99.21 | 99.21 | DC3 body compiled, tried in batch: row DOWN (99.21->78.96); kept ours |
| math/Geo.cpp | `MakeBSPTree` | 1580 | DIFF | 95.45 | 97.27 | HAND-PORT (partial): retail early-return tail; explicit clears kept on left-fail path (DC3 form shrinks frame 0x10, funclets 100->99.8) |
| math/Geo.cpp | `Sphere::GrowToContain` | 372 | DIFF | 99.12 | 99.46 | ADOPTED DC3 body (batch swap) |
| math/Geo.cpp | `fn_822BB3C8` | 128 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| math/Interp.cpp | `ATanInterpolator::Reset` | 220 | DIFF | 99.82 | 99.82 | differs from DC3; overloads could not be paired one-to-one, not swapped |
| math/Interp.cpp | `InvExpInterpolator::Eval` | 100 | OURS_ONLY | 99.60 | 99.60 | definition not present in the DC3 file |
| math/Interp.cpp | `LinearInterpolator::Reset` | 220 | OURS_ONLY | 99.82 | 99.82 | definition not present in the DC3 file |
| math/Rot.cpp | `?Multiply@@YAXABVVector3@@ABVQuat@Hmx@@AAV1@@Z` | 192 | DIFF | 85.73 | 85.73 | DC3 body compiled, tried in batch: row flat (85.73->85.73); kept ours |
| math/Rot.cpp | `?Normalize@@YAXABVQuat@Hmx@@AAV12@@Z` | 136 | SAME | 99.71 | 99.71 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| math/Rot.cpp | `Hmx::Quat::Set` | 424 | SAME | 98.16 | 98.16 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| math/Rot.cpp | `MakeEulerScale` | 224 | DIFF | 62.07 | 62.07 | DC3 body compiled, tried in batch: row DOWN (62.07->58.02); kept ours |
| math/Rot.cpp | `MakeRotQuat` | 248 | DIFF | 89.42 | 89.42 | DC3 body compiled, tried in batch: row flat (89.42->89.42); kept ours |
| math/Rot.cpp | `MakeScale` | 196 | SAME | 94.27 | 94.27 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| math/mtx.cpp | `?Multiply@@YAXABVMatrix3@Hmx@@0AAV12@@Z` | 680 | DIFF | 74.82 | 74.82 | DC3 body compiled, tried in batch: row flat (74.82->74.82); kept ours |
| math/mtx.cpp | `?Normalize@@YAXABVVector3@@AAV1@@Z` | 128 | OURS_ONLY | 99.38 | 99.38 | definition not present in the DC3 file |
| math/mtx.cpp | `FastInvert` | 232 | DIFF | 99.50 | 99.50 | DC3 body compiled, tried in batch: row DOWN (99.50->30.53); kept ours |
| meta/Achievements.cpp | `??$_M_allocate_and_copy@PBUXUSER_ACHIEVEMENT@@@?$vector@UXUSER_ACHIEVEMENT@@V?$StlNodeAlloc@UXUSER_ACHIEVEMENT` | 100 | NO_QNAME | 99.80 | 99.80 | could not derive a qualified name from the symbol |
| meta/Achievements.cpp | `Achievements::Submit` | 136 | DIFF | 93.53 | 93.53 | DC3 body does not compile here (C2511: 'void Achievements::Submit(int,Symbol,int)' : overloaded member function not found ); DC3-revision API/members |
| meta/ButtonHolder.cpp | `fn_827B869C` | 20 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/DeJitterPanel.cpp | `fn_827B36E0` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/FixedSizeSaveable.cpp | `fn_827A28E0` | 20 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/HAQManager.cpp | `fn_82BB1540` | 68 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/HAQManager.cpp | `fn_82BB1588` | 96 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/HAQManager.cpp | `fn_82BB1600` | 224 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/HAQManager.cpp | `fn_82BB1788` | 460 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/HAQManager.cpp | `fn_82BB1954` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| meta/Jukebox.cpp | `fn_827B1894` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| meta/MemcardMgr_Xbox.cpp | `Friend::SetName` | 68 | NOT_FOUND | 0.00 | 0.00 | definition not located in either tree by the extractor |
| meta/MemcardMgr_Xbox.cpp | `MemcardMgr::ThreadDone` | 364 | DIFF | 98.70 | 98.70 | DC3 body compiled, tried in batch: row DOWN (98.70->96.76); kept ours |
| meta/MemcardMgr_Xbox.cpp | `fn_8251BBC4` | 40 | ANON | 99.30 | 99.30 | anonymous fn_ row; no name to pair with DC3 |
| meta/MoviePanel.cpp | `MetaInit` | 176 | OURS_ONLY | 99.77 | 99.77 | definition not present in the DC3 file |
| meta/MoviePanel.cpp | `MoviePanel::SyncProperty` | 228 | DIFF | 99.91 | 100.00 | ADOPTED DC3 body (batch swap) |
| meta/Profile.cpp | `fn_827A5018` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/SongMgr.cpp | `fn_8257A414` | 40 | ANON | 99.80 | 99.80 | anonymous fn_ row; no name to pair with DC3 |
| meta/SongMgr.cpp | `fn_8257A43C` | 40 | ANON | 99.30 | 99.30 | anonymous fn_ row; no name to pair with DC3 |
| meta/SongMgr.cpp | `fn_8257A464` | 40 | ANON | 99.80 | 99.80 | anonymous fn_ row; no name to pair with DC3 |
| meta/SongMgr.cpp | `fn_8257A48C` | 40 | ANON | 93.90 | 93.90 | anonymous fn_ row; no name to pair with DC3 |
| meta/SongMgr.cpp | `fn_8257A4B4` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/SongMgr.cpp | `fn_8257A4DC` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/SongMgr.cpp | `fn_8257A504` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/SongMgr.cpp | `fn_827A3608` | 80 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/SongMgr.cpp | `fn_827A9D64` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| meta/StoreEnumeration.cpp | `XboxEnumeration::Poll` | 452 | DIFF | 98.94 | 98.94 | DC3 is a newer revision (debug-print arms, XMARKETPLACE struct); not taken |
| meta/StoreEnumeration.cpp | `XboxEnumeration::Start` | 328 | DIFF | 99.39 | 99.39 | DC3 body does not compile here (C2065: 'mOfferIDsCur' : undeclared identifier); DC3-revision API/members |
| meta/StoreEnumeration.cpp | `fn_827B8630` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/StoreEnumeration.cpp | `fn_827B8680` | 12 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/StoreEnumeration.cpp | `fn_827B8690` | 12 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/StoreOffer.cpp | `StoreOffer::StoreOffer` | 948 | DIFF | 99.68 | 99.68 | DC3 body does not compile here (C2065: 'date' : undeclared identifier); DC3-revision API/members |
| meta/StorePanel.cpp | `StorePanel::CheckOut` | 260 | DIFF | 55.66 | 96.52 | HAND-PORT, BEHAVIOUR: XboxPurchaser flags = server player ID (was 0); Net class split to net/NetCore.h to reach TheNet |
| meta/StorePanel.cpp | `StorePanel::OnMsg` | 172 | DIFF | 98.44 | 98.44 | DC3 body compiled, tried in batch: row DOWN (98.44->17.09); kept ours |
| meta/StorePanel.cpp | `fn_827B74D0` | 96 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| meta/StorePreviewMgr.cpp | `??_EStorePreviewMgr@@UAAPAXI@Z` | 68 | NO_QNAME | 0.00 | 0.00 | could not derive a qualified name from the symbol |
| meta/StorePreviewMgr.cpp | `StorePreviewMgr::Handle` | 704 | DIFF | 99.95 | 99.95 | DC3 body does not compile here (C2660: 'StorePreviewMgr::SetCurrentPreviewFile' : function does not take 2 arguments); DC3-revision API/members |
| meta/StorePreviewMgr.cpp | `fn_827B2660` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| meta/StorePreviewMgr.cpp | `fn_827B26A8` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| meta/StorePreviewMgr.cpp | `fn_827B26F0` | 40 | ANON | 99.80 | 99.80 | anonymous fn_ row; no name to pair with DC3 |
| meta/StorePreviewMgr.cpp | `fn_827B2738` | 40 | ANON | 99.80 | 99.80 | anonymous fn_ row; no name to pair with DC3 |
| midi/MidiParser.cpp | `MidiParser::PushIdle` | 484 | SAME | 99.75 | 99.75 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| midi/MidiParser.cpp | `fn_827E4960` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| midi/MidiParser.cpp | `fn_827E53E0` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| midi/MidiParser.cpp | `fn_827E6178` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| midi/MidiParser.cpp | `fn_827E6180` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| midi/MidiParser.cpp | `fn_827EA32C` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| midi/MidiReader.cpp | `pow` | 80 | DIFF | 90.00 | 100.00 | HAND-PORT: PowInt template (value-identical) |
| movie/Movie.cpp | `Movie::Impl::Begin` | 592 | OURS_ONLY | 99.97 | 99.97 | definition not present in the DC3 file |
| movie/Movie.cpp | `Movie::Impl::DiscContentionPublish` | 196 | OURS_ONLY | 98.78 | 98.78 | definition not present in the DC3 file |
| movie/Movie.cpp | `Movie::Impl::Draw` | 380 | OURS_ONLY | 99.58 | 99.58 | definition not present in the DC3 file |
| movie/Movie.cpp | `MovieInternalBuffers::New` | 992 | OURS_ONLY | 99.55 | 99.55 | definition not present in the DC3 file |
| movie/Splash.cpp | `Splash::Splash` | 420 | DIFF | 92.27 | 100.00 | ADOPTED DC3 body (batch swap) |
| net/HttpGet.cpp | `HttpGet::Poll` | 720 | DIFF | 97.13 | 97.13 | DC3 is a newer revision (timeouts, mFlags, mHttpStatus); not taken |
| net/HttpGet.cpp | `ParseHeader` | 208 | DIFF | 98.75 | 98.75 | DC3 body compiled, tried in batch: row DOWN (98.75->94.23); kept ours |
| net/JsonUtils.cpp | `JsonArray::AddMember` | 72 | OURS_ONLY | 98.89 | 98.89 | definition not present in the DC3 file |
| net/JsonUtils.cpp | `JsonConverter::GetValue` | 140 | DIFF | 97.14 | 97.14 | DC3 body compiled, tried in batch: row DOWN (97.14->95.43); kept ours |
| net/JsonUtils.cpp | `JsonObject::GetType` | 8 | SAME | 0.00 | 0.00 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| net/JsonUtils.cpp | `fn_827DCA78` | 44 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| net/JsonUtils.cpp | `fn_82B81F40` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| net/SessionJobs_Xbox.cpp | `MakeSessionJob::IsFinished` | 300 | DIFF | 91.67 | 91.67 | DC3 body compiled, tried in batch: row DOWN (91.67->31.36); kept ours |
| net/SessionJobs_Xbox.cpp | `MakeSessionJob::Start` | 308 | DIFF | 94.73 | 94.73 | RB3-specific session flags; DC3 body (mSessionFlags) not applicable |
| net/WebSvcMgrCurl.cpp | `??$make_pair@VString@@V1@@stlpmtx_std@@YA?AU?$pair@VString@@V1@@0@VString@@0@Z` | 88 | NO_QNAME | 99.32 | 99.32 | could not derive a qualified name from the symbol |
| net/WebSvcMgrCurl.cpp | `fn_822E7590` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| net/WebSvcMgrCurl.cpp | `fn_822E75B8` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| net/WebSvcMgrCurl.cpp | `fn_822E75E0` | 64 | ANON | 99.69 | 99.69 | anonymous fn_ row; no name to pair with DC3 |
| net/XLSPConnection.cpp | `XLSPConnection::Poll` | 388 | DIFF | 99.79 | 99.79 | DC3 body compiled, tried in batch: row DOWN (99.79->99.64); kept ours |
| net/XLSPConnection.cpp | `fn_827D9E30` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| net/curl/lib/ssluse.c | `fn_82AE51D0` | 216 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| net/json-c/json_tokener.c | `json_tokener_parse_ex` | 4872 | DIFF | 93.20 | 99.25 | ADOPTED DC3 body (batch swap) |
| obj/DataFile.cpp | `ParseNode` | 2432 | DIFF | 99.98 | 99.98 | DC3 body does not compile here (C2676: binary '++' : 'DataType' does not define this operator or a conversion to a type ac); DC3-revision API/members |
| obj/DataFlex.c | `fn_8276E210` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataFlex.c | `fn_8276E218` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataFunc.cpp | `Quasiquote` | 424 | DIFF | 99.91 | 99.91 | DC3 body compiled, tried in batch: row DOWN (99.91->98.44); kept ours |
| obj/DataFunc.cpp | `fn_82763988` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataFunc.cpp | `fn_82B8E790` | 88 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataNode.cpp | `??A?$map@VSymbol@@VDataNode@@U?$less@VSymbol@@@stlpmtx_std@@V?$StlNodeAlloc@U?$pair@$$CBVSymbol@@VDataNode@@@s` | 220 | NO_QNAME | 63.73 | 63.73 | could not derive a qualified name from the symbol |
| obj/DataNode.cpp | `?_M_create_node@?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@U?$pair@$$CBVSymbol@@VDataNode@@@3@U?$_Sele` | 84 | NOT_FOUND | 99.76 | 99.76 | definition not located in either tree by the extractor |
| obj/DataNode.cpp | `fn_82C3FFB0` | 12 | ANON | 98.33 | 98.33 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataNode.cpp | `fn_82C3FFC0` | 12 | ANON | 98.33 | 98.33 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataNode.cpp | `fn_82C40080` | 20 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataNode.cpp | `fn_82C40098` | 1372 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataNode.cpp | `fn_82C405F8` | 52 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataNode.cpp | `fn_82C40630` | 72 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataNode.cpp | `fn_82C40678` | 12 | ANON | 98.33 | 98.33 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataNode.cpp | `fn_82C40688` | 52 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataNode.cpp | `fn_82C40718` | 52 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataNode.cpp | `fn_82C40898` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataNode.cpp | `fn_82C40A20` | 12 | ANON | 98.33 | 98.33 | anonymous fn_ row; no name to pair with DC3 |
| obj/DataUtl.cpp | `fn_827690D0` | 80 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/Dir.cpp | `??$__find_if@PAVFilePath@@P6A_NABV1@@Z@stlpmtx_std@@YAPAVFilePath@@PAV1@0P6A_NABV1@@ZABUrandom_access_iterator` | 252 | NOT_FOUND | 0.00 | 0.00 | definition not located in either tree by the extractor |
| obj/Dir.cpp | `??$remove_copy_if@PAVFilePath@@PAV1@P6A_NABV1@@Z@stlpmtx_std@@YAPAVFilePath@@PAV1@00P6A_NABV1@@Z@Z` | 88 | NOT_FOUND | 0.00 | 0.00 | definition not located in either tree by the extractor |
| obj/Dir.cpp | `??$remove_if@PAVFilePath@@P6A_NABV1@@Z@stlpmtx_std@@YAPAVFilePath@@PAV1@0P6A_NABV1@@Z@Z` | 88 | NOT_FOUND | 0.00 | 0.00 | definition not located in either tree by the extractor |
| obj/Dir.cpp | `ObjectDir::PreLoad` | 3092 | DIFF | 99.70 | 99.70 | DC3 body does not compile here (C2661: 'BinStreamRev::BinStreamRev' : no overloaded function takes 3 arguments); DC3-revision API/members |
| obj/Dir.cpp | `ObjectDir::ResetViewports` | 532 | SAME | 98.35 | 98.35 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| obj/Dir.cpp | `ObjectDir::Save` | 2108 | DIFF | 99.58 | 99.58 | DC3 body does not compile here (C2065: 'mInlineProxyType' : undeclared identifier); DC3-revision API/members |
| obj/DirLoader.cpp | `DirLoader::Cleanup` | 548 | DIFF | 99.96 | 99.96 | DC3 body compiled, tried in batch: row DOWN (99.96->77.58); kept ours |
| obj/DirLoader.cpp | `DirLoader::SaveObjects` | 1044 | DIFF | 98.01 | 98.01 | DC3 body compiled, tried in batch: row DOWN (98.01->86.62); kept ours |
| obj/DirLoader.cpp | `fn_827588C8` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| obj/DirLoader.cpp | `fn_82759944` | 40 | ANON | 99.80 | 99.80 | anonymous fn_ row; no name to pair with DC3 |
| obj/MessageTimer.cpp | `fn_82272308` | 164 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/MessageTimer.cpp | `fn_82272548` | 196 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/MessageTimer.cpp | `fn_8242C710` | 92 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/Msg.cpp | `??$_M_find@VSymbol@@@?$_Rb_tree@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@U?$pair@$$CBVSymbol@@_N@3@U?$_Select1s` | 88 | NO_QNAME | 0.00 | 0.00 | could not derive a qualified name from the symbol |
| obj/Msg.cpp | `SetObjConcrete` | 104 | NOT_FOUND | 0.00 | 0.00 | definition not located in either tree by the extractor |
| obj/Msg.cpp | `fn_82766880` | 100 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/Object.cpp | `SetFileChecksumData` | 16 | NOT_FOUND | 0.00 | 0.00 | definition not located in either tree by the extractor |
| obj/Object.cpp | `fn_82272E40` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/Object.cpp | `fn_8275CE4C` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| obj/PropSync.cpp | `fn_824CB080` | 100 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/PropSync.cpp | `fn_824CB940` | 96 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/PropSync.cpp | `fn_824CE758` | 328 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/PropSync.cpp | `fn_824CE8F8` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| obj/PropSync.cpp | `fn_824CE900` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| oggvorbis/VorbisMem.cpp | `fn_8249B200` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| oggvorbis/block.c | `fn_82C2C8A0` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| oggvorbis/sharedbook.c | `fn_82C30B58` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/Archive.cpp | `Archive::GetFileInfo` | 432 | DIFF | 97.92 | 100.00 | ADOPTED DC3 body (batch swap) |
| os/Archive.cpp | `Archive::Merge` | 516 | DIFF | 93.71 | 100.00 | HAND-PORT: 5-arg FileEntry ctor, no local hash copy |
| os/Archive.cpp | `fn_825122B0` | 104 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/ArkFile.cpp | `ArkFile::ReadAsync` | 428 | PARTIAL | 98.97 | 98.97 | differs from DC3; overloads could not be paired one-to-one, not swapped |
| os/AsyncFile_Win.cpp | `AsyncFileWin::_ReadAsync` | 460 | DIFF | 94.39 | 94.39 | DC3 body compiled, tried in batch: row DOWN (94.39->82.43); kept ours |
| os/BlockMgr.cpp | `BlockMgr::Init` | 344 | DIFF | 99.94 | 99.94 | DC3 body compiled, tried in batch: row DOWN (99.94->86.80); kept ours |
| os/BlockMgr.cpp | `fn_82C3FC28` | 52 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/BlockMgr.cpp | `fn_82C3FC60` | 12 | ANON | 98.33 | 98.33 | anonymous fn_ row; no name to pair with DC3 |
| os/BlockMgr.cpp | `fn_82C3FC70` | 12 | ANON | 98.33 | 98.33 | anonymous fn_ row; no name to pair with DC3 |
| os/BlockMgr.cpp | `fn_82C3FC80` | 52 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/BlockMgr.cpp | `fn_82C3FCB8` | 56 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/BlockMgr.cpp | `fn_82C3FCF0` | 56 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/BlockMgr.cpp | `fn_82C3FD28` | 52 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/BlockMgr.cpp | `fn_82C3FD60` | 172 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/BlockMgr.cpp | `fn_82C3FE10` | 132 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/BlockMgr.cpp | `fn_82C3FE98` | 52 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/BlockMgr.cpp | `fn_82C3FED0` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/BlockMgr.cpp | `fn_82C3FEE0` | 56 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/CDReader.cpp | `ArkFilesInit` | 540 | SAME | 99.93 | 99.93 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| os/ContentMgr.cpp | `fn_8251F838` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/ContentMgr_Xbox.cpp | `XboxContent::Poll` | 424 | DIFF | 89.65 | 89.65 | DC3 body compiled, tried in batch: row DOWN (89.65->86.59); kept ours |
| os/Debug.cpp | `Debug::Fail` | 264 | DIFF | 94.09 | 94.09 | DC3 body does not compile here (C2084: function 'void Debug::Fail(const char *,void *)' already has a body); DC3-revision API/members |
| os/Debug.cpp | `fn_8250F7F8` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| os/Debug.cpp | `fn_82510798` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| os/Debug.cpp | `fn_825107C0` | 24 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/Debug.cpp | `fn_82510AEC` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| os/Debug.cpp | `fn_82510E44` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| os/Debug.cpp | `fn_82510E6C` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| os/Debug.cpp | `fn_82510E94` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| os/File.cpp | `FileLocalize` | 408 | DIFF | 99.95 | 99.95 | DC3 body compiled, tried in batch: row DOWN (99.95->53.45); kept ours |
| os/File.cpp | `FileMakePath` | 792 | DIFF | 94.70 | 94.70 | DC3 body compiled, tried in batch: row DOWN (94.70->5.64); kept ours |
| os/File.cpp | `NewFile` | 424 | DIFF | 84.39 | 84.39 | DC3 body compiled, tried in batch: row DOWN (84.39->81.27); kept ours |
| os/File.cpp | `fn_82517588` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| os/File.cpp | `fn_82518348` | 12 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/FileCache.cpp | `fn_82519FF8` | 12 | ANON | 60.00 | 60.00 | anonymous fn_ row; no name to pair with DC3 |
| os/HDCache.cpp | `HDCache::Init` | 1504 | PARTIAL | 99.79 | 99.79 | DC3 body compiled, tried in batch: row DOWN (99.79->97.06); kept ours |
| os/Joypad.cpp | `JoypadPollCommon` | 2468 | DIFF | 94.21 | 94.21 | DC3 body does not compile here (C2039: 'mNumAnalogSticks' : is not a member of 'JoypadData'); DC3-revision API/members |
| os/Joypad.cpp | `TrueColor::NuipTrueColorSetPlayer` | 24 | NOT_FOUND | 0.00 | 0.00 | definition not located in either tree by the extractor |
| os/JoypadClient.cpp | `JoypadClient::Poll` | 116 | SAME | 93.59 | 93.59 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| os/Joypad_Xinput.cpp | `JoypadResetXboxPC` | 172 | DIFF | 99.88 | 99.88 | DC3 body does not compile here (C2039: 'GetBool' : is not a member of 'UserMgr'); DC3-revision API/members |
| os/Joypad_Xinput.cpp | `ReadSingleXinputJoypad` | 812 | DIFF | 99.78 | 99.78 | DC3 body does not compile here (C2065: 'gTriggerThreshold' : undeclared identifier); DC3-revision API/members |
| os/Memcard_Xbox.cpp | `MCContainerXbox::Mount` | 332 | DIFF | 87.95 | 87.95 | DC3 body compiled, tried in batch: row DOWN (87.95->81.33); kept ours |
| os/Memcard_Xbox.cpp | `MCContainerXbox::PrintDir` | 684 | SAME | 98.95 | 98.95 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| os/Memcard_Xbox.cpp | `MemcardXbox::ShowDeviceSelector` | 252 | DIFF | 93.06 | 93.06 | DC3 body does not compile here (C2511: 'void MemcardXbox::ShowDeviceSelector(const ContainerId &,Hmx::Object *,int,bool)' ); DC3-revision API/members |
| os/Memcard_Xbox.cpp | `fn_8252A8A0` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/NetworkSocket_Win.cpp | `WinSockSocket::RecvFrom` | 136 | DIFF | 96.91 | 100.00 | ADOPTED DC3 body (batch swap) |
| os/OnlineID.cpp | `fn_82524838` | 104 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/PlatformMgr.cpp | `fn_825150C8` | 72 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/PlatformMgr_Xbox.cpp | `PlatformMgr::Init` | 220 | DIFF | 98.09 | 98.09 | DC3 body does not compile here (C3861: 'SmartGlassInit': identifier not found); DC3-revision API/members |
| os/PlatformMgr_Xbox.cpp | `PlatformMgr::Poll` | 1844 | DIFF | 99.54 | 99.54 | DC3 body does not compile here (C2065: 'mTime' : undeclared identifier); DC3-revision API/members |
| os/PlatformMgr_Xbox.cpp | `PlatformMgr::ThreadDone` | 56 | OURS_ONLY | 96.43 | 96.43 | definition not present in the DC3 file |
| os/PlatformMgr_Xbox.cpp | `fn_8251BF80` | 24 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/PlatformMgr_Xbox.cpp | `fn_8251BF98` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/PlatformMgr_Xbox.cpp | `fn_8251C190` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/PlatformMgr_Xbox.cpp | `fn_8251C550` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/PlatformMgr_Xbox.cpp | `fn_8251C560` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/PlatformMgr_Xbox.cpp | `fn_8251C590` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/PlatformMgr_Xbox.cpp | `fn_8251CBC8` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/ProfilePicture.cpp | `fn_82B90208` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| os/ProfilePicture.cpp | `fn_82B902C8` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| os/System.cpp | `InitSystem` | 228 | DIFF | 99.89 | 99.89 | DC3 body compiled, tried in batch: row DOWN (99.89->89.95); kept ours |
| os/System.cpp | `PreInitSystem` | 652 | DIFF | 99.98 | 99.98 | DC3 body compiled, tried in batch: row DOWN (99.98->79.91); kept ours |
| os/System.cpp | `SetSystemArgs` | 240 | DIFF | 94.92 | 94.92 | DC3 body compiled, tried in batch: row DOWN (94.92->78.17); kept ours |
| os/System.cpp | `fn_82C42910` | 72 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/ThreadCall_Win.cpp | `ThreadCallPoll` | 252 | SAME | 92.22 | 92.22 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| os/UsbMidiGuitar.cpp | `UsbMidiGuitar::Poll` | 1244 | DIFF | 99.90 | 99.90 | DC3 body compiled, tried in batch: row flat (99.90->99.90); kept ours |
| os/UsbMidiGuitar.cpp | `fn_8251A018` | 80 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/UsbMidiGuitar.cpp | `fn_82C3F8F0` | 12 | ANON | 98.33 | 98.33 | anonymous fn_ row; no name to pair with DC3 |
| os/UsbMidiGuitar.cpp | `fn_82C3F900` | 84 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/UsbMidiGuitar.cpp | `fn_82C3F9D8` | 72 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/UsbMidiGuitar.cpp | `fn_82C3FA20` | 12 | ANON | 98.33 | 98.33 | anonymous fn_ row; no name to pair with DC3 |
| os/UsbMidiGuitar.cpp | `fn_82C3FA30` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/UsbMidiGuitar.cpp | `fn_82C3FA60` | 52 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/UsbMidiGuitar.cpp | `fn_82C3FA94` | 40 | ANON | 78.50 | 78.50 | anonymous fn_ row; no name to pair with DC3 |
| os/UsbMidiGuitar.cpp | `fn_82C3FAC0` | 56 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| os/UsbMidiGuitar.cpp | `fn_82C3FAF8` | 52 | ANON | 99.62 | 99.62 | anonymous fn_ row; no name to pair with DC3 |
| os/UsbMidiGuitar.cpp | `fn_82C3FB30` | 12 | ANON | 98.33 | 98.33 | anonymous fn_ row; no name to pair with DC3 |
| os/UsbMidiKeyboard.cpp | `UsbMidiKeyboard::Poll` | 1164 | DIFF | 98.12 | 98.78 | ADOPTED DC3 body (batch swap) |
| os/User.cpp | `RemoteUser::GetRemoteUser` | 8 | NOT_FOUND | 0.00 | 0.00 | definition not located in either tree by the extractor |
| rnddx9/CubeTex.cpp | `DxCam::ProjectZ` | 104 | OURS_ONLY | 82.42 | 82.42 | definition not present in the DC3 file |
| rnddx9/CubeTex.cpp | `DxCam::Select` | 528 | OURS_ONLY | 97.92 | 97.92 | definition not present in the DC3 file |
| rnddx9/CubeTex.cpp | `DxCubeTex::Sync` | 456 | DIFF | 99.96 | 99.96 | DC3 body does not compile here (C2664: 'IDirect3DDevice9_CreateCubeTexture' : cannot convert parameter 7 from 'D3DBaseText); DC3-revision API/members |
| rnddx9/CubeTex.cpp | `DxMultiMesh::DrawBatchedNewGfx` | 668 | OURS_ONLY | 99.27 | 99.27 | definition not present in the DC3 file |
| rnddx9/CubeTex.cpp | `DxMultiMesh::Init` | 104 | OURS_ONLY | 95.38 | 95.38 | definition not present in the DC3 file |
| rnddx9/CubeTex.cpp | `DxMultiMesh::UpdateGeometryBuffers` | 392 | OURS_ONLY | 99.90 | 99.90 | definition not present in the DC3 file |
| rnddx9/CubeTex.cpp | `DxParticleSys::DrawParticles` | 548 | OURS_ONLY | 99.61 | 99.61 | definition not present in the DC3 file |
| rnddx9/CubeTex.cpp | `DxParticleSys::DrawShowing` | 700 | OURS_ONLY | 97.94 | 97.94 | definition not present in the DC3 file |
| rnddx9/Mat.cpp | `fn_82741340` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| rnddx9/RenderState.cpp | `RndRenderState::SetBorderColor` | 88 | DIFF | 77.05 | 100.00 | ADOPTED DC3 body (batch swap) |
| rnddx9/RenderState.cpp | `RndRenderState::SetTextureClamp` | 136 | DIFF | 99.41 | 99.71 | ADOPTED DC3 body (batch swap) |
| rnddx9/Rnd.cpp | `D3DFORMAT_BitsPerPixel` | 368 | DIFF | 94.22 | 94.22 | DC3 body compiled, tried in batch: row DOWN (94.22->77.07); kept ours |
| rnddx9/Rnd.cpp | `DxRnd::DrawLine` | 360 | DIFF | 98.09 | 100.00 | ADOPTED DC3 body (batch swap) |
| rnddx9/Rnd.cpp | `DxRnd::Offscreen` | 116 | DIFF | 89.14 | 100.00 | HAND-PORT: D3DDevice_GetRenderTarget(Device(), 0), const Device() accessor |
| rnddx9/Rnd_Xbox.cpp | `??$__destroy_aux@V?$set@VSymbol@@U?$less@VSymbol@@@stlpmtx_std@@V?$StlNodeAlloc@VSymbol@@@3@@stlpmtx_std@@@stl` | 4 | NO_QNAME | 95.00 | 95.00 | could not derive a qualified name from the symbol |
| rnddx9/Rnd_Xbox.cpp | `CreateBackBuffers` | 300 | DIFF | 83.91 | 100.00 | ADOPTED DC3 body (batch swap) |
| rnddx9/Rnd_Xbox.cpp | `DxMesh::CacheFurTransform` | 480 | OURS_ONLY | 99.10 | 99.10 | definition not present in the DC3 file |
| rnddx9/Rnd_Xbox.cpp | `DxMesh::DrawFur` | 452 | OURS_ONLY | 98.85 | 98.85 | definition not present in the DC3 file |
| rnddx9/Rnd_Xbox.cpp | `DxMesh::DrawShowing` | 196 | OURS_ONLY | 95.92 | 95.92 | definition not present in the DC3 file |
| rnddx9/Rnd_Xbox.cpp | `DxMesh::DxMesh` | 440 | OURS_ONLY | 98.85 | 98.85 | definition not present in the DC3 file |
| rnddx9/Rnd_Xbox.cpp | `DxMesh::OnSync` | 692 | OURS_ONLY | 96.13 | 96.13 | definition not present in the DC3 file |
| rnddx9/Rnd_Xbox.cpp | `DxMesh::SetTransforms` | 448 | OURS_ONLY | 99.42 | 99.42 | definition not present in the DC3 file |
| rnddx9/Rnd_Xbox.cpp | `DxRnd::BeginTiling` | 228 | SAME | 99.44 | 99.44 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| rnddx9/Rnd_Xbox.cpp | `DxRnd::DoPointTests` | 1392 | DIFF | 99.96 | 99.96 | DC3 body compiled, tried in batch: row DOWN (99.96->93.52); kept ours |
| rnddx9/Rnd_Xbox.cpp | `DxRnd::DrawString` | 836 | DIFF | 99.92 | 99.92 | DC3 body compiled, tried in batch: row DOWN (99.92->93.00); kept ours |
| rnddx9/Rnd_Xbox.cpp | `DxRnd::FinishPostProcess` | 652 | DIFF | 85.75 | 85.75 | DC3 body does not compile here (C2653: 'BaseMaterial' : is not a class or namespace name); DC3-revision API/members |
| rnddx9/Rnd_Xbox.cpp | `DxRnd::SetAspect` | 80 | SAME | 99.75 | 99.75 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| rnddx9/Rnd_Xbox.cpp | `DxRnd::SetShrinkToSafeArea` | 84 | SAME | 99.76 | 99.76 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| rnddx9/Rnd_Xbox.cpp | `NgEnviron::ClassName` | 48 | NOT_FOUND | 0.00 | 0.00 | definition not located in either tree by the extractor |
| rnddx9/Rnd_Xbox.cpp | `PackVector` | 344 | OURS_ONLY | 92.83 | 92.83 | definition not present in the DC3 file |
| rnddx9/Rnd_Xbox.cpp | `fn_8273A598` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| rnddx9/ShaderMgr.cpp | `?SetPConstant@DxShaderMgr@@UAAXW4PShaderConstant@@ABVMatrix4@Hmx@@@Z` | 220 | PARTIAL | 96.36 | 96.36 | DC3 body compiled, tried in batch: row flat (96.36->96.36); kept ours |
| rnddx9/ShaderMgr.cpp | `?SetPConstant@DxShaderMgr@@UAAXW4PShaderConstant@@ABVVector4@@@Z` | 88 | PARTIAL | 69.55 | 83.18 | ADOPTED DC3 body (batch swap) |
| rnddx9/ShaderMgr.cpp | `?SetVConstant@DxShaderMgr@@UAAXW4VShaderConstant@@ABVMatrix4@Hmx@@@Z` | 220 | PARTIAL | 96.36 | 96.36 | DC3 body compiled, tried in batch: row flat (96.36->96.36); kept ours |
| rnddx9/ShaderMgr.cpp | `?SetVConstant@DxShaderMgr@@UAAXW4VShaderConstant@@ABVVector4@@@Z` | 88 | PARTIAL | 69.55 | 83.18 | ADOPTED DC3 body (batch swap) |
| rnddx9/ShaderMgr.cpp | `DxShaderMgr::LoadShaderFile` | 644 | DIFF | 84.83 | 100.00 | ADOPTED DC3 body (batch swap) |
| rnddx9/ShaderMgr.cpp | `DxTex::DxTex` | 156 | OURS_ONLY | 92.10 | 92.10 | definition not present in the DC3 file |
| rnddx9/ShaderMgr.cpp | `DxTex::MakeDrawTarget` | 168 | OURS_ONLY | 87.26 | 87.26 | definition not present in the DC3 file |
| rnddx9/ShaderMgr.cpp | `DxTex::ResolveMipChain` | 804 | OURS_ONLY | 82.90 | 82.90 | definition not present in the DC3 file |
| rnddx9/ShaderMgr.cpp | `DxTex::StartCompress` | 364 | OURS_ONLY | 95.90 | 95.90 | definition not present in the DC3 file |
| rnddx9/ShaderMgr.cpp | `DxTex::SyncBitmap` | 1644 | OURS_ONLY | 99.76 | 99.76 | definition not present in the DC3 file |
| rnddx9/ShaderMgr.cpp | `DxTex::UnlockBitmap` | 152 | OURS_ONLY | 97.37 | 97.37 | definition not present in the DC3 file |
| rnddx9/ShaderMgr.cpp | `XGSurfaceSize` | 112 | NOT_FOUND | 0.00 | 0.00 | definition not located in either tree by the extractor |
| rnddx9/ShaderMgr.cpp | `fn_827340F4` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| rnddx9/ShaderMgr.cpp | `fn_8273411C` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| rnddx9/ShaderMgr.cpp | `fn_82736330` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| rnddx9/ShaderMgr.cpp | `fn_82736BB0` | 48 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/ADSR.cpp | `??3ADSR@@SAXPAX@Z` | 4 | NOT_FOUND | 95.00 | 95.00 | definition not located in either tree by the extractor |
| synth/ByteGrinder.cpp | `_M_fill_insert` | 108 | NOT_FOUND | 99.44 | 99.44 | definition not located in either tree by the extractor |
| synth/ByteGrinder.cpp | `fn_82725AB8` | 92 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/ByteGrinder.cpp | `op0` | 88 | SAME | 99.55 | 99.55 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| synth/ByteGrinder.cpp | `op59` | 108 | DIFF | 98.81 | 100.00 | ADOPTED DC3 body (batch swap) |
| synth/ByteGrinder.cpp | `op6` | 92 | SAME | 99.57 | 99.57 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| synth/ByteGrinder.cpp | `op9` | 96 | DIFF | 92.21 | 99.58 | HAND-PORT: byte op through a u8 temp (value-identical) |
| synth/Common_Xbox.cpp | `fn_82BBA100` | 20 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/CompressionEffect.cpp | `CompressionEffect::Process` | 632 | DIFF | 99.94 | 99.94 | DC3 body compiled, tried in batch: row flat (99.94->99.94); kept ours |
| synth/DelayEffect.cpp | `DelayEffect::Process` | 384 | NO_DC3_FILE | 95.10 | 95.10 | no counterpart file in ../dc3-decomp |
| synth/DistortionEffect.cpp | `DistortionEffect::Process` | 168 | NO_DC3_FILE | 95.24 | 95.24 | no counterpart file in ../dc3-decomp |
| synth/EQEffect.cpp | `EQEffect::Process` | 476 | NO_DC3_FILE | 95.69 | 95.69 | no counterpart file in ../dc3-decomp |
| synth/EQEffect.cpp | `EQEffect::SetParameter` | 1644 | NO_DC3_FILE | 99.95 | 99.95 | no counterpart file in ../dc3-decomp |
| synth/Emitter.cpp | `SynthEmitter::SynthEmitter` | 496 | SAME | 86.13 | 86.13 | SAME body; retail inlines ObjPtr(owner,ptr) ctor, we call it; house gate RB3_TU_OBJPTR_FORCEINLINE_CTOR is inert in a PCH TU (synth/) |
| synth/Emitter.cpp | `fn_8271EAAC` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| synth/Faders.cpp | `Fader::DoFade` | 444 | DIFF | 93.09 | 93.09 | differs from DC3; overloads could not be paired one-to-one, not swapped |
| synth/Faders.cpp | `Fader::~Fader` | 112 | SAME | 99.82 | 99.82 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| synth/Faders.cpp | `FaderGroup::~FaderGroup` | 160 | DIFF | 97.50 | 97.50 | DC3 body compiled, tried in batch: row flat (97.50->97.50); kept ours |
| synth/Faders.cpp | `fn_8270C66C` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| synth/Faders.cpp | `fn_8270C694` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| synth/Faders.cpp | `fn_8270C6BC` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| synth/Faders.cpp | `fn_8270C6E4` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| synth/FlangerEffect.cpp | `FlangerEffect::Process` | 768 | NO_DC3_FILE | 77.93 | 77.93 | no counterpart file in ../dc3-decomp |
| synth/FxSendPitchShift.cpp | `fn_827130A0` | 12 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/MetaMusic.cpp | `MetaMusic::UpdateMix` | 1572 | DIFF | 99.85 | 99.85 | DC3 body does not compile here (C3861: 'GetStream': identifier not found); DC3-revision API/members |
| synth/MetaMusic.cpp | `MetaMusicLoader::MetaMusicLoader` | 132 | OURS_ONLY | 96.06 | 96.06 | definition not present in the DC3 file |
| synth/MicNull.cpp | `MicNull::GetRecentBuf` | 72 | DIFF | 93.33 | 100.00 | ADOPTED DC3 body (batch swap) |
| synth/MidiInstrument.cpp | `NoteVoiceInst::NoteVoiceInst` | 364 | DIFF | 97.55 | 97.55 | DC3 SetBankVolume operand order tried: row DOWN 97.55->95.41, reverted |
| synth/MidiInstrument.cpp | `fn_822C5758` | 96 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/MidiInstrument.cpp | `fn_82713678` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/MidiInstrument.cpp | `fn_82713740` | 36 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/MidiInstrument.cpp | `fn_827150A8` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/MidiSynth.cpp | `??5@YAAAVBinStream@@AAV0@AAUBitmapOverride@WorldDir@@@Z` | 380 | NO_QNAME | 97.89 | 97.89 | could not derive a qualified name from the symbol |
| synth/MidiSynth.cpp | `fn_82718D54` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| synth/MidiSynth.cpp | `fn_82718DFC` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| synth/MoggClip.cpp | `MoggClip::SetupPanInfo` | 124 | DIFF | 78.71 | 100.00 | ADOPTED DC3 body (batch swap) |
| synth/MoggClipMap.cpp | `??4MoggClipMap@@QAAAAV0@ABV0@@Z` | 104 | NO_QNAME | 88.08 | 88.08 | could not derive a qualified name from the symbol |
| synth/OggMap.cpp | `fn_82BB1980` | 516 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/OggMap.cpp | `fn_82BB1DC8` | 516 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/OggMap.cpp | `fn_82BB1FD8` | 1004 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/OggMap.cpp | `fn_82BB23C4` | 40 | ANON | 99.30 | 99.30 | anonymous fn_ row; no name to pair with DC3 |
| synth/OggMap.cpp | `fn_82BB23EC` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/OggMap.cpp | `fn_82BB2418` | 180 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/SampleData.cpp | `SampleData::Load` | 440 | DIFF | 99.95 | 99.95 | DC3 body does not compile here (C2065: 'mCRC' : undeclared identifier); DC3-revision API/members |
| synth/SampleData.cpp | `SampleMarker::SampleMarker` | 64 | NOT_FOUND | 67.19 | 67.19 | definition not located in either tree by the extractor |
| synth/SampleData.cpp | `fn_822A43A4` | 60 | ANON | 99.67 | 99.67 | anonymous fn_ row; no name to pair with DC3 |
| synth/SampleInst.cpp | `??$__uninitialized_copy@PBVSampleMarker@@PAV1@@stlpmtx_std@@YAPAVSampleMarker@@PBV1@0PAV1@ABU__false_type@0@@Z` | 96 | NO_QNAME | 0.00 | 0.00 | could not derive a qualified name from the symbol |
| synth/SampleInst.cpp | `fn_822A2E78` | 40 | ANON | 60.80 | 60.80 | anonymous fn_ row; no name to pair with DC3 |
| synth/SampleInst.cpp | `fn_822A2EA0` | 112 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/Sequence.cpp | `??$__uninitialized_copy@PAUUnlockable@?A0xf8e4b4b5@@PAU12@@stlpmtx_std@@YAPAUUnlockable@?A0xf8e4b4b5@@PAU12@00` | 96 | NO_QNAME | 0.00 | 0.00 | could not derive a qualified name from the symbol |
| synth/Sequence.cpp | `?_M_insert_overflow_aux@?$vector@V?$ObjPtr@VSeqInst@@@@V?$StlNodeAlloc@V?$ObjPtr@VSeqInst@@@@@stlpmtx_std@@@st` | 328 | NOT_FOUND | 99.94 | 99.94 | definition not located in either tree by the extractor |
| synth/Sequence.cpp | `fn_82709530` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/Sequence.cpp | `fn_82709E90` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/Sequence.cpp | `fn_8270A8D8` | 112 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/Sequence.cpp | `fn_8270A9B8` | 128 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/Sequence.cpp | `fn_8270B55C` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| synth/Sequence.cpp | `fn_8270B76C` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| synth/Sfx.cpp | `??$Find@VSequence@@@Synth@@QAAPAVSequence@@PBD_N@Z` | 128 | NO_QNAME | 0.00 | 0.00 | could not derive a qualified name from the symbol |
| synth/Sfx.cpp | `??$_Destroy@UNote@MidiChannel@@@stlpmtx_std@@YAXPAUNote@MidiChannel@@@Z` | 4 | NO_QNAME | 95.00 | 95.00 | could not derive a qualified name from the symbol |
| synth/Sfx.cpp | `SfxInst::SfxInst` | 432 | DIFF | 99.72 | 99.72 | DC3 body does not compile here (C2512: 'ObjPtrList<T1>' : no appropriate default constructor available); DC3-revision API/members |
| synth/Sfx.cpp | `fn_8271A138` | 224 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/StandardStream.cpp | `StandardStream::ConsumeData` | 616 | DIFF | 96.29 | 96.29 | differs from DC3; overloads could not be paired one-to-one, not swapped |
| synth/StandardStream.cpp | `StandardStream::Init` | 672 | DIFF | 99.97 | 99.97 | DC3 body does not compile here (C2660: 'Synth::NewStreamDecoder' : function does not take 3 arguments); DC3-revision API/members |
| synth/StandardStream.cpp | `StandardStream::InitInfo` | 872 | DIFF | 98.78 | 98.78 | DC3 body compiled, tried in batch: row DOWN (98.78->76.45); kept ours |
| synth/StandardStream.cpp | `StandardStream::PollStream` | 408 | DIFF | 98.07 | 98.07 | DC3 body compiled, tried in batch: row DOWN (98.07->97.06); kept ours |
| synth/StandardStream.cpp | `erase` | 92 | NOT_FOUND | 21.48 | 21.48 | definition not located in either tree by the extractor |
| synth/StreamNull.cpp | `Sfx::SynthPoll` | 4 | NOT_FOUND | 0.00 | 0.00 | definition not located in either tree by the extractor |
| synth/StreamNull.cpp | `StreamNull::StreamNull` | 288 | SAME | 99.93 | 99.93 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| synth/StreamNull.cpp | `fn_8271A0A8` | 12 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/StreamReceiver.cpp | `StreamReceiver::Poll` | 632 | DIFF | 99.26 | 99.26 | DC3 body compiled, tried in batch: row DOWN (99.26->92.87); kept ours |
| synth/Synth.cpp | `??_G?$ObjPtr@VSynthSample@@@@UAAPAXI@Z` | 76 | NO_QNAME | 0.00 | 0.00 | could not derive a qualified name from the symbol |
| synth/Synth.cpp | `Synth::DrawMeterScale` | 344 | DIFF | 98.95 | 98.95 | DC3 body does not compile here (C2065: 'sMeterConsts' : undeclared identifier); DC3-revision API/members |
| synth/Synth.cpp | `fn_826FCCE8` | 100 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/SynthSample.cpp | `SynthSample::Init` | 72 | DIFF | 99.44 | 99.44 | DC3 body compiled, tried in batch: row flat (99.44->99.44); kept ours |
| synth/Utl.cpp | `fn_827BFBF8` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/Utl.cpp | `fn_827BFC20` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/VoiceBeat.cpp | `EventTracker::Hit` | 488 | NO_DC3_FILE | 99.92 | 99.92 | no counterpart file in ../dc3-decomp |
| synth/VoiceBeat.cpp | `VoiceBeat::Analyze` | 1408 | NO_DC3_FILE | 86.55 | 86.55 | no counterpart file in ../dc3-decomp |
| synth/VorbisReader.cpp | `??$__destroy_range_aux@V?$reverse_iterator@PAV?$vector@FV?$StlNodeAlloc@F@stlpmtx_std@@@stlpmtx_std@@@stlpmtx_` | 100 | NO_QNAME | 99.80 | 99.80 | could not derive a qualified name from the symbol |
| synth/VorbisReader.cpp | `DecodeThreadEntry` | 272 | DIFF | 99.85 | 99.85 | DC3 body does not compile here (C2039: 'Unk24' : is not a member of 'VorbisReader'); DC3-revision API/members |
| synth/VorbisReader.cpp | `VorbisReader::CheckHmxHeader` | 632 | DIFF | 99.99 | 99.99 | DC3 body does not compile here (C2065: 'mVersion' : undeclared identifier); DC3-revision API/members |
| synth/VorbisReader.cpp | `VorbisReader::Poll` | 784 | DIFF | 99.95 | 99.95 | differs from DC3; overloads could not be paired one-to-one, not swapped |
| synth/VorbisReader.cpp | `VorbisReader::~VorbisReader` | 316 | DIFF | 97.41 | 97.41 | DC3 body tried by hand: row DOWN 97.41->93.38, reverted |
| synth/VorbisReader.cpp | `fn_82BB441C` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| synth/WahEffect.cpp | `WahEffect::Process` | 760 | NO_DC3_FILE | 91.76 | 91.76 | no counterpart file in ../dc3-decomp |
| synth/WavMgr.cpp | `fn_823EA988` | 28 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823EA9A4` | 20 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823EA9B8` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823EA9E8` | 132 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823EAA6C` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823EAA98` | 92 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823EAB00` | 88 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823EAB88` | 348 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823EACE4` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823EAD0C` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823EAD34` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F3EE0` | 28 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F3F00` | 28 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F3F20` | 24 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F3F38` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F3F40` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F3F48` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F3F50` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F3F58` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F3F60` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F3F68` | 100 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F3FD0` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F3FD8` | 176 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F4090` | 144 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F4120` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F4148` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/WavMgr.cpp | `fn_823F4158` | 12 | ANON | 80.00 | 80.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/tomcrypt/aes.c | `fn_82BBA020` | 40 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/tomcrypt/aes.c | `fn_82BBA048` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/tomcrypt/aes.c | `fn_82BBA058` | 12 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth/tomcrypt/ctr.c | `ctr_encrypt_fast` | 244 | SAME | 99.84 | 99.84 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| synth_xbox/ExternalMic.cpp | `ExternalMic::dataReady` | 236 | DIFF | 94.15 | 100.00 | HAND-PORT: counter decl order / guard |
| synth_xbox/ExternalMic.cpp | `ExternalMic::sampleProcessThread` | 1300 | DIFF | 99.84 | 99.84 | DC3 body does not compile here (C2664: 'XMicGetStatus' : cannot convert parameter 1 from 'DWORD' to 'HANDLE'); DC3-revision API/members |
| synth_xbox/FFT.cpp | `CalculateSinCosTable` | 260 | DIFF | 74.06 | 100.00 | ADOPTED DC3 body (batch swap) |
| synth_xbox/FFT.cpp | `SquareComplexTransposeVector` | 296 | DIFF | 79.24 | 97.03 | ADOPTED DC3 body (batch swap) |
| synth_xbox/FFT.cpp | `fft_altivec` | 3044 | DC3_ONLY | 0.00 | 0.00 | our definition not located by the extractor (macro/inline); DC3 has one |
| synth_xbox/FFT.cpp | `fft_matrix_forward_columnwise` | 1200 | DIFF | 55.08 | 79.39 | ADOPTED DC3 body (batch swap) |
| synth_xbox/FFT.cpp | `fft_matrix_inverse_columnwise` | 1160 | DIFF | 84.13 | 85.96 | ADOPTED DC3 body (batch swap) |
| synth_xbox/FFT.cpp | `fft_real_forward_altivec` | 964 | DC3_ONLY | 0.00 | 0.00 | our definition not located by the extractor (macro/inline); DC3 has one |
| synth_xbox/FFT.cpp | `fft_real_forward_scalar` | 420 | DIFF | 86.24 | 86.67 | ADOPTED DC3 body (batch swap) |
| synth_xbox/FFT.cpp | `fft_recursive` | 2128 | DC3_ONLY | 0.00 | 0.00 | our definition not located by the extractor (macro/inline); DC3 has one |
| synth_xbox/FFT.cpp | `fft_scalar` | 772 | DIFF | 76.24 | 80.26 | ADOPTED DC3 body (batch swap) |
| synth_xbox/FftIpp.cpp | `FftIpp::FftReal` | 188 | DIFF | 92.64 | 100.00 | ADOPTED DC3 body (batch swap) |
| synth_xbox/FxSendChorus.cpp | `fn_82B624B0` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/FxSendChorus.cpp | `fn_82B627F4` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/FxSendFlanger.cpp | `fn_82B63930` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/FxSendFlanger.cpp | `fn_82B63B34` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/FxSendReverb.cpp | `ReverbConvertI3DL2ToNative` | 740 | OURS_ONLY | 99.83 | 99.83 | definition not present in the DC3 file |
| synth_xbox/FxSendReverb.cpp | `fn_82B67B40` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/FxSendSynapse.cpp | `FxSendSynapse360::SyncEffectParams` | 356 | DIFF | 95.03 | 95.03 | DC3 body does not compile here (C2039: 'coeff1' : is not a member of 'DSP::SynapseBand'); DC3-revision API/members |
| synth_xbox/GranularSynth.cpp | `DSP::Synapse::GranularSynth::ExtractGranules` | 900 | SAME | 91.62 | 91.62 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| synth_xbox/GranularSynth.cpp | `fn_82B744E8` | 80 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/IPP_basicmath_xbox.cpp | `IPP::Add_InPlace` | 48 | NOT_FOUND | 82.08 | 82.08 | definition not located in either tree by the extractor |
| synth_xbox/IPP_basicmath_xbox.cpp | `IPP::Mul_InPlace` | 48 | NOT_FOUND | 82.08 | 82.08 | definition not located in either tree by the extractor |
| synth_xbox/MeterEffect.cpp | `MeterEffect::DoProcess` | 208 | DIFF | 95.48 | 95.48 | DC3 body tried by hand: row DOWN 95.48->79.23, reverted |
| synth_xbox/Mic.cpp | `ChatReceiver::ProcessChatData` | 636 | DIFF | 99.81 | 100.00 | ADOPTED DC3 body (batch swap) |
| synth_xbox/Mic.cpp | `MicManagerXbox::AddRemoteMic` | 252 | DIFF | 93.65 | 100.00 | HAND-PORT: ChatBuffer xuid as u64 member, DC3 decl order |
| synth_xbox/Mic.cpp | `MicXbox::AddData` | 460 | DIFF | 99.35 | 99.35 | DC3 body does not compile here (C2065: 'mPlaybackBuffer' : undeclared identifier); DC3-revision API/members |
| synth_xbox/Mic.cpp | `MicXbox::ReadChatBuffer` | 136 | DIFF | 99.35 | 99.35 | DC3 body does not compile here (C2065: 'mPlaybackBuffer' : undeclared identifier); DC3-revision API/members |
| synth_xbox/Mic.cpp | `fn_82B5EE28` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/PeakDetector.cpp | `DSP::Synapse::PeakDetector::Detect` | 948 | SAME | 96.88 | 96.88 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| synth_xbox/PitchCorrectedVoice.cpp | `DSP::Synapse::PitchCorrectedVoice::GetCorrection` | 528 | DIFF | 96.67 | 96.67 | DC3 body compiled, tried in batch: row flat (96.67->96.67); kept ours |
| synth_xbox/PitchDetector.cpp | `DSP::Synapse::PitchDetector::Detect` | 676 | DIFF | 92.63 | 100.00 | ADOPTED DC3 body (batch swap) |
| synth_xbox/SpectralAnalysis.cpp | `DSP::SpectralAnalysis::Analyze` | 496 | DIFF | 70.72 | 95.00 | ADOPTED DC3 body (batch swap) |
| synth_xbox/SpectralAnalysis.cpp | `FftIpp::FftRealCcs` | 128 | OURS_ONLY | 96.25 | 96.25 | definition not present in the DC3 file |
| synth_xbox/StreamReceiver360.cpp | `StreamReceiver360::SetSlipOffset` | 508 | DIFF | 97.32 | 97.32 | DC3 body compiled, tried in batch: row DOWN (97.32->94.01); kept ours |
| synth_xbox/StreamReceiver360.cpp | `StreamReceiver360::Tag` | 120 | DIFF | 80.33 | 100.00 | ADOPTED DC3 body (batch swap) |
| synth_xbox/Synapse_dsp.cpp | `DSP::Synapse::Synapse::ProcessInPlace` | 864 | DIFF | 99.95 | 99.95 | DC3 body compiled, tried in batch: row DOWN (99.95->99.77); kept ours |
| synth_xbox/Synapse_dsp.cpp | `DSP::Synapse::Synapse::Synapse` | 1620 | DIFF | 98.40 | 98.40 | DC3 body does not compile here (C3861: 'RoundToSamples': identifier not found); DC3-revision API/members |
| synth_xbox/Synth.cpp | `CXAPOBase::QueryInterface` | 196 | DC3_ONLY | 0.00 | 0.00 | our definition not located by the extractor (macro/inline); DC3 has one |
| synth_xbox/Synth.cpp | `CXAPOBase::Release` | 112 | DC3_ONLY | 0.00 | 0.00 | our definition not located by the extractor (macro/inline); DC3 has one |
| synth_xbox/Synth.cpp | `OnSetParameters` | 16 | NOT_FOUND | 0.00 | 0.00 | definition not located in either tree by the extractor |
| synth_xbox/Synth.cpp | `Synth360::Init` | 900 | DIFF | 99.98 | 99.98 | DC3 body does not compile here (C2653: 'FxSendBitCrush360' : is not a class or namespace name); DC3-revision API/members |
| synth_xbox/Synth.cpp | `Synth360::SetupHeadsetSubmixes` | 588 | DIFF | 85.95 | 99.88 | HAND-PORT: DC3 body with real IXAudio2 vcalls, DC3 log stripped |
| synth_xbox/Synth.cpp | `fn_82B5AB80` | 128 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Synth.cpp | `fn_82B5AC00` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Synth.cpp | `fn_82B5AF08` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Synth.cpp | `fn_82B5B174` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Synth.cpp | `fn_82B5B238` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Synth.cpp | `fn_82B5E028` | 12 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/SynthSample.cpp | `fn_82B6C7B0` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `StartVoiceThreadEntry` | 972 | DIFF | 99.69 | 99.69 | DC3 body does not compile here (C2065: 'gShutdownVoiceThread' : undeclared identifier); DC3-revision API/members |
| synth_xbox/Voice.cpp | `fn_82B64C20` | 12 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C42958` | 68 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C429A0` | 72 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C429E8` | 52 | ANON | 99.23 | 99.23 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C42A20` | 72 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C42A68` | 96 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C42AC8` | 72 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C42B10` | 52 | ANON | 99.23 | 99.23 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C42B48` | 72 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C42B90` | 52 | ANON | 99.23 | 99.23 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C42BC8` | 72 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C42C10` | 60 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C42C50` | 64 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C42C98` | 92 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C42CF4` | 40 | ANON | 93.90 | 93.90 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C42D20` | 72 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/Voice.cpp | `fn_82C42D68` | 52 | ANON | 99.62 | 99.62 | anonymous fn_ row; no name to pair with DC3 |
| synth_xbox/XMAReader.cpp | `?Poll@XMAReader@@UAAXM@Z` | 1556 | NO_DC3_FILE | 99.85 | 99.85 | no counterpart file in ../dc3-decomp |
| synth_xbox/XMAReader.cpp | `XMAReader::Init` | 916 | NO_DC3_FILE | 99.56 | 99.56 | no counterpart file in ../dc3-decomp |
| synth_xbox/soundtouch/source/SoundTouch/FIRFilter.cpp | `soundtouch::FIRFilter::setCoefficients` | 152 | SAME | 77.21 | 77.21 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| track/TrackDir.cpp | `TrackDir::PollActiveWidgets` | 260 | NO_DC3_FILE | 99.92 | 99.92 | no counterpart file in ../dc3-decomp |
| track/TrackDir.cpp | `TrackDir::SetSlotXfm` | 336 | NO_DC3_FILE | 99.40 | 99.40 | no counterpart file in ../dc3-decomp |
| track/TrackDir.cpp | `fn_827DCED8` | 112 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| track/TrackDir.cpp | `fn_827DCF78` | 112 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| track/TrackDir.cpp | `fn_827DD010` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| track/TrackWidget.cpp | `??$_S_sort@VMeshInstance@@V?$StlNodeAlloc@VMeshInstance@@@stlpmtx_std@@V?$WidgetInstanceCmp@VMeshInstance@@@@@` | 424 | NO_DC3_FILE | 99.81 | 99.81 | no counterpart file in ../dc3-decomp |
| track/TrackWidget.cpp | `??_GImmediateWidgetImp@@UAAPAXI@Z` | 76 | NO_DC3_FILE | 99.74 | 99.74 | no counterpart file in ../dc3-decomp |
| track/TrackWidget.cpp | `??_GMatWidgetImp@@UAAPAXI@Z` | 76 | NO_DC3_FILE | 99.74 | 99.74 | no counterpart file in ../dc3-decomp |
| track/TrackWidget.cpp | `??_GTrackWidgetImpBase@@UAAPAXI@Z` | 68 | NO_DC3_FILE | 99.71 | 99.71 | no counterpart file in ../dc3-decomp |
| track/TrackWidget.cpp | `fn_827E2B7C` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| track/TrackWidget.cpp | `fn_827E2CE4` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| track/TrackWidget.cpp | `fn_827E3FB8` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| track/TrackWidgetImp.cpp | `??$_S_sort@VTextInstance@@V?$StlNodeAlloc@VTextInstance@@@stlpmtx_std@@V?$WidgetInstanceCmp@VTextInstance@@@@@` | 424 | NO_DC3_FILE | 99.81 | 99.81 | no counterpart file in ../dc3-decomp |
| track/TrackWidgetImp.cpp | `CharWidgetImp::AddTextInstance` | 244 | NO_DC3_FILE | 99.18 | 99.18 | no counterpart file in ../dc3-decomp |
| track/TrackWidgetImp.cpp | `CharWidgetImp::RemoveInstances` | 396 | NO_DC3_FILE | 95.84 | 95.84 | no counterpart file in ../dc3-decomp |
| track/TrackWidgetImp.cpp | `ImmediateWidgetImp::DrawInstances` | 600 | NO_DC3_FILE | 95.65 | 95.65 | no counterpart file in ../dc3-decomp |
| track/TrackWidgetImp.cpp | `fn_827E5510` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| track/TrackWidgetImp.cpp | `fn_827E5940` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| track/TrackWidgetImp.cpp | `fn_827E5F30` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| track/TrackWidgetImp.cpp | `fn_827E60F4` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| track/TrackWidgetImp.cpp | `fn_827E611C` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| track/TrackWidgetImp.cpp | `fn_827E6350` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| track/TrackWidgetImp.cpp | `fn_827E6378` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `Synth::Init` | 840 | OURS_ONLY | 99.98 | 99.98 | definition not present in the DC3 file |
| ui/CheatProvider.cpp | `fn_823F45A0` | 108 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F4618` | 64 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F4658` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F4680` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F4688` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F4698` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F46A0` | 132 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F4730` | 80 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F47B0` | 96 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F4A58` | 96 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F4AB8` | 80 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F4B10` | 96 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F4BA8` | 96 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F4C78` | 108 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F4CE8` | 112 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F4E18` | 324 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F4F64` | 60 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_823F52C0` | 116 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_826FFFC0` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| ui/CheatProvider.cpp | `fn_82700738` | 60 | ANON | 99.60 | 99.60 | anonymous fn_ row; no name to pair with DC3 |
| ui/InlineHelp.cpp | `??_GInlineHelp@@$4PPPPPPPM@A@AAPAXI@Z` | 12 | NO_QNAME | 0.00 | 0.00 | could not derive a qualified name from the symbol |
| ui/InlineHelp.cpp | `InlineHelp::SyncLabelsToConfig` | 308 | DIFF | 99.94 | 99.94 | DC3 body does not compile here (C2664: 'stlpmtx_std::vector<_Tp>::push_back' : cannot convert parameter 1 from 'UILabel *'); DC3-revision API/members |
| ui/InlineHelp.cpp | `fn_822784A8` | 60 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/InlineHelp.cpp | `fn_823163F8` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/LabelNumberTicker.cpp | `LabelNumberTicker::Poll` | 380 | SAME | 99.47 | 99.47 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| ui/LabelNumberTicker.cpp | `fn_828290A0` | 20 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/LabelNumberTicker.cpp | `fn_828290B8` | 196 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/LabelNumberTicker.cpp | `fn_82829180` | 56 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/LabelNumberTicker.cpp | `fn_828291C0` | 88 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/LabelShrinkWrapper.cpp | `??_GLabelShrinkWrapper@@$4PPPPPPPM@A@AAPAXI@Z` | 12 | NO_QNAME | 0.00 | 0.00 | could not derive a qualified name from the symbol |
| ui/PanelDir.cpp | `PanelDir::PanelNav` | 384 | DIFF | 96.77 | 100.00 | ADOPTED DC3 body (batch swap) |
| ui/PanelDir.cpp | `fn_827ED200` | 56 | ANON | 99.64 | 99.64 | anonymous fn_ row; no name to pair with DC3 |
| ui/PanelDir.cpp | `fn_828070E0` | 96 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UI.cpp | `??$__destroy_aux@UEntry@LocalePanel@@@stlpmtx_std@@YAXPAUEntry@LocalePanel@@ABU__false_type@0@@Z` | 4 | NO_QNAME | 95.00 | 95.00 | could not derive a qualified name from the symbol |
| ui/UI.cpp | `UIManager::PushScreen` | 96 | SAME | 0.00 | 0.00 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| ui/UI.cpp | `fn_823F4C38` | 60 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UI.cpp | `fn_823F4D60` | 132 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UI.cpp | `fn_823F4FA8` | 172 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UI.cpp | `fn_823F507C` | 44 | ANON | 99.45 | 99.45 | anonymous fn_ row; no name to pair with DC3 |
| ui/UI.cpp | `fn_823F50B0` | 320 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UI.cpp | `fn_823F5218` | 44 | ANON | 99.45 | 99.45 | anonymous fn_ row; no name to pair with DC3 |
| ui/UI.cpp | `fn_823F5244` | 44 | ANON | 99.91 | 99.91 | anonymous fn_ row; no name to pair with DC3 |
| ui/UI.cpp | `fn_823F5340` | 236 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UI.cpp | `fn_823F542C` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| ui/UI.cpp | `fn_823F5460` | 572 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UI.cpp | `fn_823F56C4` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| ui/UIComponent.cpp | `UIComponent::Update` | 1192 | OURS_ONLY | 98.56 | 98.56 | definition not present in the DC3 file |
| ui/UIComponent.cpp | `fn_82343940` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UIComponent.cpp | `fn_828012D8` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UIFontImporter.cpp | `fn_828027A8` | 24 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UIGridProvider.cpp | `UIGridProvider::ResizeSubProviders` | 340 | NO_DC3_FILE | 99.94 | 99.94 | no counterpart file in ../dc3-decomp |
| ui/UILabel.cpp | `UILabel::LabelUpdate` | 464 | DIFF | 99.57 | 99.57 | DC3 body does not compile here (C2511: 'void UILabel::LabelUpdate(bool)' : overloaded member function not found in 'UILabe); DC3-revision API/members |
| ui/UILabel.cpp | `UILabel::SetTokenFmtImp` | 288 | DIFF | 75.93 | 75.93 | DC3 body compiled, tried in batch: row DOWN (75.93->70.11); kept ours |
| ui/UILabelDir.cpp | `fn_82812418` | 80 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UIList.cpp | `??$__destroy_mv_srcs@PAV?$vector@VVector3@@V?$StlNodeAlloc@VVector3@@@stlpmtx_std@@@stlpmtx_std@@V12@@stlpmtx_` | 84 | NO_QNAME | 99.71 | 99.71 | could not derive a qualified name from the symbol |
| ui/UIList.cpp | `?_M_erase@?$vector@V?$vector@VVector3@@V?$StlNodeAlloc@VVector3@@@stlpmtx_std@@@stlpmtx_std@@V?$StlNodeAlloc@V` | 292 | NOT_FOUND | 99.86 | 99.86 | definition not located in either tree by the extractor |
| ui/UIList.cpp | `fn_82775C98` | 292 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UIList.cpp | `fn_827F96AC` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| ui/UIList.cpp | `fn_827FDA90` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UIListDir.cpp | `UIListDir::BuildDrawState` | 1444 | DIFF | 93.02 | 93.02 | DC3 body does not compile here (C2511: 'void UIListDir::BuildDrawState(UIListWidgetDrawState &,const UIListState &,UICompo); DC3-revision API/members |
| ui/UIListSlot.cpp | `UIListSlot::StartScroll` | 128 | SAME | 99.84 | 99.84 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| ui/UIListState.cpp | `UIListState::Scroll` | 652 | DIFF | 99.97 | 99.97 | DC3 body does not compile here (C2039: 'mSelected' : is not a member of 'ScrollState'); DC3-revision API/members |
| ui/UIPanel.cpp | `fn_82814440` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UIPicture.cpp | `fn_828161E0` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UIProxy.cpp | `?Load@UIPicture@@$4PPPPPPPM@A@AAXAAVBinStream@@@Z` | 12 | NO_DC3_FILE | 0.00 | 0.00 | no counterpart file in ../dc3-decomp |
| ui/UIProxy.cpp | `UIProxy::SyncDir` | 324 | NO_DC3_FILE | 99.26 | 99.26 | no counterpart file in ../dc3-decomp |
| ui/UIProxy.cpp | `fn_8282486C` | 48 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| ui/UISlider.cpp | `?SetTypeDef@UISlider@@$4PPPPPPPM@A@AAXPAVDataArray@@@Z` | 16 | NO_QNAME | 73.75 | 73.75 | could not derive a qualified name from the symbol |
| ui/UITrigger.cpp | `fn_82826318` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| utl/BeatMap.cpp | `fn_827D2868` | 12 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/BufStream.cpp | `BufStream::ReadImpl` | 164 | DIFF | 99.76 | 99.76 | DC3 body compiled, tried in batch: row DOWN (99.76->72.44); kept ours |
| utl/BufStream.cpp | `BufStream::~BufStream` | 104 | SAME | 92.31 | 92.31 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| utl/Cache_Xbox.cpp | `CacheXbox::GetFreeSpaceSync` | 328 | DIFF | 99.76 | 99.76 | DC3 body compiled, tried in batch: row DOWN (99.76->91.10); kept ours |
| utl/Cache_Xbox.cpp | `CacheXbox::ThreadGetFileSize` | 196 | DIFF | 90.61 | 90.61 | DC3 body compiled, tried in batch: row DOWN (90.61->83.67); kept ours |
| utl/Cache_Xbox.cpp | `CacheXbox::ThreadWrite` | 420 | DIFF | 99.52 | 100.00 | ADOPTED DC3 body (batch swap) |
| utl/ChunkStream.cpp | `ChunkStream::Eof` | 932 | DIFF | 96.91 | 96.91 | DC3 body compiled, tried in batch: row DOWN (96.91->95.69); kept ours |
| utl/ChunkStream.cpp | `fn_827CA180` | 100 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/DataPointMgr.cpp | `GameModeTerminate` | 80 | NOT_FOUND | 0.00 | 0.00 | definition not located in either tree by the extractor |
| utl/FilePath.cpp | `fn_82C40BF8` | 52 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/FilePath.cpp | `fn_82C40C30` | 20 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/FilePath.cpp | `fn_82C40C48` | 84 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/FilePath.cpp | `fn_82C40CA0` | 132 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/FilePath.cpp | `fn_82C40D28` | 12 | ANON | 98.33 | 98.33 | anonymous fn_ row; no name to pair with DC3 |
| utl/FilePath.cpp | `fn_82C40D38` | 44 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/FilePath.cpp | `fn_82C40D68` | 60 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/FilePath.cpp | `fn_82C40DA8` | 12 | ANON | 98.33 | 98.33 | anonymous fn_ row; no name to pair with DC3 |
| utl/Loader.cpp | `fn_827BFB38` | 88 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/Loader.cpp | `fn_827BFC48` | 76 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/Locale.cpp | `Locale::Init` | 1304 | DIFF | 96.47 | 96.84 | HAND-PORT (partial): LiteralArray error context = previous chunk array (retail r4 carry); rest of DC3 body is DC3-only devkit/mInitialized code |
| utl/Locale.cpp | `fn_827CA010` | 40 | ANON | 93.40 | 93.40 | anonymous fn_ row; no name to pair with DC3 |
| utl/MBT.cpp | `fn_827D1088` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/MeasureMap.cpp | `fn_827D0E70` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| utl/MemHeap.cpp | `MemHeap::Alloc` | 652 | DIFF | 91.37 | 91.37 | DC3 splits out TryAlloc + minFreeBytes (newer revision); retail 652 B body is the inline form we have; not taken |
| utl/MemHeap.cpp | `MemHeap::Init` | 216 | DIFF | 82.81 | 82.81 | DC3 body tried by hand: row DOWN 82.81->81.83, reverted; DC3 extra is mMinFreeBytes, which RB3 lacks |
| utl/MemHeap.cpp | `MemHeap::Print` | 624 | DIFF | 98.01 | 98.01 | DC3 body compiled, tried in batch: row DOWN (98.01->97.06); kept ours |
| utl/MemMgr.cpp | `AddHeap` | 344 | DIFF | 98.43 | 98.43 | DC3 body does not compile here (C2661: 'AddHeap' : no overloaded function takes 7 arguments); DC3-revision API/members |
| utl/MemMgr.cpp | `MemAlloc` | 644 | DC3_ONLY | 97.14 | 97.14 | our definition not located by the extractor (macro/inline); DC3 has one |
| utl/MemMgr.cpp | `MemTruncate` | 284 | DIFF | 97.11 | 97.11 | DC3 body does not compile here (C1004: unexpected end-of-file found); DC3-revision API/members |
| utl/MemMgr.cpp | `ThreadMemStack` | 348 | DIFF | 85.17 | 88.16 | ADOPTED DC3 body (batch swap) |
| utl/MemMgr.cpp | `fn_823F1898` | 28 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/MemTrack.cpp | `StopLog` | 92 | DIFF | 99.78 | 99.78 | DC3 body compiled, tried in batch: row DOWN (99.78->85.87); kept ours |
| utl/MemTracker.cpp | `MemTracker::DiffDump` | 500 | DIFF | 99.04 | 99.04 | DC3 body does not compile here (C2039: 'Free' : is not a member of 'AllocInfoVec'); DC3-revision API/members |
| utl/MemTracker.cpp | `MemTracker::MemTracker` | 312 | PARTIAL | 74.81 | 74.81 | differs from DC3; overloads could not be paired one-to-one, not swapped |
| utl/MultiTempoTempoMap.cpp | `MultiTempoTempoMap::GetLoopTick` | 164 | PARTIAL | 76.95 | 100.00 | ADOPTED DC3 body (batch swap) |
| utl/NetCacheMgr.cpp | `NetCacheMgr::AddLoaderRef` | 696 | DIFF | 96.44 | 97.84 | HAND-PORT: NetLoaderRef ctors (+8 funclets up) |
| utl/NetCacheMgr.cpp | `NetCacheMgr::SetState` | 124 | DIFF | 96.13 | 96.13 | DC3 body compiled, tried in batch: row DOWN (96.13->92.90); kept ours |
| utl/NetCacheMgr.cpp | `fn_827CE6C8` | 40 | ANON | 99.40 | 99.50 | anonymous fn_ row; no name to pair with DC3 [row rose 99.40->99.50 as a side effect of a sibling change] |
| utl/NetCacheMgr.cpp | `fn_827CE6F0` | 40 | ANON | 99.80 | 100.00 | anonymous fn_ row; no name to pair with DC3 [row rose 99.80->100.00 as a side effect of a sibling change] |
| utl/NetCacheMgr.cpp | `fn_827CE718` | 40 | ANON | 99.80 | 100.00 | anonymous fn_ row; no name to pair with DC3 [row rose 99.80->100.00 as a side effect of a sibling change] |
| utl/NetCacheMgr.cpp | `fn_827CE740` | 40 | ANON | 99.30 | 99.50 | anonymous fn_ row; no name to pair with DC3 [row rose 99.30->99.50 as a side effect of a sibling change] |
| utl/NetCacheMgr.cpp | `fn_827CE768` | 40 | ANON | 99.90 | 100.00 | anonymous fn_ row; no name to pair with DC3 [row rose 99.90->100.00 as a side effect of a sibling change] |
| utl/NetCacheMgr.cpp | `fn_827CE790` | 64 | ANON | 99.88 | 100.00 | anonymous fn_ row; no name to pair with DC3 [row rose 99.88->100.00 as a side effect of a sibling change] |
| utl/NetCacheMgr.cpp | `fn_827CE7D0` | 40 | ANON | 99.30 | 100.00 | anonymous fn_ row; no name to pair with DC3 [row rose 99.30->100.00 as a side effect of a sibling change] |
| utl/NetCacheMgr.cpp | `fn_827CE7F8` | 40 | ANON | 99.30 | 99.50 | anonymous fn_ row; no name to pair with DC3 [row rose 99.30->99.50 as a side effect of a sibling change] |
| utl/NetCacheMgr_Xbox.cpp | `NetCacheMgrXbox::~NetCacheMgrXbox` | 84 | DC3_ONLY | 85.19 | 85.19 | our definition not located by the extractor (macro/inline); DC3 has one |
| utl/NetLoader.cpp | `DataNetLoader::PollLoading` | 276 | SAME | 99.35 | 99.35 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| utl/PoolAlloc.cpp | `FixedSizeAlloc::RawAlloc` | 176 | DIFF | 93.52 | 93.52 | DC3 body compiled, tried in batch: row DOWN (93.52->76.14); kept ours |
| utl/Profiler.cpp | `fn_827D2858` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/Song.cpp | `??0?$list@PAVMidiParser@@V?$StlNodeAlloc@PAVMidiParser@@@stlpmtx_std@@@stlpmtx_std@@QAA@ABV01@@Z` | 116 | NO_QNAME | 97.24 | 97.24 | could not derive a qualified name from the symbol |
| utl/Song.cpp | `Song::SyncState` | 1192 | DIFF | 96.72 | 96.72 | DC3 body compiled, tried in batch: row DOWN (96.72->79.05); kept ours |
| utl/Song.cpp | `fn_8230EC54` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| utl/Song.cpp | `fn_827C6E64` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| utl/Song.cpp | `fn_827C72B8` | 44 | ANON | 99.55 | 99.55 | anonymous fn_ row; no name to pair with DC3 |
| utl/Song.cpp | `fn_827C7808` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| utl/Song.cpp | `fn_827C7830` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| utl/SongInfoCopy.cpp | `fn_827D10E8` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/SongInfoCopy.cpp | `fn_827D10F8` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/SongInfoCopy.cpp | `fn_827D1100` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/SongInfoCopy.cpp | `fn_827D1108` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/SongInfoCopy.cpp | `fn_827D15F4` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| utl/Str.cpp | `String::rfind` | 140 | OURS_ONLY | 98.57 | 98.57 | definition not present in the DC3 file |
| utl/StringTable.cpp | `fn_827C9540` | 72 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/TextFileStream.cpp | `fn_822716E8` | 16 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| utl/UTF8.cpp | `UTF8toASCIIs` | 128 | SAME | 96.25 | 96.25 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| utl/WaveFile.cpp | `fn_827D5BB0` | 80 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| world/CameraManager.cpp | `??_GTransitionEvent@@UAAPAXI@Z` | 88 | NO_QNAME | 0.00 | 0.00 | could not derive a qualified name from the symbol |
| world/CameraManager.cpp | `CameraManager::RandomizeCategory` | 428 | DIFF | 99.95 | 99.95 | DC3 body compiled, tried in batch: row flat (99.95->99.95); kept ours |
| world/CameraManager.cpp | `fn_824BC5EC` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| world/CameraManager.cpp | `fn_825AD700` | 16 | ANON | 68.75 | 68.75 | anonymous fn_ row; no name to pair with DC3 |
| world/CameraShot.cpp | `??$_Destroy@VCamShotCrowd@@@stlpmtx_std@@YAXPAVCamShotCrowd@@@Z` | 4 | NO_QNAME | 95.00 | 95.00 | could not derive a qualified name from the symbol |
| world/CameraShot.cpp | `??$__uninitialized_copy@PAVCamShotFrame@@PAV1@@stlpmtx_std@@YAPAVCamShotFrame@@PAV1@00ABU__false_type@0@@Z` | 96 | NO_QNAME | 85.92 | 85.92 | could not derive a qualified name from the symbol |
| world/CameraShot.cpp | `CamShot::CamShot` | 968 | NOT_FOUND | 87.43 | 87.43 | definition not located in either tree by the extractor |
| world/CameraShot.cpp | `CamShot::Load` | 2996 | DIFF | 99.88 | 99.88 | DC3 body does not compile here (C2352: 'RndTransformable::Load' : illegal call of non-static member function); DC3-revision API/members |
| world/CameraShot.cpp | `CamShot::SetPos` | 920 | DIFF | 99.87 | 99.87 | DC3 body does not compile here (C3861: 'WorldXfm': identifier not found); DC3-revision API/members |
| world/CameraShot.cpp | `CamShot::Shake` | 1148 | DIFF | 99.76 | 99.86 | ADOPTED DC3 body (batch swap) |
| world/CameraShot.cpp | `CamShot::StartAnim` | 552 | DIFF | 99.93 | 99.93 | DC3 body does not compile here (C2039: 'SetCrowds' : is not a member of 'CameraManager'); DC3-revision API/members |
| world/CameraShot.cpp | `CamShotFrame::BuildTransform` | 1024 | DIFF | 96.28 | 96.28 | DC3 body does not compile here (C2039: 'WorldXfm' : is not a member of 'CamShot'); DC3-revision API/members |
| world/CameraShot.cpp | `CamShotFrame::Interp` | 1772 | DIFF | 99.06 | 99.06 | DC3 body does not compile here (C2661: 'ATanInterpolator::ATanInterpolator' : no overloaded function takes 2 arguments); DC3-revision API/members |
| world/CameraShot.cpp | `fn_82371388` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| world/CameraShot.cpp | `fn_824C3EA0` | 96 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| world/CameraShot.cpp | `fn_824C47AC` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| world/CameraShot.cpp | `fn_824C4864` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| world/CameraShot.cpp | `fn_824C8BC4` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| world/CameraShot.cpp | `fn_824C8BEC` | 40 | ANON | 99.80 | 99.80 | anonymous fn_ row; no name to pair with DC3 |
| world/CameraShot.cpp | `fn_824C8C14` | 40 | ANON | 99.30 | 99.30 | anonymous fn_ row; no name to pair with DC3 |
| world/CameraShot.cpp | `fn_824C8C3C` | 40 | ANON | 99.80 | 99.80 | anonymous fn_ row; no name to pair with DC3 |
| world/CameraShot.cpp | `fn_824C8C64` | 40 | ANON | 99.80 | 99.80 | anonymous fn_ row; no name to pair with DC3 |
| world/CameraShot.cpp | `fn_824C8C8C` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| world/CameraShot.cpp | `fn_824C8CB4` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| world/ColorPalette.cpp | `resize` | 124 | NOT_FOUND | 99.84 | 99.84 | definition not located in either tree by the extractor |
| world/Crowd.cpp | `??$PropSync@UCharData@WorldCrowd@@@@YA_NAAV?$ObjList@UCharData@WorldCrowd@@@@AAVDataNode@@PAVDataArray@@HW4Pro` | 396 | NO_QNAME | 97.63 | 97.63 | could not derive a qualified name from the symbol |
| world/Crowd.cpp | `WorldCrowd::CharData::CharData` | 92 | NOT_FOUND | 80.13 | 80.13 | definition not located in either tree by the extractor |
| world/Crowd.cpp | `WorldCrowd::CharDef::Load` | 132 | DIFF | 96.52 | 96.52 | DC3 body does not compile here (C2511: 'void WorldCrowd::CharDef::Load(BinStreamRev &)' : overloaded member function not f); DC3-revision API/members |
| world/Crowd.cpp | `WorldCrowd::DrawShowing` | 2072 | DIFF | 88.68 | 88.68 | DC3 body needs DC3-revision members (mCharForceLod), already gated RB3_WORLDCROWD_DC3_REV; not taken |
| world/Crowd.cpp | `WorldCrowd::SetFullness` | 612 | DIFF | 94.50 | 94.50 | DC3 body does not compile here (C2660: 'WorldCrowd::AssignRandomColors' : function does not take 1 arguments); DC3-revision API/members |
| world/Crowd.cpp | `fn_824E3918` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| world/Crowd.cpp | `fn_824E3940` | 40 | ANON | 99.80 | 99.80 | anonymous fn_ row; no name to pair with DC3 |
| world/Crowd.cpp | `fn_824E3968` | 40 | ANON | 99.80 | 99.80 | anonymous fn_ row; no name to pair with DC3 |
| world/Dir.cpp | `fn_824CFAEC` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| world/EventAnim.cpp | `??3RndAnimatable@@SAXPAX@Z` | 4 | NO_DC3_FILE | 95.00 | 95.00 | no counterpart file in ../dc3-decomp |
| world/EventAnim.cpp | `fn_824CA264` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| world/Instance.cpp | `??_EBandRetargetVignette@@UAAPAXI@Z` | 68 | NO_QNAME | 0.00 | 0.00 | could not derive a qualified name from the symbol |
| world/Instance.cpp | `WorldInstance::SavePersistentObjects` | 676 | DIFF | 97.63 | 97.63 | DC3 spelling tried by hand: row DOWN 97.63->97.10, reverted |
| world/Instance.cpp | `fn_824EA298` | 8 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| world/LightPreset.cpp | `LightPreset::Animate` | 772 | DIFF | 99.84 | 99.84 | DC3 body compiled, tried in batch: row flat (99.84->99.84); kept ours |
| world/LightPreset.cpp | `LightPreset::Copy` | 936 | DIFF | 99.98 | 99.98 | DC3 body compiled, tried in batch: row DOWN (99.98->37.75); kept ours |
| world/LightPreset.cpp | `LightPreset::Load` | 3024 | DIFF | 99.60 | 99.60 | DC3 body does not compile here (C2679: binary '>>' : no operator found which takes a right-hand operand of type 'Spotlight); DC3-revision API/members |
| world/LightPreset.cpp | `fn_824B86B8` | 40 | ANON | 99.80 | 99.80 | anonymous fn_ row; no name to pair with DC3 |
| world/LightPreset.cpp | `fn_824B86E0` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| world/LightPreset.cpp | `fn_824B8708` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| world/LightPreset.cpp | `fn_824B8730` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| world/LightPreset.cpp | `fn_824B8758` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| world/LightPreset.cpp | `fn_824B8780` | 40 | ANON | 99.40 | 99.40 | anonymous fn_ row; no name to pair with DC3 |
| world/LightPreset.cpp | `fn_824B87A8` | 40 | ANON | 99.80 | 99.80 | anonymous fn_ row; no name to pair with DC3 |
| world/LightPreset.cpp | `fn_824B87D0` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
| world/LightPresetManager.cpp | `fn_824B9ED8` | 40 | ANON | 99.50 | 99.50 | anonymous fn_ row; no name to pair with DC3 |
| world/Spotlight.cpp | `Spotlight::BeamDef::Load` | 440 | DIFF | 98.73 | 98.73 | DC3 body does not compile here (C2511: 'void Spotlight::BeamDef::Load(BinStreamRev &)' : overloaded member function not fo); DC3-revision API/members |
| world/Spotlight.cpp | `Spotlight::BuildNGCone` | 1692 | SAME | 74.90 | 74.90 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| world/Spotlight.cpp | `Spotlight::BuildNGQuad` | 964 | SAME | 86.61 | 86.61 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| world/Spotlight.cpp | `Spotlight::BuildNGSheet` | 1232 | DIFF | 95.13 | 98.56 | ADOPTED DC3 body (batch swap) |
| world/Spotlight.cpp | `Spotlight::Copy` | 716 | DIFF | 99.64 | 99.64 | DC3 body compiled, tried in batch: row DOWN (99.64->98.52); kept ours |
| world/Spotlight.cpp | `Spotlight::DrawShowing` | 904 | DIFF | 93.89 | 93.89 | DC3 body compiled, tried in batch: row DOWN (93.89->79.34); kept ours |
| world/Spotlight.cpp | `Spotlight::Poll` | 556 | DIFF | 96.49 | 96.49 | DC3 body compiled, tried in batch: row DOWN (96.49->81.29); kept ours |
| world/Spotlight.cpp | `Spotlight::SetColor` | 176 | SAME | 91.25 | 91.25 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| world/Spotlight.cpp | `Spotlight::SyncProperty` | 4728 | DIFF | 99.39 | 99.39 | DC3 body compiled, tried in batch: row DOWN (99.39->98.46); kept ours |
| world/Spotlight.cpp | `Spotlight::UpdateTransforms` | 1340 | SAME | 99.66 | 99.66 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| world/Spotlight.cpp | `fn_824DC570` | 4 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| world/Spotlight.cpp | `fn_824DFA50` | 36 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| world/SpotlightDrawer.cpp | `??$sort@PAVSpotlightEntry@SpotlightDrawer@@VByColor@@@stlpmtx_std@@YAXPAVSpotlightEntry@SpotlightDrawer@@0VByC` | 112 | NO_QNAME | 99.82 | 99.82 | could not derive a qualified name from the symbol |
| world/SpotlightDrawer.cpp | `SpotlightDrawer::ApplyLightingApprox` | 444 | DIFF | 92.97 | 92.97 | DC3 body compiled, tried in batch: row flat (92.97->92.97); kept ours |
| world/SpotlightDrawer.cpp | `SpotlightDrawer::ClearLights` | 192 | SAME | 99.79 | 99.79 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| world/SpotlightDrawer.cpp | `SpotlightDrawer::DrawLight` | 616 | DIFF | 99.71 | 99.71 | DC3 body does not compile here (C2039: 'GetCastShadow' : is not a member of 'Spotlight'); DC3-revision API/members |
| world/SpotlightDrawer.cpp | `SpotlightDrawer::DrawShadow` | 328 | DIFF | 94.18 | 94.18 | DC3 uses RndDrawable::DrawShadow(xfm, float) API (char lane surface); not taken |
| world/SpotlightDrawer.cpp | `SpotlightDrawer::DrawShowing` | 136 | DIFF | 80.59 | 80.59 | DC3 body compiled, tried in batch: row DOWN (80.59->32.94); kept ours |
| world/SpotlightDrawer.cpp | `SpotlightDrawer::Init` | 112 | SAME | 94.11 | 94.11 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| world/SpotlightDrawer.cpp | `fn_82C43780` | 52 | ANON | 0.00 | 0.00 | anonymous fn_ row; no name to pair with DC3 |
| world/SpotlightDrawer_NG.cpp | `?_M_erase@?$vector@VSpotMeshEntry@SpotlightDrawer@@V?$StlNodeAlloc@VSpotMeshEntry@SpotlightDrawer@@@stlpmtx_st` | 96 | NOT_FOUND | 87.29 | 87.29 | definition not located in either tree by the extractor |
| world/SpotlightDrawer_NG.cpp | `NgSpotlightDrawer::ClearPostProc` | 160 | SAME | 99.75 | 99.75 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| world/SpotlightDrawer_NG.cpp | `NgSpotlightDrawer::RenderConeDefs` | 1324 | SAME | 95.27 | 95.27 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| world/SpotlightDrawer_NG.cpp | `NgSpotlightDrawer::RenderScene` | 588 | PARTIAL | 99.59 | 99.59 | differs from DC3; overloads could not be paired one-to-one, not swapped |
| world/SpotlightDrawer_NG.cpp | `NgSpotlightDrawer::SetupXSection` | 2104 | SAME | 76.45 | 76.45 | body identical to DC3 (comments/whitespace aside); nothing to adopt |
| world/SpotlightDrawer_NG.cpp | `_M_fill_insert_aux` | 396 | NOT_FOUND | 97.12 | 97.12 | definition not located in either tree by the extractor |
| world/ThreeDSoundManager.cpp | `fn_8256B544` | 40 | ANON | 99.90 | 99.90 | anonymous fn_ row; no name to pair with DC3 |
