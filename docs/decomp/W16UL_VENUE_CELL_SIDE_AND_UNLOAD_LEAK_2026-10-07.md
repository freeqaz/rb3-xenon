# W16-UL — the eight empty venue renders, and the venue unload leak (2026-10-07)

Branch `w16-ul`, worktree `~/tmp/wt-w16ul`, based on main `e58933c62`.
Takes up the two problems W16-UJ recorded and did not investigate
(`docs/decomp/W16UJ_VENUE_LOADS_AND_CLAMP_NAN_2026-10-07.md` §2, §4, §7):

- eight shipped venue milos rendered an empty frame in rb3-render: `arena_11/banner`,
  `big_club_14/banner_mim`, `video_02` to `video_07`;
- memory grew about 13 MB per venue load and unload.

## 0. Headline

- **The empty frames were the cell camera standing behind single-sided geometry.** Retail
  would show nothing from there either. The native engine already culls as retail does. The
  harness camera now takes the side the scene's own single-sided geometry faces, and all 8
  render (§1).
- **The memory growth was a real leak in the native `~ObjectDir`, and it is fixed.**
  - A native-only pre-nullify nulled the parent's `mSubDirs` `ObjDirPtr` instead of releasing
    it, so each venue's inlined `*_base.milo` subdirectory and everything under it was never
    destroyed.
  - DC3 had found and fixed the same defect on 2026-09-30 (`dc3-decomp` `cafbd23da`,
    `43bf21c36`, `1902b1704`). Its final form is ported here.
  - Live heap over the 52 venue cycles was 88 → 719 MB and is now 27 → 35 MB.
  - Peak RSS of the default rb3-render run fell from 1,028 MB (W16-UJ) to 401 MB (§2).
- **The leak also corrupted later loads.** With the leak restored and the new cells run
  first, W16-TY's fixture loaded 1,733 objects instead of 5,215: its shared sub-loads got the
  leaked directories back (§4, round A).
- **Residue, recorded and not changed: about 0.15 MB per venue** (§2.4).
  - It is retail's `gNotifies` buffer holding the native-only "Can't make <class>"
    factory-miss notices.
  - rb3-render never installs App's modal callback, which is what empties that buffer in
    retail.
- **62 new gates in rb3-render's default mode**: 8 front-side venue cells, 1 cull control, 52
  `ul-venue-freed` and 1 `ul-venues-freed`. All pass. Each round of sabotage failed exactly
  the predicted gates, plus one FAIL that was not predicted, explained in §4.

## 1. The eight empty renders

### 1.1 What retail shows

- **Retail's cull rule.** `NgMat::SetBasicState` (`rndobj/Mat_NG.cpp`) sets cull mode 2 when
  `mCull` is set, and 0 otherwise.
  - On Xenos that value goes into `PA_SU_SC_MODE_CNTL`: bit 1 is `cull_back`, and bit 2
    `face` = 0 means "front is CCW" (`xenia/src/xenia/gpu/registers.h`).
  - So retail draws only the faces that appear counter-clockwise to the viewer.
  - `mCull` loads as a one-byte bool, adjudicated on retail bytes (`BaseMaterial.cpp:253`).
- **The assets' winding.** A temporary probe (since removed) compared each face's right-hand
  winding with its own three vertex normals (DEC4N at byte 20 of the 36-byte record):

| mesh | cull | faces agreeing / disagreeing / degenerate | front, world |
|---|---|---|---|
| `arena_11/banner` `banner.mesh` | 1 | 90 / 0 / 1 | (0.00, −1.00, 0.00) |
| `arena_11/banner` `banner_pole.mesh` | 1 | 16 / 0 / 0 | — (closed) |
| `crowd_female01` horns / fist / clap / lighter | 1 | 504 / 0, 504 / 0, 501 / 0, 504 / 0 | — |
| `crowd_female01` body | 1 | 1,364 / 14 | — |

  - So retail shows a single-sided face only to a viewer on the side its vertex normal
    points to.
  - **The banner's front is −Y.** Viewed from +Y, retail culls every face of it.
- **Native agrees with retail.**
  - `Mesh_Wgpu.cpp:234` reads the same `GetCull()` byte.
  - `PipelineManager.cpp:520` has CCW front with Back cull.
  - Native renders the banner from −Y (53.37% coverage) and culls it from +Y (0.00%).
  - The −Y image shows the knight riding to the viewer's left (dexter), the heraldic
    convention for a front.

### 1.2 Why the cells were empty

- An explicit-path cell placed its camera at azimuth 0.45 rad and elevation 0.30, i.e. on
  the **+Y** side (`eye = c + d·(sin az, cos az, …)`).
- **Banners.** The scene is only the banner and its pole, so the frame is the clear colour.
- **Video venues.**
  - The backdrops (`background_state`, the splat, the ribbons) are single-sided and face −Y.
  - The one figure left is out of frame from +Y. Drawn alone with `--only-mesh
    male_crowd_body01`, it covers 0.00% from +Y and 0.26% from −Y. A closed skinned body
    shows its back from behind, so this is framing, not culling.
- `video_01` passed only narrowly from +Y: 2.94%, against 82.93% from −Y.

**Two-side sweep over all 52 milos** (`~/tmp/w16ul/sweep_az.sh`, `--frames 1`,
`--azimuth 0.45` and `--azimuth 3.5916`; summary in `~/tmp/w16ul/az1/summary.tsv`):

| | from +Y (0.45) | from −Y (0.45 + π) |
|---|---|---|
| frames with coverage ≥ 1% | 44 | **52** |
| empty (≤ 0.01%) | the 8 above | 0 |

The same probe summed the area-weighted right-hand face normals of every showing, unskinned,
single-sided mesh and dotted the sum with the +Y eye direction:

- 46 of 52 are negative, so their single-sided geometry faces −Y;
- the other 6 lie between 0.000 and +0.077, i.e. enclosed venues with no net side;
- the 8 empty ones read −0.84 to −0.90.

### 1.3 Fix (`native/src/main_render.cpp`, harness only)

- `ChooseCellSide` computes that sum (`ComputeSingleSidedFront`). It counts showing,
  unskinned meshes whose material culls. It leaves out:
  - skinned meshes, whose vertices the palette places, not the mesh transform;
  - the harness's own `x3_fallback_mat`.
- An explicit-path cell (`kSideFront`) moves its camera to azimuth + π when the eye is on the
  far side of that front.
- An explicit `--azimuth` is still taken as given.
- The two original default cells keep `kSideFixed`. Their PNGs are **byte-identical** to
  main's binary (`cmp`).
- A cell with no single-sided static geometry keeps its azimuth. The hand-pose cell
  (`crowd_female01`) reads 0 meshes, no flip. Its `--hand-audit` run gives 21 PASS / 1 FAIL
  and a PNG byte-identical to main's binary: the FAIL is the existing
  `handpose-measured-hand-geometry` baseline.
- Each cell prints a `framing:` line with the front, the mesh and face counts, and the
  azimuth change.

**Not investigated.**

- `video_04` to `video_07` are four distinct shipped files, each loading its own
  `video_0N_base.milo`, and they render byte-identical PNGs.
- The video frames are dark.
- I take these to be what the venues vary (light presets, post-processing) not acting on one
  static frame. That is an inference, not a measurement.

## 2. The unload leak

### 2.1 Measured

Live heap (`mallinfo2().uordblks`) and RSS were read after each unload in W16-UJ's venue loop
(temporary instrumentation, since removed):

- **All 52, pre-fix.** Live heap 88.1 → 719.3 MB; RSS 267.8 → 898.2 MB.
  - Heap tracks RSS, so this is retained allocation, not allocator fragmentation.
  - Per venue: about 25–35 MB for an arena, about 15 for a big club, about 10 for a small
    club, about 4 for a video venue, 0 for the 8 props, banners and `test`.
- **`small_club_01` loaded and unloaded 5 times.** It retains +16.1 MB every cycle, so this is
  a per-load leak, not a first-load cache.
- **gperftools heap profile.** `LD_PRELOAD=libtcmalloc_and_profiler.so`, with
  `HeapProfilerDump` after each unload; `pprof -base` between consecutive unloads.
  - 15.99 MB per cycle, all under `DirLoader::LoadObjs`.
  - 12.8 MB of it is `RndBitmap::AllocateBuffer` (from `RndTex::PostLoad`), and 1.74 MB is
    `RndMesh::LoadVertices`.
  - Whole objects survive: per cycle 147 of 156 `RndMesh`, 120 of 138 `NgMat` and 6 of 8
    `WorldDir` allocations are still live.
- **Directory census**, with a hook in `~ObjectDir` and a walk of the loaded tree before the
  unload:
  - the root `WorldDir` was destroyed;
  - its only subdirectory, the inlined `small_club_01_base.milo` (384 objects, one
    `ObjDirPtr`), **survived**, with all 66 directories under it;
  - in the second cycle the shared `world/shared/*` dirs showed 2 `ObjDirPtr`s: the second
    load had shared the first load's leaked copies.

### 2.2 Cause

- In retail, `~ObjectDir`'s `mSubDirs.clear()` releases the last `ObjDirPtr` to a
  subdirectory, and `ObjDirPtr::operator=` deletes it.
- The native `~ObjectDir` first runs a pre-nullify over every directory in the cascade. It
  came in with the 2026-05-26 dc3 scaffold, `c5c1650fb`.
  - The pre-nullify calls `NullifyAllRefs()` on each subdirectory.
  - That subdirectory's ref ring holds the **parent's own `mSubDirs` entry**.
  - `ObjRefConcrete::NullifyObj` (`Object.h:285`) sets `mObject = nullptr`, with no release
    and no `DirPtrRefCounts` decrement.
  - So `mSubDirs.clear()` found a null entry and released nothing, and the subdirectory was
    never destroyed.
  - Its loader also stayed registered, so a later shared load of the same file got the leaked
    directory back.
- DC3's commit `43bf21c36` describes the same mechanism on its party-mode route. There, a
  leaked `augmented_photo.milo` came back to the second panel and crashed every draw.

### 2.3 Fix (`src/system/obj/Dir.cpp`, `HX_NATIVE` only)

DC3's final form is carried over:

- **`NullifyAllRefsKeepingSubDirPtrs`.** It unlinks the `ObjDirPtr`s that cascade dirs hold
  to a subdirectory, nullifies its ring, then relinks them, so `mSubDirs.clear()` releases
  them and the last one destroys the subdirectory.
- **`CollectSurvivorClosure` / `IsSurvivor`**, the transitive survivor set.
- **Unconditional**, per `1902b1704`, which cites retail's `ObjectDir::DeleteObjects`
  deleting every named object with no DirPtr test.

**Not carried over:** DC3's separate async-unload change (`b01fd8ba7`, which stops eager
nulling under `DirUnloader`). The async path is not taken here, and that change is a
different defect.

- `gNativeLiveObjectDirs` counts `ObjectDir`s constructed and not yet destroyed. It is
  native-only and is the gate's instrument.
- **After.** `small_club_01` ×4 destroys 268 of 268 census directories. Live heap after each
  unload is 27.2, 27.3, 27.4 and 27.6 MB. Over all 52 venues, live heap is 27.2 → 35.0 MB and
  RSS 270.0 → 278.8 MB.

### 2.4 Residue (recorded, not changed)

- After the fix, live heap still grows about 0.15 MB per venue.
- The profile puts it all under `Debug::Notify` → `Debug::Modal` → `DebugModal`, from
  `DirLoader::CreateObjects`.
- `DebugModal` (`os/Debug.cpp:88`) appends every non-fatal notify to `gNotifies`, and only
  `Debug::SetModalCallback` empties it. Retail's `App::App` installs `AppDebugModal`
  (`App.cpp:182`); rb3-render never constructs `App`.
- The notices are the native-only factory misses: about 709 "Can't make <class>" per small
  club.
- This is the harness's configuration, not an engine leak, so it was left as is.

## 3. Gates (rb3-render default mode)

| gate | checks | `--no-*` |
|---|---|---|
| 8 cells `image-not-empty` | `arena_11/banner`, `big_club_14/banner_mim` and `video_02`–`video_07` rendered with `kSideFront`: coverage ≥ 1%, ≥ 16 colours | `--no-w16ul` |
| `ul-back-culled` | `arena_11/banner` with `kSideBack`: coverage 0.00%, 1 colour, over a front of > 0 faces, i.e. retail's cull. Written to `banner_back.png` | `--no-w16ul` |
| `ul-venue-freed <milo>` ×52 | in W16-UJ's venue loop: the load created > 0 `ObjectDir`s and the unload left 0 of them alive | `--no-w16uj` |
| `ul-venues-freed` | 52 of 52, with totals (4,030 created, 0 left) | `--no-w16uj` |

- **Full default run.** 410 PASS, 0 FAIL, `RESULT: ALL GATES PASSED`. Wall time 74.4 s, peak
  RSS 401 MB (`~/tmp/w16ul/full1.log`).
- **W16-UJ's run for comparison.** 52.0 s, 1,028 MB. Its run did not include the 9 cells,
  which add about 20 s.

## 4. Sabotage

The predictions were written to `~/tmp/w16ul_sab/{A,B}_predictions.txt` before each build.

**Round A.** The `Dir.cpp` pre-nullify was restored to the pre-W16-UL loop, with the counter
kept. `ChooseCellSide` was made to flip a `kSideBack` cell when `h < 0`, which puts the cull
control on the front.

- The sabotage description in the prediction file was corrected before the build: my first
  version routed the control through the wrong gate. The predicted outcomes were not
  changed.

| prediction | result |
|---|---|
| `ul-venue-freed` FAIL for the 44 venue roots, PASS for the 8 props, banners and `test` | 44 FAIL, 8 PASS |
| `ul-venues-freed` FAIL, "8 of 52" | FAIL, "8 of 52 … (3486 created, 3431 left alive)" |
| `ul-back-culled` FAIL at about 53.4% | FAIL, 53.37% |
| `all-cells-rendered` FAIL | FAIL |
| every other gate PASS; total 47 FAIL | **wrong: 48 FAIL.** `ty-fixture` also failed: `tv11_a.milo_xbox` loaded 1,733 objects and 210 meshes (it requires more than 300), and the TY phase stopped early (388 gates printed, not 410) |
| peak RSS near 1,000 MB | 1,012 MB |

**The unpredicted FAIL is the leak's second symptom.**

- The 8 new venue cells run before the TY phase and load shared milos such as
  `char/main/main.milo`. With the leak, their trees stay registered with their refs nulled.
- TY loads its vignette with `share=true` and got those directories back.
- Control: the same sabotaged binary with `--no-w16ul` (cells off) gives `ty-fixture` PASS
  with 5,215 objects and 417 meshes (`~/tmp/w16ul_sab/A2_nocells.log`).
- The fixed build reads 5,215 with the cells on.

**Round B.** `ChooseCellSide` does not flip; `Dir.cpp` is as committed.

| prediction | result |
|---|---|
| `image-not-empty` FAIL on the 8 front cells (0.00%; `video_03` 0.01%) | 8 FAIL, exactly those values |
| `ul-back-culled` PASS | PASS |
| `all-cells-rendered` FAIL | FAIL |
| `ul-venue-freed` ×52 and `ul-venues-freed` PASS | PASS, 52 of 52 |
| 9 FAIL in total | 9 FAIL |

Both rounds were restored to the committed source (`git status` clean).

## 5. X360 A/B

- **Prediction: Δ0.** Every `src/` change is inside `HX_NATIVE`. It was measured anyway,
  because `Dir.cpp`'s lines shift.
- **Method.** `python3 tools/ab_measure.py --worktree ~/tmp/wt-w16ul --revert 45e4f878b --jobs 8
  --label w16ul-dir-cascade`. Leg A is the commit, leg B is without it.

```
leg A: matched=54948 masked=25223 honest=29725 code%=59.315384  (recompiles: 0, settled)
leg B: matched=54948 masked=25223 honest=29725 code%=59.315384  (recompiles: 4, split=0, patch_steps=6, settle iterations: 2)
Δmatched=+0  Δmasked_equal=+0  Δhonest=+0  Δcode%=+0.000000pp  Δcode_bytes=+0
Δfuzzy=+0.000000pp   (legA 64.316060 -> legB 64.316060)
units at 100% [mpn ruler]: legA 608 -> legB 608
```

- The `none` control was flat as well (+0 B).
- The tool restored the worktree and verified it.

## 6. Not done

- **DC3's async-unload change** (`b01fd8ba7`) is not ported (§2.3).
- **The `gNotifies` residue** (§2.4) is not changed.
- **`video_04`–`07` byte-identical renders and the dark video frames** (§1.3) are not
  investigated.
- **rb3-render's factory misses** (`BandCamShot`, `Sfx`, …) remain, as in W16-UJ.
- **The other native targets** run their venue loads through the same `~ObjectDir`, so they
  get the leak fix. Only rb3-render gates it.

## 7. Result lines

RESULT_PLACEHOLDER

## Reproduce

```
cmake --build native/build --target rb3-render
native/build/rb3-render ~/code/milohax/rb3/orig-assets/xbox-zip <outdir>     # all W16-UL gates run by default
native/build/rb3-render <assets> <outdir> world/venue/video/video_02/gen/video_02.milo_xbox --frames 1
native/build/rb3-render <assets> <outdir> world/venue/video/video_02/gen/video_02.milo_xbox --frames 1 --azimuth 0.45   # the old, back-side view
tools/native_health.sh <worktree>
tools/native_build_gate.sh
```
