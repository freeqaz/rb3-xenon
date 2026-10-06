# W16-QO — frame-size census and fixes (2026-10-06)

Lane W16-QO, branch `w16-qo`, worktree `~/tmp/wt-w16qo`. The census was taken at `ad2929d31`;
the branch was then rebased onto main `1059a558f`. Four forks worked in their own worktrees
(`~/tmp/wt-w16qo-{a,b,c,d}`, branches `w16-qo-{a,b,c,d}`), and each was merged into `w16-qo` with
`--no-ff`.

**Brief.** A function whose stack frame differs from retail's drags down every EH funclet that
shares the frame. Each funclet opens with `subi rD, r12, <parent frame>`. W16-QK found
`CamShot::Load` (0x460 vs retail 0x450, 7 funclets) and `BandDirector::OnMidiShot5Cleanup`
(0x130 vs 0x120, 4 funclets). The lane's job was to census every in-scope paired function whose
frame differs, rank the rows by the bytes the row and its funclets would gain, and fix the top
cases in source against retail bytes.

**Ruler.** `name_check`, objdiff 4.2.9, read from `report.json` `provenance`.

## Result

| | |
|---|---|
| census | **49 in-scope rows / 51,788 B prize**, out of 145 frame-differing rows over 22,498 compared frames |
| rows crossed to 100 | **8 functions + 17 funclets** |
| whole-binary A/B | **+22 fns / +7,720 B / +0.075336 pp**; Δhonest +7, Δmasked_equal +15 |
| row-level diff of the A/B legs | **26 rows up, 0 down**, 0 rows on one side only (68,909 rows each) |
| units at 100 (mpn) | 576 → 578 (`SampleData`, `StorePreviewMgr`); 0 fell off |
| native gate | `NATIVE_GATE_RESULT verdict=PASS expected=18 verified=18 skipped=0 partial=0 failed=0 rc=0` |

**Prediction vs measurement.** Before the A/B I summed the crossed rows from the worktrees' own
`report.json` files: 548 + 440 + 776 + (2,996 + 7×40) + (404 + 3×40) + (1,172 + 3×40) + (704 + 4×40)
= **7,720 B**. The A/B measured **+7,720 B**.

## 1. The census

**Instruments.**

- **Detector:** `tools/w23_frame_scan.py`, unchanged. It decodes `stwu r1,-N(r1)` from the COFF
  bodies of both the target and base objects.
- **Collectability:** `tools/w23_collectable.py`, run on the graded ruler.
- **Funclet pricing:** new `tools/w16qo_frame_funclets.py` (see below).
- **Scope:** Quazal and XDK excluded by unit source path or `@Quazal@@` in the symbol.

| stage | count |
|---|---:|
| paired units scanned | 1,057 (2,064 unpaired) |
| frames compared | 22,498 (7,738 functions with no frame are leaves) |
| frames equal | 22,353 |
| **frames differ** | **145** (W23 measured 317 of 17,122 on 08-17) |
| Quazal | 96 |
| XDK | 0 |
| **in scope** | **49** |

**Direction.** In 26 rows retail's frame is bigger; in 23 ours is. The ±0x10 deltas account for 33
of the 49 rows.

The full ranked table is `docs/decomp/w16qo-frame-census.tsv`: prize, body, funclets, frames,
fuzzy/mpn, charge profile and verdict for each row.

### Funclet pricing — each instrument misses a case the other finds

W23's displacement-multiset count read **0** funclets for `OnMidiShot5Cleanup`, where W16-QK saw 4.
The reason is that our `BandDirector` object already carries a surplus of 0x120 displacements from
other functions, so `min(target surplus, base surplus)` clamps to zero.

The new address-adjacency walk exploits how retail lays funclets out: they follow their parent
contiguously, and each one's first word re-derives the parent frame. The walk reproduces both of
W16-QK's figures (CamShot 7/7, OnMidiShot5Cleanup 4 open of 6). It reads 0 for
`StorePreviewMgr::Handle` and `SongSectionController::Handle`, though, because their funclet runs
start 0x40 past the parent and are separated by 8-byte gaps.

Each row is therefore priced as **body + the larger of the two funclet counts**. The A/B confirmed
StorePreviewMgr's 4 funclets (multiset) and BandDirector's 3 crossers (adjacency).

### Collectability: W23's NAME-BLOCKED verdict is mostly not about names

| verdict | rows | prize |
|---|---:|---:|
| COLLECTABLE | 24 | 24,180 B |
| **SAVE/RESTORE-COUNT** (W23 labels these NAME-BLOCKED) | **21** | **21,320 B** |
| NAME-BLOCKED (a real fold name) | 1 | 5,156 B (`RndAmbientOcclusion::Tessellate`, `vector<Vector2>` vs `vector<FacePriority>`) |
| NO-FRAME-SITE | 3 | 612 B |

Of the 22 rows `w23_collectable.py` calls NAME-BLOCKED, **21 carry no name charges except one
`__savegprlr_N`/`__restgprlr_N` (or `__savefpr_N`) pair.** The helper's name encodes how many
callee-saved registers are saved, so these rows carry a register-count difference, which is a
source or liveness matter. No alias or map work applies to them. The tool's W19 rule treats any bare
symbol argument as a name charge, and those helper names pass that rule.

## 2. Fixes

| row | frame, ours → retail | before | after | cause | lever |
|---|---|---|---|---|---|
| `DirLoader::Cleanup` | 0x90 → 0x80 | 99.96 | **100** | The stripped-log `FilePath` copy was a named local at 0x58. Retail overlays it on the `mState` pointer-to-member temporary at 0x50. | `MiloStripEval(fmt, mFile, mTimer.Ms())` makes it a by-value call-argument temporary |
| `SampleData::Load` | 0xa0 → 0x90 | 99.95 | **100** | Same cause: two FilePath copies; retail overlays both at 0x60. | `MiloStripEval` / `MILO_WARN` with `fp` by value (the oracle's LOG/WARN) |
| `Synth::UpdateOverlay` (unit `CharMeshHide`) | 0x110 → 0xf0 | 96.50 | **100** | (a) Pollable count: retail saves `begin()` in r31, top-tests it, and reuses it in the draw loop; our inlined `size()` reloaded `begin()`. (b) `char buf[64]` should be `[32]`. | Oracle loop shape; 32-byte buffer |
| `CamShot::Load` + 7 funclets (fork A) | 0x460 → 0x450 | 99.88 | **100** | Six scalars where retail has `float fov[2]; Transform tf[2]; Vector2 vec[2];` (DC3's spelling). | Arrays: one contiguous block, ascending pairs |
| `XboxServer::Init` + 3 funclets (fork D) | 0x90 → 0x80 | 99.93 | **100** | The `Symbol("filter")` temporary in `String filter(cfg->FindArray("filter")->Str(1))` kept slot 0x50 until the end of the block. | Compute the `const char *` in its own statement first |
| `BandDirector::OnMidiShot5Cleanup` + 3 of 4 funclets (fork B) | 0x130 → 0x120 | 98.36 | **100** | (a) The failure path is an early return, not an enclosing `if`. (b) The loop's named `Symbol shot` took a private slot. | Early return; `const Symbol &shot = PickShot(...)` packs into the shared Symbol-temporary slot at 0x54 |
| `StorePreviewMgr::Handle` + 4 funclets (fork B) | 0xc0 → 0xd0 | 99.95 | **100** | `download_preview_file` called `AddToDownloadQueue` directly. Retail inlines the `DownloadPreviewFile` wrapper (0x827B2350), whose scope gives the two HANDLE_EXPR String temporaries their own slot. | Call the wrapper, as the oracle does |
| `UtilDrawPlane` (fork C) | 0x190 → 0x150 | 99.34 | 99.90 (not crossed) | Four `Vector3` corner locals; retail has one `Vector3 pts[4]` packed onto `tf88`'s dead slots. `mb0` is initialised before `tf88`. | Array, DC3's documented shape; the residue is the `Dot` fma chain (DC3 records it as a stop) |

The funclet left at 99.5, `fn_822993BC`, now matches retail's frame. Its only charge is a destructor
callee name (`vector<CamCatEntry>` vs retail `_Vector_base<FilePath>`), which is alias-lane work,
not source work.

### ★ Levers, and what they correct

1. **⛔ W23 §4's "named vs temporary is INERT on stack-slot merging" was refuted for the case that
   matters.** W23 tested a `(void)FilePath(fp)` *expression* temporary, and this lane re-measured it
   on `DirLoader::Cleanup`: it is byte-identical to a named local. A *by-value call argument* to
   an inlined empty function (`MiloStripEval`) behaves differently: MSVC overlays it on other
   temporaries' slots. `SampleData::Load`, the row W23 annotated "do not retry", is now 100. The
   in-source note has been rewritten.
   ⚠ Wrapping the pointer-to-member assignment in its own `{ }` block was also inert.
2. **A frame surplus with every accessed slot at retail's offset means an object nobody addresses
   is too big.** Here that was `char buf[64]` where retail has 32 bytes.
   `run_diff_inspect mode=asm_listing` confirmed every other local's offset.
   The prediction (32 bytes ⇒ 0xf0 and all 9 sites close) held exactly.
3. **Arrays pack where scalars do not** (CamShot `fov/tf/vec[2]`, UtilDrawPlane `pts[4]`). Look for
   retail slots that are contiguous and ascending in pairs. Both shapes were already DC3's.
4. **Temporaries in a class-type declaration's initializer live to the end of the block**
   (`XboxServer::Init`). Hoist the sub-expression into its own statement.
5. **Binding a returned temporary as `const T &` packs it into a shared temporary slot**, where a
   named local gets a private one (`OnMidiShot5Cleanup`).
6. **An inlined wrapper's scope changes slot sharing** (`StorePreviewMgr::Handle`).

## 3. Attempted, not fixed (spellings measured)

| row | finding | tried |
|---|---|---|
| `FocusTracker::Poll_` (retail 0xb0) | Retail keeps the second `TrackerPlayerID next` at 0x70, not overlaid on the first one's 0x60. | `b78` at the top of the block; `mult` folded; `const TrackerPlayerID &next = ...`. All inert. Hoisting `next` was **not** a valid probe (it adds a default ctor + assign). |
| `NetSession::Poll` (retail 0xa0) | Retail stores 0 to 0x50 at entry, then `startTime`; the GetTime temporary is at 0x58. Likely a construction flag for a conditional `Time` temporary. | Oracle's nested-if shape: frame 0xa0 but +2 saved registers, row falls to 87.0, reverted. `(void)&startTime` and assignment-not-declaration were inert. |
| `BandTrack::SetCrowdRating` (retail 0xb0) | Retail stores `mTrackIdx` to 0x60(r31) twice; both stores are dead. Likely an inlined helper with an address-taken parameter. | `int idx` instead of `int &`: 97.04, worse, reverted. |
| `Spotlight::UpdateTransforms` (ours 0x1e0 vs 0x1b0, mpn 100) | Retail overlays the beam block's `m6c` on the lens block's `m48`; ours does not. | Decl swaps, removing the inner braces, a named lens Transform, `m48` first. All inert. |
| `RndFlare::DrawShowing` (retail 0x130) | Slot offsets are identical; retail just has 0x10 of unused frame. | Three spellings of `useOccResult`, all worse (97.96 / 97.27 / 95.38), consistent with prior w7-n and w15-a notes. |

## 4. Deliberately not done

- **`SongSectionController::Handle`** (1,520 B, 7 funclets): retail saves 9 GPRs to our 8, with a
  75-instruction swap cascade over 11 pairs inside `HANDLER` macro code. This is a liveness job, not
  a slot fix. Diagnosed, not attempted.
- **The other 20 SAVE/RESTORE-COUNT rows**, and the COLLECTABLE rows at fuzzy ≤ 94 with 20+ hard
  sites (`DrawToTexture`, `JoypadPollCommon`, `WorldCrowd::DrawShowing`, `BuildDrawState`, `IKElbow`,
  `fft_matrix_inverse_columnwise`, …). In these the frame is a *symptom* of a divergent body or a
  register-count difference. A stack-layout screen was run on 12 of them: none has the "all slots
  match, frame differs" shape that the levers above close.
- **`Tessellate`** (the one real fold-name block): alias-lane work.
- **No map, alias or splits edits**; source only. **Permuter not run** (standing directive).
- **`docs/decomp/w23-frame-queue.tsv`** (08-17, 317 rows) is left as written; it is a dated record.
  The current census is `w16qo-frame-census.tsv`.

## Reproduce

```
python3 tools/w23_frame_scan.py --project . --json-out ~/tmp/w16qo_frames.json
# filter to in-scope rows (unit source path not under network/quazal or xdk) -> ~/tmp/w16qo_inscope.json
python3 tools/w23_collectable.py --project . --frames ~/tmp/w16qo_inscope.json --min-fuzzy 0 --top 49 --json-out ~/tmp/w16qo_collect.json
python3 tools/w16qo_frame_funclets.py ~/tmp/w16qo_collect.json ~/tmp/w16qo_census.json
```

**A/B.** `python3 tools/ab_measure.py --worktree ~/tmp/wt-w16qo-ab --patch <git diff main..w16-qo -- src>`.

- Worktree `~/tmp/wt-w16qo-ab` at main `1059a558f`.
- Run dir: `~/tmp/wt-w16qo-ab/.ab_measure_runs/20261006-141734-w16-qo-4017376/`; log
  `~/tmp/rb3_ab_w16qo.log`.
- Patch kind `['source']`; leg B had 46 recompiles and settled in 2 iterations.
- The measured patch predates the native-build guard in `SampleData.cpp`, which is byte-neutral for
  the match build: `MILO_WARN` *is* `MiloStripEval` there, and the LOG branch keeps
  `MiloStripEval` under `#else`. After a rebuild, `SampleData::Load` was re-verified with all 110
  instructions equal.
